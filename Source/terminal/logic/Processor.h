/**
 * @file Processor.h
 * @brief Terminal pipeline orchestrator: owns Parser and Video, references Grid and State.
 *
 * `Processor` is the pipeline half of the terminal emulator.  It owns the
 * Parser and Video, and routes bytes through the Grid and State received from Session:
 *
 * ```
 *  bytes → Processor::process → Parser → commands map → Video → State / Grid → Display
 * ```
 *
 * The PTY-side (TTY + History) lives in `Terminal::Session`.  Processor is
 * data-source agnostic — it receives bytes via `process()` whether they come
 * from a local PTY callback, an IPC byte-forward, or a history replay.
 *
 * ### Data flow
 * 1. Caller delivers raw bytes on the READER THREAD via `process()`.
 * 2. `process()` forwards to `Parser::process()`.
 * 3. The parser decodes VT sequences and dispatches semantic actions via `commands`.
 * 4. `commands` handlers call Video action methods; Video writes cells to `Grid`.
 * 5. Video fires events; Processor handlers write State atomics (event dispatch).
 * 6. Responses (e.g. cursor-position reports) are buffered in Video and
 *    flushed back via the `writeToHost` event handler registered in `events`.
 * 7. State::flush() propagates atomic values to the ValueTree on the timer tick,
 *    notifying Display via `juce::ValueTree::Listener`.
 * 8. Display resize: Display calls `processor.resized()` directly →
 *    Video and Grid resized.
 *
 * ### Thread safety
 * - `process()` — READER THREAD only.
 * - All other public methods — MESSAGE THREAD only.
 * - `State` and `Grid` handle their own internal thread safety.
 *
 * @see Terminal::Session — owns TTY and History (PTY side).
 * @see Grid    — flat Buffer<Cell> storage, stateless data buffer.
 * @see Parser  — VT100/VT520 state machine.
 * @see Video   — terminal state machine: pen, cursor, modes, Grid writes.
 * @see State   — atomic terminal parameter store.
 */

#pragma once

#include <JuceHeader.h>

#include "../data/Command.h"
#include "../data/Keyboard.h"
#include "../data/State.h"
#include "../data/TextBuffer.h"
#include "Grid.h"
#include "Parser.h"
#include "Skit.h"
#include "Video.h"


namespace Terminal
{ /*____________________________________________________________________________*/
// Forward declaration: Display is in component/.
class Display;
/**
 * @class Processor
 * @brief Terminal pipeline orchestrator — owns Parser and Video, receives Grid& and State& from Session.
 *
 * Processor owns the Parser and Video.  Grid and State are owned by Terminal::Session
 * and passed by reference at construction.  Processor has no knowledge of TTY, PTY, or IPC.
 * Bytes arrive via `process()` from whichever source owns the byte stream
 * (local `Terminal::Session` callback, IPC byte-forward, or history replay).
 *
 * Display subscribes as a `juce::ValueTree::Listener` on State's ValueTree.
 * `State::flush()` propagates atomic values to the ValueTree on the timer tick,
 * which notifies Display to repaint.
 *
 * ### Boundary contract
 * `State`, `Grid`, `uuid`, `video`, `parser`, and `commands` are private.
 * The `events` map is public — Session registers handlers directly on it.
 * External callers access state through the public getter API:
 * - `getState()` / `getGrid()` — mutable and const references.
 * - `getUuid()` — const reference to the stable session UUID.
 * - `setHostWriter()` — registers the `writeToHost` event handler.
 * - `flushResponses()` — flushes queued device responses to the host.
 *
 * ### Public surface
 * - **Input encoding** — `encodeKeyPress()`, `encodePaste()`, `encodeMouseEvent()`, `encodeFocusEvent()` (const, no side effects)
 * - **Output** — `process()` (called on the reader thread by the byte source)
 * - **Response flushing** — `flushResponses()` (reader thread; called by Session on drain-complete)
 * - **Lifecycle callbacks** — `ID::shellExited` on the events map
 *
 * @note Construct and destroy on the **message thread**.
 *
 * @see Grid, Parser, Video, State, Terminal::Session
 */
class Processor : public juce::ValueTree::Listener
{
public:
    //==============================================================================
    /**
     * @brief Constructs the Processor and wires the parser and video via maps.
     *
     * Receives Grid and TextBuffer by reference from the owning Session,
     * constructs State, Video, and Parser.  UUID is provided by the caller.
     * Call `setHostWriter()` immediately after construction to route video
     * responses (e.g. cursor-position reports) to the appropriate sink.
     *
     * @param grid        Live cell buffer owned by Terminal::Session.
     * @param textBuffer  Cross-thread string buffer owned by Terminal::Session.
     * @param cols        Initial terminal column count.
     * @param rows        Initial terminal row count.
     * @param uuid        Stable UUID for this Processor — generated once by the caller.
     * @note MESSAGE THREAD — must be constructed on the message thread.
     */
    Processor (Grid& grid, TextBuffer& textBuffer, int cols, int rows, const juce::String& uuid);

