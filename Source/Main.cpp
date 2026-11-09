#include "Main.h"
#include "config/ConfigDirectory.h"

static void cascadeLookAndFeelChange()
{
    auto& desktop { juce::Desktop::getInstance() };

    for (int i { 0 }; i < desktop.getNumComponents(); ++i)
        desktop.getComponent (i)->sendLookAndFeelChange();
}

static map::ImageResample::value getImageResampleFilter (ConfigModel& config)
{
    static const jam::SharedInstance<map::ImageResample> imageResample { std::in_place };

    const auto filterName { config.getRowValue (jam::Format::toType (Id::graphics), Id::filter).toString() };

    return static_cast<map::ImageResample::value> (map::ImageResample::getInstance()->get (filterName));
}

/** @cond */
#if JUCE_WINDOWS
extern "C" __declspec (dllexport) const void* enax_get_extension (const char* id);
#else
extern "C" __attribute__ ((visibility ("default"))) const void* enax_get_extension (const char* id);
#endif
/** @endcond */

const void* enax_get_extension (const char* id)
{
    return Nexus::getInstance()->getExtension (id);
}

ENDApplication::ENDApplication() {}

ENDApplication::~ENDApplication() { config.removeListener (this); }

void ENDApplication::valueTreePropertyChanged (juce::ValueTree& tree,
                                               const juce::Identifier& property)
{
    const auto key { ConfigModel::getEventKey (events, tree, property) };

    if (events.contains (key))
        events.get (key, tree);
}

void ENDApplication::registerEvents()
{
    events.add<juce::ValueTree&> (jam::Format::toType (Id::alwaysOnTop),
                                  [this] (juce::ValueTree& row)
                                  {
                                      window->setAlwaysOnTop (row.getProperty (Id::value));
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::titleBarButtons),
                                  [this] (juce::ValueTree& row)
                                  {
                                      window->setWindowButtons (row.getProperty (Id::value));
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::theme),
                                  [] (juce::ValueTree&)
                                  {
                                      cascadeLookAndFeelChange();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::useGpu),
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackground();
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::background),
                                  [this] (juce::ValueTree& row)
                                  {
                                      if (row.getParent().getType() == jam::Format::toType (Id::graphics))
                                          setBackground();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::filter),
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackground();
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::postProcessing),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcess();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::postProcessingOpacity),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcessParams();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::postProcessingResolution),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPostProcessParams();
                                  });
}

void ENDApplication::setBackground()
{
    auto* engine { jam::VulkanGraphicsEngine::getInstance() };
    const bool gpuEnabled { engine->isGpuAvailable() };

    const auto projectName { config.getRowValue (jam::Format::toType (Id::graphics), Id::background).toString() };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { config.background.state };
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

    const auto projectName { config.getRowValue (jam::Format::toType (Id::graphics), Id::postProcessing).toString() };
    const float opacity { config.getRowValue (jam::Format::toType (Id::graphics), Id::postProcessingOpacity) };
    const float resolutionScale { config.getRowValue (jam::Format::toType (Id::graphics), Id::postProcessingResolution) };

    if (gpuEnabled and projectName.isNotEmpty())
    {
        const auto shaderState { config.postProcessing.state };
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
    auto* engine { jam::VulkanGraphicsEngine::getInstance() };
    engine->setPostProcessParams (static_cast<float> (config.getRowValue (jam::Format::toType (Id::graphics), Id::postProcessingOpacity)),
                                  static_cast<float> (config.getRowValue (jam::Format::toType (Id::graphics), Id::postProcessingResolution)));
}

void ENDApplication::shutdown()
{
    settings.setValue (Id::window.toString(), window->getWindowStateAsString());

#if JAM_USING_PROFILER
    profiler.drain();
    profiler.writeSamples (Nexus::getManifestFile(), jam::Format::toType (juce::Identifier { ProjectInfo::projectName }));
#endif
}

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

    window = std::make_unique<jam::Window> (std::make_unique<ENDView>(),
                                            ProjectInfo::projectName,
                                            static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::alwaysOnTop)),
                                            static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::titleBarButtons)));
    window->addKeyListener (static_cast<ENDView*> (window->getContentComponent()));

    window->restoreWindowStateFromString (settings.getValue (Id::window.toString()));

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
    const bool canUseGpu { static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::useGpu)) };

    const auto maxImageExtent { jam::VulkanGraphicsEngine::getPrimaryDisplayExtent() };

    jam::VulkanGraphicsEngine::getOrCreate (maxImageExtent, ProjectInfo::projectName, jam::VulkanGraphicsEngine::getFrameBudget(), cacheFile, canUseGpu);

    lookAndFeel.initialiseStyle();
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
