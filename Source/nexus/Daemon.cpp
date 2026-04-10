/**
 * @file Daemon.cpp
 * @brief Implementation of Nexus::Daemon — JUCE-backed TCP connection server.
 *
 * @see Nexus::Daemon
 * @see Nexus::Session
 * @see Nexus::Channel
 */

#include "Daemon.h"
#include "Session.h"
#include "Channel.h"
#include "../AppIdentifier.h"
#include "../AppState.h"
#include <algorithm>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================
// Platform helpers — Windows

#if JUCE_WINDOWS

/**
 * @brief No-op on Windows — no dock icon exists when no window is created.
 */
void Daemon::hideDockIcon() noexcept
{
    // No-op on Windows — no dock icon exists when no window is created.
}

/**
 * @brief Spawns a detached `end --nexus <uuid>` process via CreateProcessW.
 *
 * Uses `DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP` so the child is fully
 * detached from the parent console/window.
 *
 * @param uuid  Instance UUID for the daemon.
 * @return `true` if CreateProcessW succeeded.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Daemon::spawnDaemon (const juce::String& uuid) noexcept
{
    const auto execPath { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName() };
    const juce::String cmdLine { "\"" + execPath + "\" --nexus " + uuid };

    STARTUPINFOW si {};
    si.cb = sizeof (si);
    PROCESS_INFORMATION pi {};

    const BOOL ok { CreateProcessW (
        nullptr,
        const_cast<LPWSTR> (cmdLine.toWideCharPointer()),
        nullptr,
        nullptr,
        FALSE,
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT,
        nullptr,
        nullptr,
        &si,
        &pi) };

    if (ok != 0)
    {
        CloseHandle (pi.hProcess);
        CloseHandle (pi.hThread);
    }

    return ok != 0;
}

#endif // JUCE_WINDOWS

// =============================================================================

/**
 * @brief Constructs the Daemon with a reference to the owning Session.
 *
 * @param host_  Owning Session — passed through to each Channel.
 */
Daemon::Daemon (Session& host_)
    : host (host_)
{
}

/**
 * @brief Stops the daemon.
 */
Daemon::~Daemon()
{
    stop();
}

// =============================================================================

/**
 * @brief Starts listening and writes the bound port to AppState.
 *
 * Calls `beginWaitingForSocket (port, "127.0.0.1")`.  After a successful
 * bind, `getBoundPort()` returns the actual port (useful when @p port == 0).
 * On success, calls `AppState::getContext()->setPort(activePort)` which
 * persists the port to `~/.config/end/nexus/<uuid>.nexus`.
 *
 * @param port  Preferred port.  0 = OS-assigned.
 * @return `true` if the daemon started listening successfully.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Daemon::start (int port)
{
    const bool listening { beginWaitingForSocket (port, "127.0.0.1") };

    if (listening)
    {
        activePort = getBoundPort();

        if (activePort > 0)
            AppState::getContext()->setPort (activePort);
    }

    return listening;
}

/**
 * @brief Stops the daemon.
 *
 * Calls the base `InterprocessConnectionServer::stop()`.  Nexus state file
 * deletion is handled by AppState::deleteNexusFile() on quit.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::stop()
{
    InterprocessConnectionServer::stop();
    activePort = 0;
}

/**
 * @brief Returns the currently bound port, or 0 if not listening.
 *
 * @return Active port number.
 * @note Any thread.
 */
int Daemon::getPort() const noexcept
{
    return activePort;
}

// =============================================================================

/**
 * @brief Creates and owns a Channel for each accepted client.
 *
 * Called by the base class listener thread when a new client connects.
 * Ownership is retained in `connections`; the base class receives a non-owning
 * raw pointer as required by the InterprocessConnectionServer contract.
 *
 * @return Raw pointer to the newly created Channel (non-owning).
 * @note NEXUS PROCESS LISTENER THREAD — called by juce::InterprocessConnectionServer::run().
 */
juce::InterprocessConnection* Daemon::createConnectionObject()
{
    return connections.add (std::make_unique<Channel> (*this, host)).get();
}

/**
 * @brief Removes and destroys the given Channel.
 *
 * Called from Channel::connectionLost() on the message thread.
 * Locates @p connection by raw pointer in the owner and removes (destroys) it.
 *
 * @param connection  Raw pointer to the connection to destroy.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::removeConnection (Channel* connection)
{
    const int index { connections.indexOf (connection) };

    if (index >= 0)
        connections.remove (index);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
