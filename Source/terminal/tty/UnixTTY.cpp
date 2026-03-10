/**
 * @file UnixTTY.cpp
 * @brief macOS / Linux PTY implementation: openpty, fork, read/write, resize, shutdown.
 *
 * All PTY interaction goes through the master file descriptor.  The master is
 * set to `O_NONBLOCK` after fork so the reader thread can drain it without
 * blocking.  `poll()` is used to wait for data with a timeout, keeping the
 * reader thread responsive to resize requests and thread-exit signals.
 *
 * ### Platform headers
 * - macOS: `<util.h>` provides `openpty()`
 * - Linux:  `<pty.h>` provides `openpty()`
 *
 * @see UnixTTY.h  Class declaration and member documentation
 * @see TTY.cpp    Shared reader thread loop
 */

#include "UnixTTY.h"

#if JUCE_MAC || JUCE_LINUX
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <cerrno>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

// =============================================================================
/**
 * @brief Child-process setup: connect stdio to the PTY slave and exec the shell.
 *
 * Called in the child after `fork()`.  Never returns on success; calls `_exit(1)`
 * on `execlp` failure.
 *
 * @par Steps
 * 1. Close the master fd (child does not need it).
 * 2. `setsid()` — create a new session so the child becomes a session leader.
 * 3. `ioctl(TIOCSCTTY)` — make the slave the controlling terminal of the session.
 * 4. `dup2` slave → stdin / stdout / stderr.
 * 5. Close the slave fd if it is not already one of the standard fds.
 * 6. Set `TERM=xterm-256color` so the shell and TUI apps use 256-colour sequences.
 * 7. Set `LANG=UTF-8` if not already set, to enable Unicode output.
 * 8. `execlp(shell, shell, "-l")` — replace the process image with a login shell.
 *    Short names (e.g. "zsh") are resolved via `$PATH`.
 *
 * @param master   Master fd to close in the child.
 * @param slaveFd  Slave fd to use as the controlling terminal and stdio.
 * @param shell    Shell program name or absolute path (C string, must outlive fork).
 *
 * @note Runs in the child process only.  Must not return.
 */
static void runChildProcess (int master, int slaveFd, const char* shell) noexcept
{
    ::close (master);
    setsid();
    ioctl (slaveFd, TIOCSCTTY, 0);

    dup2 (slaveFd, STDIN_FILENO);
    dup2 (slaveFd, STDOUT_FILENO);
    dup2 (slaveFd, STDERR_FILENO);

    if (slaveFd > 2)
    {
        ::close (slaveFd);
    }

    setenv ("TERM", "xterm-256color", 1);

    if (getenv ("LANG") == nullptr)
    {
        setenv ("LANG", "UTF-8", 1);
    }

    execlp (shell, shell, "-l", nullptr);
    _exit (1);
}

// =============================================================================

/**
 * @brief Destructor — delegates to close() for full PTY and process cleanup.
 *
 * @note MESSAGE THREAD context.
 */
UnixTTY::~UnixTTY()
{
    close();
}

/**
 * @brief Open the PTY pair and spawn the shell process.
 *
 * @par Sequence
 * 1. Build a `winsize` struct from @p cols / @p rows.
 * 2. `openpty()` — allocate master and slave fds with the initial window size.
 * 3. Convert @p shell to a C string (must happen before `fork()`).
 * 4. `fork()` — child calls `runChildProcess()`; parent continues.
 * 5. Parent closes the slave fd (only the child needs it).
 * 6. `fcntl(O_NONBLOCK)` — make the master fd non-blocking for the reader thread.
 * 7. `startThread()` — begin the TTY reader loop.
 *
 * @param cols   Initial terminal width in character columns.
 * @param rows   Initial terminal height in character rows.
 * @param shell  Shell program name or absolute path.  Resolved via `$PATH`
 *               using `execlp()` when not absolute.
 * @return       `true` on success; `false` if `openpty()` or `fork()` fails.
 *
 * @note MESSAGE THREAD context.
 */
bool UnixTTY::open (int cols, int rows, const juce::String& shell)
{
    int slaveFd;
    struct winsize ws { static_cast<unsigned short> (rows), static_cast<unsigned short> (cols), 0, 0 };
    const auto shellUtf8 { shell.toStdString() };
    const char* shellCStr { shellUtf8.c_str() };

    bool result { false };

    if (openpty (&master, &slaveFd, nullptr, nullptr, &ws) != -1)
    {
        childProcess = fork();

        if (childProcess == 0)
        {
            runChildProcess (master, slaveFd, shellCStr);
        }

        ::close (slaveFd);

        if (childProcess >= 0)
        {
            const int flags { fcntl (master, F_GETFL, 0) };

            if (flags != -1)
            {
                fcntl (master, F_SETFL, flags | O_NONBLOCK);
            }

            startThread();

            result = true;
        }
    }

    return result;
}

