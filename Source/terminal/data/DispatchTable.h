/**
 * @file DispatchTable.h
 * @brief VT parser state machine dispatch table.
 *
 * This file implements the state transition table for a VT100/VT220 compatible
 * terminal parser. The parser implements a state machine that processes each
 * byte of input and determines what action to take and what the next state should be.
 *
 * The dispatch table provides O(1) lookup for state transitions, making it
 * efficient for real-time terminal input processing. Each entry maps a
 * (current state, input byte) pair to a (next state, action) pair.
 *
 * Parser States:
 * - ground: Normal character processing state
 * - escape: After receiving ESC (0x1B)
 * - escapeIntermediate: After ESC + intermediate character(s)
 * - csiEntry: After CSI introducer (ESC [)
 * - csiParam: Collecting CSI parameters
 * - csiIntermediate: After CSI intermediate character(s)
 * - csiIgnore: Invalid CSI sequence
 * - dcsEntry: After DCS introducer (ESC P)
 * - dcsParam: Collecting DCS parameters
 * - dcsIntermediate: After DCS intermediate character(s)
 * - dcsPassthrough: DCS passthrough mode
 * - dcsIgnore: Invalid DCS sequence
 * - oscString: OSC (Operating System Command) string
 * - sosPmApcString: SOS/PM/APC string (ignored)
 *
 * @see Parser.h for the state machine implementation that uses this table
 * @see ECMA-48 Terminal Control Functions specification
 */

#pragma once
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Parser states for VT terminal emulation.
 *
 * These states represent the different contexts in the VT parser state machine.
 * The parser transitions between states based on input bytes according to
 * the ECMA-48 standard for terminal control functions.
 *
 * @note The stateCount enum value is used for array sizing and should not
 *       be used as an actual parser state.
 */
enum class ParserState : uint8_t
{
    ground,              ///< Normal text processing, ground state
    escape,              ///< Received ESC, waiting for next byte
    escapeIntermediate,  ///< ESC + intermediate character(s)
    csiEntry,            ///< Received CSI introducer (ESC [)
    csiParam,            ///< Collecting CSI parameters (digits, semicolons)
    csiIntermediate,    ///< CSI + intermediate character(s)
    csiIgnore,           ///< Invalid CSI sequence (private marker in wrong place)
    dcsEntry,            ///< Received DCS introducer (ESC P)
    dcsParam,            ///< Collecting DCS parameters
    dcsIntermediate,    ///< DCS + intermediate character(s)
    dcsPassthrough,      ///< DCS passthrough mode (for SGR in 256 colors)
    dcsIgnore,           ///< Invalid DCS sequence
    oscString,           ///< OSC command string (title, color changes)
    sosPmApcString,      ///< SOS/PM/APC strings (not supported, ignored)
    stateCount           ///< Total number of states (for array sizing)
};

/**
 * @brief Actions to perform when processing a byte.
 *
 * These actions are returned by the dispatch table and executed by the parser.
 * Each action represents a specific handling routine for the current byte.
 *
 * @note Renamed from `Action` to `ParserAction` to avoid a name collision with
 *       `Action::Registry` (the user-facing action registry in action/Action.h).
 */
enum class ParserAction : uint8_t
{
    none,          ///< No action, just transition state
    ignore,        ///< Ignore this byte (consume without action)
    print,         ///< Print character to screen
    execute,       ///< Execute control function (CR, LF, etc.)
    collect,       ///< Collect intermediate character(s)
    param,         ///< Collect parameter bytes
    escDispatch,   ///< Dispatch escape sequence
    csiDispatch,   ///< Dispatch CSI sequence
    put,           ///< Put character to DCS passthrough
    oscPut,        ///< Put character to OSC string
    oscEnd,        ///< End OSC string (on ST or BEL)
    hook,          ///< Hook DCS sequence
    unhook         ///< Unhook DCS sequence
};

