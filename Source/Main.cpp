#include "Main.h"

namespace end
{
/*____________________________________________________________________________*/

Application::Application() {}
void Application::shutdown() {}
void Application::systemRequestedQuit() { quit(); }
const juce::String Application::getApplicationName() { return ProjectInfo::projectName; }
const juce::String Application::getApplicationVersion() { return ProjectInfo::versionString; }
bool Application::moreThanOneInstanceAllowed() { return true; }

//==============================================================================
void Application::initialise (const juce::String& commandLine)
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
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    auto* view { new View (model) };
    window.reset (new end::Window { view, ProjectInfo::projectName });
    window->setVisible (true);
}

void Application::initialiseVulkan()
{
    // Provisional 60Hz-safe budget (11.1 ms) until per-monitor refresh-rate
    // detection supplies the real value — the literal lives at this END call
    // site deliberately; Registry's contract forbids hidden defaults inside JAM.
    constexpr double provisionalTargetFrameBudgetMs { 11.1 };

    // Vulkan pipeline cache — resolved under END's own config directory
    // (file::Config::path, ~/.config/end/), never decided by JAM. Explicit
    // per Registry's contract, mirroring provisionalTargetFrameBudgetMs above.
    const auto cacheDir { jam::File::getOrCreateDirectory (file::Config::path, IDref::cache) };
    const juce::File cacheFile { cacheDir.getChildFile (
        jam::Format::toFileName (ProjectInfo::projectName, IDref::cache)) };
    const bool canUseGpu { config.getValue (IDtype::display, ID::gpu)
                           and jam::GpuProbe::probe().isAvailable };

    jam::BackgroundBlur::setEnabled (canUseGpu);

    vulkanEngine = std::make_unique<jam::vulkan::Registry> (
        provisionalTargetFrameBudgetMs, cacheFile, canUseGpu);

    // LookAndFeel owns font knowledge but not the atlas — the atlas (owned by
    // the Registry just constructed above) does not exist at LookAndFeel
    // construction time, so registration happens here instead, the earliest
    // point both exist together.
    lookAndFeel.registerTypeface (vulkanEngine->getAtlas());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (end::Application)
