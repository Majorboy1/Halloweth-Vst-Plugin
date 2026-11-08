#include "PluginProcessor.h"

#include "AudioEngine/SampleLoader.h"
#include "AudioEngine/SfzInstrument.h"
#include "PluginEditor.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace halloweth
{
namespace
{
    // Order must match the parameter array filled in the constructor.
    enum : int
    {
        FxReverbOn, FxReverbMix, FxReverbSize, FxReverbDamp,
        FxDelayOn, FxDelayTime, FxDelayFeedback, FxDelayMix,
        FxChorusOn, FxChorusRate, FxChorusDepth, FxChorusMix,
        FxFilterOn, FxFilterCutoff, FxFilterRes, FxFilterMode,
        FxDriveOn, FxDriveAmount, FxDriveTone,
        FxCompOn, FxCompThreshold, FxCompRatio, FxCompMakeup,
        FxWidthOn, FxWidthAmount
    };

    const char* const effectParameterIds[]
    {
        param::fxReverbOn, param::fxReverbMix, param::fxReverbSize, param::fxReverbDamp,
        param::fxDelayOn, param::fxDelayTime, param::fxDelayFeedback, param::fxDelayMix,
        param::fxChorusOn, param::fxChorusRate, param::fxChorusDepth, param::fxChorusMix,
        param::fxFilterOn, param::fxFilterCutoff, param::fxFilterRes, param::fxFilterMode,
        param::fxDriveOn, param::fxDriveAmount, param::fxDriveTone,
        param::fxCompOn, param::fxCompThreshold, param::fxCompRatio, param::fxCompMakeup,
        param::fxWidthOn, param::fxWidthAmount
    };

    juce::File getPluginModuleFile()
    {
       #if JUCE_WINDOWS
        HMODULE module = nullptr;
        const auto address = reinterpret_cast<LPCWSTR> (&getPluginModuleFile);

        if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                address, &module))
        {
            wchar_t path[32768] {};
            const auto length = GetModuleFileNameW (module, path, (DWORD) std::size (path));

            if (length > 0 && length < std::size (path))
                return juce::File (juce::String (path, (size_t) length));
        }
       #endif

        return juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    }

    juce::File resolveBundledInstrument (const juce::String& instrumentPath)
    {
        constexpr auto prefix = "@HALLOWETH_RESOURCE:";

        if (! instrumentPath.startsWith (prefix))
            return juce::File (instrumentPath);

        const auto relativePath = instrumentPath.substring ((int) std::char_traits<char>::length (prefix));
        const auto moduleFolder = getPluginModuleFile().getParentDirectory();
        const juce::File candidates[]
        {
            moduleFolder.getChildFile ("Resources").getChildFile (relativePath),
            moduleFolder.getParentDirectory().getChildFile ("Resources").getChildFile (relativePath)
        };

        for (const auto& candidate : candidates)
            if (candidate.existsAsFile())
                return candidate;

        return candidates[0];
    }

    juce::String getPortableInstrumentPath (const juce::File& instrumentFile)
    {
        const auto moduleFolder = getPluginModuleFile().getParentDirectory();
        const juce::File resourceFolders[]
        {
            moduleFolder.getChildFile ("Resources"),
            moduleFolder.getParentDirectory().getChildFile ("Resources")
        };

        for (const auto& resourceFolder : resourceFolders)
            if (instrumentFile.isAChildOf (resourceFolder))
                return "@HALLOWETH_RESOURCE:" + instrumentFile.getRelativePathFrom (resourceFolder);

        return instrumentFile.getFullPathName();
    }
}

