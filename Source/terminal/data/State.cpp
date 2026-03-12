/**
 * @file State.cpp
 * @brief Implementation of the APVTS-style terminal parameter store.
 *
 * ## Thread model recap
 *
 * Three execution contexts touch this file:
 *
 * | Context        | Who                          | What it does here                        |
 * |----------------|------------------------------|------------------------------------------|
 * | READER THREAD  | PTY / VT parser thread       | Calls `set*()` → `storeAndFlush()`      |
 * | MESSAGE THREAD | JUCE message loop            | `timerCallback()` → `flush()`, UI reads  |
 * | TIMER          | juce::Timer (message thread) | Fires `timerCallback()` at 60 / 120 Hz  |
 *
 * The reader thread never touches the ValueTree.  The message thread never
 * touches the atomics except to read them during `flush()`.
 *
 * ## Adaptive timer rate
 *
 * `timerCallback()` uses a two-speed strategy:
 * - **Active** (120 Hz, ~8 ms): used when `flush()` returned `true`, meaning
 *   parameters changed and the UI may need another update soon.
 * - **Idle** (60 Hz, ~16 ms): used when nothing changed, reducing CPU overhead
 *   while still keeping latency within one display frame.
 *
 * @see State.h for the full architecture overview and API contract.
 */

#include "State.h"
#include "ValueTreeUtilities.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Builds a flat compound key from a parent node type and a parameter name.
 *
 * The key format is `"<parentType>_<paramId>"`, e.g. `"NORMAL_cursorRow"`.
 * This flattening lets `parameterMap` use a single `unordered_map` for all
 * parameters regardless of their depth in the ValueTree hierarchy.
 *
 * @param parentType  Identifier of the owning group node (e.g. `ID::NORMAL`).
 * @param paramId     Raw parameter name string (e.g. `"cursorRow"`).
 * @return A `juce::Identifier` suitable as a `parameterMap` key.
 * @note Pure function — no side effects, noexcept.
 */
juce::Identifier State::buildParamKey (const juce::Identifier& parentType, const juce::String& paramId) noexcept
{
    return juce::Identifier { parentType.toString() + "_" + paramId };
}

/**
 * @brief Appends a `PARAM` child node to a ValueTree parent.
 *
 * The created node has the structure:
 * @code
 *   <PARAM id="<paramId>" value="<defaultValue>" />
 * @endcode
 *
 * This is the only way parameters are added to the tree.  All calls happen
 * during construction before `buildParameterMap()` is invoked, so the tree
 * is fully populated before any atomic slots are allocated.
 *
 * @param parent        ValueTree node that will own the new PARAM child.
 * @param paramId       Identifier used as the `id` property of the PARAM node.
 * @param defaultValue  Initial value written to the `value` property.
 * @note MESSAGE THREAD — called during construction only.  `nullptr` is passed
 *       as the UndoManager because construction-time mutations are not undoable.
 */
/*static*/ void State::addParam (juce::ValueTree& parent, const juce::Identifier& paramId, const juce::var& defaultValue) noexcept
{
    juce::ValueTree param { ID::PARAM };
    param.setProperty (ID::id, paramId.toString(), nullptr);
    param.setProperty (ID::value, defaultValue, nullptr);
    parent.appendChild (param, nullptr);
}

/**
 * @brief Builds a fully-populated screen node for either the NORMAL or ALTERNATE buffer.
 *
 * Both screen buffers share the same set of per-screen parameters.  This
 * factory function avoids duplicating the `addParam` call list.  The returned
 * node is appended to the root SESSION tree by the constructor.
 *
 * Parameters added:
 * - `cursorRow`    — zero-based cursor row within the visible area (default 0).
 * - `cursorCol`    — zero-based cursor column (default 0).
 * - `cursorVisible`— cursor visibility flag; 1.0 = visible (default on).
 * - `wrapPending`  — pending-wrap flag; set when cursor is at the right margin.
 * - `scrollTop`    — top row of the DECSTBM scrolling region (default 0).
 * - `scrollBottom` — bottom row of the DECSTBM scrolling region (default 0).
 *
 * @param nodeId  The `juce::Identifier` for the node type (`ID::NORMAL` or
 *                `ID::ALTERNATE`).
 * @return A fully-populated `juce::ValueTree` node ready to be appended.
 * @note MESSAGE THREAD — called during construction only.
 */
