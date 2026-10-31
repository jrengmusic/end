#include "ui/ENDView.h"

void ENDView::registerGraphicsEvents()
{
    events.add<juce::ValueTree&> (Id::backgroundOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (Id::frameRate,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (Id::backgroundResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });
}

void ENDView::registerWindowEvents()
{
    events.add<juce::ValueTree&> (
        Id::alwaysOnTop,
        [this] (juce::ValueTree& row)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setAlwaysOnTop (row.getProperty (Id::value));
        });

    events.add<juce::ValueTree&> (
        Id::titleBarButtons,
        [this] (juce::ValueTree& row)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setWindowButtons (row.getProperty (Id::value));
        });
}

void ENDView::registerMouseEvents()
{
    events.add<juce::ValueTree&> (Id::enabled,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (Id::imouse,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (Id::orbit,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (Id::reset,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });
}

void ENDView::registerEvents()
{
    events.add<juce::ValueTree&> (
        Id::focusedPane,
        [this] (juce::ValueTree& tree)
        {
            // Re-points the conduit to whichever TAB row last wrote its own
            // focusedPane — valueChanged() below then mirrors that value
            // onto the SESSIONS-level parameter, last change wins.
            if (tree.getType() == Id::toType (Id::tab))
                focusedPane.referTo (tree.getPropertyAsValue (Id::focusedPane, nullptr));
        });

    events.add<juce::ValueTree&> (Id::theme,
                                  [this] (juce::ValueTree&)
                                  {
                                      getTopLevelComponent()->sendLookAndFeelChange();
                                  });

    registerGraphicsEvents();
    registerWindowEvents();
    registerMouseEvents();

    // WINDOW leaf visibility toggles (ENDActions.cpp's own
    // Position-bimap loop) relayout the dormant dock-pane machinery — one
    // registration on the shared Id::visible property key serves all
    // five leaves, no per-leaf type fallback needed (every WINDOW leaf now
    // shares Id::toType (Id::pane), so a type-keyed fallback could no longer
    // distinguish them the way the four old Position-keyed ValueTree TYPES did).
    events.add<juce::ValueTree&> (Id::visible,
                                  [this] (juce::ValueTree&)
                                  {
                                      resized();
                                  });
}

static std::tuple<float, float, int> getBackgroundRenderParams (ConfigModel& config)
{
    const float opacity { config.getRowValue (Id::toType (Id::graphics), Id::backgroundOpacity) };
    const float resolutionScale { config.getRowValue (Id::toType (Id::graphics), Id::backgroundResolution) };
    const int frameRate { config.getRowValue (Id::toType (Id::graphics), Id::frameRate) };

    return { opacity, resolutionScale, frameRate };
}

void ENDView::setBackground (std::unique_ptr<jam::VulkanShader> shader)
{
    const auto [opacity, resolutionScale, frameRate] { getBackgroundRenderParams (config) };

    background.setShader (std::move (shader), opacity, resolutionScale, frameRate);
}

void ENDView::setBackgroundParams()
{
    const auto [opacity, resolutionScale, frameRate] { getBackgroundRenderParams (config) };

    background.setParams (opacity, resolutionScale, frameRate);
}

void ENDView::setMouseConfig()
{
    const bool enabled { config.getRowValue (Id::toType (Id::mouse), Id::enabled) };
    const auto imouseButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (Id::toType (Id::mouse), Id::imouse).toString())) };
    const auto orbitButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (Id::toType (Id::mouse), Id::orbit).toString())) };
    const auto resetButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (Id::toType (Id::mouse), Id::reset).toString())) };

    background.setMouseConfig (enabled, imouseButton, orbitButton, resetButton);
}
