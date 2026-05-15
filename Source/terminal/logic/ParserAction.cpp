/**
 * @file ParserAction.cpp
 * @brief VT action dispatch, UTF-8 accumulation, and CSI parameter helpers.
 *
 * This translation unit implements `Parser::performAction()`,
 * `Parser::performEntryAction()`, the UTF-8 byte accumulation path, and the
 * buffer append primitive.  The hot-path loop and state transition machinery
 * live in `Parser.cpp`.
 *
 * ## performAction() dispatch table
 *
 * | Action       | Effect                                                                            |
 * |--------------|-----------------------------------------------------------------------------------|
 * | print        | `handlePrintByte()` → UTF-8 accumulation → `video.print()`                       |
 * | execute      | `video.applyControlCode()` — C0 control characters (CR, LF, BS, …)               |
 * | collect      | Append byte to `intermediateBuffer`                                               |
 * | param        | `handleParam()` — digit/separator into CSI accumulator                            |
 * | escDispatch  | `video.applyESC()` — complete ESC sequence                                        |
 * | csiDispatch  | `video.applyCSI()` — complete CSI sequence                                        |
 * | oscPut       | `appendToBuffer (oscBuffer, …)` — hybrid OSC buffer                               |
 * | oscEnd       | `video.applyOSC()` — complete OSC string                                          |
 * | hook         | `video.storeDCSHeader()` — DCS sequence entry, records final byte                 |
 * | put          | `appendToBuffer (dcsBuffer, …)` — DCS passthrough byte                            |
 * | unhook       | `video.applyDCSPayload()` — DCS sequence exit, dispatches accumulated payload     |
 * | apcPut       | `appendToBuffer (apcBuffer, …)` — APC passthrough byte                            |
 * | apcEnd       | `video.applyAPCPayload()` — Kitty graphics APC exit                               |
 * | ignore/none  | No-op                                                                             |
 *
 * ## Thread model
 *
 * **All methods in this file are READER THREAD only.**
 *
 * @see Parser.h        — class declaration, member documentation, and lifecycle notes
 * @see Parser.cpp      — hot-path loop and state transition machinery
 * @see Video.h         — VT command processor called directly for all semantic actions
 */

#include "Parser.h"
#include "Video.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Buffer management
// ============================================================================

/**
 * @brief Appends a byte to a hybrid buffer with lazy allocation and geometric growth.
 *
 * Handles three cases:
 *
 * - **First call** (`capacity == 0`): allocates `initialCapacity` bytes via
 *   `juce::HeapBlock::allocate`.
 * - **Overflow** (`size >= capacity`): doubles the capacity and copies the
 *   existing content to the new block via `std::memcpy`.
 * - **Normal** (`size < capacity`): writes `byte` directly.
 *
 * After ensuring sufficient capacity, `byte` is stored at `buffer[size]` and
 * `size` is incremented.
 *
 * @param buffer           The `HeapBlock` to append to.
 * @param size             Current number of valid bytes in `buffer` (in/out).
 * @param capacity         Current allocated capacity of `buffer` (in/out).
 * @param byte             The byte to append.
 * @param initialCapacity  Capacity to allocate on the first call.
 *
 * @note READER THREAD only.
 *
 * @see oscBuffer
 * @see dcsBuffer
 */
void Parser::appendToBuffer (juce::HeapBlock<uint8_t>& buffer, int& size, int& capacity, uint8_t byte, int initialCapacity) noexcept
{
    if (size >= capacity)
    {
        const int newCapacity { (capacity == 0) ? initialCapacity : capacity * 2 };
        juce::HeapBlock<uint8_t> grown;
        grown.allocate (static_cast<size_t> (newCapacity), false);

        if (size > 0)
            std::memcpy (grown.get(), buffer.get(), static_cast<size_t> (size));

        buffer = std::move (grown);
        capacity = newCapacity;
    }

    buffer[size] = byte;
    ++size;
}

// ============================================================================
// Action dispatch
// ============================================================================

/**
 * @brief Executes the action associated with a state transition, calling
 *        Video methods directly for all semantic actions.
 *
 * Central dispatcher for all VT actions.  Parser-owned accumulation actions
 * (collect, param, put, oscPut, apcPut) are handled inline.  All semantic
 * terminal actions (applyControlCode, applyESC, applyCSI, applyOSC,
 * storeDCSHeader, applyDCSPayload, applyAPCPayload) are called directly on
 * `video`.  `handlePrintByte()` remains on Parser and internally calls
 * `video.print()`.
 *
 * @param action  The action to perform, as determined by the DispatchTable.
 * @param byte    The input byte associated with the action.
 *
 * @note READER THREAD only.
 *
 * @see handlePrintByte()
 * @see Video.h
 */