static juce::ValueTree buildScreenNode (const juce::Identifier& nodeId)
{
    juce::ValueTree node { nodeId };
    State::addParam (node, ID::cursorRow, 0.0f);
    State::addParam (node, ID::cursorCol, 0.0f);
    State::addParam (node, ID::cursorVisible, 1.0f);
    State::addParam (node, ID::wrapPending, 0.0f);
    State::addParam (node, ID::scrollTop, 0.0f);
    State::addParam (node, ID::scrollBottom, 0.0f);
    return node;
}

/**
 * @brief Constructs the State, builds the ValueTree skeleton, populates the
 *        atomic parameter map, and starts the flush timer at 60 Hz.
 *
 * ## Construction sequence
 *
 * 1. Root SESSION node is created and session-level PARAMs are appended:
 *    `activeScreen`, `cols`, `visibleRows`, `scrollOffset`.
 * 2. A MODES group node is built and all terminal mode flags are appended
 *    with their VT-spec defaults (most off, `autoWrap` and `cursorVisible` on).
 * 3. Two screen nodes (`NORMAL`, `ALTERNATE`) are built via `buildScreenNode()`
 *    and appended to SESSION.
 * 4. `buildParameterMap()` walks the completed tree and allocates one
 *    `std::atomic<float>` per PARAM node (except `scrollOffset`, which is
 *    UI-owned and stored as an integer in the ValueTree only).
 * 5. The JUCE timer is started at 60 Hz.  `timerCallback()` will adapt the
 *    rate to 120 Hz when parameters are actively changing.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread so that
 *       `juce::Timer::startTimerHz()` registers with the correct event loop.
 */
State::State()
    : state (ID::SESSION)
{
    addParam (state, ID::activeScreen, 0.0f);
    addParam (state, ID::cols, 0.0f);
    addParam (state, ID::visibleRows, 0.0f);
    addParam (state, ID::scrollOffset, 0);

    // MODES
    juce::ValueTree modesNode { ID::MODES };
    addParam (modesNode, ID::originMode, 0.0f);
    addParam (modesNode, ID::autoWrap, 1.0f);
    addParam (modesNode, ID::applicationCursor, 0.0f);
    addParam (modesNode, ID::bracketedPaste, 0.0f);
    addParam (modesNode, ID::insertMode, 0.0f);
    addParam (modesNode, ID::mouseTracking, 0.0f);
    addParam (modesNode, ID::mouseMotionTracking, 0.0f);
    addParam (modesNode, ID::mouseAllTracking, 0.0f);
    addParam (modesNode, ID::mouseSgr, 0.0f);
    addParam (modesNode, ID::focusEvents, 0.0f);
    addParam (modesNode, ID::applicationKeypad, 0.0f);
    addParam (modesNode, ID::cursorVisible, 1.0f);
    addParam (modesNode, ID::reverseVideo, 0.0f);
    state.appendChild (modesNode, nullptr);

    state.appendChild (buildScreenNode (ID::NORMAL), nullptr);
    state.appendChild (buildScreenNode (ID::ALTERNATE), nullptr);

    buildParameterMap();

    stringStorage.emplace_back (nullptr);
    stringMap[ID::title] = &stringStorage.back();

    stringStorage.emplace_back (nullptr);
    stringMap[ID::cwd] = &stringStorage.back();

    stringStorage.emplace_back (nullptr);
    stringMap[ID::foregroundProcess] = &stringStorage.back();

    startTimerHz (60);
}

