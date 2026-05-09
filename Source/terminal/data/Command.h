/**
 * @file Command.h
 * @brief Command types for VT terminal dispatch.
 *
 * @see DispatchTable.h — ParserAction enum (includes both internal and external actions)
 * @see Processor — registers command handlers
 * @see Parser — dispatches commands via Function::Map
 */

#pragma once

#include <cstdint>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Command types for VT terminal dispatch.
 *
 * External actions from the Paul Williams VT100 state machine. These are
 * the actions that cross the Parser → Processor boundary via
 * `jam::Function::Map<Command::Type, void>`.
 *
 * Internal actions (none, ignore, collect, param) stay on Parser and are
 * not represented here.
 *
 * @see DispatchTable.h — ParserAction enum (includes both internal and external actions)
 * @see Processor — registers command handlers
 * @see Parser — dispatches commands via Function::Map
 */
struct Command
{
    enum class Type : uint8_t
    {
        print,            ///< Print character to screen (UTF-8 decoded codepoint)
        applyControlCode, ///< Apply C0/C1 control function (CR, LF, BS, TAB, BEL, etc.)
        applyCSI,         ///< Apply complete CSI sequence (cursor, erase, mode, SGR, etc.)
        applyESC,         ///< Apply complete ESC sequence (charset, DEC private, etc.)
        applyOSC,         ///< Apply complete OSC string — dispatch accumulated payload
        storeDCSHeader,   ///< Store DCS sequence header — record final byte for payload dispatch
        applyDCSPayload,  ///< Apply accumulated DCS payload (Sixel, SKiT, etc.)
        applyAPCPayload   ///< Apply accumulated APC payload (Kitty graphics, etc.)
    };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
