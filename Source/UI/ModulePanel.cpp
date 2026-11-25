#include "ModulePanel.h"
#include "PluginProcessor.h"

namespace mr::ui
{
//==============================================================================
PanelLayout computeLayout (const ModuleInfo& info, juce::Rectangle<float> b)
{
    PanelLayout out;
    const float sx = b.getWidth() / (float) kPanelW;
    const float sy = b.getHeight() / (float) kPanelH;
    const float s = juce::jmin (sx, sy);

    const float header = 86.0f * sy;
    float meterH = 0.0f;
    switch (info.meter)
    {
        case MeterType::VU:
        case MeterType::GainReduction: meterH = 118.0f * sy; break;
        case MeterType::LedLadder:     meterH = 34.0f * sy; break;
        case MeterType::Level:         meterH = 30.0f * sy; break;
        case MeterType::None:          break;
    }

    if (meterH > 0.0f)
    {
        const float mw = info.meter == MeterType::VU || info.meter == MeterType::GainReduction ? 200.0f * sx : b.getWidth() - 40.0f * sx;
        out.meter = juce::Rectangle<float> (mw, meterH).withCentre ({ b.getCentreX(), b.getY() + header + meterH * 0.5f });
    }

    const float rowsTop = b.getY() + header + meterH + (meterH > 0.0f ? 12.0f * sy : 0.0f);
    const float rowsBottom = b.getBottom() - 18.0f * sy;
    const int numRows = juce::jmax (1, (int) info.rows.size());
    const float rowH = (rowsBottom - rowsTop) / (float) numRows;

    for (int r = 0; r < (int) info.rows.size(); ++r)
    {
        const auto& row = info.rows[(size_t) r];
        const float y0 = rowsTop + rowH * (float) r;
        if (r > 0) out.rowSeparators.push_back (y0);

        const int k = juce::jmax (1, (int) row.size());
        const float colW = (b.getWidth() - 16.0f * sx) / (float) k;
        const float labelH = 14.0f * sy;
        const float avail = rowH - labelH - 10.0f * sy;

        for (int j = 0; j < (int) row.size(); ++j)
        {
            const int pi = row[(size_t) j];
            const auto& spec = info.params[(size_t) pi];
            const float cx = b.getX() + 8.0f * sx + colW * ((float) j + 0.5f);

            ControlPlacement cp;
            cp.param = pi;
            cp.label = { cx - colW * 0.5f, y0 + 5.0f * sy, colW, labelH };

            float size = 0.0f;
            switch (spec.ctl)
            {
                case Ctl::BigKnob:   size = juce::jmin (avail, 90.0f * s, colW - 6.0f * s); break;
                case Ctl::Knob:      size = juce::jmin (avail, 68.0f * s, colW - 6.0f * s); break;
                case Ctl::SmallKnob: size = juce::jmin (avail, 52.0f * s, colW - 6.0f * s); break;
                case Ctl::Dial:      size = juce::jmin (avail, 104.0f * s, colW - 4.0f * s); break;
                case Ctl::Selector:  size = juce::jmin (avail, 118.0f * s, colW - 2.0f * s); break;
                case Ctl::Toggle:    break;
            }

            const float cy = y0 + labelH + 5.0f * sy + avail * 0.5f;
            if (spec.ctl == Ctl::Toggle)
            {
                const float w = juce::jmin (colW - 12.0f * sx, 76.0f * sx);
                cp.control = juce::Rectangle<float> (w, 28.0f * sy).withCentre ({ cx, y0 + rowH * 0.5f });
            }
            else if (spec.ctl == Ctl::Selector)
            {
                // wider than tall so the position labels around the switch have room
                cp.control = juce::Rectangle<float> (juce::jmax (size, colW - 4.0f * s), size).withCentre ({ cx, cy });
            }
            else
            {
                cp.control = juce::Rectangle<float> (size, size).withCentre ({ cx, cy });
            }
            out.controls.push_back (cp);
        }
    }
    return out;
}

//==============================================================================
void paintFaceplate (juce::Graphics& g, const ModuleInfo& info, juce::Rectangle<float> b, int seed, bool thumbnail)
{
    const float sx = b.getWidth() / (float) kPanelW;
    const float sy = b.getHeight() / (float) kPanelH;
    const float s = juce::jmin (sx, sy);

    // body
    juce::ColourGradient grad (info.panelTop, b.getX(), b.getY(), info.panelBottom, b.getX(), b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (b, 4.0f * s);

    // brushed metal texture
    if (info.brushed)
    {
        juce::Random rng (seed * 7919 + 17);
        const int lines = thumbnail ? 40 : 520;
        for (int i = 0; i < lines; ++i)
        {
            const float y = b.getY() + rng.nextFloat() * b.getHeight();
            const float x0 = b.getX() + rng.nextFloat() * b.getWidth() * 0.6f;
            const float len = b.getWidth() * (0.2f + rng.nextFloat() * 0.8f);
            const bool light = rng.nextBool();
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.018f + rng.nextFloat() * 0.03f));
            g.drawHorizontalLine ((int) y, x0, juce::jmin (b.getRight(), x0 + len));
        }
    }

