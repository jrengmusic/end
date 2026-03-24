/**
 * @file InputHandler.h
 * @brief Handles all keyboard input routing for a terminal session.
 *
 * InputHandler centralises modal key dispatch, vim-style selection navigation,
 * open-file hint-label key matching, scrollback navigation, and the two-key `gg`
 * sequence.  It reads and writes terminal selection state via `State` parameters
 * directly — no `TerminalComponent` members are touched.
 *
 * ### Responsibilities
 * - `handleKey()` — top-level dispatch: modal intercept → action system → scroll nav → PTY.
 * - `handleScrollNav()` — Shift+PgUp/PgDn/Home/End scrollback scrolling.
 * - `clearSelectionAndScroll()` — clears drag/selection state and resets scroll offset.
 * - `buildKeyMap()` — parses Config key strings into cached `KeyPress` objects.
 * - `reset()` — clears `pendingG`; called when exiting selection mode.
 *
 * @see Terminal::Session
 * @see Terminal::Screen
 * @see LinkManager
 */

#pragma once

#include <JuceHeader.h>
#include "../terminal/rendering/ScreenSelection.h"
#include "../terminal/selection/SelectionType.h"

namespace Terminal
{
class Session;
class ScreenBase;
class LinkManager;
} // namespace Terminal

/**
 * @class InputHandler
 * @brief Keyboard input dispatcher for a single terminal session.
 *
 * Constructed with references to the session, screen, and link manager.  All
 * references must remain valid for the lifetime of the `InputHandler`.
 *
 * @par Thread context
 * All public methods are **MESSAGE THREAD** only.
 */
class InputHandler
{
public:
    /**
     * @brief Constructs an InputHandler.
     * @param session     Terminal session (state, grid, key forwarding).
     * @param screen      Terminal renderer (cursor overlay, scroll offset).
     * @param linkManager Link manager (used by `handleOpenFileKey`).
     * @note MESSAGE THREAD.
     */
    InputHandler (Terminal::Session& session,
                  Terminal::ScreenBase& screen,
                  Terminal::LinkManager& linkManager) noexcept;

    /**
     * @brief Top-level key handler.
     *
     * Dispatches:
     * 1. Modal intercept (`handleModalKey`) when a modal is active.
     * 2. Action system (`Action::handleKeyPress`).
     * 3. Scroll navigation (Shift+PgUp/PgDn/Home/End).
     * 4. Clear-and-forward — clears selection/scroll, then sends to PTY.
     *
     * @param key           The key press to handle.
     * @param isPopupTerminal  When `true`, bypasses the action system (popup model).
     * @return `true` — all key events are consumed.
     * @note MESSAGE THREAD.
     */
    bool handleKey (const juce::KeyPress& key, bool isPopupTerminal) noexcept;

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
     * Called from `TerminalComponent::initialise()` and `applyConfig()` so config
     * reloads take effect without restarting.
     *
     * @note MESSAGE THREAD.
     */
    void buildKeyMap() noexcept;

    /**
     * @brief Clears the pending-g flag.
     *
     * Called by `TerminalComponent::exitSelectionMode()` and `enterSelectionMode()`
     * to reset the two-key `gg` sequence state.
     *
     * @note MESSAGE THREAD.
     */
    void reset() noexcept;

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

    /** @brief Terminal session — provides state, grid, and key forwarding. */
    Terminal::Session& session;

    /** @brief Terminal renderer — provides scroll offset and cursor queries. */
    Terminal::ScreenBase& screen;

    /** @brief Link manager — used by open-file mode for dispatch. */
    Terminal::LinkManager& linkManager;

    /** @brief Cached selection-mode key bindings, rebuilt on config reload. */
    SelectionKeys selectionKeys;

    /** @brief Pending-g flag for the two-key `gg` (go-to-top) sequence. */
    bool pendingG { false };
};
