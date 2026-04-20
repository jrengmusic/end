/**
 * @file State.h
 * @brief APVTS-style atomic terminal state with a timer-driven ValueTree bridge.
 *
 * ## Architecture overview
 *
 * `State` mirrors the JUCE AudioProcessorValueTreeState (APVTS) pattern for a
 * terminal emulator instead of an audio plug-in:
 *
 * - **Reader thread** (PTY / VT parser) writes exclusively to `std::atomic<float>`
 *   slots via the `set*()` setters.  No ValueTree mutations, no allocations, no
 *   locks — identical to how an audio thread calls `getRawParameterValue()`.
 *
 * - **Timer** (`juce::Timer`, fires on the message thread) calls `flush()` at a
 *   fixed interval.  `flush()` copies every dirty atomic into the ValueTree,
 *   which is the Single Source of Truth (SSOT) for the UI.  This is the
 *   equivalent of `flushParameterValuesToValueTree()` in APVTS.
 *
 * - **Message thread** (UI / component code) reads exclusively from the
 *   ValueTree via `get()`, `getValue()`, and `getMode()`.  It may also
 *   write `scrollOffset` directly because that parameter is owned by the UI
 *   and never touched by the reader thread.
 *
 * ### Thread ownership summary
 * | Parameter group     | Writer            | Reader (hot path)  | Reader (UI)       |
 * |---------------------|-------------------|--------------------|-------------------|
 * | activeScreen        | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | cols / visibleRows  | MESSAGE THREAD    | Grid buffer (resizeLock) | CachedValue  |
 * | cursor (row/col/…)  | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | scroll region       | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | mode flags          | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | scrollOffset        | MESSAGE THREAD    | ValueTree          | ValueTree         |
 *
 * ### ValueTree structure
 * ```
 * SESSION  uuid=<string>  cols=<int>  visibleRows=<int>
 * ├── PARAM id="activeScreen"  value=<float>
 * ├── PARAM id="scrollOffset"  value=<float>
 * ├── MODES
 * │   ├── PARAM id="originMode"        value=<float>
 * │   ├── PARAM id="autoWrap"          value=<float>
 * │   └── … (all Terminal::ID mode identifiers)
 * ├── NORMAL
 * │   ├── PARAM id="cursorRow"    value=<float>
 * │   ├── PARAM id="cursorCol"    value=<float>
 * │   ├── PARAM id="wrapPending"  value=<float>
 * │   ├── PARAM id="scrollTop"    value=<float>
 * │   └── PARAM id="scrollBottom" value=<float>
 * └── ALTERNATE
 *     └── … (same per-screen params as NORMAL)
 * ```
 *
 * @see Terminal::ID   — all juce::Identifier constants used as parameter keys.
 * @see Terminal::Cell — the grid cell type whose layout this state governs.
 */

#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <memory>

#include "Cell.h"
#include "Identifier.h"
#include "../selection/LinkSpan.h"
#include "../../ModalType.h"
#include "../../SelectionType.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Atomically adds `delta` to `atom` and returns the value before the add.
 *
 * Equivalent to `std::atomic::fetch_add` but works with any value type via
 * `compare_exchange_weak`, allowing use with types like `float` that lack a
 * dedicated `fetch_add` overload.
 *
 * @tparam ValueType  Integral or floating-point type stored in the atomic.
 * @param atom   The target atomic variable.
 * @param delta  Value to add.
 * @param order  Memory order for the successful exchange (default: seq_cst).
 * @return The value of `atom` before the add.
 * @note Lock-free, noexcept.
 */
template <typename ValueType>
ValueType fetchAdd (std::atomic<ValueType>& atom, ValueType delta,
                    std::memory_order order = std::memory_order_seq_cst) noexcept
{
    ValueType expected { atom.load (std::memory_order_relaxed) };

    while (not atom.compare_exchange_weak (expected, expected + delta, order, std::memory_order_relaxed))
    {
    }

    return expected;
}

/**
 * @brief Atomically subtracts `delta` from `atom` and returns the value before the subtraction.
 *
 * Equivalent to `std::atomic::fetch_sub` but works with any value type via
 * `compare_exchange_weak`, allowing use with types like `float` that lack a
 * dedicated `fetch_sub` overload.
 *
 * @tparam ValueType  Integral or floating-point type stored in the atomic.
 * @param atom   The target atomic variable.
 * @param delta  Value to subtract.
 * @param order  Memory order for the successful exchange (default: seq_cst).
 * @return The value of `atom` before the subtraction.
 * @note Lock-free, noexcept.
 */
template <typename ValueType>
ValueType fetchSub (std::atomic<ValueType>& atom, ValueType delta,
                    std::memory_order order = std::memory_order_seq_cst) noexcept
{
    ValueType expected { atom.load (std::memory_order_relaxed) };

    while (not atom.compare_exchange_weak (expected, expected - delta, order, std::memory_order_relaxed))
    {
    }

    return expected;
}

/**
 * @brief Alias for the map type used to store owned atomic parameter and string slots.
 *
 * Uses `unique_ptr` for ownership so that the container itself can be a plain
 * `unordered_map` without requiring a stable-address backing deque.  Because
 * `unique_ptr` elements are heap-allocated, their addresses are stable across
 * map inserts and re-hashes.
 *
 * @tparam ValueType  The type of the owned object (e.g. `std::atomic<float>` or `StringSlot`).
 */
template <typename ValueType>
using StateMap = std::unordered_map<juce::Identifier, std::unique_ptr<ValueType>>;

/**
 * @enum ActiveScreen
 * @brief Selects between the normal and alternate terminal screen buffers.
 *
 * VT terminals support two independent screen buffers.  The alternate screen
 * is typically activated by full-screen applications (vim, less, …) via the
 * `?1049h` / `?1049l` private mode sequences.  The value doubles as an array
 * index into per-screen parameter arrays.
 */
enum ActiveScreen : size_t
{
    normal = 0,///< Primary screen buffer (default).
    alternate = 1///< Alternate screen buffer (full-screen apps).
};

// ModalType and SelectionType are now app-level — see Source/ModalType.h, Source/SelectionType.h
using ::ModalType;
using ::SelectionType;

/**
 * @struct State
 * @brief APVTS-style terminal parameter store: reader thread writes atomics,
 *        timer flushes to ValueTree, UI reads ValueTree.
 *
 * ## Design rationale
 *
 * The VT parser runs on a dedicated reader thread and must never block.
 * Directly mutating a `juce::ValueTree` from a non-message thread is unsafe
 * (ValueTree uses internal locks and may call listeners synchronously).
 * Instead, every mutable parameter is backed by a `std::atomic<float>` that
 * the reader thread can store to with `std::memory_order_relaxed`.
 *
 * A `juce::Timer` (message thread) polls `needsFlush` and, when set, copies
 * all atomics into the ValueTree in one pass.  UI components attach
 * `juce::ValueTree::Listener` to the ValueTree and receive change callbacks
 * only on the message thread — exactly the APVTS contract.
 *
 * ## Key invariants
 * - `set*()` methods: callable from ANY thread, lock-free, noexcept.
 * - `get*()` atomic getters: callable from ANY thread, lock-free, noexcept.
 * - `get()`, `getValue()`, `getMode()`: MESSAGE THREAD only.
 * - `timerCallback()`: MESSAGE THREAD only (called by juce::Timer).
 * - `flush()` and its helpers: MESSAGE THREAD only (called from `timerCallback()` and `refresh()`).
 *
 * @note `State` inherits `juce::Timer` and starts the timer in its constructor.
 *       The timer interval is chosen to keep UI latency below one frame at
 *       60 Hz (~16 ms) while avoiding unnecessary ValueTree churn.
 */
