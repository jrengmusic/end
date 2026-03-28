#include "Parser.h"
#include "../config/WhelmedConfig.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Parser::Parser (State& s, int start)
    : juce::Thread ("Whelmed Parser")
    , state (s)
    , startBlock (start)
{
    const auto* cfg { Whelmed::Config::getContext() };

    bodySize = cfg->getFloat (Whelmed::Config::Key::fontSize);
    h1Size   = cfg->getFloat (Whelmed::Config::Key::h1Size);
    h2Size   = cfg->getFloat (Whelmed::Config::Key::h2Size);
    h3Size   = cfg->getFloat (Whelmed::Config::Key::h3Size);
    h4Size   = cfg->getFloat (Whelmed::Config::Key::h4Size);
    h5Size   = cfg->getFloat (Whelmed::Config::Key::h5Size);
    h6Size   = cfg->getFloat (Whelmed::Config::Key::h6Size);

    bodyColour = cfg->getColour (Whelmed::Config::Key::bodyColour);
    h1Colour   = cfg->getColour (Whelmed::Config::Key::h1Colour);
    h2Colour   = cfg->getColour (Whelmed::Config::Key::h2Colour);
    h3Colour   = cfg->getColour (Whelmed::Config::Key::h3Colour);
    h4Colour   = cfg->getColour (Whelmed::Config::Key::h4Colour);
    h5Colour   = cfg->getColour (Whelmed::Config::Key::h5Colour);
    h6Colour   = cfg->getColour (Whelmed::Config::Key::h6Colour);
    codeColour = cfg->getColour (Whelmed::Config::Key::codeColour);
    linkColour = cfg->getColour (Whelmed::Config::Key::linkColour);
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

            const bool isCode { (span.style & jreng::Markdown::Code) != jreng::Markdown::None };
            const bool isLink { (span.style & jreng::Markdown::Link) != jreng::Markdown::None };

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
