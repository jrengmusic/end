/**
 * @file Session.cpp
 * @brief Implementation of terminal::Session — PTY-side terminal session.
 *
 * @see Session.h
 */

#include "Session.h"
#include <BinaryData.h>

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

namespace terminal
{
/*____________________________________________________________________________*/

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
        const juce::File configDir { lua::Engine::getConfigPath() };

#if JUCE_WINDOWS
        const juce::String configPath { toMsysPath (configDir.getFullPathName()) };
#else
        const juce::String configPath { configDir.getFullPathName() };
#endif

        const juce::File executable { juce::File::getSpecialLocation (juce::File::currentExecutableFile) };

#if JUCE_WINDOWS
        seedEnv.set ("END_BINARY", toMsysPath (executable.getFullPathName()));
#else
        seedEnv.set ("END_BINARY", executable.getFullPathName());
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
                                           cell cols,
                                           cell rows,
                                           const juce::String& shell,
                                           const juce::String& args,
                                           const juce::StringPairArray& seedEnv,
                                           const juce::String& uuid)
{
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    const auto* cfg { lua::Engine::getContext() };
    const juce::String effectiveShell { shell.isNotEmpty()
                                            ? shell
                                            : cfg->nexus.shell.program };
    juce::String effectiveArgs { args.isNotEmpty() ? args
                                                   : cfg->nexus.shell.args };

    juce::StringPairArray mergedEnv { seedEnv };
    applyShellIntegration (effectiveShell, effectiveArgs, mergedEnv);

    auto session { std::make_unique<Session> (cols, rows, effectiveShell, effectiveArgs, cwd, mergedEnv, uuid) };
    return session;
}

/**
 * @brief Constructs the Session, wires the TTY, and transfers TTY ownership to Processor.
 *        Does NOT open the shell — call start() after Display/Screen construction.
 *
 * History capacity comes from `lua::Engine::nexus.terminal.scrollbackLines`.
 * The `onBytes` callback may be overridden by the owner (`Nexus` /
 * `nexus::Daemon`) after construction for daemon-mode byte broadcasting.
 * All TTY callbacks are wired before ownership is transferred to Processor via setTTY().
 * Open parameters are stored in startCols/startRows/startShell/startArgs/startCwd
 * for consumption by start().
 *
 * @note MESSAGE THREAD.
 */
