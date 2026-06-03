/**
 * @file AppModel.h
 * @brief Single config authority — owns Engine privately, owns the config watcher, builds CONFIG once.
 *
 * AppModel owns `lua::Engine` by value and the `jam::File::Watcher` that
 * monitors the config directory. It builds the CONFIG subtree once in the constructor, calls
 * `engine.load(*this)` on startup and on every qualifying file-change event, and exposes
 * the CONFIG subtree read-only via `getConfig()`. All config-derived values are resolved through
 * Engine delegation methods — AppModel never exposes raw Lua struct members.
 *
 * Inherits `jam::Model` for the VT owner machinery and `jam::Context<AppModel>`
 * so any subsystem can call `AppModel::getContext()` without passing references.
 * Inherits `jam::File::Watcher::Listener` to receive file-change events from the watcher.
 *
 * ### Mechanism
 * AppParameters.xml declares the schema. `build()` walks it and creates
 * PARAM children + Parameter<int> adapters in the flat params AnyMap. All scalar
 * reads use getValueFromChildWithID; all scalar writes use setValue. flush()
 * syncs Parameters to VT on the 60 Hz timer; restoreValues() syncs VT back to Parameters
 * after replaceState.
 *
 * ### CONFIG tree (D8.2 correctness law)
 * Nodes are created once via `getOrCreateChildWithName` and are permanent — they are never
 * removed or recreated. The structure under the `END` root is:
 * ```
 * END
 * +-- CONFIG
 *     +-- NEXUS
 *     +-- DISPLAY
 *     +-- WHELMED
 *     +-- KEYS
 *     +-- POPUPS
 *     +-- ACTIONS
 * ```
 * The CONFIG node is excluded from `save()` — it is ephemeral, rebuilt by Engine on every load.
 *
 * ### Serialization — three files, three concerns
 *
 * `~/.config/end/window.state` — WINDOW width/height only (standalone, cross-instance).
 * Written by `saveWindowState()`. Read by `loadWindowState()`.
 *
 * `~/.config/end/nexus/<uuid>.display` — full state (WINDOW + TABS);
 * daemon client writes on quit; daemon deletes on clean exit.
 * Written by `save()`. Read on startup by `load()`.
 *
 * `~/.config/end/nexus/<uuid>.nexus` — plain-text port number only; daemon writes;
 * daemon deletes on exit. Written by `setPort()`. Read by startup scan (plain text).
 *
 * ### SSOT
 * - Daemon owns `.nexus` (port). Daemon never touches `.display`.
 * - Daemon client owns `.display` (full state). Client never writes `.nexus`.
 * - `getStateFile()` returns `nexus/<uuid>.display` (daemon client mode only).
 *
 * ### Instance UUID
 * The ctor calls `build()`, constructs Engine, and loads config.
 * `initialise()` must call `setInstanceID()` first, then `load()` explicitly.
 * File paths derive from the stored UUID and whether nexus mode is active.
 * UUID is stored as a property on the root VT node using `jam::ID::id`.
 *
 * ### Port
 * `setPort(n)` writes the port as plain text to `<uuid>.nexus` immediately.
 *
 * ### Destructor
 * Removes the watcher listener. Engine destroyed by value. No file I/O.
 *
 * ### Thread model
 * All methods are called on the **MESSAGE THREAD**.
 * atlasDirty is stored as a Parameter<int> in the flat params AnyMap — GL thread uses
 * storeRelease/exchangeAcquire on the underlying atomic.
 *
 * @see AppIdentifier.h
 * @see lua::Engine
 */

#pragma once

#include <JuceHeader.h>
#include "AppIdentifier.h"
#include "lua/Engine.h"
#include "terminal/Identifier.h"
#include "Map.h"