/**
 * @brief State transition entry for the dispatch table.
 *
 * Each transition contains the next parser state and the action to perform.
 * This struct is designed to be exactly 2 bytes for cache efficiency.
 *
 * @note static_assert ensures trivial copyability and exact size
 */
struct Transition
{
    ParserState nextState;  ///< The state to transition to
    ParserAction action;    ///< The action to perform
};

static_assert (std::is_trivially_copyable_v<Transition>, "Transition must be trivially copyable");
static_assert (sizeof (Transition) == 2, "Transition must be exactly 2 bytes");

/*____________________________________________________________________________*/

/**
 * @brief VT parser state machine dispatch table.
 *
 * This class provides O(1) lookup for state transitions based on the current
 * parser state and input byte. The table is built in the constructor following
 * the ECMA-48 state machine specification.
 *
 * The table dimensions are:
 * - STATE_COUNT (14): Number of active parser states
 * - BYTE_COUNT (256): All possible input byte values (0x00-0xFF)
 *
 * @note Thread safety: This class is immutable after construction.
 *       The get() method can be called from any thread context.
 *
 * @par Usage
 * @code
 * DispatchTable table;
 * auto [nextState, action] = table.get(currentState, inputByte);
 * @endcode
 *
 * @see Parser.h for the complete parser implementation
 */
class DispatchTable
{
public:
    static constexpr int STATE_COUNT { 14 };    ///< Number of active parser states
    static constexpr int BYTE_COUNT { 256 };    ///< All possible byte values (0x00-0xFF)

    /// Row type: 256 transitions for each possible input byte in a state
    using StateRow = std::array<Transition, BYTE_COUNT>;
    /// Full table type: 14 state rows
    using FullTable = std::array<StateRow, STATE_COUNT>;

    /**
     * @brief Constructs the dispatch table.
     *
     * Initializes all state transition entries according to the VT100/VT220
     * specification. Called once during construction.
     */
    DispatchTable()
    {
        buildGround();
        buildEscape();
        buildEscapeIntermediate();
        buildCSIEntry();
        buildCSIParam();
        buildCSIIntermediate();
        buildCSIIgnore();
        buildDCSEntry();
        buildDCSParam();
        buildDCSIntermediate();
        buildDCSPassthrough();
        buildDCSIgnore();
        buildOSCString();
        buildSosPmApc();
    }

    /**
     * @brief Looks up the transition for a given state and byte.
     *
     * Performs O(1) lookup in the transition table.
     *
     * @param state The current parser state
     * @param byte The input byte to look up
     * @return The Transition containing next state and action
     *
     * @note Thread safety: This method is const and noexcept.
     *       Safe to call from audio thread.
     */
    Transition get (ParserState state, uint8_t byte) const noexcept
    {
        return table.at (static_cast<size_t> (state)).at (static_cast<size_t> (byte));
    }

private:
    FullTable table {};  ///< The 14x256 transition table

    /**
     * @brief Creates a Transition with the given state and action.
     * @param s The next parser state
     * @param a The action to perform
     * @return A Transition struct
     */
    static Transition make (ParserState s, ParserAction a) noexcept
    {
        return { s, a };
    }

    /**
     * @brief Fills a range of bytes in a row with the same transition.
     * @param row The state row to modify
     * @param first First byte in range (inclusive)
     * @param last Last byte in range (inclusive)
     * @param value The transition to assign
     */
    static void fillRange (StateRow& row, uint8_t first, uint8_t last, Transition value) noexcept
    {
        for (int i { first }; i <= last; ++i)
        {
            row.at (static_cast<size_t> (i)) = value;
        }
    }

