#include "LookAndFeel.h"
#include "Widgets.h"

namespace mr::ui
{
juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour knobColour (KnobColour c)
{
    switch (c)
    {
        case KnobColour::Black:  return juce::Colour (0xff1e1f22);
        case KnobColour::Grey:   return juce::Colour (0xff9aa1a8);
        case KnobColour::White:  return juce::Colour (0xffe9e9e6);
        case KnobColour::Red:    return juce::Colour (0xffd8312a);
        case KnobColour::Orange: return juce::Colour (0xffef8a1f);
        case KnobColour::Green:  return juce::Colour (0xff2f9a45);
        case KnobColour::Blue:   return juce::Colour (0xff2a62c9);
        case KnobColour::Cream:  return juce::Colour (0xffeee6cf);
        case KnobColour::Chrome: return juce::Colour (0xffc9ccd0);
        case KnobColour::Gold:   return juce::Colour (0xffc99a3a);
    }
    return juce::Colours::grey;
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r, c.y - r + 1.0f, r * 2.0f, r * 2.0f);
    juce::ColourGradient grad (juce::Colour (0xffb9bcc0), c.x - r, c.y - r,
                               juce::Colour (0xff4a4c50), c.x + r, c.y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);

    const float s = r * 0.62f;
    juce::Path cross;
    cross.addRectangle (-s, -0.9f, s * 2.0f, 1.8f);
    cross.addRectangle (-0.9f, -s, 1.8f, s * 2.0f);
    cross.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (juce::Colour (0xff2a2b2e));
    g.fillPath (cross);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, juce::Colour accent)
{
    // Original "stacked rack units" emblem: hexagon with three horizontal rack bars.
    const auto c = area.getCentre();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;

    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> p (c.x + r * std::cos (a), c.y + r * std::sin (a));
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (ink.withAlpha (0.9f));
    g.strokePath (hex, juce::PathStrokeType (juce::jmax (1.0f, r * 0.12f)));

    const float bw = r * 1.0f, bh = r * 0.2f;
    for (int i = -1; i <= 1; ++i)
    {
        g.setColour (i == 0 ? accent : ink.withAlpha (0.9f));
        g.fillRoundedRectangle (c.x - bw * 0.5f, c.y + (float) i * bh * 1.7f - bh * 0.5f, bw, bh, bh * 0.4f);
    }
}

//==============================================================================
RackLookAndFeel::RackLookAndFeel()
{
    auto& scheme = getCurrentColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, col::bg);
    scheme.setUIColour (ColourScheme::widgetBackground, col::panel);
    scheme.setUIColour (ColourScheme::menuBackground, col::panel);
    scheme.setUIColour (ColourScheme::outline, col::border);
    scheme.setUIColour (ColourScheme::defaultText, col::text);
    scheme.setUIColour (ColourScheme::defaultFill, col::accent);
    scheme.setUIColour (ColourScheme::highlightedText, juce::Colours::black);
    scheme.setUIColour (ColourScheme::highlightedFill, col::accent);
    scheme.setUIColour (ColourScheme::menuText, col::text);

    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::accent);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0c0d));
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId, col::border);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::ScrollBar::thumbColourId, col::border);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf0101113));
    setColour (juce::BubbleComponent::outlineColourId, col::accent);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0101113));
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::border);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::border);
}

//==============================================================================
static juce::Colour inkOf (juce::Slider& s)
{
    const auto v = s.getProperties()["ink"];
    if (v.isVoid()) return col::text;
    return juce::Colour ((juce::uint32) (juce::int64) v);
}

static void drawTickRing (juce::Graphics& g, juce::Point<float> c, float radius, int ticks,
                          float startAngle, float endAngle, juce::Colour ink, float dot)
{
    g.setColour (ink.withAlpha (0.85f));
    for (int t = 0; t < ticks; ++t)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) t / (float) juce::jmax (1, ticks - 1);
        const auto p = c.getPointOnCircumference (radius, a);
        const float d = (t == 0 || t == ticks - 1 || t == ticks / 2) ? dot * 1.35f : dot;
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }
}

