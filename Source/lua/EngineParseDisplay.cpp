/**
 * @file EngineParseDisplay.cpp
 * @brief Lua table parse methods for lua::Engine display configuration.
 *
 * Contains: Engine::parseDisplay() and its 6 static helpers:
 * parseDisplayWindow, parseDisplayColours, parseDisplayCursor,
 * parseDisplayFont, parseDisplayTab, parseDisplayMisc.
 *
 * All parsed values are written via model.setValue(app::id::DISPLAY_LUA, ...).
 * Every field is always written — either the parsed Lua value or the default.
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
static void parseDisplayWindow (jam::Model& model, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["window"] };

    if (t.isTable())
    {
        auto titleVal { t["title"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowTitle,
                        titleVal.has_value() ? titleVal.value() : juce::String (ProjectInfo::projectName));

        auto widthVal { t["width"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowWidth,
                        widthVal.has_value() ? static_cast<int> (widthVal.value()) : 640);

        auto heightVal { t["height"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowHeight,
                        heightVal.has_value() ? static_cast<int> (heightVal.value()) : 480);

        auto colourVal { t["colour"].optional<juce::String>() };
        const juce::Colour windowColour { colourVal.has_value() ? Engine::parseColour (colourVal.value()) : juce::Colour (0xff090d12) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::windowColour, static_cast<int> (windowColour.getARGB()));

        auto opacityVal { t["opacity"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowOpacity,
                               opacityVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.0, 1.0, opacityVal.value()))
                                   : 0.75f);

        auto blurRadiusVal { t["blur_radius"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowBlurRadius,
                               blurRadiusVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.0, 100.0, blurRadiusVal.value()))
                                   : 32.0f);

        auto alwaysOnTopVal { t["always_on_top"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowAlwaysOnTop,
                        alwaysOnTopVal.has_value() ? (Map::Bool::getContext()->get (alwaysOnTopVal.value()) == Map::Bool::yes ? 1 : 0) : 0);

        auto buttonsVal { t["buttons"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowButtons,
                        buttonsVal.has_value() ? (Map::Bool::getContext()->get (buttonsVal.value()) == Map::Bool::yes ? 1 : 0) : 0);

        auto forceDwmVal { t["force_dwm"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowForceDwm,
                        forceDwmVal.has_value() ? (Map::Bool::getContext()->get (forceDwmVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        auto saveSizeVal { t["save_size"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowSaveSize,
                        saveSizeVal.has_value() ? (Map::Bool::getContext()->get (saveSizeVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        auto confirmationOnExitVal { t["confirmation_on_exit"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::windowConfirmationOnExit,
                        confirmationOnExitVal.has_value() ? (Map::Bool::getContext()->get (confirmationOnExitVal.value()) == Map::Bool::yes ? 1 : 0) : 1);
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::windowTitle,              juce::String (ProjectInfo::projectName));
        model.setValue (app::id::DISPLAY_LUA, app::id::windowWidth,              640);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowHeight,             480);
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::windowColour,             static_cast<int> (juce::Colour (0xff090d12).getARGB()));
        model.setValue (app::id::DISPLAY_LUA, app::id::windowOpacity,    0.75f);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowBlurRadius, 32.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowAlwaysOnTop,        0);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowButtons,            0);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowForceDwm,           1);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowSaveSize,           1);
        model.setValue (app::id::DISPLAY_LUA, app::id::windowConfirmationOnExit, 1);
    }
}

static void parseDisplayColours (jam::Model& model, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["colours"] };

    auto readColourProperty = [&t, &model] (const char* luaKey,
                                             const juce::Identifier& propId,
                                             juce::Colour defaultColour)
    {
        auto val { t[luaKey].optional<juce::String>() };
        const juce::Colour colour { val.has_value() ? Engine::parseColour (val.value()) : defaultColour };
        model.setValue<int> (app::id::DISPLAY_LUA, propId, static_cast<int> (colour.getARGB()));
    };

    if (t.isTable())
    {
        readColourProperty ("foreground",          app::id::foreground,            juce::Colour (0xffa1d6e5));
        readColourProperty ("background",          app::id::background,            juce::Colour (0x00000000));
        readColourProperty ("cursor",              app::id::cursorColour,          juce::Colour (0xff4e8c93));
        readColourProperty ("selection",           app::id::selectionColour,       juce::Colour (0x2000ddee));
        readColourProperty ("selection_cursor",    app::id::selectionCursorColour, juce::Colour (0xff00ddee));
        readColourProperty ("status_bar",          app::id::statusBarColour,       juce::Colour (0xff090d12));
        readColourProperty ("status_bar_label_bg", app::id::statusBarLabelBg,      juce::Colour (0xff112130));
        readColourProperty ("status_bar_label_fg", app::id::statusBarLabelFg,      juce::Colour (0xff4e8c93));
        readColourProperty ("status_bar_spinner",  app::id::statusBarSpinner,      juce::Colour (0xff00c8d8));
        readColourProperty ("hint_label_bg",       app::id::hintLabelBg,           juce::Colour (0xff00ffff));
        readColourProperty ("hint_label_fg",       app::id::hintLabelFg,           juce::Colour (0xff111111));
        readColourProperty ("editor_background",   app::id::editorBackground,      juce::Colour (0x00000000));
        readColourProperty ("editor_outline",      app::id::editorOutline,         juce::Colour (0x00000000));
        readColourProperty ("scrollbar_thumb",     app::id::scrollbarThumb,        juce::Colour (0x802c4144));
        readColourProperty ("scrollbar_track",     app::id::scrollbarTrack,        juce::Colour (0x00000000));

        static const std::array<const char*, 16> ansiLuaKeys {{
            "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
            "bright_black", "bright_red", "bright_green", "bright_yellow",
            "bright_blue", "bright_magenta", "bright_cyan", "bright_white"
        }};

        static const std::array<juce::uint32, 16> ansiDefaults {{
            0xff090d12, 0xfffc704c, 0xffc5f0e9, 0xfff3f5c5,
            0xff8cc9d9, 0xff519299, 0xff699daa, 0xffdddddd,
            0xff33535b, 0xfffc704c, 0xffbafffd, 0xfffeffd2,
            0xff67dfef, 0xff01c2d2, 0xff00c8d8, 0xffbafffd
        }};

        static const std::array<const juce::Identifier*, 16> ansiIds {{
            &app::id::ansi0,  &app::id::ansi1,  &app::id::ansi2,  &app::id::ansi3,
            &app::id::ansi4,  &app::id::ansi5,  &app::id::ansi6,  &app::id::ansi7,
            &app::id::ansi8,  &app::id::ansi9,  &app::id::ansi10, &app::id::ansi11,
            &app::id::ansi12, &app::id::ansi13, &app::id::ansi14, &app::id::ansi15
        }};

        for (size_t i { 0 }; i < 16; ++i)
        {
            auto val { t[ansiLuaKeys.at (i)].optional<juce::String>() };
            const juce::Colour colour { val.has_value() ? Engine::parseColour (val.value()) : juce::Colour (ansiDefaults.at (i)) };
            model.setValue<int> (app::id::DISPLAY_LUA, *ansiIds.at (i), static_cast<int> (colour.getARGB()));
        }
    }
    else
    {
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::foreground,            static_cast<int> (juce::Colour (0xffa1d6e5).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::background,            static_cast<int> (juce::Colour (0x00000000).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::cursorColour,          static_cast<int> (juce::Colour (0xff4e8c93).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::selectionColour,       static_cast<int> (juce::Colour (0x2000ddee).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::selectionCursorColour, static_cast<int> (juce::Colour (0xff00ddee).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::statusBarColour,       static_cast<int> (juce::Colour (0xff090d12).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::statusBarLabelBg,      static_cast<int> (juce::Colour (0xff112130).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::statusBarLabelFg,      static_cast<int> (juce::Colour (0xff4e8c93).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::statusBarSpinner,      static_cast<int> (juce::Colour (0xff00c8d8).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::hintLabelBg,           static_cast<int> (juce::Colour (0xff00ffff).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::hintLabelFg,           static_cast<int> (juce::Colour (0xff111111).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::editorBackground,      static_cast<int> (juce::Colour (0x00000000).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::editorOutline,         static_cast<int> (juce::Colour (0x00000000).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::scrollbarThumb,        static_cast<int> (juce::Colour (0x802c4144).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::scrollbarTrack,        static_cast<int> (juce::Colour (0x00000000).getARGB()));

        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi0,  static_cast<int> (juce::Colour (0xff090d12).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi1,  static_cast<int> (juce::Colour (0xfffc704c).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi2,  static_cast<int> (juce::Colour (0xffc5f0e9).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi3,  static_cast<int> (juce::Colour (0xfff3f5c5).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi4,  static_cast<int> (juce::Colour (0xff8cc9d9).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi5,  static_cast<int> (juce::Colour (0xff519299).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi6,  static_cast<int> (juce::Colour (0xff699daa).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi7,  static_cast<int> (juce::Colour (0xffdddddd).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi8,  static_cast<int> (juce::Colour (0xff33535b).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi9,  static_cast<int> (juce::Colour (0xfffc704c).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi10, static_cast<int> (juce::Colour (0xffbafffd).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi11, static_cast<int> (juce::Colour (0xfffeffd2).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi12, static_cast<int> (juce::Colour (0xff67dfef).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi13, static_cast<int> (juce::Colour (0xff01c2d2).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi14, static_cast<int> (juce::Colour (0xff00c8d8).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::ansi15, static_cast<int> (juce::Colour (0xffbafffd).getARGB()));
    }
}

static void parseDisplayCursor (jam::Model& model, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["cursor"] };

    if (t.isTable())
    {
        auto charVal { t["char"].optional<juce::String>() };

        if (charVal.has_value() and charVal.value().isNotEmpty())
            model.setValue (app::id::DISPLAY_LUA, app::id::cursorCodepoint, static_cast<int> (charVal.value()[0]));
        else
            model.setValue (app::id::DISPLAY_LUA, app::id::cursorCodepoint, static_cast<int> (0x2588u));

        auto blinkVal { t["blink"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorBlink,
                        blinkVal.has_value() ? (Map::Bool::getContext()->get (blinkVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        auto blinkIntervalVal { t["blink_interval"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorBlinkInterval,
                        blinkIntervalVal.has_value()
                            ? juce::jlimit (100, 5000, static_cast<int> (blinkIntervalVal.value()))
                            : 500);

        auto forceVal { t["force"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorForce,
                        forceVal.has_value() ? (Map::Bool::getContext()->get (forceVal.value()) == Map::Bool::yes ? 1 : 0) : 0);

        auto styleVal { t["style"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorStyle,
                        styleVal.has_value() ? Map::Cursor::getContext()->get (styleVal.value()) : 1);
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorCodepoint,     static_cast<int> (0x2588u));
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorBlink,         1);
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorBlinkInterval, 500);
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorForce,         0);
        model.setValue (app::id::DISPLAY_LUA, app::id::cursorStyle,         1);
    }
}

static void parseDisplayFont (jam::Model& model, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["font"] };

    if (t.isTable())
    {
        auto familyVal { t["family"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::fontFamily,
                        familyVal.has_value() ? familyVal.value() : juce::String ("Display Mono"));

        auto sizeVal { t["size"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::fontSize,
                               sizeVal.has_value()
                                   ? static_cast<float> (juce::jlimit (1.0, 200.0, sizeVal.value()))
                                   : 12.0f);

        auto ligaturesVal { t["ligatures"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::fontLigatures,
                        ligaturesVal.has_value() ? (Map::Bool::getContext()->get (ligaturesVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        auto emboldenVal { t["embolden"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::fontEmbolden,
                        emboldenVal.has_value() ? (Map::Bool::getContext()->get (emboldenVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        auto lineHeightVal { t["line_height"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::lineHeight,
                               lineHeightVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.5, 3.0, lineHeightVal.value()))
                                   : 1.0f);

        auto cellWidthVal { t["cell_width"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::cellWidth,
                               cellWidthVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.5, 3.0, cellWidthVal.value()))
                                   : 1.0f);

        auto desktopScaleVal { t["desktop_scale"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::fontDesktopScale,
                        desktopScaleVal.has_value() ? (Map::Bool::getContext()->get (desktopScaleVal.value()) == Map::Bool::yes ? 1 : 0) : 0);
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::fontFamily,       juce::String ("Display Mono"));
        model.setValue (app::id::DISPLAY_LUA, app::id::fontSize,    12.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::fontLigatures,    1);
        model.setValue (app::id::DISPLAY_LUA, app::id::fontEmbolden,     1);
        model.setValue (app::id::DISPLAY_LUA, app::id::lineHeight, 1.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::cellWidth,  1.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::fontDesktopScale, 0);
    }
}

static void parseDisplayTab (jam::Model& model, jam::lua::Value& displayTable)
{
    jam::lua::Value t { displayTable["tab"] };

    auto readColourProperty = [&t, &model] (const char* luaKey,
                                             const juce::Identifier& propId,
                                             juce::Colour defaultColour)
    {
        auto val { t[luaKey].optional<juce::String>() };
        const juce::Colour colour { val.has_value() ? Engine::parseColour (val.value()) : defaultColour };
        model.setValue<int> (app::id::DISPLAY_LUA, propId, static_cast<int> (colour.getARGB()));
    };

    if (t.isTable())
    {
        auto familyVal { t["family"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::tabFamily,
                        familyVal.has_value() ? familyVal.value() : juce::String ("Display Mono"));

        auto sizeVal { t["size"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::tabSize,
                               sizeVal.has_value()
                                   ? static_cast<float> (juce::jlimit (1.0, 200.0, sizeVal.value()))
                                   : 12.0f);

        readColourProperty ("foreground", app::id::tabForeground, juce::Colour (0xff00c8d8));
        readColourProperty ("inactive",   app::id::tabInactive,   juce::Colour (0xff33535b));
        readColourProperty ("line",       app::id::tabLine,       juce::Colour (0xff2c4144));
        readColourProperty ("active",     app::id::tabActive,     juce::Colour (0xff002b35));
        readColourProperty ("indicator",  app::id::tabIndicator,  juce::Colour (0xff01c2d2));

        auto positionVal { t["position"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::tabPosition,
                        positionVal.has_value() ? positionVal.value() : juce::String ("left"));

        auto buttonSvgVal { t["button_svg"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::tabButtonSvg,
                        buttonSvgVal.has_value() ? buttonSvgVal.value() : juce::String());
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::tabFamily,     juce::String ("Display Mono"));
        model.setValue (app::id::DISPLAY_LUA, app::id::tabSize, 12.0f);
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::tabForeground, static_cast<int> (juce::Colour (0xff00c8d8).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::tabInactive,   static_cast<int> (juce::Colour (0xff33535b).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::tabLine,       static_cast<int> (juce::Colour (0xff2c4144).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::tabActive,     static_cast<int> (juce::Colour (0xff002b35).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::tabIndicator,  static_cast<int> (juce::Colour (0xff01c2d2).getARGB()));
        model.setValue (app::id::DISPLAY_LUA, app::id::tabPosition,   juce::String ("left"));
        model.setValue (app::id::DISPLAY_LUA, app::id::tabButtonSvg,  juce::String());
    }
}

static void parseDisplayMisc (jam::Model& model, jam::lua::Value& displayTable)
{
    // Pane sub-table
    jam::lua::Value paneTable { displayTable["pane"] };

    if (paneTable.isTable())
    {
        auto barColourVal { paneTable["bar_colour"].optional<juce::String>() };
        const juce::Colour barColour { barColourVal.has_value() ? Engine::parseColour (barColourVal.value()) : juce::Colour (0xff33535b) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::paneBarColour, static_cast<int> (barColour.getARGB()));

        auto barHighlightVal { paneTable["bar_highlight"].optional<juce::String>() };
        const juce::Colour barHighlight { barHighlightVal.has_value() ? Engine::parseColour (barHighlightVal.value()) : juce::Colour (0xff4e8c93) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::paneBarHighlight, static_cast<int> (barHighlight.getARGB()));
    }
    else
    {
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::paneBarColour,    static_cast<int> (juce::Colour (0xff33535b).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::paneBarHighlight, static_cast<int> (juce::Colour (0xff4e8c93).getARGB()));
    }

    // Overlay sub-table
    jam::lua::Value overlayTable { displayTable["overlay"] };

    if (overlayTable.isTable())
    {
        auto familyVal { overlayTable["family"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::overlayFamily,
                        familyVal.has_value() ? familyVal.value() : juce::String ("Display Mono"));

        auto sizeVal { overlayTable["size"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::overlaySize,
                               sizeVal.has_value()
                                   ? static_cast<float> (juce::jlimit (1.0, 200.0, sizeVal.value()))
                                   : 14.0f);

        auto colourVal { overlayTable["colour"].optional<juce::String>() };
        const juce::Colour overlayColour { colourVal.has_value() ? Engine::parseColour (colourVal.value()) : juce::Colour (0xff4e8c93) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::overlayColour, static_cast<int> (overlayColour.getARGB()));
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::overlayFamily, juce::String ("Display Mono"));
        model.setValue (app::id::DISPLAY_LUA, app::id::overlaySize, 14.0f);
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::overlayColour, static_cast<int> (juce::Colour (0xff4e8c93).getARGB()));
    }

    // Menu sub-table
    jam::lua::Value menuTable { displayTable["menu"] };

    if (menuTable.isTable())
    {
        auto opacityVal { menuTable["opacity"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::menuOpacity,
                               opacityVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.0, 1.0, opacityVal.value()))
                                   : 0.65f);
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::menuOpacity, 0.65f);
    }

    // Action list sub-table
    jam::lua::Value t { displayTable["action_list"] };

    if (t.isTable())
    {
        auto closeOnRunVal { t["close_on_run"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListCloseOnRun,
                        closeOnRunVal.has_value() ? (Map::Bool::getContext()->get (closeOnRunVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        auto positionVal { t["position"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListPosition,
                        positionVal.has_value() ? positionVal.value() : juce::String ("top"));

        auto nameFamilyVal { t["name_font_family"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListNameFamily,
                        nameFamilyVal.has_value() ? nameFamilyVal.value() : juce::String ("Display"));

        auto nameStyleVal { t["name_font_style"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListNameStyle,
                        nameStyleVal.has_value() ? nameStyleVal.value() : juce::String ("Bold"));

        auto nameSizeVal { t["name_font_size"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListNameSize,
                               nameSizeVal.has_value()
                                   ? static_cast<float> (juce::jlimit (6.0, 72.0, nameSizeVal.value()))
                                   : 13.0f);

        auto shortcutFamilyVal { t["shortcut_font_family"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListShortcutFamily,
                        shortcutFamilyVal.has_value() ? shortcutFamilyVal.value() : juce::String ("Display Mono"));

        auto shortcutStyleVal { t["shortcut_font_style"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListShortcutStyle,
                        shortcutStyleVal.has_value() ? shortcutStyleVal.value() : juce::String ("Bold"));

        auto shortcutSizeVal { t["shortcut_font_size"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListShortcutSize,
                               shortcutSizeVal.has_value()
                                   ? static_cast<float> (juce::jlimit (6.0, 72.0, shortcutSizeVal.value()))
                                   : 12.0f);

        jam::lua::Value p { t["padding"] };

        if (p.isTable())
        {
            auto paddingTopVal { p[1].optional<double>() };
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingTop,
                            paddingTopVal.has_value()
                                ? juce::jlimit (0, 50, static_cast<int> (paddingTopVal.value()))
                                : 10);

            auto paddingRightVal { p[2].optional<double>() };
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingRight,
                            paddingRightVal.has_value()
                                ? juce::jlimit (0, 50, static_cast<int> (paddingRightVal.value()))
                                : 10);

            auto paddingBottomVal { p[3].optional<double>() };
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingBottom,
                            paddingBottomVal.has_value()
                                ? juce::jlimit (0, 50, static_cast<int> (paddingBottomVal.value()))
                                : 10);

            auto paddingLeftVal { p[4].optional<double>() };
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingLeft,
                            paddingLeftVal.has_value()
                                ? juce::jlimit (0, 50, static_cast<int> (paddingLeftVal.value()))
                                : 10);
        }
        else
        {
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingTop,    10);
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingRight,  10);
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingBottom, 10);
            model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingLeft,   10);
        }

        auto nameColourVal { t["name_colour"].optional<juce::String>() };
        const juce::Colour nameColour { nameColourVal.has_value() ? Engine::parseColour (nameColourVal.value()) : juce::Colour (0xffa1d6e5) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::actionListNameColour, static_cast<int> (nameColour.getARGB()));

        auto shortcutColourVal { t["shortcut_colour"].optional<juce::String>() };
        const juce::Colour shortcutColour { shortcutColourVal.has_value() ? Engine::parseColour (shortcutColourVal.value()) : juce::Colour (0xff00c8d8) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::actionListShortcutColour, static_cast<int> (shortcutColour.getARGB()));

        auto widthVal { t["width"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListWidth,
                               widthVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.1, 1.0, widthVal.value()))
                                   : 0.3f);

        auto heightVal { t["height"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListHeight,
                               heightVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.1, 1.0, heightVal.value()))
                                   : 0.3f);

        auto highlightColourVal { t["highlight_colour"].optional<juce::String>() };
        const juce::Colour highlightColour { highlightColourVal.has_value() ? Engine::parseColour (highlightColourVal.value()) : juce::Colour (0x2000ddee) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::actionListHighlightColour, static_cast<int> (highlightColour.getARGB()));
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListCloseOnRun,     1);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListPosition,       juce::String ("top"));
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListNameFamily,     juce::String ("Display"));
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListNameStyle,      juce::String ("Bold"));
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListNameSize,  13.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListShortcutFamily, juce::String ("Display Mono"));
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListShortcutStyle,  juce::String ("Bold"));
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListShortcutSize, 12.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingTop,     10);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingRight,   10);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingBottom,  10);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListPaddingLeft,    10);
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::actionListNameColour,     static_cast<int> (juce::Colour (0xffa1d6e5).getARGB()));
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::actionListShortcutColour, static_cast<int> (juce::Colour (0xff00c8d8).getARGB()));
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListWidth,  0.3f);
        model.setValue (app::id::DISPLAY_LUA, app::id::actionListHeight, 0.3f);
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::actionListHighlightColour, static_cast<int> (juce::Colour (0x2000ddee).getARGB()));
    }

    // Status bar sub-table
    jam::lua::Value statusBarTable { displayTable["status_bar"] };

    if (statusBarTable.isTable())
    {
        auto positionVal { statusBarTable["position"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarPosition,
                        positionVal.has_value() ? positionVal.value() : juce::String ("bottom"));

        auto fontFamilyVal { statusBarTable["font_family"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarFontFamily,
                        fontFamilyVal.has_value() ? fontFamilyVal.value() : juce::String ("Display Mono"));

        auto fontSizeVal { statusBarTable["font_size"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarFontSize,
                               fontSizeVal.has_value()
                                   ? static_cast<float> (juce::jlimit (6.0, 48.0, fontSizeVal.value()))
                                   : 12.0f);

        auto fontStyleVal { statusBarTable["font_style"].optional<juce::String>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarFontStyle,
                        fontStyleVal.has_value() ? fontStyleVal.value() : juce::String ("Bold"));
    }
    else
    {
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarPosition,   juce::String ("bottom"));
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarFontFamily, juce::String ("Display Mono"));
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarFontSize, 12.0f);
        model.setValue (app::id::DISPLAY_LUA, app::id::statusBarFontStyle,  juce::String ("Bold"));
    }

    // Popup border sub-table
    jam::lua::Value popupBorderTable { displayTable["popup"] };

    if (popupBorderTable.isTable())
    {
        auto borderColourVal { popupBorderTable["border_colour"].optional<juce::String>() };
        const juce::Colour borderColour { borderColourVal.has_value() ? Engine::parseColour (borderColourVal.value()) : juce::Colour (0xff4e8c93) };
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::popupBorderColour, static_cast<int> (borderColour.getARGB()));

        auto borderWidthVal { popupBorderTable["border_width"].optional<double>() };
        model.setValue (app::id::DISPLAY_LUA, app::id::popupBorderWidth,
                               borderWidthVal.has_value()
                                   ? static_cast<float> (juce::jlimit (0.0, 10.0, borderWidthVal.value()))
                                   : 1.0f);
    }
    else
    {
        model.setValue<int> (app::id::DISPLAY_LUA, app::id::popupBorderColour, static_cast<int> (juce::Colour (0xff4e8c93).getARGB()));
        model.setValue (app::id::DISPLAY_LUA, app::id::popupBorderWidth, 1.0f);
    }

    // Scrollbar width
    auto scrollbarWidthVal { displayTable["scrollbar_width"].optional<double>() };
    model.setValue (app::id::DISPLAY_LUA, app::id::scrollbarWidth,
                    scrollbarWidthVal.has_value()
                        ? juce::jlimit (0, 64, static_cast<int> (scrollbarWidthVal.value()))
                        : 8);
}

//==============================================================================
void Engine::parseDisplay()
{
    jassert (model != nullptr);

    jam::lua::Value displayTable { lua["END"]["display"] };

    // Each helper handles the absent-table case by writing defaults, so always call all.
    parseDisplayWindow  (*model, displayTable);
    parseDisplayColours (*model, displayTable);
    parseDisplayCursor  (*model, displayTable);
    parseDisplayFont    (*model, displayTable);
    parseDisplayTab     (*model, displayTable);
    parseDisplayMisc    (*model, displayTable);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
