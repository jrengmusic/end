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
}

Window::~Window() { config.removeListener (this); }

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

        if (property == ID::mac or property == ID::win)
            styleParameters.get<juce::ValueTree> (
                property, std::move (windowNode.getChildWithName (IDtype::blurStyle)));
        else
            styleParameters.get<juce::ValueTree> (property, std::move (windowNode));
    }
}

void Window::registerStyleParameters()
{
    styleParameters.add<juce::ValueTree> (
        jam::ID::colour,
        [this] (juce::ValueTree n)
        {
            tintColour = jam::ValueTree::toColour (n.getProperty (jam::ID::colour));
            blurRadius = static_cast<float> (n.getProperty (ID::blurRadius));
            setGlass (tintColour, blurRadius, glassBackend);
        });

    styleParameters.add<juce::ValueTree> (
        ID::blurRadius,
        [this] (juce::ValueTree n)
        {
            tintColour = jam::ValueTree::toColour (n.getProperty (jam::ID::colour));
            blurRadius = static_cast<float> (n.getProperty (ID::blurRadius));
            setGlass (tintColour, blurRadius, glassBackend);
        });

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

    styleParameters.add<juce::ValueTree> (
        ID::mac,
        [this] (juce::ValueTree n)
        {
            glassBackend = jam::BackgroundBlur::fromString (
                n.getProperty (ID::mac, juce::String { IDref::backgroundBlur }).toString());
            setGlass (tintColour, blurRadius, glassBackend);
        });

    styleParameters.add<juce::ValueTree> (
        ID::win,
        [this] (juce::ValueTree n)
        {
            glassBackend = jam::BackgroundBlur::fromString (
                n.getProperty (ID::win, juce::String { IDref::blurBehind }).toString());
            setGlass (tintColour, blurRadius, glassBackend);
        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
