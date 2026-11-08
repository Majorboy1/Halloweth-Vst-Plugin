#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace halloweth
{
class HallowethAudioProcessor;

/** Sound library: grouped browsing, favourites and preset management in one place.

    Left  - groups (Favourites, All Sounds, then every category with a count)
    Top   - free-text search
    Centre- the filtered sound list, with a clickable favourite star per row
    Right - details and actions (load, save current, delete, import, export) */
class LibraryPage : public juce::Component
{
public:
    explicit LibraryPage (HallowethAudioProcessor&);
    ~LibraryPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Rebuilds groups and the list; called when the page is shown. */
    void refresh();

private:
    struct Group
    {
        juce::String label;
        juce::String category;   // empty for the special groups
        bool favouritesOnly = false;
        bool allSounds = false;
    };

    struct GroupModel : public juce::ListBoxModel
    {
        explicit GroupModel (LibraryPage& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRowSelected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;

        LibraryPage& owner;
    };

    struct SoundModel : public juce::ListBoxModel
    {
        explicit SoundModel (LibraryPage& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRowSelected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;

        LibraryPage& owner;
    };

    void rebuildGroups();
    void rebuildList();
    void selectGroup (int groupIndex);
    void loadSelected();
    void savePreset();
    void deleteSelected();
    void toggleFavourite (int presetIndex);
    void importPreset();
    void exportSelected();
    void updateDetails();
    int getSelectedSoundIndex() const;
    juce::Rectangle<int> getStarBounds (int width, int height) const;

    HallowethAudioProcessor& processor;

    GroupModel groupModel { *this };
    SoundModel soundModel { *this };

    juce::ListBox groupList { "groups", &groupModel };
    juce::ListBox soundList { "sounds", &soundModel };

    juce::TextEditor searchBox, nameEditor;
    juce::TextButton loadButton, saveButton, deleteButton, favouriteButton,
                     importButton, exportButton;
    juce::Label detailNameLabel, detailMetaLabel, detailDescriptionLabel, statusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    std::vector<Group> groups;
    std::vector<int> filtered;

    int selectedGroup = 1;   // defaults to "All Sounds"

    juce::Rectangle<int> groupBounds, listBounds, detailBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryPage)
};
}
