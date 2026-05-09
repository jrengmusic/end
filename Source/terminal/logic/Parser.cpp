/**
 * @file Parser.cpp
 * @brief Core VT state machine — byte processing hot path and state transitions.
 *
 * This file owns the main processing loop and the state transition machinery.
 * Action dispatch, UTF-8 accumulation, and CSI/buffer helpers live in
 * `ParserAction.cpp`.
 *
 * ## Byte processing pipeline
 *
 * @code
 *  PTY bytes
 *      │
 *      ▼
 *  process()
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
 * which is called on the MESSAGE THREAD before the reader thread starts.
 *
 * @see Parser.h        — class declaration, member documentation, and lifecycle notes
 * @see ParserAction.cpp — action dispatch, UTF-8 accumulation, CSI helpers
 * @see DispatchTable.h  — O(1) `(ParserState, byte) → Transition` lookup table
 */

#include "Parser.h"

namespace Terminal
{ /*____________________________________________________________________________*/


// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Constructs the Parser, binding it to a commands map for semantic action dispatch.
 *
 * Stores a reference to `commands` for use throughout the parsing lifetime.
 * The `DispatchTable` member is default-constructed here, which populates the
 * full `(ParserState, byte) → Transition` lookup table.
 *
 * @param commands  Command dispatch map.  All decoded semantic actions
 *                  (print, execute, csiDispatch, …) are dispatched through
 *                  this map on the reader thread.
 *
 * @note MESSAGE THREAD — called before the reader thread starts.
 *
 * @see Parser.h
 */
Parser::Parser (jam::Function::Map<Command::Type, void>& commands) noexcept
    : commands (commands)
{
}

/**
 * @brief Processes a block of raw PTY bytes through the VT state machine.
 *
 * This is the hot path.  The method iterates over `[data, data+length)` and
 * routes each byte through the DispatchTable to `processTransition()`.
 *
 * @param data    Pointer to the first byte of the input buffer.
 *                Must not be null when `length > 0`.
 * @param length  Number of bytes to process.  Zero is a no-op.
 *
 * @note READER THREAD only.  Must not be called concurrently.
 *
 * @see processTransition()
 */
void Parser::process (const uint8_t* data, size_t length) noexcept
{
    size_t i { 0 };

    while (i < length)
    {
        const auto byte { data[i] };
        const auto transition { dispatchTable.get (currentState, byte) };
        processTransition (byte, transition);
        ++i;
    }
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
