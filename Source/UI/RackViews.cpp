#include "RackViews.h"
#include "PluginProcessor.h"

namespace mr::ui
{
static int parseDrag (const juce::var& description, const juce::String& prefix)
{
    const auto s = description.toString();
    if (s.startsWith (prefix))
        return s.fromFirstOccurrenceOf (":", false, false).getIntValue();
    return -1;
}

static juce::Image dragImageFor (juce::Component& c, float alpha)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    img.multiplyAllAlphas (alpha);
    return img;
}

//==============================================================================
void SlotNameButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 6)
    {
        if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            auto* slotComp = getParentComponent();
            dnd->startDragging ("slot:" + juce::String (slot), slotComp,
                                juce::ScaledImage (dragImageFor (*slotComp, 0.55f)), true);
        }
    }
    if (! dragging) juce::TextButton::mouseDrag (e);
}

void SlotNameButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    if (! isMouseButtonDown()) dragging = false;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (highlighted ? col::panelHi.brighter (0.05f) : juce::Colour (0xff121315));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    g.setColour (col::text);
    g.setFont (font (13.0f, true));
    g.drawFittedText (getButtonText(), getLocalBounds().reduced (8, 0).withTrimmedRight (18), juce::Justification::centred, 1);

    // slot letter badge
    auto badge = getLocalBounds().removeFromRight (20).reduced (0, 5).toFloat();
    g.setColour (col::accent);
    g.setFont (font (13.0f, true));
    g.drawText (letter, badge, juce::Justification::centred, false);
}

//==============================================================================
SlotView::SlotView (MultiRackProcessor& p, int slotIndex, juce::Component* pp)
    : slot (slotIndex), proc (p), popupParent (pp), name (slotIndex)
{
    name.letter = slotLetter (slot);

    power.setClickingTogglesState (true);
    power.onColour = col::accent;
    solo.setClickingTogglesState (true);
    solo.setTooltip ("Solo this module (bypasses the other modules)");
    name.setTooltip ("Click: choose module. Drag: move module to another slot.");
    remove.drawFrame = false;

    for (auto* c : std::initializer_list<juce::Component*> { &power, &solo, &name, &remove })
        addAndMakeVisible (c);

    powerAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (slotParamId (slot, "on")), power, nullptr);
    soloAtt  = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (slotParamId (slot, "solo")), solo, nullptr);

    name.onClick = [this] { if (onModuleMenu) onModuleMenu (slot); };
    remove.onClick = [this]
    {
        proc.pushUndo();
        proc.setSlotModule (slot, 0);
    };

    outGain.setSliderStyle (juce::Slider::LinearHorizontal);
    outGain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outGain.setDoubleClickReturnValue (true, 0.0);
    outGain.setTooltip ("Module output gain (double-click to reset)");
    addAndMakeVisible (outGain);
    outAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (slotParamId (slot, "out")), outGain, nullptr);

    outLabel.setText ("OUT GAIN", juce::dontSendNotification);
    outLabel.setFont (font (11.5f, true));
    outLabel.setColour (juce::Label::textColourId, col::dim);
    outValue.setFont (font (12.0f, true));
    outValue.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (outLabel);
    addAndMakeVisible (outValue);
    outGain.onValueChange = [this] { outValue.setText (proc.apvts.getParameter (slotParamId (slot, "out"))->getCurrentValueAsText(), juce::dontSendNotification); };
    outGain.onValueChange();

    rebuildPanel();
}

SlotView::~SlotView() = default;

void SlotView::rebuildPanel()
{
    shownType = proc.getSlotType (slot);
    panel.reset();
    empty.reset();

    if (shownType == 0)
    {
        empty = std::make_unique<EmptyPanel>();
        empty->onClick = [this] { if (onModuleMenu) onModuleMenu (slot); };
        addAndMakeVisible (*empty);
    }
    else
    {
        panel = std::make_unique<ModulePanel> (proc, slot, shownType, popupParent);
        panel->onKnobMenu = [this] (int s, int pIdx, juce::Component* src) { if (onKnobMenu) onKnobMenu (s, pIdx, src); };
        addAndMakeVisible (*panel);
    }

    const auto& info = getModules()[(size_t) shownType];
    name.setButtonText (shownType == 0 ? juce::String ("Empty") : info.name);
    const bool hasModule = shownType != 0;
    power.setEnabled (hasModule);
    solo.setEnabled (hasModule);
    remove.setVisible (hasModule);
    outGain.setEnabled (hasModule);
    resized();
    repaint();
}

