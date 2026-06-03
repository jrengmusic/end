#include "Parser.h"
#include "../AppModel.h"

namespace whelmed
{
/*____________________________________________________________________________*/

Parser::Parser (State& s, int start)
    : juce::Thread ("Whelmed Parser")
    , state (s)
    , startBlock (start)
{
    const auto* appState { AppModel::getContext() };

    bodySize = appState->getValue<float> (app::id::WHELMED_LUA, app::id::fontSize);
    h1Size   = appState->getValue<float> (app::id::WHELMED_LUA, app::id::h1Size);
    h2Size   = appState->getValue<float> (app::id::WHELMED_LUA, app::id::h2Size);
    h3Size   = appState->getValue<float> (app::id::WHELMED_LUA, app::id::h3Size);
    h4Size   = appState->getValue<float> (app::id::WHELMED_LUA, app::id::h4Size);
    h5Size   = appState->getValue<float> (app::id::WHELMED_LUA, app::id::h5Size);
    h6Size   = appState->getValue<float> (app::id::WHELMED_LUA, app::id::h6Size);

    bodyColour = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::bodyColour)));
    h1Colour   = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::h1Colour)));
    h2Colour   = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::h2Colour)));
    h3Colour   = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::h3Colour)));
    h4Colour   = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::h4Colour)));
    h5Colour   = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::h5Colour)));
    h6Colour   = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::h6Colour)));
    codeColour = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::codeColour)));
    linkColour = juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::WHELMED_LUA, app::id::linkColour)));
}

Parser::~Parser()
{
    signalThreadShouldExit();
    waitForThreadToExit (5000);
}

void Parser::start()
{
    startThread();
}

void Parser::run()
{
    auto& doc { state.getDocumentForWriting() };

    static constexpr int kMaxHeadingLevel { 6 };

    const float sizes[] { bodySize, h1Size, h2Size, h3Size, h4Size, h5Size, h6Size };
    const juce::Colour colours[] { bodyColour, h1Colour, h2Colour, h3Colour, h4Colour, h5Colour, h6Colour };

    for (int i { startBlock }; i < doc.blockCount and not threadShouldExit(); ++i)
    {
        auto& block { doc.blocks[i] };

        const int level { juce::jlimit (0, kMaxHeadingLevel, block.level) };
        block.fontSize = sizes[level];
        block.colour   = colours[level];

        for (int s { block.spanOffset }; s < block.spanOffset + block.spanCount; ++s)
        {
            auto& span { doc.spans[s] };

            span.fontSize = block.fontSize;

            const bool isCode { (span.style & jam::Markdown::Code) != jam::Markdown::None };
            const bool isLink { (span.style & jam::Markdown::Link) != jam::Markdown::None };

            if (isCode)
            {
                span.colour     = codeColour;
                span.fontFamily = 1;
            }
            else if (isLink)
            {
                span.colour     = linkColour;
                span.fontFamily = 0;
            }
            else
            {
                span.colour     = block.colour;
                span.fontFamily = 0;
            }
        }

        state.appendBlock();
    }

    state.setParseComplete();
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace whelmed
