#pragma once

#include <array>
#include <cmath>
#include <algorithm>

// Small, dependency-free building blocks for the fuzz circuit models.
// Everything here runs at the oversampled rate.
namespace fuzz
{
constexpr float pi = 3.14159265358979f;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// Topology-preserving one-pole filter (gives lowpass and highpass outputs).
struct OnePole
{
    float G = 0.0f, s = 0.0f;

    void setCutoff (float hz, float sampleRate)
    {
        const float g = std::tan (pi * std::min (hz, 0.49f * sampleRate) / sampleRate);
        G = g / (1.0f + g);
    }

    void reset() { s = 0.0f; }

    float lowpass (float x)
    {
        const float v = (x - s) * G;
        const float lp = v + s;
        s = lp + v;
        return lp;
    }

    float highpass (float x) { return x - lowpass (x); }
};

// Cytomic-style state variable lowpass, safe to modulate every sample.
struct SVFLowpass
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

    void setCutoff (float hz, float q, float sampleRate)
    {
        const float g = std::tan (pi * std::min (hz, 0.49f * sampleRate) / sampleRate);
        const float k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() { ic1 = ic2 = 0.0f; }

    float process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

// A biased, lopsided transistor stage. The negative half clips earlier and
// harder than the positive half, which gives even-order grit and sputter.
inline float asymmetricStage (float x, float bias, float negKnee)
{
    const auto shape = [negKnee] (float v)
    {
        return v >= 0.0f ? std::tanh (v) : -negKnee * std::tanh (-v / negKnee);
    };
    return shape (x + bias) - shape (bias);
}

// Clipping-diode style limiter. `level` is the clip ceiling, `knee` sets how
// hard the corner is (low = soft and round, high = square).
inline float diodeClip (float x, float level, float knee)
{
    const float u = x / level;
    return level * u / std::pow (1.0f + std::pow (std::abs (u), knee), 1.0f / knee);
}

// The two transistor/diode flavours.
struct Semiconductor
{
    float bias;       // operating point of the transistor stage
    float negKnee;    // how early the negative half gives up
    float clipLevel;  // diode forward voltage, relative
    float knee;       // diode corner hardness
    float smoothing;  // interstage lowpass, Hz
    float gainTrim;   // dB
};

constexpr Semiconductor germanium { 0.34f, 0.38f, 0.7f, 1.7f, 4200.0f, -2.0f };
constexpr Semiconductor silicon   { 0.12f, 0.65f, 1.0f, 3.5f, 6800.0f, 3.0f };

enum class Mode
{
    vintage = 0, // single fuzzy transistor pair, raw and splatty
    hairy,       // lopsided transistor into diode clipper (the original sound)
    sustain,     // two cascaded clipping stages plus a scooped tone stack
    octave,      // rectified fuzz, adds an octave up
    dist,        // op-amp gain into hard clipping diodes
    drive,       // soft, low-gain overdrive
    numModes
};

constexpr int numModes = (int) Mode::numModes;

// One mode + one semiconductor for one channel.
struct Voice
{
    Mode mode = Mode::hairy;
    Semiconductor semi = germanium;
    float sampleRate = 44100.0f;

    OnePole inHP, interLP, interHP, scoopLP, scoopHP, opampLP, dcBlock;
    SVFLowpass toneFilter;

    void prepare (float fs, Mode m, bool isSilicon)
    {
        sampleRate = fs;
        mode = m;
        semi = isSilicon ? silicon : germanium;

        float inputHz = 35.0f;
        switch (mode)
        {
            case Mode::vintage: inputHz = 25.0f; break;
            case Mode::hairy:   inputHz = 35.0f; break;
            case Mode::sustain: inputHz = 60.0f; break;
            case Mode::octave:  inputHz = 40.0f; break;
            case Mode::dist:    inputHz = 120.0f; break;
            case Mode::drive:   inputHz = 90.0f; break;
            default: break;
        }
        inHP.setCutoff (inputHz, fs);
        interLP.setCutoff (semi.smoothing, fs);
        interHP.setCutoff (mode == Mode::octave ? 30.0f : 70.0f, fs);
        scoopLP.setCutoff (400.0f, fs);
        scoopHP.setCutoff (1200.0f, fs);
        opampLP.setCutoff (20000.0f, fs);
        dcBlock.setCutoff (15.0f, fs);
        reset();
    }

    void reset()
    {
        for (auto* f : { &inHP, &interLP, &interHP, &scoopLP, &scoopHP, &opampLP, &dcBlock })
            f->reset();
        toneFilter.reset();
    }

    // drive is the 0..1 Fuzz knob, cutoffHz the tone filter.
    float process (float x, float drive, float cutoffHz)
    {
        const float trim = dbToGain (semi.gainTrim);
        float y = inHP.highpass (x);

        switch (mode)
        {
            case Mode::vintage:
                y = asymmetricStage (y * trim * dbToGain (10.0f + 40.0f * drive), semi.bias, semi.negKnee);
                y = interLP.lowpass (y);
                y = diodeClip (1.3f * y, semi.clipLevel, 1.4f);
                break;

            case Mode::hairy:
                y = asymmetricStage (y * trim * dbToGain (6.0f + 32.0f * drive), semi.bias, semi.negKnee);
                y = interLP.lowpass (y);
                y = interHP.highpass (y);
                y = diodeClip (y * dbToGain (3.0f + 22.0f * drive), semi.clipLevel, semi.knee);
                break;

            case Mode::sustain:
                y = diodeClip (y * trim * dbToGain (8.0f + 20.0f * drive), semi.clipLevel, semi.knee);
                y = interLP.lowpass (y);
                y = interHP.highpass (y);
                y = diodeClip (y * dbToGain (6.0f + 18.0f * drive), semi.clipLevel, semi.knee);
                y = 0.55f * scoopLP.lowpass (y) + 0.45f * scoopHP.highpass (y);
                y *= 1.6f;
                break;

            case Mode::octave:
                y = asymmetricStage (y * trim * dbToGain (8.0f + 24.0f * drive), semi.bias * 0.5f, 0.8f);
                y = interLP.lowpass (y);
                y = interHP.highpass (std::abs (y)); // full-wave rectifier = octave up
                y = diodeClip (y * dbToGain (6.0f + 16.0f * drive), semi.clipLevel, semi.knee);
                break;

            case Mode::dist:
            {
                const float gain = dbToGain (4.0f + 40.0f * drive) * trim;
                // An op-amp loses bandwidth as its gain goes up.
                opampLP.setCutoff (std::clamp (1.0e6f / gain, 3000.0f, 20000.0f), sampleRate);
                y = opampLP.lowpass (y * gain);
                y = diodeClip (y, semi.clipLevel, semi.knee + 1.5f);
                break;
            }

            case Mode::drive:
                y = std::tanh (y * trim * dbToGain (0.0f + 24.0f * drive) + 0.1f) - std::tanh (0.1f);
                y = interLP.lowpass (y);
                y *= 1.4f;
                break;

            default: break;
        }

        toneFilter.setCutoff (cutoffHz, 0.75f, sampleRate);
        y = toneFilter.process (y);
        return dcBlock.highpass (y);
    }
};

// One channel with every mode/semiconductor combination, so switching can
// crossfade between two fully independent circuits without clicks.
struct FuzzChannel
{
    static constexpr int numVoices = numModes * 2;
    std::array<Voice, numVoices> voices;

    static int voiceIndex (int mode, bool isSilicon) { return mode * 2 + (isSilicon ? 1 : 0); }

    void prepare (float fs)
    {
        for (int m = 0; m < numModes; ++m)
            for (int s = 0; s < 2; ++s)
                voices[(size_t) voiceIndex (m, s == 1)].prepare (fs, (Mode) m, s == 1);
    }

    void reset()
    {
        for (auto& v : voices)
            v.reset();
    }
};
} // namespace fuzz
