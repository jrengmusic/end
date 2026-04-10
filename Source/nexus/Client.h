/**
 * @file Client.h
 * @brief Client-side JUCE IPC connector to a remote Nexus::Session.
 *
 * `Nexus::Client` connects to a running daemon via TCP, performs the hello
 * handshake, and provides the send API for all client-to-host PDU kinds.
 * Incoming PDUs are delivered to the message thread by JUCE and dispatched
 * to `Nexus::Session` for routing to the appropriate `Terminal::Session`.
 *
 * ### Lifecycle
 * 1. Construct with default constructor.
 * 2. Call `beginConnectAttempts()` — polls AppState for the daemon port every
 *    100 ms and calls `connectToSocket` on each tick.  On success JUCE fires
 *    `connectionMade()` which sends the hello handshake.
 * 3. Call `disconnectFromHost()` on shutdown.  Destructor also calls it.
 *
 * ### Port resolution
 * The daemon port is read from `AppState::getContext()->getPort()`.  The port
 * is written to AppState by `Server::start()` and persisted to
 * `~/.config/end/nexus/<uuid>.nexus`.  During the startup scan in Main.cpp
 * the nexus file is parsed and the port is loaded into AppState before
 * `beginConnectAttempts()` is called.
 *
 * ### Threading
 * JUCE delivers `connectionMade`, `connectionLost`, and `messageReceived` on
 * the message thread (`callbacksOnMessageThread = true`).  All dispatch in
 * `messageReceived` executes directly on the message thread — no `callAsync`
 * indirection.
 *
 * ### JUCE contract
 * Derived classes MUST call `disconnect()` in their own destructor.
 *
 * ### Access
 * Client is owned privately by `Nexus::Session` in client mode.  All external
 * callers reach session operations via `Nexus::Session::getContext()`.
 *
 * @see Nexus::Session
 * @see Terminal::Session
 */

#pragma once
#include <juce_events/juce_events.h>
#include "Message.h"
#include "Wire.h"
#include <memory>

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @class Nexus::Client
 * @brief JUCE IPC client connector to a remote Nexus::Session.
 *
 * Owned privately by `Nexus::Session` in client mode.  No external caller
 * instantiates or holds a pointer to Client directly.
 *
 * @par Thread context
 * `connectToHost()` / `disconnectFromHost()` — NEXUS PROCESS MESSAGE THREAD.
 * `messageReceived()` — NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 * `send*()` methods — any thread (sendMessage is thread-safe).
 */
class Client : public juce::InterprocessConnection
{
public:
    /** @brief Magic header — single source of truth is Nexus::wireMagicHeader in Wire.h. */
    static constexpr juce::uint32 magicHeader { wireMagicHeader };

    /** @brief Timeout for initial connection attempt in milliseconds. */
    static constexpr int connectTimeoutMs { 3000 };

    Client();

    /**
     * @brief Calls `disconnect()` — JUCE contract.
     */
    ~Client() override;

    /**
     * @brief Reads port from AppState and connects to host.
     *
     * @return `true` if the connection succeeded.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    bool connectToHost();

    /**
     * @brief Disconnects from host.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void disconnectFromHost();

    /**
     * @brief Unsubscribes from render deltas for a session.
     *
     * @param uuid  UUID of the session to detach.
     * @note Any thread.
     */
    void detachSession (const juce::String& uuid);

    /**
     * @brief Forwards raw input bytes to the shell in a session.
     *
     * @param uuid  UUID of the target session.
     * @param data  Pointer to the bytes.
     * @param size  Number of bytes.
     * @note Any thread.
     */
    void sendInput (const juce::String& uuid, const void* data, int size);

    /**
     * @brief Notifies the host of a terminal resize.
     *
     * @param uuid  UUID of the session to resize.
     * @param cols  New column count.
     * @param rows  New row count.
     * @note Any thread.
     */
    void sendResize (const juce::String& uuid, int cols, int rows);

    /**
     * @brief Requests the host to kill the shell for a session.
     *
     * Sent before local processor cleanup so the socket is still live when the
     * PDU is written.  Triggers the daemon-side Session::remove which destroys
     * the Terminal::Session and its PTY process.
     *
     * @param uuid  UUID of the session to destroy.
     * @note Any thread.
     */
    void sendRemove (const juce::String& uuid);

    /**
     * @brief Encodes @p kind and @p payload, then calls sendMessage().
     *
     * Wire format: uint16_t kind (LE) | payload bytes.
     *
     * @note Any thread.
     */
    void sendPdu (Message kind, const juce::MemoryBlock& payload = {});


    /**
     * @brief Callback fired on the message thread for every incoming host PDU.
     *
     * Receives all async frames: helloResponse, processorExited, etc.
     *
     * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
     */
    std::function<void (const Message kind, const juce::MemoryBlock& payload)> onPdu;

    /**
     * @brief Kicks off async connection attempts, polling AppState for the port every 100 ms.
     *
     * Reads `AppState::getContext()->getPort()` on each tick and attempts
     * `connectToSocket`.  On success, stops retrying — JUCE fires `connectionMade()`.
     * On timeout (50 × 100 ms = 5 s), logs a failure line.
     *
     * @note NEXUS PROCESS MESSAGE THREAD — must be called on the message thread.
     */
    void beginConnectAttempts() noexcept;

private:
    void connectionMade() override;
    void connectionLost() override;
    void messageReceived (const juce::MemoryBlock& message) override;

    void handleProcessorList      (const uint8_t* payload, int payloadSize);
    void handleProcessorExited    (const uint8_t* payload, int payloadSize);
    void handleOutput             (const uint8_t* payload, int payloadSize);
    void handleLoading            (const uint8_t* payload, int payloadSize);
    void handleStateUpdate        (const uint8_t* payload, int payloadSize);
    void handleUnknown            (Message kind, const uint8_t* payload, int payloadSize);

    /**
     * @brief Inner timer that drives async connect retries.
     *
     * Fires every 100 ms on the JUCE message thread.  On each tick it reads
     * the port from AppState and calls `connectToSocket`.  Stops itself when
     * the connection succeeds or when `maxAttempts` is reached.
     *
     * @note All callbacks — NEXUS PROCESS MESSAGE THREAD.
     */
    struct ConnectTimer : public juce::Timer
    {
        Client& owner;
        int     attemptsRemaining { 0 };

        ConnectTimer (Client& ownerIn, int maxAttempts) noexcept
            : owner (ownerIn)
            , attemptsRemaining (maxAttempts)
        {
        }

        void timerCallback() override;
    };

    /** @brief Active retry timer; non-null only between `beginConnectAttempts()` and resolution. */
    std::unique_ptr<ConnectTimer> connectTimer;

    /** @brief Maximum number of 100 ms retry ticks (50 × 100 ms = 5 s). */
    static constexpr int connectMaxAttempts { 50 };

    /** @brief Interval between retry ticks in milliseconds. */
    static constexpr int connectRetryIntervalMs { 100 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Client)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
