/**
 * @file EngineDefaults.cpp
 * @brief Config-file write helpers for lua::Engine.
 *
 * Contains: Engine::writeDefaults(), all Engine::writeXxxDefaults() methods,
 * and the file-local colour formatting helpers colourToHex() and
 * colourToWhelmedHex(). Default values are expressed as literals here — the
 * deleted struct brace-inits that previously held them are gone.
 *
 * @see lua::Engine
 */

#include "Engine.h"
#include "../AppIdentifier.h"
#include "../Map.h"

namespace lua
{
/*____________________________________________________________________________*/

//==============================================================================
/** @brief Converts a juce::Colour to "#RRGGBBAA" for end.lua templates. Always 8 hex digits. */
static juce::String colourToHex (juce::Colour c)
{
    const auto aarrggbb { c.toString().paddedLeft ('0', 8) };// JUCE format: AARRGGBB, padded
    return "#" + aarrggbb.substring (2) + aarrggbb.substring (0, 2);// Rearrange to #RRGGBBAA
}

/** @brief Converts a juce::Colour to "RRGGBBAA" (no #) for whelmed.lua templates. */
static juce::String colourToWhelmedHex (juce::Colour c)
{
    const auto aarrggbb { c.toString() };// JUCE format: AARRGGBB
    return aarrggbb.substring (2) + aarrggbb.substring (0, 2);// RRGGBBAA, no #
}

//==============================================================================
/** @brief Substitutes font and cursor placeholder values into display.lua content. */
static void writeDisplayFontDefaults (juce::String& content)
{
    content = jam::Text::replaceholder (content, "font_family",     "Display Mono");
    content = jam::Text::replaceholder (content, "font_size",       "12");
    content = jam::Text::replaceholder (content, "font_ligatures",  "true");
    content = jam::Text::replaceholder (content, "font_embolden",   "true");
    content = jam::Text::replaceholder (content, "font_line_height", "1");
    content = jam::Text::replaceholder (content, "font_cell_width",  "1");
    content = jam::Text::replaceholder (content, "font_desktop_scale", "false");

    // Cursor char is stored as a unicode codepoint; emit as UTF-8 string.
    const auto cursorChar { juce::String::charToString (jam::toChar (static_cast<char32_t> (0x2588u))) };
    content = jam::Text::replaceholder (content, "cursor_char",           cursorChar);
    content = jam::Text::replaceholder (content, "cursor_blink",          "true");
    content = jam::Text::replaceholder (content, "cursor_blink_interval", "500");
    content = jam::Text::replaceholder (content, "cursor_force",          "false");
    content = jam::Text::replaceholder (content, "cursor_style",          Map::Cursor::getContext()->get (Map::Cursor::block));
}

/** @brief Substitutes colour palette placeholder values into display.lua content. */
static void writeDisplayColoursDefaults (juce::String& content)
{
    content = jam::Text::replaceholder (content, "colours_foreground",          colourToHex (juce::Colour (0xffa1d6e5)));
    content = jam::Text::replaceholder (content, "colours_background",          colourToHex (juce::Colour (0x00000000)));
    content = jam::Text::replaceholder (content, "colours_cursor",              colourToHex (juce::Colour (0xff4e8c93)));
    content = jam::Text::replaceholder (content, "colours_selection",           colourToHex (juce::Colour (0x2000ddee)));
    content = jam::Text::replaceholder (content, "colours_selection_cursor",    colourToHex (juce::Colour (0xff00ddee)));

    static const std::array<const char*, 16> ansiKeys {
        { "colours_black",
         "colours_red", "colours_green",
         "colours_yellow", "colours_blue",
         "colours_magenta", "colours_cyan",
         "colours_white", "colours_bright_black",
         "colours_bright_red", "colours_bright_green",
         "colours_bright_yellow", "colours_bright_blue",
         "colours_bright_magenta", "colours_bright_cyan",
         "colours_bright_white" }
    };

    static const std::array<juce::uint32, 16> ansiDefaults {{
        0xff090d12, 0xfffc704c, 0xffc5f0e9, 0xfff3f5c5,
        0xff8cc9d9, 0xff519299, 0xff699daa, 0xffdddddd,
        0xff33535b, 0xfffc704c, 0xffbafffd, 0xfffeffd2,
        0xff67dfef, 0xff01c2d2, 0xff00c8d8, 0xffbafffd
    }};

    for (size_t i { 0 }; i < 16; ++i)
        content = jam::Text::replaceholder (content, ansiKeys.at (i), colourToHex (juce::Colour (ansiDefaults.at (i))));

    content = jam::Text::replaceholder (content, "colours_status_bar",           colourToHex (juce::Colour (0xff090d12)));
    content = jam::Text::replaceholder (content, "colours_status_bar_label_bg",  colourToHex (juce::Colour (0xff112130)));
    content = jam::Text::replaceholder (content, "colours_status_bar_label_fg",  colourToHex (juce::Colour (0xff4e8c93)));
    content = jam::Text::replaceholder (content, "colours_status_bar_spinner",   colourToHex (juce::Colour (0xff00c8d8)));
    content = jam::Text::replaceholder (content, "colours_hint_label_bg",        colourToHex (juce::Colour (0xff00ffff)));
    content = jam::Text::replaceholder (content, "colours_hint_label_fg",        colourToHex (juce::Colour (0xff111111)));
    content = jam::Text::replaceholder (content, "colours_editor_background",    colourToHex (juce::Colour (0x00000000)));
    content = jam::Text::replaceholder (content, "colours_editor_outline",       colourToHex (juce::Colour (0x00000000)));
    content = jam::Text::replaceholder (content, "colours_scrollbar_thumb",      colourToHex (juce::Colour (0x802c4144)));
    content = jam::Text::replaceholder (content, "colours_scrollbar_track",      colourToHex (juce::Colour (0x00000000)));
}

/** @brief Substitutes window placeholder values into display.lua content. */
static void writeDisplayWindowDefaults (juce::String& content)
{
    juce::String windowTitle { ProjectInfo::projectName };
    windowTitle = windowTitle.replace ("\\", "\\\\");
    content = jam::Text::replaceholder (content, "window_title",               windowTitle);
    content = jam::Text::replaceholder (content, "window_width",               "640");
    content = jam::Text::replaceholder (content, "window_height",              "480");
    content = jam::Text::replaceholder (content, "window_colour",              colourToHex (juce::Colour (0xff090d12)));
    content = jam::Text::replaceholder (content, "window_opacity",             "0.75");
    content = jam::Text::replaceholder (content, "window_blur_radius",         "32");
    content = jam::Text::replaceholder (content, "window_always_on_top",       "false");
    content = jam::Text::replaceholder (content, "window_buttons",             "false");
    content = jam::Text::replaceholder (content, "window_force_dwm",           "true");
    content = jam::Text::replaceholder (content, "window_save_size",           "true");
    content = jam::Text::replaceholder (content, "window_confirmation_on_exit", "true");
}

/** @brief Substitutes tab placeholder values into display.lua content. */
static void writeDisplayTabDefaults (juce::String& content)
{
    content = jam::Text::replaceholder (content, "tab_family",     "Display Mono");
    content = jam::Text::replaceholder (content, "tab_size",       "12");
    content = jam::Text::replaceholder (content, "tab_foreground", colourToHex (juce::Colour (0xff00c8d8)));
    content = jam::Text::replaceholder (content, "tab_inactive",   colourToHex (juce::Colour (0xff33535b)));
    content = jam::Text::replaceholder (content, "tab_position",   "left");
    content = jam::Text::replaceholder (content, "tab_line",       colourToHex (juce::Colour (0xff2c4144)));
    content = jam::Text::replaceholder (content, "tab_active",     colourToHex (juce::Colour (0xff002b35)));
    content = jam::Text::replaceholder (content, "tab_indicator",  colourToHex (juce::Colour (0xff01c2d2)));
}

/** @brief Substitutes overlay, pane, menu, status bar, action list, and popup placeholder values. */
static void writeDisplayMiscDefaults (juce::String& content)
{
    // Menu
    content = jam::Text::replaceholder (content, "menu_opacity", "0.65");

    // Overlay
    content = jam::Text::replaceholder (content, "overlay_family", "Display Mono");
    content = jam::Text::replaceholder (content, "overlay_size",   "14");
    content = jam::Text::replaceholder (content, "overlay_colour", colourToHex (juce::Colour (0xff4e8c93)));

    // Pane
    content = jam::Text::replaceholder (content, "pane_bar_colour",    colourToHex (juce::Colour (0xff33535b)));
    content = jam::Text::replaceholder (content, "pane_bar_highlight", colourToHex (juce::Colour (0xff4e8c93)));

    // Status bar
    content = jam::Text::replaceholder (content, "status_bar_position",    "bottom");
    content = jam::Text::replaceholder (content, "status_bar_font_family", "Display Mono");
    content = jam::Text::replaceholder (content, "status_bar_font_size",   "12");
    content = jam::Text::replaceholder (content, "status_bar_font_style",  "Bold");

    // Action list
    content = jam::Text::replaceholder (content, "action_list_position",             "top");
    content = jam::Text::replaceholder (content, "action_list_close_on_run",         "true");
    content = jam::Text::replaceholder (content, "action_list_name_font_family",     "Display");
    content = jam::Text::replaceholder (content, "action_list_name_font_style",      "Bold");
    content = jam::Text::replaceholder (content, "action_list_name_font_size",       "13");
    content = jam::Text::replaceholder (content, "action_list_shortcut_font_family", "Display Mono");
    content = jam::Text::replaceholder (content, "action_list_shortcut_font_style",  "Bold");
    content = jam::Text::replaceholder (content, "action_list_shortcut_font_size",   "12");
    content = jam::Text::replaceholder (content, "action_list_padding_top",          "10");
    content = jam::Text::replaceholder (content, "action_list_padding_right",        "10");
    content = jam::Text::replaceholder (content, "action_list_padding_bottom",       "10");
    content = jam::Text::replaceholder (content, "action_list_padding_left",         "10");
    content = jam::Text::replaceholder (content, "action_list_name_colour",          colourToHex (juce::Colour (0xffa1d6e5)));
    content = jam::Text::replaceholder (content, "action_list_shortcut_colour",      colourToHex (juce::Colour (0xff00c8d8)));
    content = jam::Text::replaceholder (content, "action_list_width",                "0.3");
    content = jam::Text::replaceholder (content, "action_list_height",               "0.3");
    content = jam::Text::replaceholder (content, "action_list_highlight_colour",     colourToHex (juce::Colour (0x2000ddee)));

    // Popup border
    content = jam::Text::replaceholder (content, "popup_border_colour", colourToHex (juce::Colour (0xff4e8c93)));
    content = jam::Text::replaceholder (content, "popup_border_width",  "1");

    // Scrollbar
    content = jam::Text::replaceholder (content, "scrollbar_width", "8");
}

//==============================================================================
void Engine::writeDefaults()
{
    auto configDir { getConfigPath() };

    if (not configDir.exists())
        configDir.createDirectory();

    writeEndDefaults (configDir);
    writeNexusDefaults (configDir);
    writeDisplayDefaults (configDir);
    writeKeysDefaults (configDir);
    writePopupsDefaults (configDir);
    writeActionsDefaults (configDir);
    writeWhelmedDefaults (configDir);
}

void Engine::writeEndDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::endLua) };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_end.lua") } };
        content = jam::Text::replaceholder (content, "versionString", ProjectInfo::versionString);
        file.replaceWithText (content);
    }
}

