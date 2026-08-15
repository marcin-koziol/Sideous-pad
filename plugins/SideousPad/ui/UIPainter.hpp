/*
 * Sideous Pad - GUI layout and Cairo painting, shared between the real
 * plugin UI and an offline PNG renderer used to check the look during
 * development (same technique as every sibling, see their plan docs).
 *
 * Dark chassis, closer to sideous's original C64 vibe than sideous-noise's
 * light-pastel break, so the six panel accents can read as vivid neon
 * against near-black: a classic 6-stripe pride rainbow, one stripe per
 * panel (Unison/Formant/Filter/LFO/Envelope/Master) - see
 * ../../../sideous-pad-plan.md section 8. The title and the Voice Mode
 * selector (this instrument's signature control) are the one deliberately
 * non-single-hue element, painted with the full rainbow gradient instead of
 * a single accent.
 */

#pragma once

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#include "../Params.hpp"

namespace sideous {
namespace ui {

// -----------------------------------------------------------------------------------------------------------
// palette - dark chassis + pride rainbow accents, see sideous-pad-plan.md section 8

struct Color { double r, g, b; };

static constexpr Color kBg        { 0.055, 0.055, 0.071 }; // near-black, faint violet tint
static constexpr Color kPanelBg   { 0.114, 0.114, 0.145 }; // dark slate panel fill
static constexpr Color kPanelEdge { 0.322, 0.322, 0.376 }; // muted panel outline
static constexpr Color kTextMain  { 0.945, 0.945, 0.961 }; // near-white, contrast on dark bg
static constexpr Color kTextDim   { 0.596, 0.596, 0.643 }; // muted grey, secondary text
static constexpr Color kKnobBody  { 0.161, 0.161, 0.196 }; // knob face
static constexpr Color kKnobRing  { 0.259, 0.259, 0.298 }; // knob track (unfilled arc)
static constexpr Color kBtnBg     { 0.145, 0.145, 0.180 }; // unselected selector/dropdown fill

// pride flag, one accent per panel
static constexpr Color kAccentRed    { 0.898, 0.192, 0.192 }; // Unison
static constexpr Color kAccentOrange { 0.949, 0.541, 0.129 }; // Formant / Vowel
static constexpr Color kAccentYellow { 0.949, 0.827, 0.169 }; // Filter
static constexpr Color kAccentGreen  { 0.192, 0.749, 0.365 }; // LFO
static constexpr Color kAccentBlue   { 0.184, 0.478, 0.902 }; // Envelope
static constexpr Color kAccentViolet { 0.545, 0.271, 0.788 }; // Master

static constexpr Color kRainbow[6] = {
    kAccentRed, kAccentOrange, kAccentYellow, kAccentGreen, kAccentBlue, kAccentViolet
};

inline void setColor(cairo_t* cr, Color c, double a = 1.0) noexcept
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

inline cairo_pattern_t* makeRainbowGradient(double x0, double y0, double x1, double y1) noexcept
{
    cairo_pattern_t* grad = cairo_pattern_create_linear(x0, y0, x1, y1);
    const size_t n = 6;
    for (size_t i = 0; i < n; ++i)
    {
        const Color& c = kRainbow[i];
        cairo_pattern_add_color_stop_rgb(grad, (double)i / (double)(n - 1), c.r, c.g, c.b);
    }
    return grad;
}

// -----------------------------------------------------------------------------------------------------------
// layout model

struct Knob
{
    uint32_t param;
    float cx, cy, radius;
    const char* label;
    Color accent;
};

struct SelectorOption
{
    float value;
    const char* label;
};

struct Selector
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr;
    bool rainbow = false; // signature control - paint with the full gradient instead of accent
};

struct Dropdown
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
};

struct PanelBox { float x, y, w, h; const char* title; Color accent; };

struct EnvelopeGraph
{
    uint32_t attackParam, decayParam, sustainParam, releaseParam, curveParam;
    float x, y, w, h;
    Color accent;
};

// a plain clickable button, not tied to a DPF parameter - presets are pure
// UI-side state (see ui/PresetStore.hpp), so Prev/Next/Save/Delete can't
// reuse the param-driven Selector/Knob widgets the rest of the UI is built
// from.
struct Button { float x, y, w, h; const char* label; Color accent; };

