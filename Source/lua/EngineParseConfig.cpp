/**
 * @file EngineParseConfig.cpp
 * @brief Lua table parse methods for lua::Engine nexus and whelmed configuration.
 *
 * Contains: Engine::parseNexus(), Engine::parseWhelmed().
 *
 * All parsed scalar values are written into the CONFIG subtree via setProperty.
 * Every field is always written — either the parsed Lua value or the default.
 * This ensures every CONFIG property has a value after load().
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
void Engine::parseNexus()
{
    jassert (model != nullptr);

    jam::lua::Value nexusTable { lua["END"]["nexus"] };

    if (nexusTable.isTable())
    {
        // gpu
        auto gpuVal { nexusTable["gpu"].optional<juce::String>() };
        model->setValue (app::id::NEXUS_LUA, app::id::gpu,
                         gpuVal.has_value() ? gpuVal.value() : juce::String ("auto"));

        // daemon
        auto daemonVal { nexusTable["daemon"].optional<juce::String>() };
        model->setValue (app::id::NEXUS_LUA, app::id::daemon,
                         daemonVal.has_value() ? (Map::Bool::getContext()->get (daemonVal.value()) == Map::Bool::yes ? 1 : 0) : 0);

        // auto_reload
        auto autoReloadVal { nexusTable["auto_reload"].optional<juce::String>() };
        model->setValue (app::id::NEXUS_LUA, app::id::autoReload,
                         autoReloadVal.has_value() ? (Map::Bool::getContext()->get (autoReloadVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

        // Shell sub-table
        jam::lua::Value shellTable { nexusTable["shell"] };

        if (shellTable.isTable())
        {
            auto programVal { shellTable["program"].optional<juce::String>() };

#if JUCE_WINDOWS
            const juce::String defaultProgram { "powershell.exe" };
            const juce::String defaultArgs    { "" };
#elif JUCE_MAC
            const juce::String defaultProgram { "zsh" };
            const juce::String defaultArgs    { "" };
#else
            const juce::String defaultProgram { "bash" };
            const juce::String defaultArgs    { "-l" };
#endif

            model->setValue (app::id::NEXUS_LUA, app::id::shellProgram,
                             programVal.has_value() ? programVal.value() : defaultProgram);

            auto argsVal { shellTable["args"].optional<juce::String>() };
            model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,
                             argsVal.has_value() ? argsVal.value() : defaultArgs);

            auto integrationVal { shellTable["integration"].optional<juce::String>() };
            model->setValue (app::id::NEXUS_LUA, app::id::shellIntegration,
                             integrationVal.has_value() ? (Map::Bool::getContext()->get (integrationVal.value()) == Map::Bool::yes ? 1 : 0) : 1);
        }
        else
        {
#if JUCE_WINDOWS
            model->setValue (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("powershell.exe"));
            model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,    juce::String (""));
#elif JUCE_MAC
            model->setValue (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("zsh"));
            model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,    juce::String (""));
#else
            model->setValue (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("bash"));
            model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,    juce::String ("-l"));
#endif
            model->setValue (app::id::NEXUS_LUA, app::id::shellIntegration, 1);
        }

        // Terminal sub-table
        jam::lua::Value terminalTable { nexusTable["terminal"] };

        if (terminalTable.isTable())
        {
            auto scrollbackLinesVal { terminalTable["scrollback_lines"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::scrollbackLines,
                             scrollbackLinesVal.has_value()
                                 ? juce::jlimit (100, 1000000, static_cast<int> (scrollbackLinesVal.value()))
                                 : 10000);

            auto scrollStepVal { terminalTable["scroll_step"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::scrollStep,
                             scrollStepVal.has_value()
                                 ? juce::jlimit (1, 100, static_cast<int> (scrollStepVal.value()))
                                 : 5);

            auto dropMultifilesVal { terminalTable["drop_multifiles"].optional<juce::String>() };
            model->setValue (app::id::NEXUS_LUA, app::id::dropMultifiles,
                             dropMultifilesVal.has_value() ? dropMultifilesVal.value() : juce::String ("space"));

            auto dropQuotedVal { terminalTable["drop_quoted"].optional<juce::String>() };
            model->setValue (app::id::NEXUS_LUA, app::id::dropQuoted,
                             dropQuotedVal.has_value() ? (Map::Bool::getContext()->get (dropQuotedVal.value()) == Map::Bool::yes ? 1 : 0) : 1);

            // Padding array: { top, right, bottom, left }
            jam::lua::Value p { terminalTable["padding"] };

            if (p.isTable())
            {
                auto paddingTopVal { p[1].optional<double>() };
                model->setValue (app::id::NEXUS_LUA, app::id::paddingTop,
                                 paddingTopVal.has_value()
                                     ? juce::jlimit (0, 50, static_cast<int> (paddingTopVal.value()))
                                     : 10);

                auto paddingRightVal { p[2].optional<double>() };
                model->setValue (app::id::NEXUS_LUA, app::id::paddingRight,
                                 paddingRightVal.has_value()
                                     ? juce::jlimit (0, 50, static_cast<int> (paddingRightVal.value()))
                                     : 10);

                auto paddingBottomVal { p[3].optional<double>() };
                model->setValue (app::id::NEXUS_LUA, app::id::paddingBottom,
                                 paddingBottomVal.has_value()
                                     ? juce::jlimit (0, 50, static_cast<int> (paddingBottomVal.value()))
                                     : 10);

                auto paddingLeftVal { p[4].optional<double>() };
                model->setValue (app::id::NEXUS_LUA, app::id::paddingLeft,
                                 paddingLeftVal.has_value()
                                     ? juce::jlimit (0, 50, static_cast<int> (paddingLeftVal.value()))
                                     : 10);
            }
            else
            {
                model->setValue (app::id::NEXUS_LUA, app::id::paddingTop,    10);
                model->setValue (app::id::NEXUS_LUA, app::id::paddingRight,  10);
                model->setValue (app::id::NEXUS_LUA, app::id::paddingBottom, 10);
                model->setValue (app::id::NEXUS_LUA, app::id::paddingLeft,   10);
            }
        }
        else
        {
            model->setValue (app::id::NEXUS_LUA, app::id::scrollbackLines, 10000);
            model->setValue (app::id::NEXUS_LUA, app::id::scrollStep,      5);
            model->setValue (app::id::NEXUS_LUA, app::id::dropMultifiles,  juce::String ("space"));
            model->setValue (app::id::NEXUS_LUA, app::id::dropQuoted,      1);
            model->setValue (app::id::NEXUS_LUA, app::id::paddingTop,      10);
            model->setValue (app::id::NEXUS_LUA, app::id::paddingRight,    10);
            model->setValue (app::id::NEXUS_LUA, app::id::paddingBottom,   10);
            model->setValue (app::id::NEXUS_LUA, app::id::paddingLeft,     10);
        }

        // Hyperlinks sub-table
        jam::lua::Value hyperlinksTable { nexusTable["hyperlinks"] };

        if (hyperlinksTable.isTable())
        {
            auto editorVal { hyperlinksTable["editor"].optional<juce::String>() };
            model->setValue (app::id::NEXUS_LUA, app::id::hyperlinkEditor,
                             editorVal.has_value() ? editorVal.value() : juce::String ("nvim"));

            // Built-in handlers — always registered, user config can override.
            handlers[".md"]   = "whelmed";
            handlers[".png"]  = "image";
            handlers[".jpg"]  = "image";
            handlers[".jpeg"] = "image";
            handlers[".gif"]  = "image";

            jam::lua::Value handlersTable { hyperlinksTable["handlers"] };

            if (handlersTable.isTable())
            {
                handlersTable.forEach ([this] (const jam::lua::Value& k, const jam::lua::Value& v)
                {
                    if (k.getType() == jam::lua::Type::string and v.getType() == jam::lua::Type::string)
                        handlers[k.to<juce::String>()] = v.to<juce::String>();
                });
            }

            jam::lua::Value extensionsTable { hyperlinksTable["extensions"] };

            if (extensionsTable.isTable())
            {
                extensionsTable.forEach ([this] (const jam::lua::Value& /*k*/, const jam::lua::Value& v)
                {
                    if (v.getType() == jam::lua::Type::string)
                        extensions.insert (v.to<juce::String>());
                });
            }
        }
        else
        {
            model->setValue (app::id::NEXUS_LUA, app::id::hyperlinkEditor, juce::String ("nvim"));

            handlers[".md"]   = "whelmed";
            handlers[".png"]  = "image";
            handlers[".jpg"]  = "image";
            handlers[".jpeg"] = "image";
            handlers[".gif"]  = "image";
        }

        // Image sub-table
        jam::lua::Value imageTable { nexusTable["image"] };

        if (imageTable.isTable())
        {
            auto atlasBudgetVal { imageTable["atlas_budget"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::imageAtlasBudgetBytes,
                             atlasBudgetVal.has_value()
                                 ? juce::jlimit (1 * 1024 * 1024, 256 * 1024 * 1024, static_cast<int> (atlasBudgetVal.value()))
                                 : 32 * 1024 * 1024);

            auto atlasDimVal { imageTable["atlas_dimension"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::imageAtlasDimension,
                             atlasDimVal.has_value()
                                 ? juce::jlimit (1024, 8192, static_cast<int> (atlasDimVal.value()))
                                 : 4096);

            auto colsVal { imageTable["cols"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::imageCols,
                             colsVal.has_value()
                                 ? juce::jlimit (10, 200, static_cast<int> (colsVal.value()))
                                 : 40);

            auto rowsVal { imageTable["rows"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::imageRows,
                             rowsVal.has_value()
                                 ? juce::jlimit (5, 100, static_cast<int> (rowsVal.value()))
                                 : 20);

            auto padVal { imageTable["padding"].optional<double>() };
            model->setValue (app::id::NEXUS_LUA, app::id::imagePadding,
                             padVal.has_value()
                                 ? juce::jlimit (0, 64, static_cast<int> (padVal.value()))
                                 : 10);

            auto brdVal { imageTable["border"].optional<bool>() };
            model->setValue (app::id::NEXUS_LUA, app::id::imageBorder,
                             brdVal.has_value() ? (brdVal.value() ? 1 : 0) : 1);
        }
        else
        {
            model->setValue (app::id::NEXUS_LUA, app::id::imageAtlasBudgetBytes, 32 * 1024 * 1024);
            model->setValue (app::id::NEXUS_LUA, app::id::imageAtlasDimension,   4096);
            model->setValue (app::id::NEXUS_LUA, app::id::imageCols,             40);
            model->setValue (app::id::NEXUS_LUA, app::id::imageRows,             20);
            model->setValue (app::id::NEXUS_LUA, app::id::imagePadding,          10);
            model->setValue (app::id::NEXUS_LUA, app::id::imageBorder,           1);
        }
    }
    else
    {
        // nexus table absent — write all defaults.
        model->setValue (app::id::NEXUS_LUA, app::id::gpu,         juce::String ("auto"));
        model->setValue (app::id::NEXUS_LUA, app::id::daemon,       0);
        model->setValue (app::id::NEXUS_LUA, app::id::autoReload,   1);

#if JUCE_WINDOWS
        model->setValue (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("powershell.exe"));
        model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,    juce::String (""));
#elif JUCE_MAC
        model->setValue (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("zsh"));
        model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,    juce::String (""));
#else
        model->setValue (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("bash"));
        model->setValue (app::id::NEXUS_LUA, app::id::shellArgs,    juce::String ("-l"));
#endif
        model->setValue (app::id::NEXUS_LUA, app::id::shellIntegration,    1);
        model->setValue (app::id::NEXUS_LUA, app::id::scrollbackLines,     10000);
        model->setValue (app::id::NEXUS_LUA, app::id::scrollStep,          5);
        model->setValue (app::id::NEXUS_LUA, app::id::dropMultifiles,      juce::String ("space"));
        model->setValue (app::id::NEXUS_LUA, app::id::dropQuoted,          1);
        model->setValue (app::id::NEXUS_LUA, app::id::paddingTop,          10);
        model->setValue (app::id::NEXUS_LUA, app::id::paddingRight,        10);
        model->setValue (app::id::NEXUS_LUA, app::id::paddingBottom,       10);
        model->setValue (app::id::NEXUS_LUA, app::id::paddingLeft,         10);
        model->setValue (app::id::NEXUS_LUA, app::id::hyperlinkEditor,     juce::String ("nvim"));
        model->setValue (app::id::NEXUS_LUA, app::id::imageAtlasBudgetBytes, 32 * 1024 * 1024);
        model->setValue (app::id::NEXUS_LUA, app::id::imageAtlasDimension,   4096);
        model->setValue (app::id::NEXUS_LUA, app::id::imageCols,             40);
        model->setValue (app::id::NEXUS_LUA, app::id::imageRows,             20);
        model->setValue (app::id::NEXUS_LUA, app::id::imagePadding,          10);
        model->setValue (app::id::NEXUS_LUA, app::id::imageBorder,           1);

        handlers[".md"]   = "whelmed";
        handlers[".png"]  = "image";
        handlers[".jpg"]  = "image";
        handlers[".jpeg"] = "image";
        handlers[".gif"]  = "image";
    }
}

//==============================================================================
void Engine::parseWhelmed()
{
    jassert (model != nullptr);

    jam::lua::Value whelmedTable { lua["END"]["whelmed"] };

    auto readColourProperty = [this, &whelmedTable] (const char* luaKey,
                                                      const juce::Identifier& propId,
                                                      juce::Colour defaultColour)
    {
        auto val { whelmedTable[luaKey].optional<juce::String>() };
        const juce::Colour colour { val.has_value() ? parseColour (val.value()) : defaultColour };
        model->setValue<int> (app::id::WHELMED_LUA, propId, static_cast<int> (colour.getARGB()));
    };

    auto readFloatProperty = [this, &whelmedTable] (const char* luaKey,
                                                     const juce::Identifier& propId,
                                                     float defaultVal)
    {
        auto val { whelmedTable[luaKey].optional<double>() };
        model->setValue (app::id::WHELMED_LUA, propId,
                                val.has_value() ? static_cast<float> (val.value()) : defaultVal);
    };

    auto readStrProperty = [this, &whelmedTable] (const char* luaKey,
                                                   const juce::Identifier& propId,
                                                   const juce::String& defaultVal)
    {
        auto val { whelmedTable[luaKey].optional<juce::String>() };
        model->setValue (app::id::WHELMED_LUA, propId, val.has_value() ? val.value() : defaultVal);
    };

    auto readIntProperty = [this, &whelmedTable] (const char* luaKey,
                                                   const juce::Identifier& propId,
                                                   int defaultVal)
    {
        auto val { whelmedTable[luaKey].optional<double>() };
        model->setValue (app::id::WHELMED_LUA, propId,
                         val.has_value() ? static_cast<int> (val.value()) : defaultVal);
    };

    if (whelmedTable.isTable())
    {
        // Typography
        readStrProperty   ("font_family",  app::id::fontFamily,  "Display");
        readStrProperty   ("font_style",   app::id::fontStyle,   "Medium");
        readFloatProperty ("font_size",    app::id::fontSize,    16.0f);
        readStrProperty   ("code_family",  app::id::codeFamily,  "Display Mono");
        readStrProperty   ("code_style",   app::id::codeStyle,   "Medium");
        readFloatProperty ("code_size",    app::id::codeSize,    12.0f);
        readFloatProperty ("line_height",  app::id::lineHeight,  1.5f);

        // Heading sizes
        readFloatProperty ("h1_size", app::id::h1Size, 28.0f);
        readFloatProperty ("h2_size", app::id::h2Size, 28.0f);
        readFloatProperty ("h3_size", app::id::h3Size, 24.0f);
        readFloatProperty ("h4_size", app::id::h4Size, 20.0f);
        readFloatProperty ("h5_size", app::id::h5Size, 18.0f);
        readFloatProperty ("h6_size", app::id::h6Size, 16.0f);

        // Colours
        readColourProperty ("background",  app::id::background,  juce::Colour (0xff0d141c));
        readColourProperty ("body_colour", app::id::bodyColour,  juce::Colour (0xffb3f9f5));
        readColourProperty ("code_colour", app::id::codeColour,  juce::Colour (0xff00d0ff));
        readColourProperty ("link_colour", app::id::linkColour,  juce::Colour (0xff01c2d2));
        readColourProperty ("h1_colour",   app::id::h1Colour,    juce::Colour (0xffd4c8a0));
        readColourProperty ("h2_colour",   app::id::h2Colour,    juce::Colour (0xffd4c8a0));
        readColourProperty ("h3_colour",   app::id::h3Colour,    juce::Colour (0xffd4c8a0));
        readColourProperty ("h4_colour",   app::id::h4Colour,    juce::Colour (0xffd4c8a0));
        readColourProperty ("h5_colour",   app::id::h5Colour,    juce::Colour (0xffd4c8a0));
        readColourProperty ("h6_colour",   app::id::h6Colour,    juce::Colour (0xffd4c8a0));

        readColourProperty ("code_fence_background",   app::id::codeFenceBackground,   juce::Colour (0xff090d12));
        readColourProperty ("progress_background",     app::id::progressBackground,    juce::Colour (0xff1a1a1a));
        readColourProperty ("progress_foreground",     app::id::progressForeground,    juce::Colour (0xff4488cc));
        readColourProperty ("progress_text_colour",    app::id::progressTextColour,    juce::Colour (0xffcccccc));
        readColourProperty ("progress_spinner_colour", app::id::progressSpinnerColour, juce::Colour (0xff4488cc));

        // Syntax tokens
        readColourProperty ("token_error",        app::id::tokenError,        juce::Colour (0xfff74a4a));
        readColourProperty ("token_comment",      app::id::tokenComment,      juce::Colour (0xff6080c0));
        readColourProperty ("token_keyword",      app::id::tokenKeyword,      juce::Colour (0xff1919ff));
        readColourProperty ("token_operator",     app::id::tokenOperator,     juce::Colour (0xffb0b0b0));
        readColourProperty ("token_identifier",   app::id::tokenIdentifier,   juce::Colour (0xff00c6ff));
        readColourProperty ("token_integer",      app::id::tokenInteger,      juce::Colour (0xff00ff00));
        readColourProperty ("token_float",        app::id::tokenFloat,        juce::Colour (0xff00ff00));
        readColourProperty ("token_string",       app::id::tokenString,       juce::Colour (0xffffc0c0));
        readColourProperty ("token_bracket",      app::id::tokenBracket,      juce::Colour (0xff80ffff));
        readColourProperty ("token_punctuation",  app::id::tokenPunctuation,  juce::Colour (0xffff9080));
        readColourProperty ("token_preprocessor", app::id::tokenPreprocessor, juce::Colour (0xff9aff00));

        // Table colours
        readColourProperty ("table_background",        app::id::tableBackground,       juce::Colour (0xff090d12));
        readColourProperty ("table_header_background", app::id::tableHeaderBackground, juce::Colour (0xff112130));
        readColourProperty ("table_row_alt",           app::id::tableRowAlt,           juce::Colour (0xff0d141c));
        readColourProperty ("table_border_colour",     app::id::tableBorderColour,     juce::Colour (0xff2c4144));
        readColourProperty ("table_header_text",       app::id::tableHeaderText,       juce::Colour (0xffbafffd));
        readColourProperty ("table_cell_text",         app::id::tableCellText,         juce::Colour (0xffb3f9f5));

        // Scrollbar
        readColourProperty ("scrollbar_thumb",      app::id::scrollbarThumb,      juce::Colour (0xff2c4144));
        readColourProperty ("scrollbar_track",      app::id::scrollbarTrack,      juce::Colour (0xff0d141c));
        readColourProperty ("scrollbar_background", app::id::scrollbarBackground, juce::Colour (0xff0d141c));
        readColourProperty ("selection_colour",     app::id::selectionColour,     juce::Colour (0x8000c8d8));

        // Navigation
        readStrProperty ("scroll_down",   app::id::scrollDown,   "j");
        readStrProperty ("scroll_up",     app::id::scrollUp,     "k");
        readStrProperty ("scroll_top",    app::id::scrollTop,    "gg");
        readStrProperty ("scroll_bottom", app::id::scrollBottom, "G");
        readIntProperty ("scroll_step",   app::id::scrollStep,   50);

        // Padding array: { top, right, bottom, left }
        jam::lua::Value p { whelmedTable["padding"] };

        if (p.isTable())
        {
            auto paddingTopVal { p[1].optional<double>() };
            model->setValue (app::id::WHELMED_LUA, app::id::paddingTop,
                             paddingTopVal.has_value()
                                 ? juce::jlimit (0, 50, static_cast<int> (paddingTopVal.value()))
                                 : 10);

            auto paddingRightVal { p[2].optional<double>() };
            model->setValue (app::id::WHELMED_LUA, app::id::paddingRight,
                             paddingRightVal.has_value()
                                 ? juce::jlimit (0, 50, static_cast<int> (paddingRightVal.value()))
                                 : 10);

            auto paddingBottomVal { p[3].optional<double>() };
            model->setValue (app::id::WHELMED_LUA, app::id::paddingBottom,
                             paddingBottomVal.has_value()
                                 ? juce::jlimit (0, 50, static_cast<int> (paddingBottomVal.value()))
                                 : 10);

            auto paddingLeftVal { p[4].optional<double>() };
            model->setValue (app::id::WHELMED_LUA, app::id::paddingLeft,
                             paddingLeftVal.has_value()
                                 ? juce::jlimit (0, 50, static_cast<int> (paddingLeftVal.value()))
                                 : 10);
        }
        else
        {
            model->setValue (app::id::WHELMED_LUA, app::id::paddingTop,    10);
            model->setValue (app::id::WHELMED_LUA, app::id::paddingRight,  10);
            model->setValue (app::id::WHELMED_LUA, app::id::paddingBottom, 10);
            model->setValue (app::id::WHELMED_LUA, app::id::paddingLeft,   10);
        }
    }
    else
    {
        // whelmed table absent — write all defaults.
        model->setValue (app::id::WHELMED_LUA, app::id::fontFamily,  juce::String ("Display"));
        model->setValue (app::id::WHELMED_LUA, app::id::fontStyle,   juce::String ("Medium"));
        model->setValue (app::id::WHELMED_LUA, app::id::fontSize,   16.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::codeFamily,  juce::String ("Display Mono"));
        model->setValue (app::id::WHELMED_LUA, app::id::codeStyle,   juce::String ("Medium"));
        model->setValue (app::id::WHELMED_LUA, app::id::codeSize,   12.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::lineHeight, 1.5f);
        model->setValue (app::id::WHELMED_LUA, app::id::h1Size,     28.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::h2Size,     28.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::h3Size,     24.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::h4Size,     20.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::h5Size,     18.0f);
        model->setValue (app::id::WHELMED_LUA, app::id::h6Size,     16.0f);

        model->setValue<int> (app::id::WHELMED_LUA, app::id::background,  static_cast<int> (juce::Colour (0xff0d141c).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::bodyColour,  static_cast<int> (juce::Colour (0xffb3f9f5).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::codeColour,  static_cast<int> (juce::Colour (0xff00d0ff).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::linkColour,  static_cast<int> (juce::Colour (0xff01c2d2).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::h1Colour,    static_cast<int> (juce::Colour (0xffd4c8a0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::h2Colour,    static_cast<int> (juce::Colour (0xffd4c8a0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::h3Colour,    static_cast<int> (juce::Colour (0xffd4c8a0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::h4Colour,    static_cast<int> (juce::Colour (0xffd4c8a0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::h5Colour,    static_cast<int> (juce::Colour (0xffd4c8a0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::h6Colour,    static_cast<int> (juce::Colour (0xffd4c8a0).getARGB()));

        model->setValue<int> (app::id::WHELMED_LUA, app::id::codeFenceBackground,   static_cast<int> (juce::Colour (0xff090d12).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::progressBackground,    static_cast<int> (juce::Colour (0xff1a1a1a).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::progressForeground,    static_cast<int> (juce::Colour (0xff4488cc).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::progressTextColour,    static_cast<int> (juce::Colour (0xffcccccc).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::progressSpinnerColour, static_cast<int> (juce::Colour (0xff4488cc).getARGB()));

        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenError,        static_cast<int> (juce::Colour (0xfff74a4a).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenComment,      static_cast<int> (juce::Colour (0xff6080c0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenKeyword,      static_cast<int> (juce::Colour (0xff1919ff).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenOperator,     static_cast<int> (juce::Colour (0xffb0b0b0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenIdentifier,   static_cast<int> (juce::Colour (0xff00c6ff).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenInteger,      static_cast<int> (juce::Colour (0xff00ff00).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenFloat,        static_cast<int> (juce::Colour (0xff00ff00).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenString,       static_cast<int> (juce::Colour (0xffffc0c0).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenBracket,      static_cast<int> (juce::Colour (0xff80ffff).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenPunctuation,  static_cast<int> (juce::Colour (0xffff9080).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tokenPreprocessor, static_cast<int> (juce::Colour (0xff9aff00).getARGB()));

        model->setValue<int> (app::id::WHELMED_LUA, app::id::tableBackground,       static_cast<int> (juce::Colour (0xff090d12).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tableHeaderBackground, static_cast<int> (juce::Colour (0xff112130).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tableRowAlt,           static_cast<int> (juce::Colour (0xff0d141c).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tableBorderColour,     static_cast<int> (juce::Colour (0xff2c4144).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tableHeaderText,       static_cast<int> (juce::Colour (0xffbafffd).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::tableCellText,         static_cast<int> (juce::Colour (0xffb3f9f5).getARGB()));

        model->setValue<int> (app::id::WHELMED_LUA, app::id::scrollbarThumb,      static_cast<int> (juce::Colour (0xff2c4144).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::scrollbarTrack,      static_cast<int> (juce::Colour (0xff0d141c).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::scrollbarBackground, static_cast<int> (juce::Colour (0xff0d141c).getARGB()));
        model->setValue<int> (app::id::WHELMED_LUA, app::id::selectionColour,     static_cast<int> (juce::Colour (0x8000c8d8).getARGB()));

        model->setValue (app::id::WHELMED_LUA, app::id::scrollDown,   juce::String ("j"));
        model->setValue (app::id::WHELMED_LUA, app::id::scrollUp,     juce::String ("k"));
        model->setValue (app::id::WHELMED_LUA, app::id::scrollTop,    juce::String ("gg"));
        model->setValue (app::id::WHELMED_LUA, app::id::scrollBottom, juce::String ("G"));
        model->setValue (app::id::WHELMED_LUA, app::id::scrollStep,   50);

        model->setValue (app::id::WHELMED_LUA, app::id::paddingTop,    10);
        model->setValue (app::id::WHELMED_LUA, app::id::paddingRight,  10);
        model->setValue (app::id::WHELMED_LUA, app::id::paddingBottom, 10);
        model->setValue (app::id::WHELMED_LUA, app::id::paddingLeft,   10);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
