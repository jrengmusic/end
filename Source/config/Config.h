#include <JuceHeader.h>

namespace config
{
/*____________________________________________________________________________*/

class Model
    : public jam::Model
    , public jam::Context<Model>
{
public:
    Model();
    ~Model() = default;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
