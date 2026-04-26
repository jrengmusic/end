/**
 * @file EngineParseDisplay.cpp
 * @brief Lua table parse methods for lua::Engine display configuration.
 *
 * Contains: Engine::parseDisplay() and its 6 static helpers:
 * parseDisplayWindow, parseDisplayColours, parseDisplayCursor,
 * parseDisplayFont, parseDisplayTab, parseDisplayMisc.
 *
 * @see lua::Engine
 */

#include <jam_lua/jam_lua.h>

#include "Engine.h"
#include "../action/Action.h"

namespace lua
{

//==============================================================================
static void parseDisplayWindow (Engine::Display::Window& window, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["window"] };

    if (t.isTable())
    {
        auto title { t["title"].optional<juce::String>() };
        if (title.has_value()) window.title = title.value();

        auto width { t["width"].optional<double>() };
        if (width.has_value()) window.width = static_cast<int> (width.value());

        auto height { t["height"].optional<double>() };
        if (height.has_value()) window.height = static_cast<int> (height.value());

        auto colour { t["colour"].optional<juce::String>() };
        if (colour.has_value()) window.colour = Engine::parseColour (colour.value());

        auto opacity { t["opacity"].optional<double>() };
        if (opacity.has_value())
            window.opacity = static_cast<float> (juce::jlimit (0.0, 1.0, opacity.value()));

        auto blurRadius { t["blur_radius"].optional<double>() };
        if (blurRadius.has_value())
            window.blurRadius = static_cast<float> (juce::jlimit (0.0, 100.0, blurRadius.value()));

        auto alwaysOnTop { t["always_on_top"].optional<juce::String>() };
        if (alwaysOnTop.has_value()) window.alwaysOnTop = (alwaysOnTop.value() == "true");

        auto buttons { t["buttons"].optional<juce::String>() };
        if (buttons.has_value()) window.buttons = (buttons.value() == "true");

        auto forceDwm { t["force_dwm"].optional<juce::String>() };
        if (forceDwm.has_value()) window.forceDwm = (forceDwm.value() == "true");

        auto saveSize { t["save_size"].optional<juce::String>() };
        if (saveSize.has_value()) window.saveSize = (saveSize.value() == "true");

        auto confirmationOnExit { t["confirmation_on_exit"].optional<juce::String>() };
        if (confirmationOnExit.has_value())
            window.confirmationOnExit = (confirmationOnExit.value() == "true");
    }
}

static void parseDisplayColours (Engine::Display::Colours& colours, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["colours"] };

    if (t.isTable())
    {
        auto readColour = [&t] (const char* key, juce::Colour& target)
        {
            auto val { t[key].optional<juce::String>() };
            if (val.has_value()) target = Engine::parseColour (val.value());
        };

        readColour ("foreground",           colours.foreground);
        readColour ("background",           colours.background);
        readColour ("cursor",               colours.cursor);
        readColour ("selection",            colours.selection);
        readColour ("selection_cursor",     colours.selectionCursor);
        readColour ("status_bar",           colours.statusBar);
        readColour ("status_bar_label_bg",  colours.statusBarLabelBg);
        readColour ("status_bar_label_fg",  colours.statusBarLabelFg);
        readColour ("status_bar_spinner",   colours.statusBarSpinner);
        readColour ("hint_label_bg",        colours.hintLabelBg);
        readColour ("hint_label_fg",        colours.hintLabelFg);

        static const std::array<const char*, 16> ansiNames {{
            "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
            "bright_black", "bright_red", "bright_green", "bright_yellow",
            "bright_blue", "bright_magenta", "bright_cyan", "bright_white"
        }};

        for (size_t i { 0 }; i < 16; ++i)
        {
            auto val { t[ansiNames.at (i)].optional<juce::String>() };
            if (val.has_value()) colours.ansi.at (i) = Engine::parseColour (val.value());
        }
    }
}

static void parseDisplayCursor (Engine::Display::Cursor& cursor, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["cursor"] };

    if (t.isTable())
    {
        auto charVal { t["char"].optional<juce::String>() };

        if (charVal.has_value())
        {
            const juce::String charStr { charVal.value() };

            if (charStr.isNotEmpty())
                cursor.codepoint = static_cast<uint32_t> (charStr[0]);
        }

        auto blink { t["blink"].optional<juce::String>() };
        if (blink.has_value()) cursor.blink = (blink.value() == "true");

        auto blinkInterval { t["blink_interval"].optional<double>() };
        if (blinkInterval.has_value())
            cursor.blinkInterval = juce::jlimit (100, 5000, static_cast<int> (blinkInterval.value()));

        auto force { t["force"].optional<juce::String>() };
        if (force.has_value()) cursor.force = (force.value() == "true");
    }
}

