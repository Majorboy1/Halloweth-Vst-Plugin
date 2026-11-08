#include "DrumsPage.h"

#include "../../PluginProcessor.h"
#include "../Theme.h"

namespace halloweth
{
DrumsPage::DrumsPage (HallowethAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < DrumKit::numPads; ++i)
    {
        auto* pad = pads.add (new juce::TextButton());
        pad->setButtonText ("PAD " + juce::String (i + 1));
        pad->onClick = [this, i] { triggerPad (i); };
        addAndMakeVisible (pad);
    }

    loadKitButton.setButtonText ("LOAD KIT");
    loadKitButton.onClick = [this] { loadKitFile(); };
    addAndMakeVisible (loadKitButton);

    saveKitButton.setButtonText ("SAVE KIT");
    saveKitButton.onClick = [this] { saveKitFile(); };
    addAndMakeVisible (saveKitButton);

    loadPadButton.setButtonText ("LOAD SAMPLE");
    loadPadButton.onClick = [this] { loadPadFile(); };
    addAndMakeVisible (loadPadButton);

    clearPadButton.setButtonText ("CLEAR PAD");
    clearPadButton.onClick = [this] { clearSelectedPad(); };
    addAndMakeVisible (clearPadButton);

    kitLabel.setFont (theme::boldFont (13.0f));
    kitLabel.setColour (juce::Label::textColourId, theme::text);
    kitLabel.setJustificationType (juce::Justification::centredLeft);
    kitLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (kitLabel);

    statusLabel.setFont (theme::font (10.0f));
    statusLabel.setColour (juce::Label::textColourId, theme::textFaint);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    padNameLabel.setFont (theme::boldFont (16.0f));
    padNameLabel.setColour (juce::Label::textColourId, theme::text);
    padNameLabel.setJustificationType (juce::Justification::centredLeft);
    padNameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (padNameLabel);

    auto setUpSlider = [this] (juce::Slider& slider, double min, double max, double interval,
                               const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        slider.setRange (min, max, interval);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::trackColourId, theme::accent);
        slider.setColour (juce::Slider::backgroundColourId, theme::backgroundAlt);
        addAndMakeVisible (slider);
    };

    setUpSlider (levelSlider, -48.0, 12.0, 0.1, " dB");
    setUpSlider (panSlider, -1.0, 1.0, 0.01, "");
    setUpSlider (pitchSlider, -24.0, 24.0, 1.0, " st");

    levelSlider.onValueChange = [this]
    {
        processor.setPadAdjustment (selectedPad, (float) levelSlider.getValue(),
                                    (float) panSlider.getValue(),
                                    juce::roundToInt (pitchSlider.getValue()));
    };

    panSlider.onValueChange = [this]
    {
        processor.setPadAdjustment (selectedPad, (float) levelSlider.getValue(),
                                    (float) panSlider.getValue(),
                                    juce::roundToInt (pitchSlider.getValue()));
    };

    pitchSlider.onValueChange = [this]
    {
        processor.setPadAdjustment (selectedPad, (float) levelSlider.getValue(),
                                    (float) panSlider.getValue(),
                                    juce::roundToInt (pitchSlider.getValue()));
    };

    selectPad (0);
    pollProcessor();
}

DrumsPage::~DrumsPage() = default;

void DrumsPage::triggerPad (int padIndex)
{
    selectPad (padIndex);

    // Pad clicks play the exact same MIDI the DAW would send.
    processor.keyboardState.noteOn (1, DrumKit::noteForPad (padIndex), 0.9f);
    processor.keyboardState.noteOff (1, DrumKit::noteForPad (padIndex), 0.0f);

    if (processor.getDrumKit().pads[(size_t) padIndex].isEmpty())
        statusLabel.setText ("Pad empty - use LOAD SAMPLE to choose a file.", juce::dontSendNotification);
}

void DrumsPage::selectPad (int padIndex)
{
    selectedPad = juce::jlimit (0, DrumKit::numPads - 1, padIndex);

    for (int i = 0; i < pads.size(); ++i)
        pads[i]->setToggleState (i == selectedPad, juce::dontSendNotification);

    refreshPadValues();
}

void DrumsPage::refreshPadValues()
{
    const auto& kit = processor.getDrumKit();
    const auto& pad = kit.pads[(size_t) selectedPad];

    padNameLabel.setText (pad.isEmpty() ? ("Pad " + juce::String (selectedPad + 1)
                                          + "  (empty)")
                                        : pad.name,
                          juce::dontSendNotification);

    levelSlider.setValue (pad.gainDb, juce::dontSendNotification);
    panSlider.setValue (pad.pan, juce::dontSendNotification);
    pitchSlider.setValue (pad.pitchSemitones, juce::dontSendNotification);
}

void DrumsPage::loadPadFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load a drum sample",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                      processor.loadDrumPadSampleAsync (selectedPad, file);

                                  pollProcessor();
                              });
}

void DrumsPage::clearSelectedPad()
{
    processor.clearDrumPad (selectedPad);
    refreshPadValues();
}

void DrumsPage::loadKitFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load a drum kit (.json)",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                      processor.loadDrumKitAsync (file);

                                  pollProcessor();
                              });
}

