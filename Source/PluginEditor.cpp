#include "PluginEditor.h"

namespace mr::ui
{
//==============================================================================
/** Unscaled editor content (1480 x 872 logical pixels). The editor scales it to the window size. */
class RackContent final : public juce::Component
{
public:
    RackContent (MultiRackProcessor& p, MultiRackEditor& e)
        : proc (p), editor (e), macros (p, this), tabs (p)
    {
        //== top bar ===========================================================
        presetBox.setTooltip ("Rack presets");
        presetBox.onClick = [this] { showPresetMenu(); };
        prevBtn.onClick = [this] { proc.getPresets().previous(); };
        nextBtn.onClick = [this] { proc.getPresets().next(); };
        saveBtn.onClick = [this] { showSaveDialog(); };
        undoBtn.onClick = [this] { proc.undo(); };
        redoBtn.onClick = [this] { proc.redo(); };

        abA.setTooltip ("A/B compare: setting A");
        abB.setTooltip ("A/B compare: setting B");
        abA.onClick = [this] { proc.selectAB (0); };
        abB.onClick = [this] { proc.selectAB (1); };
        abCopy.onClick = [this] { proc.copyToOtherAB(); };
        clearBtn.onClick = [this]
        {
            proc.pushUndo();
            proc.clearRack();
        };

        hqBtn.setClickingTogglesState (true);
        hqBtn.setTooltip ("HQ: 4x oversampling for the whole rack (cleaner saturation, adds a little latency and CPU)");
        hqAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("hq"), hqBtn, nullptr);
        gearBtn.onClick = [this] { showSettingsMenu(); };

