#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <memory>
#include <vector>

#include "Instruments/InstrumentLoader.h"

namespace halloweth
{
/** Which engine is currently audible. */
enum class EngineMode
{
    sampleMap = 0,
    synth = 1,
    drums = 2,
    sfz = 3
};

/** Complete synth patch state, published as one snapshot per block. */
struct SynthSettings
{
    int waveform = 0;             // 0 saw, 1 square, 2 sine, 3 triangle
    float detuneCents = 8.0f;
    float mix = 0.35f;
    float cutoffHz = 6000.0f;
    float resonance = 1.0f;
    float subLevel = 0.0f;
    float spread = 0.35f;
    float filterEnvAmount = 0.5f;
    float lfoDepth = 0.0f;
};

/**
    Owns every voice, the melodic instrument slot, the drum kit slot and the synth
    engine settings.

    Threading model
    ---------------
    - render() and the voice code run on the audio thread.
    - setInstrument()/setDrumInstrument()/setEngineMode() run on the message thread.
    - Sample data is immutable and never freed while playing: swapping an instrument
      only changes an atomic pointer, and previously loaded instruments stay resident.
      Resident instruments are pruned in prepare(), which never runs while audio flows.
*/
class AudioEngine
{
public:
    static constexpr int maxVoices = 16;
    static constexpr int numPads = 16;

    AudioEngine();

    /** Called from prepareToPlay(). Not real-time safe. */
    void prepare (double sampleRate, int samplesPerBlock);

    /** Stops all voices. */
    void reset();

    /** Audio thread: renders all voices for this block. */
    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    //==============================================================================
    /** Published every block from the parameter tree. */
    void setEnvelopeParameters (const juce::ADSR::Parameters& parameters) noexcept;
    juce::ADSR::Parameters getEnvelopeParameters() const noexcept;

    void setLoopEnabled (bool shouldLoop) noexcept;
    bool isLoopEnabled() const noexcept { return loopEnabled.load (std::memory_order_relaxed); }

    void setEngineMode (int mode) noexcept;
    int getEngineMode() const noexcept { return engineMode.load (std::memory_order_relaxed); }

    bool shouldUseSample() const noexcept;
    bool shouldUseSynth() const noexcept;
    bool shouldUseDrums() const noexcept;

    void setSynthSettings (const SynthSettings& settings) noexcept;
    SynthSettings getSynthSettings() const noexcept;

    //==============================================================================
    /** Melodic instrument (message thread). */
    void setInstrument (std::shared_ptr<const LoadedInstrument> newInstrument);
    void clearInstrument();

    const LoadedInstrument* getActiveInstrument() const noexcept
    {
        return activeInstrument.load (std::memory_order_acquire);
    }

    /** Audio thread: picks the zone for a note and advances the round-robin counter. */
    const LoadedZone* selectZone (int midiNote, int velocity) const noexcept;

    //==============================================================================
    /** Drum kit (message thread). */
    void setDrumInstrument (std::shared_ptr<const LoadedInstrument> newInstrument);
    void clearDrumInstrument();

    const LoadedInstrument* getDrumInstrument() const noexcept
    {
        return drumInstrument.load (std::memory_order_acquire);
    }

    const LoadedZone* selectDrumZone (int midiNote, int velocity) const noexcept;

    /** Per-pad trim, pan and tuning, applied at note-on. Message thread. */
    void setPadAdjustment (int padIndex, float gainDb, float pan, int semitones) noexcept;

    float getPadGain (int padIndex) const noexcept;
    float getPadPan (int padIndex) const noexcept;
    float getPadPitchRatio (int padIndex) const noexcept;

    //==============================================================================
    int getActiveVoiceCount() const noexcept;

private:
    void pruneResidentInstruments();

    juce::Synthesiser synth;

    std::atomic<const LoadedInstrument*> activeInstrument { nullptr };
    std::atomic<const LoadedInstrument*> drumInstrument { nullptr };
    std::vector<std::shared_ptr<const LoadedInstrument>> residentInstruments;
    mutable std::atomic<int> roundRobinCounter { 0 };

    std::atomic<int> engineMode { (int) EngineMode::synth };

    std::atomic<float> padGain[numPads];
    std::atomic<float> padPan[numPads];
    std::atomic<float> padPitchRatio[numPads];

    std::atomic<float> attackSeconds  { 0.008f };
    std::atomic<float> decaySeconds   { 0.18f };
    std::atomic<float> sustainLevel   { 0.80f };
    std::atomic<float> releaseSeconds { 0.26f };
    std::atomic<bool> loopEnabled { false };

    std::atomic<int> synthWaveform { 0 };
    std::atomic<float> synthDetune { 8.0f };
    std::atomic<float> synthMix { 0.35f };
    std::atomic<float> synthCutoff { 6000.0f };
    std::atomic<float> synthResonance { 1.0f };
    std::atomic<float> synthSub { 0.0f };
    std::atomic<float> synthSpread { 0.35f };
    std::atomic<float> synthFilterEnv { 0.5f };
    std::atomic<float> synthLfoDepth { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
}
