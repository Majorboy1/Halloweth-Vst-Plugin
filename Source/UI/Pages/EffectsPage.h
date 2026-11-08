#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace halloweth
{
class HallowethAudioProcessor;
class LabeledKnob;

/** Effects page: seven real processors, each with an enable switch and its own controls.

    Effects are processed in a fixed serial chain in the audio engine; disabling one
    skips it entirely rather than muting it. */
class EffectsPage : public juce::Component
{
public:
    explicit EffectsPage (HallowethAudioProcessor&);
    ~EffectsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Card
    {
        juce::String title;
        juce::TextButton toggle;
        juce::OwnedArray<LabeledKnob> knobs;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
        juce::Rectangle<int> bounds;
    };

    void addCard (const juce::String& title, const juce::String& onParameter,
                  const std::vector<std::pair<juce::String, juce::String>>& knobs);

    HallowethAudioProcessor& processor;
    std::vector<std::unique_ptr<Card>> cards;

    juce::ComboBox filterModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsPage)
};
}
