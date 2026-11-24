#include "ModuleDefs.h"
#include <cmath>

namespace mr
{
//==============================================================================
float ParamSpec::toReal (float norm) const
{
    norm = juce::jlimit (0.0f, 1.0f, norm);

    if (unit == Unit::Choice)
        return (float) juce::roundToInt (norm * (float) juce::jmax (1, choices.size() - 1));

    if (unit == Unit::Toggle)
        return norm >= 0.5f ? 1.0f : 0.0f;

    if (logScale && minV > 0.0f)
        return minV * std::pow (maxV / minV, norm);

    return minV + (maxV - minV) * norm;
}

float ParamSpec::toNorm (float real) const
{
    if (unit == Unit::Choice)
    {
        const int n = juce::jmax (1, choices.size() - 1);
        return juce::jlimit (0.0f, 1.0f, real / (float) n);
    }

    if (unit == Unit::Toggle)
        return real >= 0.5f ? 1.0f : 0.0f;

    real = juce::jlimit (minV, maxV, real);

    if (logScale && minV > 0.0f)
        return (float) (std::log (real / minV) / std::log (maxV / minV));

    return (real - minV) / (maxV - minV);
}

static juce::String trimNumber (float v, int decimals)
{
    auto s = juce::String (v, decimals);
    if (s.containsChar ('.'))
    {
        s = s.trimCharactersAtEnd ("0");
        s = s.trimCharactersAtEnd (".");
    }
    if (s == "-0") s = "0";
    return s;
}

juce::String ParamSpec::formatReal (float v) const
{
    if (unit != Unit::Choice && unit != Unit::Toggle && isOff (v))
        return "Off";

    switch (unit)
    {
        case Unit::Choice:
        {
            const int idx = juce::jlimit (0, juce::jmax (0, choices.size() - 1), juce::roundToInt (v));
            return choices[idx];
        }
        case Unit::Toggle:  return v >= 0.5f ? "On" : "Off";
        case Unit::dB:
        {
            const auto s = juce::String (std::abs (v) < 0.05f ? 0.0f : v, 1);
            return (v > 0.049f ? "+" : "") + s + " dB";
        }
        case Unit::Hz:
            if (v < 1000.0f) return juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
            return juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz";
        case Unit::ms:
            if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
            if (v < 1.0f)     return juce::String (v, 2) + " ms";
            if (v < 10.0f)    return juce::String (v, 1) + " ms";
            return juce::String (v, 0) + " ms";
        case Unit::Percent: return juce::String (v, 0) + " %";
        case Unit::Pan:
        {
            const int p = juce::roundToInt (v);
            if (p == 0) return "C";
            return (p < 0 ? "L" : "R") + juce::String (std::abs (p));
        }
        case Unit::Signed:
            return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt (v));
        case Unit::None:
        default:            return juce::String (v, 1);
    }
}

juce::String ParamSpec::shortText (float v) const
{
    if (unit == Unit::Choice)
        return formatReal (v);

    if (unit != Unit::Toggle && isOff (v))
        return "Off";

    switch (unit)
    {
        case Unit::Hz:      return v >= 1000.0f ? trimNumber (v / 1000.0f, 1) + "k" : trimNumber (v, 0);
        case Unit::dB:      return (v > 0.0f ? "+" : "") + trimNumber (v, 1);
        case Unit::ms:      return v >= 1000.0f ? trimNumber (v / 1000.0f, 1) + "s" : trimNumber (v, 2);
        case Unit::Percent: return trimNumber (v, 0);
        case Unit::Pan:     return v < 0 ? "L" : (v > 0 ? "R" : "C");
        case Unit::Signed:  return (v > 0 ? "+" : "") + trimNumber (v, 0);
        default:            return trimNumber (v, 1);
    }
}

