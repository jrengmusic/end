/**
 * @file Video.h
 * @brief VT command processor — cursor, pen, modes, and Grid writes.
 *
 * Video is the VT command processor.  It holds calculation inputs (cursor, pen,
 * modes — working copies synced from State) and writes the Grid access buffer.
 * It receives decoded semantic actions from Parser (print, applyControlCode,
 * applyCSI, applyESC, applyOSC, storeDCSHeader/applyDCSPayload, applyAPCPayload)
 * and translates them into Grid mutations.
 *
 * ## Responsibilities
 *
 * | Action class             | Handled by                                      |
 * |--------------------------|--------------------------------------------------|
 * | Print codepoint          | `print()` → `resolveWrapPending()` → `Grid`      |
 * | C0/C1 control characters | `applyControlCode()`                             |
 * | CSI sequences            | `applyCSI()` and CSI cursor/scroll/report/erase handlers |
 * | ESC sequences            | `applyESC()` and sub-dispatchers                 |
 * | OSC sequences            | `applyOSC()` and per-command handlers            |
 * | DCS sequences (Sixel)    | `storeDCSHeader()` / `applyDCSPayload()`         |
 * | APC sequences (Kitty)    | `applyAPCPayload()`                              |
 * | SGR / mode               | `handleSGR()` / `handleMode()` / `handlePrivateMode()` |
 *
 * ## Zero Model knowledge
 *
 * Video holds calculation inputs internally — working copies of terminal state.
 * State is the SSOT.  Processor reads public getters after each command and
 * syncs to State (value delivery pattern).  No UI or Model types are referenced.
 *
 * ## Thread model
 *
 * **All methods are READER THREAD only** unless explicitly marked MESSAGE THREAD
 * in their documentation.  `setDimensions()` is the only cross-thread setter;
 * it uses relaxed atomic stores.
 *
 * @see Parser       — DFA byte decoder that drives Video's action methods
 * @see Processor    — owner that wires Parser → Video and calls `resize()` / `calc()`
 * @see Grid         — cell buffer written by `print()` and erase operations
 */

#pragma once

#include <JuceHeader.h>
#include <jam_tui/jam_tui.h>

#include "../data/Charset.h"
#include "../data/DispatchTable.h"
#include "../data/CSI.h"
#include "../data/Identifier.h"
#include "../data/CharProps.h"
#include "../data/Screen.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Video
 * @brief VT command processor: receives decoded actions, writes Grid, holds calculation inputs.
 *
 * Video is the VT command processor.  It receives decoded semantic actions from
 * Parser and translates them into mutations of the Grid cell buffer.  It holds
 * calculation inputs: the drawing pen, cursor state (both normal and alternate
 * screens), scroll region, tab stops, charset selection, and response buffering.
 *
 * @par Lifecycle
 * 1. Construct with references to a `Grid` and the events map.
 * 2. Call `calc()` once after construction (and after every `resize()`) to
 *    synchronise internal geometry.
 * 3. Parser calls action methods (`print()`, `applyControlCode()`, `applyCSI()`, …)
 *    on the reader thread as bytes are decoded.
 * 4. Call `flushResponses()` after each `process()` pass to deliver any queued
 *    device responses through the `"writeToHost"` event handler in the events map.
 *
 * @par Events
 * Video fires events through the `events` map reference received at construction.
 * Processor owns the map; Session registers handlers on it.  Events are fired
 * on the READER THREAD; handlers dispatched via `callAsync` land on the message thread.
 *
 * @note All methods are READER THREAD only unless otherwise stated.
 *
 * @see Parser  — DFA decoder that drives action methods on this class
 * @see CSI     — CSI parameter accumulator passed to dispatch handlers
 * @see Grid    — screen buffer target for print and erase operations
 */
class Video
{
public:
    /**
     * @brief Maximum number of characters accepted from an OSC title string.
     *
     * Applied when extracting the window title from an OSC 0 or OSC 2 sequence.
     * Titles longer than this are truncated before being written to State.
     */
    static constexpr int MAX_OSC_TITLE_LENGTH { 256 };