static void drawMacroArc (juce::Graphics& g, juce::Point<float> c, float radius, float startAngle, float endAngle, Knob& k)
{
    if (k.macro < 0) return;
    const float a0 = startAngle + (endAngle - startAngle) * k.macroLo;
    const float a1 = startAngle + (endAngle - startAngle) * k.macroHi;
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
    g.setColour (col::accent.withAlpha (0.9f));
    g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float av = startAngle + (endAngle - startAngle) * k.macroValue;
    const auto p = c.getPointOnCircumference (radius, av);
    g.setColour (juce::Colours::white);
    g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
}

void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto style = slider.getProperties()["style"].toString();
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (style == "selector") { drawSelector (g, area, angle, slider, pos, startAngle, endAngle); return; }
    if (style == "dial")     { drawDial (g, area, angle, slider, startAngle, endAngle); return; }
    if (style == "macro" || style == "mini") { drawMacroKnob (g, area, pos, angle, startAngle, endAngle, slider); return; }

    const auto ink = inkOf (slider);
    const auto c = area.getCentre();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
    const auto capColour = knobColour ((KnobColour) (int) slider.getProperties().getWithDefault ("colour", 0));

    drawTickRing (g, c, r * 0.94f, 11, startAngle, endAngle, ink, juce::jmax (1.6f, r * 0.075f));

    // drop shadow
    const float skirtR = r * 0.76f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR + r * 0.09f, skirtR * 2.0f, skirtR * 2.0f);

    // metal skirt with knurled edge
    {
        juce::ColourGradient grad (juce::Colour (0xff6c6f74), c.x, c.y - skirtR,
                                   juce::Colour (0xff1c1d20), c.x, c.y + skirtR, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        const int ridges = 36;
        for (int i = 0; i < ridges; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (skirtR * 0.86f, a), c.getPointOnCircumference (skirtR, a)), 1.0f);
        }
    }

    // coloured cap
    const float capR = r * 0.58f;
    {
        juce::ColourGradient grad (capColour.brighter (0.55f), c.x - capR * 0.5f, c.y - capR * 0.7f,
                                   capColour.darker (0.55f), c.x + capR * 0.6f, c.y + capR, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (c.x - capR + 0.5f, c.y - capR + 0.5f, capR * 2.0f - 1.0f, capR * 2.0f - 1.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    }

    // pointer
    const bool lightCap = capColour.getPerceivedBrightness() > 0.6f;
    const auto pointerColour = lightCap ? juce::Colour (0xff141414) : juce::Colours::white;
    g.setColour (pointerColour);
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (capR * 0.25f, angle),
                                   c.getPointOnCircumference (skirtR * 0.97f, angle)),
                juce::jmax (2.0f, r * 0.085f));

    if (auto* k = dynamic_cast<Knob*> (&slider))
        drawMacroArc (g, c, r * 0.99f, startAngle, endAngle, *k);

    if (! slider.isEnabled())
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}

