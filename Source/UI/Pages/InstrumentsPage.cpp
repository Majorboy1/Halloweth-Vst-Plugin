#include "InstrumentsPage.h"

#include "../../PluginProcessor.h"
#include "../Knobs/LabeledKnob.h"
#include "../Theme.h"

namespace halloweth
{
InstrumentsPage::InstrumentsPage (HallowethAudioProcessor& p) : processor (p)
{
    // ---- engine selector -----------------------------------------------------
    engineSampleButton.setButtonText ("SAMPLE MAP");
    engineSampleButton.setTooltip ("Play a WAV/AIFF sample or a JSON multisample map");
    engineSampleButton.setRadioGroupId (5150);
    engineSampleButton.onClick = [this] { setEngineMode (0); };
    addAndMakeVisible (engineSampleButton);

    engineSynthButton.setButtonText ("SYNTH ENGINE");
    engineSynthButton.setTooltip ("Use Halloweth's built-in oscillator synth");
    engineSynthButton.setRadioGroupId (5150);
    engineSynthButton.onClick = [this] { setEngineMode (1); };
    addAndMakeVisible (engineSynthButton);

    engineSfzButton.setButtonText ("SFZ / PIANO");
    engineSfzButton.setTooltip ("Select the SFZ instrument engine");
    engineSfzButton.setRadioGroupId (5150);
    engineSfzButton.onClick = [this] { setEngineMode (3); };
    addAndMakeVisible (engineSfzButton);

    // ---- sample map card -----------------------------------------------------
    loadFileButton.setButtonText ("LOAD SAMPLE");
    loadFileButton.setTooltip ("Load a single WAV, AIFF, FLAC or OGG sample");
    loadFileButton.onClick = [this] { chooseSampleFile(); };
    addAndMakeVisible (loadFileButton);

    loadMapButton.setButtonText ("LOAD MAP");
    loadMapButton.setTooltip ("Load a Halloweth JSON multisample map");
    loadMapButton.onClick = [this] { chooseMapFile(); };
    addAndMakeVisible (loadMapButton);

    loadSfzButton.setButtonText ("LOAD SFZ");
    loadSfzButton.setTooltip ("Load an SFZ definition and its relative sample files");
    loadSfzButton.setColour (juce::TextButton::buttonColourId, theme::accentDeep);
    loadSfzButton.setColour (juce::TextButton::textColourOffId, theme::text);
    loadSfzButton.onClick = [this] { chooseSfzFile(); };
    addAndMakeVisible (loadSfzButton);

    clearButton.setButtonText ("CLEAR");
    clearButton.setTooltip ("Unload the current sample or SFZ instrument");
    clearButton.onClick = [this]
    {
        processor.clearSample();
        refresh();
    };
    addAndMakeVisible (clearButton);

    loopButton.setButtonText ("LOOP");
    loopButton.setTooltip ("Loop mapped samples that contain loop points");
    loopButton.setClickingTogglesState (true);
    addAndMakeVisible (loopButton);

    loopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, param::loopEnabled, loopButton);

    nameLabel.setFont (theme::boldFont (18.0f));
    nameLabel.setColour (juce::Label::textColourId, theme::text);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    detailsLabel.setFont (theme::font (11.0f));
    detailsLabel.setColour (juce::Label::textColourId, theme::textDim);
    detailsLabel.setJustificationType (juce::Justification::centredLeft);
    detailsLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (detailsLabel);

    statusLabel.setFont (theme::font (10.0f));
    statusLabel.setColour (juce::Label::textColourId, theme::textFaint);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    // ---- synth card ----------------------------------------------------------
    waveCombo.addItemList ({ "Saw", "Square", "Sine", "Triangle" }, 1);
    waveCombo.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (waveCombo);

    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, param::synthWave, waveCombo);

    synthKnobs.add (new LabeledKnob ("DETUNE", processor.apvts, param::synthDetune));
    synthKnobs.add (new LabeledKnob ("OSC MIX", processor.apvts, param::synthMix));
    synthKnobs.add (new LabeledKnob ("SUB", processor.apvts, param::synthSub));
    synthKnobs.add (new LabeledKnob ("SPREAD", processor.apvts, param::synthSpread));
    synthKnobs.add (new LabeledKnob ("CUTOFF", processor.apvts, param::synthCutoff));
    synthKnobs.add (new LabeledKnob ("RES", processor.apvts, param::synthResonance));
    synthKnobs.add (new LabeledKnob ("F-ENV", processor.apvts, param::synthFilterEnv));
    synthKnobs.add (new LabeledKnob ("LFO", processor.apvts, param::synthLfoDepth));

    for (auto* knob : synthKnobs)
        addAndMakeVisible (knob);

    // ---- envelope ------------------------------------------------------------
    envelopeKnobs.add (new LabeledKnob ("ATTACK", processor.apvts, param::attack));
    envelopeKnobs.add (new LabeledKnob ("DECAY", processor.apvts, param::decay));
    envelopeKnobs.add (new LabeledKnob ("SUSTAIN", processor.apvts, param::sustain));
    envelopeKnobs.add (new LabeledKnob ("RELEASE", processor.apvts, param::release));

    for (auto* knob : envelopeKnobs)
        addAndMakeVisible (knob);

    refresh();
}

