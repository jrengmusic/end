/**
 * @file Nexus.h
 * @brief Session manager — owns all terminal::Session instances and manages
 *        data flow attachment.
 *
 * `Nexus` is a pure session container.  It owns `terminal::Session` objects
 * indexed by UUID and exposes lifecycle methods (`create`, `remove`, `get`,
 * `has`, `list`).  Data flow mode (standalone, daemon, client) is set via
 * `setMode()`.
 *
 * ### Mode model
 * - **standalone** — no IPC.  Session exit signals flow via State shellExited
 *   parameter → Panes::valueTreePropertyChanged → callAsync → Panes::closePane → Nexus::remove.
 * - **daemon** — Daemon registers on `events` and wires IPC callbacks on each new session.
 * - **client** — Link registers on `events` and sends PDUs on each session lifecycle event.
 *
 * ### Session lifecycle events
 * Nexus fires `juce::ValueTree::Listener` callbacks on the public `events` tree.
 * Listeners observe session creation via `valueTreeChildAdded` and removal via
 * `valueTreeChildRemoved`.  Child nodes are type "SESSION" with `jam::ID::id`
 * property set to the session UUID.
 *
 * ### Context
 * Nexus extends `jam::Context<Nexus>` so any subsystem can reach it via
 * `Nexus::getContext()` without a singleton pattern.  The single instance is
 * owned as a value member of `ENDApplication`.
 *
 * @note All public methods are **NEXUS PROCESS MESSAGE THREAD** only unless
 *       stated otherwise.
 *
 * @see terminal::Session
 * @see nexus::Daemon
 * @see nexus::Link
 * @see jam::Context
 */

#pragma once

#include <JuceHeader.h>
#include "../terminal/Session.h"
#include "../lua/Engine.h"

/*____________________________________________________________________________*/

/**
 * @class Nexus
 * @brief Session manager — owns terminal::Session instances and routes data flow.
 *
 * Constructed once by `ENDApplication`.  Destroyed after the main window so
 * that all Display objects are torn down before sessions are destroyed.
 *
 * @par Thread context
 * All session-management methods: **NEXUS PROCESS MESSAGE THREAD**.
 * `setMode` — any thread (atomic store only).
 */
class Nexus : public jam::Context<Nexus>
{
public:
    /** @brief Session creation mode. */
    enum class Mode { standalone, daemon, client };

    /** @brief Constructs Nexus with no attachment — standalone mode. */
    Nexus();

    ~Nexus() override;

    // =========================================================================
    /** @name Mode control
     * @{ */

    /**
     * @brief Sets the IPC mode.  Called once during ENDApplication initialization.
     * @note Any thread.
     */
    void setMode (Mode m) noexcept;

    /**
     * @brief Returns the current IPC mode.
     * @note Any thread.
     */
    Mode getMode() const noexcept;

    /** @} */

    // =========================================================================
    /** @name Session lifecycle
     * @{ */

    /**
     * @brief Creates a full PTY-backed session and stores it by UUID.
     *
     * Delegates to `terminal::Session::create(cwd, cols, rows, shell, args,
     * seedEnv, uuid)`.  Session exit is signalled via the State shellExited parameter,
     * which flows to registered VT listeners via the VT flush chain.
     * Fires a child-added event on `events` after the session is stored.
     *
     * @param cwd      Initial working directory.  Empty = inherit.
     * @param dims     Terminal dimensions in cells. Must be valid.
     * @param shell    Shell program override.  Empty = read from Config.
     * @param args     Shell arguments override.  Empty = read from Config.
     * @param seedEnv  Extra environment variables merged before shell open.
     * @param uuid     Explicit UUID to assign.  Must be non-empty.
     * @return Mutable reference to the newly constructed terminal::Session.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    terminal::Session& create (const juce::String& cwd,
                               jam::Cell::Rectangle dims,
                               const juce::String& shell,
                               const juce::String& args,
                               const juce::StringPairArray& seedEnv,
                               const juce::String& uuid);

    /**
     * @brief Creates a remote (no-TTY) session and stores it by UUID.
     *
     * Delegates to `terminal::Session::create(cols, rows, cwd, shell, uuid)`.
     * Used in client mode where the daemon owns the PTY.  CWD and shell are
     * written to State so display logic works without waiting for a stateUpdate PDU.
     * Fires a child-added event on `events` after the session is stored.
     *
     * @param dims   Terminal dimensions in cells (width = cols, height = rows).  Must be valid.
     * @param cwd    Initial working directory — written to State.
     * @param shell  Shell program name — written to State for displayName logic.
     * @param uuid   Explicit UUID to assign.  Must be non-empty.
     * @return Mutable reference to the newly constructed terminal::Session.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    terminal::Session& create (jam::Cell::Rectangle dims,
                               const juce::String& cwd,
                               const juce::String& shell,
                               const juce::String& uuid);

    /**
     * @brief Mode-routing session creation.
     *
     * Routes internally based on `mode`:
     * - **client**: creates a remote (no-TTY) session; fires `events` child-added
     *   which Link observes to send the createSession PDU and wire IPC.
     * - **standalone / daemon**: creates a full PTY-backed session; fires `events`
     *   child-added which Daemon observes to wire IPC callbacks.
     *
     * Returns the existing session immediately if @p uuid already exists
     * (idempotency guard for GUI reconnect).
     *
     * @param cwd   Initial working directory.  Empty = inherit.
     * @param uuid  Explicit UUID to assign.  Must be non-empty.
     * @param cols  Initial column count.  Must be > 0.
     * @param rows  Initial row count.  Must be > 0.
     * @return Mutable reference to the terminal::Session (use getProcessor() for the Processor).
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    terminal::Session& create (const juce::String& cwd,
                               const juce::String& uuid,
                               jam::Cell::Rectangle dims);

    /**
     * @brief Removes and destroys the session with the given UUID.
     *
     * Fires a child-removed event on `events` before erasing the session.
     *
     * @param uuid  UUID of the session to remove.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void remove (const juce::String& uuid);

    /**
     * @brief Returns a mutable reference to the session with the given UUID.
     *
     * jasserts if no session with @p uuid exists.
     *
     * @param uuid  UUID of the target session.
     * @return Mutable reference to the owned terminal::Session.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    terminal::Session& get (const juce::String& uuid);

    /**
     * @brief Returns true if a session with @p uuid is live.
     *
     * @param uuid  UUID to test.
     * @return true if found in sessions map.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    bool has (const juce::String& uuid) const noexcept;

    /**
     * @brief Returns a snapshot of all live session UUIDs.
     *
     * @return StringArray of UUID strings for all currently live sessions.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    juce::StringArray list() const;

    /** @} */

    // =========================================================================

    /**
     * @brief Event surface — listeners observe session lifecycle via valueTreeChildAdded/Removed.
     *
     * Child nodes are type "SESSION" with jam::ID::id property = uuid.
     * Added on session creation, removed on session removal.
     */
    juce::ValueTree events { "NEXUS_EVENTS" };

private:
    /**
     * @brief Owned terminal::Session map: UUID → unique_ptr<terminal::Session>.
     */
    std::unordered_map<juce::String, std::unique_ptr<terminal::Session>> sessions;

    /**
     * @brief Current IPC mode.
     */
    Mode mode { Mode::standalone };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};

/**______________________________END OF FILE___________________________________*/
