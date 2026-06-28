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
            const auto gpu { jam::GpuProbe::probe() };
            bool canUseGpu { config.getValue (IDtype::display, ID::gpu) and gpu.isAvailable };
            jam::BackgroundBlur::setEnabled (canUseGpu);

            if (canUseGpu and not vulkanEngine)
                vulkanEngine = std::make_unique<jam::VulkanEngineRegistry>();
            else if (not canUseGpu and vulkanEngine)
                vulkanEngine.reset();
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
                window->setShowWindowButtons (tree.getProperty (ID::titleBarButtons));
        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