struct State : public juce::Timer
{
    /**
     * @brief Constructs the State, builds the ValueTree skeleton, populates
     *        the atomic parameter map, and starts the flush timer.
     *
     * @note MESSAGE THREAD — must be constructed on the message thread so that
     *       the timer is registered with the correct thread's event loop.
     */
    /** Maximum byte length for string slots (title, CWD, foreground process). */
    static constexpr int maxStringLength { 256 };

    /**
     * @brief Fixed-size char buffer for string parameters.
     *
     * Used by State internally for string storage (title, cwd, foreground
     * process) and externally by Parser for in-flight string tracking
     * without heap allocation on the reader thread.
     */
    struct StringSlot
    {
        char buffer[maxStringLength] {};
    };

    State();

    /**
     * @brief Stops the timer and destroys the State.
     *
     * @note MESSAGE THREAD — must be destroyed on the message thread.
     */
    ~State() override;

    // =========================================================================
    /** @name Reader-thread setters
     *  Write to the backing `std::atomic<float>` slots and set `needsFlush`.
     *  Safe to call from any thread.  Never touch the ValueTree directly.
     *  Analogous to calling `param->setValueNotifyingHost()` from an audio
     *  thread — the actual ValueTree update is deferred to the timer.
     * @{ */

    /**
     * @brief Sets the session UUID identifier on the root ValueTree node.
     *
     * Called once at construction by `Nexus::create` to stamp each
     * Processor's state with its owning session's UUID.  Consumers (e.g.
     * `Tabs::globalFocusChanged`) read this via `getValueTree().getProperty(jam::ID::id)`.
     *
     * @param uuid  The session UUID to store.
     * @note MESSAGE THREAD.
     */
    void setId (const juce::String& uuid);

    /**
     * @brief Sets the currently active screen buffer.
     * @param s  `ActiveScreen::normal` or `ActiveScreen::alternate`.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setScreen (ActiveScreen s) noexcept;

    /**
     * @brief Sets a named terminal mode flag.
     * @param id     A `Terminal::ID` mode identifier (e.g. `ID::autoWrap`).
     * @param value  `true` to enable the mode, `false` to disable.
     * @note READER THREAD — lock-free, noexcept.
     *       The key is built via `modeKey (id)` and must exist in the
     *       parameter map (asserted in debug builds).
     */
    void setMode (const juce::Identifier& id, bool value) noexcept;

    /**
     * @brief Sets the cursor row for the specified screen buffer.
     * @param s    Target screen (`normal` or `alternate`).
     * @param row  Zero-based row index within the visible area.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setCursorRow (ActiveScreen s, int row) noexcept;

    /**
     * @brief Sets the cursor column for the specified screen buffer.
     * @param s    Target screen (`normal` or `alternate`).
     * @param col  Zero-based column index.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setCursorCol (ActiveScreen s, int col) noexcept;

    /**
     * @brief Sets cursor visibility for the specified screen buffer.
     * @param s  Target screen (`normal` or `alternate`).
     * @param v  `true` to show the cursor, `false` to hide it.
     * @note READER THREAD — lock-free, noexcept.
     *       Corresponds to the `?25h` / `?25l` private mode sequences.
     */
    void setCursorVisible (ActiveScreen s, bool v) noexcept;

    /**
     * @brief Sets the pending-wrap flag for the specified screen buffer.
     * @param s  Target screen (`normal` or `alternate`).
     * @param p  `true` when the cursor is at the right margin and the next
     *           printable character should trigger a line wrap.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setWrapPending (ActiveScreen s, bool p) noexcept;

    /**
     * @brief Sets the top row of the scrolling region for the specified screen.
     * @param s    Target screen (`normal` or `alternate`).
     * @param top  Zero-based row index of the first scrolling row (DECSTBM top).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setScrollTop (ActiveScreen s, int top) noexcept;

    /**
     * @brief Sets the bottom row of the scrolling region for the specified screen.
     * @param s       Target screen (`normal` or `alternate`).
     * @param bottom  Zero-based row index of the last scrolling row (DECSTBM bottom).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setScrollBottom (ActiveScreen s, int bottom) noexcept;

    /**
     * @brief Sets the DECSCUSR cursor shape for the specified screen.
     * @param s      Target screen.
     * @param shape  DECSCUSR Ps value (0-6). 0 = default (user config).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setCursorShape (ActiveScreen s, int shape) noexcept;

    /**
     * @brief Sets the cursor color override from OSC 12.
     * @param s  Target screen.
     * @param r  Red component (0-255).
     * @param g  Green component (0-255).
     * @param b  Blue component (0-255).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setCursorColor (ActiveScreen s, int r, int g, int b) noexcept;

    /**
     * @brief Resets the cursor color override (OSC 112). Reverts to user config.
     * @param s  Target screen.
     * @note READER THREAD — lock-free, noexcept.
     */
    void resetCursorColor (ActiveScreen s) noexcept;

    /**
     * @brief Pushes keyboard mode flags onto the per-screen stack.
     *
     * Implements `CSI > flags u` from the progressive keyboard protocol (CSI u).
     * Stores `flags` on the stack and flushes the top-of-stack value
     * to the atomic slot for the given screen.
     *
     * @param s      Target screen (`normal` or `alternate`).
     * @param flags  Bitmask of keyboard enhancement flags to push.
     * @note READER THREAD — lock-free, noexcept.
     */
    void pushKeyboardMode (ActiveScreen s, uint32_t flags) noexcept;

    /**
     * @brief Pops entries from the per-screen keyboard mode stack.
     *
     * Implements `CSI < number u` from the progressive keyboard protocol (CSI u).
     * Pops up to `count` entries and flushes the new top-of-stack
     * (or 0 if the stack is empty) to the atomic slot.
     *
     * @param s      Target screen (`normal` or `alternate`).
     * @param count  Number of entries to pop (clamped to stack size).
     * @note READER THREAD — lock-free, noexcept.
     */
    void popKeyboardMode (ActiveScreen s, int count) noexcept;

    /**
     * @brief Sets keyboard mode flags directly on the per-screen stack top.
     *
     * Implements `CSI = flags ; mode u` from the progressive keyboard protocol (CSI u).
     *
     * @param s      Target screen (`normal` or `alternate`).
     * @param flags  Bitmask of keyboard enhancement flags.
     * @param mode   1 = set (absolute), 2 = OR, 3 = AND-NOT.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setKeyboardMode (ActiveScreen s, uint32_t flags, int mode) noexcept;

    /**
     * @brief Resets the keyboard mode stack for the specified screen.
     *
     * Clears the stack and flushes 0 to the atomic slot. Called during
     * terminal reset (RIS).
     *
     * @param s  Target screen (`normal` or `alternate`).
     * @note READER THREAD — lock-free, noexcept.
     */
    void resetKeyboardMode (ActiveScreen s) noexcept;