void RackLookAndFeel::drawSelector (juce::Graphics& g, juce::Rectangle<float> area, float angle, juce::Slider& slider,
                                    float pos, float startAngle, float endAngle)
{
    const auto ink = inkOf (slider);
    const auto c = area.getCentre();
    const float full = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float spanX = area.getWidth() * 0.5f;

    juce::StringArray choices;
    if (auto* k = dynamic_cast<Knob*> (&slider))
        choices = k->choices;

    const int steps = juce::jmax (2, choices.size());
    const bool labelsAround = choices.size() > 0 && choices.size() <= 6;
    const float r = labelsAround ? full * 0.56f : full * 0.80f;
    const int selected = juce::roundToInt (pos * (float) (steps - 1));

    // position marks & labels
    for (int t = 0; t < steps; ++t)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) t / (float) (steps - 1);
        const auto p0 = c.getPointOnCircumference (r * 1.02f, a);
        const auto p1 = c.getPointOnCircumference (r * 1.16f, a);
        g.setColour (t == selected ? col::accent : ink.withAlpha (0.8f));
        g.drawLine (juce::Line<float> (p0, p1), 1.6f);

        if (labelsAround)
        {
            auto lp = c.getPointOnCircumference (r * 1.5f, a);
            g.setFont (font (juce::jlimit (8.5f, 11.0f, full * 0.17f), t == selected));
            const float tw = juce::jmin (64.0f, juce::jmax (30.0f, spanX - r * 1.1f)), th = 13.0f;
            // keep side labels outside the knob body and inside the component
            if (std::abs (lp.x - c.x) > r * 0.6f)
                lp.x = lp.x < c.x ? juce::jmax (area.getX() + tw * 0.5f, lp.x - tw * 0.3f)
                                  : juce::jmin (area.getRight() - tw * 0.5f, lp.x + tw * 0.3f);
            g.drawText (choices[t], juce::Rectangle<float> (lp.x - tw * 0.5f, lp.y - th * 0.5f, tw, th),
                        juce::Justification::centred, false);
        }
    }

    // body
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r, c.y - r + r * 0.1f, r * 2.0f, r * 2.0f);
    juce::ColourGradient body (juce::Colour (0xff4a4c51), c.x, c.y - r, juce::Colour (0xff141517), c.x, c.y + r, false);
    g.setGradientFill (body);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    // pointer bar ("chicken head" style)
    juce::Path bar;
    bar.addRoundedRectangle (-r * 0.26f, -r * 1.05f, r * 0.52f, r * 2.1f, r * 0.24f);
    bar.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    juce::ColourGradient barGrad (juce::Colour (0xff3a3c40), c.x - r, c.y - r, juce::Colour (0xff0b0b0c), c.x + r, c.y + r, false);
    g.setGradientFill (barGrad);
    g.fillPath (bar);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (bar, juce::PathStrokeType (1.0f));

    g.setColour (juce::Colours::white);
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * 0.25f, angle), c.getPointOnCircumference (r * 0.95f, angle)), 2.2f);

    if (! labelsAround && choices.size() > 0)
    {
        g.setColour (ink);
        g.setFont (font (11.0f, true));
        g.drawText (choices[juce::jlimit (0, choices.size() - 1, selected)],
                    juce::Rectangle<float> (area.getX(), area.getBottom() - 13.0f, area.getWidth(), 13.0f),
                    juce::Justification::centred, false);
    }
}

void RackLookAndFeel::drawDial (juce::Graphics& g, juce::Rectangle<float> area, float angle, juce::Slider& slider,
                                float startAngle, float endAngle)
{
    const auto ink = inkOf (slider);
    const auto c = area.getCentre();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;

    // numbered scale
    for (int t = 0; t <= 10; ++t)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) t / 10.0f;
        g.setColour (ink.withAlpha (0.85f));
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * 0.80f, a), c.getPointOnCircumference (r * 0.88f, a)), 1.2f);
        if (t % 2 == 0)
        {
            const auto lp = c.getPointOnCircumference (r * 0.97f, a);
            g.setFont (font (juce::jmax (7.5f, r * 0.13f)));
            g.drawText (juce::String (t), juce::Rectangle<float> (lp.x - 8.0f, lp.y - 6.0f, 16.0f, 12.0f), juce::Justification::centred, false);
        }
    }

    // cream ridged body
    const float br = r * 0.74f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - br, c.y - br + br * 0.1f, br * 2.0f, br * 2.0f);
    juce::ColourGradient grad (juce::Colour (0xfff6f1e3), c.x - br * 0.4f, c.y - br * 0.6f,
                               juce::Colour (0xffb5ad98), c.x + br * 0.5f, c.y + br, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (int i = 0; i < 48; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 48.0f;
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (br * 0.82f, a), c.getPointOnCircumference (br, a)), 1.0f);
    }

    // inner raised disc
    const float ir = br * 0.62f;
    juce::ColourGradient inner (juce::Colour (0xfffbf8ef), c.x, c.y - ir, juce::Colour (0xffcfc7b2), c.x, c.y + ir, false);
    g.setGradientFill (inner);
    g.fillEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f);
    g.setColour (juce::Colour (0xff111111));
    g.drawEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f, 2.2f);

    g.setColour (juce::Colour (0xff111111));
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (ir * 0.95f, angle), c.getPointOnCircumference (br * 0.98f, angle)), 3.0f);

    if (auto* k = dynamic_cast<Knob*> (&slider))
        drawMacroArc (g, c, r * 0.76f, startAngle, endAngle, *k);
}