HallowethAudioProcessor::HallowethAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HALLOWETH_STATE", createParameterLayout())
{
    sfzEngine = std::make_unique<SfzEngine>();

    masterGainParam  = apvts.getRawParameterValue (param::masterGain);
    toneParam        = apvts.getRawParameterValue (param::tone);
    attackParam      = apvts.getRawParameterValue (param::attack);
    decayParam       = apvts.getRawParameterValue (param::decay);
    sustainParam     = apvts.getRawParameterValue (param::sustain);
    releaseParam     = apvts.getRawParameterValue (param::release);
    loopParam        = apvts.getRawParameterValue (param::loopEnabled);
    engineModeParam  = apvts.getRawParameterValue (param::engineMode);
    synthWaveParam   = apvts.getRawParameterValue (param::synthWave);
    synthDetuneParam = apvts.getRawParameterValue (param::synthDetune);
    synthMixParam    = apvts.getRawParameterValue (param::synthMix);
    synthCutoffParam = apvts.getRawParameterValue (param::synthCutoff);
    synthResParam    = apvts.getRawParameterValue (param::synthResonance);
    synthSubParam    = apvts.getRawParameterValue (param::synthSub);
    synthSpreadParam = apvts.getRawParameterValue (param::synthSpread);
    synthFilterEnvParam = apvts.getRawParameterValue (param::synthFilterEnv);
    synthLfoDepthParam  = apvts.getRawParameterValue (param::synthLfoDepth);

    applyAllPadAdjustments();

    for (size_t i = 0; i < fxParams.size(); ++i)
        fxParams[i] = apvts.getRawParameterValue (effectParameterIds[i]);

    startTimer (100);
}

HallowethAudioProcessor::~HallowethAudioProcessor()
{
    stopTimer();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HallowethAudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const String& name, NormalisableRange<float> range,
                               float defaultValue, const String& label)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 },
            name,
            range,
            defaultValue,
            AudioParameterFloatAttributes().withLabel (label)));
    };

    addFloat (param::masterGain, "Master",
              NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -6.0f, "dB");

    addFloat (param::tone, "Tone",
              NormalisableRange<float> (400.0f, 18000.0f, 1.0f, 0.3f), 12000.0f, "Hz");

    addFloat (param::attack, "Attack",
              NormalisableRange<float> (0.0f, 2000.0f, 0.5f, 0.4f), 8.0f, "ms");

    addFloat (param::decay, "Decay",
              NormalisableRange<float> (0.0f, 3000.0f, 1.0f, 0.4f), 180.0f, "ms");

    addFloat (param::sustain, "Sustain",
              NormalisableRange<float> (0.0f, 100.0f, 0.5f), 80.0f, "%");

    addFloat (param::release, "Release",
              NormalisableRange<float> (10.0f, 4000.0f, 1.0f, 0.4f), 260.0f, "ms");

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { param::loopEnabled, 1 }, "Loop", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { param::engineMode, 1 }, "Engine",
        StringArray { "Sample Map", "Synth Engine", "Drums", "SFZ" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { param::synthWave, 1 }, "Waveform",
        StringArray { "Saw", "Square", "Sine", "Triangle" }, 0));

    addFloat (param::synthDetune, "Detune",
              NormalisableRange<float> (0.0f, 50.0f, 0.1f), 10.0f, "ct");

    addFloat (param::synthMix, "Osc Mix",
              NormalisableRange<float> (0.0f, 100.0f, 0.5f), 40.0f, "%");

    addFloat (param::synthCutoff, "Cutoff",
              NormalisableRange<float> (60.0f, 18000.0f, 1.0f, 0.3f), 4500.0f, "Hz");

    addFloat (param::synthResonance, "Resonance",
              NormalisableRange<float> (0.6f, 12.0f, 0.01f, 0.45f), 1.2f, "Q");

    addFloat (param::synthSub, "Sub",
              NormalisableRange<float> (0.0f, 100.0f, 0.5f), 18.0f, "%");

    addFloat (param::synthSpread, "Spread",
              NormalisableRange<float> (0.0f, 100.0f, 0.5f), 45.0f, "%");

    addFloat (param::synthFilterEnv, "Filter Env",
              NormalisableRange<float> (0.0f, 100.0f, 0.5f), 60.0f, "%");

    addFloat (param::synthLfoDepth, "LFO Depth",
              NormalisableRange<float> (0.0f, 100.0f, 0.5f), 6.0f, "%");

    // ---- effects ---------------------------------------------------------
    auto addBool = [&layout] (const char* id, const String& name, bool defaultValue)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, defaultValue));
    };

    addBool (param::fxReverbOn, "Reverb On", false);
    addFloat (param::fxReverbMix, "Reverb Mix", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 25.0f, "%");
    addFloat (param::fxReverbSize, "Reverb Size", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f, "%");
    addFloat (param::fxReverbDamp, "Reverb Damp", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f, "%");

    addBool (param::fxDelayOn, "Delay On", false);
    addFloat (param::fxDelayTime, "Delay Time", NormalisableRange<float> (20.0f, 1500.0f, 1.0f, 0.4f), 350.0f, "ms");
    addFloat (param::fxDelayFeedback, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f, 0.5f), 35.0f, "%");
    addFloat (param::fxDelayMix, "Delay Mix", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 25.0f, "%");

    addBool (param::fxChorusOn, "Chorus On", false);
    addFloat (param::fxChorusRate, "Chorus Rate", NormalisableRange<float> (0.05f, 8.0f, 0.01f, 0.5f), 0.8f, "Hz");
    addFloat (param::fxChorusDepth, "Chorus Depth", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 40.0f, "%");
    addFloat (param::fxChorusMix, "Chorus Mix", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 30.0f, "%");

    addBool (param::fxFilterOn, "Filter On", false);
    addFloat (param::fxFilterCutoff, "Filter Cutoff", NormalisableRange<float> (30.0f, 18000.0f, 1.0f, 0.3f), 12000.0f, "Hz");
    addFloat (param::fxFilterRes, "Filter Res", NormalisableRange<float> (0.5f, 12.0f, 0.01f, 0.45f), 0.707f, "Q");
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { param::fxFilterMode, 1 }, "Filter Mode",
        StringArray { "Low Pass", "High Pass" }, 0));

    addBool (param::fxDriveOn, "Drive On", false);
    addFloat (param::fxDriveAmount, "Drive Amount", NormalisableRange<float> (1.0f, 20.0f, 0.1f), 2.0f, "x");
    addFloat (param::fxDriveTone, "Drive Tone", NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f, "%");

    addBool (param::fxCompOn, "Compressor On", false);
    addFloat (param::fxCompThreshold, "Comp Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f, "dB");
    addFloat (param::fxCompRatio, "Comp Ratio", NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f), 3.0f, ":1");
    addFloat (param::fxCompMakeup, "Comp Makeup", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, "dB");

    addBool (param::fxWidthOn, "Width On", false);
    addFloat (param::fxWidthAmount, "Width", NormalisableRange<float> (0.0f, 200.0f, 0.5f), 100.0f, "%");

    return layout;
}