Session::Session (cell cols,
                  cell rows,
                  const juce::String& shell,
                  const juce::String& args,
                  const juce::String& cwd,
                  const juce::StringPairArray& seedEnv,
                  const juce::String& uuid)
    : history { lua::Engine::getContext()->nexus.terminal.scrollbackLines }
{
#if JUCE_MAC || JUCE_LINUX
    auto tty { std::make_unique<UnixTTY>() };
#elif JUCE_WINDOWS
    auto tty { std::make_unique<WindowsTTY>() };
#endif

    // Create Processor before wiring TTY callbacks so procRawPtr is valid in lambdas.
    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    processor = std::make_unique<terminal::Processor> (grid, textBuffer, cols, rows, effectiveUuid);
    processor->getState().setId (effectiveUuid);

    terminal::Processor* procRawPtr { processor.get() };
    TTY* ttyRawPtr { tty.get() };

    ttyObserver = ttyRawPtr;

    tty->onShellExited = [procRawPtr]
    {
        procRawPtr->getState().setShellExited (true);
    };

    const auto& keys { seedEnv.getAllKeys() };

    for (const auto& key : keys)
        tty->addShellEnv (key, seedEnv[key]);

    // Store open parameters for start() — TTY open is deferred until after
    // Display/Screen are created so screen node atomics exist before the
    // reader thread writes to them.
    startCols  = cols;
    startRows  = rows;
    startShell = shell;
    startArgs  = args;
    startCwd   = cwd;

    // 1. Parser responses (DSR, DA, CPR) → PTY stdin.
    processor->setHostWriter ([ttyRawPtr] (const char* data, int len)
    {
        ttyRawPtr->write (data, len);
    });

    // 2. User input (keyboard, mouse) → PTY stdin.
    processor->setInputWriter ([ttyRawPtr] (const char* data, int len)
    {
        ttyRawPtr->write (data, len);
    });

    // 3. PTY output → history + external onBytes + Processor (with resize lock).
    tty->onData = [this, procRawPtr] (const char* bytes, int len)
    {
        history.append (bytes, static_cast<size_t> (len));

        if (onBytes != nullptr)
            onBytes (bytes, len);

        procRawPtr->process (bytes, len);
    };

    // 4. Drain-complete: flush parser responses, clear paste gate, sync resize.
    tty->onDrainComplete = [procRawPtr]
    {
        procRawPtr->flushResponses();
        procRawPtr->getState().clearPasteEchoGate();

        if (procRawPtr->getState().consumeSyncResize())
            procRawPtr->platformResize (
                cell (procRawPtr->getState().loadValue (terminal::id::SESSION, terminal::id::cols)),
                cell (procRawPtr->getState().loadValue (terminal::id::SESSION, terminal::id::visibleRows)));
    };

    // Transfer TTY ownership to Processor. setTTY() wires the TTY into GridSizeTransition for SIGWINCH delivery.
    processor->setTTY (std::move (tty));
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
Session::Session (cell cols,
                  cell rows,
                  const juce::String& cwd,
                  const juce::String& shell,
                  const juce::String& uuid)
    : history { lua::Engine::getContext()->nexus.terminal.scrollbackLines }
{
    jassert (cols.value > 0);
    jassert (rows.value > 0);

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    processor = std::make_unique<terminal::Processor> (grid, textBuffer, cols, rows, effectiveUuid);
    processor->getState().setId (effectiveUuid);
    processor->getState().get().setProperty (terminal::id::cwd, cwd, nullptr);
}

/**
 * @brief Stops the PTY and releases all resources.
 * @note MESSAGE THREAD.
 */
Session::~Session() { stop(); }

/**
 * @brief Writes raw input bytes to the PTY.
 *
 * Delegates to the TTY via ttyObserver (non-owning pointer; Processor owns the TTY).
 *
 * @note MESSAGE THREAD.
 */
void Session::sendInput (const char* data, int len)
{
    jassert (data != nullptr);
    jassert (len > 0);

    if (ttyObserver != nullptr)
        ttyObserver->write (data, len);
}


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
std::unique_ptr<Session> Session::create (cell cols,
                                           cell rows,
                                           const juce::String& cwd,
                                           const juce::String& shell,
                                           const juce::String& uuid)
{
    jassert (cols.value > 0);
    jassert (rows.value > 0);

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
    processor->flushResponses();
}

/**
 * @brief Serializes Processor state into @p block for daemon → GUI sync.
 *
 * @note MESSAGE THREAD.
 */
void Session::getStateInformation (juce::MemoryBlock& /*block*/) const
{
    // State serialization deferred — migrated from Grid to Screen, implementation pending.
}

/**
 * @brief Restores Processor state from a snapshot received from the daemon.
 *
 * @note MESSAGE THREAD.
 */
void Session::setStateInformation (const void* /*data*/, int /*size*/)
{
    // State serialization deferred — migrated from Grid to Screen, implementation pending.
}

/**
 * @brief Returns the owned Processor.
 *
 * @note MESSAGE THREAD.
 */
terminal::Processor& Session::getProcessor() noexcept
{
    jassert (processor != nullptr);
    return *processor;
}

/**
 * @brief Opens the TTY and starts the reader thread.
 *
 * No-op for remote (no-TTY) sessions — ttyObserver is null.
 * Must be called after Display/Screen are created and their screen nodes
 * grafted into the ValueTree, so all screen node atomics exist before the
 * reader thread fires cursorRow or screenDirty events.
 *
 * @note MESSAGE THREAD.
 */
void Session::start() noexcept
{
    if (ttyObserver != nullptr)
    {
        ttyObserver->open (startCols, startRows, startShell, startArgs, startCwd);

        // Force clear-screen on first prompt. Readline picks up this buffered byte when it
        // initializes and fires its clear-screen widget, wiping any stale bytes from the
        // resize chain and redrawing the prompt at the current PTY winsize.
        const char clearScreen { '\x0c' };
        ttyObserver->write (&clearScreen, 1);
    }
}

/**
 * @brief Closes the PTY and stops the reader thread.  Idempotent.
 *
 * Processor::~Processor() nulls TTY callbacks and calls tty->close() before
 * the reader thread can fire any further callbacks.  This matches the previous
 * explicit teardown sequence, now encapsulated in the Processor destructor.
 * Remote sessions (no TTY) follow the same path — Processor destructs cleanly.
 *
 * @note MESSAGE THREAD.
 */
void Session::stop()
{
    ttyObserver = nullptr;
    processor.reset();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal

