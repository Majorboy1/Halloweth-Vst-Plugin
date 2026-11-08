#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace halloweth
{
class HallowethAudioProcessor;
class LabeledKnob;

/** Instruments page for the sample-map, SFZ and synth engines.

    Every control acts on the running engine. */
class InstrumentsPage : public juce::Component
{
public:
    explicit InstrumentsPage (HallowethAudioProcessor&);
    ~InstrumentsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called from the editor timer; refreshes labels when a load completes. */
    void pollProcessor();

private:
    void chooseSampleFile();
    void chooseMapFile();
    void chooseSfzFile();
    void refresh();
    void setEngineMode (int mode);

    HallowethAudioProcessor& processor;

    juce::OwnedArray<LabeledKnob> envelopeKnobs;
    juce::OwnedArray<LabeledKnob> synthKnobs;

    juce::TextButton engineSampleButton, engineSynthButton, engineSfzButton;
    juce::TextButton loadFileButton, loadMapButton, loadSfzButton, clearButton, loopButton;
    juce::ComboBox waveCombo;

    juce::Label nameLabel, detailsLabel, statusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;

    juce::Rectangle<int> headerBounds, sampleBounds, synthBounds, envelopeBounds;
    juce::String engineStatusText;
    int lastRevision = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentsPage)
};
}
