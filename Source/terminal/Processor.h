/**
 * @file terminal/Processor.h
 * @brief Terminal engine — AudioProcessor analog. Owns pipeline.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Model.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal engine — owns the reader-thread pipeline.
 *
 *  AudioProcessor analog in the MVP (Model-View-Processor) pattern. Owns
 *  `jam::terminal::Video`, `jam::terminal::Parser`, `jam::terminal::CellFifo`
 *  (three rings), the caller-owned `jam::terminal::Events` surface Video
 *  fires through, and the platform TTY (`jam::terminal::UnixTTY` /
 *  `jam::terminal::WindowsTTY`). Buffer (`jam::TextModel`, the document) is
 *  owned by Session, not Processor.
 *
 *  Holds a reference to Session's terminal::Model — registered as a
 *  jam::Model::Listener for Direction B. parameterChanged() fires on the
 *  CALLING thread (jam_Model.h:36-48), i.e. the message thread — it is the
 *  WAKE seam only, never reader-owned state.
 *
 *  @par Reader-thread event surface
 *  `jam::terminal::TTY::run()` (final, framework-owned) drives the reader
 *  thread; Processor never owns a thread of its own. Reader-thread work runs
 *  synchronously inside the TTY event handlers registered in
 *  registerTtyEvents(): `jam::ID::data` (one call per drained chunk — feeds
 *  `parser.process()`, which dispatches directly into `video`), and
 *  `jam::ID::drainComplete` (flushes Video's queued device responses, clears
 *  an expired DECSET 2026 SYNC_OUTPUT grant, and pushes the active screen's
 *  live rows into the CellFifo active ring). `jam::ID::shellExited` marks the
 *  SESSION `shellExited` Direction-A parameter.
 *
 *  @par Video's Events surface
 *  `videoEvents` is the caller-owned `jam::terminal::Events` aggregate Video
 *  fires through (jam_VideoEvents.h) — plain member assignment of static
 *  trampolines, precedent `end/tests/TestTerm.h`. `writeToHost` forwards
 *  device-response bytes to the TTY stdin. `pushLine` forwards a departed
 *  top row (now carrying the departing `jam::Row`'s own `flags` byte,
 *  translated via `toCellFifoFlags()`) to the CellFifo history ring —
 *  filtered to the NORMAL screen only (alternate-screen scrolls never reach
 *  real scrollback, matching every other terminal emulator's convention;
 *  `jam::terminal::Video` fires `pushLine` unconditionally for whichever
 *  screen scrolled). `stateChanged`/`textChanged`/`modeChanged` are the
 *  three Model-agnostic channels `jam::terminal::Video` fires every former
 *  Model-collaborator write through (zero Model knowledge, ARCHITECT ruling
 *  2026-07-05) — their trampolines (`onStateChanged`/`onTextChanged`/
 *  `onModeChanged`, below) resolve straight onto `model`, the ONLY place in
 *  this codebase that reconnects Video's decoded VT state to the Model it no
 *  longer references.
 *
 *  @par Ring sizing (S6) and live resize (P6/P9)
 *  `prepare()` sizes the three CellFifo rings exactly ONCE, before `start()`
 *  opens the TTY — the single call site S6 ratifies, safe because no reader
 *  thread exists yet (never called with pending entries). Ring capacities
 *  are a function of `scrollbackLines` and the viewport-independent
 *  `ringMaxCols`/`ringMaxRows` upper bounds only — a live winsize change
 *  never touches them (resizing CellFifo per SIGWINCH is the recorded R7
 *  anti-pattern, endless's own `Processor::prepare()` bug — never
 *  reproduced here).
 *
 *  A live winsize change instead uses the COOPERATIVE suspend flag
 *  (`suspendProcessing()`/`isSuspended()`) — Session's `jam::Resizer` START
 *  trigger raises it the instant Direction-B `winsize` changes; `onData()`/
 *  `onDrainComplete()` check it and skip their Video/CellFifo work while it
 *  is up (data arriving mid-resize is not fed to the parser — same contract
 *  as endless's own `suspendProcessing()`, "while suspended, process()
 *  outputs nothing" — reproduced here as a lock-free atomic instead of
 *  endless's blocking `CriticalSection`, since END's reader thread is
 *  framework-owned, `jam::terminal::TTY::run()`, FINAL, and the Threading
 *  Contract forbids any lock/wait/stall/yield/sleep on it). 16ms after the
 *  last change the Resizer's STOP trigger fires on the message thread:
 *  Session drains history+active fully, extracts the GROW-delta document
 *  tail into the popback ring (`pushPopback()`), then calls `setWinsize()`
 *  — which resizes Video's grid (`jam_CursorState.h`'s own "MESSAGE THREAD
 *  — called while processing is suspended" contract) and notifies the TTY
 *  (`tty->setWinsize()`, SIGWINCH/ConPTY) — before lowering the suspend flag.
 *
 *  @par Flagged gap — popback ring consumption
 *  `pushPopback()` queues the GROW-delta rows (message-thread producer); no
 *  `jam::terminal::Video` hook exists yet to consume `CellFifo::
 *  drainPopback()` (reader thread) and write those rows into the resized
 *  grid — `jam::terminal::Video` exposes no public verb for writing
 *  arbitrary rows into grid positions from outside `Parser`-driven action
 *  methods. Creating that hook is a jam_terminal (JAM-side) change, outside
 *  this pass's edit surface — flagged for ARCHITECT (PLAN-terminal-editor.md
 *  Step 6's own Decision Gate: "hook mechanism proposal goes to ARCHITECT at
 *  implementation time if any shape exceeds what S1 already names"). Rows
 *  pushed here sit inert in the popback ring (drop-oldest fabric absorbs
 *  them harmlessly) until that hook lands.
 */
struct Processor : public jam::Model::Listener
{
    /** @brief Constructs the Processor and registers it as a listener on
     *  @p terminalModel.
     *  @param terminalModel  Session's owned terminal::Model. Must outlive
     *                        this Processor (Session declares model before
     *                        processor — construction/destruction order).
     */
    explicit Processor (Model& terminalModel);

    ~Processor() override;

    /** @brief Sizes the three CellFifo rings from the current scrollbackLines
     *  Direction B value — S6's single call site:
     *  \code
     *  cells = scrollbackLines * ringMaxCols
     *  headers = scrollbackLines
     *  fifo.setSize(cells + headers, ringActiveCapacity, ringPopbackCapacity)
     *  \endcode
     *  Must run before `start()` — never called with pending entries.
     *  @note MESSAGE THREAD — called once, before the reader thread exists.
     */

    void prepare() noexcept;

    /** @brief Opens the platform TTY and starts the reader thread.
     *  Reads the current Direction-B `winsize` atom (already published by
     *  terminal::View's `resized()` before this is ever called) and applies
     *  it to `video.setWinsize()` before any bytes flow — satisfies
     *  `jam::terminal::Video`'s "call setWinsize() after construction"
     *  contract.
     *  @param shell             Shell program name or absolute path
     *                           (config.lua `shell.program`).
     *  @param args              Space-separated shell arguments
     *                           (config.lua `shell.args`).
     *  @param workingDirectory  Initial working directory. Empty = shell
     *                           inherits this process's cwd.
     *  @note MESSAGE THREAD.
     */
    void start (const juce::String& shell,
                const juce::String& args,
                const juce::String& workingDirectory);

    /** @brief Closes the TTY and stops the reader thread. Safe to call when
     *  the TTY was never started (no-op).
     *  @note MESSAGE THREAD.
     */
    void stop() noexcept;

    /** @brief Forwards raw keystroke/paste bytes to the TTY stdin —
     *  keystrokes bypass the Model entirely (terminal::View -> here -> TTY).
     *  No-op before `start()`.
     *  @param data      Raw bytes to write (UTF-8 encoded key sequence).
     *  @param numBytes  Number of bytes in @p data.
     *  @note MESSAGE THREAD.
     */
    void writeInput (const void* data, int numBytes) noexcept;

    /** @brief Drains one logical history line from the CellFifo history
     *  ring — forwards to the owned CellFifo (S3 Phase 1, Session::drain()).
     *  @param outLine  Receives the joined jam::TextLine.
     *  @return true if a complete (or partial tail) entry was produced.
     *  @note MESSAGE THREAD.
     */
    bool drainHistory (jam::TextLine& outLine) noexcept;

    /** @brief Drains one live viewport row from the CellFifo active ring —
     *  forwards to the owned CellFifo (S3 Phase 2, Session::drain()).
     *  @param outLine  Receives the viewport row jam::TextLine.
     *  @return true if an entry was produced.
     *  @note MESSAGE THREAD.
     */
    bool drainActive (jam::TextLine& outLine) noexcept;

    /** @brief Raises or lowers the cooperative reader-suspend flag (P6/P9
     *  live-resize protocol) — Session's Resizer START/STOP triggers call
     *  this. `onData()`/`onDrainComplete()` check `isSuspended()` and skip
     *  their Video/CellFifo work while it is up — see this struct's own doc
     *  comment.
     *  @note MESSAGE THREAD.
     */
    void suspendProcessing (bool shouldBeSuspended) noexcept;

