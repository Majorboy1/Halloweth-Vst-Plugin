#include "LabeledKnob.h"
#include "../Theme.h"

namespace halloweth
{
LabeledKnob::LabeledKnob (const juce::String& labelText,
                          juce::AudioProcessorValueTreeState& stateToUse,
                          const juce::String& parameterID)
{
    nameLabel.setText (labelText, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (theme::boldFont (10.5f));
    nameLabel.setColour (juce::Label::textColourId, theme::textDim);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f,
                                true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 15);
    slider.setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        stateToUse, parameterID, slider);
}

void LabeledKnob::resized()
{
    auto area = getLocalBounds();

    nameLabel.setBounds (area.removeFromTop (14));
    area.removeFromTop (2);
    slider.setBounds (area);
}
}
