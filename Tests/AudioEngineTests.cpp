#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <iostream>
#include <iterator>
#include <memory>

#include "../Source/AudioEngine/SampleLoader.h"
#include "../Source/Effects/EffectChain.h"
#include "../Source/Instruments/DrumKit.h"
#include "../Source/Instruments/InstrumentLoader.h"
#include "../Source/Instruments/SampleMap.h"
#include "../Source/PluginProcessor.h"

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;

    if (! condition)
        ++failures;
}

void renderBlock (halloweth::HallowethAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi)
{
    buffer.clear();
    processor.processBlock (buffer, midi);
    midi.clear();
}

std::shared_ptr<const halloweth::SampleData> makeSine (double frequency, double sampleRate,
                                                      double seconds, const juce::String& name,
                                                      int rootNote = 60)
{
    const auto numSamples = juce::jmax (1, (int) (sampleRate * seconds));
    juce::AudioBuffer<float> buffer (1, numSamples);
    auto* data = buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
        data[i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi
                                          * frequency * (double) i / sampleRate);

    return halloweth::SampleLoader::fromBuffer (std::move (buffer), sampleRate, rootNote, name);
}

bool writeWav (const juce::File& file, float frequency, double sampleRate, double seconds)
{    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (new juce::FileOutputStream (file), sampleRate, 1, 24, {}, 0));

    if (writer == nullptr)
        return false;

    const auto numSamples = juce::jmax (1, (int) (sampleRate * seconds));
    juce::AudioBuffer<float> buffer (1, numSamples);
    auto* data = buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
        data[i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi
                                          * frequency * (double) i / sampleRate);

    return writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
}

