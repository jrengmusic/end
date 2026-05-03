/**
 * @file EngineDefaults.cpp
 * @brief Default-value initialisation and config-file write helpers for lua::Engine.
 *
 * Contains: Engine::initDefaults(), Engine::writeDefaults(), all
 * Engine::writeXxxDefaults() methods, and the file-local colour
 * formatting helpers colourToHex() and colourToWhelmedHex().
 *
 * @see lua::Engine
 */

#include "Engine.h"

namespace lua
{

//==============================================================================
/** @brief Converts a juce::Colour to "#RRGGBB" or "#RRGGBBAA" for end.lua templates. */
static juce::String colourToHex (juce::Colour c)
{
    const auto aarrggbb { c.toString() };// JUCE format: AARRGGBB
    juce::String result { "#" + aarrggbb.substring (2) };

    if (c.getAlpha() != 0xff)
        result += aarrggbb.substring (0, 2);// Rearrange AARRGGBB -> RRGGBBAA for #RRGGBBAA format

    return result;
}

/** @brief Converts a juce::Colour to "RRGGBBAA" (no #) for whelmed.lua templates. */
static juce::String colourToWhelmedHex (juce::Colour c)
{
    const auto aarrggbb { c.toString() };// JUCE format: AARRGGBB
    return aarrggbb.substring (2) + aarrggbb.substring (0, 2);// RRGGBBAA, no #
}

//==============================================================================
/** @brief Substitutes font and cursor placeholder values into display.lua content. */
static void writeDisplayFontDefaults (juce::String& content, const Engine::Display& display)
{
    content = jam::String::replaceholder (content, "font_family", display.font.family);
    content = jam::String::replaceholder (content, "font_size", juce::String (display.font.size));
    content = jam::String::replaceholder (content, "font_ligatures", display.font.ligatures ? "true" : "false");
    content = jam::String::replaceholder (content, "font_embolden", display.font.embolden ? "true" : "false");
    content = jam::String::replaceholder (content, "font_line_height", juce::String (display.font.lineHeight));
    content = jam::String::replaceholder (content, "font_cell_width", juce::String (display.font.cellWidth));
    content = jam::String::replaceholder (content, "font_desktop_scale", display.font.desktopScale ? "true" : "false");

    // Cursor char is stored as a unicode codepoint; emit as UTF-8 string
    const auto cursorChar { juce::String::charToString (static_cast<juce::juce_wchar> (display.cursor.codepoint)) };
    content = jam::String::replaceholder (content, "cursor_char", cursorChar);
    content = jam::String::replaceholder (content, "cursor_blink", display.cursor.blink ? "true" : "false");
    content =
        jam::String::replaceholder (content, "cursor_blink_interval", juce::String (display.cursor.blinkInterval));
    content = jam::String::replaceholder (content, "cursor_force", display.cursor.force ? "true" : "false");
}

/** @brief Substitutes colour palette placeholder values into display.lua content. */
static void writeDisplayColoursDefaults (juce::String& content, const Engine::Display& display)
{
    content = jam::String::replaceholder (content, "colours_foreground", colourToHex (display.colours.foreground));
    content = jam::String::replaceholder (content, "colours_background", colourToHex (display.colours.background));
    content = jam::String::replaceholder (content, "colours_cursor", colourToHex (display.colours.cursor));
    content = jam::String::replaceholder (content, "colours_selection", colourToHex (display.colours.selection));
    content =
        jam::String::replaceholder (content, "colours_selection_cursor", colourToHex (display.colours.selectionCursor));

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

    for (size_t i { 0 }; i < 16; ++i)
        content = jam::String::replaceholder (content, ansiKeys.at (i), colourToHex (display.colours.ansi.at (i)));

    content = jam::String::replaceholder (content, "colours_status_bar", colourToHex (display.colours.statusBar));
    content = jam::String::replaceholder (
        content, "colours_status_bar_label_bg", colourToHex (display.colours.statusBarLabelBg));
    content = jam::String::replaceholder (
        content, "colours_status_bar_label_fg", colourToHex (display.colours.statusBarLabelFg));
    content = jam::String::replaceholder (
        content, "colours_status_bar_spinner", colourToHex (display.colours.statusBarSpinner));
    content = jam::String::replaceholder (content, "colours_hint_label_bg", colourToHex (display.colours.hintLabelBg));
    content = jam::String::replaceholder (content, "colours_hint_label_fg", colourToHex (display.colours.hintLabelFg));
}

/** @brief Substitutes window placeholder values into display.lua content. */
static void writeDisplayWindowDefaults (juce::String& content, const Engine::Display& display)
{
    auto windowTitle { display.window.title };
    windowTitle = windowTitle.replace ("\\", "\\\\");
    content = jam::String::replaceholder (content, "window_title", windowTitle);
    content = jam::String::replaceholder (content, "window_width", juce::String (display.window.width));
    content = jam::String::replaceholder (content, "window_height", juce::String (display.window.height));
    content = jam::String::replaceholder (content, "window_colour", colourToHex (display.window.colour));
    content = jam::String::replaceholder (content, "window_opacity", juce::String (display.window.opacity));
    content = jam::String::replaceholder (content, "window_blur_radius", juce::String (display.window.blurRadius));
    content =
        jam::String::replaceholder (content, "window_always_on_top", display.window.alwaysOnTop ? "true" : "false");
    content = jam::String::replaceholder (content, "window_buttons", display.window.buttons ? "true" : "false");
    content = jam::String::replaceholder (content, "window_force_dwm", display.window.forceDwm ? "true" : "false");
    content = jam::String::replaceholder (content, "window_save_size", display.window.saveSize ? "true" : "false");
    content = jam::String::replaceholder (
        content, "window_confirmation_on_exit", display.window.confirmationOnExit ? "true" : "false");
}

/** @brief Substitutes tab placeholder values into display.lua content. */
static void writeDisplayTabDefaults (juce::String& content, const Engine::Display& display)
{
    content = jam::String::replaceholder (content, "tab_family", display.tab.family);
    content = jam::String::replaceholder (content, "tab_size", juce::String (display.tab.size));
    content = jam::String::replaceholder (content, "tab_foreground", colourToHex (display.tab.foreground));
    content = jam::String::replaceholder (content, "tab_inactive", colourToHex (display.tab.inactive));
    content = jam::String::replaceholder (content, "tab_position", display.tab.position);
    content = jam::String::replaceholder (content, "tab_line", colourToHex (display.tab.line));
    content = jam::String::replaceholder (content, "tab_active", colourToHex (display.tab.active));
    content = jam::String::replaceholder (content, "tab_indicator", colourToHex (display.tab.indicator));
}

/** @brief Substitutes overlay, pane, menu, status bar, action list, and popup placeholder values. */
static void writeDisplayMiscDefaults (juce::String& content, const Engine::Display& display)
{
    // Menu
    content = jam::String::replaceholder (content, "menu_opacity", juce::String (display.menu.opacity));

    // Overlay
    content = jam::String::replaceholder (content, "overlay_family", display.overlay.family);
    content = jam::String::replaceholder (content, "overlay_size", juce::String (display.overlay.size));
    content = jam::String::replaceholder (content, "overlay_colour", colourToHex (display.overlay.colour));

    // Pane
    content = jam::String::replaceholder (content, "pane_bar_colour", colourToHex (display.pane.barColour));
    content = jam::String::replaceholder (content, "pane_bar_highlight", colourToHex (display.pane.barHighlight));

    // Status bar
    content = jam::String::replaceholder (content, "status_bar_position", display.statusBar.position);
    content = jam::String::replaceholder (content, "status_bar_font_family", display.statusBar.fontFamily);
    content = jam::String::replaceholder (content, "status_bar_font_size", juce::String (display.statusBar.fontSize));
    content = jam::String::replaceholder (content, "status_bar_font_style", display.statusBar.fontStyle);

    // Action list
    content = jam::String::replaceholder (content, "action_list_position", display.actionList.position);
    content = jam::String::replaceholder (
        content, "action_list_close_on_run", display.actionList.closeOnRun ? "true" : "false");
    content = jam::String::replaceholder (content, "action_list_name_font_family", display.actionList.nameFamily);
    content = jam::String::replaceholder (content, "action_list_name_font_style", display.actionList.nameStyle);
    content =
        jam::String::replaceholder (content, "action_list_name_font_size", juce::String (display.actionList.nameSize));
    content =
        jam::String::replaceholder (content, "action_list_shortcut_font_family", display.actionList.shortcutFamily);
    content = jam::String::replaceholder (content, "action_list_shortcut_font_style", display.actionList.shortcutStyle);
    content = jam::String::replaceholder (
        content, "action_list_shortcut_font_size", juce::String (display.actionList.shortcutSize));
    content =
        jam::String::replaceholder (content, "action_list_padding_top", juce::String (display.actionList.paddingTop));
    content = jam::String::replaceholder (
        content, "action_list_padding_right", juce::String (display.actionList.paddingRight));
    content = jam::String::replaceholder (
        content, "action_list_padding_bottom", juce::String (display.actionList.paddingBottom));
    content =
        jam::String::replaceholder (content, "action_list_padding_left", juce::String (display.actionList.paddingLeft));
    content =
        jam::String::replaceholder (content, "action_list_name_colour", colourToHex (display.actionList.nameColour));
    content = jam::String::replaceholder (
        content, "action_list_shortcut_colour", colourToHex (display.actionList.shortcutColour));
    content = jam::String::replaceholder (content, "action_list_width", juce::String (display.actionList.width));
    content = jam::String::replaceholder (content, "action_list_height", juce::String (display.actionList.height));
    content = jam::String::replaceholder (
        content, "action_list_highlight_colour", colourToHex (display.actionList.highlightColour));

    // Popup border
    content = jam::String::replaceholder (content, "popup_border_colour", colourToHex (display.popup.borderColour));
    content = jam::String::replaceholder (content, "popup_border_width", juce::String (display.popup.borderWidth));
}

//==============================================================================
void Engine::initDefaults()
{
    // Window
    display.window.title = ProjectInfo::projectName;
    display.window.colour = juce::Colour (0xff090d12);// bunker

    // Colours — foreground, background, cursor, selection
    display.colours.foreground = juce::Colour (0xffa1d6e5);// skyFall
    display.colours.background = juce::Colour (0xff090d12);// bunker (opaque default)
    display.colours.cursor = juce::Colour (0xff4e8c93);// paradiso
    display.colours.selection = juce::Colour (0x2000ddee);// fishBoy semi-transparent
    display.colours.selectionCursor = juce::Colour (0xff00ddee);// fishBoy

    // ANSI palette — normal 0-7
    display.colours.ansi.at (0) = juce::Colour (0xff090d12);// black    — bunker
    display.colours.ansi.at (1) = juce::Colour (0xfffc704c);// red      — preciousPersimmon
    display.colours.ansi.at (2) = juce::Colour (0xffc5f0e9);// green    — gentleCold
    display.colours.ansi.at (3) = juce::Colour (0xfff3f5c5);// yellow   — silkStar
    display.colours.ansi.at (4) = juce::Colour (0xff8cc9d9);// blue     — dolphin
    display.colours.ansi.at (5) = juce::Colour (0xff519299);// magenta  — lagoon
    display.colours.ansi.at (6) = juce::Colour (0xff699daa);// cyan     — tranquiliTeal
    display.colours.ansi.at (7) = juce::Colour (0xffdddddd);// white    — frostbite

    // ANSI palette — bright 8-15
    display.colours.ansi.at (8) = juce::Colour (0xff33535b);// bright black   — mediterranea
    display.colours.ansi.at (9) = juce::Colour (0xfffc704c);// bright red     — preciousPersimmon
    display.colours.ansi.at (10) = juce::Colour (0xffbafffd);// bright green   — paleSky
    display.colours.ansi.at (11) = juce::Colour (0xfffeffd2);// bright yellow  — mattWhite
    display.colours.ansi.at (12) = juce::Colour (0xff67dfef);// bright blue    — poseidonJr
    display.colours.ansi.at (13) = juce::Colour (0xff01c2d2);// bright magenta — caribbeanBlue
    display.colours.ansi.at (14) = juce::Colour (0xff00c8d8);// bright cyan    — blueBikini
    display.colours.ansi.at (15) = juce::Colour (0xffbafffd);// bright white   — paleSky

    // Status bar colours
    display.colours.statusBar = juce::Colour (0xff090d12);// bunker
    display.colours.statusBarLabelBg = juce::Colour (0xff112130);// trappedDarkness
    display.colours.statusBarLabelFg = juce::Colour (0xff4e8c93);// paradiso
    display.colours.statusBarSpinner = juce::Colour (0xff00c8d8);// blueBikini

    // Hint label colours
    display.colours.hintLabelBg = juce::Colour (0xff00ffff);// cyan
    display.colours.hintLabelFg = juce::Colour (0xff111111);// near-black

    // Tab colours
    display.tab.foreground = juce::Colour (0xff00c8d8);// blueBikini
    display.tab.inactive = juce::Colour (0xff33535b);// mediterranea
    display.tab.line = juce::Colour (0xff2c4144);// littleMermaid
    display.tab.active = juce::Colour (0xff002b35);// midnightDreams
    display.tab.indicator = juce::Colour (0xff01c2d2);// caribbeanBlue

    // Overlay colour
    display.overlay.colour = juce::Colour (0xff4e8c93);// paradiso

    // Pane colours
    display.pane.barColour = juce::Colour (0xff33535b);// mediterranea
    display.pane.barHighlight = juce::Colour (0xff4e8c93);// paradiso

    // Action list colours
    display.actionList.nameColour = juce::Colour (0xffa1d6e5);// skyFall
    display.actionList.shortcutColour = juce::Colour (0xff00c8d8);// blueBikini
    display.actionList.highlightColour = juce::Colour (0x2000ddee);// fishBoy semi-transparent

    // Popup border colour
    display.popup.borderColour = juce::Colour (0xff4e8c93);// paradiso

    // Whelmed colours
    whelmed.background = juce::Colour (0xff0d141c);// corbeau (AARRGGBB: ff0d141c)
    whelmed.bodyColour = juce::Colour (0xffb3f9f5);
    whelmed.codeColour = juce::Colour (0xff00d0ff);
    whelmed.linkColour = juce::Colour (0xff01c2d2);
    whelmed.h1Colour = juce::Colour (0xffd4c8a0);
    whelmed.h2Colour = juce::Colour (0xffd4c8a0);
    whelmed.h3Colour = juce::Colour (0xffd4c8a0);
    whelmed.h4Colour = juce::Colour (0xffd4c8a0);
    whelmed.h5Colour = juce::Colour (0xffd4c8a0);
    whelmed.h6Colour = juce::Colour (0xffd4c8a0);
    whelmed.codeFenceBackground = juce::Colour (0xff090d12);
    whelmed.progressBackground = juce::Colour (0xff1a1a1a);
    whelmed.progressForeground = juce::Colour (0xff4488cc);
    whelmed.progressTextColour = juce::Colour (0xffcccccc);
    whelmed.progressSpinnerColour = juce::Colour (0xff4488cc);

    whelmed.tokenError = juce::Colour (0xfff74a4a);
    whelmed.tokenComment = juce::Colour (0xff6080c0);
    whelmed.tokenKeyword = juce::Colour (0xff1919ff);
    whelmed.tokenOperator = juce::Colour (0xffb0b0b0);
    whelmed.tokenIdentifier = juce::Colour (0xff00c6ff);
    whelmed.tokenInteger = juce::Colour (0xff00ff00);
    whelmed.tokenFloat = juce::Colour (0xff00ff00);
    whelmed.tokenString = juce::Colour (0xffffc0c0);
    whelmed.tokenBracket = juce::Colour (0xff80ffff);
    whelmed.tokenPunctuation = juce::Colour (0xffff9080);
    whelmed.tokenPreprocessor = juce::Colour (0xff9aff00);

    whelmed.tableBackground = juce::Colour (0xff090d12);
    whelmed.tableHeaderBackground = juce::Colour (0xff112130);
    whelmed.tableRowAlt = juce::Colour (0xff0d141c);
    whelmed.tableBorderColour = juce::Colour (0xff2c4144);
    whelmed.tableHeaderText = juce::Colour (0xffbafffd);
    whelmed.tableCellText = juce::Colour (0xffb3f9f5);

    whelmed.scrollbarThumb = juce::Colour (0xff2c4144);
    whelmed.scrollbarTrack = juce::Colour (0xff0d141c);
    whelmed.scrollbarBackground = juce::Colour (0xff0d141c);

    whelmed.selectionColour = juce::Colour (0x8000c8d8);

    // Shell — platform-conditional
#if JUCE_MAC
    nexus.shell.program = "zsh";
    nexus.shell.args = "-l";
#elif JUCE_LINUX
    nexus.shell.program = "bash";
    nexus.shell.args = "-l";
#elif JUCE_WINDOWS
    nexus.shell.program = "powershell.exe";
    nexus.shell.args = "";
#endif
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
    const auto file { configDir.getChildFile ("end.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_end.lua") } };
        content = jam::String::replaceholder (content, "versionString", ProjectInfo::versionString);
        file.replaceWithText (content);
    }
}