    /**
     * @brief Constructs Video and initialises internal geometry.
     *
     * The constructor does not call `calc()`.  The owner must call `calc()`
     * after construction (and after every `resize()`) to synchronise the
     * internal geometry.
     *
     * @param grid    Live cell buffer. Video writes in-place; Display reads dirty rows.
     * @param events  Events map owned by Processor.  Video fires events through
     *                this map instead of holding std::function callbacks directly.
     *
     * @note MESSAGE THREAD — construction happens before the reader thread starts.
     *
     * @see calc()
     */
    explicit Video (Grid& grid, jam::Function::Map<juce::Identifier, void>& events) noexcept;

    /**
     * @brief Notifies Video that the terminal dimensions have changed.
     *
     * Re-clamps the cursor and scroll region to the new bounds on both screens
     * and reinitialises tab stops.  Must be preceded by `setDimensions()` and
     * followed by `calc()` to propagate the change to internal cached geometry.
     *
     * @param newCols         New terminal width in character columns.
     * @param newVisibleRows  New terminal height in visible rows.
     *
     * @note MESSAGE THREAD — called from Processor::resized().
     *
     * @see setDimensions()
     * @see calc()
     */
    void resize (int newCols, int newVisibleRows) noexcept;

    /**
     * @brief Resets Video and the terminal to a clean initial state.
     *
     * Clears the pen, cursor, scroll region, tab stops, and all mode flags.
     * Equivalent to a hard terminal reset (RIS, ESC c).
     *
     * @note READER THREAD only.
     */
    void reset() noexcept;

    /**
     * @brief Synchronises internal cached geometry.
     *
     * Copies the current pen style and colour attributes from `pen` into
     * `stamp` so that DECSC saves the up-to-date pen.  Must be called after
     * construction and after every `resize()`.
     *
     * @note READER THREAD only.
     *
     * @see resize()
     */
    void calc() noexcept;

    // =========================================================================
    /** @name State getters for Processor sync (value delivery)
     *  Processor reads these after each command and writes State atomics.
     * @{ */

    int getActiveScreen() const noexcept { return activeScreen; }
    int getCursorRow (int s) const noexcept { return cursorRow[s]; }
    int getCursorCol (int s) const noexcept { return cursorCol[s]; }
    bool isWrapPending (int s) const noexcept { return wrapPending[s]; }
    int getScrollTop (int s) const noexcept { return scrollTop[s]; }
    int getScrollBottom (int s) const noexcept { return scrollBottom[s]; }
    bool isCursorVisible (int s) const noexcept { return cursorVisible[s]; }
    int getCols() const noexcept { return cols.load (std::memory_order_relaxed); }
    int getVisibleRows() const noexcept { return visibleRows.load (std::memory_order_relaxed); }

    /** @brief Returns the value of the named mode flag.
     *
     *  Uses a data-driven lookup over the mode table — avoids chained branches.
     *  O(n) over ~13 entries; negligible cost on the reader thread.
     *
     *  @param id  A Terminal::ID mode identifier.
     *  @return    `true` if the mode is currently set, `false` otherwise or if unknown.
     *  @note READER THREAD only.
     */
    bool getMode (juce::Identifier id) const noexcept;

    /** @brief Sets the named mode flag. */
    void setMode (juce::Identifier id, bool value) noexcept;

    uint32_t getKeyboardFlags (int s) const noexcept { return keyboardFlags[s]; }
    const jam::Cell& getPen() const noexcept { return pen; }
    const jam::Cell& getStamp() const noexcept { return stamp; }

    /** @brief Sets terminal dimensions from Processor (message thread → reader thread).
     *
     *  Stores new column and row counts atomically.  Called by Processor before
     *  delegating to `resize()`, so that internal geometry is consistent on
     *  the reader thread without requiring a State read.
     *
     *  @param newCols  New terminal width in character columns.
     *  @param newRows  New terminal height in visible rows.
     *  @note MESSAGE THREAD — atomic stores with relaxed ordering.
     */
    void setDimensions (int newCols, int newRows) noexcept;

