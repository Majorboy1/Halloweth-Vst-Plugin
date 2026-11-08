#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

#include "SampleMap.h"
#include "DrumKit.h"
#include "AudioEngine/SampleData.h"

namespace halloweth
{
/** A zone with its audio resolved. data is null when the file could not be read. */
struct LoadedZone
{
    SampleZone zone;
    std::shared_ptr<const SampleData> data;

    bool isPlayable() const noexcept { return data != nullptr && data->getNumSamples() > 0; }
};

/** An instrument ready for playback: zones plus their immutable audio. */
struct LoadedInstrument
{
    juce::String name;
    juce::String category;
    juce::String sourcePath;

    std::vector<LoadedZone> zones;
    juce::StringArray missingFiles;

    int getNumZones() const noexcept        { return (int) zones.size(); }
    int getNumPlayableZones() const noexcept;

    /** Chooses the zone for a note.

        Prefers key+velocity matches, then key matches, then any playable zone.
        Matching zones are treated as a round-robin group. Returns nullptr when
        nothing is playable. */
    const LoadedZone* selectZone (int midiNote, int velocity, int roundRobinCounter) const;
};

/** Builds LoadedInstrument objects. Reading files must happen off the audio thread. */
class InstrumentLoader
{
public:
    /** Loads a JSON map and every referenced sample. Relative paths resolve against
        the map file's folder. Returns nullptr (and an error) if the map is unusable. */
    static std::shared_ptr<const LoadedInstrument> loadMapFile (const juce::File& mapFile,
                                                               juce::String& errorMessage);

    /** Loads every pad file of a drum kit. Relative paths resolve against baseFolder. */
    static std::shared_ptr<const LoadedInstrument> loadDrumKit (const DrumKit& kit,
                                                                const juce::File& baseFolder,
                                                                juce::String& errorMessage);

    /** Builds a single-zone instrument from one audio file (root note C4, full range). */
    static std::shared_ptr<const LoadedInstrument> fromSingleSample (
        std::shared_ptr<const SampleData> sample, int rootNote);
};
}