void Engine::writeNexusDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("nexus.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_nexus.lua") } };
        content = jam::String::replaceholder (content, "gpu", nexus.gpu);
        content = jam::String::replaceholder (content, "daemon", nexus.daemon ? "true" : "false");
        content = jam::String::replaceholder (content, "auto_reload", nexus.autoReload ? "true" : "false");

        auto shellProgram { nexus.shell.program };
        shellProgram = shellProgram.replace ("\\", "\\\\");
        content = jam::String::replaceholder (content, "shell_program", shellProgram);
        content = jam::String::replaceholder (content, "shell_args", nexus.shell.args);
        content = jam::String::replaceholder (content, "shell_integration", nexus.shell.integration ? "true" : "false");

        content = jam::String::replaceholder (
            content, "terminal_scrollback_lines", juce::String (nexus.terminal.scrollbackLines));
        content =
            jam::String::replaceholder (content, "terminal_scroll_step", juce::String (nexus.terminal.scrollStep));
        content =
            jam::String::replaceholder (content, "terminal_padding_top", juce::String (nexus.terminal.paddingTop));
        content =
            jam::String::replaceholder (content, "terminal_padding_right", juce::String (nexus.terminal.paddingRight));
        content = jam::String::replaceholder (
            content, "terminal_padding_bottom", juce::String (nexus.terminal.paddingBottom));
        content =
            jam::String::replaceholder (content, "terminal_padding_left", juce::String (nexus.terminal.paddingLeft));
        content = jam::String::replaceholder (content, "terminal_drop_multifiles", nexus.terminal.dropMultifiles);
        content =
            jam::String::replaceholder (content, "terminal_drop_quoted", nexus.terminal.dropQuoted ? "true" : "false");
        content = jam::String::replaceholder (content, "hyperlinks_editor", nexus.hyperlinks.editor);
        content = jam::String::replaceholder (
            content, "image_atlas_dimension", juce::String (nexus.image.atlasDimension));
        content = jam::String::replaceholder (
            content, "image_cols", juce::String (nexus.image.cols));
        content = jam::String::replaceholder (
            content, "image_rows", juce::String (nexus.image.rows));
        content = jam::String::replaceholder (
            content, "image_padding", juce::String (nexus.image.padding));
        content = jam::String::replaceholder (
            content, "image_border", nexus.image.border ? "true" : "false");
        file.replaceWithText (content);
    }
}

