#include "Parser.h"
#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Parser::Parser (State& stateToWriteTo, const jreng::Markdown::DocConfig& docConfigToUse)
    : juce::Thread ("Whelmed Parser")
    , state (stateToWriteTo)
    , docConfig (docConfigToUse)
{
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

    // Resolve styles into each block and span.
    // Document is already stored in State by the message thread before start() was called.
    // Parser thread is the sole writer here; message thread only reads blocks[0..builtCount-1].
    for (int i { 0 }; i < doc.blockCount and not threadShouldExit(); ++i)
    {
        auto& block { doc.blocks[i] };

        switch (block.level)
        {
            case 1:
                block.fontSize = docConfig.h1Size;
                block.colour   = docConfig.h1Colour;
                break;
            case 2:
                block.fontSize = docConfig.h2Size;
                block.colour   = docConfig.h2Colour;
                break;
            case 3:
                block.fontSize = docConfig.h3Size;
                block.colour   = docConfig.h3Colour;
                break;
            case 4:
                block.fontSize = docConfig.h4Size;
                block.colour   = docConfig.h4Colour;
                break;
            case 5:
                block.fontSize = docConfig.h5Size;
                block.colour   = docConfig.h5Colour;
                break;
            case 6:
                block.fontSize = docConfig.h6Size;
                block.colour   = docConfig.h6Colour;
                break;
            default:
                block.fontSize = docConfig.bodySize;
                block.colour   = docConfig.bodyColour;
                break;
        }

        for (int s { block.spanOffset }; s < block.spanOffset + block.spanCount; ++s)
        {
            auto& span { doc.spans[s] };

            bool isCode { (span.style & jreng::Markdown::Code) != jreng::Markdown::None };
            bool isLink { (span.style & jreng::Markdown::Link) != jreng::Markdown::None };

            if (isCode)
            {
                span.fontSize   = docConfig.codeSize;
                span.colour     = docConfig.codeColour;
                span.fontFamily = 1;
            }
            else
            {
                span.fontSize   = block.fontSize;
                span.fontFamily = 0;

                if (isLink)
                    span.colour = docConfig.linkColour;
                else
                    span.colour = block.colour;
            }
        }

        state.appendBlock();
    }

    state.setParseComplete();
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
