#include "Widgets.h"
#include "Dsp.h"

namespace mr::ui
{
//==============================================================================
Knob::Knob()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
    setStyle ("knob");
}

void Knob::setMacroDisplay (int m, float lo, float hi, float value)
{
    if (m != macro || lo != macroLo || hi != macroHi || std::abs (value - macroValue) > 0.0005f)
    {
        macro = m; macroLo = lo; macroHi = hi; macroValue = value;
        repaint();
    }
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    rightClicked = e.mods.isPopupMenu();
    if (rightClicked)
    {
        if (onRightClick) onRightClick();
        return;
    }
    // Fine adjustment with Ctrl / Cmd / Shift
    setMouseDragSensitivity (e.mods.isCommandDown() || e.mods.isShiftDown() ? 1200 : 220);
    juce::Slider::mouseDown (e);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! rightClicked) juce::Slider::mouseDrag (e);
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (! rightClicked) juce::Slider::mouseUp (e);
    rightClicked = false;
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) juce::Slider::mouseDoubleClick (e);
}

//==============================================================================
LampButton::LampButton (const juce::String& text) : juce::Button (text)
{
    setClickingTogglesState (true);
}

void LampButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    // bezel
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 3.0f);

    juce::ColourGradient cap (juce::Colour (0xfff4f4f1), r.getX(), r.getY(),
                              juce::Colour (0xffb9bbbd), r.getX(), r.getBottom(), false);
    if (down) cap.multiplyOpacity (0.85f);
    g.setGradientFill (cap);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (highlighted ? 0.9f : 0.7f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    // LED
    const float d = juce::jmin (7.0f, r.getHeight() * 0.32f);
    const juce::Point<float> led (r.getRight() - d - 4.0f, r.getCentreY());
    if (on)
    {
        g.setColour (ledColour.withAlpha (0.35f));
        g.fillEllipse (led.x - d, led.y - d, d * 2.0f, d * 2.0f);
    }
    g.setColour (on ? ledColour.brighter (0.3f) : juce::Colour (0xff4b1d1a));
    g.fillEllipse (led.x - d * 0.5f, led.y - d * 0.5f, d, d);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (led.x - d * 0.5f, led.y - d * 0.5f, d, d, 0.8f);

    g.setColour (juce::Colour (0xff141414));
    g.setFont (font (juce::jmin (12.0f, r.getHeight() * 0.48f), true));
    g.drawFittedText (getButtonText(), r.withTrimmedRight (d + 6.0f).toNearestInt().reduced (3, 0),
                      juce::Justification::centred, 1);
}

