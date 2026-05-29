/**
 * @file Session.cpp
 * @brief Implementation of terminal::Session — PTY-side terminal session.
 *
 * @see Session.h
 */

#include "Session.h"

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
                                           jam::Cell::Rectangle dims,
                                           const juce::String& shell,
                                           const juce::String& args,
                                           const juce::StringPairArray& seedEnv,
                                           const juce::String& uuid)
{
    jassert (dims.isValid());

    const auto* cfg { lua::Engine::getContext() };
    const juce::String effectiveShell { shell.isNotEmpty()
                                            ? shell
                                            : cfg->nexus.shell.program };
    juce::String effectiveArgs { args.isNotEmpty() ? args
                                                   : cfg->nexus.shell.args };

    juce::StringPairArray mergedEnv { seedEnv };
    applyShellIntegration (effectiveShell, effectiveArgs, mergedEnv);

    const auto* appState { AppState::getContext() };
    const jam::Font font { appState->getFontFamily(),
                           appState->getFontSize(),
                           static_cast<float> (appState->getCellWidth()),
                           static_cast<float> (appState->getLineHeight()) };

    auto session { std::make_unique<Session> (dims, effectiveShell, effectiveArgs, cwd, mergedEnv, uuid, font) };
    return session;
}

/**
 * @brief Constructs the Session and stores deferred TTY open parameters.
 *
 * Creates Screen, Processor (owns Video and TextLineArray with its own buffer).
 * Stores shell/args/cwd/env for start().
 * Does NOT create the TTY — that happens in start() via Processor::startTTY() so
 * screen node atomics exist before the reader thread fires.
 *
 * @note MESSAGE THREAD.
 */
Session::Session (jam::Cell::Rectangle dims,
                  const juce::String& shell,
                  const juce::String& args,
                  const juce::String& cwd,
                  const juce::StringPairArray& seedEnv,
                  const juce::String& uuid,
                  const jam::Font& font)
    : textBuffer {}
    , state (textBuffer)
    , screen (state, font)
{
    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    processor = std::make_unique<terminal::Processor> (state, dims, textBuffer, effectiveUuid);
    state.setId (effectiveUuid);

    // Resize coordinator — coalesces dimension changes, suspends/resumes processing.
    wireResizer();

    // Store open parameters for start() — TTY open is deferred until after
    // Display/Screen are created so screen node atomics exist before the
    // reader thread writes to them.
    startCols  = dims.getWidth();
    startRows  = dims.getHeight();
    startShell = shell;
    startArgs  = args;
    startCwd   = cwd;
    startEnv   = seedEnv;
}

/**
 * @brief Constructs a remote Session — Processor + State only, no TTY.
 *
 * Creates Screen, Processor (owns Video and TextLineArray with its own buffer).
 * Wires CWD into State so display logic (tab title, cwd badge)
 * works identically to a local session.  TTY is not created.  Bytes must be
 * fed externally via getProcessor().process().
 *
 * @note MESSAGE THREAD.
 */
Session::Session (jam::Cell::Rectangle dims,
                  const juce::String& cwd,
                  const juce::String& shell,
                  const juce::String& uuid,
                  const jam::Font& font)
    : textBuffer {}
    , state (textBuffer)
    , screen (state, font)
{
    jassert (dims.isValid());

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    processor = std::make_unique<terminal::Processor> (state, dims, textBuffer, effectiveUuid);
    state.setId (effectiveUuid);

    state.get().setProperty (terminal::id::cwd, cwd, nullptr);

    wireResizer();
}

/**
 * @brief Wires Resizer trigger lambdas and Value::Listener binding.
 *
 * Called from both constructors after processor and screen are initialized.
 * Creates the jam::Resizer, registers start and stop triggers, and binds
 * winsize Value::Listener to TextEditor's viewport property in State.
 *
 * Start trigger (buffer/state setup while processing is suspended):
 *   processor->suspendProcessing(true)
 *   processor->prepare(newCols, newRows)  — CellFifo reset, Video resize, dirty flags, alt screen rebuild
 *   processor->suspendProcessing(false)
 *
 * Stop trigger (SIGWINCH after coalescing completes):
 *   processor->prepare(newCols, newRows)
 *
 * Display handles screen.setText via its own vTPC on screenDirty.
 *
 * @note MESSAGE THREAD.
 */
