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

void ENDApplication::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child)
{
    if (parentTree == model.state and child.hasType (jam::Format::toType (Id::end)))
    {
        getOrCreateWindow().toFront (true);
#if JUCE_MAC
        juce::Process::setDockIconVisible (true);
        juce::Process::makeForegroundProcess();
#endif
    }
}

void ENDApplication::registerEvents()
{
    events.add (jam::Format::toType (Id::alwaysOnTop),
                [this] (const juce::Identifier&, juce::ValueTree& row)
                {
                    window->setAlwaysOnTop (row.getProperty (Id::value));
                });

    events.add (jam::Format::toType (Id::titleBarButtons),
                [this] (const juce::Identifier&, juce::ValueTree& row)
                {
                    window->setWindowButtons (row.getProperty (Id::value));
                });

    events.add (jam::Format::toType (Id::theme),
                [] (const juce::Identifier&, juce::ValueTree&)
                {
                    cascadeLookAndFeelChange();
                });

    events.add (jam::Format::toType (Id::useGpu),
                [this] (const juce::Identifier&, juce::ValueTree&)
                {
                    setBackground();
                    setPostProcess();
                });

    events.add (jam::Format::toType (Id::background),
                [this] (const juce::Identifier&, juce::ValueTree& row)
                {
                    if (row.getParent().getType() == jam::Format::toType (Id::graphics))
                        setBackground();
                });

    events.add (jam::Format::toType (Id::filter),
                [this] (const juce::Identifier&, juce::ValueTree&)
                {
                    setBackground();
                    setPostProcess();
                });

    events.add (jam::Format::toType (Id::postProcessing),
                [this] (const juce::Identifier&, juce::ValueTree&)
                {
                    setPostProcess();
                });

    events.add (jam::Format::toType (Id::postProcessingOpacity),
                [this] (const juce::Identifier&, juce::ValueTree&)
                {
                    setPostProcessParams();
                });

    events.add (jam::Format::toType (Id::postProcessingResolution),
                [this] (const juce::Identifier&, juce::ValueTree&)
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
#if JAM_USING_PROFILER
    {
        const jam::Profiler::Zone zone { Id::applicationShutdown };
#endif
        if (window != nullptr)
            destroyWindow();
        model.state.removeListener (this);

        if (nexus != nullptr)
        {
            juce::MemoryBlock savedSessions;

            {
                juce::MemoryOutputStream stream { savedSessions, false };
                nexus->getSessionsState().writeToStream (stream);
            }

            settings.setValue (Id::sessions.toString(), savedSessions.toBase64Encoding());
            settings.saveIfNeeded();
        }
#if JAM_USING_PROFILER
    }
#endif

#if JAM_USING_PROFILER
    profiler.drain();
    profiler.writeSamples (Nexus::getManifestFile(), jam::Format::toType (juce::Identifier { ProjectInfo::projectName }));
#endif
}

void ENDApplication::systemRequestedQuit()
{
    if (isServer() and getCommandLineParameters().contains (serverArgument) and window != nullptr)
    {
        destroyWindow();
#if JUCE_MAC
        juce::Process::setDockIconVisible (false);
#endif
    }
    else
    {
        quit();
    }
}
const juce::String ENDApplication::getApplicationName() { return ProjectInfo::projectName; }
const juce::String ENDApplication::getApplicationVersion() { return ProjectInfo::versionString; }
bool ENDApplication::moreThanOneInstanceAllowed() { return true; }

bool ENDApplication::isServer() const noexcept
{
    return static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::server));
}

//==============================================================================
void ENDApplication::initialise (const juce::String& commandLine)
{
#if JAM_USING_PROFILER
    applicationMembers.reset();
    const jam::Profiler::Zone zone { Id::applicationInitialise };
#endif

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

    if (isServer() and not commandLine.contains (serverArgument))
        launch();
    else
        host();
}

void ENDApplication::host()
{
    const bool isHosting { not isServer() or serverLock.enter (0) };

    if (isHosting)
    {
        {
#if JAM_USING_PROFILER
            const jam::Profiler::Zone zone { Id::graphicsEngine };
#endif
            initialiseVulkan();
        }

        nexus = std::make_unique<Nexus>();
        model.state.addListener (this);

        juce::MemoryBlock savedSessions;

        if (settings.containsKey (Id::sessions.toString()) and savedSessions.fromBase64Encoding (settings.getValue (Id::sessions.toString())))
            nexus->setSessionsState (juce::ValueTree::readFromData (savedSessions.getData(), savedSessions.getSize()));

        registerEvents();

        if (isServer())
        {
            settings.setValue (Id::port.toString(), nexus->getServerPort());
            settings.saveIfNeeded();
        }

        getOrCreateWindow();
    }
    else
    {
        quit();
    }
}

void ENDApplication::launch()
{
    if (serverLock.enter (0))
    {
        serverLock.exit();

        jam::ChildProcess::startDetached ({ juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName().toStdString(), serverArgument });
        quit();
    }
    else
    {
        launchConnection = std::make_unique<jam::ValueTreeConnection> (model.state);
        launchConnection->connectToSocket (jam::ValueTreeConnection::loopbackAddress, settings.getIntValue (Id::port.toString()), jam::ValueTreeConnection::connectTimeoutMs);

        juce::MessageManager::callAsync ([] { quit(); });
    }
}

jam::Window& ENDApplication::getOrCreateWindow()
{
    if (window == nullptr)
    {
        {
#if JAM_USING_PROFILER
            const jam::Profiler::Zone zone { Id::applicationWindow };
#endif
            window = std::make_unique<jam::Window> (std::make_unique<ENDView>(),
                                                    ProjectInfo::projectName,
                                                    static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::alwaysOnTop)),
                                                    static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::titleBarButtons)));
            window->addKeyListener (static_cast<ENDView*> (window->getContentComponent()));

            window->restoreWindowStateFromString (settings.getValue (Id::window.toString()));

            window->setVisible (true);

#if JUCE_WINDOWS
            if (auto* peer { window->getPeer() })
                jam::StyleWindow::setForeground (*peer);
#endif
        }

        {
#if JAM_USING_PROFILER
            const jam::Profiler::Zone zone { Id::applicationBackground };
#endif
            config.addListener (this);
            setBackground();
            setPostProcess();
        }
    }

    return *window;
}

void ENDApplication::destroyWindow()
{
    config.removeListener (this);
    settings.setValue (Id::window.toString(), window->getWindowStateAsString());
    window.reset();
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
