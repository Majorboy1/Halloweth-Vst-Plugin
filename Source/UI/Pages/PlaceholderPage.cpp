#include "PlaceholderPage.h"
#include "../Theme.h"

namespace halloweth
{
PlaceholderPage::PlaceholderPage (juce::String titleText, juce::String summaryText, juce::String phaseText)
    : title (std::move (titleText)),
      summary (std::move (summaryText)),
      phase (std::move (phaseText))
{
}

void PlaceholderPage::paint (juce::Graphics& g)
{
    theme::drawCard (g, getLocalBounds());

    auto area = getLocalBounds().reduced (40);
    auto centre = area.withSizeKeepingCentre (juce::jmin (620, area.getWidth()),
                                              juce::jmin (190, area.getHeight()));

    auto chipArea = centre.removeFromTop (28);
    auto chip = chipArea.withSizeKeepingCentre (150, 22).toFloat();
    g.setColour (theme::accent.withAlpha (0.14f));
    g.fillRoundedRectangle (chip, 11.0f);
    g.setColour (theme::accentBright);
    g.setFont (theme::boldFont (9.5f));
    g.drawText (phase, chip.toNearestInt(), juce::Justification::centred, false);

    centre.removeFromTop (14);

    g.setColour (theme::text);
    g.setFont (theme::boldFont (26.0f));
    g.drawText (title, centre.removeFromTop (34), juce::Justification::centred, false);

    centre.removeFromTop (10);

    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawFittedText (summary, centre.removeFromTop (52), juce::Justification::centredTop, 3);

    centre.removeFromTop (16);

    g.setColour (theme::textFaint);
    g.setFont (theme::font (10.5f));
    g.drawText ("This section is not implemented yet.", centre.removeFromTop (16),
                juce::Justification::centred, false);
}
}
