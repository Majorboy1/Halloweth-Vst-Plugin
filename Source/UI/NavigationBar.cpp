#include "NavigationBar.h"
#include "Theme.h"

namespace halloweth
{
NavigationBar::NavigationBar()
{
    static const char* names[] { "HOME", "INSTRUMENTS", "DRUMS", "VOCALS",
                                 "LIBRARY", "EFFECTS", "SETTINGS" };

    for (int i = 0; i < (int) Page::numPages; ++i)
    {
        auto* button = buttons.add (new juce::TextButton (names[i]));
        button->setRadioGroupId (4711);
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::textColourOffId, theme::textDim);

        const auto page = (Page) i;
        button->onClick = [this, page]
        {
            setSelectedPage (page, juce::sendNotificationSync);
        };

        addAndMakeVisible (button);
    }

    setSelectedPage (Page::home, juce::dontSendNotification);
}

juce::String NavigationBar::getNameForPage (Page page)
{
    switch (page)
    {
        case Page::home:        return "Home";
        case Page::instruments: return "Instruments";
        case Page::drums:       return "Drums";
        case Page::vocals:      return "Vocals";
        case Page::library:     return "Library";
        case Page::effects:     return "Effects";
        case Page::settings:    return "Settings";
        case Page::numPages:    break;
    }

    return {};
}

void NavigationBar::setSelectedPage (Page page, juce::NotificationType notification)
{
    if (! juce::isPositiveAndBelow ((int) page, (int) Page::numPages))
        return;

    const auto changed = page != currentPage;
    currentPage = page;

    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == (int) page, juce::dontSendNotification);

    if (changed && notification != juce::dontSendNotification && onPageSelected != nullptr)
        onPageSelected (page);
}

void NavigationBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (theme::panel);
    g.fillRect (bounds);

    g.setColour (theme::outline);
    g.fillRect (bounds.getRight() - 1, 0, 1, bounds.getHeight());

    // Wordmark
    auto logoArea = bounds.removeFromTop (logoHeight).reduced (22, 20);
    g.setColour (theme::accentBright);
    g.setFont (theme::boldFont (22.0f));
    g.drawText ("HALLOWETH", logoArea.removeFromTop (28), juce::Justification::centredLeft, false);

    g.setColour (theme::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) logoArea.getX(),
                                                    (float) logoArea.getY() + 4.0f,
                                                    26.0f, 3.0f), 1.5f);

    g.setColour (theme::textFaint);
    g.setFont (theme::font (10.0f));
    g.drawText ("AFRICAN MUSIC STUDIO", logoArea.withTrimmedTop (16),
                juce::Justification::topLeft, false);

    // Footer
    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (22, 16);

    g.setColour (theme::textFaint);
    g.setFont (theme::font (10.0f));
    g.drawText ("WINDOWS  ·  VST3", footer, juce::Justification::bottomLeft, false);
}

void NavigationBar::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (logoHeight);
    area.removeFromBottom (footerHeight);
    area.reduce (16, 0);

    constexpr int buttonHeight = 36;
    constexpr int gap = 5;

    auto y = area.getY() + 6;

    for (auto* button : buttons)
    {
        button->setBounds (area.getX(), y, area.getWidth(), buttonHeight);
        y += buttonHeight + gap;
    }
}
}
