/**
 * @file WindowsTTY.h
 * @brief Windows pseudo-terminal implementation using the ConPTY API.
 *
 * WindowsTTY wraps the Windows Pseudo Console (ConPTY) API introduced in
 * Windows 10 version 1809 (build 17763).  ConPTY provides a VT-compatible
 * pseudo-terminal that allows Win32 console applications to be driven by a
 * terminal emulator without the legacy Console Host.
 *
 * ### ConPTY pipe topology
 *
 * ```
 *  WindowsTTY (parent)                  cmd.exe / shell (child)
 *  ─────────────────────────────────    ──────────────────────────────────
 *  inputWriter  ──PIPE──► pipeReadEnd ──► ConPTY ──► child stdin
 *  outputReader ◄──PIPE── pipeWriteEnd ◄── ConPTY ◄── child stdout/stderr
 * ```
 *
 * After `CreatePseudoConsole()` the pipe ends that ConPTY owns internally
 * (`pipeReadEnd`, `pipeWriteEnd`) are closed in the parent — ConPTY holds
 * them.  The parent retains `inputWriter` (to send keystrokes) and
 * `outputReader` (to receive terminal output).
 *
 * ### Process creation
 * The child process is created with `EXTENDED_STARTUPINFO_PRESENT` and the
 * `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE` attribute so that ConPTY intercepts
 * its console I/O.  The shell is hard-coded to `cmd.exe`; a future version
 * may read `COMSPEC` or a user preference.
 *
 * ### Status
 * Implementation is in progress.  `read()` uses a non-overlapped
 * `ReadFile` with a fallback to `GetOverlappedResult` for async pipes.
 *
 * @see TTY         Abstract base class and reader thread
 * @see WindowsTTY.cpp  Implementation details
 * @see https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/
 */

#pragma once
#include "TTY.h"

#ifdef JUCE_WINDOWS
#include <windows.h>

/**
 * @class WindowsTTY
 * @brief Windows ConPTY backend for the terminal PTY abstraction.
 *
 * Manages the pseudo-console handle, two anonymous pipe pairs, and the child
 * process handle.  The reader thread (inherited from TTY) calls `waitForData()`
 * and `read()` on the output pipe.  The message thread calls `write()` on the
 * input pipe.
 *
 * @par Thread model
 * - `open()`, `close()`, `write()` — message thread
 * - `read()`, `resize()`, `waitForData()` — reader thread (called from TTY::run)
 *
 * @see TTY
 */
class WindowsTTY final : public TTY
{
public:
    using TTY::TTY;

    /**
     * @brief Destructor — closes the ConPTY and terminates the shell process.
     *
     * Delegates to `close()`.
     *
     * @note MESSAGE THREAD context.
     */
    ~WindowsTTY() override;

    // =========================================================================
    /** @name TTY interface implementation
     * @{ */

    /**
     * @brief Open the ConPTY and spawn the shell process.
     *
     * Creates the two anonymous pipe pairs, calls `CreatePseudoConsole()` with
     * the initial size, then spawns the configured shell with the ConPTY
     * attribute.  Starts the reader thread on success.
     *
     * @param cols   Initial terminal width in character columns.
     * @param rows   Initial terminal height in character rows.
     * @param shell  Shell program name or absolute path (e.g. "cmd.exe",
     *               "pwsh").  Resolved via `%PATH%` when not absolute.
     * @return       `true` on success; `false` if any Win32 call fails.
     *
     * @note MESSAGE THREAD context.
     */
    bool open (int cols, int rows, const juce::String& shell) override;

    /**
     * @brief Close the ConPTY, pipes, and child process.
     *
     * Sequence:
     * 1. Signal reader thread to exit.
     * 2. `TerminateProcess()` — forcibly terminate the shell.
     * 3. `ClosePseudoConsole()` — release the ConPTY handle.
     * 4. Close all pipe handles (`outputReader`, `inputWriter`,
     *    `pipeReadEnd`, `pipeWriteEnd`).
     * 5. `stopThread (5000)` — wait for the reader thread to exit.
     *
     * @note MESSAGE THREAD context.
     */
    void close() override;

    /**
     * @brief Query whether the shell process is still alive.
     *
     * Uses `GetExitCodeProcess()` and checks for `STILL_ACTIVE`.
     *
     * @return `true` if the child process has not yet exited.
     *
     * @note May be called from any thread.
     */
    bool isRunning() const override;

    /**
     * @brief Read available bytes from the ConPTY output pipe.
     *
     * Attempts a synchronous `ReadFile()`.  If the pipe is in overlapped mode
     * and returns `ERROR_IO_PENDING`, falls back to `GetOverlappedResult` with
     * a non-blocking check.
     *
     * @param buf       Destination buffer.
     * @param maxBytes  Maximum bytes to read.
     * @return          Bytes read (> 0), 0 if no data available,
     *                  -1 on broken pipe or fatal error.
     *
     * @note READER THREAD context.
     */
    int read (char* buf, int maxBytes) override;

    /**
     * @brief Write bytes to the ConPTY input pipe (keyboard input to the shell).
     *
     * @param buf  Data to write.
     * @param len  Number of bytes.
     * @return     `true` if all bytes were written successfully.
     *
     * @note MESSAGE THREAD context.
     */
    bool write (const char* buf, int len) override;

    /**
     * @brief Resize the ConPTY window.
     *
     * Calls `ResizePseudoConsole()` with the new dimensions.  The child
     * process receives a `WINDOW_BUFFER_SIZE_EVENT` console event.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     *
     * @note READER THREAD context (dispatched via TTY::run resize handling).
     */
    void resize (int cols, int rows) override;

    /**
     * @brief Block until data is available on the output pipe or the timeout expires.
     *
     * Uses `WaitForSingleObject()` on the `outputReader` handle.
     *
     * @param timeoutMs  Maximum wait time in milliseconds.
     * @return           `true` if the handle is signalled; `false` on timeout
     *                   or if the handle is invalid.
     *
     * @note READER THREAD context.
     */
    bool waitForData (int timeoutMs) override;

    /** @} */

private:
    /** @brief ConPTY handle created by `CreatePseudoConsole()`.  nullptr when not open. */
    HPCON pseudoConsole { nullptr };

    /** @brief Read end of the output pipe — receives VT data from the shell.
     *
     *  The reader thread calls `ReadFile()` on this handle.
     */
    HANDLE outputReader { INVALID_HANDLE_VALUE };

    /** @brief Write end of the input pipe — sends keystrokes to the shell.
     *
     *  The message thread calls `WriteFile()` on this handle.
     */
    HANDLE inputWriter { INVALID_HANDLE_VALUE };

    /** @brief Child process handle — used for `isRunning()` and `TerminateProcess()`. */
    HANDLE process { INVALID_HANDLE_VALUE };

    /** @brief Temporary read end of the input pipe — passed to `CreatePseudoConsole()`,
     *         then closed after process creation. */
    HANDLE pipeReadEnd { INVALID_HANDLE_VALUE };

    /** @brief Temporary write end of the output pipe — passed to `CreatePseudoConsole()`,
     *         then closed after process creation. */
    HANDLE pipeWriteEnd { INVALID_HANDLE_VALUE };
};

#endif
