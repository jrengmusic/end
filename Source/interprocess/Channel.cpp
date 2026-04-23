/**
 * @file Channel.cpp
 * @brief Implementation of Interprocess::Channel — server-side JUCE IPC connection.
 *
 * @see Interprocess::Channel
 * @see Nexus
 */

#include "Channel.h"
#include "Daemon.h"
#include "../nexus/Nexus.h"
#include "EncoderDecoder.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/logic/Session.h"

namespace Interprocess
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Constructs the Channel with message-thread callbacks and custom magic.
 *
 * @param daemon_  Owning Daemon — used in connectionLost() to release this object
 *                 and for all subscriber/broadcast registry operations.
 * @param nexus_   Session pool — used in messageReceived for session lookup and creation.
 */
Channel::Channel (Daemon& daemon_, Nexus& nexus_)
    : juce::InterprocessConnection (true, magicHeader)
    , daemon (daemon_)
    , nexus (nexus_)
{
}

/**
 * @brief Calls `disconnect()` before the vtable is torn down — JUCE contract.
 */
Channel::~Channel()
{
    disconnect();
}

// =============================================================================

/**
 * @brief Called on the message thread when a client connects.
 *
 * Adds this connection to the Daemon broadcast list, then immediately pushes
 * a `sessions` PDU containing the UUIDs of all currently-live sessions.
 * The client does not need to request the list — it arrives unsolicited.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Channel::connectionMade()
{
    daemon.attach (*this);
    daemon.broadcastSessions (*this);
}

/**
 * @brief Called on the message thread when the connection is lost.
 *
 * Removes this connection from the Daemon broadcast list and all per-session
 * subscriber lists.  `daemon.removeConnection` destroys this object last.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Channel::connectionLost()
{
    daemon.detach (*this);
    daemon.removeConnection (this);
}

// =============================================================================

/**
 * @brief Encodes @p kind and @p payload into a MemoryBlock and sends it.
 *
 * Wire format inside the JUCE IPC frame: uint16_t kind (LE) | payload bytes.
 *
 * @param kind     PDU kind identifier.
 * @param payload  Optional payload bytes.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Channel::sendPdu (Message kind, const juce::MemoryBlock& payload)
{
    sendMessage (encodePdu (kind, payload));
}

// =============================================================================

/**
 * @brief Dispatches an incoming message from the client.
 *
 * Decodes `Message` kind from the first 2 bytes of @p message, then handles each
 * PDU kind.  All work runs on the message thread because `callbacksOnMessageThread`
 * is `true`.
 *
 * PDU kinds handled:
 * - `hello`           → sends `helloResponse`
 * - `ping`            → sends `pong`
 * - `createSession`   → decodes cwd/uuid/cols/rows; creates if new via nexus.create,
 *                       subscribes+sends history, then broadcastSessions
 * - `input`           → decodes uuid + bytes, calls nexus.get(uuid).sendInput
 * - `resizeSession`   → decodes uuid + cols + rows, resizes Processor then PTY
 * - `detachSession`   → unregisters subscription via daemon.detachSession
 * - `killSession`     → calls nexus.remove(uuid)
 * - `killDaemon`     → calls daemon.killAll() to shut down the daemon process
 *
 * @param message  Raw MemoryBlock received from the JUCE IPC layer.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Channel::messageReceived (const juce::MemoryBlock& message)
{
    const auto total { static_cast<int> (message.getSize()) };

    if (total >= 2)
    {
        const auto* data { static_cast<const uint8_t*> (message.getData()) };
        uint16_t rawKind { 0 };
        std::memcpy (&rawKind, data, sizeof (rawKind));

        const auto kind { static_cast<Message> (rawKind) };
        const uint8_t* payload { data + 2 };
        const int payloadSize { total - 2 };

        switch (kind)
        {
            case Message::hello:
            {
                sendPdu (Message::helloResponse);
                break;
            }

            case Message::ping:
            {
                sendPdu (Message::pong);
                break;
            }

            case Message::createSession:
            {
                const auto parsed { parseSpawnPayload (payload, payloadSize) };

                if (parsed.valid)
                {
                    const bool exists { nexus.has (parsed.uuid) };

                    if (not exists)
                        nexus.create (parsed.cwd, parsed.cols, parsed.rows, {}, {}, {}, parsed.uuid);

                    // Subscribe, send history (rebuilds terminal state: alt screen, cursor, etc.),
                    // then resize PTY. SIGWINCH redraw overwrites any dim-garbled history output.
                    daemon.attachSession (parsed.uuid, *this, true, parsed.cols, parsed.rows);

                    daemon.broadcastSessions();
                }

                break;
            }

            case Message::input:
            {
                // Payload: uuid | raw input bytes
                juce::String uuid;
                const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty())
                {
                    const int inputLen { payloadSize - uuidConsumed };

                    if (inputLen > 0 and nexus.has (uuid))
                    {
                        nexus.get (uuid).sendInput (
                            reinterpret_cast<const char*> (payload + uuidConsumed),
                            inputLen);
                    }
                }

                break;
            }

            case Message::resizeSession:
            {
                // Payload: uuid | cols (uint16_t LE) | rows (uint16_t LE)
                juce::String uuid;
                const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty() and (payloadSize - uuidConsumed) >= 4)
                {
                    const uint16_t cols { Codec::readUint16 (payload + uuidConsumed) };
                    const uint16_t rows { Codec::readUint16 (payload + uuidConsumed + 2) };

                    if (nexus.has (uuid))
                    {
                        // Grid pipeline side resize (stub processor on daemon).
                        nexus.get (uuid).getProcessor().resized (static_cast<int> (cols), static_cast<int> (rows));
                        // PTY side resize.
                        nexus.get (uuid).resize (static_cast<int> (cols), static_cast<int> (rows));
                    }
                }

                break;
            }

            case Message::detachSession:
            {
                // Payload: uuid (length-prefixed)
                juce::String uuid;
                const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty())
                    daemon.detachSession (uuid, *this);

                break;
            }

            case Message::killSession:
            {
                // Payload: uuid (length-prefixed)
                juce::String uuid;
                const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty())
                {
                    nexus.remove (uuid);
                    daemon.broadcastSessions();

                    if (nexus.list().isEmpty() and daemon.onAllSessionsExited != nullptr)
                        daemon.onAllSessionsExited();
                }

                break;
            }

            case Message::killDaemon:
            {
                daemon.killAll();
                break;
            }

            default:
                break;
        }
    }
}

// =============================================================================

Channel::SpawnPayload
Channel::parseSpawnPayload (const uint8_t* payload, int payloadSize)
{
    SpawnPayload out;
    int cursor { 0 };

    const int cwdConsumed { Codec::readString (payload + cursor, payloadSize - cursor, out.cwd) };
    cursor += cwdConsumed;

    const int uuidConsumed { Codec::readString (payload + cursor, payloadSize - cursor, out.uuid) };
    cursor += uuidConsumed;

    if (cwdConsumed > 0 and uuidConsumed > 0 and (payloadSize - cursor) >= 4)
    {
        out.cols = static_cast<int> (Codec::readUint16 (payload + cursor));
        cursor += 2;
        out.rows = static_cast<int> (Codec::readUint16 (payload + cursor));

        out.valid = true;
    }

    return out;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess
