/**
 * @file EngineDefaults.cpp
 * @brief Config-file write helpers for lua::Engine.
 *
 * Contains: Engine::writeDefaults(), all Engine::writeXxxDefaults() methods,
 * and the file-local colour formatting helpers colourToHex() and
 * colourToWhelmedHex(). Struct default values live in Engine.h brace-inits.
 *
 * @see lua::Engine
 */

#include "Engine.h"
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
static void writeDisplayFontDefaults (juce::String& content, const Engine::Display& display)
{
    content = jam::Text::replaceholder (content, "font_family", display.font.family);
    content = jam::Text::replaceholder (content, "font_size", juce::String (display.font.size));
    content = jam::Text::replaceholder (content, "font_ligatures", display.font.ligatures ? "true" : "false");
    content = jam::Text::replaceholder (content, "font_embolden", display.font.embolden ? "true" : "false");
    content = jam::Text::replaceholder (content, "font_line_height", juce::String (display.font.lineHeight));
    content = jam::Text::replaceholder (content, "font_cell_width", juce::String (display.font.cellWidth));
    content = jam::Text::replaceholder (content, "font_desktop_scale", display.font.desktopScale ? "true" : "false");

    // Cursor char is stored as a unicode codepoint; emit as UTF-8 string
    const auto cursorChar { juce::String::charToString (jam::toChar (display.cursor.codepoint)) };
    content = jam::Text::replaceholder (content, "cursor_char", cursorChar);
    content = jam::Text::replaceholder (content, "cursor_blink", display.cursor.blink ? "true" : "false");
    content =
        jam::Text::replaceholder (content, "cursor_blink_interval", juce::String (display.cursor.blinkInterval));
    content = jam::Text::replaceholder (content, "cursor_force", display.cursor.force ? "true" : "false");
    content = jam::Text::replaceholder (content, "cursor_style", Map::Cursor::getContext()->get (display.cursor.style));
}

/** @brief Substitutes colour palette placeholder values into display.lua content. */
static void writeDisplayColoursDefaults (juce::String& content, const Engine::Display& display)
{
    content = jam::Text::replaceholder (content, "colours_foreground", colourToHex (display.colours.foreground));
    content = jam::Text::replaceholder (content, "colours_background", colourToHex (display.colours.background));
    content = jam::Text::replaceholder (content, "colours_cursor", colourToHex (display.colours.cursor));
    content = jam::Text::replaceholder (content, "colours_selection", colourToHex (display.colours.selection));
    content =
        jam::Text::replaceholder (content, "colours_selection_cursor", colourToHex (display.colours.selectionCursor));

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
        content = jam::Text::replaceholder (content, ansiKeys.at (i), colourToHex (display.colours.ansi.at (i)));

    content = jam::Text::replaceholder (content, "colours_status_bar", colourToHex (display.colours.statusBar));
    content = jam::Text::replaceholder (
        content, "colours_status_bar_label_bg", colourToHex (display.colours.statusBarLabelBg));
    content = jam::Text::replaceholder (
        content, "colours_status_bar_label_fg", colourToHex (display.colours.statusBarLabelFg));
    content = jam::Text::replaceholder (
        content, "colours_status_bar_spinner", colourToHex (display.colours.statusBarSpinner));
    content = jam::Text::replaceholder (content, "colours_hint_label_bg", colourToHex (display.colours.hintLabelBg));
    content = jam::Text::replaceholder (content, "colours_hint_label_fg", colourToHex (display.colours.hintLabelFg));
    content = jam::Text::replaceholder (content, "colours_editor_background", colourToHex (display.colours.editorBackground));
    content = jam::Text::replaceholder (content, "colours_editor_outline",    colourToHex (display.colours.editorOutline));
    content = jam::Text::replaceholder (content, "colours_scrollbar_thumb",   colourToHex (display.colours.scrollbarThumb));
    content = jam::Text::replaceholder (content, "colours_scrollbar_track",   colourToHex (display.colours.scrollbarTrack));
}

