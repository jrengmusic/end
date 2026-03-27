#pragma once
#include <JuceHeader.h>
#include "../AppIdentifier.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class State
{
public:
    State();

    void setDocument (jreng::Markdown::ParsedDocument&& doc);
    const jreng::Markdown::ParsedDocument& getDocument() const noexcept;
    juce::ValueTree getValueTree() const noexcept;

private:
    juce::ValueTree state;
    jreng::Markdown::ParsedDocument document;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (State)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
