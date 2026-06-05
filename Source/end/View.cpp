#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View() noexcept
{
    setOpaque (false);
    config.addListener (this);

    auto window { jam::ValueTree::getChildWithName (config, IDtype::window) };
    int width { window.getProperty (jam::ID::width) };
    int height { window.getProperty (jam::ID::height) };
    setSize (width, height);
}

View::~View() { config.removeListener (this); }
void View::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) {}

void View::resized() {}
void View::paint (juce::Graphics& g) {}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
