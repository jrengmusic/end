#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

class CodeBlock : public juce::Component
{
public:
    CodeBlock (const juce::String& code, const juce::String& language);
    ~CodeBlock() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredHeight() const noexcept;

private:
    juce::CodeDocument document;
    std::unique_ptr<juce::CodeTokeniser> tokeniser;
    std::unique_ptr<juce::CodeEditorComponent> editor;

    int preferredHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CodeBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