    /** @brief Returns true when reading is suspended.
     *  @note Any thread — lock-free read.
     */
    bool isSuspended() const noexcept;

    /** @brief Returns Video's CURRENT (pre-resize) live row count — Session
     *  reads this before `setWinsize()` to compute the P9 height delta.
     *  @note MESSAGE THREAD — safe only while reading is suspended.
     */
    int getVisibleRows() const noexcept;

    /** @brief Pushes one row into the popback ring (P9 GROW case) —
     *  forwards to the owned CellFifo. Session calls this once per
     *  newly-live document line, oldest first, before `setWinsize()`.
     *  @param chars   Pointer to the row's Char data.
     *  @param count   Number of Chars in the row.
     *  @param flags   Row flags — bit 0: isContinued, bit 1: isJustified,
     *                 bits 2-4: mark (jam::TextLine::Mark).
     *  @note MESSAGE THREAD.
     */
    void pushPopback (const jam::Char* chars, int count, uint8_t flags) noexcept;

    /** @brief Applies new terminal dimensions to Video and notifies the TTY
     *  (P9 STOP sequence) — called while reading is suspended.
     *  `video.setWinsize()` resizes Video's grid — safe here per its own
     *  documented contract ("MESSAGE THREAD — called while processing is
     *  suspended", jam_CursorState.h). `tty->setWinsize()` is the OS-level
     *  ioctl TIOCSWINSZ / ResizePseudoConsole notification (SIGWINCH).
     *  CellFifo rings are NEVER resized here — see this struct's own doc
     *  comment (R7 anti-pattern).
     *  @param dims  New terminal dimensions in cells.
     *  @note MESSAGE THREAD — called while reading is suspended.
     */
    void setWinsize (jam::Cell::Rectangle dims) noexcept;

    /** @brief Direction B wake seam.
     *  @note Fires on the MESSAGE thread (jam_Model.h:36-48). Touches no
     *  reader-owned state.
     */
    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

private:
    /** @brief Video's own {80}/{24} construction-time default — Processor
     *  constructs Video with this placeholder rectangle (the real terminal::
     *  View has not been constructed yet when Session builds Processor);
     *  `start()` immediately re-syncs via `video.setWinsize()` from the
     *  already-published Direction-B winsize atom before any bytes flow. */
    static constexpr int defaultCols { 80 };
    static constexpr int defaultRows { 24 };

    /** @brief CellFifo requires non-zero construction capacities; `prepare()`
     *  is the real sizing call site (S6) — this is a placeholder only. */
    static constexpr int placeholderRingCapacity { 1 };

    /** @brief Upper bound on terminal columns used to size the history
     *  ring's cell budget (`cells = scrollbackLines * ringMaxCols`, S6) —
     *  independent of the live viewport width, which is Direction-B
     *  `winsize` and may change at any time. */
    static constexpr int ringMaxCols { 1024 };

    /** @brief Upper bound on terminal rows used to size the active/popback
     *  rings at one screenful each (`jam_CellFifo.h`'s own ctor doc:
     *  "popback's ratified capacity is one screenful, same order of
     *  magnitude as the active ring"). */
    static constexpr int ringMaxRows { 256 };

    /** @brief Registers the TTY events map (`jam::ID::data` / `drainComplete`
     *  / `shellExited`) — called once from `start()`, before `tty->open()`. */
    void registerTtyEvents();

    /** @brief `jam::ID::data` dispatch — one call per drained chunk. Feeds
     *  `parser.process()`, which dispatches directly into `video` on this
     *  (reader) thread, UNLESS reading is suspended (P6/P9 live resize) —
     *  bytes arriving mid-resize are not fed to the parser (cooperative
     *  skip, positive check on `isSuspended()`; no lock/wait/sleep).
     *  @note READER THREAD. */
    void onData (const char* data, int length) noexcept;

    /** @brief `jam::ID::drainComplete` dispatch — fires once after a full
     *  read drain. Flushes Video's queued device responses, clears an
     *  expired DECSET 2026 SYNC_OUTPUT grant, and pushes the active
     *  screen's live rows into the CellFifo active ring — UNLESS reading is
     *  suspended (P6/P9 live resize), same cooperative skip as onData().
     *  @note READER THREAD. */
    void onDrainComplete() noexcept;

    /** @brief `jam::ID::shellExited` dispatch — fires on EOF.
     *  @note READER THREAD. */
    void onShellExited() noexcept;