//==============================================================================
IconButton::IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    if (drawFrame)
    {
        g.setColour (down ? col::panelHi.darker (0.2f) : (highlighted ? col::panelHi.brighter (0.06f) : col::panelHi));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? onColour : col::border);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    auto c = isEnabled() ? (on ? onColour : (highlighted ? juce::Colours::white : col::text)) : col::dim.withAlpha (0.5f);
    g.setColour (c);

    const auto b = r.reduced (r.getHeight() * 0.26f);
    const float s = juce::jmin (b.getWidth(), b.getHeight());
    const auto ic = juce::Rectangle<float> (s, s).withCentre (b.getCentre());
    const float lw = juce::jmax (1.5f, s * 0.11f);
    juce::PathStrokeType stroke (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;

    switch (icon)
    {
        case Icon::Undo:
        case Icon::Redo:
        {
            p.addCentredArc (ic.getCentreX(), ic.getCentreY() + s * 0.08f, s * 0.36f, s * 0.32f, 0.0f,
                             -juce::MathConstants<float>::halfPi * 1.4f, juce::MathConstants<float>::halfPi, true);
            juce::Path head;
            const float hx = ic.getCentreX() - s * 0.36f * std::sin (juce::MathConstants<float>::halfPi * 1.4f);
            const float hy = ic.getCentreY() + s * 0.08f - s * 0.32f * std::cos (juce::MathConstants<float>::halfPi * 1.4f);
            head.addTriangle (hx - s * 0.2f, hy - s * 0.05f, hx + s * 0.12f, hy - s * 0.2f, hx + s * 0.05f, hy + s * 0.15f);
            if (icon == Icon::Redo)
            {
                const auto flip = juce::AffineTransform::scale (-1.0f, 1.0f, ic.getCentreX(), ic.getCentreY());
                p.applyTransform (flip);
                head.applyTransform (flip);
            }
            g.strokePath (p, stroke);
            g.fillPath (head);
            return;
        }
        case Icon::Save:
            p.addRoundedRectangle (ic, s * 0.1f);
            g.strokePath (p, stroke);
            g.fillRect (ic.getX() + s * 0.25f, ic.getY(), s * 0.5f, s * 0.3f);
            g.drawRect (ic.getX() + s * 0.22f, ic.getY() + s * 0.55f, s * 0.56f, s * 0.45f, lw * 0.8f);
            return;
        case Icon::Trash:
            g.drawLine (ic.getX(), ic.getY() + s * 0.15f, ic.getRight(), ic.getY() + s * 0.15f, lw);
            g.drawLine (ic.getCentreX() - s * 0.15f, ic.getY(), ic.getCentreX() + s * 0.15f, ic.getY(), lw);
            p.startNewSubPath (ic.getX() + s * 0.12f, ic.getY() + s * 0.25f);
            p.lineTo (ic.getX() + s * 0.2f, ic.getBottom());
            p.lineTo (ic.getRight() - s * 0.2f, ic.getBottom());
            p.lineTo (ic.getRight() - s * 0.12f, ic.getY() + s * 0.25f);
            g.strokePath (p, stroke);
            return;
        case Icon::Menu:
            for (int i = 0; i < 3; ++i)
            {
                const float y = ic.getY() + s * (0.15f + 0.35f * (float) i);
                g.drawLine (ic.getX(), y, ic.getRight(), y, lw);
            }
            return;
        case Icon::Left:
            p.startNewSubPath (ic.getCentreX() + s * 0.2f, ic.getY() + s * 0.1f);
            p.lineTo (ic.getCentreX() - s * 0.2f, ic.getCentreY());
            p.lineTo (ic.getCentreX() + s * 0.2f, ic.getBottom() - s * 0.1f);
            g.strokePath (p, stroke);
            return;
        case Icon::Right:
            p.startNewSubPath (ic.getCentreX() - s * 0.2f, ic.getY() + s * 0.1f);
            p.lineTo (ic.getCentreX() + s * 0.2f, ic.getCentreY());
            p.lineTo (ic.getCentreX() - s * 0.2f, ic.getBottom() - s * 0.1f);
            g.strokePath (p, stroke);
            return;
        case Icon::Power:
            p.addCentredArc (ic.getCentreX(), ic.getCentreY() + s * 0.05f, s * 0.4f, s * 0.4f, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            g.strokePath (p, stroke);
            g.drawLine (ic.getCentreX(), ic.getY() - s * 0.05f, ic.getCentreX(), ic.getCentreY(), lw);
            return;
        case Icon::Close:
            g.drawLine (ic.getX() + s * 0.1f, ic.getY() + s * 0.1f, ic.getRight() - s * 0.1f, ic.getBottom() - s * 0.1f, lw);
            g.drawLine (ic.getRight() - s * 0.1f, ic.getY() + s * 0.1f, ic.getX() + s * 0.1f, ic.getBottom() - s * 0.1f, lw);
            return;
        case Icon::Copy:
            g.drawRoundedRectangle (ic.getX(), ic.getY() + s * 0.25f, s * 0.7f, s * 0.75f, 2.0f, lw);
            g.drawRoundedRectangle (ic.getX() + s * 0.3f, ic.getY(), s * 0.7f, s * 0.75f, 2.0f, lw);
            return;
        case Icon::Plus:
            g.drawLine (ic.getCentreX(), ic.getY(), ic.getCentreX(), ic.getBottom(), lw);
            g.drawLine (ic.getX(), ic.getCentreY(), ic.getRight(), ic.getCentreY(), lw);
            return;
        case Icon::Folder:
            p.startNewSubPath (ic.getX(), ic.getBottom() - s * 0.1f);
            p.lineTo (ic.getX(), ic.getY() + s * 0.1f);
            p.lineTo (ic.getX() + s * 0.4f, ic.getY() + s * 0.1f);
            p.lineTo (ic.getX() + s * 0.5f, ic.getY() + s * 0.25f);
            p.lineTo (ic.getRight(), ic.getY() + s * 0.25f);
            p.lineTo (ic.getRight(), ic.getBottom() - s * 0.1f);
            p.closeSubPath();
            g.strokePath (p, stroke);
            return;
        case Icon::Gear:
        {
            const auto c = ic.getCentre();
            for (int i = 0; i < 8; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                g.drawLine (juce::Line<float> (c.getPointOnCircumference (s * 0.3f, a), c.getPointOnCircumference (s * 0.5f, a)), lw * 1.3f);
            }
            g.drawEllipse (c.x - s * 0.3f, c.y - s * 0.3f, s * 0.6f, s * 0.6f, lw);
            return;
        }
    }
}

//==============================================================================
NeedleMeter::NeedleMeter (bool gainReduction) : grMode (gainReduction)
{
    setInterceptsMouseClicks (false, false);
}

void NeedleMeter::setValue (float v)
{
    float target;
    if (grMode)
        target = juce::jlimit (0.0f, 1.0f, v / 20.0f);                     // 0..20 dB GR
    else
        target = juce::jlimit (0.0f, 1.0f, (v + 18.0f + 20.0f) / 23.0f);    // -20..+3 VU (0 VU = -18 dBFS)

    // simple needle ballistics (30 Hz refresh)
    display += (target - display) * 0.35f;
    repaint();
}

void NeedleMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // chrome bezel
    juce::ColourGradient bezel (juce::Colour (0xffd8dadd), r.getX(), r.getY(), juce::Colour (0xff55585d), r.getRight(), r.getBottom(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (r, 6.0f);

    auto face = r.reduced (5.0f);
    juce::ColourGradient faceGrad (juce::Colour (0xfffff2c8), face.getCentreX(), face.getY(),
                                   juce::Colour (0xffe4c372), face.getCentreX(), face.getBottom(), false);
    g.setGradientFill (faceGrad);
    g.fillRoundedRectangle (face, 4.0f);

    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.25f);
    const float radius = face.getHeight() * 0.98f;
    const float a0 = -0.82f, a1 = 0.82f;
    const auto ink = juce::Colour (0xff2a2116);

    // scale arc
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius * 0.82f, radius * 0.82f, 0.0f, a0, a1, true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.2f));

    // red zone
    const float redStart = grMode ? a1 + 1.0f : a0 + (a1 - a0) * (20.0f / 23.0f);
    if (! grMode)
    {
        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, radius * 0.84f, radius * 0.84f, 0.0f, redStart, a1, true);
        g.setColour (juce::Colour (0xffc8281e));
        g.strokePath (red, juce::PathStrokeType (3.5f));
    }

    // ticks and numbers
    g.setFont (font (juce::jmax (8.0f, face.getHeight() * 0.11f), true));
    auto tick = [&] (float norm, const juce::String& label, bool major)
    {
        const float a = a0 + (a1 - a0) * norm;
        const auto p0 = pivot.getPointOnCircumference (radius * 0.82f, a);
        const auto p1 = pivot.getPointOnCircumference (radius * (major ? 0.9f : 0.87f), a);
        g.setColour (a >= redStart ? juce::Colour (0xffc8281e) : ink);
        g.drawLine (juce::Line<float> (p0, p1), major ? 1.4f : 1.0f);
        if (label.isNotEmpty())
        {
            const auto lp = pivot.getPointOnCircumference (radius * 0.98f, a);
            g.drawText (label, juce::Rectangle<float> (lp.x - 12.0f, lp.y - 6.0f, 24.0f, 12.0f), juce::Justification::centred, false);
        }
    };

    if (grMode)
    {
        const char* labels[] = { "0", "2", "4", "6", "8", "10", "", "15", "", "20" };
        const float vals[]   = { 0, 2, 4, 6, 8, 10, 12.5f, 15, 17.5f, 20 };
        for (int i = 0; i < 10; ++i)
            tick (vals[i] / 20.0f, labels[i], labels[i][0] != 0);
    }
    else
    {
        const float vals[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        const char* labels[] = { "20", "10", "7", "5", "3", "", "1", "0", "1", "", "3" };
        for (int i = 0; i < 11; ++i)
            tick ((vals[i] + 20.0f) / 23.0f, labels[i], labels[i][0] != 0);
    }

    g.setColour (ink);
    g.setFont (font (juce::jmax (9.0f, face.getHeight() * 0.13f), true));
    g.drawText (label, face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (face.getHeight() * 0.2f),
                juce::Justification::centred, false);

    // needle (GR meters rest at 0 on the left and move right)
    const float a = a0 + (a1 - a0) * display;
    const auto tip = pivot.getPointOnCircumference (radius * 0.92f, a);
    const auto base = pivot.getPointOnCircumference (radius * 0.28f, a);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (juce::Line<float> (base.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f)), 1.6f);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.drawLine (juce::Line<float> (base, tip), 1.5f);

    // glass highlight + pivot cover
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.35f), 4.0f);
    g.setColour (juce::Colour (0xff2b2b2b));
    g.fillRoundedRectangle (face.getCentreX() - 14.0f, face.getBottom() - 8.0f, 28.0f, 8.0f, 2.0f);
}

