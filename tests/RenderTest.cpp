// Offline check: renders a plucked test note through the plugin to a WAV,
// prints level stats, and saves a PNG of the editor.
// Usage: HairyFuzzRenderTest <outDir>
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

static juce::AudioBuffer<float> makePluck (double sr, double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (sr * seconds));
    juce::Random rng (1);
    const double freqs[] = { 82.41, 123.47, 164.81 }; // E power chord
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        double s = 0;
        for (auto f : freqs)
            for (int h = 1; h <= 6; ++h)
                s += std::sin (juce::MathConstants<double>::twoPi * f * h * t) / (h * h);
        s *= 0.12 * std::exp (-t * 1.2);
        b.setSample (0, i, (float) s);
        b.setSample (1, i, (float) s);
    }
    return b;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const double sr = 48000.0;
    const int block = 256;

    struct Setting { const char* name; float fuzz, filter, volume; };
    const Setting settings[] = { { "default", 0.7f, 0.35f, 0.7f }, { "max", 1.0f, 0.0f, 0.7f },
                                 { "low", 0.0f, 0.5f, 0.7f }, { "dark", 0.8f, 1.0f, 0.7f } };

    for (auto& s : settings)
    {
        HairyFuzzProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.apvts.getParameter ("fuzz")->setValueNotifyingHost (s.fuzz);
        p.apvts.getParameter ("filter")->setValueNotifyingHost (s.filter);
        p.apvts.getParameter ("volume")->setValueNotifyingHost (s.volume);
        p.prepareToPlay (sr, block);

        auto audio = makePluck (sr, 3.0);
        const float inPeak = audio.getMagnitude (0, 0, audio.getNumSamples());
        juce::MidiBuffer midi;
        bool finite = true;
        for (int pos = 0; pos < audio.getNumSamples(); pos += block)
        {
            const int n = juce::jmin (block, audio.getNumSamples() - pos);
            juce::AudioBuffer<float> view (audio.getArrayOfWritePointers(), 2, pos, n);
            p.processBlock (view, midi);
        }
        for (int i = 0; i < audio.getNumSamples(); ++i)
            finite &= std::isfinite (audio.getSample (0, i));

        std::printf ("%-8s inPeak=%.3f outPeak=%.3f outRMS=%.3f latency=%d finite=%d\n", s.name, inPeak,
                     audio.getMagnitude (0, 0, audio.getNumSamples()), audio.getRMSLevel (0, 0, audio.getNumSamples()),
                     p.getLatencySamples(), (int) finite);

        juce::WavAudioFormat wav;
        auto file = outDir.getChildFile (juce::String ("render_") + s.name + ".wav");
        file.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    HairyFuzzProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    auto png = outDir.getChildFile ("editor.png");
    png.deleteFile();
    juce::FileOutputStream os (png);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", png.getFullPathName().toRawUTF8());
    return 0;
}