static void parseDisplayFont (Engine::Display::Font& font, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["font"] };

    if (t.isTable())
    {
        auto family { t["family"].optional<juce::String>() };
        if (family.has_value()) font.family = family.value();

        auto size { t["size"].optional<double>() };
        if (size.has_value())
            font.size = static_cast<float> (juce::jlimit (1.0, 200.0, size.value()));

        auto ligatures { t["ligatures"].optional<juce::String>() };
        if (ligatures.has_value()) font.ligatures = (ligatures.value() == "true");

        auto embolden { t["embolden"].optional<juce::String>() };
        if (embolden.has_value()) font.embolden = (embolden.value() == "true");

        auto lineHeight { t["line_height"].optional<double>() };
        if (lineHeight.has_value())
            font.lineHeight = static_cast<float> (juce::jlimit (0.5, 3.0, lineHeight.value()));

        auto cellWidth { t["cell_width"].optional<double>() };
        if (cellWidth.has_value())
            font.cellWidth = static_cast<float> (juce::jlimit (0.5, 3.0, cellWidth.value()));

        auto desktopScale { t["desktop_scale"].optional<juce::String>() };
        if (desktopScale.has_value()) font.desktopScale = (desktopScale.value() == "true");
    }
}

static void parseDisplayTab (Engine::Display::Tab& tab, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["tab"] };

    if (t.isTable())
    {
        auto family { t["family"].optional<juce::String>() };
        if (family.has_value()) tab.family = family.value();

        auto size { t["size"].optional<double>() };
        if (size.has_value())
            tab.size = static_cast<float> (juce::jlimit (1.0, 200.0, size.value()));

        auto readColour = [&t] (const char* key, juce::Colour& target)
        {
            auto val { t[key].optional<juce::String>() };
            if (val.has_value()) target = Engine::parseColour (val.value());
        };

        readColour ("foreground", tab.foreground);
        readColour ("inactive",   tab.inactive);
        readColour ("line",       tab.line);
        readColour ("active",     tab.active);
        readColour ("indicator",  tab.indicator);

        auto position { t["position"].optional<juce::String>() };
        if (position.has_value()) tab.position = position.value();

        auto buttonSvg { t["button_svg"].optional<juce::String>() };
        if (buttonSvg.has_value()) tab.buttonSvg = buttonSvg.value();
    }
}

