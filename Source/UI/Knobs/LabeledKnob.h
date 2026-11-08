#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace halloweth
{
/** A rotary control with a caption, attached to an APVTS parameter. */
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (const juce::String& labelText,
                 juce::AudioProcessorValueTreeState& stateToUse,
                 const juce::String& parameterID);

    void resized() override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::Label nameLabel;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabeledKnob)
};
}
