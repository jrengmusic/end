#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

class TextBlock : public juce::Component
{
public:
    TextBlock();

    void appendStyledText (const juce::String& text,
                           const juce::FontOptions& fontOptions,
                           juce::Colour colour);

    void resized() override;
    int getPreferredHeight() const noexcept;

private:
    juce::TextEditor editor;
    int preferredHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
