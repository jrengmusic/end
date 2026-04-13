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
 * This class is `Terminal::Session`.  `Nexus` is the session manager that
 * owns one or more `Terminal::Session` objects.
 * The two classes live in different namespaces; all source files that need
 * both must qualify fully.
 *
 * @see Terminal::History
 * @see Terminal::Processor
 * @see Nexus
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
 * Constructed by `Nexus` (or its mode-specific delegates).  Caller
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
     * @brief Populates shell integration env vars and sideloads hook scripts.
     *
     * Detects the shell type from @p shell, sideloads the matching integration
     * scripts from BinaryData to `~/.config/end/`, and sets the env vars that
     * cause the shell to source them on startup (ZDOTDIR for zsh, ENV for bash,
     * XDG_DATA_DIRS for fish, launch args for pwsh).
     *
     * Gated on `Config::Key::shellIntegration` (default true).
     *
     * @param shell    Shell program path — used for type detection (contains "zsh", "bash", etc.).
     * @param args     Shell arguments — modified in place for bash (--posix) and pwsh (launch command).
     * @param seedEnv  Env var accumulator — populated with shell-specific integration vars.
     *
     * @note MESSAGE THREAD.  Called before Terminal::Session construction.
     */
    static void applyShellIntegration (const juce::String& shell, juce::String& args,
                                       juce::StringPairArray& seedEnv);

    /**
     * @brief Factory — resolves shell/args from config, applies shell integration, and constructs.
     *
     * This is the single creation entry point for all PTY-backed terminal sessions.
     * When @p shell is empty, reads `Config::Key::shellProgram`.
     * When @p args is empty, reads `Config::Key::shellArgs`.
     * Calls `applyShellIntegration` before construction.
     * UUID defaults to empty — constructor generates one when empty.
     *
     * The caller is responsible for seeding additional env vars (e.g. PATH from a
     * parent session on non-Windows) by constructing with the lower-level constructor
     * after reading from the returned session if needed — or by passing a pre-built
     * seedEnv to the constructor directly.
     *
     * @param cwd    Initial working directory.  Empty = inherit.
     * @param cols   Initial column count.  Must be > 0.
     * @param rows   Initial row count.  Must be > 0.
     * @param shell  Shell program override.  Empty = read from config.
     * @param args   Shell arguments override.  Empty = read from config.
     * @param seedEnv  Extra environment variables.  Merged before construction.
     * @param uuid   Explicit UUID.  Empty = auto-generated.
     * @return Owning unique_ptr to the constructed Terminal::Session.
     * @note MESSAGE THREAD.
     */
    static std::unique_ptr<Session> create (const juce::String& cwd,
                                             int cols,
                                             int rows,
                                             const juce::String& shell = {},
                                             const juce::String& args = {},
                                             const juce::StringPairArray& seedEnv = {},
                                             const juce::String& uuid = {});

    /**
     * @brief Factory overload — creates a Processor-only Session with no TTY.
     *
     * Used by GUI connected to a daemon where the shell runs on the daemon process.
     * Bytes are fed externally via `process()`.  CWD and shellProgram are written
     * to State so display logic (tab title, cwd badge) works identically to a local session.
     *
     * History capacity is read from `Config::Key::terminalScrollbackLines`.
     *
     * @param cols   Terminal width in character columns.  Must be > 0.
     * @param rows   Terminal height in character rows.  Must be > 0.
     * @param cwd    Initial working directory — written to State.
     * @param shell  Shell program name — written to State for displayName logic.
     * @param uuid   Session UUID.  Empty = auto-generated.
     * @return Owning unique_ptr to the constructed Terminal::Session.
     * @note MESSAGE THREAD.
     */
    static std::unique_ptr<Session> create (int cols, int rows,
                                             const juce::String& cwd,
                                             const juce::String& shell,
                                             const juce::String& uuid);

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
     * @param uuid     Session UUID.  Empty = auto-generated.
     */
    Session (int cols, int rows,
             const juce::String& shell,
             const juce::String& args,
             const juce::String& cwd,
             const juce::StringPairArray& seedEnv,
             const juce::String& uuid);

    /**
     * @brief Constructs a remote Session — Processor + State only, no TTY.
     *
     * Used by Nexus client mode where the daemon owns the shell process.
     * Bytes are fed externally via `getProcessor().process()`.
     * CWD and shellProgram are written to State so display logic works identically.
     *
     * @param cols   Terminal width.  Must be > 0.
     * @param rows   Terminal height.  Must be > 0.
     * @param cwd    Initial working directory — written to State.
     * @param shell  Shell program name — written to State for displayName logic.
     * @param uuid   Session UUID.  Empty = auto-generated.
     * @note MESSAGE THREAD.
     */
    Session (int cols, int rows,
             const juce::String& cwd,
             const juce::String& shell,
             const juce::String& uuid);

    /**
     * @brief Stops the PTY and releases all resources.
     * @note MESSAGE THREAD.
     */
    ~Session();

    /**
     * @brief Called on the READER THREAD for every chunk of PTY output.
     *
     * `Nexus` sets this before construction completes:
     * - Non-nexus: wired to `Processor::process`.
     * - Daemon: wired to broadcast-to-subscribers (byte forwarding via IPC).
     */
    std::function<void (const char*, int)> onBytes;

    /**
     * @brief Called on the MESSAGE THREAD when the shell process exits.
     *
     * Set by `Nexus` to handle lifecycle cleanup.
     */
    std::function<void()> onExit;

    /**
     * @brief Called on the READER THREAD after each state flush (cwd and foreground process updated in State).
     *
     * Fires at the end of the internal `state.onFlush` lambda, after cwd and
     * foreground process have been written into State.  `Interprocess::Daemon` sets
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
     * @note READER THREAD (called from tty->onDrainComplete during sync-resize).
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
     * @brief Feeds raw bytes into the Processor pipeline and flushes parser responses.
     *
     * Used by GUI connected to daemon — byte chunks arrive from the daemon IPC
     * layer and are injected here rather than from a live PTY.  Mirrors the
     * pattern used in the local PTY onBytes path.
     *
     * @param data  Raw byte buffer.  Must not be null.
     * @param len   Number of bytes to feed.  Must be > 0.
     * @note MESSAGE THREAD.
     */
    void process (const char* data, int len);

    /**
     * @brief Serializes Processor state into @p block for daemon → GUI sync.
     *
     * Delegates to `Processor::getStateInformation`.  Wraps the call at Session
     * level so callers do not need to reach into `getProcessor()` directly.
     *
     * @param block  Destination block.  Appended to, not replaced.
     * @note MESSAGE THREAD.
     */
    void getStateInformation (juce::MemoryBlock& block) const;

    /**
     * @brief Restores Processor state from a snapshot received from the daemon.
     *
     * Delegates to `Processor::setStateInformation`.  Wraps the call at Session
     * level so callers do not need to reach into `getProcessor()` directly.
     *
     * @param data  Snapshot bytes produced by `getStateInformation`.  Must not be null.
     * @param size  Byte count of the snapshot.  Must be > 0.
     * @note MESSAGE THREAD.
     */
    void setStateInformation (const void* data, int size);

    /**
     * @brief Returns the owned Processor.
     * @note MESSAGE THREAD.
     */
    Terminal::Processor& getProcessor() noexcept;

private:
    std::unique_ptr<TTY> tty;
    History history;
    std::unique_ptr<Terminal::Processor> processor;

    /** @brief True when the OS-level getCwd query should be used in onFlush.
     *  Set to !shellIntegrationEnabled at construction time.  When false,
     *  CWD tracking relies entirely on OSC 7 from shell integration hooks. */
    bool shouldTrackCwdFromOs { false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