void Parser::performAction (ParserAction action, uint8_t byte) noexcept
{
    switch (action)
    {
        case ParserAction::none:
        case ParserAction::ignore:
            break;

        case ParserAction::print:
            handlePrintByte (byte);
            break;

        case ParserAction::execute:
            video.applyControlCode (byte);
            break;

        case ParserAction::collect:
            if (intermediateCount < MAX_INTERMEDIATES)
            {
                intermediateBuffer[intermediateCount] = byte;
                ++intermediateCount;
            }
            break;

        case ParserAction::param:
            handleParam (byte);
            break;

        case ParserAction::escDispatch:
            video.applyESC (intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::csiDispatch:
            csi.finalize();
            video.applyCSI (csi, intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::put:
            appendToBuffer (dcsBuffer, dcsBufferSize, dcsBufferCapacity, byte, 65536);
            break;

        case ParserAction::oscPut:
            appendToBuffer (oscBuffer, oscBufferSize, oscBufferCapacity, byte, OSC_BUFFER_CAPACITY);
            break;

        case ParserAction::oscEnd:
            video.applyOSC (oscBuffer.get(), oscBufferSize);
            break;

        case ParserAction::hook:
            csi.finalize();
            video.storeDCSHeader (csi, intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::unhook:
            video.applyDCSPayload (dcsBuffer.get(), dcsBufferSize);
            dcsBufferSize = 0;
            break;

        case ParserAction::apcPut:
            appendToBuffer (apcBuffer, apcBufferSize, apcBufferCapacity, byte, 65536);
            break;

        case ParserAction::apcEnd:
            video.applyAPCPayload (apcBuffer.get(), apcBufferSize);
            apcBufferSize = 0;
            break;
    }
}

/**
 * @brief Performs the entry action for a newly entered parser state.
 *
 * Called by `processTransition()` immediately after `performAction()` when
 * the state changes.  Entry actions reset the accumulators that belong to
 * the new state, ensuring no stale data from a previous sequence leaks in:
 *
 * - **escape**    — `intermediateCount = 0`: clears intermediate buffer for
 *                   the new ESC sequence.
 * - **csiEntry**  — `csi.reset()` + `intermediateCount = 0`: clears both the
 *                   CSI parameter accumulator and the intermediate buffer.
 * - **dcsEntry**  — same as `csiEntry` plus `dcsBufferSize = 0`: resets the
 *                   DCS payload accumulator.
 * - **oscString** — `oscBufferSize = 0`: clears the hybrid OSC payload buffer.
 * - **apcString** — `apcBufferSize = 0`: clears the APC payload buffer.
 * - All other states have no entry action (default branch is a no-op).
 *
 * @param newState  The state being entered.
 *
 * @note READER THREAD only.
 *
 * @see processTransition()
 */
void Parser::performEntryAction (ParserState newState) noexcept
{
    switch (newState)
    {
        case ParserState::escape:
            intermediateCount = 0;
            utf8AccumulatorLength = 0;
            break;

        case ParserState::csiEntry:
        case ParserState::dcsEntry:
            csi.reset();
            intermediateCount = 0;
            if (newState == ParserState::dcsEntry)
                dcsBufferSize = 0;
            break;

        case ParserState::oscString:
            oscBufferSize = 0;
            break;

        case ParserState::apcString:
            apcBufferSize = 0;
            break;

        default:
            break;
    }
}

// ============================================================================
// UTF-8 accumulation
// ============================================================================

/**
 * @brief Returns the expected total byte length of a UTF-8 sequence from its lead byte.
 *
 * Decodes the sequence length from the high bits of the lead byte using the
 * standard UTF-8 encoding rules:
 *
 * | Lead byte range | High bits | Sequence length |
 * |-----------------|-----------|-----------------|
 * | 0xF0–0xF7       | 11110xxx  | 4               |
 * | 0xE0–0xEF       | 1110xxxx  | 3               |
 * | 0xC0–0xDF       | 110xxxxx  | 2               |
 * | 0x00–0x7F       | 0xxxxxxx  | 1 (ASCII)       |
 *
 * Values in the range 0x80–0xBF (continuation bytes) are not valid lead bytes
 * and return 1 as a safe fallback.
 *
 * @param leadByte  The first byte of the UTF-8 sequence.
 *
 * @return Expected total byte count (1–4).
 *
 * @note Pure function — no side effects, no state access.
 * @note READER THREAD only.
 *
 * @see accumulateUTF8Byte()
 */
uint8_t Parser::expectedUTF8Length (uint8_t leadByte) noexcept
{
    static constexpr uint8_t lengths[256] = {
        // 0x00–0xBF: 1 (ASCII or continuation byte)
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        // 0xC0–0xDF: 2-byte sequence
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        // 0xE0–0xEF: 3-byte sequence
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        // 0xF0–0xF7: 4-byte sequence; 0xF8–0xFF: invalid — treat as 1
        4,4,4,4,4,4,4,4, 1,1,1,1,1,1,1,1
    };

    return lengths[leadByte];
}

/**
 * @brief Accumulates a byte into the UTF-8 sequence buffer and decodes when complete.
 *
 * UTF-8 multi-byte sequences arrive one byte at a time from the PTY stream.
 * This method maintains `utf8Accumulator` and `utf8AccumulatorLength` across
 * successive calls:
 *
 * 1. **Lead byte** (≥ 0xC0): resets the accumulator and stores the byte at index 0.
 * 2. **Continuation byte** (0x80–0xBF): appended to the accumulator if a sequence
 *    is in progress (`utf8AccumulatorLength > 0`) and the buffer is not full.
 * 3. **Completion check**: after each append, if `utf8AccumulatorLength` equals
 *    `expectedUTF8Length (utf8Accumulator[0])`, the sequence is complete.
 *    `juce::CharPointer_UTF8` decodes the null-terminated buffer to a Unicode
 *    codepoint, which is dispatched via `video.print()`.  The accumulator is then reset.
 *
 * Invalid sequences (e.g. a continuation byte with no preceding lead byte, or
 * a lead byte that interrupts an in-progress sequence) are silently discarded
 * because the accumulator is only appended to when `utf8AccumulatorLength > 0`.
 *
 * @param byte  The next byte of the UTF-8 sequence (lead or continuation).
 *
 * @note READER THREAD only.
 *
 * @see expectedUTF8Length()
 * @see handlePrintByte()
 */
void Parser::accumulateUTF8Byte (uint8_t byte) noexcept
{
    const bool isLeadByte { byte >= 0xC0 };
    const bool isContinuationByte { byte >= 0x80 and byte < 0xC0 };

    if (isLeadByte)
    {
        utf8Accumulator[0] = static_cast<char> (byte);
        utf8AccumulatorLength = 1;
    }

    if (isContinuationByte and utf8AccumulatorLength > 0 and utf8AccumulatorLength < 4)
    {
        utf8Accumulator[utf8AccumulatorLength] = static_cast<char> (byte);
        ++utf8AccumulatorLength;
    }

    if (utf8AccumulatorLength > 1)
    {
        const auto expected { expectedUTF8Length (static_cast<uint8_t> (utf8Accumulator[0])) };
        if (utf8AccumulatorLength == expected)
        {
            utf8Accumulator[utf8AccumulatorLength] = '\0';
            juce::CharPointer_UTF8 decoder (utf8Accumulator);
            video.print (static_cast<uint32_t> (*decoder));
            utf8AccumulatorLength = 0;
        }
    }
}

/**
 * @brief Routes a printable byte to `video.print()` directly or through UTF-8 accumulation.
 *
 * Called by `performAction()` for every `Action::print` byte.  Two paths:
 *
 * - **ASCII** (byte ≤ 0x7F): resets `utf8AccumulatorLength` to discard any
 *   in-progress multi-byte sequence, then calls `video.print()` directly with
 *   the codepoint value.
 * - **Non-ASCII** (byte > 0x7F): delegates to `accumulateUTF8Byte()` to
 *   begin or continue a multi-byte UTF-8 sequence.
 *
 * @param byte  The printable input byte (0x20–0x7E for ASCII, ≥ 0x80 for UTF-8).
 *
 * @note READER THREAD only.
 *
 * @see accumulateUTF8Byte()
 */
void Parser::handlePrintByte (uint8_t byte) noexcept
{
    const bool isASCII { byte <= 0x7F };

    if (isASCII)
    {
        utf8AccumulatorLength = 0;
        video.print (static_cast<uint32_t> (byte));
    }
    else
    {
        accumulateUTF8Byte (byte);
    }
}

// ============================================================================
// CSI parameter accumulation
// ============================================================================

/**
 * @brief Feeds a CSI parameter byte into the CSI accumulator.
 *
 * Called by `performAction()` for every `Action::param` byte.  The byte is
 * classified and forwarded to the appropriate `CSI` method:
 *
 * - **Digit** ('0'–'9'): `csi.addDigit (byte - '0')` — extends the current
 *   numeric parameter.
 * - **Separator** (';' or ':'): `csi.addSeparator (byte)` — commits the
 *   current parameter and begins the next.  Colon (`:`) is used for
 *   sub-parameters in extended SGR colour sequences (e.g. `38:2:r:g:b`).
 *
 * @param byte  The parameter byte (0x30–0x3B range per the VT spec).
 *
 * @note READER THREAD only.
 *
 * @see CSI::addDigit()
 * @see CSI::addSeparator()
 */
void Parser::handleParam (uint8_t byte) noexcept
{
    const bool isDigit { byte >= '0' and byte <= '9' };
    const bool isSeparator { byte == ';' or byte == ':' };

    if (isDigit)
    {
        csi.addDigit (static_cast<uint8_t> (byte - '0'));
    }

    if (isSeparator)
    {
        csi.addSeparator (byte);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
