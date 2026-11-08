#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "MidiActivityLight.h"

namespace halloweth
{
class HallowethAudioProcessor;

/** Central preset strip: name, category, description, previous/next, favourite
    and the MIDI activity lamp. */
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (HallowethAudioProcessor&);
    ~PresetBar() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setMidiActive (bool shouldBeActive) { midiLight.setActive (shouldBeActive); }
    void pollProcessor();
    void refreshFromProcessor();

    std::function<void()> onPresetChanged;

private:
    void step (int delta);

    HallowethAudioProcessor& processor;

    juce::TextButton prevButton, nextButton, favouriteButton;
    juce::Label nameLabel, categoryLabel, descriptionLabel;
    MidiActivityLight midiLight;

    int lastPresetIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
}
