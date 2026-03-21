/**
 * @file UnixTTY.h
 * @brief macOS / Linux pseudo-terminal implementation using openpty() + fork().
 *
 * UnixTTY creates a POSIX PTY pair via `openpty()`, forks a child process, and
 * connects the child's stdio to the slave fd.  The parent retains the master fd
 * for reading shell output and writing keyboard input.
 *
 * ### PTY pair lifecycle
 *
 * ```
 *  Parent (UnixTTY)                Child (shell)
 *  ─────────────────────────────   ─────────────────────────────────────
 *  openpty() → master, slaveFd     (inherits slaveFd from fork)
 *  fork()                          setsid() — new session leader
 *  close(slaveFd)                  ioctl(TIOCSCTTY) — controlling terminal
 *  fcntl(O_NONBLOCK) on master     dup2(slaveFd, 0/1/2) — stdio → PTY
 *  startThread()                   execl(shell, "-l") — replace with shell
 * ```
 *
 * ### Environment setup
 * The child sets `TERM=xterm-256color` unconditionally and sets `LANG=UTF-8`
 * if not already present in the environment.  The shell binary is taken from
 * the `SHELL` environment variable, falling back to `/bin/bash`.
 *
 * ### Destructor / shutdown sequence
 * 1. Signal reader thread to exit.
 * 2. Close master fd (causes the child's read to return EOF).
 * 3. Wait up to 500 ms for the child to exit voluntarily (SIGTERM + poll).
 * 4. Send SIGKILL and block on waitpid if the child is still alive.
 *
 * @see TTY       Abstract base class and reader thread
 * @see UnixTTY.cpp  Implementation details
 */

#pragma once
#include "TTY.h"

#if JUCE_MAC || JUCE_LINUX

#include <unistd.h>
#include <utility>
#include <vector>

/**
 * @class UnixTTY
 * @brief macOS / Linux PTY backend using openpty() and fork().
 *
 * Manages the master file descriptor and child process PID.  All blocking
 * I/O on the master fd is performed on the reader thread inherited from TTY.
 * The master fd is set to `O_NONBLOCK` after fork so that `read()` returns
 * immediately when no data is available.
 *
 * @par Thread model
 * - `open()`, `close()`, `write()` — message thread
 * - `read()`, `resize()`, `waitForData()` — reader thread (called from TTY::run)
 *
 * @see TTY
 */
class UnixTTY final : public TTY
{
public:
    using TTY::TTY;

    /**
     * @brief Destructor — closes the PTY and terminates the shell.
     *
     * Delegates to `close()` which performs the full SIGTERM → poll → SIGKILL
     * shutdown sequence.
     *
     * @note MESSAGE THREAD context.
     */
    ~UnixTTY() override;

    // =========================================================================
    /** @name TTY interface implementation
     * @{ */

    /**
     * @brief Open the PTY pair and spawn the shell process.
     *
     * Calls `openpty()` to create the master/slave pair with the requested
     * initial window size, forks the child, runs `runChildProcess()` in the
     * child, closes the slave fd in the parent, sets the master to
     * `O_NONBLOCK`, and starts the reader thread.
     *
     * @param cols             Initial terminal width in character columns.
     * @param rows             Initial terminal height in character rows.
     * @param shell            Shell program name or absolute path.  Resolved via
     *                         `$PATH` using `execvp()` when not absolute.
     * @param args             Space-separated arguments for the shell (e.g. "-l").
     * @param workingDirectory Optional initial working directory for the shell.
     *                         If empty, the shell inherits the parent's cwd.
     * @return                 `true` on success; `false` if `openpty()` or `fork()` fails.
     *
     * @note MESSAGE THREAD context.
     */
    bool open (int cols, int rows, const juce::String& shell,
               const juce::String& args = {}, const juce::String& workingDirectory = {}) override;

    /**
     * @brief Close the PTY and terminate the shell process.
     *
     * Sequence:
     * 1. Signal reader thread to exit.
     * 2. Close master fd.
     * 3. Wait up to 5 seconds for the reader thread to stop.
     * 4. Non-blocking `waitpid()` — if child is still alive, send SIGTERM.
     * 5. Poll up to `killTimeoutIterations × killPollInterval` µs for exit.
     * 6. If still alive, send SIGKILL and block on `waitpid()`.
     *
     * @note MESSAGE THREAD context.
     */
    void close() override;

