#pragma once
#include <JuceHeader.h>
#include "action/ENDActions.h"
#include "config/ConfigModel.h"
#include "end/ENDModel.h"
#include "end/ENDView.h"
#include "end/ENDWindow.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "Bimap.h"
#include "Nexus.h"

class ENDApplication : public juce::JUCEApplication
{
public:
    ENDApplication();
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;
    void shutdown() override;
    void systemRequestedQuit() override;

private:
    //==============================================================================
#if JUCE_DEBUG
    /** @brief Diagnostic log sink — canonical location: FileConfig::path
     *  (\~/.config/end/end.ode, jam::Format::toFileName), never the launch
     *  cwd — the same deterministic path regardless of how the app was
     *  started (IDE, Finder, terminal), so runtime diagnostics always land
     *  in one known, readable file. */

    jam::debug::Log::Scope logScope {
        juce::File ("~/Documents/Poems/dev/end")
            .getChildFile (jam::Format::toFileName (ProjectInfo::projectName, ".ode"))
    };
#endif

    //==============================================================================
    // CONTEXT — jam::Bimap<T> owner. Declared before any consumer so the
    // single-global-pointer Instance<T> slot is populated before first use.
    Map context;

    // Nexus MUST construct before ConfigModel: ConfigModel::appModel is an
    // ENDModel& bound via *ENDModel::getInstance() in its own member
    // initializer, evaluated at ConfigModel construction time — ENDModel
    // (owned by Nexus) must already exist or that dereference is undefined
    // behaviour.
    Nexus nexus;
    ConfigModel config;
    ENDActions actions;

    //==============================================================================
    ENDLookAndFeel lookAndFeel;

    /** @brief Unified Vulkan resource-ownership tree — constructed unconditionally
     *  in initialiseVulkan(), after lookAndFeel exists, and never reset/
     *  reconstructed thereafter (see EventRegistration.cpp's ID::gpu handler).
     *  Owns the shared Device, every SharedResources<T> interning table
     *  (Typeface, Stamp, Grapheme, Link — each self-registers as its own
     *  getInstance() singleton on construction), the shared glyph atlas, and
     *  per-window Graphics instances — member-declaration order inside
     *  jam::VulkanEngine governs teardown (reverse-order destruction). GPU
     *  availability/preference only selects, via
     *  jam::VulkanEngine::getInstance()->setGpuEnabled(), which rendering
     *  engine createContext() dispatches to per paint (native Vulkan vs the
     *  CPU-fallback jam::LowLevelGraphicsGlyphRenderer) — never whether this
     *  VulkanEngine, its Device, or its shared glyph atlas exist. The atlas and
     *  every registered typeface therefore survive every GPU toggle. Declared
     *  after lookAndFeel so construction order lets registerTypeface() reach an
     *  already-constructed LookAndFeel, and destructs before lookAndFeel
     *  (reverse declaration order) while window (declared after, torn down
     *  first) never outlives it. */
    std::unique_ptr<jam::VulkanEngine> vulkanEngine;

    std::unique_ptr<ENDWindow> window;

    //==============================================================================
    void initialise (const juce::String& commandLine) override;

    /** @brief Constructs vulkanEngine, registers END's embedded typefaces with
     *  its atlas, and enables the post-process background-blur shader — the
     *  whole GPU-availability-gated setup block, called once from initialise(). */
    void initialiseVulkan();

    /** @brief Detects the primary display's native vertical refresh rate, in Hz.
     *
     *  Feeds initialiseVulkan()'s targetFrameBudgetMs selection — queried once,
     *  never polled.
     *
     *  Windows/Linux: juce::Displays::Display::verticalFrequencyHz is populated
     *  by JUCE's own findDisplays(), read via
     *  juce::Desktop::getInstance().getDisplays().getPrimaryDisplay().
     *
     *  macOS: verticalFrequencyHz is never populated (confirmed by direct read
     *  of juce_Windowing_mac.mm's findDisplays() — no assignment to that field
     *  exists on this platform), so the rate is queried directly via
     *  CoreVideo's CVDisplayLinkGetNominalOutputVideoRefreshPeriod against the
     *  main display.
     *
     *  @return The detected refresh rate, or indeterminateRefreshRateHz
     *          (Main.cpp) if no rate could be determined — a deterministic
     *          fallback, never an unhandled/unspecified case. */
    static double queryPrimaryDisplayRefreshRateHz() noexcept;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDApplication)
};
