#include "endWindow.h"

namespace end
{
/*____________________________________________________________________________*/

Window::Window (juce::Component* mainComponent, const juce::String& name, bool alwaysOnTop, bool showWindowButtons)
    : jam::Window { mainComponent, name, alwaysOnTop, showWindowButtons }
{
    config.addListener (this);
    registerStyleParameters();
    setStyle();
}

Window::~Window()
{
    config.removeListener (this);
}

void Window::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused (tree);
    applyStyleFor (property);
}

void Window::registerStyleParameters()
{
    styleParameters.add<juce::ValueTree> (jam::ID::colour,
        [this] (juce::ValueTree node)
        {
            tintColour = LookAndFeel::fromRGBA (node.getProperty (jam::ID::colour).toString());
            setGlass (tintColour, blurRadius, glassBackend);
        });

    styleParameters.add<juce::ValueTree> (ID::blurRadius,
        [this] (juce::ValueTree node)
        {
            blurRadius = static_cast<float> (node.getProperty (ID::blurRadius, 0.0));
            setGlass (tintColour, blurRadius, glassBackend);
        });

    styleParameters.add<juce::ValueTree> (ID::alwaysOnTop,
        [this] (juce::ValueTree node)
        {
            setAlwaysOnTop (static_cast<int> (node.getProperty (ID::alwaysOnTop, 0)) != 0);
        });

    styleParameters.add<juce::ValueTree> (ID::buttons,
        [this] (juce::ValueTree node)
        {
            setShowWindowButtons (static_cast<int> (node.getProperty (ID::buttons, 0)) != 0);
        });

    styleParameters.add<juce::ValueTree> (jam::ID::width,
        [this] (juce::ValueTree node)
        {
            setSize (static_cast<int> (node.getProperty (jam::ID::width, 640)), getHeight());
        });

    styleParameters.add<juce::ValueTree> (jam::ID::height,
        [this] (juce::ValueTree node)
        {
            setSize (getWidth(), static_cast<int> (node.getProperty (jam::ID::height, 480)));
        });

    styleParameters.add<juce::ValueTree> (ID::mac,
        [this] (juce::ValueTree node)
        {
            glassBackend = jam::BackgroundBlur::fromString (
                node.getProperty (ID::mac, juce::String { IDref::backgroundBlur }).toString());
            setGlass (tintColour, blurRadius, glassBackend);
        });

    styleParameters.add<juce::ValueTree> (ID::win,
        [this] (juce::ValueTree node)
        {
            glassBackend = jam::BackgroundBlur::fromString (
                node.getProperty (ID::win, juce::String { IDref::blurBehind }).toString());
            setGlass (tintColour, blurRadius, glassBackend);
        });
}

void Window::applyStyleFor (const juce::Identifier& property)
{
    if (styleParameters.contains (property))
    {
        auto windowNode { config.getChildWithName (IDtype::display).getChildWithName (IDtype::window) };

        if (property == ID::mac or property == ID::win)
            styleParameters.get<juce::ValueTree> (property, std::move (windowNode.getChildWithName (IDtype::blurStyle)));
        else
            styleParameters.get<juce::ValueTree> (property, std::move (windowNode));
    }
}

void Window::setStyle()
{
    applyStyleFor (jam::ID::colour);
    applyStyleFor (ID::blurRadius);
    applyStyleFor (ID::alwaysOnTop);
    applyStyleFor (ID::buttons);
    applyStyleFor (jam::ID::width);
    applyStyleFor (jam::ID::height);
    applyStyleFor (ID::mac);
    applyStyleFor (ID::win);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
