#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

/**
    Common base for all renderable block types in the Whelmed document stack.

    Provides a uniform interface for height queries, eliminating dynamic_cast
    chains in the layout pass.
*/
class Block : public juce::Component
{
public:
    ~Block() override = default;

    virtual int getPreferredHeight() const noexcept = 0;
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
