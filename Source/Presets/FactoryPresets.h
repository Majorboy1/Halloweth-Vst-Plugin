#pragma once

#include <juce_core/juce_core.h>

namespace halloweth
{
/** One factory preset definition.

    All values are real parameter values (ms, Hz, %, semitone cents) so they read
    the same as the UI. */
struct FactoryPresetDefinition
{
    const char* name;
    const char* category;
    const char* genre;
    const char* description;

    int waveform;        // 0 saw, 1 square, 2 sine, 3 triangle
    float detune;        // cents
    float mix;           // %
    float cutoff;        // Hz
    float resonance;     // Q
    float sub;           // %
    float spread;        // %
    float filterEnv;     // %
    float lfo;           // %
    float attack;        // ms
    float decay;         // ms
    float sustain;       // %
    float release;       // ms

    int engineMode;      // 0 sample map, 1 synth, 2 drums
    bool needsSample;
};

/** The 50 factory presets. Presets that need audio say so explicitly. */
const FactoryPresetDefinition* getFactoryPresetDefinitions (int& count);
}
