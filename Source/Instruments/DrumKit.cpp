#include "DrumKit.h"

namespace halloweth
{
namespace
{
    float readFloat (const juce::var& object, const char* key, float fallback)
    {
        const auto value = object.getProperty (juce::Identifier (key), juce::var());

        if (value.isVoid() || value.isUndefined())
            return fallback;

        return (float) (double) value;
    }

    int readInt (const juce::var& object, const char* key, int fallback)
    {
        const auto value = object.getProperty (juce::Identifier (key), juce::var());

        if (value.isVoid() || value.isUndefined())
            return fallback;

        return (int) value;
    }

    juce::String readString (const juce::var& object, const char* key)
    {
        return object.getProperty (juce::Identifier (key), juce::var()).toString();
    }
}

int DrumKit::getNumLoadedPads() const noexcept
{
    int count = 0;

    for (const auto& pad : pads)
        if (! pad.isEmpty())
            ++count;

    return count;
}

DrumKit DrumKit::fromJson (const juce::String& json, juce::String& errorMessage)
{
    errorMessage.clear();

    const auto parsed = juce::JSON::parse (json);

    if (! parsed.isObject())
    {
        errorMessage = "Kit file is not a JSON object";
        return {};
    }

    DrumKit kit;
    const auto name = readString (parsed, "name");

    if (name.isNotEmpty())
        kit.name = name;

    const auto padsVar = parsed.getProperty (juce::Identifier ("pads"), juce::var());
    const auto* padArray = padsVar.getArray();

    if (padArray == nullptr)
    {
        errorMessage = "Kit file has no \"pads\" array";
        return {};
    }

    const auto count = juce::jmin ((int) padArray->size(), numPads);

    for (int i = 0; i < count; ++i)
    {
        const auto& padVar = padArray->getReference (i);

        if (! padVar.isObject())
            continue;

        auto& pad = kit.pads[(size_t) i];
        pad.file = readString (padVar, "file");
        pad.name = readString (padVar, "name");

        if (pad.name.isEmpty() && pad.file.isNotEmpty())
            pad.name = juce::File (pad.file).getFileNameWithoutExtension();

        pad.gainDb = juce::jlimit (-60.0f, 24.0f, readFloat (padVar, "gainDb", 0.0f));
        pad.pan = juce::jlimit (-1.0f, 1.0f, readFloat (padVar, "pan", 0.0f));
        pad.pitchSemitones = juce::jlimit (-24, 24, readInt (padVar, "pitch", 0));
    }

    return kit;
}

juce::String DrumKit::toJson() const
{
    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("name", name);

    juce::Array<juce::var> padArray;

    for (const auto& pad : pads)
    {
        juce::DynamicObject::Ptr object (new juce::DynamicObject());
        object->setProperty ("file", pad.file);
        object->setProperty ("name", pad.name);
        object->setProperty ("gainDb", (double) pad.gainDb);
        object->setProperty ("pan", (double) pad.pan);
        object->setProperty ("pitch", pad.pitchSemitones);
        padArray.add (juce::var (object.get()));
    }

    root->setProperty ("pads", padArray);
    return juce::JSON::toString (juce::var (root.get()), true);
}

SampleMap DrumKit::toSampleMap() const
{
    SampleMap map;
    map.name = name;
    map.category = "Drums";

    for (int i = 0; i < numPads; ++i)
    {
        const auto& pad = pads[(size_t) i];

        if (pad.isEmpty())
            continue;

        SampleZone zone;
        zone.file = pad.file;
        zone.keyLow = noteForPad (i);
        zone.keyHigh = noteForPad (i);

        // Pads play at their recorded pitch; tuning is applied live by the engine so
        // the knob responds without reloading files.
        zone.rootNote = noteForPad (i);
        zone.velLow = 1;
        zone.velHigh = 127;
        zone.oneShot = true;

        map.zones.push_back (std::move (zone));
    }

    return map;
}
}
