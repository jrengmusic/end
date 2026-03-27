#pragma once
#include <JuceHeader.h>
#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class TextBlock : public Block
{
public:
    TextBlock();

    void clear() noexcept;
    void appendStyledText (const juce::String& text,
                           const juce::FontOptions& fontOptions,
                           juce::Colour colour);

    void resized() override;
    int getPreferredHeight() const noexcept override;

private:
    juce::TextEditor editor;
    int preferredHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
