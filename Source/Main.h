#pragma once
#include <JuceHeader.h>
#include <JamFontsBinaryData.h>
#include "config/Config.h"
#include "end/Model.h"
#include "end/View.h"
#include "end/Window.h"
#include "lookAndFeel/LookAndFeel.h"
#include "end/Map.h"

namespace end
{
/*____________________________________________________________________________*/

class Application : public juce::JUCEApplication
{
public:
    Application();
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;
    void shutdown() override;
    void systemRequestedQuit() override;

private:
    //==============================================================================
#if JUCE_DEBUG
    jam::debug::Log::Scope logScope { juce::File::getCurrentWorkingDirectory().getChildFile (
        jam::Format::toFileName (ProjectInfo::projectName, ".ode")) };
#endif

    //==============================================================================
    // CONTEXT — all jam::Map::Instance<T> owners live here, lifetime bound
    // to the JUCEApplication instance. Declared before any consumer so the
    // single-global-pointer Context<T> slot is populated before first use.
    Map context;

    /** @brief Application-owned typeface registry and shared glyph atlas. */
    jam::TypefaceResources typefaceResources;

    /** @brief Application-owned style table — self-registers as jam::Stamp::getInstance() on construction. */
    jam::Stamp stampInstance;

    /** @brief Application-owned grapheme cluster table — self-registers as jam::Grapheme::getInstance() on construction. */
    jam::Grapheme graphemeInstance;

    config::Model config;

    //==============================================================================
    Model model;
    LookAndFeel lookAndFeel;
    std::unique_ptr<end::Window> window;

    //==============================================================================
    void initialise (const juce::String& commandLine) override;
    void registerTypefaces();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Application)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
