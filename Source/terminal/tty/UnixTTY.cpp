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
#include <libproc.h>
#include <sys/sysctl.h>
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
 * 2. Change to the specified working directory (if provided).
 * 3. `setsid()` — create a new session so the child becomes a session leader.
 * 4. `ioctl(TIOCSCTTY)` — make the slave the controlling terminal of the session.
 * 5. `dup2` slave → stdin / stdout / stderr.
 * 6. Close the slave fd if it is not already one of the standard fds.
 * 7. Set `TERM=xterm-256color` so the shell and TUI apps use 256-colour sequences.
 * 8. Set `LC_CTYPE=en_US.UTF-8` if not already set, so child shell uses UTF-8 character
 *    classification (works around inherited invalid `LANG=UTF-8` on macOS).
 * 9. `execvp(shell, argv)` — replace the process image with the shell.
 *    Short names (e.g. "zsh") are resolved via `$PATH`.
 *
 * @param master           Master fd to close in the child.
 * @param slaveFd          Slave fd to use as the controlling terminal and stdio.
 * @param shell            Shell program name or absolute path (C string, must outlive fork).
 * @param argv             Null-terminated argument vector (argv[0] = shell).
 * @param workingDirectory Optional working directory to chdir to before exec.
 *
 * @note Runs in the child process only.  Must not return.
 */
