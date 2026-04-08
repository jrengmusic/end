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
#include "Log.h"
#include <algorithm>

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Returns the path to the port lockfile: `~/.config/end/end.port`.
 */
juce::File Server::getLockfile()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile (".config/end/end.port");
}

/**
 * @brief Writes @p port as a decimal string to the lockfile.
 *
 * Creates `~/.config/end/` if it does not exist.
 *
 * @param port  Bound TCP port number.
 */
void Server::writeLockfile (int port)
{
    const juce::File lockfile { getLockfile() };
    lockfile.getParentDirectory().createDirectory();
    lockfile.replaceWithText (juce::String (port));
}

/**
 * @brief Deletes the lockfile if it exists.
 */
void Server::deleteLockfile()
{
    getLockfile().deleteFile();
}

// =============================================================================

/**
 * @brief Constructs the Server with a reference to the owning Host.
 *
 * @param host_  Owning Session — passed through to each ServerConnection.
 */
Server::Server (Session& host_)
    : host (host_)
{
}

/**
 * @brief Stops the server and removes the lockfile.
 */
Server::~Server()
{
    stop();
}

// =============================================================================

/**
 * @brief Starts listening and writes the bound port to the lockfile.
 *
 * Calls `beginWaitingForSocket (port, "127.0.0.1")`.  After a successful
 * bind, `getBoundPort()` returns the actual port (useful when @p port == 0).
 *
 * @param port  Preferred port.  0 = OS-assigned.
 * @return `true` if the server started listening successfully.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Server::start (int port)
{
    Nexus::logLine ("Server::start: entry port=" + juce::String (port));
    Nexus::logLine ("Server::start: calling beginWaitingForSocket");
    const bool listening { beginWaitingForSocket (port, "127.0.0.1") };
    Nexus::logLine ("Server::start: beginWaitingForSocket returned result=" + juce::String (listening ? 1 : 0));

    if (listening)
    {
        activePort = getBoundPort();

        if (activePort > 0)
        {
            const juce::String lockfilePath { getLockfile().getFullPathName() };
            Nexus::logLine ("Server::start: writing lockfile at " + lockfilePath);
            writeLockfile (activePort);
            Nexus::logLine ("Server::start: lockfile written port=" + juce::String (activePort));
        }
        else
        {
            Nexus::logLine ("Server::start: listening but getBoundPort returned 0 - lockfile NOT written");
        }
    }
    else
    {
        Nexus::logLine ("Server::start: beginWaitingForSocket failed - not listening");
    }

    Nexus::logLine ("Server::start: exit listening=" + juce::String (listening ? 1 : 0));
    return listening;
}

/**
 * @brief Stops the server and deletes the lockfile.
 *
 * Calls the base `InterprocessConnectionServer::stop()` then removes the port
 * file.  Safe to call when not listening.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Server::stop()
{
    InterprocessConnectionServer::stop();
    deleteLockfile();
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
