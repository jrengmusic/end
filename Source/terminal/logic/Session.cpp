/**
 * @file Session.cpp
 * @brief Implementation of the terminal session orchestrator.
 *
 * Implements Session — the top-level object that wires `State`, `Grid`,
 * `Parser`, and `TTY` together.  See Session.h for the full architectural
 * overview and thread-safety contract.
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — background thread owned by TTY; only `process()`.
 *
 * @see Session.h
 */

#include "Session.h"

#if JUCE_MAC || JUCE_LINUX
#include "../tty/UnixTTY.h"
#elif JUCE_WINDOWS
#include "../tty/WindowsTTY.h"
#endif

#include "../data/Keyboard.h"
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Wires all inter-component callbacks.
 *
 * Called once from the constructor.  Establishes the full data-flow graph:
 *
 * @par Parser → TTY (host writes)
 * `parser.writeToHost` forwards VT response bytes (e.g. cursor-position
 * reports, device-attribute replies) directly to `tty->write()`.
 *
 * @par TTY → Session (incoming PTY data)
 * `tty->onData` calls `Session::process()` on the **reader thread**, which
 * forwards the raw bytes to `Parser::process()`.
 *
 * @par TTY drain → Parser flush
 * `tty->onDrainComplete` calls `parser.flushResponses()` so that any
 * buffered response sequences are sent after the write buffer drains.
 *
 * @par TTY resize → Parser + Grid
 * `tty->onResize` propagates the new dimensions to both `parser.resize()`
 * (updates the VT state machine's viewport) and `grid.resize()` (reallocates
 * the ring-buffer to match).
 *
 * @par TTY exit → onShellExited callback
 * `tty->onExit` invokes the public `onShellExited` callback if set.
 *
 * @par Parser clipboard/bell → public callbacks
 * `parser.onClipboardChanged` and `parser.onBell` forward to the
 * corresponding public `Session` callbacks. Title is now written
 * directly to State by Parser (no callback).
 *
 * @note MESSAGE THREAD — called once from the constructor.
 */
void Session::setupCallbacks()
{
    parser.writeToHost = [this] (const char* data, int len) { tty->write (data, len); };

    tty->onData = [this] (const char* data, int len) { process (data, len); };
    tty->onDrainComplete = [this] { parser.flushResponses(); };
    tty->onResize = [this] (int c, int r)
    {
        parser.resize (c, r);
        grid.resize();
    };
    tty->onExit = [this]
    {
        if (onShellExited)
        {
            onShellExited();
        }
    };

    parser.onClipboardChanged = [this] (const juce::String& c)
    {
        if (onClipboardChanged)
        {
            onClipboardChanged (c);
        }
    };
    parser.onBell = [this]
    {
        if (onBell)
        {
            onBell();
        }
    };
}

/**
 * @brief Constructs the Session, creates the platform TTY, and wires callbacks.
 *
 * Member initialisation order (matches declaration order in Session.h):
 * 1. `state`  — default-constructed terminal parameter store.
 * 2. `grid`   — constructed with a reference to `state` (reads dimensions).
 * 3. `parser` — constructed with references to `state` and `grid`.
 * 4. `tty`    — platform-specific PTY created via `std::make_unique`.
 *
 * After construction, `setupCallbacks()` connects all inter-component lambdas.
 * The PTY is **not** opened here; it is opened on the first `resized()` call.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Session::Session()
    : grid (state)
    , parser (state, grid)
{
#if JUCE_MAC || JUCE_LINUX
    tty = std::make_unique<UnixTTY> ();
#elif JUCE_WINDOWS
    tty = std::make_unique<WindowsTTY> ();
#endif

    setupCallbacks();
}

/**
 * @brief Destroys the Session.
 *
 * The compiler-generated destructor destroys members in reverse declaration
 * order: `tty` first (stops the reader thread and closes the PTY fd), then
 * `parser`, `grid`, and finally `state`.  This ordering ensures the reader
 * thread cannot call `process()` after `parser` or `grid` are torn down.
 *
 * @note MESSAGE THREAD — must be destroyed on the message thread.
 */
Session::~Session()
{
}


/**
 * @brief Notifies the session of a terminal viewport resize.
 *
 * On the **first call** (`ttyOpened == false`):
 * 1. Calls `parser.resize (cols, rows)` to initialise the VT state machine
 *    viewport before any data arrives.
 * 2. Calls `grid.resize()` to allocate the ring-buffer.
 * 3. Reads `Config::Key::shellProgram` and calls `tty->open()` to fork the
 *    shell and start the reader thread.
 * 4. Sets `ttyOpened = true`.
 *
 * On **subsequent calls** (`ttyOpened == true`):
 * - Calls `tty->requestResize (cols, rows)`, which sends `SIGWINCH` to the
 *   child process and fires `tty->onResize`, which in turn calls
 *   `parser.resize()` and `grid.resize()`.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 *
 * @note MESSAGE THREAD only.
 */