void Session::wireResizer() noexcept
{
    resizer = std::make_unique<jam::Resizer>();

    resizer->addTrigger<int, int> (jam::ID::start,
        [this] (int newCols, int newRows)
        {
            processor->suspendProcessing (true);
            processor->prepare (jam::Cell::Rectangle (cell (newCols), cell (newRows)));
            processor->suspendProcessing (false);
        });

    resizer->addTrigger<int, int> (jam::ID::stop,
        [this] (int newCols, int newRows)
        {
            processor->prepare (jam::Cell::Rectangle (cell (newCols), cell (newRows)));
        });

    // Bind winsize to TextEditor's state property — Value::Listener fires on change.
    auto teNode { state.get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };
    winsize.referTo (teNode.getPropertyAsValue (jam::TextEditor::properties.at (jam::TextEditor::viewportId), nullptr));
    winsize.addListener (this);
}

/**
 * @brief Fires on the message thread when TextEditor's winsize property changes.
 *
 * Triggers the Resizer when viewport dimensions change.  The Resizer's 16ms
 * coalescing timer prevents redundant resizes on rapid successive changes.
 *
 * @note MESSAGE THREAD — juce::Value::Listener fires on the message thread.
 */
void Session::valueChanged (juce::Value& value)
{
    if (value.refersToSameSourceAs (winsize))
    {
        const auto dims { jam::Cell::Rectangle::unpack (static_cast<int64_t> (static_cast<int> (winsize.getValue()))) };

        if (dims.isValid())
            resizer->set (jam::ID::start, dims.getWidth().value, dims.getHeight().value);
    }
}

/**
 * @brief Stops the PTY and releases all resources.
 * @note MESSAGE THREAD.
 */
Session::~Session() { stop(); }

/**
 * @brief Factory overload — creates a Processor-only Session with no TTY.
 *
 * @see Session::create (int, int, const juce::String&, const juce::String&, const juce::String&)
 *      in Session.h for full documentation.
 * @note MESSAGE THREAD.
 */
std::unique_ptr<Session> Session::create (jam::Cell::Rectangle dims,
                                           const juce::String& cwd,
                                           const juce::String& shell,
                                           const juce::String& uuid)
{
    jassert (dims.isValid());

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };

    const auto* appState { AppState::getContext() };
    const jam::Font font { appState->getFontFamily(),
                           appState->getFontSize(),
                           static_cast<float> (appState->getCellWidth()),
                           static_cast<float> (appState->getLineHeight()) };

    return std::make_unique<Session> (dims, cwd, shell, effectiveUuid, font);
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

    const juce::ScopedLock sl (processor->getCallbackLock());

    if (not processor->isSuspended())
    {
        processor->process (data, len);
        processor->flushResponses();
    }
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
 * @brief Returns the owned Screen.
 *
 * Display calls addAndMakeVisible(session.getScreen()) to parent Screen for rendering.
 * Screen always exists — owned by Session regardless of whether Display is attached.
 *
 * @note MESSAGE THREAD.
 */
terminal::Screen& Session::getScreen() noexcept { return screen; }

/**
 * @brief Creates the TTY via Processor::startTTY and starts the reader thread.
 *
 * No-op for remote (no-TTY) sessions — startShell is empty.
 * Must be called after Display/Screen are created and their screen nodes
 * grafted into the ValueTree, so all screen node atomics exist before the
 * reader thread fires cursorRow or screenDirty events.
 *
 * @note MESSAGE THREAD.
 */
void Session::start() noexcept
{
    processor->prepare (jam::Cell::Rectangle (startCols, startRows));

    if (startShell.isNotEmpty())
    {
        processor->startTTY (startShell, startArgs, startCwd, startEnv, jam::Cell::Rectangle (startCols, startRows));
    }
}

/**
 * @brief Closes the PTY and stops the reader thread.  Idempotent.
 *
 * Processor::~Processor() nulls onData and calls tty->close() before
 * the reader thread can fire any further callbacks.  This matches the previous
 * explicit teardown sequence, now encapsulated in the Processor destructor.
 * Remote sessions (no TTY) follow the same path — Processor destructs cleanly.
 *
 * @note MESSAGE THREAD.
 */
void Session::stop()
{
    processor.reset();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal

