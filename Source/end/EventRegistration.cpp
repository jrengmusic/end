#include "ENDView.h"

void ENDView::registerEvents()
{
    events.add<juce::ValueTree&> (
        jam::ID::focusedPane,
        [this] (juce::ValueTree& tree)
        {
            if (tree.getType() == IDtype::tab)
                focusedPane.referTo (tree.getPropertyAsValue (jam::ID::focusedPane, nullptr));
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
            // ENDApplication, and never reset/reconstructed here — GPU
            // availability/preference only selects which rendering engine
            // createContext() dispatches to per paint (see ENDApplication's
            // vulkanEngine doc comment, Main.h).
            const bool canUseGpu { config.getValue (IDtype::display, ID::gpu)
                                   and jam::GpuProbe::probe().isAvailable };

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
            setBackground();
            setPostProcess();
        });

    events.add<juce::ValueTree&> (ID::background,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackground();
                                  });

    events.add<juce::ValueTree&> (ID::backgroundOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (ID::frameRate,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (ID::backgroundResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (ID::filter,
                                  [this] (juce::ValueTree&)
                                  {
                                      // filter is shared by both slots (display.lua: applies to both the
                                      // background and post-processing upscale) — baked into the
                                      // compiled prelude (jam::vulkan::ShaderCompiler::channelMacros()/sceneMacro()),
                                      // so a filter change requires a full recompile on both funnels,
                                      // never the cheaper *Params() path.
                                      setBackground();
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (ID::postProcessing,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (ID::postProcessingOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcessParams();
                                  });

    events.add<juce::ValueTree&> (ID::postProcessingResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcessParams();
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
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (ID::imouse,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (ID::orbit,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (ID::reset,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    // WINDOW leaf visibility toggles (ENDActions.cpp's own
    // Position-bimap loop) relayout the dormant dock-pane machinery — one
    // registration on the shared jam::ID::visible property key serves all
    // five leaves, no per-leaf type fallback needed (every WINDOW leaf now
    // shares IDtype::pane, so a type-keyed fallback could no longer
    // distinguish them the way the four old Position edge node TYPES did).
    events.add<juce::ValueTree&> (jam::ID::visible,
                                  [this] (juce::ValueTree&)
                                  {
                                      resized();
                                  });
}

void ENDView::setBackground()
{
    // Same effective-gpu truth ENDApplication resolves for the VulkanEngine
    // ctor and the gpu event handler resolves for setGpuEnabled() — never
    // raw config alone.
    const bool gpuEnabled { config.getValue (IDtype::display, ID::gpu)
                            and jam::GpuProbe::probe().isAvailable };

    const auto projectName { config.getValue (IDtype::graphics, ID::background).toString() };
    const float opacity { config.getValue (IDtype::graphics, ID::backgroundOpacity) };
    const float resolutionScale { config.getValue (IDtype::graphics, ID::backgroundResolution) };
    const int frameRate { config.getValue (IDtype::graphics, ID::frameRate) };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (config.state, IDtype::background) };
        const auto filterName { config.getValue (IDtype::graphics, ID::filter).toString() };
        const auto filter { jam::map::ImageResample::get (filterName) };

        // ConfigShader::loadFromPath() always stamps ID::shaderFormat with a
        // definite format ordinal (jam::vulkan::ShaderFormat::shadertoy or
        // ::slang) before this state is ever readable here — ConfigModel's
        // constructor runs loadFromPath() to completion, and
        // jam::Instance<ConfigModel>::getInstance() (which View::config
        // resolves through) cannot return before that constructor finishes.
        const int shaderFormat { shaderState.getProperty (ID::shaderFormat) };

        auto compiled { jam::vulkan::ShaderCompiler::compile (
            shaderState, true, shaderFormat, filter) };

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

void ENDView::setBackgroundParams()
{
    const float opacity { config.getValue (IDtype::graphics, ID::backgroundOpacity) };
    const float resolutionScale { config.getValue (IDtype::graphics, ID::backgroundResolution) };
    const int frameRate { config.getValue (IDtype::graphics, ID::frameRate) };

    background.setParams (opacity, resolutionScale, frameRate);
}

void ENDView::setPostProcess()
{
    const bool gpuEnabled { config.getValue (IDtype::display, ID::gpu)
                            and jam::GpuProbe::probe().isAvailable };

    const auto projectName { config.getValue (IDtype::graphics, ID::postProcessing).toString() };
    const float opacity { config.getValue (IDtype::graphics, ID::postProcessingOpacity) };
    const float resolutionScale { config.getValue (
        IDtype::graphics, ID::postProcessingResolution) };

    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (
            config.state, IDtype::postProcessing) };
        const auto filterName { config.getValue (IDtype::graphics, ID::filter).toString() };
        const auto filter { jam::map::ImageResample::get (filterName) };

        // See setBackground()'s matching comment — ID::shaderFormat is
        // always a definite format ordinal by the time this state is
        // readable here.
        const int shaderFormat { shaderState.getProperty (ID::shaderFormat) };

        auto compiled { jam::vulkan::ShaderCompiler::compile (
            shaderState, false, shaderFormat, filter) };

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

void ENDView::setPostProcessParams()
{
    const float opacity { config.getValue (IDtype::graphics, ID::postProcessingOpacity) };
    const float resolutionScale { config.getValue (
        IDtype::graphics, ID::postProcessingResolution) };

    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);
    engine->setPostProcessParams (opacity, resolutionScale);
}

void ENDView::setMouseConfig()
{
    const bool enabled { config.getValue (IDtype::mouse, jam::ID::enabled) };
    const auto imouseButton { jam::map::MouseButton::get (
        config.getValue (IDtype::mouse, ID::imouse).toString()) };
    const auto orbitButton { jam::map::MouseButton::get (
        config.getValue (IDtype::mouse, ID::orbit).toString()) };
    const auto resetButton { jam::map::MouseButton::get (
        config.getValue (IDtype::mouse, ID::reset).toString()) };

    background.setMouseConfig (enabled, imouseButton, orbitButton, resetButton);
}
