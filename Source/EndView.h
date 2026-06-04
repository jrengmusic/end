#include <JuceHeader.h>
#include "lookAndFeel/LookAndFeel.h"
namespace end
{
/*____________________________________________________________________________*/
class View : public juce::Component
{
public:
    View() noexcept;
    ~View() = default;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    LookAndFeel defaultLookAndFeel;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