/** @brief Substitutes window placeholder values into display.lua content. */
static void writeDisplayWindowDefaults (juce::String& content, const Engine::Display& display)
{
    auto windowTitle { display.window.title };
    windowTitle = windowTitle.replace ("\\", "\\\\");
    content = jam::Text::replaceholder (content, "window_title", windowTitle);
    content = jam::Text::replaceholder (content, "window_width", juce::String (display.window.width));
    content = jam::Text::replaceholder (content, "window_height", juce::String (display.window.height));
    content = jam::Text::replaceholder (content, "window_colour", colourToHex (display.window.colour));
    content = jam::Text::replaceholder (content, "window_opacity", juce::String (display.window.opacity));
    content = jam::Text::replaceholder (content, "window_blur_radius", juce::String (display.window.blurRadius));
    content =
        jam::Text::replaceholder (content, "window_always_on_top", display.window.alwaysOnTop ? "true" : "false");
    content = jam::Text::replaceholder (content, "window_buttons", display.window.buttons ? "true" : "false");
    content = jam::Text::replaceholder (content, "window_force_dwm", display.window.forceDwm ? "true" : "false");
    content = jam::Text::replaceholder (content, "window_save_size", display.window.saveSize ? "true" : "false");
    content = jam::Text::replaceholder (
        content, "window_confirmation_on_exit", display.window.confirmationOnExit ? "true" : "false");
}

/** @brief Substitutes tab placeholder values into display.lua content. */
static void writeDisplayTabDefaults (juce::String& content, const Engine::Display& display)
{
    content = jam::Text::replaceholder (content, "tab_family", display.tab.family);
    content = jam::Text::replaceholder (content, "tab_size", juce::String (display.tab.size));
    content = jam::Text::replaceholder (content, "tab_foreground", colourToHex (display.tab.foreground));
    content = jam::Text::replaceholder (content, "tab_inactive", colourToHex (display.tab.inactive));
    content = jam::Text::replaceholder (content, "tab_position", display.tab.position);
    content = jam::Text::replaceholder (content, "tab_line", colourToHex (display.tab.line));
    content = jam::Text::replaceholder (content, "tab_active", colourToHex (display.tab.active));
    content = jam::Text::replaceholder (content, "tab_indicator", colourToHex (display.tab.indicator));
}

/** @brief Substitutes overlay, pane, menu, status bar, action list, and popup placeholder values. */
static void writeDisplayMiscDefaults (juce::String& content, const Engine::Display& display)
{
    // Menu
    content = jam::Text::replaceholder (content, "menu_opacity", juce::String (display.menu.opacity));

    // Overlay
    content = jam::Text::replaceholder (content, "overlay_family", display.overlay.family);
    content = jam::Text::replaceholder (content, "overlay_size", juce::String (display.overlay.size));
    content = jam::Text::replaceholder (content, "overlay_colour", colourToHex (display.overlay.colour));

    // Pane
    content = jam::Text::replaceholder (content, "pane_bar_colour", colourToHex (display.pane.barColour));
    content = jam::Text::replaceholder (content, "pane_bar_highlight", colourToHex (display.pane.barHighlight));

    // Status bar
    content = jam::Text::replaceholder (content, "status_bar_position", display.statusBar.position);
    content = jam::Text::replaceholder (content, "status_bar_font_family", display.statusBar.fontFamily);
    content = jam::Text::replaceholder (content, "status_bar_font_size", juce::String (display.statusBar.fontSize));
    content = jam::Text::replaceholder (content, "status_bar_font_style", display.statusBar.fontStyle);

    // Action list
    content = jam::Text::replaceholder (content, "action_list_position", display.actionList.position);
    content = jam::Text::replaceholder (
        content, "action_list_close_on_run", display.actionList.closeOnRun ? "true" : "false");
    content = jam::Text::replaceholder (content, "action_list_name_font_family", display.actionList.nameFamily);
    content = jam::Text::replaceholder (content, "action_list_name_font_style", display.actionList.nameStyle);
    content =
        jam::Text::replaceholder (content, "action_list_name_font_size", juce::String (display.actionList.nameSize));
    content =
        jam::Text::replaceholder (content, "action_list_shortcut_font_family", display.actionList.shortcutFamily);
    content = jam::Text::replaceholder (content, "action_list_shortcut_font_style", display.actionList.shortcutStyle);
    content = jam::Text::replaceholder (
        content, "action_list_shortcut_font_size", juce::String (display.actionList.shortcutSize));
    content =
        jam::Text::replaceholder (content, "action_list_padding_top", juce::String (display.actionList.paddingTop));
    content = jam::Text::replaceholder (
        content, "action_list_padding_right", juce::String (display.actionList.paddingRight));
    content = jam::Text::replaceholder (
        content, "action_list_padding_bottom", juce::String (display.actionList.paddingBottom));
    content =
        jam::Text::replaceholder (content, "action_list_padding_left", juce::String (display.actionList.paddingLeft));
    content =
        jam::Text::replaceholder (content, "action_list_name_colour", colourToHex (display.actionList.nameColour));
    content = jam::Text::replaceholder (
        content, "action_list_shortcut_colour", colourToHex (display.actionList.shortcutColour));
    content = jam::Text::replaceholder (content, "action_list_width", juce::String (display.actionList.width));
    content = jam::Text::replaceholder (content, "action_list_height", juce::String (display.actionList.height));
    content = jam::Text::replaceholder (
        content, "action_list_highlight_colour", colourToHex (display.actionList.highlightColour));

    // Popup border
    content = jam::Text::replaceholder (content, "popup_border_colour", colourToHex (display.popup.borderColour));
    content = jam::Text::replaceholder (content, "popup_border_width", juce::String (display.popup.borderWidth));

    // Scrollbar
    content = jam::Text::replaceholder (content, "scrollbar_width", juce::String (display.scrollbarWidth));
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
        content = jam::Text::replaceholder (content, "versionString", ProjectInfo::versionString);
        file.replaceWithText (content);
    }
}