static void runChildProcess (int master, int slaveFd, const char* shell,
                             char* const* argv, const char* workingDirectory,
                             const std::vector<std::pair<std::string, std::string>>& shellEnvVars) noexcept
{
    ::close (master);

    if (workingDirectory != nullptr and workingDirectory[0] != '\0')
    {
        chdir (workingDirectory);
    }

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
    setenv ("COLORTERM", "truecolor", 1);

    if (getenv ("LC_CTYPE") == nullptr)
    {
        setenv ("LC_CTYPE", "en_US.UTF-8", 1);
    }

    for (const auto& [key, value] : shellEnvVars)
    {
        setenv (key.c_str(), value.c_str(), 1);
    }

    execvp (shell, argv);
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
 * 3. Convert @p shell and @p workingDirectory to C strings (must happen before `fork()`).
 * 4. `fork()` — child calls `runChildProcess()`; parent continues.
 * 5. Parent closes the slave fd (only the child needs it).
 * 6. `fcntl(O_NONBLOCK)` — make the master fd non-blocking for the reader thread.
 * 7. `startThread()` — begin the TTY reader loop.
 *
 * @param cols             Initial terminal width in character columns.
 * @param rows             Initial terminal height in character rows.
 * @param shell            Shell program name or absolute path.  Resolved via `$PATH`
 *                         using `execvp()` when not absolute.
 * @param args             Space-separated arguments for the shell (e.g. "-l").
 * @param workingDirectory Optional initial working directory for the shell.
 * @return                 `true` on success; `false` if `openpty()` or `fork()` fails.
 *
 * @note MESSAGE THREAD context.
 */
bool UnixTTY::open (int cols, int rows, const juce::String& shell,
                    const juce::String& args, const juce::String& workingDirectory)
{
    int slaveFd { -1 };
    struct winsize ws { static_cast<unsigned short> (rows), static_cast<unsigned short> (cols), 0, 0 };
    const auto shellUtf8 { shell.toStdString() };
    const char* shellCStr { shellUtf8.c_str() };
    const auto cwdUtf8 { workingDirectory.toStdString() };
    const char* cwdCStr { cwdUtf8.c_str() };

    // Build argv: [shell, ...tokens, nullptr]
    const auto tokens { juce::StringArray::fromTokens (args, true) };
    std::vector<std::string> argStrings;
    argStrings.reserve (static_cast<size_t> (tokens.size()));

    for (const auto& t : tokens)
    {
        argStrings.push_back (t.toStdString());
    }

    std::vector<char*> argv;
    argv.reserve (argStrings.size() + 2);
    argv.push_back (const_cast<char*> (shellCStr));

    for (auto& s : argStrings)
    {
        argv.push_back (s.data());
    }

    argv.push_back (nullptr);

    bool result { false };

    if (openpty (&master, &slaveFd, nullptr, nullptr, &ws) != -1)
    {
        childProcess = fork();

        if (childProcess == 0)
        {
            runChildProcess (master, slaveFd, shellCStr, argv.data(), cwdCStr, shellIntegrationEnv);
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
 * Retries on partial writes and EAGAIN/EWOULDBLOCK until all bytes are
 * delivered or an unrecoverable error occurs.  This prevents bracketed
 * paste sequences from being truncated when the PTY kernel buffer is full.
 *
 * @param buf  Data to write.
 * @param len  Number of bytes.
 * @return     `true` if all bytes were written successfully.
 *
 * @note MESSAGE THREAD context.
 */
bool UnixTTY::write (const char* buf, int len)
{
    int written { 0 };
    bool success { true };

    while (success and written < len)
    {
        const auto result { ::write (master, buf + written, len - written) };

        if (result > 0)
        {
            written += static_cast<int> (result);
        }
        else if (result == -1 and (errno == EAGAIN or errno == EWOULDBLOCK))
        {
            usleep (100);
        }
        else
        {
            success = false;
        }
    }

    return success;
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
void UnixTTY::platformResize (int cols, int rows)
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

int UnixTTY::getForegroundPid() const noexcept
{
    if (master >= 0)
    {
        const pid_t fg { tcgetpgrp (master) };

        if (fg > 0)
        {
            return static_cast<int> (fg);
        }
    }

    return -1;
}

int UnixTTY::getProcessName (int pid, char* buffer, int maxLength) const noexcept
{
    if (pid > 0 and buffer != nullptr and maxLength > 0)
    {
#if JUCE_MAC
        char name[PROC_PIDPATHINFO_MAXSIZE] {};
        const int result { proc_name (pid, name, sizeof (name)) };

        if (result > 0)
        {
            const int length { juce::jmin (result, maxLength - 1) };
            std::memcpy (buffer, name, static_cast<size_t> (length));
            buffer[length] = '\0';
            return length;
        }
#elif JUCE_LINUX
        char path[64] {};
        std::snprintf (path, sizeof (path), "/proc/%d/comm", pid);
        const int fd { ::open (path, O_RDONLY) };

        if (fd >= 0)
        {
            const auto bytesRead { ::read (fd, buffer, static_cast<size_t> (maxLength - 1)) };
            ::close (fd);

            if (bytesRead > 0)
            {
                int length { static_cast<int> (bytesRead) };

                if (length > 0 and buffer[length - 1] == '\n')
                {
                    --length;
                }

                buffer[length] = '\0';
                return length;
            }
        }
#endif
    }

    return 0;
}

int UnixTTY::getCwd (int pid, char* buffer, int maxLength) const noexcept
{
    if (pid > 0 and buffer != nullptr and maxLength > 0)
    {
#if JUCE_MAC
        struct proc_vnodepathinfo pathInfo {};
        const int result { proc_pidinfo (pid, PROC_PIDVNODEPATHINFO, 0, &pathInfo, sizeof (pathInfo)) };

        if (result > 0)
        {
            const int length { juce::jmin (static_cast<int> (std::strlen (pathInfo.pvi_cdir.vip_path)), maxLength - 1) };
            std::memcpy (buffer, pathInfo.pvi_cdir.vip_path, static_cast<size_t> (length));
            buffer[length] = '\0';
            return length;
        }
#elif JUCE_LINUX
        char path[64] {};
        std::snprintf (path, sizeof (path), "/proc/%d/cwd", pid);
        const auto bytesRead { ::readlink (path, buffer, static_cast<size_t> (maxLength - 1)) };

        if (bytesRead > 0)
        {
            const int length { static_cast<int> (bytesRead) };
            buffer[length] = '\0';
            return length;
        }
#endif
    }

    return 0;
}

int UnixTTY::getEnvVar (int pid, const char* varName, char* buffer, int maxLength) const
{
    if (pid > 0 and varName != nullptr and buffer != nullptr and maxLength > 0)
    {
#if JUCE_MAC
        int mib[3] { CTL_KERN, KERN_PROCARGS2, pid };
        size_t argmax { 0 };

        if (sysctl (mib, 3, nullptr, &argmax, nullptr, 0) == 0 and argmax > 0)
        {
            std::vector<char> procargs (argmax);

            if (sysctl (mib, 3, procargs.data(), &argmax, nullptr, 0) == 0)
            {
                const size_t varNameLen { std::strlen (varName) };
                const char* p { procargs.data() };
                const char* end { p + argmax };

                // Skip argc (first sizeof(int) bytes)
                if (argmax > sizeof (int))
                {
                    int argc { 0 };
                    std::memcpy (&argc, p, sizeof (int));
                    p += sizeof (int);

                    // Skip exec path (null-terminated)
                    while (p < end and *p != '\0')
                        ++p;

                    // Skip padding nulls after exec path
                    while (p < end and *p == '\0')
                        ++p;

                    // Skip argc argument strings
                    for (int i { 0 }; i < argc and p < end; ++i)
                    {
                        while (p < end and *p != '\0')
                            ++p;

                        if (p < end)
                            ++p;
                    }

                    // Now at environment variables — null-separated KEY=VALUE pairs
                    while (p < end and *p != '\0')
                    {
                        const char* entry { p };

                        while (p < end and *p != '\0')
                            ++p;

                        const size_t entryLen { static_cast<size_t> (p - entry) };

                        if (entryLen > varNameLen and entry[varNameLen] == '='
                            and std::strncmp (entry, varName, varNameLen) == 0)
                        {
                            const char* value { entry + varNameLen + 1 };
                            const int valueLen { juce::jmin (static_cast<int> (entryLen - varNameLen - 1), maxLength - 1) };
                            std::memcpy (buffer, value, static_cast<size_t> (valueLen));
                            buffer[valueLen] = '\0';
                            return valueLen;
                        }

                        if (p < end)
                            ++p;
                    }
                }
            }
        }
#elif JUCE_LINUX
        char path[64] {};
        std::snprintf (path, sizeof (path), "/proc/%d/environ", pid);
        const int fd { ::open (path, O_RDONLY) };

        if (fd >= 0)
        {
            constexpr int envBufSize { 65536 };
            std::vector<char> envBuf (envBufSize);
            const auto bytesRead { ::read (fd, envBuf.data(), envBufSize) };
            ::close (fd);

            if (bytesRead > 0)
            {
                const size_t varNameLen { std::strlen (varName) };
                const char* p { envBuf.data() };
                const char* end { p + bytesRead };

                while (p < end)
                {
                    const char* entry { p };

                    while (p < end and *p != '\0')
                        ++p;

                    const size_t entryLen { static_cast<size_t> (p - entry) };

                    if (entryLen > varNameLen and entry[varNameLen] == '='
                        and std::strncmp (entry, varName, varNameLen) == 0)
                    {
                        const char* value { entry + varNameLen + 1 };
                        const int valueLen { juce::jmin (static_cast<int> (entryLen - varNameLen - 1), maxLength - 1) };
                        std::memcpy (buffer, value, static_cast<size_t> (valueLen));
                        buffer[valueLen] = '\0';
                        return valueLen;
                    }

                    if (p < end)
                        ++p;
                }
            }
        }
#endif
    }

    return 0;
}

#endif
