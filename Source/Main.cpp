#include "Main.h"

namespace end
{
/*____________________________________________________________________________*/

Application::Application() {}
void Application::initialise (const juce::String& commandLine)
{
#if JUCE_WINDOWS
    // Safety net: create a Job Object with KILL_ON_JOB_CLOSE so that all
    // child processes (shell, OpenConsole.exe from ConPTY) are killed when
    // this process exits — even on crash.  The daemon has its own Job Object
    // via Daemon::installPlatformProcessCleanup(); this covers the GUI
    // (standalone and client) process.  Handle intentionally not stored —
    // the OS closes it on process exit, which triggers the kill.
    {
        HANDLE job { CreateJobObject (nullptr, nullptr) };

        if (job != nullptr)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
            SetInformationJobObject (job, JobObjectExtendedLimitInformation, &info, sizeof (info));
            AssignProcessToJobObject (job, GetCurrentProcess());
        }
    }
#endif

    auto* view { new View() };
    window.reset (new jam::Window { view, ProjectInfo::projectName, false, false });
    window->setVisible (true);

    juce::MessageManager::callAsync (
        [this]
        {
            if (auto* view { window->getContentComponent() })
                view->grabKeyboardFocus();
        });
}
void Application::shutdown() {}
void Application::systemRequestedQuit() { quit(); }
const juce::String Application::getApplicationName() { return ProjectInfo::projectName; }
const juce::String Application::getApplicationVersion() { return ProjectInfo::versionString; }
bool Application::moreThanOneInstanceAllowed() { return true; }

//==============================================================================
void Application::fileChanged (const juce::File& file, jam::File::Watcher::Event event) {}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (end::Application)
