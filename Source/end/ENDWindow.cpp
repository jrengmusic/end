#include "end/ENDWindow.h"

ENDWindow::ENDWindow (juce::Component* mainComponent,
                      const juce::String& name)
    : jam::Window { mainComponent, name, false, true }
{
    lookAndFeelChanged();
}

void ENDWindow::lookAndFeelChanged()
{
    auto [colour, blur, fx, windowButtons] = lookAndFeel.getWindowStyle();

    jam::StyleWindow::apply (this, colour);
    jam::BackgroundBlur::enable (this,
                                 static_cast<jam::BackgroundBlur::WindowFX> (fx),
                                 static_cast<float> (blur),
                                 colour);

    if (auto* peer { getPeer() })
        jam::StyleWindow::setButtons (*peer, windowButtons);
}
