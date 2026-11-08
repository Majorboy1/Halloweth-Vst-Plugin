#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace halloweth
{
/** Immutable audio data for one sample.

    Once constructed, a SampleData is never modified, so the audio thread may read
    it without locking. Ownership is shared: the engine keeps every loaded sample
    alive for as long as it might be referenced by a voice. */
struct SampleData
{
    juce::String name;
    juce::String sourcePath;

    juce::AudioBuffer<float> audio;

    double sourceSampleRate = 44100.0;
    int rootNote = 60;          // MIDI note that plays back at the original pitch
    int loopStart = 0;          // valid when loopEnd > loopStart
    int loopEnd = 0;

    int getNumSamples() const noexcept  { return audio.getNumSamples(); }
    int getNumChannels() const noexcept { return audio.getNumChannels(); }

    double getLengthInSeconds() const noexcept
    {
        return sourceSampleRate > 0.0 ? (double) getNumSamples() / sourceSampleRate : 0.0;
    }

    bool hasLoopPoints() const noexcept
    {
        return loopEnd > loopStart && loopEnd <= getNumSamples();
    }
};
}
