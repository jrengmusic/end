/**
 * @file EngineConfig.cpp
 * @brief Domain utility methods for lua::Engine.
 *
 * Contains: Engine::parseColour(), Engine::buildTheme(),
 * Engine::dpiCorrectedFontSize(), Engine::getHandler(),
 * Engine::isClickableExtension().
 *
 * @see lua::Engine
 */

#include "Engine.h"

namespace lua
{

//==============================================================================
juce::Colour Engine::parseColour (const juce::String& input)
{
    const juce::String trimmed { input.trim() };
    juce::Colour result { juce::Colours::magenta };

    if (trimmed.startsWithIgnoreCase ("rgba"))
    {
        const int open  { trimmed.indexOfChar ('(') };
        const int close { trimmed.indexOfChar (')') };

        if (open >= 0 and close > open)
        {
            juce::StringArray parts;
            parts.addTokens (trimmed.substring (open + 1, close), ",", "");
            parts.trim();

            if (parts.size() == 4)
            {
                const uint8_t r { static_cast<uint8_t> (juce::jlimit (0, 255, parts[0].getIntValue())) };
                const uint8_t g { static_cast<uint8_t> (juce::jlimit (0, 255, parts[1].getIntValue())) };
                const uint8_t b { static_cast<uint8_t> (juce::jlimit (0, 255, parts[2].getIntValue())) };
                const uint8_t a { static_cast<uint8_t> (
                    juce::jlimit (0, 255, juce::roundToInt (parts[3].getFloatValue() * 255.0f))) };
                result = juce::Colour (r, g, b, a);
            }
            else
            {
                jassertfalse; // rgba() with wrong component count
            }
        }
        else
        {
            jassertfalse; // malformed rgba() — missing parentheses
        }
    }
    else if (trimmed.startsWithChar ('#'))
    {
        const juce::String hex { trimmed.substring (1) };

        if (hex.length() == 3)
        {
            // #RGB — each nibble expanded to two digits (× 17)
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 1).getHexValue32() * 17) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (1, 2).getHexValue32() * 17) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (2, 3).getHexValue32() * 17) };
            result = juce::Colour (r, g, b);
        }
        else if (hex.length() == 4)
        {
            // #RGBA — each nibble expanded to two digits (× 17)
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 1).getHexValue32() * 17) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (1, 2).getHexValue32() * 17) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (2, 3).getHexValue32() * 17) };
            const uint8_t a { static_cast<uint8_t> (hex.substring (3, 4).getHexValue32() * 17) };
            result = juce::Colour (r, g, b, a);
        }
        else if (hex.length() == 6)
        {
            // #RRGGBB — fully opaque
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            result = juce::Colour (r, g, b);
        }
        else if (hex.length() == 8)
        {
            // #RRGGBBAA — alpha in low byte
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            const uint8_t a { static_cast<uint8_t> (hex.substring (6, 8).getHexValue32()) };
            result = juce::Colour (r, g, b, a);
        }
        else
        {
            jassertfalse; // unrecognised # hex length
        }
    }
    else if (trimmed.length() == 6)
    {
        // Bare RRGGBB — fully opaque (Whelmed format)
        const uint8_t r { static_cast<uint8_t> (trimmed.substring (0, 2).getHexValue32()) };
        const uint8_t g { static_cast<uint8_t> (trimmed.substring (2, 4).getHexValue32()) };
        const uint8_t b { static_cast<uint8_t> (trimmed.substring (4, 6).getHexValue32()) };
        result = juce::Colour (r, g, b);
    }
    else if (trimmed.length() == 8)
    {
        // Bare RRGGBBAA (Whelmed format)
        const uint8_t r { static_cast<uint8_t> (trimmed.substring (0, 2).getHexValue32()) };
        const uint8_t g { static_cast<uint8_t> (trimmed.substring (2, 4).getHexValue32()) };
        const uint8_t b { static_cast<uint8_t> (trimmed.substring (4, 6).getHexValue32()) };
        const uint8_t a { static_cast<uint8_t> (trimmed.substring (6, 8).getHexValue32()) };
        result = juce::Colour (r, g, b, a);
    }
    else
    {
        jassertfalse; // unrecognised colour format
    }

    return result;
}

//==============================================================================
Engine::Theme Engine::buildTheme() const
{
    Theme theme;
    theme.defaultForeground     = display.colours.foreground;
    theme.defaultBackground     = display.colours.background;
    theme.selectionColour       = display.colours.selection;
    theme.selectionCursorColour = display.colours.selectionCursor;
    theme.cursorColour          = display.colours.cursor;
    theme.cursorCodepoint       = display.cursor.codepoint;
    theme.cursorForce           = display.cursor.force;
    theme.hintLabelBg           = display.colours.hintLabelBg;
    theme.hintLabelFg           = display.colours.hintLabelFg;
    theme.ansi                  = display.colours.ansi;
    return theme;
}

float Engine::dpiCorrectedFontSize() const noexcept
{
    float corrected { display.font.size };

#if JUCE_WINDOWS
    if (not display.font.desktopScale)
    {
        const float scale { jam::Typeface::getDisplayScale() };

        if (scale > 0.0f)
            corrected = display.font.size / scale;
    }
#endif

    return corrected;
}

juce::String Engine::getHandler (const juce::String& extension) const noexcept
{
    juce::String result;
    const auto it { nexus.hyperlinks.handlers.find (extension) };

    if (it != nexus.hyperlinks.handlers.end())
        result = it->second;

    return result;
}

bool Engine::isClickableExtension (const juce::String& extension) const noexcept
{
    return nexus.hyperlinks.extensions.count (extension) > 0
        or nexus.hyperlinks.handlers.count (extension) > 0;
}

} // namespace lua
