/*
 * Sideous Pad - vowel formant filter bank, the "choir" half of this
 * instrument (see UnisonOscillator.hpp for the "pad" half). Three parallel
 * resonant bandpasses (reusing Filter.hpp's SVF in Bandpass mode) at F1/F2/F3
 * with per-formant gain, continuously morphed across an ordered vowel table
 * by a single 0..4 Vowel knob (Ah -> Eh -> Ee -> Oh -> Oo), so the LFO or mod
 * wheel can sweep through it smoothly without a discrete mode switch.
 *
 * Formant frequencies/gains are approximate literature values for spoken/
 * sung vowels, not tuned by ear against real audio yet - see
 * sideous-pad-plan.md section 9. Mono only; PadVoice.hpp runs two instances
 * (Left/Right) for the stereo-width unison signal rather than teaching this
 * class about stereo itself.
 *
 * setResonance()'s 0..1 knob is remapped to a narrower, higher-Q slice of
 * Filter.hpp's own resonance curve than the general filter uses (real vocal
 * formants are much sharper peaks than a musically-useful lowpass
 * resonance), plus a small makeup gain since a narrower band passes less of
 * a harmonic-rich source's energy - without both of these the vowel morph
 * is barely audible even at extreme Vowel knob settings.
 *
 * Keep the makeup gain modest: three parallel bandpasses already sum to
 * more energy than the dry input on their own (each captures a different
 * slice of the harmonic series), and this stage runs per-voice under an
 * 8-voice polyphonic mix - stacking a large multiplier on top of that,
 * across a full chord, blew well past 0dBFS into harsh, audibly "screaming"
 * clipping at the master limiter (measured ~2x full scale on just a 4-note
 * chord at defaults before this was dialed back - see git history for the
 * numbers). Re-check with a multi-voice chord (not a single sustained note)
 * before raising either constant again.
 */

#pragma once

#include <algorithm>
#include <cmath>

#include "Filter.hpp"

namespace sideous {

class FormantFilter
{
public:
    // ordered so the Vowel knob sweeps through a natural mouth-shape path
    // rather than an arbitrary table order
    enum class VowelName { Ah = 0, Eh, Ee, Oh, Oo, kCount };

    void setSampleRate(double sampleRate) noexcept
    {
        for (Filter& band : fBands)
        {
            band.setSampleRate(sampleRate);
            band.setType(FilterType::Bandpass); // fixed for this class's lifetime
        }
    }

    // shared Q across all three bands, 0..1. Remapped into a narrower,
    // consistently-resonant slice of Filter's own 0..1 curve (roughly Q 1.5
    // at knob=0 up to Q 4.5 at knob=1 - deliberately kept well short of
    // self-oscillation, both for stability and headroom) plus a modest
    // makeup gain - see file header for why both are kept conservative.
    void setResonance(float res01) noexcept
    {
        res01 = std::clamp(res01, 0.0f, 1.0f);
        const float sharpened = 0.55f + res01 * 0.3f;
        for (Filter& band : fBands)
            band.setResonance(sharpened);
        fMakeupGain = 0.35f + res01 * 0.5f;
    }

    // 0..4, continuous - interpolates between adjacent rows of kTable
    void setVowel(float vowel) noexcept
    {
        constexpr float kMax = (float)(int)VowelName::kCount - 1.0f;
        vowel = std::clamp(vowel, 0.0f, kMax);

        const int i0 = std::min((int)vowel, (int)VowelName::kCount - 2);
        const int i1 = i0 + 1;
        const float frac = vowel - (float)i0;

        for (int b = 0; b < 3; ++b)
        {
            fFreq[b] = kTable[i0].freq[b] + (kTable[i1].freq[b] - kTable[i0].freq[b]) * frac;
            fGain[b] = kTable[i0].gain[b] + (kTable[i1].gain[b] - kTable[i0].gain[b]) * frac;
        }
    }

    float process(float in) noexcept
    {
        float out = 0.0f;
        for (int b = 0; b < 3; ++b)
            out += fBands[b].process(in, fFreq[b]) * fGain[b];
        return out * fMakeupGain;
    }

private:
    struct VowelRow { float freq[3]; float gain[3]; };

    // F1, F2, F3 (Hz) and relative per-formant gain, approximate values for
    // a sung/spoken vowel chart - see file header. F2 gains raised relative
    // to the original literature-loudness ratios: F2 is the formant that
    // actually carries most of the front/back vowel identity, and burying
    // it under F1 (as raw formant amplitude charts do) made the Vowel knob
    // sweep read as "duller/brighter" rather than a real vowel-color change.
    static constexpr VowelRow kTable[(int)VowelName::kCount] = {
        /* Ah */ { { 800.0f, 1150.0f, 2900.0f }, { 1.00f, 0.70f, 0.30f } },
        /* Eh */ { { 550.0f, 1770.0f, 2490.0f }, { 0.90f, 0.85f, 0.35f } },
        /* Ee */ { { 270.0f, 2290.0f, 3010.0f }, { 0.60f, 1.00f, 0.40f } },
        /* Oh */ { { 450.0f,  800.0f, 2830.0f }, { 1.00f, 0.60f, 0.25f } },
        /* Oo */ { { 300.0f,  870.0f, 2240.0f }, { 1.00f, 0.50f, 0.20f } },
    };

    Filter fBands[3];
    float fFreq[3] { 800.0f, 1150.0f, 2900.0f };
    float fGain[3] { 1.00f, 0.70f, 0.30f };
    float fMakeupGain = 0.6f;
};

} // namespace sideous
