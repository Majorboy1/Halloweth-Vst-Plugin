#include "EffectChain.h"

namespace halloweth
{
void EffectChain::prepare (double newSampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    const auto delaySize = (int) (sampleRate * 2.0) + 8;
    delayLeft.assign ((size_t) delaySize, 0.0f);
    delayRight.assign ((size_t) delaySize, 0.0f);

    const auto chorusSize = (int) (sampleRate * 0.08) + 8;
    chorusLeft.assign ((size_t) chorusSize, 0.0f);
    chorusRight.assign ((size_t) chorusSize, 0.0f);

    reverb.setSampleRate (sampleRate);

    reset();
}

void EffectChain::reset()
{
    std::fill (delayLeft.begin(), delayLeft.end(), 0.0f);
    std::fill (delayRight.begin(), delayRight.end(), 0.0f);
    std::fill (chorusLeft.begin(), chorusLeft.end(), 0.0f);
    std::fill (chorusRight.begin(), chorusRight.end(), 0.0f);

    delayWrite = 0;
    chorusWrite = 0;
    chorusPhase = 0.0;
    svfIc1 = {};
    svfIc2 = {};
    toneState = {};
    compressorEnv = 0.0f;

    reverb.reset();
}

float EffectChain::readFromBuffer (const std::vector<float>& data, int writeIndex,
                                   float delaySamples) const
{
    const auto size = (int) data.size();

    if (size <= 0)
        return 0.0f;

    auto readPosition = (float) writeIndex - delaySamples;
    readPosition = std::fmod (readPosition + (float) size, (float) size);

    const auto index0 = (int) readPosition;
    const auto index1 = (index0 + 1) % size;
    const auto fraction = readPosition - (float) index0;

    return data[(size_t) index0] + (data[(size_t) index1] - data[(size_t) index0]) * fraction;
}

void EffectChain::process (juce::AudioBuffer<float>& buffer, const EffectParameters& parameters)
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || buffer.getNumChannels() < 2 || ! parameters.anyActive())
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    if (parameters.filterOn)  processFilter (left, right, numSamples, parameters);
    if (parameters.driveOn)   processDrive (left, right, numSamples, parameters);
    if (parameters.chorusOn)  processChorus (left, right, numSamples, parameters);
    if (parameters.delayOn)   processDelay (left, right, numSamples, parameters);
    if (parameters.reverbOn)  processReverb (left, right, numSamples, parameters);
    if (parameters.compOn)    processCompressor (left, right, numSamples, parameters);
    if (parameters.widthOn)   processWidth (left, right, numSamples, parameters);
}

//==============================================================================
void EffectChain::processFilter (float* left, float* right, int numSamples,
                                 const EffectParameters& p)
{
    const auto cutoff = juce::jlimit (30.0f, (float) (sampleRate * 0.45), p.filterCutoff);
    const auto g = (float) std::tan (juce::MathConstants<double>::pi * cutoff / sampleRate);
    const auto k = 1.0f / juce::jmax (0.5f, p.filterResonance);
    const auto a1 = 1.0f / (1.0f + g * (g + k));
    const auto a2 = g * a1;
    const auto a3 = g * a2;

    auto runChannel = [&] (float* data, int channel)
    {
        auto& ic1 = svfIc1[(size_t) channel];
        auto& ic2 = svfIc2[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = data[i];
            const auto v3 = input - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;

            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            const auto low = v2;
            data[i] = p.filterMode == 1 ? input - k * (v1) - low : low;
        }
    };

    runChannel (left, 0);
    runChannel (right, 1);
}

void EffectChain::processDrive (float* left, float* right, int numSamples,
                                const EffectParameters& p)
{
    const auto drive = juce::jmax (1.0f, p.driveAmount);
    const auto makeUp = 1.0f / std::tanh (drive);

    // Tone: 0 = dark, 1 = open.
    const auto toneHz = juce::jmap (juce::jlimit (0.0f, 1.0f, p.driveTone), 400.0f, 18000.0f);
    const auto coefficient = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                              * toneHz / (float) sampleRate);

    auto runChannel = [&] (float* data, int channel)
    {
        auto& state = toneState[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            auto value = std::tanh (data[i] * drive) * makeUp;
            state += (value - state) * coefficient;
            data[i] = state;
        }
    };

    runChannel (left, 0);
    runChannel (right, 1);
}