    /**
     * @brief Query whether the shell process is still alive.
     *
     * Uses a non-blocking `waitpid (WNOHANG)` to check child status.
     *
     * @return `true` if the child process has not yet exited.
     *
     * @note May be called from any thread.
     */
    bool isRunning() const override;

    /**
     * @brief Read available bytes from the PTY master fd.
     *
     * The master fd is `O_NONBLOCK`.  Returns immediately if no data is ready.
     *
     * @param buf       Destination buffer.
     * @param maxBytes  Maximum bytes to read.
     * @return          Bytes read (> 0), 0 if EAGAIN / EWOULDBLOCK, -1 on EOF or error.
     *
     * @note READER THREAD context.
     */
    int read (char* buf, int maxBytes) override;

    /**
     * @brief Write bytes to the PTY master fd (keyboard input to the shell).
     *
     * @param buf  Data to write.
     * @param len  Number of bytes.
     * @return     `true` if all bytes were written.
     *
     * @note MESSAGE THREAD context.
     */
    bool write (const char* buf, int len) override;

    /**
     * @brief Resize the terminal window via ioctl and SIGWINCH.
     *
     * Issues `ioctl (master, TIOCSWINSZ, &ws)` to update the kernel's idea of
     * the window size, then sends `SIGWINCH` to the child process so the shell
     * and any running TUI application can re-query the size.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     *
     * @note READER THREAD context (dispatched via TTY::run resize handling).
     */
    void resize (int cols, int rows) override;

    /**
     * @brief Block until data is available on the master fd or the timeout expires.
     *
     * Uses `poll()` with `POLLIN` on the master fd.
     *
     * @param timeoutMs  Maximum wait time in milliseconds.
     * @return           `true` if `POLLIN` is set; `false` on timeout or error.
     *
     * @note READER THREAD context.
     */
    bool waitForData (int timeoutMs) override;

    /**
     * @brief Returns the PID of the foreground process group leader via tcgetpgrp.
     * @return The foreground PID, or -1 if unavailable.
     * @note READER THREAD.
     */
    int getForegroundPid() const noexcept override;

    /**
     * @brief Writes the process name for the given PID into the buffer.
     *
     * Uses proc_name() on macOS.
     *
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer.
     * @param maxLength  Buffer size in bytes.
     * @return Bytes written (excluding null), or 0 on failure.
     * @note READER THREAD.
     */
    int getProcessName (int pid, char* buffer, int maxLength) const noexcept override;

    /**
     * @brief Writes the cwd for the given PID into the buffer.
     *
     * Uses proc_pidinfo with PROC_PIDVNODEPATHINFO on macOS.
     *
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer.
     * @param maxLength  Buffer size in bytes.
     * @return Bytes written (excluding null), or 0 on failure.
     * @note READER THREAD.
     */
    int getCwd (int pid, char* buffer, int maxLength) const noexcept override;

    /** @} */

    /**
     * @brief Adds a key-value pair to the shell integration environment.
     *
     * Each pair is injected via `setenv()` in the child process before `execvp()`.
     * Must be called before `open()`.
     *
     * @param key    Environment variable name.
     * @param value  Value to set.
     *
     * @note MESSAGE THREAD.
     */
    void addShellEnv (const juce::String& key, const juce::String& value);

    /**
     * @brief Clears all previously registered shell integration environment pairs.
     *
     * Call before re-populating for a new shell type.  Must be called before `open()`.
     *
     * @note MESSAGE THREAD.
     */
    void clearShellEnv();

private:
    /** @brief Master side of the PTY pair.  -1 when not open. */
    int master { -1 };

    /** @brief PID of the forked shell process.  -1 when not running. */
    pid_t childProcess { -1 };

    /** @brief Shell integration environment variable pairs injected before execvp(). */
    std::vector<std::pair<std::string, std::string>> shellIntegrationEnv;

    /** @brief Number of 10 ms poll iterations before escalating to SIGKILL.
     *
     *  Total grace period = killTimeoutIterations × killPollInterval µs = 500 ms.
     */
    static constexpr int killTimeoutIterations { 50 };

    /** @brief Microseconds to sleep between SIGTERM poll iterations. */
    static constexpr useconds_t killPollInterval { 10000 };
};

#endif