/**
 * @brief Stops the flush timer and destroys the State.
 *
 * `stopTimer()` must be called before destruction to prevent `timerCallback()`
 * from firing after the `parameterMap` and `storage` members have been
 * destroyed.  JUCE's `Timer` destructor does NOT stop the timer automatically.
 *
 * @note MESSAGE THREAD — must be destroyed on the message thread.
 */
State::~State()
{
    stopTimer();
}

// --- Reader thread ---

/**
 * @brief Stores a float value into the atomic slot for `key` and marks the
 *        state as needing a flush.
 *
 * This is the single write path shared by all `set*()` methods.  The store
 * uses `memory_order_relaxed` because ordering between individual parameter
 * writes is not required — the `needsFlush` release store provides the
 * necessary happens-before edge so that `flush()` (which acquires
 * `needsFlush`) sees all preceding relaxed stores.
 *
 * @param key  Compound parameter key (from `buildParamKey`, `screenKey`, or
 *             `modeKey`).  Must exist in `parameterMap`.
 * @param v    New float value to store.
 * @note READER THREAD — lock-free, noexcept.
 */
void State::storeAndFlush (const juce::Identifier& key, float v) noexcept
{
    parameterMap.at (key)->store (v, std::memory_order_relaxed);
    needsFlush.store (true, std::memory_order_release);
}

/** @note READER THREAD — delegates to `storeAndFlush (ID::activeScreen, …)`. */
void State::setScreen (ActiveScreen s) noexcept              { storeAndFlush (ID::activeScreen, static_cast<float> (s)); }
/** @note READER THREAD — delegates to `storeAndFlush (ID::cols, …)`. */
void State::setCols (int c) noexcept                         { storeAndFlush (ID::cols, static_cast<float> (c)); }
/** @note READER THREAD — delegates to `storeAndFlush (ID::visibleRows, …)`. */
void State::setVisibleRows (int r) noexcept                  { storeAndFlush (ID::visibleRows, static_cast<float> (r)); }
/** @note READER THREAD — key is built via `modeKey (id)`. */
void State::setMode (const juce::Identifier& id, bool v) noexcept { storeAndFlush (modeKey (id), static_cast<float> (v)); }

/** @note READER THREAD — key is built via `screenKey (s, ID::cursorRow)`. */
void State::setCursorRow (ActiveScreen s, int row) noexcept      { storeAndFlush (screenKey (s, ID::cursorRow), static_cast<float> (row)); }
/** @note READER THREAD — key is built via `screenKey (s, ID::cursorCol)`. */
void State::setCursorCol (ActiveScreen s, int col) noexcept      { storeAndFlush (screenKey (s, ID::cursorCol), static_cast<float> (col)); }
/** @note READER THREAD — key is built via `screenKey (s, ID::cursorVisible)`. */
void State::setCursorVisible (ActiveScreen s, bool v) noexcept   { storeAndFlush (screenKey (s, ID::cursorVisible), static_cast<float> (v)); }
/** @note READER THREAD — key is built via `screenKey (s, ID::wrapPending)`. */
void State::setWrapPending (ActiveScreen s, bool v) noexcept     { storeAndFlush (screenKey (s, ID::wrapPending), static_cast<float> (v)); }
/** @note READER THREAD — key is built via `screenKey (s, ID::scrollTop)`. */
void State::setScrollTop (ActiveScreen s, int top) noexcept      { storeAndFlush (screenKey (s, ID::scrollTop), static_cast<float> (top)); }
/** @note READER THREAD — key is built via `screenKey (s, ID::scrollBottom)`. */
void State::setScrollBottom (ActiveScreen s, int bottom) noexcept { storeAndFlush (screenKey (s, ID::scrollBottom), static_cast<float> (bottom)); }

