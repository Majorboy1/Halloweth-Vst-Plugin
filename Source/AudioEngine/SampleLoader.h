#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

#include "SampleData.h"

namespace halloweth
{
/** Loads audio files into immutable SampleData objects.

    All functions must be called from a background or message thread - never from
    the audio callback. */
class SampleLoader
{
public:
    /** Longest sample accepted, in seconds. Streaming for longer files arrives with
        the library manager in Phase 5. */
    static constexpr double maxLengthInSeconds = 120.0;

    /** Reads a WAV/AIFF/FLAC/OGG file. Returns nullptr and fills errorMessage on failure. */
    static std::shared_ptr<const SampleData> loadFromFile (const juce::File& file,
                                                           juce::String& errorMessage);

    /** Wraps an in-memory buffer, e.g. for tests or generated content. */
    static std::shared_ptr<const SampleData> fromBuffer (juce::AudioBuffer<float> audio,
                                                        double sourceSampleRate,
                                                        int rootNote,
                                                        const juce::String& name);
};
}
