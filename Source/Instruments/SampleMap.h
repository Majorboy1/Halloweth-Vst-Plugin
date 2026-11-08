#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace halloweth
{
/** One mapped region of a multisampled instrument.

    A zone describes which audio file plays for a range of notes and velocities.
    Zones sharing the same key/velocity range form a round-robin group. */
struct SampleZone
{
    juce::String file;          // absolute, or relative to the map file's folder

    int keyLow = 0;
    int keyHigh = 127;
    int rootNote = 60;
    int velLow = 1;
    int velHigh = 127;

    int roundRobin = 0;         // ordering hint inside a round-robin group
    int loopStart = 0;          // 0/0 = use the file's embedded loop, if any
    int loopEnd = 0;
    bool oneShot = false;       // ignore note-off (drums, hits, chops)

    float gainDb = 0.0f;        // per-zone trim
    float pan = 0.0f;           // -1 = left, 0 = centre, +1 = right
};

/** A serialisable description of a multisampled instrument.

    Map files are plain JSON so users can author and version them:

        {
          "name": "My Piano",
          "category": "Piano",
          "zones": [
            { "file": "samples/C4_v1.wav", "keyLow": 48, "keyHigh": 59,
              "rootNote": 48, "velLow": 1, "velHigh": 63 }
          ]
        }
*/
struct SampleMap
{
    juce::String name;
    juce::String category;
    std::vector<SampleZone> zones;

    static SampleMap fromJson (const juce::String& json, juce::String& errorMessage);
    juce::String toJson() const;

    bool isValid() const noexcept { return ! zones.empty(); }
};
}
