#include "Main.h"

#if JUCE_MAC
#include <CoreGraphics/CGDirectDisplay.h>
#include <CoreVideo/CoreVideo.h>
#endif

namespace end
{
/*____________________________________________________________________________*/

Application::Application() {}

void Application::shutdown()
{
    // The one real window-close seam this single-window app has: shutdown()
    // runs strictly before window's own automatic (member-declaration-order)
    // destruction deletes the peer, so peer->getNativeHandle() (read inside
    // removePeer()) is still valid here. Releases this window's Graphics —
    // and, through its destructor, this window's slot in every shared
    // glyph-atlas bindless slot book (jam::vulkan::VulkanEngine::removePeer()'s
    // doc comment) — before the peer itself goes away.
    if (auto* peer { window->getPeer() })
        vulkanEngine->removePeer (peer);
}
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

// SPEC.md:938 — 120fps GPU target, frame time < 5.8ms (70% of 8.33ms budget).
static constexpr double highRefreshFrameBudgetMs { 5.8 };

// SPEC.md:939 — 60fps CPU-safe fallback, frame time < 11.1ms (67% of 16.6ms
// budget). Also the budget an indeterminate refresh-rate reading resolves to
// (see indeterminateRefreshRateHz below), so "rate unknown" and "rate is 60Hz"
// are deliberately the same, deterministic outcome.
static constexpr double standardRefreshFrameBudgetMs { 11.1 };

// Refresh rate at/above which highRefreshFrameBudgetMs applies instead of
// standardRefreshFrameBudgetMs.
static constexpr double highRefreshRateThresholdHz { 120.0 };

// Deterministic stand-in for "refresh rate could not be determined" — chosen
// below highRefreshRateThresholdHz so the indeterminate case always resolves
// to standardRefreshFrameBudgetMs, never an unhandled branch.
static constexpr double indeterminateRefreshRateHz { 60.0 };

#if JUCE_MAC
double Application::queryPrimaryDisplayRefreshRateHz() noexcept
{
    CVDisplayLinkRef displayLink { nullptr };
    const auto createResult { CVDisplayLinkCreateWithCGDisplay (CGMainDisplayID(), &displayLink) };

    auto refreshRateHz { indeterminateRefreshRateHz };

    if (createResult == kCVReturnSuccess and displayLink != nullptr)
    {
        const auto nominalPeriod { CVDisplayLinkGetNominalOutputVideoRefreshPeriod (displayLink) };

        if ((nominalPeriod.flags & kCVTimeIsIndefinite) == 0 and nominalPeriod.timeValue > 0)
            refreshRateHz = static_cast<double> (nominalPeriod.timeScale) / static_cast<double> (nominalPeriod.timeValue);

        CVDisplayLinkRelease (displayLink);
    }

    return refreshRateHz;
}
#else
double Application::queryPrimaryDisplayRefreshRateHz() noexcept
{
    const auto* primaryDisplay { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() };

    return (primaryDisplay != nullptr)
        ? primaryDisplay->verticalFrequencyHz.value_or (indeterminateRefreshRateHz)
        : indeterminateRefreshRateHz;
}
#endif

void Application::initialiseVulkan()
{
    // Refresh-rate-derived per-frame time budget, detected once here (never
    // polled) — feeds VulkanEngine's session-locked MSAA calibration.
    const auto refreshRateHz { queryPrimaryDisplayRefreshRateHz() };
    const auto targetFrameBudgetMs { refreshRateHz >= highRefreshRateThresholdHz
        ? highRefreshFrameBudgetMs
        : standardRefreshFrameBudgetMs };

    // Vulkan pipeline cache — resolved under END's own config directory
    // (file::Config::path, ~/.config/end/), never decided by JAM. Explicit
    // per VulkanEngine's contract, mirroring targetFrameBudgetMs above.
    const auto cacheDir { jam::File::getOrCreateDirectory (file::Config::path, IDref::cache) };
    const juce::File cacheFile { cacheDir.getChildFile (
        jam::Format::toFileName (ProjectInfo::projectName, IDref::cache)) };
    const bool canUseGpu { config.getValue (IDtype::display, ID::gpu)
                           and jam::GpuProbe::probe().isAvailable };

    jam::BackgroundBlur::setEnabled (canUseGpu);

    vulkanEngine = std::make_unique<jam::VulkanEngine> (
        targetFrameBudgetMs, cacheFile, canUseGpu);

    // LookAndFeel owns font knowledge but not the atlas — the atlas (owned by
    // the VulkanEngine just constructed above) does not exist at LookAndFeel
    // construction time, so registration happens here instead, the earliest
    // point both exist together.
    lookAndFeel.registerTypeface (*jam::GlyphAtlas::getInstance());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (end::Application)
