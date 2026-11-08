#include "MidiActivityLight.h"
#include "Theme.h"

namespace halloweth
{
MidiActivityLight::MidiActivityLight()
{
    startTimerHz (30);
    setInterceptsMouseClicks (false, false);
}

void MidiActivityLight::setActive (bool shouldBeActive)
{
    target = shouldBeActive ? 1.0f : 0.0f;
}

void MidiActivityLight::timerCallback()
{
    const auto next = current + (target - current) * 0.35f;

    if (std::abs (next - current) > 0.004f)
    {
        current = next;
        repaint();
    }
    else if (current != target)
    {
        current = target;
        repaint();
    }
}

void MidiActivityLight::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (0.0f, 2.0f);

    g.setColour (theme::textFaint.withAlpha (0.9f));
    g.setFont (theme::boldFont (9.0f));
    g.drawText ("MIDI", area.removeFromLeft (30.0f), juce::Justification::centredLeft, false);

    auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre (area.getCentre());

    g.setColour (theme::accent.withAlpha (0.25f * current));
    g.fillEllipse (lamp.expanded (5.0f * current));

    g.setColour (theme::accent.withAlpha (0.22f + 0.78f * current));
    g.fillEllipse (lamp);
}
}
