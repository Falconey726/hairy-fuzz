#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Draws the chunky black pedal knobs and the pointer-style mode selector.
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

private:
    void drawSelector (juce::Graphics&, juce::Rectangle<float>, float angle);
};

// A stomp-box footswitch. Toggled on = bypassed.
class Footswitch : public juce::Button
{
public:
    Footswitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

// A mini bat-lever toggle. Up = germanium, down = silicon.
class ToggleSwitch : public juce::Button
{
public:
    ToggleSwitch() : juce::Button ("Diode") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

class HairyFuzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HairyFuzzEditor (HairyFuzzProcessor&);
    ~HairyFuzzEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&);
    juce::Rectangle<float> modeLabelBounds (int index) const;

    HairyFuzzProcessor& processor;
    PedalLookAndFeel lookAndFeel;
    juce::Image portrait;

    juce::Slider fuzzKnob, filterKnob, volumeKnob, modeKnob;
    Footswitch footswitch;
    ToggleSwitch diodeSwitch;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> fuzzAttach, filterAttach, volumeAttach, modeAttach;
    std::unique_ptr<ButtonAttachment> bypassAttach, diodeAttach;

    juce::Rectangle<float> ledArea, portraitArea;
    bool ledOn = true;
    int shownMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairyFuzzEditor)
};