    /**
     * @brief Sets the number of rows currently used in the scrollback buffer.
     *
     * Written by the reader thread after scroll operations that change the
     * scrollback depth, and by the message thread during `Grid::resize()`.
     * Uses `storeAndFlush` so the value is visible to the message thread on
     * the next flush pass.
     *
     * @param value  Row count currently occupied in the scrollback buffer.
     * @note READER THREAD or MESSAGE THREAD — lock-free, noexcept.
     */
    void setScrollbackUsed (int value) noexcept
    {
        storeAndFlush (ID::scrollbackUsed, static_cast<float> (value));
    }

    /**
     * @brief Sets the window title from OSC 0/2 escape sequences.
     *
     * Copies `src` (up to `length` bytes, capped at `StringSlot::maxLength - 1`)
     * into the State-owned title buffer, then increments the generation counter
     * so that `flushStrings()` can detect the change without touching the
     * caller's buffer.
     *
     * @param src     Pointer to the raw UTF-8 title bytes.  Need not be
     *                null-terminated; `length` governs the byte count.
     * @param length  Number of bytes to copy from `src`.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setTitle (const char* src, int length) noexcept;

    /**
     * @brief Sets the current working directory from OSC 7 or OS query.
     *
     * Copies `src` into the State-owned cwd buffer.  See `setTitle()` for the
     * full semantics of the SeqLock-style snap-copy used in `flushStrings()`.
     *
     * @param src     Pointer to the raw UTF-8 path bytes.
     * @param length  Number of bytes to copy from `src`.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setCwd (const char* src, int length) noexcept;

    /**
     * @brief Sets the foreground process name from OS query.
     *
     * Copies `src` into the State-owned foreground-process buffer.
     * See `setTitle()` for the full semantics.
     *
     * @param src     Pointer to the raw UTF-8 process name bytes.
     * @param length  Number of bytes to copy from `src`.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setForegroundProcess (const char* src, int length) noexcept;

    /**
     * @brief Records an OSC 8 hyperlink span from the parser.
     *
     * Emplaces or overwrites the entry in `hyperlinkMap` keyed by `id` and
     * increments the generation counter with a release store so that
     * `flushHyperlinks()` (message thread) observes the completed write.
     * Sets `needsFlush` so the timer wakes on the next tick.
     *
     * @param id          Unique identifier for this span (e.g. `"row_startCol"`).
     * @param uri         Pointer to the raw UTF-8 URI bytes.
     * @param uriLength   Number of bytes in `uri`.
     * @param row         Zero-based visible row where the span lives.
     * @param startCol    Zero-based start column (inclusive).
     * @param endCol      Zero-based end column (exclusive).
     * @note READER THREAD — lock-free, noexcept.
     */
    void storeHyperlink (const juce::Identifier& id, const char* uri, int uriLength,
                         int row, int startCol, int endCol) noexcept;

    /**
     * @brief Clears all recorded OSC 8 hyperlink spans.
     *
     * Clears `hyperlinkMap` and bumps the generation counter so
     * `flushHyperlinks()` rebuilds the HYPERLINKS ValueTree children on
     * the next flush.
     *
     * @note READER THREAD — lock-free, noexcept.
     */
    void clearHyperlinks() noexcept;

    /** @} */

    // =========================================================================
    /** @name Message-thread ValueTree getters for active-screen cursor state
     *  These getters determine the active screen internally from the ValueTree
     *  and return post-flush values.  MESSAGE THREAD only.
     * @{ */

    /**
     * @brief Returns the currently active screen buffer from the ValueTree
     *        (post-flush value).
     *
     * Reads the `activeScreen` PARAM child of the root SESSION node.  This
     * method reflects the state as of the last timer flush and is therefore
     * consistent with all other ValueTree properties.  Use this inside
     * message-thread code (UI components, ValueTree listeners, mouse/keyboard
     * handlers) where consistency with the rest of the flushed state matters.
     *
     * @return `ActiveScreen::normal` or `ActiveScreen::alternate`.
     * @note MESSAGE THREAD only — reads from the ValueTree (post-flush values).
     */
    ActiveScreen getActiveScreen() const noexcept;

    /**
     * @brief Returns the current terminal column count.
     * @return Number of columns (e.g. 80 or 132).
     * @note MESSAGE THREAD — reads from CachedValue, noexcept.
     */
    int getCols() const noexcept;

    /**
     * @brief Returns the number of rows visible in the terminal viewport.
     * @return Number of visible rows (e.g. 24).
     * @note MESSAGE THREAD — reads from CachedValue, noexcept.
     */
    int getVisibleRows() const noexcept;

    /** @} */

    // =========================================================================
    /** @name Message-thread ValueTree accessors
     *  Read from (or write to) the ValueTree, which is the SSOT for the UI.
     *  All methods in this group must be called on the message thread only.
     * @{ */

    /**
     * @brief Returns the root `SESSION` ValueTree.
     *
     * UI components can attach a `juce::ValueTree::Listener` to this tree to
     * receive change notifications after each timer flush.
     *
     * @return A reference-counted copy of the root ValueTree node.
     * @note MESSAGE THREAD only.
     */
    juce::ValueTree get() noexcept;
    juce::ValueTree get() const noexcept;

    /**
     * @brief Returns a `juce::Value` bound to a specific parameter in the tree.
     *
     * The returned `juce::Value` stays live as long as the ValueTree exists.
     * UI controls (sliders, buttons, …) can attach to it directly.
     *
     * @param paramId  The parameter identifier (e.g. `ID::cols`).
     * @return A `juce::Value` that reflects and can modify the parameter's
     *         `value` property in the ValueTree.
     * @note MESSAGE THREAD only.  Writing through the returned Value does NOT
     *       update the backing atomic; it is intended for UI-owned parameters
     *       such as `scrollOffset`.
     */
    juce::Value getValue (const juce::Identifier& paramId);

    /**
     * @brief Reads a mode flag directly from the ValueTree (post-flush value).
     *
     * Unlike `getMode()`, which reads the atomic (reader-thread value), this
     * method reads the ValueTree and therefore reflects the last flushed state.
     * Use this when you need the value to be consistent with other ValueTree
     * properties (e.g. inside a ValueTree listener callback).
     *
     * @param id  A `Terminal::ID` mode identifier.
     * @return `true` if the mode is enabled in the ValueTree.
     * @note MESSAGE THREAD only.
     */
    bool getMode (const juce::Identifier& id) const noexcept;

    /**
     * @brief Reads the progressive keyboard protocol flags from the ValueTree (post-flush value).
     *
     * Determines the active screen via `getActiveScreen()`, navigates to the
     * corresponding screen child node (`NORMAL` or `ALTERNATE`), and reads
     * the `keyboardFlags` parameter.
     *
     * @return The active keyboard enhancement flags (0 = legacy mode).
     * @note MESSAGE THREAD only — reads from the ValueTree (post-flush values).
     */
    uint32_t getKeyboardFlags() const noexcept;

