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
            // jam::VulkanEngine is constructed unconditionally, once, by
            // end::Application, and never reset/reconstructed here — GPU
            // availability/preference only selects which rendering engine
            // createContext() dispatches to per paint (see end::Application's
            // vulkanEngine doc comment, Main.h).
            const bool canUseGpu {
                config.getValue (IDtype::display, ID::gpu) and jam::GpuProbe::probe().isAvailable
            };

            jam::BackgroundBlur::setEnabled (canUseGpu);

            auto* engine { jam::VulkanEngine::getInstance() };
            jassert (engine != nullptr);
            engine->setGpuEnabled (canUseGpu);

            // Background/post-process shaders never exist independent of the
            // effective GPU state (Locked Decision 4) — both funnels re-derive
            // it themselves and discard/clear on every toggle away from GPU,
            // recompile from current config on every toggle onto it. This is
            // also the SAME call this View makes at initial config load — see
            // this constructor's callAsync block, which fires this very
            // handler once at startup.
            applyBackground();
            applyPostProcess();
        });

    events.add<juce::ValueTree&> (ID::background,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyBackground();
                                  });

    events.add<juce::ValueTree&> (ID::backgroundOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (ID::frameRate,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (ID::backgroundResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (
        ID::filter,
        [this] (juce::ValueTree&)
        {
            // filter is shared by both slots (display.lua: applies to both the
            // background and post-processing upscale) — baked into the
            // compiled prelude (jam::vulkan::ShaderCompiler::channelMacros()/sceneMacro()),
            // so a filter change requires a full recompile on both funnels,
            // never the cheaper *Params() path.
            applyBackground();
            applyPostProcess();
        });

    events.add<juce::ValueTree&> (ID::postProcessing,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyPostProcess();
                                  });

    events.add<juce::ValueTree&> (ID::postProcessingOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyPostProcessParams();
                                  });

    events.add<juce::ValueTree&> (ID::postProcessingResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyPostProcessParams();
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

    events.add<juce::ValueTree&> (jam::ID::enabled,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyMouseConfig();
                                  });

    events.add<juce::ValueTree&> (ID::imouse,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyMouseConfig();
                                  });

    events.add<juce::ValueTree&> (ID::orbit,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyMouseConfig();
                                  });

    events.add<juce::ValueTree&> (ID::reset,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyMouseConfig();
                                  });
}

void View::applyBackground()
{
    // Same effective-gpu truth end::Application resolves for the VulkanEngine
    // ctor and the gpu event handler resolves for setGpuEnabled() — never
    // raw config alone.
    const bool gpuEnabled {
        config.getValue (IDtype::display, ID::gpu) and jam::GpuProbe::probe().isAvailable
    };

    const auto projectName { config.getValue (IDtype::graphics, ID::background).toString() };
    const float opacity { config.getValue (IDtype::graphics, ID::backgroundOpacity) };
    const float resolutionScale { config.getValue (IDtype::graphics, ID::backgroundResolution) };
    const int frameRate { config.getValue (IDtype::graphics, ID::frameRate) };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (config.state, IDtype::background) };
        const auto filterName { config.getValue (IDtype::graphics, ID::filter).toString() };
        const auto filter { jam::map::ImageResample::get (filterName) };

        // config::Shader::loadFromPath() always stamps ID::shaderFormat with a
        // definite format ordinal (jam::vulkan::ShaderFormat::shadertoy or
        // ::slang) before this state is ever readable here — config::Model's
        // constructor runs loadFromPath() to completion, and
        // jam::Instance<config::Model>::getInstance() (which View::config
        // resolves through) cannot return before that constructor finishes.
        const int shaderFormat { shaderState.getProperty (ID::shaderFormat) };

        auto compiled { jam::vulkan::ShaderCompiler::compile (shaderState, true, shaderFormat, filter) };

        // nullptr (compile failure, diagnostic already logged inside ShaderCompiler)
        // keeps whichever shader background currently holds — call nothing (last-good).
        if (compiled != nullptr)
            background.setShader (std::move (compiled), opacity, resolutionScale, frameRate);
    }
    else
    {
        background.setShader (nullptr, opacity, resolutionScale, frameRate);
    }
}

void View::applyBackgroundParams()
{
    const float opacity { config.getValue (IDtype::graphics, ID::backgroundOpacity) };
    const float resolutionScale { config.getValue (IDtype::graphics, ID::backgroundResolution) };
    const int frameRate { config.getValue (IDtype::graphics, ID::frameRate) };

    background.setParams (opacity, resolutionScale, frameRate);
}

void View::applyPostProcess()
{
    const bool gpuEnabled {
        config.getValue (IDtype::display, ID::gpu) and jam::GpuProbe::probe().isAvailable
    };

    const auto projectName { config.getValue (IDtype::graphics, ID::postProcessing).toString() };
    const float opacity { config.getValue (IDtype::graphics, ID::postProcessingOpacity) };
    const float resolutionScale { config.getValue (IDtype::graphics, ID::postProcessingResolution) };

    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (config.state, IDtype::postProcessing) };
        const auto filterName { config.getValue (IDtype::graphics, ID::filter).toString() };
        const auto filter { jam::map::ImageResample::get (filterName) };

        // See applyBackground()'s matching comment — ID::shaderFormat is
        // always a definite format ordinal by the time this state is
        // readable here.
        const int shaderFormat { shaderState.getProperty (ID::shaderFormat) };

        auto compiled { jam::vulkan::ShaderCompiler::compile (shaderState, false, shaderFormat, filter) };

        // nullptr (compile failure, diagnostic already logged inside ShaderCompiler)
        // keeps whichever chain VulkanEngine currently holds — call nothing.
        if (compiled != nullptr)
            engine->setPostProcess (std::move (compiled), opacity, resolutionScale);
    }
    else
    {
        engine->setPostProcess (nullptr, opacity, resolutionScale);
    }
}

void View::applyPostProcessParams()
{
    const float opacity { config.getValue (IDtype::graphics, ID::postProcessingOpacity) };
    const float resolutionScale { config.getValue (IDtype::graphics, ID::postProcessingResolution) };

    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);
    engine->setPostProcessParams (opacity, resolutionScale);
}

void View::applyMouseConfig()
{
    const bool enabled { config.getValue (IDtype::mouse, jam::ID::enabled) };
    const auto imouseButton { jam::map::MouseButton::get (config.getValue (IDtype::mouse, ID::imouse).toString()) };
    const auto orbitButton { jam::map::MouseButton::get (config.getValue (IDtype::mouse, ID::orbit).toString()) };
    const auto resetButton { jam::map::MouseButton::get (config.getValue (IDtype::mouse, ID::reset).toString()) };

    mouseEnabled = enabled;
    orbitButtonConfig = orbitButton;
    resetButtonConfig = resetButton;

    background.setMouseConfig (enabled, imouseButton, orbitButton);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