//==============================================================================
void LedLadder::setValue (float grDb)
{
    if (std::abs (grDb - value) > 0.05f) { value = grDb; repaint(); }
}

void LedLadder::paint (juce::Graphics& g)
{
    static const float steps[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 17, 20, 23, 26 };
    constexpr int n = 16;
    auto r = getLocalBounds().toFloat();
    const float labelH = 11.0f;
    const float w = r.getWidth() / (float) n;
    const float d = juce::jmin (w * 0.55f, r.getHeight() - labelH - 2.0f);

    for (int i = 0; i < n; ++i)
    {
        const float cx = r.getX() + w * ((float) i + 0.5f);
        const float cy = r.getY() + d * 0.5f + 1.0f;
        const bool lit = value >= steps[i] - 0.5f;
        const auto c = i < 6 ? col::green : (i < 11 ? col::accent : col::red);
        if (lit)
        {
            g.setColour (c.withAlpha (0.35f));
            g.fillEllipse (cx - d * 0.9f, cy - d * 0.9f, d * 1.8f, d * 1.8f);
        }
        g.setColour (lit ? c.brighter (0.2f) : c.darker (2.2f));
        g.fillEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d);
        g.setColour (ink.withAlpha (0.8f));
        g.setFont (font (8.0f));
        if (i % 3 == 0 || i == n - 1)
            g.drawText (juce::String ((int) steps[i]), juce::Rectangle<float> (cx - 10.0f, r.getBottom() - labelH, 20.0f, labelH),
                        juce::Justification::centred, false);
    }
}

