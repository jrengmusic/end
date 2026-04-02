/**
 * @file Session.h
 * @brief Terminal session orchestrator: owns State, Grid, Parser, and TTY.
 *
 * `Session` is the top-level object that wires together all subsystems of the
 * terminal emulator.  It owns the four core components and routes data between
 * them via callbacks established in `setupCallbacks()`:
 *
 * ```
 *  ┌──────────────────────────────────────────────────────────────────┐
 *  │                         Session                                  │
 *  │                                                                  │
 *  │  ┌────────┐   ┌────────┐   ┌────────┐   ┌──────────────────┐     │
 *  │  │ State  │◄──│ Parser │◄──│  TTY   │   │      Grid        │     │
 *  │  │ (SSOT) │   │ (VT SM)│──►│(PTY I/O│   │ (ring-buf cells) │     │
 *  │  └────────┘   └────────┘   └────────┘   └──────────────────┘     │
 *  │       ▲            │                             ▲               │
 *  │       └────────────┴─────────────────────────────┘               │
 *  └──────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ### Data flow
 * 1. The PTY (TTY) delivers raw bytes on the **reader thread** via `tty->onData`.
 * 2. `Session::process()` forwards them to `Parser::process()`.
 * 3. The parser decodes VT sequences and writes cells to `Grid` / state to `State`.
 * 4. Responses (e.g. cursor-position reports) are buffered in the parser and
 *    flushed back to the PTY via `tty->onDrainComplete → parser.flushResponses()`.
 * 5. UI events (key presses, mouse, paste, resize) arrive on the **message thread**
 *    and are forwarded to the PTY via `tty->write()`.
 *
 * ### Thread safety
 * - `process()` — **READER THREAD** only (called from `tty->onData`).
 * - All other public methods — **MESSAGE THREAD** only.
 * - `State` and `Grid` handle their own internal thread safety.
 *
 * @see Grid    — ring-buffer cell storage with dirty tracking.
 * @see Parser  — VT100/VT520 state machine.
 * @see State   — APVTS-style atomic parameter store.
 * @see TTY     — platform PTY abstraction (UnixTTY / WindowsTTY).
 */

#pragma once

#include <JuceHeader.h>

#include "../data/State.h"
#include "Grid.h"
#include "Parser.h"

class TTY;

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Session
 * @brief Top-level terminal emulator orchestrator.
 *
 * Session owns the four core subsystems (`State`, `Grid`, `Parser`, `TTY`) and
 * is the single object that the UI layer interacts with.  It exposes a minimal
 * public API:
 *
 * - **Input** — `handleKeyPress()`, `paste()`, `writeMouseEvent()`, `writeFocusEvent()`
 * - **Resize** — `resized()` (opens the PTY on first call, then requests resize)
 * - **Output** — `process()` (called internally by the TTY reader thread)
 * - **State access** — `getState()`, `getGrid()`
 * - **Lifecycle callbacks** — `onShellExited`, `onClipboardChanged`, `onBell`
 *
 * ### Ownership
 * Session holds all four subsystems by value (State, Grid, Parser) or by
 * `std::unique_ptr` (TTY).  Destruction order is guaranteed by declaration order:
 * TTY is destroyed first (stops the reader thread), then Parser, Grid, State.
 *
 * @note Construct and destroy on the **message thread**.
 *
 * @see Grid, Parser, State, TTY
 */
class Session
{
public:
    /**
     * @brief Constructs the Session, creates the platform TTY, and wires callbacks.
     *
     * Instantiates `Grid (state)` and `Parser (state, grid)`, then creates the
     * platform-specific TTY (`UnixTTY` on macOS/Linux, `WindowsTTY` on Windows).
     * Calls `setupCallbacks()` to connect all inter-component lambdas.
     *
     * @note MESSAGE THREAD — must be constructed on the message thread.
     */
    Session();

    /**
     * @brief Destroys the Session.
     *
     * The TTY unique_ptr is destroyed first, which stops the reader thread and
     * closes the PTY file descriptor before the Parser and Grid are torn down.
     *
     * @note MESSAGE THREAD — must be destroyed on the message thread.
     */
    ~Session();

    /**
     * @brief Notifies the session of a terminal viewport resize.
     *
     * Always calls `grid.resize()` and `parser.resize()` directly on the
     * message thread.  On the first call, defers `tty->open()` via
     * `callAsync`.  On subsequent calls while the TTY is running, calls
     * `platformResize()` to send SIGWINCH to the shell.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     *
     * @note MESSAGE THREAD only.
     */
    void resized (int cols, int rows);

    /**
     * @brief Sets the initial working directory for the shell process.
     *
     * Must be called before the first resized() call which opens the PTY.
     *
     * @param path  Absolute path to start the shell in.
     * @note MESSAGE THREAD.
     */
    void setWorkingDirectory (const juce::String& path);

    /**
     * @brief Overrides the shell program and arguments for this session.
     *
     * When set, `resized()` launches this command instead of
     * `Config::Key::shellProgram`.  Must be called before the first
     * `resized()` call which opens the PTY.
     *
     * @param program  Shell command or executable path.
     * @param args     Arguments passed to the command.
     * @note MESSAGE THREAD.
     */
    void setShellProgram (const juce::String& program, const juce::String& args);

    /**
     * @brief Reads an environment variable from the shell's foreground process.
     *
     * Queries the TTY for the foreground PID and reads the named variable
     * from that process's environment.  Returns an empty string on failure.
     *
     * @param varName  The environment variable name (e.g. "PATH").
     * @return The variable's value, or empty on failure.
     * @note MESSAGE THREAD.
     */
    juce::String getShellEnvVar (const juce::String& varName) const;

    /**
     * @brief Injects an environment variable into the shell startup environment.
     *
     * Must be called before the first resized() call which opens the PTY.
     * Delegates to TTY::addShellEnv().
     *
     * @param key    The environment variable name.
     * @param value  The environment variable value.
     * @note MESSAGE THREAD.
     */
    void addExtraEnv (const juce::String& key, const juce::String& value);

    /**
     * @brief Translates a JUCE key press into a VT escape sequence and writes it to the PTY.
     *
     * Reads `ID::applicationCursor` from the ValueTree (message-thread SSOT) to
     * select the correct cursor-key encoding (ANSI vs. application mode).
     * Uses `Keyboard::map()` to perform the translation.
     *
     * @param key  The JUCE key press event to translate and forward.
     *
     * @note MESSAGE THREAD only.
     * @see Keyboard::map()
     */
    void handleKeyPress (const juce::KeyPress& key);

    /**
     * @brief Pastes text into the terminal, optionally wrapping in bracketed-paste markers.
     *
     * Reads `ID::bracketedPaste` from the ValueTree.  If bracketed paste mode is
     * active, wraps the text with `ESC[200~` … `ESC[201~` before writing to the PTY.
     *
     * @param text  The UTF-8 text to paste.
     *
     * @note MESSAGE THREAD only.
     */
    void paste (const juce::String& text);

    /** @brief Writes raw bytes to the PTY. @note MESSAGE THREAD. */
    void writeToPty (const char* data, int len);

    /**
     * @brief Encodes and writes a mouse event to the PTY.
     *
     * Supports both X10 (legacy) and SGR (extended) mouse encoding.  Reads
     * `ID::mouseSgr` from the ValueTree to select the encoding format.
     *
     * @param button  Mouse button index (0 = left, 1 = middle, 2 = right).
     * @param col     Zero-based column of the mouse event.
     * @param row     Zero-based row of the mouse event.
     * @param press   `true` for button press, `false` for button release.
     *
     * @note MESSAGE THREAD only.
     */
    void writeMouseEvent (int button, int col, int row, bool press) noexcept;

    /**
     * @brief Writes a focus-in or focus-out event to the PTY.
     *
     * Sends `ESC[I` (focus gained) or `ESC[O` (focus lost) if `ID::focusEvents`
     * mode is enabled.
     *
     * @param gained  `true` when the terminal window gained focus, `false` when lost.
     *
     * @note MESSAGE THREAD only.
     */
    void writeFocusEvent (bool gained) noexcept;

    /**
     * @brief Processes raw bytes received from the PTY.
     *
     * Forwards the byte buffer to `Parser::process()`.  Called exclusively from
     * the `tty->onData` callback, which fires on the reader thread.
     *
     * @param data    Pointer to the raw byte buffer from the PTY.
     * @param length  Number of bytes in the buffer.
     *
     * @note READER THREAD only — never call from the message thread.
     */
    void process (const char* data, int length) noexcept;

    /**
     * @brief Returns a const reference to the terminal State.
     * @return Const reference to the owned `State` object.
     * @note Safe to call from any thread for atomic reads; ValueTree reads are MESSAGE THREAD only.
     */
    const State& getState() const noexcept;