        for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &undoBtn, &redoBtn,
                                                               &abA, &abCopy, &abB, &clearBtn, &hqBtn, &gearBtn, &inMeter, &outMeter })
            addAndMakeVisible (c);

        setupMini (inKnob, "input", true);
        setupMini (outKnob, "output", true);
        setupMini (mixKnob, "mix", false);

        //== browser, macros ===================================================
        browser.onAdd = [this] (int type) { addModuleToFreeSlot (type); };
        addAndMakeVisible (browser);
        addAndMakeVisible (macros);

        //== rack ===============================================================
        for (int s = 0; s < kNumSlots; ++s)
        {
            auto* v = slotViews.add (new SlotView (proc, s, this));
            v->onModuleMenu = [this] (int slot) { showModuleMenu (slot); };
            v->onKnobMenu = [this] (int slot, int param, juce::Component* src) { showKnobMenu (slot, param, src); };
            strip.addAndMakeVisible (v);
        }
        strip.setSize (kSlotW * kNumSlots, kSlotH);
        for (int s = 0; s < kNumSlots; ++s)
            slotViews[s]->setBounds (s * kSlotW, 0, kSlotW, kSlotH);

        viewport.setViewedComponent (&strip, false);
        viewport.setScrollBarsShown (false, true);
        viewport.setScrollBarThickness (10);
        viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
        addAndMakeVisible (viewport);

        tabs.onSelect = [this] (int s) { scrollToSlot (s); };
        tabs.onMove = [this] (int from, int to) { proc.pushUndo(); proc.moveSlot (from, to); scrollToSlot (to); };
        tabs.onDropModule = [this] (int s, int type) { proc.pushUndo(); proc.setSlotModule (s, type); scrollToSlot (s); };
        addAndMakeVisible (tabs);

        setSize (MultiRackEditor::kBaseW, MultiRackEditor::kBaseH);
    }

    ~RackContent() override
    {
        miniAtts.clear();
        hqAtt.reset();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        g.setColour (col::bar);
        g.fillRect (0, 0, getWidth(), 58);
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (58, 0.0f, (float) getWidth());

        drawLogo (g, { 14.0f, 13.0f, 32.0f, 32.0f }, col::text, col::accent);
        g.setColour (col::text);
        g.setFont (font (22.0f, true).withExtraKerningFactor (0.06f));
        g.drawText ("MULTI", 54, 9, 80, 24, juce::Justification::centredLeft, false);
        g.setColour (col::accent);
        g.drawText ("RACK", 122, 9, 80, 24, juce::Justification::centredLeft, false);
        g.setColour (col::dim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.2f));
        g.drawText ("MODULAR CHANNEL STRIP", 55, 32, 170, 14, juce::Justification::centredLeft, false);

        g.setFont (font (10.0f, true));
        g.setColour (col::dim);
        for (auto* k : { &inKnob, &outKnob, &mixKnob })
            g.drawText (k->getName(), k->getBounds().withY (k->getBottom() - 2).withHeight (12).expanded (8, 0), juce::Justification::centred, false);
        g.drawText ("IN", inMeter.getBounds().withY (inMeter.getBottom() + 1).withHeight (11).expanded (6, 0), juce::Justification::centred, false);
        g.drawText ("OUT", outMeter.getBounds().withY (outMeter.getBottom() + 1).withHeight (11).expanded (8, 0), juce::Justification::centred, false);

        g.setColour (col::dim.withAlpha (0.8f));
        g.setFont (font (11.5f));
        g.drawText ("Drag modules from the browser into a slot  |  Drag a slot name or tab to reorder  |  Right-click a knob to assign a macro  |  Ctrl/Shift-drag: fine  |  Double-click: reset",
                    juce::Rectangle<int> (326, getHeight() - 22, getWidth() - 334, 18), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        const int y = 12, h = 34;
        presetBox.setBounds (232, y, 270, h);
        prevBtn.setBounds (506, y, 30, h);
        nextBtn.setBounds (540, y, 30, h);
        saveBtn.setBounds (576, y, 34, h);
        undoBtn.setBounds (646, y, 34, h);
        redoBtn.setBounds (684, y, 34, h);
        abA.setBounds (748, y, 34, h);
        abCopy.setBounds (786, y, 34, h);
        abB.setBounds (824, y, 34, h);
        clearBtn.setBounds (884, y, 34, h);
        hqBtn.setBounds (936, y, 52, h);
        inKnob.setBounds (1016, 4, 42, 42);
        outKnob.setBounds (1078, 4, 42, 42);
        mixKnob.setBounds (1140, 4, 42, 42);
        inMeter.setBounds (1204, 8, 8, 36);
        outMeter.setBounds (1226, 8, 8, 36);
        gearBtn.setBounds (getWidth() - 46, y, 34, h);

        browser.setBounds (8, 66, 220, getHeight() - 74);
        macros.setBounds (236, 66, 82, getHeight() - 74);

        const int rackX = 326, rackW = getWidth() - rackX - 8;
        viewport.setBounds (rackX, 66, rackW, kSlotH + viewport.getScrollBarThickness() + 2);
        tabs.setBounds (rackX, viewport.getBottom() + 2, rackW, 40);
    }

    void refresh()
    {
        for (auto* v : slotViews) v->refresh();
        macros.refresh();

        const int first = viewport.getViewPositionX() / kSlotW;
        const int last = juce::jmin (kNumSlots - 1, (viewport.getViewPositionX() + viewport.getWidth() - kSlotW / 3) / kSlotW);
        tabs.firstVisible = first;
        tabs.lastVisible = last;
        tabs.repaint();

        inMeter.setValue (proc.inMeter.load());
        outMeter.setValue (proc.outMeter.load());

        auto& pm = proc.getPresets();
        if (presetBox.getButtonText() != pm.currentName)
            presetBox.setButtonText (pm.currentName);

        undoBtn.setEnabled (proc.canUndo());
        redoBtn.setEnabled (proc.canRedo());
        abA.setToggleState (proc.getAB() == 0, juce::dontSendNotification);
        abB.setToggleState (proc.getAB() == 1, juce::dontSendNotification);
    }

