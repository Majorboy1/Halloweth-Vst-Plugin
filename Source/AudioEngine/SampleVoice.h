#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Instruments/InstrumentLoader.h"

namespace halloweth
{
class AudioEngine;

/** Plays one zone from either the melodic or the drum instrument.

    Melodic: pitch-tracks from the zone root, supports looping.
    Drums:   one-shot, per-pad gain/pan/tuning read live from the engine. */
class SampleVoice : public juce::SynthesiserVoice
{
public:
    enum class Slot
    {
        melodic,
        drums
    };

    SampleVoice (const AudioEngine& ownerToUse, Slot slotToUse)
        : owner (ownerToUse), slot (slotToUse) {}

    bool canPlaySound (juce::SynthesiserSound* sound) override;

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;

    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate (double newRate) override;

private:
    void updateIncrement();

    const AudioEngine& owner;
    const Slot slot;

    const LoadedZone* zone = nullptr;

    juce::ADSR envelope;
    double position = 0.0;
    double noteRatio = 1.0;
    double increment = 1.0;
    float gainLeft = 0.0f;
    float gainRight = 0.0f;
    bool oneShot = false;
    bool looping = false;
    int loopStart = 0;
    int loopEnd = 0;
    int lastPitchWheelValue = 8192;
};
}
