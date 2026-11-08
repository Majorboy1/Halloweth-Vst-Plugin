#include "LibraryPage.h"

#include "../../PluginProcessor.h"
#include "../Theme.h"

namespace halloweth
{
namespace
{
    const juce::String filledStar = juce::String::fromUTF8 ("\xe2\x98\x85");
    const juce::String emptyStar  = juce::String::fromUTF8 ("\xe2\x98\x86");
}

//==============================================================================
LibraryPage::LibraryPage (HallowethAudioProcessor& p) : processor (p)
{
    groupList.setRowHeight (30);
    groupList.setColour (juce::ListBox::backgroundColourId, theme::panelRaised);
    groupList.setColour (juce::ListBox::outlineColourId, theme::outline);
    groupList.setOutlineThickness (1);
    addAndMakeVisible (groupList);

    soundList.setRowHeight (44);
    soundList.setColour (juce::ListBox::backgroundColourId, theme::panelRaised);
    soundList.setColour (juce::ListBox::outlineColourId, theme::outline);
    soundList.setOutlineThickness (1);
    addAndMakeVisible (soundList);

    searchBox.setTextToShowWhenEmpty ("Search sounds...", theme::textFaint);
    searchBox.setColour (juce::TextEditor::backgroundColourId, theme::panelRaised);
    searchBox.setColour (juce::TextEditor::outlineColourId, theme::outline);
    searchBox.onTextChange = [this] { rebuildList(); };
    addAndMakeVisible (searchBox);

    nameEditor.setTextToShowWhenEmpty ("Name for SAVE CURRENT", theme::textFaint);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, theme::panelRaised);
    nameEditor.setColour (juce::TextEditor::outlineColourId, theme::outline);
    addAndMakeVisible (nameEditor);

    detailNameLabel.setFont (theme::boldFont (17.0f));
    detailNameLabel.setColour (juce::Label::textColourId, theme::text);
    detailNameLabel.setJustificationType (juce::Justification::topLeft);
    detailNameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (detailNameLabel);

    detailMetaLabel.setFont (theme::boldFont (9.5f));
    detailMetaLabel.setColour (juce::Label::textColourId, theme::accentBright);
    detailMetaLabel.setJustificationType (juce::Justification::topLeft);
    detailMetaLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (detailMetaLabel);

    detailDescriptionLabel.setFont (theme::font (10.5f));
    detailDescriptionLabel.setColour (juce::Label::textColourId, theme::textDim);
    detailDescriptionLabel.setJustificationType (juce::Justification::topLeft);
    detailDescriptionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (detailDescriptionLabel);

    statusLabel.setFont (theme::font (10.0f));
    statusLabel.setColour (juce::Label::textColourId, theme::textFaint);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    auto setUpButton = [this] (juce::TextButton& button, const juce::String& text,
                               std::function<void()> action)
    {
        button.setButtonText (text);
        button.onClick = std::move (action);
        addAndMakeVisible (button);
    };

    setUpButton (loadButton, "LOAD", [this] { loadSelected(); });
    setUpButton (favouriteButton, "FAVOURITE", [this]
    {
        const auto index = getSelectedSoundIndex();

        if (index >= 0)
            toggleFavourite (index);
    });
    setUpButton (saveButton, "SAVE CURRENT", [this] { savePreset(); });
    setUpButton (deleteButton, "DELETE", [this] { deleteSelected(); });
    setUpButton (importButton, "IMPORT...", [this] { importPreset(); });
    setUpButton (exportButton, "EXPORT...", [this] { exportSelected(); });

    rebuildGroups();
    selectGroup (1);
}

LibraryPage::~LibraryPage() = default;

void LibraryPage::refresh()
{
    rebuildGroups();
    rebuildList();
    updateDetails();
}