    /** @brief Returns the cursor row for the active screen (post-flush). MESSAGE THREAD. */
    int getCursorRow() const noexcept;

    /** @brief Returns the cursor column for the active screen (post-flush). MESSAGE THREAD. */
    int getCursorCol() const noexcept;

    /** @brief Returns cursor visibility for the active screen (post-flush). MESSAGE THREAD. */
    bool isCursorVisible() const noexcept;

    /** @brief Returns the cursor shape for the active screen (post-flush). MESSAGE THREAD. */
    int getCursorShape() const noexcept;

    /** @brief Returns the cursor colour red component for the active screen (post-flush). MESSAGE THREAD. */
    float getCursorColorR() const noexcept;

    /** @brief Returns the cursor colour green component for the active screen (post-flush). MESSAGE THREAD. */
    float getCursorColorG() const noexcept;

    /** @brief Returns the cursor colour blue component for the active screen (post-flush). MESSAGE THREAD. */
    float getCursorColorB() const noexcept;

    /**
     * @brief Sets the vertical scroll offset (UI-owned parameter).
     *
     * `scrollOffset` is the only parameter written exclusively by the message
     * thread.  It represents how many lines the viewport has been scrolled
     * back into the scrollback buffer.  The reader thread never touches it.
     *
     * @param offset  Number of lines scrolled back (0 = bottom / live view).
     * @note MESSAGE THREAD only.  Writes directly to the ValueTree.
     */
    void setScrollOffset (int offset) noexcept;

    /**
     * @brief Returns the current vertical scroll offset from the ValueTree.
     * @return Number of lines scrolled back (0 = live view).
     * @note MESSAGE THREAD only.
     */
    int getScrollOffset() const noexcept;

    /** @brief Returns the number of scrollback lines currently stored (post-flush). MESSAGE THREAD. */
    int getScrollbackUsed() const noexcept;

    /** @brief Returns the terminal window title (post-flush). MESSAGE THREAD. */
    juce::String getTitle() const noexcept;

    /** @brief Returns the current working directory (post-flush). MESSAGE THREAD. */
    juce::String getCwd() const noexcept;

    /**
     * @brief Returns the current cursor blink phase.
     *
     * `true` during the visible half of the blink cycle, `false` during the
     * hidden half.  Always `true` when blinking is disabled (steady cursor).
     *
     * The blink counter is ticked in `timerCallback()` and reset to "on"
     * whenever `flush()` detects that the cursor position has changed.
     *
     * @return `true` if the cursor should be drawn this frame.
     * @note MESSAGE THREAD only.
     */
    bool isCursorBlinkOn() const noexcept { return getRawParam (ID::cursorBlinkOn)->load (std::memory_order_relaxed) != 0.0f; }

    /**
     * @brief Sets whether the terminal component currently has keyboard focus.
     *
     * Called from `focusGained()` / `focusLost()`.  The value flows through
     * `isCursorFocused()` → `updateSnapshot()` → `snapshot.cursorFocused`
     * so the GL renderer can hide the cursor when the terminal is unfocused.
     *
     * @param focused  `true` if the component has keyboard focus.
     * @note MESSAGE THREAD only.
     */
    void setCursorFocused (bool focused) noexcept { getRawParam (ID::cursorFocused)->store (focused ? 1.0f : 0.0f, std::memory_order_relaxed); }

    /**
     * @brief Returns whether the terminal component currently has keyboard focus.
     * @return `true` if focused.
     * @note MESSAGE THREAD only.
     */
    bool isCursorFocused() const noexcept { return getRawParam (ID::cursorFocused)->load (std::memory_order_relaxed) != 0.0f; }

    /** @} */

    // =========================================================================

    /**
     * @brief Marks the cell-grid snapshot as dirty, requesting a repaint.
     *
     * Called by the reader thread after writing one or more cells to the grid,
     * and by the message thread when toggling the cursor blink phase.
     * The UI polls `consumeSnapshotDirty()` (typically from a timer or
     * `timerCallback`) to decide whether to trigger a repaint.
     *
     * When a paste echo gate is active (`pasteEchoRemaining > 0`), this call
     * is suppressed — the grid is still updated, but the UI is not notified
     * until the echo has fully arrived, producing a single visual update.
     *
     * @note Thread-safe — called from the READER THREAD (cell writes) and
     *       MESSAGE THREAD (cursor blink toggle). The atomic store with
     *       `memory_order_release` is safe from any thread.
     */
    void setSnapshotDirty() noexcept;

    /**
     * @brief Arms the paste echo gate to suppress rendering during paste echo.
     *
     * After a non-bracketed paste, the shell echoes the pasted text back
     * character by character.  The gate suppresses `setSnapshotDirty()` until
     * the expected number of echo bytes have been received, producing a single
     * visual update instead of per-character repaints.
     *
     * @param bytes  Expected echo length in bytes.
     * @note MESSAGE THREAD — called from `Session::paste()`.
     */
    void setPasteEchoGate (int bytes) noexcept;

    /**
     * @brief Decrements the paste echo gate by the number of bytes received.
     *
     * When the counter reaches zero (or below), marks the snapshot dirty and
     * clears the gate, triggering a single repaint for the entire paste.
     *
     * @param bytes  Number of bytes just received from the PTY.
     * @note READER THREAD — called from `Session::process()`.
     */
    void consumePasteEcho (int bytes) noexcept;

    /**
     * @brief Clears the paste echo gate unconditionally and marks dirty.
     *
     * Fallback for when the echo is shorter than expected (e.g. shell
     * transforms input).  Called on drain complete.
     *
     * @note READER THREAD — called from `onDrainComplete`.
     */
    void clearPasteEchoGate() noexcept;

    /**
     * @brief Enables or disables synchronized output (mode 2026).
     *
     * When enabled, `Session::process()` suppresses its post-parse dirty
     * signal.  When disabled, fires `setSnapshotDirty()` so the renderer
     * sees the completed sync block as a single atomic update.
     *
     * @param active  `true` on `ESC[?2026h`, `false` on `ESC[?2026l`.
     * @note READER THREAD — called from the private mode handler.
     */
    void setSyncOutput (bool active) noexcept;

    /**
     * @brief Returns true if synchronized output (mode 2026) is active.
     * @note Safe from any thread (relaxed atomic load).
     */
    bool isSyncOutputActive() const noexcept;

    /**
     * @brief Sets the active modal input type via AppState and triggers render rebuild.
     *
     * Delegates storage to AppState (SSOT).  Also fires setFullRebuild() and
     * setSnapshotDirty() so the renderer updates on the next frame.
     *
     * @param type  The modal to activate, or `ModalType::none` to deactivate.
     * @note MESSAGE THREAD — called from keyboard event handlers.
     */
    void setModalType (ModalType type) noexcept;

    /**
     * @brief Sets the current hint page index (0-based).
     * @param page  Zero-based index into the paginated hint list.
     * @note MESSAGE THREAD — calls storeAndFlush.
     */
    void setHintPage (int page) noexcept;

