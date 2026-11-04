#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 360;
constexpr int editorHeight = 520;
constexpr int knobSize = 96;
constexpr int knobTop = 62;

const juce::Colour enclosureTop { 0xff1f1f1f };
const juce::Colour enclosureBottom { 0xff0b0b0b };
const juce::Colour silkscreen { 0xfff2efe6 };
const juce::Colour ledRed { 0xffff2a1a };

juce::Font boldFont (float height)
{
    return juce::Font (juce::FontOptions (height).withStyle ("Bold"));
}

void drawScrew (juce::Graphics& g, juce::Point<float> c)
{
    const float r = 7.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8d8), c.x - r, c.y - r,
                                             juce::Colour (0xff6a6a6a), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.0f);
    g.drawLine (c.x - r * 0.6f, c.y - r * 0.2f, c.x + r * 0.6f, c.y + r * 0.2f, 1.6f);
}
} // namespace

//==============================================================================
void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider&)
{
    using namespace juce;
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float outer = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float knobR = outer * 0.74f;

    // Scale ticks printed on the enclosure, 0 to 10.
    g.setColour (silkscreen.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const float inner = outer * (i % 5 == 0 ? 0.82f : 0.87f);
        const auto p1 = centre.getPointOnCircumference (inner, a);
        const auto p2 = centre.getPointOnCircumference (outer, a);
        g.drawLine ({ p1, p2 }, i % 5 == 0 ? 2.0f : 1.2f);
    }

    // Drop shadow.
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (centre.x - knobR + 2.0f, centre.y - knobR + 4.0f, knobR * 2.0f, knobR * 2.0f);

    // Knurled skirt.
    g.setGradientFill (ColourGradient (Colour (0xff3a3a3a), centre.x, centre.y - knobR,
                                       Colour (0xff050505), centre.x, centre.y + knobR, false));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (Colour (0xff000000));
    for (int i = 0; i < 36; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine ({ centre.getPointOnCircumference (knobR * 0.86f, a),
                      centre.getPointOnCircumference (knobR, a) }, 1.4f);
    }

    // Cap.
    const float capR = knobR * 0.78f;
    g.setGradientFill (ColourGradient (Colour (0xff2c2c2c), centre.x - capR * 0.5f, centre.y - capR,
                                       Colour (0xff0e0e0e), centre.x + capR * 0.5f, centre.y + capR,
                                       true));
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (Colour (0xff4a4a4a));
    g.drawEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // White pointer line.
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    g.setColour (silkscreen);
    g.drawLine ({ centre.getPointOnCircumference (capR * 0.25f, angle),
                  centre.getPointOnCircumference (knobR * 0.95f, angle) }, 3.5f);
}

//==============================================================================
void Footswitch::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    using namespace juce;
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;

    // Hex nut.
    Path nut;
    nut.addPolygon (c, 6, r, MathConstants<float>::pi / 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xffcfcfcf), c.x, c.y - r, Colour (0xff5c5c5c), c.x, c.y + r, false));
    g.fillPath (nut);
    g.setColour (Colour (0xff2a2a2a));
    g.strokePath (nut, PathStrokeType (1.0f));

    // Plunger.
    const float pr = r * (isButtonDown ? 0.56f : 0.6f);
    g.setGradientFill (ColourGradient (Colour (0xfff5f5f5), c.x - pr, c.y - pr,
                                       Colour (0xff7d7d7d), c.x + pr, c.y + pr, false));
    g.fillEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f);
    g.setColour (Colour (0xff3a3a3a));
    g.drawEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f, 1.2f);

    if (isMouseOver)
    {
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f);
    }
}

//==============================================================================
HairyFuzzEditor::HairyFuzzEditor (HairyFuzzProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setupKnob (fuzzKnob);
    setupKnob (filterKnob);
    setupKnob (volumeKnob);
    addAndMakeVisible (footswitch);
    footswitch.setTooltip ("Bypass");

    fuzzAttach = std::make_unique<SliderAttachment> (processor.apvts, "fuzz", fuzzKnob);
    filterAttach = std::make_unique<SliderAttachment> (processor.apvts, "filter", filterKnob);
    volumeAttach = std::make_unique<SliderAttachment> (processor.apvts, "volume", volumeKnob);
    bypassAttach = std::make_unique<ButtonAttachment> (processor.apvts, "bypass", footswitch);

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

HairyFuzzEditor::~HairyFuzzEditor()
{
    for (auto* k : { &fuzzKnob, &filterKnob, &volumeKnob })
        k->setLookAndFeel (nullptr);
}

void HairyFuzzEditor::setupKnob (juce::Slider& knob)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    knob.setDoubleClickReturnValue (true, 0.5);
    knob.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (knob);
}

