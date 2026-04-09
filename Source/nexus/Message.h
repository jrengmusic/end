/**
 * @file Message.h
 * @brief Nexus protocol message types.
 *
 * Defines the message kind enumeration used by ServerConnection and Client to
 * identify messages exchanged over JUCE's InterprocessConnection (TCP).
 *
 * Wire format inside each JUCE IPC frame: uint16_t kind (LE) | payload bytes.
 *
 * ### Wire protocol
 * - `spawnProcessor(shell, args, cwd, uuid, cols, rows, envID)` — client→daemon
 * - `spawnProcessorResponse(uuid)` — daemon→client ack
 * - `attachProcessor(uuid)` — client→daemon "I want bytes for this uuid"
 * - `loading(uuid, bytes)` — daemon→client initial byte snapshot for the loading phase
 * - `output(uuid, bytes)` — daemon→client live PTY bytes
 * - `input(uuid, bytes)` — client→daemon keyboard/mouse input
 * - `resizeSession(uuid, cols, rows)` — client→daemon
 * - `detachProcessor(uuid)` — client→daemon "stop forwarding" (session keeps running)
 * - `removeProcessor(uuid)` — client→daemon "kill this shell" (session destroyed)
 * - `processorExited(uuid)` — daemon→client "shell exited"
 * - `processorList(uuids)` — daemon→client initial session list on hello
 * - `hello` / `helloResponse` / `ping` / `pong` / `shutdown` — control
 */

#pragma once

#include <juce_core/juce_core.h>

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @enum Message
 * @brief Identifies the type of a Nexus protocol message.
 *
 * Each enumerator maps to a fixed uint16_t wire value.  The kind occupies the
 * first 2 bytes of every JUCE IPC message payload (after JUCE's 8-byte header).
 *
 * Values are stable — do not reuse or renumber existing entries.
 */
enum class Message : uint16_t
{
    hello                   = 0x01, ///< Client → Host: initiate connection, carry version info.
    helloResponse           = 0x02, ///< Host → Client: acknowledge Hello, confirm protocol version.
    ping                    = 0x03, ///< Either direction: liveness probe.
    pong                    = 0x04, ///< Either direction: reply to Ping.

    spawnProcessor          = 0x10, ///< Client → Host: create a new PTY session.
    spawnProcessorResponse  = 0x11, ///< Host → Client: confirm session creation, carry UUID.
    attachProcessor         = 0x12, ///< Client → Host: subscribe to byte output for a session.
    attachProcessorResponse = 0x13, ///< Host → Client: confirm attach (reserved for future use).
    detachProcessor         = 0x14, ///< Client → Host: unsubscribe from a session (session keeps running).
    resizeSession           = 0x15, ///< Client → Host: change PTY dimensions for a session.
    input                   = 0x16, ///< Client → Host: raw bytes to write to PTY stdin.
    removeProcessor         = 0x17, ///< Client → Host: kill the shell for a session (session is destroyed).

    loading                 = 0x20, ///< Host → Client: loading-phase byte snapshot for a newly-attached session.
                                    ///<   Payload: uuid (length-prefixed string) + raw PTY bytes.
    output                  = 0x21, ///< Host → Client: live PTY byte output for a running session.
                                    ///<   Payload: uuid (length-prefixed string) + raw PTY bytes.

    processorExited         = 0x40, ///< Host → Client: shell process exited, session will be removed.
    processorList           = 0x50, ///< Host → Client: full list of live session UUIDs (pushed unsolicited).

    shutdown                = 0x60, ///< Client → Host: request graceful host shutdown.
};


/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
