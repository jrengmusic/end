/**
 * @file Session.h
 * @brief PTY-side terminal session: CodeModel, CodeView, Model, and Processor.
 *
 * `terminal::Session` is the data-source half of a terminal connection.  It owns:
 * - `jam::CodeModel` — multi-screen dimensionless document (2 screens for terminal).
 * - `jam::CodeView` — pure view rendering the CodeModel.
 * - `Model` — atomic parameter store.
 * - `terminal::Processor` — VT pipeline (Parser → Video → Buffer<Row> → CellFifo → Display → CodeModel).
 *   Processor owns the TTY — created and opened via `Processor::startTTY()` in `start()`.
 *
 * ### Data flow
 * ```
 * PTY → Processor::onData → Processor::onBytesReceived (IPC broadcast, daemon mode only)
 *                         → Processor::process (local + daemon)
 * ```
 *
 * ### Thread ownership
 * - All public methods are MESSAGE THREAD only.
 *
 * ### Naming disambiguation
 * This class is `terminal::Session`.  `Nexus` is the session manager that
 * owns one or more `terminal::Session` objects.
 * The two classes live in different namespaces; all source files that need
 * both must qualify fully.
 *
 * @see terminal::Processor
 * @see Nexus
 */

#pragma once

#include <JuceHeader.h>
#include "Model.h"
#include "Processor.h"
#include "../lua/Engine.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @class terminal::Session
 * @brief PTY-side terminal session — CodeModel, CodeView, Model, and Processor.
 *
 * Constructed by `Nexus` (or its mode-specific delegates).
 * TTY is created and opened by Processor::startTTY(), called from start().
 *
 * @par Thread context
 * All public methods — MESSAGE THREAD.
 */