    /** @brief Sets physical cell dimensions for CSI pixel dimension reports.
     *
     *  Stores cell width and height used by CSI `t` ps=14 and ps=16 handlers
     *  in `VideoCSI.cpp`.  Image decode cell dimensions are now owned by `Skit`.
     *  Processor forwards to both Video and Skit via `Processor::setCellSize()`.
     *
     *  @param widthPx   Cell width in pixels.
     *  @param heightPx  Cell height in pixels.
     *  @note MESSAGE THREAD — relaxed atomic stores.
     */
    void setCellSize (int widthPx, int heightPx) noexcept;

    /** @brief Sets the active OSC 8 hyperlink ID.
     *
     *  Called by the `"registerLink"` event handler registered in Processor.
     *  The handler receives the URI from Video, registers it in State, and
     *  pushes the assigned ID back via this setter.
     *
     *  @param id  1-based link ID (OSC 8), or 0 to clear.
     *  @note READER THREAD — called synchronously from the event handler.
     */
    void setActiveLinkId (uint16_t id) noexcept { activeLinkId = id; }

    /** @} */

    // =========================================================================

    /**
     * @brief Delivers any queued device responses to the host via the `writeToHost` event.
     *
     * During action processing, responses (DA, CPR, …) are accumulated in
     * `responseBuf` rather than sent immediately.  Call this method after
     * Parser's `process()` returns to flush the buffer through the `"writeToHost"`
     * handler registered in the `events` map.
     *
     * @note READER THREAD only.
     *
     * @see sendResponse()
     * @see Processor::events
     */
    void flushResponses() noexcept;

    // =========================================================================
    /** @name Dispatch API — called by Processor command handlers
     * @{ */

    /** @brief Writes a Unicode codepoint to the active screen at the cursor position. */
    void print (uint32_t codepoint) noexcept;

    /** @brief Applies a C0/C1 control character (CR, LF, BS, BEL, etc.). */
    void applyControlCode (uint8_t controlByte) noexcept;

    /** @brief Applies a complete CSI sequence. */
    void applyCSI (const CSI& params, const uint8_t* intermediates, uint8_t intermediateCount, uint8_t finalByte) noexcept;

    /** @brief Applies a complete ESC sequence. */
    void applyESC (const uint8_t* intermediates, uint8_t intermediateCount, uint8_t finalByte) noexcept;

    /** @brief Applies a complete OSC string. */
    void applyOSC (const uint8_t* payload, int length) noexcept;

    /** @brief Stores the DCS sequence header (records final byte for use by `applyDCSPayload()`). */
    void storeDCSHeader (const CSI& params, const uint8_t* intermediates, uint8_t intermediateCount, uint8_t finalByte) noexcept;

    /** @brief Applies the accumulated DCS payload (Sixel, SKiT, etc.) when ST is received.
     *
     *  Image decode and SKiT filepath logic has been moved to `Skit`.  After
     *  calling this method, Processor calls `Skit::processDCS()` with the
     *  DCS final byte (readable via `getDcsFinalByte()`), then calls
     *  `advanceCursorForImage()` with the row count Skit reports.
     */
    void applyDCSPayload (const uint8_t* data, int length) noexcept;

    /** @brief Applies the accumulated APC payload (Kitty graphics, etc.) when ST/BEL is received.
     *
     *  Image decode and SKiT filepath logic has been moved to `Skit`.  After
     *  calling this method, Processor calls `Skit::processAPC()`, then calls
     *  `advanceCursorForImage()` with the row count Skit reports.
     *
     *  @param data    Pointer to the raw APC payload bytes.
     *  @param length  Number of bytes in @p data.
     */
    void applyAPCPayload (const uint8_t* data, int length) noexcept;

    /** @brief Advances cursor after image placement — moves down by `numRows`, resets to column 0.
     *
     *  Called by Processor after `Skit::processDCS()`, `Skit::processAPC()`, or
     *  `Skit::processOSC1337()` to apply the post-decode cursor position update.
     *  No-op when `numRows <= 0`.
     *
     *  @param numRows  Number of cell rows to advance downward.
     *  @note READER THREAD only.
     */
    void advanceCursorForImage (int numRows) noexcept;