struct PresetBarLayout
{
    Button prev, next, save, del;
    float nameX, nameY, nameW, nameH;
    Color accent;
};

struct Layout
{
    float width, height;
    std::vector<PanelBox> panels;
    std::vector<Knob> knobs;
    std::vector<Selector> selectors;
    std::vector<Dropdown> dropdowns;
    std::vector<EnvelopeGraph> envelopeGraphs;
    PresetBarLayout presetBar;
};

// -----------------------------------------------------------------------------------------------------------
// layout builder

inline void addKnobRow(std::vector<Knob>& knobs, float panelX, float panelW, float rowY, float radius,
                        Color accent, std::initializer_list<std::pair<uint32_t, const char*>> items)
{
    const float slot = panelW / (float)items.size();
    size_t i = 0;
    for (const auto& item : items)
    {
        const float cx = panelX + slot * ((float)i + 0.5f);
        knobs.push_back({ item.first, cx, rowY, radius, item.second, accent });
        ++i;
    }
}

inline std::vector<SelectorOption> syncOptions()
{
    return {
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
}

inline Layout buildLayout(float width, float height)
{
    Layout L;
    L.width = width;
    L.height = height;

    const float margin = 12.0f;
    const float gap = 8.0f;
    const float colW = (width - margin * 2.0f - gap) / 2.0f;
    const float colBx = margin + colW + gap;
    const float fullW = width - margin * 2.0f;

    // preset bar: full width, everything else starts below it
    const float presetBarY = 48.0f, presetBarH = 36.0f;

    const float modeY = presetBarY + presetBarH + gap, modeH = 28.0f;
    const float row1Y = modeY + modeH + gap, row1H = 140.0f;
    const float row2Y = row1Y + row1H + gap, row2H = 170.0f;
    const float row3Y = row2Y + row2H + gap, row3H = 150.0f;
    const float barY  = row3Y + row3H + gap, barH = height - barY - margin;

    // --- preset bar --- (left-to-right: prev/next, name field filling the
    // middle, save/delete flush right) - see ui/PresetStore.hpp for the
    // file-based preset library this drives; presets are pure UI-side state
    {
        L.presetBar.accent = kAccentBlue;
        const float y = presetBarY, h = presetBarH;
        const float barLeftX = margin;
        const float barRightX = margin + fullW;

        L.presetBar.prev = { barLeftX, y, 40.0f, h, "<", kAccentBlue };
        L.presetBar.next = { barLeftX + 40.0f + 8.0f, y, 40.0f, h, ">", kAccentBlue };

        const float delX = barRightX - 100.0f;
        const float saveX = delX - 8.0f - 90.0f;
        L.presetBar.save = { saveX, y, 90.0f, h, "SAVE", kAccentBlue };
        L.presetBar.del  = { delX, y, 100.0f, h, "DELETE", kAccentRed };

        L.presetBar.nameX = L.presetBar.next.x + 40.0f + 14.0f;
        L.presetBar.nameY = y;
        L.presetBar.nameW = saveX - 14.0f - L.presetBar.nameX;
        L.presetBar.nameH = h;
    }

    // --- Voice Mode: the signature control, full-width, rainbow gradient ---
    {
        Selector mode;
        mode.param = kParamVoiceMode;
        mode.accent = kAccentRed; // unused when rainbow=true, kept for consistency
        mode.rainbow = true;
        mode.x = margin; mode.y = modeY; mode.w = fullW; mode.h = modeH;
        mode.options = { { 0.0f, "UNISON" }, { 1.0f, "FORMANT" }, { 2.0f, "BOTH" } };
        L.selectors.push_back(mode);
    }

    L.panels.push_back({ margin, row1Y, colW, row1H, "UNISON",   kAccentRed });
    L.panels.push_back({ colBx,  row1Y, colW, row1H, "FORMANT",  kAccentOrange });
    L.panels.push_back({ margin, row2Y, colW, row2H, "FILTER",   kAccentYellow });
    L.panels.push_back({ colBx,  row2Y, colW, row2H, "LFO",      kAccentGreen });
    L.panels.push_back({ margin, row3Y, fullW, row3H, "ENVELOPE", kAccentBlue });
    L.panels.push_back({ margin, barY,  fullW, barH, nullptr,    kAccentViolet });

    // --- Unison ---
    {
        const PanelBox& p = L.panels[0];

        Selector wave;
        wave.param = kParamUnisonWaveform;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + 26.0f; wave.w = p.w - 24.0f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SAW" }, { 1.0f, "PULSE" }, { 2.0f, "TRI" } };
        L.selectors.push_back(wave);

        addKnobRow(L.knobs, p.x, p.w, wave.y + wave.h + 40.0f, 18.0f, p.accent,
                   {
                       { kParamUnisonVoices,      "VOICES" },
                       { kParamUnisonDetune,      "DETUNE" },
                       { kParamUnisonStereoWidth, "WIDTH" },
                       { kParamUnisonDrift,       "DRIFT" },
                       { kParamUnisonPulseWidth,  "PW" },
                   });
    }

    // --- Formant (Vowel) ---
    {
        const PanelBox& p = L.panels[1];
        addKnobRow(L.knobs, p.x, p.w, p.y + 26.0f + 44.0f, 26.0f, p.accent,
                   {
                       { kParamVowel,             "VOWEL" },
                       { kParamFormantAmount,     "AMOUNT" },
                       { kParamFormantResonance,  "RESO" },
                   });
    }

    // --- Filter ---
    {
        const PanelBox& p = L.panels[2];
        Dropdown mode;
        mode.param = kParamFilterMode;
        mode.accent = p.accent;
        mode.x = p.x + 12.0f; mode.y = p.y + 26.0f; mode.w = p.w - 24.0f; mode.h = 24.0f;
        mode.options = {
            { 0.0f, "LOWPASS 12dB" },
            { 1.0f, "LOWPASS 24dB" },
            { 2.0f, "HIGHPASS 12dB" },
            { 3.0f, "HIGHPASS 24dB" },
            { 4.0f, "BANDPASS 12dB" },
            { 5.0f, "LADDER 24dB" },
        };
        L.dropdowns.push_back(mode);

        addKnobRow(L.knobs, p.x, p.w, p.y + 26.0f + 24.0f + 40.0f, 22.0f, p.accent,
                   {
                       { kParamFilterCutoff,    "CUTOFF" },
                       { kParamFilterResonance, "RESO" },
                       { kParamFilterDrive,     "DRIVE" },
                   });
    }

    // --- LFO ---
    {
        const PanelBox& p = L.panels[3];

        Selector wave;
        wave.param = kParamLfoWaveform;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + 26.0f; wave.w = p.w * 0.48f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SINE" }, { 1.0f, "SAW" }, { 2.0f, "SQR" }, { 3.0f, "GATE" }, { 4.0f, "S&H" } };
        L.selectors.push_back(wave);

        Selector dest;
        dest.param = kParamLfoDestination;
        dest.accent = p.accent;
        dest.x = wave.x + wave.w + 8.0f; dest.y = wave.y; dest.w = (p.x + p.w - 12.0f) - dest.x; dest.h = 24.0f;
        dest.options = { { 0.0f, "PITCH" }, { 1.0f, "CUTOFF" }, { 2.0f, "AMP" }, { 3.0f, "VOWEL" } };
        L.selectors.push_back(dest);

        Dropdown sync;
        sync.param = kParamLfoSync;
        sync.accent = p.accent;
        sync.x = p.x + 12.0f; sync.y = wave.y + wave.h + 8.0f; sync.w = p.w - 24.0f; sync.h = 24.0f;
        sync.options = syncOptions();
        L.dropdowns.push_back(sync);

        addKnobRow(L.knobs, p.x, p.w, sync.y + sync.h + 32.0f, 20.0f, p.accent,
                   {
                       { kParamLfoWidth,  "WIDTH" },
                       { kParamLfoRateHz, "RATE" },
                       { kParamLfoAmount, "AMOUNT" },
                   });
    }

    // --- Envelope (full width) ---
    {
        const PanelBox& p = L.panels[4];
        const float rowY = p.y + 26.0f + 30.0f;
        addKnobRow(L.knobs, p.x, p.w, rowY, 20.0f, p.accent,
                   {
                       { kParamAmpAttack,          "ATTACK" },
                       { kParamAmpDecay,            "DECAY" },
                       { kParamAmpSustain,          "SUSTAIN" },
                       { kParamAmpRelease,          "RELEASE" },
                       { kParamAmpCurve,            "CURVE" },
                       { kParamVelocitySensitivity, "VELOCITY" },
                   });

        L.envelopeGraphs.push_back({
            kParamAmpAttack, kParamAmpDecay, kParamAmpSustain, kParamAmpRelease, kParamAmpCurve,
            p.x + 12.0f, rowY + 20.0f + 22.0f, p.w - 24.0f, 38.0f, p.accent });
    }

    // --- bottom bar: master + performance controls ---
    {
        const PanelBox& p = L.panels[5];
        const float cy = p.y + p.h * 0.5f;

        L.knobs.push_back({ kParamMasterVolume, p.x + 44.0f,  cy, 20.0f, "VOLUME", p.accent });
        L.knobs.push_back({ kParamMasterDrive,  p.x + 104.0f, cy, 17.0f, "DRIVE",  p.accent });

        L.knobs.push_back({ kParamPitchBendRange, p.x + 160.0f, cy, 16.0f, "BEND", p.accent });

        L.knobs.push_back({ kParamModWheelAmount, p.x + 220.0f, cy, 16.0f, "MW AMT", p.accent });

        Selector modWheel;
        modWheel.param = kParamModWheelDestination;
        modWheel.accent = p.accent;
        modWheel.caption = "MOD WHEEL";
        modWheel.x = p.x + 260.0f; modWheel.y = cy - 12.0f; modWheel.w = (p.x + p.w - 12.0f) - modWheel.x; modWheel.h = 24.0f;
        modWheel.options = { { 0.0f, "OFF" }, { 1.0f, "VIBRATO" }, { 2.0f, "CUTOFF" }, { 3.0f, "VOLUME" }, { 4.0f, "VOWEL" } };
        L.selectors.push_back(modWheel);
    }

    return L;
}

// -----------------------------------------------------------------------------------------------------------
// value <-> knob-rotation mapping (shared by drawing and mouse-drag hit logic)

inline float paramToNormalized(uint32_t param, float value) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    float t;
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        const float v = value < lo ? lo : (value > hi ? hi : value);
        t = (float)(std::log(v / lo) / std::log(hi / lo));
    }
    else
    {
        t = (value - info.min) / (info.max - info.min);
    }
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float normalizedToParam(uint32_t param, float t) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        return lo * std::pow(hi / lo, t);
    }
    return info.min + (info.max - info.min) * t;
}