    /**
     * @brief Returns the current hint page index.
     * @return Zero-based page index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getHintPage() const noexcept;

    /**
     * @brief Sets the total number of hint pages.
     * @param total  Number of pages (ceil of link count / 26).
     * @note MESSAGE THREAD — calls storeAndFlush.
     */
    void setHintTotalPages (int total) noexcept;

    /**
     * @brief Returns the total number of hint pages.
     * @return Total page count (0 when no hints are active).
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getHintTotalPages() const noexcept;

    /**
     * @brief Stores the active hint overlay pointer and count.
     *
     * Non-owning.  Caller (InputHandler) is responsible for keeping the
     * pointed-to data valid until the overlay is cleared.  Marks the
     * snapshot dirty so Screen picks it up on the next render.
     *
     * @param data   Pointer to the first `LinkSpan`, or `nullptr` to clear.
     * @param count  Number of valid elements in @p data.
     * @note MESSAGE THREAD — called from InputHandler only.
     */
    void setHintOverlay (const LinkSpan* data, int count) noexcept;

    /**
     * @brief Returns the active hint overlay pointer.
     * @return Non-owning pointer, or `nullptr` when no overlay is active.
     * @note MESSAGE THREAD.
     */
    const LinkSpan* getHintOverlayData() const noexcept;

    /**
     * @brief Returns the number of elements in the active hint overlay.
     * @return Element count, or 0 when no overlay is active.
     * @note MESSAGE THREAD.
     */
    int getHintOverlayCount() const noexcept;

    // =========================================================================
    /** @name Selection state convenience wrappers
     *  All selection state is stored in the parameterMap and flows through
     *  the normal storeAndFlush → ValueTree pipeline.  These wrappers provide
     *  a typed API over the raw float storage.
     * @{ */

    /**
     * @brief Sets the active selection type via AppState and triggers render rebuild.
     * @param type  Integer cast of the SelectionType enum value.
     * @note MESSAGE THREAD — delegates to AppState.
     */
    void setSelectionType (int type) noexcept;

    /**
     * @brief Returns the active selection type from AppState.
     * @return Integer cast of the current SelectionType.
     * @note MESSAGE THREAD — reads from AppState ValueTree.
     */
    int getSelectionType() const noexcept;

    /**
     * @brief Sets both the selection cursor row and column atomically.
     * @param row  Scrollback-aware grid row index.
     * @param col  Zero-based column index.
     * @note MESSAGE THREAD — calls storeAndFlush for each component.
     */
    void setSelectionCursor (int row, int col) noexcept;

    /**
     * @brief Returns the selection cursor row.
     * @return Scrollback-aware grid row index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getSelectionCursorRow() const noexcept;

    /**
     * @brief Returns the selection cursor column.
     * @return Zero-based column index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getSelectionCursorCol() const noexcept;

    /**
     * @brief Sets both the selection anchor row and column atomically.
     * @param row  Scrollback-aware grid row index.
     * @param col  Zero-based column index.
     * @note MESSAGE THREAD — calls storeAndFlush for each component.
     */
    void setSelectionAnchor (int row, int col) noexcept;

    /**
     * @brief Returns the selection anchor row.
     * @return Scrollback-aware grid row index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getSelectionAnchorRow() const noexcept;

    /**
     * @brief Returns the selection anchor column.
     * @return Zero-based column index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getSelectionAnchorCol() const noexcept;

    /**
     * @brief Sets the mouse drag anchor position (absolute row, column).
     *
     * Written by the mouse-down handler.  Row is in scrollback-aware
     * coordinates (same coordinate system as selection cursor/anchor).
     *
     * @param row  Absolute grid row of the mouse-down cell.
     * @param col  Zero-based column index of the mouse-down cell.
     * @note MESSAGE THREAD — calls storeAndFlush for each component.
     */
    void setDragAnchor (int row, int col) noexcept;

    /**
     * @brief Returns the drag anchor row in absolute grid coordinates.
     * @return Scrollback-aware grid row index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getDragAnchorRow() const noexcept;

    /**
     * @brief Returns the drag anchor column.
     * @return Zero-based column index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getDragAnchorCol() const noexcept;

    /**
     * @brief Sets whether a mouse drag selection is currently active.
     *
     * Set to `true` once the Manhattan distance from the drag anchor exceeds
     * the 2-cell threshold.  Reset to `false` on mouse-up.
     *
     * @param active  `true` when a drag selection is in progress.
     * @note MESSAGE THREAD — calls storeAndFlush.
     */
    void setDragActive (bool active) noexcept;

    /**
     * @brief Returns whether a mouse drag selection is currently active.
     * @return `true` if a drag is in progress.
     * @note ANY THREAD — lock-free, noexcept.
     */
    bool isDragActive() const noexcept;

    /** @} */

    /**
     * @brief Returns the currently active modal input type from AppState.
     *
     * @return The active `ModalType`, or `ModalType::none` if no modal is active.
     * @note MESSAGE THREAD — reads from AppState ValueTree.
     */
    ModalType getModalType() const noexcept;

    /**
     * @brief Returns `true` when any modal input mode is active.
     *
     * Convenience wrapper around `getModalType() != ModalType::none`.
     * Used by `keyPressed()` as the single guard before the Action system.
     *
     * @return `true` if a modal is intercepting input.
     * @note ANY THREAD — lock-free, noexcept.
     */
    bool isModal() const noexcept;

    /**
     * @brief Requests a same-size PTY resize on the next drain completion.
     *
     * Called by the parser when mode 2026 is first activated, ensuring the
     * PTY dimensions match the actual grid before the TUI renders.
     *
     * @note READER THREAD — called from the private mode handler.
     */
    void requestSyncResize() noexcept;

    /**
     * @brief Consumes the sync resize request, returning true if one was pending.
     * @note READER THREAD — called from onDrainComplete.
     */
    bool consumeSyncResize() noexcept;

    /**
     * @brief Sets terminal dimensions from the message thread.
     *
     * Writes cols and visibleRows to the CachedValue store — ValueTree updated
     * synchronously, listeners fire on the message thread.  Also stores both
     * values into the parameterMap atomics (`ID::cols`, `ID::visibleRows`) with
     * `memory_order_release` so the reader thread can read them without locking.
     *
     * @param cols  New terminal width in character columns.
     * @param rows  New terminal height in character rows.
     * @note MESSAGE THREAD only.
     * @see getCols(), getVisibleRows()
     */
    void setDimensions (int cols, int rows) noexcept;

