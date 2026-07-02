#include "View.h"

namespace end
{
/*____________________________________________________________________________*/

void View::registerEvents()
{
    events.add<juce::ValueTree&> (ID::tabOrientation,
                                  [this] (juce::ValueTree&)
                                  {
                                      setTabOrientation();
                                  });

    events.add<juce::ValueTree&> (ID::focus,
                                  [this] (juce::ValueTree& tree)
                                  {
                                      if (jam::toBool (tree.getProperty (ID::focus)))
                                      {
                                          auto id { tree.getProperty (jam::ID::id) };
                                          state.setProperty (ID::focusedPane, id, nullptr);
                                      }
                                  });

    events.add<juce::ValueTree&> (ID::theme,
                                  [this] (juce::ValueTree&)
                                  {
                                      getTopLevelComponent()->sendLookAndFeelChange();
                                  });

    events.add<juce::ValueTree&> (
        ID::gpu,
        [this] (juce::ValueTree&)
        {
            // jam::vulkan::Registry is constructed unconditionally, once, by
            // end::Application, and never reset/reconstructed here — GPU
            // availability/preference only selects which rendering engine
            // createContext() dispatches to per paint (see end::Application's
            // vulkanEngine doc comment, Main.h).
            const bool canUseGpu {
                config.getValue (IDtype::display, ID::gpu) and jam::GpuProbe::probe().isAvailable
            };

            jam::BackgroundBlur::setEnabled (canUseGpu);

            auto* registry { jam::vulkan::Registry::getInstance() };
            jassert (registry != nullptr);
            registry->setGpuEnabled (canUseGpu);
        });

    events.add<juce::ValueTree&> (
        ID::alwaysOnTop,
        [this] (juce::ValueTree& tree)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setAlwaysOnTop (tree.getProperty (ID::alwaysOnTop));
        });

    events.add<juce::ValueTree&> (
        ID::titleBarButtons,
        [this] (juce::ValueTree& tree)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setWindowButtons (tree.getProperty (ID::titleBarButtons));
        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
