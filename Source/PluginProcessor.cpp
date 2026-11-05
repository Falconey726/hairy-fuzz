#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray HairyFuzzProcessor::modeNames { "Vintage", "Hairy", "Sustain", "Octave", "Dist", "Drive" };

namespace
{
// Output trim per voice (mode x germanium/silicon) so switching modes keeps a similar loudness.
constexpr std::array<float, fuzz::FuzzChannel::numVoices> voiceTrimDb {
    3.7f, 0.9f,   // vintage  (germanium, silicon)
    0.5f, -3.4f,  // hairy
    3.8f, -0.5f,  // sustain
    2.6f, -1.4f,  // octave
    2.8f, -1.1f,  // dist
    3.0f, 1.0f    // drive
};

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
    modeValue = apvts.getRawParameterValue ("mode");
    siliconValue = apvts.getRawParameterValue ("silicon");
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
        std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false),
        std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, 1),
        std::make_unique<AudioParameterBool> (ParameterID { "silicon", 1 }, "Diode", false,
            AudioParameterBoolAttributes().withStringFromValueFunction (
                [] (bool si, int) { return si ? String ("Silicon") : String ("Germanium"); }))
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

    currentVoice = fuzz::FuzzChannel::voiceIndex ((int) modeValue->load(), siliconValue->load() > 0.5f);
    previousVoice = -1;
    fadeLength = juce::jmax (1, (int) (osRate * 0.015f));
    fadePos = fadeLength;
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

    const int targetVoice = fuzz::FuzzChannel::voiceIndex (
        juce::jlimit (0, fuzz::numModes - 1, (int) modeValue->load()), siliconValue->load() > 0.5f);
    if (targetVoice != currentVoice && fadePos >= fadeLength)
    {
        previousVoice = currentVoice;
        currentVoice = targetVoice;
        for (auto& ch : channels)
            ch.voices[(size_t) currentVoice].reset();
        fadePos = 0;
    }

    // Wet path: upsample, run the circuit, downsample.
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels,
                                        (size_t) numSamples);
    auto osBlock = oversampling->processSamplesUp (block);
    const auto osSamples = osBlock.getNumSamples();

    for (size_t i = 0; i < osSamples; ++i)
    {
        const float drive = fuzzSmooth.getNextValue();
        const float cutoff = filterCutoff (filterSmooth.getNextValue());
        const bool fading = fadePos < fadeLength;
        const float newGain = fading ? (float) fadePos / (float) fadeLength : 1.0f;
        if (fading)
            ++fadePos;

        const float curTrim = juce::Decibels::decibelsToGain (voiceTrimDb[(size_t) currentVoice]);
        const float prevTrim = previousVoice >= 0 ? juce::Decibels::decibelsToGain (voiceTrimDb[(size_t) previousVoice]) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = osBlock.getChannelPointer ((size_t) ch);
            auto& voices = channels[(size_t) ch].voices;
            const float x = data[i];
            float y = curTrim * voices[(size_t) currentVoice].process (x, drive, cutoff);
            if (fading && previousVoice >= 0)
                y = newGain * y + (1.0f - newGain) * prevTrim * voices[(size_t) previousVoice].process (x, drive, cutoff);
            data[i] = y;
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
