#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace halloweth
{
/** Honest placeholder for a section that has not been built yet.

    It states what the section will contain and which development phase will
    deliver it - it does not present inactive controls. */
class PlaceholderPage : public juce::Component
{
public:
    PlaceholderPage (juce::String titleText, juce::String summaryText, juce::String phaseText);

    void paint (juce::Graphics&) override;

private:
    juce::String title, summary, phase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaceholderPage)
};
}