void State::setTitle (const char* ptr) noexcept
{
    // READER THREAD
    stringMap.at (ID::title)->store (ptr, std::memory_order_relaxed);
    needsFlush.store (true, std::memory_order_release);
}

void State::setCwd (const char* ptr) noexcept
{
    // READER THREAD
    stringMap.at (ID::cwd)->store (ptr, std::memory_order_relaxed);
    needsFlush.store (true, std::memory_order_release);
}

void State::setForegroundProcess (const char* ptr) noexcept
{
    // READER THREAD
    stringMap.at (ID::foregroundProcess)->store (ptr, std::memory_order_relaxed);
    needsFlush.store (true, std::memory_order_release);
}

/**
 * @brief Signals that the cell-grid snapshot has new data and a repaint is needed.
 *
 * The reader thread calls this after writing one or more cells to the grid
 * buffer.  The flag is separate from `needsFlush` so that a cell-only update
 * does not force a full parameter flush, and a parameter-only update does not
 * force a grid repaint.
 *
 * @note READER THREAD — `memory_order_release` store so that the message
 *       thread's `memory_order_acquire` exchange in `consumeSnapshotDirty()`
 *       sees all preceding cell writes.
 */
// READER THREAD
void State::setSnapshotDirty() noexcept
{
    snapshotDirty.store (true, std::memory_order_release);
}

/**
 * @brief Atomically tests and clears the snapshot-dirty flag.
 *
 * Uses `exchange` rather than `load + store` to avoid a race where two
 * callers both see `true` and both trigger a repaint.  The `memory_order_acquire`
 * ensures that all cell writes made by the reader thread before its
 * `memory_order_release` store in `setSnapshotDirty()` are visible to the
 * caller after this returns `true`.
 *
 * @return `true` if new cell data is available since the last call.
 * @note MESSAGE THREAD — called from the UI timer or paint cycle.
 */
// MESSAGE THREAD
bool State::consumeSnapshotDirty() noexcept
{
    return snapshotDirty.exchange (false, std::memory_order_acquire);
}

// --- Reader thread getters (read from parameterMap) ---

/**
 * @brief Returns the currently active screen buffer.
 *
 * Reads the atomic slot for `ID::activeScreen` via `getRawValue<ActiveScreen>`.
 *
 * @return `ActiveScreen::normal` or `ActiveScreen::alternate`.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
ActiveScreen State::getScreen() const noexcept
{
    return getRawValue<ActiveScreen> (ID::activeScreen);
}

/**
 * @brief Returns the current terminal column count.
 * @return Number of columns (e.g. 80 or 132).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getCols() const noexcept
{
    return getRawValue<int> (ID::cols);
}

/**
 * @brief Returns the number of rows visible in the terminal viewport.
 * @return Number of visible rows (e.g. 24).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getVisibleRows() const noexcept
{
    return getRawValue<int> (ID::visibleRows);
}

/**
 * @brief Returns the current value of a named terminal mode flag.
 *
 * The key is built via `modeKey (id)` which prepends `"MODES_"` to the
 * identifier string, matching the key format used by `setMode()`.
 *
 * @param id  A `Terminal::ID` mode identifier (e.g. `ID::autoWrap`).
 * @return `true` if the mode is enabled, `false` otherwise.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
bool State::getMode (const juce::Identifier& id) const noexcept
{
    return getRawValue<bool> (modeKey (id));
}

/**
 * @brief Returns the cursor row for the specified screen buffer.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based row index within the visible area.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getCursorRow (ActiveScreen s) const noexcept
{
    return getRawValue<int> (screenKey (s, ID::cursorRow));
}

/**
 * @brief Returns the cursor column for the specified screen buffer.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based column index.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getCursorCol (ActiveScreen s) const noexcept
{
    return getRawValue<int> (screenKey (s, ID::cursorCol));
}

/**
 * @brief Returns whether the cursor is visible on the specified screen.
 * @param s  Target screen (`normal` or `alternate`).
 * @return `true` if the cursor is visible.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
bool State::isCursorVisible (ActiveScreen s) const noexcept
{
    return getRawValue<bool> (screenKey (s, ID::cursorVisible));
}

/**
 * @brief Returns whether a wrap is pending on the specified screen.
 * @param s  Target screen (`normal` or `alternate`).
 * @return `true` if the next printable character will trigger a line wrap.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
bool State::isWrapPending (ActiveScreen s) const noexcept
{
    return getRawValue<bool> (screenKey (s, ID::wrapPending));
}

/**
 * @brief Returns the top row of the scrolling region for the specified screen.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based row index of the first scrolling row (DECSTBM top).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getScrollTop (ActiveScreen s) const noexcept
{
    return getRawValue<int> (screenKey (s, ID::scrollTop));
}

/**
 * @brief Returns the bottom row of the scrolling region for the specified screen.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based row index of the last scrolling row (DECSTBM bottom).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getScrollBottom (ActiveScreen s) const noexcept
{
    return getRawValue<int> (screenKey (s, ID::scrollBottom));
}

// --- Message thread (read from ValueTree, the SSOT) ---

/**
 * @brief Returns the root SESSION ValueTree.
 *
 * UI components attach `juce::ValueTree::Listener` to this tree to receive
 * change notifications after each timer flush.  The returned tree is a
 * reference-counted handle — it stays valid as long as `State` is alive.
 *
 * @return The root `SESSION` ValueTree node.
 * @note MESSAGE THREAD only.
 */
juce::ValueTree State::get() noexcept
{
    return state;
}

/**
 * @brief Returns a live `juce::Value` bound to a specific parameter in the tree.
 *
 * The returned `juce::Value` reflects the `value` property of the PARAM child
 * whose `id` property matches `paramId`.  UI controls can attach to it
 * directly via `juce::Value::addListener()` or `juce::Value::referTo()`.
 *
 * @param paramId  The parameter identifier to look up (e.g. `ID::cols`).
 * @return A live `juce::Value` for the parameter's `value` property.
 * @note MESSAGE THREAD only.  Writing through the returned Value does NOT
 *       update the backing atomic; use only for UI-owned parameters such as
 *       `scrollOffset`.
 */
juce::Value State::getValue (const juce::Identifier& paramId)
{
    return ValueTreeUtilities::getValueFromChildWithID (state, paramId);
}

/**
 * @brief Reads a mode flag directly from the ValueTree (post-flush value).
 *
 * Navigates to the MODES child of SESSION, finds the PARAM whose `id`
 * matches `id`, and returns its `value` property cast to `bool`.
 *
 * Unlike `getMode()`, which reads the atomic (always current), this method
 * reads the ValueTree and therefore reflects the state as of the last timer
 * flush.  Use this inside `juce::ValueTree::Listener` callbacks where
 * consistency with other ValueTree properties matters more than recency.
 *
 * @param id  A `Terminal::ID` mode identifier (e.g. `ID::autoWrap`).
 * @return `true` if the mode is enabled in the ValueTree, `false` otherwise.
 *         Returns `false` if the MODES node or the parameter is not found.
 * @note MESSAGE THREAD only.
 */
bool State::getTreeMode (const juce::Identifier& id) const noexcept
{
    auto modesNode { state.getChildWithName (ID::MODES) };
    auto param { ValueTreeUtilities::getChildWithID (modesNode, id.toString()) };
    bool result { false };

    if (param.isValid())
    {
        result = static_cast<bool> (param.getProperty (ID::value));
    }

    return result;
}

