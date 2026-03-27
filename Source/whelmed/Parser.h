/**
 * @file Parser.h
 * @brief Background thread for markdown file parsing.
 *
 * Analogous to Terminal's TTY reader thread. Reads file and parses
 * on background thread. Signals completion via atomic fence on State.
 *
 * @see Whelmed::State
 * @see jreng::Markdown::Parser
 */

#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

class State;

class Parser : public juce::Thread
{
public:
    explicit Parser (State& stateToWriteTo);
    ~Parser() override;

    void startParsing (const juce::File& file);

    void run() override;

private:
    State& state;
    juce::File fileToParse;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Parser)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
