/**
 * @file Processor.h
 * @brief Terminal pipeline orchestrator: owns State, Grid, and Parser.
 *
 * `Processor` is the pipeline half of the terminal emulator.  It owns the
 * three core pipeline components and routes bytes through them:
 *
 * ```
 *  bytes → Processor::process → Parser → State / Grid → Display
 * ```
 *
 * The PTY-side (TTY + History) lives in `Terminal::Session`.  Processor is
 * data-source agnostic — it receives bytes via `process()` whether they come
 * from a local PTY callback, an IPC byte-forward, or a history replay.
 *
 * ### Data flow
 * 1. Caller delivers raw bytes on the READER THREAD via `process()`.
 * 2. `process()` forwards to `Parser::process()`.
 * 3. The parser decodes VT sequences and writes cells to `Grid` / state to `State`.
 * 4. Responses (e.g. cursor-position reports) are buffered in the parser and
 *    flushed back via `parser.writeToHost` (wired by the owner to the appropriate
 *    sink — local TTY write or IPC output).
 * 5. `sendChangeMessage()` fires on the reader thread to notify Display.
 *
 * ### Thread safety
 * - `process()` — READER THREAD only.
 * - All other public methods — MESSAGE THREAD only.
 * - `State` and `Grid` handle their own internal thread safety.
 *
 * @see Terminal::Session — owns TTY and History (PTY side).
 * @see Grid    — ring-buffer cell storage with dirty tracking.
 * @see Parser  — VT100/VT520 state machine.
 * @see State   — atomic terminal parameter store.
 */

#pragma once

#include <JuceHeader.h>

#include "../data/State.h"
#include "Grid.h"
#include "Parser.h"

// Forward declaration: Display is in component/, Processor is in terminal/logic/.
// Including TerminalDisplay.h here would create a circular include chain.
namespace Terminal { class Display; }

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Processor
 * @brief Terminal pipeline orchestrator — owns State, Grid, and Parser.
 *
 * Processor owns the pipeline only.  It has no knowledge of TTY, PTY, or IPC.
 * Bytes arrive via `process()` from whichever source owns the byte stream
 * (local `Terminal::Session` callback, IPC byte-forward, or history replay).
 *
 * Processor inherits `juce::ChangeBroadcaster`.  `process()` calls
 * `sendChangeMessage()` so that `Terminal::Display` (subscribed as a
 * `juce::ChangeListener`) repaints after each update.
 *
 * - **Input encoding** — `encodeKeyPress()`, `encodePaste()`, `encodeMouseEvent()`, `encodeFocusEvent()` (const, no side effects)
 * - **Resize** — `resized()` resizes grid and parser only; PTY SIGWINCH is handled by Terminal::Session.
 * - **Output** — `process()` (called on the reader thread by the byte source)
 * - **State/Grid** — public `state`, `grid`, `uuid` members
 * - **Lifecycle callbacks** — `onShellExited`, `onClipboardChanged`, `onBell`
 *
 * @note Construct and destroy on the **message thread**.
 *
 * @see Grid, Parser, State, Terminal::Session
 */
class Processor : public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    /** @brief Default terminal column count used when no geometry is known yet. */
    static constexpr int defaultCols { 80 };

    /** @brief Default terminal row count used when no geometry is known yet. */
    static constexpr int defaultRows { 24 };

    //==============================================================================
    /** @brief Terminal parameter store — accessible directly by Display and Nexus. */
    State state;

    /** @brief Ring-buffer cell grid with dual-screen and dirty tracking. */
    Grid grid;

    /** @brief Stable UUID identifying this Processor across process boundaries. */
    const juce::String uuid;

    //==============================================================================
    /**
     * @brief Constructs the Processor and wires the parser callbacks.
     *
     * Constructs State, Grid, and Parser.  UUID is provided by the caller — no
     * internal generation.  The `parser.writeToHost` callback must be set by the
     * owner immediately after construction to route parser responses (e.g.
     * cursor-position reports) to the appropriate sink.
     *
     * @param cols  Initial terminal column count.
     * @param rows  Initial terminal row count.
     * @param uuid  Stable UUID for this Processor — generated once by the caller.
     * @note MESSAGE THREAD — must be constructed on the message thread.
     */
    Processor (int cols, int rows, const juce::String& uuid);

    /**
     * @brief Destroys the Processor.
     * @note MESSAGE THREAD — must be destroyed on the message thread.
     */
    ~Processor() = default;

    /**
     * @brief Resizes the grid and notifies the parser.
     *
     * Resizes Grid and Parser only.  Does NOT touch any TTY — SIGWINCH is
     * handled by Terminal::Session.  Called from Display when its component
     * resizes; the caller also routes the resize to the appropriate TTY side
     * via `Nexus::Session::getContext()->sendResize()`.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     * @note MESSAGE THREAD only.
     */
    void resized (int cols, int rows) noexcept;

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
     * Forwards the byte buffer to `Parser::process()`.  Called on the READER
     * THREAD by whichever byte source owns this Processor — the `Terminal::Session`
     * onBytes callback (local/daemon) or the IPC byte-forward dispatch (client).
     *
     * @param data    Pointer to the raw byte buffer.
     * @param length  Number of bytes in the buffer.
     * @note READER THREAD only — never call from the message thread.
     */
    void process (const char* data, int length) noexcept;

    /**
     * @brief Returns a const reference to the VT parser.
     *
     * Used by `Terminal::Display::scanViewportForLinks()` to read OSC 8
     * hyperlink spans accumulated on the reader thread.
     *
     * @return Const reference to the owned `Parser` object.
     * @note The caller must only read parser state when no active parse is in
     *       progress (i.e. after a repaint snapshot has been taken).
     */
    const Parser& getParser() const noexcept;
    Parser& getParser() noexcept;

    /**
     * @brief Creates and returns a Display for this Processor.
     *
     * Mirrors `AudioProcessor::createEditor()`.  The returned Display holds a
     * back-reference to this Processor.  Caller is responsible for placing the
     * Display in the component hierarchy.
     *
     * @param font  Font instance providing metrics, shaping, and rasterisation.
     * @return Unique pointer to the newly created Display.
     * @note MESSAGE THREAD.
     */
    std::unique_ptr<Display> createDisplay (jreng::Typeface& font);

    /** @name Lifecycle callbacks
     *  Set these to receive asynchronous notifications.
     *  All callbacks are invoked on the **message thread** via `callAsync`.
     * @{ */

    /** Called when the child shell process exits. */
    std::function<void()> onShellExited;

    /** Called when the terminal writes to the clipboard (OSC 52). */
    std::function<void (const juce::String&)> onClipboardChanged;

    /** Called when the terminal rings the bell (BEL, 0x07). */
    std::function<void()> onBell;

    /** Called when the terminal requests a desktop notification (OSC 9 / OSC 777).
     *  `title` is empty for OSC 9. */
    std::function<void (const juce::String&, const juce::String&)> onDesktopNotification;

    /** @} */

    /** @brief VT100/VT520 state machine that decodes PTY output. */
    std::unique_ptr<Parser> parser;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
