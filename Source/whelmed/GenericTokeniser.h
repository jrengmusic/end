#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

struct LanguageDefinition
{
    juce::StringArray keywords;
    juce::String lineComment;       // e.g. "//", "#", "--"
    juce::String blockCommentStart; // e.g. "/*", "<!--"
    juce::String blockCommentEnd;   // e.g. "*/", "-->"
    bool hasBacktickStrings { false };
};

class GenericTokeniser : public juce::CodeTokeniser
{
public:
    explicit GenericTokeniser (const LanguageDefinition& definition);

    int readNextToken (juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;

private:
    LanguageDefinition lang;
    juce::StringArray sortedKeywords;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericTokeniser)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