void EffectChain::processChorus (float* left, float* right, int numSamples,
                                 const EffectParameters& p)
{
    if (chorusLeft.empty())
        return;

    const auto size = (int) chorusLeft.size();
    const auto rate = juce::jlimit (0.05f, 8.0f, p.chorusRate);
    const auto depth = juce::jlimit (0.0f, 1.0f, p.chorusDepth);
    const auto mix = juce::jlimit (0.0f, 1.0f, p.chorusMix);

    const auto baseDelay = (float) (sampleRate * 0.012);
    const auto modulation = (float) (sampleRate * 0.007) * depth;
    const auto phaseIncrement = rate / (float) sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto lfoLeft = std::sin ((float) chorusPhase * juce::MathConstants<float>::twoPi);
        const auto lfoRight = std::sin (((float) chorusPhase + 0.25f)
                                        * juce::MathConstants<float>::twoPi);

        const auto wetLeft = readFromBuffer (chorusLeft, chorusWrite,
                                             baseDelay + modulation * (1.0f + lfoLeft));
        const auto wetRight = readFromBuffer (chorusRight, chorusWrite,
                                              baseDelay + modulation * (1.0f + lfoRight));

        chorusLeft[(size_t) chorusWrite] = left[i];
        chorusRight[(size_t) chorusWrite] = right[i];
        chorusWrite = (chorusWrite + 1) % size;

        left[i] = left[i] * (1.0f - mix) + wetLeft * mix;
        right[i] = right[i] * (1.0f - mix) + wetRight * mix;

        chorusPhase += phaseIncrement;

        if (chorusPhase >= 1.0)
            chorusPhase -= 1.0;
    }
}

void EffectChain::processDelay (float* left, float* right, int numSamples,
                                const EffectParameters& p)
{
    if (delayLeft.empty())
        return;

    const auto size = (int) delayLeft.size();
    const auto delaySamples = juce::jlimit (1.0f, (float) (size - 4),
                                            juce::jmax (1.0f, p.delayTimeMs)
                                                * 0.001f * (float) sampleRate);
    const auto feedback = juce::jlimit (0.0f, 0.95f, p.delayFeedback);
    const auto mix = juce::jlimit (0.0f, 1.0f, p.delayMix);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetLeft = readFromBuffer (delayLeft, delayWrite, delaySamples);
        const auto wetRight = readFromBuffer (delayRight, delayWrite, delaySamples);

        delayLeft[(size_t) delayWrite] = left[i] + wetLeft * feedback;
        delayRight[(size_t) delayWrite] = right[i] + wetRight * feedback;
        delayWrite = (delayWrite + 1) % size;

        left[i] = left[i] * (1.0f - mix) + wetLeft * mix;
        right[i] = right[i] * (1.0f - mix) + wetRight * mix;
    }
}

void EffectChain::processReverb (float* left, float* right, int numSamples,
                                 const EffectParameters& p)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit (0.0f, 1.0f, p.reverbSize);
    rp.damping = juce::jlimit (0.0f, 1.0f, p.reverbDamping);
    rp.wetLevel = juce::jlimit (0.0f, 1.0f, p.reverbMix);
    rp.dryLevel = 1.0f - rp.wetLevel;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;

    reverb.setParameters (rp);
    reverb.processStereo (left, right, numSamples);
}

void EffectChain::processCompressor (float* left, float* right, int numSamples,
                                     const EffectParameters& p)
{
    const auto threshold = juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 0.0f,
                                                                        p.compThresholdDb));
    const auto ratio = juce::jmax (1.0f, p.compRatio);
    const auto makeUp = juce::Decibels::decibelsToGain (p.compMakeupDb);

    const auto attackCoefficient = 1.0f - std::exp (-1.0f / (0.010f * (float) sampleRate));
    const auto releaseCoefficient = 1.0f - std::exp (-1.0f / (0.150f * (float) sampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto level = juce::jmax (std::abs (left[i]), std::abs (right[i]));
        const auto coefficient = level > compressorEnv ? attackCoefficient : releaseCoefficient;
        compressorEnv += (level - compressorEnv) * coefficient;

        auto gain = 1.0f;

        if (compressorEnv > threshold && compressorEnv > 1.0e-6f)
        {
            const auto overDb = juce::Decibels::gainToDecibels (compressorEnv / threshold);
            const auto reducedDb = overDb * (1.0f - 1.0f / ratio);
            gain = juce::Decibels::decibelsToGain (-reducedDb);
        }

        left[i] *= gain * makeUp;
        right[i] *= gain * makeUp;
    }
}

void EffectChain::processWidth (float* left, float* right, int numSamples,
                                const EffectParameters& p)
{
    const auto width = juce::jlimit (0.0f, 2.0f, p.widthAmount);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto mid = (left[i] + right[i]) * 0.5f;
        const auto side = (left[i] - right[i]) * 0.5f * width;

        left[i] = mid + side;
        right[i] = mid - side;
    }
}
}
