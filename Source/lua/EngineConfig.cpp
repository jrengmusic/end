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
#include "../AppIdentifier.h"

namespace lua
{
/*____________________________________________________________________________*/

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
    jassert (model != nullptr);

    auto readColour = [this] (const juce::Identifier& propId, juce::Colour fallback) -> juce::Colour
    {
        const int raw { model->getValue<int> (app::id::DISPLAY_LUA, propId) };
        return raw != 0 ? juce::Colour (static_cast<juce::uint32> (raw)) : fallback;
    };

    Theme theme;
    theme.defaultForeground     = readColour (app::id::foreground,            juce::Colour (0xffa1d6e5));
    theme.defaultBackground     = readColour (app::id::background,            juce::Colour (0x00000000));
    theme.selectionColour       = readColour (app::id::selectionColour,       juce::Colour (0x2000ddee));
    theme.selectionCursorColour = readColour (app::id::selectionCursorColour, juce::Colour (0xff00ddee));
    theme.cursorColour          = readColour (app::id::cursorColour,          juce::Colour (0xff4e8c93));
    theme.hintLabelBg           = readColour (app::id::hintLabelBg,           juce::Colour (0xff00ffff));
    theme.hintLabelFg           = readColour (app::id::hintLabelFg,           juce::Colour (0xff111111));
    theme.cursorCodepoint       = static_cast<char32_t> (model->getValue<int> (app::id::DISPLAY_LUA, app::id::cursorCodepoint));
    theme.cursorForce           = model->getValue<int> (app::id::DISPLAY_LUA, app::id::cursorForce) != 0;

    static const std::array<const juce::Identifier*, 16> ansiIds {{
        &app::id::ansi0,  &app::id::ansi1,  &app::id::ansi2,  &app::id::ansi3,
        &app::id::ansi4,  &app::id::ansi5,  &app::id::ansi6,  &app::id::ansi7,
        &app::id::ansi8,  &app::id::ansi9,  &app::id::ansi10, &app::id::ansi11,
        &app::id::ansi12, &app::id::ansi13, &app::id::ansi14, &app::id::ansi15
    }};

    static const std::array<juce::uint32, 16> ansiDefaults {{
        0xff090d12, 0xfffc704c, 0xffc5f0e9, 0xfff3f5c5,
        0xff8cc9d9, 0xff519299, 0xff699daa, 0xffdddddd,
        0xff33535b, 0xfffc704c, 0xffbafffd, 0xfffeffd2,
        0xff67dfef, 0xff01c2d2, 0xff00c8d8, 0xffbafffd
    }};

    for (size_t i { 0 }; i < 16; ++i)
        theme.ansi.at (i) = readColour (*ansiIds.at (i), juce::Colour (ansiDefaults.at (i)));

    return theme;
}

float Engine::dpiCorrectedFontSize() const noexcept
{
    jassert (model != nullptr);
    float corrected { model->getValue<float> (app::id::DISPLAY_LUA, app::id::fontSize) };

#if JUCE_WINDOWS
    if (model->getValue<int> (app::id::DISPLAY_LUA, app::id::fontDesktopScale) == 0)
    {
        const float scale { jam::Typeface::getDisplayScale() };

        if (scale > 0.0f)
            corrected = corrected / scale;
    }
#endif

    return corrected;
}

juce::String Engine::getHandler (const juce::String& extension) const noexcept
{
    juce::String result;
    const auto it { handlers.find (extension) };

    if (it != handlers.end())
        result = it->second;

    return result;
}

bool Engine::isClickableExtension (const juce::String& extension) const noexcept
{
    return extensions.count (extension) > 0
        or handlers.count (extension) > 0;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
