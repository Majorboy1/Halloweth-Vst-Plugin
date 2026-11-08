#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace halloweth
{
/** Description of one preset in the browser. */
struct PresetInfo
{
    juce::String name;
    juce::String category;
    juce::String genre;
    juce::String description;
    bool isFactory = false;
    bool needsSample = false;

    /** True when this preset expects audio the user has not supplied yet. */
    bool requiresExternalAudio() const noexcept { return needsSample; }
};

/** Result of loading a preset, so the processor can load any referenced instrument. */
struct PresetLoadResult
{
    juce::String instrumentPath;
    bool instrumentIsMap = false;
    bool instrumentIsSfz = false;
    bool instrumentIsDrumKit = false;
    int engineMode = 1;
};

/**
    Factory and user presets.

    Factory presets live in code (no files needed); user presets are JSON files in
    %APPDATA%/Halloweth/Presets. Each preset stores every parameter value plus the
    instantiated instrument and engine mode.
*/
class PresetManager
{
public:
    PresetManager();

    /** Re-reads the user preset folder. Call after importing files. */
    void rescan();

    int getNumPresets() const noexcept { return (int) presets.size(); }
    const PresetInfo& getPreset (int index) const;

    bool isFavourite (const juce::String& name) const;
    void toggleFavourite (const juce::String& name);

    /** Applies preset parameters to the plugin. Message thread only. */
    PresetLoadResult applyPreset (int index, juce::AudioProcessorValueTreeState& apvts);

    /** Returns the instrument reference without applying or changing parameters. */
    PresetLoadResult getInstrumentForPreset (int index) const;

    bool saveUserPreset (const juce::String& name,
                         const juce::String& category,
                         const juce::String& genre,
                         const juce::String& description,
                         bool needsSample,
                         const juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& instrumentPath,
                         bool instrumentIsMap,
                         bool instrumentIsSfz);

    bool deleteUserPreset (int index);

    bool exportPreset (int index, const juce::File& destination) const;
    bool importPreset (const juce::File& source);

    static juce::File getPresetFolder();
    static juce::File getFavouritesFile();

private:
    struct Entry
    {
        PresetInfo info;
        juce::var data;
        juce::File file;   // empty for factory presets
    };

    void addFactoryPresets();
    void loadUserPresets();
    void loadFavourites();
    void saveFavourites() const;
    void rebuildIndices();

    std::vector<Entry> presets;
    juce::StringArray favourites;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
}
