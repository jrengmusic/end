#include "Main.h"

namespace end
{
/*____________________________________________________________________________*/

Application::Application() {}

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

    // Config: build from binary defaults, overlay from disk
    config.loadPath (config::File::path);

    // GPU probe: resolve renderer, gate glass
    const auto probe { jam::GpuProbe::probe() };
    jam::BackgroundBlur::setEnabled (probe.isAvailable);

    // LookAndFeel: set as default, register as ValueTree listener
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    // end::Window reads all remaining WINDOW properties (colour, blur, backend,
    // always_on_top, buttons) from config inside its own setStyle() call.
    auto* view { new View() };
    window.reset (new end::Window { view, ProjectInfo::projectName, false, false });
    viewAttachment = std::make_unique<jam::ValueTree::Attachment> (model.getRootTree(), *view);
    window->setVisible (true);

    // File watcher: watch config directory for hot-reload
    watcher.addFolder (config::File::path);
    watcher.coalesceEvents (300);
    watcher.addListener (this);

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
void Application::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated
        and file.hasFileExtension (config::File::extension))
    {
        juce::String errorOut;
        config.load (file, errorOut);
        // load() overlays the changed file onto the live tree, firing
        // valueTreePropertyChanged on all mutated properties.
        // LookAndFeel and end::Window react as registered listeners.
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (end::Application)
