/**
 * @file TTY.h
 * @brief Abstract base class for platform pseudo-terminal (PTY) implementations.
 *
 * TTY is the platform-agnostic interface for managing a pseudo-terminal connection
 * to a shell process.  It owns the reader thread (inheriting from juce::Thread) and
 * defines the contract that platform implementations must fulfil.
 *
 * ### Architecture
 *
 * ```
 *  Message thread          Reader thread (TTY::run)
 *  ─────────────           ──────────────────────────────────────────────
 *  open()                  waitForData() — blocks up to 100 ms
 *  write()                 read()        — drains all available bytes
 *  resize()                onData()  — fires callback per chunk
 *  close()                 onDrainComplete() — fires after full drain
 *                          onExit()      — fires via MessageManager on EOF
 * ```
 *
 * ### Callback model
 * All four callbacks are plain `std::function` fields set by the owner before
 * calling `open()`.  The reader thread invokes `onData` and `onDrainComplete`
 * directly on the reader thread.  `onExit` is dispatched to the message thread
 * via `juce::MessageManager::callAsync`.
 *
 * ### Resize protocol
 * The message thread calls `grid.resize()` and `parser.resize()` directly,
 * then calls `platformResize()` to notify the shell via SIGWINCH.
 *
 * @see UnixTTY  macOS / Linux implementation via forkpty()
 * @see WindowsTTY  Windows implementation via ConPTY
 */

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @class TTY
 * @brief Abstract pseudo-terminal base: reader thread + platform PTY interface.
 *
 * Subclasses implement the six pure-virtual methods that interact with the
 * underlying OS PTY API.  TTY::run() provides the shared reader-thread loop
 * that drives data delivery, resize handling, and shell-exit detection.
 *
 * @par Ownership
 * The owner (typically Session) creates a concrete subclass, sets the four
 * callbacks, then calls `open()`.  The reader thread starts inside `open()`.
 * The owner must call `close()` (or destroy the object) to stop the thread
 * and reap the child process.
 *
 * @par Thread safety
 * - `open()`, `write()`, `resize()`, `close()` — message thread only
 * - `hasShellExited()` — any thread (atomic load)
 * - `run()` — reader thread only (called by juce::Thread)
 */
class TTY : public juce::Thread
{
public:
    TTY()
        : juce::Thread ("TTY Reader")
    {
    }

    ~TTY() override = default;

    // =========================================================================
    /** @name Platform interface (pure virtual)
     *  Implemented by UnixTTY and WindowsTTY.
     * @{ */

    /**
     * @brief Open the pseudo-terminal and spawn the shell process.
     *
     * Creates the PTY pair, forks (Unix) or spawns (Windows) the shell, sets
     * the master fd to non-blocking, and starts the reader thread.
     *
     * @param cols             Initial terminal width in character columns.
     * @param rows             Initial terminal height in character rows.
     * @param shell            Shell program name or absolute path (e.g. "zsh",
     *                         "/opt/homebrew/bin/fish").  Resolved via `$PATH` when
     *                         not an absolute path.
     * @param args             Space-separated arguments to pass to the shell
     *                         (e.g. "-l", "--login -i").  Tokenized at the call site.
     * @param workingDirectory Optional initial working directory for the shell.
     *                         If empty, the shell inherits the parent's cwd.
     * @return                 `true` on success; `false` if the PTY or process could not
     *                         be created.
     *
     * @note Called from the message thread.  Must not be called while the
     *       reader thread is already running.
     */
    virtual bool open (int cols, int rows, const juce::String& shell,
                       const juce::String& args = {}, const juce::String& workingDirectory = {}) = 0;

    /**
     * @brief Close the PTY and terminate the shell process.
     *
     * Signals the reader thread to exit, closes the master fd, waits for the
     * thread to stop, then terminates the child process if still alive.
     *
     * @note Called from the message thread.  Blocks until the reader thread
     *       has stopped (up to 5 seconds).
     */
    virtual void close() = 0;

    /**
     * @brief Query whether the shell process is still alive.
     *
     * @return `true` if the child process has not yet exited.
     *
     * @note May be called from any thread.  Uses a non-blocking waitpid /
     *       GetExitCodeProcess check — no blocking.
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Read available bytes from the PTY master into a caller-supplied buffer.
     *
     * The master fd is non-blocking.  Returns immediately if no data is ready.
     *
     * @param buf       Destination buffer.  Must be at least @p maxBytes bytes.
     * @param maxBytes  Maximum number of bytes to read.
     * @return          Number of bytes read (> 0), 0 if no data is currently
     *                  available (EAGAIN / EWOULDBLOCK), or -1 on EOF / error.
     *
     * @note Called from the reader thread only.
     */
    virtual int read (char* buf, int maxBytes) = 0;

    /**
     * @brief Write bytes to the PTY master (i.e. send input to the shell).
     *
     * @param buf  Data to write.
     * @param len  Number of bytes to write.
     * @return     `true` if all @p len bytes were written successfully.
     *
     * @note Called from the message thread.
     */
    virtual bool write (const char* buf, int len) = 0;

    /**
     * @brief Block until data is available on the PTY master or the timeout expires.
     *
     * Used by the reader thread to avoid a busy-wait spin.  Implemented with
     * poll() (Unix) or WaitForSingleObject (Windows).
     *
     * @param timeoutMs  Maximum time to wait in milliseconds.
     * @return           `true` if data is available; `false` on timeout or error.
     *
     * @note Called from the reader thread only.
     */
    virtual bool waitForData (int timeoutMs) = 0;

    /** @} */

    // =========================================================================
    /** @name Shell exit query
     * @{ */

    /**
     * @brief Query whether the shell has exited.
     *
     * Set to `true` by the reader thread when `read()` returns -1 (EOF).
     * The `onExit` callback is also dispatched to the message thread at that
     * point.
     *
     * @return `true` if the shell process has exited.
     *
     * @note Safe to call from any thread.
     */
    bool hasShellExited() const noexcept
    {
        return shellExited.load (std::memory_order_acquire);
    }

    /** @} */

    // =========================================================================
    /** @name Shell integration environment
     *  Env vars injected into the child shell process to enable OSC sequences.
     *  Must be called before open().
     * @{ */

    /**
     * @brief Adds a key-value pair to the shell integration environment.
     *
     * Each pair is injected into the child shell process before it starts.
     * Must be called before `open()`.
     *
     * @param key    Environment variable name.
     * @param value  Value to set.
     *
     * @note MESSAGE THREAD.
     */
    void addShellEnv (const juce::String& key, const juce::String& value)
    {
        shellIntegrationEnv.emplace_back (key.toStdString(), value.toStdString());
    }

    /**
     * @brief Clears all previously registered shell integration environment pairs.
     *
     * Call before re-populating for a new shell type.  Must be called before `open()`.
     *
     * @note MESSAGE THREAD.
     */
    void clearShellEnv()
    {
        shellIntegrationEnv.clear();
    }

    /** @} */

    // =========================================================================
    /** @name Process introspection
     *  Query the foreground process running in the terminal.
     *  Default implementations return empty/invalid values.
     * @{ */

    /**
     * @brief Returns the PID of the foreground process group leader.
     * @return The foreground PID, or -1 if unavailable.
     * @note READER THREAD — called from Session::process().
     */
    virtual int getForegroundPid() const noexcept { return -1; }

    /**
     * @brief Writes the process name for the given PID into the buffer.
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer for the null-terminated name.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return Number of bytes written (excluding null terminator), or 0 on failure.
     * @note READER THREAD — no allocation, writes directly into caller's buffer.
     */
    virtual int getProcessName (int pid, char* buffer, int maxLength) const noexcept { return 0; }

    /**
     * @brief Writes the current working directory for the given PID into the buffer.
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer for the null-terminated path.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return Number of bytes written (excluding null terminator), or 0 on failure.
     * @note READER THREAD — no allocation, writes directly into caller's buffer.
     */
    virtual int getCwd (int pid, char* buffer, int maxLength) const noexcept { return 0; }

    /**
     * @brief Reads an environment variable from the given PID's environment.
     * @param pid        The process ID to query.
     * @param varName    The environment variable name (e.g. "PATH").
     * @param buffer     Destination buffer for the null-terminated value.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return Number of bytes written (excluding null terminator), or 0 on failure.
     * @note READER THREAD — no allocation, writes directly into caller's buffer.
     */
    virtual int getEnvVar (int pid, const char* varName, char* buffer, int maxLength) const { return 0; }

    /** @} */

    // =========================================================================
    /** @name Reader thread entry point
     * @{ */

    /**
     * @brief Reader thread main loop — drives data delivery and shell-exit detection.
     *
     * Runs at high priority.  Each iteration:
     * 1. Calls `waitForData (100)` to block up to 100 ms.
     * 2. If data is available, drains all bytes in a tight inner loop,
     *    calling `onData` for each chunk.
     * 3. After the drain, calls `onDrainComplete` once.
     * 4. On EOF (`read()` returns -1), sets `shellExited`, dispatches `onExit`
     *    to the message thread, and returns.
     *
     * @note Called by juce::Thread infrastructure — do not call directly.
     *       READER THREAD context.
     */
    void run() override;

    /** @} */

    // =========================================================================
    /** @name Callbacks
     *  Set by the owner before calling open().  All are optional (null-checked
     *  before invocation).
     * @{ */

    /** @brief Called on the reader thread with each chunk of PTY output data.
     *
     *  @param buf  Pointer to the received bytes (valid only for the duration
     *              of the call).
     *  @param len  Number of bytes in @p buf.
     */
    std::function<void (const char*, int)> onData;

    /** @brief Called on the reader thread after a full drain of available data.
     *
     *  Allows the owner to flush any queued writes that were deferred until
     *  the child process had finished its initial output (e.g. waiting for
     *  the shell to set raw / no-echo mode).
     */
    std::function<void()> onDrainComplete;

    /** @brief Called on the message thread when the shell process exits.
     *
     *  Dispatched via `juce::MessageManager::callAsync` from the reader thread.
     */
    std::function<void()> onExit;

    /** @} */

    // =========================================================================
    /** @name Constants
     * @{ */

    /** @brief Read buffer size in bytes.
     *
     *  64 KiB matches the typical Linux PTY kernel buffer and allows the
     *  reader to drain a full burst in a single syscall.
     */
    inline static constexpr size_t READ_CHUNK_SIZE { 65536 };

    /** @} */

    /**
     * @brief Performs the OS-level PTY resize.
     *
     * Calls `ioctl TIOCSWINSZ` + `SIGWINCH` on Unix, or
     * `ResizePseudoConsole()` on Windows.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     * @note MESSAGE THREAD.
     */
    virtual void platformResize (int cols, int rows) = 0;

protected:
    /** @brief Shell integration environment variable pairs injected before shell start. */
    std::vector<std::pair<std::string, std::string>> shellIntegrationEnv;

    /** @brief Set to `true` by the reader thread on EOF; read by any thread via hasShellExited(). */
    std::atomic<bool> shellExited { false };
};
