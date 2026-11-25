#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ModuleDefs.h"
#include "Modules.h"
#include "Presets.h"
#include <array>

class MultiRackProcessor final : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    MultiRackProcessor();
    ~MultiRackProcessor() override;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Multi Rack"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //== Rack editing (message thread) =========================================
    int  getSlotType (int slot) const { return slotTypeCache[(size_t) slot].load(); }
    void setSlotModule (int slot, int type);
    void resetSlot (int slot);
    void swapSlots (int a, int b);
    void moveSlot (int from, int to);
    void clearRack();
    void copySlotToClipboard (int slot);
    void pasteSlotFromClipboard (int slot);
    bool hasSlotClipboard() const { return slotClipboard.isValid(); }

    juce::RangedAudioParameter* getKnobParam (int slot, int index) const { return knobParams[(size_t) slot][(size_t) index]; }

    //== Macros ================================================================
    struct MacroLink
    {
        std::atomic<int>   macro { -1 };
        std::atomic<float> lo { 0.0f }, hi { 1.0f };
    };
    MacroLink& getLink (int slot, int index) { return links[(size_t) slot][(size_t) index]; }
    void assignMacro (int slot, int index, int macro);
    void setMacroRange (int slot, int index, float lo, float hi);
    void removeMacroLink (int slot, int index);
    void clearMacro (int macro);
    juce::String getMacroName (int m) const { return macroNames[(size_t) m]; }
    void setMacroName (int m, const juce::String& n) { macroNames[(size_t) m] = n; }
    float getMacroValue (int m) const { return macroParams[(size_t) m]->load(); }

    //== Whole-state helpers ===================================================
    juce::ValueTree getFullState();
    void setFullState (const juce::ValueTree& state);
    void resetToDefaults();
    void applyPreset (const mr::FactoryPreset& preset);

    //== Undo / Redo ===========================================================
    void beginGesture();
    void endGesture();
    void pushUndo();
    bool undo();
    bool redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    //== A/B ===================================================================
    int  getAB() const { return currentAB; }
    void selectAB (int which);
    void copyToOtherAB();

    //== Metering ==============================================================
    float getSlotMeter (int slot) const { return slotMeters[(size_t) slot].load(); }
    std::atomic<float> inMeter { -100.0f }, outMeter { -100.0f };

    mr::PresetManager& getPresets() { return presets; }

    float uiScale = 1.0f;
    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void setParamNotifying (juce::RangedAudioParameter* p, float norm);
    void pushUndoState (const juce::ValueTree& t);
    void setRunRate (bool hq);
    void processSubBlock (juce::AudioBuffer<float>& buffer, int start, int num, bool hasSidechain);
    void runRack (float* L, float* R, const float* scL, const float* scR, int n);

    std::array<std::atomic<int>, mr::kNumSlots> slotTypeCache {};
    std::array<std::atomic<int>, mr::kNumSlots> pendingType {};

    std::array<juce::AudioParameterChoice*, mr::kNumSlots> typeParams {};
    std::array<juce::RangedAudioParameter*, mr::kNumSlots> onParams {}, soloParams {}, outParams {};
    std::array<std::array<juce::RangedAudioParameter*, mr::kParamsPerSlot>, mr::kNumSlots> knobParams {};
    std::array<std::array<std::atomic<float>*, mr::kParamsPerSlot>, mr::kNumSlots> knobRaw {};
    std::array<std::atomic<float>*, mr::kNumSlots> onRaw {}, soloRaw {}, outRaw {};
    std::array<std::atomic<float>*, mr::kNumMacros> macroParams {};
    std::atomic<float>* inGainRaw = nullptr;
    std::atomic<float>* outGainRaw = nullptr;
    std::atomic<float>* mixRaw = nullptr;
    std::atomic<float>* hqRaw = nullptr;

    std::array<std::array<MacroLink, mr::kParamsPerSlot>, mr::kNumSlots> links;
    std::array<juce::String, mr::kNumMacros> macroNames;

    struct Slot
    {
        std::vector<std::unique_ptr<mr::ModuleDSP>> modules;
        int activeType = -1;
        mr::dsp::Ramp outRamp;
    };
    std::array<Slot, mr::kNumSlots> slots;
    std::array<std::atomic<float>, mr::kNumSlots> slotMeters {};

    static constexpr int kSubBlock = 256;
    static constexpr int kOversampleLog2 = 2; // 4x in HQ
    juce::AudioBuffer<float> work, scWork, dryWork;
    std::unique_ptr<juce::dsp::Oversampling<float>> osMain, osSide;
    mr::dsp::DelayLine dryDelay, wetDelay;
    mr::dsp::Ramp inRamp, outRamp, mixRamp;
    double baseRate = 44100.0;
    bool hqActive = false;
    int latency = 0;
    bool prepared = false;

    std::vector<juce::ValueTree> undoStack, redoStack;
    juce::ValueTree gestureStart;
    int gestureDepth = 0;

    juce::ValueTree abStates[2];
    int currentAB = 0;

    juce::ValueTree slotClipboard;

    mr::PresetManager presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiRackProcessor)
};
