/**
 * @file Session.cpp
 * @brief Implementation of Terminal::Session — PTY-side terminal session.
 *
 * @see Session.h
 */

#include "Session.h"
#include "../tty/TTY.h"
#include "../../lua/Engine.h"
#include <BinaryData.h>
#if JUCE_MAC || JUCE_LINUX
#include "../tty/UnixTTY.h"
#elif JUCE_WINDOWS
#include "../tty/WindowsTTY.h"
#endif

// =============================================================================
// Shell integration helpers
// =============================================================================

#if JUCE_WINDOWS
/**
 * @brief Converts a Windows path to MSYS2/Cygwin format.
 *
 * `C:\foo\bar` becomes `/c/foo/bar`.  Needed because MSYS2 zsh reads
 * ZDOTDIR as a POSIX path, not a Windows path.
 *
 * @param path  Windows-style path (backslashes, drive letter).
 * @return MSYS2-style path (forward slashes, `/driveletter/`).
 */
static juce::String toMsysPath (const juce::String& path)
{
    juce::String result { path.replace (juce::File::getSeparatorString(), "/") };

    if (result.length() >= 3 and std::isalpha (static_cast<unsigned char> (result[0])) and result[1] == ':'
        and result[2] == '/')
    {
        result = "/" + juce::String::charToString (std::tolower (static_cast<unsigned char> (result[0])))
                 + result.substring (2);
    }

    return result;
}
#endif

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Populates shell integration env vars and sideloads hook scripts.
 *
 * @see Session::applyShellIntegration (Session.h) for full documentation.
 * @note MESSAGE THREAD.
 */
void Session::applyShellIntegration (const juce::String& shell, juce::String& args, juce::StringPairArray& seedEnv)
{
    // Prevent MSYS2 /etc/post-install/05-home-dir.post from cd-ing to $HOME in
    // login shells when SHLVL<=1.  Harmless on macOS/Linux where it is unused.
    seedEnv.set ("CHERE_INVOKING", "1");

    if (lua::Engine::getContext()->nexus.shell.integration)
    {
        const juce::File configDir {
            juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end")
        };

#if JUCE_WINDOWS
        const juce::String configPath { toMsysPath (configDir.getFullPathName()) };
#else
        const juce::String configPath { configDir.getFullPathName() };
#endif

        if (shell.contains ("zsh"))
        {
            const juce::File zshDir { configDir.getChildFile ("zsh") };
            zshDir.createDirectory();

            const BinaryData::Raw zshenv { "zsh_zshenv.zsh" };
            const BinaryData::Raw endInteg { "zsh_end_integration.zsh" };

            if (zshenv.exists())
            {
                zshDir.getChildFile (".zshenv").replaceWithData (zshenv.data, static_cast<size_t> (zshenv.size));
            }

            if (endInteg.exists())
            {
                zshDir.getChildFile ("end-integration")
                    .replaceWithData (endInteg.data, static_cast<size_t> (endInteg.size));
            }

            const char* origZdotdir { getenv ("ZDOTDIR") };

            if (origZdotdir != nullptr)
                seedEnv.set ("END_ORIG_ZDOTDIR", origZdotdir);

            seedEnv.set ("ZDOTDIR", configPath + "/zsh");
        }
        else if (shell.contains ("bash"))
        {
            const BinaryData::Raw bashScript { "bash_integration.bash" };

            if (bashScript.exists())
            {
                configDir.createDirectory();
                configDir.getChildFile ("bash_integration.bash")
                    .replaceWithData (bashScript.data, static_cast<size_t> (bashScript.size));

                seedEnv.set ("ENV", configPath + "/bash_integration.bash");
                seedEnv.set ("END_BASH_INJECT", "1");
                seedEnv.set ("END_BASH_UNEXPORT_HISTFILE", "1");

                if (args.contains ("--norc"))
                    seedEnv.set ("END_BASH_NORC", "1");

                args = "--posix " + args;
            }
        }
        else if (shell.contains ("fish"))
        {
            const BinaryData::Raw fishScript { "end-shell-integration.fish" };

            if (fishScript.exists())
            {
                const juce::File fishDir { configDir.getChildFile ("fish/vendor_conf.d") };
                fishDir.createDirectory();
                fishDir.getChildFile ("end-shell-integration.fish")
                    .replaceWithData (fishScript.data, static_cast<size_t> (fishScript.size));

                juce::String newXdg { configPath };
                const char* origXdg { getenv ("XDG_DATA_DIRS") };

                if (origXdg != nullptr and juce::String (origXdg).isNotEmpty())
                    newXdg += ":" + juce::String (origXdg);

                seedEnv.set ("XDG_DATA_DIRS", newXdg);
                seedEnv.set ("END_FISH_XDG_DATA_DIR", configPath);
            }
        }
        else if (shell.contains ("pwsh") or shell.contains ("powershell"))
        {
            const BinaryData::Raw psScript { "powershell_integration.ps1" };

            if (psScript.exists())
            {
                configDir.createDirectory();
                const juce::File scriptFile { configDir.getChildFile ("powershell_integration.ps1") };
                scriptFile.replaceWithData (psScript.data, static_cast<size_t> (psScript.size));

                args = "-NoLogo -NoProfile -NoExit -Command \". '" + scriptFile.getFullPathName() + "'\"";
            }
        }
    }
}

