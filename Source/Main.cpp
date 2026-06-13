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
    // GPU probe: resolve renderer, gate glass
    const auto probe { jam::GpuProbe::probe() };
    jam::BackgroundBlur::setEnabled (probe.isAvailable);

    // LookAndFeel: set as default, register as ValueTree listener
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    // end::Window reads all remaining WINDOW properties (colour, blur, backend,
    // always_on_top, buttons) from config inside its own setStyle() call.
    auto* view { new View (model) };
    window.reset (new end::Window { view, ProjectInfo::projectName, false, false });
    window->setVisible (true);

    juce::MessageManager::callAsync (
        [this]
        {
            if (auto* v { dynamic_cast<View*> (window->getContentComponent()) })
                v->grabKeyboardFocus();
        });
}


/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (end::Application)
