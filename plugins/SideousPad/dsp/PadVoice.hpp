/*
 * Sideous Pad - a single voice: UnisonOscillator -> FormantFilter (gated by
 * VoiceMode) -> general switchable Filter/LadderFilter -> amp ADSR, with a
 * shared LFO modulating Pitch/Cutoff/Amplitude/Vowel. Modeled on sideous's
 * Voice.hpp / sideous-noise's NoiseVoice.hpp, but stereo end-to-end (returns
 * outL/outR instead of a single float) since unison stereo width has to
 * survive the formant/filter stages to be audible - see
 * sideous-pad-plan.md's UnisonOscillator notes. No portamento/glide: this is
 * a purely polyphonic pad, not a mono lead, so it wasn't in scope.
 */

#pragma once

#include <algorithm>
#include <cmath>

#include "UnisonOscillator.hpp"
#include "FormantFilter.hpp"
#include "Filter.hpp"
#include "LadderFilter.hpp"
#include "ADSR.hpp"
#include "LFO.hpp"

namespace sideous {

enum class VoiceMode { Unison = 0, Formant, Both };

// mod wheel (MIDI CC1) is a separate, independent modulation source from the
// LFO - always targets whichever of these is picked, regardless of the
// LFO's own Destination.
enum class ModWheelDestination { Off = 0, Vibrato, Cutoff, Volume, Vowel };

struct PadVoiceParams
{
    VoiceMode voiceMode = VoiceMode::Both;

    Waveform unisonWaveform = Waveform::Saw;
    float unisonPulseWidth = 0.5f;
    int unisonVoices = 3;
    float unisonDetune = 12.0f;      // cents, symmetric spread
    float unisonStereoWidth = 0.6f;  // 0..1
    float unisonDrift = 0.3f;        // 0..1

    float vowel = 0.0f;              // 0..4, Ah..Oo
    float formantAmount = 0.85f;     // 0..1, wet/dry in Both mode
    float formantResonance = 0.7f;   // 0..1

    FilterMode filterMode = FilterMode::LP12;
    float filterCutoff = 4000.0f;    // Hz
    float filterResonance = 0.15f;
    float filterDrive = 0.0f;

    float ampAttack = 0.3f, ampDecay = 0.4f, ampSustain = 0.8f, ampRelease = 0.8f;
    float ampCurve = 0.35f;
    float velocitySensitivity = 0.6f;

    LfoWaveform lfoWaveform = LfoWaveform::Sine;
    float lfoWidth = 0.5f;
    LfoDestination lfoDestination = LfoDestination::Vowel;
    float lfoAmount = 0.0f;

    ModWheelDestination modWheelDestination = ModWheelDestination::Off;
    float modWheelAmount = 0.5f;
};

class PadVoice
{
public:
    // must match Params.hpp's kParamFilterCutoff range
    static constexpr float kCutoffMin = 20.0f;
    static constexpr float kCutoffMax = 20000.0f;
    static constexpr float kVibratoMaxSemitones = 2.0f;
    // how far the LFO/mod-wheel can push the Vowel knob, in table-index units
    static constexpr float kVowelModRange = 2.0f;

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fUnison.setSampleRate(sampleRate);
        fFormantL.setSampleRate(sampleRate);
        fFormantR.setSampleRate(sampleRate);
        fFilterAL.setSampleRate(sampleRate);
        fFilterAR.setSampleRate(sampleRate);
        fFilterBL.setSampleRate(sampleRate);
        fFilterBR.setSampleRate(sampleRate);
        fLadderL.setSampleRate(sampleRate);
        fLadderR.setSampleRate(sampleRate);
        fAmpEnv.setSampleRate(sampleRate);
        fLfo.setSampleRate(sampleRate);
    }

    // distinct per-voice seed, so a chord's unison drift doesn't move in
    // lockstep across notes (same idea as sideous-noise's per-voice seeding)
    void setSeed(uint32_t seed) noexcept
    {
        fUnison.setSeed(seed);
        fLfo.setSeed(seed ^ 0xa5a5a5a5u);
    }

    void noteOn(int note, float velocity) noexcept
    {
        fNote = note;
        fVelocity = velocity;
        fCurrentFreq = noteToHz(note);
        fUnison.resetPhases();
        fAmpEnv.noteOn();
        fLfo.reset();
        fActive = true;
    }

