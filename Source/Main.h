#pragma once
#include <JuceHeader.h>
#include "action/ENDActions.h"
#include "config/ConfigModel.h"
#include "end/ENDModel.h"
#include "ui/ENDView.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"
#include "plugins/Nexus.h"

class ENDApplication : public juce::JUCEApplication,
                       private juce::ValueTree::Listener
{
public:
    ENDApplication();
    ~ENDApplication() override;
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
    jam::SharedInstance<map::FontRasterizerBackend> fontRasterizerBackend { std::in_place };

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

    /** @brief Callbacks for the config properties and tables this application applies, keyed by property, tree type or parent tree type. valueTreePropertyChanged() dispatches through it. */
    jam::Function::Map<juce::Identifier, void> events;

    //==============================================================================
    void initialise (const juce::String& commandLine) override;

    /** @brief Creates the engine through jam::VulkanGraphicsEngine::getOrCreate, registers END's embedded typefaces in
     *  jam::Typeface, and applies the configured glyph rasterization backend and
     *  embolden state — called once from initialise(). The background shader and
     *  the post-process chain are applied later in initialise(), after the window exists. */
    void initialiseVulkan();

    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Populates @ref events with the callbacks for every config value this application applies: font rasterization, embolden, GPU use, background, filter, post-process, and the two post-process render parameters. */
    void registerEvents();

    /** @brief Reads the font rasterizer backend, gamma and contrast from config, applies them to jam::CpuGraphicsEngine, then cascades a LookAndFeel change to every top-level window so the new rasterization repaints. */
    void setFontRasterization();

    /** @brief Reads Id::embolden from config, applies it to jam::CpuGraphicsEngine, then cascades a LookAndFeel change to every top-level window so the new embolden state repaints. */
    void setEmbolden();

    /** @brief Compiles the configured background shader and installs it on the view, or clears the background when the GPU is unavailable or no project is named. */
    void setBackground();

    /** @brief Compiles the configured post-process chain and installs it on the engine with the current opacity and resolution scale, or installs no chain when the GPU is unavailable or no project is named. */
    void setPostProcess();

    /** @brief Applies the post-process opacity and resolution scale from config to the chain the engine already holds. */
    void setPostProcessParams();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDApplication)
};
