/**
 * @file Session.h
 * @brief PTY-side terminal session: owns TTY, byte history, and Processor.
 *
 * `Terminal::Session` is the data-source half of a terminal connection.  It owns:
 * - The PTY (`TTY`) that communicates with the shell process.
 * - A `Terminal::History` ring buffer that records every byte the shell emits.
 * - The `Terminal::Processor` pipeline (Parser → Grid → Display).
 *
 * ### Data flow
 * ```
 * PTY → History::append → onBytes (IPC broadcast in daemon mode)
 *                       → Processor::processWithLock (local + daemon)
 * ```
 *
 * ### Thread ownership
 * - `onBytes` fires on the READER THREAD (called from tty::onData).
 * - All other public methods are MESSAGE THREAD only.
 *
 * ### Naming disambiguation
 * This class is `Terminal::Session`.  `Nexus::Session` is the cross-mode
 * session pool that owns one or more `Terminal::Session` objects.
 * The two classes live in different namespaces; all source files that need
 * both must qualify fully.
 *
 * @see Terminal::History
 * @see Terminal::Processor
 * @see Nexus::Session
 */

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

#include "../data/History.h"
#include "Processor.h"

class TTY;

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Terminal::Session
 * @brief PTY-side terminal session — owns TTY, byte history, and Processor.
 *
 * Constructed by `Nexus::Session` (or its mode-specific delegates).  Caller
 * sets `onBytes` before calling any method that starts the reader thread.
 *
 * @par Thread context
 * - `onBytes` callback — READER THREAD.
 * - All other public methods — MESSAGE THREAD.
 */
class Session
{
public:
    /**
     * @brief Constructs the Session, creates the TTY, and opens the shell.
     *
     * History capacity is read from `Config::Key::terminalScrollbackLines`.
     *
     * @param cols     Initial terminal width in character columns.
     * @param rows     Initial terminal height in character rows.
     * @param shell    Shell program path (e.g. "zsh", "/usr/bin/fish").
     * @param args     Shell arguments string.  Empty = none.
     * @param cwd      Initial working directory.  Empty = inherit.
     * @param seedEnv  Extra environment variables injected before shell open.
     *                 Iterated and pushed via TTY::addShellEnv before tty->open().
     *                 Default empty — preserves all existing callers unchanged.
     */
    Session (int cols, int rows,
             const juce::String& shell,
             const juce::String& args,
             const juce::String& cwd,
             const juce::StringPairArray& seedEnv = {},
             const juce::String& uuid = {});

    /**
     * @brief Stops the PTY and releases all resources.
     * @note MESSAGE THREAD.
     */
    ~Session();

    /**
     * @brief Called on the READER THREAD for every chunk of PTY output.
     *
     * `Nexus::Session` sets this before construction completes:
     * - Non-nexus: wired to `Processor::process`.
     * - Daemon: wired to broadcast-to-subscribers (byte forwarding via IPC).
     */
    std::function<void (const char*, int)> onBytes;

    /**
     * @brief Called on the READER THREAD after a full drain of PTY output.
     *
     * Allows the pipeline owner to flush parser responses and handle
     * sync-resize.  Set by `Nexus::Session` in local mode.
     */
    std::function<void()> onDrainComplete;

    /**
     * @brief Called on the MESSAGE THREAD when the shell process exits.
     *
     * Set by `Nexus::Session` to handle lifecycle cleanup.
     */
    std::function<void()> onExit;

    /**
     * @brief Called on the READER THREAD after each state flush (cwd and foreground process updated in State).
     *
     * Fires at the end of the internal `state.onFlush` lambda, after cwd and
     * foreground process have been written into State.  `Nexus::Session` sets
     * this in daemon mode to broadcast a `Message::stateUpdate` PDU.
     *
     * @note READER THREAD.
     */
    std::function<void()> onStateFlush;

    /**
     * @brief Returns the PID of the foreground process running in the PTY.
     *
     * Delegates to TTY::getForegroundPid().
     *
     * @return The foreground PID, or -1 if unavailable.
     * @note READER THREAD — called from state.onFlush.
     */
    int getForegroundPid() const noexcept;

    /**
     * @brief Writes the process name for the given PID into the buffer.
     *
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer for the null-terminated name.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return Number of bytes written (excluding null terminator), or 0 on failure.
     */
    int getProcessName (int pid, char* buffer, int maxLength) const noexcept;

    /**
     * @brief Writes the current working directory for the given PID into the buffer.
     *
     * @param pid        The process ID to query.
     * @param buffer     Destination buffer for the null-terminated path.
     * @param maxLength  Size of the destination buffer in bytes.
     * @return Number of bytes written (excluding null terminator), or 0 on failure.
     */
    int getCwd (int pid, char* buffer, int maxLength) const noexcept;

  #if ! JUCE_WINDOWS
    /**
     * @brief Reads an environment variable from the given PID's live environment.
     *
     * Pass-through to TTY::getEnvVar.  Uses a stack buffer internally.
     *
     * @param pid   The process ID to query.
     * @param name  The variable name (e.g. "PATH").
     * @return The variable's value as a juce::String, or empty on failure.
     * @note MESSAGE THREAD.
     */
    juce::String getEnvVar (int pid, const juce::String& name) const;
  #endif

    /**
     * @brief Performs the OS-level PTY resize (SIGWINCH to shell).
     *
     * Called by the pipeline when a sync-resize is needed.
     *
     * @param cols  New column count.
     * @param rows  New row count.
     * @note READER THREAD (called from onDrainComplete during sync-resize).
     */
    void platformResize (int cols, int rows);

    /**
     * @brief Writes raw input bytes to the PTY (keyboard/mouse from client).
     *
     * @param data  Raw byte buffer.  Must not be null.
     * @param len   Number of bytes to write.
     * @note MESSAGE THREAD.
     */
    void sendInput (const char* data, int len);

    /**
     * @brief Notifies the shell of a terminal resize via SIGWINCH.
     *
     * @param cols  New column count.
     * @param rows  New row count.
     * @note MESSAGE THREAD.
     */
    void resize (int cols, int rows);

    /**
     * @brief Returns a snapshot of all buffered PTY output bytes.
     *
     * Used to produce the `Message::loading` payload sent to a newly-attaching
     * client so it can reconstruct the display from scratch.
     *
     * @return `juce::MemoryBlock` containing the history, oldest bytes first.
     * @note MESSAGE THREAD.
     */
    juce::MemoryBlock snapshotHistory() const;

    /**
     * @brief Closes the PTY and stops the reader thread.
     *
     * Called explicitly when the shell exits or the session is torn down.
     * Idempotent — safe to call more than once.
     *
     * @note MESSAGE THREAD.
     */
    void stop();

    /**
     * @brief Returns the owned Processor.
     * @note MESSAGE THREAD.
     */
    Terminal::Processor& getProcessor() noexcept;

private:
    std::unique_ptr<TTY> tty;
    History history;
    std::unique_ptr<Terminal::Processor> processor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
