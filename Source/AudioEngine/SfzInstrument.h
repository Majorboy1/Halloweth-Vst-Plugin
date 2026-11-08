#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <sfizz.h>

#include <atomic>
#include <memory>
#include <vector>

namespace halloweth
{
/** One fully loaded SFZ instance. Create and destroy it away from the audio thread. */
class SfzInstrument
{
public:
    static std::unique_ptr<SfzInstrument> loadFromFile (const juce::File& file,
                                                        double sampleRate,
                                                        int maximumBlockSize,
                                                        juce::String& error);

    ~SfzInstrument();

    SfzInstrument (const SfzInstrument&) = delete;
    SfzInstrument& operator= (const SfzInstrument&) = delete;

    void prepare (double sampleRate, int maximumBlockSize) noexcept;
    void allSoundsOff() noexcept;
    void render (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi,
                 juce::AudioBuffer<float>& monoScratch) noexcept;

    int getActiveVoiceCount() const noexcept;
    int getNumRegions() const noexcept;

private:
    SfzInstrument() = default;

    sfizz_synth_t* synth = nullptr;
    int regionCount = 0;
};

/** Publishes loaded SFZ instances without locking or freeing from the audio thread. */
class SfzEngine
{
public:
    SfzEngine() = default;
    ~SfzEngine() = default;

    SfzEngine (const SfzEngine&) = delete;
    SfzEngine& operator= (const SfzEngine&) = delete;

    void prepare (double sampleRate, int maximumBlockSize);
    void install (std::shared_ptr<SfzInstrument> instrument);
    void clear();
    void render (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi) noexcept;
    void allSoundsOff() noexcept;
    int getActiveVoiceCount() const noexcept;
    int getNumRegions() const noexcept;
    bool hasInstrument() const noexcept { return active.load (std::memory_order_acquire) != nullptr; }

private:
    void pruneRetired();

    std::vector<std::shared_ptr<SfzInstrument>> residentInstruments;
    std::atomic<SfzInstrument*> active { nullptr };
    mutable std::atomic<int> audioReaders { 0 };
    juce::AudioBuffer<float> monoScratch;
    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
};
}