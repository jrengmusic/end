#include "end/ENDWindow.h"

ENDWindow::ENDWindow (juce::Component* mainComponent,
                      const juce::String& name)
    : jam::Window { mainComponent, name, false, true }
{
    lookAndFeelChanged();
}

void ENDWindow::lookAndFeelChanged()
{
    lookAndFeel.prepareWindow (*this);
}