inline void getSelectorOptionRect(const Selector& sel, size_t i, double& ox, double& ow) noexcept
{
    const size_t n = sel.options.size();
    const double gap = 6.0;
    ow = (sel.w - gap * (double)(n - 1)) / (double)n;
    ox = sel.x + (ow + gap) * (double)i;
}

inline void getDropdownOptionRect(const Dropdown& dd, size_t i, float layoutHeight, double& oy, double& oh) noexcept
{
    const double n = (double)dd.options.size();
    oh = dd.h - 4.0;

    const double downSpace = (double)layoutHeight - (dd.y + dd.h);
    const double upSpace = dd.y;
    const bool openUpward = (n * oh > downSpace) && (upSpace > downSpace);

    if (openUpward)
        oy = (dd.y - n * oh) + oh * (double)i;
    else
        oy = dd.y + dd.h + oh * (double)i;
}

// -----------------------------------------------------------------------------------------------------------
// value formatting

inline const char* vowelName(int index) noexcept
{
    static const char* names[5] = { "AH", "EH", "EE", "OH", "OO" };
    return names[index < 0 ? 0 : (index > 4 ? 4 : index)];
}

inline void formatParamValue(uint32_t index, float value, char* out, size_t outSize) noexcept
{
    switch (index)
    {
    case kParamVowel:
        std::snprintf(out, outSize, "%s", vowelName((int)(value + 0.5f)));
        return;
    case kParamUnisonVoices:
        std::snprintf(out, outSize, "%dx", (int)(value + 0.5f));
        return;
    default:
        break;
    }

    const ParamInfo& info = getParamInfo(index);

    if (std::strcmp(info.unit, "Hz") == 0)
    {
        if (value >= 1000.0f)
            std::snprintf(out, outSize, "%.2fk", value / 1000.0);
        else
            std::snprintf(out, outSize, "%.2f", value);
    }
    else if (std::strcmp(info.unit, "s") == 0)
    {
        if (value < 1.0f)
            std::snprintf(out, outSize, "%.0fms", value * 1000.0);
        else
            std::snprintf(out, outSize, "%.2fs", value);
    }
    else if (std::strcmp(info.unit, "%") == 0)
    {
        std::snprintf(out, outSize, "%.0f%%", value);
    }
    else if (std::strcmp(info.unit, "st") == 0)
    {
        std::snprintf(out, outSize, "\xc2\xb1%dst", (int)(value + 0.5f)); // U+00B1 PLUS-MINUS SIGN
    }
    else if (std::strcmp(info.unit, "ct") == 0)
    {
        std::snprintf(out, outSize, "%.0fct", value);
    }
    else
    {
        std::snprintf(out, outSize, "%.2f", value);
    }
}

