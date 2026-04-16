/**
 * @file WindowsTTY.h
 * @brief Windows pseudo-terminal implementation using the ConPTY API and overlapped I/O.
 *
 * WindowsTTY wraps the Windows Pseudo Console (ConPTY) API introduced in
 * Windows 10 version 1809 (build 17763).  ConPTY provides a VT-compatible
 * pseudo-terminal that allows Win32 console applications to be driven by a
 * terminal emulator without the legacy Console Host.
 *
 * ### ConPTY pipe topology (Microsoft Terminal model)
 *
 * ```
 *  WindowsTTY (parent)                  ConPTY                  shell (child)
 *  ─────────────────────────────────    ──────────────────────  ──────────────
 *  pipe (server, DUPLEX, overlapped) ◄──► client (hInput)  ──► child stdin
 *                                    ◄──► client (hOutput) ◄── child stdout/stderr
 *
 *  Pipe created via NtCreateNamedPipeFile (unnamed, overlapped, full-duplex).
 *  Client opened via NtCreateFile relative to server handle.
 *  Same client handle passed for both hInput and hOutput to CreatePseudoConsole.
 * ```
 *
 * ### Pipe creation — NT API (matches Microsoft Terminal exactly)
 * The pipe is created using `NtCreateNamedPipeFile` and `NtCreateFile` loaded
 * via `GetProcAddress` on `ntdll.dll`, replicating the `CreateOverlappedPipe`
 * implementation in Microsoft Terminal's `utils.cpp`.  This avoids the
 * read/write contention that occurs with `CreateNamedPipeW` on a duplex pipe.
 *
 * 1. A handle to `\Device\NamedPipe\` is opened once via `NtCreateFile` and
 *    cached in a static local.
 * 2. The server end is created via `NtCreateNamedPipeFile` with an empty
 *    `UNICODE_STRING` name (unnamed pipe) relative to the pipe directory.
 *    `CreateOptions = 0` → asynchronous (overlapped) I/O.
 * 3. The client end is opened via `NtCreateFile` relative to the server handle,
 *    with `FILE_NON_DIRECTORY_FILE` and an empty name.
 *
 * ### Overlapped I/O model
 * Both read and write operations use overlapped I/O with manual-reset events:
 * - `waitForData()` issues an overlapped `ReadFile`, waits for the event, and
 *   stores the result in an internal buffer.
 * - `read()` copies from the internal buffer to the caller's buffer.
 * - `write()` issues an overlapped `WriteFile` and waits for completion.
 *
 * ### Handle ownership table
 *
 * | Handle           | Owner after open()  | Purpose                              |
 * |------------------|---------------------|--------------------------------------|
 * | `pipe`           | WindowsTTY (parent) | Duplex overlapped I/O with ConPTY    |
 * | `client`         | Closed              | Passed to CreatePseudoConsole        |
 * | `pseudoConsole`  | WindowsTTY          | ConPTY handle for resize / close     |
 * | `process`        | WindowsTTY          | Child process handle for exit checks |
 * | `readEvent`      | WindowsTTY          | Manual-reset event for read OVERLAPPED  |
 * | `writeEvent`     | WindowsTTY          | Manual-reset event for write OVERLAPPED |
 *
 * ### Thread model
 * - `open()`, `close()`, `write()`, `platformResize()` — message thread
 * - `read()`, `waitForData()` — reader thread (called from TTY::run)
 *
 * ### Shutdown sequence
 * 1. `signalThreadShouldExit()` — tell reader thread to stop.
 * 2. `ClosePseudoConsole()` — sends CTRL_CLOSE_EVENT to clients, then breaks
 *    the pipe.  **Must** happen while the reader thread is still alive to avoid
 *    a ConPTY deadlock.  The broken pipe unblocks the reader's overlapped read.
 * 3. `stopThread (5000)` — wait for reader thread to exit.
 * 4. Clean up process handle (TerminateProcess as last resort).
 * 5. Close pipe handle.
 *
 * @see TTY         Abstract base class and reader thread
 * @see WindowsTTY.cpp  Implementation details
 * @see https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/
 * @see https://github.com/microsoft/terminal/blob/main/src/cascadia/TerminalConnection/ConptyConnection.cpp
 * @see https://github.com/microsoft/terminal/blob/main/src/types/utils.cpp  (CreateOverlappedPipe)
 */

#pragma once
#include "TTY.h"

#ifdef JUCE_WINDOWS
#include <windows.h>

