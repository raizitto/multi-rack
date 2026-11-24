#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace mr
{
constexpr int kNumSlots      = 8;
constexpr int kParamsPerSlot = 16;
constexpr int kNumMacros     = 8;

enum class Unit { None, dB, Hz, ms, Percent, Choice, Toggle, Pan, Signed };

enum class Ctl { Knob, BigKnob, SmallKnob, Dial, Selector, Toggle };

enum class KnobColour { Black, Grey, White, Red, Orange, Green, Blue, Cream, Chrome, Gold };

enum class Category { Dynamics, EQ, Saturation, Utility };

enum class MeterType { None, VU, GainReduction, LedLadder, Level };

struct ParamSpec
{
    juce::String name;          // full name (host / tooltips)
    juce::String label;         // short label printed on the panel
    Unit unit = Unit::None;
    float minV = 0.0f, maxV = 1.0f, defV = 0.0f;
    bool logScale = false;
    juce::StringArray choices;  // for Unit::Choice
    float offBelow = -1.0e9f;   // real values below this print "Off"
    float offAbove =  1.0e9f;   // real values above this print "Off"
    Ctl ctl = Ctl::Knob;
    KnobColour colour = KnobColour::Black;

    float toReal (float norm) const;
    float toNorm (float real) const;
    float defaultNorm() const   { return toNorm (defV); }
    int   numChoices() const    { return choices.size(); }
    bool  isOff (float real) const { return real < offBelow || real > offAbove; }
    juce::String formatReal (float real) const;
    juce::String toText (float norm) const { return formatReal (toReal (norm)); }
    juce::String shortText (float real) const; // compact tick label (e.g. "16k")
    float fromText (const juce::String& text) const; // returns normalised
};

struct ModuleInfo
{
    juce::String id;           // stable id used in presets
    juce::String name;         // display name
    juce::String shortName;    // tab label
    juce::String subtitle;
    juce::String description;
    Category category = Category::Utility;
    juce::Colour panelTop, panelBottom, ink, accent;
    bool brushed = true;
    MeterType meter = MeterType::None;
    juce::String meterLabel;
    std::vector<ParamSpec> params;
    std::vector<std::vector<int>> rows; // panel layout, rows of param indices
};

// Index 0 is the "Empty" slot. Order is stable (stored in sessions): only append new modules.
const std::vector<ModuleInfo>& getModules();
int numModuleTypes();
int findModuleById (const juce::String& id);
juce::StringArray getModuleNames();
juce::String categoryName (Category c);

juce::String slotLetter (int slot);
juce::String slotParamId (int slot, const char* suffix);
juce::String slotKnobId (int slot, int index);
juce::String macroId (int macro);

} // namespace mr
