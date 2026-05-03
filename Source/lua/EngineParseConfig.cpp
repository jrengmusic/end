/**
 * @file EngineParseConfig.cpp
 * @brief Lua table parse methods for lua::Engine nexus and whelmed configuration.
 *
 * Contains: Engine::parseNexus(), Engine::parseWhelmed().
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
    jam::lua::Value nexusTable { lua["END"]["nexus"] };

    if (nexusTable.isTable())
    {
        auto gpu { nexusTable["gpu"].optional<juce::String>() };
        if (gpu.has_value()) nexus.gpu = gpu.value();

        auto daemon { nexusTable["daemon"].optional<juce::String>() };
        if (daemon.has_value()) nexus.daemon = (daemon.value() == "true");

        auto autoReload { nexusTable["auto_reload"].optional<juce::String>() };
        if (autoReload.has_value()) nexus.autoReload = (autoReload.value() == "true");

        // Shell sub-table
        jam::lua::Value shellTable { nexusTable["shell"] };

        if (shellTable.isTable())
        {
            auto program { shellTable["program"].optional<juce::String>() };
            if (program.has_value()) nexus.shell.program = program.value();

            auto args { shellTable["args"].optional<juce::String>() };
            if (args.has_value()) nexus.shell.args = args.value();

            auto integration { shellTable["integration"].optional<juce::String>() };
            if (integration.has_value()) nexus.shell.integration = (integration.value() == "true");
        }

        // Terminal sub-table
        jam::lua::Value terminalTable { nexusTable["terminal"] };

        if (terminalTable.isTable())
        {
            auto scrollbackLines { terminalTable["scrollback_lines"].optional<double>() };
            if (scrollbackLines.has_value())
                nexus.terminal.scrollbackLines = juce::jlimit (100, 1000000, static_cast<int> (scrollbackLines.value()));

            auto scrollStep { terminalTable["scroll_step"].optional<double>() };
            if (scrollStep.has_value())
                nexus.terminal.scrollStep = juce::jlimit (1, 100, static_cast<int> (scrollStep.value()));

            auto dropMultifiles { terminalTable["drop_multifiles"].optional<juce::String>() };
            if (dropMultifiles.has_value()) nexus.terminal.dropMultifiles = dropMultifiles.value();

            auto dropQuoted { terminalTable["drop_quoted"].optional<juce::String>() };
            if (dropQuoted.has_value()) nexus.terminal.dropQuoted = (dropQuoted.value() == "true");

            // Padding array: { top, right, bottom, left }
            jam::lua::Value p { terminalTable["padding"] };

            if (p.isTable())
            {
                auto paddingTop { p[1].optional<double>() };
                if (paddingTop.has_value())
                    nexus.terminal.paddingTop = juce::jlimit (0, 50, static_cast<int> (paddingTop.value()));

                auto paddingRight { p[2].optional<double>() };
                if (paddingRight.has_value())
                    nexus.terminal.paddingRight = juce::jlimit (0, 50, static_cast<int> (paddingRight.value()));

                auto paddingBottom { p[3].optional<double>() };
                if (paddingBottom.has_value())
                    nexus.terminal.paddingBottom = juce::jlimit (0, 50, static_cast<int> (paddingBottom.value()));

                auto paddingLeft { p[4].optional<double>() };
                if (paddingLeft.has_value())
                    nexus.terminal.paddingLeft = juce::jlimit (0, 50, static_cast<int> (paddingLeft.value()));
            }
        }

        // Hyperlinks sub-table
        jam::lua::Value hyperlinksTable { nexusTable["hyperlinks"] };

        if (hyperlinksTable.isTable())
        {
            auto editor { hyperlinksTable["editor"].optional<juce::String>() };
            if (editor.has_value()) nexus.hyperlinks.editor = editor.value();

            // Handlers map
            jam::lua::Value handlersTable { hyperlinksTable["handlers"] };

            // Built-in handlers — always registered, user config can override.
            nexus.hyperlinks.handlers[".md"]   = "whelmed";
            nexus.hyperlinks.handlers[".png"]  = "image";
            nexus.hyperlinks.handlers[".jpg"]  = "image";
            nexus.hyperlinks.handlers[".jpeg"] = "image";
            nexus.hyperlinks.handlers[".gif"]  = "image";

            if (handlersTable.isTable())
            {
                handlersTable.forEach ([this] (const jam::lua::Value& k, const jam::lua::Value& v)
                {
                    if (k.getType() == jam::lua::Type::string and v.getType() == jam::lua::Type::string)
                        nexus.hyperlinks.handlers[k.to<juce::String>()] = v.to<juce::String>();
                });
            }

            // Extensions set
            jam::lua::Value extensionsTable { hyperlinksTable["extensions"] };

            if (extensionsTable.isTable())
            {
                extensionsTable.forEach ([this] (const jam::lua::Value& /*k*/, const jam::lua::Value& v)
                {
                    if (v.getType() == jam::lua::Type::string)
                        nexus.hyperlinks.extensions.insert (v.to<juce::String>());
                });
            }
        }

        // Image sub-table
        jam::lua::Value imageTable { nexusTable["image"] };

        if (imageTable.isTable())
        {
            auto atlasBudget { imageTable["atlas_budget"].optional<double>() };

            if (atlasBudget.has_value())
                nexus.image.atlasBudgetBytes = juce::jlimit (1 * 1024 * 1024,
                                                              256 * 1024 * 1024,
                                                              static_cast<int> (atlasBudget.value()));

            auto atlasDim { imageTable["atlas_dimension"].optional<double>() };

            if (atlasDim.has_value())
                nexus.image.atlasDimension = juce::jlimit (1024, 8192, static_cast<int> (atlasDim.value()));

            auto cols { imageTable["cols"].optional<double>() };
            if (cols.has_value())
                nexus.image.cols = juce::jlimit (10, 200, static_cast<int> (cols.value()));

            auto rows { imageTable["rows"].optional<double>() };
            if (rows.has_value())
                nexus.image.rows = juce::jlimit (5, 100, static_cast<int> (rows.value()));

            auto pad { imageTable["padding"].optional<double>() };
            if (pad.has_value())
                nexus.image.padding = juce::jlimit (0, 64, static_cast<int> (pad.value()));

            auto brd { imageTable["border"].optional<bool>() };
            if (brd.has_value())
                nexus.image.border = brd.value();
        }
    }
}