void Engine::writeNexusDefaults (const juce::File& configDir)
{
    const auto file { configDir.getChildFile ("nexus.lua") };

    if (not file.existsAsFile())
    {
        auto content { juce::String { BinaryData::getString ("default_nexus.lua") } };
        content = jam::Text::replaceholder (content, "gpu", nexus.gpu);
        content = jam::Text::replaceholder (content, "daemon", nexus.daemon ? "true" : "false");
        content = jam::Text::replaceholder (content, "auto_reload", nexus.autoReload ? "true" : "false");

        auto shellProgram { nexus.shell.program };
        shellProgram = shellProgram.replace ("\\", "\\\\");
        content = jam::Text::replaceholder (content, "shell_program", shellProgram);
        content = jam::Text::replaceholder (content, "shell_args", nexus.shell.args);
        content = jam::Text::replaceholder (content, "shell_integration", nexus.shell.integration ? "true" : "false");

        content = jam::Text::replaceholder (
            content, "terminal_scrollback_lines", juce::String (nexus.terminal.scrollbackLines));
        content =
            jam::Text::replaceholder (content, "terminal_scroll_step", juce::String (nexus.terminal.scrollStep));
        content =
            jam::Text::replaceholder (content, "terminal_padding_top", juce::String (nexus.terminal.paddingTop));
        content =
            jam::Text::replaceholder (content, "terminal_padding_right", juce::String (nexus.terminal.paddingRight));
        content = jam::Text::replaceholder (
            content, "terminal_padding_bottom", juce::String (nexus.terminal.paddingBottom));
        content =
            jam::Text::replaceholder (content, "terminal_padding_left", juce::String (nexus.terminal.paddingLeft));
        content = jam::Text::replaceholder (content, "terminal_drop_multifiles", nexus.terminal.dropMultifiles);
        content =
            jam::Text::replaceholder (content, "terminal_drop_quoted", nexus.terminal.dropQuoted ? "true" : "false");
        content = jam::Text::replaceholder (content, "hyperlinks_editor", nexus.hyperlinks.editor);
        content = jam::Text::replaceholder (
            content, "image_atlas_dimension", juce::String (nexus.image.atlasDimension));
        content = jam::Text::replaceholder (
            content, "image_cols", juce::String (nexus.image.cols.value));
        content = jam::Text::replaceholder (
            content, "image_rows", juce::String (nexus.image.rows.value));
        content = jam::Text::replaceholder (
            content, "image_padding", juce::String (nexus.image.padding));
        content = jam::Text::replaceholder (
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
        content = jam::Text::replaceholder (content, "copy", "cmd+c");
        content = jam::Text::replaceholder (content, "paste", "cmd+v");
        content = jam::Text::replaceholder (content, "quit", "cmd+q");
        content = jam::Text::replaceholder (content, "close_tab", "cmd+w");
        content = jam::Text::replaceholder (content, "reload", "cmd+r");
        content = jam::Text::replaceholder (content, "zoom_in", "cmd+=");
        content = jam::Text::replaceholder (content, "zoom_out", "cmd+-");
        content = jam::Text::replaceholder (content, "zoom_reset", "cmd+0");
        content = jam::Text::replaceholder (content, "new_window", "cmd+n");
        content = jam::Text::replaceholder (content, "new_tab", "cmd+t");
        content = jam::Text::replaceholder (content, "prev_tab", "cmd+[");
        content = jam::Text::replaceholder (content, "next_tab", "cmd+]");
#else
        content = jam::Text::replaceholder (content, "copy", "ctrl+c");
        content = jam::Text::replaceholder (content, "paste", "ctrl+v");
        content = jam::Text::replaceholder (content, "quit", "ctrl+q");
        content = jam::Text::replaceholder (content, "close_tab", "ctrl+w");
        content = jam::Text::replaceholder (content, "reload", "ctrl+/");
        content = jam::Text::replaceholder (content, "zoom_in", "ctrl+=");
        content = jam::Text::replaceholder (content, "zoom_out", "ctrl+-");
        content = jam::Text::replaceholder (content, "zoom_reset", "ctrl+0");
        content = jam::Text::replaceholder (content, "new_window", "ctrl+n");
        content = jam::Text::replaceholder (content, "new_tab", "ctrl+t");
        content = jam::Text::replaceholder (content, "prev_tab", "ctrl+[");
        content = jam::Text::replaceholder (content, "next_tab", "ctrl+]");
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
        content = jam::Text::replaceholder (content, "popup_cols", juce::String (popup.defaultCols.value));
        content = jam::Text::replaceholder (content, "popup_rows", juce::String (popup.defaultRows.value));
        content = jam::Text::replaceholder (content, "popup_position", popup.defaultPosition);
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
        content = jam::Text::replaceholder (content, "font_family", whelmed.fontFamily);
        content = jam::Text::replaceholder (content, "font_style", whelmed.fontStyle);
        content = jam::Text::replaceholder (content, "font_size", juce::String (whelmed.fontSize));
        content = jam::Text::replaceholder (content, "code_family", whelmed.codeFamily);
        content = jam::Text::replaceholder (content, "code_style", whelmed.codeStyle);
        content = jam::Text::replaceholder (content, "code_size", juce::String (whelmed.codeSize));
        content = jam::Text::replaceholder (content, "line_height", juce::String (whelmed.lineHeight));

        // Heading sizes
        content = jam::Text::replaceholder (content, "h1_size", juce::String (whelmed.h1Size));
        content = jam::Text::replaceholder (content, "h2_size", juce::String (whelmed.h2Size));
        content = jam::Text::replaceholder (content, "h3_size", juce::String (whelmed.h3Size));
        content = jam::Text::replaceholder (content, "h4_size", juce::String (whelmed.h4Size));
        content = jam::Text::replaceholder (content, "h5_size", juce::String (whelmed.h5Size));
        content = jam::Text::replaceholder (content, "h6_size", juce::String (whelmed.h6Size));

        // Layout
        content = jam::Text::replaceholder (content, "padding_top", juce::String (whelmed.paddingTop));
        content = jam::Text::replaceholder (content, "padding_right", juce::String (whelmed.paddingRight));
        content = jam::Text::replaceholder (content, "padding_bottom", juce::String (whelmed.paddingBottom));
        content = jam::Text::replaceholder (content, "padding_left", juce::String (whelmed.paddingLeft));

        // Colours
        content = jam::Text::replaceholder (content, "background", colourToWhelmedHex (whelmed.background));
        content = jam::Text::replaceholder (content, "body_colour", colourToWhelmedHex (whelmed.bodyColour));
        content = jam::Text::replaceholder (content, "link_colour", colourToWhelmedHex (whelmed.linkColour));
        content = jam::Text::replaceholder (content, "h1_colour", colourToWhelmedHex (whelmed.h1Colour));
        content = jam::Text::replaceholder (content, "h2_colour", colourToWhelmedHex (whelmed.h2Colour));
        content = jam::Text::replaceholder (content, "h3_colour", colourToWhelmedHex (whelmed.h3Colour));
        content = jam::Text::replaceholder (content, "h4_colour", colourToWhelmedHex (whelmed.h4Colour));
        content = jam::Text::replaceholder (content, "h5_colour", colourToWhelmedHex (whelmed.h5Colour));
        content = jam::Text::replaceholder (content, "h6_colour", colourToWhelmedHex (whelmed.h6Colour));

        // Code blocks
        content = jam::Text::replaceholder (
            content, "code_fence_background", colourToWhelmedHex (whelmed.codeFenceBackground));
        content = jam::Text::replaceholder (content, "code_colour", colourToWhelmedHex (whelmed.codeColour));

        // Syntax tokens
        content = jam::Text::replaceholder (content, "token_error", colourToWhelmedHex (whelmed.tokenError));
        content = jam::Text::replaceholder (content, "token_comment", colourToWhelmedHex (whelmed.tokenComment));
        content = jam::Text::replaceholder (content, "token_keyword", colourToWhelmedHex (whelmed.tokenKeyword));
        content = jam::Text::replaceholder (content, "token_operator", colourToWhelmedHex (whelmed.tokenOperator));
        content =
            jam::Text::replaceholder (content, "token_identifier", colourToWhelmedHex (whelmed.tokenIdentifier));
        content = jam::Text::replaceholder (content, "token_integer", colourToWhelmedHex (whelmed.tokenInteger));
        content = jam::Text::replaceholder (content, "token_float", colourToWhelmedHex (whelmed.tokenFloat));
        content = jam::Text::replaceholder (content, "token_string", colourToWhelmedHex (whelmed.tokenString));
        content = jam::Text::replaceholder (content, "token_bracket", colourToWhelmedHex (whelmed.tokenBracket));
        content =
            jam::Text::replaceholder (content, "token_punctuation", colourToWhelmedHex (whelmed.tokenPunctuation));
        content =
            jam::Text::replaceholder (content, "token_preprocessor", colourToWhelmedHex (whelmed.tokenPreprocessor));

        // Table
        content =
            jam::Text::replaceholder (content, "table_background", colourToWhelmedHex (whelmed.tableBackground));
        content = jam::Text::replaceholder (
            content, "table_header_background", colourToWhelmedHex (whelmed.tableHeaderBackground));
        content = jam::Text::replaceholder (content, "table_row_alt", colourToWhelmedHex (whelmed.tableRowAlt));
        content =
            jam::Text::replaceholder (content, "table_border_colour", colourToWhelmedHex (whelmed.tableBorderColour));
        content =
            jam::Text::replaceholder (content, "table_header_text", colourToWhelmedHex (whelmed.tableHeaderText));
        content = jam::Text::replaceholder (content, "table_cell_text", colourToWhelmedHex (whelmed.tableCellText));

        // Progress bar
        content = jam::Text::replaceholder (
            content, "progress_background", colourToWhelmedHex (whelmed.progressBackground));
        content = jam::Text::replaceholder (
            content, "progress_foreground", colourToWhelmedHex (whelmed.progressForeground));
        content = jam::Text::replaceholder (
            content, "progress_text_colour", colourToWhelmedHex (whelmed.progressTextColour));
        content = jam::Text::replaceholder (
            content, "progress_spinner_colour", colourToWhelmedHex (whelmed.progressSpinnerColour));

        // Scrollbar
        content = jam::Text::replaceholder (content, "scrollbar_thumb", colourToWhelmedHex (whelmed.scrollbarThumb));
        content = jam::Text::replaceholder (content, "scrollbar_track", colourToWhelmedHex (whelmed.scrollbarTrack));
        content = jam::Text::replaceholder (
            content, "scrollbar_background", colourToWhelmedHex (whelmed.scrollbarBackground));
        content =
            jam::Text::replaceholder (content, "selection_colour", colourToWhelmedHex (whelmed.selectionColour));

        // Navigation
        content = jam::Text::replaceholder (content, "scroll_down", whelmed.scrollDown);
        content = jam::Text::replaceholder (content, "scroll_up", whelmed.scrollUp);
        content = jam::Text::replaceholder (content, "scroll_top", whelmed.scrollTop);
        content = jam::Text::replaceholder (content, "scroll_bottom", whelmed.scrollBottom);
        content = jam::Text::replaceholder (content, "scroll_step", juce::String (whelmed.scrollStep));

        file.replaceWithText (content);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
