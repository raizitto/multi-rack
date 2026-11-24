#include "Presets.h"
#include "PluginProcessor.h"

namespace mr
{
//==============================================================================
// Factory rack presets. Values are given in real units (dB, Hz, ms, choice index...).
//==============================================================================
const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Studio Strip", {
            { "channel",   { { 1, 3.0f }, { 2, 1 } } },
            { "quadeq",    {} },
            { "buscomp",   {} } } },

        { "Init (Empty Rack)", {} },

        { "Vocal Strip", {
            { "quadeq",     { { 12, 90.0f }, { 3, -2.0f }, { 4, 350.0f }, { 0, 2.5f }, { 1, 10000.0f } } },
            { "stresscomp", { { 0, 12.0f }, { 1, -6.0f }, { 2, 0.3f }, { 3, 250.0f }, { 4, 1 } } },
            { "deesser",    { { 0, 6500.0f }, { 1, -28.0f }, { 2, 8.0f } } },
            { "optocomp",   { { 0, 25.0f }, { 1, 2.0f } } },
            { "airlift",    { { 1, 3.0f }, { 0, 12000.0f }, { 2, 2.0f } } } } },

        { "Drum Bus", {
            { "channel",    { { 1, 4.0f }, { 2, 4 } } },
            { "transient",  { { 0, 35.0f }, { 1, -10.0f } } },
            { "buscomp",    { { 0, -18.0f }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 3.0f }, { 6, 60.0f } } },
            { "tape",       { { 1, 3.0f }, { 2, 1 } } } } },

        { "Mix Bus Glue", {
            { "programeq",  { { 0, 2 }, { 1, 3.0f }, { 2, 1.5f }, { 3, 5 }, { 4, 2.0f }, { 5, 7.0f } } },
            { "buscomp",    { { 0, -16.0f }, { 1, 1 }, { 2, 3 }, { 3, 4 }, { 4, 1.5f }, { 5, 80.0f } } },
            { "tape",       { { 1, 2.0f }, { 2, 2 } } },
            { "limiter",    { { 0, 1.5f }, { 1, -0.3f } } } } },

        { "Dark Strip", {
            { "heritageeq", { { 6, 3.0f }, { 7, 1 }, { 0, 2.0f }, { 1, 1 }, { 10, 1 }, { 9, 8.0f } } },
            { "optocomp",   { { 0, 35.0f }, { 1, 3.0f }, { 4, 4.0f } } },
            { "tape",       { { 1, 5.0f }, { 2, 1 } } } } },

        { "Warm Strip", {
            { "channel",    { { 1, 5.0f }, { 2, 3 } } },
            { "programeq",  { { 0, 1 }, { 1, 4.0f }, { 3, 4 }, { 4, 3.0f } } },
            { "optocomp",   { { 0, 30.0f }, { 1, 2.0f }, { 4, 5.0f } } },
            { "harmonics",  { { 0, 6.0f }, { 1, 0 }, { 4, 35.0f } } } } },

        { "Aggressive Strip", {
            { "channel",    { { 1, 8.0f }, { 2, 4 } } },
            { "stresscomp", { { 0, 20.0f }, { 1, -8.0f }, { 4, 5 }, { 7, 4.0f }, { 5, 70.0f } } },
            { "quadeq",     { { 0, 4.0f }, { 9, 3.0f }, { 3, -3.0f } } },
            { "limiter",    { { 0, 3.0f }, { 3, 2 } } } } },

        { "Bass Strip", {
            { "heritageeq", { { 8, 1 }, { 6, 3.0f }, { 7, 1 }, { 2, 2.0f }, { 3, 0 } } },
            { "stresscomp", { { 0, 10.0f }, { 1, -4.0f }, { 2, 0.4f }, { 3, 150.0f }, { 4, 1 } } },
            { "harmonics",  { { 0, 9.0f }, { 1, 0 }, { 2, -20.0f }, { 4, 40.0f } } },
            { "trimmer",    { { 3, 120.0f } } } } },

        { "Acoustic Guitar", {
            { "quadeq",     { { 12, 70.0f }, { 6, -3.0f }, { 7, 250.0f }, { 0, 2.0f }, { 1, 12000.0f } } },
            { "optocomp",   { { 0, 25.0f }, { 1, 1.5f } } },
            { "airlift",    { { 1, 2.5f }, { 2, 1.5f } } } } },

        { "Triple EQ", {
            { "quadeq",     {} },
            { "heritageeq", {} },
            { "programeq",  {} } } },

        { "Mastering Chain", {
            { "programeq",  { { 0, 1 }, { 1, 2.0f }, { 3, 6 }, { 4, 1.5f }, { 5, 8.0f }, { 8, 1.0f } } },
            { "buscomp",    { { 0, -14.0f }, { 1, 0 }, { 2, 4 }, { 3, 4 }, { 4, 1.0f }, { 5, 100.0f } } },
            { "airlift",    { { 1, 1.5f }, { 0, 16000.0f } } },
            { "trimmer",    { { 3, 100.0f } } },
            { "limiter",    { { 0, 3.0f }, { 1, -0.5f }, { 2, 120.0f } } } } },

        { "Lo-Fi Crunch", {
            { "harmonics",  { { 0, 14.0f }, { 1, 4 }, { 4, 45.0f } } },
            { "tape",       { { 1, 7.0f }, { 2, 0 }, { 4, 6.0f } } },
            { "quadeq",     { { 12, 200.0f }, { 13, 6000.0f } } } } },

        { "Gated Drums", {
            { "gate",       { { 0, -30.0f }, { 1, 60.0f }, { 3, 40.0f }, { 4, 120.0f } } },
            { "transient",  { { 0, 40.0f }, { 1, -30.0f } } },
            { "stresscomp", { { 0, 14.0f }, { 1, -6.0f }, { 4, 5 }, { 5, 50.0f } } } } },
    };
    return presets;
}

