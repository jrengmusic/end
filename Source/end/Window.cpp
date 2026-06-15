#include "end/Window.h"

namespace end
{
/*____________________________________________________________________________*/

Window::Window (juce::Component* mainComponent,
                const juce::String& name)
    : jam::Window { mainComponent, name, false, true }
{
    lookAndFeelChanged();
}

void Window::lookAndFeelChanged()
{
    auto& laf { static_cast<end::LookAndFeel&> (getLookAndFeel()) };
    auto [colour, blur, fx] = laf.getWindowGlass();

    setGlass (colour,
              static_cast<float> (blur),
              static_cast<jam::BackgroundBlur::WindowFX> (fx));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
