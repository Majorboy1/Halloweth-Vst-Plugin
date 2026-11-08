#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace halloweth
{
/** Small MIDI activity lamp. The editor drives it from the processor's live
    voice count, so it reflects real note activity rather than a timer loop. */
class MidiActivityLight : public juce::Component,
                          private juce::Timer
{
public:
    MidiActivityLight();

    void setActive (bool shouldBeActive);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    float target = 0.0f;
    float current = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiActivityLight)
};
}
