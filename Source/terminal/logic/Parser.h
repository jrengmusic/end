/**
 * @file Parser.h
 * @brief VT100/VT520 DFA byte decoder for terminal emulation.
 *
 * Parser is the byte-processing DFA of the terminal emulator.  It consumes
 * raw bytes from the PTY reader thread, drives a Paul Flo Williams–style state
 * machine, decodes multi-byte UTF-8 sequences, and dispatches semantic actions
 * via `jam::Function::Map<Command::Type, void>`.  Parser holds no terminal
 * state, no Grid reference, no pen, and no cursor — those live entirely in Video.
 *
 * ## State machine model
 *
 * The parser follows the state machine described at:
 * https://vt100.net/emu/dec_ansi_parser
 *
 * Each input byte is looked up in a DispatchTable that maps
 * `(ParserState, byte) → (nextState, Action)` in O(1).  The parser then:
 * 1. Performs the exit action for the current state (if any).
 * 2. Performs the transition action (print, collect, param, …).
 * 3. Performs the entry action for the new state (if any).
 *
 * @par State diagram (simplified)
 * @code
 *  ground ──ESC──► escape ──[──► csiEntry ──digit──► csiParam ──final──► ground
 *    │                │                                                      ▲
 *    │               ]──► oscString ──BEL/ST──────────────────────────────────┘
 *    │               P──► dcsEntry ──► dcsPassthrough ──ST──────────────────────┘
 *    └──printable──► commands.get(Command::Type::print, codepoint)
 * @endcode
 *
 * ## UTF-8 decoding
 *
 * Multi-byte UTF-8 sequences are accumulated byte-by-byte in `utf8Accumulator`.
 * `expectedUTF8Length()` determines the total byte count from the lead byte.
 * Once the expected number of continuation bytes have been received, the
 * codepoint is decoded and dispatched via `commands.get(Command::Type::print, …)`.
 * Invalid sequences are silently discarded (the accumulator is reset).
 *
 * ## Dispatch model
 *
 * | Action        | Command dispatched                                                  |
 * |---------------|---------------------------------------------------------------------|
 * | print         | `handlePrintByte()` → `accumulateUTF8Byte()` → `Command::print`   |
 * | execute       | `Command::execute` — C0/C1 control characters (CR, LF, BS, …)     |
 * | csiDispatch   | `Command::csiDispatch` — cursor, erase, SGR, mode, report          |
 * | escDispatch   | `Command::escDispatch` — ESC sequences (charset, DEC, …)           |
 * | oscEnd        | `Command::oscEnd` — title, clipboard (OSC 0/2/52)                  |
 * | hook/unhook   | `Command::dcsHook` / `Command::dcsUnhook` — DCS passthrough (Sixel)|
 * | apcEnd        | `Command::apcEnd` — APC termination (Kitty graphics)               |
 *
 * ## Thread model
 *
 * **All methods are READER THREAD only.**  Parser is constructed on the message
 * thread but `process()` and every method it calls run exclusively on the PTY
 * reader thread.  No locks are held during processing; cross-thread
 * communication goes through the commands map handlers.
 *
 * @see DispatchTable — O(1) state transition lookup table
 * @see CSI           — parameter accumulator for CSI sequences
 * @see Command       — external action enum dispatched via Function::Map
 */

#pragma once

#include <JuceHeader.h>

#include "../data/Command.h"
#include "../data/DispatchTable.h"
#include "../data/CSI.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Parser
 * @brief VT100/VT520 DFA byte decoder — decodes VT sequences and dispatches
 *        semantic actions via `jam::Function::Map<Command::Type, void>`.
 *
 * Parser owns the DFA state, intermediate and parameter accumulators, UTF-8
 * accumulation, and OSC/DCS/APC payload buffers.  It holds a reference to the
 * commands map and dispatches every decoded semantic action through it — it
 * does not write cells, move cursors, or touch any terminal state itself.
 *
 * @par Lifecycle
 * 1. Construct with a reference to a `jam::Function::Map<Command::Type, void>`.
 * 2. Feed raw PTY bytes via `process()` on the reader thread.
 *
 * @note All methods are READER THREAD only unless otherwise stated.
 *
 * @see DispatchTable — state transition table used internally
 * @see CSI           — CSI parameter accumulator
 * @see Command       — external action enum
 */
class Parser
{
public:
    /**
     * @brief Maximum number of intermediate bytes collected during ESC/CSI sequences.
     *
     * Intermediate bytes occupy the range 0x20–0x2F and appear between the
     * introducer and the final byte of an escape or CSI sequence.  Four slots
     * is more than sufficient for all standard VT sequences (most use zero or one).
     */
    static constexpr uint8_t MAX_INTERMEDIATES { 4 };

