#include "Main.h"
#include "config/ConfigDirectory.h"

ENDApplication::ENDApplication() {}

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

    window = std::make_unique<jam::Window> (std::make_unique<ENDView> (*ENDModel::getInstance()),
                                            ProjectInfo::projectName,
                                            static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::alwaysOnTop)),
                                            static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::titleBarButtons)));
    window->addKeyListener (static_cast<ENDView*> (window->getContentComponent()));
    window->setVisible (true);
}

void ENDApplication::initialiseVulkan()
{
    // Vulkan pipeline cache — resolved under END's own config directory
    // (ConfigDirectory::Config::path, ~/.config/end/), never decided by JAM. Explicit
    // per VulkanEngine's contract.
    const auto cacheDir { jam::File::getOrCreateDirectory (ConfigDirectory::Config::path, Id::cache) };
    const juce::File cacheFile { cacheDir.getChildFile (
        jam::Format::toFileName (ProjectInfo::projectName, Id::cache)) };
    const bool canUseGpu { static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::useGpu)) };

    const auto maxImageExtent { jam::VulkanEngine::getPrimaryDisplayExtent() };

    jam::VulkanEngine::getOrCreate (maxImageExtent, ProjectInfo::projectName, jam::VulkanEngine::getFrameBudget(), cacheFile, canUseGpu);

    // LookAndFeel owns font knowledge but not the atlas — the atlas (owned by
    // the VulkanEngine created above) does not exist at LookAndFeel
    // construction time, so registration happens here instead, the earliest
    // point both exist together.
    lookAndFeel.registerTypeface (*jam::GlyphAtlas::getInstance());
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