InstrumentsPage::~InstrumentsPage() = default;

void InstrumentsPage::setEngineMode (int mode)
{
    if (auto* parameter = processor.apvts.getParameter (param::engineMode))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) mode));

    pollProcessor();
}

void InstrumentsPage::chooseSampleFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load a WAV or AIFF sample",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                  {
                                      processor.loadSampleFileAsync (file);
                                      refresh();
                                  }
                              });
}

void InstrumentsPage::chooseMapFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load an HALLOWETH multisample map (.json)",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                  {
                                      processor.loadSampleMapAsync (file);
                                      refresh();
                                  }
                              });
}

void InstrumentsPage::chooseSfzFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load an SFZ instrument",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.sfz");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                  {
                                      processor.loadSfzFileAsync (file);
                                      setEngineMode (3);
                                      refresh();
                                  }
                              });
}

void InstrumentsPage::pollProcessor()
{
    const auto mode = processor.apvts.getRawParameterValue (param::engineMode) != nullptr
                          ? processor.apvts.getRawParameterValue (param::engineMode)->load()
                          : 1.0f;

    engineSampleButton.setToggleState (mode < 0.5f, juce::dontSendNotification);
    engineSynthButton.setToggleState (mode >= 0.5f && mode < 1.5f, juce::dontSendNotification);
    engineSfzButton.setToggleState (mode >= 2.5f, juce::dontSendNotification);

    juce::String status;

    if (mode >= 2.5f)
        status = processor.getSfzFilePath().isNotEmpty() ? "SFZ ENGINE ACTIVE" : "SFZ SELECTED - NO FILE";
    else if (mode >= 1.5f)
        status = "DRUM ENGINE ACTIVE";
    else if (mode >= 0.5f)
        status = "SYNTH ENGINE ACTIVE";
    else if (processor.getLoadedInstrument() != nullptr)
        status = "SAMPLE MAP ACTIVE";
    else
        status = "NO SAMPLE LOADED - SYNTH FALLBACK";

    if (status != engineStatusText)
    {
        engineStatusText = status;
        repaint (headerBounds);
    }

    if (processor.getSampleRevision() != lastRevision)
        refresh();
}

void InstrumentsPage::refresh()
{
    lastRevision = processor.getSampleRevision();

    const auto instrument = processor.getLoadedInstrument();

    if (instrument != nullptr)
    {
        nameLabel.setText (instrument->name, juce::dontSendNotification);

        juce::String details;
        details << juce::String (instrument->getNumPlayableZones()) << " zone(s)  ·  "
                << juce::String (instrument->getNumZones()) << " mapped  ·  "
                << juce::String (juce::File (instrument->sourcePath).getFileName());

        detailsLabel.setText (details, juce::dontSendNotification);
    }
    else if (processor.getSfzFilePath().isNotEmpty())
    {
        nameLabel.setText ("SFZ Instrument", juce::dontSendNotification);
        detailsLabel.setText (juce::String (processor.getSfzNumRegions()) + " regions  ·  "
                                  + juce::File (processor.getSfzFilePath()).getFileName(),
                              juce::dontSendNotification);
    }
    else
    {
        nameLabel.setText ("No instrument loaded", juce::dontSendNotification);
        detailsLabel.setText ("Load one sample, or a JSON map for multisampling and velocity layers.",
                              juce::dontSendNotification);
    }

    auto status = processor.getSampleStatusText();

    if (processor.getSampleErrorText().isNotEmpty())
        status << "  ·  " << processor.getSampleErrorText();

    statusLabel.setText (status, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           processor.getSampleErrorText().isNotEmpty() ? theme::pending
                                                                       : theme::textFaint);
}