private:
    void setupMini (Knob& k, const juce::String& paramId, bool bipolar)
    {
        k.setStyle ("mini");
        k.getProperties().set ("bipolar", bipolar);
        k.setName (paramId == "input" ? "INPUT" : (paramId == "output" ? "OUTPUT" : "MIX"));
        k.setPopupDisplayEnabled (true, false, this);
        k.setTooltip (proc.apvts.getParameter (paramId)->getName (64));
        addAndMakeVisible (k);
        miniAtts.add (new juce::SliderParameterAttachment (*proc.apvts.getParameter (paramId), k, nullptr));
        auto* param = proc.apvts.getParameter (paramId);
        k.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    void scrollToSlot (int s)
    {
        const int maxX = juce::jmax (0, strip.getWidth() - viewport.getViewWidth());
        const int target = juce::jlimit (0, maxX, s * kSlotW - (viewport.getViewWidth() - kSlotW) / 2);
        viewport.setViewPosition (target, 0);
    }

    void addModuleToFreeSlot (int type)
    {
        for (int s = 0; s < kNumSlots; ++s)
            if (proc.getSlotType (s) == 0)
            {
                proc.pushUndo();
                proc.setSlotModule (s, type);
                scrollToSlot (s);
                return;
            }
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Rack is full",
                                                "All slots are in use. Drag the module onto a slot to replace it.", "OK", this);
    }

    //==========================================================================
    void showModuleMenu (int slot)
    {
        const int current = proc.getSlotType (slot);
        juce::PopupMenu menu;
        menu.addSectionHeader ("Slot " + slotLetter (slot));

        const Category cats[] = { Category::Dynamics, Category::EQ, Category::Saturation, Category::Utility };
        const char* catNames[] = { "Dynamics", "EQ", "Saturation", "Utility" };
        for (int c = 0; c < 4; ++c)
        {
            juce::PopupMenu sub;
            for (int t = 1; t < numModuleTypes(); ++t)
                if (getModules()[(size_t) t].category == cats[c])
                    sub.addItem (100 + t, getModules()[(size_t) t].name, true, t == current);
            menu.addSubMenu (catNames[c], sub);
        }

        menu.addSeparator();
        menu.addItem (1, "Reset module to default", current != 0);
        menu.addItem (2, "Copy module", current != 0);
        menu.addItem (3, "Paste module", proc.hasSlotClipboard());
        menu.addItem (4, "Move left", slot > 0);
        menu.addItem (5, "Move right", slot < kNumSlots - 1);
        menu.addSeparator();
        menu.addItem (6, "Remove module", current != 0);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slotViews[slot]).withMinimumWidth (200),
            [this, slot] (int r)
            {
                if (r == 0) return;
                if (r == 2) { proc.copySlotToClipboard (slot); return; }
                proc.pushUndo();
                if (r >= 100)      proc.setSlotModule (slot, r - 100);
                else if (r == 1)   proc.resetSlot (slot);
                else if (r == 3)   proc.pasteSlotFromClipboard (slot);
                else if (r == 4)   { proc.moveSlot (slot, slot - 1); scrollToSlot (slot - 1); }
                else if (r == 5)   { proc.moveSlot (slot, slot + 1); scrollToSlot (slot + 1); }
                else if (r == 6)   proc.setSlotModule (slot, 0);
            });
    }

    void showKnobMenu (int slot, int param, juce::Component* source)
    {
        const auto& info = getModules()[(size_t) proc.getSlotType (slot)];
        if (param >= (int) info.params.size()) return;
        const auto& spec = info.params[(size_t) param];
        auto& link = proc.getLink (slot, param);
        const int current = link.macro.load();
        auto* kp = proc.getKnobParam (slot, param);

        juce::PopupMenu menu;
        menu.addSectionHeader (slotLetter (slot) + " " + info.shortName + " > " + spec.name + "  (" + kp->getCurrentValueAsText() + ")");

        juce::PopupMenu assign;
        for (int m = 0; m < kNumMacros; ++m)
            assign.addItem (100 + m, proc.getMacroName (m), true, m == current);
        menu.addSubMenu ("Assign to macro", assign);

        if (current >= 0)
        {
            menu.addItem (1, "Macro range: start at knob position");
            menu.addItem (2, "Macro range: end at knob position");
            menu.addItem (3, "Macro range: invert");
            menu.addItem (4, "Macro range: full");
            menu.addItem (5, "Remove from " + proc.getMacroName (current));
        }
        menu.addSeparator();
        menu.addItem (6, "Reset to default (" + spec.formatReal (spec.defV) + ")");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (source),
            [this, slot, param, kp, def = spec.defaultNorm()] (int r)
            {
                if (r == 0) return;
                proc.pushUndo();
                auto& l = proc.getLink (slot, param);
                const float knobPos = kp->getValue();
                if (r >= 100)    proc.assignMacro (slot, param, r - 100);
                else if (r == 1) proc.setMacroRange (slot, param, knobPos, l.hi.load());
                else if (r == 2) proc.setMacroRange (slot, param, l.lo.load(), knobPos);
                else if (r == 3) proc.setMacroRange (slot, param, l.hi.load(), l.lo.load());
                else if (r == 4) proc.setMacroRange (slot, param, 0.0f, 1.0f);
                else if (r == 5) proc.removeMacroLink (slot, param);
                else if (r == 6)
                {
                    kp->beginChangeGesture();
                    kp->setValueNotifyingHost (def);
                    kp->endChangeGesture();
                }
            });
    }

    //==========================================================================
    void showPresetMenu()
    {
        auto& pm = proc.getPresets();
        juce::PopupMenu menu, factory, user;
        const int nf = (int) getFactoryPresets().size();
        for (int i = 0; i < nf; ++i)
            factory.addItem (1000 + i, pm.getPresetName (i), true, i == pm.currentIndex);

        const auto users = pm.getUserPresetNames();
        for (int i = 0; i < users.size(); ++i)
            user.addItem (1000 + nf + i, users[i], true, nf + i == pm.currentIndex);
        if (users.isEmpty())
            user.addItem (1, "(no user presets yet)", false);

        menu.addSubMenu ("Factory", factory);
        menu.addSubMenu ("User", user);
        menu.addSeparator();
        menu.addItem (2, "Save preset as...");
        menu.addItem (3, "Delete user preset \"" + pm.currentName + "\"", users.contains (pm.currentName));
        menu.addSeparator();
        menu.addItem (4, "Import preset file...");
        menu.addItem (5, "Export preset file...");
        menu.addItem (6, "Open preset folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox).withMinimumWidth (270),
            [this] (int r)
            {
                auto& p = proc.getPresets();
                if (r >= 1000) p.load (r - 1000);
                else if (r == 2) showSaveDialog();
                else if (r == 3) { p.deleteUser (p.currentName); }
                else if (r == 4) importPreset();
                else if (r == 5) exportPreset();
                else if (r == 6) PresetManager::getUserDirectory().startAsProcess();
            });
    }

    void showSaveDialog()
    {
        saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
        saveDialog->addTextEditor ("name", proc.getPresets().currentName);
        saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        addAndMakeVisible (*saveDialog);
        saveDialog->setCentrePosition (getLocalBounds().getCentre());
        saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 1 && saveDialog != nullptr)
                proc.getPresets().saveUser (saveDialog->getTextEditorContents ("name"));
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RackContent> (this)]
            {
                if (safe != nullptr) safe->saveDialog.reset();
            });
        }), false);
    }

    void importPreset()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import preset", PresetManager::getUserDirectory(), "*.mrpreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile()) proc.getPresets().importFile (f);
            });
    }

    void exportPreset()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export preset",
                                                       PresetManager::getUserDirectory().getChildFile (proc.getPresets().currentName + ".mrpreset"),
                                                       "*.mrpreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File())
                    proc.getPresets().exportFile (f.withFileExtension ("mrpreset"));
            });
    }

    void showSettingsMenu()
    {
        juce::PopupMenu menu, size;
        const float scales[] = { 0.6f, 0.75f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f };
        for (int i = 0; i < 7; ++i)
            size.addItem (100 + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true,
                          std::abs (proc.uiScale - scales[i]) < 0.01f);
        menu.addSubMenu ("Window size", size);
        menu.addItem (1, "Clear rack");
        menu.addItem (2, "Reset everything to default");
        menu.addSeparator();
        menu.addItem (3, "About Multi Rack");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearBtn),
            [this, scales] (int r)
            {
                if (r >= 100) editor.setUiScale (scales[r - 100]);
                else if (r == 1) { proc.pushUndo(); proc.clearRack(); }
                else if (r == 2) { proc.pushUndo(); proc.getPresets().load (0); }
                else if (r == 3)
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Multi Rack",
                        "Multi Rack " JucePlugin_VersionString "\nModular channel-strip rack\n\n"
                        "8 slots, 15 modules, 8 macros, A/B, undo/redo, HQ oversampling, external side-chain.\n"
                        "Presets folder: " + PresetManager::getUserDirectory().getFullPathName(), "OK", this);
            });
    }

    MultiRackProcessor& proc;
    MultiRackEditor& editor;

    juce::TextButton presetBox { "Studio Strip" };
    IconButton prevBtn { IconButton::Icon::Left, "Previous preset" };
    IconButton nextBtn { IconButton::Icon::Right, "Next preset" };
    IconButton saveBtn { IconButton::Icon::Save, "Save preset" };
    IconButton undoBtn { IconButton::Icon::Undo, "Undo" };
    IconButton redoBtn { IconButton::Icon::Redo, "Redo" };
    juce::TextButton abA { "A" }, abB { "B" };
    IconButton abCopy { IconButton::Icon::Copy, "Copy current setting to the other A/B slot" };
    IconButton clearBtn { IconButton::Icon::Trash, "Clear rack" };
    juce::TextButton hqBtn { "HQ" };
    IconButton gearBtn { IconButton::Icon::Gear, "Settings" };
    Knob inKnob, outKnob, mixKnob;
    MiniMeter inMeter, outMeter;
    juce::OwnedArray<juce::SliderParameterAttachment> miniAtts;
    std::unique_ptr<juce::ButtonParameterAttachment> hqAtt;

    ModuleBrowser browser;
    MacroColumn macros;
    juce::Viewport viewport;
    juce::Component strip;
    juce::OwnedArray<SlotView> slotViews;
    SlotTabs tabs;

    std::unique_ptr<juce::AlertWindow> saveDialog;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace mr::ui

