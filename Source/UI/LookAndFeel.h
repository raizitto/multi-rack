#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModuleDefs.h"

namespace mr::ui
{
namespace col
{
    const juce::Colour bg        { 0xff0f1012 };
    const juce::Colour bar       { 0xff17181b };
    const juce::Colour panel     { 0xff1c1d21 };
    const juce::Colour panelHi   { 0xff26282d };
    const juce::Colour border    { 0xff33363c };
    const juce::Colour text      { 0xffdcdde1 };
    const juce::Colour dim       { 0xff8c9098 };
    const juce::Colour accent    { 0xfff0a830 };
    const juce::Colour accentDim { 0xff7a5717 };
    const juce::Colour red       { 0xffe5483b };
    const juce::Colour green     { 0xff4fd17a };
}

juce::Font font (float height, bool bold = false);
juce::Colour knobColour (KnobColour c);

void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius, float angle);
void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, juce::Colour accent);

/** Look and feel for the whole plugin: hardware style knobs, dark UI chrome. */
class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return font (13.0f, true); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }

    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;

private:
    void drawSelector (juce::Graphics&, juce::Rectangle<float> area, float angle, juce::Slider&,
                       float pos, float startAngle, float endAngle);
    void drawDial (juce::Graphics&, juce::Rectangle<float> area, float angle, juce::Slider&,
                   float startAngle, float endAngle);
    void drawMacroKnob (juce::Graphics&, juce::Rectangle<float> area, float pos, float angle,
                        float startAngle, float endAngle, juce::Slider&);
};

} // namespace mr::ui
