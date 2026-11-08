#include "SynthVoice.h"

#include "AudioEngine.h"

namespace halloweth
{
namespace
{
    /** PolyBLEP residual used to soften the discontinuity of saw and square waves. */
    float polyBlep (double phase, double increment)
    {
        if (increment <= 0.0)
            return 0.0f;

        if (phase < increment)
        {
            const auto t = phase / increment;
            return (float) (t + t - t * t - 1.0);
        }

        if (phase > 1.0 - increment)
        {
            const auto t = (phase - 1.0) / increment;
            return (float) (t * t + t + t + 1.0);
        }

        return 0.0f;
    }

    float renderOscillator (int waveform, double phase, double increment)
    {
        switch (waveform)
        {
            case 1: // square
            {
                const auto value = phase < 0.5 ? 1.0f : -1.0f;
                auto shifted = phase + 0.5;

                if (shifted >= 1.0)
                    shifted -= 1.0;

                return value + polyBlep (phase, increment) - polyBlep (shifted, increment);
            }

            case 2: // sine
                return (float) std::sin (phase * juce::MathConstants<double>::twoPi);

            case 3: // triangle
                return (float) (4.0 * std::abs (phase - 0.5) - 1.0);

            case 0: // saw
            default:
                return 2.0f * (float) phase - 1.0f - polyBlep (phase, increment);
        }
    }

    void panGains (float pan, float& left, float& right)
    {
        const auto angle = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.25f
                           * juce::MathConstants<float>::pi;
        left = std::cos (angle);
        right = std::sin (angle);
    }
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound*)
{
    return owner.shouldUseSynth();
}

void SynthVoice::startNote (int midiNoteNumber, float velocity,
                            juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    noteNumber = midiNoteNumber;
    velocityValue = juce::jlimit (0.0f, 1.0f, velocity);
    level = 0.30f * (0.30f + 0.70f * velocityValue);

    phase1 = 0.0;
    phase2 = 0.25;   // offset so the oscillators do not start in phase
    subPhase = 0.0;
    lfoPhase = 0.0;

    for (int ch = 0; ch < 2; ++ch)
    {
        ic1[ch] = 0.0f;
        ic2[ch] = 0.0f;
    }

    const auto envelope = owner.getEnvelopeParameters();

    ampEnvelope.setSampleRate (getSampleRate());
    ampEnvelope.setParameters (envelope);
    ampEnvelope.noteOn();

    // Percussive filter envelope: same attack, decays to zero.
    filterEnvelope.setSampleRate (getSampleRate());
    filterEnvelope.setParameters ({ envelope.attack,
                                    juce::jmax (0.02f, envelope.decay),
                                    0.0f,
                                    envelope.release });
    filterEnvelope.noteOn();

    pitchWheelMoved (currentPitchWheelPosition);
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
        filterEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        filterEnvelope.reset();
        clearCurrentNote();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    bendSemitones = 2.0f * ((float) (newPitchWheelValue - 8192) / 8192.0f);
}

void SynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    ampEnvelope.setSampleRate (newRate);
    filterEnvelope.setSampleRate (newRate);
}