void Engine::writeNexusDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::nexusLua) };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_nexus.lua") } };
        content = jam::Text::replaceholder (content, "gpu",        "auto");
        content = jam::Text::replaceholder (content, "daemon",     "false");
        content = jam::Text::replaceholder (content, "auto_reload", "true");

#if JUCE_WINDOWS
        juce::String shellProgram { "powershell.exe" };
        const juce::String shellArgs;
#elif JUCE_MAC
        juce::String shellProgram { "zsh" };
        const juce::String shellArgs;
#else
        juce::String shellProgram { "bash" };
        const juce::String shellArgs { "-l" };
#endif

        shellProgram = shellProgram.replace ("\\", "\\\\");
        content = jam::Text::replaceholder (content, "shell_program",    shellProgram);
        content = jam::Text::replaceholder (content, "shell_args",       shellArgs);
        content = jam::Text::replaceholder (content, "shell_integration", "true");

        content = jam::Text::replaceholder (content, "terminal_scrollback_lines", "10000");
        content = jam::Text::replaceholder (content, "terminal_scroll_step",       "5");
        content = jam::Text::replaceholder (content, "terminal_padding_top",       "10");
        content = jam::Text::replaceholder (content, "terminal_padding_right",     "10");
        content = jam::Text::replaceholder (content, "terminal_padding_bottom",    "10");
        content = jam::Text::replaceholder (content, "terminal_padding_left",      "10");
        content = jam::Text::replaceholder (content, "terminal_drop_multifiles",   "space");
        content = jam::Text::replaceholder (content, "terminal_drop_quoted",       "true");
        content = jam::Text::replaceholder (content, "hyperlinks_editor",          "nvim");
        content = jam::Text::replaceholder (content, "image_atlas_dimension",      "4096");
        content = jam::Text::replaceholder (content, "image_cols",                 "40");
        content = jam::Text::replaceholder (content, "image_rows",                 "20");
        content = jam::Text::replaceholder (content, "image_padding",              "10");
        content = jam::Text::replaceholder (content, "image_border",               "true");
        file.replaceWithText (content);
    }
}