    /**
     * @brief Initial capacity of the OSC string hybrid buffer in bytes.
     *
     * OSC (Operating System Command) strings are terminated by BEL (0x07) or
     * ST (ESC \\).  512 bytes covers all practical title and clipboard payloads.
     * The buffer grows geometrically beyond this initial size as needed.
     */
    static constexpr int OSC_BUFFER_CAPACITY { 512 };

    /**
     * @brief Constructs the Parser and initialises the dispatch table.
     *
     * @param commands  Command dispatch map.  All decoded semantic actions
     *                  (print, execute, csiDispatch, …) are dispatched through
     *                  this map on the reader thread.
     *
     * @note MESSAGE THREAD — construction happens before the reader thread starts.
     */
    explicit Parser (jam::Function::Map<Command::Type, void>& commands) noexcept;

    /**
     * @brief Processes a block of raw bytes from the PTY.
     *
     * This is the hot path.  Each byte is looked up in the DispatchTable and
     * routed to the appropriate action handler.  The method processes the entire
     * `[data, data+length)` range before returning.
     *
     * @param data    Pointer to the first byte of the input buffer.
     *                Must not be null if `length > 0`.
     * @param length  Number of bytes to process.  May be zero (no-op).
     *
     * @note READER THREAD only.  Must not be called concurrently.
     */
    void process (const uint8_t* data, size_t length) noexcept;

private:
    /**
     * @brief Command dispatch map for all external terminal actions.
     *
     * All decoded semantic actions are dispatched through this map.  Never
     * accessed from the message thread during parsing.
     *
     * @note READER THREAD — all calls are on the reader thread.
     */
    jam::Function::Map<Command::Type, void>& commands;

    /**
     * @brief O(1) state transition lookup table.
     *
     * Maps `(ParserState, byte) → (nextState, Action)`.  Built once in the
     * constructor and never modified.  Immutable after construction, so it is
     * safe to read from any thread (though in practice only the reader thread
     * calls `get()`).
     *
     * @see DispatchTable
     */
    DispatchTable dispatchTable;

    /**
     * @brief Current state of the VT parser state machine.
     *
     * Starts at `ParserState::ground`.  Updated by `processTransition()` on
     * every byte that causes a state change.  The state determines how
     * subsequent bytes are interpreted.
     *
     * @see ParserState
     * @see processTransition()
     */
    ParserState currentState { ParserState::ground };

    /**
     * @brief CSI parameter accumulator for the current CSI sequence.
     *
     * Digits and separators are fed into this accumulator via `handleParam()`.
     * `csi.finalize()` is called before dispatching `Command::csiDispatch` to
     * commit the last parameter.  `csi.reset()` is called on entry to `csiEntry`
     * state.
     *
     * @see CSI
     * @see handleParam()
     */
    CSI csi;

    /**
     * @brief Buffer for intermediate bytes collected during ESC/CSI sequences.
     *
     * Intermediate bytes (0x20–0x2F) appear between the sequence introducer
     * and the final byte.  They qualify the meaning of the final byte (e.g.
     * `ESC ( B` selects ASCII charset: intermediate = '(', final = 'B').
     * At most `MAX_INTERMEDIATES` bytes are stored; extras are discarded.
     *
     * @see intermediateCount
     * @see MAX_INTERMEDIATES
     */
    uint8_t intermediateBuffer[MAX_INTERMEDIATES] {};

    /**
     * @brief Number of valid bytes currently stored in `intermediateBuffer`.
     *
     * Reset to zero on entry to `escape`, `csiEntry`, and `dcsEntry` states.
     * Incremented by the `collect` action handler.
     *
     * @invariant intermediateCount <= MAX_INTERMEDIATES
     */
    uint8_t intermediateCount { 0 };

    /**
     * @brief Partial UTF-8 sequence accumulation buffer (null-terminated).
     *
     * As multi-byte UTF-8 sequences arrive byte-by-byte, continuation bytes
     * are appended here.  When `utf8AccumulatorLength` reaches the expected
     * length (from `expectedUTF8Length()`), the codepoint is decoded and
     * dispatched via `Command::print`.  The buffer is 5 bytes: up to 4 UTF-8
     * bytes plus a null terminator.
     *
     * @see utf8AccumulatorLength
     * @see accumulateUTF8Byte()
     * @see expectedUTF8Length()
     */
    char utf8Accumulator[5] {};

    /**
     * @brief Number of bytes currently stored in `utf8Accumulator`.
     *
     * Zero means no multi-byte sequence is in progress.  When this equals
     * `expectedUTF8Length (utf8Accumulator[0])`, the sequence is complete.
     *
     * @see utf8Accumulator
     * @see accumulateUTF8Byte()
     */
    uint8_t utf8AccumulatorLength { 0 };

