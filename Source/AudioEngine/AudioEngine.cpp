#include "AudioEngine.h"

#include "SampleVoice.h"
#include "SynthVoice.h"

namespace halloweth
{
namespace
{
    struct EngineSound : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };
}

AudioEngine::AudioEngine()
{
    for (int i = 0; i < maxVoices; ++i)
        synth.addVoice (new SynthVoice (*this));

    for (int i = 0; i < maxVoices; ++i)
        synth.addVoice (new SampleVoice (*this, SampleVoice::Slot::melodic));

    for (int i = 0; i < maxVoices; ++i)
        synth.addVoice (new SampleVoice (*this, SampleVoice::Slot::drums));

    synth.addSound (new EngineSound());

    for (int i = 0; i < numPads; ++i)
    {
        padGain[i].store (1.0f, std::memory_order_relaxed);
        padPan[i].store (0.0f, std::memory_order_relaxed);
        padPitchRatio[i].store (1.0f, std::memory_order_relaxed);
    }
}

void AudioEngine::prepare (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    synth.allNotesOff (0, false);
    pruneResidentInstruments();
}

void AudioEngine::reset()
{
    synth.allNotesOff (0, false);
}

void AudioEngine::render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
void AudioEngine::setEnvelopeParameters (const juce::ADSR::Parameters& parameters) noexcept
{
    attackSeconds.store (juce::jlimit (0.0f, 10.0f, parameters.attack), std::memory_order_relaxed);
    decaySeconds.store (juce::jlimit (0.0f, 10.0f, parameters.decay), std::memory_order_relaxed);
    sustainLevel.store (juce::jlimit (0.0f, 1.0f, parameters.sustain), std::memory_order_relaxed);
    releaseSeconds.store (juce::jlimit (0.005f, 10.0f, parameters.release), std::memory_order_relaxed);
}

juce::ADSR::Parameters AudioEngine::getEnvelopeParameters() const noexcept
{
    return { attackSeconds.load (std::memory_order_relaxed),
             decaySeconds.load (std::memory_order_relaxed),
             sustainLevel.load (std::memory_order_relaxed),
             releaseSeconds.load (std::memory_order_relaxed) };
}

void AudioEngine::setLoopEnabled (bool shouldLoop) noexcept
{
    loopEnabled.store (shouldLoop, std::memory_order_relaxed);
}

void AudioEngine::setEngineMode (int mode) noexcept
{
    engineMode.store (juce::jlimit (0, 3, mode), std::memory_order_relaxed);
}

bool AudioEngine::shouldUseSample() const noexcept
{
    return getEngineMode() == (int) EngineMode::sampleMap && getActiveInstrument() != nullptr;
}

bool AudioEngine::shouldUseSynth() const noexcept
{
    const auto mode = getEngineMode();

    // Sample mode without an instrument falls back to the synth so MIDI stays audible.
    return mode == (int) EngineMode::synth
           || (mode == (int) EngineMode::sampleMap && getActiveInstrument() == nullptr);
}

bool AudioEngine::shouldUseDrums() const noexcept
{
    return getEngineMode() == (int) EngineMode::drums && getDrumInstrument() != nullptr;
}

void AudioEngine::setSynthSettings (const SynthSettings& settings) noexcept
{
    synthWaveform.store (juce::jlimit (0, 3, settings.waveform), std::memory_order_relaxed);
    synthDetune.store (juce::jlimit (0.0f, 50.0f, settings.detuneCents), std::memory_order_relaxed);
    synthMix.store (juce::jlimit (0.0f, 1.0f, settings.mix), std::memory_order_relaxed);
    synthCutoff.store (juce::jlimit (60.0f, 18000.0f, settings.cutoffHz), std::memory_order_relaxed);
    synthResonance.store (juce::jlimit (0.6f, 12.0f, settings.resonance), std::memory_order_relaxed);
    synthSub.store (juce::jlimit (0.0f, 1.0f, settings.subLevel), std::memory_order_relaxed);
    synthSpread.store (juce::jlimit (0.0f, 1.0f, settings.spread), std::memory_order_relaxed);
    synthFilterEnv.store (juce::jlimit (0.0f, 1.0f, settings.filterEnvAmount), std::memory_order_relaxed);
    synthLfoDepth.store (juce::jlimit (0.0f, 1.0f, settings.lfoDepth), std::memory_order_relaxed);
}