class Session : private juce::Value::Listener
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
     * Gated on `lua::Engine::nexus.shell.integration` (default true).
     *
     * @param shell    Shell program path — used for type detection (contains "zsh", "bash", etc.).
     * @param args     Shell arguments — modified in place for bash (--posix) and pwsh (launch command).
     * @param seedEnv  Env var accumulator — populated with shell-specific integration vars.
     *
     * @note MESSAGE THREAD.  Called before terminal::Session construction.
     */
    static void applyShellIntegration (const juce::String& shell, juce::String& args,
                                       juce::StringPairArray& seedEnv);

    /**
     * @brief Factory — resolves shell/args from config, applies shell integration, and constructs.
     *
     * This is the single creation entry point for all PTY-backed terminal sessions.
     * When @p shell is empty, reads `lua::Engine::nexus.shell.program`.
     * When @p args is empty, reads `lua::Engine::nexus.shell.args`.
     * Calls `applyShellIntegration` before construction.
     * UUID defaults to empty — constructor generates one when empty.
     *
     * The caller is responsible for seeding additional env vars (e.g. PATH from a
     * parent session on non-Windows) by constructing with the lower-level constructor
     * after reading from the returned session if needed — or by passing a pre-built
     * seedEnv to the constructor directly.
     *
     * @param cwd    Initial working directory.  Empty = inherit.
     * @param dims   Terminal dimensions in cells. Must be valid.
     * @param shell  Shell program override.  Empty = read from config.
     * @param args   Shell arguments override.  Empty = read from config.
     * @param seedEnv  Extra environment variables.  Merged before construction.
     * @param uuid   Explicit UUID.  Empty = auto-generated.
     * @return Owning unique_ptr to the constructed terminal::Session.
     * @note MESSAGE THREAD.
     */
    static std::unique_ptr<Session> create (const juce::String& cwd,
                                             jam::Cell::Rectangle dims,
                                             const juce::String& shell = {},
                                             const juce::String& args = {},
                                             const juce::StringPairArray& seedEnv = {},
                                             const juce::String& uuid = {});

    /**
     * @brief Factory overload — creates a Processor-only Session with no TTY.
     *
     * Used by GUI connected to a daemon where the shell runs on the daemon process.
     * Bytes are fed externally via `process()`.  CWD is written
     * to Model so display logic (tab title, cwd badge) works identically to a local session.
     *
     * @param dims   Terminal dimensions in cells. Must be valid.
     * @param cwd    Initial working directory — written to Model.
     * @param shell  Shell program name (not stored in Model; reserved for future use).
     * @param uuid   Session UUID.  Empty = auto-generated.
     * @return Owning unique_ptr to the constructed terminal::Session.
     * @note MESSAGE THREAD.
     */
    static std::unique_ptr<Session> create (jam::Cell::Rectangle dims,
                                             const juce::String& cwd,
                                             const juce::String& shell,
                                             const juce::String& uuid);

    /**
     * @brief Constructs the Session and stores deferred TTY open parameters.
     *
     * Creates Processor and Screen; stores shell/args/cwd/env for use by start().
     * Does NOT open the TTY — call start() after Display/Screen construction so
     * screen node atomics exist before the reader thread fires.
     *
     * @param dims     Terminal dimensions in cells. Must be valid.
     * @param shell    Shell program path (e.g. "zsh", "/usr/bin/fish").
     * @param args     Shell arguments string.  Empty = none.
     * @param cwd      Initial working directory.  Empty = inherit.
     * @param seedEnv  Shell integration environment variable pairs.
     * @param uuid     Session UUID.  Empty = auto-generated.
     * @param font     Font metrics used for the text editor.
     */
    Session (jam::Cell::Rectangle dims,
             const juce::String& shell,
             const juce::String& args,
             const juce::String& cwd,
             const juce::StringPairArray& seedEnv,
             const juce::String& uuid,
             const jam::Font& font);

    /**
     * @brief Constructs a remote Session — Processor + Model only, no TTY.
     *
     * Used by Nexus client mode where the daemon owns the shell process.
     * Bytes are fed externally via `getProcessor().process()`.
     * CWD is written to Model so display logic works identically.
     *
     * @param dims   Terminal dimensions in cells. Must be valid.
     * @param cwd    Initial working directory — written to Model.
     * @param shell  Shell program name (not stored in Model; reserved for future use).
     * @param uuid   Session UUID.  Empty = auto-generated.
     * @param font   Font metrics used for the text editor.
     * @note MESSAGE THREAD.
     */
    Session (jam::Cell::Rectangle dims,
             const juce::String& cwd,
             const juce::String& shell,
             const juce::String& uuid,
             const jam::Font& font);

    /**
     * @brief Stops the PTY and releases all resources.
     * @note MESSAGE THREAD.
     */
    ~Session();

    /**
     * @brief Grafts the SESSION tree (terminal::Model root) into the given PANE node.
     *
     * Creates a jam::ValueTree::Attachment that owns the graft for the lifetime of this
     * Session. Destruction of Session destroys the Attachment, which ungrafts the
     * SESSION tree from the PANE node automatically.
     *
     * @param paneNode  The PANE juce::ValueTree node to graft into. Must be valid.
     * @note MESSAGE THREAD. Called by Panes after addLeaf / split.
     */
    void graftInto (juce::ValueTree paneNode);

    /**
     * @brief Creates the TTY via Processor::startTTY and starts the reader thread.
     *
     *  Must be called after Display/Screen are created and grafted,
     *  so screen node atomics exist before the reader writes to them.
     *
     *  No-op for remote (no-TTY) sessions.
     *
     *  @note MESSAGE THREAD.
     */
    void start() noexcept;

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
     * Currently stubbed — state serialization has been migrated from Grid to Screen
     * and the new path is not yet implemented.  The block is left unmodified.
     *
     * @param block  Destination block (unused while stubbed).
     * @note MESSAGE THREAD.
     */
    void getStateInformation (juce::MemoryBlock& block) const;

    /**
     * @brief Restores Processor state from a snapshot received from the daemon.
     *
     * Currently stubbed — state serialization has been migrated from Grid to Screen
     * and the new path is not yet implemented.  The snapshot is ignored.
     *
     * @param data  Snapshot bytes (unused while stubbed).
     * @param size  Byte count (unused while stubbed).
     * @note MESSAGE THREAD.
     */
    void setStateInformation (const void* data, int size);

    /**
     * @brief Returns the owned Processor.
     * @note MESSAGE THREAD.
     */
    terminal::Processor& getProcessor() noexcept;

    /**
     * @brief Returns the owned TextEditor.
     *
     * Display calls addAndMakeVisible(session.getTextEditor()) to parent it for rendering.
     * TextEditor always exists — owned by Session regardless of whether Display is attached.
     *
     * @note MESSAGE THREAD.
     */
    jam::CodeView& getTextEditor() noexcept;

    /**
     * @brief Returns the owned CodeModel.
     *
     * Display drains CellFifo entries into this model via append/replaceAt.
     * CodeModel is declared before textEditor — it constructs first and destroys last.
     *
     * @note MESSAGE THREAD.
     */
    jam::CodeModel& getCodeModel() noexcept;

private:
    // Cross-thread string buffer — gone. Slots now live in ParameterText.
    Model model;                                        ///< Terminal parameter store — constructed before CodeModel.
    jam::CodeModel codeModel;                           ///< Document model — constructed before textEditor; outlives it.
    jam::CodeView textEditor;                           ///< Terminal viewport renderer — references codeModel.
    std::unique_ptr<terminal::Processor> processor;     ///< VT pipeline orchestrator — constructed last.

    /** @brief RAII graft of the SESSION tree into the owning PANE node. Created by graftInto(). */
    std::unique_ptr<jam::ValueTree::Attachment> sessionAttachment;

    /** @brief Resize coordinator — coalesces dimension changes, suspends/resumes Processor.
     *  Constructed in the constructor body after processor is valid. */
    std::unique_ptr<jam::Resizer> resizer;

    /** @brief Bound to TextEditor's winsize property — fires valueChanged when cell dimensions change. */
    juce::Value winsize;

    /** @brief Called on the message thread when winsize changes. Triggers resize via Resizer. */
    void valueChanged (juce::Value& value) override;

    /** @brief Wires Resizer trigger lambdas and Value::Listener binding — called from both constructors. */
    void wireResizer() noexcept;

    // Deferred TTY open parameters — consumed by start().
    // Populated in the PTY constructor; empty for remote sessions.
    cell                    startCols  { 0 };
    cell                    startRows  { 0 };
    juce::String            startShell {};
    juce::String            startArgs  {};
    juce::String            startCwd   {};
    juce::StringPairArray   startEnv   {};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