/** Goertzel magnitude of one frequency in a block - used to check filter and sub behaviour. */
double magnitudeAt (const float* data, int numSamples, double frequency, double sampleRate)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return 0.0;

    const auto omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto coefficient = 2.0 * std::cos (omega);

    double s1 = 0.0, s2 = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto s0 = (double) data[i] + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const auto power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::sqrt (juce::jmax (0.0, power)) * 2.0 / (double) numSamples;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    std::cout << "HALLOWETH engine tests" << std::endl;

    halloweth::HallowethAudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    auto setParamReal = [&processor] (const char* id, float realValue)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (realValue));
    };

    auto setChoice = [&processor] (const char* id, int index)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
    };

    // Renders a note and estimates the fundamental from positive zero crossings.
    auto measureFrequency = [&] (int note, int numBlocks)
    {
        // Clear any release tail from earlier measurements so it cannot skew crossings.
        processor.audioEngine.reset();
        renderBlock (processor, buffer, midi);

        setParamReal (halloweth::param::attack, 0.0f);
        setParamReal (halloweth::param::loopEnabled, 0.0f);

        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        int crossings = 0;
        float previous = 0.0f;
        int totalSamples = 0;

        for (int b = 0; b < numBlocks; ++b)
        {
            renderBlock (processor, buffer, midi);

            const auto* data = buffer.getReadPointer (0);

            for (int i = 0; i < blockSize; ++i)
            {
                if (previous <= 0.0f && data[i] > 0.0f)
                    ++crossings;

                previous = data[i];
            }

            totalSamples += blockSize;
        }

        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        renderBlock (processor, buffer, midi);

        return (double) crossings / ((double) totalSamples / sampleRate);
    };

    // ---------------------------------------------------------------- idle
    renderBlock (processor, buffer, midi);
    check (buffer.getMagnitude (0, 0, blockSize) < 1.0e-6f, "silent output with no MIDI input");

    // ---------------------------------------------------------------- synth engine
    setChoice (halloweth::param::engineMode, 1);   // synth
    setParamReal (halloweth::param::synthDetune, 0.0f);
    setParamReal (halloweth::param::synthMix, 0.0f);
    setChoice (halloweth::param::synthWave, 0);    // saw

    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    renderBlock (processor, buffer, midi);

    const auto synthPeak = buffer.getMagnitude (0, 0, blockSize);
    check (synthPeak > 0.001f, "synth note produces audio (peak " + juce::String (synthPeak, 4) + ")");
    check (std::isfinite (synthPeak) && synthPeak < 1.5f, "synth output is finite and sane");
    check (buffer.getMagnitude (1, 0, blockSize) > 0.001f, "synth signal on right channel");
    check (processor.getActiveVoiceCount() > 0, "active voice count reported to the UI");
    check (processor.getOutputPeak() > 0.0f, "output peak reported to the UI");

    midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    renderBlock (processor, buffer, midi);

    // Polyphony
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
    renderBlock (processor, buffer, midi);

    auto chordPeak = 0.0f;

    for (int i = 0; i < 20; ++i)
    {
        renderBlock (processor, buffer, midi);
        chordPeak = juce::jmax (chordPeak, buffer.getMagnitude (0, 0, blockSize));
    }

    check (chordPeak > 0.001f, "chord keeps producing audio");

    midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    renderBlock (processor, buffer, midi);

    for (int i = 0; i < 400; ++i)
        renderBlock (processor, buffer, midi);

    check (processor.getActiveVoiceCount() == 0, "voices released after all-notes-off");
    check (buffer.getMagnitude (0, 0, blockSize) < 1.0e-5f, "output returns to silence");

    // Synth pitch tracking (single oscillator, no detune)
    const auto synthFrequency = measureFrequency (69, 40);
    check (std::abs (synthFrequency - 440.0) < 10.0,
           "synth tracks pitch (A4 = " + juce::String (synthFrequency, 1) + " Hz)");

    // ---------------------------------------------------------------- presets & state
    check (processor.getNumPresets() >= 72, "factory bank contains a rich Halloweth palette");

    const auto guitarPreset = processor.findPresetByName ("Velvet Nylon Guitar");
    check (guitarPreset >= 0, "guitar palette is available");

    processor.applyPreset (guitarPreset);
    const auto guitarMode = processor.apvts.getRawParameterValue (halloweth::param::engineMode)->load();
    check (std::abs (guitarMode) < 0.01f, "guitar preset selects the recorded sample engine");

    for (int attempt = 0; attempt < 300 && processor.getLoadedInstrument() == nullptr; ++attempt)
    {
        juce::Thread::sleep (10);
        processor.pollSampleLoad();
    }

    const auto guitarInstrument = processor.getLoadedInstrument();
    check (guitarInstrument != nullptr && guitarInstrument->getNumPlayableZones() >= 45,
           "factory guitar preset loads its multisampled recordings");

    if (guitarInstrument != nullptr)
    {
        const auto* guitarZone = guitarInstrument->selectZone (60, 90, 0);
        check (guitarZone != nullptr && guitarZone->zone.file.endsWithIgnoreCase (".ogg"),
               "guitar preset selects an original recorded audio sample");

         midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
         renderBlock (processor, buffer, midi);
         check (buffer.getMagnitude (0, 0, blockSize) > 0.001f,
             "recorded guitar sample produces audio in the plugin engine");
         midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
         renderBlock (processor, buffer, midi);
    }

    const auto afroGuitarPreset = processor.findPresetByName ("Afrobeat Pocket Chords");
    check (afroGuitarPreset >= 0, "real guitar performance is available in Other Sounds");
    if (afroGuitarPreset >= 0)
    {
        check (processor.getPresetInfo (afroGuitarPreset).category == "Other Sounds",
               "recorded guitar preset is filed under Other Sounds");
        processor.applyPreset (afroGuitarPreset);
        check (std::abs (processor.apvts.getRawParameterValue (halloweth::param::engineMode)->load() - 3.0f) < 0.01f,
               "recorded guitar performance selects the SFZ sample engine");

        for (int attempt = 0; attempt < 1000 && processor.getSfzNumRegions() == 0; ++attempt)
        {
            juce::Thread::sleep (10);
            processor.pollSampleLoad();
        }

        check (processor.getSfzNumRegions() > 0,
               "Afrobeat chord guitar loads its recorded SFZ sample regions");
    }

    const auto highlifeGuitarPreset = processor.findPresetByName ("Clean Highlife Guitar");
    check (highlifeGuitarPreset >= 0, "melodic recorded guitar is available in Other Sounds");
    if (highlifeGuitarPreset >= 0)
    {
        processor.applyPreset (highlifeGuitarPreset);
        for (int attempt = 0; attempt < 1000 && processor.getSfzNumRegions() == 0; ++attempt)
        {
            juce::Thread::sleep (10);
            processor.pollSampleLoad();
        }

        juce::MidiBuffer sfzMidi;
        sfzMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, sfzMidi);
        check (buffer.getMagnitude (0, 0, blockSize) > 0.001f,
               "recorded highlife guitar sample produces audio through the SFZ engine");
    }

    processor.applyPreset (0);   // Warm Pad: triangle wave, 2200 Hz cutoff
    check (processor.getCurrentPresetIndex() == 0, "current preset index updates");

    const auto presetWave = processor.apvts.getRawParameterValue (halloweth::param::synthWave)->load();
    check (std::abs (presetWave - 3.0f) < 0.01f, "preset applies its parameters (triangle wave)");

    const auto presetCutoff = processor.apvts.getRawParameterValue (halloweth::param::synthCutoff)->load();
    check (std::abs (presetCutoff - 2200.0f) < 1.0f, "preset applies cutoff");

    processor.selectNextPreset (-1);
    check (processor.getCurrentPresetIndex() == processor.getNumPresets() - 1,
           "previous preset wraps around the list");

    processor.togglePresetFavourite (1);
    check (processor.isPresetFavourite (1), "favourite toggles on");
    processor.togglePresetFavourite (1);
    check (! processor.isPresetFavourite (1), "favourite toggles off");

    {
        const auto countBefore = processor.getNumPresets();
        processor.saveCurrentAsPreset ("HALLOWETH Unit Test Preset", "User", "Test", "unit test");
        check (processor.getNumPresets() == countBefore + 1, "user preset is saved");

        const auto userIndex = processor.findPresetByName ("HALLOWETH Unit Test Preset");
        check (userIndex >= 0, "saved preset can be found by name");

        if (userIndex >= 0)
            processor.deletePreset (userIndex);

        check (processor.getNumPresets() == countBefore, "user preset can be deleted");
    }

    processor.applyPreset (3);   // Choir Pad
    processor.togglePresetFavourite (3);

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);

    {
        halloweth::HallowethAudioProcessor restored;
        restored.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        restored.prepareToPlay (sampleRate, blockSize);
        restored.setStateInformation (saved.getData(), (int) saved.getSize());

        const auto restoredCutoff = restored.apvts.getRawParameterValue (halloweth::param::synthCutoff)->load();

        check (std::abs (restoredCutoff - 2500.0f) < 1.0f, "parameters restored from saved state");
        check (restored.getCurrentPresetIndex() == 3, "preset index restored by name");
        check (restored.isPresetFavourite (3), "favourites are shared across instances");

        restored.togglePresetFavourite (3);
    }

    // ---------------------------------------------------------------- buffer sizes
    processor.prepareToPlay (48000.0, 128);
    setParamReal (halloweth::param::attack, 0.0f);

    juce::AudioBuffer<float> smallBuffer (2, 128);
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90), 0);
    auto smallPeak = 0.0f;

    for (int i = 0; i < 40; ++i)
    {
        renderBlock (processor, smallBuffer, midi);
        smallPeak = juce::jmax (smallPeak, smallBuffer.getMagnitude (0, 0, 128));
    }

    check (smallPeak > 0.001f, "audio works at 48 kHz / 128 samples");

    processor.prepareToPlay (sampleRate, blockSize);
    midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    renderBlock (processor, buffer, midi);

    // ---------------------------------------------------------------- sample map model
    {
        halloweth::SampleMap map;
        map.name = "Test Map";
        map.category = "Piano";

        halloweth::SampleZone zone;
        zone.file = "samples/a.wav";
        zone.keyLow = 36;
        zone.keyHigh = 60;
        zone.rootNote = 48;
        zone.velLow = 1;
        zone.velHigh = 63;
        zone.roundRobin = 2;
        zone.loopStart = 100;
        zone.loopEnd = 200;
        zone.oneShot = true;
        map.zones.push_back (zone);

        juce::String error;
        const auto parsed = halloweth::SampleMap::fromJson (map.toJson(), error);

        check (parsed.isValid(), "map JSON round-trips" + (error.isNotEmpty() ? " (" + error + ")" : juce::String()));
        check (parsed.name == "Test Map" && parsed.category == "Piano", "map metadata round-trips");

        if (parsed.isValid())
        {
            const auto& z = parsed.zones.front();
            check (z.file == "samples/a.wav", "zone file round-trips");
            check (z.keyLow == 36 && z.keyHigh == 60 && z.rootNote == 48,
                   "zone key range and root round-trip");
            check (z.velLow == 1 && z.velHigh == 63, "zone velocity range round-trips");
            check (z.roundRobin == 2, "round robin index round-trips");
            check (z.loopStart == 100 && z.loopEnd == 200, "loop points round-trip");
            check (z.oneShot, "one-shot flag round-trips");
        }

        juce::String badError;
        const auto invalid = halloweth::SampleMap::fromJson ("{ \"name\": \"x\" }", badError);
        check (! invalid.isValid() && badError.isNotEmpty(), "map without zones is rejected");
    }

    // ---------------------------------------------------------------- zone selection
    {
        auto instrument = std::make_shared<halloweth::LoadedInstrument>();
        instrument->name = "Selection Test";

        auto addZone = [&instrument] (const juce::String& name, double frequency,
                                      int keyLow, int keyHigh, int rootNote,
                                      int velLow, int velHigh)
        {
            halloweth::LoadedZone zone;
            zone.zone.file = name;
            zone.zone.keyLow = keyLow;
            zone.zone.keyHigh = keyHigh;
            zone.zone.rootNote = rootNote;
            zone.zone.velLow = velLow;
            zone.zone.velHigh = velHigh;
            zone.data = makeSine (frequency, 44100.0, 1.0, name, rootNote);
            instrument->zones.push_back (std::move (zone));
        };

        addZone ("soft.wav", 220.0, 0, 127, 48, 1, 63);
        addZone ("hard1.wav", 330.0, 0, 127, 48, 64, 127);
        addZone ("hard2.wav", 440.0, 0, 127, 48, 64, 127);

        const auto* soft = instrument->selectZone (60, 40, 0);
        check (soft != nullptr && soft->zone.file == "soft.wav", "soft velocity selects the soft layer");

        const auto* hardFirst = instrument->selectZone (60, 100, 0);
        const auto* hardSecond = instrument->selectZone (60, 100, 1);
        const auto* hardWrap = instrument->selectZone (60, 100, 2);

        check (hardFirst != nullptr && hardFirst->zone.file == "hard1.wav", "round robin starts at the first layer");
        check (hardSecond != nullptr && hardSecond->zone.file == "hard2.wav", "round robin advances");
        check (hardWrap != nullptr && hardWrap->zone.file == "hard1.wav", "round robin wraps");

        check (instrument->getNumPlayableZones() == 3, "playable zone count reported");

        // Key ranges win over velocity when nothing matches both.
        auto keyed = std::make_shared<halloweth::LoadedInstrument>();
        halloweth::LoadedZone low;
        low.zone.file = "low.wav";
        low.zone.keyLow = 0;
        low.zone.keyHigh = 59;
        low.zone.rootNote = 40;
        low.data = makeSine (200.0, 44100.0, 1.0, "low", 40);
        keyed->zones.push_back (low);

        halloweth::LoadedZone high;
        high.zone.file = "high.wav";
        high.zone.keyLow = 60;
        high.zone.keyHigh = 127;
        high.zone.rootNote = 72;
        high.data = makeSine (400.0, 44100.0, 1.0, "high", 72);
        keyed->zones.push_back (high);

        const auto* lowZone = keyed->selectZone (40, 100, 0);
        const auto* highZone = keyed->selectZone (80, 100, 0);

        check (lowZone != nullptr && lowZone->zone.file == "low.wav", "key range selects the low zone");
        check (highZone != nullptr && highZone->zone.file == "high.wav", "key range selects the high zone");
    }

    // ---------------------------------------------------------------- multisample playback
    {
        auto instrument = std::make_shared<halloweth::LoadedInstrument>();
        instrument->name = "Two Zone";

        halloweth::LoadedZone low;
        low.zone.file = "low.wav";
        low.zone.keyLow = 0;
        low.zone.keyHigh = 59;
        low.zone.rootNote = 48;
        low.data = makeSine (220.0, 44100.0, 2.0, "low", 48);
        instrument->zones.push_back (low);

        halloweth::LoadedZone high;
        high.zone.file = "high.wav";
        high.zone.keyLow = 60;
        high.zone.keyHigh = 127;
        high.zone.rootNote = 72;
        high.data = makeSine (440.0, 44100.0, 2.0, "high", 72);
        instrument->zones.push_back (high);

        processor.audioEngine.setInstrument (instrument);
        setChoice (halloweth::param::engineMode, 0);   // sample map
        processor.prepareToPlay (sampleRate, blockSize);

        renderBlock (processor, buffer, midi);
        check (processor.audioEngine.shouldUseSample(), "engine switches to sample playback");

        const auto lowFrequency = measureFrequency (48, 40);
        check (std::abs (lowFrequency - 220.0) < 8.0,
               "low zone plays at its own root pitch (" + juce::String (lowFrequency, 1) + " Hz)");

        const auto highFrequency = measureFrequency (72, 40);
        check (std::abs (highFrequency - 440.0) < 15.0,
               "high zone plays at its own root pitch (" + juce::String (highFrequency, 1) + " Hz)");

        const auto octaveUp = measureFrequency (84, 30);
        check (std::abs (octaveUp - 880.0) < 30.0,
               "zone transposes within its key range (" + juce::String (octaveUp, 1) + " Hz)");
    }

    // ---------------------------------------------------------------- one-shot vs loop
    {
        auto sample = makeSine (440.0, sampleRate, 1.0, "One Shot", 69);
        auto instrument = halloweth::InstrumentLoader::fromSingleSample (sample, 69);
        processor.audioEngine.setInstrument (instrument);
        setChoice (halloweth::param::engineMode, 0);
        processor.prepareToPlay (sampleRate, blockSize);

        setParamReal (halloweth::param::loopEnabled, 0.0f);
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);

        for (int b = 0; b < 140; ++b)
            renderBlock (processor, buffer, midi);

        check (buffer.getMagnitude (0, 0, blockSize) < 1.0e-4f, "one-shot sample stops at its end");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);

        setParamReal (halloweth::param::loopEnabled, 1.0f);
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);

        auto loopedPeak = 0.0f;

        for (int b = 0; b < 140; ++b)
        {
            renderBlock (processor, buffer, midi);
            loopedPeak = juce::jmax (loopedPeak, buffer.getMagnitude (0, 0, blockSize));
        }

        check (loopedPeak > 0.001f, "looped sample keeps sounding past its end");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);
        setParamReal (halloweth::param::loopEnabled, 0.0f);
    }

    // ---------------------------------------------------------------- map files on disk
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("halloweth_phase3_tests");
        folder.deleteRecursively();
        folder.createDirectory();

        const auto goodFile = folder.getChildFile ("good.wav");
        check (writeWav (goodFile, 220.0f, 48000.0, 0.5), "test WAV written");

        juce::String mapJson =
            "{ \"name\": \"Disk Map\", \"category\": \"Piano\", \"zones\": ["
            "  { \"file\": \"good.wav\", \"keyLow\": 0, \"keyHigh\": 127, \"rootNote\": 60,"
            "    \"velLow\": 1, \"velHigh\": 127 },"
            "  { \"file\": \"absent.wav\", \"keyLow\": 0, \"keyHigh\": 127, \"rootNote\": 60,"
            "    \"velLow\": 1, \"velHigh\": 127 }"
            "]}";

        const auto mapFile = folder.getChildFile ("map.json");
        mapFile.replaceWithText (mapJson);

        juce::String error;
        const auto loaded = halloweth::InstrumentLoader::loadMapFile (mapFile, error);

        check (loaded != nullptr, "map file loads" + (error.isNotEmpty() ? " (" + error + ")" : juce::String()));

        if (loaded != nullptr)
        {
            check (loaded->name == "Disk Map", "map name read from file");
            check (loaded->getNumZones() == 2, "both zones present");
            check (loaded->getNumPlayableZones() == 1, "only the readable zone is playable");
            check (loaded->missingFiles.size() == 1, "missing sample file reported");
        }

        // Async path through the processor
        processor.clearSample();
        processor.loadSampleMapAsync (mapFile);

        for (int i = 0; i < 300 && processor.getLoadedInstrument() == nullptr; ++i)
        {
            juce::Thread::sleep (10);
            processor.pollSampleLoad();
        }

        check (processor.getLoadedInstrument() != nullptr, "async map load completes");
        check (processor.audioEngine.getActiveInstrument() != nullptr, "engine received the map instrument");
        check (processor.getSampleErrorText().isNotEmpty(), "missing zone surfaces in the status text");

        const auto mode = processor.apvts.getRawParameterValue (halloweth::param::engineMode)->load();
        check (mode < 0.5f, "loading a map switches the engine to sample mode");

        // A map with no readable zones must fail cleanly.
        const auto brokenMap = folder.getChildFile ("broken.json");
        brokenMap.replaceWithText ("{ \"name\": \"Broken\", \"zones\": [ { \"file\": \"nope.wav\" } ] }");

        juce::String brokenError;
        const auto broken = halloweth::InstrumentLoader::loadMapFile (brokenMap, brokenError);
        check (broken == nullptr && brokenError.isNotEmpty(), "map with no readable zones fails cleanly");

        folder.deleteRecursively();
    }

    // ---------------------------------------------------------------- SFZ engine
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("halloweth_sfz_tests");
        folder.deleteRecursively();
        folder.createDirectory();
        const auto samplesFolder = folder.getChildFile ("Samples");
        samplesFolder.createDirectory();

        const auto sampleFile = samplesFolder.getChildFile ("middleC.wav");
        check (writeWav (sampleFile, 440.0f, sampleRate, 1.0), "SFZ test sample written");

        const auto sfzFile = folder.getChildFile ("test.sfz");
        sfzFile.replaceWithText (
            "<control>\n"
            "default_path=Samples/\n"
            "<region> key=69 pitch_keycenter=69 sample=middleC.wav\n");

        processor.clearSample();
        processor.loadSfzFileAsync (sfzFile);

        for (int attempt = 0; attempt < 300 && processor.getSfzFilePath().isEmpty(); ++attempt)
        {
            juce::Thread::sleep (10);
            processor.pollSampleLoad();
        }

        check (processor.getSfzFilePath() == sfzFile.getFullPathName(), "SFZ loads asynchronously");
        check (processor.getSfzNumRegions() == 1, "SFZ region is parsed by sfizz");

        setChoice (halloweth::param::engineMode, 3);
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, midi);
        check (buffer.getMagnitude (0, 0, blockSize) > 0.001f,
               "SFZ note produces rendered audio");
        midi.addEvent (juce::MidiMessage::noteOff (1, 69), 0);
        renderBlock (processor, buffer, midi);

        juce::MemoryBlock savedState;
        processor.getStateInformation (savedState);

        {
            halloweth::HallowethAudioProcessor restored;
            restored.setPlayConfigDetails (0, 2, sampleRate, blockSize);
            restored.prepareToPlay (sampleRate, blockSize);
            restored.setStateInformation (savedState.getData(), (int) savedState.getSize());

            for (int attempt = 0; attempt < 300 && restored.getSfzFilePath().isEmpty(); ++attempt)
            {
                juce::Thread::sleep (10);
                restored.pollSampleLoad();
            }

            check (restored.getSfzFilePath() == sfzFile.getFullPathName(),
                   "SFZ file path restores from plugin state");

                 restored.clearSample();
                 juce::MemoryBlock clearedState;
                 restored.getStateInformation (clearedState);

                 halloweth::HallowethAudioProcessor cleared;
                 cleared.setPlayConfigDetails (0, 2, sampleRate, blockSize);
                 cleared.prepareToPlay (sampleRate, blockSize);
                 cleared.setStateInformation (clearedState.getData(), (int) clearedState.getSize());
                 check (cleared.getSfzFilePath().isEmpty(), "cleared SFZ path is removed from saved state");
        }

        processor.clearSample();
        folder.deleteRecursively();
    }

    // ---------------------------------------------------------------- drum engine
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("halloweth_drum_tests");
        folder.deleteRecursively();
        folder.createDirectory();

        const auto kickFile = folder.getChildFile ("kick.wav");
        check (writeWav (kickFile, 200.0f, 48000.0, 2.0), "drum sample written");

        halloweth::DrumKit kit;
        kit.name = "Test Kit";
        kit.pads[0].file = kickFile.getFullPathName();
        kit.pads[0].name = "Kick";
        kit.pads[0].gainDb = -3.0f;
        kit.pads[0].pan = 0.5f;

        juce::String kitError;
        const auto parsedKit = halloweth::DrumKit::fromJson (kit.toJson(), kitError);

        check (parsedKit.getNumLoadedPads() == 1, "kit JSON round-trips");
        check (parsedKit.name == "Test Kit", "kit name round-trips");
        check (std::abs (parsedKit.pads[0].gainDb + 3.0f) < 0.01f, "pad gain round-trips");
        check (std::abs (parsedKit.pads[0].pan - 0.5f) < 0.01f, "pad pan round-trips");

        const auto drumMap = kit.toSampleMap();
        check (drumMap.zones.size() == 1, "kit produces one zone per loaded pad");
        check (drumMap.zones[0].keyLow == 36 && drumMap.zones[0].keyHigh == 36,
               "pad maps to its own MIDI note");
        check (drumMap.zones[0].oneShot, "drum zones are one-shot");

        juce::String kitLoadError;
        auto drumInstrument = halloweth::InstrumentLoader::loadDrumKit (kit, folder, kitLoadError);

        check (drumInstrument != nullptr,
               "drum kit loads" + (kitLoadError.isNotEmpty() ? " (" + kitLoadError + ")" : juce::String()));

        if (drumInstrument != nullptr)
        {
            processor.audioEngine.setDrumInstrument (drumInstrument);
            setChoice (halloweth::param::engineMode, 2);   // drums
            processor.prepareToPlay (sampleRate, blockSize);
            processor.setPadAdjustment (0, 0.0f, 0.0f, 0);

            renderBlock (processor, buffer, midi);
            check (processor.audioEngine.shouldUseDrums(), "engine switches to the drum slot");

            const auto kickFrequency = measureFrequency (36, 40);
            check (std::abs (kickFrequency - 200.0) < 6.0,
                   "drum pad plays at its recorded pitch (" + juce::String (kickFrequency, 1) + " Hz)");

            // Tuning is applied live, without reloading the file.
            processor.setPadAdjustment (0, 0.0f, 0.0f, 12);
            const auto tunedFrequency = measureFrequency (36, 30);
            check (std::abs (tunedFrequency - 400.0) < 12.0,
                   "pad tuning transposes the sample (" + juce::String (tunedFrequency, 1) + " Hz)");

processor.setPadAdjustment (0, -48.0f, 0.0f, 0);
        processor.audioEngine.reset();   // kill the still-ringing one-shot
        renderBlock (processor, buffer, midi);

            auto quietPeak = 0.0f;

            for (int i = 0; i < 20; ++i)
            {
                renderBlock (processor, buffer, midi);
                quietPeak = juce::jmax (quietPeak, buffer.getMagnitude (0, 0, blockSize));
            }

            check (quietPeak < 0.01f, "pad level attenuates the pad");

            midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            renderBlock (processor, buffer, midi);

            // One-shot behaviour: note-off must not cut the hit short.
            processor.setPadAdjustment (0, 0.0f, 0.0f, 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110), 0);
            renderBlock (processor, buffer, midi);
            midi.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
            renderBlock (processor, buffer, midi);

            auto afterNoteOff = 0.0f;

            for (int i = 0; i < 10; ++i)
            {
                renderBlock (processor, buffer, midi);
                afterNoteOff = juce::jmax (afterNoteOff, buffer.getMagnitude (0, 0, blockSize));
            }

            check (afterNoteOff > 0.001f, "one-shot pad keeps playing after note-off");
        }

        // Async kit file load through the processor
        const auto kitFile = folder.getChildFile ("kit.json");
        kitFile.replaceWithText (kit.toJson());

        const auto revisionBefore = processor.getDrumKitRevision();
        processor.loadDrumKitAsync (kitFile);

        for (int i = 0; i < 300 && processor.getDrumKitRevision() == revisionBefore; ++i)
        {
            juce::Thread::sleep (10);
            processor.pollSampleLoad();
        }

        check (processor.getDrumKit().name == "Test Kit", "async kit load publishes the kit");
        check (processor.getDrumStatusText().contains ("Test Kit"), "kit status text reports the kit");

        // Factory drum presets must resolve their bundled kit JSON and samples
        // from the test app's Resources folder, just as they do in the plugin.
        const char* factoryKitPresets[] { "Afrobeats Kit", "Amapiano Kit", "Gospel Percussion",
                                          "Highlife Kit", "Afro Percussion" };
        const char* factoryKitNames[] { "Afrobeats Essentials", "Amapiano Bounce", "Afro Gospel Praise",
                                        "Highlife Guitar Band", "Afro Percussion Ensemble" };

        for (int i = 0; i < (int) std::size (factoryKitPresets); ++i)
        {
            const auto presetIndex = processor.findPresetByName (factoryKitPresets[i]);
            check (presetIndex >= 0, juce::String ("factory drum preset exists: ") + factoryKitPresets[i]);

            if (presetIndex < 0)
                continue;

            const auto revision = processor.getDrumKitRevision();
            processor.applyPreset (presetIndex);

            for (int attempt = 0; attempt < 300 && processor.getDrumKitRevision() == revision; ++attempt)
            {
                juce::Thread::sleep (10);
                processor.pollSampleLoad();
            }

            const auto& factoryKit = processor.getDrumKit();
            check (factoryKit.name == factoryKitNames[i],
                   juce::String (factoryKitPresets[i]) + " loads its bundled kit map");
            check (factoryKit.getNumLoadedPads() == halloweth::DrumKit::numPads
                       && processor.getDrumErrorText().isEmpty(),
                   juce::String (factoryKitPresets[i]) + " resolves all 16 bundled samples"
                       + (processor.getDrumErrorText().isNotEmpty()
                              ? " (" + processor.getDrumErrorText() + ")"
                              : juce::String()));

            midi.addEvent (juce::MidiMessage::noteOn (1, halloweth::DrumKit::firstNote,
                                                       (juce::uint8) 110), 0);
            renderBlock (processor, buffer, midi);
            check (buffer.getMagnitude (0, 0, blockSize) > 0.0001f,
                   juce::String (factoryKitPresets[i]) + " plays audio from its first pad");
            midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            renderBlock (processor, buffer, midi);
        }

        const auto savedFile = folder.getChildFile ("saved-kit.json");
        check (processor.saveDrumKit (savedFile), "kit saves to disk");

        folder.deleteRecursively();
    }

    // Every oscillator waveform must produce sound.
    {
        setChoice (halloweth::param::engineMode, 1);
        setParamReal (halloweth::param::synthCutoff, 8000.0f);
        setParamReal (halloweth::param::synthFilterEnv, 0.0f);
        setParamReal (halloweth::param::synthSpread, 0.0f);
        setParamReal (halloweth::param::synthDetune, 0.0f);
        setParamReal (halloweth::param::synthMix, 0.0f);
        setParamReal (halloweth::param::synthSub, 0.0f);
        setParamReal (halloweth::param::attack, 0.0f);
        setParamReal (halloweth::param::sustain, 100.0f);

        const char* waveNames[] { "saw", "square", "sine", "triangle" };

        for (int wave = 0; wave < 4; ++wave)
        {
            setChoice (halloweth::param::synthWave, wave);
            processor.prepareToPlay (sampleRate, blockSize);

            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            renderBlock (processor, buffer, midi);

            const auto peak = buffer.getMagnitude (0, 0, blockSize);

            check (peak > 0.001f,
                   juce::String ("waveform \"") + waveNames[wave] + "\" produces audio ("
                       + juce::String (peak, 4) + ")");

            midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            renderBlock (processor, buffer, midi);
        }
    }

    // ---------------------------------------------------------------- effects chain
    {
        halloweth::EffectChain chain;
        chain.prepare (sampleRate, blockSize);

        auto fillSine = [] (juce::AudioBuffer<float>& b, double frequency, float amplitude)
        {
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto* data = b.getWritePointer (ch);

                for (int i = 0; i < b.getNumSamples(); ++i)
                    data[i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi
                                                            * frequency * (double) i / 44100.0);
            }
        };

        auto rms = [] (const juce::AudioBuffer<float>& b)
        {
            double sum = 0.0;
            int count = 0;

            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const auto v = (double) b.getReadPointer (ch)[i];
                    sum += v * v;
                    ++count;
                }

            return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
        };

        auto maxDifference = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
        {
            float worst = 0.0f;

            for (int ch = 0; ch < a.getNumChannels(); ++ch)
                for (int i = 0; i < a.getNumSamples(); ++i)
                    worst = juce::jmax (worst, std::abs (a.getReadPointer (ch)[i]
                                                         - b.getReadPointer (ch)[i]));

            return worst;
        };

        // Bypass must be bit-transparent.
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 1000.0, 0.3f);
            juce::AudioBuffer<float> copy (audio);

            halloweth::EffectParameters bypassed;
            chain.process (audio, bypassed);

            check (maxDifference (audio, copy) == 0.0f, "bypassed effect chain is transparent");
        }

        // Low pass removes high frequencies.
        {
            chain.reset();
            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 4000.0, 0.5f);
            const auto before = rms (audio);

            halloweth::EffectParameters p;
            p.filterOn = true;
            p.filterCutoff = 500.0f;
            p.filterResonance = 0.707f;
            p.filterMode = 0;

            chain.process (audio, p);
            check (rms (audio) < before * 0.2, "low-pass filter attenuates high frequencies");
        }

        // High pass removes low frequencies.
        {
            chain.reset();
            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 200.0, 0.5f);
            const auto before = rms (audio);

            halloweth::EffectParameters p;
            p.filterOn = true;
            p.filterCutoff = 4000.0f;
            p.filterResonance = 0.707f;
            p.filterMode = 1;

            chain.process (audio, p);
            check (rms (audio) < before * 0.2, "high-pass filter attenuates low frequencies");
        }

        // Drive raises level and shapes the waveform.
        {
            chain.reset();
            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 1000.0, 0.2f);
            const auto before = rms (audio);

            halloweth::EffectParameters p;
            p.driveOn = true;
            p.driveAmount = 8.0f;
            p.driveTone = 1.0f;

            chain.process (audio, p);
            check (rms (audio) > before * 1.2, "drive pushes level and shapes the signal");
        }

        // Delay produces an audible tail after the input stops.
        {
            chain.reset();

            halloweth::EffectParameters p;
            p.delayOn = true;
            p.delayTimeMs = 60.0f;
            p.delayFeedback = 0.6f;
            p.delayMix = 1.0f;

            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 1000.0, 0.5f);
            chain.process (audio, p);   // feed the delay

            double tail = 0.0;

            for (int i = 0; i < 6; ++i)
            {
                audio.clear();
                chain.process (audio, p);
                tail = juce::jmax (tail, rms (audio));
            }

            check (tail > 0.01, "delay produces a tail after the input stops");
        }

        // Reverb produces a tail after the input stops.
        {
            chain.reset();

            halloweth::EffectParameters p;
            p.reverbOn = true;
            p.reverbMix = 1.0f;
            p.reverbSize = 1.0f;
            p.reverbDamping = 0.2f;

            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 1000.0, 0.5f);

            for (int i = 0; i < 8; ++i)
                chain.process (audio, p);

            double tail = 0.0;

            for (int i = 0; i < 8; ++i)
            {
                audio.clear();
                chain.process (audio, p);
                tail = juce::jmax (tail, rms (audio));
            }

            check (tail > 0.001, "reverb produces a tail after the input stops");
        }

        // Compressor reduces peaks above the threshold.
        {
            chain.reset();
            juce::AudioBuffer<float> audio (2, blockSize);
            fillSine (audio, 1000.0, 0.8f);
            const auto before = audio.getMagnitude (0, 0, blockSize);

            halloweth::EffectParameters p;
            p.compOn = true;
            p.compThresholdDb = -30.0f;
            p.compRatio = 10.0f;
            p.compMakeupDb = 0.0f;

            for (int i = 0; i < 4; ++i)   // let the envelope settle
                chain.process (audio, p);

            const auto after = audio.getMagnitude (0, 0, blockSize);
            check (after < before * 0.6f, "compressor reduces peaks above the threshold");
        }

        // Width at 0 collapses to mono.
        {
            chain.reset();
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.getWritePointer (0)[0] = 0.5f;
            audio.getWritePointer (1)[0] = -0.5f;

            halloweth::EffectParameters p;
            p.widthOn = true;
            p.widthAmount = 0.0f;

            chain.process (audio, p);
            check (std::abs (audio.getReadPointer (0)[0] - audio.getReadPointer (1)[0]) < 1.0e-6f,
                   "stereo width at 0% collapses to mono");
        }

        // Width at 2x widens the side signal.
        {
            chain.reset();
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.getWritePointer (0)[0] = 0.5f;
            audio.getWritePointer (1)[0] = -0.5f;

            halloweth::EffectParameters p;
            p.widthOn = true;
            p.widthAmount = 2.0f;

            chain.process (audio, p);
            check (std::abs (audio.getReadPointer (0)[0] - 1.0f) < 1.0e-5f,
                   "stereo width at 200% widens the side signal");
        }
    }

    // ---------------------------------------------------------------- synth sound design
    {
        setChoice (halloweth::param::engineMode, 1);   // synth
        processor.prepareToPlay (sampleRate, blockSize);

        setParamReal (halloweth::param::attack, 0.0f);
        setParamReal (halloweth::param::decay, 120.0f);
        setParamReal (halloweth::param::sustain, 100.0f);
        setChoice (halloweth::param::synthWave, 0);    // saw
        setParamReal (halloweth::param::synthDetune, 0.0f);
        setParamReal (halloweth::param::synthMix, 0.0f);
        setParamReal (halloweth::param::synthSpread, 0.0f);
        setParamReal (halloweth::param::synthLfoDepth, 0.0f);
        setParamReal (halloweth::param::synthSub, 0.0f);

        // Filter envelope: the attack must be measurably brighter than the sustain.
        setParamReal (halloweth::param::synthCutoff, 300.0f);
        setParamReal (halloweth::param::synthFilterEnv, 100.0f);

        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, midi);

        const auto brightAttack = magnitudeAt (buffer.getReadPointer (0), blockSize, 1760.0, sampleRate);

        for (int i = 0; i < 25; ++i)
            renderBlock (processor, buffer, midi);

        const auto closedSustain = magnitudeAt (buffer.getReadPointer (0), blockSize, 1760.0, sampleRate);

        check (brightAttack > closedSustain * 3.0,
               "filter envelope opens the attack ("
                   + juce::String (brightAttack, 4) + " vs " + juce::String (closedSustain, 4) + ")");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);

        // Sub oscillator adds energy an octave below the note.
        setParamReal (halloweth::param::synthCutoff, 12000.0f);
        setParamReal (halloweth::param::synthFilterEnv, 0.0f);
        setParamReal (halloweth::param::synthSub, 0.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, midi);
        const auto noSub = magnitudeAt (buffer.getReadPointer (0), blockSize, 110.0, sampleRate);

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);

        setParamReal (halloweth::param::synthSub, 100.0f);
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, midi);
        const auto withSub = magnitudeAt (buffer.getReadPointer (0), blockSize, 110.0, sampleRate);

        check (withSub > noSub * 2.5,
               "sub oscillator adds content an octave below ("
                   + juce::String (noSub, 4) + " -> " + juce::String (withSub, 4) + ")");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);
        setParamReal (halloweth::param::synthSub, 0.0f);

        // Stereo spread: at 0% the channels are identical, at 100% they differ.
        setParamReal (halloweth::param::synthSpread, 0.0f);
        processor.prepareToPlay (sampleRate, blockSize);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, midi);

        auto monoDifference = 0.0f;

        for (int i = 0; i < blockSize; ++i)
            monoDifference = juce::jmax (monoDifference,
                                         std::abs (buffer.getReadPointer (0)[i]
                                                   - buffer.getReadPointer (1)[i]));

        check (monoDifference < 1.0e-6f, "spread at 0% keeps the signal centred");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);

        setParamReal (halloweth::param::synthMix, 100.0f);
        setParamReal (halloweth::param::synthDetune, 20.0f);
        setParamReal (halloweth::param::synthSpread, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        renderBlock (processor, buffer, midi);

        auto wideDifference = 0.0f;

        for (int i = 0; i < blockSize; ++i)
            wideDifference = juce::jmax (wideDifference,
                                         std::abs (buffer.getReadPointer (0)[i]
                                                   - buffer.getReadPointer (1)[i]));

        check (wideDifference > 0.01f, "spread at 100% creates stereo width");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        renderBlock (processor, buffer, midi);
    }

    processor.releaseResources();

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