    /**
     * @brief Records the start of an OSC 133 command output block.
     *
     * Called when OSC 133 ; C is received.  Sets `outputBlockTop` to `row`,
     * resets `outputBlockBottom` to `row`, and sets `outputScanActive`.
     *
     * @param row  Current cursor row (zero-based visible row index).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setOutputBlockStart (int row) noexcept;

    /**
     * @brief Records the end of the current OSC 133 command output block.
     *
     * Called when OSC 133 ; D is received.  Sets `outputBlockBottom` to `row`
     * and clears `outputScanActive`.
     *
     * @param row  Current cursor row (zero-based visible row index).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setOutputBlockEnd (int row) noexcept;

    /**
     * @brief Records the cursor row at which an OSC 133 ; A prompt marker was received.
     *
     * Called when OSC 133 ; A is received.  Stores `row` so that `Grid::resize()`
     * can detect that shell integration is active and fill dead space below the
     * cursor after the terminal grows taller.
     *
     * @param row  Current cursor row (zero-based visible row index).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setPromptRow (int row) noexcept;

    /**
     * @brief Returns the cursor row of the most-recently received OSC 133 ; A marker.
     *
     * Returns -1 if no OSC 133 A has been received since session start.
     * A non-negative value indicates shell integration is active and the
     * prompt is (or was) at that row.
     *
     * @return Zero-based visible row index, or -1.
     * @note READER THREAD — lock-free, noexcept.
     */
    int getPromptRow() const noexcept;

    /**
     * @brief Extends the output block bottom boundary to `row`.
     *
     * Called on LF while `outputScanActive` is true.  No-op when scan is not active.
     *
     * @param row  Current cursor row after the line feed.
     * @note READER THREAD — lock-free, noexcept.
     */
    void extendOutputBlock (int row) noexcept;

    /**
     * @brief Returns the first row of the current or most-recent output block.
     *
     * Returns -1 if no OSC 133 C has been received.
     *
     * @return Zero-based visible row index, or -1.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    int getOutputBlockTop() const noexcept;

    /**
     * @brief Returns the last row of the current or most-recent output block.
     *
     * Returns -1 if no output rows have been recorded yet.
     *
     * @return Zero-based visible row index, or -1.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    int getOutputBlockBottom() const noexcept;

    /**
     * @brief Returns `true` when a valid completed output block exists.
     *
     * Reads `outputBlockTop`, `promptRow`, and `activeScreen` from the
     * `parameterMap` atomics.  Valid when: `outputBlockTop >= 0`,
     * `promptRow > outputBlockTop`, and `activeScreen == normal`.
     *
     * @return `true` if a valid output block is present.
     * @note MESSAGE THREAD only.
     */
    bool hasOutputBlock() const noexcept;

    /** Called by `timerCallback()` after each flush cycle. MESSAGE THREAD only. */
    std::function<void()> onFlush;

    /**
     * @brief Signals that the entire viewport must be rebuilt on the next frame.
     *
     * Called by state-change setters that alter the visual appearance of all
     * rows (scroll offset change, modal toggle, selection change, drag toggle,
     * cursor blink toggle).  The renderer consumes the flag via
     * `consumeFullRebuild()` at the start of `buildSnapshot()` and sets all
     * dirty bits to all-ones so every row is reprocessed.
     *
     * Stored as a standalone atomic (not in parameterMap) because it is a
     * transient signal, not persistent state.  Same pattern as `snapshotDirty`.
     *
     * @note Thread-safe — may be called from any thread.
     */
    void setFullRebuild() noexcept
    {
        getRawParam (ID::fullRebuild)->store (1.0f, std::memory_order_release);
    }

    /**
     * @brief Atomically tests and clears the full-rebuild flag.
     *
     * @return `true` if a full rebuild was requested since the last call.
     * @note MESSAGE THREAD — called from `buildSnapshot()`.
     */
    bool consumeFullRebuild() noexcept
    {
        return getRawParam (ID::fullRebuild)->exchange (0.0f, std::memory_order_acquire) != 0.0f;
    }

    /**
     * @brief Atomically tests and clears the snapshot-dirty flag.
     *
     * Returns `true` and clears the flag if the reader thread has written new
     * cell data since the last call.  The UI calls this once per paint cycle
     * to decide whether a repaint is needed.
     *
     * @return `true` if new cell data is available, `false` otherwise.
     * @note MESSAGE THREAD — uses `compare_exchange_strong` with
     *       `memory_order_acq_rel` to ensure the reader thread's cell writes
     *       are visible before the UI reads the grid.
     */
    bool consumeSnapshotDirty() noexcept;

    /**
     * @brief Hot-path typed getter that reads directly from the atomic store.
     *
     * Template specialisations handle `bool`, `int`, `ActiveScreen`, and any
     * other type via a `static_cast<ValueType>` from the stored `float`.
     * Equivalent to dereferencing a `std::atomic<float>*` returned by
     * `getRawParameterValue()` in APVTS.
     *
     * @tparam ValueType  Desired return type (`bool`, `int`, `ActiveScreen`, …).
     * @param  id         Parameter identifier whose atomic slot to read.
     * @return The current atomic value cast to `ValueType`.
     * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
     *       Asserts (debug only) that `id` exists in `parameterMap`.
     */
    template<typename ValueType>
    ValueType getRawValue (const juce::Identifier& id) const noexcept;

    /**
     * @brief Adds a `PARAM` child node to a ValueTree parent.
     *
     * Creates a child with type `ID::PARAM`, sets `ID::id = paramId` and
     * `ID::value = defaultValue`.  Used during construction to build the
     * ValueTree skeleton before `buildParameterMap()` populates the atomic map.
     *
     * @param parent        The ValueTree node to append the PARAM child to.
     * @param paramId       The parameter's string identifier.
     * @param defaultValue  Initial value stored in the `value` property.
     * @note Called on the MESSAGE THREAD during construction only.
     */
    static void
    addParam (juce::ValueTree& parent, const juce::Identifier& paramId, const juce::var& defaultValue) noexcept;

    /**
     * @brief Builds a compound parameter key from a parent node type and a parameter name.
     *
     * Concatenates the parent type string and the parameter ID string with an
     * underscore separator to produce a unique flat key used in `parameterMap`.
     * For example, `buildParamKey (ID::NORMAL, "cursorRow")` → `"NORMAL_cursorRow"`.
     *
     * @param parentType  The `juce::Identifier` of the parent ValueTree node
     *                    (e.g. `ID::NORMAL`, `ID::MODES`).
     * @param paramId     The parameter name string (e.g. `"cursorRow"`).
     * @return A `juce::Identifier` that uniquely identifies the parameter
     *         within the flat `parameterMap`.
     * @note Pure function — no side effects, noexcept.
     */
    static juce::Identifier buildParamKey (const juce::Identifier& parentType, const juce::String& paramId) noexcept;

    /**
     * @brief Builds the compound key for a per-screen parameter.
     *
     * Delegates to `buildParamKey (screenId, property)` where `screenId` is
     * `ID::NORMAL` or `ID::ALTERNATE` depending on `s`.
     *
     * @param s         Target screen buffer.
     * @param property  Per-screen parameter name (e.g. `ID::cursorRow`).
     * @return Compound key suitable for `parameterMap` lookup.
     * @note Pure function — noexcept.
     */
    juce::Identifier screenKey (ActiveScreen s, const juce::Identifier& property) const noexcept;

    /**
     * @brief Builds the compound key for a mode parameter.
     *
     * Delegates to `buildParamKey (ID::MODES, property)`.
     *
     * @param property  Mode parameter name (e.g. `ID::autoWrap`).
     * @return Compound key suitable for `parameterMap` lookup.
     * @note Pure function — noexcept.
     */
    juce::Identifier modeKey (const juce::Identifier& property) const noexcept;