    /**
     * @brief Hybrid OSC payload buffer.  Lazy-allocated on first OSC sequence.
     *
     * OSC strings are accumulated here byte-by-byte as `oscPut` actions arrive.
     * When the OSC terminator (BEL or ST) is received, `Command::oscEnd` is
     * dispatched with a pointer to this buffer and `oscBufferSize`.  The buffer
     * is not null-terminated; `oscBufferSize` tracks the valid byte count.  Starts
     * unallocated and grows geometrically from `OSC_BUFFER_CAPACITY` on first use.
     *
     * @see oscBufferSize
     * @see oscBufferCapacity
     * @see OSC_BUFFER_CAPACITY
     * @see appendToBuffer()
     */
    juce::HeapBlock<uint8_t> oscBuffer;

    /**
     * @brief Number of valid bytes currently stored in `oscBuffer`.
     *
     * Reset to zero on entry to `oscString` state.
     *
     * @see oscBuffer
     */
    int oscBufferSize { 0 };

    /**
     * @brief Allocated capacity of `oscBuffer` in bytes.
     *
     * Zero until the first OSC sequence is received.  Doubles on overflow.
     *
     * @see oscBuffer
     */
    int oscBufferCapacity { 0 };

    /**
     * @brief Hybrid DCS payload buffer (Sixel).  Lazy-allocated on first DCS q sequence.
     *
     * DCS passthrough bytes are accumulated here byte-by-byte as `put` actions
     * arrive.  When the DCS terminator (ST) is received, `Command::dcsUnhook` is
     * dispatched with a pointer to this buffer and `dcsBufferSize`.  Grows
     * geometrically from 64 KB on first use.
     *
     * @see dcsBufferSize
     * @see dcsBufferCapacity
     * @see appendToBuffer()
     */
    juce::HeapBlock<uint8_t> dcsBuffer;

    /**
     * @brief Number of valid bytes currently stored in `dcsBuffer`.
     *
     * Reset to zero on entry to `dcsEntry` state.
     *
     * @see dcsBuffer
     */
    int dcsBufferSize { 0 };

    /**
     * @brief Allocated capacity of `dcsBuffer` in bytes.
     *
     * Zero until the first DCS sequence is received.  Doubles on overflow.
     *
     * @see dcsBuffer
     */
    int dcsBufferCapacity { 0 };

    /**
     * @brief Hybrid APC payload buffer (Kitty).  Lazy-allocated on first APC sequence.
     *
     * APC passthrough bytes are accumulated here byte-by-byte as `apcPut` actions
     * arrive.  When the APC terminator (ST) is received, `Command::apcEnd` is
     * dispatched with a pointer to this buffer and `apcBufferSize`.  Grows
     * geometrically from 64 KB on first use.
     *
     * @see apcBufferSize
     * @see apcBufferCapacity
     * @see appendToBuffer()
     */
    juce::HeapBlock<uint8_t> apcBuffer;

    /**
     * @brief Number of valid bytes currently stored in `apcBuffer`.
     *
     * Reset to zero on entry to `apcString` state.
     *
     * @see apcBuffer
     */
    int apcBufferSize { 0 };

    /**
     * @brief Allocated capacity of `apcBuffer` in bytes.
     *
     * Zero until the first APC sequence is received.  Doubles on overflow.
     *
     * @see apcBuffer
     */
    int apcBufferCapacity { 0 };

    /**
     * @brief Applies a state transition: exit action, transition action, entry action.
     *
     * Called by `process()` for every byte that produces a non-trivial
     * transition (i.e. state change or action).  Performs the two-phase
     * Williams model:
     * 1. `performAction (transition.action, byte)`.
     * 2. `performEntryAction (transition.nextState)` if the state is changing.
     *
     * @param byte        The input byte that triggered the transition.
     * @param transition  The `(nextState, action)` pair from the DispatchTable.
     *
     * @note READER THREAD only.
     *
     * @see performAction()
     * @see performEntryAction()
     */
    void processTransition (uint8_t byte, const Transition& transition) noexcept;

    /**
     * @brief Executes the action associated with a state transition.
     *
     * Dispatches to the appropriate handler based on `action`:
     * - `print`       → `handlePrintByte()`
     * - `execute`     → `commands.get (Command::Type::applyControlCode, byte)`
     * - `collect`     → appends to `intermediateBuffer`
     * - `param`       → `handleParam()`
     * - `escDispatch` → `commands.get (Command::Type::applyESC, …)`
     * - `csiDispatch` → `commands.get (Command::Type::applyCSI, …)`
     * - `oscPut`      → `appendToBuffer (oscBuffer, …)`
     * - `oscEnd`      → `commands.get (Command::Type::applyOSC, …)`
     * - `hook`        → `commands.get (Command::Type::storeDCSHeader, …)`
     * - `put`         → `appendToBuffer (dcsBuffer, …)`
     * - `unhook`      → `commands.get (Command::Type::applyDCSPayload, …)`
     * - `apcPut`      → `appendToBuffer (apcBuffer, …)`
     * - `apcEnd`      → `commands.get (Command::Type::applyAPCPayload, …)`
     * - `ignore`/`none` → no-op
     *
     * @param action  The action to perform.
     * @param byte    The input byte associated with the action.
     *
     * @note READER THREAD only.
     */
    void performAction (ParserAction action, uint8_t byte) noexcept;

