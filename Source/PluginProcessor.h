#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <array>
#include <memory>
#include <vector>

#include "AudioEngine/AudioEngine.h"
#include "Effects/EffectChain.h"
#include "Instruments/DrumKit.h"
#include "Instruments/InstrumentLoader.h"
#include "Parameters/ParameterIDs.h"
#include "Presets/PresetManager.h"

namespace halloweth
{
class SfzEngine;
class SfzInstrument;

/**
    HALLOWETH audio processor.

    Phases 1-5: plugin shell, sample engine, multisample maps, drum kit, synth
    engine, effect chain and a preset system. All file reading happens on worker
    threads; the audio callback never allocates or touches the filesystem.
*/
class HallowethAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    HallowethAudioProcessor();
    ~HallowethAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HALLOWETH"; }
    bool acceptsMidi() const override        { return true; }
    bool producesMidi() const override       { return false; }
    bool isMidiEffect() const override       { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                  { return 1; }
    int getCurrentProgram() override               { return 0; }
    void setCurrentProgram (int) override          {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Thread-safe readouts for the editor, polled on a timer. */
    int getActiveVoiceCount() const noexcept { return activeVoices.load (std::memory_order_relaxed); }
    float getOutputPeak() const noexcept     { return outputPeak.load (std::memory_order_relaxed); }

    //==============================================================================
    /** Presets - message thread only. */
    int getNumPresets() const noexcept { return presetManager.getNumPresets(); }
    const PresetInfo& getPresetInfo (int index) const { return presetManager.getPreset (index); }
    int getCurrentPresetIndex() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
    void applyPreset (int index);
    void selectNextPreset (int delta);
    bool isPresetFavourite (int index) const;
    void togglePresetFavourite (int index);
    void saveCurrentAsPreset (const juce::String& name, const juce::String& category,
                              const juce::String& genre, const juce::String& description);
    void deletePreset (int index);
    int findPresetByName (const juce::String& name) const noexcept;

    //==============================================================================
    /** Melodic instruments - message thread only. */
    void loadSampleFileAsync (const juce::File& file);   // single sample, full range
    void loadSampleMapAsync (const juce::File& mapFile); // JSON multisample map
    void loadSfzFileAsync (const juce::File& sfzFile);
    void clearSample();

    std::shared_ptr<const LoadedInstrument> getLoadedInstrument() const { return loadedInstrument; }
    juce::String getSampleStatusText() const { return sampleStatus; }
    juce::String getSampleErrorText() const  { return sampleError; }
    int getSampleRevision() const noexcept   { return sampleRevision.load (std::memory_order_relaxed); }
    bool isSampleLoading() const noexcept    { return pendingJob != nullptr; }
    juce::String getSfzFilePath() const      { return loadedSfzFile.getFullPathName(); }
    int getSfzNumRegions() const noexcept;

    //==============================================================================
    /** Drum kit - message thread only. */
    void loadDrumKitAsync (const juce::File& kitFile);
    void loadDrumPadSampleAsync (int padIndex, const juce::File& sampleFile);
    void clearDrumPad (int padIndex);
    bool saveDrumKit (const juce::File& kitFile) const;
    void setPadAdjustment (int padIndex, float gainDb, float pan, int semitones);

    const DrumKit& getDrumKit() const noexcept { return drumKit; }
    int getDrumKitRevision() const noexcept   { return drumKitRevision.load (std::memory_order_relaxed); }
    juce::String getDrumStatusText() const    { return drumStatus; }
    juce::String getDrumErrorText() const     { return drumError; }

    /** Publishes a finished background load. Runs on the message thread; the editor
        timer calls this automatically, tests call it directly. */
    void pollSampleLoad();

    //==============================================================================
    AudioEngine audioEngine;
    std::unique_ptr<SfzEngine> sfzEngine;
    PresetManager presetManager;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    struct InstrumentLoadJob
    {
        enum class Kind { singleSample, map, sfz, drumKit, drumPad, drumKitJson };

        Kind kind = Kind::singleSample;
        juce::File file;
        juce::String jsonText;
        int padIndex = -1;
        double sampleRate = 44100.0;
        int maximumBlockSize = 512;
        DrumKit kit;
        bool activateDrums = false;

        std::atomic<bool> finished { false };
        std::shared_ptr<const LoadedInstrument> result;
        std::shared_ptr<SfzInstrument> sfzResult;
        juce::String error;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;
    void startLoad (std::shared_ptr<InstrumentLoadJob> job);
    void publishDrumKit (const DrumKit& kit, std::shared_ptr<const LoadedInstrument> instrument,
                         const juce::String& error);
    void applyAllPadAdjustments();
    void loadInstrumentForPreset (const PresetLoadResult& result);
    void updateToneFilter();

    std::atomic<float>* masterGainParam = nullptr;
    std::atomic<float>* toneParam       = nullptr;
    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* decayParam      = nullptr;
    std::atomic<float>* sustainParam    = nullptr;
    std::atomic<float>* releaseParam    = nullptr;
    std::atomic<float>* loopParam       = nullptr;
    std::atomic<float>* engineModeParam = nullptr;
    std::atomic<float>* synthWaveParam  = nullptr;
    std::atomic<float>* synthDetuneParam = nullptr;
    std::atomic<float>* synthMixParam   = nullptr;
    std::atomic<float>* synthCutoffParam = nullptr;
    std::atomic<float>* synthResParam   = nullptr;
    std::atomic<float>* synthSubParam   = nullptr;
    std::atomic<float>* synthSpreadParam = nullptr;
    std::atomic<float>* synthFilterEnvParam = nullptr;
    std::atomic<float>* synthLfoDepthParam = nullptr;

    juce::IIRFilter toneFilters[2];
    float currentToneHz = -1.0f;

    static constexpr int numEffectParams = 25;
    EffectChain effects;
    std::array<std::atomic<float>*, (size_t) numEffectParams> fxParams {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGainSmoothed { 1.0f };

    std::atomic<int> activeVoices { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<int> currentPreset { 0 };

    // Loading state (message thread owns these; the worker only touches its job).
    std::shared_ptr<InstrumentLoadJob> pendingJob;
    std::shared_ptr<const LoadedInstrument> loadedInstrument;
    juce::File loadedSfzFile;

    juce::String sampleStatus { "No instrument loaded" };
    juce::String sampleError;
    std::atomic<int> sampleRevision { 0 };

    DrumKit drumKit;
    juce::String drumStatus { "No kit loaded" };
    juce::String drumError;
    bool hasDrumInstrument = false;
    std::atomic<int> drumKitRevision { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallowethAudioProcessor)
};
}
