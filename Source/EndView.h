#pragma once
#include <JuceHeader.h>
#include "config/Config.h"
namespace end
{
/*____________________________________________________________________________*/
class View
    : public juce::Component
    , public juce::ValueTree::Listener
{
public:
    View() noexcept;
    ~View();

    void resized() override;

    /** @brief Intentionally empty — the View surface is transparent so the
        jam::Window glass tint shows through.  No ResizableWindow background
        colour is applied at this layer; the window owns the visual floor. */
    void paint (juce::Graphics&) override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    //==============================================================================
private:
    juce::ValueTree config { config::Model::get() };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
