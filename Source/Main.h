#pragma once
#include <JuceHeader.h>
#include "config/Config.h"
#include "lookAndFeel/LookAndFeel.h"
#include "endView.h"
#include "endWindow.h"

namespace end
{
/*____________________________________________________________________________*/

class Application
    : public juce::JUCEApplication
    , public jam::File::Watcher::Listener
{
public:
    Application();
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;
    void initialise (const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;

private:
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    //==============================================================================
    // CONTEXT
    config::Model config;

    //==============================================================================
    LookAndFeel lookAndFeel;
    jam::File::Watcher watcher;
    std::unique_ptr<end::Window> window;

    //==============================================================================
#if JUCE_DEBUG
    jam::debug::Log::Scope logScope { juce::File::getCurrentWorkingDirectory().getChildFile (
        jam::Text::toFileName (ProjectInfo::projectName, ".ode")) };
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Application)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