void Engine::writeDisplayDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::displayLua) };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_display.lua") } };

        writeDisplayFontDefaults (content);
        writeDisplayColoursDefaults (content);
        writeDisplayWindowDefaults (content);
        writeDisplayTabDefaults (content);
        writeDisplayMiscDefaults (content);

        file.replaceWithText (content);
    }
}

void Engine::writeKeysDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::keysLua) };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_keys.lua") } };

#if JUCE_MAC
        content = jam::Text::replaceholder (content, "copy",       "cmd+c");
        content = jam::Text::replaceholder (content, "paste",      "cmd+v");
        content = jam::Text::replaceholder (content, "quit",       "cmd+q");
        content = jam::Text::replaceholder (content, "close_tab",  "cmd+w");
        content = jam::Text::replaceholder (content, "reload",     "cmd+r");
        content = jam::Text::replaceholder (content, "zoom_in",    "cmd+=");
        content = jam::Text::replaceholder (content, "zoom_out",   "cmd+-");
        content = jam::Text::replaceholder (content, "zoom_reset", "cmd+0");
        content = jam::Text::replaceholder (content, "new_window", "cmd+n");
        content = jam::Text::replaceholder (content, "new_tab",    "cmd+t");
        content = jam::Text::replaceholder (content, "prev_tab",   "cmd+[");
        content = jam::Text::replaceholder (content, "next_tab",   "cmd+]");
