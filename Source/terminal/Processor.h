/**
 * @file Processor.h
 * @brief Terminal pipeline orchestrator: owns Parser and Video, references Buffer<Row> and State.
 *
 * `Processor` is the pipeline half of the terminal emulator.  It owns the
 * Parser and Video, and routes bytes through the
 * Buffer<Row> and State received from Session:
 *
 * ```
 *  bytes → Processor::process → Parser → Video → State / Buffer<Row> → Display
 * ```
 *
 * The PTY-side (TTY + History) lives in `terminal::Session`.  Processor is
 * data-source agnostic — it receives bytes via `process()` whether they come
 * from a local PTY callback, an IPC byte-forward, or a history replay.
 *
 * ### Data flow
 * 1. Caller delivers raw bytes on the READER THREAD via `process()`.
 * 2. `process()` forwards to `Parser::process()`.
 * 3. The parser decodes VT sequences and calls Video action methods directly.
 * 4. Video writes cells to `Buffer<Row>`.
 * 5. Video fires events; Processor handlers write State atomics (event dispatch).
 * 6. Responses (e.g. cursor-position reports) are buffered in Video and
 *    flushed back via the `writeToHost` event handler registered in `events`.
 * 7. State::flush() propagates atomic values to the ValueTree on the timer tick,
 *    notifying Display via `juce::ValueTree::Listener`.
 * 8. Resize: Screen owns DiscreteStateTransition; Display wires trigger/onStop.
 *    Processor exposes finishResize() (onStop handler); dimension detection lives in vTPC.
 *    Cold-start allocation happens in the constructor before the first bytes flow.
 *    Cell pixel changes are applied to Video directly — no DST coalescing needed.
 *
 * ### Thread safety
 * - `process()` — READER THREAD only.
 * - All other public methods — MESSAGE THREAD only.
 * - `State` and `Buffer<Row>` handle their own internal thread safety.
 *
 * @see terminal::Session — owns TTY and History (PTY side).
 * @see jam::Buffer<jam::Row> — flat row storage, stateless data buffer.
 * @see jam::DiscreteStateTransition — coalescing resize lifecycle manager (owned by Screen).
 * @see Parser     — VT100/VT520 state machine.
 * @see Video      — terminal state machine: pen, cursor, modes, Buffer<Row> writes.
 * @see State      — atomic terminal parameter store.
 */

#pragma once

#include <JuceHeader.h>

#include "Keyboard.h"
#include "State.h"
#include "TextBuffer.h"
#include "tty/TTY.h"
#include "Parser.h"
#include "Skit.h"
#include "Video.h"
#include "Notifications.h"
namespace terminal
{
/*____________________________________________________________________________*/
/**
 * @class Processor
 * @brief Terminal pipeline orchestrator — owns Parser and Video, receives Buffer<Row>& and State& from Session.
 *
 * Processor owns the Parser and Video.
 * Buffer<Row> and State are owned by terminal::Session and passed by reference at construction.
 * Processor has no knowledge of TTY, PTY, or IPC.
 * Bytes arrive via `process()` from whichever source owns the byte stream
 * (local `terminal::Session` callback, IPC byte-forward, or history replay).
 *
 * Display subscribes as a `juce::ValueTree::Listener` on State's ValueTree.
 * `State::flush()` propagates atomic values to the ValueTree on the timer tick,
 * which notifies Display to repaint.
 *
 * ### Boundary contract
 * `State`, `buffer`, `uuid`, `video`, `parser`, and `events` are private.
 * The events map is private. Session registers handlers via setHostWriter(), setInputWriter(), and setTTY().
 * External callers access state through the public getter API:
 * - `getState()` / `getBuffer()` — mutable and const references.
 * - `getUuid()` — const reference to the stable session UUID.
 * - `setHostWriter()` — registers the `writeToHost` event handler.
 * - `setTTY()` — transfers TTY ownership; Processor calls platformResize directly.
 * - `flushResponses()` — flushes queued device responses to the host.
 *
 * ### Public surface
 * - **Input encoding** — `encodeKeyPress()`, `encodePaste()`, `encodeMouseEvent()`, `encodeFocusEvent()` (const, no side effects)
 * - **Output** — `process()` (called on the reader thread by the byte source)
 * - **Response flushing** — `flushResponses()` (reader thread; called by Session on drain-complete)
 * - **TTY ownership** — `setTTY()` transfers unique_ptr from Session; `platformResize()` delegates to owned TTY
 *
 * @note Construct and destroy on the **message thread**.
 *
 * @see jam::Buffer<jam::Row>, Parser, Video, State, terminal::Session, jam::DiscreteStateTransition
 */
class Processor : public juce::ValueTree::Listener
{
public:
    //==============================================================================
    /**
     * @brief Constructs the Processor and wires the parser, video, and resize pipeline.
     *
     * Receives Buffer<Row> and TextBuffer by reference from the owning Session,
     * constructs State, Video, and Parser.
     * UUID is provided by the caller.
     * Call `setHostWriter()` immediately after construction to route video
     * responses (e.g. cursor-position reports) to the appropriate sink.
     *
     * @param buffer      Live row buffer — owned by terminal::Session.
     * @param textBuffer  Cross-thread string buffer owned by terminal::Session.
     * @param cols        Initial terminal column count.
     * @param rows        Initial terminal row count.
     * @param uuid        Stable UUID for this Processor — generated once by the caller.
     * @note MESSAGE THREAD — must be constructed on the message thread.
     */
    Processor (jam::Buffer<jam::Row>& buffer,
               TextBuffer& textBuffer,
               cell cols,
               cell rows,
               const juce::String& uuid);

