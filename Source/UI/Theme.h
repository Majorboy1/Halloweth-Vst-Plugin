#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Shared visual language for HALLOWETH: charcoal base, violet accent, white type. */
namespace halloweth::theme
{
    inline const juce::Colour background     { 0xff08080c };
    inline const juce::Colour backgroundAlt  { 0xff0e0e16 };
    inline const juce::Colour panel          { 0xff12121b };
    inline const juce::Colour panelRaised    { 0xff191924 };
    inline const juce::Colour outline        { 0xff262636 };
    inline const juce::Colour accent         { 0xff8b5cf6 };
    inline const juce::Colour accentBright   { 0xffb388ff };
    inline const juce::Colour accentDeep     { 0xff4c1d95 };
    inline const juce::Colour text           { 0xfff4f4f9 };
    inline const juce::Colour textDim        { 0xff8f8fa6 };
    inline const juce::Colour textFaint      { 0xff5a5a72 };
    inline const juce::Colour success        { 0xff4ade80 };
    inline const juce::Colour pending        { 0xffe0a44a };

    inline juce::Font font (float height, int styleFlags = juce::Font::plain)
    {
        return juce::Font (juce::FontOptions (height, styleFlags));
    }

    inline juce::Font boldFont (float height)
    {
        return font (height, juce::Font::bold);
    }

    /** Draws the standard HALLOWETH card panel with an optional caption. */
    inline void drawCard (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title = {})
    {
        const auto area = bounds.toFloat();

        g.setColour (panel);
        g.fillRoundedRectangle (area, 10.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

        if (title.isNotEmpty())
        {
            g.setColour (textDim);
            g.setFont (boldFont (10.5f));
            g.drawText (title, bounds.reduced (18, 12).removeFromTop (14),
                        juce::Justification::centredLeft, false);
        }
    }
}
