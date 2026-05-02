/**
 * @file Parser.h
 * @brief VT100/VT520 state machine parser for terminal emulation.
 *
 * Parser is the central byte-processing engine of the terminal emulator.  It
 * consumes raw bytes from the PTY reader thread, drives a Paul Flo Williams–
 * style state machine, decodes multi-byte UTF-8 sequences, and dispatches
 * semantic actions (print, execute, CSI dispatch, ESC dispatch, OSC handling,
 * DCS hook/unhook) to Grid and State.
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
 *    └──printable──► print (UTF-8 accumulation) ──► Grid::writeCell
 * @endcode
 *
 * ## UTF-8 decoding
 *
 * Multi-byte UTF-8 sequences are accumulated byte-by-byte in `utf8Accumulator`.
 * `expectedUTF8Length()` determines the total byte count from the lead byte.
 * Once the expected number of continuation bytes have been received, the
 * codepoint is decoded and forwarded to `print()`.  Invalid sequences are
 * silently discarded (the accumulator is reset).
 *
 * ## Dispatch model
 *
 * | Action        | Handler called                                      |
 * |---------------|-----------------------------------------------------|
 * | print         | `handlePrintByte()` → `accumulateUTF8Byte()` → `print()` |
 * | execute       | `execute()` — C0/C1 control characters (CR, LF, BS, …) |
 * | csiDispatch   | `csiDispatch()` — cursor, erase, SGR, mode, report  |
 * | escDispatch   | `escDispatch()` — ESC sequences (charset, DEC, …)   |
 * | oscEnd        | `oscDispatch()` — title, clipboard (OSC 0/2/52)     |
 * | hook/unhook   | `dcsHook()` / `dcsUnhook()` — DCS passthrough (Sixel) |
 * | apcEnd        | `apcEnd()` — APC termination (Kitty graphics)       |
 *
 * ## Thread model
 *
 * **All methods are READER THREAD only.**  Parser is constructed on the message
 * thread but `process()` and every method it calls run exclusively on the PTY
 * reader thread.  No locks are held during processing; cross-thread
 * communication goes through `State`'s atomic setters and the `writeToHost`
 * callback (which must be thread-safe on the caller's side).
 *
 * @see DispatchTable — O(1) state transition lookup table
 * @see CSI           — parameter accumulator for CSI sequences
 * @see Grid          — terminal screen buffer written by `print()` and erase ops
 * @see State         — atomic terminal parameter store updated by the parser
 */

#pragma once

#include <JuceHeader.h>
#include <jam_tui/jam_tui.h>

#include "../data/Charset.h"
#include "../data/DispatchTable.h"
#include "../data/CSI.h"
#include "../data/State.h"
#include "../data/CharProps.h"
#include "Grid.h"
#include "SixelDecoder.h"
#include "KittyDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Parser
 * @brief VT100/VT520 byte-stream state machine for terminal emulation.
 *
 * Parser owns the complete VT parsing pipeline: state machine transitions,
 * UTF-8 accumulation, intermediate/parameter collection, and dispatch to Grid
 * and State.  It is the only component that writes cell data to the Grid on
 * the reader thread.
 *
 * @par Lifecycle
 * 1. Construct with references to a `State` and a `Grid`.
 * 2. Call `calc()` once after construction (and after every `resize()`) to
 *    synchronise internal geometry from State.
 * 3. Feed raw PTY bytes via `process()` on the reader thread.
 * 4. Call `flushResponses()` after `process()` to deliver any queued device
 *    responses (DA, CPR, …) through `writeToHost`.
 *
 * @par Callbacks
 * The four `std::function` members are set by the owner (Session) before the
 * first call to `process()`.  They are invoked on the READER THREAD and must
 * be thread-safe.
 *
 * @note All methods are READER THREAD only unless otherwise stated.
 *
 * @see DispatchTable — state transition table used internally
 * @see CSI           — CSI parameter accumulator
 * @see Grid          — screen buffer target for print and erase operations
 * @see State         — atomic terminal parameter store
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
     * @brief Maximum number of characters accepted from an OSC title string.
     *
     * Applied when extracting the window title from an OSC 0 or OSC 2 sequence.
     * Titles longer than this are truncated before being written to State.
     */
    static constexpr int MAX_OSC_TITLE_LENGTH { 256 };

    /**
     * @brief Constructs the Parser and initialises the dispatch table.
     *
     * The constructor does not call `calc()`.  The owner must call `calc()`
     * after construction (and after every `resize()`) to synchronise the
     * parser's internal geometry with the current State values.
     *
     * @param state  Reference to the shared terminal parameter store.
     *               The parser reads cursor position, mode flags, and screen
     *               dimensions from State and writes updates back via setters.
     * @param writer Write-only facade to the screen buffer.  The parser writes
     *               cells, erases regions, and scrolls lines through Writer.
     *
     * @note MESSAGE THREAD — construction happens before the reader thread starts.
     *
     * @see calc()
     */
    explicit Parser (State& state, Grid::Writer writer) noexcept;

    /**
     * @brief Processes a block of raw bytes from the PTY.
     *
     * This is the hot path.  Each byte is looked up in the DispatchTable and
     * routed to the appropriate action handler.  The method processes the entire
     * `[data, data+length)` range before returning.
     *
     * @par Fast path
     * When the parser is in the `ground` state and the input contains a run of
     * printable ASCII bytes, `processGroundChunk()` is called to handle the
     * entire run without per-byte dispatch overhead.
     *
     * @param data    Pointer to the first byte of the input buffer.
     *                Must not be null if `length > 0`.
     * @param length  Number of bytes to process.  May be zero (no-op).
     *
     * @note READER THREAD only.  Must not be called concurrently.
     * @note Does not flush responses — call `flushResponses()` after this method.
     *
     * @see flushResponses()
     * @see processGroundChunk()
     */
    void process (const uint8_t* data, size_t length) noexcept;

    /**
     * @brief Notifies the parser that the terminal dimensions have changed.
     *
     * Updates State with the new geometry and re-clamps the cursor and scroll
     * region to the new bounds.  Must be followed by a call to `calc()` to
     * propagate the change to internal cached values.
     *
     * @param newCols         New terminal width in character columns.
     * @param newVisibleRows  New terminal height in visible rows.
     *
     * @note MESSAGE THREAD — called from Processor::resized().
     *
     * @see calc()
     */
    void resize (int newCols, int newVisibleRows) noexcept;

    /**
     * @brief Resets the parser and terminal to a clean initial state.
     *
     * Clears the state machine (returns to `ground`), resets the pen, cursor,
     * scroll region, tab stops, and all mode flags.  Equivalent to a hard
     * terminal reset (RIS, ESC c).
     *
     * @note READER THREAD only.
     */
    void reset() noexcept;

    /**
     * @brief Synchronises internal cached geometry from State.
     *
     * Reads `State::getCols()`, `State::getVisibleRows()`, and the scroll
     * region bottom from State and caches them in `scrollBottom`.  Must be
     * called after construction and after every `resize()`.
     *
     * @note READER THREAD only.
     *
     * @see resize()
     */
    void calc() noexcept;

    /**
     * @brief Callback invoked to send a response string back to the PTY host.
     *
     * Set by the owner (Session) before the first call to `process()`.
     * Called on the READER THREAD when the parser needs to reply to a device
     * attribute request (DA), cursor position report (CPR), or similar query.
     *
     * The callback receives a pointer to a null-terminated C string and its
     * byte length (excluding the null terminator).
     *
     * @par Example
     * @code
     * parser.writeToHost = [&] (const char* data, int len)
     * {
     *     ptyWriter.write (data, len);  // thread-safe PTY write
     * };
     * @endcode
     *
     * @note READER THREAD — the callback must be thread-safe.
     */
    std::function<void (const char*, int)> writeToHost;

    /**
     * @brief Callback invoked when the terminal requests a clipboard write.
     *
     * Triggered by OSC 52 (`ESC]52;c;<base64-data>BEL`).  The argument is
     * the decoded clipboard text.
     *
     * @note READER THREAD — the callback must be thread-safe.
     *
     * @see handleOscClipboard()
     */
    std::function<void (const juce::String&)> onClipboardChanged;

    /**
     * @brief Callback invoked when the terminal emits a BEL character (0x07).
     *
     * The owner (Session / UI) should produce an audible or visual bell.
     *
     * @note READER THREAD — the callback must be thread-safe.
     */
    std::function<void()> onBell;

    /**
     * @brief Callback invoked when the terminal requests a desktop notification.
     *
     * Triggered by OSC 9 (`ESC]9;<body>BEL`) and OSC 777
     * (`ESC]777;notify;<title>;<body>BEL`).  For OSC 9, `title` is empty.
     *
     * @note READER THREAD — dispatched to the message thread via `callAsync`.
     *
     * @see handleOscNotification()
     * @see handleOsc777()
     */
    std::function<void (const juce::String&, const juce::String&)> onDesktopNotification;

    /**
     * @brief Callback invoked when an inline image is decoded on the READER THREAD.
     *
     * Decoders (Sixel, iTerm2, Kitty) invoke this after decoding an image.
     * The callback transfers decoded pixel data and grid position metadata
     * to the renderer's staging pipeline without any direct header coupling
     * between Parser and Image.
     *
     * @param pixels     All frames contiguous, RGBA8, row-major.  Ownership transferred via move.
     * @param delays     Per-frame delay in milliseconds.  Null for static images.  Ownership transferred.
     * @param frameCount Number of frames (1 = static).
     * @param widthPx    Frame width in pixels.
     * @param heightPx   Frame height in pixels.
     * @param gridRow    Absolute grid row of image placement.
     * @param gridCol    Grid column of image placement.
     * @param cellCols   Image span in cell columns.
     * @param cellRows   Image span in cell rows.
     *
     * @note READER THREAD only.
     */
    std::function<void (juce::HeapBlock<uint8_t>&&,
                        juce::HeapBlock<int>&&,
                        int, int, int,
                        int, int, int, int,
                        bool)> onImageDecoded;

    /**
     * @brief Callback invoked when a SKiT END; filepath is received on the READER THREAD.
     *
     * Fires when `handleSkitFilepath` processes a non-empty filepath.  The file
     * is NOT loaded here — the receiver must load it on the MESSAGE THREAD.
     * An empty filepath signals preview dismissal.
     *
     * @param filepath  Absolute path to the image file, or empty string to dismiss.
     * @param gridRow   Absolute grid row at the trigger cursor position.
     *
     * @note READER THREAD only.
     */
    std::function<void (const juce::String& filepath, int gridRow)> onPreviewFile;

    // =========================================================================

    /**
     * @brief Delivers any queued device responses to the host via `writeToHost`.
     *
     * During `process()`, responses (DA, CPR, …) are accumulated in
     * `responseBuf` rather than sent immediately.  Call this method after
     * `process()` returns to flush the buffer through `writeToHost`.
     *
     * @note READER THREAD only.
     *
     * @see sendResponse()
     * @see writeToHost
     */
    void flushResponses() noexcept;

    /**
     * @brief Sets the callback fired when the scrollback row count changes.
     *
     * Forwards the callback to the internal `Grid::Writer`.  Must be called on
     * the MESSAGE THREAD before the reader thread starts.
     *
     * @param callback  Called with the updated scrollback count after any scroll
     *                  or clear operation.
     *
     * @note MESSAGE THREAD — set before `process()` is first called.
     */
    void setScrollbackCallback (std::function<void (int)> callback) noexcept;

    /**
     * @brief Updates the physical cell dimensions used by image decoders.
     *
     * Called by `Screen::calc()` on the MESSAGE THREAD whenever the font metrics
     * change.  The READER THREAD reads these via `physCellWidthAtomic` and
     * `physCellHeightAtomic` inside `dcsUnhook()` when writing image cells to
     * the grid.  Zero values (initial state) suppress image cell writing until
     * real dimensions are known.
     *
     * @param widthPx   Physical cell width in pixels (HiDPI-scaled).
     * @param heightPx  Physical cell height in pixels (HiDPI-scaled).
     *
     * @note MESSAGE THREAD — store is `memory_order_relaxed`; the READER THREAD
     *       will see the value on its next pass (eventual consistency is acceptable
     *       because image decode is not latency-critical).
     */
    void setPhysCellDimensions (int widthPx, int heightPx) noexcept;