    /** @brief Returns the DCS final byte stored by `storeDCSHeader()`.
     *
     *  Processor reads this after `applyDCSPayload()` to pass the correct byte
     *  to `Skit::processDCS()`.
     *
     *  @note READER THREAD only.
     */
    uint8_t getDcsFinalByte() const noexcept { return dcsFinalByte; }

    /** @} */

private:
    /**
     * @brief Live cell buffer. Video writes in-place; Display reads dirty rows.
     */
    Grid& grid;

    /**
     * @brief Events map owned by Processor.  Video fires events through this map.
     *
     * Registered event keys fired by Video:
     * - `"writeToHost"` — `(const char*, int)` — device response bytes, reader thread
     * - `"bell"`        — `()` — BEL character, dispatched via callAsync
     * - `"clipboardChanged"` — `(const juce::String&)` — OSC 52, dispatched via callAsync
     * - `"desktopNotification"` — `(const juce::String&, const juce::String&)` — OSC 9/777, dispatched via callAsync
     * - `"imageDecoded"` — image pixel data + placement metadata, reader thread
     * - `"previewFile"`  — `(const juce::String&, int, int, int, int)` — reader thread
     *
     * @note READER THREAD — events are fired on the reader thread; callAsync handlers
     *       land on the message thread.
     */
    jam::Function::Map<juce::Identifier, void>& events;

    // =========================================================================
    /** @name Internal terminal state
     *  Working copy of terminal state.  Processor reads these via public getters
     *  after each command and syncs to State (value delivery pattern).
     * @{ */

    /** @brief Active screen buffer (normal or alternate). */
    int activeScreen { map::Screen::normal };

    /** @brief Terminal width in character columns. Cross-thread (message → reader). */
    std::atomic<int> cols { 80 };

    /** @brief Terminal height in visible rows. Cross-thread (message → reader). */
    std::atomic<int> visibleRows { 24 };

    /** @brief Cell width in pixels. Cross-thread (message → reader). Used by CSI `t` pixel dimension reports. */
    std::atomic<int> cellWidth { 0 };

    /** @brief Cell height in pixels. Cross-thread (message → reader). Used by CSI `t` pixel dimension reports. */
    std::atomic<int> cellHeight { 0 };

    /** @brief Per-screen cursor row (zero-based). */
    int cursorRow[2] {};

    /** @brief Per-screen cursor column (zero-based). */
    int cursorCol[2] {};

    /** @brief Per-screen wrap-pending flag. */
    bool wrapPending[2] {};

    /** @brief Per-screen scroll region top row (zero-based). */
    int scrollTop[2] {};

    /** @brief Per-screen scroll region bottom row (0 = full screen sentinel). */
    int scrollBottom[2] {};

    /** @brief Per-screen cursor visibility. */
    bool cursorVisible[2] { true, true };

    // Mode flags
    bool originMode { false };
    bool autoWrap { true };
    bool applicationCursor { false };
    bool bracketedPaste { false };
    bool insertMode { false };
    bool mouseTracking { false };
    bool mouseMotionTracking { false };
    bool mouseAllTracking { false };
    bool mouseSgr { false };
    bool focusEvents { false };
    bool applicationKeypad { false };
    bool reverseVideo { false };
    bool win32InputMode { false };

    /** @brief Per-screen keyboard enhancement flags. */
    uint32_t keyboardFlags[2] {};

    /** @} */

    // =========================================================================

    /**
     * @brief Final byte from DCS header, recorded by `storeDCSHeader()` for dispatch
     *        in `applyDCSPayload()`.
     *
     * @see storeDCSHeader()
     * @see applyDCSPayload()
     */
    uint8_t dcsFinalByte { 0 };

