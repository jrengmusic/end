#include "end/Window.h"

namespace end
{
/*____________________________________________________________________________*/

Window::Window (juce::Component* mainComponent,
                const juce::String& name,
                bool alwaysOnTop,
                bool showWindowButtons)
    : jam::Window { mainComponent, name, alwaysOnTop, showWindowButtons }
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