// -----------------------------------------------------------------------------------------------------------
// drawing helpers

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI_2, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0,      M_PI_2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI_2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,     3.0 * M_PI_2);
    cairo_close_path(cr);
}

// cairo_show_text() leaves a dangling current point; if the next drawing call
// is cairo_arc()/cairo_line_to() without an explicit new subpath, cairo will
// implicitly connect from that stray point. Always reset the path after text.
inline void drawText(cairo_t* cr, const char* text, double x, double y)
{
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

inline void centeredText(cairo_t* cr, const char* text, double cx, double cy)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    drawText(cr, text, cx - ext.width / 2.0 - ext.x_bearing, cy - ext.height / 2.0 - ext.y_bearing);
}

inline void setFont(cairo_t* cr, double size, bool bold = true)
{
    cairo_select_font_face(cr, "monospace",
                            CAIRO_FONT_SLANT_NORMAL,
                            bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

// -----------------------------------------------------------------------------------------------------------
// paint

struct PaintState
{
    const float* values = nullptr;
    int hoverKnob = -1;
    int dragKnob = -1;
    int hoverSelector = -1;
    int hoverSelectorOption = -1;
    int hoverDropdown = -1;
    int openDropdown = -1;
    int hoverDropdownOption = -1;
    const bool* autoShowValue = nullptr;

    const char* presetName = "INIT";
    bool presetIsUnsaved = false; // true once a param has changed since the last load/save
    bool presetEditingName = false;
    const char* presetEditBuffer = "";
    int hoverPresetButton = -1;       // 0=prev,1=next,2=save,3=delete, or -1
    bool presetDeleteEnabled = false; // false while on INIT - nothing to delete
};

inline void paintKnob(cairo_t* cr, const Knob& k, float value, bool hovered, bool dragging, bool autoShow)
{
    const float t = paramToNormalized(k.param, value);

    const double startAngle = M_PI * 0.75;
    const double sweep = M_PI * 1.5;
    const double valueAngle = startAngle + sweep * t;

    cairo_set_line_width(cr, 5.0);
    setColor(cr, kKnobRing);
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, startAngle + sweep);
    cairo_stroke(cr);

    setColor(cr, k.accent, dragging ? 1.0 : (hovered ? 0.9 : 0.85));
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, valueAngle);
    cairo_stroke(cr);

    setColor(cr, kKnobBody);
    cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
    cairo_fill(cr);

    if (hovered || dragging)
    {
        setColor(cr, k.accent, 0.22);
        cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
    }

    setColor(cr, kTextMain);
    cairo_set_line_width(cr, 3.0);
    const double px = k.cx + std::cos(valueAngle) * (k.radius - 10.0);
    const double py = k.cy + std::sin(valueAngle) * (k.radius - 10.0);
    cairo_move_to(cr, k.cx, k.cy);
    cairo_line_to(cr, px, py);
    cairo_stroke(cr);

    setFont(cr, 10.5);
    setColor(cr, kTextDim);
    centeredText(cr, k.label, k.cx, k.cy + k.radius + 14.0);

    if (dragging || autoShow)
    {
        char buf[32];
        formatParamValue(k.param, value, buf, sizeof(buf));
        setFont(cr, 11.5);
        setColor(cr, kTextMain);
        centeredText(cr, buf, k.cx, k.cy - k.radius - 12.0);
    }
}

