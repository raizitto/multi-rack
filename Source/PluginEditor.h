#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/RackViews.h"

namespace mr::ui { class RackContent; }

class MultiRackEditor final : public juce::AudioProcessorEditor,
                              public juce::DragAndDropContainer,
                              private juce::Timer
{
public:
    explicit MultiRackEditor (MultiRackProcessor&);
    ~MultiRackEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setUiScale (float s);

    static constexpr int kBaseW = 1480;
    static constexpr int kBaseH = 872;

private:
    void timerCallback() override;

    MultiRackProcessor& proc;
    mr::ui::RackLookAndFeel lnf;
    std::unique_ptr<mr::ui::RackContent> content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiRackEditor)
};
