#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "FuzzDSP.h"

class HairyFuzzProcessor : public juce::AudioProcessor
{
public:
    static const juce::StringArray modeNames;

    HairyFuzzProcessor();
    ~HairyFuzzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    static constexpr int oversamplingOrder = 2; // 2^2 = 4x
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::array<fuzz::FuzzChannel, 2> channels;

    // Dry path delayed to line up with the oversampling latency, for click-free bypass.
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dryDelay;
    int dryDelayWrite = 0;
    int latency = 0;

    std::atomic<float>* fuzzValue = nullptr;
    std::atomic<float>* filterValue = nullptr;
    std::atomic<float>* volumeValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    std::atomic<float>* modeValue = nullptr;
    std::atomic<float>* siliconValue = nullptr;

    // Mode / diode switching crossfades between two independent voices.
    int currentVoice = -1, previousVoice = -1;
    int fadePos = 0, fadeLength = 1;
    juce::AudioParameterBool* bypassParam = nullptr;

    juce::SmoothedValue<float> fuzzSmooth, filterSmooth;    // oversampled rate
    juce::SmoothedValue<float> volumeSmooth, wetSmooth;     // base rate

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairyFuzzProcessor)
};
