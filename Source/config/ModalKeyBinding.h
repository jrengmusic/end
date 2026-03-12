/**
 * @file ModalKeyBinding.h
 * @brief Prefix-key modal keybinding system for pane navigation.
 *
 * `ModalKeyBinding` implements a two-key sequence: a configurable prefix key
 * (default: backtick) followed by an action key (h/j/k/l).  It is globally
 * accessible via `ModalKeyBinding::getContext()`.
 *
 * ### State machine
 * - **idle** — normal operation; prefix key starts the sequence.
 * - **waiting** — prefix was consumed; next key dispatches an action or
 *   returns the key to the caller.  A timer cancels the sequence on timeout.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all methods must be called from the JUCE message thread.
 *
 * @see Config::Key::keysPrefix
 * @see Config::Key::keysPrefixTimeout
 */

#pragma once

#include <JuceHeader.h>
#include "Config.h"
#include "KeyBinding.h"

/**
 * @struct ModalKeyBinding
 * @brief Prefix-key modal keybinding dispatcher for pane navigation.
 *
 * Inherits `jreng::Context<ModalKeyBinding>` for global singleton access and
 * `juce::Timer` (privately) for prefix-sequence timeout.
 *
 * Constructed as a member of `MainComponent` after `Config` exists.
 *
 * @par Thread context
 * **MESSAGE THREAD**
 */
struct ModalKeyBinding : jreng::Context<ModalKeyBinding>, private juce::Timer
{
    ModalKeyBinding();
    ~ModalKeyBinding() override;

    /**
     * @brief Processes a key event from the terminal.
     *
     * In **idle** state: consumes the prefix key and enters **waiting** state.
     * In **waiting** state: dispatches a registered action or returns the key
     * to the caller.
     *
     * @param key  The key event to process.
     * @return `true` if the key was consumed; `false` if the caller should
     *         forward it to the terminal.
     */
    bool handleKeyPress (const juce::KeyPress& key);

    /**
     * @brief Reloads prefix key, timeout, and action keys from Config.
     *
     * Called on Cmd+R config reload.
     */
    void reload();

    /** @brief Pane navigation actions dispatched by the modal sequence. */
    enum class Action { paneLeft, paneDown, paneUp, paneRight, splitHorizontal, splitVertical };

    /**
     * @brief Registers a callback for the given action.
     *
     * Replaces any previously registered callback for @p action.
     *
     * @param action    The action to bind.
     * @param callback  Invoked when the action key is pressed after the prefix.
     */
    void setAction (Action action, std::function<void()> callback);

private:
    enum class State { idle, waiting };
    State state { State::idle };

    juce::KeyPress prefixKey;
    int timeoutMs { 1000 };

    struct ActionBinding
    {
        juce::KeyPress key;
        std::function<void()> callback;
    };

    std::vector<ActionBinding> actions;

    void loadFromConfig();
    void timerCallback() override;
};
