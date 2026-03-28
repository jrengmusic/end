#pragma once
#include <JuceHeader.h>
#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Parser : public juce::Thread
{
public:
    Parser (State& state, int startBlock);
    ~Parser() override;

    void start();

private:
    void run() override;

    State& state;
    int startBlock;

    float bodySize;
    float h1Size;
    float h2Size;
    float h3Size;
    float h4Size;
    float h5Size;
    float h6Size;

    juce::Colour bodyColour;
    juce::Colour h1Colour;
    juce::Colour h2Colour;
    juce::Colour h3Colour;
    juce::Colour h4Colour;
    juce::Colour h5Colour;
    juce::Colour h6Colour;
    juce::Colour codeColour;
    juce::Colour linkColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Parser)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
