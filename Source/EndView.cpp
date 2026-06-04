#include "EndView.h"

namespace end
{
/*____________________________________________________________________________*/
View::View() noexcept
{
    juce::LookAndFeel::setDefaultLookAndFeel (&defaultLookAndFeel);

    setSize (300, 300);
}
void View::resized() {}

void View::paint (juce::Graphics& g)
{
    if (isOpaque())
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
    }
}
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