    // bevel
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawLine (b.getX() + 3.0f, b.getY() + 1.0f, b.getRight() - 3.0f, b.getY() + 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f * s, 1.0f);

    const auto ink = info.ink;

    if (! thumbnail)
    {
        // screws
        const float sr = 7.0f * s;
        juce::Random rng (seed + 3);
        drawScrew (g, { b.getX() + 16.0f * sx, b.getY() + 16.0f * sy }, sr, rng.nextFloat() * 3.0f);
        drawScrew (g, { b.getRight() - 16.0f * sx, b.getY() + 16.0f * sy }, sr, rng.nextFloat() * 3.0f);
        drawScrew (g, { b.getX() + 16.0f * sx, b.getBottom() - 16.0f * sy }, sr, rng.nextFloat() * 3.0f);
        drawScrew (g, { b.getRight() - 16.0f * sx, b.getBottom() - 16.0f * sy }, sr, rng.nextFloat() * 3.0f);

        // header
        drawLogo (g, juce::Rectangle<float> (26.0f * s, 26.0f * s).withCentre ({ b.getCentreX(), b.getY() + 22.0f * sy }), ink, info.accent);
        g.setColour (ink);
        g.setFont (font (21.0f * s, true));
        g.drawText (info.name.toUpperCase(), juce::Rectangle<float> (b.getX(), b.getY() + 38.0f * sy, b.getWidth(), 26.0f * sy),
                    juce::Justification::centred, false);
        g.setColour (ink.withAlpha (0.7f));
        g.setFont (font (9.5f * s, true).withExtraKerningFactor (0.18f));
        g.drawText (info.subtitle, juce::Rectangle<float> (b.getX(), b.getY() + 63.0f * sy, b.getWidth(), 14.0f * sy),
                    juce::Justification::centred, false);
        g.setColour (info.accent.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (40.0f * s, 2.0f * s).withCentre ({ b.getCentreX(), b.getY() + 80.0f * sy }));
    }
    else
    {
        g.setColour (ink.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (b.getWidth() * 0.5f, 3.0f).withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.06f }));
    }

    const auto layout = computeLayout (info, b);

    // meter window recess
    if (! layout.meter.isEmpty())
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (layout.meter.expanded (3.0f * s), 6.0f * s);
        if (thumbnail)
        {
            g.setColour (info.meter == MeterType::VU || info.meter == MeterType::GainReduction
                             ? juce::Colour (0xfff0d48a) : juce::Colour (0xff101010));
            g.fillRoundedRectangle (layout.meter, 2.0f);
        }
    }

    // row separators
    g.setColour (ink.withAlpha (0.14f));
    for (auto y : layout.rowSeparators)
        g.drawHorizontalLine ((int) y, b.getX() + 14.0f * sx, b.getRight() - 14.0f * sx);

    for (const auto& cp : layout.controls)
    {
        const auto& spec = info.params[(size_t) cp.param];

        if (thumbnail)
        {
            const auto c = cp.control.getCentre();
            if (spec.ctl == Ctl::Toggle)
            {
                g.setColour (juce::Colour (0xffe8e8e4));
                g.fillRoundedRectangle (cp.control, 1.0f);
            }
            else
            {
                const float r = cp.control.getWidth() * (spec.ctl == Ctl::Selector ? 0.28f : 0.36f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillEllipse (c.x - r, c.y - r + 1.0f, r * 2.0f, r * 2.0f);
                g.setColour (spec.ctl == Ctl::Dial ? juce::Colour (0xffeee6cf) : knobColour (spec.colour));
                g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            }
            continue;
        }

        if (spec.ctl != Ctl::Toggle)
        {
            g.setColour (ink);
            g.setFont (font (11.0f * s, true));
            g.drawText (spec.label, cp.label, juce::Justification::centred, false);
        }

        const bool numericKnob = spec.ctl == Ctl::Knob || spec.ctl == Ctl::BigKnob || spec.ctl == Ctl::SmallKnob;
        if (numericKnob && spec.unit != Unit::Choice)
        {
            const auto& r = cp.control;
            g.setColour (ink.withAlpha (0.75f));
            g.setFont (font (8.5f * s));
            const float tw = 34.0f * s, th = 10.0f * s;
            g.drawText (spec.shortText (spec.minV), juce::Rectangle<float> (r.getX() - tw * 0.45f, r.getBottom() - th * 0.6f, tw, th),
                        juce::Justification::centred, false);
            g.drawText (spec.shortText (spec.maxV), juce::Rectangle<float> (r.getRight() - tw * 0.55f, r.getBottom() - th * 0.6f, tw, th),
                        juce::Justification::centred, false);
        }
    }
}

