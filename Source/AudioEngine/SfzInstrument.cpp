#include "SfzInstrument.h"

namespace halloweth
{
std::unique_ptr<SfzInstrument> SfzInstrument::loadFromFile (const juce::File& file,
                                                            double newSampleRate,
                                                            int newMaximumBlockSize,
                                                            juce::String& error)
{
    error.clear();

    if (! file.existsAsFile())
    {
        error = "SFZ file not found: " + file.getFullPathName();
        return nullptr;
    }

    auto instrument = std::unique_ptr<SfzInstrument> (new SfzInstrument());
    instrument->synth = sfizz_create_synth();

    if (instrument->synth == nullptr)
    {
        error = "Could not create the SFZ engine";
        return nullptr;
    }

    sfizz_set_num_voices (instrument->synth, 64);
    instrument->prepare (newSampleRate, newMaximumBlockSize);

    const auto path = file.getFullPathName().toStdString();

    if (! sfizz_load_file (instrument->synth, path.c_str()))
    {
        error = "Could not load SFZ: " + file.getFullPathName();
        return nullptr;
    }

    instrument->regionCount = sfizz_get_num_regions (instrument->synth);

    return instrument;
}

SfzInstrument::~SfzInstrument()
{
    if (synth != nullptr)
        sfizz_free (synth);
}

void SfzInstrument::prepare (double newSampleRate, int newMaximumBlockSize) noexcept
{
    if (synth == nullptr)
        return;

    sfizz_set_sample_rate (synth, (float) juce::jmax (1.0, newSampleRate));
    sfizz_set_samples_per_block (synth, juce::jmax (1, newMaximumBlockSize));
}

void SfzInstrument::allSoundsOff() noexcept
{
    if (synth != nullptr)
        sfizz_send_cc (synth, 0, 120, 0);
}

void SfzInstrument::render (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi,
                            juce::AudioBuffer<float>& monoScratch) noexcept
{
    if (synth == nullptr || output.getNumSamples() <= 0)
        return;

    const auto numSamples = output.getNumSamples();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto delay = juce::jlimit (0, numSamples - 1, metadata.samplePosition);

        if (message.isNoteOn())
            sfizz_send_note_on (synth, delay, message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff())
            sfizz_send_note_off (synth, delay, message.getNoteNumber(), message.getVelocity());
        else if (message.isController())
            sfizz_send_cc (synth, delay, message.getControllerNumber(), message.getControllerValue());
        else if (message.isPitchWheel())
            sfizz_send_pitch_wheel (synth, delay, message.getPitchWheelValue());
        else if (message.isChannelPressure())
            sfizz_send_channel_aftertouch (synth, delay, message.getChannelPressureValue());
    }

    if (output.getNumChannels() == 1)
    {
        if (monoScratch.getNumChannels() < 2 || monoScratch.getNumSamples() < numSamples)
            return;

        float* channels[2] { monoScratch.getWritePointer (0), monoScratch.getWritePointer (1) };
        sfizz_render_block (synth, channels, 2, numSamples);

        auto* destination = output.getWritePointer (0);
        const auto* left = monoScratch.getReadPointer (0);
        const auto* right = monoScratch.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
            destination[i] = 0.5f * (left[i] + right[i]);

        return;
    }

    float* channels[2] { output.getWritePointer (0), output.getWritePointer (1) };
    sfizz_render_block (synth, channels, 2, numSamples);
}

int SfzInstrument::getActiveVoiceCount() const noexcept
{
    return synth != nullptr ? sfizz_get_num_active_voices (synth) : 0;
}

int SfzInstrument::getNumRegions() const noexcept
{
    return regionCount;
}

void SfzEngine::prepare (double newSampleRate, int newMaximumBlockSize)
{
    sampleRate = juce::jmax (1.0, newSampleRate);
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    monoScratch.setSize (2, maximumBlockSize, false, false, true);

    for (auto& instrument : residentInstruments)
        instrument->prepare (sampleRate, maximumBlockSize);
}

void SfzEngine::install (std::shared_ptr<SfzInstrument> instrument)
{
    if (instrument == nullptr)
        return;

    instrument->prepare (sampleRate, maximumBlockSize);
    auto* newInstrument = instrument.get();
    residentInstruments.push_back (std::move (instrument));
    active.store (newInstrument, std::memory_order_seq_cst);
    pruneRetired();
}

void SfzEngine::clear()
{
    active.store (nullptr, std::memory_order_seq_cst);
    pruneRetired();
}

void SfzEngine::render (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi) noexcept
{
    audioReaders.fetch_add (1, std::memory_order_seq_cst);

    if (auto* instrument = active.load (std::memory_order_seq_cst))
        instrument->render (output, midi, monoScratch);

    audioReaders.fetch_sub (1, std::memory_order_seq_cst);
}

void SfzEngine::allSoundsOff() noexcept
{
    audioReaders.fetch_add (1, std::memory_order_seq_cst);

    if (auto* instrument = active.load (std::memory_order_seq_cst))
        instrument->allSoundsOff();

    audioReaders.fetch_sub (1, std::memory_order_seq_cst);
}

int SfzEngine::getActiveVoiceCount() const noexcept
{
    audioReaders.fetch_add (1, std::memory_order_seq_cst);
    const auto* instrument = active.load (std::memory_order_seq_cst);
    const auto count = instrument != nullptr ? instrument->getActiveVoiceCount() : 0;
    audioReaders.fetch_sub (1, std::memory_order_seq_cst);
    return count;
}

int SfzEngine::getNumRegions() const noexcept
{
    audioReaders.fetch_add (1, std::memory_order_seq_cst);
    const auto* instrument = active.load (std::memory_order_seq_cst);
    const auto count = instrument != nullptr ? instrument->getNumRegions() : 0;
    audioReaders.fetch_sub (1, std::memory_order_seq_cst);
    return count;
}

void SfzEngine::pruneRetired()
{
    if (audioReaders.load (std::memory_order_seq_cst) != 0)
        return;

    const auto* current = active.load (std::memory_order_seq_cst);
    residentInstruments.erase (
        std::remove_if (residentInstruments.begin(), residentInstruments.end(),
                        [current] (const auto& instrument) { return instrument.get() != current; }),
        residentInstruments.end());
}
}