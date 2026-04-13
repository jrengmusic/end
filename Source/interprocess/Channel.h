/**
 * @file Channel.h
 * @brief One accepted client connection to the Interprocess host.
 *
 * `Interprocess::Channel` is the server-side half of a JUCE IPC connection.
 * It is created by `Interprocess::Daemon::createConnectionObject()` and its lifetime
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
 * @see Interprocess::Daemon
 * @see Nexus
 * @see Interprocess::Message
 */

#pragma once
#include <juce_events/juce_events.h>
#include "Message.h"
#include "EncoderDecoder.h"

class Nexus;

namespace Interprocess
{
/*____________________________________________________________________________*/

class Daemon;

/**
 * @class Interprocess::Channel
 * @brief Message-thread IPC connection representing one connected Interprocess client.
 *
 * Created and owned by `Interprocess::Daemon::connections` (a `jreng::Owner<Channel>`).
 * The base `InterprocessConnectionServer` receives a non-owning raw pointer.
 * Registered with `Daemon::attach()` in `connectionMade()` and cleaned up via
 * `Daemon::detach()` + `Daemon::removeConnection()` in `connectionLost()`.
 * All PDU dispatch happens on the message thread.
 *
 * @par Thread context
 * All callbacks — NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 * `sendPdu()` — NEXUS PROCESS MESSAGE THREAD (called from dispatch / fanout).
 */
class Channel : public juce::InterprocessConnection
{
public:
    /** @brief Magic header — single source of truth is Interprocess::wireMagicHeader in EncoderDecoder.h. */
    static constexpr juce::uint32 magicHeader { wireMagicHeader };

    Channel (Daemon& daemon, Nexus& nexus);

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

    Daemon& daemon;
    Nexus&  nexus;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Channel)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess
