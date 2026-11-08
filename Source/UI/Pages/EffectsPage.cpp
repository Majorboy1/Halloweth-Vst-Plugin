#include "EffectsPage.h"

#include "../../PluginProcessor.h"
#include "../Knobs/LabeledKnob.h"
#include "../Theme.h"

namespace halloweth
{
EffectsPage::EffectsPage (HallowethAudioProcessor& p) : processor (p)
{
    addCard ("REVERB", param::fxReverbOn,
             { { param::fxReverbMix, "MIX" },
               { param::fxReverbSize, "SIZE" },
               { param::fxReverbDamp, "DAMP" } });

    addCard ("DELAY", param::fxDelayOn,
             { { param::fxDelayTime, "TIME" },
               { param::fxDelayFeedback, "FBACK" },
               { param::fxDelayMix, "MIX" } });

    addCard ("CHORUS", param::fxChorusOn,
             { { param::fxChorusRate, "RATE" },
               { param::fxChorusDepth, "DEPTH" },
               { param::fxChorusMix, "MIX" } });

    addCard ("FILTER", param::fxFilterOn,
             { { param::fxFilterCutoff, "CUTOFF" },
               { param::fxFilterRes, "RES" } });

    addCard ("DRIVE", param::fxDriveOn,
             { { param::fxDriveAmount, "AMOUNT" },
               { param::fxDriveTone, "TONE" } });

    addCard ("COMPRESSOR", param::fxCompOn,
             { { param::fxCompThreshold, "THRESH" },
               { param::fxCompRatio, "RATIO" },
               { param::fxCompMakeup, "MAKEUP" } });

    addCard ("STEREO WIDTH", param::fxWidthOn,
             { { param::fxWidthAmount, "WIDTH" } });

    filterModeCombo.addItemList ({ "Low Pass", "High Pass" }, 1);
    filterModeCombo.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (filterModeCombo);

    filterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, param::fxFilterMode, filterModeCombo);
}

EffectsPage::~EffectsPage() = default;

void EffectsPage::addCard (const juce::String& title, const juce::String& onParameter,
                           const std::vector<std::pair<juce::String, juce::String>>& knobs)
{
    auto card = std::make_unique<Card>();
    card->title = title;

    card->toggle.setButtonText ("ON");
    card->toggle.setClickingTogglesState (true);
    addAndMakeVisible (card->toggle);

    card->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, onParameter, card->toggle);

    for (const auto& spec : knobs)
    {
        auto* knob = card->knobs.add (new LabeledKnob (spec.second, processor.apvts, spec.first));
        addAndMakeVisible (knob);
    }

    cards.push_back (std::move (card));
}

void EffectsPage::resized()
{
    if (cards.empty())
        return;

    constexpr int columns = 2;
    constexpr int gap = 10;

    const auto totalRows = (int) ((cards.size() + columns - 1) / columns);
    const auto cellWidth = (getWidth() - gap) / columns;
    const auto cellHeight = (getHeight() - gap * (totalRows - 1)) / totalRows;

    for (int i = 0; i < (int) cards.size(); ++i)
    {
        auto& card = *cards[(size_t) i];
        const auto column = i % columns;
        const auto row = i / columns;

        card.bounds = juce::Rectangle<int> (column * (cellWidth + gap),
                                            row * (cellHeight + gap),
                                            cellWidth, cellHeight);

        auto inner = card.bounds.reduced (16, 12);

        auto header = inner.removeFromTop (24);
        card.toggle.setBounds (header.removeFromRight (64).reduced (0, 1));

        inner.removeFromTop (6);

        if (card.knobs.isEmpty())
            continue;

        const auto knobWidth = inner.getWidth() / card.knobs.size();
        auto x = inner.getX();

        for (auto* knob : card.knobs)
        {
            knob->setBounds (x, inner.getY(), knobWidth, inner.getHeight());
            x += knobWidth;
        }
    }

    // The filter type selector sits in the FILTER card, above its knobs.
    if (cards.size() > 3)
    {
        auto filterBounds = cards[3]->bounds.reduced (16, 12).withTrimmedTop (52).withTrimmedBottom (12);
        filterModeCombo.setBounds (filterBounds.removeFromTop (22).withTrimmedLeft (44));
    }
}

void EffectsPage::paint (juce::Graphics& g)
{
    for (const auto& cardPtr : cards)
    {
        const auto& card = *cardPtr;

        theme::drawCard (g, card.bounds, card.title);

        if (card.title == "FILTER")
        {
            g.setColour (theme::textFaint);
            g.setFont (theme::boldFont (9.0f));
            g.drawText ("MODE", card.bounds.reduced (16, 12).withTrimmedTop (52).removeFromTop (22),
                        juce::Justification::centredLeft, false);
        }
    }
}
}