/**
 * @brief Factory — resolves shell/args from config, applies shell integration, and constructs.
 *
 * @see Session::create (Session.h) for full documentation.
 * @note MESSAGE THREAD.
 */
std::unique_ptr<Session> Session::create (const juce::String& cwd,
                                           int cols,
                                           int rows,
                                           const juce::String& shell,
                                           const juce::String& args,
                                           const juce::StringPairArray& seedEnv,
                                           const juce::String& uuid)
{
    jassert (cols > 0);
    jassert (rows > 0);

    const auto* cfg { lua::Engine::getContext() };
    const juce::String effectiveShell { shell.isNotEmpty()
                                            ? shell
                                            : cfg->nexus.shell.program };
    juce::String effectiveArgs { args.isNotEmpty() ? args
                                                   : cfg->nexus.shell.args };

    juce::StringPairArray mergedEnv { seedEnv };
    applyShellIntegration (effectiveShell, effectiveArgs, mergedEnv);

    const bool integrationEnabled { cfg->nexus.shell.integration };

    auto session { std::make_unique<Session> (cols, rows, effectiveShell, effectiveArgs, cwd, mergedEnv, uuid) };
    session->shouldTrackCwdFromOs = not integrationEnabled;
    return session;
}

/**
 * @brief Constructs the Session, creates the TTY, opens the shell, and wires the Processor.
 *
 * History capacity comes from `lua::Engine::nexus.terminal.scrollbackLines`.
 * The `onBytes` and `onExit` callbacks may be overridden by the owner (`Nexus` /
 * `Interprocess::Daemon`) after construction for daemon-mode byte broadcasting.  The Processor pipeline
 * callbacks (tty->onDrainComplete, state.onFlush, setHostWriter) are wired internally.
 *
 * @note MESSAGE THREAD.
 */
