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
    registerStyleParameters();

    for (const auto& [key, value] : styleParameters)
        setStyle (key);

    config.addListener (this);
    lookAndFeelChanged();
}

Window::~Window() { config.removeListener (this); }

void Window::lookAndFeelChanged()
{
    auto& laf { static_cast<end::LookAndFeel&> (getLookAndFeel()) };
    auto [argb, blur, fx] = laf.getWindowGlass();

    setGlass (juce::Colour (argb),
              static_cast<float> (blur),
              static_cast<jam::BackgroundBlur::WindowFX> (fx));
}

void Window::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused (tree);
    setStyle (property);
}

void Window::setStyle (const juce::Identifier& property)
{
    if (styleParameters.contains (property))
    {
        auto windowNode {
            config.getChildWithName (IDtype::display).getChildWithName (IDtype::window)
        };

        styleParameters.get<juce::ValueTree> (property, std::move (windowNode));
    }
}

void Window::registerStyleParameters()
{
    styleParameters.add<juce::ValueTree> (
        ID::alwaysOnTop,
        [this] (juce::ValueTree n)
        {
            setAlwaysOnTop (Boolean::get (n.getProperty (ID::alwaysOnTop).toString()));
        });

    styleParameters.add<juce::ValueTree> (
        ID::buttons,
        [this] (juce::ValueTree n)
        {
            setShowWindowButtons (Boolean::get (n.getProperty (ID::buttons).toString()));
        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
