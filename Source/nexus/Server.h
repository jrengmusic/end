/**
 * @file Server.h
 * @brief JUCE-backed TCP server that accepts Nexus client connections.
 *
 * `Nexus::Server` wraps `juce::InterprocessConnectionServer` to listen on a
 * TCP port, write the bound port to AppState (which persists it to
 * `~/.config/end/nexus/<uuid>.nexus`), and create a `Nexus::ServerConnection`
 * for each accepted client.
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
 * the bound port to AppState (persisted to the nexus state file), and caches
 * it in `activePort`.  `stop()` calls the base `InterprocessConnectionServer::stop()`.
 * Nexus port file deletion is handled by AppState::deleteNexusFile() on quit.
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
     * @brief Starts listening on @p port and writes the bound port to AppState.
     *
     * Tries @p port first.  If @p port is 0, `beginWaitingForSocket` returns the
     * OS-assigned port via `getBoundPort()`.  On success, calls
     * `AppState::getContext()->setPort(activePort)` which persists the port to
     * `~/.config/end/nexus/<uuid>.nexus` so clients can probe it.
     *
     * @param port  Port to try.  0 = let OS choose.
     * @return `true` if `beginWaitingForSocket` succeeded.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    bool start (int port = defaultPort);

    /**
     * @brief Stops the server.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void stop();

    /**
     * @brief Returns the port currently bound, or 0 if not listening.
     * @note Any thread.
     */
    int getPort() const noexcept;

    /**
     * @brief Returns the nexus port file (`~/.config/end/nexus/<uuid>.nexus`).
     *
     * The nexus port file IS the lockfile — it contains the plain-text port number
     * so client startup scans can discover a running daemon.
     *
     * @return Path to the nexus port file.
     * @note Any thread.
     */
    static juce::File getLockfile();

    /**
     * @brief Persists @p port to the nexus port file via AppState.
     *
     * Delegates to `AppState::getContext()->setPort(port)` which writes the
     * plain-text port number to `<uuid>.nexus` immediately.
     *
     * @param port  The bound TCP port to persist.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void writeLockfile (int port);

    /**
     * @brief Deletes the nexus port file via AppState.
     *
     * Delegates to `AppState::getContext()->deleteNexusFile()`.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void deleteLockfile();

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

private:
    juce::InterprocessConnection* createConnectionObject() override;

    Session& host;
    int activePort { 0 };

    /** @brief Owns all live ServerConnection objects. */
    jreng::Owner<ServerConnection> connections;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Server)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
