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
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    auto* view { new View (model) };
    window.reset (new end::Window { view, ProjectInfo::projectName });
    window->setVisible (true);
}


/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (end::Application)