/**
 * @class WindowsTTY
 * @brief Windows ConPTY backend using a single duplex overlapped pipe (NT API).
 *
 * Manages the pseudo-console handle, a single duplex overlapped unnamed pipe
 * created via `NtCreateNamedPipeFile`, and the child process handle.  The
 * reader thread (inherited from TTY) calls `waitForData()` and `read()` on
 * the pipe server end.  The message thread calls `write()` on the same pipe
 * server end, serialised by `writeLock`.
 *
 * @par Pipe creation
 * The pipe is created using the NT API (`NtCreateNamedPipeFile` /
 * `NtCreateFile`) loaded via `GetProcAddress`, matching Microsoft Terminal's
 * `CreateOverlappedPipe` implementation exactly.  This eliminates the
 * read/write contention that `CreateNamedPipeW` introduces on a duplex pipe.
 *
 * @par Overlapped read model
 * `waitForData()` issues an overlapped `ReadFile` into `readBuffer`, waits for
 * `readEvent`, and stores the byte count in `readBufferBytes`.  `read()` then
 * copies from `readBuffer` into the caller's buffer.  A `readPending` flag
 * prevents double-issuing a read while one is already in flight.
 *
 * @par Thread model
 * - `open()`, `close()`, `write()`, `platformResize()` — message thread
 * - `read()`, `waitForData()` — reader thread (called from TTY::run)
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
     * Creates an unnamed duplex overlapped pipe via `NtCreateNamedPipeFile`,
     * calls `CreatePseudoConsole()` with the client end for both `hInput` and
     * `hOutput`, then spawns the configured shell with the ConPTY attribute.
     * Starts the reader thread on success.
     *
     * @param cols             Initial terminal width in character columns.
     * @param rows             Initial terminal height in character rows.
     * @param shell            Shell program name or absolute path (e.g. "cmd.exe",
     *                         "pwsh").  Resolved via `%PATH%` when not absolute.
     * @param args             Space-separated arguments for the shell (e.g. "--login").
     * @param workingDirectory Optional initial working directory for the shell.
     *                         If empty, the shell inherits the parent's cwd.
     * @return                 `true` on success; `false` if any Win32 or NT call fails.
     *
     * @note MESSAGE THREAD context.
     */
    bool open (int cols, int rows, const juce::String& shell,
               const juce::String& args = {}, const juce::String& workingDirectory = {}) override;

    /**
     * @brief Close the ConPTY, pipe, and child process.
     *
     * Sequence:
     * 1. `signalThreadShouldExit()` — ask the reader thread to stop.
     * 2. `ClosePseudoConsole()` — breaks the pipe, unblocking the reader's
     *    overlapped read.  Called while the reader thread is still alive.
     * 3. `stopThread (5000)` — wait up to 5 s for the reader thread to exit.
     * 4. Check if child process is still alive; if so, `TerminateProcess()`.
     * 5. Close all remaining handles.
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
     * @brief Copy bytes from the internal overlapped read buffer to the caller.
     *
     * Returns bytes that were stored by the most recent completed `waitForData()`
     * call.  When the buffer is exhausted, attempts an immediate overlapped read
     * with a zero-timeout wait so the TTY::run() drain loop can pull multiple
     * chunks without re-entering `waitForData()` between each one.
     *
     * Returns -1 if the pipe is broken (indicated by `readBufferBytes == -1`).
     * Returns 0 if no data is available right now.
     *
     * @param buf       Destination buffer.
     * @param maxBytes  Maximum bytes to copy.
     * @return          Bytes copied (> 0), 0 if no data available,
     *                  -1 on broken pipe or fatal error.
     *
     * @note READER THREAD context.
     */
    int read (char* buf, int maxBytes) override;

    /**
     * @brief Write bytes to the ConPTY pipe (keyboard input to the shell).
     *
     * Issues an overlapped `WriteFile` and waits for completion.  Serialised
     * by `writeLock` to prevent interleaving from concurrent callers.
     *
     * @param buf  Data to write.
     * @param len  Number of bytes.
     * @return     `true` if all bytes were written successfully.
     *
     * @note MESSAGE THREAD context.
     */
    bool write (const char* buf, int len) override;

    /**
     * @brief Issue an overlapped read and block until data arrives or the timeout expires.
     *
     * If no read is already pending, issues `ReadFile` with the `readOverlapped`
     * structure.  Then waits on `readEvent` for up to `timeoutMs` milliseconds.
     * On completion, stores the byte count in `readBufferBytes` and resets
     * `readPending`.  Returns `true` if bytes are available or the pipe is broken
     * (so the caller can detect EOF via `read()`).
     *
     * @param timeoutMs  Maximum wait time in milliseconds.
     * @return           `true` if data is available or the pipe is broken;
     *                   `false` on timeout.
     *
     * @note READER THREAD context.
     */
    bool waitForData (int timeoutMs) override;

    /**
     * @brief Returns the PID of the child shell process.
     *
     * ConPTY does not expose a foreground process group like Unix tcgetpgrp.
     * Returns the shell PID obtained from CreateProcessW at open() time.
     *
     * @return The shell PID, or -1 if not running.
     * @note Any thread.
     */
    int getForegroundPid() const noexcept override;

    /**
     * @brief Writes the process name for the given PID into the buffer.
     *
     * Uses QueryFullProcessImageNameW to obtain the executable path,
     * then extracts the filename stem.
     *
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer for the null-terminated name.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return Number of bytes written (excluding null terminator), or 0 on failure.
     * @note Any thread.
     */
    int getProcessName (int pid, char* buffer, int maxLength) const noexcept override;

    /**
     * @brief Reads the current working directory of the given process via its PEB.
     *
     * Opens the target process, walks its Process Environment Block (PEB) via
     * NtQueryInformationProcess and ReadProcessMemory, extracts the
     * RTL_USER_PROCESS_PARAMETERS::CurrentDirectory.DosPath UNICODE_STRING,
     * converts it to UTF-8, replaces backslashes with forward slashes, and
     * writes the result into @p buffer.
     *
     * NtQueryInformationProcess is resolved once per process from ntdll.dll via
     * GetProcAddress.
     *
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer for the null-terminated UTF-8 path.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return           Number of bytes written (excluding null terminator), or 0 on failure.
     * @note Any thread.
     */
    int getCwd (int pid, char* buffer, int maxLength) const noexcept override;

    /** @} */