void RackLookAndFeel::drawMacroKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, float angle,
                                     float startAngle, float endAngle, juce::Slider& slider)
{
    const auto c = area.getCentre();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const bool mini = slider.getProperties()["style"].toString() == "mini";
    const float thick = mini ? 2.5f : 3.5f;
    const auto arcColour = mini ? col::accent : col::red;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2c2e33));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = slider.getProperties()["bipolar"];
    juce::Path val;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (arcColour);
    if (pos > 0.0f || bipolar)
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - thick - 3.0f;
    juce::ColourGradient grad (juce::Colour (0xff3b3d42), c.x, c.y - kr, juce::Colour (0xff121315), c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    g.setColour (juce::Colours::white);
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (kr * 0.35f, angle), c.getPointOnCircumference (kr * 0.92f, angle)), mini ? 1.8f : 2.4f);
}

//==============================================================================
void RackLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                        float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
        return;
    }

    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cy = area.getCentreY() - 2.0f;

    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (area.getX(), cy - 2.0f, area.getWidth(), 4.0f, 2.0f);

    // tick dots
    g.setColour (col::dim.withAlpha (0.6f));
    for (int i = 0; i <= 20; ++i)
    {
        const float tx = area.getX() + area.getWidth() * (float) i / 20.0f;
        const float d = (i % 5 == 0) ? 2.0f : 1.2f;
        g.fillEllipse (tx - d * 0.5f, area.getBottom() - 4.0f, d, d);
    }

    // centre mark
    g.setColour (col::dim);
    g.fillRect (area.getCentreX() - 0.5f, cy - 6.0f, 1.0f, 12.0f);

    // handle
    const float hw = 8.0f, hh = 18.0f;
    juce::Rectangle<float> handle (sliderPos - hw * 0.5f, cy - hh * 0.5f, hw, hh);
    juce::ColourGradient grad (juce::Colour (0xffd6d8db), handle.getX(), handle.getY(),
                               juce::Colour (0xff6a6d72), handle.getRight(), handle.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (handle, 1.5f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (handle, 1.5f, 1.0f);
    g.fillRect (handle.getCentreX() - 0.5f, handle.getY() + 3.0f, 1.0f, hh - 6.0f);
}

//==============================================================================
void RackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = col::panelHi;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.2f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? col::accent : col::border);
    g.drawRoundedRectangle (r, 4.0f, on ? 1.4f : 1.0f);
}

void RackLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    g.setColour (on ? col::accent : (b.isEnabled() ? col::text : col::dim));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

juce::Font RackLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (15.0f, (float) buttonHeight * 0.55f), true);
}

void RackLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                     int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (juce::Colour (0xff0b0c0d));
    g.fillRect (x, y, w, h);
    juce::Rectangle<float> thumb = vertical
        ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
        : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f);
    g.setColour ((over || down) ? col::accentDim : col::border);
    g.fillRoundedRectangle (thumb, 3.0f);
}

void RackLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                  const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf20d0e10));
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (col::accent);
    g.drawRoundedRectangle (body, 4.0f, 1.0f);
}

void RackLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (juce::Colour (0xff0b0c0d));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
}

void RackLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? col::accent : col::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
}

} // namespace mr::ui
