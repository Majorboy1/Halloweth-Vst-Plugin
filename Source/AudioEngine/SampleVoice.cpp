#include "SampleVoice.h"

#include "AudioEngine.h"

namespace halloweth
{
namespace
{
    void equalPowerGains (float pan, float& left, float& right) noexcept
    {
        const auto angle = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.25f
                           * juce::MathConstants<float>::pi;
        left = std::cos (angle);
        right = std::sin (angle);
    }
}

bool SampleVoice::canPlaySound (juce::SynthesiserSound*)
{
    return slot == Slot::drums ? owner.shouldUseDrums() : owner.shouldUseSample();
}

void SampleVoice::startNote (int midiNoteNumber, float velocity,
                             juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    const auto velocityInt = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));

    zone = slot == Slot::drums ? owner.selectDrumZone (midiNoteNumber, velocityInt)
                               : owner.selectZone (midiNoteNumber, velocityInt);

    if (zone == nullptr || ! zone->isPlayable())
    {
        zone = nullptr;
        clearCurrentNote();
        return;
    }

    const auto& sample = *zone->data;

    position = 0.0;
    noteRatio = std::pow (2.0, (double) (midiNoteNumber - zone->zone.rootNote) / 12.0);
    oneShot = zone->zone.oneShot;

    auto gain = 0.30f * (0.25f + 0.75f * velocity)
                * juce::Decibels::decibelsToGain (zone->zone.gainDb);
    auto pan = zone->zone.pan;

    if (slot == Slot::drums)
    {
        const auto padIndex = juce::jlimit (0, AudioEngine::numPads - 1,
                                            midiNoteNumber - 36);
        gain *= owner.getPadGain (padIndex);
        pan = juce::jlimit (-1.0f, 1.0f, pan + owner.getPadPan (padIndex));
        noteRatio *= owner.getPadPitchRatio (padIndex);
        oneShot = true;
    }

    equalPowerGains (pan, gainLeft, gainRight);
    gainLeft *= gain;
    gainRight *= gain;

    loopStart = zone->zone.loopStart;
    loopEnd = zone->zone.loopEnd;

    if (loopEnd <= loopStart || loopEnd > sample.getNumSamples())
    {
        loopStart = sample.loopStart;
        loopEnd = sample.loopEnd;
    }

    if (loopEnd <= loopStart || loopEnd > sample.getNumSamples())
    {
        loopStart = 0;
        loopEnd = sample.getNumSamples();
    }

    looping = owner.isLoopEnabled() && ! oneShot && loopEnd > loopStart;

    envelope.setSampleRate (getSampleRate());
    envelope.setParameters (owner.getEnvelopeParameters());
    envelope.noteOn();

    updateIncrement();
    pitchWheelMoved (currentPitchWheelPosition);
}

void SampleVoice::stopNote (float, bool allowTailOff)
{
    // One-shot zones (drums, hits) ignore note-off and play to their natural end,
    // but a forced stop (panic / prepare / all-notes-off) must always win.
    if (allowTailOff && oneShot)
        return;

    if (allowTailOff)
    {
        envelope.noteOff();
    }
    else
    {
        zone = nullptr;   // the instrument may be pruned once this voice is idle
        envelope.reset();
        clearCurrentNote();
    }
}

void SampleVoice::pitchWheelMoved (int newPitchWheelValue)
{
    lastPitchWheelValue = newPitchWheelValue;
    updateIncrement();
}

void SampleVoice::updateIncrement()
{
    // Never touch the zone when idle: the selected instrument may already have been
    // pruned, which would leave this pointer dangling.
    if (zone == nullptr || ! isVoiceActive())
        return;

    const auto hostRate = getSampleRate();

    if (hostRate <= 0.0)
        return;

    const auto bendSemitones = 2.0f * ((float) (lastPitchWheelValue - 8192) / 8192.0f);
    const auto bendRatio = std::pow (2.0, (double) bendSemitones / 12.0);

    increment = noteRatio * bendRatio * (zone->data->sourceSampleRate / hostRate);
}

void SampleVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    envelope.setSampleRate (newRate);
    updateIncrement();
}

void SampleVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    // Activity is checked before the zone is touched - see updateIncrement().
    if (! isVoiceActive() || zone == nullptr)
        return;

    if (! zone->isPlayable())
    {
        zone = nullptr;
        clearCurrentNote();
        return;
    }

    const auto& sample = *zone->data;
    const auto length = sample.getNumSamples();

    if (length <= 0)
    {
        zone = nullptr;
        clearCurrentNote();
        return;
    }

    const auto end = looping ? juce::jmin (loopEnd, length) : length;
    const auto numChannels = juce::jmin (outputBuffer.getNumChannels(), 2);
    const auto sampleChannels = sample.getNumChannels();

    const float* sourceLeft = sample.audio.getReadPointer (0);
    const float* sourceRight = sampleChannels > 1 ? sample.audio.getReadPointer (1) : sourceLeft;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! envelope.isActive())
        {
            zone = nullptr;
            clearCurrentNote();
            return;
        }

        if (position >= (double) end)
        {
            if (looping)
            {
                position -= (double) (end - loopStart);
            }
            else
            {
                zone = nullptr;
                clearCurrentNote();
                return;
            }
        }

        const auto index = juce::jlimit (0, length - 1, (int) position);
        const auto fraction = (float) (position - (double) index);
        auto nextIndex = index + 1;

        if (nextIndex >= end)
            nextIndex = looping ? loopStart : index;

        if (nextIndex >= length)
            nextIndex = length - 1;

        const auto env = envelope.getNextSample();

        if (numChannels > 0)
        {
            const auto s0 = sourceLeft[index];
            const auto s1 = sourceLeft[nextIndex];
            outputBuffer.addSample (0, startSample + i,
                                    (s0 + (s1 - s0) * fraction) * env * gainLeft);
        }

        if (numChannels > 1)
        {
            const auto s0 = sourceRight[index];
            const auto s1 = sourceRight[nextIndex];
            outputBuffer.addSample (1, startSample + i,
                                    (s0 + (s1 - s0) * fraction) * env * gainRight);
        }

        position += increment;
    }
}
}