static void parseDisplayMisc (Engine::Display& display, jam::lua::Value& displayTable)
{
    // Pane sub-table
    jam::lua::Value paneTable { displayTable["pane"] };

    if (paneTable.isTable())
    {
        auto barColour { paneTable["bar_colour"].optional<juce::String>() };
        if (barColour.has_value()) display.pane.barColour = Engine::parseColour (barColour.value());

        auto barHighlight { paneTable["bar_highlight"].optional<juce::String>() };
        if (barHighlight.has_value()) display.pane.barHighlight = Engine::parseColour (barHighlight.value());
    }

    // Overlay sub-table
    jam::lua::Value overlayTable { displayTable["overlay"] };

    if (overlayTable.isTable())
    {
        auto family { overlayTable["family"].optional<juce::String>() };
        if (family.has_value()) display.overlay.family = family.value();

        auto size { overlayTable["size"].optional<double>() };
        if (size.has_value())
            display.overlay.size = static_cast<float> (juce::jlimit (1.0, 200.0, size.value()));

        auto colour { overlayTable["colour"].optional<juce::String>() };
        if (colour.has_value()) display.overlay.colour = Engine::parseColour (colour.value());
    }

    // Menu sub-table
    jam::lua::Value menuTable { displayTable["menu"] };

    if (menuTable.isTable())
    {
        auto opacity { menuTable["opacity"].optional<double>() };
        if (opacity.has_value())
            display.menu.opacity = static_cast<float> (juce::jlimit (0.0, 1.0, opacity.value()));
    }

    // Action list sub-table
    jam::lua::Value t { displayTable["action_list"] };

    if (t.isTable())
    {
        auto closeOnRun { t["close_on_run"].optional<juce::String>() };
        if (closeOnRun.has_value()) display.actionList.closeOnRun = (closeOnRun.value() == "true");

        auto position { t["position"].optional<juce::String>() };
        if (position.has_value()) display.actionList.position = position.value();

        auto nameFamily { t["name_font_family"].optional<juce::String>() };
        if (nameFamily.has_value()) display.actionList.nameFamily = nameFamily.value();

        auto nameStyle { t["name_font_style"].optional<juce::String>() };
        if (nameStyle.has_value()) display.actionList.nameStyle = nameStyle.value();

        auto nameSize { t["name_font_size"].optional<double>() };
        if (nameSize.has_value())
            display.actionList.nameSize = static_cast<float> (juce::jlimit (6.0, 72.0, nameSize.value()));

        auto shortcutFamily { t["shortcut_font_family"].optional<juce::String>() };
        if (shortcutFamily.has_value()) display.actionList.shortcutFamily = shortcutFamily.value();

        auto shortcutStyle { t["shortcut_font_style"].optional<juce::String>() };
        if (shortcutStyle.has_value()) display.actionList.shortcutStyle = shortcutStyle.value();

        auto shortcutSize { t["shortcut_font_size"].optional<double>() };
        if (shortcutSize.has_value())
            display.actionList.shortcutSize = static_cast<float> (juce::jlimit (6.0, 72.0, shortcutSize.value()));

        jam::lua::Value p { t["padding"] };

        if (p.isTable())
        {
            auto paddingTop { p[1].optional<double>() };
            if (paddingTop.has_value())
                display.actionList.paddingTop = juce::jlimit (0, 50, static_cast<int> (paddingTop.value()));

            auto paddingRight { p[2].optional<double>() };
            if (paddingRight.has_value())
                display.actionList.paddingRight = juce::jlimit (0, 50, static_cast<int> (paddingRight.value()));

            auto paddingBottom { p[3].optional<double>() };
            if (paddingBottom.has_value())
                display.actionList.paddingBottom = juce::jlimit (0, 50, static_cast<int> (paddingBottom.value()));

            auto paddingLeft { p[4].optional<double>() };
            if (paddingLeft.has_value())
                display.actionList.paddingLeft = juce::jlimit (0, 50, static_cast<int> (paddingLeft.value()));
        }

        auto nameColour { t["name_colour"].optional<juce::String>() };
        if (nameColour.has_value()) display.actionList.nameColour = Engine::parseColour (nameColour.value());

        auto shortcutColour { t["shortcut_colour"].optional<juce::String>() };
        if (shortcutColour.has_value())
            display.actionList.shortcutColour = Engine::parseColour (shortcutColour.value());

        auto width { t["width"].optional<double>() };
        if (width.has_value())
            display.actionList.width = static_cast<float> (juce::jlimit (0.1, 1.0, width.value()));

        auto height { t["height"].optional<double>() };
        if (height.has_value())
            display.actionList.height = static_cast<float> (juce::jlimit (0.1, 1.0, height.value()));

        auto highlightColour { t["highlight_colour"].optional<juce::String>() };
        if (highlightColour.has_value())
            display.actionList.highlightColour = Engine::parseColour (highlightColour.value());
    }

    // Status bar sub-table
    jam::lua::Value statusBarTable { displayTable["status_bar"] };

    if (statusBarTable.isTable())
    {
        auto position { statusBarTable["position"].optional<juce::String>() };
        if (position.has_value()) display.statusBar.position = position.value();

        auto fontFamily { statusBarTable["font_family"].optional<juce::String>() };
        if (fontFamily.has_value()) display.statusBar.fontFamily = fontFamily.value();

        auto fontSize { statusBarTable["font_size"].optional<double>() };
        if (fontSize.has_value())
            display.statusBar.fontSize = static_cast<float> (juce::jlimit (6.0, 48.0, fontSize.value()));

        auto fontStyle { statusBarTable["font_style"].optional<juce::String>() };
        if (fontStyle.has_value()) display.statusBar.fontStyle = fontStyle.value();
    }

    // Popup border sub-table
    jam::lua::Value popupBorderTable { displayTable["popup"] };

    if (popupBorderTable.isTable())
    {
        auto borderColour { popupBorderTable["border_colour"].optional<juce::String>() };
        if (borderColour.has_value()) display.popup.borderColour = Engine::parseColour (borderColour.value());

        auto borderWidth { popupBorderTable["border_width"].optional<double>() };
        if (borderWidth.has_value())
            display.popup.borderWidth = static_cast<float> (juce::jlimit (0.0, 10.0, borderWidth.value()));
    }
}

//==============================================================================
void Engine::parseDisplay()
{
    jam::lua::Value displayTable { lua["END"]["display"] };

    if (displayTable.isTable())
    {
        parseDisplayWindow  (display.window,  displayTable);
        parseDisplayColours (display.colours, displayTable);
        parseDisplayCursor  (display.cursor,  displayTable);
        parseDisplayFont    (display.font,    displayTable);
        parseDisplayTab     (display.tab,     displayTable);
        parseDisplayMisc    (display,         displayTable);
    }
}

} // namespace lua