void LibraryPage::rebuildGroups()
{
    groups.clear();

    int favouriteCount = 0;

    for (int i = 0; i < processor.getNumPresets(); ++i)
        if (processor.isPresetFavourite (i))
            ++favouriteCount;

    Group favourites;
    favourites.label = filledStar + "  Favourites  (" + juce::String (favouriteCount) + ")";
    favourites.favouritesOnly = true;
    groups.push_back (favourites);

    Group all;
    all.label = "All Sounds  (" + juce::String (processor.getNumPresets()) + ")";
    all.allSounds = true;
    groups.push_back (all);

    juce::StringArray categories;
    juce::Array<int> counts;

    for (int i = 0; i < processor.getNumPresets(); ++i)
    {
        const auto& info = processor.getPresetInfo (i);
        const auto category = info.category.isEmpty() ? juce::String ("Other") : info.category;

        const auto existing = categories.indexOf (category);

        if (existing < 0)
        {
            categories.add (category);
            counts.add (1);
        }
        else
        {
            counts.set (existing, counts[existing] + 1);
        }
    }

    // Sort categories alphabetically while keeping the counts aligned.
    juce::Array<int> order;

    for (int i = 0; i < categories.size(); ++i)
        order.add (i);

    std::sort (order.begin(), order.end(), [&categories] (int a, int b)
    {
        return categories[a].compareIgnoreCase (categories[b]) < 0;
    });

    for (auto index : order)
    {
        Group group;
        group.category = categories[index];
        group.label = group.category + "  (" + juce::String (counts[index]) + ")";
        groups.push_back (group);
    }

    selectedGroup = juce::jlimit (0, (int) groups.size() - 1, selectedGroup);
    groupList.updateContent();

    if (groupList.getSelectedRow() != selectedGroup)
        groupList.selectRow (selectedGroup, true);
}

void LibraryPage::selectGroup (int groupIndex)
{
    selectedGroup = juce::jlimit (0, juce::jmax (0, (int) groups.size() - 1), groupIndex);
    rebuildList();
}

void LibraryPage::rebuildList()
{
    filtered.clear();

    const auto search = searchBox.getText().trim().toLowerCase();
    const Group* group = juce::isPositiveAndBelow (selectedGroup, (int) groups.size())
                             ? &groups[(size_t) selectedGroup]
                             : nullptr;

    for (int i = 0; i < processor.getNumPresets(); ++i)
    {
        const auto& info = processor.getPresetInfo (i);

        if (group != nullptr)
        {
            if (group->favouritesOnly && ! processor.isPresetFavourite (i))
                continue;

            if (group->category.isNotEmpty() && info.category != group->category)
                continue;
        }

        if (search.isNotEmpty()
            && ! info.name.toLowerCase().contains (search)
            && ! info.category.toLowerCase().contains (search)
            && ! info.genre.toLowerCase().contains (search)
            && ! info.description.toLowerCase().contains (search))
            continue;

        filtered.push_back (i);
    }

    soundList.updateContent();
    soundList.deselectAllRows();

    statusLabel.setText (juce::String ((int) filtered.size()) + " sound"
                             + ((int) filtered.size() == 1 ? "" : "s") + " shown",
                         juce::dontSendNotification);

    updateDetails();
}

int LibraryPage::getSelectedSoundIndex() const
{
    const auto row = soundList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, (int) filtered.size()))
        return -1;

    return filtered[(size_t) row];
}

juce::Rectangle<int> LibraryPage::getStarBounds (int width, int height) const
{
    return { width - 44, 0, 36, height };
}

//==============================================================================
int LibraryPage::GroupModel::getNumRows()
{
    return (int) owner.groups.size();
}

void LibraryPage::GroupModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                                bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.groups.size()))
        return;

    if (selected)
    {
        g.setColour (theme::accent.withAlpha (0.20f));
        g.fillRect (0, 0, width, height);

        g.setColour (theme::accentBright);
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 6.0f, 2.5f,
                                                        (float) height - 12.0f), 1.25f);
    }

    const auto& group = owner.groups[(size_t) row];

    g.setColour (selected ? theme::text : theme::textDim);
    g.setFont (selected ? theme::boldFont (11.5f) : theme::font (11.5f));
    g.drawText (group.label, 14, 0, width - 20, height,
                juce::Justification::centredLeft, true);
}

void LibraryPage::GroupModel::selectedRowsChanged (int lastRowSelected)
{
    owner.selectGroup (lastRowSelected);
}

void LibraryPage::GroupModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    owner.selectGroup (row);
}

//==============================================================================
int LibraryPage::SoundModel::getNumRows()
{
    return (int) owner.filtered.size();
}