    /**
     * @brief Signals that the caller is about to read State values.
     *
     * Ensures all pending atomic writes from the reader thread are
     * visible in the ValueTree before the caller proceeds.
     * Call from `onVBlank` before reading cursor, selection, or
     * any other State properties for rendering.
     *
     * @note MESSAGE THREAD only.
     */
    void refresh() noexcept;

    // =========================================================================
    /** @name Subscriber seqno tracking (sidecar / NEXUS PROCESS side)
     *  Manage per-subscriber last-delivered seqno inside the SUBSCRIBERS child
     *  tree of this State's root SESSION ValueTree.  All methods operate
     *  directly on the ValueTree — they are called on the message thread by
     *  the fan-out machinery in SessionFanout.cpp.
     * @{ */

    /**
     * @brief Adds a SUBSCRIBER child node for @p subscriberId with lastKnownSeqno = 0.
     *
     * No-op if the subscriber is already present.
     *
     * @param subscriberId  Stable UUID string of the connecting Channel.
     * @note MESSAGE THREAD.
     */
    void attachSubscriber (juce::StringRef subscriberId);

    /**
     * @brief Removes the SUBSCRIBER child node for @p subscriberId.
     *
     * No-op if the subscriber is not found.
     *
     * @param subscriberId  Stable UUID string of the disconnecting Channel.
     * @note MESSAGE THREAD.
     */
    void detachSubscriber (juce::StringRef subscriberId);

    /**
     * @brief Writes the last-delivered seqno for @p subscriberId.
     *
     * Called by SessionFanout after each successful fan-out for this subscriber.
     *
     * @param subscriberId  Subscriber UUID.
     * @param seqno         Most-recently delivered seqno.
     * @note MESSAGE THREAD.
     */
    void setSubscriberSeqno (juce::StringRef subscriberId, uint64_t seqno);

    /**
     * @brief Returns the last-delivered seqno for @p subscriberId.
     *
     * Returns 0 if the subscriber is not found (triggers a full delta on the
     * first fan-out cycle).
     *
     * @param subscriberId  Subscriber UUID.
     * @return Last-delivered seqno, or 0.
     * @note MESSAGE THREAD.
     */
    uint64_t getSubscriberSeqno (juce::StringRef subscriberId) const;

    /** @} */

    // =========================================================================
    /** @name Last-known seqno (reserved — no longer used in byte-forward architecture)
     *  Previously tracked the seqno of the most recently applied state snapshot.
     *  Retained for binary compatibility; not called in normal operation.
     * @{ */

    /**
     * @brief Stores the last-known grid seqno (reserved, not called in byte-forward mode).
     *
     * @param seqno  seqno value.
     * @note MESSAGE THREAD.
     */
    void setLastKnownSeqno (uint64_t seqno);

    /**
     * @brief Returns the seqno of the most recently applied StateInfo snapshot.
     *
     * Returns 0 if no snapshot has been applied yet.
     *
     * @return Last-applied seqno, or 0.
     * @note MESSAGE THREAD.
     */
    uint64_t getLastKnownSeqno() const;

    /** @} */

private:
    /**
     * @brief Timer callback — flushes dirty atomics into the ValueTree.
     *
     * Called by the JUCE timer infrastructure on the message thread at the
     * interval set in the constructor.  Checks `needsFlush`; if set, calls
     * `flush()` to copy all atomic values into the ValueTree, triggering any
     * attached `juce::ValueTree::Listener` callbacks.
     *
     * @note MESSAGE THREAD — called by juce::Timer, never call directly.
     * @see flush()
     */
    void timerCallback() override;

    /**
     * @brief Copies all dirty atomic values into the ValueTree in a single pass.
     *
     * @return `true` if `needsFlush` was set and the ValueTree was updated;
     *         `false` if nothing had changed and the flush was skipped.
     * @note MESSAGE THREAD — called from `timerCallback()` and `refresh()`.
     */
    bool flush() noexcept;

    /**
     * @brief Root `SESSION` ValueTree — the Single Source of Truth for the UI.
     *
     * All parameter values are stored as `PARAM` children (or grandchildren)
     * of this tree.  Only the message thread (timer flush + UI code) may read
     * or write this tree.  The reader thread never touches it.
     */
    juce::ValueTree state;

    /**
     * @brief Flat map from compound parameter key → owned atomic float slot.
     *
     * Keys are built by `buildParamKey()`.  Each slot is heap-allocated via
     * `unique_ptr` so that the map can be re-hashed without invalidating the
     * addresses held by external readers.  The reader thread looks up a key
     * and stores/loads the pointed-to atomic directly.  The message thread
     * iterates this map during `flush()` to copy atomics back to the ValueTree.
     *
     * @note Populated once during construction by `buildParameterMap()` and the
     *       explicit `addParam` calls for stray-atomic identifiers.
     *       Never modified after construction, so concurrent reads are safe
     *       without a lock.
     */
    StateMap<std::atomic<float>> parameterMap;

    /**
     * @brief Returns a raw pointer to the atomic float slot for `id`.
     *
     * Equivalent to dereferencing `parameterMap.at(id).get()`.  Used internally
     * by setters and getters that need direct atomic access without going through
     * the full `getRawValue<T>` / `storeAndFlush` API — for example, in the
     * stray-atomic migration methods that call `fetchAdd`, `fetchSub`, or plain
     * `store`/`load` with explicit memory orders.
     *
     * @param id  Parameter identifier.  Must exist in `parameterMap` (asserted in debug).
     * @return    Non-owning pointer to the backing `std::atomic<float>`.
     * @note READER THREAD or MESSAGE THREAD — lock-free, noexcept.
     */
    std::atomic<float>* getRawParam (const juce::Identifier& id) const noexcept
    {
        jassert (jam::Map::contains (parameterMap, id));
        return parameterMap.at (id).get();
    }

    /**
     * @brief Walks the ValueTree skeleton and registers every PARAM node in
     *        `parameterMap`, allocating an atomic slot in `parameterMap` for each.
     *
     * Called once from the constructor after the ValueTree has been fully built.
     * After this call, `parameterMap` is immutable and safe to read from any thread.
     *
     * @note MESSAGE THREAD — called during construction only.
     */
    void buildParameterMap() noexcept;

    /**
     * @brief Stores `value` into the atomic for `key` and sets `needsFlush`.
     *
     * The single write path used by all `set*()` methods.  Performs a
     * `memory_order_relaxed` store (ordering is provided by the timer's
     * acquire fence in `flush()`).
     *
     * @param key    Compound parameter key (from `buildParamKey` or `screenKey`).
     * @param value  Float representation of the new parameter value.
     * @note READER THREAD — lock-free, noexcept.
     */
    void storeAndFlush (const juce::Identifier& key, float value) noexcept;

    /**
     * @brief Maximum depth of the per-screen keyboard mode stack.
     *
     * The progressive keyboard protocol (CSI u) uses a push/pop stack for keyboard
     * enhancement flags.  Capped to prevent unbounded growth from
     * misbehaving programs.
     */
    static constexpr int maxKeyboardStackDepth { 16 };

