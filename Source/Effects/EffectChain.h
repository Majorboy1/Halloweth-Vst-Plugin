#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

namespace halloweth
{
/** Snapshot of the effect settings, published once per block. */
struct EffectParameters
{
    bool reverbOn = false;
    float reverbMix = 0.25f, reverbSize = 0.5f, reverbDamping = 0.5f;

    bool delayOn = false;
    float delayTimeMs = 350.0f, delayFeedback = 0.35f, delayMix = 0.25f;

    bool chorusOn = false;
    float chorusRate = 0.8f, chorusDepth = 0.4f, chorusMix = 0.30f;

    bool filterOn = false;
    float filterCutoff = 18000.0f, filterResonance = 0.707f;
    int filterMode = 0;   // 0 = low pass, 1 = high pass

    bool driveOn = false;
    float driveAmount = 2.0f, driveTone = 0.5f;

    bool compOn = false;
    float compThresholdDb = -18.0f, compRatio = 3.0f, compMakeupDb = 0.0f;

    bool widthOn = false;
    float widthAmount = 1.0f;

    bool anyActive() const noexcept
    {
        return reverbOn || delayOn || chorusOn || filterOn || driveOn || compOn || widthOn;
    }
};

/**
    Serial stereo effect chain: filter → drive → chorus → delay → reverb →
    compressor → stereo width.

    All buffers are allocated in prepare(); process() performs no allocation and
    only touches its own state, so it is safe on the audio thread.
*/
class EffectChain
{
public:
    EffectChain() = default;

    void prepare (double newSampleRate, int maxBlockSize);
    void reset();

    void process (juce::AudioBuffer<float>& buffer, const EffectParameters& parameters);

private:
    void processFilter (float* left, float* right, int numSamples, const EffectParameters&);
    void processDrive (float* left, float* right, int numSamples, const EffectParameters&);
    void processChorus (float* left, float* right, int numSamples, const EffectParameters&);
    void processDelay (float* left, float* right, int numSamples, const EffectParameters&);
    void processReverb (float* left, float* right, int numSamples, const EffectParameters&);
    void processCompressor (float* left, float* right, int numSamples, const EffectParameters&);
    void processWidth (float* left, float* right, int numSamples, const EffectParameters&);

    float readFromBuffer (const std::vector<float>& data, int writeIndex, float delaySamples) const;

    double sampleRate = 44100.0;

    juce::Reverb reverb;

    // Delay
    std::vector<float> delayLeft, delayRight;
    int delayWrite = 0;

    // Chorus
    std::vector<float> chorusLeft, chorusRight;
    int chorusWrite = 0;
    double chorusPhase = 0.0;

    // Filter state (TPT SVF)
    std::array<float, 2> svfIc1 {};
    std::array<float, 2> svfIc2 {};

    // Drive tone filter state
    std::array<float, 2> toneState {};

    // Compressor envelope
    float compressorEnv = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectChain)
};
}
