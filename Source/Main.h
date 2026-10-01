#pragma once
#include <JuceHeader.h>
#include "action/ENDActions.h"
#include "config/ConfigModel.h"
#include "end/ENDModel.h"
#include "end/ENDView.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"
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
    static inline const char* const debugLogFileExtension { ".ode" };

    /** @brief Diagnostic log sink — canonical location: ConfigDirectory::Config::path
     *  (\~/.config/end/end.ode, jam::Format::toFileName), never the launch
     *  cwd — the same deterministic path regardless of how the app was
     *  started (IDE, Finder, terminal), so runtime diagnostics always land
     *  in one known, readable file. */

    jam::debug::Log::Scope logScope {
        ConfigDirectory::Config::path
            .getChildFile (jam::Format::toFileName (ProjectInfo::projectName, debugLogFileExtension))
    };
#endif

    //==============================================================================
    // Owned global registry aggregate — jam::Bimap<T> owner. Declared before any consumer so the
    // single-global-pointer Instance<T> slot is populated before first use.
    Generated generated;

    jam::SharedInstance<jam::VulkanShaderFormat> shaderFormat { std::in_place };

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

    std::unique_ptr<jam::Window> window;

    //==============================================================================
    void initialise (const juce::String& commandLine) override;

    /** @brief Creates the engine through jam::VulkanEngine::getOrCreate, registers END's embedded typefaces with
     *  its atlas, and enables the post-process background-blur shader — the
     *  whole GPU-availability-gated setup block, called once from initialise(). */
    void initialiseVulkan();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDApplication)
};