private:
    /**
     * @brief Reference to the shared terminal parameter store.
     *
     * The parser reads cursor position, mode flags, and screen dimensions from
     * State and writes updates back via atomic setters.  Never accessed from
     * the message thread during parsing.
     *
     * @note READER THREAD — all accesses are through State's atomic API.
     */
    State& state;

    /**
     * @brief Write-only facade to the terminal screen buffer.
     *
     * The parser writes cells, erases regions, and scrolls lines through Writer.
     * No geometry reads — those route through State.
     */
    Grid::Writer writer;

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
     * `csi.finalize()` is called before `csiDispatch()` to commit the last
     * parameter.  `csi.reset()` is called on entry to `csiEntry` state.
     *
     * @see CSI
     * @see handleParam()
     * @see csiDispatch()
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
     * forwarded to `print()`.  The buffer is 5 bytes: up to 4 UTF-8 bytes
     * plus a null terminator.
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
     * When the OSC terminator (BEL or ST) is received, `oscDispatch()` is called
     * with a pointer to this buffer and `oscBufferSize`.  The buffer is not
     * null-terminated; `oscBufferSize` tracks the valid byte count.  Starts
     * unallocated and grows geometrically from `OSC_BUFFER_CAPACITY` on first use.
     *
     * @see oscBufferSize
     * @see oscBufferCapacity
     * @see OSC_BUFFER_CAPACITY
     * @see oscDispatch()
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
     * arrive.  When the DCS terminator (ST) is received, `dcsUnhook()` is called.
     * Grows geometrically from 64 KB on first use.
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
     * @brief Final byte from DCS header, recorded by `performAction()` hook case
     *        for dispatch in `dcsUnhook()`.
     *
     * @see dcsHook()
     * @see dcsUnhook()
     */
    uint8_t dcsFinalByte { 0 };

    /**
     * @brief Hybrid APC payload buffer (Kitty).  Lazy-allocated on first APC sequence.
     *
     * APC passthrough bytes are accumulated here byte-by-byte as `apcPut` actions
     * arrive.  When the APC terminator (ST) is received, `apcEnd()` is called.
     * Grows geometrically from 64 KB on first use.
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
     * @brief Kitty graphics protocol decoder.
     *
     * Accumulates chunked APC payloads across multiple `apcPut`/`apcEnd` cycles
     * and decodes the final image when the last chunk arrives (`m=0`).
     * Persistent member because Kitty uses multi-chunk transmissions — unlike
     * Sixel and iTerm2 which are single parse events.
     *
     * @see apcEnd()
     */
    KittyDecoder kittyDecoder;

    /**
     * @brief Physical cell width in pixels, set by Screen::calc() on MESSAGE THREAD.
     *
     * Read by `dcsUnhook()` and `apcEnd()` on the READER THREAD to pass to the
     * `onImageDecoded` callback for image cell placement.  Uses relaxed ordering —
     * eventual consistency is acceptable.
     *
     * @see setPhysCellDimensions()
     * @see onImageDecoded
     */
    std::atomic<int> physCellWidthAtomic { 0 };

    /**
     * @brief Physical cell height in pixels, set by Screen::calc() on MESSAGE THREAD.
     *
     * @see setPhysCellDimensions()
     */
    std::atomic<int> physCellHeightAtomic { 0 };

    /**
     * @brief Pixel-to-cell conversion helper; rebuilt by setPhysCellDimensions() whenever
     *        physCellWidthAtomic / physCellHeightAtomic are updated.
     *
     * Consumed by dcsUnhook(), apcEnd(), and handleSkitFilepath() on the READER
     * THREAD to compute image cell spans.  Scale is always 1.0 — the parser
     * operates exclusively in physical pixels.
     *
     * @see setPhysCellDimensions()
     */
    jam::tui::Metrics metrics {};

    /**
     * @brief Current drawing attributes applied to newly written cells.
     *
     * Mutated by SGR sequences (`applySGR()` / `handleSGR()`).  Copied into
     * each Cell written by `print()`.  Saved and restored by DECSC/DECRC
     * (ESC 7 / ESC 8) via `stamp`.
     *
     * @see Pen
     * @see stamp
     * @see applySGR()
     */
    Pen pen {};

    /**
     * @brief Saved cursor state for DECSC/DECRC (ESC 7 / ESC 8).
     *
     * Per-screen saved state: cursor position, pen, wrap-pending, origin
     * mode, and line-drawing charset.  Indexed by `ActiveScreen`.
     */
    struct SavedCursor
    {
        int row { 0 };
        int col { 0 };
        Pen pen {};
        bool wrapPending { false };
        bool originMode { false };
        bool lineDrawing { false };
    };

    /// Per-screen DECSC saved cursor state (normal / alternate).
    std::array<SavedCursor, 2> savedCursor {};

    /**
     * @brief Saved pen state — legacy alias kept for `bg` fill in erase ops.
     *
     * `stamp.bg` is used as the fill colour for scroll and erase operations.
     * Kept in sync with `pen` by SGR handlers.
     *
     * @see pen
     */
    Pen stamp;

    /** @brief Last graphic character printed, for REP (CSI Ps b). */
    uint32_t lastGraphicChar { 0 };

    /**
     * @brief Returns the effective bottom row of the current scrolling region.
     *
     * Reads from Grid buffer dims (safe on reader thread) on every call.
     * Wraps `effectiveScrollBottom()` with the current screen and visible rows.
     *
     * @see effectiveScrollBottom()
     */
    int activeScrollBottom() const noexcept;

    /**
     * @brief Whether the VT100 line-drawing character set is active in GL.
     *
     * When `true`, printable bytes in the range 0x60–0x7E are mapped to
     * box-drawing characters (DEC Special Graphics).  Set by SO (0x0E)
     * when G1 contains line-drawing, cleared by SI (0x0F) to return to G0.
     */
    bool useLineDrawing { false };

    /// Whether G0 is designated as DEC Special Graphics (ESC ( 0).
    bool g0LineDrawing { false };

    /// Whether G1 is designated as DEC Special Graphics (ESC ) 0).
    bool g1LineDrawing { false };

    /**
     * @brief Unicode grapheme segmentation state for cluster boundary detection.
     *
     * Tracks the Unicode grapheme cluster break algorithm state across
     * successive codepoints.  Used by `print()` to decide whether a new
     * codepoint starts a new cluster or extends the previous one.
     *
     * @see graphemeSegmentationInit()
     * @see GraphemeSegmentationResult
     */
    GraphemeSegmentationResult graphemeState { graphemeSegmentationInit() };

    /**
     * @brief Buffer for device response strings queued during `process()`.
     *
     * Responses (DA, CPR, …) are written here by `sendResponse()` rather than
     * sent immediately.  `flushResponses()` delivers the accumulated content
     * through `writeToHost` after `process()` returns.
     *
     * @see responseLen
     * @see sendResponse()
     * @see flushResponses()
     */
    char responseBuf[256] {};

    /**
     * @brief Number of valid bytes currently stored in `responseBuf`.
     *
     * Reset to zero by `flushResponses()` after delivery.
     *
     * @see responseBuf
     */
    int responseLen { 0 };

    /**
     * @brief Link ID of the currently active OSC 8 hyperlink, or 0 when no link is open.
     *
     * Set by `handleOsc8()` via `state.registerLinkUri()` when an OSC 8 open
     * is received.  Cleared to 0 on OSC 8 close.  Stamped onto every cell
     * written by `print()` and `processGroundChunk()` while non-zero.
     *
     * Lives in Parser (not Pen) because hyperlink state must not survive
     * DECSC/DECRC cursor save/restore.
     */
    uint16_t activeLinkId { 0 };

    // =========================================================================
    /** @name SGR and mode helpers
     * @{ */

    /**
     * @brief Applies a single SGR parameter group to the active pen.
     *
     * Handles one logical SGR parameter (which may span multiple raw parameters
     * for 256-colour and 24-bit RGB colour sequences).  Called iteratively by
     * `handleSGR()` for each parameter in the CSI sequence.
     *
     * @param params  The full CSI parameter set for the SGR sequence.
     *
     * @note READER THREAD only.
     *
     * @see handleSGR()
     * @see Pen
     */
    void applySGR (const CSI& params) noexcept;

    /**
     * @brief Dispatches an SGR (Select Graphic Rendition) CSI sequence.
     *
     * Iterates over all parameters in `params` and calls `applySGR()` for each
     * logical group.  An empty parameter list (`ESC[m`) resets the pen to
     * default attributes.
     *
     * @param params  The CSI parameter set for the SGR sequence (final byte 'm').
     *
     * @note READER THREAD only.
     *
     * @see applySGR()
     */
    void handleSGR (const CSI& params) noexcept;

    /**
     * @brief Resets all terminal mode flags to their power-on defaults.
     *
     * Called by `reset()` and by the RIS (ESC c) handler.  Writes default
     * values for autoWrap, originMode, cursor visibility, and all other
     * mode flags through `State::setMode()`.
     *
     * @note READER THREAD only.
     */
    void resetModes() noexcept;

    /**
     * @brief Resets the active pen to default attributes (no style, default colours).
     *
     * Called by `reset()` and by SGR 0 (reset all attributes).
     *
     * @note READER THREAD only.
     *
     * @see pen
     */
    void resetPen() noexcept;

    /**
     * @brief Resets the cursor to the home position and clears wrap-pending.
     *
     * Moves the cursor to row 0, column 0 on both screens and clears the
     * wrap-pending flag.  Called by `reset()`.
     *
     * @param cols  Current terminal column count (used to clamp the cursor).
     *
     * @note READER THREAD only.
     */
    void resetCursor (int cols) noexcept;

    /** @} */

    // =========================================================================
    /** @name Cursor movement primitives
     *
     * These methods implement the low-level cursor movement operations used by
     * both CSI dispatch handlers and internal logic.  All coordinates are
     * zero-based.  Clamping to valid bounds is the responsibility of each method.
     * @{ */

    /**
     * @brief Moves the cursor up by `count` rows, clamped to the scroll region top.
     *
     * @param s      Target screen buffer.
     * @param count  Number of rows to move up (>= 1).
     *
     * @note READER THREAD only.
     */
    void cursorMoveUp (ActiveScreen s, int count) noexcept;

    /**
     * @brief Moves the cursor down by `count` rows, clamped to `bottom`.
     *
     * @param s      Target screen buffer.
     * @param count  Number of rows to move down (>= 1).
     * @param bottom Zero-based index of the last row the cursor may reach.
     *
     * @note READER THREAD only.
     */
    void cursorMoveDown (ActiveScreen s, int count, int bottom) noexcept;

    /**
     * @brief Moves the cursor right by `count` columns, clamped to `cols - 1`.
     *
     * @param s     Target screen buffer.
     * @param count Number of columns to move right (>= 1).
     * @param cols  Current terminal column count (right margin = cols - 1).
     *
     * @note READER THREAD only.
     */
    void cursorMoveForward (ActiveScreen s, int count, int cols) noexcept;

    /**
     * @brief Moves the cursor left by `count` columns, clamped to column 0.
     *
     * @param s     Target screen buffer.
     * @param count Number of columns to move left (>= 1).
     *
     * @note READER THREAD only.
     */
    void cursorMoveBackward (ActiveScreen s, int count) noexcept;

    /**
     * @brief Sets the cursor to an absolute (row, col) position.
     *
     * Coordinates are zero-based.  Values are clamped to the valid screen area.
     * Does not respect origin mode — use `cursorSetPositionInOrigin()` for that.
     *
     * @param s           Target screen buffer.
     * @param row         Zero-based target row.
     * @param col         Zero-based target column.
     * @param cols        Current terminal column count.
     * @param visibleRows Current terminal visible row count.
     *
     * @note READER THREAD only.
     *
     * @see cursorSetPositionInOrigin()
     */
    void cursorSetPosition (ActiveScreen s, int row, int col, int cols, int visibleRows) noexcept;

    /**
     * @brief Sets the cursor position relative to the scroll region origin.
     *
     * When origin mode (DECOM) is active, row 1 refers to the top of the
     * scrolling region rather than the top of the screen.  This method applies
     * the appropriate offset before calling `cursorSetPosition()`.
     *
     * @param s           Target screen buffer.
     * @param row         One-based target row (relative to scroll region top in origin mode).
     * @param col         One-based target column.
     * @param cols        Current terminal column count.
     * @param visibleRows Current terminal visible row count.
     *
     * @note READER THREAD only.
     *
     * @see cursorSetPosition()
     */
    void cursorSetPositionInOrigin (ActiveScreen s, int row, int col, int cols, int visibleRows) noexcept;

    /**
     * @brief Advances the cursor to the next line, scrolling if at the bottom.
     *
     * If the cursor is at the bottom of the scrolling region, the region is
     * scrolled up by one line (the top line is discarded, a blank line is
     * inserted at the bottom).  Otherwise the cursor simply moves down one row.
     *
     * @param s      Target screen buffer.
     * @param bottom Zero-based index of the last row of the scrolling region.
     *
     * @return `true` if a scroll occurred, `false` if the cursor just moved down.
     *
     * @note READER THREAD only.
     *
     * @see executeLineFeed()
     */
    bool cursorGoToNextLine (ActiveScreen s, int bottom, int visibleRows) noexcept;

    /**
     * @brief Clamps the cursor to the valid screen area after a resize.
     *
     * Ensures the cursor row and column do not exceed the new terminal
     * dimensions.  Called by `resize()` after updating State.
     *
     * @param s           Target screen buffer.
     * @param cols        New terminal column count.
     * @param visibleRows New terminal visible row count.
     *
     * @note READER THREAD only.
     */
    void cursorClamp (ActiveScreen s, int cols, int visibleRows) noexcept;

    /**
     * @brief Saves cursor position and associated state for DECSC (ESC 7).
     *
     * Captures cursor row, column, pen, wrap-pending flag, origin mode, and
     * line-drawing charset into `savedCursor[scr]`.  Called by ESC 7 and by
     * the ?1049h alternate-screen entry path.
     *
     * @param scr  Target screen whose cursor state is saved.
     *
     * @note READER THREAD only.
     *
     * @see restoreCursor()
     * @see savedCursor
     */
    void saveCursor (ActiveScreen scr) noexcept;

    /**
     * @brief Restores cursor position and associated state for DECRC (ESC 8).
     *
     * Applies the state previously saved by `saveCursor()` back to live State
     * and Parser members: cursor row/col, pen, stamp, wrap-pending, origin
     * mode, and line-drawing charset.  Called by ESC 8 and by the ?1049l
     * alternate-screen exit path.
     *
     * @param scr  Target screen whose cursor state is restored.
     *
     * @note READER THREAD only.
     *
     * @see saveCursor()
     * @see savedCursor
     */
    void restoreCursor (ActiveScreen scr) noexcept;

    /**
     * @brief Sets the scrolling region (DECSTBM) for the specified screen.
     *
     * The scrolling region is defined by `top` and `bottom` (zero-based).
     * After setting the region, the cursor is moved to the home position.
     *
     * @param s      Target screen buffer.
     * @param top    Zero-based index of the first scrolling row.
     * @param bottom Zero-based index of the last scrolling row.
     *
     * @note READER THREAD only.
     *
     * @see cursorResetScrollRegion()
     */
    void cursorSetScrollRegion (ActiveScreen s, int top, int bottom) noexcept;

    /**
     * @brief Resets the scrolling region to the full screen height.
     *
     * Equivalent to `DECSTBM 1;<visibleRows>`.  Called by `reset()` and when
     * the alternate screen is activated.
     *
     * @param s  Target screen buffer.
     *
     * @note READER THREAD only.
     *
     * @see cursorSetScrollRegion()
     */
    void cursorResetScrollRegion (ActiveScreen s) noexcept;

    /**
     * @brief Returns the effective scroll-region bottom row for the given screen.
     *
     * If the stored scroll bottom is 0 (unset), returns `visibleRows - 1`.
     * Otherwise returns the stored value clamped to `visibleRows - 1`.
     *
     * @param s           Target screen buffer.
     * @param visibleRows Current terminal visible row count.
     *
     * @return Zero-based index of the last row of the scrolling region.
     *
     * @note READER THREAD only.
     */
    int effectiveScrollBottom (ActiveScreen s, int visibleRows) const noexcept;

    /**
     * @brief Returns the effective downward clamp for cursor movement on screen `s`.
     *
     * If the cursor is within the scroll margins (row >= scrollTop and
     * row <= scrollBottom), returns `scrollBottom`.  Otherwise returns
     * `visibleRows - 1`.  Used by `moveCursorDown()` and `moveCursorNextLine()`
     * to avoid duplicating the margin-awareness check.
     *
     * @param s  Target screen buffer.
     *
     * @return Zero-based index of the last row the cursor may reach moving down.
     *
     * @note READER THREAD only.
     */
    int effectiveClampBottom (ActiveScreen s) const noexcept;

    /** @} */

    // =========================================================================
    /** @name Tab stop management
     * @{ */

    /**
     * @brief Per-column tab stop flags.
     *
     * A vector of `char` values (0 or 1) indexed by column.  A non-zero value
     * at index `c` means column `c` is a tab stop.  Initialised by
     * `initializeTabStops()` with stops every 8 columns.
     *
     * @see initializeTabStops()
     * @see nextTabStop()
     */
    std::vector<char> tabStops;

    /**
     * @brief Initialises tab stops to the standard 8-column interval.
     *
     * Resizes `tabStops` to `numCols` and sets a stop at every column that is
     * a multiple of 8 (columns 8, 16, 24, …).  Called by `reset()` and
     * `resize()`.
     *
     * @param numCols  Number of columns in the terminal.
     *
     * @note READER THREAD only.
     */
    void initializeTabStops (int numCols) noexcept;

    /**
     * @brief Returns the column index of the next tab stop to the right of the cursor.
     *
     * If no tab stop exists to the right, returns `cols - 1` (the right margin).
     *
     * @param s     Target screen buffer (used to read the current cursor column).
     * @param cols  Current terminal column count.
     *
     * @return Zero-based column index of the next tab stop.
     *
     * @note READER THREAD only.
     */
    int nextTabStop (ActiveScreen s, int cols) noexcept;

    /**
     * @brief Returns the column index of the previous tab stop to the left of the cursor.
     *
     * Scans `tabStops` from `cursorCol - 1` leftward, returning the first column
     * with a non-zero entry.  If no tab stop exists to the left, returns 0.
     *
     * @param s  Target screen buffer (used to read the current cursor column).
     *
     * @return Zero-based column index of the previous tab stop, or 0 if none.
     *
     * @note READER THREAD only.
     *
     * @see nextTabStop()  — forward direction counterpart
     */
    int prevTabStop (ActiveScreen s) noexcept;

    /**
     * @brief Sets a tab stop at the current cursor column.
     *
     * Corresponds to the HTS (Horizontal Tab Set, ESC H) sequence.
     *
     * @param s  Target screen buffer (used to read the current cursor column).
     *
     * @note READER THREAD only.
     */
    void setTabStop (ActiveScreen s) noexcept;

    /**
     * @brief Clears the tab stop at the current cursor column.
     *
     * Corresponds to `CSI 0 g` (TBC — Tab Clear, current column).
     *
     * @param s  Target screen buffer (used to read the current cursor column).
     *
     * @note READER THREAD only.
     */
    void clearTabStop (ActiveScreen s) noexcept;

    /**
     * @brief Clears all tab stops.
     *
     * Corresponds to `CSI 3 g` (TBC — Tab Clear, all columns).
     *
     * @note READER THREAD only.
     */
    void clearAllTabStops() noexcept;

    /** @} */

    // =========================================================================
    /** @name State machine core
     * @{ */

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
     * - `execute`     → `execute()`
     * - `collect`     → appends to `intermediateBuffer`
     * - `param`       → `handleParam()`
     * - `escDispatch` → `escDispatch()`
     * - `csiDispatch` → `csiDispatch()`
     * - `oscPut`      → `appendToBuffer (oscBuffer, …)`
     * - `oscEnd`      → `oscDispatch()`
     * - `hook`        → `dcsHook()` (records `dcsFinalByte`)
     * - `put`         → `appendToBuffer (dcsBuffer, …)`
     * - `unhook`      → `dcsUnhook()`
     * - `apcPut`      → `appendToBuffer (apcBuffer, …)`
     * - `apcEnd`      → `apcEnd()`
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
     * If the byte is a single-byte ASCII character (0x20–0x7E), it is passed
     * directly to `print()`.  Otherwise it is forwarded to `accumulateUTF8Byte()`
     * to begin or continue a multi-byte sequence.
     *
     * @param byte  The printable input byte.
     *
     * @note READER THREAD only.
     *
     * @see accumulateUTF8Byte()
     * @see print()
     */
    void handlePrintByte (uint8_t byte) noexcept;

    /**
     * @brief Accumulates a byte into the UTF-8 sequence buffer.
     *
     * Appends `byte` to `utf8Accumulator`.  When the accumulated length equals
     * `expectedUTF8Length (utf8Accumulator[0])`, the sequence is decoded to a
     * Unicode codepoint and forwarded to `print()`.  Invalid sequences (bad
     * continuation bytes, overlong encodings) are silently discarded and the
     * accumulator is reset.
     *
     * @param byte  The next byte of the UTF-8 sequence (lead or continuation).
     *
     * @note READER THREAD only.
     *
     * @see expectedUTF8Length()
     * @see print()
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


    /** @} */

    // =========================================================================
    /** @name Core dispatch actions
     * @{ */

    /**
     * @brief Writes a Unicode codepoint to the active screen at the cursor position.
     *
     * Handles wrap-pending resolution, wide character placement, grapheme cluster
     * extension, line-drawing character substitution, and cursor advancement.
     * After writing, the cursor is advanced (or wrap-pending is set if at the
     * right margin with autoWrap enabled).
     *
     * @param codepoint  Unicode scalar value to print (U+0000–U+10FFFF).
     *
     * @note READER THREAD only.
     *
     * @see resolveWrapPending()
     * @see handlePrintByte()
     */
    void print (uint32_t codepoint) noexcept;

    /**
     * @brief Resolves a pending line wrap before writing a new character.
     *
     * When `State::isWrapPending()` is true and a printable character arrives,
     * the cursor is moved to column 0 of the next line (scrolling if necessary)
     * before the character is written.  The wrap-pending flag is then cleared.
     *
     * @param scr  Target screen buffer.
     *
     * @note READER THREAD only.
     *
     * @see print()
     * @see cursorGoToNextLine()
     */
    void resolveWrapPending (ActiveScreen scr) noexcept;

    /**
     * @brief Processes a contiguous run of ground-state printable ASCII bytes.
     *
     * Fast path for the common case where the parser is in `ground` state and
     * the input contains a run of bytes in the range 0x20–0x7E.  Avoids
     * per-byte dispatch table lookups for the run.
     *
     * @param data    Pointer to the first byte of the run.
     * @param length  Maximum number of bytes to consume.
     *
     * @return Number of bytes consumed from `data`.
     *
     * @note READER THREAD only.
     *
     * @see process()
     */
    size_t processGroundChunk (const uint8_t* data, size_t length) noexcept;

    /**
     * @brief Executes a C0 or C1 control character.
     *
     * Handles the standard control characters:
     * - 0x07 BEL → `onBell()`
     * - 0x08 BS  → cursor left
     * - 0x09 HT  → advance to next tab stop
     * - 0x0A LF / 0x0B VT / 0x0C FF → `executeLineFeed()`
     * - 0x0D CR  → cursor to column 0
     * - 0x0E SO  → enable line-drawing charset
     * - 0x0F SI  → disable line-drawing charset
     *
     * @param controlByte  The control character byte (0x00–0x1F or 0x80–0x9F).
     *
     * @note READER THREAD only.
     */
    void execute (uint8_t controlByte) noexcept;

    /**
     * @brief Performs a line feed, advancing the cursor or scrolling the region.
     *
     * If LNM (line-feed / new-line mode) is active, also moves the cursor to
     * column 0.  Delegates to `cursorGoToNextLine()` for the actual movement
     * and scroll logic.
     *
     * @param scr  Target screen buffer.
     *
     * @note READER THREAD only.
     *
     * @see cursorGoToNextLine()
     * @see execute()
     */
    void executeLineFeed (ActiveScreen scr) noexcept;

    /**
     * @brief Dispatches a complete CSI sequence to the appropriate handler.
     *
     * Called after the CSI final byte is received.  Routes to the correct
     * handler based on `finalByte` and the presence of private-mode markers
     * in `intermediates`:
     *
     * | Final byte | Handler                          |
     * |------------|----------------------------------|
     * | 'A'        | `moveCursorUp()`                 |
     * | 'B'        | `moveCursorDown()`               |
     * | 'C'        | `moveCursorForward()`            |
     * | 'D'        | `moveCursorBackward()`           |
     * | 'E'        | `moveCursorNextLine()`           |
     * | 'F'        | `moveCursorPrevLine()`           |
     * | 'G'        | `setCursorColumn()`              |
     * | 'H'/'f'    | `setCursorPosition()`            |
     * | 'J'        | `eraseInDisplay()`               |
     * | 'K'        | `eraseInLine()`                  |
     * | 'L'        | `shiftLinesDown()`               |
     * | 'M'        | `shiftLinesUp()`                 |
     * | 'P'        | `removeCells()`                  |
     * | 'S'        | `scrollUp()`                     |
     * | 'T'        | `scrollDown()`                   |
     * | 'X'        | `eraseCells()`                   |
     * | '@'        | `shiftCellsRight()`              |
     * | 'd'        | `setCursorLine()`                |
     * | 'g'        | tab stop clear                   |
     * | 'h'/'l'    | `handleMode()` / `handlePrivateMode()` |
     * | 'm'        | `handleSGR()`                    |
     * | 'n'        | `reportCursorPosition()`         |
     * | 'r'        | `setScrollRegion()`              |
     * | 'c'        | `reportDeviceAttributes()`       |
     *
     * @param params             Finalised CSI parameter accumulator.
     * @param intermediates      Pointer to the intermediate byte buffer.
     * @param intermediateCount  Number of valid bytes in `intermediates`.
     * @param finalByte          The CSI final byte (0x40–0x7E).
     *
     * @note READER THREAD only.
     */
    void csiDispatch (const CSI& params, const uint8_t* intermediates, uint8_t intermediateCount, uint8_t finalByte) noexcept;

    /**
     * @brief Dispatches a complete ESC sequence to the appropriate handler.
     *
     * Routes based on the number of intermediate bytes:
     * - Zero intermediates → `escDispatchNoIntermediate()`
     * - One intermediate in 0x28–0x2B range → `escDispatchCharset()`
     * - Other → `escDispatchDEC()`
     *
     * @param intermediates      Pointer to the intermediate byte buffer.
     * @param intermediateCount  Number of valid bytes in `intermediates`.
     * @param finalByte          The ESC final byte (0x30–0x7E).
     *
     * @note READER THREAD only.
     *
     * @see escDispatchNoIntermediate()
     * @see escDispatchCharset()
     * @see escDispatchDEC()
     */
    void escDispatch (const uint8_t* intermediates, uint8_t intermediateCount, uint8_t finalByte) noexcept;

    /**
     * @brief Handles ESC sequences with no intermediate bytes.
     *
     * Covers the most common ESC sequences:
     * - ESC 7 (DECSC) — save cursor and pen
     * - ESC 8 (DECRC) — restore cursor and pen
     * - ESC c (RIS)   — full reset
     * - ESC D (IND)   — index (line feed without CR)
     * - ESC E (NEL)   — next line
     * - ESC H (HTS)   — set tab stop
     * - ESC M (RI)    — reverse index (scroll down)
     * - ESC = / ESC > — application/normal keypad mode
     *
     * @param scr        Target screen buffer.
     * @param finalByte  The ESC final byte.
     *
     * @note READER THREAD only.
     */
    void escDispatchNoIntermediate (ActiveScreen scr, uint8_t finalByte) noexcept;

    /**
     * @brief Handles ESC sequences that select a character set (G0–G3 designators).
     *
     * Intermediate bytes 0x28–0x2B select the target charset slot (G0–G3).
     * The final byte selects the charset (e.g. 'B' = ASCII, '0' = DEC Special
     * Graphics).  Updates `useLineDrawing` accordingly.
     *
     * @param interByte  The intermediate byte (0x28 = '(', 0x29 = ')', …).
     * @param finalByte  The charset designator byte.
     *
     * @note READER THREAD only.
     */
    void escDispatchCharset (uint8_t interByte, uint8_t finalByte) noexcept;

    /**
     * @brief Handles DEC-private ESC sequences (intermediate byte '#').
     *
     * Covers DECDHL (double-height line), DECDWL (double-width line), and
     * DECALN (screen alignment test, fills screen with 'E').
     *
     * @param scr        Target screen buffer.
     * @param finalByte  The ESC final byte following '#'.
     *
     * @note READER THREAD only.
     */
    void escDispatchDEC (ActiveScreen scr, uint8_t finalByte) noexcept;

    /**
     * @brief Dispatches a complete OSC (Operating System Command) string.
     *
     * Parses the numeric command code from the start of `payload` and routes:
     * - OSC 0 / OSC 2 → `handleOscTitle()`
     * - OSC 52        → `handleOscClipboard()`
     * - Others        → silently ignored
     *
     * @param payload  Pointer to the raw OSC payload bytes (not null-terminated).
     * @param length   Number of valid bytes in `payload`.
     *
     * @note READER THREAD only.
     *
     * @see handleOscTitle()
     * @see handleOscClipboard()
     */
    void oscDispatch (const uint8_t* payload, int length) noexcept;

    /**
     * @brief Handles an OSC title-change command (OSC 0 or OSC 2).
     *
     * Passes `data` and `dataLength` directly to `state.setTitle()`, which
     * owns the backing buffer.  The UTF-8 safety trim (avoiding broken
     * multi-byte sequences at the boundary) is performed here before the call.
     *
     * @param data        Pointer to the title bytes (after the "0;" or "2;" prefix).
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.
     *
     * @see State::setTitle()
     */
    void handleOscTitle (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles OSC 7 — current working directory notification.
     *
     * Parses the `file://hostname/path` URI and extracts the path portion,
     * then passes it directly to `state.setCwd()` which owns the backing
     * buffer.
     *
     * @param data        Pointer to the OSC 7 payload bytes (after "7;").
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     * @note READER THREAD only.
     */
    void handleOscCwd (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles an OSC clipboard-write command (OSC 52).
     *
     * Decodes the base64-encoded clipboard payload and invokes `onClipboardChanged`
     * with the decoded text.  Malformed base64 is silently ignored.
     *
     * @param data        Pointer to the OSC 52 payload bytes (after "52;c;").
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.
     *
     * @see onClipboardChanged
     */
    void handleOscClipboard (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles OSC 9 — desktop notification (body only).
     *
     * The entire payload is treated as the notification body; title is empty.
     * Invokes `onDesktopNotification` on the message thread.
     *
     * @param data        Pointer to the OSC 9 payload bytes (after "9;").
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.  `onDesktopNotification` dispatched via `callAsync`.
     *
     * @see onDesktopNotification
     */
    void handleOscNotification (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles OSC 777 — desktop notification with title and body.
     *
     * Payload format: `notify;<title>;<body>`.  Verifies the `notify;` prefix,
     * then extracts title and body separated by `;`.  Invokes
     * `onDesktopNotification` on the message thread.
     *
     * @param data        Pointer to the OSC 777 payload bytes (after "777;").
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.  `onDesktopNotification` dispatched via `callAsync`.
     *
     * @see onDesktopNotification
     */
    void handleOsc777 (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Called when a DCS (Device Control String) sequence is hooked.
     *
     * Invoked on entry to `dcsPassthrough` state.  The current implementation
     * records the DCS parameters for potential future use but does not process
     * any DCS commands (they are passed through and ignored).
     *
     * @param params             Finalised CSI/DCS parameter accumulator.
     * @param intermediates      Pointer to the intermediate byte buffer.
     * @param intermediateCount  Number of valid bytes in `intermediates`.
     * @param finalByte          The DCS final byte.
     *
     * @note READER THREAD only.
     *
     * @see dcsPut()
     * @see dcsUnhook()
     */
    void dcsHook (const CSI& params, const uint8_t* intermediates, uint8_t intermediateCount, uint8_t finalByte) noexcept;

    /**
     * @brief Called for each byte in the DCS passthrough data stream.
     *
     * Currently unused — DCS passthrough accumulation is performed directly
     * in `performAction()` via `appendToBuffer()`.  Retained as an extension
     * point for future DCS command support (e.g. DECRQSS).
     *
     * @param byte  The DCS passthrough byte.
     *
     * @note READER THREAD only.
     */
    void dcsPut (uint8_t byte) noexcept;

    /**
     * @brief Called when a DCS sequence is terminated (ST received).
     *
     * Resets `dcsBufferSize`.  Will be wired to dispatch the accumulated
     * `dcsBuffer` to a Sixel decoder in a future step.
     *
     * @note READER THREAD only.
     *
     * @see dcsHook()
     * @see appendToBuffer()
     */
    void dcsUnhook() noexcept;

    /**
     * @brief Called when an APC sequence is terminated (BEL or ST received).
     *
     * @note READER THREAD only.
     */
    void apcEnd() noexcept;

    /** @brief Handles an END; filepath signal from any SKiT protocol envelope.
     *
     * Extracts cursor position and invokes `onPreviewFile` with the filepath and
     * absolute grid row.  Does NOT load or decode the file — the receiver must
     * perform file I/O on the MESSAGE THREAD.  An empty filepath signals dismiss.
     *
     * @param filepath  Absolute path extracted after the END; marker.  Empty = dismiss.
     * @note READER THREAD only.
     */
    void handleSkitFilepath (const juce::String& filepath) noexcept;

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

    /** @} */

    // =========================================================================
    /** @name Mode handlers
     * @{ */

    /**
     * @brief Handles DEC private mode set/reset sequences (`CSI ? Pm h/l`).
     *
     * Iterates over all parameters in `params` and enables or disables the
     * corresponding private mode.  Recognised modes include:
     * - ?1   — application cursor keys (DECCKM)
     * - ?7   — auto-wrap mode (DECAWM)
     * - ?25  — cursor visibility (DECTCEM)
     * - ?1049 — alternate screen buffer
     * - ?2004 — bracketed paste mode
     *
     * @param params  CSI parameters containing the mode numbers.
     * @param enable  `true` to set the mode (h), `false` to reset it (l).
     *
     * @note READER THREAD only.
     *
     * @see handleMode()
     */
    void handlePrivateMode (const CSI& params, bool enable) noexcept;

    /**
     * @brief Handles ANSI standard mode set/reset sequences (`CSI Pm h/l`).
     *
     * Iterates over all parameters in `params` and enables or disables the
     * corresponding ANSI mode.  Recognised modes include:
     * - 4  — insert mode (IRM)
     * - 20 — line-feed / new-line mode (LNM)
     *
     * @param params  CSI parameters containing the mode numbers.
     * @param enable  `true` to set the mode (h), `false` to reset it (l).
     *
     * @note READER THREAD only.
     *
     * @see handlePrivateMode()
     */
    void handleMode (const CSI& params, bool enable) noexcept;

    /** @brief Handles DECSCUSR (CSI Ps SP q) — set cursor style.
     *  @param params  CSI parameters. Ps 0-6.
     *  @note READER THREAD.
     */
    void handleCursorStyle (const CSI& params) noexcept;

    /**
     * @brief Handles progressive keyboard protocol sequences (`CSI … u`).
     *
     * Dispatches based on intermediate byte:
     * - `>` — push flags onto per-screen keyboard mode stack
     * - `<` — pop entries from per-screen keyboard mode stack
     * - `?` — query current flags (responds with `CSI ? flags u`)
     * - `=` — set/OR/AND-NOT flags directly
     *
     * @param params      CSI parameters (flags, count, or mode).
     * @param inter       Intermediate byte buffer.
     * @param interCount  Number of intermediate bytes collected.
     * @note READER THREAD only.
     */
    void handleKeyboardMode (const CSI& params, const uint8_t* inter, uint8_t interCount) noexcept;

    /** @brief Handles OSC 8 — explicit hyperlink start/end.
     *
     *  Payload format: `8;params;uri`
     *  - Non-empty URI: registers URI via `state.registerLinkUri()` and stores
     *    the returned ID in `activeLinkId` to stamp subsequent cell writes.
     *  - Empty URI:     clears `activeLinkId` to 0, ending the stamp run.
     *  - Malformed:     clears `activeLinkId` to 0.
     *
     *  @param data        Pointer to OSC payload after the `"8;"` prefix.
     *  @param dataLength  Length of the payload in bytes.
     *  @note READER THREAD.
     */
    void handleOsc8 (const uint8_t* data, int dataLength) noexcept;

    /** @brief Handles OSC 12 — set cursor color.
     *  @param data        Pointer to OSC payload after the command number separator.
     *  @param dataLength  Length of the payload in bytes.
     *  @note READER THREAD.
     */
    void handleOscCursorColor (const uint8_t* data, int dataLength) noexcept;

    /** @brief Handles OSC 112 — reset cursor color to default.
     *  @note READER THREAD.
     */
    void handleOscResetCursorColor() noexcept;

    /**
     * @brief Handles OSC 133 — shell integration semantic prompt markers.
     *
     * Dispatches single-letter subcommands A/B/C/D:
     * - A: prompt start (no-op for output tracking).
     * - B: command start (no-op for output tracking).
     * - C: command output start — records outputBlockTop and activates scan.
     * - D: command output end — records outputBlockBottom and deactivates scan.
     *
     * The subcommand byte is the first byte of `data`.
     *
     * @param scr         Active screen at the time of dispatch.
     * @param data        Pointer to OSC payload bytes after the `"133;"` prefix.
     * @param dataLength  Length of the payload in bytes (at least 1 for a valid subcommand).
     * @note READER THREAD only.
     */
    void handleOsc133 (ActiveScreen scr, const uint8_t* data, int dataLength) noexcept;

    /** @brief Handles OSC 1337 — iTerm2 inline image display.
     *
     *  Delegates to `ITerm2Decoder` to parse the `File=` header, base64-decode
     *  the payload, and produce a `DecodedImage`.  On success, reserves an image
     *  ID, writes image cells to the grid via `activeWriteImage()`, stages the
     *  pending image, and publishes it for the MESSAGE THREAD.
     *
     *  @param data        Pointer to OSC payload bytes after `"1337;"`.
     *                     Expected to begin with `"File="`.
     *  @param dataLength  Length of the payload in bytes.
     *  @note READER THREAD only.
     *
     *  @see ITerm2Decoder
     */
    void handleOsc1337 (const uint8_t* data, int dataLength) noexcept;

    /** @} */

    // =========================================================================
    /** @name CSI cursor handlers
     * @{ */

    /**
     * @brief Handles `CSI Pn A` — Cursor Up (CUU).
     *
     * Moves the cursor up by `params.param(0, 1)` rows, clamped to the
     * scroll region top.
     *
     * @param params  CSI parameters (Pn = row count, default 1).
     *
     * @note READER THREAD only.
     */
    void moveCursorUp (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn B` — Cursor Down (CUD).
     *
     * Moves the cursor down by `params.param(0, 1)` rows, clamped to the
     * scroll region bottom.
     *
     * @param params  CSI parameters (Pn = row count, default 1).
     *
     * @note READER THREAD only.
     */
    void moveCursorDown (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn C` — Cursor Forward (CUF).
     *
     * Moves the cursor right by `params.param(0, 1)` columns, clamped to
     * the right margin.
     *
     * @param params  CSI parameters (Pn = column count, default 1).
     *
     * @note READER THREAD only.
     */
    void moveCursorForward (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn D` — Cursor Backward (CUB).
     *
     * Moves the cursor left by `params.param(0, 1)` columns, clamped to
     * column 0.
     *
     * @param params  CSI parameters (Pn = column count, default 1).
     *
     * @note READER THREAD only.
     */
    void moveCursorBackward (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn E` — Cursor Next Line (CNL).
     *
     * Moves the cursor down by `params.param(0, 1)` rows and sets the column
     * to 0.
     *
     * @param params  CSI parameters (Pn = row count, default 1).
     *
     * @note READER THREAD only.
     */
    void moveCursorNextLine (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn F` — Cursor Previous Line (CPL).
     *
     * Moves the cursor up by `params.param(0, 1)` rows and sets the column
     * to 0.
     *
     * @param params  CSI parameters (Pn = row count, default 1).
     *
     * @note READER THREAD only.
     */
    void moveCursorPrevLine (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn I` — Cursor Forward Tabulation (CHT).
     *
     * Advances the cursor to the next tab stop `params.param(0, 1)` times.
     * Stops at the right margin if no further tab stops exist.
     *
     * @param params  CSI parameters (Pn = tab count, default 1).
     *
     * @note READER THREAD only.
     *
     * @see nextTabStop()
     */
    void cursorForwardTab (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn Z` — Cursor Backward Tabulation (CBT).
     *
     * Moves the cursor to the previous tab stop `params.param(0, 1)` times.
     * Stops at column 0 if no further tab stops exist to the left.
     *
     * @param params  CSI parameters (Pn = tab count, default 1).
     *
     * @note READER THREAD only.
     *
     * @see prevTabStop()
     */
    void cursorBackTab (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn G` — Cursor Horizontal Absolute (CHA).
     *
     * Sets the cursor column to `params.param(0, 1) - 1` (one-based input,
     * zero-based internal), clamped to the right margin.
     *
     * @param params  CSI parameters (Pn = column, 1-based, default 1).
     *
     * @note READER THREAD only.
     */
    void setCursorColumn (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pr ; Pc H` / `CSI Pr ; Pc f` — Cursor Position (CUP/HVP).
     *
     * Sets the cursor to the absolute position (row, col), both one-based.
     * Respects origin mode (DECOM) via `cursorSetPositionInOrigin()`.
     *
     * @param params  CSI parameters (P0 = row, P1 = col, both 1-based, default 1).
     *
     * @note READER THREAD only.
     *
     * @see cursorSetPositionInOrigin()
     */
    void setCursorPosition (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn d` — Line Position Absolute (VPA).
     *
     * Sets the cursor row to `params.param(0, 1) - 1` (one-based input),
     * preserving the current column.
     *
     * @param params  CSI parameters (Pn = row, 1-based, default 1).
     *
     * @note READER THREAD only.
     */
    void setCursorLine (const CSI& params) noexcept;

    /**
     * @brief Moves the cursor to an absolute (row, col) position (zero-based).
     *
     * Internal helper used by `setCursorPosition()` and `setCursorLine()` after
     * converting from one-based CSI parameters.
     *
     * @param row  Zero-based target row.
     * @param col  Zero-based target column.
     *
     * @note READER THREAD only.
     */
    void moveCursorTo (int row, int col) noexcept;

    /** @} */

    // =========================================================================
    /** @name CSI scroll handlers
     * @{ */

    /**
     * @brief Handles `CSI Pn S` — Scroll Up (SU).
     *
     * Scrolls the scrolling region up by `params.param(0, 1)` lines.  New
     * blank lines are inserted at the bottom of the region.
     *
     * @param params  CSI parameters (Pn = line count, default 1).
     *
     * @note READER THREAD only.
     */
    void scrollUp (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pn T` — Scroll Down (SD).
     *
     * Scrolls the scrolling region down by `params.param(0, 1)` lines.  New
     * blank lines are inserted at the top of the region.
     *
     * @param params  CSI parameters (Pn = line count, default 1).
     *
     * @note READER THREAD only.
     */
    void scrollDown (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI Pt ; Pb r` — Set Top and Bottom Margins (DECSTBM).
     *
     * Sets the scrolling region to rows `Pt`–`Pb` (one-based).  After setting
     * the region, the cursor is moved to the home position.
     *
     * @param params  CSI parameters (P0 = top, P1 = bottom, both 1-based).
     *
     * @note READER THREAD only.
     *
     * @see cursorSetScrollRegion()
     */
    void setScrollRegion (const CSI& params) noexcept;

    /** @} */

    // =========================================================================
    /** @name CSI report handlers
     * @{ */

    /**
     * @brief Handles `CSI 6 n` — Device Status Report, Cursor Position (CPR).
     *
     * Queues a `ESC[<row>;<col>R` response in `responseBuf`.  Row and column
     * are one-based in the response.
     *
     * @param params  CSI parameters (must contain Pn = 6 to reach this handler).
     *
     * @note READER THREAD only.
     *
     * @see sendResponse()
     * @see flushResponses()
     */
    void reportCursorPosition (const CSI& params) noexcept;

    /**
     * @brief Handles `CSI c` / `CSI ? c` — Device Attributes (DA1).
     *
     * Queues a primary device attributes response identifying this terminal as
     * a VT220-compatible device.
     *
     * @param isPrivate  `true` if the sequence was `CSI ? c` (secondary DA),
     *                   `false` if it was `CSI c` (primary DA).
     *
     * @note READER THREAD only.
     *
     * @see sendResponse()
     */
    void reportDeviceAttributes (bool isPrivate) noexcept;

    /** @} */

    // =========================================================================
    /** @name Erase operations
     * @{ */

    /**
     * @brief Handles `CSI Ps J` — Erase in Display (ED).
     *
     * | Ps | Effect                                      |
     * |----|---------------------------------------------|
     * | 0  | Erase from cursor to end of screen          |
     * | 1  | Erase from start of screen to cursor        |
     * | 2  | Erase entire screen                         |
     * | 3  | Erase entire screen + scrollback buffer     |
     *
     * @param mode  The erase mode (0–3).
     *
     * @note READER THREAD only.
     */
    void eraseInDisplay (int mode) noexcept;

    /**
     * @brief Handles `CSI Ps K` — Erase in Line (EL).
     *
     * | Ps | Effect                                      |
     * |----|---------------------------------------------|
     * | 0  | Erase from cursor to end of line            |
     * | 1  | Erase from start of line to cursor          |
     * | 2  | Erase entire line                           |
     *
     * @param mode  The erase mode (0–2).
     *
     * @note READER THREAD only.
     */
    void eraseInLine (int mode) noexcept;

    /** @} */

    // =========================================================================
    /** @name Line and cell shift operations
     * @{ */

    /**
     * @brief Handles `CSI Pn L` — Insert Lines (IL).
     *
     * Inserts `count` blank lines at the cursor row, pushing existing lines
     * down.  Lines that fall below the scroll region bottom are discarded.
     *
     * @param count  Number of lines to insert (>= 1).
     *
     * @note READER THREAD only.
     *
     * @see shiftLines()
     */
    void shiftLinesDown (int count) noexcept;

    /**
     * @brief Handles `CSI Pn M` — Delete Lines (DL).
     *
     * Deletes `count` lines starting at the cursor row, pulling subsequent
     * lines up.  Blank lines are inserted at the scroll region bottom.
     *
     * @param count  Number of lines to delete (>= 1).
     *
     * @note READER THREAD only.
     *
     * @see shiftLines()
     */
    void shiftLinesUp (int count) noexcept;

    /**
     * @brief Shared implementation for IL (insert lines) and DL (delete lines).
     *
     * Shifts lines within the scrolling region starting at the cursor row.
     *
     * @param count  Number of lines to shift (>= 1).
     * @param up     `true` to shift lines up (DL), `false` to shift down (IL).
     *
     * @note READER THREAD only.
     *
     * @see shiftLinesDown()
     * @see shiftLinesUp()
     */
    void shiftLines (int count, bool up) noexcept;

    /**
     * @brief Handles `CSI Pn @` — Insert Characters (ICH).
     *
     * Inserts `count` blank cells at the cursor position, shifting existing
     * cells to the right.  Cells that fall beyond the right margin are discarded.
     *
     * @param count  Number of blank cells to insert (>= 1).
     *
     * @note READER THREAD only.
     */
    void shiftCellsRight (int count) noexcept;

    /**
     * @brief Handles `CSI Pn P` — Delete Characters (DCH).
     *
     * Deletes `count` cells starting at the cursor position, pulling subsequent
     * cells to the left.  Blank cells are inserted at the right margin.
     *
     * @param count  Number of cells to delete (>= 1).
     *
     * @note READER THREAD only.
     */
    void removeCells (int count) noexcept;

    /**
     * @brief Handles `CSI Pn X` — Erase Characters (ECH).
     *
     * Replaces `count` cells starting at the cursor position with blank cells
     * using the current background colour.  The cursor does not move.
     *
     * @param count  Number of cells to erase (>= 1).
     *
     * @note READER THREAD only.
     */
    void eraseCells (int count) noexcept;

    /** @brief Handles `CSI Ps b` — Repeat preceding graphic character (REP). */
    void repeatCharacter (int count) noexcept;

    /** @} */

    // =========================================================================
    /** @name Screen buffer management
     * @{ */

    /**
     * @brief Switches between the normal and alternate screen buffers.
     *
     * When switching to the alternate screen, the normal screen state is saved
     * and the alternate screen is cleared.  When switching back, the normal
     * screen state is restored.  Corresponds to the `?1049h` / `?1049l`
     * private mode sequences.
     *
     * @param shouldUseAlternate  `true` to activate the alternate screen,
     *                            `false` to return to the normal screen.
     *
     * @note READER THREAD only.
     */
    void setScreen (bool shouldUseAlternate) noexcept;

    /** @} */

    // =========================================================================
    /** @name Response buffering
     * @{ */

    /**
     * @brief Appends a null-terminated response string to `responseBuf`.
     *
     * Accumulates device responses during `process()` so they can be delivered
     * in a single write after processing completes.  If the response would
     * overflow `responseBuf`, it is silently truncated.
     *
     * @param response  Null-terminated ASCII response string (e.g. `"\x1B[0c"`).
     *
     * @note READER THREAD only.
     *
     * @see flushResponses()
     * @see responseBuf
     */
    void sendResponse (const char* response) noexcept;

    /** @} */
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
