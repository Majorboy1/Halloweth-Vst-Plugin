#include "PresetManager.h"

#include "FactoryPresets.h"
#include "Parameters/ParameterIDs.h"

namespace halloweth
{
namespace
{
    juce::String sanitiseName (const juce::String& name)
    {
        auto cleaned = name.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                              "0123456789 -_");

        cleaned = cleaned.trim();

        if (cleaned.isEmpty())
            cleaned = "Preset";

        return cleaned;
    }

    juce::var makeFactoryData (const FactoryPresetDefinition& definition)
    {
        const auto isGuitar = juce::String (definition.category) == "Guitars";
        const auto isBundledPiano = juce::String (definition.name) == "Meta Upright Piano";
        const auto presetName = juce::String (definition.name);
        const auto isBundledDrumKit = presetName == "Afrobeats Kit"
                                      || presetName == "Amapiano Kit"
                                      || presetName == "Gospel Percussion"
                                      || presetName == "Highlife Kit"
                                      || presetName == "Afro Percussion";
        const auto guitarMap = juce::String (definition.name).contains ("Steel")
                                   ? "steel-guitar/steel-guitar.json"
                                   : "nylon-guitar/nylon-guitar.json";
        const auto drumKitPath = presetName == "Amapiano Kit"
                                     ? "DrumKits/Amapiano/kit.json"
                                 : presetName == "Highlife Kit"
                                     ? "DrumKits/Highlife/kit.json"
                                 : presetName == "Gospel Percussion"
                                     ? "DrumKits/Gospel/kit.json"
                                 : presetName == "Afro Percussion"
                                     ? "DrumKits/AfroPercussion/kit.json"
                                     : "DrumKits/Afrobeats/kit.json";

        juce::DynamicObject::Ptr root (new juce::DynamicObject());
        root->setProperty ("name", definition.name);
        root->setProperty ("category", definition.category);
        root->setProperty ("genre", definition.genre);
        root->setProperty ("description", definition.description);
        root->setProperty ("needsSample", definition.needsSample);
        root->setProperty ("engineMode", isGuitar ? 0 : isBundledPiano ? 3 : definition.engineMode);
        root->setProperty ("instrumentPath", isGuitar
                              ? "@HALLOWETH_RESOURCE:GuitarSamples/" + juce::String (guitarMap)
                              : isBundledPiano
                                  ? "@HALLOWETH_RESOURCE:PianoSamples/meta-piano/meta-piano.sfz"
                                  : isBundledDrumKit
                                      ? "@HALLOWETH_RESOURCE:" + juce::String (drumKitPath)
                                  : juce::String());
        root->setProperty ("instrumentIsMap", isGuitar);
        root->setProperty ("instrumentIsSfz", isBundledPiano);
        root->setProperty ("instrumentIsDrumKit", isBundledDrumKit);
        root->setProperty ("isFactory", true);

        juce::DynamicObject::Ptr parameters (new juce::DynamicObject());

        auto set = [&parameters] (const char* id, double value)
        {
            parameters->setProperty (id, value);
        };

        set (param::synthWave, definition.waveform);
        set (param::synthDetune, (double) definition.detune);
        set (param::synthMix, (double) definition.mix);
        set (param::synthCutoff, (double) definition.cutoff);
        set (param::synthResonance, (double) definition.resonance);
        set (param::synthSub, (double) definition.sub);
        set (param::synthSpread, (double) definition.spread);
        set (param::synthFilterEnv, (double) definition.filterEnv);
        set (param::synthLfoDepth, (double) definition.lfo);
        set (param::attack, (double) definition.attack);
        set (param::decay, (double) definition.decay);
        set (param::sustain, (double) definition.sustain);
        set (param::release, (double) definition.release);
        set (param::masterGain, -6.0);
        set (param::tone, 12000.0);

        root->setProperty ("parameters", juce::var (parameters.get()));
        return juce::var (root.get());
    }
}

//==============================================================================
PresetManager::PresetManager()
{
    addFactoryPresets();
    loadUserPresets();
    loadFavourites();
}

juce::File PresetManager::getPresetFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("Halloweth")
                      .getChildFile ("Presets");

    if (! folder.exists())
        folder.createDirectory();

    return folder;
}

juce::File PresetManager::getFavouritesFile()
{
    return getPresetFolder().getChildFile ("favourites.json");
}

void PresetManager::addFactoryPresets()
{
    int count = 0;
    const auto* definitions = getFactoryPresetDefinitions (count);

    for (int i = 0; i < count; ++i)
    {
        const auto& definition = definitions[i];

        Entry entry;
        entry.info.name = definition.name;
        entry.info.category = definition.category;
        entry.info.genre = definition.genre;
        entry.info.description = definition.description;
        entry.info.isFactory = true;
        entry.info.needsSample = definition.needsSample;
        entry.data = makeFactoryData (definition);

        presets.push_back (std::move (entry));
    }
}