//==============================================================================
ModulePanel::ModulePanel (MultiRackProcessor& p, int s, int t, juce::Component* popupParent)
    : proc (p), slot (s), type (t), info (getModules()[(size_t) t])
{
    layout = computeLayout (info, { 0.0f, 0.0f, (float) kPanelW, (float) kPanelH });

    for (const auto& cp : layout.controls)
    {
        const auto& spec = info.params[(size_t) cp.param];
        auto* param = proc.getKnobParam (slot, cp.param);
        Control c;
        c.param = cp.param;

        if (spec.ctl == Ctl::Toggle)
        {
            c.button = std::make_unique<LampButton> (spec.label);
            c.button->ledColour = info.accent == juce::Colour (0xff3d7bd9) ? col::red : info.accent;
            c.button->setTooltip (spec.name);
            addAndMakeVisible (*c.button);
            c.bAtt = std::make_unique<juce::ButtonParameterAttachment> (*param, *c.button, nullptr);
        }
        else
        {
            c.knob = std::make_unique<Knob>();
            auto& k = *c.knob;
            switch (spec.ctl)
            {
                case Ctl::BigKnob:   k.setStyle ("big"); break;
                case Ctl::SmallKnob: k.setStyle ("small"); break;
                case Ctl::Dial:      k.setStyle ("dial"); break;
                case Ctl::Selector:  k.setStyle ("selector"); k.setChoices (spec.choices); break;
                default:             k.setStyle ("knob"); break;
            }
            if (spec.unit == Unit::Choice && spec.ctl != Ctl::Selector)
                k.setChoices (spec.choices);

            k.setKnobColour (spec.colour);
            k.setInk (info.ink);
            k.setTooltip (spec.name);
            k.setPopupDisplayEnabled (true, false, popupParent);
            addAndMakeVisible (k);
            c.sAtt = std::make_unique<juce::SliderParameterAttachment> (*param, k, nullptr);

            if (spec.unit == Unit::Choice && spec.numChoices() > 1)
            {
                k.numSteps = spec.numChoices();
                k.setMouseDragSensitivity (60 * spec.numChoices());
            }
            k.setDoubleClickReturnValue (true, spec.defaultNorm());

            const int paramIndex = cp.param;
            k.onRightClick = [this, paramIndex, &k] { if (onKnobMenu) onKnobMenu (slot, paramIndex, &k); };
        }
        controls.push_back (std::move (c));
    }

    switch (info.meter)
    {
        case MeterType::VU:
            needle = std::make_unique<NeedleMeter> (false);
            needle->label = info.meterLabel;
            addAndMakeVisible (*needle);
            break;
        case MeterType::GainReduction:
            needle = std::make_unique<NeedleMeter> (true);
            needle->label = "GR";
            addAndMakeVisible (*needle);
            break;
        case MeterType::LedLadder:
            leds = std::make_unique<LedLadder>();
            leds->ink = info.ink;
            addAndMakeVisible (*leds);
            break;
        case MeterType::Level:
            bar = std::make_unique<LevelBar>();
            bar->label = info.meterLabel;
            bar->ink = info.ink;
            addAndMakeVisible (*bar);
            break;
        case MeterType::None:
            break;
    }

    setSize (kPanelW, kPanelH);
}