    void noteOff() noexcept { fAmpEnv.noteOff(); }
    void kill() noexcept { fActive = false; }

    bool isActive() const noexcept { return fActive && !fAmpEnv.isIdle(); }
    bool isReleasing() const noexcept { return fAmpEnv.getStage() == ADSR::Stage::Release; }
    int getNote() const noexcept { return fNote; }

    void setModWheel(float value01) noexcept { fModWheel = value01; }
    void setPitchBend(float semitones) noexcept { fPitchBendSemitones = semitones; }

    void applyParams(const PadVoiceParams& p) noexcept
    {
        fVoiceMode = p.voiceMode;

        fUnison.setWaveform(p.unisonWaveform);
        fUnison.setPulseWidth(p.unisonPulseWidth);
        fUnison.setVoiceCount(p.unisonVoices);
        fUnison.setDetuneCents(p.unisonDetune);
        fUnison.setStereoWidth(p.unisonStereoWidth);
        fUnison.setDriftDepth(p.unisonDrift);

        fVowel = p.vowel;
        fFormantAmount = p.formantAmount;
        fFormantL.setResonance(p.formantResonance);
        fFormantR.setResonance(p.formantResonance);

        fFilterMode = p.filterMode;
        const FilterType stageType = p.filterMode == FilterMode::HP12 || p.filterMode == FilterMode::HP24
                                    ? FilterType::Highpass
                                    : p.filterMode == FilterMode::BP12
                                    ? FilterType::Bandpass
                                    : FilterType::Lowpass;
        fFilterAL.setType(stageType); fFilterAR.setType(stageType);
        fFilterBL.setType(stageType); fFilterBR.setType(stageType);
        fFilterAL.setResonance(p.filterResonance); fFilterAR.setResonance(p.filterResonance);
        // only the first stage carries the resonant peak in 24dB modes -
        // cascading two resonant stages compounds gain quadratically, see
        // sideous's Voice.hpp for the same rule
        fFilterBL.setResonance(0.0f); fFilterBR.setResonance(0.0f);
        fFilterAL.setDrive(p.filterDrive); fFilterAR.setDrive(p.filterDrive);
        fLadderL.setResonance(p.filterResonance); fLadderR.setResonance(p.filterResonance);
        fLadderL.setDrive(p.filterDrive); fLadderR.setDrive(p.filterDrive);
        fBaseCutoff = p.filterCutoff;

        fAmpEnv.setAttack(p.ampAttack);
        fAmpEnv.setDecay(p.ampDecay);
        fAmpEnv.setSustain(p.ampSustain);
        fAmpEnv.setRelease(p.ampRelease);
        fAmpEnv.setCurve(p.ampCurve);
        fVelocitySensitivity = p.velocitySensitivity;

        fLfo.setWaveform(p.lfoWaveform);
        fLfo.setWidth(p.lfoWidth);
        fLfoDestination = p.lfoDestination;
        fLfoAmount = p.lfoAmount;

        fModWheelDestination = p.modWheelDestination;
        fModWheelAmount = p.modWheelAmount;
    }

    // pushed every audio block from the host tempo (or the free-rate knob)
    void setLfoFrequency(float hz) noexcept { fLfo.setFrequency(hz); }

