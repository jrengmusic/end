/**
 * @file Session.cpp
 * @brief Implementation of Terminal::Session — PTY-side terminal session.
 *
 * @see Session.h
 */

#include "Session.h"
#include "../tty/TTY.h"
#include "../../config/Config.h"
#include "../../nexus/Log.h"

#if JUCE_MAC || JUCE_LINUX
#include "../tty/UnixTTY.h"
#elif JUCE_WINDOWS
#include "../tty/WindowsTTY.h"
#endif

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Constructs the Session, creates the TTY, and opens the shell.
 *
 * History capacity comes from `Config::Key::terminalScrollbackLines`.
 * The `onBytes`, `onDrainComplete`, and `onExit` callbacks are wired by the
 * owner (Nexus::Session) after construction.
 *
 * @note MESSAGE THREAD.
 */
Session::Session (int cols, int rows,
                  const juce::String& shell,
                  const juce::String& args,
                  const juce::String& cwd)
    : history { Config::getContext()->getInt (Config::Key::terminalScrollbackLines) }
{
#if JUCE_MAC || JUCE_LINUX
    tty = std::make_unique<UnixTTY>();
#elif JUCE_WINDOWS
    tty = std::make_unique<WindowsTTY>();
#endif

    tty->onData = [this] (const char* bytes, int len)
    {
        history.append (bytes, static_cast<size_t> (len));

        if (onBytes != nullptr)
            onBytes (bytes, len);
    };

    tty->onDrainComplete = [this]
    {
        if (onDrainComplete != nullptr)
            onDrainComplete();
    };

    tty->onExit = [this]
    {
        if (onExit != nullptr)
            onExit();
    };

    tty->open (cols, rows, shell, args, cwd);

    Nexus::logLine ("Terminal::Session ctor: shell=" + shell + " cols=" + juce::String (cols)
                    + " rows=" + juce::String (rows));
}

/**
 * @brief Stops the PTY and releases all resources.
 * @note MESSAGE THREAD.
 */
Session::~Session()
{
    stop();
}

/**
 * @brief Writes raw input bytes to the PTY.
 *
 * @note MESSAGE THREAD.
 */
void Session::sendInput (const char* data, int len)
{
    jassert (tty != nullptr);
    jassert (data != nullptr);
    jassert (len > 0);

    tty->write (data, len);
}

/**
 * @brief Notifies the shell of a terminal resize via SIGWINCH.
 *
 * @note MESSAGE THREAD.
 */
void Session::resize (int cols, int rows)
{
    jassert (tty != nullptr);

    if (tty->isThreadRunning())
        tty->platformResize (cols, rows);
}

/**
 * @brief Performs the OS-level PTY resize.
 *
 * Called by the pipeline during sync-resize (from onDrainComplete on READER THREAD).
 *
 * @note READER THREAD.
 */
void Session::platformResize (int cols, int rows)
{
    jassert (tty != nullptr);
    tty->platformResize (cols, rows);
}

/**
 * @brief Returns the PID of the foreground process running in the PTY.
 *
 * @note READER THREAD.
 */
int Session::getForegroundPid() const noexcept
{
    jassert (tty != nullptr);
    return tty->getForegroundPid();
}

/**
 * @brief Writes the process name for the given PID into the buffer.
 *
 * @note READER THREAD.
 */
int Session::getProcessName (int pid, char* buffer, int maxLength) const noexcept
{
    jassert (tty != nullptr);
    return tty->getProcessName (pid, buffer, maxLength);
}

/**
 * @brief Writes the current working directory for the given PID into the buffer.
 *
 * @note READER THREAD.
 */
int Session::getCwd (int pid, char* buffer, int maxLength) const noexcept
{
    jassert (tty != nullptr);
    return tty->getCwd (pid, buffer, maxLength);
}

/**
 * @brief Returns a snapshot of all buffered history bytes.
 *
 * @note MESSAGE THREAD.
 */
juce::MemoryBlock Session::snapshotHistory() const
{
    return history.snapshot();
}

/**
 * @brief Closes the PTY and stops the reader thread.  Idempotent.
 *
 * @note MESSAGE THREAD.
 */
void Session::stop()
{
    if (tty != nullptr)
    {
        tty->onExit          = nullptr;
        tty->onData          = nullptr;
        tty->onDrainComplete = nullptr;
        tty->close();
        tty = nullptr;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