void Session::resized (int cols, int rows)
{
    if (not ttyOpened)
    {
        parser.resize (cols, rows);
        grid.resize();
        tty->open (cols, rows, Config::getContext()->getString (Config::Key::shellProgram), workingDirectory);
        ttyOpened = true;
    }
    else
    {
        tty->requestResize (cols, rows);
    }
}

void Session::setWorkingDirectory (const juce::String& path)
{
    workingDirectory = path;
}

/**
 * @brief Translates a JUCE key press into a VT escape sequence and writes it to the PTY.
 *
 * Reads `ID::applicationCursor` from the ValueTree (message-thread SSOT) to
 * select the correct cursor-key encoding:
 * - **Normal mode** — ANSI sequences (`ESC[A` … `ESC[D`).
 * - **Application mode** — SS3 sequences (`ESSA` … `ESSD`).
 *
 * Delegates the translation to `Keyboard::map()`.  If the key has no VT
 * mapping (returns an empty string), nothing is written to the PTY.
 *
 * @param key  The JUCE key press event to translate and forward.
 *
 * @note MESSAGE THREAD only — reads from the ValueTree.
 * @see Keyboard::map()
 */
void Session::handleKeyPress (const juce::KeyPress& key)
{
    // MESSAGE THREAD — read from ValueTree, the SSOT
    const bool applicationCursor { state.getTreeMode (ID::applicationCursor) };

    juce::String seq { Keyboard::map (key, applicationCursor) };
    if (seq.isNotEmpty())
    {
        tty->write (seq.toRawUTF8(), static_cast<int> (seq.getNumBytesAsUTF8()));
    }
}

/**
 * @brief Pastes text into the terminal, optionally wrapping in bracketed-paste markers.
 *
 * Reads `ID::bracketedPaste` from the ValueTree.  If bracketed paste mode is
 * active (set by the application via `ESC[?2004h`), wraps the text:
 *
 * @code
 * ESC[200~  <text>  ESC[201~
 * @endcode
 *
 * This allows the receiving application to distinguish pasted text from typed
 * input and suppress newline interpretation.
 *
 * Does nothing if `text` is empty or `tty` is null.
 *
 * @param text  The UTF-8 text to paste.
 *
 * @note MESSAGE THREAD only — reads from the ValueTree.
 */
void Session::paste (const juce::String& text)
{
    if (text.isNotEmpty() and tty != nullptr)
    {
        // MESSAGE THREAD — read from ValueTree, the SSOT
        const bool bracketed { state.getTreeMode (ID::bracketedPaste) };

        if (bracketed)
        {
            tty->write ("\x1b[200~", 6);
        }

        tty->write (text.toRawUTF8(), static_cast<int> (text.getNumBytesAsUTF8()));

        if (bracketed)
        {
            tty->write ("\x1b[201~", 6);
        }
    }
}

/**
 * @brief Writes a focus-in or focus-out event to the PTY.
 *
 * Sends a 3-byte sequence to the PTY if `ID::focusEvents` mode is enabled
 * (set by the application via `ESC[?1004h`):
 * - Focus gained: `ESC [ I`
 * - Focus lost:   `ESC [ O`
 *
 * Does nothing if focus-event mode is disabled.
 *
 * @param gained  `true` when the terminal window gained focus, `false` when lost.
 *
 * @note MESSAGE THREAD only — reads from the ValueTree.
 */
void Session::writeFocusEvent (bool gained) noexcept
{
    // MESSAGE THREAD
    if (state.getTreeMode (ID::focusEvents))
    {
        const char seq[3] { '\x1b', '[', gained ? 'I' : 'O' };
        tty->write (seq, 3);
    }
}

/**
 * @brief Encodes and writes a mouse event to the PTY.
 *
 * Reads `ID::mouseSgr` from the ValueTree to select the encoding format:
 *
 * @par SGR encoding (`ESC[<Pb;Px;PyM` / `ESC[<Pb;Px;Pym`)
 * Used when `ID::mouseSgr` is true (set by `ESC[?1006h`).  Supports
 * unlimited column/row values.  Final character is `M` for press, `m` for
 * release.
 *
 * @par X10 encoding (`ESC[M Pb Px Py`)
 * Legacy 6-byte encoding.  Button, column, and row are each clamped to
 * `[0, 223]` (X10 offset 32, max byte value 255) to fit in a single byte.
 * Columns and rows are 1-based in the encoded sequence.
 *
 * @param button  Mouse button index (0 = left, 1 = middle, 2 = right).
 * @param col     Zero-based column of the mouse event (converted to 1-based internally).
 * @param row     Zero-based row of the mouse event (converted to 1-based internally).
 * @param press   `true` for button press, `false` for button release.
 *
 * @note MESSAGE THREAD only — reads from the ValueTree.
 */