/**
 * @brief Sets the vertical scroll offset (UI-owned parameter).
 *
 * `scrollOffset` is the only parameter written exclusively by the message
 * thread.  It represents how many lines the viewport has been scrolled back
 * into the scrollback buffer (0 = live view at the bottom).  The reader
 * thread never touches this parameter, so it is written directly to the
 * ValueTree without going through the atomic / flush path.
 *
 * @param offset  Number of lines scrolled back (0 = live view).
 * @note MESSAGE THREAD only.  Writes directly to the ValueTree; no atomic
 *       involved.  Triggers any attached `juce::ValueTree::Listener` callbacks
 *       synchronously on the message thread.
 */
void State::setScrollOffset (int offset) noexcept
{
    // MESSAGE THREAD
    auto param { ValueTreeUtilities::getChildWithID (state, ID::scrollOffset.toString()) };

    if (param.isValid())
    {
        param.setProperty (ID::value, offset, nullptr);
    }
}

/**
 * @brief Returns the current vertical scroll offset from the ValueTree.
 *
 * Reads the `value` property of the `scrollOffset` PARAM child directly from
 * the ValueTree.  Returns 0 if the parameter node is not found.
 *
 * @return Number of lines scrolled back (0 = live view at the bottom).
 * @note MESSAGE THREAD only.
 */
int State::getScrollOffset() const noexcept
{
    // MESSAGE THREAD
    auto param { ValueTreeUtilities::getChildWithID (state, ID::scrollOffset.toString()) };
    int result { 0 };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (ID::value));
    }

    return result;
}

/**
 * @brief Returns a ValueTree snapshot of the cursor state for the active screen.
 *
 * Reads `getScreen()` from the atomic (always current) to determine which
 * screen is active, then returns the corresponding NORMAL or ALTERNATE child
 * node from the ValueTree.  The returned node contains the post-flush cursor
 * PARAM children (`cursorRow`, `cursorCol`, `cursorVisible`, `wrapPending`).
 *
 * Used by `CursorComponent` to attach a single `ValueTree::Listener` to the
 * active screen node rather than listening to the entire SESSION tree.
 *
 * @return The NORMAL or ALTERNATE ValueTree child for the currently active screen.
 * @note MESSAGE THREAD only — reads from the ValueTree (post-flush values).
 */
// Cursor state for external VT listeners (CursorComponent)
juce::ValueTree State::getCursorState() noexcept
{
    const auto scr { getScreen() };
    return state.getChildWithName (scr == normal ? ID::NORMAL : ID::ALTERNATE);
}

/**
 * @brief JUCE timer callback — flushes dirty atomics into the ValueTree.
 *
 * ## Adaptive rate mechanism
 *
 * The timer uses two speeds to balance UI responsiveness against CPU overhead:
 *
 * - **120 Hz (~8 ms)** when `flush()` returns `true`: parameters changed this
 *   tick, so the next change is likely imminent (e.g. during rapid VT output).
 *   Running faster reduces the latency between a VT escape and the UI update.
 *
 * - **60 Hz (~16 ms)** when `flush()` returns `false`: nothing changed, so
 *   the terminal is idle.  60 Hz is sufficient to catch the next change within
 *   one display frame without burning CPU on empty flushes.
 *
 * `startTimer()` is called with an explicit millisecond value (rather than
 * `startTimerHz`) to avoid floating-point rounding on every tick.
 *
 * @note TIMER / MESSAGE THREAD — called by `juce::Timer` infrastructure.
 *       Never call directly.
 */
// Timer
void State::timerCallback()
{
    // MESSAGE THREAD
    static constexpr int flushHz { 120 };
    static constexpr int idleHz { 60 };
    const bool anythingUpdated { flush() };
    flushStrings();
    startTimer (anythingUpdated ? 1000 / flushHz : 1000 / idleHz);
}