    /**
     * @brief Returns a mutable reference to the terminal State.
     * @return Mutable reference to the owned `State` object.
     * @note Safe to call from any thread for atomic reads; ValueTree reads are MESSAGE THREAD only.
     */
    State& getState() noexcept;

    /**
     * @brief Returns a mutable reference to the terminal Grid.
     * @return Mutable reference to the owned `Grid` object.
     * @note Cell reads on the message thread must hold `grid.getResizeLock()`.
     */
    Grid& getGrid() noexcept;

    /**
     * @brief Returns a const reference to the terminal Grid.
     * @return Const reference to the owned `Grid` object.
     * @note Cell reads on the message thread must hold `grid.getResizeLock()`.
     */
    const Grid& getGrid() const noexcept;

    /**
     * @brief Returns a const reference to the VT parser.
     *
     * Used by `Terminal::Component::scanViewportForLinks()` to read OSC 8
     * hyperlink spans accumulated on the reader thread.
     *
     * @return Const reference to the owned `Parser` object.
     * @note The caller must only read parser state when no active parse is in
     *       progress (i.e. after a repaint snapshot has been taken).
     */
    const Parser& getParser() const noexcept;
    Parser& getParser() noexcept;

    /**
     * @brief Returns whether the child shell process has exited.
     * @return `true` if the PTY child process has terminated.
     * @note MESSAGE THREAD only.
     */
    bool hasShellExited() const noexcept;

    /** @name Lifecycle callbacks
     *  Set these before calling `resized()` to receive asynchronous notifications.
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

private:
    /**
     * @brief Wires all inter-component callbacks.
     *
     * Connects:
     * - `parser.writeToHost` → `tty->write()`
     * - `tty->onData` → `Session::process()`
     * - `tty->onDrainComplete` → `parser.flushResponses()`
     * - `tty->onExit` → `onShellExited` (via callAsync)
     * - `parser.onClipboardChanged` → `onClipboardChanged` (via callAsync)
     * - `parser.onBell` → `onBell` (via callAsync)
     * - `parser.onDesktopNotification` → `onDesktopNotification` (via callAsync)
     *
     * @note Called once from the constructor on the MESSAGE THREAD.
     */
    void setupCallbacks();

    /**
     * @brief Sideloads shell integration scripts and configures env var injection.
     *
     * Dispatches to the appropriate shell-specific integration strategy based on
     * the shell name:
     * - **zsh**        — ZDOTDIR approach: writes `.zshenv` + `end-integration` to
     *                    `~/.config/end/zsh/`, injects `ZDOTDIR` via `addShellEnv`.
     * - **bash**       — ENV + --posix approach: writes `bash_integration.bash` to
     *                    `~/.config/end/`, injects `ENV` and `END_BASH_INJECT`,
     *                    prepends `--posix` to @p args.
     * - **fish**       — XDG_DATA_DIRS prepend: writes `end-shell-integration.fish`
     *                    to `~/.config/end/fish/vendor_conf.d/`, injects
     *                    `XDG_DATA_DIRS` via `addShellEnv`.
     * - **pwsh/powershell** — Launch-args modification: writes
     *                    `powershell_integration.ps1` to `~/.config/end/`, replaces
     *                    @p args with `-NoLogo -NoProfile -NoExit -Command`. ...`.
     *
     * When integration is disabled: deletes all integration files/dirs and clears
     * all env overrides.
     *
     * Does NOT write to the PTY.
     *
     * @param shell  Full shell program string (e.g. "zsh", "/usr/bin/zsh").
     * @param args   Shell arguments string, may be modified by some integrations.
     *
     * @note MESSAGE THREAD — must be called BEFORE tty->open().
     */
    void applyShellIntegration (const juce::String& shell, juce::String& args);

    /** @brief Terminal parameter store — the SSOT for the UI. */
    State state;

    /** @brief Ring-buffer cell grid with dual-screen and dirty tracking. */
    Grid grid;

    /** @brief VT100/VT520 state machine that decodes PTY output. */
    Parser parser;

    /** @brief Platform PTY abstraction (UnixTTY or WindowsTTY). */
    std::unique_ptr<TTY> tty;

    /** @brief Initial working directory for the shell process. */
    juce::String workingDirectory;

    /** @brief Shell program override; empty = use Config default. */
    juce::String shellOverride;

    /** @brief Shell arguments override; used only when shellOverride is set. */
    juce::String shellArgsOverride;

};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