#else
        content = jam::Text::replaceholder (content, "copy",       "ctrl+c");
        content = jam::Text::replaceholder (content, "paste",      "ctrl+v");
        content = jam::Text::replaceholder (content, "quit",       "ctrl+q");
        content = jam::Text::replaceholder (content, "close_tab",  "ctrl+w");
        content = jam::Text::replaceholder (content, "reload",     "ctrl+/");
        content = jam::Text::replaceholder (content, "zoom_in",    "ctrl+=");
        content = jam::Text::replaceholder (content, "zoom_out",   "ctrl+-");
        content = jam::Text::replaceholder (content, "zoom_reset", "ctrl+0");
        content = jam::Text::replaceholder (content, "new_window", "ctrl+n");
        content = jam::Text::replaceholder (content, "new_tab",    "ctrl+t");
        content = jam::Text::replaceholder (content, "prev_tab",   "ctrl+[");
        content = jam::Text::replaceholder (content, "next_tab",   "ctrl+]");
#endif

        file.replaceWithText (content);
    }
}

void Engine::writePopupsDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::popupsLua) };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_popups.lua") } };
        content = jam::Text::replaceholder (content, "popup_cols",     "70");
        content = jam::Text::replaceholder (content, "popup_rows",     "20");
        content = jam::Text::replaceholder (content, "popup_position", "center");
        file.replaceWithText (content);
    }
}

