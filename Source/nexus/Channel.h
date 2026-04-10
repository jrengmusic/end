/**
 * @file Channel.h
 * @brief One accepted client connection to the Nexus host.
 *
 * `Nexus::Channel` is the server-side half of a JUCE IPC connection.
 * It is created by `Nexus::Daemon::createConnectionObject()` and its lifetime
 * is managed by the `juce::InterprocessConnectionServer` base.
 *
 * ### Thread model
 * JUCE's `InterprocessConnection` delivers all three callbacks
 * (`connectionMade`, `connectionLost`, `messageReceived`) on the message
 * thread when constructed with `callbacksOnMessageThread = true`.
 *
 * ### JUCE contract
 * Derived classes MUST call `disconnect()` in their own destructor to cancel
 * any pending message delivery before the vtable is torn down.
 *
 * @see Nexus::Daemon
 * @see Nexus::Session
 * @see Nexus::Message
 */

#pragma once
#include <juce_events/juce_events.h>
#include "Message.h"
#include "Wire.h"

namespace Nexus
{
/*____________________________________________________________________________*/

class Daemon;
class Session;

/**
 * @class Nexus::Channel
 * @brief Message-thread IPC connection representing one connected Nexus client.
 *
 * Created and owned by `Nexus::Daemon::connections` (a `jreng::Owner<Channel>`).
 * The base `InterprocessConnectionServer` receives a non-owning raw pointer.
 * Registered with `Session::attach()` in `connectionMade()` and cleaned up via
 * `Session::detach()` + `Daemon::removeConnection()` in `connectionLost()`.
 * All PDU dispatch happens on the message thread.
 *
 * @par Thread context
 * All callbacks — NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 * `sendPdu()` — NEXUS PROCESS MESSAGE THREAD (called from dispatch / fanout).
 */
class Channel : public juce::InterprocessConnection
{
public:
    /** @brief Magic header — single source of truth is Nexus::wireMagicHeader in Wire.h. */
    static constexpr juce::uint32 magicHeader { wireMagicHeader };

    Channel (Daemon& daemon, Session& session);

    /**
     * @brief Disconnects before the vtable is torn down — JUCE contract.
     */
    ~Channel() override;

    void connectionMade() override;
    void connectionLost() override;
    void messageReceived (const juce::MemoryBlock& message) override;

    /**
     * @brief Encodes @p kind + @p payload into a MemoryBlock and calls sendMessage().
     *
     * Wire format: uint16_t kind (LE) | payload bytes.
     *
     * @param kind     PDU kind identifier.
     * @param payload  Optional payload bytes.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void sendPdu (Message kind, const juce::MemoryBlock& payload = {});

    /**
     * @brief Returns the stable UUID string that identifies this connection in subscriber maps.
     * @note Any thread — `id` is const, no synchronization needed.
     */
    juce::StringRef getId() const noexcept { return id; }

    /** @brief Stable UUID assigned at construction — unique for the lifetime of this connection. */
    const juce::String id { juce::Uuid().toString() };

private:
    struct SpawnPayload
    {
        juce::String cwd;
        juce::String uuid;
        int cols { 0 };
        int rows { 0 };
        bool valid { false };
    };

    static SpawnPayload parseSpawnPayload (const uint8_t* payload, int payloadSize);

    Daemon&  daemon;
    Session& session;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Channel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
