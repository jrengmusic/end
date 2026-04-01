/**
 * @file Parser.cpp
 * @brief Core VT state machine implementation — byte processing, UTF-8 decoding,
 *        and action dispatch.
 *
 * This file implements the central processing loop of the terminal emulator.
 * It owns the three-phase Williams state machine (exit → action → entry),
 * the ground-state fast path, multi-byte UTF-8 accumulation, and the
 * `performAction()` dispatcher that routes every parsed token to its handler.
 *
 * ## Byte processing pipeline
 *
 * @code
 *  PTY bytes
 *      │
 *      ▼
 *  process()
 *      │
 *      ├─ ground state? ──► processGroundChunk()   ← bulk ASCII fast path
 *      │                        │
 *      │                        └─ print() per ASCII byte
 *      │
 *      └─ dispatchTable.get(currentState, byte) ──► Transition{nextState, action}
 *              │
 *              ▼
 *         processTransition()
 *              │
 *              ├─ performAction(action, byte)       ← routes to handler
 *              └─ performEntryAction(nextState)
 * @endcode
 *
 * ## UTF-8 decoding
 *
 * Single-byte ASCII (≤ 0x7F) is forwarded directly to `print()`.  Multi-byte
 * sequences are accumulated byte-by-byte in `utf8Accumulator`.
 * `expectedUTF8Length()` derives the expected total length from the lead byte.
 * When the accumulator reaches that length, `juce::CharPointer_UTF8` decodes
 * the codepoint and `print()` is called.  Invalid or truncated sequences are
 * silently discarded.
 *
 * ## performAction() dispatch table
 *
 * | Action       | Effect                                                  |
 * |--------------|---------------------------------------------------------|
 * | print        | `handlePrintByte()` → UTF-8 accumulation → `print()`   |
 * | execute      | `execute()` — C0 control characters (CR, LF, BS, …)    |
 * | collect      | Append byte to `intermediateBuffer`                     |
 * | param        | `handleParam()` — digit/separator into CSI accumulator  |
 * | escDispatch  | `escDispatch()` — complete ESC sequence                 |
 * | csiDispatch  | `csiDispatch()` — complete CSI sequence                 |
 * | oscPut       | Append byte to `oscBuffer`                              |
 * | oscEnd       | `oscDispatch()` — complete OSC string                   |
 * | hook         | `dcsHook()` — DCS sequence entry                        |
 * | put          | `dcsPut()` — DCS passthrough byte                       |
 * | unhook       | `dcsUnhook()` — DCS sequence exit                       |
 * | ignore/none  | No-op                                                   |
 *
 * ## Thread model
 *
 * **All methods in this file are READER THREAD only.**  `Parser` is constructed
 * on the message thread, but every method called from `process()` runs
 * exclusively on the PTY reader thread.  No locks are held during processing.
 *
 * @see Parser.h      — class declaration, member documentation, and lifecycle notes
 * @see DispatchTable.h — O(1) `(ParserState, byte) → Transition` lookup table
 */

#include "Parser.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/


// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Constructs the Parser, binding it to the shared State and Grid.
 *
 * Stores references to `state` and `grid` for use throughout the parsing
 * lifetime.  The `DispatchTable` member is default-constructed here, which
 * populates the full `(ParserState, byte) → Transition` lookup table.
 *
 * The constructor does **not** call `calc()`.  The owner must call `calc()`
 * after construction (and after every `resize()`) to synchronise internal
 * cached geometry with the current State values.
 *
 * @param state  Shared terminal parameter store.  Read for cursor position,
 *               mode flags, and screen dimensions; written via atomic setters.
 * @param grid   Terminal screen buffer.  Written by `print()`, erase ops,
 *               and scroll operations on the READER THREAD.
 *
 * @note MESSAGE THREAD — called before the reader thread starts.
 *
 * @see calc()
 * @see Parser.h
 */
Parser::Parser (State& state, Grid::Writer writer) noexcept
    : state (state)
    , writer (writer)
{
}