    /**
     * @brief Destroys the Processor.
     * @note MESSAGE THREAD — must be destroyed on the message thread.
     */
    ~Processor() override;

    /**
     * @brief Encodes a JUCE key press into a VT escape sequence.
     *
     * Reads `ID::applicationCursor` from the ValueTree (message-thread SSOT) to
     * select the correct cursor-key encoding (ANSI vs. application mode).
     * No side effects — does NOT write to any PTY.
     *
     * @param key  The JUCE key press event to translate.
     * @return UTF-8 encoded byte sequence, or empty string if no mapping exists.
     * @note MESSAGE THREAD only.
     */
    juce::String encodeKeyPress (const juce::KeyPress& key) const noexcept;

    /**
     * @brief Encodes a bracketed-paste text block into a PTY byte sequence.
     *
     * Reads `ID::bracketedPaste` from the ValueTree.  If bracketed paste mode is
     * active, wraps the text with `ESC[200~` … `ESC[201~`.
     * No side effects — does NOT write to any PTY.
     *
     * @param text  The UTF-8 text to encode.
     * @return The encoded byte sequence ready for delivery.
     * @note MESSAGE THREAD only.
     */
    juce::String encodePaste (const juce::String& text) const noexcept;

    /**
     * @brief Encodes a mouse event into a VT byte sequence.
     *
     * Supports SGR (extended) mouse encoding.
     * No side effects — does NOT write to any PTY.
     *
     * @param button  Mouse button index (0 = left, 1 = middle, 2 = right).
     * @param col     Zero-based column of the mouse event.
     * @param row     Zero-based row of the mouse event.
     * @param press   `true` for button press, `false` for button release.
     * @return UTF-8 encoded SGR mouse escape sequence.
     * @note MESSAGE THREAD only.
     */
    juce::String encodeMouseEvent (int button, int col, int row, bool press) const noexcept;

    /**
     * @brief Encodes a focus-in or focus-out event into a VT byte sequence.
     *
     * Returns `ESC[I` (focus gained) or `ESC[O` (focus lost) if `ID::focusEvents`
     * mode is enabled; returns empty string if focus events are disabled.
     * No side effects — does NOT write to any PTY.
     *
     * @param gained  `true` when the terminal window gained focus, `false` when lost.
     * @return Encoded focus sequence, or empty string if focus events are disabled.
     * @note MESSAGE THREAD only.
     */
    juce::String encodeFocusEvent (bool gained) const noexcept;

    /**
     * @brief Processes raw bytes through the parser pipeline.
     *
     * Forwards the byte buffer to `Parser::process()`, then flushes any queued
     * Video responses via `video.flushResponses()`.  Called on the READER THREAD
     * by whichever byte source owns this Processor — the `Terminal::Session`
     * onBytes callback (local/daemon) or the IPC byte-forward dispatch (client).
     *
     * @param data    Pointer to the raw byte buffer.
     * @param length  Number of bytes in the buffer.
     * @note READER THREAD only — never call from the message thread.
     */
    void process (const char* data, int length) noexcept;

    /**
     * @brief Returns a mutable reference to the terminal parameter store.
     * @return Mutable reference to the owned `State` object.
     * @note MESSAGE THREAD only.
     */
    State& getState() noexcept;

