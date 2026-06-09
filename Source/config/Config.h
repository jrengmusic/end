#pragma once
#include <JuceHeader.h>
#include "../end/Map.h"

namespace config
{
/*____________________________________________________________________________*/

class Model
    : public jam::Model
    , public jam::Context<Model>
{
public:
    //==========================================================================
    Model();

    ~Model() = default;

    juce::Rectangle<int> getInitWindowSize() const noexcept;

private:
    void initialise();
    void writeToPath();
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