struct AppModel
    : public jam::Model
    , public jam::Context<AppModel>
    , public jam::File::Watcher::Listener
{
    AppModel();
    ~AppModel();

    //==============================================================================

    juce::ValueTree getWindow() noexcept;
    juce::ValueTree getNexusNode() noexcept;
    juce::ValueTree getSessionsNode() noexcept;
    juce::ValueTree getTabs() noexcept;

    //==============================================================================

    int getWindowWidth() const noexcept;
    int getWindowHeight() const noexcept;
    float getWindowZoom() const noexcept;

    void setWindowSize (int width, int height);
    void setWindowZoom (float zoom);

    /** @brief Marks the glyph atlas as stale. Writes to VT; flush timer syncs to atomic for GL thread. */
    void markAtlasDirty() noexcept;

    /** @brief Returns the resolved renderer type enum. */
    app::RendererType getRendererType() const noexcept;

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
    void setInstanceID (const juce::String& uuid);

    /**
     * @brief Stores whether this instance is running as a daemon client.
     *
     * Call once in initialise() before creating the main window.
     */
    void setDaemonMode (bool isDaemon);

    /** @brief Returns true when this instance is running as a daemon client. */
    bool isDaemonMode() const noexcept;

    void setActiveTabIndex (int index);

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
     * Reads `terminal::id::cwd` directly from `activeSession` — the ref-counted
     * juce::ValueTree handle stored by `setPwd()`. Reading a live VT reference
     * produces the current value without any binding or listener.
     */
    juce::String getPwd() const noexcept;

    /**
     * @brief Stores a reference to the active session ValueTree.
     *
     * `getPwd()` reads `terminal::id::cwd` from this tree directly. Because
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
     * Called by Daemon::start() after binding. Reading this value from the
     * nexus file during the startup scan tells the client which port to probe.
     * Writes ONLY the plain-text port number — no XML, no ValueTree.
     * Daemon calls this. Daemon never touches `.display`.
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
     * Daemon client mode only. The NEXUS and CONFIG subtrees are excluded —
     * NEXUS is rebuilt live on reconnect; CONFIG is ephemeral, rebuilt by Engine on load.
     * Port and atlasDirty PARAM children are also excluded — transient, not serialized.
     * Daemon never calls this — daemon only writes its port via setPort().
     *
     * @note MESSAGE THREAD.
     */
    void save();

    /**
     * @brief Writes the current WINDOW subtree XML to `getWindowState()`.
     *
     * Cross-instance shared file, independent of session restore state.
     * Called on every quit when CONFIG/DISPLAY windowSaveSize is true.
     *
     * @note MESSAGE THREAD.
     */
    void saveWindowState();

    /**
     * @brief Reads `getWindowState()` and applies PARAM values into the in-memory tree.
     *
     * Called only for new instances (no prior session state) when
     * CONFIG/DISPLAY windowSaveSize is true. Silently no-ops on missing
     * file or parse failure — constructor defaults remain.
     *
     * @note MESSAGE THREAD.
     */
    void loadWindowState();

    /**
     * @brief Reads the full state from `nexus/<uuid>.display` into the in-memory tree.
     *
     * Daemon client mode only. Uses replaceState which syncs Parameters via restoreValues().
     * Falls back silently to constructor defaults if the file is absent or
     * cannot be parsed.
     *
     * @note MESSAGE THREAD.
     */
    void load();

    /**
     * @brief Deletes `~/.config/end/nexus/<uuid>.nexus`.
     *
     * Called by daemon on clean exit via `AppModel::getContext()->deleteNexusFile()`.
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
    // Config authority — single entry point for all config access (D8.1)
    //==============================================================================

    /** @brief Returns the CONFIG subtree — read-only handle for orchestrators to bind against. */
    juce::ValueTree getConfig() const noexcept;

    // Resolved aggregates (D8.9) — recomputed on reload, read CONFIG.
    lua::Engine::Theme buildTheme() const;
    const lua::Engine::SelectionKeys& getSelectionKeys() const;
    bool isClickableExtension (const juce::String& ext) const noexcept;
    juce::String getHandler (const juce::String& ext) const noexcept;
    float dpiCorrectedFontSize() const noexcept;

    // Non-config Engine delegation (D8.6).
    void registerActions (action::Registry& r);
    void buildKeyMap (action::Registry& r);
    void registerApiTable();
    void setDisplayCallbacks (lua::Engine::DisplayCallbacks c);
    void setPopupCallbacks (lua::Engine::PopupCallbacks c);
    const juce::String& getLoadError() const;
    juce::String getShortcutString (const juce::String& k) const;
    juce::String getActionLuaKey (const juce::String& a) const;
    juce::String getPrefixString() const noexcept;
    bool isKeyFileRemappable() const noexcept;

    // Sole consumer-initiated config mutation (D8.8).
    void overrideShortcut (const juce::String& key, const juce::String& value);

    /**
     * @brief Triggers an immediate config reload — equivalent to the file-watcher path.
     *
     * Calls engine->load(getConfig()) directly. Used by the reload_config action and by
     * ActionList on dtor when bindings were modified during the session.
     *
     * @note MESSAGE THREAD.
     */
    void reload();

    //==============================================================================

private:
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /**
     * @brief Walks the source VT (from AppParameters.xml) and populates this model.
     *
     * Pass 1: structural copy — every child of source becomes a child of state.
     * Pass 2: createAndAddParameter on every node via applyFunctionRecursively.
     *
     * @param source  The template VT (from juce::ValueTree::fromXml).
     * @note MESSAGE THREAD — called once during construction.
     */
    void build (const juce::ValueTree& source);

    /**
     * @brief Creates a Parameter and adds it to the AnyMap for a given PARAM node.
     *
     * Called on every node by applyFunctionRecursively. PARAM nodes: read
     * type/default/maxlen, dispatch to addParameter or addTextParameter
     * (idempotent). Group nodes under CONFIG: pre-register an AnyMap group.
     *
     * @param node  The VT node to process.
     * @return true if the node is a leaf (PARAM), false to continue recursion.
     * @note MESSAGE THREAD — called from build.
     */
    bool createAndAddParameter (const juce::ValueTree& node);

    /** @brief Returns the stored instance ID. */
    juce::String getInstanceID() const noexcept;

    /** @brief Ref-counted handle to the active session's VT. Set by setPwd(). */
    juce::ValueTree activeSession;

    /** @brief Value member — Lua config parser. Never exposed externally (D8.1). */
    lua::Engine engine;

    /** @brief File watcher monitoring the config directory. Sole watcher instance (D8.7). */
    jam::File::Watcher watcher;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppModel)
};
