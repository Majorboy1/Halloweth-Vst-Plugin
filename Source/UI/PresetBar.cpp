#include "PresetBar.h"

#include "../PluginProcessor.h"
#include "Theme.h"

namespace halloweth
{
PresetBar::PresetBar (HallowethAudioProcessor& p) : processor (p)
{
    auto setUp = [this] (juce::TextButton& button, const juce::String& text)
    {
        button.setButtonText (text);
        button.setColour (juce::TextButton::buttonColourId, theme::panelRaised);
        addAndMakeVisible (button);
    };

    setUp (prevButton, juce::String::fromUTF8 ("\xe2\x80\xb9"));
    setUp (nextButton, juce::String::fromUTF8 ("\xe2\x80\xba"));
    setUp (favouriteButton, juce::String::fromUTF8 ("\xe2\x98\x85"));

    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    favouriteButton.setTooltip ("Favourite");

    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    favouriteButton.onClick = [this]
    {
        processor.togglePresetFavourite (processor.getCurrentPresetIndex());
        refreshFromProcessor();

        if (onFavouriteChanged != nullptr)
            onFavouriteChanged();
    };

    nameLabel.setFont (theme::boldFont (19.0f));
    nameLabel.setColour (juce::Label::textColourId, theme::text);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    categoryLabel.setFont (theme::boldFont (10.0f));
    categoryLabel.setColour (juce::Label::textColourId, theme::accentBright);
    categoryLabel.setJustificationType (juce::Justification::centredLeft);
    categoryLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (categoryLabel);

    descriptionLabel.setFont (theme::font (11.0f));
    descriptionLabel.setColour (juce::Label::textColourId, theme::textDim);
    descriptionLabel.setJustificationType (juce::Justification::centredLeft);
    descriptionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (descriptionLabel);

    addAndMakeVisible (midiLight);

    refreshFromProcessor();
}

void PresetBar::step (int delta)
{
    processor.selectNextPreset (delta);
    refreshFromProcessor();

    if (onPresetChanged != nullptr)
        onPresetChanged();
}

void PresetBar::pollProcessor()
{
    if (processor.getCurrentPresetIndex() != lastPresetIndex)
        refreshFromProcessor();
}

void PresetBar::refreshFromProcessor()
{
    const auto index = processor.getCurrentPresetIndex();
    lastPresetIndex = index;

    const auto& info = processor.getPresetInfo (index);

    nameLabel.setText (info.name, juce::dontSendNotification);

    juce::String category;
    category << info.category.toUpperCase();

    if (info.genre.isNotEmpty())
        category << "  ·  " << info.genre.toUpperCase();

    if (info.requiresExternalAudio())
        category << "  ·  NEEDS YOUR SAMPLE";

    categoryLabel.setText (category, juce::dontSendNotification);
    descriptionLabel.setText (info.description, juce::dontSendNotification);

    const auto isFavourite = processor.isPresetFavourite (index);
    favouriteButton.setToggleState (isFavourite, juce::dontSendNotification);
    favouriteButton.setColour (juce::TextButton::textColourOffId,
                               isFavourite ? theme::accentBright : theme::textDim);
    favouriteButton.setColour (juce::TextButton::buttonColourId,
                               isFavourite ? theme::accent.withAlpha (0.18f) : theme::panelRaised);
}

void PresetBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (theme::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + 1.0f,
                                                    bounds.getY() + 12.0f,
                                                    3.0f,
                                                    bounds.getHeight() - 24.0f), 1.5f);
}

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced (16, 10);

    auto buttonsRow = area;
    area.removeFromTop (2);

    prevButton.setBounds (buttonsRow.removeFromLeft (32).withSizeKeepingCentre (32, 32));
    buttonsRow.removeFromLeft (10);
    midiLight.setBounds (buttonsRow.removeFromRight (60).withSizeKeepingCentre (60, 20));
    nextButton.setBounds (buttonsRow.removeFromRight (32).withSizeKeepingCentre (32, 32));
    buttonsRow.removeFromRight (8);
    favouriteButton.setBounds (buttonsRow.removeFromRight (32).withSizeKeepingCentre (32, 32));
    buttonsRow.removeFromRight (14);

    auto textArea = buttonsRow;
    nameLabel.setBounds (textArea.removeFromTop (22));
    categoryLabel.setBounds (textArea.removeFromTop (13));
    descriptionLabel.setBounds (textArea.removeFromTop (14));
}
}
