/**
 * @file Action.h
 * @brief Standalone action registry with key dispatch and prefix state machine.
 *
 * `Action::Registry` is the single owner of all user-performable actions in
 * END.  It holds a fixed action table (entries never change after construction),
 * a key map rebuilt on every config reload, and the prefix state machine
 * previously implemented in `ModalKeyBinding`.
 *
 * ### Lifecycle
 * 1. Constructed once (typically by `MainComponent`).
 * 2. Caller registers every action via `registerAction()`.
 * 3. `buildKeyMap()` is called automatically at construction and on `reload()`.
 * 4. `TerminalDisplay::keyPressed` delegates to `handleKeyPress()`.
 * 5. If `handleKeyPress()` returns `true` the key is consumed; otherwise the
 *    caller forwards it to the PTY.
 *
 * ### Key resolution order
 * 1. If in **waiting** state: look up key in modal bindings → execute → idle.
 * 2. If in **idle** state and key equals the prefix key: enter **waiting** state.
 * 3. If in **idle** state: look up key in global bindings → execute.
 * 4. No match → return `false`.
 *
 * @note All methods must be called on the **MESSAGE THREAD**.
 *
 * @see Config::Key
 * @see jam::Context
 */

#pragma once

#include <JuceHeader.h>
#include "../config/Config.h"

// ---------------------------------------------------------------------------
// std::hash specialisation for juce::KeyPress
// Required so juce::KeyPress can be used as a key in std::unordered_map.
// Ported from KeyBinding.h.
// ---------------------------------------------------------------------------
namespace std
{
template<>
struct hash<juce::KeyPress>
{
    size_t operator() (const juce::KeyPress& k) const noexcept
    {
        const auto h1 { std::hash<int> {}(k.getKeyCode()) };
        const auto h2 { std::hash<int> {}(k.getModifiers().getRawFlags()) };
        return h1 ^ (h2 << 16);
    }
};
}