void LibraryPage::SoundModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                                bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.filtered.size()))
        return;

    const auto presetIndex = owner.filtered[(size_t) row];
    const auto& info = owner.processor.getPresetInfo (presetIndex);

    if (selected)
    {
        g.setColour (theme::accent.withAlpha (0.18f));
        g.fillRect (0, 0, width, height);
    }

    const auto isCurrent = presetIndex == owner.processor.getCurrentPresetIndex();

    g.setColour (isCurrent ? theme::accentBright : theme::text);
    g.setFont (theme::boldFont (12.5f));
    g.drawText (info.name, 12, 5, width - 170, 18,
                juce::Justification::centredLeft, true);

    juce::String meta;
    meta << info.category;

    if (info.genre.isNotEmpty())
        meta << "  ·  " << info.genre;

    g.setColour (theme::textFaint);
    g.setFont (theme::font (9.5f));
    g.drawText (meta, 12, height - 20, width - 170, 14,
                juce::Justification::centredLeft, true);

    if (info.requiresExternalAudio())
    {
        g.setColour (theme::pending.withAlpha (0.9f));
        g.setFont (theme::boldFont (8.5f));
        g.drawText ("NEEDS SAMPLE", width - 160, 0, 108, height,
                    juce::Justification::centredRight, false);
    }

    // Always draw a star so favourites are obvious and clickable.
    const auto starBounds = owner.getStarBounds (width, height);
    const auto isFavourite = owner.processor.isPresetFavourite (presetIndex);

    g.setColour (isFavourite ? theme::accentBright : theme::textFaint.withAlpha (0.55f));
    g.setFont (theme::font (16.0f));
    g.drawText (isFavourite ? filledStar : emptyStar, starBounds,
                juce::Justification::centred, false);
}

void LibraryPage::SoundModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    owner.loadSelected();
}

void LibraryPage::SoundModel::listBoxItemClicked (int row, const juce::MouseEvent& event)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.filtered.size()))
        return;

    const auto starBounds = owner.getStarBounds (owner.soundList.getWidth(),
                                                 owner.soundList.getRowHeight());

    if (starBounds.contains (event.getPosition()))
        owner.toggleFavourite (owner.filtered[(size_t) row]);

    owner.updateDetails();
}

void LibraryPage::SoundModel::selectedRowsChanged (int)
{
    owner.updateDetails();
}

//==============================================================================
void LibraryPage::toggleFavourite (int presetIndex)
{
    processor.togglePresetFavourite (presetIndex);
    rebuildGroups();
    soundList.repaint();
    updateDetails();
}

void LibraryPage::updateDetails()
{
    const auto index = getSelectedSoundIndex();

    if (index < 0)
    {
        detailNameLabel.setText ("Select a sound", juce::dontSendNotification);
        detailMetaLabel.setText ("", juce::dontSendNotification);
        detailDescriptionLabel.setText ("Double-click a sound, or press LOAD, to play it.",
                                        juce::dontSendNotification);
        return;
    }

    const auto& info = processor.getPresetInfo (index);

    detailNameLabel.setText (info.name, juce::dontSendNotification);

    juce::String meta;
    meta << info.category.toUpperCase();

    if (info.genre.isNotEmpty())
        meta << "  ·  " << info.genre.toUpperCase();

    if (info.requiresExternalAudio())
        meta << "  ·  NEEDS YOUR SAMPLE";

    if (processor.isPresetFavourite (index))
        meta << "  ·  " << filledStar;

    detailMetaLabel.setText (meta, juce::dontSendNotification);
    detailDescriptionLabel.setText (info.description, juce::dontSendNotification);

    favouriteButton.setToggleState (processor.isPresetFavourite (index),
                                    juce::dontSendNotification);
}

void LibraryPage::loadSelected()
{
    const auto index = getSelectedSoundIndex();

    if (index < 0)
    {
        statusLabel.setText ("Select a sound first.", juce::dontSendNotification);
        return;
    }

    processor.applyPreset (index);
    soundList.repaint();
    rebuildGroups();

    const auto& info = processor.getPresetInfo (index);
    statusLabel.setText ("Loaded " + info.name
                             + (info.requiresExternalAudio()
                                    ? "  ·  load your own samples on the DRUMS page"
                                    : juce::String()),
                         juce::dontSendNotification);
}

void LibraryPage::savePreset()
{
    auto name = nameEditor.getText().trim();

    if (name.isEmpty())
        name = "My Preset";

    juce::String category = "User";

    if (juce::isPositiveAndBelow (selectedGroup, (int) groups.size())
        && groups[(size_t) selectedGroup].category.isNotEmpty())
        category = groups[(size_t) selectedGroup].category;

    processor.saveCurrentAsPreset (name, category, "User", "User preset");
    refresh();
    statusLabel.setText ("Saved \"" + name + "\" - it now appears in the library.",
                         juce::dontSendNotification);
}

