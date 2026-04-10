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
 * ### Serialization — two files, two concerns
 *
 * `~/.config/end/end.state` — full state (window + sessions); standalone mode only.
 * Written by `save()`.  Read on startup by `load()`.
 *
 * `~/.config/end/nexus/<uuid>.nexus` — plain-text port number only; daemon writes;
 * daemon deletes on exit.  Written by `setPort()`.  Read by startup scan (plain text).
 *
 * `~/.config/end/nexus/<uuid>.display` — full state (window + sessions + connected);
 * nexus client writes; daemon deletes both files on clean exit.
 * Written by `save()`.  Read on startup by `load()`.
 *
 * ### SSOT
 * - Daemon owns `.nexus` (port).  Daemon never touches `.display`.
 * - MainComponent owns `.display` (full state).  MainComponent never writes `.nexus`.
 * - `getStateFile()` returns the correct path for the current mode.
 *
 * ### Instance UUID
 * The ctor calls only `initDefaults()`.  `initialise()` must call
 * `setInstanceUuid()` first, then `load()` explicitly.  File paths derive
 * from the stored UUID and whether nexus mode is active.
 *
 * ### Connected flag and port
 * `setConnected(true/false)` sets the property only — caller must call `save()` when needed.
 * `setPort(n)` writes the port as plain text to `<uuid>.nexus` immediately.
 *
 * ### Destructor
 * The dtor is trivial — no file I/O.  Main owns all file decisions via `systemRequestedQuit()`.
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
    juce::ValueTree getNexusNode() noexcept;
    juce::ValueTree getSessionsNode() noexcept;

    juce::ValueTree getLoadingNode() noexcept;
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
     * @param setting The raw gpu config value.
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
     * @brief Stores the UUID for this instance — used by file path methods.
     *
     * Must be called in initialise() before load(), save(), or setPort().
     *
     * @param uuid  The UUID string for this instance.
     */
    void setInstanceUuid (const juce::String& uuid);

    /**
     * @brief Returns the stored instance UUID.
     * @note Any thread.
     */
    juce::String getInstanceUuid() const noexcept;

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

    /**
     * @brief Sets the connected flag on the state tree.
     *
     * Does not save to disk — caller is responsible for calling save() when needed.
     *
     * @param isConnected  true = a client has established IPC; false = disconnected.
     *
     * @note MESSAGE THREAD.
     */
    void setConnected (bool isConnected);

    /**
     * @brief Returns the current connected flag from the state tree.
     * @note Any thread (reads ValueTree property — message-thread safe).
     */
    bool isConnected() const noexcept;

    /**
     * @brief Stores the daemon's bound TCP port and writes it to `<uuid>.nexus` as plain text.
     *
     * Called by Daemon::start() after binding.  Reading this value from the
     * nexus file during the startup scan tells the client which port to probe.
     * Writes ONLY the plain-text port number — no XML, no ValueTree.
     * Daemon calls this.  Daemon never touches `.display`.
     *
     * @param activePort  The bound TCP port.
     * @note MESSAGE THREAD.
     */
    void setPort (int activePort);

    /**
     * @brief Returns the stored daemon port, or 0 if none.
     * @note Any thread.
     */
    int getPort() const noexcept;

    //==============================================================================

    /**
     * @brief Writes the full state (window + sessions + connected) to `getStateFile()`.
     *
     * Standalone mode: writes to `~/.config/end/end.state`.
     * Nexus client mode: writes to `~/.config/end/nexus/<uuid>.display`.
     *
     * The NEXUS subtree (processors, loading ops) is excluded from persistence
     * because it is rebuilt live when the daemon reconnects.
     * Port is NOT written here — port lives in `<uuid>.nexus` (daemon's file).
     * Daemon never calls this directly — daemon only writes its port via setPort().
     *
     * @note MESSAGE THREAD.
     */
    void save();

    /**
     * @brief Reads the full state from `getStateFile()` into the in-memory tree.
     *
     * Standalone mode: reads from `~/.config/end/end.state`.
     * Nexus client mode: reads from `~/.config/end/nexus/<uuid>.display`.
     * Falls back silently to initDefaults() values if the file is absent or
     * cannot be parsed.
     *
     * @note MESSAGE THREAD.
     */
    void load();

    /**
     * @brief Deletes `~/.config/end/nexus/<uuid>.nexus`.
     *
     * Called by daemon on clean exit via `AppState::getContext()->deleteNexusFile()`.
     * Link never calls this.
     *
     * @note MESSAGE THREAD.
     */
    void deleteNexusFile();

    /** @brief Returns the state file path for the current mode.
     *
     *  UUID set (nexus client or daemon): `~/.config/end/nexus/<uuid>.display`.
     *  No UUID (standalone): `~/.config/end/end.state`.
     */
    juce::File getStateFile() const;

    /** @brief Returns `~/.config/end/nexus/<uuid>.nexus` (daemon's port file). */
    juce::File getNexusFile() const;

private:
    juce::ValueTree state;
    juce::Value pwdValue;
    juce::String instanceUuid;

    void initDefaults();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppState)
};