protected:
    /**
     * @brief Resize the ConPTY window.
     *
     * Calls `ResizePseudoConsole()` with the new dimensions.  The child
     * process receives a `WINDOW_BUFFER_SIZE_EVENT` console event.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     *
     * @note MESSAGE THREAD context — called by TTY::platformResize() when dims changed.
     */
    void doPlatformResize (int cols, int rows) override;

private:
    /** @brief ConPTY handle created by `CreatePseudoConsole()`.  nullptr when not open. */
    HPCON pseudoConsole { nullptr };

    /** @brief Server end of the duplex overlapped unnamed pipe (NT API).
     *
     *  The parent uses this handle for all I/O with ConPTY.  Both read and
     *  write operations use overlapped I/O on this single handle.
     */
    HANDLE pipe { INVALID_HANDLE_VALUE };

    /** @brief Child process handle — used for `isRunning()` and `TerminateProcess()`. */
    HANDLE process { INVALID_HANDLE_VALUE };

    /** @brief OVERLAPPED structure for asynchronous `ReadFile` operations.
     *
     *  `hEvent` is set to `readEvent`.  Owned exclusively by the reader thread
     *  after `open()` returns.
     */
    OVERLAPPED readOverlapped {};

    /** @brief OVERLAPPED structure for asynchronous `WriteFile` operations.
     *
     *  `hEvent` is set to `writeEvent`.  Owned exclusively by the message thread
     *  (serialised by `writeLock`).
     */
    OVERLAPPED writeOverlapped {};

    /** @brief Manual-reset event signalled when a `ReadFile` overlapped operation completes. */
    HANDLE readEvent { INVALID_HANDLE_VALUE };

    /** @brief Manual-reset event signalled when a `WriteFile` overlapped operation completes. */
    HANDLE writeEvent { INVALID_HANDLE_VALUE };

    /** @brief Internal buffer that receives bytes from the overlapped `ReadFile`.
     *
     *  Sized to `READ_CHUNK_SIZE` (65536 bytes).  Written by `waitForData()`,
     *  consumed by `read()`.
     */
    juce::HeapBlock<char> readBuffer;

    /** @brief Number of valid bytes currently in `readBuffer`. */
    int readBufferBytes { 0 };

    /** @brief Current read offset into `readBuffer` (bytes already consumed by `read()`). */
    int readBufferOffset { 0 };

    /** @brief `true` while an overlapped `ReadFile` is in flight on the reader thread. */
    bool readPending { false };

    /** @brief Serialises `write()` calls from the message thread. */
    juce::CriticalSection writeLock;

    /** @brief PID of the child shell process, obtained from CreateProcessW.  0 when not running. */
    DWORD childPid { 0 };
};

#endif