//==============================================================================
MultiRackEditor::MultiRackEditor (MultiRackProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    content = std::make_unique<mr::ui::RackContent> (p, *this);
    addAndMakeVisible (*content);
    addMouseListener (this, true);

    const float s = juce::jlimit (0.5f, 2.0f, proc.uiScale);
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kBaseW * 0.5f), juce::roundToInt (kBaseH * 0.5f), kBaseW * 2, kBaseH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setSize (juce::roundToInt (kBaseW * s), juce::roundToInt (kBaseH * s));

    startTimerHz (30);
}

MultiRackEditor::~MultiRackEditor()
{
    stopTimer();
    removeMouseListener (this);
    content.reset();
    setLookAndFeel (nullptr);
}

void MultiRackEditor::setUiScale (float s)
{
    setSize (juce::roundToInt (kBaseW * s), juce::roundToInt (kBaseH * s));
}

void MultiRackEditor::paint (juce::Graphics& g)
{
    g.fillAll (mr::ui::col::bg);
}

void MultiRackEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    proc.uiScale = scale;
    content->setBounds (0, 0, kBaseW, kBaseH);
    content->setTransform (juce::AffineTransform::scale (scale));
}

void MultiRackEditor::mouseDown (const juce::MouseEvent&) { proc.beginGesture(); }
void MultiRackEditor::mouseUp (const juce::MouseEvent&)   { proc.endGesture(); }

void MultiRackEditor::timerCallback()
{
    content->refresh();
}