ModulePanel::~ModulePanel() = default;

void ModulePanel::resized()
{
    for (size_t i = 0; i < controls.size(); ++i)
    {
        const auto r = layout.controls[i].control.toNearestInt();
        if (controls[i].knob)   controls[i].knob->setBounds (r);
        if (controls[i].button) controls[i].button->setBounds (r);
    }
    if (needle) needle->setBounds (layout.meter.toNearestInt());
    if (leds)   leds->setBounds (layout.meter.toNearestInt());
    if (bar)    bar->setBounds (layout.meter.toNearestInt());
}

void ModulePanel::paint (juce::Graphics& g)
{
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int w = juce::roundToInt ((float) getWidth() * scale);
    const int h = juce::roundToInt ((float) getHeight() * scale);

    if (! cache.isValid() || cache.getWidth() != w || cache.getHeight() != h)
    {
        cache = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true, juce::SoftwareImageType());
        juce::Graphics cg (cache);
        cg.addTransform (juce::AffineTransform::scale (scale));
        paintFaceplate (cg, info, getLocalBounds().toFloat(), type, false);
    }
    g.drawImage (cache, getLocalBounds().toFloat());
}

void ModulePanel::refresh()
{
    const float m = proc.getSlotMeter (slot);
    if (needle) needle->setValue (m);
    if (leds)   leds->setValue (m);
    if (bar)    bar->setValue (m);

    for (auto& c : controls)
    {
        if (c.knob == nullptr) continue;
        auto& link = proc.getLink (slot, c.param);
        const int macro = link.macro.load();
        if (macro >= 0)
        {
            const float lo = link.lo.load(), hi = link.hi.load();
            c.knob->setMacroDisplay (macro, lo, hi, lo + (hi - lo) * proc.getMacroValue (macro));
        }
        else
        {
            c.knob->setMacroDisplay (-1, 0.0f, 1.0f, 0.0f);
        }
    }
}

//==============================================================================
void EmptyPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff141518));
    g.fillRoundedRectangle (b, 4.0f);

    auto inner = b.reduced (14.0f);
    juce::Path border;
    border.addRoundedRectangle (inner, 6.0f);
    juce::Path dashed;
    const float dashes[] = { 6.0f, 5.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (dragHover ? col::accent : col::border);
    g.fillPath (dashed);

    const auto c = inner.getCentre().translated (0.0f, -30.0f);
    g.setColour (dragHover ? col::accent : col::dim);
    g.drawEllipse (c.x - 26.0f, c.y - 26.0f, 52.0f, 52.0f, 1.5f);
    g.drawLine (c.x - 12.0f, c.y, c.x + 12.0f, c.y, 2.0f);
    g.drawLine (c.x, c.y - 12.0f, c.x, c.y + 12.0f, 2.0f);

    g.setFont (font (13.0f, true));
    g.drawText ("EMPTY SLOT", inner.withY (c.y + 36.0f).withHeight (18.0f), juce::Justification::centred, false);
    g.setFont (font (11.5f));
    g.setColour (col::dim.withAlpha (0.8f));
    g.drawText ("Drag a module here", inner.withY (c.y + 56.0f).withHeight (16.0f), juce::Justification::centred, false);
    g.drawText ("or click to choose", inner.withY (c.y + 72.0f).withHeight (16.0f), juce::Justification::centred, false);
}

void EmptyPanel::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasDraggedSinceMouseDown() && onClick)
        onClick();
}

} // namespace mr::ui
