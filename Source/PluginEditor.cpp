#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "UI/Theme.h"

namespace halloweth
{
HallowethAudioProcessorEditor::HallowethAudioProcessorEditor (HallowethAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      presetBar (p),
      homePage (p),
      instrumentsPage (p),
      drumsPage (p),
      libraryPage (p),
      effectsPage (p),
      vocalsPage ("VOCALS",
                  "Vocal chops, choir textures and ad-libs with pitch, reverse and stutter.",
                  "PLANNED"),
      settingsPage ("SETTINGS",
                    "Sample library folders, missing-file relinking, MIDI mapping and "
                    "plugin preferences.",
                    "PLANNED"),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    pages = { &homePage, &instrumentsPage, &drumsPage, &vocalsPage,
              &libraryPage, &effectsPage, &settingsPage };

    addAndMakeVisible (navBar);
    addAndMakeVisible (presetBar);

    for (auto* page : pages)
        addChildComponent (page);

    addAndMakeVisible (keyboard);

    navBar.onPageSelected = [this] (NavigationBar::Page page) { showPage (page); };
    presetBar.onFavouriteChanged = [this] { libraryPage.refresh(); };

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (19.0f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdedee9));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1b1b26));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, theme::background);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        theme::accent.withAlpha (0.30f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId,
                        juce::Colours::black.withAlpha (0.5f));
    keyboard.setWantsKeyboardFocus (true);

    showPage (NavigationBar::Page::home);

    setResizable (true, true);
    setResizeLimits (980, 660, 1800, 1100);
    setSize (1180, 760);

    startTimerHz (30);
}

HallowethAudioProcessorEditor::~HallowethAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HallowethAudioProcessorEditor::showPage (NavigationBar::Page page)
{
    const auto index = juce::jlimit (0, (int) NavigationBar::Page::numPages - 1,
                                     NavigationBar::toIndex (page));

    for (size_t i = 0; i < pages.size(); ++i)
        if (pages[i] != nullptr)
            pages[i]->setVisible ((int) i == index);

    if (page == NavigationBar::Page::library)
        libraryPage.refresh();
}

void HallowethAudioProcessorEditor::timerCallback()
{
    const auto peak = processor.getOutputPeak();
    displayPeak = juce::jmax (peak, displayPeak * 0.88f);

    homePage.updateMeters (displayPeak);
    instrumentsPage.pollProcessor();
    drumsPage.pollProcessor();
    presetBar.pollProcessor();
    presetBar.setMidiActive (processor.getActiveVoiceCount() > 0);
}

void HallowethAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (theme::backgroundAlt, bounds.getTopLeft(),
                                             theme::background, bounds.getBottomRight(), false));
    g.fillRect (bounds);

    const auto centre = juce::Point<float> (bounds.getWidth() * 0.68f, bounds.getHeight() * 0.18f);
    g.setGradientFill (juce::ColourGradient (theme::accent.withAlpha (0.10f), centre,
                                             juce::Colours::transparentBlack,
                                             centre.translated (0.0f, bounds.getHeight() * 0.75f),
                                             true));
    g.fillRect (bounds);

    const auto keyboardTop = keyboard.getY() - 10;

    if (keyboardTop > 0)
    {
        g.setColour (theme::outline);
        g.fillRect (keyboard.getX(), keyboardTop, keyboard.getWidth(), 1);
    }
}

void HallowethAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    const auto sidebarWidth = juce::jlimit (184, 216,
                                            juce::roundToInt (area.getWidth() * 0.165f));
    navBar.setBounds (area.removeFromLeft (sidebarWidth));

    const auto headerHeight = juce::jlimit (88, 104,
                                            juce::roundToInt (area.getHeight() * 0.13f));
    presetBar.setBounds (area.removeFromTop (headerHeight).reduced (16, 10));

    const auto keyboardHeight = juce::jlimit (104, 136,
                                              juce::roundToInt (getHeight() * 0.155f));
    keyboard.setBounds (area.removeFromBottom (keyboardHeight).reduced (16, 8));

    const auto content = area.reduced (16, 6);

    for (auto* page : pages)
        if (page != nullptr)
            page->setBounds (content);
}
}
