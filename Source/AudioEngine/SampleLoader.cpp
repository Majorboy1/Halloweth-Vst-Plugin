#include "SampleLoader.h"

#include <mutex>

namespace halloweth
{
namespace
{
    juce::AudioFormatManager& getFormatManager()
    {
        static juce::AudioFormatManager manager;
        static std::once_flag once;

        std::call_once (once, [] { manager.registerBasicFormats(); });
        return manager;
    }
}

std::shared_ptr<const SampleData> SampleLoader::fromBuffer (juce::AudioBuffer<float> audio,
                                                            double sourceSampleRate,
                                                            int rootNote,
                                                            const juce::String& name)
{
    if (audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0 || sourceSampleRate <= 0.0)
        return nullptr;

    auto data = std::make_shared<SampleData>();
    data->name = name.isNotEmpty() ? name : juce::String ("Sample");
    data->audio = std::move (audio);
    data->sourceSampleRate = sourceSampleRate;
    data->rootNote = juce::jlimit (0, 127, rootNote);
    return data;
}

std::shared_ptr<const SampleData> SampleLoader::loadFromFile (const juce::File& file,
                                                              juce::String& errorMessage)
{
    errorMessage.clear();

    if (! file.existsAsFile())
    {
        errorMessage = "File not found";
        return nullptr;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (getFormatManager().createReaderFor (file));

    if (reader == nullptr)
    {
        errorMessage = "Unsupported or unreadable audio file";
        return nullptr;
    }

    if (reader->lengthInSamples <= 0 || reader->numChannels == 0)
    {
        errorMessage = "File contains no audio";
        return nullptr;
    }

    const auto sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    const auto maxSamples = (juce::int64) (sampleRate * maxLengthInSeconds);
    const auto numSamples = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    const auto numChannels = juce::jlimit (1, 2, (int) reader->numChannels);

    if (numSamples <= 0)
    {
        errorMessage = "File contains no audio";
        return nullptr;
    }

    auto data = std::make_shared<SampleData>();
    data->name = file.getFileNameWithoutExtension();
    data->sourcePath = file.getFullPathName();
    data->sourceSampleRate = sampleRate;
    data->audio.setSize (numChannels, numSamples);
    data->audio.clear();

    reader->read (&data->audio, 0, numSamples, 0, true, numChannels > 1);

    // Loop points embedded by editors (RIFF "smpl" chunk) are read by JUCE's WAV reader.
    const auto numLoops = reader->metadataValues.getValue ("NumSampleLoops", "0").getIntValue();

    if (numLoops > 0)
    {
        const auto loopStart = reader->metadataValues.getValue ("Loop0Start", "-1").getIntValue();
        const auto loopEnd   = reader->metadataValues.getValue ("Loop0End", "-1").getIntValue();

        if (loopStart >= 0 && loopEnd > loopStart && loopEnd <= numSamples)
        {
            data->loopStart = loopStart;
            data->loopEnd = loopEnd;
        }
    }

    return data;
}
}