/**
 * @brief Close the PTY and terminate the shell process.
 *
 * @par Shutdown sequence
 * 1. `signalThreadShouldExit()` — ask the reader thread to stop.
 * 2. Close master fd — causes the child's next read to return EOF.
 * 3. `stopThread (5000)` — wait up to 5 s for the reader thread to exit.
 * 4. Non-blocking `waitpid (WNOHANG)` — check if child already exited.
 * 5. If still alive: send `SIGTERM`, then poll every `killPollInterval` µs
 *    for up to `killTimeoutIterations` iterations (500 ms total).
 * 6. If still alive after SIGTERM grace period: send `SIGKILL` and block on
 *    `waitpid()` to reap the zombie.
 *
 * @note MESSAGE THREAD context.
 */
void UnixTTY::close()
{
    signalThreadShouldExit();

    if (master >= 0)
    {
        ::close (master);
        master = -1;
    }

    stopThread (5000);

    if (childProcess > 0)
    {
        int status { 0 };
        pid_t result { waitpid (childProcess, &status, WNOHANG) };

        if (result == 0)
        {
            kill (childProcess, SIGTERM);

            for (int i { 0 }; i < killTimeoutIterations; ++i)
            {
                usleep (killPollInterval);
                result = waitpid (childProcess, &status, WNOHANG);
                if (result != 0)
                {
                    break;
                }
            }

            if (result == 0)
            {
                kill (childProcess, SIGKILL);
                waitpid (childProcess, nullptr, 0);
            }
        }

        childProcess = -1;
    }
}

/**
 * @brief Query whether the shell process is still alive.
 *
 * Uses a non-blocking `waitpid (WNOHANG)`.  Returns 0 if the child is still
 * running, which is mapped to `true`.
 *
 * @return `true` if the child process has not yet exited.
 *
 * @note May be called from any thread.
 */
bool UnixTTY::isRunning() const
{
    bool running { false };

    if (childProcess > 0)
    {
        running = waitpid (childProcess, nullptr, WNOHANG) == 0;
    }

    return running;
}

/**
 * @brief Read available bytes from the PTY master fd.
 *
 * The master fd is `O_NONBLOCK`.  Maps POSIX `read()` return values to the
 * TTY contract:
 * - `n > 0` → return n (bytes read)
 * - `n == 0` → return -1 (EOF — PTY closed)
 * - `errno == EAGAIN / EWOULDBLOCK` → return 0 (no data yet)
 * - other error → return -1 (treat as EOF / fatal)
 *
 * @param buf       Destination buffer.
 * @param maxBytes  Maximum bytes to read.
 * @return          Bytes read (> 0), 0 if no data available, -1 on EOF or error.
 *
 * @note READER THREAD context.
 */
int UnixTTY::read (char* buf, int maxBytes)
{
    const ssize_t n { ::read (master, buf, maxBytes) };
    int result { -1 };

    if (n > 0)
    {
        result = static_cast<int> (n);
    }
    else if (n == 0)
    {
        result = -1;
    }
    else
    {
        result = (errno == EAGAIN or errno == EWOULDBLOCK) ? 0 : -1;
    }

    return result;
}

/**
 * @brief Write bytes to the PTY master fd (keyboard input to the shell).
 *
 * A single `write()` syscall.  Returns `true` only if all @p len bytes were
 * accepted by the kernel.
 *
 * @param buf  Data to write.
 * @param len  Number of bytes.
 * @return     `true` if all bytes were written successfully.
 *
 * @note MESSAGE THREAD context.
 */
bool UnixTTY::write (const char* buf, int len)
{
    return ::write (master, buf, len) == len;
}

/**
 * @brief Resize the terminal window via ioctl and SIGWINCH.
 *
 * Updates the kernel's `winsize` record for the PTY via `TIOCSWINSZ`, then
 * sends `SIGWINCH` to the child process.  The shell (and any foreground TUI
 * application) handles `SIGWINCH` by re-querying the terminal size and
 * redrawing.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 *
 * @note READER THREAD context (dispatched via TTY::run resize handling).
 */
void UnixTTY::resize (int cols, int rows)
{
    struct winsize ws { static_cast<unsigned short> (rows), static_cast<unsigned short> (cols), 0, 0 };
    ioctl (master, TIOCSWINSZ, &ws);
    
    if (childProcess > 0)
    {
        kill (childProcess, SIGWINCH);
    }
}

/**
 * @brief Block until data is available on the master fd or the timeout expires.
 *
 * Uses `poll()` with `POLLIN` on the master fd.  A 100 ms timeout (the value
 * passed by TTY::run) keeps the reader thread responsive to resize requests
 * and thread-exit signals without burning CPU.
 *
 * @param timeoutMs  Maximum wait time in milliseconds.
 * @return           `true` if `POLLIN` is set on the master fd; `false` on
 *                   timeout, error, or if the master fd is not open.
 *
 * @note READER THREAD context.
 */
bool UnixTTY::waitForData (int timeoutMs)
{
    bool result { false };

    if (master >= 0)
    {
        struct pollfd pfd;
        pfd.fd = master;
        pfd.events = POLLIN;
        pfd.revents = 0;

        const int pollResult { ::poll (&pfd, 1, timeoutMs) };
        result = pollResult > 0 and (pfd.revents & POLLIN) != 0;
    }

    return result;
}

#endif
