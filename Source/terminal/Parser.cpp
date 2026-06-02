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

namespace terminal
{
/*____________________________________________________________________________*/


// ============================================================================
// Public API
// ============================================================================

Parser::Parser (Video& video) noexcept
    : video (video)
{
}

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
} // namespace terminal
