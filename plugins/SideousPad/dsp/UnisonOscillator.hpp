/*
 * Sideous Pad - unison stack of detuned chip oscillators, the "pad" half of
 * this instrument (see FormantFilter.hpp for the "choir" half). Up to
 * kMaxVoices Oscillator.hpp instances per note, spread across a detune
 * (cents) and stereo pan, each with its own slow, independent pitch Drift -
 * a smoothed random wander (Noise.hpp low-passed with a one-pole smoother)
 * so the voices don't all breathe in lockstep, which is what makes a unison
 * stack sound like several living voices instead of one fat static one.
 */

#pragma once

#include <algorithm>
#include <cmath>

#include "Oscillator.hpp"
#include "Noise.hpp"

namespace sideous {

class UnisonOscillator
{
public:
    static constexpr int kMaxVoices = 5;
    // how far the Drift knob (0..1) can push pitch, in cents, at full depth
    static constexpr float kMaxDriftCents = 8.0f;

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        for (int i = 0; i < kMaxVoices; ++i)
            fOsc[i].setSampleRate(fSampleRate);
        // ~2.5s time constant - slow enough to read as "drift", not vibrato
        fDriftCoeff = 1.0f - std::exp(-1.0f / (2.5f * (float)fSampleRate));
    }

    // distinct per-note seed so every unison voice in every polyphonic note
    // drifts independently, same idea as sideous-noise's per-voice noise seed
    void setSeed(uint32_t seed) noexcept
    {
        for (int i = 0; i < kMaxVoices; ++i)
            fDrift[i].setSeed(seed + (uint32_t)i * 747796405u + 2891336453u);
    }

    void setWaveform(Waveform w) noexcept
    {
        for (int i = 0; i < kMaxVoices; ++i)
            fOsc[i].setWaveform(w);
    }

    void setPulseWidth(float pw) noexcept
    {
        for (int i = 0; i < kMaxVoices; ++i)
            fOsc[i].setPulseWidth(pw);
    }

    void setVoiceCount(int n) noexcept { fVoiceCount = std::clamp(n, 1, kMaxVoices); }
    void setDetuneCents(float cents) noexcept { fDetuneCents = std::max(0.0f, cents); }
    void setStereoWidth(float width01) noexcept { fStereoWidth = std::clamp(width01, 0.0f, 1.0f); }
    void setDriftDepth(float depth01) noexcept { fDriftDepth = std::clamp(depth01, 0.0f, 1.0f); }
    void setFrequency(float hz) noexcept { fBaseFreq = hz; }

    void resetPhases() noexcept
    {
        for (int i = 0; i < kMaxVoices; ++i)
            fOsc[i].resetPhase((float)i / (float)kMaxVoices); // spread start phases, avoids an initial mono-summed transient
    }

    void process(float& outL, float& outR) noexcept
    {
        if (fVoiceCount <= 1)
        {
            fOsc[0].setFrequency(fBaseFreq);
            const float s = fOsc[0].process();
            outL = outR = s;
            return;
        }

        outL = outR = 0.0f;
        // equal-power-ish normalization so more unison voices doesn't just mean louder
        const float gain = 1.0f / std::sqrt((float)fVoiceCount);

        for (int i = 0; i < fVoiceCount; ++i)
        {
            const float posN = (float)i / (float)(fVoiceCount - 1) * 2.0f - 1.0f; // -1..1 spread position

            fDriftSmooth[i] += (fDrift[i].process() - fDriftSmooth[i]) * fDriftCoeff;
            const float driftCents = fDriftSmooth[i] * fDriftDepth * kMaxDriftCents;
            const float detuneCents = posN * fDetuneCents * 0.5f + driftCents;

            fOsc[i].setFrequency(fBaseFreq * std::exp2(detuneCents / 1200.0f));
            const float s = fOsc[i].process() * gain;

            const float panPos = posN * fStereoWidth; // -1..1
            const float angle = (panPos * 0.5f + 0.5f) * (float)M_PI_2; // equal-power pan, 0..pi/2
            outL += s * std::cos(angle);
            outR += s * std::sin(angle);
        }
    }

private:
    Oscillator fOsc[kMaxVoices];
    Noise fDrift[kMaxVoices];
    float fDriftSmooth[kMaxVoices] { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float fDriftCoeff = 0.001f;

    double fSampleRate = 44100.0;
    float fBaseFreq = 440.0f;
    int fVoiceCount = 3;
    float fDetuneCents = 12.0f;
    float fStereoWidth = 0.6f;
    float fDriftDepth = 0.3f;
};

} // namespace sideous