    /**
     * @brief Applies "anywhere" entries that are valid from multiple states.
     *
     * Certain control characters (CAN, SUB, ESC, etc.) can interrupt
     * sequences from any state and return to ground.
     *
     * @param row The state row to modify
     */
    void applyAnywhere (StateRow& row) noexcept
    {
        row.at (0x18) = make (ParserState::ground, ParserAction::execute);  // CAN
        row.at (0x1A) = make (ParserState::ground, ParserAction::execute);  // SUB
        row.at (0x1B) = make (ParserState::escape, ParserAction::none);      // ESC
        fillRange (row, 0x80, 0x8F, make (ParserState::ground, ParserAction::execute));
        row.at (0x90) = make (ParserState::dcsEntry, ParserAction::none);   // DCS
        fillRange (row, 0x91, 0x97, make (ParserState::ground, ParserAction::execute));
        row.at (0x98) = make (ParserState::sosPmApcString, ParserAction::none);  // SOS
        row.at (0x99) = make (ParserState::ground, ParserAction::execute);
        row.at (0x9A) = make (ParserState::ground, ParserAction::execute);   // DECID
        row.at (0x9B) = make (ParserState::csiEntry, ParserAction::none);   // CSI
        row.at (0x9C) = make (ParserState::ground, ParserAction::none);     // ST
        row.at (0x9D) = make (ParserState::oscString, ParserAction::none);  // OSC
        row.at (0x9E) = make (ParserState::sosPmApcString, ParserAction::none);  // PM
        row.at (0x9F) = make (ParserState::sosPmApcString, ParserAction::none);  // APC
    }

    /**
     * @brief Gets a reference to the row for a given state.
     * @param state The parser state
     * @return Reference to the state's row
     */
    StateRow& rowFor (ParserState state) noexcept
    {
        return table.at (static_cast<size_t> (state));
    }

    // State-specific table builders follow ECMA-48 specification