float ParamSpec::fromText (const juce::String& textIn) const
{
    auto text = textIn.trim();

    if (unit == Unit::Choice)
    {
        for (int i = 0; i < choices.size(); ++i)
            if (choices[i].equalsIgnoreCase (text))
                return toNorm ((float) i);
        return toNorm ((float) text.getIntValue());
    }

    if (unit == Unit::Toggle)
        return (text.equalsIgnoreCase ("on") || text.getFloatValue() >= 0.5f) ? 1.0f : 0.0f;

    if (text.equalsIgnoreCase ("off"))
        return offBelow > -1.0e8f ? 0.0f : 1.0f;

    if (unit == Unit::Pan)
    {
        const auto up = text.toUpperCase();
        if (up == "C") return toNorm (0.0f);
        if (up.startsWith ("L")) return toNorm (-up.substring (1).getFloatValue());
        if (up.startsWith ("R")) return toNorm (up.substring (1).getFloatValue());
    }

    float v = text.retainCharacters ("0123456789.-+").getFloatValue();
    const auto lower = text.toLowerCase();

    if (unit == Unit::Hz && (lower.endsWith ("k") || lower.contains ("khz")))
        v *= 1000.0f;
    if (unit == Unit::ms && lower.endsWith ("s") && ! lower.endsWith ("ms"))
        v *= 1000.0f;

    return toNorm (v);
}

//==============================================================================
namespace
{
    ParamSpec knob (const char* name, const char* label, Unit u, float mn, float mx, float def,
                    KnobColour c = KnobColour::Black, Ctl ctl = Ctl::Knob, bool log = false)
    {
        ParamSpec p;
        p.name = name; p.label = label; p.unit = u;
        p.minV = mn; p.maxV = mx; p.defV = def;
        p.colour = c; p.ctl = ctl; p.logScale = log;
        return p;
    }

    ParamSpec logKnob (const char* name, const char* label, Unit u, float mn, float mx, float def,
                       KnobColour c = KnobColour::Black, Ctl ctl = Ctl::Knob)
    {
        return knob (name, label, u, mn, mx, def, c, ctl, true);
    }

    ParamSpec choice (const char* name, const char* label, juce::StringArray items, int def,
                      KnobColour c = KnobColour::Black, Ctl ctl = Ctl::Selector)
    {
        ParamSpec p;
        p.name = name; p.label = label; p.unit = Unit::Choice;
        p.choices = items; p.minV = 0; p.maxV = (float) (items.size() - 1); p.defV = (float) def;
        p.colour = c; p.ctl = ctl;
        return p;
    }

    ParamSpec toggle (const char* name, const char* label, bool def)
    {
        ParamSpec p;
        p.name = name; p.label = label; p.unit = Unit::Toggle;
        p.minV = 0; p.maxV = 1; p.defV = def ? 1.0f : 0.0f;
        p.ctl = Ctl::Toggle;
        return p;
    }

    ParamSpec withOffBelow (ParamSpec p, float v) { p.offBelow = v; return p; }
    ParamSpec withOffAbove (ParamSpec p, float v) { p.offAbove = v; return p; }

    juce::Colour rgb (juce::uint32 c) { return juce::Colour (0xff000000 | c); }

