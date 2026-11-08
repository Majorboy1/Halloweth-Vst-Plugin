#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace halloweth
{
class HallowethAudioProcessor;

/** Drums page: 16 pads, per-pad level/pan/tuning and kit load/save.

    Pads trigger MIDI notes 36-51 through the shared keyboard state, so clicking a
    pad and playing that note from a DAW go through exactly the same code path. */
class DrumsPage : public juce::Component
{
public:
    explicit DrumsPage (HallowethAudioProcessor&);
    ~DrumsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void pollProcessor();

private:
    void triggerPad (int padIndex);
    void loadPadFile();
    void clearSelectedPad();
    void loadKitFile();
    void saveKitFile();
    void selectPad (int padIndex);
    void refreshPadValues();

    HallowethAudioProcessor& processor;

    juce::OwnedArray<juce::TextButton> pads;
    juce::TextButton loadKitButton, saveKitButton;
    juce::TextButton loadPadButton, clearPadButton;

    juce::Label kitLabel, statusLabel, padNameLabel;
    juce::Slider levelSlider, panSlider, pitchSlider;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> gridBounds, editorBounds, kitBarBounds;
    int selectedPad = 0;
    int lastRevision = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumsPage)
};
}