void Engine::writeDisplayDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("display.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_display.lua") } };

        writeDisplayFontDefaults (content, display);
        writeDisplayColoursDefaults (content, display);
        writeDisplayWindowDefaults (content, display);
        writeDisplayTabDefaults (content, display);
        writeDisplayMiscDefaults (content, display);

        file.replaceWithText (content);
    }
}

void Engine::writeKeysDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("keys.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_keys.lua") } };

#if JUCE_MAC
        content = jam::String::replaceholder (content, "copy", "cmd+c");
        content = jam::String::replaceholder (content, "paste", "cmd+v");
        content = jam::String::replaceholder (content, "quit", "cmd+q");
        content = jam::String::replaceholder (content, "close_tab", "cmd+w");
        content = jam::String::replaceholder (content, "reload", "cmd+r");
        content = jam::String::replaceholder (content, "zoom_in", "cmd+=");
        content = jam::String::replaceholder (content, "zoom_out", "cmd+-");
        content = jam::String::replaceholder (content, "zoom_reset", "cmd+0");
        content = jam::String::replaceholder (content, "new_window", "cmd+n");
        content = jam::String::replaceholder (content, "new_tab", "cmd+t");
        content = jam::String::replaceholder (content, "prev_tab", "cmd+[");
        content = jam::String::replaceholder (content, "next_tab", "cmd+]");
