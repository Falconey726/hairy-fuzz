#pragma once

#include <cmath>
#include <algorithm>

// Small, dependency-free building blocks for the fuzz circuit model.
// Everything here runs at the oversampled rate.
namespace fuzz
{
constexpr float pi = 3.14159265358979f;

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

// Stage 1: a biased, lopsided transistor stage. The negative half clips
// earlier and harder than the positive half, which gives the even-order
// "hairy" grit and a bit of sputter as notes decay.
inline float asymmetricStage (float x, float bias)
{
    const float v = x + bias;
    const float y = v >= 0.0f ? std::tanh (v)
                              : -0.45f * std::tanh (-v / 0.45f);
    return y - std::tanh (bias);
}

// Stage 2: clipping-diode style limiter with a fairly hard knee,
// which squares the wave off into a thick, saturated fuzz.
inline float diodeStage (float x)
{
    constexpr float n = 2.5f;
    return x / std::pow (1.0f + std::pow (std::abs (x), n), 1.0f / n);
}

// One channel of the full fuzz circuit.
struct FuzzChannel
{
    OnePole inputCoupling, interstageLow, interstageCoupling, dcBlock;
    SVFLowpass toneFilter;
    float sampleRate = 44100.0f;

    void prepare (float fs)
    {
        sampleRate = fs;
        inputCoupling.setCutoff (35.0f, fs);       // keeps the low end big but not flabby
        interstageLow.setCutoff (5200.0f, fs);     // tames fizz between the stages
        interstageCoupling.setCutoff (70.0f, fs);  // coupling cap between the stages
        dcBlock.setCutoff (15.0f, fs);
        reset();
    }

    void reset()
    {
        inputCoupling.reset();
        interstageLow.reset();
        interstageCoupling.reset();
        dcBlock.reset();
        toneFilter.reset();
    }

    // gain1/gain2 are linear gains for the two stages, cutoffHz is the tone filter.
    float process (float x, float gain1, float gain2, float bias, float cutoffHz)
    {
        float y = inputCoupling.highpass (x);
        y = asymmetricStage (gain1 * y, bias);
        y = interstageLow.lowpass (y);
        y = interstageCoupling.highpass (y);
        y = diodeStage (gain2 * y);
        toneFilter.setCutoff (cutoffHz, 0.75f, sampleRate);
        y = toneFilter.process (y);
        return dcBlock.highpass (y);
    }
};
} // namespace fuzz
