#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <cmath>

namespace mr::ui
{
//==============================================================================
/** Rotary control. Visual style is chosen with setStyle():
    "knob", "big", "small", "dial", "selector", "macro", "mini". */
class Knob : public juce::Slider
{
public:
    Knob();

    void setStyle (const juce::String& s)       { getProperties().set ("style", s); repaint(); }
    juce::String getStyle() const               { return getProperties()["style"].toString(); }
    void setKnobColour (KnobColour c)           { getProperties().set ("colour", (int) c); }
    void setInk (juce::Colour c)                { getProperties().set ("ink", (juce::int64) c.getARGB()); }
    void setChoices (const juce::StringArray& c) { choices = c; getProperties().set ("steps", c.size()); }

    juce::StringArray choices;
    int numSteps = 0;   // > 1: user drags snap to this many positions

    double snapValue (double attempted, DragMode) override
    {
        if (numSteps > 1)
            return std::round (attempted * (numSteps - 1)) / (double) (numSteps - 1);
        return attempted;
    }

    // Macro display (normalised values)
    int macro = -1;
    float macroLo = 0.0f, macroHi = 1.0f, macroValue = 0.0f;
    void setMacroDisplay (int m, float lo, float hi, float value);

    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    bool rightClicked = false;
};

//==============================================================================
/** Hardware style latching push button with LED. */
class LampButton : public juce::Button
{
public:
    explicit LampButton (const juce::String& text);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::Colour ledColour { col::red };
    juce::Colour panelInk { col::text };
};

//==============================================================================
/** Small icon button used in the top bar and slot headers. */
class IconButton : public juce::Button
{
public:
    enum class Icon { Undo, Redo, Save, Trash, Menu, Left, Right, Power, Close, Copy, Plus, Folder, Gear };

    IconButton (Icon i, const juce::String& tip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::Colour onColour { col::accent };
    bool drawFrame = true;

private:
    Icon icon;
};

//==============================================================================
/** Analog style needle meter: VU (level) or gain-reduction. */
class NeedleMeter : public juce::Component
{
public:
    explicit NeedleMeter (bool gainReduction);
    void setValue (float v);            // dB level or dB gain reduction
    void paint (juce::Graphics&) override;
    juce::String label;

private:
    bool grMode;
    float display = 0.0f;
};

/** LED ladder for gain reduction. */
class LedLadder : public juce::Component
{
public:
    void setValue (float grDb);
    void paint (juce::Graphics&) override;
    juce::Colour ink { col::text };

private:
    float value = 0.0f;
};

/** Horizontal level bar (peak dB). */
class LevelBar : public juce::Component
{
public:
    void setValue (float db);
    void paint (juce::Graphics&) override;
    juce::String label;
    juce::Colour ink { col::text };

private:
    float value = -100.0f;
};

/** Vertical stereo-ish level meter used for the master in/out. */
class MiniMeter : public juce::Component
{
public:
    void setValue (float db);
    void paint (juce::Graphics&) override;

private:
    float value = -100.0f;
};

} // namespace mr::ui
