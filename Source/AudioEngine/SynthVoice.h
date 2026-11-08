#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace halloweth
{
class AudioEngine;

/** Two-oscillator subtractive synth voice (real synthesis - no samples involved).

    Sound design features:
    - PolyBLEP anti-aliased saw / square, plus sine and triangle
    - Detune and oscillator mix
    - Sub oscillator one octave down
    - Stereo spread between the two oscillators
    - Resonant TPT filter with a percussive filter envelope (follows the amp
      envelope's attack/decay) and a slow LFO for movement

    All settings are read from the engine every block, so the controls respond
    while notes are held. */
class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (const AudioEngine& ownerToUse) : owner (ownerToUse) {}

    bool canPlaySound (juce::SynthesiserSound* sound) override;

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;

    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate (double newRate) override;

private:
    void updateCoefficients (float cutoff);

    const AudioEngine& owner;

    juce::ADSR ampEnvelope;
    juce::ADSR filterEnvelope;

    double phase1 = 0.0;
    double phase2 = 0.0;
    double subPhase = 0.0;
    double baseFrequency = 440.0;
    float bendSemitones = 0.0f;
    float level = 0.0f;

    int noteNumber = 60;
    float velocityValue = 0.5f;

    double lfoPhase = 0.0;

    // TPT state-variable filter state, per channel
    float ic1[2] { 0.0f, 0.0f };
    float ic2[2] { 0.0f, 0.0f };

    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, filterK = 1.0f;
};
}