//==============================================================================
juce::File PresetManager::getUserDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Multi Rack").getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getUserDirectory().findChildFiles (juce::File::findFiles, false, "*.mrpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

int PresetManager::getNumPresets() const
{
    return (int) getFactoryPresets().size() + getUserPresetNames().size();
}

juce::String PresetManager::getPresetName (int index) const
{
    const int nf = (int) getFactoryPresets().size();
    if (index < nf) return getFactoryPresets()[(size_t) index].name;
    return getUserPresetNames()[index - nf];
}

bool PresetManager::isFactory (int index) const
{
    return index < (int) getFactoryPresets().size();
}

bool PresetManager::load (int index)
{
    const int nf = (int) getFactoryPresets().size();
    if (index < 0 || index >= getNumPresets()) return false;

    if (index < nf)
    {
        proc.pushUndo();
        proc.applyPreset (getFactoryPresets()[(size_t) index]);
        currentName = getFactoryPresets()[(size_t) index].name;
        currentIndex = index;
        return true;
    }

    const auto name = getUserPresetNames()[index - nf];
    if (loadUser (name))
    {
        currentIndex = index;
        return true;
    }
    return false;
}

bool PresetManager::loadUser (const juce::String& name)
{
    const auto file = getUserDirectory().getChildFile (name + ".mrpreset");
    if (! importFile (file)) return false;
    currentName = name;
    const int pos = getUserPresetNames().indexOf (name);
    currentIndex = pos >= 0 ? (int) getFactoryPresets().size() + pos : -1;
    return true;
}

bool PresetManager::importFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (proc.apvts.state.getType())) return false;
    proc.pushUndo();
    proc.setFullState (tree);
    currentName = file.getFileNameWithoutExtension();
    return true;
}

bool PresetManager::exportFile (const juce::File& file)
{
    if (auto xml = proc.getFullState().createXml())
        return xml->writeTo (file);
    return false;
}

bool PresetManager::saveUser (const juce::String& nameIn)
{
    const auto name = juce::File::createLegalFileName (nameIn.trim());
    if (name.isEmpty()) return false;
    if (! exportFile (getUserDirectory().getChildFile (name + ".mrpreset"))) return false;
    currentName = name;
    const int pos = getUserPresetNames().indexOf (name);
    currentIndex = pos >= 0 ? (int) getFactoryPresets().size() + pos : -1;
    return true;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return getUserDirectory().getChildFile (name + ".mrpreset").deleteFile();
}

void PresetManager::next()
{
    const int n = getNumPresets();
    if (n == 0) return;
    load ((currentIndex + 1 + n) % n);
}

void PresetManager::previous()
{
    const int n = getNumPresets();
    if (n == 0) return;
    load (currentIndex < 0 ? n - 1 : (currentIndex - 1 + n) % n);
}

} // namespace mr
