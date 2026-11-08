#include "InstrumentLoader.h"

#include "AudioEngine/SampleLoader.h"

namespace halloweth
{
namespace
{
    juce::File resolveFile (const juce::String& path, const juce::File& baseFolder)
    {
        const juce::File file (path);
        return juce::File::isAbsolutePath (path) ? file : baseFolder.getChildFile (path);
    }

    std::shared_ptr<const LoadedInstrument> buildFromMap (const SampleMap& map,
                                                          const juce::File& baseFolder,
                                                          const juce::String& sourcePath,
                                                          juce::String& errorMessage)
    {
        auto instrument = std::make_shared<LoadedInstrument>();
        instrument->name = map.name;
        instrument->category = map.category;
        instrument->sourcePath = sourcePath;

        for (const auto& zone : map.zones)
        {
            LoadedZone loaded;
            loaded.zone = zone;
            loaded.zone.file = resolveFile (zone.file, baseFolder).getFullPathName();

            juce::String loadError;
            loaded.data = SampleLoader::loadFromFile (juce::File (loaded.zone.file), loadError);

            if (loaded.data == nullptr)
            {
                instrument->missingFiles.add (loaded.zone.file
                                              + (loadError.isNotEmpty() ? " (" + loadError + ")" : juce::String()));
            }
            else if (loaded.zone.loopEnd <= loaded.zone.loopStart && loaded.data->hasLoopPoints())
            {
                // Fall back to loop points stored in the audio file.
                loaded.zone.loopStart = loaded.data->loopStart;
                loaded.zone.loopEnd = loaded.data->loopEnd;
            }

            instrument->zones.push_back (std::move (loaded));
        }

        if (instrument->getNumPlayableZones() == 0)
        {
            errorMessage = "No zones could be loaded"
                           + (instrument->missingFiles.isEmpty()
                                  ? juce::String()
                                  : " (" + instrument->missingFiles.joinIntoString ("; ") + ")");
            return nullptr;
        }

        return instrument;
    }
}

int LoadedInstrument::getNumPlayableZones() const noexcept
{
    int count = 0;

    for (const auto& zone : zones)
        if (zone.isPlayable())
            ++count;

    return count;
}

const LoadedZone* LoadedInstrument::selectZone (int midiNote, int velocity,
                                                int roundRobinCounter) const
{
    std::vector<const LoadedZone*> exactMatches;
    std::vector<const LoadedZone*> keyMatches;
    std::vector<const LoadedZone*> anyPlayable;

    for (const auto& zone : zones)
    {
        if (! zone.isPlayable())
            continue;

        anyPlayable.push_back (&zone);

        const auto keyMatchesNote = midiNote >= zone.zone.keyLow && midiNote <= zone.zone.keyHigh;
        const auto velocityMatches = velocity >= zone.zone.velLow && velocity <= zone.zone.velHigh;

        if (keyMatchesNote && velocityMatches)
            exactMatches.push_back (&zone);
        else if (keyMatchesNote)
            keyMatches.push_back (&zone);
    }

    const auto* candidates = ! exactMatches.empty() ? &exactMatches
                            : ! keyMatches.empty()   ? &keyMatches
                                                     : &anyPlayable;

    if (candidates->empty())
        return nullptr;

    const auto index = (size_t) (juce::jmax (0, roundRobinCounter) % (int) candidates->size());
    return (*candidates)[index];
}

std::shared_ptr<const LoadedInstrument> InstrumentLoader::fromSingleSample (
    std::shared_ptr<const SampleData> sample, int rootNote)
{
    if (sample == nullptr)
        return nullptr;

    auto instrument = std::make_shared<LoadedInstrument>();
    instrument->name = sample->name;
    instrument->category = "Single Sample";
    instrument->sourcePath = sample->sourcePath;

    LoadedZone zone;
    zone.zone.file = sample->sourcePath;
    zone.zone.rootNote = juce::jlimit (0, 127, rootNote);
    zone.zone.keyLow = 0;
    zone.zone.keyHigh = 127;
    zone.zone.velLow = 1;
    zone.zone.velHigh = 127;
    zone.data = std::move (sample);

    instrument->zones.push_back (std::move (zone));
    return instrument;
}

std::shared_ptr<const LoadedInstrument> InstrumentLoader::loadMapFile (const juce::File& mapFile,
                                                                       juce::String& errorMessage)
{
    errorMessage.clear();

    if (! mapFile.existsAsFile())
    {
        errorMessage = "Map file not found";
        return nullptr;
    }

    juce::String parseError;
    auto map = SampleMap::fromJson (mapFile.loadFileAsString(), parseError);

    if (! map.isValid())
    {
        errorMessage = parseError.isNotEmpty() ? parseError : juce::String ("Map could not be parsed");
        return nullptr;
    }

    return buildFromMap (map, mapFile.getParentDirectory(), mapFile.getFullPathName(), errorMessage);
}

std::shared_ptr<const LoadedInstrument> InstrumentLoader::loadDrumKit (const DrumKit& kit,
                                                                       const juce::File& baseFolder,
                                                                       juce::String& errorMessage)
{
    errorMessage.clear();

    if (kit.isEmpty())
    {
        errorMessage = "Kit has no pads with samples";
        return nullptr;
    }

    auto map = kit.toSampleMap();
    map.name = kit.name;

    return buildFromMap (map, baseFolder, {}, errorMessage);
}
}