    std::vector<ModuleInfo> buildModules()
    {
        std::vector<ModuleInfo> m;

        //== 0: Empty ==========================================================
        {
            ModuleInfo e;
            e.id = "empty"; e.name = "Empty"; e.shortName = "Empty";
            e.panelTop = rgb (0x202124); e.panelBottom = rgb (0x18191b);
            e.ink = rgb (0x8a8d93); e.accent = rgb (0xe0a92c);
            m.push_back (e);
        }

        //== 1: Analog Channel ================================================
        {
            ModuleInfo x;
            x.id = "channel"; x.name = "Analog Channel"; x.shortName = "Channel";
            x.subtitle = "CONSOLE SATURATION";
            x.description = "Console channel emulation: drive the input into six different analog circuit characters.";
            x.category = Category::Saturation;
            x.panelTop = rgb (0x2a2b2e); x.panelBottom = rgb (0x151618);
            x.ink = rgb (0xe8e4da); x.accent = rgb (0xf2b13a);
            x.meter = MeterType::VU; x.meterLabel = "VU";
            x.params = {
                knob ("Input", "INPUT", Unit::dB, -18, 18, 0, KnobColour::Black),
                knob ("Drive", "DRIVE", Unit::None, 0, 10, 3, KnobColour::Black),
                choice ("Character", "CHARACTER", { "Clean", "Iron", "Glass", "Tube", "Punch", "Silk" }, 1,
                        KnobColour::Black, Ctl::Selector),
                choice ("Low Cut", "LOW CUT", { "Off", "20", "40", "80", "120" }, 0, KnobColour::Black),
                knob ("Tone", "TONE", Unit::dB, -5, 5, 0, KnobColour::Black),
                knob ("Output", "OUTPUT", Unit::dB, -18, 18, 0, KnobColour::Black),
            };
            x.rows = { { 0, 5 }, { 2 }, { 1, 4 }, { 3 } };
            m.push_back (x);
        }

        //== 2: Quad EQ =======================================================
        {
            ModuleInfo x;
            x.id = "quadeq"; x.name = "Quad EQ"; x.shortName = "Quad EQ";
            x.subtitle = "4-BAND CONSOLE EQ";
            x.description = "Four band console-style equaliser with switchable shelves, filters and proportional Q.";
            x.category = Category::EQ;
            x.panelTop = rgb (0x34363a); x.panelBottom = rgb (0x1d1e21);
            x.ink = rgb (0xf1f1f1); x.accent = rgb (0xe0402e);
            x.params = {
                knob ("HF Gain", "HF", Unit::dB, -15, 15, 0, KnobColour::Red, Ctl::Knob),
                logKnob ("HF Freq", "kHz", Unit::Hz, 1500, 16000, 8000, KnobColour::Red, Ctl::SmallKnob),
                toggle ("HF Bell", "BELL", false),
                knob ("HMF Gain", "HMF", Unit::dB, -15, 15, 0, KnobColour::Orange, Ctl::Knob),
                logKnob ("HMF Freq", "kHz", Unit::Hz, 600, 7000, 2500, KnobColour::Orange, Ctl::SmallKnob),
                logKnob ("HMF Q", "Q", Unit::None, 0.3f, 3.0f, 0.7f, KnobColour::Orange, Ctl::SmallKnob),
                knob ("LMF Gain", "LMF", Unit::dB, -15, 15, 0, KnobColour::Green, Ctl::Knob),
                logKnob ("LMF Freq", "kHz", Unit::Hz, 200, 2500, 600, KnobColour::Green, Ctl::SmallKnob),
                logKnob ("LMF Q", "Q", Unit::None, 0.3f, 3.0f, 0.7f, KnobColour::Green, Ctl::SmallKnob),
                knob ("LF Gain", "LF", Unit::dB, -15, 15, 0, KnobColour::Blue, Ctl::Knob),
                logKnob ("LF Freq", "Hz", Unit::Hz, 30, 450, 100, KnobColour::Blue, Ctl::SmallKnob),
                toggle ("LF Bell", "BELL", false),
                withOffBelow (logKnob ("High Pass", "HPF", Unit::Hz, 16, 500, 16, KnobColour::Grey, Ctl::SmallKnob), 18.0f),
                withOffAbove (logKnob ("Low Pass", "LPF", Unit::Hz, 3000, 22000, 22000, KnobColour::Grey, Ctl::SmallKnob), 21000.0f),
                choice ("EQ Type", "TYPE", { "Const Q", "Prop Q" }, 0, KnobColour::Grey, Ctl::Selector),
                knob ("Output", "OUTPUT", Unit::dB, -12, 12, 0, KnobColour::Grey, Ctl::SmallKnob),
            };
            x.rows = { { 12, 13 }, { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 }, { 9, 10, 11 }, { 14, 15 } };
            m.push_back (x);
        }

        //== 3: Heritage EQ ===================================================
        {
            ModuleInfo x;
            x.id = "heritageeq"; x.name = "Heritage EQ"; x.shortName = "Heritage";
            x.subtitle = "CLASS-A INDUCTOR EQ";
            x.description = "Musical class-A style equaliser with stepped frequencies and a transformer drive stage.";
            x.category = Category::EQ;
            x.panelTop = rgb (0x2f5f80); x.panelBottom = rgb (0x1d405a);
            x.ink = rgb (0xeef4f8); x.accent = rgb (0xd8433a);
            x.params = {
                knob ("High Gain", "HIGH", Unit::dB, -16, 16, 0, KnobColour::Grey),
                choice ("High Freq", "kHz", { "10k", "12k", "16k" }, 1, KnobColour::Grey),
                knob ("Hi-Mid Gain", "HI MID", Unit::dB, -18, 18, 0, KnobColour::Grey),
                choice ("Hi-Mid Freq", "kHz", { "1.6k", "3.2k", "4.8k", "7.2k" }, 1, KnobColour::Grey),
                knob ("Lo-Mid Gain", "LO MID", Unit::dB, -18, 18, 0, KnobColour::Grey),
                choice ("Lo-Mid Freq", "Hz", { "360", "700", "1.6k", "3.2k" }, 1, KnobColour::Grey),
                knob ("Low Gain", "LOW", Unit::dB, -16, 16, 0, KnobColour::Grey),
                choice ("Low Freq", "Hz", { "35", "60", "110", "220" }, 1, KnobColour::Grey),
                choice ("High Pass", "HPF", { "Off", "50", "80", "160", "300" }, 0, KnobColour::Blue),
                knob ("Drive", "DRIVE", Unit::dB, 0, 20, 6, KnobColour::Red),
                toggle ("Drive On", "DRIVE", false),
                knob ("Output", "LINE", Unit::dB, -12, 12, 0, KnobColour::Red),
            };
            x.rows = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 8, 10, 9 }, { 11 } };
            m.push_back (x);
        }

        //== 4: Program EQ ====================================================
        {
            ModuleInfo x;
            x.id = "programeq"; x.name = "Program EQ"; x.shortName = "Program";
            x.subtitle = "PASSIVE TUBE PROGRAM EQ";
            x.description = "Passive-style program equaliser: boost and cut the same band for the classic low-end trick.";
            x.category = Category::EQ;
            x.panelTop = rgb (0x5d6f7c); x.panelBottom = rgb (0x45545f);
            x.ink = rgb (0xf6f3ea); x.accent = rgb (0xe6c15a);
            x.params = {
                choice ("Low Freq", "CPS", { "20", "30", "60", "100" }, 2, KnobColour::Black),
                knob ("Low Boost", "BOOST", Unit::None, 0, 10, 0, KnobColour::Black),
                knob ("Low Atten", "ATTEN", Unit::None, 0, 10, 0, KnobColour::Black),
                choice ("High Freq", "KCS", { "3k", "4k", "5k", "8k", "10k", "12k", "16k" }, 4, KnobColour::Black),
                knob ("High Boost", "BOOST", Unit::None, 0, 10, 0, KnobColour::Black),
                knob ("Bandwidth", "BANDWIDTH", Unit::None, 0, 10, 5, KnobColour::Black),
                choice ("Atten Freq", "ATTEN SEL", { "5k", "10k", "20k" }, 1, KnobColour::Black),
                knob ("High Atten", "ATTEN", Unit::None, 0, 10, 0, KnobColour::Black),
                knob ("Tube Drive", "TUBE", Unit::None, 0, 10, 2, KnobColour::Gold),
                knob ("Output", "OUTPUT", Unit::dB, -12, 12, 0, KnobColour::Gold),
            };
            x.rows = { { 1, 0, 2 }, { 4, 3, 5 }, { 7, 6 }, { 8, 9 } };
            m.push_back (x);
        }

        //== 5: Bus Comp ======================================================
        {
            ModuleInfo x;
            x.id = "buscomp"; x.name = "Bus Comp"; x.shortName = "Bus Comp";
            x.subtitle = "VCA BUS COMPRESSOR";
            x.description = "Glue compressor for buses and the mix: stepped attack/release with auto release.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0x3b3d42); x.panelBottom = rgb (0x24262a);
            x.ink = rgb (0xf0f0f0); x.accent = rgb (0xe2a93b);
            x.meter = MeterType::GainReduction; x.meterLabel = "GAIN REDUCTION";
            x.params = {
                knob ("Threshold", "THRESHOLD", Unit::dB, -40, 0, -12, KnobColour::White, Ctl::BigKnob),
                choice ("Ratio", "RATIO", { "1.5", "2", "4", "10" }, 2, KnobColour::Black),
                choice ("Attack", "ATTACK", { "0.1", "0.3", "1", "3", "10", "30" }, 4, KnobColour::Black),
                choice ("Release", "RELEASE", { "0.1", "0.3", "0.6", "1.2", "Auto" }, 4, KnobColour::Black),
                knob ("Makeup", "MAKEUP", Unit::dB, 0, 20, 0, KnobColour::White, Ctl::BigKnob),
                withOffBelow (logKnob ("SC High Pass", "S/C HPF", Unit::Hz, 20, 500, 20, KnobColour::Grey, Ctl::SmallKnob), 21.0f),
                knob ("Mix", "MIX", Unit::Percent, 0, 100, 100, KnobColour::Grey, Ctl::SmallKnob),
                toggle ("External SC", "EXT S/C", false),
            };
            x.rows = { { 0, 4 }, { 1, 2, 3 }, { 5, 6 }, { 7 } };
            m.push_back (x);
        }

        //== 6: Stress Comp ===================================================
        {
            ModuleInfo x;
            x.id = "stresscomp"; x.name = "Stress Comp"; x.shortName = "Stress";
            x.subtitle = "FET PEAK COMPRESSOR";
            x.description = "Fast FET-style compressor with an all-in 'Nuke' ratio and switchable distortion.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0x232427); x.panelBottom = rgb (0x101113);
            x.ink = rgb (0xf3f1ea); x.accent = rgb (0xff4b3a);
            x.meter = MeterType::LedLadder; x.meterLabel = "GR";
            x.params = {
                knob ("Input", "INPUT", Unit::dB, -12, 36, 0, KnobColour::Cream, Ctl::Dial),
                knob ("Output", "OUTPUT", Unit::dB, -24, 12, 0, KnobColour::Cream, Ctl::Dial),
                logKnob ("Attack", "ATTACK", Unit::ms, 0.02f, 0.8f, 0.2f, KnobColour::Cream, Ctl::Dial),
                logKnob ("Release", "RELEASE", Unit::ms, 50, 1100, 300, KnobColour::Cream, Ctl::Dial),
                choice ("Ratio", "RATIO", { "2", "4", "8", "12", "20", "Nuke" }, 1, KnobColour::Black),
                knob ("Mix", "MIX", Unit::Percent, 0, 100, 100, KnobColour::Grey, Ctl::SmallKnob),
                toggle ("Detector HP", "DET HP", false),
                knob ("Distortion", "DIST", Unit::None, 0, 10, 0, KnobColour::Grey, Ctl::SmallKnob),
                toggle ("External SC", "EXT S/C", false),
            };
            x.rows = { { 0, 1 }, { 2, 3 }, { 4 }, { 7, 5 }, { 6, 8 } };
            m.push_back (x);
        }

        //== 7: Opto Comp =====================================================
        {
            ModuleInfo x;
            x.id = "optocomp"; x.name = "Opto Comp"; x.shortName = "Opto";
            x.subtitle = "OPTICAL LEVELING AMP";
            x.description = "Smooth program-dependent optical leveler with tube make-up stage.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0xb9bcc0); x.panelBottom = rgb (0x8f9398);
            x.ink = rgb (0x1b1c1e); x.accent = rgb (0xb3261e);
            x.meter = MeterType::GainReduction; x.meterLabel = "GAIN REDUCTION";
            x.params = {
                knob ("Peak Reduction", "PEAK REDUCTION", Unit::None, 0, 100, 30, KnobColour::Black, Ctl::BigKnob),
                knob ("Gain", "GAIN", Unit::dB, -10, 30, 0, KnobColour::Black, Ctl::BigKnob),
                choice ("Mode", "MODE", { "Compress", "Limit" }, 0, KnobColour::Black),
                knob ("Emphasis", "EMPHASIS", Unit::Percent, 0, 100, 0, KnobColour::Black, Ctl::SmallKnob),
                knob ("Tube Color", "COLOR", Unit::None, 0, 10, 2, KnobColour::Black, Ctl::SmallKnob),
                knob ("Mix", "MIX", Unit::Percent, 0, 100, 100, KnobColour::Black, Ctl::SmallKnob),
                toggle ("External SC", "EXT S/C", false),
            };
            x.rows = { { 0 }, { 1 }, { 2, 3 }, { 4, 5, 6 } };
            m.push_back (x);
        }

        //== 8: Tape ==========================================================
        {
            ModuleInfo x;
            x.id = "tape"; x.name = "Tape Machine"; x.shortName = "Tape";
            x.subtitle = "ANALOG TAPE SATURATION";
            x.description = "Two-track tape: head bump, HF compression, saturation and wow & flutter.";
            x.category = Category::Saturation;
            x.panelTop = rgb (0x5a3d2b); x.panelBottom = rgb (0x382419);
            x.ink = rgb (0xf5e7cf); x.accent = rgb (0xf0b04a);
            x.meter = MeterType::VU; x.meterLabel = "VU";
            x.params = {
                knob ("Input", "INPUT", Unit::dB, -12, 18, 0, KnobColour::Cream),
                knob ("Saturation", "SATURATION", Unit::None, 0, 10, 4, KnobColour::Cream),
                choice ("Speed", "SPEED", { "7.5 ips", "15 ips", "30 ips" }, 2, KnobColour::Black),
                toggle ("Head Bump", "BUMP", true),
                knob ("Wow & Flutter", "WOW", Unit::None, 0, 10, 0, KnobColour::Cream, Ctl::SmallKnob),
                knob ("Output", "OUTPUT", Unit::dB, -18, 18, 0, KnobColour::Cream),
                knob ("Mix", "MIX", Unit::Percent, 0, 100, 100, KnobColour::Cream, Ctl::SmallKnob),
                toggle ("Auto Gain", "AUTO GAIN", true),
            };
            x.rows = { { 0, 5 }, { 1, 2 }, { 4, 6 }, { 3, 7 } };
            m.push_back (x);
        }

        //== 9: Gate ==========================================================
        {
            ModuleInfo x;
            x.id = "gate"; x.name = "Gate"; x.shortName = "Gate";
            x.subtitle = "GATE / EXPANDER";
            x.description = "Noise gate and downward expander with hold and side-chain filters.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0x234a46); x.panelBottom = rgb (0x14302d);
            x.ink = rgb (0xe6f4f1); x.accent = rgb (0x58e0b6);
            x.meter = MeterType::GainReduction; x.meterLabel = "ATTENUATION";
            x.params = {
                knob ("Threshold", "THRESHOLD", Unit::dB, -80, 0, -40, KnobColour::Grey, Ctl::BigKnob),
                knob ("Range", "RANGE", Unit::dB, 0, 80, 40, KnobColour::Grey, Ctl::BigKnob),
                logKnob ("Attack", "ATTACK", Unit::ms, 0.05f, 50, 0.5f, KnobColour::Grey, Ctl::SmallKnob),
                knob ("Hold", "HOLD", Unit::ms, 0, 500, 20, KnobColour::Grey, Ctl::SmallKnob),
                logKnob ("Release", "RELEASE", Unit::ms, 5, 2000, 150, KnobColour::Grey, Ctl::SmallKnob),
                withOffBelow (logKnob ("SC High Pass", "S/C HPF", Unit::Hz, 20, 2000, 20, KnobColour::Black, Ctl::SmallKnob), 21.0f),
                withOffAbove (logKnob ("SC Low Pass", "S/C LPF", Unit::Hz, 1000, 20000, 20000, KnobColour::Black, Ctl::SmallKnob), 19500.0f),
                toggle ("External SC", "EXT S/C", false),
                choice ("Mode", "MODE", { "Gate", "Expander" }, 0, KnobColour::Black),
            };
            x.rows = { { 0, 1 }, { 2, 3, 4 }, { 5, 6 }, { 8, 7 } };
            m.push_back (x);
        }

        //== 10: De-Esser =====================================================
        {
            ModuleInfo x;
            x.id = "deesser"; x.name = "De-Esser"; x.shortName = "De-Esser";
            x.subtitle = "SIBILANCE CONTROL";
            x.description = "Split-band or wide-band de-esser with listen mode.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0x3b2f4f); x.panelBottom = rgb (0x231b31);
            x.ink = rgb (0xefe9f8); x.accent = rgb (0xb88cff);
            x.meter = MeterType::GainReduction; x.meterLabel = "REDUCTION";
            x.params = {
                logKnob ("Frequency", "FREQUENCY", Unit::Hz, 2000, 16000, 6000, KnobColour::Chrome, Ctl::BigKnob),
                knob ("Threshold", "THRESHOLD", Unit::dB, -60, 0, -24, KnobColour::Chrome),
                knob ("Range", "RANGE", Unit::dB, 0, 24, 10, KnobColour::Chrome),
                choice ("Mode", "MODE", { "Split", "Wide" }, 0, KnobColour::Black),
                toggle ("Listen", "LISTEN", false),
            };
            x.rows = { { 0 }, { 1, 2 }, { 3, 4 } };
            m.push_back (x);
        }

        //== 11: Limiter ======================================================
        {
            ModuleInfo x;
            x.id = "limiter"; x.name = "Limiter"; x.shortName = "Limiter";
            x.subtitle = "ZERO-LATENCY PEAK LIMITER";
            x.description = "Brickwall peak limiter with selectable character and stereo link.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0x2b1d1f); x.panelBottom = rgb (0x170f10);
            x.ink = rgb (0xf6e9e9); x.accent = rgb (0xff3d4d);
            x.meter = MeterType::GainReduction; x.meterLabel = "GAIN REDUCTION";
            x.params = {
                knob ("Gain", "GAIN", Unit::dB, 0, 24, 0, KnobColour::Red, Ctl::BigKnob),
                knob ("Ceiling", "CEILING", Unit::dB, -12, 0, -0.3f, KnobColour::Grey, Ctl::BigKnob),
                logKnob ("Release", "RELEASE", Unit::ms, 1, 1000, 60, KnobColour::Grey),
                choice ("Character", "CHARACTER", { "Clean", "Soft", "Loud" }, 1, KnobColour::Black),
                knob ("Stereo Link", "LINK", Unit::Percent, 0, 100, 100, KnobColour::Grey, Ctl::SmallKnob),
            };
            x.rows = { { 0, 1 }, { 2, 3 }, { 4 } };
            m.push_back (x);
        }

        //== 12: Transient ====================================================
        {
            ModuleInfo x;
            x.id = "transient"; x.name = "Transient Shaper"; x.shortName = "Transient";
            x.subtitle = "ATTACK / SUSTAIN DESIGNER";
            x.description = "Level-independent transient designer.";
            x.category = Category::Dynamics;
            x.panelTop = rgb (0x5a3714); x.panelBottom = rgb (0x37210b);
            x.ink = rgb (0xfdf0dc); x.accent = rgb (0xffa53a);
            x.meter = MeterType::Level; x.meterLabel = "OUTPUT";
            x.params = {
                knob ("Attack", "ATTACK", Unit::Signed, -100, 100, 0, KnobColour::Orange, Ctl::BigKnob),
                knob ("Sustain", "SUSTAIN", Unit::Signed, -100, 100, 0, KnobColour::Orange, Ctl::BigKnob),
                choice ("Speed", "SPEED", { "Fast", "Medium", "Slow" }, 1, KnobColour::Black),
                knob ("Output", "OUTPUT", Unit::dB, -12, 12, 0, KnobColour::Black),
                knob ("Mix", "MIX", Unit::Percent, 0, 100, 100, KnobColour::Black),
            };
            x.rows = { { 0 }, { 1 }, { 2 }, { 3, 4 } };
            m.push_back (x);
        }

        //== 13: Air Lift =====================================================
        {
            ModuleInfo x;
            x.id = "airlift"; x.name = "Air Lift"; x.shortName = "Air Lift";
            x.subtitle = "AIR & EARTH ENHANCER";
            x.description = "Open up the top with a smooth air shelf and harmonic exciter; add weight with the earth band.";
            x.category = Category::EQ;
            x.panelTop = rgb (0xefece4); x.panelBottom = rgb (0xd4d0c6);
            x.ink = rgb (0x2a2b2d); x.accent = rgb (0x3d7bd9);
            x.params = {
                logKnob ("Air Freq", "AIR FREQ", Unit::Hz, 2500, 20000, 10000, KnobColour::Chrome, Ctl::SmallKnob),
                knob ("Air Gain", "AIR", Unit::dB, 0, 12, 0, KnobColour::Chrome, Ctl::BigKnob),
                knob ("Harmonics", "HARMONICS", Unit::None, 0, 10, 0, KnobColour::Chrome),
                logKnob ("Earth Freq", "EARTH FREQ", Unit::Hz, 20, 250, 60, KnobColour::Chrome, Ctl::SmallKnob),
                knob ("Earth Gain", "EARTH", Unit::dB, -12, 12, 0, KnobColour::Chrome, Ctl::BigKnob),
                knob ("Output", "OUTPUT", Unit::dB, -12, 12, 0, KnobColour::Chrome, Ctl::SmallKnob),
            };
            x.rows = { { 1, 0 }, { 2 }, { 4, 3 }, { 5 } };
            m.push_back (x);
        }

        //== 14: Trimmer ======================================================
        {
            ModuleInfo x;
            x.id = "trimmer"; x.name = "Trimmer"; x.shortName = "Trimmer";
            x.subtitle = "GAIN / PAN / WIDTH";
            x.description = "Utility: gain, balance, stereo width, bass mono, polarity and channel tools.";
            x.category = Category::Utility;
            x.panelTop = rgb (0x303236); x.panelBottom = rgb (0x1e1f22);
            x.ink = rgb (0xeceff3); x.accent = rgb (0x4fb3ff);
            x.meter = MeterType::Level; x.meterLabel = "LEVEL";
            x.params = {
                knob ("Gain", "GAIN", Unit::dB, -24, 24, 0, KnobColour::Blue, Ctl::BigKnob),
                knob ("Pan", "PAN", Unit::Pan, -100, 100, 0, KnobColour::Grey),
                knob ("Width", "WIDTH", Unit::Percent, 0, 200, 100, KnobColour::Grey),
                withOffBelow (logKnob ("Mono Bass", "MONO BASS", Unit::Hz, 20, 300, 20, KnobColour::Grey, Ctl::SmallKnob), 21.0f),
                toggle ("Phase L", "INV L", false),
                toggle ("Phase R", "INV R", false),
                toggle ("Mono", "MONO", false),
                toggle ("Swap L/R", "SWAP", false),
            };
            x.rows = { { 0 }, { 1, 2 }, { 3 }, { 4, 5 }, { 6, 7 } };
            m.push_back (x);
        }

        //== 15: Harmonics ====================================================
        {
            ModuleInfo x;
            x.id = "harmonics"; x.name = "Harmonics"; x.shortName = "Harmonics";
            x.subtitle = "MULTI-MODE SATURATOR";
            x.description = "Saturator with tube, tape, transistor, clip and wave-fold curves.";
            x.category = Category::Saturation;
            x.panelTop = rgb (0x5b1a1d); x.panelBottom = rgb (0x350d10);
            x.ink = rgb (0xfbe9e6); x.accent = rgb (0xffb03a);
            x.meter = MeterType::Level; x.meterLabel = "OUTPUT";
            x.params = {
                knob ("Drive", "DRIVE", Unit::dB, 0, 24, 6, KnobColour::Gold, Ctl::BigKnob),
                choice ("Type", "TYPE", { "Tube", "Tape", "Transistor", "Clip", "Fold" }, 0, KnobColour::Black),
                knob ("Tone", "TONE", Unit::Signed, -100, 100, 0, KnobColour::Black),
                knob ("Output", "OUTPUT", Unit::dB, -18, 18, 0, KnobColour::Black),
                knob ("Mix", "MIX", Unit::Percent, 0, 100, 100, KnobColour::Black),
                toggle ("Auto Gain", "AUTO GAIN", true),
            };
            x.rows = { { 0 }, { 1 }, { 2, 4 }, { 3, 5 } };
            m.push_back (x);
        }

        for ([[maybe_unused]] auto& mod : m)
            jassert ((int) mod.params.size() <= kParamsPerSlot);

        return m;
    }
}

const std::vector<ModuleInfo>& getModules()
{
    static const std::vector<ModuleInfo> modules = buildModules();
    return modules;
}

int numModuleTypes() { return (int) getModules().size(); }

int findModuleById (const juce::String& id)
{
    const auto& m = getModules();
    for (int i = 0; i < (int) m.size(); ++i)
        if (m[(size_t) i].id == id)
            return i;
    return 0;
}

juce::StringArray getModuleNames()
{
    juce::StringArray s;
    for (auto& m : getModules())
        s.add (m.name);
    return s;
}

juce::String categoryName (Category c)
{
    switch (c)
    {
        case Category::Dynamics:   return "Dyn";
        case Category::EQ:         return "EQ";
        case Category::Saturation: return "Sat";
        case Category::Utility:    return "Util";
    }
    return {};
}

juce::String slotLetter (int slot)            { return juce::String::charToString ((juce::juce_wchar) ('A' + slot)); }
juce::String slotParamId (int slot, const char* suffix) { return slotLetter (slot) + "_" + suffix; }
juce::String slotKnobId (int slot, int index)  { return slotLetter (slot) + "_p" + juce::String (index + 1); }
juce::String macroId (int macro)               { return "macro" + juce::String (macro + 1); }

} // namespace mr