void PresetManager::loadUserPresets()
{
    // Drop previously loaded user presets, keep factory ones.
    presets.erase (std::remove_if (presets.begin(), presets.end(),
                                   [] (const Entry& entry) { return ! entry.info.isFactory; }),
                   presets.end());

    for (const auto& file : getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        const auto data = juce::JSON::parse (file.loadFileAsString());

        if (! data.isObject() || file == getFavouritesFile())
            continue;

        Entry entry;
        entry.file = file;
        entry.data = data;
        entry.info.name = data.getProperty ("name", juce::var()).toString();

        if (entry.info.name.isEmpty())
            entry.info.name = file.getFileNameWithoutExtension();

        entry.info.category = data.getProperty ("category", juce::var()).toString();
        entry.info.genre = data.getProperty ("genre", juce::var()).toString();
        entry.info.description = data.getProperty ("description", juce::var()).toString();
        entry.info.needsSample = (bool) data.getProperty ("needsSample", false);
        entry.info.isFactory = false;

        presets.push_back (std::move (entry));
    }
}

void PresetManager::rescan()
{
    loadUserPresets();
}

void PresetManager::loadFavourites()
{
    favourites.clear();

    const auto file = getFavouritesFile();

    if (! file.existsAsFile())
        return;

    const auto data = juce::JSON::parse (file.loadFileAsString());

    if (const auto* array = data.getArray())
        for (const auto& item : *array)
            favourites.add (item.toString());
}

void PresetManager::saveFavourites() const
{
    juce::Array<juce::var> array;

    for (const auto& name : favourites)
        array.add (name);

    getFavouritesFile().replaceWithText (juce::JSON::toString (juce::var (array), true));
}

bool PresetManager::isFavourite (const juce::String& name) const
{
    return favourites.contains (name);
}

void PresetManager::toggleFavourite (const juce::String& name)
{
    if (favourites.contains (name))
        favourites.removeString (name);
    else
        favourites.add (name);

    saveFavourites();
}

const PresetInfo& PresetManager::getPreset (int index) const
{
    static const PresetInfo empty;
    return juce::isPositiveAndBelow (index, (int) presets.size())
               ? presets[(size_t) index].info
               : empty;
}

PresetLoadResult PresetManager::applyPreset (int index, juce::AudioProcessorValueTreeState& apvts)
{
    PresetLoadResult result;

    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return result;

    const auto& entry = presets[(size_t) index];
    const auto data = entry.data;

    if (const auto* parameters = data.getProperty ("parameters", juce::var()).getDynamicObject())
    {
        const auto& values = parameters->getProperties();

        for (int i = 0; i < values.size(); ++i)
        {
            const auto id = values.getName (i).toString();

            if (auto* parameter = apvts.getParameter (id))
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 ((float) (double) values.getValueAt (i)));
        }
    }

    result.engineMode = (int) data.getProperty ("engineMode", 1);
    result.instrumentPath = data.getProperty ("instrumentPath", juce::var()).toString();
    result.instrumentIsMap = (bool) data.getProperty ("instrumentIsMap", false);
    result.instrumentIsSfz = (bool) data.getProperty ("instrumentIsSfz", false);
    result.instrumentIsDrumKit = (bool) data.getProperty ("instrumentIsDrumKit", false);

    if (auto* modeParameter = apvts.getParameter (param::engineMode))
        modeParameter->setValueNotifyingHost (modeParameter->convertTo0to1 ((float) result.engineMode));

    return result;
}

bool PresetManager::saveUserPreset (const juce::String& name,
                                    const juce::String& category,
                                    const juce::String& genre,
                                    const juce::String& description,
                                    bool needsSample,
                                    const juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& instrumentPath,
                                    bool instrumentIsMap,
                                    bool instrumentIsSfz)
{
    if (name.trim().isEmpty())
        return false;

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("name", name.trim());
    root->setProperty ("category", category.isEmpty() ? "User" : category);
    root->setProperty ("genre", genre.isEmpty() ? "User" : genre);
    root->setProperty ("description", description);
    root->setProperty ("needsSample", needsSample);
    root->setProperty ("instrumentPath", instrumentPath);
    root->setProperty ("instrumentIsMap", instrumentIsMap);
    root->setProperty ("instrumentIsSfz", instrumentIsSfz);
    root->setProperty ("isFactory", false);

    juce::DynamicObject::Ptr parameters (new juce::DynamicObject());

    for (auto* parameter : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            const auto value = ranged->convertFrom0to1 (ranged->getValue());
            parameters->setProperty (ranged->paramID, (double) value);
        }
    }

    root->setProperty ("parameters", juce::var (parameters.get()));

    const auto file = getPresetFolder().getChildFile (sanitiseName (name) + ".json");

    if (! file.replaceWithText (juce::JSON::toString (juce::var (root.get()), true)))
        return false;

    rescan();
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return false;

    auto& entry = presets[(size_t) index];

    if (entry.info.isFactory || entry.file == juce::File())
        return false;

    const auto removed = entry.file.deleteFile();
    rescan();
    return removed;
}

bool PresetManager::exportPreset (int index, const juce::File& destination) const
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return false;

    const auto& entry = presets[(size_t) index];
    return destination.replaceWithText (juce::JSON::toString (entry.data, true));
}

bool PresetManager::importPreset (const juce::File& source)
{
    if (! source.existsAsFile())
        return false;

    const auto data = juce::JSON::parse (source.loadFileAsString());

    if (! data.isObject())
        return false;

    auto name = data.getProperty ("name", juce::var()).toString();

    if (name.isEmpty())
        name = source.getFileNameWithoutExtension();

    const auto target = getPresetFolder().getChildFile (sanitiseName (name) + ".json");

    if (! target.replaceWithText (juce::JSON::toString (data, true)))
        return false;

    rescan();
    return true;
}
}
