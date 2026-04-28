/**
 * @file Parser.cpp
 * @brief Core VT state machine — byte processing hot path and state transitions.
 *
 * This file owns the main processing loop, the ground-state fast path,
 * and the state transition machinery.  Action dispatch, UTF-8 accumulation,
 * and CSI/buffer helpers live in `ParserAction.cpp`.
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
 *              ├─ performAction(action, byte)       ← ParserAction.cpp
 *              └─ performEntryAction(nextState)     ← ParserAction.cpp
 * @endcode
 *
 * ## Thread model
 *
 * **All methods in this file are READER THREAD only**, except the constructor
 * and `resize()` which are called on the MESSAGE THREAD before the reader
 * thread starts.
 *
 * @see Parser.h        — class declaration, member documentation, and lifecycle notes
 * @see ParserAction.cpp — action dispatch, UTF-8 accumulation, CSI helpers
 * @see DispatchTable.h  — O(1) `(ParserState, byte) → Transition` lookup table
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
 * @note MESSAGE THREAD — called from Processor::resized().
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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