    void buildGround()
    {
        auto& row { rowFor (ParserState::ground) };
        fillRange (row, 0x00, 0x17, make (ParserState::ground, ParserAction::execute));
        row.at (0x19) = make (ParserState::ground, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::ground, ParserAction::execute));
        fillRange (row, 0x20, 0x7E, make (ParserState::ground, ParserAction::print));
        row.at (0x7F) = make (ParserState::ground, ParserAction::ignore);
        fillRange (row, 0xA0, 0xFF, make (ParserState::ground, ParserAction::print));
        applyAnywhere (row);
        fillRange (row, 0x80, 0x9F, make (ParserState::ground, ParserAction::print));
    }

    void buildEscape()
    {
        auto& row { rowFor (ParserState::escape) };
        fillRange (row, 0x00, 0x17, make (ParserState::escape, ParserAction::execute));
        row.at (0x19) = make (ParserState::escape, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::escape, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::escapeIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x4F, make (ParserState::ground, ParserAction::escDispatch));
        row.at (0x50) = make (ParserState::dcsEntry, ParserAction::none);
        fillRange (row, 0x51, 0x57, make (ParserState::ground, ParserAction::escDispatch));
        row.at (0x58) = make (ParserState::sosPmApcString, ParserAction::none);
        row.at (0x59) = make (ParserState::ground, ParserAction::escDispatch);
        row.at (0x5A) = make (ParserState::ground, ParserAction::escDispatch);
        row.at (0x5B) = make (ParserState::csiEntry, ParserAction::none);
        row.at (0x5C) = make (ParserState::ground, ParserAction::escDispatch);
        row.at (0x5D) = make (ParserState::oscString, ParserAction::none);
        row.at (0x5E) = make (ParserState::sosPmApcString, ParserAction::none);
        row.at (0x5F) = make (ParserState::sosPmApcString, ParserAction::none);
        fillRange (row, 0x60, 0x7E, make (ParserState::ground, ParserAction::escDispatch));
        row.at (0x7F) = make (ParserState::escape, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildEscapeIntermediate()
    {
        auto& row { rowFor (ParserState::escapeIntermediate) };
        fillRange (row, 0x00, 0x17, make (ParserState::escapeIntermediate, ParserAction::execute));
        row.at (0x19) = make (ParserState::escapeIntermediate, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::escapeIntermediate, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::escapeIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x7E, make (ParserState::ground, ParserAction::escDispatch));
        row.at (0x7F) = make (ParserState::escapeIntermediate, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildCSIEntry()
    {
        auto& row { rowFor (ParserState::csiEntry) };
        fillRange (row, 0x00, 0x17, make (ParserState::csiEntry, ParserAction::execute));
        row.at (0x19) = make (ParserState::csiEntry, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::csiEntry, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::csiIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x39, make (ParserState::csiParam, ParserAction::param));
        row.at (0x3A) = make (ParserState::csiParam, ParserAction::param);
        row.at (0x3B) = make (ParserState::csiParam, ParserAction::param);
        fillRange (row, 0x3C, 0x3F, make (ParserState::csiParam, ParserAction::collect));
        fillRange (row, 0x40, 0x7E, make (ParserState::ground, ParserAction::csiDispatch));
        row.at (0x7F) = make (ParserState::csiEntry, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildCSIParam()
    {
        auto& row { rowFor (ParserState::csiParam) };
        fillRange (row, 0x00, 0x17, make (ParserState::csiParam, ParserAction::execute));
        row.at (0x19) = make (ParserState::csiParam, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::csiParam, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::csiIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x39, make (ParserState::csiParam, ParserAction::param));
        row.at (0x3A) = make (ParserState::csiParam, ParserAction::param);
        row.at (0x3B) = make (ParserState::csiParam, ParserAction::param);
        fillRange (row, 0x3C, 0x3F, make (ParserState::csiIgnore, ParserAction::collect));
        fillRange (row, 0x40, 0x7E, make (ParserState::ground, ParserAction::csiDispatch));
        row.at (0x7F) = make (ParserState::csiParam, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildCSIIntermediate()
    {
        auto& row { rowFor (ParserState::csiIntermediate) };
        fillRange (row, 0x00, 0x17, make (ParserState::csiIntermediate, ParserAction::execute));
        row.at (0x19) = make (ParserState::csiIntermediate, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::csiIntermediate, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::csiIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x3F, make (ParserState::csiIgnore, ParserAction::ignore));
        fillRange (row, 0x40, 0x7E, make (ParserState::ground, ParserAction::csiDispatch));
        row.at (0x7F) = make (ParserState::csiIntermediate, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildCSIIgnore()
    {
        auto& row { rowFor (ParserState::csiIgnore) };
        fillRange (row, 0x00, 0x17, make (ParserState::csiIgnore, ParserAction::execute));
        row.at (0x19) = make (ParserState::csiIgnore, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::csiIgnore, ParserAction::execute));
        fillRange (row, 0x20, 0x3F, make (ParserState::csiIgnore, ParserAction::ignore));
        fillRange (row, 0x40, 0x7E, make (ParserState::ground, ParserAction::none));
        row.at (0x7F) = make (ParserState::csiIgnore, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildDCSEntry()
    {
        auto& row { rowFor (ParserState::dcsEntry) };
        fillRange (row, 0x00, 0x17, make (ParserState::dcsEntry, ParserAction::execute));
        row.at (0x19) = make (ParserState::dcsEntry, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::dcsEntry, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::dcsIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x39, make (ParserState::dcsParam, ParserAction::param));
        row.at (0x3A) = make (ParserState::dcsParam, ParserAction::param);
        row.at (0x3B) = make (ParserState::dcsParam, ParserAction::param);
        fillRange (row, 0x3C, 0x3F, make (ParserState::dcsParam, ParserAction::collect));
        fillRange (row, 0x40, 0x7E, make (ParserState::dcsPassthrough, ParserAction::hook));
        row.at (0x7F) = make (ParserState::dcsEntry, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildDCSParam()
    {
        auto& row { rowFor (ParserState::dcsParam) };
        fillRange (row, 0x00, 0x17, make (ParserState::dcsParam, ParserAction::execute));
        row.at (0x19) = make (ParserState::dcsParam, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::dcsParam, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::dcsIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x39, make (ParserState::dcsParam, ParserAction::param));
        row.at (0x3A) = make (ParserState::dcsParam, ParserAction::param);
        row.at (0x3B) = make (ParserState::dcsParam, ParserAction::param);
        fillRange (row, 0x3C, 0x3F, make (ParserState::dcsIgnore, ParserAction::collect));
        fillRange (row, 0x40, 0x7E, make (ParserState::dcsPassthrough, ParserAction::hook));
        row.at (0x7F) = make (ParserState::dcsParam, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildDCSIntermediate()
    {
        auto& row { rowFor (ParserState::dcsIntermediate) };
        fillRange (row, 0x00, 0x17, make (ParserState::dcsIntermediate, ParserAction::execute));
        row.at (0x19) = make (ParserState::dcsIntermediate, ParserAction::execute);
        fillRange (row, 0x1C, 0x1F, make (ParserState::dcsIntermediate, ParserAction::execute));
        fillRange (row, 0x20, 0x2F, make (ParserState::dcsIntermediate, ParserAction::collect));
        fillRange (row, 0x30, 0x3F, make (ParserState::dcsIgnore, ParserAction::ignore));
        fillRange (row, 0x40, 0x7E, make (ParserState::dcsPassthrough, ParserAction::hook));
        row.at (0x7F) = make (ParserState::dcsIntermediate, ParserAction::ignore);
        applyAnywhere (row);
    }

    void buildDCSPassthrough()
    {
        auto& row { rowFor (ParserState::dcsPassthrough) };
        fillRange (row, 0x00, 0x17, make (ParserState::dcsPassthrough, ParserAction::put));
        row.at (0x19) = make (ParserState::dcsPassthrough, ParserAction::put);
        fillRange (row, 0x1C, 0x1F, make (ParserState::dcsPassthrough, ParserAction::put));
        fillRange (row, 0x20, 0x7E, make (ParserState::dcsPassthrough, ParserAction::put));
        row.at (0x7F) = make (ParserState::dcsPassthrough, ParserAction::ignore);
        row.at (0x1B) = make (ParserState::escape, ParserAction::none);
    }

    void buildDCSIgnore()
    {
        auto& row { rowFor (ParserState::dcsIgnore) };
        fillRange (row, 0x00, 0x7F, make (ParserState::dcsIgnore, ParserAction::ignore));
        row.at (0x1B) = make (ParserState::escape, ParserAction::none);
    }

    void buildOSCString()
    {
        auto& row { rowFor (ParserState::oscString) };
        fillRange (row, 0x00, 0x06, make (ParserState::oscString, ParserAction::ignore));
        row.at (0x07) = make (ParserState::ground, ParserAction::oscEnd);
        fillRange (row, 0x08, 0x17, make (ParserState::oscString, ParserAction::ignore));
        row.at (0x19) = make (ParserState::oscString, ParserAction::ignore);
        fillRange (row, 0x1C, 0x1F, make (ParserState::oscString, ParserAction::ignore));
        fillRange (row, 0x20, 0x7F, make (ParserState::oscString, ParserAction::oscPut));
        fillRange (row, 0x80, 0xFF, make (ParserState::oscString, ParserAction::oscPut));
        row.at (0x1B) = make (ParserState::escape, ParserAction::oscEnd);
    }

    void buildSosPmApc()
    {
        auto& row { rowFor (ParserState::sosPmApcString) };
        fillRange (row, 0x00, 0x17, make (ParserState::sosPmApcString, ParserAction::ignore));
        row.at (0x19) = make (ParserState::sosPmApcString, ParserAction::ignore);
        fillRange (row, 0x1C, 0x1F, make (ParserState::sosPmApcString, ParserAction::ignore));
        fillRange (row, 0x20, 0x7F, make (ParserState::sosPmApcString, ParserAction::ignore));
        row.at (0x1B) = make (ParserState::escape, ParserAction::none);
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