inline float envelopeCurveShape(float t, float curveAmount) noexcept
{
    const float exponent = std::pow(4.0f, -curveAmount);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return std::pow(t, exponent);
}

inline void paintEnvelopeGraph(cairo_t* cr, const EnvelopeGraph& eg, const float* values)
{
    const float sustain = values[eg.sustainParam];
    const float curve = values[eg.curveParam];

    const float attackFrac = 0.25f, decayFrac = 0.25f, holdFrac = 0.20f, releaseFrac = 0.30f;

    roundedRect(cr, eg.x, eg.y, eg.w, eg.h, 4.0);
    setColor(cr, kBg, 0.7);
    cairo_fill(cr);

    const int steps = 14;
    const double x0 = eg.x, y0 = eg.y, w = eg.w, h = eg.h;
    auto plotX = [&](float frac) { return x0 + w * (double)frac; };
    auto plotY = [&](float level) { return y0 + h * (1.0 - (double)level); };

    cairo_new_path(cr);
    cairo_move_to(cr, plotX(0.0f), plotY(0.0f));

    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float level = envelopeCurveShape(t, curve);
        cairo_line_to(cr, plotX(attackFrac * t), plotY(level));
    }
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float shaped = envelopeCurveShape(t, curve);
        const float level = 1.0f + (sustain - 1.0f) * shaped;
        cairo_line_to(cr, plotX(attackFrac + decayFrac * t), plotY(level));
    }
    cairo_line_to(cr, plotX(attackFrac + decayFrac + holdFrac), plotY(sustain));
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float shaped = envelopeCurveShape(t, curve);
        const float level = sustain * (1.0f - shaped);
        cairo_line_to(cr, plotX(attackFrac + decayFrac + holdFrac + releaseFrac * t), plotY(level));
    }

    setColor(cr, eg.accent, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke_preserve(cr);

    cairo_line_to(cr, plotX(1.0f), plotY(0.0f));
    cairo_line_to(cr, plotX(0.0f), plotY(0.0f));
    cairo_close_path(cr);
    setColor(cr, eg.accent, 0.22);
    cairo_fill(cr);
}

