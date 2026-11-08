#include "HallowethLookAndFeel.h"
#include "Theme.h"

namespace halloweth
{
HallowethLookAndFeel::HallowethLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, theme::outline);
    setColour (juce::Slider::textBoxTextColourId, theme::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::Label::textColourId, theme::text);

    setColour (juce::TextButton::buttonColourId, theme::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, theme::accent.withAlpha (0.22f));
    setColour (juce::TextButton::textColourOffId, theme::textDim);
    setColour (juce::TextButton::textColourOnId, theme::text);

    setColour (juce::TextEditor::backgroundColourId, theme::panelRaised);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::outline);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.35f));

    setColour (juce::ComboBox::backgroundColourId, theme::panelRaised);
    setColour (juce::ComboBox::outlineColourId, theme::outline);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);
}

void HallowethLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centreX = bounds.getCentreX();
    const auto centreY = bounds.getCentreY();
    const auto lineWidth = juce::jmax (2.0f, radius * 0.11f);
    const auto arcRadius = radius - lineWidth * 0.5f;
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::outline);
    g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    const auto activeColour = slider.findColour (juce::Slider::rotarySliderFillColourId);

    if (slider.isEnabled() && sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (activeColour);
        g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    const auto knobRadius = juce::jmax (2.0f, arcRadius - lineWidth * 1.7f);

    g.setGradientFill (juce::ColourGradient (theme::panelRaised, centreX, centreY - knobRadius,
                                             theme::backgroundAlt, centreX, centreY + knobRadius,
                                             false));
    g.fillEllipse (centreX - knobRadius, centreY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour (theme::outline.brighter (0.35f));
    g.drawEllipse (centreX - knobRadius, centreY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f,
                   1.0f);

    const auto pointerThickness = juce::jmax (2.0f, knobRadius * 0.13f);
    const auto pointerLength = knobRadius * 0.66f;

    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -knobRadius * 0.90f,
                                 pointerThickness, pointerLength, pointerThickness * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

    g.setColour (slider.isEnabled() ? activeColour.brighter (0.45f) : theme::textFaint);
    g.fillPath (pointer);
}

void HallowethLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = juce::jmin (bounds.getHeight() * 0.5f, 8.0f);
    const auto isOn = button.getToggleState();
    const auto isNav = button.getRadioGroupId() != 0;

    juce::Colour fill;

    if (isNav)
    {
        fill = isOn ? theme::accent.withAlpha (0.20f)
                    : (shouldDrawButtonAsHighlighted ? juce::Colours::white.withAlpha (0.045f)
                                                     : juce::Colours::transparentBlack);
    }
    else
    {
        fill = isOn ? theme::accent.withAlpha (0.24f) : backgroundColour;

        if (shouldDrawButtonAsDown)
            fill = fill.brighter (0.18f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter (0.10f);
    }

    if (shouldDrawButtonAsDown && isNav)
        fill = fill.brighter (0.25f);

    if (! fill.isTransparent())
    {
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, corner);
    }

    if (isOn)
    {
        g.setColour (theme::accent.withAlpha (0.60f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);

        // Active marker on the left edge of navigation entries.
        if (isNav)
        {
            g.setColour (theme::accentBright);
            g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + 3.0f,
                                                            bounds.getCentreY() - 7.0f,
                                                            2.5f, 14.0f), 1.25f);
        }
    }
    else if (! isNav)
    {
        g.setColour (theme::outline);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
}

void HallowethLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                          bool, bool)
{
    const auto textColour = button.getToggleState()
                              ? theme::text
                              : button.findColour (juce::TextButton::textColourOffId);

    g.setColour (button.isEnabled() ? textColour : theme::textFaint);
    g.setFont (getTextButtonFont (button, button.getHeight()));

    const auto justification = button.getRadioGroupId() != 0
                                 ? juce::Justification::centredLeft
                                 : juce::Justification::centred;

    auto textArea = button.getLocalBounds();

    if (button.getRadioGroupId() != 0)
        textArea = textArea.withTrimmedLeft (14);

    g.drawFittedText (button.getButtonText(), textArea, justification, 1, 0.9f);
}

juce::Font HallowethLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return theme::boldFont (juce::jlimit (11.0f, 14.0f, buttonHeight * 0.42f));
}

juce::Font HallowethLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}
}