Session::Session (int cols,
                  int rows,
                  const juce::String& shell,
                  const juce::String& args,
                  const juce::String& cwd,
                  const juce::StringPairArray& seedEnv,
                  const juce::String& uuid)
    : history { lua::Engine::getContext()->nexus.terminal.scrollbackLines }
{
#if JUCE_MAC || JUCE_LINUX
    tty = std::make_unique<UnixTTY>();
#elif JUCE_WINDOWS
    tty = std::make_unique<WindowsTTY>();
#endif

    tty->onExit = [this]
    {
        if (processor != nullptr and processor->onShellExited != nullptr)
            processor->onShellExited();

        if (onExit != nullptr)
            onExit();
    };

    const auto& keys { seedEnv.getAllKeys() };

    for (const auto& key : keys)
        tty->addShellEnv (key, seedEnv[key]);

    tty->open (cols, rows, shell, args, cwd);

    // Force clear-screen on first prompt. Readline picks up this buffered byte when it
    // initializes and fires its clear-screen widget, wiping any stale bytes from the
    // resize chain and redrawing the prompt at the current PTY winsize.
    const char clearScreen { '\x0c' };
    tty->write (&clearScreen, 1);

    // Create Processor and wire the terminal pipeline.
    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    processor = std::make_unique<Terminal::Processor> (cols, rows, effectiveUuid);
    processor->getState().setId (effectiveUuid);

    Terminal::Processor* procRawPtr { processor.get() };

    // 1. Parser responses (DSR, DA, CPR) → PTY stdin.
    processor->setHostWriter (
        [this] (const char* data, int len)
        {
            sendInput (data, len);
        });

    // 2a. User input (keyboard, mouse) → PTY stdin.
    processor->writeInput = [this] (const char* data, int len)
    {
        sendInput (data, len);
    };

    // 2b. Terminal resize → PTY SIGWINCH.
    processor->onResize = [this] (int cols, int rows)
    {
        resize (cols, rows);
    };

    // 2. PTY output → history + external onBytes + Processor (with resize lock).
    tty->onData = [this, procRawPtr] (const char* bytes, int len)
    {
        history.append (bytes, static_cast<size_t> (len));

        if (onBytes != nullptr)
            onBytes (bytes, len);

        procRawPtr->processWithLock (bytes, len);
    };

    // 3. Drain-complete: flush parser responses, clear paste gate, sync resize.
    tty->onDrainComplete = [this, procRawPtr]
    {
        procRawPtr->getParser().flushResponses();
        procRawPtr->getState().clearPasteEchoGate();

        if (procRawPtr->getState().consumeSyncResize())
            platformResize (procRawPtr->getGrid().getCols(), procRawPtr->getGrid().getVisibleRows());
    };

    // 4. State flush: query cwd + foreground process from PTY, then fire external onStateFlush.
    procRawPtr->getState().onFlush = [this, procRawPtr]
    {
        const int fgPid { getForegroundPid() };
        const int shellPid { getShellPid() };

        if (fgPid > 0)
        {
            if (fgPid == shellPid)
            {
                procRawPtr->getState().setForegroundProcess ("", 0);
            }
            else
            {
                char fgNameBuf[Terminal::State::maxStringLength] {};
                const int fgNameLen { getProcessName (fgPid, fgNameBuf, Terminal::State::maxStringLength) };

                if (fgNameLen > 0)
                    procRawPtr->getState().setForegroundProcess (fgNameBuf, fgNameLen);
            }

            if (shouldTrackCwdFromOs)
            {
                char cwdBuf[Terminal::State::maxStringLength] {};
                const int cwdLen { getCwd (fgPid, cwdBuf, Terminal::State::maxStringLength) };

                if (cwdLen > 0)
                    procRawPtr->getState().setCwd (cwdBuf, cwdLen);
            }
        }

        if (onStateFlush != nullptr)
            onStateFlush();
    };
}

/**
 * @brief Constructs a remote Session — Processor + State only, no TTY.
 *
 * Creates Processor and wires CWD into State so display logic
 * (tab title, cwd badge) works identically to a local session.  TTY is not
 * created.  Bytes must be fed externally via getProcessor().process().
 *
 * @note MESSAGE THREAD.
 */
Session::Session (int cols, int rows,
                  const juce::String& cwd,
                  const juce::String& shell,
                  const juce::String& uuid)
    : history { lua::Engine::getContext()->nexus.terminal.scrollbackLines }
{
    jassert (cols > 0);
    jassert (rows > 0);

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    processor = std::make_unique<Terminal::Processor> (cols, rows, effectiveUuid);
    processor->getState().setId (effectiveUuid);
    processor->getState().get().setProperty (Terminal::ID::cwd, cwd, nullptr);
}

/**
 * @brief Stops the PTY and releases all resources.
 * @note MESSAGE THREAD.
 */
Session::~Session() { stop(); }

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
 * @note MESSAGE THREAD — called from state.onFlush via State::timerCallback.
 */
int Session::getForegroundPid() const noexcept
{
    jassert (tty != nullptr);
    return tty->getForegroundPid();
}

/**
 * @brief Returns the PID of the spawned shell process.
 *
 * @note MESSAGE THREAD — called from state.onFlush via State::timerCallback.
 */
int Session::getShellPid() const noexcept
{
    jassert (tty != nullptr);
    return tty->getShellPid();
}

