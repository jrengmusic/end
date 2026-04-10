/**
 * @file Server.cpp
 * @brief Implementation of Nexus::Server — JUCE-backed TCP connection server.
 *
 * @see Nexus::Server
 * @see Nexus::Session
 * @see Nexus::ServerConnection
 */

#include "Server.h"
#include "Session.h"
#include "ServerConnection.h"
#include "../AppIdentifier.h"
#include "../AppState.h"
#include <algorithm>

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Constructs the Server with a reference to the owning Session.
 *
 * @param host_  Owning Session — passed through to each ServerConnection.
 */
Server::Server (Session& host_)
    : host (host_)
{
}

/**
 * @brief Stops the server.
 */
Server::~Server()
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
 * @return `true` if the server started listening successfully.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Server::start (int port)
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
 * @brief Stops the server.
 *
 * Calls the base `InterprocessConnectionServer::stop()`.  Nexus state file
 * deletion is handled by AppState::deleteNexusFile() on quit.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Server::stop()
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
int Server::getPort() const noexcept
{
    return activePort;
}

// =============================================================================

/**
 * @brief Returns the nexus port file (`~/.config/end/nexus/<uuid>.nexus`).
 *
 * The nexus port file IS the lockfile — it contains the plain-text port number
 * so client startup scans can discover a running daemon.
 *
 * @return Path to the nexus port file.
 * @note Any thread.
 */
juce::File Server::getLockfile()
{
    return AppState::getContext()->getNexusFile();
}

/**
 * @brief Persists @p port to the nexus state file via AppState.
 *
 * Delegates to `AppState::getContext()->setPort(port)` which writes the
 * nexus file immediately.
 *
 * @param port  The bound TCP port to persist.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Server::writeLockfile (int port)
{
    AppState::getContext()->setPort (port);
}

/**
 * @brief Deletes the nexus port file via AppState.
 *
 * Delegates to `AppState::getContext()->deleteNexusFile()`.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Server::deleteLockfile()
{
    AppState::getContext()->deleteNexusFile();
}

// =============================================================================

/**
 * @brief Creates and owns a ServerConnection for each accepted client.
 *
 * Called by the base class listener thread when a new client connects.
 * Ownership is retained in `connections`; the base class receives a non-owning
 * raw pointer as required by the InterprocessConnectionServer contract.
 *
 * @return Raw pointer to the newly created ServerConnection (non-owning).
 * @note NEXUS PROCESS LISTENER THREAD — called by juce::InterprocessConnectionServer::run().
 */
juce::InterprocessConnection* Server::createConnectionObject()
{
    return connections.add (std::make_unique<ServerConnection> (*this, host)).get();
}

/**
 * @brief Removes and destroys the given ServerConnection.
 *
 * Called from ServerConnection::connectionLost() on the message thread.
 * Locates @p connection by raw pointer in the owner and removes (destroys) it.
 *
 * @param connection  Raw pointer to the connection to destroy.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Server::removeConnection (ServerConnection* connection)
{
    const int index { connections.indexOf (connection) };

    if (index >= 0)
        connections.remove (index);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