//==============================================================================
void HallowethAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    audioEngine.prepare (sampleRate, samplesPerBlock);
    sfzEngine->prepare (sampleRate, samplesPerBlock);
    effects.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();

    for (auto& filter : toneFilters)
        filter.reset();

    currentToneHz = -1.0f;
    updateToneFilter();

    masterGainSmoothed.reset (sampleRate, 0.02);
    masterGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (masterGainParam->load()));

    outputPeak.store (0.0f, std::memory_order_relaxed);
    activeVoices.store (0, std::memory_order_relaxed);
}

void HallowethAudioProcessor::releaseResources()
{
    audioEngine.reset();
    sfzEngine->allSoundsOff();
    effects.reset();
    activeVoices.store (0, std::memory_order_relaxed);
}

bool HallowethAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainOutput != juce::AudioChannelSet::stereo()
        && mainOutput != juce::AudioChannelSet::mono())
        return false;

    return layouts.getMainInputChannelSet().isDisabled()
           || layouts.getMainInputChannelSet() == mainOutput;
}

void HallowethAudioProcessor::updateToneFilter()
{
    if (toneParam == nullptr || getSampleRate() <= 0.0)
        return;

    const auto cutoff = juce::jlimit (100.0f, (float) getSampleRate() * 0.45f, toneParam->load());
    const auto coefficients = juce::IIRCoefficients::makeLowPass (getSampleRate(), (double) cutoff);

    for (auto& filter : toneFilters)
        filter.setCoefficients (coefficients);

    currentToneHz = cutoff;
}

void HallowethAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    audioEngine.setEnvelopeParameters ({ attackParam->load() * 0.001f,
                                         decayParam->load() * 0.001f,
                                         sustainParam->load() * 0.01f,
                                         releaseParam->load() * 0.001f });
    audioEngine.setLoopEnabled (loopParam->load() > 0.5f);
    const auto selectedEngineMode = juce::roundToInt (engineModeParam->load());
    const auto sfzReady = selectedEngineMode == (int) EngineMode::sfz && sfzEngine->hasInstrument();
    audioEngine.setEngineMode (selectedEngineMode == (int) EngineMode::sfz && ! sfzReady
                                   ? (int) EngineMode::synth
                                   : selectedEngineMode);

    {
        SynthSettings synth;
        synth.waveform = juce::roundToInt (synthWaveParam->load());
        synth.detuneCents = synthDetuneParam->load();
        synth.mix = synthMixParam->load() * 0.01f;
        synth.cutoffHz = synthCutoffParam->load();
        synth.resonance = synthResParam->load();
        synth.subLevel = synthSubParam->load() * 0.01f;
        synth.spread = synthSpreadParam->load() * 0.01f;
        synth.filterEnvAmount = synthFilterEnvParam->load() * 0.01f;
        synth.lfoDepth = synthLfoDepthParam->load() * 0.01f;
        audioEngine.setSynthSettings (synth);
    }

    if (sfzReady)
        sfzEngine->render (buffer, midiMessages);
    else
        audioEngine.render (buffer, midiMessages);

    const auto requestedTone = toneParam->load();

    if (std::abs (requestedTone - currentToneHz) > 1.0f)
        updateToneFilter();

    for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), 2); ++ch)
        toneFilters[ch].processSamples (buffer.getWritePointer (ch), numSamples);

    // Effects chain (all controls are real; bypassed effects are skipped entirely).
    {
        auto read = [this] (int index) { return fxParams[(size_t) index]->load(); };

        EffectParameters fx;
        fx.reverbOn      = read (FxReverbOn) > 0.5f;
        fx.reverbMix     = read (FxReverbMix) * 0.01f;
        fx.reverbSize    = read (FxReverbSize) * 0.01f;
        fx.reverbDamping = read (FxReverbDamp) * 0.01f;

        fx.delayOn       = read (FxDelayOn) > 0.5f;
        fx.delayTimeMs   = read (FxDelayTime);
        fx.delayFeedback = read (FxDelayFeedback) * 0.01f;
        fx.delayMix      = read (FxDelayMix) * 0.01f;

        fx.chorusOn      = read (FxChorusOn) > 0.5f;
        fx.chorusRate    = read (FxChorusRate);
        fx.chorusDepth   = read (FxChorusDepth) * 0.01f;
        fx.chorusMix     = read (FxChorusMix) * 0.01f;

        fx.filterOn      = read (FxFilterOn) > 0.5f;
        fx.filterCutoff  = read (FxFilterCutoff);
        fx.filterResonance = read (FxFilterRes);
        fx.filterMode    = juce::roundToInt (read (FxFilterMode));

        fx.driveOn       = read (FxDriveOn) > 0.5f;
        fx.driveAmount   = read (FxDriveAmount);
        fx.driveTone     = read (FxDriveTone) * 0.01f;

        fx.compOn          = read (FxCompOn) > 0.5f;
        fx.compThresholdDb = read (FxCompThreshold);
        fx.compRatio       = read (FxCompRatio);
        fx.compMakeupDb    = read (FxCompMakeup);

        fx.widthOn     = read (FxWidthOn) > 0.5f;
        fx.widthAmount = read (FxWidthAmount) * 0.01f;

        effects.process (buffer, fx);
    }

    masterGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (masterGainParam->load()));

    const auto numChannels = juce::jmin (buffer.getNumChannels(), 2);
    float* channelPointers[2] { nullptr, nullptr };

    for (int ch = 0; ch < numChannels; ++ch)
        channelPointers[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = masterGainSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[ch][i] *= gain;
    }

    activeVoices.store (sfzReady ? sfzEngine->getActiveVoiceCount()
                                 : audioEngine.getActiveVoiceCount(),
                        std::memory_order_relaxed);

    float peak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    outputPeak.store (peak, std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* HallowethAudioProcessor::createEditor()
{
    return new HallowethAudioProcessorEditor (*this);
}

//==============================================================================
int HallowethAudioProcessor::findPresetByName (const juce::String& name) const noexcept
{
    for (int i = 0; i < presetManager.getNumPresets(); ++i)
        if (presetManager.getPreset (i).name == name)
            return i;

    return -1;
}

int HallowethAudioProcessor::getSfzNumRegions() const noexcept
{
    return sfzEngine != nullptr ? sfzEngine->getNumRegions() : 0;
}

void HallowethAudioProcessor::loadInstrumentForPreset (const PresetLoadResult& result)
{
    if (result.instrumentPath.isEmpty())
        return;

    if (result.instrumentIsSfz)
    {
        loadSfzFileAsync (resolveBundledInstrument (result.instrumentPath));
        return;
    }

    const auto file = resolveBundledInstrument (result.instrumentPath);

    if (! file.existsAsFile())
    {
        sampleStatus = "Preset instrument missing";
        sampleError = result.instrumentPath;
        sampleRevision.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    if (loadedInstrument != nullptr && loadedInstrument->sourcePath == result.instrumentPath)
        return;

    if (result.instrumentIsMap)
        loadSampleMapAsync (file);
    else
        loadSampleFileAsync (file);
}

void HallowethAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, presetManager.getNumPresets()))
        return;

    const auto result = presetManager.applyPreset (index, apvts);
    loadInstrumentForPreset (result);
    currentPreset.store (index, std::memory_order_relaxed);
}

void HallowethAudioProcessor::selectNextPreset (int delta)
{
    const auto count = presetManager.getNumPresets();

    if (count == 0)
        return;

    const auto next = (getCurrentPresetIndex() + delta % count + count) % count;
    applyPreset (next);
}

bool HallowethAudioProcessor::isPresetFavourite (int index) const
{
    if (! juce::isPositiveAndBelow (index, presetManager.getNumPresets()))
        return false;

    return presetManager.isFavourite (presetManager.getPreset (index).name);
}

void HallowethAudioProcessor::togglePresetFavourite (int index)
{
    if (! juce::isPositiveAndBelow (index, presetManager.getNumPresets()))
        return;

    presetManager.toggleFavourite (presetManager.getPreset (index).name);
}

void HallowethAudioProcessor::saveCurrentAsPreset (const juce::String& name,
                                                  const juce::String& category,
                                                  const juce::String& genre,
                                                  const juce::String& description)
{
    const auto isSfz = loadedSfzFile.existsAsFile();
    const auto instrumentPath = isSfz ? loadedSfzFile.getFullPathName()
                                     : loadedInstrument != nullptr ? loadedInstrument->sourcePath
                                                                   : juce::String();
    const auto isMap = instrumentPath.endsWithIgnoreCase (".json");

    if (presetManager.saveUserPreset (name, category, genre, description, false, apvts,
                                      instrumentPath, isMap, isSfz))
    {
        const auto index = findPresetByName (name.trim());

        if (index >= 0)
            currentPreset.store (index, std::memory_order_relaxed);
    }
}

void HallowethAudioProcessor::deletePreset (int index)
{
    if (presetManager.deleteUserPreset (index))
    {
        if (getCurrentPresetIndex() >= presetManager.getNumPresets())
            currentPreset.store (juce::jmax (0, presetManager.getNumPresets() - 1),
                                 std::memory_order_relaxed);
    }
}

//==============================================================================
void HallowethAudioProcessor::startLoad (std::shared_ptr<InstrumentLoadJob> job)
{
    pendingJob = job;
    sampleStatus = "Loading " + job->file.getFileName() + "...";
    sampleError.clear();
    sampleRevision.fetch_add (1, std::memory_order_relaxed);

    juce::Thread::launch ([job]
    {
        juce::String error;
        std::shared_ptr<const LoadedInstrument> instrument;

        switch (job->kind)
        {
            case InstrumentLoadJob::Kind::singleSample:
            {
                auto sample = SampleLoader::loadFromFile (job->file, error);
                instrument = InstrumentLoader::fromSingleSample (std::move (sample), 60);
                break;
            }

            case InstrumentLoadJob::Kind::map:
                instrument = InstrumentLoader::loadMapFile (job->file, error);
                break;

            case InstrumentLoadJob::Kind::sfz:
            {
                auto sfzInstrument = SfzInstrument::loadFromFile (job->file, job->sampleRate,
                                                                  job->maximumBlockSize, error);

                if (sfzInstrument != nullptr)
                    job->sfzResult = std::shared_ptr<SfzInstrument> (std::move (sfzInstrument));

                break;
            }

            case InstrumentLoadJob::Kind::drumKit:
            {
                auto kit = DrumKit::fromJson (job->file.loadFileAsString(), error);

                if (kit.getNumLoadedPads() > 0)
                {
                    const auto folder = job->file.getParentDirectory();

                    for (auto& pad : kit.pads)
                        if (! pad.isEmpty())
                        {
                            const juce::File padFile (pad.file);
                            pad.file = (juce::File::isAbsolutePath (pad.file)
                                            ? padFile
                                            : folder.getChildFile (pad.file)).getFullPathName();
                        }

                    job->kit = kit;
                    instrument = InstrumentLoader::loadDrumKit (kit, job->file.getParentDirectory(), error);
                }

                break;
            }

            case InstrumentLoadJob::Kind::drumKitJson:
            {
                juce::String parseError;
                job->kit = DrumKit::fromJson (job->jsonText, parseError);

                if (job->kit.getNumLoadedPads() > 0)
                    instrument = InstrumentLoader::loadDrumKit (job->kit, {}, error);
                else
                    error = parseError;

                break;
            }

            case InstrumentLoadJob::Kind::drumPad:
                instrument = InstrumentLoader::loadDrumKit (job->kit, {}, error);
                break;
        }

        job->error = error;
        job->result = std::move (instrument);
        job->finished.store (true, std::memory_order_release);
    });
}

void HallowethAudioProcessor::loadSampleFileAsync (const juce::File& file)
{
    auto job = std::make_shared<InstrumentLoadJob>();
    job->kind = InstrumentLoadJob::Kind::singleSample;
    job->file = file;
    startLoad (job);
}

void HallowethAudioProcessor::loadSampleMapAsync (const juce::File& mapFile)
{
    auto job = std::make_shared<InstrumentLoadJob>();
    job->kind = InstrumentLoadJob::Kind::map;
    job->file = mapFile;
    startLoad (job);
}

void HallowethAudioProcessor::loadSfzFileAsync (const juce::File& sfzFile)
{
    auto job = std::make_shared<InstrumentLoadJob>();
    job->kind = InstrumentLoadJob::Kind::sfz;
    job->file = sfzFile;
    job->sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    job->maximumBlockSize = juce::jmax (512, getBlockSize());
    startLoad (job);
}

void HallowethAudioProcessor::loadDrumKitAsync (const juce::File& kitFile)
{
    auto job = std::make_shared<InstrumentLoadJob>();
    job->kind = InstrumentLoadJob::Kind::drumKit;
    job->file = kitFile;
    job->activateDrums = true;
    startLoad (job);
}

void HallowethAudioProcessor::loadDrumPadSampleAsync (int padIndex, const juce::File& sampleFile)
{
    if (! juce::isPositiveAndBelow (padIndex, DrumKit::numPads))
        return;

    auto job = std::make_shared<InstrumentLoadJob>();
    job->kind = InstrumentLoadJob::Kind::drumPad;
    job->file = sampleFile;
    job->padIndex = padIndex;
    job->activateDrums = true;

    auto kit = drumKit;
    kit.pads[(size_t) padIndex].file = sampleFile.getFullPathName();
    kit.pads[(size_t) padIndex].name = sampleFile.getFileNameWithoutExtension();
    job->kit = kit;

    startLoad (job);
}

void HallowethAudioProcessor::clearDrumPad (int padIndex)
{
    if (! juce::isPositiveAndBelow (padIndex, DrumKit::numPads))
        return;

    auto kit = drumKit;
    kit.pads[(size_t) padIndex] = DrumPad();
    drumKit = kit;

    if (kit.isEmpty())
    {
        audioEngine.clearDrumInstrument();
        hasDrumInstrument = false;
        drumStatus = "No kit loaded";
    }
    else
    {
        auto job = std::make_shared<InstrumentLoadJob>();
        job->kind = InstrumentLoadJob::Kind::drumKitJson;
        job->jsonText = kit.toJson();
        startLoad (job);
    }

    drumError.clear();
    drumKitRevision.fetch_add (1, std::memory_order_relaxed);
}

bool HallowethAudioProcessor::saveDrumKit (const juce::File& kitFile) const
{
    if (drumKit.isEmpty())
        return false;

    return kitFile.replaceWithText (drumKit.toJson());
}

void HallowethAudioProcessor::setPadAdjustment (int padIndex, float gainDb, float pan, int semitones)
{
    if (! juce::isPositiveAndBelow (padIndex, DrumKit::numPads))
        return;

    auto& pad = drumKit.pads[(size_t) padIndex];
    pad.gainDb = juce::jlimit (-48.0f, 12.0f, gainDb);
    pad.pan = juce::jlimit (-1.0f, 1.0f, pan);
    pad.pitchSemitones = juce::jlimit (-24, 24, semitones);

    audioEngine.setPadAdjustment (padIndex, pad.gainDb, pad.pan, pad.pitchSemitones);
    drumKitRevision.fetch_add (1, std::memory_order_relaxed);
}

void HallowethAudioProcessor::applyAllPadAdjustments()
{
    for (int i = 0; i < DrumKit::numPads; ++i)
    {
        const auto& pad = drumKit.pads[(size_t) i];
        audioEngine.setPadAdjustment (i, pad.gainDb, pad.pan, pad.pitchSemitones);
    }
}

void HallowethAudioProcessor::clearSample()
{
    audioEngine.clearInstrument();
    sfzEngine->clear();
    loadedInstrument.reset();
    loadedSfzFile = {};
    pendingJob.reset();

    sampleStatus = "No instrument loaded";
    sampleError.clear();
    sampleRevision.fetch_add (1, std::memory_order_relaxed);
}

void HallowethAudioProcessor::timerCallback()
{
    pollSampleLoad();
}

void HallowethAudioProcessor::publishDrumKit (const DrumKit& kit,
                                             std::shared_ptr<const LoadedInstrument> instrument,
                                             const juce::String& error)
{
    drumKit = kit;
    applyAllPadAdjustments();

    if (instrument != nullptr)
    {
        audioEngine.setDrumInstrument (std::move (instrument));
        hasDrumInstrument = true;
        drumStatus = "Kit: " + kit.name + " (" + juce::String (kit.getNumLoadedPads()) + " pads)";
        drumError = error;
    }
    else
    {
        drumError = error.isNotEmpty() ? error : juce::String ("Kit could not be loaded");
        drumStatus = "Kit load failed";
    }

    if (auto* modeParameter = apvts.getParameter (param::engineMode))
        modeParameter->setValueNotifyingHost (
            modeParameter->convertTo0to1 ((float) EngineMode::drums));

    drumKitRevision.fetch_add (1, std::memory_order_relaxed);
}

void HallowethAudioProcessor::pollSampleLoad()
{
    const auto job = pendingJob;

    if (job == nullptr || ! job->finished.load (std::memory_order_acquire))
        return;

    pendingJob.reset();

    const auto isDrumJob = job->kind == InstrumentLoadJob::Kind::drumKit
                           || job->kind == InstrumentLoadJob::Kind::drumPad
                           || job->kind == InstrumentLoadJob::Kind::drumKitJson;

    if (isDrumJob)
    {
        publishDrumKit (job->kit, job->result, job->error);
        return;
    }

    if (job->kind == InstrumentLoadJob::Kind::sfz)
    {
        if (job->sfzResult != nullptr)
        {
            audioEngine.clearInstrument();
            loadedInstrument.reset();
            loadedSfzFile = job->file;
            sfzEngine->install (std::move (job->sfzResult));
            sampleStatus = "Loaded SFZ (" + juce::String (sfzEngine->getNumRegions()) + " regions)";
            sampleError.clear();

            if (auto* modeParameter = apvts.getParameter (param::engineMode))
                modeParameter->setValueNotifyingHost (
                    modeParameter->convertTo0to1 ((float) EngineMode::sfz));
        }
        else
        {
            sampleStatus = "SFZ load failed";
            sampleError = job->error.isNotEmpty() ? job->error : juce::String ("Unknown SFZ error");
        }

        sampleRevision.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    if (job->result != nullptr)
    {
        sfzEngine->clear();
        loadedSfzFile = {};
        loadedInstrument = job->result;
        audioEngine.setInstrument (job->result);

        sampleStatus = "Loaded " + job->result->name + " ("
                       + juce::String (job->result->getNumPlayableZones()) + " zones)";
        sampleError = job->result->missingFiles.isEmpty()
                          ? juce::String()
                          : juce::String (job->result->missingFiles.size())
                                + " zone file(s) missing: "
                                + job->result->missingFiles.joinIntoString ("; ");

        if (auto* modeParameter = apvts.getParameter (param::engineMode))
            modeParameter->setValueNotifyingHost (0.0f);
    }
    else
    {
        sampleStatus = "Load failed";
        sampleError = job->error.isNotEmpty() ? job->error : juce::String ("Unknown error");
    }

    sampleRevision.fetch_add (1, std::memory_order_relaxed);
}

//==============================================================================
void HallowethAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (loadedInstrument != nullptr)
    {
        state.setProperty ("instrumentPath", loadedInstrument->sourcePath, nullptr);
        state.setProperty ("instrumentIsMap",
                           loadedInstrument->sourcePath.endsWithIgnoreCase (".json"), nullptr);
    }
    else
    {
        state.removeProperty ("instrumentPath", nullptr);
        state.removeProperty ("instrumentIsMap", nullptr);
    }

    if (loadedSfzFile.existsAsFile())
        state.setProperty ("sfzPath", getPortableInstrumentPath (loadedSfzFile), nullptr);
    else
        state.removeProperty ("sfzPath", nullptr);

    if (! drumKit.isEmpty())
        state.setProperty ("drumKit", drumKit.toJson(), nullptr);

    const auto presetName = getPresetInfo (getCurrentPresetIndex()).name;
    state.setProperty ("presetName", presetName, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HallowethAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || xml->getTagName() != apvts.state.getType().toString())
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    apvts.replaceState (tree);

    const auto presetName = tree.getProperty ("presetName", juce::var()).toString();
    const auto presetIndex = findPresetByName (presetName);
    currentPreset.store (presetIndex >= 0 ? presetIndex : 0, std::memory_order_relaxed);

    const auto instrumentPath = tree.getProperty ("instrumentPath", juce::var()).toString();
    const auto isMap = (bool) tree.getProperty ("instrumentIsMap", false);
    const auto sfzPath = tree.getProperty ("sfzPath", juce::var()).toString();

    if (sfzPath.isNotEmpty())
    {
        const auto file = resolveBundledInstrument (sfzPath);

        if (file.existsAsFile())
            loadSfzFileAsync (file);
        else
        {
            sampleStatus = "Missing SFZ instrument";
            sampleError = sfzPath + " was not found";
            sampleRevision.fetch_add (1, std::memory_order_relaxed);
        }
    }
    else if (instrumentPath.isEmpty())
    {
        clearSample();
    }
    else if (loadedInstrument == nullptr || loadedInstrument->sourcePath != instrumentPath)
    {
        const juce::File file (instrumentPath);

        if (file.existsAsFile())
        {
            if (isMap)
                loadSampleMapAsync (file);
            else
                loadSampleFileAsync (file);
        }
        else
        {
            sampleStatus = "Missing instrument";
            sampleError = instrumentPath + " was not found";
            sampleRevision.fetch_add (1, std::memory_order_relaxed);
        }
    }

    const auto kitJson = tree.getProperty ("drumKit", juce::var()).toString();

    if (kitJson.isNotEmpty())
    {
        auto job = std::make_shared<InstrumentLoadJob>();
        job->kind = InstrumentLoadJob::Kind::drumKitJson;
        job->jsonText = kitJson;
        startLoad (job);
    }
}
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new halloweth::HallowethAudioProcessor();
}
