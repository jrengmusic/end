/**
 * @file Channel.cpp
 * @brief Implementation of Nexus::Channel — server-side JUCE IPC connection.
 *
 * @see Nexus::Channel
 * @see Nexus::Session
 */

#include "Channel.h"
#include "Daemon.h"
#include "Session.h"
#include "Wire.h"
#include "../terminal/logic/Processor.h"

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Constructs the Channel with message-thread callbacks and custom magic.
 *
 * @param daemon_   Owning Daemon — used in connectionLost() to release this object.
 * @param session_  Session pool — used in connectionMade/Lost and messageReceived.
 */
Channel::Channel (Daemon& daemon_, Session& session_)
    : juce::InterprocessConnection (true, magicHeader)
    , daemon (daemon_)
    , session (session_)
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
 * Adds this connection to the Session broadcast list, then immediately pushes
 * a `sessions` PDU containing the UUIDs of all currently-live Processors.
 * The client does not need to request the list — it arrives unsolicited.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Channel::connectionMade()
{
    session.attach (*this);
    session.broadcastSessions (*this);
}

/**
 * @brief Called on the message thread when the connection is lost.
 *
 * Removes this connection from the Session broadcast list and all per-processor
 * subscriber lists.  `daemon.removeConnection` destroys this object last.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Channel::connectionLost()
{
    session.detach (*this);
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
 * - `hello`            → sends `helloResponse`
 * - `ping`             → sends `pong`
 * - `createSession`  → decodes shell/args/cwd/uuid/cols/rows; creates if new,
 *                      resizes+attaches if existing, then broadcastSessions
 * - `input`          → decodes uuid + bytes, calls session.sendInput
 * - `resizeSession`  → decodes uuid + cols + rows, calls session.sendResize
 * - `detachSession`  → unregisters subscription
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
                    const bool exists { session.hasSession (parsed.uuid) };

                    if (not exists)
                        session.openTerminal (parsed.cwd, parsed.uuid, parsed.cols, parsed.rows);

                    // Subscribe, send history (rebuilds terminal state: alt screen, cursor, etc.),
                    // then resize PTY. SIGWINCH redraw overwrites any dim-garbled history output.
                    session.attach (parsed.uuid, *this, true, parsed.cols, parsed.rows);

                    session.broadcastSessions();
                }

                break;
            }

            case Message::input:
            {
                // Payload: uuid | raw input bytes
                juce::String uuid;
                const int uuidConsumed { readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty())
                {
                    const int inputLen { payloadSize - uuidConsumed };

                    if (inputLen > 0)
                    {
                        session.sendInput (uuid,
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
                const int uuidConsumed { readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty() and (payloadSize - uuidConsumed) >= 4)
                {
                    const uint16_t cols { readUint16 (payload + uuidConsumed) };
                    const uint16_t rows { readUint16 (payload + uuidConsumed + 2) };
                    // Grid pipeline side resize (stub processor on daemon).
                    session.get (uuid).resized (static_cast<int> (cols), static_cast<int> (rows));
                    // PTY side resize.
                    session.sendResize (uuid, static_cast<int> (cols), static_cast<int> (rows));
                }

                break;
            }

            case Message::detachSession:
            {
                // Payload: uuid (length-prefixed)
                juce::String uuid;
                const int uuidConsumed { readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty())
                    session.detachConnection (uuid, *this);

                break;
            }

            case Message::killSession:
            {
                // Payload: uuid (length-prefixed)
                juce::String uuid;
                const int uuidConsumed { readString (payload, payloadSize, uuid) };

                if (uuidConsumed > 0 and uuid.isNotEmpty())
                    session.remove (uuid);

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

    const int cwdConsumed { readString (payload + cursor, payloadSize - cursor, out.cwd) };
    cursor += cwdConsumed;

    const int uuidConsumed { readString (payload + cursor, payloadSize - cursor, out.uuid) };
    cursor += uuidConsumed;

    if (cwdConsumed > 0 and uuidConsumed > 0 and (payloadSize - cursor) >= 4)
    {
        out.cols = static_cast<int> (readUint16 (payload + cursor));
        cursor += 2;
        out.rows = static_cast<int> (readUint16 (payload + cursor));

        out.valid = true;
    }

    return out;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