void SlotView::refresh()
{
    if (proc.getSlotType (slot) != shownType)
        rebuildPanel();
    if (panel) panel->refresh();
}

void SlotView::resized()
{
    auto r = getLocalBounds().reduced (5, 0);
    auto header = r.removeFromTop (kHeaderH).reduced (0, 4);
    power.setBounds (header.removeFromLeft (28));
    header.removeFromLeft (4);
    solo.setBounds (header.removeFromLeft (26));
    header.removeFromLeft (4);
    remove.setBounds (header.removeFromRight (24));
    header.removeFromRight (4);
    name.setBounds (header);

    auto panelArea = r.removeFromTop (kPanelH);
    if (panel) panel->setBounds (panelArea);
    if (empty) empty->setBounds (panelArea);

    auto footer = r.removeFromTop (kFooterH).reduced (6, 6);
    outLabel.setBounds (footer.removeFromLeft (70));
    outValue.setBounds (footer.removeFromRight (66));
    outGain.setBounds (footer.reduced (4, 2));
}

void SlotView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (2, 0).toFloat();
    g.setColour (col::bar);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff0a0a0b));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const bool off = shownType != 0 && proc.apvts.getRawParameterValue (slotParamId (slot, "on"))->load() < 0.5f;
    if (off)
    {
        g.setColour (col::dim);
        g.setFont (font (10.0f, true));
    }
}

void SlotView::paintOverChildren (juce::Graphics& g)
{
    const bool off = shownType != 0 && proc.apvts.getRawParameterValue (slotParamId (slot, "on"))->load() < 0.5f;
    if (off && panel != nullptr)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (panel->getBounds());
        g.setColour (col::text.withAlpha (0.8f));
        g.setFont (font (16.0f, true));
        g.drawText ("BYPASSED", panel->getBounds().withHeight (40).withY (panel->getBounds().getCentreY() - 20), juce::Justification::centred, false);
    }
    if (dropHover)
    {
        g.setColour (col::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 5.0f);
        g.setColour (col::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 5.0f, 2.0f);
    }
}

bool SlotView::isInterestedInDragSource (const SourceDetails& d)
{
    const auto s = d.description.toString();
    return s.startsWith ("module:") || (s.startsWith ("slot:") && parseDrag (d.description, "slot:") != slot);
}

void SlotView::itemDragEnter (const SourceDetails&) { dropHover = true; repaint(); }
void SlotView::itemDragExit (const SourceDetails&)  { dropHover = false; repaint(); }

void SlotView::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    repaint();

    const int type = parseDrag (d.description, "module:");
    if (type >= 0)
    {
        proc.pushUndo();
        proc.setSlotModule (slot, type);
        return;
    }
    const int from = parseDrag (d.description, "slot:");
    if (from >= 0 && from != slot)
    {
        proc.pushUndo();
        proc.moveSlot (from, slot);
    }
}

//==============================================================================
juce::Rectangle<int> SlotTabs::tabBounds (int i) const
{
    const float w = (float) getWidth() / (float) kNumSlots;
    return juce::Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (2.0f, 4.0f).toNearestInt();
}

int SlotTabs::tabAt (juce::Point<int> p) const
{
    for (int i = 0; i < kNumSlots; ++i)
        if (tabBounds (i).contains (p)) return i;
    return -1;
}

void SlotTabs::paint (juce::Graphics& g)
{
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto r = tabBounds (i).toFloat();
        const int type = proc.getSlotType (i);
        const bool visible = i >= firstVisible && i <= lastVisible;
        const bool on = type != 0 && proc.apvts.getRawParameterValue (slotParamId (i, "on"))->load() >= 0.5f;

        g.setColour (visible ? col::panelHi : col::bar);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (hoverDrop == i ? col::accent : (visible ? col::accentDim : col::border));
        g.drawRoundedRectangle (r, 4.0f, hoverDrop == i ? 2.0f : 1.0f);

        auto inner = r.reduced (10.0f, 0.0f);
        g.setFont (font (13.0f, true));
        g.setColour (col::dim);
        g.drawText (slotLetter (i), inner.removeFromLeft (16.0f), juce::Justification::centredLeft, false);
        g.setColour (type == 0 ? col::dim.withAlpha (0.6f) : (on ? col::text : col::dim));
        g.drawFittedText (type == 0 ? juce::String ("Empty") : getModules()[(size_t) type].shortName,
                          inner.toNearestInt(), juce::Justification::centredLeft, 1);

        if (type != 0)
        {
            g.setColour (on ? col::accent : col::border);
            g.fillEllipse (r.getRight() - 12.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
    }
}

void SlotTabs::mouseDown (const juce::MouseEvent& e) { pressed = tabAt (e.getPosition()); dragging = false; }

void SlotTabs::mouseDrag (const juce::MouseEvent& e)
{
    if (pressed >= 0 && ! dragging && e.getDistanceFromDragStart() > 6)
        if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            auto img = createComponentSnapshot (tabBounds (pressed), true, 1.0f);
            dnd->startDragging ("slot:" + juce::String (pressed), this, juce::ScaledImage (img), true);
        }
}

void SlotTabs::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging && pressed >= 0 && tabAt (e.getPosition()) == pressed && onSelect)
        onSelect (pressed);
    pressed = -1;
    dragging = false;
}

bool SlotTabs::isInterestedInDragSource (const SourceDetails& d)
{
    const auto s = d.description.toString();
    return s.startsWith ("slot:") || s.startsWith ("module:");
}

void SlotTabs::itemDragMove (const SourceDetails& d)
{
    const int t = tabAt (d.localPosition);
    if (t != hoverDrop) { hoverDrop = t; repaint(); }
}

void SlotTabs::itemDragExit (const SourceDetails&) { hoverDrop = -1; repaint(); }

void SlotTabs::itemDropped (const SourceDetails& d)
{
    const int target = tabAt (d.localPosition);
    hoverDrop = -1;
    repaint();
    if (target < 0) return;

    const int from = parseDrag (d.description, "slot:");
    if (from >= 0 && from != target && onMove) onMove (from, target);

    const int type = parseDrag (d.description, "module:");
    if (type >= 0 && onDropModule) onDropModule (target, type);
}

//==============================================================================
BrowserItem::BrowserItem (int moduleType) : type (moduleType)
{
    setTooltip (getModules()[(size_t) type].description + "\nDrag into a slot, or double-click to add.");
}

void BrowserItem::paint (juce::Graphics& g)
{
    const auto& info = getModules()[(size_t) type];
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (hover ? col::panelHi : juce::Colour (0xff15161a));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (hover ? col::accentDim : col::border.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    auto thumbArea = r.reduced (8.0f).removeFromLeft (r.getHeight() * 0.44f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (thumbArea.translated (2.0f, 3.0f), 3.0f);
    paintFaceplate (g, info, thumbArea, type, true);

    auto text = r.withTrimmedLeft (thumbArea.getRight() - r.getX() + 8.0f).reduced (4.0f, 10.0f);
    g.setColour (col::text);
    g.setFont (font (14.5f, true));
    g.drawFittedText (info.name, text.removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    g.setColour (col::accent.withAlpha (0.9f));
    g.setFont (font (10.0f, true));
    g.drawText (categoryName (info.category).toUpperCase(), text.removeFromTop (14.0f), juce::Justification::centredLeft, false);
    g.setColour (col::dim);
    g.setFont (font (11.0f));
    g.drawFittedText (info.subtitle, text.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2);
}

void BrowserItem::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 5)
        if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            dnd->startDragging ("module:" + juce::String (type), this, juce::ScaledImage (dragImageFor (*this, 0.75f)), true);
        }
}

void BrowserItem::mouseUp (const juce::MouseEvent&) { dragging = false; }

void BrowserItem::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onAdd) onAdd (type);
}

//==============================================================================
ModuleBrowser::ModuleBrowser()
{
    const char* cats[] = { "All", "Dyn", "EQ", "Sat", "Util" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = catButtons.add (new juce::TextButton (cats[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->onClick = [this, i] { category = i - 1; updateList(); };
        addAndMakeVisible (b);
    }
    catButtons[0]->setToggleState (true, juce::dontSendNotification);

    search.setTextToShowWhenEmpty ("Search modules...", col::dim);
    search.setFont (font (13.0f));
    search.setIndents (8, 6);
    search.onTextChange = [this] { updateList(); };
    addAndMakeVisible (search);

    for (int t = 1; t < numModuleTypes(); ++t)
    {
        auto* item = items.add (new BrowserItem (t));
        item->onAdd = [this] (int type) { if (onAdd) onAdd (type); };
        list.addChildComponent (item);
    }

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    updateList();
}

void ModuleBrowser::updateList()
{
    const auto q = search.getText().trim().toLowerCase();
    int y = 0;
    const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    for (auto* item : items)
    {
        const auto& info = getModules()[(size_t) item->type];
        const bool catOk = category < 0 || (int) info.category == category;
        const bool searchOk = q.isEmpty() || info.name.toLowerCase().contains (q)
                              || info.subtitle.toLowerCase().contains (q)
                              || categoryName (info.category).toLowerCase().contains (q);
        item->setVisible (catOk && searchOk);
        if (item->isVisible())
        {
            item->setBounds (0, y, w, 96);
            y += 98;
        }
    }
    list.setSize (w, juce::jmax (y, 10));
}

void ModuleBrowser::paint (juce::Graphics& g)
{
    g.setColour (col::bar);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (col::text);
    g.setFont (font (12.0f, true));
    g.drawText ("MODULES", getLocalBounds().reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft, false);
}

void ModuleBrowser::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (24);
    auto cats = r.removeFromTop (28);
    const int bw = cats.getWidth() / catButtons.size();
    for (auto* b : catButtons)
        b->setBounds (cats.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (8);
    search.setBounds (r.removeFromTop (30));
    r.removeFromTop (8);
    viewport.setBounds (r);
    updateList();
}

//==============================================================================
MacroColumn::MacroColumn (MultiRackProcessor& p, juce::Component* popupParent) : proc (p)
{
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto* k = knobs.add (new Knob());
        k->setStyle ("macro");
        k->setPopupDisplayEnabled (true, false, popupParent);
        k->setTooltip ("Macro " + juce::String (m + 1) + ": right-click a module knob to assign it. Right-click here for options.");
        k->onRightClick = [this, m] { showMenu (m); };
        addAndMakeVisible (k);
        atts.add (new juce::SliderParameterAttachment (*proc.apvts.getParameter (macroId (m)), *k, nullptr));
        k->setDoubleClickReturnValue (true, 0.0);

        auto* l = labels.add (new juce::Label());
        l->setText (proc.getMacroName (m), juce::dontSendNotification);
        l->setJustificationType (juce::Justification::centred);
        l->setFont (font (12.0f, true));
        l->setEditable (false, true, false);
        l->setTooltip ("Double-click to rename");
        l->onTextChange = [this, m, l] { proc.setMacroName (m, l->getText().trim().isEmpty() ? "Macro " + juce::String (m + 1) : l->getText().trim()); };
        addAndMakeVisible (l);
    }
}

MacroColumn::~MacroColumn()
{
    atts.clear();
}

void MacroColumn::refresh()
{
    for (int m = 0; m < kNumMacros; ++m)
        if (! labels[m]->isBeingEdited() && labels[m]->getText() != proc.getMacroName (m))
            labels[m]->setText (proc.getMacroName (m), juce::dontSendNotification);
}

void MacroColumn::showMenu (int macro)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (proc.getMacroName (macro));

    juce::PopupMenu targets;
    int count = 0;
    struct Target { int slot, param; };
    std::vector<Target> list;
    for (int s = 0; s < kNumSlots; ++s)
        for (int i = 0; i < kParamsPerSlot; ++i)
            if (proc.getLink (s, i).macro.load() == macro)
            {
                const auto& info = getModules()[(size_t) proc.getSlotType (s)];
                const auto pname = i < (int) info.params.size() ? info.params[(size_t) i].name : juce::String ("Param " + juce::String (i + 1));
                list.push_back ({ s, i });
                targets.addItem (1000 + (int) list.size() - 1, slotLetter (s) + " " + info.shortName + " > " + pname + "   (remove)");
                ++count;
            }

    if (count == 0)
        menu.addItem (1, "No assignments: right-click any module knob", false, false);
    else
        menu.addSubMenu ("Assigned (" + juce::String (count) + ")", targets);

    menu.addSeparator();
    menu.addItem (2, "Rename...");
    menu.addItem (3, "Clear all assignments", count > 0);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (knobs[macro]),
        [this, macro, list] (int r)
        {
            if (r == 2) labels[macro]->showEditor();
            else if (r == 3) { proc.pushUndo(); proc.clearMacro (macro); }
            else if (r >= 1000 && r - 1000 < (int) list.size())
            {
                proc.pushUndo();
                proc.removeMacroLink (list[(size_t) (r - 1000)].slot, list[(size_t) (r - 1000)].param);
            }
        });
}

void MacroColumn::paint (juce::Graphics& g)
{
    g.setColour (col::bar);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (col::text);
    g.setFont (font (12.0f, true));
    g.drawText ("MACROS", getLocalBounds().reduced (0, 8).removeFromTop (16), juce::Justification::centred, false);
}

void MacroColumn::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    r.removeFromTop (30);
    const int h = r.getHeight() / kNumMacros;
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto cell = r.removeFromTop (h);
        labels[m]->setBounds (cell.removeFromBottom (20));
        knobs[m]->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight()), juce::jmin (cell.getWidth(), cell.getHeight())));
    }
}

} // namespace mr::ui
