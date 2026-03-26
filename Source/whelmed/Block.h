#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

class Block : public juce::Component
{
public:
    explicit Block (const juce::AttributedString& attributedString);

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredHeight() const noexcept;

private:
    void rebuildLayout();

    juce::AttributedString attributedString;
    juce::TextLayout layout;
    int preferredHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Block)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