    /**
     * @brief Flat storage for two per-screen keyboard mode stacks.
     *
     * Layout: `[screen * maxKeyboardStackDepth + index]`.
     * Allocated once in the constructor; never reallocated.
     */
    juce::HeapBlock<uint32_t> keyboardModeStack;

    /**
     * @brief Current size of each per-screen keyboard mode stack.
     *
     * Layout: `[screen]` — index 0 = normal, index 1 = alternate.
     * Allocated once in the constructor; never reallocated.
     */
    juce::HeapBlock<int> keyboardModeStackSize;

    /** @brief Non-owning pointer to the active hint label spans; nullptr when no overlay. */
    const LinkSpan* hintOverlayData  { nullptr };

    /** @brief Number of valid elements in @p hintOverlayData. */
    int             hintOverlayCount { 0 };

    // needsFlush, snapshotDirty, fullRebuild, cursorBlinkOn, cursorBlinkElapsed,
    // prevFlushedCursorRow, prevFlushedCursorCol, cursorBlinkInterval,
    // cursorBlinkEnabled, and cursorFocused are all stored in parameterMap
    // as std::atomic<float> slots (see constructor in State.cpp).

    /**
     * @brief Flat map from identifier → owned StringSlot.
     *
     * Uses `unique_ptr` for stable addressing so the map can be re-hashed
     * without invalidating the pointers accessed from the reader thread.
     * Populated once during construction and immutable thereafter — safe to
     * read from any thread without a lock.
     */
    StateMap<StringSlot> stringMap;

    /**
     * @brief Backing store for a single OSC 8 hyperlink span.
     *
     * Written exclusively by the reader thread via `storeHyperlink()`.
     * Read by the message thread in `flushHyperlinks()` after acquiring
     * the generation fence on `hyperlinksGeneration`.
     *
     * `uri` holds the null-terminated URI string.
     */
    struct HyperlinkEntry
    {
        char uri[maxStringLength] {};
        int  row      { 0 };
        int  startCol { 0 };
        int  endCol   { 0 };
    };

    /**
     * @brief Flat map from hyperlink identifier to owned HyperlinkEntry holding the URI
     *        and position data.
     *
     * Mutable — emplaced by storeHyperlink, cleared by clearHyperlinks.
     */
    StateMap<HyperlinkEntry> hyperlinkMap;

    /** @brief Cached terminal width in character columns (MESSAGE THREAD). */
    juce::CachedValue<int> cachedCols;

    /** @brief Cached terminal height in visible rows (MESSAGE THREAD). */
    juce::CachedValue<int> cachedVisibleRows;

    /**
     * @brief Flushes PARAM children that are direct children of the root SESSION node.
     *
     * Handles session-level parameters: `activeScreen`.
     * (`scrollOffset` is skipped — it is UI-owned and never in `parameterMap`.)
     * `cols` and `visibleRows` are CachedValue properties and are not processed
     * by the flush system.
     *
     * @note MESSAGE THREAD — called from `flush()` only.
     */
    void flushRootParams() noexcept;

    /**
     * @brief Flushes PARAM children of a single group node (MODES, NORMAL, ALTERNATE).
     *
     * Iterates the PARAM children of `group`, looks up each parameter's atomic
     * via `parameterMap`, and writes the current float value into the
     * ValueTree's `value` property if it has changed.
     *
     * @param group  A group-level ValueTree child of SESSION (e.g. the MODES node).
     * @note MESSAGE THREAD — called from `flush()` only.
     */
    void flushGroupParams (juce::ValueTree& group) noexcept;

    /**
     * @brief SSOT writer for all three string slots (title, cwd, foreground process).
     *
     * Copies up to `StringSlot::maxLength - 1` bytes from `src` into the
     * slot's `buffer`, null-terminates, then increments the `generation`
     * counter with a release store so that `flushStrings()` (message thread)
     * observes the completed write.  Sets `needsFlush` so the timer wakes.
     *
     * @param id      One of `ID::title`, `ID::cwd`, `ID::foregroundProcess`.
     * @param src     Pointer to the source bytes.  Need not be null-terminated.
     * @param length  Number of source bytes.
     * @note READER THREAD — lock-free, noexcept.
     */
    void writeStringSlot (const juce::Identifier& id, const char* src, int length) noexcept;

    /**
     * @brief Flushes string slots to the SESSION ValueTree as direct properties
     *        and computes the displayName.
     *
     * For each slot whose `generation` has advanced since the last flush, the
     * slot's `buffer` is snap-copied into a stack-local array.  A second
     * `generation` read after the copy detects torn writes; if detected, the
     * copy is repeated once.  The resulting string is written to the ValueTree
     * via `juce::String::fromUTF8`.  Then `displayName` is recomputed from
     * the priority: foreground process (when non-empty) → cwd leaf name.
     * The shell-filter logic lives in Session::onFlush (PID-based).
     *
     * @note MESSAGE THREAD — called from timerCallback() only.
     */
    void flushStrings() noexcept;

    /**
     * @brief Synchronises OSC 8 hyperlink spans from the reader-thread pool
     *        into the HYPERLINKS ValueTree node.
     *
     * Reads `hyperlinksGeneration` with `memory_order_acquire`.  If the
     * generation has advanced since `hyperlinksLastFlushedGeneration`, rebuilds
     * the HYPERLINKS children: removes all existing children, then creates one
     * child per entry in `hyperlinkMap`.
     *
     * Each child node has the type `ID::HYPERLINK` and carries properties:
     * - `ID::uri`      — the hyperlink URI.
     * - `ID::row`      — zero-based visible row.
     * - `ID::startCol` — start column (inclusive).
     * - `ID::endCol`   — end column (exclusive).
     *
     * @note MESSAGE THREAD — called from `timerCallback()` only.
     */
    void flushHyperlinks() noexcept;

    /**
     * @brief Advances the cursor blink counter and toggles the phase.
     *
     * Called from `timerCallback()` after `flush()`.  Accumulates elapsed
     * milliseconds and toggles `cursorBlinkOn` when `cursorBlinkInterval` is
     * reached.  Respects DECSCUSR shape parity: odd shapes blink, even shapes
     * are steady.  When blinking is disabled, forces `cursorBlinkOn = true`.
     *
     * @param elapsedMs  Milliseconds since the previous timer tick.
     * @note MESSAGE THREAD — called from `timerCallback()` only.
     */
    void tickCursorBlink (int elapsedMs) noexcept;
};

template <typename ValueType>
ValueType State::getRawValue (const juce::Identifier& id) const noexcept
{
    jassert (jam::Map::contains (parameterMap, id));
    const float raw { parameterMap.at (id).get()->load (std::memory_order_relaxed) };

    if constexpr (std::is_same_v<ValueType, bool>)
        return jam::toBool (raw);

    if constexpr (std::is_same_v<ValueType, int>)
        return jam::toInt (raw);

    if constexpr (std::is_same_v<ValueType, ActiveScreen>)
        return static_cast<ActiveScreen> (jam::toInt (raw));

    return static_cast<ValueType> (raw);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
