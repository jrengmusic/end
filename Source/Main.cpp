#include "Main.h"
#include "config/ConfigDirectory.h"

static void cascadeLookAndFeelChange()
{
    auto& desktop { juce::Desktop::getInstance() };

    for (int i { 0 }; i < desktop.getNumComponents(); ++i)
        desktop.getComponent (i)->sendLookAndFeelChange();
}

static std::tuple<float, float> getPostProcessRenderParams (ConfigModel& config)
{
    const float opacity { config.getRowValue (Id::toType (Id::graphics), Id::postProcessingOpacity) };
    const float resolutionScale { config.getRowValue (Id::toType (Id::graphics), Id::postProcessingResolution) };

    return { opacity, resolutionScale };
}

static map::ImageResample::value getImageResampleFilter (ConfigModel& config)
{
    static const jam::SharedInstance<map::ImageResample> imageResample { std::in_place };

    const auto filterName { config.getRowValue (Id::toType (Id::graphics), Id::filter).toString() };

    return static_cast<map::ImageResample::value> (map::ImageResample::getInstance()->get (filterName));
}

ENDApplication::ENDApplication() {}

ENDApplication::~ENDApplication() { config.removeListener (this); }

void ENDApplication::valueTreePropertyChanged (juce::ValueTree& tree,
                                               const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (not events.contains (key))
        key = tree.getParent().getType();

    if (events.contains (key))
        events.get (key, tree);
}

void ENDApplication::registerEvents()
{
    events.add<juce::ValueTree&> (Id::fontRasterizer,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (Id::fontGamma,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (Id::fontContrast,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (Id::embolden,
                                  [this] (juce::ValueTree&)
                                  {
                                      setEmbolden();
                                  });

    events.add<juce::ValueTree&> (Id::useGpu,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackground();
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (Id::background,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackground();
                                  });

    events.add<juce::ValueTree&> (Id::filter,
                                  [this] (juce::ValueTree&)
                                  {
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

void ENDApplication::setFontRasterization()
{
    const auto backendName { config.getRowValue (Id::toType (Id::graphics), Id::fontRasterizer).toString() };
    const auto backend { static_cast<map::FontRasterizerBackend::value> (map::FontRasterizerBackend::getInstance()->get (backendName)) };
    const float gamma { config.getRowValue (Id::toType (Id::graphics), Id::fontGamma) };
    const float contrast { config.getRowValue (Id::toType (Id::graphics), Id::fontContrast) };

    jam::CpuGraphicsEngine::getInstance()->setRasterization (backend, gamma, contrast);

    cascadeLookAndFeelChange();
}

void ENDApplication::setEmbolden()
{
    const bool embolden { config.getRowValue (Id::toType (Id::code), Id::embolden) };

    jam::CpuGraphicsEngine::getInstance()->setEmbolden (embolden);

    cascadeLookAndFeelChange();
}

void ENDApplication::setBackground()
{
    auto* engine { jam::VulkanGraphicsEngine::getInstance() };
    const bool gpuEnabled { engine->isGpuAvailable() };

    const auto projectName { config.getRowValue (Id::toType (Id::graphics), Id::background).toString() };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (config.state, Id::toType (Id::background)) };
        const auto filter { getImageResampleFilter (config) };

        const int shaderFormat { shaderState.getProperty (Id::shaderFormat) };

        auto compiled { jam::VulkanShaderCompiler::compile (
            shaderState, true, shaderFormat, filter) };

        if (compiled != nullptr)
            static_cast<ENDView*> (window->getContentComponent())->setBackground (std::move (compiled));
    }
    else
    {
        static_cast<ENDView*> (window->getContentComponent())->setBackground (nullptr);
    }
}

void ENDApplication::setPostProcess()
{
    auto* engine { jam::VulkanGraphicsEngine::getInstance() };
    const bool gpuEnabled { engine->isGpuAvailable() };

    const auto projectName { config.getRowValue (Id::toType (Id::graphics), Id::postProcessing).toString() };
    const auto [opacity, resolutionScale] { getPostProcessRenderParams (config) };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { jam::Model::getChildWithName (
            config.state, Id::toType (Id::postProcessing)) };
        const auto filter { getImageResampleFilter (config) };

        const int shaderFormat { shaderState.getProperty (Id::shaderFormat) };

        auto compiled { jam::VulkanShaderCompiler::compile (
            shaderState, false, shaderFormat, filter) };

        if (compiled != nullptr)
            engine->setPostProcess (std::move (compiled), opacity, resolutionScale);
    }
    else
    {
        engine->setPostProcess (nullptr, opacity, resolutionScale);
    }
}

void ENDApplication::setPostProcessParams()
{
    const auto [opacity, resolutionScale] { getPostProcessRenderParams (config) };

    auto* engine { jam::VulkanGraphicsEngine::getInstance() };
    engine->setPostProcessParams (opacity, resolutionScale);
}

// juce::JUCEApplicationBase::shutdown() is pure virtual — an override must
// exist even though its body is empty. Vulkan peer teardown (formerly done
// here manually) now happens automatically in jam::Window::~Window().
void ENDApplication::shutdown() {}

void ENDApplication::systemRequestedQuit() { quit(); }
const juce::String ENDApplication::getApplicationName() { return ProjectInfo::projectName; }
const juce::String ENDApplication::getApplicationVersion() { return ProjectInfo::versionString; }
bool ENDApplication::moreThanOneInstanceAllowed() { return true; }

//==============================================================================
void ENDApplication::initialise (const juce::String& commandLine)
{
#if JUCE_WINDOWS
    {
        HANDLE job { CreateJobObject (nullptr, nullptr) };

        if (job != nullptr)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
            info.BasicLimitInformation.LimitFlags =
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
            SetInformationJobObject (job, JobObjectExtendedLimitInformation, &info, sizeof (info));
            AssignProcessToJobObject (job, GetCurrentProcess());
        }
    }
#endif
    initialiseVulkan();
    nexus.initialiseServices();

    window = std::make_unique<jam::Window> (std::make_unique<ENDView>(),
                                            ProjectInfo::projectName,
                                            static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::alwaysOnTop)),
                                            static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::titleBarButtons)));
    window->addKeyListener (static_cast<ENDView*> (window->getContentComponent()));
    window->setVisible (true);

    registerEvents();
    config.addListener (this);
    setBackground();
    setPostProcess();
}

void ENDApplication::initialiseVulkan()
{
    // Vulkan pipeline cache — resolved under END's own config directory
    // (ConfigDirectory::Config::path, ~/.config/end/), never decided by JAM. Explicit
    // per VulkanGraphicsEngine's contract.
    const auto cacheDir { jam::File::getOrCreateDirectory (ConfigDirectory::Config::path, Id::cache) };
    const juce::File cacheFile { cacheDir.getChildFile (
        jam::Format::toFileName (ProjectInfo::projectName, Id::cache)) };
    const bool canUseGpu { static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::useGpu)) };

    const auto maxImageExtent { jam::VulkanGraphicsEngine::getPrimaryDisplayExtent() };

    jam::VulkanGraphicsEngine::getOrCreate (maxImageExtent, ProjectInfo::projectName, jam::VulkanGraphicsEngine::getFrameBudget(), cacheFile, canUseGpu);

    lookAndFeel.registerTypeface();
    setFontRasterization();
    setEmbolden();
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