inline void paintSelector(cairo_t* cr, const Selector& sel, float value, int hoverOption)
{
    if (sel.caption != nullptr)
    {
        setFont(cr, 9.0, false);
        setColor(cr, kTextDim);
        drawText(cr, sel.caption, sel.x, sel.y - 6.0);
    }

    const size_t n = sel.options.size();

    for (size_t i = 0; i < n; ++i)
    {
        double ox, optW;
        getSelectorOptionRect(sel, i, ox, optW);
        const bool active = std::fabs(sel.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        roundedRect(cr, ox, sel.y, optW, sel.h, 4.0);
        if (active)
            setColor(cr, sel.accent, 0.85);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.9);
        cairo_fill_preserve(cr);
        setColor(cr, active ? sel.accent : kPanelEdge, active ? 1.0 : 0.6);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        setFont(cr, 10.5);
        setColor(cr, active ? Color{ 0.06, 0.06, 0.08 } : kTextMain);
        centeredText(cr, sel.options[i].label, ox + optW / 2.0, sel.y + sel.h / 2.0);
    }
}

// the Voice Mode strip: whole strip painted as one continuous rainbow
// gradient, the active option full-strength, inactive options dimmed under
// a translucent dark overlay - see file header
inline void paintRainbowSelector(cairo_t* cr, const Selector& sel, float value, int hoverOption)
{
    roundedRect(cr, sel.x, sel.y, sel.w, sel.h, 6.0);
    cairo_pattern_t* grad = makeRainbowGradient(sel.x, 0.0, sel.x + sel.w, 0.0);
    cairo_set_source(cr, grad);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    const size_t n = sel.options.size();
    for (size_t i = 0; i < n; ++i)
    {
        double ox, optW;
        getSelectorOptionRect(sel, i, ox, optW);
        const bool active = std::fabs(sel.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        roundedRect(cr, ox, sel.y, optW, sel.h, 4.0);
        if (!active)
            setColor(cr, kBg, hovered ? 0.55 : 0.72);
        else
            setColor(cr, kBg, 0.08);
        cairo_fill(cr);

        if (active)
        {
            roundedRect(cr, ox, sel.y, optW, sel.h, 4.0);
            setColor(cr, kTextMain, 0.9);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
        }

        setFont(cr, 11.0);
        setColor(cr, kTextMain);
        centeredText(cr, sel.options[i].label, ox + optW / 2.0, sel.y + sel.h / 2.0);
    }
}

inline const char* findOptionLabel(const std::vector<SelectorOption>& options, float value) noexcept
{
    for (const SelectorOption& opt : options)
        if (std::fabs(opt.value - value) < 0.001f)
            return opt.label;
    return "?";
}

inline void paintDropdownClosed(cairo_t* cr, const Dropdown& dd, float value, bool hovered, bool open)
{
    roundedRect(cr, dd.x, dd.y, dd.w, dd.h, 4.0);
    setColor(cr, kBtnBg, (hovered || open) ? 1.0 : 0.9);
    cairo_fill_preserve(cr);
    setColor(cr, open ? dd.accent : kPanelEdge, open ? 1.0 : 0.6);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.0);
    setColor(cr, kTextMain);
    cairo_text_extents_t ext;
    const char* label = findOptionLabel(dd.options, value);
    cairo_text_extents(cr, label, &ext);
    drawText(cr, label, dd.x + 12.0, dd.y + dd.h / 2.0 - ext.height / 2.0 - ext.y_bearing);

    const double ax = dd.x + dd.w - 20.0, ay = dd.y + dd.h / 2.0;
    setColor(cr, kTextDim);
    cairo_move_to(cr, ax - 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax + 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax,       open ? ay - 3.0 : ay + 3.0);
    cairo_close_path(cr);
    cairo_fill(cr);
}