    /** @brief `videoEvents.writeToHost` trampoline — forwards device
     *  response bytes to the TTY stdin.
     *  @note READER THREAD (Video fires writeToHost on the reader thread). */
    static void onWriteToHost (void* context, const char* data, int length) noexcept;

    /** @brief `videoEvents.pushLine` trampoline — forwards a departed top
     *  row (with its own row-flags byte, translated via `toCellFifoFlags()`)
     *  to the CellFifo history ring, filtered to the NORMAL screen (see
     *  this struct's own doc comment).
     *  @note READER THREAD. */
    static void onPushLine (void* context, int screen, const jam::Char* chars, int count, uint8_t flags) noexcept;

    /** @brief Translates `jam::Row::flags` bit layout (flexWrap/collapsed/
     *  justify/mark, bits 0-5) into `jam::terminal::CellFifo`'s own flags
     *  byte layout (isContinued/isJustified/mark, bits 0-4) — the two types
     *  pack the same three concepts at different bit positions. */
    static uint8_t toCellFifoFlags (uint8_t rowFlags) noexcept;

    /** @brief `videoEvents.stateChanged` trampoline — resolves `(tag, id)`
     *  onto `model`'s `jam::Parameter<int>` and stores `value`. The generic
     *  commit-side counterpart of every scalar SESSION/NORMAL/ALTERNATE
     *  write `jam::terminal::Video` used to make directly through a Model
     *  collaborator (zero Model knowledge now — `jam_VideoEvents.h`).
     *  @note READER THREAD (Video fires stateChanged on the reader thread;
     *        `jam::Parameter<int>::setValue()` is lock-free any-thread). */
    static void onStateChanged (void* context, juce::Identifier tag, juce::Identifier id, int value) noexcept;

    /** @brief `videoEvents.textChanged` trampoline — resolves `(tag, id)`
     *  onto `model`'s `jam::ParameterText` and stores `(chars, length)`. The
     *  TEXT-group (title/cwd) counterpart of `onStateChanged()`.
     *  @note READER THREAD. */
    static void onTextChanged (void* context, juce::Identifier tag, juce::Identifier id, const char* chars, int length) noexcept;

    /** @brief `videoEvents.modeChanged` trampoline — forwards the decoded
     *  DEC private / ANSI mode change to `model.setMode()`, the vocabulary
     *  resolution Video itself no longer performs.
     *  @note READER THREAD. */
    static void onModeChanged (void* context, bool isPrivate, int number, int value) noexcept;

    /** @brief Session's owned VT state SSOT — Direction A/B parameter host. */
    Model& model;

    /** @brief Caller-owned event surface `jam::terminal::Video` fires
     *  through — plain member assignment of the trampolines above, assigned
     *  in the constructor before `video` is ever driven. */
    jam::terminal::Events videoEvents;

    /** @brief Terminal state machine — pen, cursor, modes, grid writes.
     *  Constructed with the {80}/{24} placeholder — see defaultCols/
     *  defaultRows doc. */
    jam::terminal::Video video;

    /** @brief VT100/VT520 DFA byte decoder — dispatches decoded actions
     *  directly onto `video`. */
    jam::terminal::Parser parser;

    /** @brief Three-ring lock-free transport — history/active (reader
     *  producer, message consumer) and popback (message producer, reader
     *  consumer). Constructed with placeholder capacities; `prepare()` is
     *  the real sizing call site (S6). */
    jam::terminal::CellFifo cellFifo;

    /** @brief TTY's own events map — `jam::ID::data` / `drainComplete` /
     *  `shellExited`, registered once in `registerTtyEvents()`. */
    jam::Function::Map<juce::Identifier, void> ttyEvents;

    /** @brief Cooperative reader-suspend flag (P6/P9 live-resize protocol)
     *  — set/cleared on the message thread by `suspendProcessing()`, read
     *  lock-free by `onData()`/`onDrainComplete()` on the reader thread. No
     *  lock, no wait, no sleep — the reader only ever reads this flag. */
    std::atomic<bool> suspended { false };

#if JUCE_WINDOWS
    /** @brief Directory `WindowsTTY` sideloads conpty.dll / OpenConsole.exe
     *  from — extracted from BinaryData once, before `tty` is constructed
     *  (WindowsTTY's ctor binds the reference for the process lifetime). */
    juce::File conptyDir;
#endif

    /** @brief Owned platform TTY — created in `start()`. Declared LAST so
     *  its destructor (stops the reader thread, joins) runs FIRST, before
     *  `video` / `parser` / `cellFifo` / `videoEvents` — every reader-thread
     *  handler above reaches through those members and must never run past
     *  their destruction. */
    std::unique_ptr<jam::terminal::TTY> tty;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