namespace Action
{

/**
 * @class Registry
 * @brief Global action registry, key dispatcher, and prefix state machine.
 *
 * Inherits `jam::Context<Registry>` for process-wide singleton access via
 * `Registry::getContext()`.  Inherits `juce::Timer` (privately) to implement
 * the prefix-sequence timeout.
 *
 * Registry is deliberately ignorant of `Terminal::Display`, `Session`,
 * `Grid`, `Tabs`, or any other subsystem.  All behaviour is injected as
 * `std::function<bool()>` callbacks at registration time.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods must be called from the JUCE
 * message thread.
 */
class Registry
    : public jam::Context<Registry>
    , private juce::Timer
{
public:
    //==========================================================================
    /**
     * @struct Entry
     * @brief Immutable descriptor for a single user-performable action.
     *
     * Populated once via `registerAction()` and never modified afterwards.
     * The `shortcut` field is the only member updated indirectly: it is
     * re-resolved from Config during `buildKeyMap()`, but the Entry itself
     * is not mutated — the key maps are rebuilt from scratch.
     */
    struct Entry
    {
        /** @brief Unique machine-readable identifier (e.g. `"copy"`). */
        juce::String id;

        /** @brief Human-readable display name (e.g. `"Copy"`). */
        juce::String name;

        /** @brief One-line description shown in the command palette. */
        juce::String description;

        /** @brief Logical grouping (e.g. `"Edit"`, `"View"`, `"Panes"`). */
        juce::String category;

        /** @brief Resolved shortcut key (rebuilt from Config on reload). */
        juce::KeyPress shortcut;

        /** @brief True if this action requires the prefix key before its shortcut. */
        bool isModal { false };

        /**
         * @brief Invoked when the action is triggered.
         *
         * Returns `true` if the action consumed the event (key should not be
         * forwarded to the PTY).  Returns `false` to let the key fall through
         * (e.g. copy with no active selection).
         */
        std::function<bool()> execute;
    };

    //==========================================================================
    /**
     * @brief Constructs the registry. Key map must be built after actions are registered via `buildKeyMap()`.
     *
     * Config must already be constructed before calling this constructor.
     */
    Registry();

    /** @brief Stops any active prefix timer before destruction. */
    ~Registry() override;

    //==========================================================================
    /**
     * @brief Registers a new action in the table.
     *
     * Must be called before `handleKeyPress()` is ever invoked.  Registering
     * the same `id` twice is a programming error (asserted in debug builds).
     *
     * @param id           Unique machine-readable identifier (e.g. `"copy"`).
     * @param name         Human-readable display name (e.g. `"Copy"`).
     * @param description  One-line description for the command palette.
     * @param category     Logical grouping (e.g. `"Edit"`).
     * @param isModal      `true` if the prefix key must precede the shortcut.
     * @param callback     Invoked on dispatch; returns `true` if consumed.
     */
    void registerAction (const juce::String& id,
                         const juce::String& name,
                         const juce::String& description,
                         const juce::String& category,
                         bool isModal,
                         std::function<bool()> callback);

    /**
     * @brief Processes a key event from the terminal.
     *
     * Implements the two-state prefix machine:
     * - **idle**: prefix key → **waiting**; global binding → execute.
     * - **waiting**: modal binding → execute → **idle**; unknown → **idle**.
     *
     * @param key  The key event to evaluate.
     * @return `true` if the key was consumed; `false` if the caller should
     *         forward it to the PTY.
     */
    bool handleKeyPress (const juce::KeyPress& key);

    /**
     * @brief Clears all entries and key bindings.
     *
     * Called before re-registering all actions (e.g. on config reload).
     * The prefix state machine is reset to idle.
     */
    void clear();

    /**
     * @brief Reads Config and rebuilds `globalBindings`, `modalBindings`,
     *        `prefixKey`, and `prefixTimeoutMs` from registered entries.
     *
     * Called after all actions are registered (or re-registered on reload).
     * The prefix state machine is reset to idle before this runs.
     */
    void buildKeyMap();

    /**
     * @brief Re-reads shortcuts from Config and rebuilds the key maps.
     *
     * Registered callbacks are not touched.  The prefix state machine is
     * reset to idle before rebuilding.
     */
    void reload();

    /**
     * @brief Returns the full action table (read-only).
     *
     * Intended for the command palette / action list UI.
     *
     * @return Const reference to the internal entries vector.
     */
    const std::vector<Entry>& getEntries() const noexcept;

    /**
     * @brief Parses a shortcut string (e.g. `"ctrl+shift+["`) into a KeyPress.
     *
     * Supports modifier tokens `ctrl`, `cmd`, `shift`, `alt`, `opt` and key
     * names `pageup`, `pagedown`, `home`, `end`, `delete`, `insert`,
     * `escape`, `return`, `tab`, `space`, `backspace`, `f1`–`f12`, and any
     * single printable character.
     *
     * @param shortcutString  The shortcut string from Config (case-insensitive).
     * @return The parsed `juce::KeyPress`, or an invalid KeyPress on failure.
     */
    static juce::KeyPress parseShortcut (const juce::String& shortcutString);

    /**
     * @brief Converts a KeyPress back to its canonical shortcut string.
     *
     * Inverse of `parseShortcut`.  Produces the same `"modifier+key"` format
     * so that `parseShortcut (shortcutToString (kp)) == kp` for any valid
     * KeyPress.
     *
     * @param key  The KeyPress to serialise.
     * @return Canonical shortcut string (e.g. `"cmd+c"`, `"shift+f1"`).
     */
    static juce::String shortcutToString (const juce::KeyPress& key);

    /**
     * @brief Returns the Config key string for a given action ID, or empty if none.
     *
     * Looks up the action ID in the internal config key table.  Actions that
     * are not backed by a Config key (e.g. popup actions) return an empty string.
     *
     * @param actionId  The action ID (e.g. `"copy"`).
     * @return The config key string (e.g. `"keys.copy"`), or empty.
     */
    static juce::String configKeyForAction (const juce::String& actionId);

private:
    //==========================================================================
    /** @brief All registered actions, in registration order. */
    std::vector<Entry> entries;

    /** @brief Global bindings: KeyPress → index into `entries`. */
    std::unordered_map<juce::KeyPress, int> globalBindings;

    /** @brief Modal bindings: KeyPress → index into `entries` (post-prefix). */
    std::unordered_map<juce::KeyPress, int> modalBindings;

    //==========================================================================
    /** @brief States of the prefix state machine. */
    enum class PrefixState
    {
        idle,
        waiting
    };

    /** @brief Current state of the prefix machine. */
    PrefixState prefixState { PrefixState::idle };

    /** @brief The configured prefix key (e.g. backtick). */
    juce::KeyPress prefixKey;

    /** @brief Milliseconds before the waiting state times out. */
    int prefixTimeoutMs { 1000 };

    //==========================================================================
    /**
     * @brief Cancels the waiting state when the prefix-sequence timeout fires.
     *
     * Called by `juce::Timer` on the message thread.
     */
    void timerCallback() override;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Registry)
};

} // namespace Action
