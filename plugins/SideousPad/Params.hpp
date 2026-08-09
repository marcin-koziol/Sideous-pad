/*
 * Sideous Pad - shared parameter definitions used by both the DSP plugin
 * side and the UI, so ranges/units/labels can't drift between the two.
 */

#pragma once

#include <cstdint>

namespace sideous {

enum Params : uint32_t {
    kParamVoiceMode = 0,

    kParamUnisonWaveform,
    kParamUnisonPulseWidth,
    kParamUnisonVoices,
    kParamUnisonDetune,
    kParamUnisonStereoWidth,
    kParamUnisonDrift,

    kParamVowel,
    kParamFormantAmount,
    kParamFormantResonance,

    kParamFilterMode,
    kParamFilterCutoff,
    kParamFilterResonance,
    kParamFilterDrive,

    kParamAmpAttack,
    kParamAmpDecay,
    kParamAmpSustain,
    kParamAmpRelease,
    kParamAmpCurve,
    kParamVelocitySensitivity,

    kParamLfoWaveform,
    kParamLfoWidth,
    kParamLfoDestination,
    kParamLfoRateHz,
    kParamLfoSync,
    kParamLfoAmount,

    kParamPitchBendRange,
    kParamModWheelDestination,
    kParamModWheelAmount,

    kParamMasterVolume,
    kParamMasterDrive,

    kParamCount
};

enum class ParamShape { Linear, Logarithmic };

struct ParamInfo
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    ParamShape shape;
};

inline const ParamInfo& getParamInfo(uint32_t index) noexcept
{
    static constexpr ParamInfo table[kParamCount] = {
        { "Voice Mode",       "voice_mode",          "",   0.0f,     2.0f,     2.0f,   ParamShape::Linear },

        { "Unison Waveform",  "unison_waveform",     "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "Unison Pulse Wid", "unison_pulse_width",  "",   0.01f,    0.99f,    0.5f,   ParamShape::Linear },
        { "Unison Voices",    "unison_voices",       "",   1.0f,     5.0f,     3.0f,   ParamShape::Linear },
        { "Unison Detune",    "unison_detune",       "ct", 0.0f,    50.0f,    12.0f,   ParamShape::Linear },
        { "Unison Width",     "unison_stereo_width", "",   0.0f,     1.0f,     0.6f,   ParamShape::Linear },
        { "Unison Drift",     "unison_drift",        "",   0.0f,     1.0f,     0.3f,   ParamShape::Linear },

        { "Vowel",            "vowel",               "",   0.0f,     4.0f,     0.0f,   ParamShape::Linear },
        { "Formant Amount",   "formant_amount",      "",   0.0f,     1.0f,     0.85f,  ParamShape::Linear },
        { "Formant Reso",     "formant_resonance",   "",   0.0f,     1.0f,     0.7f,   ParamShape::Linear },

        { "Filter Mode",      "filter_mode",         "",   0.0f,     5.0f,     0.0f,   ParamShape::Linear },
        { "Filter Cutoff",    "filter_cutoff",       "Hz", 20.0f, 20000.0f, 4000.0f,   ParamShape::Logarithmic },
        { "Filter Resonance", "filter_resonance",    "",   0.0f,     1.0f,     0.15f,  ParamShape::Linear },
        { "Filter Drive",     "filter_drive",        "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },

        { "Amp Attack",       "amp_attack",          "s",  0.005f,   8.0f,     0.3f,   ParamShape::Logarithmic },
        { "Amp Decay",        "amp_decay",           "s",  0.005f,   8.0f,     0.4f,   ParamShape::Logarithmic },
        { "Amp Sustain",      "amp_sustain",         "",   0.0f,     1.0f,     0.8f,   ParamShape::Linear },
        { "Amp Release",      "amp_release",         "s",  0.005f,  12.0f,     0.8f,   ParamShape::Logarithmic },
        { "Amp Env Curve",    "amp_curve",           "",   0.0f,     1.0f,     0.35f,  ParamShape::Linear },
        { "Velocity Sens",    "velocity_sens",       "",   0.0f,     1.0f,     0.6f,   ParamShape::Linear },

        { "LFO Waveform",     "lfo_waveform",        "",   0.0f,     4.0f,     0.0f,   ParamShape::Linear },
        { "LFO Width",        "lfo_width",           "%",  1.0f,    99.0f,    50.0f,   ParamShape::Linear },
        { "LFO Destination",  "lfo_destination",     "",   0.0f,     3.0f,     3.0f,   ParamShape::Linear },
        { "LFO Rate",         "lfo_rate_hz",         "Hz", 0.02f,   20.0f,     0.2f,   ParamShape::Logarithmic },
        { "LFO Sync",         "lfo_sync",            "",   0.0f,    14.0f,     0.0f,   ParamShape::Linear },
        { "LFO Amount",       "lfo_amount",          "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },

        { "Pitch Bend Range", "pitch_bend_range",    "st", 1.0f,    24.0f,    12.0f,   ParamShape::Linear },
        { "Mod Wheel Dest",   "mod_wheel_dest",      "",   0.0f,     4.0f,     0.0f,   ParamShape::Linear },
        { "Mod Wheel Amount", "mod_wheel_amount",    "",   0.0f,     1.0f,     0.5f,   ParamShape::Linear },

        { "Master Volume",    "master_volume",       "",   0.0f,     1.0f,     0.8f,   ParamShape::Linear },
        { "Master Drive",     "master_drive",        "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
    };
    return table[index];
}

} // namespace sideous