    void process(float& outL, float& outR) noexcept
    {
        if (!fActive)
        {
            outL = outR = 0.0f;
            return;
        }

        const float lfo = fLfo.process(); // -1..1, always advance so it stays click-free if destination changes mid-note

        const float vibratoSemitones = fModWheelDestination == ModWheelDestination::Vibrato
                                      ? fModWheel * fModWheelAmount * kVibratoMaxSemitones : 0.0f;
        const float lfoPitchSemitones = (fLfoDestination == LfoDestination::Pitch ? fLfoAmount : 0.0f) * 12.0f;
        const float pitchSemitones = lfo * (lfoPitchSemitones + vibratoSemitones) + fPitchBendSemitones;
        fUnison.setFrequency(fCurrentFreq * std::exp2(pitchSemitones / 12.0f));

        float uL, uR;
        fUnison.process(uL, uR);

        float vowel = fVowel;
        if (fLfoDestination == LfoDestination::Vowel)
            vowel += lfo * fLfoAmount * kVowelModRange;
        if (fModWheelDestination == ModWheelDestination::Vowel)
            vowel += fModWheel * fModWheelAmount * kVowelModRange;
        vowel = std::clamp(vowel, 0.0f, (float)(int)FormantFilter::VowelName::kCount - 1.0f);
        fFormantL.setVowel(vowel);
        fFormantR.setVowel(vowel);

        float preFilterL = uL, preFilterR = uR;
        if (fVoiceMode != VoiceMode::Unison)
        {
            const float formantL = fFormantL.process(uL);
            const float formantR = fFormantR.process(uR);
            if (fVoiceMode == VoiceMode::Formant)
            {
                preFilterL = formantL;
                preFilterR = formantR;
            }
            else // Both: crossfade dry unison sum against the wet formant signal
            {
                preFilterL = uL + (formantL - uL) * fFormantAmount;
                preFilterR = uR + (formantR - uR) * fFormantAmount;
            }
        }

        float t = cutoffToNormalized(fBaseCutoff);
        if (fLfoDestination == LfoDestination::Cutoff)
            t += fLfoAmount * lfo;
        if (fModWheelDestination == ModWheelDestination::Cutoff)
            t += fModWheel * fModWheelAmount;
        t = std::clamp(t, 0.0f, 1.0f);
        const float cutoff = normalizedToCutoff(t);

        float filteredL, filteredR;
        switch (fFilterMode)
        {
        case FilterMode::LP24:
        case FilterMode::HP24:
            filteredL = fFilterBL.process(fFilterAL.process(preFilterL, cutoff), cutoff);
            filteredR = fFilterBR.process(fFilterAR.process(preFilterR, cutoff), cutoff);
            break;
        case FilterMode::Ladder24:
            filteredL = fLadderL.process(preFilterL, cutoff);
            filteredR = fLadderR.process(preFilterR, cutoff);
            break;
        case FilterMode::LP12:
        case FilterMode::HP12:
        case FilterMode::BP12:
        default:
            filteredL = fFilterAL.process(preFilterL, cutoff);
            filteredR = fFilterAR.process(preFilterR, cutoff);
            break;
        }

        const float ampLevel = fAmpEnv.process();
        if (fAmpEnv.isIdle())
        {
            fActive = false;
            outL = outR = 0.0f;
            return;
        }

        float ampMod = 1.0f;
        if (fLfoDestination == LfoDestination::Amplitude)
            ampMod = 1.0f - fLfoAmount * (1.0f - (lfo * 0.5f + 0.5f));
        if (fModWheelDestination == ModWheelDestination::Volume)
            ampMod *= (1.0f + fModWheel * fModWheelAmount);

        const float velocityGain = 1.0f - fVelocitySensitivity * (1.0f - fVelocity);
        const float g = ampLevel * velocityGain * ampMod;
        outL = filteredL * g;
        outR = filteredR * g;
    }

private:
    static float noteToHz(int note) noexcept
    {
        return 440.0f * std::exp2(((float)note - 69.0f) / 12.0f);
    }

    static float cutoffToNormalized(float hz) noexcept
    {
        return std::log(hz / kCutoffMin) / std::log(kCutoffMax / kCutoffMin);
    }

    static float normalizedToCutoff(float t) noexcept
    {
        return kCutoffMin * std::pow(kCutoffMax / kCutoffMin, t);
    }

    UnisonOscillator fUnison;
    FormantFilter fFormantL, fFormantR;
    Filter fFilterAL, fFilterAR;
    Filter fFilterBL, fFilterBR;     // only used for the 24dB cascaded modes
    LadderFilter fLadderL, fLadderR; // only used for the ladder mode
    ADSR fAmpEnv;
    LFO fLfo;

    VoiceMode fVoiceMode = VoiceMode::Both;
    float fVowel = 0.0f;
    float fFormantAmount = 0.85f;

    FilterMode fFilterMode = FilterMode::LP12;
    float fBaseCutoff = 4000.0f;
    float fVelocitySensitivity = 0.6f;

    double fSampleRate = 44100.0;
    float fCurrentFreq = 440.0f;

    LfoDestination fLfoDestination = LfoDestination::Vowel;
    float fLfoAmount = 0.0f;

    ModWheelDestination fModWheelDestination = ModWheelDestination::Off;
    float fModWheel = 0.0f;
    float fModWheelAmount = 0.5f;
    float fPitchBendSemitones = 0.0f;

    int fNote = -1;
    float fVelocity = 0.0f;
    bool fActive = false;
};

} // namespace sideous
