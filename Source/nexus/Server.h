/**
 * @file Server.h
 * @brief JUCE-backed TCP server that accepts Nexus client connections.
 *
 * `Nexus::Server` wraps `juce::InterprocessConnectionServer` to listen on a
 * TCP port, write the bound port to `~/.config/end/end.port`, and create a
 * `Nexus::ServerConnection` for each accepted client.
 *
 * @see Nexus::Session
 * @see Nexus::ServerConnection
 */

#pragma once
#include <juce_events/juce_events.h>
#include <jreng_core/jreng_core.h>

namespace Nexus
{
/*____________________________________________________________________________*/

class Session;
class ServerConnection;

/**
 * @class Nexus::Server
 * @brief InterprocessConnectionServer that creates ServerConnection objects.
 *
 * Owned by `Nexus::Session`.  `start()` calls `beginWaitingForSocket()`, writes
 * the bound port to the lockfile, and caches it in `activePort`.  `stop()`
 * calls the base `InterprocessConnectionServer::stop()` and deletes the
 * lockfile.
 *
 * @par Thread context
 * `start()` / `stop()` — NEXUS PROCESS MESSAGE THREAD.
 * `createConnectionObject()` — NEXUS PROCESS LISTENER THREAD (called by base class).
 *
 * @see juce::InterprocessConnectionServer
 */
class Server : public juce::InterprocessConnectionServer
{
public:
    /** @brief Default port: 0 instructs the OS to assign a free ephemeral port. */
    static constexpr int defaultPort { 0 };

    explicit Server (Session& host);
    ~Server() override;

    /**
     * @brief Starts listening on @p port and writes the bound port to lockfile.
     *
     * Tries @p port first.  If @p port is 0, `beginWaitingForSocket` returns the
     * OS-assigned port via `getBoundPort()`.
     *
     * @param port  Port to try.  0 = let OS choose.
     * @return `true` if `beginWaitingForSocket` succeeded.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    bool start (int port = defaultPort);

    /**
     * @brief Stops the server and deletes the lockfile.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void stop();

    /**
     * @brief Returns the port currently bound, or 0 if not listening.
     * @note Any thread.
     */
    int getPort() const noexcept;

    /**
     * @brief Removes and destroys the connection owned by this server.
     *
     * Called from ServerConnection::connectionLost() on the message thread.
     * Triggers destruction of the ServerConnection.
     *
     * @param connection  Raw pointer to the connection to remove.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void removeConnection (ServerConnection* connection);

    /**
     * @brief Returns the path to the port lockfile: `~/.config/end/end.port`.
     *
     * Single source of truth for the lockfile path — all callers use this method.
     *
     * @note Any thread.
     */
    static juce::File getLockfile();

private:
    juce::InterprocessConnection* createConnectionObject() override;

    static void writeLockfile (int port);
    static void deleteLockfile();

    Session& host;
    int activePort { 0 };

    /** @brief Owns all live ServerConnection objects. */
    jreng::Owner<ServerConnection> connections;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Server)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