//==============================================================================
void Engine::parseWhelmed()
{
    jam::lua::Value whelmedTable { lua["END"]["whelmed"] };

    if (whelmedTable.isTable())
    {
        auto readStr = [&whelmedTable] (const char* key, juce::String& target)
        {
            auto val { whelmedTable[key].optional<juce::String>() };
            if (val.has_value()) target = val.value();
        };

        auto readFloat = [&whelmedTable] (const char* key, float& target)
        {
            auto val { whelmedTable[key].optional<double>() };
            if (val.has_value()) target = static_cast<float> (val.value());
        };

        auto readColour = [&whelmedTable] (const char* key, juce::Colour& target)
        {
            auto val { whelmedTable[key].optional<juce::String>() };
            if (val.has_value()) target = parseColour (val.value());
        };

        auto readInt = [&whelmedTable] (const char* key, int& target)
        {
            auto val { whelmedTable[key].optional<double>() };
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
        jam::lua::Value p { whelmedTable["padding"] };

        if (p.isTable())
        {
            auto paddingTop { p[1].optional<double>() };
            if (paddingTop.has_value())
                whelmed.paddingTop = juce::jlimit (0, 50, static_cast<int> (paddingTop.value()));

            auto paddingRight { p[2].optional<double>() };
            if (paddingRight.has_value())
                whelmed.paddingRight = juce::jlimit (0, 50, static_cast<int> (paddingRight.value()));

            auto paddingBottom { p[3].optional<double>() };
            if (paddingBottom.has_value())
                whelmed.paddingBottom = juce::jlimit (0, 50, static_cast<int> (paddingBottom.value()));

            auto paddingLeft { p[4].optional<double>() };
            if (paddingLeft.has_value())
                whelmed.paddingLeft = juce::jlimit (0, 50, static_cast<int> (paddingLeft.value()));
        }
    }
}

} // namespace lua
