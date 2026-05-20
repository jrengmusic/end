/**
 * @file Nexus.cpp
 * @brief Implementation of Nexus — session manager.
 *
 * @see Nexus
 * @see terminal::Session
 */

#include "Nexus.h"

// =============================================================================

/**
 * @brief Constructs Nexus in standalone mode — no IPC attachment.
 */
Nexus::Nexus() = default;

/**
 * @brief Destructs Nexus — releases all owned terminal::Session objects.
 */
Nexus::~Nexus() = default;

// =============================================================================

/**
 * @brief Sets the IPC mode.
 *
 * @note Any thread.
 */
void Nexus::setMode (Mode m) noexcept { mode = m; }

/**
 * @brief Returns the current IPC mode.
 *
 * @note Any thread.
 */
Nexus::Mode Nexus::getMode() const noexcept { return mode; }

// =============================================================================

/**
 * @brief Creates a full PTY-backed session and stores it by UUID.
 *
 * Fires a child-added event on `events` after the session is stored so that
 * registered listeners (Daemon, Link) can react to the new session.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
terminal::Session& Nexus::create (const juce::String& cwd,
                                   cell cols,
                                   cell rows,
                                   const juce::String& shell,
                                   const juce::String& args,
                                   const juce::StringPairArray& seedEnv,
                                   const juce::String& uuid)
{
    jassert (uuid.isNotEmpty());
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    auto termSession { terminal::Session::create (cwd, cols, rows, shell, args, seedEnv, uuid) };
    terminal::Session* rawPtr { termSession.get() };

    sessions.emplace (uuid, std::move (termSession));

    juce::ValueTree node { "SESSION" };
    node.setProperty (jam::ID::id, uuid, nullptr);
    events.appendChild (node, nullptr);

    return *rawPtr;
}

/**
 * @brief Creates a remote (no-TTY) session and stores it by UUID.
 *
 * Fires a child-added event on `events` after the session is stored.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
terminal::Session& Nexus::create (cell cols, cell rows,
                                   const juce::String& cwd,
                                   const juce::String& shell,
                                   const juce::String& uuid)
{
    jassert (uuid.isNotEmpty());
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    auto termSession { terminal::Session::create (cols, rows, cwd, shell, uuid) };
    terminal::Session* rawPtr { termSession.get() };

    sessions.emplace (uuid, std::move (termSession));

    juce::ValueTree node { "SESSION" };
    node.setProperty (jam::ID::id, uuid, nullptr);
    events.appendChild (node, nullptr);

    return *rawPtr;
}

/**
 * @brief Mode-routing session creation.
 *
 * Routes based on mode:
 * - Client mode: creates remote session and lets the `events` VT event notify Link.
 * - Standalone/daemon mode: creates PTY-backed session; `events` VT event notifies Daemon.
 *
 * Returns the existing session immediately if @p uuid already exists
 * (idempotency guard for GUI reconnect).
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
terminal::Session& Nexus::create (const juce::String& cwd,
                                   const juce::String& uuid,
                                   cell cols,
                                   cell rows)
{
    jassert (uuid.isNotEmpty());
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    const auto existing { sessions.find (uuid) };
    const bool alreadyExists { existing != sessions.end() };

    terminal::Session* result { nullptr };

    if (alreadyExists)
    {
        result = existing->second.get();
    }
    else if (mode == Mode::client)
    {
        // Client mode — create a local remote (no-TTY) session.
        // The events VT child-added fires inside the delegated create overload,
        // which Link observes to send the createSession PDU and wire IPC.
        const juce::String shell { lua::Engine::getContext()->nexus.shell.program };
        result = &create (cols, rows, cwd, shell, uuid);
    }
    else
    {
        // Standalone or daemon mode — full PTY-backed session.
        // The events VT child-added fires inside the delegated create overload,
        // which Daemon observes to wire session callbacks.
        result = &create (cwd, cols, rows, {}, {}, {}, uuid);
    }

    jassert (result != nullptr);
    return *result;
}

/**
 * @brief Removes and destroys the session with the given UUID.
 *
 * Fires a child-removed event on `events` before erasing the session so that
 * registered listeners (Daemon, Link) can react before the session is gone.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Nexus::remove (const juce::String& uuid)
{
    auto node { events.getChildWithProperty (jam::ID::id, uuid) };

    if (node.isValid())
        events.removeChild (node, nullptr);

    sessions.erase (uuid);
    fireIfAllExited();
}

/**
 * @brief Returns a mutable reference to the session with the given UUID.
 *
 * jasserts if no session with @p uuid exists.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
terminal::Session& Nexus::get (const juce::String& uuid)
{
    const auto it { sessions.find (uuid) };
    jassert (it != sessions.end());

    terminal::Session* result { nullptr };

    if (it != sessions.end())
        result = it->second.get();

    jassert (result != nullptr);
    return *result;
}

/**
 * @brief Returns true if a session with @p uuid is live.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Nexus::has (const juce::String& uuid) const noexcept
{
    return sessions.find (uuid) != sessions.end();
}

/**
 * @brief Returns a snapshot of all live session UUIDs.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
juce::StringArray Nexus::list() const
{
    juce::StringArray uuids;

    for (const auto& pair : sessions)
        uuids.add (pair.first);

    return uuids;
}

// =============================================================================

/**
 * @brief Fires onAllSessionsExited if the sessions map is now empty.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Nexus::fireIfAllExited() noexcept
{
    if (sessions.empty() and onAllSessionsExited != nullptr)
        onAllSessionsExited();
}

/**______________________________END OF FILE___________________________________*/