    /**
     * @brief Handles a printable byte, routing it through UTF-8 accumulation.
     *
     * If the byte is a single-byte ASCII character (0x20–0x7E), dispatches
     * `Command::print` directly.  Otherwise forwards to `accumulateUTF8Byte()`
     * to begin or continue a multi-byte sequence.
     *
     * @param byte  The printable input byte.
     *
     * @note READER THREAD only.
     *
     * @see accumulateUTF8Byte()
     */
    void handlePrintByte (uint8_t byte) noexcept;

    /**
     * @brief Accumulates a byte into the UTF-8 sequence buffer.
     *
     * Appends `byte` to `utf8Accumulator`.  When the accumulated length equals
     * `expectedUTF8Length (utf8Accumulator[0])`, the sequence is decoded to a
     * Unicode codepoint and dispatched via `Command::print`.  Invalid sequences
     * (bad continuation bytes, overlong encodings) are silently discarded and
     * the accumulator is reset.
     *
     * @param byte  The next byte of the UTF-8 sequence (lead or continuation).
     *
     * @note READER THREAD only.
     *
     * @see expectedUTF8Length()
     */
    void accumulateUTF8Byte (uint8_t byte) noexcept;

    /**
     * @brief Processes a CSI parameter byte (digit, semicolon, or colon).
     *
     * Feeds the byte into the `csi` accumulator:
     * - '0'–'9' → `csi.addDigit()`
     * - ';' or ':' → `csi.addSeparator()`
     *
     * @param byte  The parameter byte (0x30–0x3B range).
     *
     * @note READER THREAD only.
     *
     * @see CSI::addDigit()
     * @see CSI::addSeparator()
     */
    void handleParam (uint8_t byte) noexcept;

    /**
     * @brief Returns the expected total byte length of a UTF-8 sequence from its lead byte.
     *
     * | Lead byte range | Sequence length |
     * |-----------------|-----------------|
     * | 0x00–0x7F       | 1               |
     * | 0xC0–0xDF       | 2               |
     * | 0xE0–0xEF       | 3               |
     * | 0xF0–0xF7       | 4               |
     * | Other           | 1 (fallback)    |
     *
     * @param leadByte  The first byte of the UTF-8 sequence.
     *
     * @return Expected total byte count (1–4).
     *
     * @note Pure function — no side effects.
     * @note READER THREAD only.
     */
    static uint8_t expectedUTF8Length (uint8_t leadByte) noexcept;

    /**
     * @brief Performs the entry action for a newly entered parser state.
     *
     * Called by `processTransition()` when the state changes.  Entry actions
     * reset state-specific accumulators:
     * - `csiEntry`  → `csi.reset()`, `intermediateCount = 0`
     * - `escape`    → `intermediateCount = 0`
     * - `oscString` → `oscBufferSize = 0`
     * - `apcString` → `apcBufferSize = 0`
     * - `dcsEntry`  → `csi.reset()`, `intermediateCount = 0`, `dcsBufferSize = 0`
     *
     * @param newState  The state being entered.
     *
     * @note READER THREAD only.
     */
    void performEntryAction (ParserState newState) noexcept;

    /**
     * @brief Appends a byte to a hybrid buffer with lazy allocation and geometric growth.
     *
     * On first call (`capacity == 0`), allocates `initialCapacity` bytes.  On
     * subsequent overflow (`size >= capacity`), doubles the capacity.  The existing
     * contents are preserved across reallocations via `std::memcpy`.
     *
     * @param buffer           The `HeapBlock` to append to.
     * @param size             Current number of valid bytes in `buffer` (in/out).
     * @param capacity         Current allocated capacity of `buffer` (in/out).
     * @param byte             The byte to append.
     * @param initialCapacity  Capacity to allocate on the first call.
     *
     * @note READER THREAD only.
     * @note `noexcept` — allocation failure on the reader thread is unrecoverable;
     *       `juce::HeapBlock::allocate` asserts in debug builds.
     */
    void appendToBuffer (juce::HeapBlock<uint8_t>& buffer, int& size, int& capacity, uint8_t byte, int initialCapacity) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Parser)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