/**
 * @brief Processes a block of raw PTY bytes through the VT state machine.
 *
 * This is the hot path.  The method iterates over `[data, data+length)` and
 * routes each byte through the DispatchTable.  Two code paths exist:
 *
 * @par Ground-state fast path
 * When `currentState == ParserState::ground`, `processGroundChunk()` is called
 * first.  It scans forward for a contiguous run of printable ASCII bytes
 * (0x20–0x7E) and common C0 controls (LF, CR, BS, TAB), handling them without
 * per-byte dispatch overhead.  If it consumes at least one byte, the loop
 * advances `i` by the consumed count and continues.
 *
 * @par General path
 * For any byte that `processGroundChunk()` cannot handle (or when not in
 * ground state), the byte is looked up in `dispatchTable` to obtain a
 * `Transition{nextState, action}`, then forwarded to `processTransition()`.
 *
 * @param data    Pointer to the first byte of the input buffer.
 *                Must not be null when `length > 0`.
 * @param length  Number of bytes to process.  Zero is a no-op.
 *
 * @note READER THREAD only.  Must not be called concurrently.
 * @note Responses queued during processing are not flushed here — call
 *       `flushResponses()` after this method returns.
 *
 * @see processGroundChunk()
 * @see processTransition()
 * @see flushResponses()
 */
void Parser::process (const uint8_t* data, size_t length) noexcept
{
    size_t i { 0 };

    while (i < length)
    {
        // READER THREAD — bulk ground-state fast path
        // Handles printable ASCII + common C0 controls (LF, CR, BS, TAB)
        if (currentState == ParserState::ground)
        {
            const size_t consumed { processGroundChunk (data + i, length - i) };

            if (consumed > 0)
            {
                i += consumed;
                continue;
            }
        }

        const auto byte { data[i] };
        const auto transition { dispatchTable.get (currentState, byte) };
        processTransition (byte, transition);
        ++i;
    }
}

/**
 * @brief Notifies the parser that the terminal dimensions have changed.
 *
 * Propagates the new geometry to `State`, re-clamps the cursor and scroll
 * region to the new bounds on both screen buffers, reinitialises tab stops,
 * and calls `calc()` to update internal cached values.
 *
 * @param newCols         New terminal width in character columns.
 * @param newVisibleRows  New terminal height in visible rows.
 *
 * @note MESSAGE THREAD — called from Session::resized().
 *
 * @see calc()
 * @see initializeTabStops()
 * @see cursorResetScrollRegion()
 */
void Parser::resize (int newCols, int newVisibleRows) noexcept
{
    state.setWrapPending (normal, false);
    state.setWrapPending (alternate, false);
    cursorClamp (normal, newCols, newVisibleRows);
    cursorClamp (alternate, newCols, newVisibleRows);
    cursorResetScrollRegion (normal);
    cursorResetScrollRegion (alternate);
    initializeTabStops (newCols);
    calc();
}

/**
 * @brief Synchronises internal cached geometry from State.
 *
 * Copies the current pen style and colour attributes from `pen` into `stamp`
 * (so DECSC saves the up-to-date pen), then recomputes `scrollBottom` from
 * the current visible row count and the active screen's scroll region.
 *
 * Must be called after construction and after every `resize()`.  Also called
 * internally by `cursorSetScrollRegion()` and `cursorResetScrollRegion()`.
 *
 * @note READER THREAD only.
 *
 * @see resize()
 * @see effectiveScrollBottom()
 */
void Parser::calc() noexcept
{
    stamp.style = pen.style;
    stamp.fg = pen.fg;
    stamp.bg = pen.bg;
}

/**
 * @brief Returns the effective bottom row of the current scrolling region.
 *
 * Reads from Grid buffer dims (safe on reader thread) on every call.
 * Wraps `effectiveScrollBottom()` with the current screen and visible rows.
 *
 * @see effectiveScrollBottom()
 */
int Parser::activeScrollBottom() const noexcept
{
    return effectiveScrollBottom (state.getRawValue<ActiveScreen> (ID::activeScreen), state.getRawValue<int> (ID::visibleRows));
}

// ============================================================================
// Parser Methods
// ============================================================================

/**
 * @brief Applies a state transition: exit action → transition action → entry action.
 *
 * Implements the two-phase transition model for every byte that produces
 * a non-trivial result:
 *
 * 1. Calls `performAction (transition.action, byte)` unconditionally.
 * 2. If `transition.nextState != currentState`, calls
 *    `performEntryAction (transition.nextState)` to initialise the new
 *    state's accumulators, then updates `currentState`.
 *
 * When the state does not change (self-transition), only step 1 runs.
 *
 * @param byte        The input byte that triggered the transition.
 * @param transition  The `{nextState, action}` pair returned by `dispatchTable.get()`.
 *
 * @note READER THREAD only.
 *
 * @see performAction()
 * @see performEntryAction()
 * @see DispatchTable.h
 */
void Parser::processTransition (uint8_t byte, const Transition& transition) noexcept
{
    if (transition.nextState != currentState)
    {
        performAction (transition.action, byte);
        performEntryAction (transition.nextState);
        currentState = transition.nextState;
    }
    else
    {
        performAction (transition.action, byte);
    }
}

