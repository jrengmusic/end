/**
 * @file Input.h
 * @brief Handles all keyboard input routing for a terminal processor.
 *
 * Terminal::Input centralises modal key dispatch, vim-style selection navigation,
 * open-file hint-label key matching, scrollback navigation, and the two-key `gg`
 * sequence.  It reads and writes terminal selection state via `State` parameters
 * directly — no `TerminalDisplay` members are touched.
 *
 * ### Responsibilities
 * - `handleKey()` — top-level dispatch for normal terminals: modal intercept → action system → scroll nav → PTY.
 * - `handleKeyDirect()` — direct PTY forward for popup terminals, bypasses action system.
 * - `handleScrollNav()` — Shift+PgUp/PgDn/Home/End scrollback scrolling.
 * - `clearSelectionAndScroll()` — clears drag/selection state and resets scroll offset.
 * - `buildKeyMap()` — parses Config key strings into cached `KeyPress` objects.
 * - `reset()` — clears `pendingG`; called when exiting selection mode.
 *
 * @see Terminal::Processor
 * @see Terminal::Screen
 * @see Terminal::LinkManager
 */

#pragma once

#include <JuceHeader.h>
#include "../rendering/ScreenSelection.h"
#include "../../SelectionType.h"

namespace Terminal
{
class Processor;
class LinkManager;
} // namespace Terminal

namespace Terminal
{

/**
 * @class Terminal::Input
 * @brief Keyboard input dispatcher for a single terminal processor.
 *
 * Constructed with references to the processor and link manager.  All
 * references must remain valid for the lifetime of the `Input` instance.
 *
 * @par Thread context
 * All public methods are **MESSAGE THREAD** only.
 */
class Input
{
public:
    /**
     * @brief Constructs an Input handler.
     * @param processor   Terminal processor (state, grid, key forwarding).
     * @param linkManager Link manager (used by `handleOpenFileKey`).
     * @note MESSAGE THREAD.
     */
    Input (Terminal::Processor& processor,
           Terminal::LinkManager& linkManager) noexcept;

    /**
     * @brief Top-level key handler for normal (non-popup) terminals.
     *
     * Dispatches:
     * 1. Modal intercept (`handleModalKey`) when a modal is active.
     * 2. Action system (`Action::handleKeyPress`).
     * 3. Scroll navigation (Shift+PgUp/PgDn/Home/End).
     * 4. Clear-and-forward — clears selection/scroll, then sends to PTY.
     *
     * @param key  The key press to handle.
     * @return `true` — all key events are consumed.
     * @note MESSAGE THREAD.
     */
    bool handleKey (const juce::KeyPress& key) noexcept;

    /**
     * @brief Key handler for popup terminals — bypasses the action system.
     *
     * Forwards the key directly to the processor PTY without action-system
     * interception.  Used when the terminal is hosted inside a popup where
     * the action system must not fire.
     *
     * @param key  The key press to handle.
     * @return `true` — all key events are consumed.
     * @note MESSAGE THREAD.
     */
    bool handleKeyDirect (const juce::KeyPress& key) noexcept;

    /**
     * @brief Handles Shift+PgUp/PgDn/Home/End scrollback navigation.
     *
     * @param keyCode      Key code of the pressed key.
     * @param newOffsetFn  Callable that writes the new scroll offset (clamped).
     *                     Signature: `void(int)`.
     * @note MESSAGE THREAD.
     */
    void handleScrollNav (int keyCode,
                          std::function<void (int)> newOffsetFn) noexcept;

    /**
     * @brief Clears any active drag/selection state and resets the scroll offset.
     *
     * Called before forwarding non-navigation keys to the PTY.
     *
     * @note MESSAGE THREAD.
     */
    void clearSelectionAndScroll() noexcept;

    /**
     * @brief Parses all selection-mode key strings from Config into the cached key map.
     *
     * Called from `TerminalDisplay::initialise()` and `applyConfig()` so config
     * reloads take effect without restarting.
     *
     * @note MESSAGE THREAD.
     */
    void buildKeyMap() noexcept;

    /**
     * @brief Clears the pending-g flag.
     *
     * Called by `TerminalDisplay::exitSelectionMode()` and `enterSelectionMode()`
     * to reset the two-key `gg` sequence state.
     *
     * @note MESSAGE THREAD.
     */
    void reset() noexcept;

    bool isSelectionCopyKey (const juce::KeyPress& key) const noexcept;

private:
    /**
     * @brief Dispatches a key to the handler for the currently active modal.
     *
     * @param key  The key press.
     * @return `true` if the modal consumed the key.
     * @note MESSAGE THREAD.
     */
    bool handleModalKey (const juce::KeyPress& key) noexcept;

    /**
     * @brief Handles a key event while vim-style selection mode is active.
     *
     * Reads/writes selection state via `State` params.  Returns `true` if the
     * key was consumed; always returns `true` (selection mode is fully modal).
     *
     * @param key  The key press.
     * @return `true` — selection mode consumes all keys.
     * @note MESSAGE THREAD.
     */
    bool handleSelectionKey (const juce::KeyPress& key) noexcept;

    /**
     * @brief Handles a key event while open-file hint-label mode is active.
     *
     * Escape exits the mode.  Letter keys `a`–`z` match against hint labels
     * via `LinkManager::hitTestHint()` and dispatch via `LinkManager::dispatch()`.
     *
     * @param key  The key press.
     * @return Always `true` — open-file mode is fully modal.
     * @note MESSAGE THREAD.
     */
    bool handleOpenFileKey (const juce::KeyPress& key) noexcept;

    /**
     * @brief Clamps and applies a new scroll offset.
     *
     * Reads the current offset and the scrollback high-water mark, clamps
     * @p newOffset to [0, scrollbackUsed], and writes it back if changed.
     *
     * @param newOffset  Desired scroll offset in lines (positive = scrolled back).
     * @note MESSAGE THREAD.
     */
    void setScrollOffsetClamped (int newOffset) noexcept;

    /**
     * @brief Parsed KeyPress objects for every selection-mode binding.
     *
     * Built by `buildKeyMap()` from Config strings.
     */
    struct SelectionKeys
    {
        juce::KeyPress up;
        juce::KeyPress down;
        juce::KeyPress left;
        juce::KeyPress right;
        juce::KeyPress visual;
        juce::KeyPress visualLine;
        juce::KeyPress visualBlock;
        juce::KeyPress copy;
        juce::KeyPress globalCopy;
        juce::KeyPress top;
        juce::KeyPress bottom;
        juce::KeyPress lineStart;
        juce::KeyPress lineEnd;
        juce::KeyPress exit;
    };

    /** @brief Terminal processor — provides state, grid, and key forwarding. */
    Terminal::Processor& processor;

    /** @brief Link manager — used by open-file mode for dispatch. */
    Terminal::LinkManager& linkManager;

    /** @brief Cached selection-mode key bindings, rebuilt on config reload. */
    SelectionKeys selectionKeys;

    /** @brief Cached open-file page-cycle key, rebuilt on config reload. */
    juce::KeyPress openFileNextPage;

    /** @brief Pending-g flag for the two-key `gg` (go-to-top) sequence. */
    bool pendingG { false };
};

} // namespace Terminal
