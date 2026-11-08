#include "SampleMap.h"

namespace halloweth
{
namespace
{
    int readInt (const juce::var& object, const char* key, int fallback)
    {
        const auto value = object.getProperty (juce::Identifier (key), juce::var());

        if (value.isVoid() || value.isUndefined() || value.isArray() || value.isObject())
            return fallback;

        return (int) value;
    }

    bool readBool (const juce::var& object, const char* key, bool fallback)
    {
        const auto value = object.getProperty (juce::Identifier (key), juce::var());

        if (value.isVoid() || value.isUndefined())
            return fallback;

        return (bool) value;
    }

    juce::String readString (const juce::var& object, const char* key)
    {
        return object.getProperty (juce::Identifier (key), juce::var()).toString();
    }
}

SampleMap SampleMap::fromJson (const juce::String& json, juce::String& errorMessage)
{
    errorMessage.clear();

    const auto parsed = juce::JSON::parse (json);

    if (! parsed.isObject())
    {
        errorMessage = "Map file is not a JSON object";
        return {};
    }

    SampleMap map;
    map.name = readString (parsed, "name");
    map.category = readString (parsed, "category");

    const auto zonesVar = parsed.getProperty (juce::Identifier ("zones"), juce::var());
    const auto* zoneArray = zonesVar.getArray();

    if (zoneArray == nullptr)
    {
        errorMessage = "Map file has no \"zones\" array";
        return {};
    }

    for (const auto& zoneVar : *zoneArray)
    {
        if (! zoneVar.isObject())
            continue;

        SampleZone zone;
        zone.file = readString (zoneVar, "file");

        if (zone.file.isEmpty())
            continue;

        zone.keyLow = juce::jlimit (0, 127, readInt (zoneVar, "keyLow", 0));
        zone.keyHigh = juce::jlimit (0, 127, readInt (zoneVar, "keyHigh", 127));

        if (zone.keyHigh < zone.keyLow)
            std::swap (zone.keyLow, zone.keyHigh);

        zone.rootNote = juce::jlimit (0, 127, readInt (zoneVar, "rootNote", 60));
        zone.velLow = juce::jlimit (1, 127, readInt (zoneVar, "velLow", 1));
        zone.velHigh = juce::jlimit (1, 127, readInt (zoneVar, "velHigh", 127));

        if (zone.velHigh < zone.velLow)
            std::swap (zone.velLow, zone.velHigh);

        zone.roundRobin = juce::jmax (0, readInt (zoneVar, "roundRobin", 0));
        zone.loopStart = juce::jmax (0, readInt (zoneVar, "loopStart", 0));
        zone.loopEnd = juce::jmax (0, readInt (zoneVar, "loopEnd", 0));
        zone.oneShot = readBool (zoneVar, "oneShot", false);

        zone.gainDb = juce::jlimit (-60.0f, 24.0f,
                                    (float) (double) zoneVar.getProperty (juce::Identifier ("gainDb"), 0.0));
        zone.pan = juce::jlimit (-1.0f, 1.0f,
                                 (float) (double) zoneVar.getProperty (juce::Identifier ("pan"), 0.0));

        map.zones.push_back (std::move (zone));
    }

    if (map.zones.empty())
    {
        errorMessage = "Map contains no usable zones";
        return {};
    }

    if (map.name.isEmpty())
        map.name = "Untitled Map";

    return map;
}

juce::String SampleMap::toJson() const
{
    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("name", name);
    root->setProperty ("category", category);

    juce::Array<juce::var> zoneArray;

    for (const auto& zone : zones)
    {
        juce::DynamicObject::Ptr object (new juce::DynamicObject());
        object->setProperty ("file", zone.file);
        object->setProperty ("keyLow", zone.keyLow);
        object->setProperty ("keyHigh", zone.keyHigh);
        object->setProperty ("rootNote", zone.rootNote);
        object->setProperty ("velLow", zone.velLow);
        object->setProperty ("velHigh", zone.velHigh);
        object->setProperty ("roundRobin", zone.roundRobin);
        object->setProperty ("loopStart", zone.loopStart);
        object->setProperty ("loopEnd", zone.loopEnd);
        object->setProperty ("oneShot", zone.oneShot);
        object->setProperty ("gainDb", (double) zone.gainDb);
        object->setProperty ("pan", (double) zone.pan);
        zoneArray.add (juce::var (object.get()));
    }

    root->setProperty ("zones", zoneArray);

    return juce::JSON::toString (juce::var (root.get()), true);
}
}
