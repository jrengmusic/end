/**
 * @file Processor.h
 * @brief Terminal pipeline orchestrator — owns Parser, Video, CellFifo, and Model&.
 *
 * Processor is the Controller in the audio-plugin architecture:
 *
 * ```
 *  bytes → process() → Parser → Video → events → Processor handlers → CellFifo → drainHistory()/drainActive() → Display
 * ```
 *
 * The PTY-side TTY lives in `terminal::Processor` (created by startTTY()).  Processor is
 * data-source agnostic — it receives bytes via `process()` whether they come
 * from a local PTY callback, an IPC byte-forward, or a history replay.
 *
 * ### Data flow
 * 1. Caller delivers raw bytes on the READER THREAD via `process()`.
 * 2. `process()` forwards to `Parser::process()`.
 * 3. The parser decodes VT sequences and calls Video action methods directly.
 * 4. Video writes cells to `Buffer<Row>` and fires events.
 * 5. Processor reader-thread handlers push departing rows into the history ring
 *    via pushHistory() (on pushLine) and live viewport rows into the active ring
 *    via pushActive() (on screenDirty), then bump the screenDirty ValueTree counter.
 * 6. Model::flush() propagates atomic values to the ValueTree on the timer tick,
 *    notifying Display via `juce::ValueTree::Listener`.
 * 7. Display pulls via drainHistory()/drainActive() on the message thread and feeds its own CodeView.
 * 8. Resize: prepare() resizes Video's grid only — CellFifo and CodeView untouched.
 *    Cell pixel changes are applied to Video directly.
 *
 * ### Thread safety
 * - `process()` — READER THREAD only.
 * - All other public methods — MESSAGE THREAD only.
 * - `Model` and `Buffer<Row>` handle their own internal thread safety.
 * - CellFifo is SPSC: reader thread pushes, message thread drains.
 *
 * @see terminal::Session — owns TTY (via Processor::startTTY) and orchestrates startup.
 * @see jam::Buffer<jam::Row> — flat row storage, stateless data buffer.
 * @see jam::Resizer — coalescing resize coordinator (owned by Session).
 * @see Parser     — VT100/VT520 state machine.
 * @see Video      — terminal state machine: pen, cursor, modes, Buffer<Row> writes.
 * @see Model      — atomic terminal parameter store.
 * @see CellFifo   — SPSC ring: reader pushes Char rows, message thread drains jam::CodeLine.
 */

#pragma once

#include <JuceHeader.h>

