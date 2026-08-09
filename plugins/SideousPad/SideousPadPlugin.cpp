/*
 * Sideous Pad - retro chiptune choir/pad instrument. Each voice is a stack
 * of detuned chip oscillators (UnisonOscillator, the "pad" half) optionally
 * run through a vowel formant filter bank (FormantFilter, the "choir" half),
 * selectable via Voice Mode (Unison / Formant / Both). See
 * ../../../sideous-pad-plan.md for the full design rationale.
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "dsp/PadVoice.hpp"
#include "dsp/Sync.hpp"

#include <array>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace sideous;

// -----------------------------------------------------------------------------------------------------------

// pads are chord instruments, not 16-note runs, and this is the most
// expensive per-voice engine in the family so far (unison x formant x
// stereo) - see sideous-pad-plan.md section 7
static constexpr const uint32_t kNumVoices = 8;

// LFO "Sync" dropdown: 0 = free-running (use the Rate Hz knob),
// 1..14 = SyncDivision::Whole..ThirtySecond
static ParameterEnumerationValue kSyncEnumValues[15] = {
    {  0.0f, "Free" },
    {  1.0f, "1/1" },
    {  2.0f, "1/2." },
    {  3.0f, "1/2" },
    {  4.0f, "1/2T" },
    {  5.0f, "1/4." },
    {  6.0f, "1/4" },
    {  7.0f, "1/4T" },
    {  8.0f, "1/8." },
    {  9.0f, "1/8" },
    { 10.0f, "1/8T" },
    { 11.0f, "1/16." },
    { 12.0f, "1/16" },
    { 13.0f, "1/16T" },
    { 14.0f, "1/32" },
};

static float syncedHz(float syncParam, float freeHz, double bpm) noexcept
{
    const int sync = (int)(syncParam + 0.5f);
    if (sync <= 0)
        return freeHz;
    return hzFromSyncDivision((SyncDivision)(sync - 1), bpm);
}

class SideousPadPlugin : public Plugin
{
public:
    SideousPadPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        // distinct per-voice seed so each note's unison stack drifts
        // independently instead of every chord tone breathing in lockstep
        for (uint32_t i = 0; i < kNumVoices; ++i)
            fVoices[i].setSeed(0x9e3779b9u * (i + 1));

        sampleRateChanged(getSampleRate());
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "Sideous Pad"; }
    const char* getDescription() const override
    {
        return "Retro chiptune choir/pad instrument: a stack of detuned chip oscillators "
               "(unison, for width and movement) optionally run through a vowel formant "
               "filter bank (for a sung-choir character), selectable per Voice Mode "
               "(Unison / Formant / Both). Switchable general filter, amp ADSR, a free/"
               "tempo-synced LFO routable to pitch/cutoff/amplitude/vowel, and mod wheel/"
               "pitch-bend performance controls.";
    }
    const char* getMaker() const override { return "Sideous"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    // ---------------------------------------------------------------------
    // Init

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const ParamInfo& info = getParamInfo(index);

        parameter.hints = kParameterIsAutomatable;
        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;

        if (info.shape == ParamShape::Logarithmic)
            parameter.hints |= kParameterIsLogarithmic;

        switch (index)
        {
        case kParamVoiceMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[3] = {
                    { 0.0f, "Unison" },
                    { 1.0f, "Formant" },
                    { 2.0f, "Both" },
                };
                parameter.enumValues.count = 3;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamUnisonWaveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[3] = {
                    { 0.0f, "Saw" },
                    { 1.0f, "Pulse" },
                    { 2.0f, "Triangle" },
                };
                parameter.enumValues.count = 3;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamUnisonVoices:
            parameter.hints |= kParameterIsInteger;
            break;

        case kParamFilterMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[6] = {
                    { 0.0f, "LP 12dB" },
                    { 1.0f, "LP 24dB" },
                    { 2.0f, "HP 12dB" },
                    { 3.0f, "HP 24dB" },
                    { 4.0f, "BP 12dB" },
                    { 5.0f, "Ladder 24dB" },
                };
                parameter.enumValues.count = 6;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfoWaveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[5] = {
                    { 0.0f, "Sine" },
                    { 1.0f, "Saw" },
                    { 2.0f, "Square" },
                    { 3.0f, "Gate" },
                    { 4.0f, "S&H" },
                };
                parameter.enumValues.count = 5;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfoDestination:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { 0.0f, "Pitch" },
                    { 1.0f, "Cutoff" },
                    { 2.0f, "Amplitude" },
                    { 3.0f, "Vowel" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfoSync:
            parameter.hints |= kParameterIsInteger;
            parameter.enumValues.count = 15;
            parameter.enumValues.restrictedMode = true;
            parameter.enumValues.values = kSyncEnumValues;
            parameter.enumValues.deleteLater = false;
            break;

        case kParamPitchBendRange:
            parameter.hints |= kParameterIsInteger;
            break;

        case kParamModWheelDestination:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[5] = {
                    { 0.0f, "Off" },
                    { 1.0f, "Vibrato" },
                    { 2.0f, "Cutoff" },
                    { 3.0f, "Volume" },
                    { 4.0f, "Vowel" },
                };
                parameter.enumValues.count = 5;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        default:
            break;
        }
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParamVoiceMode:            return (float)fParams.voiceMode;
        case kParamUnisonWaveform:       return (float)fParams.unisonWaveform;
        case kParamUnisonPulseWidth:     return fParams.unisonPulseWidth;
        case kParamUnisonVoices:         return (float)fParams.unisonVoices;
        case kParamUnisonDetune:         return fParams.unisonDetune;
        case kParamUnisonStereoWidth:    return fParams.unisonStereoWidth;
        case kParamUnisonDrift:          return fParams.unisonDrift;
        case kParamVowel:                return fParams.vowel;
        case kParamFormantAmount:        return fParams.formantAmount;
        case kParamFormantResonance:     return fParams.formantResonance;
        case kParamFilterMode:           return (float)fParams.filterMode;
        case kParamFilterCutoff:         return fParams.filterCutoff;
        case kParamFilterResonance:      return fParams.filterResonance;
        case kParamFilterDrive:          return fParams.filterDrive;
        case kParamAmpAttack:            return fParams.ampAttack;
        case kParamAmpDecay:             return fParams.ampDecay;
        case kParamAmpSustain:           return fParams.ampSustain;
        case kParamAmpRelease:           return fParams.ampRelease;
        case kParamAmpCurve:             return fParams.ampCurve;
        case kParamVelocitySensitivity:  return fParams.velocitySensitivity;
        case kParamLfoWaveform:          return (float)fParams.lfoWaveform;
        case kParamLfoWidth:             return fParams.lfoWidth * 100.0f;
        case kParamLfoDestination:       return (float)fParams.lfoDestination;
        case kParamLfoRateHz:            return fLfoRateHz;
        case kParamLfoSync:              return fLfoSync;
        case kParamLfoAmount:            return fParams.lfoAmount;
        case kParamPitchBendRange:       return fPitchBendRangeSemitones;
        case kParamModWheelDestination:  return (float)fParams.modWheelDestination;
        case kParamModWheelAmount:       return fParams.modWheelAmount;
        case kParamMasterVolume:         return fMasterVolume;
        case kParamMasterDrive:          return fMasterDrive;
        default:                         return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamVoiceMode:         fParams.voiceMode = (sideous::VoiceMode)(int)(value + 0.5f); break;
        case kParamUnisonWaveform:    fParams.unisonWaveform = (sideous::Waveform)(int)(value + 0.5f); break;
        case kParamUnisonPulseWidth:  fParams.unisonPulseWidth = value; break;
        case kParamUnisonVoices:      fParams.unisonVoices = (int)(value + 0.5f); break;
        case kParamUnisonDetune:      fParams.unisonDetune = value; break;
        case kParamUnisonStereoWidth: fParams.unisonStereoWidth = value; break;
        case kParamUnisonDrift:       fParams.unisonDrift = value; break;
        case kParamVowel:             fParams.vowel = value; break;
        case kParamFormantAmount:     fParams.formantAmount = value; break;
        case kParamFormantResonance:  fParams.formantResonance = value; break;
        case kParamFilterMode:        fParams.filterMode = (sideous::FilterMode)(int)(value + 0.5f); break;
        case kParamFilterCutoff:      fParams.filterCutoff = value; break;
        case kParamFilterResonance:   fParams.filterResonance = value; break;
        case kParamFilterDrive:       fParams.filterDrive = value; break;
        case kParamAmpAttack:         fParams.ampAttack = value; break;
        case kParamAmpDecay:          fParams.ampDecay = value; break;
        case kParamAmpSustain:        fParams.ampSustain = value; break;
        case kParamAmpRelease:        fParams.ampRelease = value; break;
        case kParamAmpCurve:          fParams.ampCurve = value; break;
        case kParamVelocitySensitivity: fParams.velocitySensitivity = value; break;
        case kParamLfoWaveform:       fParams.lfoWaveform = (sideous::LfoWaveform)(int)(value + 0.5f); break;
        case kParamLfoWidth:          fParams.lfoWidth = value * 0.01f; break;
        case kParamLfoDestination:    fParams.lfoDestination = (sideous::LfoDestination)(int)(value + 0.5f); break;
        case kParamLfoRateHz:         fLfoRateHz = value; break;
        case kParamLfoSync:           fLfoSync = value; break;
        case kParamLfoAmount:         fParams.lfoAmount = value; break;
        case kParamPitchBendRange:    fPitchBendRangeSemitones = value; break;
        case kParamModWheelDestination:
            fParams.modWheelDestination = (sideous::ModWheelDestination)(int)(value + 0.5f);
            break;
        case kParamModWheelAmount:    fParams.modWheelAmount = value; break;
        case kParamMasterVolume:      fMasterVolume = value; break;
        case kParamMasterDrive:       fMasterDrive = value; break;
        default: return;
        }

        for (sideous::PadVoice& voice : fVoices)
            voice.applyParams(fParams);
    }

    // ---------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        sampleRateChanged(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        for (sideous::PadVoice& voice : fVoices)
        {
            voice.setSampleRate(newSampleRate);
            voice.applyParams(fParams);
        }
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        float* outL = outputs[0];
        float* outR = outputs[1];

        const TimePosition& timePos = getTimePosition();
        const double bpm = timePos.bbt.valid && timePos.bbt.beatsPerMinute > 0.0
                          ? timePos.bbt.beatsPerMinute : 120.0;

        const float lfoHz = syncedHz(fLfoSync, fLfoRateHz, bpm);
        for (sideous::PadVoice& voice : fVoices)
            voice.setLfoFrequency(lfoHz);

        uint32_t nextEvent = 0;

        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            bool controllersChanged = false;
            while (nextEvent < midiEventCount && midiEvents[nextEvent].frame == frame)
            {
                const uint8_t statusBefore = midiEvents[nextEvent].data[0] & 0xF0;
                if (statusBefore == 0xE0 || statusBefore == 0xB0)
                    controllersChanged = true;
                handleMidiEvent(midiEvents[nextEvent]);
                ++nextEvent;
            }

            if (controllersChanged)
            {
                const float pitchBendSemitones = fPitchBendNormalized * fPitchBendRangeSemitones;
                for (sideous::PadVoice& voice : fVoices)
                {
                    voice.setPitchBend(pitchBendSemitones);
                    voice.setModWheel(fModWheelValue);
                }
            }

            float mixL = 0.0f, mixR = 0.0f;
            for (sideous::PadVoice& voice : fVoices)
            {
                float vL, vR;
                voice.process(vL, vR);
                mixL += vL;
                mixR += vR;
            }

            mixL *= fMasterVolume * kVoiceHeadroom;
            mixR *= fMasterVolume * kVoiceHeadroom;

            // Drive: character/vibe saturation, not just a safety limiter -
            // same tanh-crossfade trick as sideous's Filter.hpp
            if (fMasterDrive > 0.0f)
            {
                const float driveGain = 1.0f + fMasterDrive * 7.0f;
                mixL = mixL + fMasterDrive * (std::tanh(mixL * driveGain) - mixL);
                mixR = mixR + fMasterDrive * (std::tanh(mixR * driveGain) - mixR);
            }

            // gentle safety saturation: a full 8-voice unison+formant chord
            // can otherwise stack well past 0dBFS
            outL[frame] = std::tanh(mixL);
            outR[frame] = std::tanh(mixR);
        }

        while (nextEvent < midiEventCount)
            handleMidiEvent(midiEvents[nextEvent++]);
    }

private:
    static constexpr const float kVoiceHeadroom = 1.0f / 3.0f;

    void handleMidiEvent(const MidiEvent& event) noexcept
    {
        if (event.size < 2 || event.size > 3)
            return;

        const uint8_t status = event.data[0] & 0xF0;
        const uint8_t note = event.data[1];
        const uint8_t velocity = event.size > 2 ? event.data[2] : 0;

        if (status == 0x90 && velocity > 0)
        {
            triggerVoiceNoteOn(note, velocity);
        }
        else if (status == 0x80 || (status == 0x90 && velocity == 0))
        {
            triggerVoiceNoteOff(note);
        }
        else if (status == 0xE0 && event.size >= 3) // pitch bend, 14-bit, center = 8192
        {
            const int raw = (int)event.data[1] | ((int)event.data[2] << 7);
            float normalized = ((float)raw - 8192.0f) / 8192.0f;
            fPitchBendNormalized = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
        }
        else if (status == 0xB0 && event.size >= 3 && event.data[1] == 1) // CC1 = mod wheel
        {
            fModWheelValue = (float)event.data[2] / 127.0f;
        }
    }

    void triggerVoiceNoteOn(int note, int velocity) noexcept
    {
        sideous::PadVoice* target = nullptr;

        for (sideous::PadVoice& voice : fVoices)
        {
            if (!voice.isActive())
            {
                target = &voice;
                break;
            }
        }

        if (target == nullptr)
        {
            for (sideous::PadVoice& voice : fVoices)
            {
                if (voice.isReleasing())
                {
                    target = &voice;
                    break;
                }
            }
        }

        if (target == nullptr)
            target = &fVoices[fStealCursor++ % kNumVoices];

        target->applyParams(fParams);
        target->noteOn(note, (float)velocity / 127.0f);
    }

    void triggerVoiceNoteOff(int note) noexcept
    {
        for (sideous::PadVoice& voice : fVoices)
        {
            if (voice.isActive() && voice.getNote() == note && !voice.isReleasing())
                voice.noteOff();
        }
    }

    std::array<sideous::PadVoice, kNumVoices> fVoices;
    sideous::PadVoiceParams fParams;
    float fMasterVolume = 0.8f;
    float fMasterDrive = 0.0f;
    uint32_t fStealCursor = 0;
    double fSampleRate = 44100.0;

    float fPitchBendNormalized = 0.0f; // -1..1, from MIDI pitch bend (0xE0)
    float fPitchBendRangeSemitones = 12.0f;
    float fModWheelValue = 0.0f;       // 0..1, from MIDI CC1

    float fLfoRateHz = 0.2f;
    float fLfoSync = 0.0f; // 0 = free, see kSyncEnumValues

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideousPadPlugin)
};

// -----------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new SideousPadPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
