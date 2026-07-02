#pragma once
#include <JuceHeader.h>
#include "config/Config.h"
#include "end/Model.h"
#include "end/View.h"
#include "end/Window.h"
#include "lookAndFeel/LookAndFeel.h"
#include "Bimap.h"
#include "Nexus.h"

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
    // CONTEXT — jam::Bimap<T> owner. Declared before any consumer so the
    // single-global-pointer Instance<T> slot is populated before first use.
    Map context;

    Model model;
    config::Model config;
    Nexus nexus;

    //==============================================================================
    LookAndFeel lookAndFeel;

    /** @brief Vulkan registry — constructed unconditionally in initialiseVulkan(),
     *  after lookAndFeel exists, and never reset/reconstructed thereafter (see
     *  EventRegistration.cpp's ID::gpu handler). GPU availability/preference only
     *  selects, via jam::vulkan::Registry::getInstance()->setGpuEnabled(), which
     *  rendering engine createContext() dispatches to per paint (native Vulkan
     *  vs the CPU-fallback jam::LowLevelGraphicsGlyphRenderer) — never whether
     *  this Registry, its Device, or its shared glyph atlas exist. The atlas and
     *  every registered typeface therefore survive every GPU toggle. Declared
     *  after lookAndFeel so construction order lets registerTypeface() reach an
     *  already-constructed LookAndFeel, and destructs before lookAndFeel
     *  (reverse declaration order) while window (declared after, torn down
     *  first) never outlives it. */
    std::unique_ptr<jam::vulkan::Registry> vulkanEngine;

    std::unique_ptr<end::Window> window;

    //==============================================================================
    void initialise (const juce::String& commandLine) override;

    /** @brief Constructs vulkanEngine, registers END's embedded typefaces with
     *  its atlas, and enables the post-process background-blur shader — the
     *  whole GPU-availability-gated setup block, called once from initialise(). */
    void initialiseVulkan();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Application)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