/**
 * @brief Executes the action associated with a state transition.
 *
 * Central dispatcher for all VT actions.  Each `Action` enum value maps to
 * a specific handler or inline operation:
 *
 * - **none / ignore** — no-op; byte is silently consumed.
 * - **print** — `handlePrintByte()`: routes through UTF-8 accumulation to `print()`.
 * - **execute** — `execute()`: handles C0/C1 control characters (CR, LF, BS, …).
 *   Resets `graphemeState` first so the next printable starts a fresh cluster.
 * - **collect** — appends `byte` to `intermediateBuffer` (up to `MAX_INTERMEDIATES`).
 * - **param** — `handleParam()`: feeds digit or separator into the CSI accumulator.
 * - **escDispatch** — `escDispatch()`: dispatches a complete ESC sequence.
 *   Resets `graphemeState` before dispatch.
 * - **csiDispatch** — `csiDispatch()`: finalises the CSI accumulator and dispatches.
 *   Resets `graphemeState` before dispatch.
 * - **put** — `dcsPut()`: forwards a DCS passthrough byte.
 * - **oscPut** — appends `byte` to `oscBuffer` (up to `OSC_BUFFER_CAPACITY`).
 * - **oscEnd** — `oscDispatch()`: dispatches the complete OSC string.
 * - **hook** — `dcsHook()`: finalises CSI params and enters DCS passthrough.
 * - **unhook** — `dcsUnhook()`: exits DCS passthrough.
 *
 * @param action  The action to perform, as determined by the DispatchTable.
 * @param byte    The input byte associated with the action.
 *
 * @note READER THREAD only.
 *
 * @see handlePrintByte()
 * @see execute()
 * @see handleParam()
 * @see escDispatch()
 * @see csiDispatch()
 * @see oscDispatch()
 * @see dcsHook()
 * @see dcsUnhook()
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
            graphemeState = graphemeSegmentationInit();
            execute (byte);
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
            graphemeState = graphemeSegmentationInit();
            escDispatch (intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::csiDispatch:
            graphemeState = graphemeSegmentationInit();
            csi.finalize();
            csiDispatch (csi, intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::put:
            dcsPut (byte);
            break;

        case ParserAction::oscPut:
            if (oscLength < OSC_BUFFER_CAPACITY)
            {
                oscBuffer[oscLength] = byte;
                ++oscLength;
            }
            break;

        case ParserAction::oscEnd:
            oscDispatch (oscBuffer, oscLength);
            break;

        case ParserAction::hook:
            csi.finalize();
            dcsHook (csi, intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::unhook:
            dcsUnhook();
            break;
    }
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
 *    codepoint, which is forwarded to `print()`.  The accumulator is then reset.
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
 * @see print()
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
            print (static_cast<uint32_t> (*decoder));
            utf8AccumulatorLength = 0;
        }
    }
}

/**
 * @brief Routes a printable byte to `print()` directly or through UTF-8 accumulation.
 *
 * Called by `performAction()` for every `Action::print` byte.  Two paths:
 *
 * - **ASCII** (byte ≤ 0x7F): resets `utf8AccumulatorLength` to discard any
 *   in-progress multi-byte sequence, then calls `print()` directly with the
 *   codepoint value.
 * - **Non-ASCII** (byte > 0x7F): delegates to `accumulateUTF8Byte()` to
 *   begin or continue a multi-byte UTF-8 sequence.
 *
 * @param byte  The printable input byte (0x20–0x7E for ASCII, ≥ 0x80 for UTF-8).
 *
 * @note READER THREAD only.
 *
 * @see accumulateUTF8Byte()
 * @see print()
 */
void Parser::handlePrintByte (uint8_t byte) noexcept
{
    const bool isASCII { byte <= 0x7F };

    if (isASCII)
    {
        utf8AccumulatorLength = 0;
        print (static_cast<uint32_t> (byte));
    }
    else
    {
        accumulateUTF8Byte (byte);
    }
}

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
 * @see csiDispatch()
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
    return (leadByte >= 0xF0) ? uint8_t { 4 }
         : (leadByte >= 0xE0) ? uint8_t { 3 }
         : (leadByte >= 0xC0) ? uint8_t { 2 }
         :                      uint8_t { 1 };
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
 * - **dcsEntry**  — same as `csiEntry` (DCS uses the same accumulators).
 * - **oscString** — `oscLength = 0`: clears the OSC payload buffer.
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
            break;

        case ParserState::oscString:
            oscLength = 0;
            break;

        default:
            break;
    }
}


/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