void SynthVoice::updateCoefficients (float cutoff)
{
    const auto sampleRate = juce::jmax (1.0, getSampleRate());
    const auto clamped = juce::jlimit (20.0f, (float) (sampleRate * 0.45), cutoff);
    const auto g = (float) std::tan (juce::MathConstants<double>::pi * clamped / sampleRate);

    a1 = 1.0f / (1.0f + g * (g + filterK));
    a2 = g * a1;
    a3 = g * a2;
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const auto sampleRate = getSampleRate();

    if (sampleRate <= 0.0)
        return;

    const auto settings = owner.getSynthSettings();

    const auto detuneRatio = std::pow (2.0, (double) settings.detuneCents / 1200.0);
    const auto bendRatio = std::pow (2.0, (double) bendSemitones / 12.0);
    const auto frequency = baseFrequency * bendRatio;

    const auto increment1 = juce::jlimit (0.0, 0.45, frequency / sampleRate);
    const auto increment2 = juce::jlimit (0.0, 0.45, frequency * detuneRatio / sampleRate);
    const auto subIncrement = juce::jlimit (0.0, 0.45, frequency * 0.5 / sampleRate);

    const auto mix = juce::jlimit (0.0f, 1.0f, settings.mix);
    const auto subLevel = juce::jlimit (0.0f, 1.0f, settings.subLevel);
    const auto spread = juce::jlimit (0.0f, 1.0f, settings.spread);
    const auto filterEnvAmount = juce::jlimit (0.0f, 1.0f, settings.filterEnvAmount);
    const auto lfoDepth = juce::jlimit (0.0f, 1.0f, settings.lfoDepth);

    filterK = 1.0f / juce::jmax (0.6f, settings.resonance);

    // Oscillator panning gives the patch width without a separate chorus.
    float osc1Left, osc1Right, osc2Left, osc2Right;
    panGains (-spread * 0.8f, osc1Left, osc1Right);
    panGains (spread * 0.8f, osc2Left, osc2Right);

    constexpr double lfoHz = 0.8;
    const auto lfoIncrement = lfoHz / sampleRate;

    const auto numChannels = juce::jmin (outputBuffer.getNumChannels(), 2);

    updateCoefficients (settings.cutoffHz);

    for (int i = 0; i < numSamples; ++i)
    {
        if (! ampEnvelope.isActive())
        {
            clearCurrentNote();
            return;
        }

        const auto filterEnvValue = filterEnvelope.getNextSample();
        const auto lfoValue = (float) std::sin (lfoPhase * juce::MathConstants<double>::twoPi);

        lfoPhase += lfoIncrement;

        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        // Recompute the filter coefficients every 16 samples: smooth sweeps at a
        // fraction of the cost of a per-sample tangent.
        if ((i & 15) == 0)
        {
            // Key tracking keeps high notes as bright as low ones, and velocity adds
            // expression - without these, dark patches go silent up the keyboard.
            const auto keyTracking = (float) (noteNumber - 60) / 12.0f * 0.5f;
            const auto velocityOffset = (velocityValue - 0.5f) * 0.8f;

            const auto octaves = keyTracking + velocityOffset
                                 + filterEnvAmount * filterEnvValue * 5.0f
                                 + lfoDepth * lfoValue * 2.0f;
            updateCoefficients (settings.cutoffHz * std::pow (2.0f, octaves));
        }

        const auto osc1 = renderOscillator (settings.waveform, phase1, increment1);
        const auto osc2 = renderOscillator (settings.waveform, phase2, increment2);
        const auto sub = (float) std::sin (subPhase * juce::MathConstants<double>::twoPi) * subLevel;

        const auto leftSig  = osc1 * (1.0f - mix) * osc1Left + osc2 * mix * osc2Left + sub * 0.707f;
        const auto rightSig = osc1 * (1.0f - mix) * osc1Right + osc2 * mix * osc2Right + sub * 0.707f;

        phase1 += increment1;

        if (phase1 >= 1.0)
            phase1 -= 1.0;

        phase2 += increment2;

        if (phase2 >= 1.0)
            phase2 -= 1.0;

        subPhase += subIncrement;

        if (subPhase >= 1.0)
            subPhase -= 1.0;

        const auto env = ampEnvelope.getNextSample();
        const auto gain = env * level;

        float filtered[2];
        const float inputs[2] { leftSig, rightSig };

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto v3 = inputs[ch] - ic2[ch];
            const auto v1 = a1 * ic1[ch] + a2 * v3;
            const auto v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;

            ic1[ch] = 2.0f * v1 - ic1[ch];
            ic2[ch] = 2.0f * v2 - ic2[ch];

            // Gentle saturation adds analog warmth and keeps resonance peaks in check.
            filtered[ch] = std::tanh (juce::jlimit (-4.0f, 4.0f, v2) * 1.15f);
        }

        for (int ch = 0; ch < numChannels; ++ch)
            outputBuffer.addSample (ch, startSample + i, filtered[ch] * gain);
    }
}
}