#include "Keyboard.h"
#include "Model.h"
#include "TextBuffer.h"
#include "Winsize.h"
#include "tty/TTY.h"
#if JUCE_MAC || JUCE_LINUX
#include "tty/UnixTTY.h"
#elif JUCE_WINDOWS
#include "tty/WindowsTTY.h"
#endif
#include "CellFifo.h"
#include "Parser.h"
#include "Skit.h"
#include "Video.h"
#include "Notifications.h"
namespace terminal
{
/*____________________________________________________________________________*/
/**
 * @class Processor
 * @brief Terminal pipeline orchestrator — the single bridge owner and Controller.
 *
 * Processor owns Parser, Video, CellFifo, and holds Model& from Session.
 * Video owns Buffer<Row> — the flat cell buffer (reader thread, volatile).
 * Processor is the only object that orchestrates the reader→message bridge:
 * - Reader-thread event handlers push Char rows into CellFifo.
 * - drainHistory()/drainActive() expose history and active entries to the View (message thread).
 * - The View (Display) owns CodeView and applies the drained entries — Processor
 *   never names CodeView (no Controller→View poking).
 *
 * Processor owns TTY (created by startTTY()) and routes bytes through the VT pipeline.
 * Bytes arrive via `process()` from whichever source owns the byte stream
 * (local `terminal::Session` callback or IPC byte-forward).
 *
 * Display subscribes as a `juce::ValueTree::Listener` on Model's ValueTree.
 * `Model::flush()` propagates atomic values to the ValueTree on the timer tick,
 * which notifies Display to repaint.
 *
 * ### Boundary contract
 * `Model`, `uuid`, `video`, `parser`, `tty`, `cellFifo`, and `events` are private.
 * External callers access state through the public getter API:
 * - `getState()` — mutable and const references (Model is owned by Session).
 * - `getUuid()` — const reference to the stable session UUID.
 * - `startTTY()` — creates and opens the platform TTY, wires data/writeInput/writeToHost events.
 * - `flushResponses()` — flushes queued device responses to the host.
 * - `registerEvent()` — external collaborators wire functions to events via this pass-through.
 * - `drainHistory()` — message thread; joins continued rows into one logical jam::CodeLine from the history ring.
 * - `drainActive()` — message thread; yields one viewport row per call from the active ring.
 *
 * ### Public surface
 * - **Input encoding** — `encodeKeyPress()`, `encodePaste()`, `encodeMouseEvent()`, `encodeFocusEvent()` (const, no side effects)
 * - **Output** — `process()` (called on the reader thread by the byte source)
 * - **Response flushing** — `flushResponses()` (reader thread; called by drain-complete event handler)
 * - **TTY lifecycle** — `startTTY()` creates, wires, and opens the platform TTY; `prepare()` resizes Video's grid only — CellFifo and CodeView untouched
 * - **Bridge drain** — `drainHistory()`/`drainActive()` delegate to CellFifo; View calls these to pull rows
 *
 * @note Construct and destroy on the **message thread**.
 *
 * @see jam::Buffer<jam::Row>, Parser, Video, Model, terminal::Session, jam::Resizer, CellFifo
 */
class Processor : public juce::ValueTree::Listener
{
public:
    //==============================================================================
    /**
     * @brief Constructs the Processor and wires the parser and video pipeline.
     *
     * Constructs Video with the initial terminal dimensions and Parser.
     * Constructs CellFifo sized for scrollbackLines × cols worst-case slots.
     * UUID is provided by the caller.
     * Call `startTTY()` from Session::start() to create the platform TTY and
     * route video responses (e.g. cursor-position reports) to the PTY sink.
     *
     * @param stateRef    Terminal parameter store — owned by terminal::Session.
     * @param dims        Terminal dimensions in cells.
     * @param textBuffer  Cross-thread string buffer owned by terminal::Session.
     * @param uuid        Stable UUID for this Processor — generated once by the caller.
     * @note MESSAGE THREAD — must be constructed on the message thread.
     */
    Processor (Model& stateRef,
               jam::Cell::Rectangle dims,
               TextBuffer& textBuffer,
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
     * Clean pipeline entry point — forwards to Parser::process(), flushes Video,
     * and consumes the paste echo gate.  Lock acquisition and suspend check are
     * the caller's responsibility (host wrapper pattern — see Session.cpp onData).
     *
     * @param data    Pointer to the raw byte buffer.
     * @param length  Number of bytes in the buffer.
     * @note READER THREAD only — never call from the message thread.
     */
    void process (const char* data, int length) noexcept;

    /**
     * @brief Returns a mutable reference to the terminal parameter store.
     * @return Mutable reference to the owned `Model` object.
     * @note MESSAGE THREAD only.
     */
    Model& getState() noexcept;

    /**
     * @brief Returns a const reference to the terminal parameter store.
     * @return Const reference to the owned `Model` object.
     * @note MESSAGE THREAD only.
     */
    const Model& getState() const noexcept;

    /** @brief Suspends processing — blocks until current process() completes, then sets flag.
     *  While suspended, process() outputs nothing. Same contract as AudioProcessor::suspendProcessing.
     *  @note MESSAGE THREAD — blocks on callbackLock. */
    void suspendProcessing (bool shouldBeSuspended) noexcept;

    /** @brief Returns true when processing is suspended.
     *  @note Only safe to read under callbackLock. */
    bool isSuspended() const noexcept { return suspended; }

    /** @brief Returns the callback lock — held during process() on the reader thread.
     *  Acquire from the message thread to serialize against process(). */
    const juce::CriticalSection& getCallbackLock() const noexcept { return callbackLock; }

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
     * @brief Creates the platform TTY, adds shell env vars, wires callbacks, and opens the PTY.
     *
     * Called by Session::start() after Display/Screen are created and grafted.
     * Creates the platform-specific TTY (UnixTTY/WindowsTTY) with the shared events map,
     * adds all shell integration env vars, wires onData for byte delivery and
     * writeInput/writeToHost events for PTY stdin/response routing, then calls open().
     *
     * No-op in remote (no-TTY) mode — Session::start() is never called for remote sessions.
     *
     * @param shell   Shell program path (e.g. "zsh", "/usr/bin/fish").
     * @param args    Shell arguments string.  Empty = none.
     * @param cwd     Initial working directory.  Empty = inherit.
     * @param env     Shell integration environment variable pairs.
     * @param dims    Terminal dimensions in cells.
     * @note MESSAGE THREAD — called from Session::start().
     */
    void startTTY (const juce::String& shell,
                   const juce::String& args,
                   const juce::String& cwd,
                   const juce::StringPairArray& env,
                   jam::Cell::Rectangle dims) noexcept;

    /** @brief Registers a function against an event id — transparent pass-through to the events map.
     *
     *  External collaborators (nexus Link/Daemon) wire their own functions to named
     *  events without the events map being exposed. Add-only: callers register, never fire.
     *
     *  @tparam Args  Event signature argument types, matching the registered id.
     *  @param event  Event identifier.
     *  @param fn     Function to register under @p event.
     *  @note MESSAGE THREAD — call before the event first fires.
     */
    template <typename... Args, typename FunctionType>
    void registerEvent (juce::Identifier event, FunctionType&& fn) noexcept
    {
        events.add<Args...> (event, std::forward<FunctionType> (fn));
    }

    /** @brief Drains one logical history line from the history ring.  Delegates to CellFifo::drainHistory.
     *
     *  Display calls this on the message thread to pull departed scrollback rows.
     *  Continued rows are joined into one logical jam::CodeLine by CellFifo before being returned.
     *
     *  @param outLine  Receives the joined jam::CodeLine built from Char data and flags.
     *  @return true if a complete (or partial tail) entry was produced; false when the history ring is empty.
     *  @note MESSAGE THREAD only.
     */
    bool drainHistory (jam::CodeLine& outLine) noexcept;

    /** @brief Drains one viewport row from the active ring.  Delegates to CellFifo::drainActive.
     *
     *  Display calls this on the message thread to pull live viewport rows.
     *  One jam::CodeLine is produced per ring entry — no joining.
     *
     *  @param outLine  Receives the viewport row jam::CodeLine built from Char data and flags.
     *  @return true if an entry was produced; false when the active ring is empty.
     *  @note MESSAGE THREAD only.
     */
    bool drainActive (jam::CodeLine& outLine) noexcept;

    /** @brief Prepares Processor for new terminal dimensions.
     *  Resizes Video's grid only — CellFifo and CodeView are untouched (I3).
     *  Live zone refreshes on the next screenDirty tick after the shell redraws.
     *  Called by Session's Resizer stop trigger while processing is suspended.
     *  @param dims  Terminal dimensions in cells.
     *  @note MESSAGE THREAD — safe only while processing is suspended. */
    void prepare (jam::Cell::Rectangle dims) noexcept;


    /** @brief Writes raw input bytes to the PTY via the id::writeInput event.
     *
     *  Display and Input call this instead of touching events directly.
     *  Forwards to the id::writeInput event registered in startTTY() (PTY mode)
     *  or via registerEvent() (remote/IPC mode).
     *  No-op when id::writeInput is not registered.
     *
     *  @param data  Raw byte buffer.
     *  @param len   Number of bytes.
     *  @note MESSAGE THREAD.
     */
    void writeInput (const char* data, int len) noexcept;

private:
    //==============================================================================
    /** @brief Callback lock — held for the duration of process() on the reader thread.
     *  suspendProcessing() acquires this lock to serialize against process(). */
    juce::CriticalSection callbackLock;

    /** @brief Processing suspended flag — plain bool under callbackLock, matching AudioProcessor contract.
     *  Transient — only true during resize. */
    bool suspended { false };

    /** @brief Owned PTY — created by startTTY().  Null in IPC client mode (remote sessions). */
    std::unique_ptr<TTY> tty;

    /** @brief Cross-thread string buffer — owned by terminal::Session. */
    TextBuffer& textBuffer;

    /** @brief Two-ring SPSC buffer for lock-free cross-thread cell row delivery.
     *  history ring — pushLine handler writes departed scrollback rows via pushHistory(); message thread drains via drainHistory().
     *  active ring  — screenDirty handler writes live viewport rows via pushActive(); message thread drains via drainActive().
     *  Sized to {1,1} placeholder here; setSize() is called from the ctor and clearBuffer handler with live dims. */
    CellFifo cellFifo { 1, 1 };

    /** @brief Terminal parameter store — owned by terminal::Session, received by reference. */
    Model& state;

    /** @brief Events map — Video fired events routed through Processor to Session.
     *
     *  Registered event keys:
     *  - `id::writeToHost`         — `(const char*, int)` — flushed from Video on reader thread
     *  - `id::imageDecoded`        — see Video::onImageDecoded signature — reader thread
     *  - `id::previewFile`         — `(const juce::String&, int, int, int, int)` — reader thread
     *  - `id::registerLink`        — `(const juce::String& uri, const juce::String& params)` — OSC 8 open
     *  - `id::writeInput`          — `(const char*, int)` — PTY stdin; wired internally by startTTY() or via registerEvent() for remote sessions
     *  - `id::bell`                — `()` — BEL 0x07; writes `\a` to stderr; message thread
     *  - `id::clipboardChanged`    — `(const juce::String&)` — OSC 52 clipboard write; message thread
     *  - `id::desktopNotification` — `(const juce::String& title, const juce::String& body)` — OSC 9/777; message thread
     *  - `id::activeScreen`        — `(int)` — active screen index flush; reader thread
     *  - `id::cursor`              — `(int screen, int packed)` — packed cursor (row+col+visible+kbFlags); reader thread
     *  - `id::applicationCursor` / `id::bracketedPaste` / ... — `(bool)` — mode flag flushes; reader thread
     *
     *  @note READER THREAD — all handlers execute on the reader thread except bell, clipboardChanged, and desktopNotification (message thread via callAsync). */
    jam::Function::Map<juce::Identifier, void> events;

    /** @brief Dispatch map for PARAM-path valueTreePropertyChanged handlers.
     *  Keys are paramId values (cellWidth, cellHeight, screenDirty).
     *  Registered in the constructor after registerEvents(). */
    jam::Function::Map<juce::Identifier, void> parameters;

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
     *  Model access (which Video does not hold): link ID assignment, shell
     *  integration row conversion (screen-relative → absolute), and others.
     *
     *  Called once from the constructor.
     */
    void registerEvents() noexcept;

    /** @brief Applies cellWidth/cellHeight PARAM changes to Video. */
    void setCellSize() noexcept;

    /** @brief Recomputes displayName from foregroundProcess/cwd on the session root node. */
    void setDisplayName (const juce::ValueTree& tree) noexcept;

    /** @brief ValueTree::Listener — reacts to top-down property changes from Display.
     *
     *  Fires on the message thread when Model's ValueTree properties change.
     *  Handles cell pixel changes (cellWidth, cellHeight) applied directly to Video,
     *  and displayName recomputation from foregroundProcess / cwd.
     *  The screenDirty parameter is bumped by the reader-thread screenDirty event handler
     *  (after pushing live content to CellFifo); the message-thread handler here only
     *  dispatches the PARAM-path parameters map (setCellSize / future params).
     *  Foreground process query and clear happen on the reader thread in the
     *  outputBlockStart and promptRow event handlers (registerEvents).
     *
     *  @note MESSAGE THREAD.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