SynthSettings AudioEngine::getSynthSettings() const noexcept
{
    SynthSettings settings;
    settings.waveform = synthWaveform.load (std::memory_order_relaxed);
    settings.detuneCents = synthDetune.load (std::memory_order_relaxed);
    settings.mix = synthMix.load (std::memory_order_relaxed);
    settings.cutoffHz = synthCutoff.load (std::memory_order_relaxed);
    settings.resonance = synthResonance.load (std::memory_order_relaxed);
    settings.subLevel = synthSub.load (std::memory_order_relaxed);
    settings.spread = synthSpread.load (std::memory_order_relaxed);
    settings.filterEnvAmount = synthFilterEnv.load (std::memory_order_relaxed);
    settings.lfoDepth = synthLfoDepth.load (std::memory_order_relaxed);
    return settings;
}

//==============================================================================
void AudioEngine::setInstrument (std::shared_ptr<const LoadedInstrument> newInstrument)
{
    if (newInstrument == nullptr)
    {
        clearInstrument();
        return;
    }

    residentInstruments.push_back (std::move (newInstrument));
    activeInstrument.store (residentInstruments.back().get(), std::memory_order_release);
}

void AudioEngine::clearInstrument()
{
    activeInstrument.store (nullptr, std::memory_order_release);
}

const LoadedZone* AudioEngine::selectZone (int midiNote, int velocity) const noexcept
{
    const auto* instrument = activeInstrument.load (std::memory_order_acquire);

    if (instrument == nullptr)
        return nullptr;

    const auto counter = roundRobinCounter.fetch_add (1, std::memory_order_relaxed);
    return instrument->selectZone (midiNote, velocity, counter);
}

void AudioEngine::setDrumInstrument (std::shared_ptr<const LoadedInstrument> newInstrument)
{
    if (newInstrument == nullptr)
    {
        clearDrumInstrument();
        return;
    }

    residentInstruments.push_back (std::move (newInstrument));
    drumInstrument.store (residentInstruments.back().get(), std::memory_order_release);
}

void AudioEngine::clearDrumInstrument()
{
    drumInstrument.store (nullptr, std::memory_order_release);
}

const LoadedZone* AudioEngine::selectDrumZone (int midiNote, int velocity) const noexcept
{
    const auto* instrument = drumInstrument.load (std::memory_order_acquire);

    if (instrument == nullptr)
        return nullptr;

    const auto counter = roundRobinCounter.fetch_add (1, std::memory_order_relaxed);
    return instrument->selectZone (midiNote, velocity, counter);
}

void AudioEngine::setPadAdjustment (int padIndex, float gainDb, float pan, int semitones) noexcept
{
    if (! juce::isPositiveAndBelow (padIndex, numPads))
        return;

    padGain[padIndex].store (juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 24.0f, gainDb)),
                             std::memory_order_relaxed);
    padPan[padIndex].store (juce::jlimit (-1.0f, 1.0f, pan), std::memory_order_relaxed);
    padPitchRatio[padIndex].store ((float) std::pow (2.0, juce::jlimit (-24, 24, semitones) / 12.0),
                                   std::memory_order_relaxed);
}

float AudioEngine::getPadGain (int padIndex) const noexcept
{
    return juce::isPositiveAndBelow (padIndex, numPads)
               ? padGain[padIndex].load (std::memory_order_relaxed)
               : 1.0f;
}

float AudioEngine::getPadPan (int padIndex) const noexcept
{
    return juce::isPositiveAndBelow (padIndex, numPads)
               ? padPan[padIndex].load (std::memory_order_relaxed)
               : 0.0f;
}

float AudioEngine::getPadPitchRatio (int padIndex) const noexcept
{
    return juce::isPositiveAndBelow (padIndex, numPads)
               ? padPitchRatio[padIndex].load (std::memory_order_relaxed)
               : 1.0f;
}

void AudioEngine::pruneResidentInstruments()
{
    const auto* melodic = activeInstrument.load (std::memory_order_acquire);
    const auto* drums = drumInstrument.load (std::memory_order_acquire);

    std::vector<std::shared_ptr<const LoadedInstrument>> kept;

    for (auto& instrument : residentInstruments)
        if (instrument != nullptr && (instrument.get() == melodic || instrument.get() == drums))
            kept.push_back (instrument);

    residentInstruments = std::move (kept);
}

int AudioEngine::getActiveVoiceCount() const noexcept
{
    int count = 0;

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = synth.getVoice (i); voice != nullptr && voice->isVoiceActive())
            ++count;

    return count;
}
}
