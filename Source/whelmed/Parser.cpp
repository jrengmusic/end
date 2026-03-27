#include "Parser.h"
#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Parser::Parser (State& stateToWriteTo)
    : juce::Thread ("Whelmed Parser")
    , state (stateToWriteTo)
{
}

Parser::~Parser()
{
    signalThreadShouldExit();
    waitForThreadToExit (5000);
}

void Parser::startParsing (const juce::File& file)
{
    fileToParse = file;
    startThread();
}

void Parser::run()
{
    auto content { fileToParse.loadFileAsString() };

    if (content.isNotEmpty() and not threadShouldExit())
    {
        auto parsed { jreng::Markdown::Parser::parse (content) };

        if (not threadShouldExit())
            state.commitDocument (std::move (parsed));
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
