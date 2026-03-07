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
 *   ValueTree via `get()`, `getValue()`, and `getTreeMode()`.  It may also
 *   write `scrollOffset` directly because that parameter is owned by the UI
 *   and never touched by the reader thread.
 *
 * ### Thread ownership summary
 * | Parameter group     | Writer            | Reader (hot path)  | Reader (UI)       |
 * |---------------------|-------------------|--------------------|-------------------|
 * | activeScreen        | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | cols / visibleRows  | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | cursor (row/col/…)  | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | scroll region       | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | mode flags          | READER THREAD     | atomic getter      | ValueTree (timer) |
 * | scrollOffset        | MESSAGE THREAD    | ValueTree          | ValueTree         |
 *
 * ### ValueTree structure
 * ```
 * SESSION
 * ├── PARAM id="activeScreen"  value=<float>
 * ├── PARAM id="cols"          value=<float>
 * ├── PARAM id="visibleRows"   value=<float>
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
#include <deque>
#include <unordered_map>

#include "Cell.h"
#include "Identifier.h"

namespace Terminal
{ /*____________________________________________________________________________*/

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
    normal    = 0, ///< Primary screen buffer (default).
    alternate = 1  ///< Alternate screen buffer (full-screen apps).
};

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
 * - `get()`, `getValue()`, `getTreeMode()`: MESSAGE THREAD only.
 * - `timerCallback()`: MESSAGE THREAD only (called by juce::Timer).
 * - `flush()` and its helpers: MESSAGE THREAD only (called from timerCallback).
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
     * @brief Sets the currently active screen buffer.
     * @param s  `ActiveScreen::normal` or `ActiveScreen::alternate`.
     * @note READER THREAD — lock-free, noexcept.
     */
    void setScreen (ActiveScreen s) noexcept;

    /**
     * @brief Sets the terminal column count (width in characters).
     * @param c  Number of columns (e.g. 80 or 132).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setCols (int c) noexcept;

    /**
     * @brief Sets the number of rows visible in the terminal viewport.
     * @param r  Number of visible rows (e.g. 24).
     * @note READER THREAD — lock-free, noexcept.
     */
    void setVisibleRows (int r) noexcept;

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

    /** @} */

    // =========================================================================
    /** @name Reader-thread getters
     *  Read directly from `std::atomic<float>` slots — equivalent to calling
     *  `getRawParameterValue()` in APVTS.  Safe to call from any thread.
     *  Return the most recently stored value without waiting for a flush.
     * @{ */

    /**
     * @brief Returns the currently active screen buffer.
     * @return `ActiveScreen::normal` or `ActiveScreen::alternate`.
     * @note READER THREAD — lock-free, noexcept.
     */
    ActiveScreen getScreen() const noexcept;

    /**
     * @brief Returns the current terminal column count.
     * @return Number of columns (e.g. 80 or 132).
     * @note READER THREAD — lock-free, noexcept.
     */
    int getCols() const noexcept;

    /**
     * @brief Returns the number of rows visible in the terminal viewport.
     * @return Number of visible rows (e.g. 24).
     * @note READER THREAD — lock-free, noexcept.
     */
    int getVisibleRows() const noexcept;

    /**
     * @brief Returns the current value of a named terminal mode flag.
     * @param id  A `Terminal::ID` mode identifier (e.g. `ID::autoWrap`).
     * @return `true` if the mode is enabled, `false` otherwise.
     * @note READER THREAD — lock-free, noexcept.
     */
    bool getMode (const juce::Identifier& id) const noexcept;

    /**
     * @brief Returns the cursor row for the specified screen buffer.
     * @param s  Target screen (`normal` or `alternate`).
     * @return Zero-based row index.
     * @note READER THREAD — lock-free, noexcept.
     */
    int getCursorRow (ActiveScreen s) const noexcept;

    /**
     * @brief Returns the cursor column for the specified screen buffer.
     * @param s  Target screen (`normal` or `alternate`).
     * @return Zero-based column index.
     * @note READER THREAD — lock-free, noexcept.
     */
    int getCursorCol (ActiveScreen s) const noexcept;

    /**
     * @brief Returns whether the cursor is visible on the specified screen.
     * @param s  Target screen (`normal` or `alternate`).
     * @return `true` if the cursor is visible.
     * @note READER THREAD — lock-free, noexcept.
     */
    bool isCursorVisible (ActiveScreen s) const noexcept;

    /**
     * @brief Returns whether a wrap is pending on the specified screen.
     * @param s  Target screen (`normal` or `alternate`).
     * @return `true` if the next printable character will trigger a line wrap.
     * @note READER THREAD — lock-free, noexcept.
     */
    bool isWrapPending (ActiveScreen s) const noexcept;

    /**
     * @brief Returns the top row of the scrolling region for the specified screen.
     * @param s  Target screen (`normal` or `alternate`).
     * @return Zero-based row index of the first scrolling row.
     * @note READER THREAD — lock-free, noexcept.
     */
    int getScrollTop (ActiveScreen s) const noexcept;

    /**
     * @brief Returns the bottom row of the scrolling region for the specified screen.
     * @param s  Target screen (`normal` or `alternate`).
     * @return Zero-based row index of the last scrolling row.
     * @note READER THREAD — lock-free, noexcept.
     */
    int getScrollBottom (ActiveScreen s) const noexcept;

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
    bool getTreeMode (const juce::Identifier& id) const noexcept;

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

    /** @} */

    // =========================================================================

    /**
     * @brief Marks the cell-grid snapshot as dirty, requesting a repaint.
     *
     * Called by the reader thread after writing one or more cells to the grid.
     * The UI polls `consumeSnapshotDirty()` (typically from a timer or
     * `timerCallback`) to decide whether to trigger a repaint.
     *
     * @note READER THREAD — sets `snapshotDirty` with `memory_order_relaxed`.
     *       Lock-free, noexcept.
     */
    void setSnapshotDirty() noexcept;

    /**
     * @brief Returns a ValueTree snapshot of the cursor state for the active screen.
     *
     * Convenience accessor used by the cursor renderer to obtain all cursor
     * properties (row, col, visible, wrapPending) in a single ValueTree node
     * without querying each atomic individually.
     *
     * @return A ValueTree child node (`NORMAL` or `ALTERNATE`) containing the
     *         cursor-related PARAM children for the currently active screen.
     * @note MESSAGE THREAD only — reads from the ValueTree (post-flush values).
     */
    juce::ValueTree getCursorState() noexcept;

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

private:
    /**
     * @brief Root `SESSION` ValueTree — the Single Source of Truth for the UI.
     *
     * All parameter values are stored as `PARAM` children (or grandchildren)
     * of this tree.  Only the message thread (timer flush + UI code) may read
     * or write this tree.  The reader thread never touches it.
     */
    juce::ValueTree state;

    /**
     * @brief Stable backing store for all `std::atomic<float>` parameter slots.
     *
     * A `std::deque` is used instead of `std::vector` so that push_back never
     * invalidates existing element addresses — `parameterMap` holds raw
     * pointers into this container.  Each element corresponds to one PARAM
     * node in the ValueTree.
     */
    std::deque<std::atomic<float>> storage;

    /**
     * @brief Flat map from compound parameter key → atomic slot pointer.
     *
     * Keys are built by `buildParamKey()`.  The reader thread looks up a key
     * and stores/loads the pointed-to atomic directly.  The message thread
     * iterates this map during `flush()` to copy atomics back to the ValueTree.
     *
     * @note Populated once during construction by `buildParameterMap()`.
     *       Never modified after construction, so concurrent reads are safe
     *       without a lock.
     */
    std::unordered_map<juce::Identifier, std::atomic<float>*> parameterMap;

    /**
     * @brief Walks the ValueTree skeleton and registers every PARAM node in
     *        `parameterMap`, allocating an atomic slot in `storage` for each.
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
     * @brief Set by `storeAndFlush()` (READER THREAD) when any atomic has changed.
     *        Cleared by `flush()` (MESSAGE THREAD) after copying to the ValueTree.
     *
     * Acts as a cheap dirty flag so the timer can skip the flush pass entirely
     * when no parameters have changed since the last tick.
     */
    std::atomic<bool> needsFlush { false };

    /**
     * @brief Set by `setSnapshotDirty()` (READER THREAD) after new cell data
     *        has been written to the grid.  Cleared by `consumeSnapshotDirty()`
     *        (MESSAGE THREAD) once the UI has acknowledged the update.
     *
     * Decoupled from `needsFlush` so that a parameter-only change does not
     * force a full grid repaint, and a cell-only change does not force a
     * ValueTree flush.
     */
    std::atomic<bool> snapshotDirty { false };

    /**
     * @brief Copies all atomic parameter values into the ValueTree.
     *
     * Iterates `flushRootParams()` then `flushGroupParams()` for every group
     * child of the root.  Returns `true` if at least one value was changed in
     * the ValueTree (triggering listeners), `false` if all values were already
     * up to date.
     *
     * @return `true` if the ValueTree was modified.
     * @note MESSAGE THREAD — called from `timerCallback()` only.
     */
    bool flush() noexcept;

    /**
     * @brief Flushes PARAM children that are direct children of the root SESSION node.
     *
     * Handles session-level parameters: `activeScreen`, `cols`, `visibleRows`.
     * (`scrollOffset` is skipped — it is UI-owned and never in `parameterMap`.)
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
};

template<typename ValueType>
ValueType State::getRawValue (const juce::Identifier& id) const noexcept
{
    jassert (jreng::Map::contains (parameterMap, id));
    const float raw { parameterMap.at (id)->load (std::memory_order_relaxed) };

    if constexpr (std::is_same_v<ValueType, bool>)
    {
        return static_cast<bool> (jreng::toBool (raw));
    }
    else if constexpr (std::is_same_v<ValueType, int>)
    {
        return jreng::toInt (raw);
    }
    else if constexpr (std::is_same_v<ValueType, ActiveScreen>)
    {
        return static_cast<ActiveScreen> (jreng::toInt (raw));
    }
    else
    {
        return static_cast<ValueType> (raw);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
