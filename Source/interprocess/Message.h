/**
 * @file Message.h
 * @brief Interprocess protocol message types.
 *
 * Defines the message kind enumeration used by Channel and Link to
 * identify messages exchanged over JUCE's InterprocessConnection (TCP).
 *
 * Wire format inside each JUCE IPC frame: uint16_t kind (LE) | payload bytes.
 *
 * ### Wire protocol
 * - `createSession(shell, args, cwd, uuid, cols, rows, envID)` — client→daemon;
 *   daemon creates if uuid is new, resizes+attaches if uuid exists
 * - `loading(uuid, bytes)` — daemon→client initial byte snapshot for the loading phase
 * - `output(uuid, bytes)` — daemon→client live PTY bytes
 * - `input(uuid, bytes)` — client→daemon keyboard/mouse input
 * - `resizeSession(uuid, cols, rows)` — client→daemon
 * - `detachSession(uuid)` — client→daemon "stop forwarding" (session keeps running)
 * - `killSession(uuid)` — client→daemon "kill this shell" (session destroyed)
 * - `sessionKilled(uuid)` — daemon→client "shell exited"
 * - `sessions(uuids)` — daemon→client initial session list on hello
 * - `stateUpdate(uuid, cwd, fgProcess)` — daemon→client state sync (cwd + foreground process)
 * - `hello` / `helloResponse` / `ping` / `pong` — control
 */

#pragma once

#include <juce_core/juce_core.h>

namespace Interprocess
{
/*____________________________________________________________________________*/

/**
 * @enum Message
 * @brief Identifies the type of an Interprocess protocol message.
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

    createSession           = 0x10, ///< Client → Host: create or attach to a PTY session.
                                    ///<   Payload: shell | args | cwd | uuid | cols (uint16) | rows (uint16) | envID.
                                    ///<   Daemon creates if uuid is new, resizes + attaches if uuid exists.
    detachSession           = 0x14, ///< Client → Host: unsubscribe from a session (session keeps running).
    resizeSession           = 0x15, ///< Client → Host: change PTY dimensions for a session.
    input                   = 0x16, ///< Client → Host: raw bytes to write to PTY stdin.
    killSession             = 0x17, ///< Client → Host: kill the shell for a session (session is destroyed).
    killDaemon              = 0x18, ///< Client -> Host: shut down the daemon process.

    loading                 = 0x20, ///< Host → Client: loading-phase byte snapshot for a newly-attached session.
                                    ///<   Payload: uuid (length-prefixed string) + raw PTY bytes.
    output                  = 0x21, ///< Host → Client: live PTY byte output for a running session.
                                    ///<   Payload: uuid (length-prefixed string) + raw PTY bytes.
    stateUpdate             = 0x22, ///< Host → Client: Session state update (cwd, foreground process).
                                    ///<   Payload: uuid (length-prefixed) + cwd (length-prefixed) + fgProcess (length-prefixed).

    sessionKilled           = 0x40, ///< Host → Client: shell process exited, session will be removed.
    sessions                = 0x50, ///< Host → Client: full list of live session UUIDs (pushed unsolicited).
};


/**______________________________END OF NAMESPACE______________________________*/
}// namespace Interprocess
