#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace halloweth
{
class HallowethAudioProcessor;
class LabeledKnob;

/** Home page: build status, output meter and the master/tone controls. */
class HomePage : public juce::Component
{
public:
    explicit HomePage (HallowethAudioProcessor&);
    ~HomePage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called from the editor timer with a decayed peak value. */
    void updateMeters (float peak);

private:
    HallowethAudioProcessor& processor;

    juce::OwnedArray<LabeledKnob> knobs;

    juce::Rectangle<int> heroBounds, outputBounds, statusBounds, meterBounds;
    float meterLevel = 0.0f;
    float meterDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomePage)
};
}
