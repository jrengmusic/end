#include "end/ENDView.h"

void ENDView::registerGraphicsEvents()
{
    events.add<juce::ValueTree&> (
        Id::useGpu,
        [this] (juce::ValueTree&)
        {
            // jam::VulkanEngine is constructed unconditionally, once, by
            // ENDApplication, and never reset/reconstructed here — GPU
            // availability/preference only selects which rendering engine
            // createContext() dispatches to per paint (see ENDApplication's
            // vulkanEngine doc comment, Main.h).

            // Background/post-process shaders never exist independent of the
            // effective GPU state (Locked Decision 4) — both funnels re-derive
            // it themselves and discard/clear on every toggle away from GPU,
            // recompile from current config on every toggle onto it. This is
            // also the SAME call this View makes at initial config load — see
            // this constructor's callAsync block, which fires this very
            // callback once at startup.
            setBackground();
            setPostProcess();
        });

    events.add<juce::ValueTree&> (Id::background,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackground();
                                  });

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

    events.add<juce::ValueTree&> (Id::filter,
                                  [this] (juce::ValueTree&)
                                  {
                                      // filter is shared by both slots (display.md: applies to both the
                                      // background and post-processing upscale) — baked into the
                                      // compiled prelude (jam::VulkanShaderCompiler::channelMacros()/sceneMacro()),
                                      // so a filter change requires a full recompile on both funnels,
                                      // never the cheaper *Params() path.
                                      setBackground();
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (Id::postProcessing,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (Id::postProcessingOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcessParams();
                                  });

    events.add<juce::ValueTree&> (Id::postProcessingResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcessParams();
                                  });
}

void ENDView::registerWindowEvents()
{
    events.add<juce::ValueTree&> (
        Id::alwaysOnTop,
        [this] (juce::ValueTree& tree)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setAlwaysOnTop (tree.getProperty (Id::alwaysOnTop));
        });

    events.add<juce::ValueTree&> (
        Id::titleBarButtons,
        [this] (juce::ValueTree& tree)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setWindowButtons (tree.getProperty (Id::titleBarButtons));
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
    const float opacity { config.getValue (Id::toType (Id::graphics), Id::backgroundOpacity) };
    const float resolutionScale { config.getValue (Id::toType (Id::graphics), Id::backgroundResolution) };
    const int frameRate { config.getValue (Id::toType (Id::graphics), Id::frameRate) };

    return { opacity, resolutionScale, frameRate };
}

static std::tuple<float, float> getPostProcessRenderParams (ConfigModel& config)
{
    const float opacity { config.getValue (Id::toType (Id::graphics), Id::postProcessingOpacity) };
    const float resolutionScale { config.getValue (Id::toType (Id::graphics), Id::postProcessingResolution) };

    return { opacity, resolutionScale };
}

static map::ImageResample::value getImageResampleFilter (ConfigModel& config)
{
    const auto filterName { config.getValue (Id::toType (Id::graphics), Id::filter).toString() };

    return static_cast<map::ImageResample::value> (map::ImageResample::getInstance()->get (filterName));
}

void ENDView::setBackground()
{
    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);
    const bool gpuEnabled { engine->isGpuAvailable() };

    const auto projectName { config.getValue (Id::toType (Id::graphics), Id::background).toString() };
    const auto [opacity, resolutionScale, frameRate] { getBackgroundRenderParams (config) };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (config.state, Id::toType (Id::background)) };
        const auto filter { getImageResampleFilter (config) };

        // ConfigShader::loadFromPath() always stamps Id::shaderFormat with a
        // definite format ordinal (jam::VulkanShaderFormat::shadertoy or
        // ::slang) before this state is ever readable here — ConfigModel's
        // constructor runs loadFromPath() to completion, and
        // jam::Instance<ConfigModel>::getInstance() (which View::config
        // resolves through) cannot return before that constructor finishes.
        const int shaderFormat { shaderState.getProperty (Id::shaderFormat) };

        auto compiled { jam::VulkanShaderCompiler::compile (
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
    const auto [opacity, resolutionScale, frameRate] { getBackgroundRenderParams (config) };

    background.setParams (opacity, resolutionScale, frameRate);
}

void ENDView::setPostProcess()
{
    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);
    const bool gpuEnabled { engine->isGpuAvailable() };

    const auto projectName { config.getValue (Id::toType (Id::graphics), Id::postProcessing).toString() };
    const auto [opacity, resolutionScale] { getPostProcessRenderParams (config) };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (
            config.state, Id::toType (Id::postProcessing)) };
        const auto filter { getImageResampleFilter (config) };

        // See setBackground()'s matching comment — Id::shaderFormat is
        // always a definite format ordinal by the time this state is
        // readable here.
        const int shaderFormat { shaderState.getProperty (Id::shaderFormat) };

        auto compiled { jam::VulkanShaderCompiler::compile (
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
    const auto [opacity, resolutionScale] { getPostProcessRenderParams (config) };

    auto* engine { jam::VulkanEngine::getInstance() };
    jassert (engine != nullptr);
    engine->setPostProcessParams (opacity, resolutionScale);
}

void ENDView::setMouseConfig()
{
    const bool enabled { config.getValue (Id::toType (Id::mouse), Id::enabled) };
    const auto imouseButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getValue (Id::toType (Id::mouse), Id::imouse).toString())) };
    const auto orbitButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getValue (Id::toType (Id::mouse), Id::orbit).toString())) };
    const auto resetButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getValue (Id::toType (Id::mouse), Id::reset).toString())) };

    background.setMouseConfig (enabled, imouseButton, orbitButton, resetButton);
}
