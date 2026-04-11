/**
 * @file Nexus.h
 * @brief Session manager — owns all Terminal::Session instances and manages
 *        data flow attachment.
 *
 * `Nexus` is a pure session container.  It owns `Terminal::Session` objects
 * indexed by UUID and exposes lifecycle methods (`create`, `remove`, `get`,
 * `has`, `list`).  Data flow mode (standalone, daemon, client) is determined
 * by which `attach` overload the caller invokes.
 *
 * ### Attachment model
 * - **No attachment** — standalone.  Sessions fire `onExit` locally; when the
 *   last session exits `onAllSessionsExited` is called.
 * - **attach(Daemon&)** — daemon mode.  IPC wiring is layered on top of the
 *   session container by a higher-level coordinator (Step 4/5 work).
 * - **attach(Link&)** — client mode.  Same: IPC wiring applied externally.
 *
 * ### Context
 * Nexus extends `jreng::Context<Nexus>` so any subsystem can reach it via
 * `Nexus::getContext()` without a singleton pattern.  The single instance is
 * owned as a value member of `ENDApplication`.
 *
 * @note All public methods are **NEXUS PROCESS MESSAGE THREAD** only unless
 *       stated otherwise.
 *
 * @see Terminal::Session
 * @see Interprocess::Daemon
 * @see Interprocess::Link
 * @see jreng::Context
 */

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <jreng_core/jreng_core.h>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Terminal  { class Session; }
namespace Interprocess { class Daemon; class Link; }

/*____________________________________________________________________________*/

/**
 * @class Nexus
 * @brief Session manager — owns Terminal::Session instances and routes data flow.
 *
 * Constructed once by `ENDApplication`.  Destroyed after the main window so
 * that all Display objects are torn down before sessions are destroyed.
 *
 * @par Thread context
 * All session-management methods: **NEXUS PROCESS MESSAGE THREAD**.
 * `attach` overloads: any thread (pointer store only — no contention).
 */
class Nexus : public jreng::Context<Nexus>
{
public:
    /** @brief Constructs Nexus with no attachment — standalone mode. */
    Nexus();

    ~Nexus() override;

    // =========================================================================
    /** @name Session lifecycle
     * @{ */

    /**
     * @brief Creates a full PTY-backed session and stores it by UUID.
     *
     * Delegates to `Terminal::Session::create(cwd, cols, rows, shell, args,
     * seedEnv, uuid)`.  Wires a standalone `onExit` callback that removes the
     * session and calls `fireIfAllExited`.
     *
     * @param cwd      Initial working directory.  Empty = inherit.
     * @param cols     Initial column count.  Must be > 0.
     * @param rows     Initial row count.  Must be > 0.
     * @param shell    Shell program override.  Empty = read from Config.
     * @param args     Shell arguments override.  Empty = read from Config.
     * @param seedEnv  Extra environment variables merged before shell open.
     * @param uuid     Explicit UUID to assign.  Must be non-empty.
     * @return Mutable reference to the newly constructed Terminal::Session.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Session& create (const juce::String& cwd,
                               int cols,
                               int rows,
                               const juce::String& shell,
                               const juce::String& args,
                               const juce::StringPairArray& seedEnv,
                               const juce::String& uuid);

    /**
     * @brief Creates a remote (no-TTY) session and stores it by UUID.
     *
     * Delegates to `Terminal::Session::create(cols, rows, cwd, shell, uuid)`.
     * Used in client mode where the daemon owns the PTY.  CWD and shell are
     * written to State so display logic works without waiting for a stateUpdate PDU.
     *
     * @param cols   Terminal width in character columns.  Must be > 0.
     * @param rows   Terminal height in character rows.  Must be > 0.
     * @param cwd    Initial working directory — written to State.
     * @param shell  Shell program name — written to State for displayName logic.
     * @param uuid   Explicit UUID to assign.  Must be non-empty.
     * @return Mutable reference to the newly constructed Terminal::Session.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Session& create (int cols, int rows,
                               const juce::String& cwd,
                               const juce::String& shell,
                               const juce::String& uuid);

    /**
     * @brief Mode-routing session creation.
     *
     * Routes internally based on which attachment is live:
     * - If `attachedLink != nullptr` (client mode): creates a remote (no-TTY) session
     *   and sends a `createSession` PDU to the daemon via Link.  Wires writeInput /
     *   onResize callbacks on the Processor to route through Link.
     * - Otherwise (standalone / daemon mode): creates a full PTY-backed session.
     *   In daemon mode Nexus additionally calls
     *   `attachedDaemon->wireSessionCallbacks(uuid, session)` to wire IPC broadcast.
     *
     * Returns the existing session immediately if @p uuid already exists
     * (idempotency guard for GUI reconnect).
     *
     * @param cwd   Initial working directory.  Empty = inherit.
     * @param uuid  Explicit UUID to assign.  Must be non-empty.
     * @param cols  Initial column count.  Must be > 0.
     * @param rows  Initial row count.  Must be > 0.
     * @return Mutable reference to the Terminal::Session (use getProcessor() for the Processor).
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Session& create (const juce::String& cwd,
                               const juce::String& uuid,
                               int cols,
                               int rows);

    /**
     * @brief Removes and destroys the session with the given UUID.
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
     * @return Mutable reference to the owned Terminal::Session.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Session& get (const juce::String& uuid);

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
    /** @name Interprocess attachment
     *  Determines data flow mode.  Non-owning pointers — ENDApplication owns
     *  the actual objects.
     * @{ */

    /**
     * @brief Attaches a Daemon — enables daemon-mode data flow wiring.
     *
     * Stores a non-owning pointer.  IPC callback wiring is applied by a
     * higher-level coordinator (Step 4/5 work).
     *
     * @param daemon  The daemon instance owned by ENDApplication.
     * @note Any thread.
     */
    void attach (Interprocess::Daemon& daemon);

    /**
     * @brief Attaches a Link — enables client-mode data flow wiring.
     *
     * Stores a non-owning pointer.  IPC callback wiring is applied by a
     * higher-level coordinator (Step 4/5 work).
     *
     * @param link  The link instance owned by ENDApplication.
     * @note Any thread.
     */
    void attach (Interprocess::Link& link);

    /** @} */

    // =========================================================================
    /**
     * @brief Called when the last session exits.
     *
     * Set by ENDApplication in headless daemon mode to trigger shutdown.
     */
    std::function<void()> onAllSessionsExited;

private:
    /**
     * @brief Owned Terminal::Session map: UUID → unique_ptr<Terminal::Session>.
     */
    std::unordered_map<juce::String, std::unique_ptr<Terminal::Session>> sessions;

    /**
     * @brief Non-owning pointer to an attached Daemon.  Null when not in daemon mode.
     *
     * Ownership lives in ENDApplication.
     */
    Interprocess::Daemon* attachedDaemon { nullptr };

    /**
     * @brief Non-owning pointer to an attached Link.  Null when not in client mode.
     *
     * Ownership lives in ENDApplication.
     */
    Interprocess::Link* attachedLink { nullptr };

    /**
     * @brief Fires onAllSessionsExited if the sessions map is now empty.
     *
     * Called at every exit path after the departing entry has been erased.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void fireIfAllExited() noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};

/**______________________________END OF FILE___________________________________*/