#else
        content = jam::String::replaceholder (content, "copy", "ctrl+c");
        content = jam::String::replaceholder (content, "paste", "ctrl+v");
        content = jam::String::replaceholder (content, "quit", "ctrl+q");
        content = jam::String::replaceholder (content, "close_tab", "ctrl+w");
        content = jam::String::replaceholder (content, "reload", "ctrl+/");
        content = jam::String::replaceholder (content, "zoom_in", "ctrl+=");
        content = jam::String::replaceholder (content, "zoom_out", "ctrl+-");
        content = jam::String::replaceholder (content, "zoom_reset", "ctrl+0");
        content = jam::String::replaceholder (content, "new_window", "ctrl+n");
        content = jam::String::replaceholder (content, "new_tab", "ctrl+t");
        content = jam::String::replaceholder (content, "prev_tab", "ctrl+[");
        content = jam::String::replaceholder (content, "next_tab", "ctrl+]");
#endif

        file.replaceWithText (content);
    }
}

void Engine::writePopupsDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("popups.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_popups.lua") } };
        content = jam::String::replaceholder (content, "popup_cols", juce::String (popup.defaultCols));
        content = jam::String::replaceholder (content, "popup_rows", juce::String (popup.defaultRows));
        content = jam::String::replaceholder (content, "popup_position", popup.defaultPosition);
        file.replaceWithText (content);
    }
}