/**
 * @brief Walks the ValueTree skeleton and registers every PARAM node in
 *        `parameterMap`, allocating one atomic slot per parameter.
 *
 * ## Key-building rules
 *
 * - **Root-level PARAMs** (direct children of SESSION): key = raw `id` string
 *   (e.g. `"activeScreen"`, `"cols"`).
 * - **Group-level PARAMs** (children of MODES, NORMAL, ALTERNATE): key =
 *   `buildParamKey (parentType, id)` (e.g. `"NORMAL_cursorRow"`).
 *
 * `scrollOffset` is stored as an integer `juce::var` (not a `double`), so the
 * `isDouble()` guard skips it automatically — it has no atomic backing and is
 * managed exclusively by the message thread via the ValueTree.
 *
 * `std::deque` is used for `storage` so that `emplace_back` never invalidates
 * existing element addresses.  `parameterMap` holds raw pointers into
 * `storage`, which would dangle if `std::vector` reallocated.
 *
 * @note MESSAGE THREAD — called once from the constructor.  After this call
 *       `parameterMap` is immutable and safe to read from any thread without
 *       a lock.
 */
void State::buildParameterMap() noexcept
{
    ValueTreeUtilities::applyFunctionRecursively (state, [this] (const juce::ValueTree& node) -> bool
    {
        if (node.getType() == ID::PARAM and node.getProperty (ID::value).isDouble())
        {
            const auto paramId { node.getProperty (ID::id).toString() };
            const auto parent { node.getParent() };
            const bool isRoot { parent.getType() == ID::SESSION };
            const juce::Identifier key { isRoot ? juce::Identifier { paramId } : buildParamKey (parent.getType(), paramId) };
            storage.emplace_back (static_cast<float> (node.getProperty (ID::value)));
            parameterMap[key] = &storage.back();
        }

        return false;
    });
}

/**
 * @brief Builds the compound key for a per-screen parameter.
 *
 * Selects `ID::NORMAL` or `ID::ALTERNATE` based on `s`, then delegates to
 * `buildParamKey (screenId, property)`.  The resulting key matches the format
 * used when the parameter was registered in `buildParameterMap()`.
 *
 * @param s         Target screen buffer.
 * @param property  Per-screen parameter identifier (e.g. `ID::cursorRow`).
 * @return Compound key (e.g. `"NORMAL_cursorRow"`) for `parameterMap` lookup.
 * @note Pure function — noexcept.
 */
juce::Identifier State::screenKey (ActiveScreen s, const juce::Identifier& property) const noexcept
{
    const auto& parent { s == normal ? ID::NORMAL : ID::ALTERNATE };
    return buildParamKey (parent, property.toString());
}

/**
 * @brief Builds the compound key for a mode parameter.
 *
 * Delegates to `buildParamKey (ID::MODES, property)`, producing keys of the
 * form `"MODES_autoWrap"`, `"MODES_originMode"`, etc.
 *
 * @param property  Mode parameter identifier (e.g. `ID::autoWrap`).
 * @return Compound key (e.g. `"MODES_autoWrap"`) for `parameterMap` lookup.
 * @note Pure function — noexcept.
 */
juce::Identifier State::modeKey (const juce::Identifier& property) const noexcept
{
    return buildParamKey (ID::MODES, property.toString());
}

void State::flushStrings() noexcept
{
    // MESSAGE THREAD
    for (const auto& [id, slot] : stringMap)
    {
        const char* ptr { slot->load (std::memory_order_relaxed) };

        if (ptr != nullptr)
        {
            state.setProperty (id, juce::String::fromUTF8 (ptr), nullptr);
        }
    }

    const auto foreground { state.getProperty (ID::foregroundProcess).toString() };
    const auto cwdPath { state.getProperty (ID::cwd).toString() };
    const auto shell { state.getProperty (ID::shellProgram).toString() };
    juce::String name;

    if (foreground.isNotEmpty() and foreground != shell)
    {
        name = foreground;
    }
    else if (cwdPath.isNotEmpty())
    {
        name = juce::File (cwdPath).getFileName();
    }

    if (name.isNotEmpty())
    {
        state.setProperty (ID::displayName, name, nullptr);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
