/**
 * @file Client.h
 * @brief Client-side JUCE IPC connector to a remote Nexus::Session.
 *
 * `Nexus::Client` connects to a running Host via TCP, performs the hello
 * handshake, and provides the send API for all client-to-host PDU kinds.
 * Incoming PDUs are delivered to the message thread by JUCE and dispatched
 * to registered `Terminal::Processor` objects.
 *
 * ### Lifecycle
 * 1. Construct with default constructor.
 * 2. Call `connectToHost()` — reads port from lockfile, connects socket, performs
 *    hello handshake.  Returns `true` on success.
 * 3. For ongoing delta requests: register hosted Processor objects via
 *    `registerProcessor()`.
 * 4. Call `disconnectFromHost()` on shutdown.  Destructor also calls it.
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
 * @see Terminal::Processor
 */

#pragma once
#include <juce_events/juce_events.h>
#include "Message.h"
#include "Wire.h"
#include <memory>
#include <map>

namespace Terminal { class Processor; }

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
     * @brief Reads port from lockfile and connects to host.
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
     * @brief Returns a snapshot of all live processor UUIDs known to this client.
     *
     * Populated by unsolicited `Message::processorList` pushes from the host.
     * Non-blocking — returns whatever the last push delivered.
     *
     * @return Snapshot of processor UUID strings.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    juce::StringArray getProcessorList() const;

    /**
     * @brief Requests the host to spawn a new PTY session.
     *
     * Payload: shell | args | cwd | uuid | cols (uint16_t LE) | rows (uint16_t LE)
     *
     * @param cols    Initial column count.
     * @param rows    Initial row count.
     * @param shell   Shell program path.  Empty = host default.
     * @param cwd     Initial working directory.  Empty = inherit.
     * @param uuid    UUID hint for the new session.  Empty = host generates one.
     * @note Any thread.
     */
    void spawnSession (int cols, int rows,
                       const juce::String& shell,
                       const juce::String& cwd,
                       const juce::String& uuid = {});

    /**
     * @brief Subscribes to render deltas for a session.
     *
     * @param uuid  UUID of the session to attach.
     * @note Any thread.
     */
    void attachSession (const juce::String& uuid);

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
     * @brief Takes ownership of @p processor and registers it to receive incoming
     *        `Message::output` / `Message::history` PDUs for its UUID.
     *
     * The UUID is read from `processor->uuid`.  Ownership transfers to Client;
     * the Processor is destroyed when `unregisterProcessor` is called or when
     * Client is destroyed.  Must be called before `attachSession()` so that the
     * first output/history push is not missed.
     *
     * @param processor  Owning pointer to the Processor.  Must not be null.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void registerProcessor (std::unique_ptr<Terminal::Processor> processor);

    /**
     * @brief Removes and destroys the Processor registered for @p uuid.
     *
     * The Display referencing this Processor must be destroyed before this call.
     * Panes calls this from `teardownTerminal()` after the Display is erased.
     *
     * @param uuid  UUID to unregister and destroy.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void unregisterProcessor (const juce::String& uuid);

    /**
     * @brief Returns a non-owning pointer to the Processor registered for @p uuid.
     *
     * Returns nullptr if @p uuid is not registered.  The pointer is valid until
     * `unregisterProcessor` is called for the same uuid.
     *
     * @param uuid  UUID to look up.
     * @return Non-owning pointer, or nullptr.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor* getProcessor (const juce::String& uuid) const;

    /**
     * @brief Callback fired on the message thread for every incoming host PDU.
     *
     * Receives all async frames: RenderDelta, AttachSessionResponse,
     * SpawnSessionResponse, SessionExited, etc.
     *
     * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
     */
    std::function<void (const Message kind, const juce::MemoryBlock& payload)> onPdu;

    /**
     * @brief Fired on the message thread when the async connection attempt succeeds.
     *
     * Set by `ENDApplication` before calling `beginConnectAttempts()`.
     * Called from `connectionMade()` once the socket handshake completes.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    std::function<void()> onConnectionMade;

    /**
     * @brief Fired on the message thread when all retry attempts are exhausted.
     *
     * Set by `ENDApplication` before calling `beginConnectAttempts()`.
     * Called when the 5-second retry window expires without a successful connect.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    std::function<void()> onConnectionFailed;

    /**
     * @brief Kicks off async connection attempts, polling the lockfile every 100 ms.
     *
     * Reads @p lockfilePath, parses the port, and attempts `connectToSocket`.
     * On success, stops retrying — JUCE fires `connectionMade()` which invokes
     * `onConnectionMade`.  On timeout (50 × 100 ms = 5 s), fires `onConnectionFailed`.
     *
     * @param lockfilePath  Path to the port lockfile written by the daemon.
     * @note NEXUS PROCESS MESSAGE THREAD — must be called on the message thread.
     */
    void beginConnectAttempts (const juce::File& lockfilePath) noexcept;

private:
    void connectionMade() override;
    void connectionLost() override;
    void messageReceived (const juce::MemoryBlock& message) override;

    void sendPdu (Message kind, const juce::MemoryBlock& payload = {});

    static juce::File getLockfile();

    /**
     * @brief Live processor UUID list populated by unsolicited `Message::processorList` pushes.
     *
     * All access on the message thread.
     */
    juce::StringArray processorUuids;

    /** @brief UUIDs of sessions this client has attached to (for DetachSession on disconnect). */
    juce::StringArray attachedUuids;

    /** @brief Guards attachedUuids for cross-thread access. */
    juce::CriticalSection attachedUuidsLock;

    /**
     * @brief Maps session UUID → owned Terminal::Processor for output/history PDU routing.
     *
     * Client owns the Processor lifetime.  All access on the message thread.
     * Processors are inserted by registerProcessor and destroyed by unregisterProcessor
     * or when Client itself is destroyed (before JUCE leak detector runs).
     */
    std::map<juce::String, std::unique_ptr<Terminal::Processor>> hostedProcessors;

    /**
     * @brief Inner timer that drives async connect retries.
     *
     * Fires every 100 ms on the JUCE message thread.  On each tick it reads the
     * lockfile, parses the port, and calls `connectToSocket`.  Stops itself when
     * the connection succeeds or when `maxAttempts` is reached.
     *
     * @note All callbacks — NEXUS PROCESS MESSAGE THREAD.
     */
    struct ConnectTimer : public juce::Timer
    {
        Client&     owner;
        juce::File  lockfile;
        int         attemptsRemaining { 0 };

        ConnectTimer (Client& ownerIn, const juce::File& lockfileIn, int maxAttempts) noexcept
            : owner (ownerIn)
            , lockfile (lockfileIn)
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
