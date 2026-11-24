#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Widgets.h"
#include "ModuleDefs.h"

class MultiRackProcessor;

namespace mr::ui
{
constexpr int kPanelW = 282;
constexpr int kPanelH = 640;

/** Geometry of one control on a module faceplate (panel coordinates). */
struct ControlPlacement
{
    int param = 0;
    juce::Rectangle<float> control;
    juce::Rectangle<float> label;
};

struct PanelLayout
{
    juce::Rectangle<float> meter;
    std::vector<ControlPlacement> controls;
    std::vector<float> rowSeparators;
};

PanelLayout computeLayout (const ModuleInfo& info, juce::Rectangle<float> bounds);
void paintFaceplate (juce::Graphics& g, const ModuleInfo& info, juce::Rectangle<float> bounds, int seed, bool thumbnail);

//==============================================================================
/** The hardware style faceplate of one module, with all its controls attached to the slot's parameters. */
class ModulePanel final : public juce::Component
{
public:
    ModulePanel (MultiRackProcessor& p, int slot, int type, juce::Component* popupParent);
    ~ModulePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();   // meters + macro rings (called from the editor timer)

    std::function<void (int slot, int param, juce::Component* source)> onKnobMenu;

private:
    struct Control
    {
        int param = 0;
        std::unique_ptr<Knob> knob;
        std::unique_ptr<LampButton> button;
        std::unique_ptr<juce::SliderParameterAttachment> sAtt;
        std::unique_ptr<juce::ButtonParameterAttachment> bAtt;
    };

    MultiRackProcessor& proc;
    const int slot, type;
    const ModuleInfo& info;
    std::vector<Control> controls;
    std::unique_ptr<NeedleMeter> needle;
    std::unique_ptr<LedLadder> leds;
    std::unique_ptr<LevelBar> bar;
    juce::Image cache;
    PanelLayout layout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulePanel)
};

/** Placeholder shown in empty slots. */
class EmptyPanel final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onClick;
    bool dragHover = false;
};

} // namespace mr::ui
