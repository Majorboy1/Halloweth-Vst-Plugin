#pragma once

#include <juce_core/juce_core.h>

#include <array>

#include "SampleMap.h"

namespace halloweth
{
/** One of the 16 drum pads. Pad i answers to MIDI note 36 + i. */
struct DrumPad
{
    juce::String name;
    juce::String file;

    float gainDb = 0.0f;
    float pan = 0.0f;
    int pitchSemitones = 0;

    bool isEmpty() const noexcept { return file.isEmpty(); }
};

/** A 16-pad drum kit. Persisted as JSON, both inside the plugin state and as files. */
struct DrumKit
{
    static constexpr int numPads = 16;
    static constexpr int firstNote = 36;   // C1 in JUCE's C3=60 naming

    juce::String name { "Untitled Kit" };
    std::array<DrumPad, numPads> pads;

    static int noteForPad (int padIndex) noexcept { return firstNote + padIndex; }

    int getNumLoadedPads() const noexcept;
    bool isEmpty() const noexcept { return getNumLoadedPads() == 0; }

    static DrumKit fromJson (const juce::String& json, juce::String& errorMessage);
    juce::String toJson() const;

    /** Builds a one-shot sample map for the loaded pads. The returned map keeps the
        kit's file paths; InstrumentLoader resolves them against baseFolder. */
    SampleMap toSampleMap() const;
};
}