    /**
     * @brief Destroys the Processor.
     * @note MESSAGE THREAD — must be destroyed on the message thread.
     */
    ~Processor() override;

    /**
     * @brief Encodes a JUCE key press into a VT escape sequence.
     *
     * Reads `id::applicationCursor` from the ValueTree (message-thread SSOT) to
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
     * Reads `id::bracketedPaste` from the ValueTree.  If bracketed paste mode is
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
    juce::String encodeMouseEvent (int button, cell col, cell row, bool press) const noexcept;

    /**
     * @brief Encodes a focus-in or focus-out event into a VT byte sequence.
     *
     * Returns `ESC[I` (focus gained) or `ESC[O` (focus lost) if `id::focusEvents`
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
     * Pure bytes-to-Buffer pipeline: forwards to Parser::process(), flushes Video,
     * and consumes the paste echo gate.  No lock, no suspended check, no cell-size
     * detection — all resize work is handled exclusively by DiscreteStateTransition.
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
     * @brief Returns a mutable reference to the row buffer.
     * @return Mutable reference to the Session-owned `Buffer<Row>` object.
     * @note MESSAGE THREAD only.
     */
    jam::Buffer<jam::Row>& getBuffer() noexcept;

    /**
     * @brief Returns a const reference to the row buffer.
     * @return Const reference to the Session-owned `Buffer<Row>` object.
     * @note MESSAGE THREAD only.
     */
    const jam::Buffer<jam::Row>& getBuffer() const noexcept;

    /** @brief Delivers SIGWINCH after resize settles.
     *  Called from Screen's DST onStop handler (wired by Display).
     *  @note MESSAGE THREAD. */
    void finishResize() noexcept;

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
     * @brief Transfers TTY ownership to this Processor.
     *
     * Called by Session after wiring all TTY callbacks but before the reader
     * thread starts producing bytes.  After this call, Processor calls
     * tty->platformResize() from finishResize().
     *
     * @param ttyToOwn  TTY unique_ptr to take ownership of.
     * @note MESSAGE THREAD.
     */
    void setTTY (std::unique_ptr<TTY> ttyToOwn) noexcept;

    /**
     * @brief Performs the OS-level PTY resize (SIGWINCH to shell).
     *
     * Delegates to tty->platformResize() when a TTY is owned and its thread
     * is running.  Called from Session::onDrainComplete for the sync-resize path.
     * No-op if no TTY is owned.
     *
     * @param cols        New column count.
     * @param rows        New row count.
     * @param pixelWidth  Total viewport width in physical pixels (0 if unknown).
     * @param pixelHeight Total viewport height in physical pixels (0 if unknown).
     * @note READER THREAD (called from tty->onDrainComplete during sync-resize).
     */
    void platformResize (cell cols, cell rows, int pixelWidth = 0, int pixelHeight = 0) noexcept;

    /** @brief Fires after cwd and foreground process are written to State.
     *
     *  Set by nexus::Daemon in daemon mode to broadcast a stateUpdate PDU.
     *  Fires from valueTreePropertyChanged on outputBlockTop and promptRow changes.
     *  @note MESSAGE THREAD.
     */
    std::function<void()> onStateFlush;

