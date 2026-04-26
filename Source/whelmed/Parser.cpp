#include "Parser.h"
#include "../lua/Engine.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Parser::Parser (State& s, int start)
    : juce::Thread ("Whelmed Parser")
    , state (s)
    , startBlock (start)
{
    const auto* cfg { lua::Engine::getContext() };

    bodySize = cfg->whelmed.fontSize;
    h1Size   = cfg->whelmed.h1Size;
    h2Size   = cfg->whelmed.h2Size;
    h3Size   = cfg->whelmed.h3Size;
    h4Size   = cfg->whelmed.h4Size;
    h5Size   = cfg->whelmed.h5Size;
    h6Size   = cfg->whelmed.h6Size;

    bodyColour = cfg->whelmed.bodyColour;
    h1Colour   = cfg->whelmed.h1Colour;
    h2Colour   = cfg->whelmed.h2Colour;
    h3Colour   = cfg->whelmed.h3Colour;
    h4Colour   = cfg->whelmed.h4Colour;
    h5Colour   = cfg->whelmed.h5Colour;
    h6Colour   = cfg->whelmed.h6Colour;
    codeColour = cfg->whelmed.codeColour;
    linkColour = cfg->whelmed.linkColour;
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
} // namespace Whelmed
