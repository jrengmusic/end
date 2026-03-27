#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

/**
    Base class for all renderable block types in the Whelmed document stack.

    Blocks are NOT juce::Components. They are data objects that know how to
    measure their height and paint themselves into a Graphics context.
    Whelmed::Screen owns blocks and renders them in a single unified paint() call.
*/
class Block
{
public:
    Block() = default;
    virtual ~Block() = default;

    /** Returns the preferred height for a given width. */
    virtual int getPreferredHeight (int width) const noexcept = 0;

    /** Paints this block into the given area. */
    virtual void paint (juce::Graphics& g, juce::Rectangle<int> area) const = 0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Block)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