    /**
     * @brief Returns a const reference to the terminal parameter store.
     * @return Const reference to the owned `State` object.
     * @note MESSAGE THREAD only.
     */
    const State& getState() const noexcept;

    /**
     * @brief Returns a mutable reference to the cell grid.
     * @return Mutable reference to the Session-owned `Grid` object.
     * @note MESSAGE THREAD only.
     */
    Grid& getGrid() noexcept;

    /**
     * @brief Returns a const reference to the cell grid.
     * @return Const reference to the Session-owned `Grid` object.
     * @note MESSAGE THREAD only.
     */
    const Grid& getGrid() const noexcept;

    /**
     * @brief Returns the stable UUID identifying this Processor across process boundaries.
     * @return Const reference to the UUID string.
     * @note ANY THREAD — UUID is immutable after construction.
     */
    const juce::String& getUuid() const noexcept;

    /** @brief Flushes any queued device responses (DA, CPR) to the host.
     *  @note READER THREAD only. */
    void flushResponses() noexcept;

    /**
     * @brief Registers the `writeToHost` event handler in the events map.
     *
     * Adds a handler under the `"writeToHost"` key so Video responses (DSR, DA,
     * CPR, etc.) are forwarded to the caller's sink — local TTY write or IPC
     * output.  Must be called by the owner before bytes start flowing.
     *
     * @param writer  Callback invoked with `(const char* data, int len)` on the
     *                reader thread whenever Video produces a response.
     * @note MESSAGE THREAD — call before the first `process()` invocation.
     */
    void setHostWriter (std::function<void (const char*, int)> writer) noexcept;

    /**
     * @brief Creates and returns a Display for this Processor.
     *
     * Mirrors `AudioProcessor::createEditor()`.  The returned Display holds a
     * back-reference to this Processor.  Caller is responsible for placing the
     * Display in the component hierarchy.
     *
     * @return Unique pointer to the newly created Display.
     * @note MESSAGE THREAD.
     */
    std::unique_ptr<Display> createDisplay();

    /**
     * @brief Delivers cell pixel dimensions to Skit and Video.
     *
     * Called by Screen (or Display) when font metrics change.  Forwards
     * `widthPx` and `heightPx` to `Skit::setCellSize()` (image decode) and
     * `Video::setCellSize()` (CSI `t` pixel dimension reports).
     *
     * @param widthPx   Cell width in pixels.
     * @param heightPx  Cell height in pixels.
     * @note MESSAGE THREAD.
     */
    void setCellSize (int widthPx, int heightPx) noexcept;

    /** @brief Fires when OSC 133;C marks command start (outputBlockStart changes). MESSAGE THREAD. */
    std::function<void()> onCommandStarted;

    /** @brief Fires when OSC 133;A marks return to prompt (promptRow changes). MESSAGE THREAD. */
    std::function<void()> onCommandEnded;

    /** @brief Events map — Video → Processor → Session.
     *  Session registers handlers directly on this map.
     *
     *  Registered event keys:
     *  - `ID::writeToHost`         — `(const char*, int)` — flushed from Video on reader thread
     *  - `ID::bell`                — `()` — BEL character, dispatched via callAsync
     *  - `ID::clipboardChanged`    — `(const juce::String&)` — OSC 52, dispatched via callAsync
     *  - `ID::desktopNotification` — `(const juce::String&, const juce::String&)` — OSC 9/777, dispatched via callAsync
     *  - `ID::imageDecoded`        — see Video::onImageDecoded signature — reader thread
     *  - `ID::previewFile`         — `(const juce::String&, int, int, int, int)` — reader thread
     *  - `ID::registerLink`        — `(const juce::String& uri, const juce::String& params)` — OSC 8 open;
     *                               Processor handler registers the URI in State and calls `Video::setActiveLinkId()`.
     *  - `ID::writeInput`          — `(const char*, int)` — user input (keyboard, mouse) → PTY stdin; message thread
     *  - `ID::terminalResize`      — `(int, int, int, int)` — PTY SIGWINCH; message thread
     *  - `ID::shellExited`         — `()` — child shell process exited; message thread
     *  - `ID::activeScreen`        — `(int)` — active screen index flush; reader thread
     *  - `ID::cursorRow`           — `(int screen, int row)` — cursor row flush; reader thread
     *  - `ID::cursorCol`           — `(int screen, int col)` — cursor col flush; reader thread
     *  - `ID::cursorVisible`       — `(int screen, bool visible)` — cursor visibility flush; reader thread
     *  - `ID::scrolledRows`        — `(int count)` — rows scrolled off since last flush; reader thread
     *  - `ID::applicationCursor` / `ID::bracketedPaste` / ... — `(bool)` — mode flag flushes; reader thread
     *  - `ID::screenSwitch`        — `(int newScreen, int oldRow, int oldCol, bool oldVisible, int, int, bool, uint32_t)` — screen switch mediation; reader thread
     *
     *  @note READER THREAD for most event handlers; callAsync handlers land on message thread. */
    jam::Function::Map<juce::Identifier, void> events;

private:
    //==============================================================================
    /** @brief Live cell buffer — owned by Terminal::Session. */
    Grid& grid;