void Engine::writeActionsDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::actionsLua) };

    if (not file.existsAsFile())
    {
        const auto content { juce::String { BinaryData::getString ("default_actions.lua") } };
        file.replaceWithText (content);
    }
}

void Engine::writeWhelmedDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile (app::id::whelmedLua) };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_whelmed.lua") } };

        // Typography
        content = jam::Text::replaceholder (content, "font_family",  "Display");
        content = jam::Text::replaceholder (content, "font_style",   "Medium");
        content = jam::Text::replaceholder (content, "font_size",    "16");
        content = jam::Text::replaceholder (content, "code_family",  "Display Mono");
        content = jam::Text::replaceholder (content, "code_style",   "Medium");
        content = jam::Text::replaceholder (content, "code_size",    "12");
        content = jam::Text::replaceholder (content, "line_height",  "1.5");

        // Heading sizes
        content = jam::Text::replaceholder (content, "h1_size", "28");
        content = jam::Text::replaceholder (content, "h2_size", "28");
        content = jam::Text::replaceholder (content, "h3_size", "24");
        content = jam::Text::replaceholder (content, "h4_size", "20");
        content = jam::Text::replaceholder (content, "h5_size", "18");
        content = jam::Text::replaceholder (content, "h6_size", "16");

        // Layout
        content = jam::Text::replaceholder (content, "padding_top",    "10");
        content = jam::Text::replaceholder (content, "padding_right",  "10");
        content = jam::Text::replaceholder (content, "padding_bottom", "10");
        content = jam::Text::replaceholder (content, "padding_left",   "10");

        // Colours
        content = jam::Text::replaceholder (content, "background",  colourToWhelmedHex (juce::Colour (0xff0d141c)));
        content = jam::Text::replaceholder (content, "body_colour", colourToWhelmedHex (juce::Colour (0xffb3f9f5)));
        content = jam::Text::replaceholder (content, "link_colour", colourToWhelmedHex (juce::Colour (0xff01c2d2)));
        content = jam::Text::replaceholder (content, "h1_colour",   colourToWhelmedHex (juce::Colour (0xffd4c8a0)));
        content = jam::Text::replaceholder (content, "h2_colour",   colourToWhelmedHex (juce::Colour (0xffd4c8a0)));
        content = jam::Text::replaceholder (content, "h3_colour",   colourToWhelmedHex (juce::Colour (0xffd4c8a0)));
        content = jam::Text::replaceholder (content, "h4_colour",   colourToWhelmedHex (juce::Colour (0xffd4c8a0)));
        content = jam::Text::replaceholder (content, "h5_colour",   colourToWhelmedHex (juce::Colour (0xffd4c8a0)));
        content = jam::Text::replaceholder (content, "h6_colour",   colourToWhelmedHex (juce::Colour (0xffd4c8a0)));

        // Code blocks
        content = jam::Text::replaceholder (content, "code_fence_background", colourToWhelmedHex (juce::Colour (0xff090d12)));
        content = jam::Text::replaceholder (content, "code_colour",           colourToWhelmedHex (juce::Colour (0xff00d0ff)));

        // Syntax tokens
        content = jam::Text::replaceholder (content, "token_error",        colourToWhelmedHex (juce::Colour (0xfff74a4a)));
        content = jam::Text::replaceholder (content, "token_comment",      colourToWhelmedHex (juce::Colour (0xff6080c0)));
        content = jam::Text::replaceholder (content, "token_keyword",      colourToWhelmedHex (juce::Colour (0xff1919ff)));
        content = jam::Text::replaceholder (content, "token_operator",     colourToWhelmedHex (juce::Colour (0xffb0b0b0)));
        content = jam::Text::replaceholder (content, "token_identifier",   colourToWhelmedHex (juce::Colour (0xff00c6ff)));
        content = jam::Text::replaceholder (content, "token_integer",      colourToWhelmedHex (juce::Colour (0xff00ff00)));
        content = jam::Text::replaceholder (content, "token_float",        colourToWhelmedHex (juce::Colour (0xff00ff00)));
        content = jam::Text::replaceholder (content, "token_string",       colourToWhelmedHex (juce::Colour (0xffffc0c0)));
        content = jam::Text::replaceholder (content, "token_bracket",      colourToWhelmedHex (juce::Colour (0xff80ffff)));
        content = jam::Text::replaceholder (content, "token_punctuation",  colourToWhelmedHex (juce::Colour (0xffff9080)));
        content = jam::Text::replaceholder (content, "token_preprocessor", colourToWhelmedHex (juce::Colour (0xff9aff00)));

        // Table
        content = jam::Text::replaceholder (content, "table_background",        colourToWhelmedHex (juce::Colour (0xff090d12)));
        content = jam::Text::replaceholder (content, "table_header_background", colourToWhelmedHex (juce::Colour (0xff112130)));
        content = jam::Text::replaceholder (content, "table_row_alt",           colourToWhelmedHex (juce::Colour (0xff0d141c)));
        content = jam::Text::replaceholder (content, "table_border_colour",     colourToWhelmedHex (juce::Colour (0xff2c4144)));
        content = jam::Text::replaceholder (content, "table_header_text",       colourToWhelmedHex (juce::Colour (0xffbafffd)));
        content = jam::Text::replaceholder (content, "table_cell_text",         colourToWhelmedHex (juce::Colour (0xffb3f9f5)));

        // Progress bar
        content = jam::Text::replaceholder (content, "progress_background",     colourToWhelmedHex (juce::Colour (0xff1a1a1a)));
        content = jam::Text::replaceholder (content, "progress_foreground",     colourToWhelmedHex (juce::Colour (0xff4488cc)));
        content = jam::Text::replaceholder (content, "progress_text_colour",    colourToWhelmedHex (juce::Colour (0xffcccccc)));
        content = jam::Text::replaceholder (content, "progress_spinner_colour", colourToWhelmedHex (juce::Colour (0xff4488cc)));

        // Scrollbar
        content = jam::Text::replaceholder (content, "scrollbar_thumb",      colourToWhelmedHex (juce::Colour (0xff2c4144)));
        content = jam::Text::replaceholder (content, "scrollbar_track",      colourToWhelmedHex (juce::Colour (0xff0d141c)));
        content = jam::Text::replaceholder (content, "scrollbar_background", colourToWhelmedHex (juce::Colour (0xff0d141c)));
        content = jam::Text::replaceholder (content, "selection_colour",     colourToWhelmedHex (juce::Colour (0x8000c8d8)));

        // Navigation
        content = jam::Text::replaceholder (content, "scroll_down",   "j");
        content = jam::Text::replaceholder (content, "scroll_up",     "k");
        content = jam::Text::replaceholder (content, "scroll_top",    "gg");
        content = jam::Text::replaceholder (content, "scroll_bottom", "G");
        content = jam::Text::replaceholder (content, "scroll_step",   "50");

        file.replaceWithText (content);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
