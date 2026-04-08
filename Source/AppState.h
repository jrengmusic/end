/**
 * @file AppState.h
 * @brief Application-level ValueTree owner — Single Source of Truth for all UI state.
 *
 * AppState owns the root `END` ValueTree that holds window dimensions, tab layout,
 * split pane configuration, and (via grafted subtrees) each terminal's session state.
 *
 * Inherits `jreng::Context<AppState>` so any subsystem can call
 * `AppState::getContext()` without passing references.
 *
 * ### Serialization
 * `save()` writes `~/.config/end/state.xml` on quit.
 * `load()` reads it on launch to restore the previous session layout.
 *
 * ### Replaces
 * - `Config::loadState()` (was reading state.lua)
 * - `Config::saveWindowSize()` (was writing state.lua)
 * - `Config::saveZoom()` (was writing state.lua)
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD**.
 *
 * @see AppIdentifier.h
 * @see Config
 */

#pragma once

#include <JuceHeader.h>
#include "AppIdentifier.h"

struct AppState : jreng::Context<AppState>
{
    AppState();
    ~AppState();

    //==============================================================================

    juce::ValueTree& get() noexcept;

    juce::ValueTree getWindow() noexcept;
    juce::ValueTree getTabs() noexcept;

    //==============================================================================

    int getWindowWidth() const noexcept;
    int getWindowHeight() const noexcept;
    float getWindowZoom() const noexcept;

    void setWindowSize (int width, int height);
    void setWindowZoom (float zoom);

    /** @brief Returns the resolved renderer type enum. */
    App::RendererType getRendererType() const noexcept;

    /**
     * @brief Resolves and stores the renderer type from a config setting.
     *
     * Takes the raw config value ("auto", "true", or "false") and resolves
     * against the stored gpuAvailable flag:
     * - setting != "false" AND gpuAvailable → "gpu"
     * - otherwise → "cpu"
     *
     * @param setting The raw gpu.acceleration config value.
     */
    void setRendererType (const juce::String& setting);

    /**
     * @brief Stores the GPU availability flag from a probe result.
     *
     * Call once at startup before setRendererType(). The probe determines
     * whether the GL pipeline is hardware-accelerated.
     */
    void setGpuAvailable (bool available);

    /**
     * @brief Stores whether this instance is running as a Nexus client.
     *
     * Call once in initialise() before creating the main window.
     */
    void setNexusMode (bool isNexus);

    /** @brief Returns true when this instance is running as a Nexus client. */
    bool isNexusMode() const noexcept;

    int getActiveTabIndex() const noexcept;
    void setActiveTabIndex (int index);

    juce::String getTabPosition() const noexcept;
    void setTabPosition (const juce::String& position);

    //==============================================================================

    juce::ValueTree addTab();
    void removeTab (int index);
    juce::ValueTree getTab (int index) noexcept;

    juce::String getActivePaneID() const noexcept;
    void setActivePaneID (const juce::String& uuid);

    juce::String getActivePaneType() const noexcept;
    void setActivePaneType (const juce::String& type);

    void setModalType (int type);
    int  getModalType() const noexcept;

    void setSelectionType (int type);
    int  getSelectionType() const noexcept;

    juce::String getPwd() const noexcept;
    void setPwd (juce::ValueTree sessionTree);

    //==============================================================================

    void save();
    void load();

    juce::File getStateFile() const;

private:
    juce::ValueTree state;
    juce::Value pwdValue;

    void initDefaults();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppState)
};