inline void paintDropdownOpenList(cairo_t* cr, const Dropdown& dd, float value, int hoverOption, float layoutHeight)
{
    const size_t n = dd.options.size();
    double oyFirst, oyLast, oh;
    getDropdownOptionRect(dd, 0, layoutHeight, oyFirst, oh);
    getDropdownOptionRect(dd, n - 1, layoutHeight, oyLast, oh);
    const double listTop = oyFirst < oyLast ? oyFirst : oyLast;
    const double listBottom = (oyFirst < oyLast ? oyLast : oyFirst) + oh;

    setColor(cr, kBg, 0.98);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_fill(cr);

    for (size_t i = 0; i < n; ++i)
    {
        double oy;
        getDropdownOptionRect(dd, i, layoutHeight, oy, oh);
        const bool active = std::fabs(dd.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        cairo_rectangle(cr, dd.x, oy, dd.w, oh);
        if (active)
            setColor(cr, dd.accent, 0.85);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.0);
        cairo_fill(cr);

        setFont(cr, 10.5);
        setColor(cr, active ? Color{ 0.06, 0.06, 0.08 } : kTextMain);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, dd.options[i].label, &ext);
        drawText(cr, dd.options[i].label, dd.x + 12.0, oy + oh / 2.0 - ext.height / 2.0 - ext.y_bearing);
    }

    setColor(cr, dd.accent, 0.9);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_stroke(cr);
}

inline void paintButton(cairo_t* cr, const Button& b, bool hovered, bool enabled)
{
    roundedRect(cr, b.x, b.y, b.w, b.h, 4.0);
    setColor(cr, kBtnBg, enabled ? (hovered ? 1.0 : 0.85) : 0.4);
    cairo_fill_preserve(cr);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.5);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    centeredText(cr, b.label, b.x + b.w / 2.0, b.y + b.h / 2.0);
}