void HairyFuzzEditor::timerCallback()
{
    const bool on = ! footswitch.getToggleState();
    if (on != ledOn)
    {
        ledOn = on;
        repaint (ledArea.expanded (16.0f).toNearestInt());
    }
}

void HairyFuzzEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Colour (0xff2a2a2a));

    // Enclosure.
    auto box = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (box.translated (0.0f, 3.0f), 16.0f);
    g.setGradientFill (ColourGradient (enclosureTop, box.getX(), box.getY(),
                                       enclosureBottom, box.getX(), box.getBottom(), false));
    g.fillRoundedRectangle (box, 16.0f);
    g.setColour (Colour (0xff4a4a4a));
    g.drawRoundedRectangle (box.reduced (1.0f), 15.0f, 1.5f);

    const float inset = 20.0f;
    drawScrew (g, { box.getX() + inset, box.getY() + inset });
    drawScrew (g, { box.getRight() - inset, box.getY() + inset });
    drawScrew (g, { box.getX() + inset, box.getBottom() - inset });
    drawScrew (g, { box.getRight() - inset, box.getBottom() - inset });

    // Knob labels.
    g.setColour (silkscreen);
    g.setFont (boldFont (15.0f));
    const auto labelAbove = [&] (const Slider& k, const String& text)
    {
        g.drawText (text, k.getBounds().withY (k.getY() - 24).withHeight (20), Justification::centred);
    };
    labelAbove (fuzzKnob, "FUZZ");
    labelAbove (filterKnob, "FILTER");
    labelAbove (volumeKnob, "VOLUME");

    // LED.
    if (ledOn)
    {
        g.setGradientFill (ColourGradient (ledRed.withAlpha (0.55f), ledArea.getCentre(),
                                           ledRed.withAlpha (0.0f), ledArea.getCentre().translated (18.0f, 0.0f), true));
        g.fillEllipse (ledArea.expanded (12.0f));
    }
    g.setColour (Colour (0xff111111));
    g.fillEllipse (ledArea.expanded (2.5f));
    g.setGradientFill (ColourGradient (ledOn ? Colour (0xffffb0a0) : Colour (0xff5a1410),
                                       ledArea.getCentre().translated (-2.0f, -2.0f),
                                       ledOn ? ledRed : Colour (0xff2a0806),
                                       ledArea.getBottomRight(), true));
    g.fillEllipse (ledArea);

    // Title block.
    auto title = Rectangle<float> (box.getX(), 236.0f, box.getWidth(), 120.0f);
    g.setColour (silkscreen);
    g.setFont (boldFont (58.0f).withHorizontalScale (0.92f));
    g.drawText ("HAIRY", title.removeFromTop (58.0f), Justification::centred);
    g.setFont (boldFont (58.0f).withHorizontalScale (0.92f));
    g.drawText ("FUZZ", title.removeFromTop (58.0f), Justification::centred);

    g.setColour (silkscreen.withAlpha (0.6f));
    g.setFont (boldFont (11.0f));
    g.drawText ("-  SEVENTIES FUZZ BOX  -", Rectangle<float> (box.getX(), 356.0f, box.getWidth(), 16.0f),
                Justification::centred);
}

void HairyFuzzEditor::resized()
{
    const int gap = (editorWidth - 3 * knobSize) / 4;
    fuzzKnob.setBounds (gap, knobTop, knobSize, knobSize);
    filterKnob.setBounds (2 * gap + knobSize, knobTop, knobSize, knobSize);
    volumeKnob.setBounds (3 * gap + 2 * knobSize, knobTop, knobSize, knobSize);

    ledArea = { editorWidth * 0.5f - 7.0f, 196.0f, 14.0f, 14.0f };
    footswitch.setBounds (editorWidth / 2 - 42, 392, 84, 84);
}
