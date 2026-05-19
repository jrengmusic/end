/**
 * @file Nexus.cpp
 * @brief Implementation of Nexus — session manager.
 *
 * @see Nexus
 * @see Terminal::Session
 */

#include "Nexus.h"
#include "../terminal/logic/Session.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/data/Identifier.h"
#include "../interprocess/Daemon.h"
#include "../interprocess/Link.h"
#include "../lua/Engine.h"

// =============================================================================

/**
 * @brief Constructs Nexus in standalone mode — no IPC attachment.
 */
Nexus::Nexus() = default;

/**
 * @brief Destructs Nexus — releases all owned Terminal::Session objects.
 */
Nexus::~Nexus() = default;

// =============================================================================

/**
 * @brief Creates a full PTY-backed session and stores it by UUID.
 *
 * After creating the session, if an Interprocess::Daemon is attached, calls
 * `attachedDaemon->wireSessionCallbacks` which wires daemon-mode IPC callbacks.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Session& Nexus::create (const juce::String& cwd,
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

    auto termSession { Terminal::Session::create (cwd, cols, rows, shell, args, seedEnv, uuid) };
    Terminal::Session* rawPtr { termSession.get() };

    sessions.emplace (uuid, std::move (termSession));

    if (attachedDaemon != nullptr)
        attachedDaemon->wireSessionCallbacks (uuid, *rawPtr);

    return *rawPtr;
}

/**
 * @brief Creates a remote (no-TTY) session and stores it by UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Session& Nexus::create (cell cols, cell rows,
                                   const juce::String& cwd,
                                   const juce::String& shell,
                                   const juce::String& uuid)
{
    jassert (uuid.isNotEmpty());
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    auto termSession { Terminal::Session::create (cols, rows, cwd, shell, uuid) };
    Terminal::Session* rawPtr { termSession.get() };

    sessions.emplace (uuid, std::move (termSession));
    return *rawPtr;
}

/**
 * @brief Mode-routing session creation.
 *
 * Routes based on attachment:
 * - Client mode (attachedLink != nullptr): creates remote session, sends
 *   createSession PDU, wires setInputWriter() on the Processor to route through Link.
 * - Standalone/daemon mode: creates PTY-backed session via the TTY overload
 *   (which also calls wireSessionCallbacks when a Daemon is attached).
 *
 * Returns the existing session immediately if @p uuid already exists
 * (idempotency guard for GUI reconnect).
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Session& Nexus::create (const juce::String& cwd,
                                   const juce::String& uuid,
                                   cell cols,
                                   cell rows)
{
    jassert (uuid.isNotEmpty());
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    const auto existing { sessions.find (uuid) };
    const bool alreadyExists { existing != sessions.end() };

    Terminal::Session* result { nullptr };

    if (alreadyExists)
    {
        result = existing->second.get();
    }
    else if (attachedLink != nullptr)
    {
        // Client mode — send createSession PDU and create a local remote (no-TTY) session.
        const juce::String shell { lua::Engine::getContext()->nexus.shell.program };
        attachedLink->sendCreateSession (cwd, uuid, cols.value, rows.value);

        auto termSession { Terminal::Session::create (cols, rows, cwd, shell, uuid) };
        Terminal::Session* rawPtr { termSession.get() };

        // Wire user input (keyboard, mouse) to daemon via Link IPC.
        rawPtr->getProcessor().setInputWriter ([this, uuid] (const char* data, int len)
        {
            attachedLink->sendInput (uuid, data, len);
        });

        // Wire resize forwarding: State VT cols/visibleRows changes → Link::sendResize.
        juce::ValueTree stateRoot { rawPtr->getProcessor().getState().get() };
        clientSessionStateRoots[uuid] = stateRoot;
        stateRoot.addListener (this);

        sessions.emplace (uuid, std::move (termSession));
        result = rawPtr;
    }
    else
    {
        // Standalone or daemon mode — full PTY-backed session.
        // The TTY overload also calls wireSessionCallbacks when Daemon is attached.
        result = &create (cwd, cols, rows, {}, {}, {}, uuid);
    }

    jassert (result != nullptr);
    return *result;
}

/**
 * @brief Removes and destroys the session with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Nexus::remove (const juce::String& uuid)
{
    if (attachedLink != nullptr)
        attachedLink->sendRemove (uuid);

    const auto stateRootIt { clientSessionStateRoots.find (uuid) };

    if (stateRootIt != clientSessionStateRoots.end())
    {
        stateRootIt->second.removeListener (this);
        clientSessionStateRoots.erase (stateRootIt);
    }

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
Terminal::Session& Nexus::get (const juce::String& uuid)
{
    const auto it { sessions.find (uuid) };
    jassert (it != sessions.end());

    Terminal::Session* result { nullptr };

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
 * @brief Attaches a Daemon — stores non-owning pointer for IPC wiring.
 *
 * @note Any thread.
 */
void Nexus::attach (Interprocess::Daemon& daemon)
{
    attachedDaemon = &daemon;
}

/**
 * @brief Attaches a Link — stores non-owning pointer for IPC wiring.
 *
 * @note Any thread.
 */
void Nexus::attach (Interprocess::Link& link)
{
    attachedLink = &link;
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

// =============================================================================

/**
 * @brief Detects cols/visibleRows changes on client-mode session State VTs and
 *        forwards a resize PDU to the daemon via Link.
 *
 * Checks for a PARAM node whose `id` property matches `Terminal::ID::cols` or
 * `Terminal::ID::visibleRows` with a `value` property change.  Matches the
 * tree's root against `clientSessionStateRoots` to find the affected UUID.
 * Reads both dimensions from the parent session root so the PDU always carries
 * the fully-current pair regardless of which property changed first.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Nexus::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Terminal::ID::value
        and tree.getType() == jam::ValueTree::PARAM)
    {
        const juce::String paramId { tree.getProperty (Terminal::ID::id).toString() };
        const bool isColsChange       { paramId == Terminal::ID::cols.toString() };
        const bool isVisibleRowsChange { paramId == Terminal::ID::visibleRows.toString() };

        if (isColsChange or isVisibleRowsChange)
        {
            const juce::ValueTree sessionRoot { tree.getParent() };

            juce::String resizeUuid;

            for (const auto& entry : clientSessionStateRoots)
            {
                if (entry.second == sessionRoot)
                {
                    resizeUuid = entry.first;
                    break;
                }
            }

            if (resizeUuid.isNotEmpty() and attachedLink != nullptr)
            {
                auto colsParam { jam::ValueTree::getChildWithID (sessionRoot, Terminal::ID::cols.toString()) };
                auto rowsParam { jam::ValueTree::getChildWithID (sessionRoot, Terminal::ID::visibleRows.toString()) };

                if (colsParam.isValid() and rowsParam.isValid())
                {
                    const int cols { static_cast<int> (colsParam.getProperty (Terminal::ID::value)) };
                    const int rows { static_cast<int> (rowsParam.getProperty (Terminal::ID::value)) };
                    attachedLink->sendResize (resizeUuid, cols, rows);
                }
            }
        }
    }
}

/**______________________________END OF FILE___________________________________*/
