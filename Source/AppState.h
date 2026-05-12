/**
 * @file AppState.h
 * @brief Application-level ValueTree owner — Single Source of Truth for all UI state.
 *
 * AppState owns the root `END` ValueTree that holds window dimensions, tab layout,
 * split pane configuration, and (via grafted subtrees) each terminal's session state.
 *
 * Inherits `jam::ValueTree` for the VT owner machinery and `jam::Context<AppState>`
 * so any subsystem can call `AppState::getContext()` without passing references.
 *
 * ### Mechanism
 * AppParameters.xml declares the schema. AppLayout::build walks it and creates
 * PARAM children + Parameter<int> adapters in the flat params AnyMap. All scalar
 * reads use getValueFromChildWithID; all scalar writes use setValue. flush()
 * syncs Parameters to VT on the 60 Hz timer; restoreValues() syncs VT back to Parameters
 * after replaceState.
 *
 * ### Serialization — three files, three concerns
 *
 * `~/.config/end/window.state` — WINDOW width/height only (standalone, cross-instance).
 * Written by `saveWindowState()`.  Read by `loadWindowState()`.
 *
 * `~/.config/end/nexus/<uuid>.display` — full state (WINDOW + TABS);
 * daemon client writes on quit; daemon deletes on clean exit.
 * Written by `save()`.  Read on startup by `load()`.
 *
 * `~/.config/end/nexus/<uuid>.nexus` — plain-text port number only; daemon writes;
 * daemon deletes on exit.  Written by `setPort()`.  Read by startup scan (plain text).
 *
 * ### SSOT
 * - Daemon owns `.nexus` (port).  Daemon never touches `.display`.
 * - Daemon client owns `.display` (full state).  Client never writes `.nexus`.
 * - `getStateFile()` returns `nexus/<uuid>.display` (daemon client mode only).
 *
 * ### Instance UUID
 * The ctor calls AppLayout::build and overlays Lua runtime defaults.
 * `initialise()` must call `setInstanceUuid()` first, then `load()` explicitly.
 * File paths derive from the stored UUID and whether nexus mode is active.
 * UUID is stored as a property on the root VT node using `jam::ID::id`.
 *
 * ### Port
 * `setPort(n)` writes the port as plain text to `<uuid>.nexus` immediately.
 *
 * ### Destructor
 * The dtor is trivial — no file I/O.  Main owns all file decisions via `systemRequestedQuit()`.
 *
 * ### Thread model
 * All methods are called on the **MESSAGE THREAD**.
 * atlasDirty is stored as a Parameter<int> in the flat params AnyMap — GL thread uses
 * storeRelease/exchangeAcquire; message thread reads via consumeAtlasDirty().
 *
 * @see AppIdentifier.h
 * @see AppLayout.h
 * @see Config
 */

#pragma once

#include <JuceHeader.h>
#include "AppIdentifier.h"
#include "AppLayout.h"

struct AppState : public jam::ValueTree, public jam::Context<AppState>
{
    AppState();
    ~AppState();

    //==============================================================================

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

    juce::String getFontFamily() const noexcept;
    void setFontFamily (const juce::String& family);
    float getFontSize() const noexcept;
    void setFontSize (float size);

    /** @brief Marks the glyph atlas as stale. GL thread safe — uses atomic store/release. */
    void markAtlasDirty() noexcept;

    /** @brief Consumes and clears the atlas-dirty flag. GL thread safe — uses atomic exchange/acquire. */
    bool consumeAtlasDirty() noexcept;

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
     * Writes `jam::ID::id` property on the root VT node.
     * Must be called in initialise() before load(), save(), or setPort().
     *
     * @param uuid  The UUID string for this instance.
     */
    void setInstanceUuid (const juce::String& uuid);

    /**
     * @brief Returns the stored instance UUID.
     * @note MESSAGE THREAD.
     */
    juce::String getInstanceUuid() const noexcept;

    /**
     * @brief Stores whether this instance is running as a daemon client.
     *
     * Call once in initialise() before creating the main window.
     */
    void setDaemonMode (bool isDaemon);

    /** @brief Returns true when this instance is running as a daemon client. */
    bool isDaemonMode() const noexcept;

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

    /** @brief Sets the modal overlay type on the TABS node. */
    void setModalType (int type);

    /** @brief Returns the modal overlay type from the TABS node. */
    int getModalType() const noexcept;

    /** @brief Sets the selection mode type on the TABS node. */
    void setSelectionType (int type);

    /** @brief Returns the selection mode type from the TABS node. */
    int getSelectionType() const noexcept;

    /**
     * @brief Returns the cwd of the active session, or the user home directory if none.
     *
     * Reads `Terminal::ID::cwd` directly from `activeSession` — the ref-counted
     * juce::ValueTree handle stored by `setPwd()`.  Reading a live VT reference
     * produces the current value without any binding or listener.
     */
    juce::String getPwd() const noexcept;

    /**
     * @brief Stores a reference to the active session ValueTree.
     *
     * `getPwd()` reads `Terminal::ID::cwd` from this tree directly.  Because
     * juce::ValueTree is ref-counted, the stored handle always reflects current
     * session state — no binding, no listener, no stale reads.
     *
     * @param sessionTree  The SESSION subtree of the newly focused pane.
     */
    void setPwd (juce::ValueTree sessionTree);

    //==============================================================================

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
     * @note MESSAGE THREAD.
     */
    int getPort() const noexcept;

    //==============================================================================

    /**
     * @brief Writes the full state (WINDOW + TABS) to `nexus/<uuid>.display`.
     *
     * Daemon client mode only.  The NEXUS subtree (sessions, loading ops) is
     * excluded — rebuilt live on reconnect.  Port and atlasDirty PARAM children
     * are also excluded — transient, not serialized.
     * Daemon never calls this — daemon only writes its port via setPort().
     *
     * @note MESSAGE THREAD.
     */
    void save();

    /**
     * @brief Writes the current WINDOW subtree XML to `getWindowState()`.
     *
     * Cross-instance shared file, independent of session restore state.
     * Called on every quit when `lua::Engine::display.window.saveSize` is true.
     *
     * @note MESSAGE THREAD.
     */
    void saveWindowState();

    /**
     * @brief Reads `getWindowState()` and applies PARAM values into the in-memory tree.
     *
     * Called only for new instances (no prior session state) when
     * `lua::Engine::display.window.saveSize` is true.  Silently no-ops on missing
     * file or parse failure — constructor defaults remain.
     *
     * @note MESSAGE THREAD.
     */
    void loadWindowState();

    /**
     * @brief Reads the full state from `nexus/<uuid>.display` into the in-memory tree.
     *
     * Daemon client mode only.  Uses replaceState which syncs Parameters via restoreValues().
     * Falls back silently to constructor defaults if the file is absent or
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

    /** @brief Returns `~/.config/end/nexus/<uuid>.display` (daemon client mode only). */
    juce::File getStateFile() const;

    /** @brief Returns `~/.config/end/nexus/<uuid>.nexus` (daemon's port file). */
    juce::File getNexusFile() const;

    /** @brief Returns `~/.config/end/window.state` — cross-instance window size file. */
    juce::File getWindowState() const;

    //==============================================================================

private:
    /** @brief Ref-counted handle to the active session's VT. Set by setPwd(). */
    juce::ValueTree activeSession;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppState)
};