void DrumsPage::saveKitFile()
{
    if (processor.getDrumKit().isEmpty())
    {
        statusLabel.setText ("Nothing to save - the kit is empty.", juce::dontSendNotification);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save drum kit as JSON",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("halloweth-kit.json"),
        "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  auto file = chooser.getResult();

                                  if (file.getFullPathName().isNotEmpty())
                                  {
                                      if (! file.hasFileExtension ("json"))
                                          file = file.withFileExtension ("json");

                                      const auto saved = processor.saveDrumKit (file);
                                      statusLabel.setText (saved ? "Kit saved to " + file.getFileName()
                                                                 : "Kit could not be saved",
                                                           juce::dontSendNotification);
                                  }
                              });
}

void DrumsPage::pollProcessor()
{
    if (processor.getDrumKitRevision() != lastRevision)
    {
        lastRevision = processor.getDrumKitRevision();

        const auto& kit = processor.getDrumKit();
        kitLabel.setText (kit.isEmpty() ? "No kit loaded"
                                        : kit.name + "  ·  " + juce::String (kit.getNumLoadedPads()) + "/16 pads",
                          juce::dontSendNotification);

        auto status = processor.getDrumStatusText();

        if (processor.getDrumErrorText().isNotEmpty())
            status << "  ·  " << processor.getDrumErrorText();

        statusLabel.setText (status, juce::dontSendNotification);

        for (int i = 0; i < pads.size(); ++i)
        {
            const auto& pad = kit.pads[(size_t) i];
            pads[i]->setButtonText (pad.isEmpty() ? ("PAD " + juce::String (i + 1))
                                                  : pad.name);
        }

        refreshPadValues();
    }
}

void DrumsPage::resized()
{
    auto area = getLocalBounds();

    kitBarBounds = area.removeFromTop (36);
    area.removeFromTop (10);

    auto kitBar = kitBarBounds;
    loadKitButton.setBounds (kitBar.removeFromLeft (110).reduced (0, 3));
    kitBar.removeFromLeft (8);
    saveKitButton.setBounds (kitBar.removeFromLeft (110).reduced (0, 3));
    kitBar.removeFromLeft (14);
    kitLabel.setBounds (kitBar.removeFromTop (18));
    statusLabel.setBounds (kitBar.removeFromTop (16));

    auto editor = area.removeFromRight (juce::jmax (300, area.getWidth() * 38 / 100));
    area.removeFromRight (12);

    gridBounds = area;
    editorBounds = editor;

    // 4x4 pad grid
    {
        const auto cellW = gridBounds.getWidth() / 4;
        const auto cellH = gridBounds.getHeight() / 4;

        for (int i = 0; i < pads.size(); ++i)
        {
            const auto row = i / 4;
            const auto column = i % 4;
            pads[i]->setBounds (gridBounds.getX() + column * cellW + 3,
                                gridBounds.getY() + row * cellH + 3,
                                cellW - 6, cellH - 6);
        }
    }

    // Pad editor
    {
        auto inner = editorBounds.reduced (18, 0).withTrimmedTop (38).withTrimmedBottom (14);

        padNameLabel.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (6);

        auto buttons = inner.removeFromTop (32);
        loadPadButton.setBounds (buttons.removeFromLeft (130));
        buttons.removeFromLeft (8);
        clearPadButton.setBounds (buttons.removeFromLeft (110));

        inner.removeFromTop (14);

        constexpr int rowHeight = 26;
        constexpr int gap = 10;

        auto addSliderRow = [&inner] (juce::Slider& slider)
        {
            slider.setBounds (inner.removeFromTop (rowHeight));
            inner.removeFromTop (gap);
        };

        addSliderRow (levelSlider);
        addSliderRow (panSlider);
        addSliderRow (pitchSlider);
    }
}

void DrumsPage::paint (juce::Graphics& g)
{
    theme::drawCard (g, gridBounds);

    // Pad captions live in the buttons, so only the card frame is drawn here.
    theme::drawCard (g, editorBounds, "PAD EDITOR");

    {
        auto inner = editorBounds.reduced (18, 0).withTrimmedTop (38).withTrimmedBottom (14);
        inner.removeFromTop (30);   // pad name
        inner.removeFromTop (38);   // buttons

        g.setColour (theme::textFaint);
        g.setFont (theme::boldFont (9.5f));

        const int labelHeight = 26;
        const int gap = 10;

        auto drawLabel = [&g, &inner, labelHeight, gap] (const char* text)
        {
            g.drawText (text, inner.removeFromTop (labelHeight).removeFromLeft (48),
                        juce::Justification::centredLeft, false);
            inner.removeFromTop (gap);
        };

        drawLabel ("LEVEL");
        drawLabel ("PAN");
        drawLabel ("PITCH");
    }

    {
        auto hint = editorBounds.reduced (18, 0).removeFromBottom (14);
        g.setColour (theme::textFaint);
        g.setFont (theme::font (9.5f));
        g.drawText ("Pads send MIDI 36-51. Adjustments apply instantly.",
                    hint, juce::Justification::bottomLeft, false);
    }
}
}
