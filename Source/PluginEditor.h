#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Draws the chunky black pedal knobs.
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// A stomp-box footswitch. Toggled on = bypassed.
class Footswitch : public juce::Button
{
public:
    Footswitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

class HairyFuzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HairyFuzzEditor (HairyFuzzProcessor&);
    ~HairyFuzzEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&);

    HairyFuzzProcessor& processor;
    PedalLookAndFeel lookAndFeel;

    juce::Slider fuzzKnob, filterKnob, volumeKnob;
    Footswitch footswitch;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> fuzzAttach, filterAttach, volumeAttach;
    std::unique_ptr<ButtonAttachment> bypassAttach;

    juce::Rectangle<float> ledArea;
    bool ledOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairyFuzzEditor)
};