void LibraryPage::deleteSelected()
{
    const auto index = getSelectedSoundIndex();

    if (index < 0)
    {
        statusLabel.setText ("Select a sound first.", juce::dontSendNotification);
        return;
    }

    if (processor.getPresetInfo (index).isFactory)
    {
        statusLabel.setText ("Factory sounds cannot be deleted.", juce::dontSendNotification);
        return;
    }

    processor.deletePreset (index);
    refresh();
    statusLabel.setText ("Sound deleted.", juce::dontSendNotification);
}

void LibraryPage::importPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import a preset (.json)",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                  {
                                      const auto ok = processor.presetManager.importPreset (file);
                                      refresh();
                                      statusLabel.setText (ok ? "Preset imported."
                                                              : "Import failed.",
                                                           juce::dontSendNotification);
                                  }
                              });
}

void LibraryPage::exportSelected()
{
    const auto index = getSelectedSoundIndex();

    if (index < 0)
    {
        statusLabel.setText ("Select a sound first.", juce::dontSendNotification);
        return;
    }

    const auto& info = processor.getPresetInfo (index);

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export preset",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory)
            .getChildFile (info.name + ".json"),
        "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, index] (const juce::FileChooser& chooser)
                              {
                                  auto file = chooser.getResult();

                                  if (file.getFullPathName().isNotEmpty())
                                  {
                                      if (! file.hasFileExtension ("json"))
                                          file = file.withFileExtension ("json");

                                      const auto ok = processor.presetManager.exportPreset (index, file);
                                      statusLabel.setText (ok ? "Preset exported."
                                                              : "Export failed.",
                                                           juce::dontSendNotification);
                                  }
                              });
}

//==============================================================================
void LibraryPage::resized()
{
    auto area = getLocalBounds();

    groupBounds = area.removeFromLeft (206);
    area.removeFromLeft (12);

    detailBounds = area.removeFromRight (juce::jmax (280, area.getWidth() * 34 / 100));
    area.removeFromRight (12);

    listBounds = area;

    auto groupInner = groupBounds.reduced (12, 14);
    groupList.setBounds (groupInner);

    auto listInner = listBounds.reduced (12, 14);
    listInner.removeFromTop (18);   // card caption
    searchBox.setBounds (listInner.removeFromTop (24));
    listInner.removeFromTop (8);
    soundList.setBounds (listInner);

    auto detailInner = detailBounds.reduced (14, 14);
    detailNameLabel.setBounds (detailInner.removeFromTop (22));
    detailMetaLabel.setBounds (detailInner.removeFromTop (14));
    detailInner.removeFromTop (8);
    detailDescriptionLabel.setBounds (detailInner.removeFromTop (54));
    detailInner.removeFromTop (10);

    nameEditor.setBounds (detailInner.removeFromTop (26));
    detailInner.removeFromTop (10);

    constexpr int buttonHeight = 30;
    constexpr int gap = 7;

    auto place = [&detailInner, buttonHeight, gap] (juce::Component& component)
    {
        component.setBounds (detailInner.removeFromTop (buttonHeight));
        detailInner.removeFromTop (gap);
    };

    place (loadButton);
    place (favouriteButton);
    place (saveButton);
    place (deleteButton);
    place (importButton);
    place (exportButton);

    statusLabel.setBounds (detailInner.removeFromTop (40));
}

void LibraryPage::paint (juce::Graphics& g)
{
    theme::drawCard (g, groupBounds, "GROUPS");
    theme::drawCard (g, listBounds);
    theme::drawCard (g, detailBounds, "SOUND");

    g.setColour (theme::textFaint);
    g.setFont (theme::font (9.0f));
    g.drawText ("Click the star to favourite  ·  double-click to load",
                listBounds.reduced (12, 14).removeFromBottom (14),
                juce::Justification::bottomRight, false);

    g.setColour (theme::textDim);
    g.setFont (theme::boldFont (10.0f));
    g.drawText ("SOUND LIBRARY", listBounds.reduced (12, 14).removeFromTop (14),
                juce::Justification::topLeft, false);
}
}