    /** @brief Writes raw input bytes to the PTY via the registered handler.
     *
     *  Display and Input call this instead of touching events directly.
     *  Forwards to the writeInput handler registered by Session.
     *
     *  @param data  Raw byte buffer.
     *  @param len   Number of bytes.
     *  @note MESSAGE THREAD.
     */
    void writeInput (const char* data, int len) noexcept;

    /** @brief Registers the handler invoked by writeInput().
     *
     *  Session calls this to wire PTY stdin. The handler receives raw bytes
     *  to be written to the PTY.
     *
     *  @param handler  Callback invoked with (const char* data, int len).
     *  @note MESSAGE THREAD — call before the first writeInput() invocation.
     */
    void setInputWriter (std::function<void (const char*, int)> handler) noexcept;

private:
    //==============================================================================
    /** @brief Owned PTY — transferred from Session via setTTY().  Null in IPC client mode. */
    std::unique_ptr<TTY> tty;

    /** @brief Live row buffer — owned by terminal::Session. */
    jam::Buffer<jam::Row>& buffer;

    /** @brief Cross-thread string buffer — owned by terminal::Session. */
    TextBuffer& textBuffer;

    /** @brief Terminal parameter store — constructed after references are bound. */
    State state;

    /** @brief Events map — Video fired events routed through Processor to Session.
     *
     *  Registered event keys:
     *  - `id::writeToHost`         — `(const char*, int)` — flushed from Video on reader thread
     *  - `id::imageDecoded`        — see Video::onImageDecoded signature — reader thread
     *  - `id::previewFile`         — `(const juce::String&, int, int, int, int)` — reader thread
     *  - `id::registerLink`        — `(const juce::String& uri, const juce::String& params)` — OSC 8 open
     *  - `id::writeInput`          — `(const char*, int)` — PTY stdin; wired via setInputWriter()
     *  - `id::bell`                — `()` — BEL 0x07; writes `\a` to stderr; message thread
     *  - `id::clipboardChanged`    — `(const juce::String&)` — OSC 52 clipboard write; message thread
     *  - `id::desktopNotification` — `(const juce::String& title, const juce::String& body)` — OSC 9/777; message thread
     *  - `id::activeScreen`        — `(int)` — active screen index flush; reader thread
     *  - `id::cursorRow`           — `(int screen, int row)` — cursor row flush; reader thread
     *  - `id::cursorCol`           — `(int screen, int col)` — cursor col flush; reader thread
     *  - `id::cursorVisible`       — `(int screen, bool visible)` — cursor visibility flush; reader thread
     *  - `id::applicationCursor` / `id::bracketedPaste` / ... — `(bool)` — mode flag flushes; reader thread
     *  - `id::screenSwitch`        — `(int newScreen, int oldRow, int oldCol, bool oldVisible)` — screen switch mediation; reader thread
     *
     *  @note READER THREAD — all handlers execute on the reader thread except bell, clipboardChanged, and desktopNotification (message thread via callAsync). */
    jam::Function::Map<juce::Identifier, void> events;

    /** @brief Terminal state machine — pen, cursor, modes, Buffer<Row> writes. */
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

    /** @brief Stable UUID identifying this Processor across process boundaries. */
    const juce::String uuid;

    /** @brief VT100/VT520 state machine that decodes PTY output. */
    std::unique_ptr<Parser> parser;

    // Keyboard mode stack — per-screen progressive enhancement flags (CSI u protocol).
    static constexpr int maxKeyboardStackDepth { 16 };
    std::array<uint32_t, 2 * maxKeyboardStackDepth> keyboardModeStack {};
    std::array<int, 2> keyboardModeStackSize {};

    void pushKeyboardMode (int screen, uint32_t flags) noexcept;
    void popKeyboardMode (int screen, int count) noexcept;
    void setKeyboardMode (int screen, uint32_t flags, int mode) noexcept;
    void resetKeyboardMode (int screen) noexcept;

    /** @brief Registers Processor-owned event handlers on the events map.
     *
     *  Handlers registered here intercept Video-fired events that require
     *  State access (which Video does not hold): link ID assignment, shell
     *  integration row conversion (screen-relative → absolute), and others.
     *
     *  Called once from the constructor.
     */
    void registerEvents() noexcept;

    /** @brief ValueTree::Listener — reacts to top-down property changes from Display.
     *
     *  Fires on the message thread when State's ValueTree properties change.
     *  Handles shell integration callbacks (outputBlockTop → command process query,
     *  promptRow → clear foreground), and cell pixel changes (cellWidth, cellHeight)
     *  applied directly to Video.
     *
     *  @note MESSAGE THREAD.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