    /** @brief Cross-thread string buffer — owned by Terminal::Session. */
    TextBuffer& textBuffer;

    /** @brief Terminal parameter store — constructed after references are bound. */
    State state;

    /** @brief Terminal state machine — pen, cursor, modes, Grid writes. */
    Video video;

    /**
     * @brief Image decode and SKiT filepath handler.
     *
     * Encapsulates Sixel, Kitty, and iTerm2 image decoding plus the SKiT
     * filepath preview protocol.  Receives raw payloads from Video command
     * handlers, fires `"imageDecoded"` and `"previewFile"` events.
     *
     * @see Skit
     * @see Skit::processDCS()
     * @see Skit::processAPC()
     * @see Skit::processOSC1337()
     */
    Skit skit;

    /** @brief Command dispatch map — Parser → Processor → Video. */
    jam::Function::Map<Command::Type, void> commands;

    /** @brief Stable UUID identifying this Processor across process boundaries. */
    const juce::String uuid;

    /** @brief VT100/VT520 state machine that decodes PTY output. */
    std::unique_ptr<Parser> parser;

    /** @brief Registers all command handlers that forward Parser actions to Video. */
    void registerCommands() noexcept;

    /** @brief Registers Processor-owned event handlers on the events map.
     *
     *  Handlers registered here intercept Video-fired events that require
     *  State access (which Video does not hold): link ID assignment, shell
     *  integration row conversion (screen-relative → absolute), and others.
     *
     *  Called once from the constructor, after `registerCommands()`.
     */
    void registerEvents() noexcept;

    /** @brief Stores pending dimensions for deferred application on the reader thread.
     *
     *  Called from `valueTreePropertyChanged` when State dimensions change.
     *  Stores cols/rows atomically. process() applies them at batch start.
     *
     *  @param cols  New terminal width in character columns.
     *  @param rows  New terminal height in character rows.
     *  @note MESSAGE THREAD — atomic stores only. No Video/Grid writes here.
     */
    void resized (int cols, int rows) noexcept;

    /** @brief Pending column count for deferred resize. Written on message thread, read on reader thread. */
    std::atomic<int> pendingCols { 0 };

    /** @brief Pending row count for deferred resize. Written on message thread, read on reader thread. */
    std::atomic<int> pendingRows { 0 };

    /** @brief Set to true when a resize is pending. Consumed by process(). */
    std::atomic<bool> resizePending { false };

    /** @brief Pending cell width in pixels for deferred setCellSize. */
    std::atomic<int> pendingCellWidth { 0 };

    /** @brief Pending cell height in pixels for deferred setCellSize. */
    std::atomic<int> pendingCellHeight { 0 };

    /** @brief Set to true when a cell size change is pending. Consumed by process(). */
    std::atomic<bool> cellSizePending { false };

    /** @brief ValueTree::Listener — reacts to top-down dimension changes from Display.
     *
     *  Fires on the message thread when State's ValueTree properties change.
     *  Handles `ID::cols` and `ID::visibleRows` to push new dimensions into Video.
     *  Mirrors the `ProcessorChain::parameterChanged` pattern from JFS.
     *
     *  @note MESSAGE THREAD.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