//==============================================================================
void LevelBar::setValue (float db)
{
    if (std::abs (db - value) > 0.1f) { value = db; repaint(); }
}

void LevelBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto barArea = r.withTrimmedTop (12.0f).reduced (1.0f);
    g.setColour (ink.withAlpha (0.85f));
    g.setFont (font (9.0f, true));
    g.drawText (label, r.withHeight (11.0f), juce::Justification::centredLeft, false);
    g.drawText (value > -60.0f ? juce::String (value, 1) + " dB" : "-inf", r.withHeight (11.0f), juce::Justification::centredRight, false);

    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (barArea, 2.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, (value + 60.0f) / 66.0f);
    auto fill = barArea.reduced (1.5f);
    fill.setWidth (fill.getWidth() * norm);
    juce::ColourGradient grad (col::green, barArea.getX(), 0.0f, col::red, barArea.getRight(), 0.0f, false);
    grad.addColour (0.75, col::accent);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 1.5f);
}

//==============================================================================
void MiniMeter::setValue (float db)
{
    if (std::abs (db - value) > 0.1f) { value = db; repaint(); }
}

void MiniMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (r, 2.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, (value + 60.0f) / 63.0f);
    auto fill = r.reduced (1.5f);
    fill = fill.withTrimmedTop (fill.getHeight() * (1.0f - norm));
    juce::ColourGradient grad (col::red, 0.0f, r.getY(), col::green, 0.0f, r.getBottom(), false);
    grad.addColour (0.2, col::accent);
    g.setGradientFill (grad);
    g.fillRect (fill);
    if (value > -0.1f)
    {
        g.setColour (col::red);
        g.fillRect (r.withHeight (3.0f));
    }
}

} // namespace mr::ui
