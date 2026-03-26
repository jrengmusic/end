#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

class BlockRenderer : public jreng::GLComponent
{
public:
    BlockRenderer (const jreng::Markdown::Block& block,
                   jreng::Typeface& typeface);

    void paintGL (jreng::GLGraphics& g) noexcept override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredHeight() const noexcept;

private:
    juce::AttributedString attributedString;
    jreng::TextLayout layout;
    jreng::Typeface& typeface;
    int preferredHeight { 0 };

    void rebuildLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockRenderer)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
