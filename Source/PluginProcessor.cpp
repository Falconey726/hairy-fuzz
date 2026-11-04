#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float stageBias = 0.22f;

float gainStage1 (float fuzz) { return juce::Decibels::decibelsToGain (6.0f + 32.0f * fuzz); }
float gainStage2 (float fuzz) { return juce::Decibels::decibelsToGain (3.0f + 22.0f * fuzz); }

// Filter works like the RAT: turning it up makes the sound darker.
float filterCutoff (float filter) { return 14000.0f * std::pow (450.0f / 14000.0f, filter); }

float volumeGain (float volume)
{
    return volume <= 0.001f ? 0.0f : juce::Decibels::decibelsToGain (-40.0f + 46.0f * volume);
}
} // namespace

HairyFuzzProcessor::HairyFuzzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HairyFuzz", createLayout())
{
    fuzzValue = apvts.getRawParameterValue ("fuzz");
    filterValue = apvts.getRawParameterValue ("filter");
    volumeValue = apvts.getRawParameterValue ("volume");
    bypassValue = apvts.getRawParameterValue ("bypass");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

juce::AudioProcessorValueTreeState::ParameterLayout HairyFuzzProcessor::createLayout()
{
    using namespace juce;
    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 10.0f * 10.0f) / 10.0f, 1); });

    return {
        std::make_unique<AudioParameterFloat> (ParameterID { "fuzz", 1 }, "Fuzz",
                                               NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent),
        std::make_unique<AudioParameterFloat> (ParameterID { "filter", 1 }, "Filter",
                                               NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent),
        std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
                                               NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent),
        std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false)
    };
}

bool HairyFuzzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void HairyFuzzProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (size_t) juce::jmax (1, getTotalNumOutputChannels());

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numChannels, oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    latency = juce::roundToInt (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    const auto osRate = (float) sampleRate * (float) (1 << oversamplingOrder);
    for (auto& ch : channels)
        ch.prepare (osRate);

    dryBuffer.setSize ((int) numChannels, samplesPerBlock);
    dryDelay.setSize ((int) numChannels, latency + 1);
    dryDelay.clear();
    dryDelayWrite = 0;

    fuzzSmooth.reset (osRate, 0.03);
    filterSmooth.reset (osRate, 0.03);
    volumeSmooth.reset (sampleRate, 0.03);
    wetSmooth.reset (sampleRate, 0.02);

    fuzzSmooth.setCurrentAndTargetValue (fuzzValue->load());
    filterSmooth.setCurrentAndTargetValue (filterValue->load());
    volumeSmooth.setCurrentAndTargetValue (volumeValue->load());
    wetSmooth.setCurrentAndTargetValue (bypassValue->load() > 0.5f ? 0.0f : 1.0f);
}

void HairyFuzzProcessor::reset()
{
    if (oversampling != nullptr)
        oversampling->reset();
    for (auto& ch : channels)
        ch.reset();
    dryDelay.clear();
}

void HairyFuzzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels(),
                                        dryBuffer.getNumChannels(), (int) channels.size());

    for (int ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0 || numChannels == 0)
        return;

    // Some hosts send larger blocks than promised; grow the scratch buffers if so.
    if (numSamples > dryBuffer.getNumSamples())
        dryBuffer.setSize (dryBuffer.getNumChannels(), numSamples, false, false, true);

    // Delay the dry signal by the oversampling latency so bypass fades line up.
    const int delayLength = dryDelay.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        float* dry = dryBuffer.getWritePointer (ch);
        float* line = dryDelay.getWritePointer (ch);
        int w = dryDelayWrite;
        for (int i = 0; i < numSamples; ++i)
        {
            line[w] = in[i];
            int r = w - latency;
            if (r < 0)
                r += delayLength;
            dry[i] = line[r];
            if (++w >= delayLength)
                w = 0;
        }
    }
    dryDelayWrite = (dryDelayWrite + numSamples) % delayLength;

    fuzzSmooth.setTargetValue (fuzzValue->load());
    filterSmooth.setTargetValue (filterValue->load());
    volumeSmooth.setTargetValue (volumeValue->load());
    wetSmooth.setTargetValue (bypassValue->load() > 0.5f ? 0.0f : 1.0f);

    // Wet path: upsample, run the circuit, downsample.
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels,
                                        (size_t) numSamples);
    auto osBlock = oversampling->processSamplesUp (block);
    const auto osSamples = osBlock.getNumSamples();

    for (size_t i = 0; i < osSamples; ++i)
    {
        const float f = fuzzSmooth.getNextValue();
        const float g1 = gainStage1 (f);
        const float g2 = gainStage2 (f);
        const float cutoff = filterCutoff (filterSmooth.getNextValue());

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = osBlock.getChannelPointer ((size_t) ch);
            data[i] = channels[(size_t) ch].process (data[i], g1, g2, stageBias, cutoff);
        }
    }

    oversampling->processSamplesDown (block);

    // Output volume and bypass crossfade.
    for (int i = 0; i < numSamples; ++i)
    {
        const float vol = volumeGain (volumeSmooth.getNextValue());
        const float wet = wetSmooth.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* out = buffer.getWritePointer (ch);
            const float dry = dryBuffer.getReadPointer (ch)[i];
            out[i] = wet * vol * out[i] + (1.0f - wet) * dry;
        }
    }
}

void HairyFuzzProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HairyFuzzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HairyFuzzProcessor::createEditor()
{
    return new HairyFuzzEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HairyFuzzProcessor();
}