void Engine::writeActionsDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("actions.lua") };

    if (not file.existsAsFile())
    {
        const auto content { juce::String { BinaryData::getString ("default_actions.lua") } };
        file.replaceWithText (content);
    }
}

void Engine::writeWhelmedDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("whelmed.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_whelmed.lua") } };

        // Typography
        content = jam::String::replaceholder (content, "font_family", whelmed.fontFamily);
        content = jam::String::replaceholder (content, "font_style", whelmed.fontStyle);
        content = jam::String::replaceholder (content, "font_size", juce::String (whelmed.fontSize));
        content = jam::String::replaceholder (content, "code_family", whelmed.codeFamily);
        content = jam::String::replaceholder (content, "code_style", whelmed.codeStyle);
        content = jam::String::replaceholder (content, "code_size", juce::String (whelmed.codeSize));
        content = jam::String::replaceholder (content, "line_height", juce::String (whelmed.lineHeight));

        // Heading sizes
        content = jam::String::replaceholder (content, "h1_size", juce::String (whelmed.h1Size));
        content = jam::String::replaceholder (content, "h2_size", juce::String (whelmed.h2Size));
        content = jam::String::replaceholder (content, "h3_size", juce::String (whelmed.h3Size));
        content = jam::String::replaceholder (content, "h4_size", juce::String (whelmed.h4Size));
        content = jam::String::replaceholder (content, "h5_size", juce::String (whelmed.h5Size));
        content = jam::String::replaceholder (content, "h6_size", juce::String (whelmed.h6Size));

        // Layout
        content = jam::String::replaceholder (content, "padding_top", juce::String (whelmed.paddingTop));
        content = jam::String::replaceholder (content, "padding_right", juce::String (whelmed.paddingRight));
        content = jam::String::replaceholder (content, "padding_bottom", juce::String (whelmed.paddingBottom));
        content = jam::String::replaceholder (content, "padding_left", juce::String (whelmed.paddingLeft));

        // Colours
        content = jam::String::replaceholder (content, "background", colourToWhelmedHex (whelmed.background));
        content = jam::String::replaceholder (content, "body_colour", colourToWhelmedHex (whelmed.bodyColour));
        content = jam::String::replaceholder (content, "link_colour", colourToWhelmedHex (whelmed.linkColour));
        content = jam::String::replaceholder (content, "h1_colour", colourToWhelmedHex (whelmed.h1Colour));
        content = jam::String::replaceholder (content, "h2_colour", colourToWhelmedHex (whelmed.h2Colour));
        content = jam::String::replaceholder (content, "h3_colour", colourToWhelmedHex (whelmed.h3Colour));
        content = jam::String::replaceholder (content, "h4_colour", colourToWhelmedHex (whelmed.h4Colour));
        content = jam::String::replaceholder (content, "h5_colour", colourToWhelmedHex (whelmed.h5Colour));
        content = jam::String::replaceholder (content, "h6_colour", colourToWhelmedHex (whelmed.h6Colour));

        // Code blocks
        content = jam::String::replaceholder (
            content, "code_fence_background", colourToWhelmedHex (whelmed.codeFenceBackground));
        content = jam::String::replaceholder (content, "code_colour", colourToWhelmedHex (whelmed.codeColour));

        // Syntax tokens
        content = jam::String::replaceholder (content, "token_error", colourToWhelmedHex (whelmed.tokenError));
        content = jam::String::replaceholder (content, "token_comment", colourToWhelmedHex (whelmed.tokenComment));
        content = jam::String::replaceholder (content, "token_keyword", colourToWhelmedHex (whelmed.tokenKeyword));
        content = jam::String::replaceholder (content, "token_operator", colourToWhelmedHex (whelmed.tokenOperator));
        content =
            jam::String::replaceholder (content, "token_identifier", colourToWhelmedHex (whelmed.tokenIdentifier));
        content = jam::String::replaceholder (content, "token_integer", colourToWhelmedHex (whelmed.tokenInteger));
        content = jam::String::replaceholder (content, "token_float", colourToWhelmedHex (whelmed.tokenFloat));
        content = jam::String::replaceholder (content, "token_string", colourToWhelmedHex (whelmed.tokenString));
        content = jam::String::replaceholder (content, "token_bracket", colourToWhelmedHex (whelmed.tokenBracket));
        content =
            jam::String::replaceholder (content, "token_punctuation", colourToWhelmedHex (whelmed.tokenPunctuation));
        content =
            jam::String::replaceholder (content, "token_preprocessor", colourToWhelmedHex (whelmed.tokenPreprocessor));

        // Table
        content =
            jam::String::replaceholder (content, "table_background", colourToWhelmedHex (whelmed.tableBackground));
        content = jam::String::replaceholder (
            content, "table_header_background", colourToWhelmedHex (whelmed.tableHeaderBackground));
        content = jam::String::replaceholder (content, "table_row_alt", colourToWhelmedHex (whelmed.tableRowAlt));
        content =
            jam::String::replaceholder (content, "table_border_colour", colourToWhelmedHex (whelmed.tableBorderColour));
        content =
            jam::String::replaceholder (content, "table_header_text", colourToWhelmedHex (whelmed.tableHeaderText));
        content = jam::String::replaceholder (content, "table_cell_text", colourToWhelmedHex (whelmed.tableCellText));

        // Progress bar
        content = jam::String::replaceholder (
            content, "progress_background", colourToWhelmedHex (whelmed.progressBackground));
        content = jam::String::replaceholder (
            content, "progress_foreground", colourToWhelmedHex (whelmed.progressForeground));
        content = jam::String::replaceholder (
            content, "progress_text_colour", colourToWhelmedHex (whelmed.progressTextColour));
        content = jam::String::replaceholder (
            content, "progress_spinner_colour", colourToWhelmedHex (whelmed.progressSpinnerColour));

        // Scrollbar
        content = jam::String::replaceholder (content, "scrollbar_thumb", colourToWhelmedHex (whelmed.scrollbarThumb));
        content = jam::String::replaceholder (content, "scrollbar_track", colourToWhelmedHex (whelmed.scrollbarTrack));
        content = jam::String::replaceholder (
            content, "scrollbar_background", colourToWhelmedHex (whelmed.scrollbarBackground));
        content =
            jam::String::replaceholder (content, "selection_colour", colourToWhelmedHex (whelmed.selectionColour));

        // Navigation
        content = jam::String::replaceholder (content, "scroll_down", whelmed.scrollDown);
        content = jam::String::replaceholder (content, "scroll_up", whelmed.scrollUp);
        content = jam::String::replaceholder (content, "scroll_top", whelmed.scrollTop);
        content = jam::String::replaceholder (content, "scroll_bottom", whelmed.scrollBottom);
        content = jam::String::replaceholder (content, "scroll_step", juce::String (whelmed.scrollStep));

        file.replaceWithText (content);
    }
}

}// namespace lua