void Session::writeMouseEvent (int button, int col, int row, bool press) noexcept
{
    // MESSAGE THREAD — read from ValueTree, the SSOT
    const bool sgr { state.getTreeMode (ID::mouseSgr) };

    if (sgr)
    {
        const char finalChar { press ? 'M' : 'm' };
        const juce::String seq { juce::String ("\x1b[<")
                                 + juce::String (button) + ";"
                                 + juce::String (col + 1) + ";"
                                 + juce::String (row + 1)
                                 + finalChar };
        tty->write (seq.toRawUTF8(), static_cast<int> (seq.getNumBytesAsUTF8()));
    }
    else
    {
        constexpr int x10Offset { 32 };
        constexpr int x10Max    { 255 - x10Offset };

        const char bytes[6] { '\x1b', '[', 'M',
                               static_cast<char> (juce::jmin (button, x10Max) + x10Offset),
                               static_cast<char> (juce::jmin (col + 1, x10Max) + x10Offset),
                               static_cast<char> (juce::jmin (row + 1, x10Max) + x10Offset) };
        tty->write (bytes, 6);
    }
}

/**
 * @brief Processes raw bytes received from the PTY.
 *
 * Casts the byte buffer to `const uint8_t*` and forwards it to
 * `Parser::process()`, which drives the VT100/VT520 state machine.  The
 * parser writes decoded cells to `Grid` and updates `State` atomics; it also
 * buffers any response sequences that will be flushed via
 * `tty->onDrainComplete → parser.flushResponses()`.
 *
 * @param data    Pointer to the raw byte buffer received from the PTY.
 * @param length  Number of valid bytes in the buffer.
 *
 * @note READER THREAD only — called exclusively from `tty->onData`.
 *       Never call this from the message thread.
 */
void Session::process (const char* data, int length) noexcept
{
    // READER THREAD
    parser.process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));

    const int fgPid { tty->getForegroundPid() };

    if (fgPid > 0)
    {
        tty->getProcessName (fgPid, foregroundProcessBuffer, maxStringLength);
        state.setForegroundProcess (foregroundProcessBuffer);

        tty->getCwd (fgPid, cwdBuffer, maxStringLength);
        state.setCwd (cwdBuffer);
    }
}

/**
 * @brief Returns a const reference to the terminal State.
 * @return Const reference to the owned `State` object.
 * @note Atomic reads are safe from any thread; ValueTree reads are MESSAGE THREAD only.
 */
const State& Session::getState() const noexcept { return state; }

/**
 * @brief Returns a mutable reference to the terminal State.
 * @return Mutable reference to the owned `State` object.
 * @note Atomic reads are safe from any thread; ValueTree reads are MESSAGE THREAD only.
 */
State& Session::getState() noexcept { return state; }

/**
 * @brief Returns a mutable reference to the terminal Grid.
 * @return Mutable reference to the owned `Grid` object.
 * @note Cell reads on the message thread must hold `grid.getResizeLock()`.
 */
Grid& Session::getGrid() noexcept { return grid; }

/**
 * @brief Returns a const reference to the terminal Grid.
 * @return Const reference to the owned `Grid` object.
 * @note Cell reads on the message thread must hold `grid.getResizeLock()`.
 */
const Grid& Session::getGrid() const noexcept { return grid; }

/**
 * @brief Returns a ValueTree snapshot of the cursor state for the active screen.
 *
 * Delegates to `State::getCursorState()`.  The returned ValueTree contains
 * cursor row, col, visible flag, and wrapPending flag — all post-flush values
 * written by the parser on the reader thread and read here on the message thread.
 *
 * @return A ValueTree node with cursor position and visibility properties.
 * @note MESSAGE THREAD only — reads from the ValueTree.
 */
juce::ValueTree Session::getCursorState() noexcept { return state.getCursorState(); }

/**
 * @brief Returns whether the child shell process has exited.
 *
 * Delegates to `tty->hasShellExited()`.  Returns `false` if `tty` is null
 * (i.e. before the first `resized()` call).
 *
 * @return `true` if the PTY child process has terminated, `false` otherwise.
 * @note MESSAGE THREAD only.
 */
bool Session::hasShellExited() const noexcept
{
    bool exited { false };

    if (tty != nullptr)
    {
        exited = tty->hasShellExited();
    }

    return exited;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
