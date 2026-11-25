#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ModulePanel.h"

class MultiRackProcessor;

namespace mr::ui
{
constexpr int kSlotW = kPanelW + 10;
constexpr int kHeaderH = 34;
constexpr int kFooterH = 44;
constexpr int kSlotH = kHeaderH + kPanelH + kFooterH;

//==============================================================================
class SlotNameButton final : public juce::TextButton
{
public:
    SlotNameButton (int slotIndex) : slot (slotIndex) {}
    void mouseDrag (const juce::MouseEvent&) override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String letter;

private:
    int slot;
    bool dragging = false;
};

//==============================================================================
/** One rack slot: header (power / solo / name / remove), faceplate and output trim. */
class SlotView final : public juce::Component,
                       public juce::DragAndDropTarget
{
public:
    SlotView (MultiRackProcessor& p, int slotIndex, juce::Component* popupParent);
    ~SlotView() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (int slot)> onModuleMenu;
    std::function<void (int slot, int param, juce::Component*)> onKnobMenu;

    const int slot;

private:
    void rebuildPanel();

    MultiRackProcessor& proc;
    juce::Component* popupParent;
    int shownType = -1, pendingShow = -1, stableTicks = 0;
    bool dropHover = false;

    IconButton power { IconButton::Icon::Power, "Module on/off" };
    juce::TextButton solo { "S" };
    SlotNameButton name;
    IconButton remove { IconButton::Icon::Close, "Remove module" };
    juce::Slider outGain;
    juce::Label outLabel, outValue;

    std::unique_ptr<juce::ButtonParameterAttachment> powerAtt, soloAtt;
    std::unique_ptr<juce::SliderParameterAttachment> outAtt;

    std::unique_ptr<ModulePanel> panel;
    std::unique_ptr<EmptyPanel> empty;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotView)
};

//==============================================================================
/** Tabs under the rack (A..H). Click to scroll to a slot, drag to reorder. */
class SlotTabs final : public juce::Component,
                       public juce::DragAndDropTarget
{
public:
    explicit SlotTabs (MultiRackProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (int slot)> onSelect;
    std::function<void (int from, int to)> onMove;
    std::function<void (int slot, int type)> onDropModule;

    int firstVisible = 0, lastVisible = 3;

private:
    int tabAt (juce::Point<int> p) const;
    juce::Rectangle<int> tabBounds (int i) const;

    MultiRackProcessor& proc;
    int pressed = -1, hoverDrop = -1;
    bool dragging = false;
};

//==============================================================================
class BrowserItem final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    explicit BrowserItem (int moduleType);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    const int type;
    std::function<void (int type)> onAdd;

private:
    bool hover = false, dragging = false;
};

/** Left side module browser with category filters and search. */
class ModuleBrowser final : public juce::Component
{
public:
    ModuleBrowser();
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (int type)> onAdd;

private:
    void updateList();

    juce::OwnedArray<juce::TextButton> catButtons;
    juce::TextEditor search;
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<BrowserItem> items;
    int category = -1; // -1 = all
};

//==============================================================================
/** Column of eight macro knobs. */
class MacroColumn final : public juce::Component
{
public:
    MacroColumn (MultiRackProcessor& p, juce::Component* popupParent);
    ~MacroColumn() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void showMenu (int macro);

    MultiRackProcessor& proc;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::SliderParameterAttachment> atts;
};

} // namespace mr::ui