inline void paintPresetBar(cairo_t* cr, const PresetBarLayout& pb, const PaintState& state)
{
    paintButton(cr, pb.prev, state.hoverPresetButton == 0, true);
    paintButton(cr, pb.next, state.hoverPresetButton == 1, true);
    paintButton(cr, pb.save, state.hoverPresetButton == 2, true);
    paintButton(cr, pb.del,  state.hoverPresetButton == 3, state.presetDeleteEnabled);

    roundedRect(cr, pb.nameX, pb.nameY, pb.nameW, pb.nameH, 4.0);
    setColor(cr, state.presetEditingName ? kKnobBody : kBtnBg, 0.9);
    cairo_fill_preserve(cr);
    setColor(cr, state.presetEditingName ? pb.accent : kPanelEdge, state.presetEditingName ? 1.0 : 0.8);
    cairo_set_line_width(cr, state.presetEditingName ? 2.0 : 1.5);
    cairo_stroke(cr);

    setFont(cr, 9.0, false);
    setColor(cr, kTextDim);
    drawText(cr, "PRESET", pb.nameX + 12.0, pb.nameY + 13.0);

    setFont(cr, 13.0);
    if (state.presetEditingName)
    {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s_", state.presetEditBuffer);
        setColor(cr, kTextMain);
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 10.0);
    }
    else
    {
        setColor(cr, state.presetIsUnsaved ? kAccentOrange : kTextMain);
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s%s", state.presetName, state.presetIsUnsaved ? " *" : "");
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 10.0);
    }
}

inline void paint(cairo_t* cr, const Layout& L, const PaintState& state)
{
    setColor(cr, kBg);
    cairo_paint(cr);

    setFont(cr, 26.0);
    {
        cairo_text_extents_t ext;
        cairo_text_extents(cr, "SIDEOUS PAD", &ext);
        cairo_pattern_t* grad = makeRainbowGradient(20.0, 0.0, 20.0 + ext.width, 0.0);
        cairo_set_source(cr, grad);
        drawText(cr, "SIDEOUS PAD", 20.0, 40.0);
        cairo_pattern_destroy(grad);
    }

    setFont(cr, 10.0, false);
    setColor(cr, kTextDim);
    {
        const char* subtitle = "RETRO CHIPTUNE CHOIR / PAD";
        cairo_text_extents_t ext;
        cairo_text_extents(cr, subtitle, &ext);
        drawText(cr, subtitle, L.width - 20.0 - ext.width, 24.0);
    }

    paintPresetBar(cr, L.presetBar, state);

    for (const PanelBox& p : L.panels)
    {
        roundedRect(cr, p.x, p.y, p.w, p.h, 8.0);
        setColor(cr, kPanelBg);
        cairo_fill_preserve(cr);
        setColor(cr, kPanelEdge, 0.7);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        if (p.title != nullptr)
        {
            setFont(cr, 12.0);
            setColor(cr, p.accent);
            drawText(cr, p.title, p.x + 14.0, p.y + 22.0);
        }
    }

    for (const EnvelopeGraph& eg : L.envelopeGraphs)
        paintEnvelopeGraph(cr, eg, state.values);

    for (size_t i = 0; i < L.selectors.size(); ++i)
    {
        const Selector& sel = L.selectors[i];
        const int hoverOption = (int)i == state.hoverSelector ? state.hoverSelectorOption : -1;
        if (sel.rainbow)
            paintRainbowSelector(cr, sel, state.values[sel.param], hoverOption);
        else
            paintSelector(cr, sel, state.values[sel.param], hoverOption);
    }

    for (size_t i = 0; i < L.dropdowns.size(); ++i)
    {
        const Dropdown& dd = L.dropdowns[i];
        paintDropdownClosed(cr, dd, state.values[dd.param], (int)i == state.hoverDropdown, (int)i == state.openDropdown);
    }

    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        const bool autoShow = state.autoShowValue != nullptr && state.autoShowValue[k.param];
        paintKnob(cr, k, state.values[k.param], (int)i == state.hoverKnob, (int)i == state.dragKnob, autoShow);
    }

    if (state.openDropdown >= 0 && (size_t)state.openDropdown < L.dropdowns.size())
    {
        const Dropdown& dd = L.dropdowns[state.openDropdown];
        paintDropdownOpenList(cr, dd, state.values[dd.param], state.hoverDropdownOption, L.height);
    }
}

} // namespace ui
} // namespace sideous