/**
 * @brief Writes the process name for the given PID into the buffer.
 *
 * @note MESSAGE THREAD — called from state.onFlush via State::timerCallback.
 */
int Session::getProcessName (int pid, char* buffer, int maxLength) const noexcept
{
    jassert (tty != nullptr);
    return tty->getProcessName (pid, buffer, maxLength);
}

/**
 * @brief Writes the current working directory for the given PID into the buffer.
 *
 * @note MESSAGE THREAD — called from state.onFlush via State::timerCallback.
 */
int Session::getCwd (int pid, char* buffer, int maxLength) const noexcept
{
    jassert (tty != nullptr);
    return tty->getCwd (pid, buffer, maxLength);
}

#if ! JUCE_WINDOWS
/**
 * @brief Reads an environment variable from the given PID's live environment.
 *
 * Uses a stack buffer and delegates to TTY::getEnvVar.
 *
 * @note MESSAGE THREAD.
 */
juce::String Session::getEnvVar (int pid, const juce::String& name) const
{
    jassert (tty != nullptr);

    static constexpr int envVarBufferBytes { 8192 };
    char buf[envVarBufferBytes] {};
    const int written { tty->getEnvVar (pid, name.toRawUTF8(), buf, static_cast<int> (sizeof (buf))) };

    juce::String result;

    if (written > 0)
        result = juce::String::fromUTF8 (buf, written);

    return result;
}
#endif

/**
 * @brief Returns a snapshot of all buffered history bytes.
 *
 * @note MESSAGE THREAD.
 */
juce::MemoryBlock Session::snapshotHistory() const { return history.snapshot(); }

/**
 * @brief Factory overload — creates a Processor-only Session with no TTY.
 *
 * @see Session::create (int, int, const juce::String&, const juce::String&, const juce::String&)
 *      in Session.h for full documentation.
 * @note MESSAGE THREAD.
 */
std::unique_ptr<Session> Session::create (int cols, int rows,
                                           const juce::String& cwd,
                                           const juce::String& shell,
                                           const juce::String& uuid)
{
    jassert (cols > 0);
    jassert (rows > 0);

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };

    return std::make_unique<Session> (cols, rows, cwd, shell, effectiveUuid);
}

/**
 * @brief Feeds raw bytes into the Processor pipeline and flushes parser responses.
 *
 * @note MESSAGE THREAD.
 */
void Session::process (const char* data, int len)
{
    jassert (processor != nullptr);
    jassert (data != nullptr);
    jassert (len > 0);

    processor->process (data, len);
    processor->getParser().flushResponses();
}

/**
 * @brief Serializes Processor state into @p block for daemon → GUI sync.
 *
 * @note MESSAGE THREAD.
 */
void Session::getStateInformation (juce::MemoryBlock& block) const
{
    jassert (processor != nullptr);
    processor->getStateInformation (block);
}

/**
 * @brief Restores Processor state from a snapshot received from the daemon.
 *
 * @note MESSAGE THREAD.
 */
void Session::setStateInformation (const void* data, int size)
{
    jassert (processor != nullptr);
    jassert (data != nullptr);
    jassert (size > 0);

    processor->setStateInformation (data, size);
}

/**
 * @brief Returns the owned Processor.
 *
 * @note MESSAGE THREAD.
 */
Terminal::Processor& Session::getProcessor() noexcept
{
    jassert (processor != nullptr);
    return *processor;
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
        tty->onExit = nullptr;
        tty->onData = nullptr;
        tty->onDrainComplete = nullptr;

        // Close TTY first — joins the reader thread.  Processor must outlive
        // the reader because TTY::onData captures procRawPtr and the reader
        // may be mid-processWithLock when close() is called.  Once close()
        // returns, the reader thread is gone and Processor can be destroyed
        // safely.  The State timer runs on the message thread (same thread
        // as stop()), so it cannot fire mid-shutdown.
        tty->close();
        processor.reset();
        tty = nullptr;
    }
    else
    {
        // Remote session — no TTY.  Processor may still be running its State timer.
        // Resetting here stops the timer (via State::~State → stopTimer) safely.
        processor.reset();
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

