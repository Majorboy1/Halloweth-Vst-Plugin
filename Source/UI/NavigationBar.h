#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace halloweth
{
/** Left-hand navigation rail. Switching pages is real: the editor swaps the
    visible content page in response. */
class NavigationBar : public juce::Component
{
public:
    enum class Page
    {
        home = 0,
        instruments,
        drums,
        vocals,
        library,
        effects,
        settings,
        numPages
    };

    NavigationBar();

    void setSelectedPage (Page page, juce::NotificationType notification = juce::sendNotificationSync);
    Page getSelectedPage() const noexcept { return currentPage; }

    static juce::String getNameForPage (Page page);
    static int toIndex (Page page) noexcept { return (int) page; }

    /** Fired when the user clicks a navigation entry. */
    std::function<void (Page)> onPageSelected;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int logoHeight = 108;
    static constexpr int footerHeight = 52;

    juce::OwnedArray<juce::TextButton> buttons;
    Page currentPage = Page::home;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NavigationBar)
};
}
