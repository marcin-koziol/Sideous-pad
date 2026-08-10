/*
 * Sideous Pad - a handful of factory example presets, seeded into the
 * user's preset directory (see ui/PresetStore.hpp) the first time the UI
 * ever runs with an empty preset library. Compiled in (rather than shipped
 * as separate data files) so they exist identically across every build/
 * install of the plugin regardless of platform or plugin format - no
 * bundle-path discovery needed, they just get written out as ordinary,
 * user-editable ".sppreset" files on first run and behave exactly like any
 * preset the user saves themselves from that point on. Copied near-verbatim
 * from sideous-bass's ui/FactoryPresets.hpp.
 */

#pragma once

#include "PresetStore.hpp"

#include <cstring>
#include <utility>
#include <vector>

namespace sideous {
namespace ui {

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> overrides; // symbol -> value; everything else stays at that param's Params.hpp default
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        // Both engines, wide slow-drifting unison feeding a resonant Eh-ish
        // formant, LFO sweeping Vowel very slowly - the "default" showcase
        // of what makes this instrument distinct from a plain pad
        { "Angelic Choir", {
            { "voice_mode", 2.0f }, // Both
            { "unison_voices", 5.0f }, { "unison_detune", 15.0f }, { "unison_stereo_width", 0.8f }, { "unison_drift", 0.4f },
            { "vowel", 1.0f }, { "formant_amount", 0.9f }, { "formant_resonance", 0.8f },
            { "filter_mode", 0.0f }, { "filter_cutoff", 6000.0f }, { "filter_resonance", 0.1f },
            { "amp_attack", 1.2f }, { "amp_decay", 0.6f }, { "amp_sustain", 0.9f }, { "amp_release", 2.0f },
            { "lfo_destination", 3.0f }, { "lfo_sync", 0.0f }, { "lfo_rate_hz", 0.15f }, { "lfo_amount", 0.5f },
        }},
        // Maximum unison + drift + detune through a mellow ladder filter,
        // very long attack/release - a huge, slow-blooming wall of sound
        { "8-Bit Cathedral", {
            { "voice_mode", 2.0f },
            { "unison_voices", 5.0f }, { "unison_detune", 25.0f }, { "unison_stereo_width", 1.0f }, { "unison_drift", 0.6f },
            { "vowel", 0.0f }, { "formant_amount", 0.8f }, { "formant_resonance", 0.75f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 3000.0f }, { "filter_resonance", 0.2f }, { "filter_drive", 0.1f },
            { "amp_attack", 2.0f }, { "amp_decay", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 3.5f },
            { "lfo_destination", 3.0f }, { "lfo_rate_hz", 0.08f }, { "lfo_amount", 0.7f },
        }},
        // Formant-only, faster LFO sweeping Vowel plus mod wheel patched to
        // Vowel too - a performance patch built to be "talked" during a take
        { "Talking Synth Voice", {
            { "voice_mode", 1.0f }, // Formant
            { "unison_waveform", 1.0f }, { "unison_pulse_width", 0.3f },
            { "vowel", 2.0f }, { "formant_resonance", 0.85f },
            { "filter_cutoff", 8000.0f },
            { "amp_attack", 0.05f }, { "amp_decay", 0.2f }, { "amp_sustain", 0.8f }, { "amp_release", 0.5f },
            { "lfo_destination", 3.0f }, { "lfo_rate_hz", 2.5f }, { "lfo_amount", 0.8f },
            { "mod_wheel_dest", 4.0f }, { "mod_wheel_amount", 1.0f },
        }},
        // Unison-only, no formant at all - a classic wide detuned saw pad,
        // the "plain pad" baseline this instrument can still do
        { "Retro Saw Pad", {
            { "voice_mode", 0.0f }, // Unison
            { "unison_voices", 5.0f }, { "unison_detune", 18.0f }, { "unison_stereo_width", 0.9f }, { "unison_drift", 0.3f },
            { "filter_cutoff", 5000.0f }, { "filter_resonance", 0.2f },
            { "amp_attack", 0.8f }, { "amp_decay", 0.5f }, { "amp_sustain", 0.85f }, { "amp_release", 1.5f },
            { "lfo_destination", 1.0f }, { "lfo_rate_hz", 0.3f }, { "lfo_amount", 0.2f },
        }},
        // Formant-only, pulse wave, very sharp formant resonance, fast punchy
        // envelope - vocoder-ish stab rather than a sustained pad
        { "Vocoder Stab", {
            { "voice_mode", 1.0f },
            { "unison_waveform", 1.0f }, { "unison_pulse_width", 0.5f },
            { "vowel", 3.0f }, { "formant_resonance", 0.95f },
            { "filter_mode", 1.0f }, { "filter_cutoff", 4000.0f }, { "filter_resonance", 0.3f },
            { "amp_attack", 0.005f }, { "amp_decay", 0.15f }, { "amp_sustain", 0.3f }, { "amp_release", 0.2f },
        }},
        // Both engines, wide drift, highpassed and Oo-voiced - thin, breathy,
        // distant - the "ghost" end of the choir range
        { "Ghost Choir", {
            { "voice_mode", 2.0f },
            { "unison_voices", 4.0f }, { "unison_detune", 30.0f }, { "unison_stereo_width", 1.0f }, { "unison_drift", 0.8f },
            { "vowel", 4.0f }, { "formant_amount", 0.6f }, { "formant_resonance", 0.6f },
            { "filter_mode", 2.0f }, { "filter_cutoff", 300.0f },
            { "amp_attack", 2.5f }, { "amp_decay", 0.8f }, { "amp_sustain", 0.7f }, { "amp_release", 4.0f },
            { "lfo_destination", 2.0f }, { "lfo_rate_hz", 0.2f }, { "lfo_amount", 0.15f },
        }},
        // Both engines, a tempo-synced Gate LFO chopping the Vowel knob at
        // 1/8 - rhythmic vowel-chatter rather than a smooth morph
        { "Wobbly Vowels", {
            { "voice_mode", 2.0f },
            { "unison_voices", 3.0f }, { "unison_detune", 10.0f },
            { "formant_amount", 0.9f }, { "formant_resonance", 0.8f },
            { "lfo_waveform", 3.0f }, { "lfo_width", 30.0f }, { "lfo_destination", 3.0f }, { "lfo_sync", 9.0f }, { "lfo_amount", 1.0f },
            { "amp_sustain", 0.9f },
        }},
        // Unison-only, triangle wave, small detune, quicker envelope - closer
        // to a playable chip lead than a slow pad
        { "Chip Chorus Lead", {
            { "voice_mode", 0.0f },
            { "unison_waveform", 2.0f }, { "unison_voices", 3.0f }, { "unison_detune", 8.0f }, { "unison_stereo_width", 0.5f },
            { "filter_cutoff", 7000.0f },
            { "amp_attack", 0.1f }, { "amp_decay", 0.3f }, { "amp_sustain", 0.7f }, { "amp_release", 0.6f },
        }},
    };
    return presets;
}

// writes every factory preset to disk via the normal savePreset() path (so
// they're indistinguishable from user-saved presets from that point on) -
// call once, only when the preset library is empty (first run).
inline void seedFactoryPresets()
{
    for (const FactoryPreset& fp : factoryPresets())
    {
        float values[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            values[i] = getParamInfo(i).def;

        for (const auto& kv : fp.overrides)
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
            {
                if (std::strcmp(getParamInfo(i).symbol, kv.first) == 0)
                {
                    values[i] = kv.second;
                    break;
                }
            }
        }

        savePreset(fp.name, values);
    }
}

} // namespace ui
} // namespace sideous