    /**
     * @brief Current drawing attributes applied to newly written cells.
     *
     * Mutated by SGR sequences (`applySGR()` / `handleSGR()`).  Copied into
     * each jam::Cell written by `print()`.  Saved and restored by DECSC/DECRC
     * (ESC 7 / ESC 8) via `stamp`.
     *
     * @see stamp
     * @see applySGR()
     */
    jam::Cell pen {};

    /**
     * @brief Saved cursor state for DECSC/DECRC (ESC 7 / ESC 8).
     *
     * Per-screen saved state: cursor position, pen, wrap-pending, origin
     * mode, and line-drawing charset.  Indexed by screen index.
     */
    struct SavedCursor
    {
        int row { 0 };
        int col { 0 };
        jam::Cell pen {};
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
    jam::Cell stamp {};

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
     * @brief Buffer for device response strings queued during action processing.
     *
     * Responses (DA, CPR, …) are written here by `sendResponse()` rather than
     * sent immediately.  `flushResponses()` delivers the accumulated content
     * through the `"writeToHost"` event after Parser's `process()` returns.
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
     * Set by `handleOsc8()` via Processor event handler when an OSC 8 open
     * is received.  Cleared to 0 on OSC 8 close.  Stamped onto every cell
     * written by `print()` while non-zero.
     *
     * Lives in Video (not the pen cell) because hyperlink state must not survive
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
     * mode flags through `Video::setMode()`.
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

    /**
     * @brief Returns a mutable pointer to the named mode flag, or nullptr if unknown.
     *
     * Single SSOT for mode flag lookup used by both `getMode()` and `setMode()`.
     * O(n) over ~13 entries; negligible cost on the reader thread.
     *
     * @param id  A Terminal::ID mode identifier.
     * @return    Pointer to the corresponding member, or nullptr if id is unknown.
     *
     * @note READER THREAD only.
     */
    bool* modePtr (juce::Identifier id) noexcept;

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
    void cursorMoveUp (int s, int count) noexcept;

    /**
     * @brief Moves the cursor down by `count` rows, clamped to `bottom`.
     *
     * @param s      Target screen buffer.
     * @param count  Number of rows to move down (>= 1).
     * @param bottom Zero-based index of the last row the cursor may reach.
     *
     * @note READER THREAD only.
     */
    void cursorMoveDown (int s, int count, int bottom) noexcept;

    /**
     * @brief Moves the cursor right by `count` columns, clamped to `cols - 1`.
     *
     * @param s     Target screen buffer.
     * @param count Number of columns to move right (>= 1).
     * @param cols  Current terminal column count (right margin = cols - 1).
     *
     * @note READER THREAD only.
     */
    void cursorMoveForward (int s, int count, int cols) noexcept;

    /**
     * @brief Moves the cursor left by `count` columns, clamped to column 0.
     *
     * @param s     Target screen buffer.
     * @param count Number of columns to move left (>= 1).
     *
     * @note READER THREAD only.
     */
    void cursorMoveBackward (int s, int count) noexcept;

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
    void cursorSetPosition (int s, int row, int col, int cols, int visibleRows) noexcept;

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
    void cursorSetPositionInOrigin (int s, int row, int col, int cols, int visibleRows) noexcept;

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
    bool cursorGoToNextLine (int s, int bottom, int visibleRows) noexcept;

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
    void cursorClamp (int s, int cols, int visibleRows) noexcept;

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
    void saveCursor (int scr) noexcept;

    /**
     * @brief Restores cursor position and associated state for DECRC (ESC 8).
     *
     * Applies the state previously saved by `saveCursor()` back to live State
     * and Video members: cursor row/col, pen, stamp, wrap-pending, origin
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
    void restoreCursor (int scr) noexcept;

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
    void cursorSetScrollRegion (int s, int top, int bottom) noexcept;

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
    void cursorResetScrollRegion (int s) noexcept;

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
    int effectiveScrollBottom (int s, int visibleRows) const noexcept;

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
    int effectiveClampBottom (int s) const noexcept;

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
    int nextTabStop (int s, int cols) noexcept;

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
    int prevTabStop (int s) noexcept;

    /**
     * @brief Sets a tab stop at the current cursor column.
     *
     * Corresponds to the HTS (Horizontal Tab Set, ESC H) sequence.
     *
     * @param s  Target screen buffer (used to read the current cursor column).
     *
     * @note READER THREAD only.
     */
    void setTabStop (int s) noexcept;