void InstrumentsPage::resized()
{
    auto area = getLocalBounds();

    headerBounds = area.removeFromTop (40);
    area.removeFromTop (10);

    envelopeBounds = area.removeFromBottom (146);
    area.removeFromBottom (10);

    auto headerInner = headerBounds.reduced (2, 4);
    headerInner.removeFromLeft (62);   // leaves room for the ENGINE caption
    engineSampleButton.setBounds (headerInner.removeFromLeft (112));
    headerInner.removeFromLeft (8);
    engineSynthButton.setBounds (headerInner.removeFromLeft (112));
    headerInner.removeFromLeft (8);
    engineSfzButton.setBounds (headerInner.removeFromLeft (110));

    auto leftColumn = area.removeFromLeft (juce::jmax (300, area.getWidth() * 55 / 100));
    area.removeFromLeft (10);

    sampleBounds = leftColumn;
    synthBounds = area;

    // Sample card internals
    {
        auto inner = sampleBounds.reduced (18, 0).withTrimmedTop (36).withTrimmedBottom (14);

        nameLabel.setBounds (inner.removeFromTop (24));
        detailsLabel.setBounds (inner.removeFromTop (18));
        inner.removeFromTop (4);
        statusLabel.setBounds (inner.removeFromTop (20));

        auto controls = inner.removeFromBottom (34);
        inner.removeFromBottom (6);
        auto loadButtons = inner.removeFromBottom (34);
        const auto loadButtonGap = 6;
        const auto loadButtonWidth = (loadButtons.getWidth() - loadButtonGap * 2) / 3;
        loadFileButton.setBounds (loadButtons.removeFromLeft (loadButtonWidth));
        loadButtons.removeFromLeft (loadButtonGap);
        loadMapButton.setBounds (loadButtons.removeFromLeft (loadButtonWidth));
        loadButtons.removeFromLeft (loadButtonGap);
        loadSfzButton.setBounds (loadButtons);

        const auto controlWidth = (controls.getWidth() - 8) / 2;
        clearButton.setBounds (controls.removeFromLeft (controlWidth));
        controls.removeFromLeft (8);
        loopButton.setBounds (controls);
    }

    // Synth card internals
    {
        auto inner = synthBounds.reduced (18, 0).withTrimmedTop (36).withTrimmedBottom (14);

        auto comboRow = inner.removeFromTop (26);
        comboRow.removeFromLeft (60);
        waveCombo.setBounds (comboRow);

        inner.removeFromTop (8);

        constexpr int columns = 4;
        const auto rows = juce::jmax (1, (synthKnobs.size() + columns - 1) / columns);
        const auto knobWidth = inner.getWidth() / columns;
        const auto knobHeight = juce::jmax (68, inner.getHeight() / rows);

        for (int i = 0; i < synthKnobs.size(); ++i)
        {
            const auto column = i % columns;
            const auto row = i / columns;

            synthKnobs[i]->setBounds (inner.getX() + column * knobWidth,
                                      inner.getY() + row * knobHeight,
                                      knobWidth, knobHeight);
        }
    }

    // Envelope knobs
    {
        auto inner = envelopeBounds.reduced (18, 0).withTrimmedTop (36).withTrimmedBottom (14);
        const auto knobWidth = juce::jmin (124, inner.getWidth() / juce::jmax (1, envelopeKnobs.size()));
        const auto knobHeight = juce::jmax (80, juce::jmin (120, inner.getHeight()));

        auto x = inner.getX();

        for (auto* knob : envelopeKnobs)
        {
            knob->setBounds (x, inner.getY(), knobWidth, knobHeight);
            x += knobWidth;
        }
    }
}

void InstrumentsPage::paint (juce::Graphics& g)
{
    // Engine selector strip
    {
        auto area = headerBounds.reduced (2, 0);

        g.setColour (theme::textFaint);
        g.setFont (theme::boldFont (9.5f));
        g.drawText ("ENGINE", area.removeFromLeft (60),
                    juce::Justification::centredLeft, false);

        area.removeFromLeft (350);

        g.setColour (engineStatusText.startsWith ("NO SAMPLE") ? theme::pending : theme::textDim);
        g.setFont (theme::boldFont (10.0f));
        g.drawFittedText (engineStatusText, area, juce::Justification::centredLeft, 1);
    }

    theme::drawCard (g, sampleBounds, "SAMPLE MAP / SFZ  ·  WAV / AIFF / JSON / SFZ");

    if (processor.getLoadedInstrument() == nullptr)
    {
        g.setColour (theme::textFaint);
        g.setFont (theme::font (9.5f));
        g.drawText ("One WAV/AIFF = one zone across the whole keyboard. A JSON map adds "
                    "key ranges, velocity layers and round robin.",
                    sampleBounds.reduced (18, 0).removeFromBottom (46),
                    juce::Justification::bottomLeft, false);
    }

    theme::drawCard (g, synthBounds, "SYNTH ENGINE  ·  2 OSC + SUB + FILTER ENV");

    {
        g.setColour (theme::textFaint);
        g.setFont (theme::font (9.5f));
        g.drawText ("WAVE", synthBounds.reduced (18, 0).withTrimmedTop (36).withHeight (26),
                    juce::Justification::centredLeft, false);
    }

    theme::drawCard (g, envelopeBounds, "VOICE ENVELOPE");
}
}
