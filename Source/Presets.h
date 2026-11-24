#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <utility>

class MultiRackProcessor;

namespace mr
{
struct FactorySlot
{
    juce::String module;                         // module id
    std::vector<std::pair<int, float>> values;   // param index -> real value
};

struct FactoryPreset
{
    juce::String name;
    std::vector<FactorySlot> slots;
};

const std::vector<FactoryPreset>& getFactoryPresets();

class PresetManager
{
public:
    explicit PresetManager (MultiRackProcessor& p) : proc (p) {}

    static juce::File getUserDirectory();
    juce::StringArray getUserPresetNames() const;

    /** Combined list: factory presets first, then user presets. */
    int getNumPresets() const;
    juce::String getPresetName (int index) const;
    bool isFactory (int index) const;
    bool load (int index);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    bool deleteUser (const juce::String& name);
    bool importFile (const juce::File& file);
    bool exportFile (const juce::File& file);

    void next();
    void previous();

    juce::String currentName { "Studio Strip" };
    int currentIndex = -1;

private:
    MultiRackProcessor& proc;
};

} // namespace mr
