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
 * @par TTY resize → Grid + Parser
 * `tty->onResize` calls `grid.resize()` first — which acquires `resizeLock`
 * and writes the new dimensions to State atomically with the buffer
 * reallocation — then calls `parser.resize()` to reset scroll regions and
 * tab stops.  The Grid-first ordering ensures that State dimensions always
 * match the Grid's actual allocation from the message thread's perspective.
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
    tty->onDrainComplete = [this]
    {
        parser.flushResponses();
        state.clearPasteEchoGate();

        if (state.consumeSyncResize())
            tty->requestResize (grid.getCols(), grid.getVisibleRows());
    };
    tty->onResize = [this] (int c, int r)
    {
        grid.resize (c, r);
        parser.resize (c, r);
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
    if (tty != nullptr)
    {
        // Threat: onExit is dispatched via callAsync (TTY.cpp:100).  If we
        // close() first, the reader thread may post callAsync(onExit) just
        // before stopping.  That async callback captures `this` (Session)
        // and will fire on the message thread after Session is destroyed.
        //
        // Fix: null onExit BEFORE close().  The reader thread checks
        // `if (onExit)` before posting callAsync — if we null it first and
        // the reader hasn't posted yet, no async callback is queued.  If the
        // reader already posted, the lambda captured a copy of onExit by
        // reference through `this`, which will be dangling.  To handle that
        // case, close() calls stopThread() which waits for the reader to
        // fully exit.  After stopThread() returns, no more callbacks will be
        // posted.  Any already-posted callAsync is harmless because
        // onShellExited is a stateless lambda (captures nothing from Session).
        onShellExited = nullptr;
        tty->onExit = nullptr;
        tty->close();
        tty->onData = nullptr;
        tty->onResize = nullptr;
        tty->onDrainComplete = nullptr;
    }
}


/**
 * @brief Notifies the session of a terminal viewport resize.
 *
 * On the **first call** (`ttyOpened == false`):
 * 1. Calls `grid.resize (cols, rows)` to allocate the ring-buffer and write
 *    dimensions to State inside `resizeLock`.
 * 2. Calls `parser.resize (cols, rows)` to reset scroll regions and tab stops.
 * 3. Reads `Config::Key::shellProgram` and calls `tty->open()` to fork the
 *    shell and start the reader thread.
 * 4. Sets `ttyOpened = true`.
 *
 * On **subsequent calls** (`ttyOpened == true`):
 * - Calls `tty->requestResize (cols, rows)`, which sets the pending resize
 *   atomics.  The reader thread picks this up and calls `grid.resize()` then
 *   `parser.resize()`.
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
        grid.resize (cols, rows);
        parser.resize (cols, rows);

        if (not ttyOpenPending)
        {
            ttyOpenPending = true;

            juce::MessageManager::callAsync ([this]
            {
                if (ttyOpened)
                    return;

                const int finalCols { grid.getCols() };
                const int finalRows { grid.getVisibleRows() };

                grid.resize (finalCols, finalRows);
                parser.resize (finalCols, finalRows);

                const auto shell { shellOverride.isNotEmpty()
                    ? shellOverride
                    : Config::getContext()->getString (Config::Key::shellProgram) };
                const auto args { shellOverride.isNotEmpty()
                    ? shellArgsOverride
                    : Config::getContext()->getString (Config::Key::shellArgs) };
                tty->open (finalCols, finalRows, shell, args, workingDirectory);
                const juce::String shellName { shell.contains (juce::File::getSeparatorString())
                    ? juce::File (shell).getFileName()
                    : shell };
                state.get().setProperty (ID::shellProgram, shellName, nullptr);
                ttyOpened = true;
                ttyOpenPending = false;
            });
        }
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

void Session::setShellProgram (const juce::String& program, const juce::String& args)
{
    shellOverride = program;
    shellArgsOverride = args;
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
    juce::String seq;

#if JUCE_WINDOWS
    if (state.getTreeMode (ID::win32InputMode))
    {
        seq = Keyboard::encodeWin32Input (key);
    }
    else
#endif
    {
        const bool applicationCursor { state.getTreeMode (ID::applicationCursor) };
        const uint32_t keyboardFlags { state.getTreeKeyboardFlags() };
        seq = Keyboard::map (key, applicationCursor, keyboardFlags);
    }

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
        const bool bracketed { state.getTreeMode (ID::bracketedPaste) };
        const auto utf8 { text.toRawUTF8() };
        const int utf8Len { static_cast<int> (text.getNumBytesAsUTF8()) };

        if (bracketed)
        {
            static constexpr const char open[]  { "\x1b[200~" };
            static constexpr const char close[] { "\x1b[201~" };
            static constexpr int bracketLen { 6 };

            juce::HeapBlock<char> buf (static_cast<size_t> (bracketLen + utf8Len + bracketLen));
            std::memcpy (buf.get(), open, bracketLen);
            std::memcpy (buf.get() + bracketLen, utf8, static_cast<size_t> (utf8Len));
            std::memcpy (buf.get() + bracketLen + utf8Len, close, bracketLen);
            tty->write (buf.get(), bracketLen + utf8Len + bracketLen);
        }
        else
        {
            state.setPasteEchoGate (utf8Len);
            tty->write (utf8, utf8Len);
        }
    }
}

void Session::writeToPty (const char* data, int len)
{
    if (tty != nullptr)
    {
        tty->write (data, len);
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
    const bool useSgr { true };

    if (useSgr)
    {
        // SGR encoding
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
        // X10 encoding - 6-byte sequence
        // Clamp to [0, 223] (X10 offset 32, max byte value 255)
        const int clampedButton { juce::jlimit (0, 223, button + 32) };
        const int clampedCol { juce::jlimit (0, 223, col + 1 + 32) };
        const int clampedRow { juce::jlimit (0, 223, row + 1 + 32) };

        char seq[6] { '\x1b', '[', 'M',
                      static_cast<char> (clampedButton),
                      static_cast<char> (clampedCol),
                      static_cast<char> (clampedRow) };
        tty->write (seq, 6);
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
    state.consumePasteEcho (length);

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
