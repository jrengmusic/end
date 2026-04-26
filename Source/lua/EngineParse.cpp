/**
 * @file EngineParse.cpp
 * @brief Lua table parse methods for lua::Engine.
 *
 * Contains: Engine::parseNexus(), Engine::parseDisplay(),
 * Engine::parseWhelmed(), Engine::parseKeys(), Engine::parsePopups(),
 * Engine::parseActions(), Engine::parseSelectionKeys().
 *
 * @see lua::Engine
 */

#include <jam_lua/jam_lua.h>

#include "Engine.h"
#include "../action/Action.h"

namespace lua
{

//==============================================================================
void Engine::parseNexus()
{
    jam::lua::optional<jam::lua::table> nexusOpt { lua["END"]["nexus"].get<jam::lua::optional<jam::lua::table>>() };

    if (nexusOpt.has_value())
    {
        auto& nexusTable { nexusOpt.value() };

        jam::lua::optional<std::string> gpu { nexusTable["gpu"].get<jam::lua::optional<std::string>>() };
        if (gpu.has_value()) nexus.gpu = juce::String (gpu.value());

        jam::lua::optional<std::string> daemon { nexusTable["daemon"].get<jam::lua::optional<std::string>>() };
        if (daemon.has_value()) nexus.daemon = (daemon.value() == "true");

        jam::lua::optional<std::string> autoReload { nexusTable["auto_reload"].get<jam::lua::optional<std::string>>() };
        if (autoReload.has_value()) nexus.autoReload = (autoReload.value() == "true");

        // Shell sub-table
        jam::lua::optional<jam::lua::table> shellOpt { nexusTable["shell"].get<jam::lua::optional<jam::lua::table>>() };

        if (shellOpt.has_value())
        {
            auto& shellTable { shellOpt.value() };

            jam::lua::optional<std::string> program { shellTable["program"].get<jam::lua::optional<std::string>>() };
            if (program.has_value()) nexus.shell.program = juce::String (program.value());

            jam::lua::optional<std::string> args { shellTable["args"].get<jam::lua::optional<std::string>>() };
            if (args.has_value()) nexus.shell.args = juce::String (args.value());

            jam::lua::optional<std::string> integration { shellTable["integration"].get<jam::lua::optional<std::string>>() };
            if (integration.has_value()) nexus.shell.integration = (integration.value() == "true");
        }

        // Terminal sub-table
        jam::lua::optional<jam::lua::table> terminalOpt { nexusTable["terminal"].get<jam::lua::optional<jam::lua::table>>() };

        if (terminalOpt.has_value())
        {
            auto& terminalTable { terminalOpt.value() };

            jam::lua::optional<double> scrollbackLines { terminalTable["scrollback_lines"].get<jam::lua::optional<double>>() };
            if (scrollbackLines.has_value())
                nexus.terminal.scrollbackLines = juce::jlimit (100, 1000000, static_cast<int> (scrollbackLines.value()));

            jam::lua::optional<double> scrollStep { terminalTable["scroll_step"].get<jam::lua::optional<double>>() };
            if (scrollStep.has_value())
                nexus.terminal.scrollStep = juce::jlimit (1, 100, static_cast<int> (scrollStep.value()));

            jam::lua::optional<std::string> dropMultifiles { terminalTable["drop_multifiles"].get<jam::lua::optional<std::string>>() };
            if (dropMultifiles.has_value()) nexus.terminal.dropMultifiles = juce::String (dropMultifiles.value());

            jam::lua::optional<std::string> dropQuoted { terminalTable["drop_quoted"].get<jam::lua::optional<std::string>>() };
            if (dropQuoted.has_value()) nexus.terminal.dropQuoted = (dropQuoted.value() == "true");

            // Padding array: { top, right, bottom, left }
            jam::lua::optional<jam::lua::table> paddingOpt { terminalTable["padding"].get<jam::lua::optional<jam::lua::table>>() };

            if (paddingOpt.has_value())
            {
                auto& p { paddingOpt.value() };

                jam::lua::optional<double> paddingTop { p[1].get<jam::lua::optional<double>>() };
                if (paddingTop.has_value())
                    nexus.terminal.paddingTop = juce::jlimit (0, 50, static_cast<int> (paddingTop.value()));

                jam::lua::optional<double> paddingRight { p[2].get<jam::lua::optional<double>>() };
                if (paddingRight.has_value())
                    nexus.terminal.paddingRight = juce::jlimit (0, 50, static_cast<int> (paddingRight.value()));

                jam::lua::optional<double> paddingBottom { p[3].get<jam::lua::optional<double>>() };
                if (paddingBottom.has_value())
                    nexus.terminal.paddingBottom = juce::jlimit (0, 50, static_cast<int> (paddingBottom.value()));

                jam::lua::optional<double> paddingLeft { p[4].get<jam::lua::optional<double>>() };
                if (paddingLeft.has_value())
                    nexus.terminal.paddingLeft = juce::jlimit (0, 50, static_cast<int> (paddingLeft.value()));
            }
        }

        // Hyperlinks sub-table
        jam::lua::optional<jam::lua::table> hyperlinksOpt { nexusTable["hyperlinks"].get<jam::lua::optional<jam::lua::table>>() };

        if (hyperlinksOpt.has_value())
        {
            auto& hyperlinksTable { hyperlinksOpt.value() };

            jam::lua::optional<std::string> editor { hyperlinksTable["editor"].get<jam::lua::optional<std::string>>() };
            if (editor.has_value()) nexus.hyperlinks.editor = juce::String (editor.value());

            // Handlers map
            jam::lua::optional<jam::lua::table> handlersOpt { hyperlinksTable["handlers"].get<jam::lua::optional<jam::lua::table>>() };

            if (handlersOpt.has_value())
            {
                nexus.hyperlinks.handlers[".md"] = "whelmed";

                handlersOpt.value().for_each ([this] (const jam::lua::object& k, const jam::lua::object& v)
                {
                    if (k.get_type() == jam::lua::type::string and v.get_type() == jam::lua::type::string)
                        nexus.hyperlinks.handlers[juce::String (k.as<std::string>())] = juce::String (v.as<std::string>());
                });
            }

            // Extensions set
            jam::lua::optional<jam::lua::table> extensionsOpt { hyperlinksTable["extensions"].get<jam::lua::optional<jam::lua::table>>() };

            if (extensionsOpt.has_value())
            {
                extensionsOpt.value().for_each ([this] (const jam::lua::object& /*k*/, const jam::lua::object& v)
                {
                    if (v.get_type() == jam::lua::type::string)
                        nexus.hyperlinks.extensions.insert (juce::String (v.as<std::string>()));
                });
            }
        }
    }
}

//==============================================================================
static void parseDisplayWindow (Engine::Display::Window& window, jam::lua::table& displayTable)
{
    jam::lua::optional<jam::lua::table> windowOpt { displayTable["window"].get<jam::lua::optional<jam::lua::table>>() };

    if (windowOpt.has_value())
    {
        auto& t { windowOpt.value() };

        jam::lua::optional<std::string> title { t["title"].get<jam::lua::optional<std::string>>() };
        if (title.has_value()) window.title = juce::String (title.value());

        jam::lua::optional<double> width { t["width"].get<jam::lua::optional<double>>() };
        if (width.has_value()) window.width = static_cast<int> (width.value());

        jam::lua::optional<double> height { t["height"].get<jam::lua::optional<double>>() };
        if (height.has_value()) window.height = static_cast<int> (height.value());

        jam::lua::optional<std::string> colour { t["colour"].get<jam::lua::optional<std::string>>() };
        if (colour.has_value()) window.colour = Engine::parseColour (juce::String (colour.value()));

        jam::lua::optional<double> opacity { t["opacity"].get<jam::lua::optional<double>>() };
        if (opacity.has_value())
            window.opacity = static_cast<float> (juce::jlimit (0.0, 1.0, opacity.value()));

        jam::lua::optional<double> blurRadius { t["blur_radius"].get<jam::lua::optional<double>>() };
        if (blurRadius.has_value())
            window.blurRadius = static_cast<float> (juce::jlimit (0.0, 100.0, blurRadius.value()));

        jam::lua::optional<std::string> alwaysOnTop { t["always_on_top"].get<jam::lua::optional<std::string>>() };
        if (alwaysOnTop.has_value()) window.alwaysOnTop = (alwaysOnTop.value() == "true");

        jam::lua::optional<std::string> buttons { t["buttons"].get<jam::lua::optional<std::string>>() };
        if (buttons.has_value()) window.buttons = (buttons.value() == "true");

        jam::lua::optional<std::string> forceDwm { t["force_dwm"].get<jam::lua::optional<std::string>>() };
        if (forceDwm.has_value()) window.forceDwm = (forceDwm.value() == "true");

        jam::lua::optional<std::string> saveSize { t["save_size"].get<jam::lua::optional<std::string>>() };
        if (saveSize.has_value()) window.saveSize = (saveSize.value() == "true");

        jam::lua::optional<std::string> confirmationOnExit { t["confirmation_on_exit"].get<jam::lua::optional<std::string>>() };
        if (confirmationOnExit.has_value())
            window.confirmationOnExit = (confirmationOnExit.value() == "true");
    }
}

static void parseDisplayColours (Engine::Display::Colours& colours, jam::lua::table& displayTable)
{
    jam::lua::optional<jam::lua::table> coloursOpt { displayTable["colours"].get<jam::lua::optional<jam::lua::table>>() };

    if (coloursOpt.has_value())
    {
        auto& t { coloursOpt.value() };

        auto readColour = [&t] (const char* key, juce::Colour& target)
        {
            jam::lua::optional<std::string> val { t[key].get<jam::lua::optional<std::string>>() };
            if (val.has_value()) target = Engine::parseColour (juce::String (val.value()));
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
            jam::lua::optional<std::string> val { t[ansiNames.at (i)].get<jam::lua::optional<std::string>>() };
            if (val.has_value()) colours.ansi.at (i) = Engine::parseColour (juce::String (val.value()));
        }
    }
}

static void parseDisplayCursor (Engine::Display::Cursor& cursor, jam::lua::table& displayTable)
{
    jam::lua::optional<jam::lua::table> cursorOpt { displayTable["cursor"].get<jam::lua::optional<jam::lua::table>>() };

    if (cursorOpt.has_value())
    {
        auto& t { cursorOpt.value() };

        jam::lua::optional<std::string> charVal { t["char"].get<jam::lua::optional<std::string>>() };

        if (charVal.has_value())
        {
            const juce::String charStr { charVal.value() };

            if (charStr.isNotEmpty())
                cursor.codepoint = static_cast<uint32_t> (charStr[0]);
        }

        jam::lua::optional<std::string> blink { t["blink"].get<jam::lua::optional<std::string>>() };
        if (blink.has_value()) cursor.blink = (blink.value() == "true");

        jam::lua::optional<double> blinkInterval { t["blink_interval"].get<jam::lua::optional<double>>() };
        if (blinkInterval.has_value())
            cursor.blinkInterval = juce::jlimit (100, 5000, static_cast<int> (blinkInterval.value()));

        jam::lua::optional<std::string> force { t["force"].get<jam::lua::optional<std::string>>() };
        if (force.has_value()) cursor.force = (force.value() == "true");
    }
}

static void parseDisplayFont (Engine::Display::Font& font, jam::lua::table& displayTable)
{
    jam::lua::optional<jam::lua::table> fontOpt { displayTable["font"].get<jam::lua::optional<jam::lua::table>>() };

    if (fontOpt.has_value())
    {
        auto& t { fontOpt.value() };

        jam::lua::optional<std::string> family { t["family"].get<jam::lua::optional<std::string>>() };
        if (family.has_value()) font.family = juce::String (family.value());

        jam::lua::optional<double> size { t["size"].get<jam::lua::optional<double>>() };
        if (size.has_value())
            font.size = static_cast<float> (juce::jlimit (1.0, 200.0, size.value()));

        jam::lua::optional<std::string> ligatures { t["ligatures"].get<jam::lua::optional<std::string>>() };
        if (ligatures.has_value()) font.ligatures = (ligatures.value() == "true");

        jam::lua::optional<std::string> embolden { t["embolden"].get<jam::lua::optional<std::string>>() };
        if (embolden.has_value()) font.embolden = (embolden.value() == "true");

        jam::lua::optional<double> lineHeight { t["line_height"].get<jam::lua::optional<double>>() };
        if (lineHeight.has_value())
            font.lineHeight = static_cast<float> (juce::jlimit (0.5, 3.0, lineHeight.value()));

        jam::lua::optional<double> cellWidth { t["cell_width"].get<jam::lua::optional<double>>() };
        if (cellWidth.has_value())
            font.cellWidth = static_cast<float> (juce::jlimit (0.5, 3.0, cellWidth.value()));

        jam::lua::optional<std::string> desktopScale { t["desktop_scale"].get<jam::lua::optional<std::string>>() };
        if (desktopScale.has_value()) font.desktopScale = (desktopScale.value() == "true");
    }
}

static void parseDisplayTab (Engine::Display::Tab& tab, jam::lua::table& displayTable)
{
    jam::lua::optional<jam::lua::table> tabOpt { displayTable["tab"].get<jam::lua::optional<jam::lua::table>>() };

    if (tabOpt.has_value())
    {
        auto& t { tabOpt.value() };

        jam::lua::optional<std::string> family { t["family"].get<jam::lua::optional<std::string>>() };
        if (family.has_value()) tab.family = juce::String (family.value());

        jam::lua::optional<double> size { t["size"].get<jam::lua::optional<double>>() };
        if (size.has_value())
            tab.size = static_cast<float> (juce::jlimit (1.0, 200.0, size.value()));

        auto readColour = [&t] (const char* key, juce::Colour& target)
        {
            jam::lua::optional<std::string> val { t[key].get<jam::lua::optional<std::string>>() };
            if (val.has_value()) target = Engine::parseColour (juce::String (val.value()));
        };

        readColour ("foreground", tab.foreground);
        readColour ("inactive",   tab.inactive);
        readColour ("line",       tab.line);
        readColour ("active",     tab.active);
        readColour ("indicator",  tab.indicator);

        jam::lua::optional<std::string> position { t["position"].get<jam::lua::optional<std::string>>() };
        if (position.has_value()) tab.position = juce::String (position.value());
    }
}

static void parseDisplayMisc (Engine::Display& display, jam::lua::table& displayTable)
{
    // Pane sub-table
    jam::lua::optional<jam::lua::table> paneOpt { displayTable["pane"].get<jam::lua::optional<jam::lua::table>>() };

    if (paneOpt.has_value())
    {
        auto& t { paneOpt.value() };

        jam::lua::optional<std::string> barColour { t["bar_colour"].get<jam::lua::optional<std::string>>() };
        if (barColour.has_value()) display.pane.barColour = Engine::parseColour (juce::String (barColour.value()));

        jam::lua::optional<std::string> barHighlight { t["bar_highlight"].get<jam::lua::optional<std::string>>() };
        if (barHighlight.has_value()) display.pane.barHighlight = Engine::parseColour (juce::String (barHighlight.value()));
    }

    // Overlay sub-table
    jam::lua::optional<jam::lua::table> overlayOpt { displayTable["overlay"].get<jam::lua::optional<jam::lua::table>>() };

    if (overlayOpt.has_value())
    {
        auto& t { overlayOpt.value() };

        jam::lua::optional<std::string> family { t["family"].get<jam::lua::optional<std::string>>() };
        if (family.has_value()) display.overlay.family = juce::String (family.value());

        jam::lua::optional<double> size { t["size"].get<jam::lua::optional<double>>() };
        if (size.has_value())
            display.overlay.size = static_cast<float> (juce::jlimit (1.0, 200.0, size.value()));

        jam::lua::optional<std::string> colour { t["colour"].get<jam::lua::optional<std::string>>() };
        if (colour.has_value()) display.overlay.colour = Engine::parseColour (juce::String (colour.value()));
    }

    // Menu sub-table
    jam::lua::optional<jam::lua::table> menuOpt { displayTable["menu"].get<jam::lua::optional<jam::lua::table>>() };

    if (menuOpt.has_value())
    {
        jam::lua::optional<double> opacity { menuOpt.value()["opacity"].get<jam::lua::optional<double>>() };
        if (opacity.has_value())
            display.menu.opacity = static_cast<float> (juce::jlimit (0.0, 1.0, opacity.value()));
    }

    // Action list sub-table
    jam::lua::optional<jam::lua::table> actionListOpt { displayTable["action_list"].get<jam::lua::optional<jam::lua::table>>() };

    if (actionListOpt.has_value())
    {
        auto& t { actionListOpt.value() };

        jam::lua::optional<std::string> closeOnRun { t["close_on_run"].get<jam::lua::optional<std::string>>() };
        if (closeOnRun.has_value()) display.actionList.closeOnRun = (closeOnRun.value() == "true");

        jam::lua::optional<std::string> position { t["position"].get<jam::lua::optional<std::string>>() };
        if (position.has_value()) display.actionList.position = juce::String (position.value());

        jam::lua::optional<std::string> nameFamily { t["name_font_family"].get<jam::lua::optional<std::string>>() };
        if (nameFamily.has_value()) display.actionList.nameFamily = juce::String (nameFamily.value());

        jam::lua::optional<std::string> nameStyle { t["name_font_style"].get<jam::lua::optional<std::string>>() };
        if (nameStyle.has_value()) display.actionList.nameStyle = juce::String (nameStyle.value());

        jam::lua::optional<double> nameSize { t["name_font_size"].get<jam::lua::optional<double>>() };
        if (nameSize.has_value())
            display.actionList.nameSize = static_cast<float> (juce::jlimit (6.0, 72.0, nameSize.value()));

        jam::lua::optional<std::string> shortcutFamily { t["shortcut_font_family"].get<jam::lua::optional<std::string>>() };
        if (shortcutFamily.has_value()) display.actionList.shortcutFamily = juce::String (shortcutFamily.value());

        jam::lua::optional<std::string> shortcutStyle { t["shortcut_font_style"].get<jam::lua::optional<std::string>>() };
        if (shortcutStyle.has_value()) display.actionList.shortcutStyle = juce::String (shortcutStyle.value());

        jam::lua::optional<double> shortcutSize { t["shortcut_font_size"].get<jam::lua::optional<double>>() };
        if (shortcutSize.has_value())
            display.actionList.shortcutSize = static_cast<float> (juce::jlimit (6.0, 72.0, shortcutSize.value()));

        jam::lua::optional<jam::lua::table> paddingOpt { t["padding"].get<jam::lua::optional<jam::lua::table>>() };

        if (paddingOpt.has_value())
        {
            auto& p { paddingOpt.value() };

            jam::lua::optional<double> paddingTop { p[1].get<jam::lua::optional<double>>() };
            if (paddingTop.has_value())
                display.actionList.paddingTop = juce::jlimit (0, 50, static_cast<int> (paddingTop.value()));

            jam::lua::optional<double> paddingRight { p[2].get<jam::lua::optional<double>>() };
            if (paddingRight.has_value())
                display.actionList.paddingRight = juce::jlimit (0, 50, static_cast<int> (paddingRight.value()));

            jam::lua::optional<double> paddingBottom { p[3].get<jam::lua::optional<double>>() };
            if (paddingBottom.has_value())
                display.actionList.paddingBottom = juce::jlimit (0, 50, static_cast<int> (paddingBottom.value()));

            jam::lua::optional<double> paddingLeft { p[4].get<jam::lua::optional<double>>() };
            if (paddingLeft.has_value())
                display.actionList.paddingLeft = juce::jlimit (0, 50, static_cast<int> (paddingLeft.value()));
        }

        jam::lua::optional<std::string> nameColour { t["name_colour"].get<jam::lua::optional<std::string>>() };
        if (nameColour.has_value()) display.actionList.nameColour = Engine::parseColour (juce::String (nameColour.value()));

        jam::lua::optional<std::string> shortcutColour { t["shortcut_colour"].get<jam::lua::optional<std::string>>() };
        if (shortcutColour.has_value())
            display.actionList.shortcutColour = Engine::parseColour (juce::String (shortcutColour.value()));

        jam::lua::optional<double> width { t["width"].get<jam::lua::optional<double>>() };
        if (width.has_value())
            display.actionList.width = static_cast<float> (juce::jlimit (0.1, 1.0, width.value()));

        jam::lua::optional<double> height { t["height"].get<jam::lua::optional<double>>() };
        if (height.has_value())
            display.actionList.height = static_cast<float> (juce::jlimit (0.1, 1.0, height.value()));

        jam::lua::optional<std::string> highlightColour { t["highlight_colour"].get<jam::lua::optional<std::string>>() };
        if (highlightColour.has_value())
            display.actionList.highlightColour = Engine::parseColour (juce::String (highlightColour.value()));
    }

    // Status bar sub-table
    jam::lua::optional<jam::lua::table> statusBarOpt { displayTable["status_bar"].get<jam::lua::optional<jam::lua::table>>() };

    if (statusBarOpt.has_value())
    {
        auto& t { statusBarOpt.value() };

        jam::lua::optional<std::string> position { t["position"].get<jam::lua::optional<std::string>>() };
        if (position.has_value()) display.statusBar.position = juce::String (position.value());

        jam::lua::optional<std::string> fontFamily { t["font_family"].get<jam::lua::optional<std::string>>() };
        if (fontFamily.has_value()) display.statusBar.fontFamily = juce::String (fontFamily.value());

        jam::lua::optional<double> fontSize { t["font_size"].get<jam::lua::optional<double>>() };
        if (fontSize.has_value())
            display.statusBar.fontSize = static_cast<float> (juce::jlimit (6.0, 48.0, fontSize.value()));

        jam::lua::optional<std::string> fontStyle { t["font_style"].get<jam::lua::optional<std::string>>() };
        if (fontStyle.has_value()) display.statusBar.fontStyle = juce::String (fontStyle.value());
    }

    // Popup border sub-table
    jam::lua::optional<jam::lua::table> popupBorderOpt { displayTable["popup"].get<jam::lua::optional<jam::lua::table>>() };

    if (popupBorderOpt.has_value())
    {
        auto& t { popupBorderOpt.value() };

        jam::lua::optional<std::string> borderColour { t["border_colour"].get<jam::lua::optional<std::string>>() };
        if (borderColour.has_value()) display.popup.borderColour = Engine::parseColour (juce::String (borderColour.value()));

        jam::lua::optional<double> borderWidth { t["border_width"].get<jam::lua::optional<double>>() };
        if (borderWidth.has_value())
            display.popup.borderWidth = static_cast<float> (juce::jlimit (0.0, 10.0, borderWidth.value()));
    }
}

//==============================================================================
void Engine::parseDisplay()
{
    jam::lua::optional<jam::lua::table> displayOpt { lua["END"]["display"].get<jam::lua::optional<jam::lua::table>>() };

    if (displayOpt.has_value())
    {
        auto& displayTable { displayOpt.value() };
        parseDisplayWindow  (display.window,  displayTable);
        parseDisplayColours (display.colours, displayTable);
        parseDisplayCursor  (display.cursor,  displayTable);
        parseDisplayFont    (display.font,    displayTable);
        parseDisplayTab     (display.tab,     displayTable);
        parseDisplayMisc    (display,         displayTable);
    }
}

//==============================================================================
void Engine::parseWhelmed()
{
    jam::lua::optional<jam::lua::table> whelmedOpt { lua["END"]["whelmed"].get<jam::lua::optional<jam::lua::table>>() };

    if (whelmedOpt.has_value())
    {
        auto& whelmedTable { whelmedOpt.value() };

        auto readStr = [&whelmedTable] (const char* key, juce::String& target)
        {
            jam::lua::optional<std::string> val { whelmedTable[key].get<jam::lua::optional<std::string>>() };
            if (val.has_value()) target = juce::String (val.value());
        };

        auto readFloat = [&whelmedTable] (const char* key, float& target)
        {
            jam::lua::optional<double> val { whelmedTable[key].get<jam::lua::optional<double>>() };
            if (val.has_value()) target = static_cast<float> (val.value());
        };

        auto readColour = [&whelmedTable] (const char* key, juce::Colour& target)
        {
            jam::lua::optional<std::string> val { whelmedTable[key].get<jam::lua::optional<std::string>>() };
            if (val.has_value()) target = parseColour (juce::String (val.value()));
        };

        auto readInt = [&whelmedTable] (const char* key, int& target)
        {
            jam::lua::optional<double> val { whelmedTable[key].get<jam::lua::optional<double>>() };
            if (val.has_value()) target = static_cast<int> (val.value());
        };

        // Typography
        readStr   ("font_family",  whelmed.fontFamily);
        readStr   ("font_style",   whelmed.fontStyle);
        readFloat ("font_size",    whelmed.fontSize);
        readStr   ("code_family",  whelmed.codeFamily);
        readStr   ("code_style",   whelmed.codeStyle);
        readFloat ("code_size",    whelmed.codeSize);
        readFloat ("line_height",  whelmed.lineHeight);

        // Heading sizes
        readFloat ("h1_size", whelmed.h1Size);
        readFloat ("h2_size", whelmed.h2Size);
        readFloat ("h3_size", whelmed.h3Size);
        readFloat ("h4_size", whelmed.h4Size);
        readFloat ("h5_size", whelmed.h5Size);
        readFloat ("h6_size", whelmed.h6Size);

        // Colours
        readColour ("background",  whelmed.background);
        readColour ("body_colour", whelmed.bodyColour);
        readColour ("code_colour", whelmed.codeColour);
        readColour ("link_colour", whelmed.linkColour);
        readColour ("h1_colour",   whelmed.h1Colour);
        readColour ("h2_colour",   whelmed.h2Colour);
        readColour ("h3_colour",   whelmed.h3Colour);
        readColour ("h4_colour",   whelmed.h4Colour);
        readColour ("h5_colour",   whelmed.h5Colour);
        readColour ("h6_colour",   whelmed.h6Colour);

        readColour ("code_fence_background",  whelmed.codeFenceBackground);
        readColour ("progress_background",    whelmed.progressBackground);
        readColour ("progress_foreground",    whelmed.progressForeground);
        readColour ("progress_text_colour",   whelmed.progressTextColour);
        readColour ("progress_spinner_colour", whelmed.progressSpinnerColour);

        // Syntax tokens
        readColour ("token_error",        whelmed.tokenError);
        readColour ("token_comment",      whelmed.tokenComment);
        readColour ("token_keyword",      whelmed.tokenKeyword);
        readColour ("token_operator",     whelmed.tokenOperator);
        readColour ("token_identifier",   whelmed.tokenIdentifier);
        readColour ("token_integer",      whelmed.tokenInteger);
        readColour ("token_float",        whelmed.tokenFloat);
        readColour ("token_string",       whelmed.tokenString);
        readColour ("token_bracket",      whelmed.tokenBracket);
        readColour ("token_punctuation",  whelmed.tokenPunctuation);
        readColour ("token_preprocessor", whelmed.tokenPreprocessor);

        // Table colours
        readColour ("table_background",        whelmed.tableBackground);
        readColour ("table_header_background", whelmed.tableHeaderBackground);
        readColour ("table_row_alt",           whelmed.tableRowAlt);
        readColour ("table_border_colour",     whelmed.tableBorderColour);
        readColour ("table_header_text",       whelmed.tableHeaderText);
        readColour ("table_cell_text",         whelmed.tableCellText);

        // Scrollbar
        readColour ("scrollbar_thumb",      whelmed.scrollbarThumb);
        readColour ("scrollbar_track",      whelmed.scrollbarTrack);
        readColour ("scrollbar_background", whelmed.scrollbarBackground);

        readColour ("selection_colour", whelmed.selectionColour);

        // Navigation
        readStr ("scroll_down",   whelmed.scrollDown);
        readStr ("scroll_up",     whelmed.scrollUp);
        readStr ("scroll_top",    whelmed.scrollTop);
        readStr ("scroll_bottom", whelmed.scrollBottom);
        readInt ("scroll_step",   whelmed.scrollStep);

        // Padding array: { top, right, bottom, left }
        jam::lua::optional<jam::lua::table> paddingOpt { whelmedTable["padding"].get<jam::lua::optional<jam::lua::table>>() };

        if (paddingOpt.has_value())
        {
            auto& p { paddingOpt.value() };

            jam::lua::optional<double> paddingTop { p[1].get<jam::lua::optional<double>>() };
            if (paddingTop.has_value())
                whelmed.paddingTop = juce::jlimit (0, 50, static_cast<int> (paddingTop.value()));

            jam::lua::optional<double> paddingRight { p[2].get<jam::lua::optional<double>>() };
            if (paddingRight.has_value())
                whelmed.paddingRight = juce::jlimit (0, 50, static_cast<int> (paddingRight.value()));

            jam::lua::optional<double> paddingBottom { p[3].get<jam::lua::optional<double>>() };
            if (paddingBottom.has_value())
                whelmed.paddingBottom = juce::jlimit (0, 50, static_cast<int> (paddingBottom.value()));

            jam::lua::optional<double> paddingLeft { p[4].get<jam::lua::optional<double>>() };
            if (paddingLeft.has_value())
                whelmed.paddingLeft = juce::jlimit (0, 50, static_cast<int> (paddingLeft.value()));
        }
    }
}

//==============================================================================
void Engine::parseKeys()
{
    jam::lua::optional<jam::lua::table> keysOpt { lua["END"]["keys"].get<jam::lua::optional<jam::lua::table>>() };

    if (keysOpt.has_value())
    {
        auto& keysTable { keysOpt.value() };
        const auto& mappings { keyMappings };
        const int count { static_cast<int> (mappings.size()) };

        for (int i { 0 }; i < count; ++i)
        {
            jam::lua::optional<std::string> val { keysTable[mappings.at (i).luaKey].get<jam::lua::optional<std::string>>() };

            if (val.has_value())
            {
                Keys::Binding binding;
                binding.actionId       = juce::String (mappings.at (i).actionId);
                binding.shortcutString = juce::String (val.value());
                binding.isModal        = mappings.at (i).isModal;
                keys.bindings.push_back (std::move (binding));
            }
        }

        jam::lua::optional<std::string> prefix { keysTable["prefix"].get<jam::lua::optional<std::string>>() };
        if (prefix.has_value()) keys.prefix = juce::String (prefix.value());

        jam::lua::optional<int> timeout { keysTable["prefix_timeout"].get<jam::lua::optional<int>>() };
        if (timeout.has_value()) keys.prefixTimeout = timeout.value();
    }
}

//==============================================================================
void Engine::parsePopups()
{
    jam::lua::optional<jam::lua::table> popupsOpt { lua["END"]["popups"].get<jam::lua::optional<jam::lua::table>>() };

    if (popupsOpt.has_value())
    {
        auto& popupsTable { popupsOpt.value() };

        // Read defaults sub-table first
        jam::lua::optional<jam::lua::table> defaultsOpt { popupsTable["defaults"].get<jam::lua::optional<jam::lua::table>>() };

        if (defaultsOpt.has_value())
        {
            auto& defaultsTable { defaultsOpt.value() };

            jam::lua::optional<int> defaultCols { defaultsTable["cols"].get<jam::lua::optional<int>>() };
            if (defaultCols.has_value()) popup.defaultCols = defaultCols.value();

            jam::lua::optional<int> defaultRows { defaultsTable["rows"].get<jam::lua::optional<int>>() };
            if (defaultRows.has_value()) popup.defaultRows = defaultRows.value();

            jam::lua::optional<std::string> defaultPosition { defaultsTable["position"].get<jam::lua::optional<std::string>>() };
            if (defaultPosition.has_value()) popup.defaultPosition = juce::String (defaultPosition.value());
        }

        // Iterate remaining entries (skip "defaults" key)
        for (auto& [key, value] : popupsTable)
        {
            if (value.get_type() == jam::lua::type::table
                and key.get_type() == jam::lua::type::string
                and key.as<std::string>() != "defaults")
            {
                jam::lua::table entry { value };
                Popup::Entry popupEntry;
                popupEntry.name = juce::String (key.as<std::string>());

                jam::lua::optional<std::string> command { entry["command"].get<jam::lua::optional<std::string>>() };

                if (command.has_value())
                {
                    popupEntry.command = juce::String (command.value());

                    jam::lua::optional<std::string> args { entry["args"].get<jam::lua::optional<std::string>>() };
                    if (args.has_value()) popupEntry.args = juce::String (args.value());

                    jam::lua::optional<std::string> cwd { entry["cwd"].get<jam::lua::optional<std::string>>() };
                    if (cwd.has_value()) popupEntry.cwd = juce::String (cwd.value());

                    jam::lua::optional<int> cols { entry["cols"].get<jam::lua::optional<int>>() };
                    if (cols.has_value()) popupEntry.cols = cols.value();

                    jam::lua::optional<int> rows { entry["rows"].get<jam::lua::optional<int>>() };
                    if (rows.has_value()) popupEntry.rows = rows.value();

                    jam::lua::optional<std::string> modal { entry["modal"].get<jam::lua::optional<std::string>>() };
                    if (modal.has_value()) popupEntry.modal = juce::String (modal.value());

                    jam::lua::optional<std::string> global { entry["global"].get<jam::lua::optional<std::string>>() };
                    if (global.has_value()) popupEntry.global = juce::String (global.value());

                    popup.entries.push_back (std::move (popupEntry));
                }
            }
        }
    }
}

//==============================================================================
void Engine::parseActions()
{
    jam::lua::optional<jam::lua::table> actionsOpt { lua["END"]["actions"].get<jam::lua::optional<jam::lua::table>>() };

    if (actionsOpt.has_value())
    {
        auto& actionsTable { actionsOpt.value() };

        for (auto& [key, value] : actionsTable)
        {
            if (value.get_type() == jam::lua::type::table)
            {
                jam::lua::table entry { value };
                Action::Entry actionEntry;
                actionEntry.id = "lua:" + juce::String (key.as<std::string>());

                jam::lua::optional<std::string> name { entry["name"].get<jam::lua::optional<std::string>>() };
                if (name.has_value()) actionEntry.name = juce::String (name.value());

                jam::lua::optional<std::string> desc { entry["description"].get<jam::lua::optional<std::string>>() };
                if (desc.has_value()) actionEntry.description = juce::String (desc.value());

                jam::lua::optional<std::string> modal  { entry["modal"].get<jam::lua::optional<std::string>>() };
                jam::lua::optional<std::string> global { entry["global"].get<jam::lua::optional<std::string>>() };

                if (modal.has_value())
                {
                    actionEntry.shortcut = juce::String (modal.value());
                    actionEntry.isModal  = true;
                }
                else if (global.has_value())
                {
                    actionEntry.shortcut = juce::String (global.value());
                    actionEntry.isModal  = false;
                }

                jam::lua::optional<jam::lua::protected_function> exec { entry["execute"].get<jam::lua::optional<jam::lua::protected_function>>() };

                if (exec.has_value())
                {
                    actionEntry.execute = exec.value();
                    action.entries.push_back (std::move (actionEntry));
                }
            }
        }
    }
}

//==============================================================================
void Engine::parseSelectionKeys()
{
    jam::lua::optional<jam::lua::table> keysOpt { lua["END"]["keys"].get<jam::lua::optional<jam::lua::table>>() };

    if (keysOpt.has_value())
    {
        auto& keysTable { keysOpt.value() };

        auto parse = [] (jam::lua::table& t, const char* field) -> juce::KeyPress
        {
            jam::lua::optional<std::string> val { t[field].get<jam::lua::optional<std::string>>() };
            juce::KeyPress result {};

            if (val.has_value())
                result = ::Action::Registry::parseShortcut (juce::String (val.value()));

            return result;
        };

        keys.selection.up               = parse (keysTable, "selection_up");
        keys.selection.down             = parse (keysTable, "selection_down");
        keys.selection.left             = parse (keysTable, "selection_left");
        keys.selection.right            = parse (keysTable, "selection_right");
        keys.selection.visual           = parse (keysTable, "selection_visual");
        keys.selection.visualLine       = parse (keysTable, "selection_visual_line");
        keys.selection.copy             = parse (keysTable, "selection_copy");
        keys.selection.top              = parse (keysTable, "selection_top");
        keys.selection.bottom           = parse (keysTable, "selection_bottom");
        keys.selection.lineStart        = parse (keysTable, "selection_line_start");
        keys.selection.lineEnd          = parse (keysTable, "selection_line_end");
        keys.selection.exit             = parse (keysTable, "selection_exit");
        keys.selection.openFileNextPage = parse (keysTable, "open_file_next_page");
        keys.selection.globalCopy       = parse (keysTable, "copy");

        // Visual block: special handling for Ctrl on macOS.
        // parseShortcut maps "ctrl" to Cmd on macOS. For visual block we need
        // real ctrlModifier so Ctrl+V doesn't conflict with paste.
        {
            jam::lua::optional<std::string> raw { keysTable["selection_visual_block"].get<jam::lua::optional<std::string>>() };

            if (raw.has_value())
            {
                const juce::String rawStr { raw.value() };
                const bool hasCtrl { rawStr.containsIgnoreCase ("ctrl")
                                     and not rawStr.containsIgnoreCase ("cmd") };

                if (hasCtrl)
                    keys.selection.visualBlock = juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0);
                else
                    keys.selection.visualBlock = ::Action::Registry::parseShortcut (rawStr);
            }
        }
    }
}

} // namespace lua
