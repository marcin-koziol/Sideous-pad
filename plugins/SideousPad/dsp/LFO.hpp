/*
 * Sideous Pad - free-running or tempo-synced LFO. Adapted from
 * sideous-noise's version (keeps the Gate/SampleHold waveforms - a slow
 * Gate is a nice pulsing-choir effect, S&H a glitchier alternative to the
 * smooth shapes), but the destination set is this instrument's own: pads
 * have a pitched fundamental (unlike sideous-noise), so Pitch comes back,
 * and a new Vowel destination lets the LFO sweep the formant morph knob for
 * a slow "talking pad" effect - see PadVoice.hpp and FormantFilter.hpp.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace sideous {

enum class LfoWaveform { Sine = 0, Saw, Square, Gate, SampleHold };
enum class LfoDestination { Pitch = 0, Cutoff, Amplitude, Vowel };

class LFO
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void setWaveform(LfoWaveform w) noexcept { fWaveform = w; }
    void setFrequency(float hz) noexcept { fIncrement = (double)hz / fSampleRate; }

    // 0.01..0.99, only used by the Gate waveform: fraction of each cycle
    // spent "on".
    void setWidth(float width) noexcept
    {
        fWidth = width < 0.01f ? 0.01f : (width > 0.99f ? 0.99f : width);
    }

    // per-voice decorrelation for SampleHold, same idea as Noise::setSeed()
    void setSeed(uint32_t seed) noexcept { fRngState = seed != 0 ? seed : 0x9e3779b9u; }

    // retrigger to a consistent starting phase, called on note-on
    void reset() noexcept { fPhase = 0.0; }

    // returns -1..1
    float process() noexcept
    {
        float out;
        switch (fWaveform)
        {
        case LfoWaveform::Saw:
            out = 2.0f * (float)fPhase - 1.0f;
            break;
        case LfoWaveform::Square:
            out = fPhase < 0.5 ? 1.0f : -1.0f;
            break;
        case LfoWaveform::Gate:
            out = fPhase < (double)fWidth ? 1.0f : -1.0f;
            break;
        case LfoWaveform::SampleHold:
            out = fHeldValue;
            break;
        case LfoWaveform::Sine:
        default:
            out = std::sin(2.0f * (float)M_PI * (float)fPhase);
            break;
        }

        fPhase += fIncrement;
        if (fPhase >= 1.0)
        {
            fPhase -= 1.0;
            fHeldValue = nextRandom();
        }

        return out;
    }

private:
    float nextRandom() noexcept
    {
        fRngState ^= fRngState << 13;
        fRngState ^= fRngState >> 17;
        fRngState ^= fRngState << 5;
        return ((float)(fRngState >> 8) / (float)(1u << 24)) * 2.0f - 1.0f;
    }

    double fSampleRate = 44100.0;
    double fPhase = 0.0;
    double fIncrement = 0.0;
    float fWidth = 0.5f;
    LfoWaveform fWaveform = LfoWaveform::Sine;

    uint32_t fRngState = 0x9e3779b9u;
    float fHeldValue = 0.0f;
};

} // namespace sideous