    /**
     * @brief Clears the tab stop at the current cursor column.
     *
     * Corresponds to `CSI 0 g` (TBC — Tab Clear, current column).
     *
     * @param s  Target screen buffer (used to read the current cursor column).
     *
     * @note READER THREAD only.
     */
    void clearTabStop (int s) noexcept;

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
    /** @name Scroll helpers
     * @{ */

    /** @brief Scrolls the region up one line and fills the bottom row with the current background. */
    void scrollUpAndFill (int top, int bottom) noexcept;

    /** @brief Scrolls the region down one line and fills the top row with the current background. */
    void scrollDownAndFill (int top, int bottom) noexcept;

    /** @} */

    // =========================================================================
    /** @name Core dispatch helpers
     * @{ */

    /**
     * @brief Resolves a pending line wrap before writing a new character.
     *
     * When `wrapPending` is true and a printable character arrives, the cursor
     * is moved to column 0 of the next line (scrolling if necessary) before the
     * character is written.  The wrap-pending flag is then cleared.
     *
     * @param scr  Target screen buffer.
     *
     * @note READER THREAD only.
     *
     * @see print()
     * @see cursorGoToNextLine()
     */
    void resolveWrapPending (int scr) noexcept;

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
     * @see applyControlCode()
     */
    void executeLineFeed (int scr) noexcept;

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
    void escDispatchNoIntermediate (int scr, uint8_t finalByte) noexcept;

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
    void escDispatchDEC (int scr, uint8_t finalByte) noexcept;

    /**
     * @brief Handles an OSC title-change command (OSC 0 or OSC 2).
     *
     * Trims the data to avoid splitting a multi-byte UTF-8 sequence at the
     * boundary, then fires the `"title"` event with the raw bytes.
     *
     * @param data        Pointer to the title bytes (after the "0;" or "2;" prefix).
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.
     *
     * @see events
     */
    void handleOscTitle (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles OSC 7 — current working directory notification.
     *
     * Parses the `file://hostname/path` URI and extracts the path portion,
     * then fires the `"cwd"` event with the path bytes.
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
     * Decodes the base64-encoded clipboard payload and fires the `"clipboardChanged"`
     * event with the decoded text.  Malformed base64 is silently ignored.
     *
     * @param data        Pointer to the OSC 52 payload bytes (after "52;c;").
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.
     *
     * @see events
     */
    void handleOscClipboard (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles OSC 9 — desktop notification (body only).
     *
     * The entire payload is treated as the notification body; title is empty.
     * Fires the `"desktopNotification"` event on the message thread.
     *
     * @param data        Pointer to the OSC 9 payload bytes (after "9;").
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.  Event dispatched via `callAsync`.
     *
     * @see events
     */
    void handleOscNotification (const uint8_t* data, int dataLength) noexcept;

    /**
     * @brief Handles OSC 777 — desktop notification with title and body.
     *
     * Payload format: `notify;<title>;<body>`.  Verifies the `notify;` prefix,
     * then extracts title and body separated by `;`.  Fires the
     * `"desktopNotification"` event on the message thread.
     *
     * @param data        Pointer to the OSC 777 payload bytes (after "777;").
     *                    Not null-terminated.
     * @param dataLength  Number of bytes in `data`.
     *
     * @note READER THREAD only.  Event dispatched via `callAsync`.
     *
     * @see events
     */
    void handleOsc777 (const uint8_t* data, int dataLength) noexcept;

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
     *  - Non-empty URI: registers URI via Processor event handler and stores
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
    void handleOsc133 (int scr, const uint8_t* data, int dataLength) noexcept;

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
     * Accumulates device responses during action processing so they can be
     * delivered in a single write after processing completes.  If the response
     * would overflow `responseBuf`, it is silently truncated.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Video)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
