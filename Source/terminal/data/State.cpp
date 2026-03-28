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
#include <JuceHeader.h>
#include "../../config/Config.h"

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
/*static*/ void
State::addParam (juce::ValueTree& parent, const juce::Identifier& paramId, const juce::var& defaultValue) noexcept
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
    State::addParam (node, ID::cursorShape, 0.0f);
    State::addParam (node, ID::cursorColorR, -1.0f);
    State::addParam (node, ID::cursorColorG, -1.0f);
    State::addParam (node, ID::cursorColorB, -1.0f);
    State::addParam (node, ID::keyboardFlags, 0.0f);
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
    addParam (state, ID::modalType, 0.0f);
    addParam (state, ID::hintPage, 0.0f);
    addParam (state, ID::hintTotalPages, 0.0f);
    addParam (state, ID::selectionType, 0.0f);
    addParam (state, ID::selectionCursorRow, 0.0f);
    addParam (state, ID::selectionCursorCol, 0.0f);
    addParam (state, ID::selectionAnchorRow, 0.0f);
    addParam (state, ID::selectionAnchorCol, 0.0f);
    addParam (state, ID::dragAnchorRow, 0.0f);
    addParam (state, ID::dragAnchorCol, 0.0f);
    addParam (state, ID::dragActive, 0.0f);

    // Transient session parameters (formerly stray atomics).
    addParam (state, ID::pasteEchoRemaining, 0.0f);
    addParam (state, ID::syncOutputActive, 0.0f);
    addParam (state, ID::syncResizePending, 0.0f);
    addParam (state, ID::outputBlockTop, -1.0f);
    addParam (state, ID::outputBlockBottom, -1.0f);
    addParam (state, ID::outputScanActive, 0.0f);
    addParam (state, ID::promptRow, -1.0f);
    // Flush and repaint signals.
    addParam (state, ID::needsFlush, 0.0f);
    addParam (state, ID::snapshotDirty, 0.0f);
    addParam (state, ID::fullRebuild, 0.0f);

    // Cursor blink state.
    addParam (state, ID::cursorBlinkOn, 1.0f);
    addParam (state, ID::cursorBlinkElapsed, 0.0f);
    addParam (state, ID::prevFlushedCursorRow, 0.0f);
    addParam (state, ID::prevFlushedCursorCol, 0.0f);
    addParam (state, ID::cursorBlinkInterval, 500.0f);
    addParam (state, ID::cursorBlinkEnabled, 1.0f);
    addParam (state, ID::cursorFocused, 0.0f);

    // String slot generation counters.
    addParam (state, ID::titleGeneration, 0.0f);
    addParam (state, ID::cwdGeneration, 0.0f);
    addParam (state, ID::foregroundProcessGeneration, 0.0f);
    addParam (state, ID::titleLastFlushedGeneration, 0.0f);
    addParam (state, ID::cwdLastFlushedGeneration, 0.0f);
    addParam (state, ID::foregroundProcessLastFlushedGeneration, 0.0f);

    // Hyperlink generation counters.
    addParam (state, ID::hyperlinksGeneration, 0.0f);
    addParam (state, ID::hyperlinksLastFlushedGeneration, 0.0f);

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
    addParam (modesNode, ID::win32InputMode, 0.0f);
    state.appendChild (modesNode, nullptr);

    state.appendChild (buildScreenNode (ID::NORMAL), nullptr);
    state.appendChild (buildScreenNode (ID::ALTERNATE), nullptr);

    // HYPERLINKS container node — children are created/removed by flushHyperlinks().
    state.appendChild (juce::ValueTree { ID::HYPERLINKS }, nullptr);

    buildParameterMap();

    keyboardModeStack.allocate (2 * maxKeyboardStackDepth, true);
    keyboardModeStackSize.allocate (2, true);

    stringMap[ID::title] = std::make_unique<StringSlot>();
    stringMap[ID::cwd] = std::make_unique<StringSlot>();
    stringMap[ID::foregroundProcess] = std::make_unique<StringSlot>();

    getRawParam (ID::cursorBlinkEnabled)
        ->store (Config::getContext()->getBool (Config::Key::cursorBlink) ? 1.0f : 0.0f, std::memory_order_relaxed);
    getRawParam (ID::cursorBlinkInterval)
        ->store (static_cast<float> (Config::getContext()->getInt (Config::Key::cursorBlinkInterval)),
                 std::memory_order_relaxed);

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
State::~State() { stopTimer(); }

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
    parameterMap.at (key).get()->store (v, std::memory_order_relaxed);
    getRawParam (ID::needsFlush)->store (1.0f, std::memory_order_release);
}

/** @note READER THREAD — delegates to `storeAndFlush (ID::activeScreen, …)`. */
void State::setScreen (ActiveScreen s) noexcept { storeAndFlush (ID::activeScreen, static_cast<float> (s)); }
/** @note READER THREAD — delegates to `storeAndFlush (ID::cols, …)`. */
void State::setCols (int c) noexcept { storeAndFlush (ID::cols, static_cast<float> (c)); }
/** @note READER THREAD — delegates to `storeAndFlush (ID::visibleRows, …)`. */
void State::setVisibleRows (int r) noexcept { storeAndFlush (ID::visibleRows, static_cast<float> (r)); }
/** @note READER THREAD — key is built via `modeKey (id)`. */
void State::setMode (const juce::Identifier& id, bool v) noexcept
{
    storeAndFlush (modeKey (id), static_cast<float> (v));
}

/** @note READER THREAD — key is built via `screenKey (s, ID::cursorRow)`. */
void State::setCursorRow (ActiveScreen s, int row) noexcept
{
    storeAndFlush (screenKey (s, ID::cursorRow), static_cast<float> (row));
    setSnapshotDirty();
}
/** @note READER THREAD — key is built via `screenKey (s, ID::cursorCol)`. */
void State::setCursorCol (ActiveScreen s, int col) noexcept
{
    storeAndFlush (screenKey (s, ID::cursorCol), static_cast<float> (col));
    setSnapshotDirty();
}
/** @note READER THREAD — key is built via `screenKey (s, ID::cursorVisible)`. */
void State::setCursorVisible (ActiveScreen s, bool v) noexcept
{
    storeAndFlush (screenKey (s, ID::cursorVisible), static_cast<float> (v));
}
/** @note READER THREAD — key is built via `screenKey (s, ID::wrapPending)`. */
void State::setWrapPending (ActiveScreen s, bool v) noexcept
{
    storeAndFlush (screenKey (s, ID::wrapPending), static_cast<float> (v));
}
/** @note READER THREAD — key is built via `screenKey (s, ID::scrollTop)`. */
void State::setScrollTop (ActiveScreen s, int top) noexcept
{
    storeAndFlush (screenKey (s, ID::scrollTop), static_cast<float> (top));
}
/** @note READER THREAD — key is built via `screenKey (s, ID::scrollBottom)`. */
void State::setScrollBottom (ActiveScreen s, int bottom) noexcept
{
    storeAndFlush (screenKey (s, ID::scrollBottom), static_cast<float> (bottom));
}

void State::setCursorShape (ActiveScreen s, int shape) noexcept
{
    storeAndFlush (screenKey (s, ID::cursorShape), static_cast<float> (shape));
}

void State::setCursorColor (ActiveScreen s, int r, int g, int b) noexcept
{
    storeAndFlush (screenKey (s, ID::cursorColorR), static_cast<float> (r));
    storeAndFlush (screenKey (s, ID::cursorColorG), static_cast<float> (g));
    storeAndFlush (screenKey (s, ID::cursorColorB), static_cast<float> (b));
}

void State::resetCursorColor (ActiveScreen s) noexcept
{
    storeAndFlush (screenKey (s, ID::cursorColorR), -1.0f);
    storeAndFlush (screenKey (s, ID::cursorColorG), -1.0f);
    storeAndFlush (screenKey (s, ID::cursorColorB), -1.0f);
}

void State::pushKeyboardMode (ActiveScreen s, uint32_t flags) noexcept
{
    const int base { static_cast<int> (s) * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize[static_cast<int> (s)] };

    if (size >= maxKeyboardStackDepth)
    {
        // Evict oldest entry by shifting the stack down
        for (int i { 0 }; i < maxKeyboardStackDepth - 1; ++i)
        {
            keyboardModeStack[base + i] = keyboardModeStack[base + i + 1];
        }

        --size;
    }

    keyboardModeStack[base + size] = flags;
    ++size;
    storeAndFlush (screenKey (s, ID::keyboardFlags), static_cast<float> (flags));
}

void State::popKeyboardMode (ActiveScreen s, int count) noexcept
{
    auto& size { keyboardModeStackSize[static_cast<int> (s)] };
    const int toPop { std::min (count, size) };
    size -= toPop;

    const int base { static_cast<int> (s) * maxKeyboardStackDepth };
    const uint32_t current { size > 0 ? keyboardModeStack[base + size - 1] : 0u };
    storeAndFlush (screenKey (s, ID::keyboardFlags), static_cast<float> (current));
}

void State::setKeyboardMode (ActiveScreen s, uint32_t flags, int mode) noexcept
{
    const int base { static_cast<int> (s) * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize[static_cast<int> (s)] };

    if (size == 0)
    {
        keyboardModeStack[base] = 0u;
        size = 1;
    }

    auto& top { keyboardModeStack[base + size - 1] };

    if (mode == 1)
    {
        top = flags;
    }
    else if (mode == 2)
    {
        top |= flags;
    }
    else if (mode == 3)
    {
        top &= ~flags;
    }

    storeAndFlush (screenKey (s, ID::keyboardFlags), static_cast<float> (top));
}

uint32_t State::getKeyboardFlags (ActiveScreen s) const noexcept
{
    return static_cast<uint32_t> (getRawValue<int> (screenKey (s, ID::keyboardFlags)));
}

void State::resetKeyboardMode (ActiveScreen s) noexcept
{
    keyboardModeStackSize[static_cast<int> (s)] = 0;
    storeAndFlush (screenKey (s, ID::keyboardFlags), 0.0f);
}

/** @note READER THREAD — sets outputBlockTop / outputBlockBottom and activates scan. */
void State::setOutputBlockStart (int row) noexcept
{
    storeAndFlush (ID::outputBlockTop, static_cast<float> (row));
    storeAndFlush (ID::outputBlockBottom, static_cast<float> (row));
    storeAndFlush (ID::outputScanActive, 1.0f);
}

/** @note READER THREAD — records final row and deactivates scan. */
void State::setOutputBlockEnd (int row) noexcept
{
    storeAndFlush (ID::outputBlockBottom, static_cast<float> (row));
    storeAndFlush (ID::outputScanActive, 0.0f);
}

/** @note READER THREAD — extends the output block bottom while scan is active. */
void State::extendOutputBlock (int row) noexcept
{
    if (getRawParam (ID::outputScanActive)->load (std::memory_order_relaxed) != 0.0f)
    {
        storeAndFlush (ID::outputBlockBottom, static_cast<float> (row));
    }
}

/** @note MESSAGE THREAD — relaxed load (snapshot-dirty handshake provides ordering). */
int State::getOutputBlockTop() const noexcept
{
    return jreng::toInt (getRawParam (ID::outputBlockTop)->load (std::memory_order_relaxed));
}

/** @note MESSAGE THREAD — relaxed load (snapshot-dirty handshake provides ordering). */
int State::getOutputBlockBottom() const noexcept
{
    return jreng::toInt (getRawParam (ID::outputBlockBottom)->load (std::memory_order_relaxed));
}

/**
 * @brief Returns `true` when a valid completed output block exists.
 *
 * Valid when: outputScanActive is false (D fired), outputBlockTop >= 0,
 * promptRow > outputBlockTop, and on normal screen.
 *
 * @return `true` if a valid output block is present.
 * @note MESSAGE THREAD only.
 */
bool State::hasOutputBlock() const noexcept
{
    const int blockTop { getOutputBlockTop() };
    const int prompt { getPromptRow() };
    const int screenVal { getRawValue<int> (ID::activeScreen) };
    const bool normalScreen { screenVal == static_cast<int> (ActiveScreen::normal) };

    return blockTop >= 0 and prompt > blockTop and normalScreen;
}

/** @note READER THREAD — stores the prompt row from OSC 133 A. */
void State::setPromptRow (int row) noexcept { storeAndFlush (ID::promptRow, static_cast<float> (row)); }

/** @note READER THREAD — relaxed load; called from resize on the reader thread. */
int State::getPromptRow() const noexcept
{
    return jreng::toInt (getRawParam (ID::promptRow)->load (std::memory_order_relaxed));
}

/**
 * @brief SSOT writer for all three string slots (title, cwd, foreground process).
 *
 * Copies up to `maxStringLength - 1` bytes from `src` into the slot's
 * `buffer` and null-terminates it.  The generation counter is then incremented
 * with a `memory_order_release` store so that `flushStrings()` (message thread)
 * observes the completed write via a matching `memory_order_acquire` load.
 * Finally, `needsFlush` is released so the timer wakes on the next tick.
 *
 * This is the single write path for all string state — `setTitle`, `setCwd`,
 * and `setForegroundProcess` all delegate here (SSOT / DRY).
 *
 * @param id      One of `ID::title`, `ID::cwd`, `ID::foregroundProcess`.
 * @param src     Pointer to the source bytes.  Need not be null-terminated.
 * @param length  Number of source bytes to copy.
 * @note READER THREAD — lock-free, noexcept.
 */
void State::writeStringSlot (const juce::Identifier& id, const char* src, int length) noexcept
{
    // READER THREAD
    auto* slot { stringMap.at (id).get() };
    const int len { juce::jmin (length, maxStringLength - 1) };
    std::memcpy (slot->buffer, src, static_cast<size_t> (len));
    slot->buffer[len] = '\0';

    const juce::Identifier genId { id == ID::title ? ID::titleGeneration
                                   : id == ID::cwd ? ID::cwdGeneration
                                                   : ID::foregroundProcessGeneration };
    fetchAdd (*getRawParam (genId), 1.0f, std::memory_order_release);
    getRawParam (ID::needsFlush)->store (1.0f, std::memory_order_release);
}

/** @note READER THREAD — delegates to `writeStringSlot (ID::title, …)`. */
void State::setTitle (const char* src, int length) noexcept { writeStringSlot (ID::title, src, length); }

/** @note READER THREAD — delegates to `writeStringSlot (ID::cwd, …)`. */
void State::setCwd (const char* src, int length) noexcept { writeStringSlot (ID::cwd, src, length); }

/** @note READER THREAD — delegates to `writeStringSlot (ID::foregroundProcess, …)`. */
void State::setForegroundProcess (const char* src, int length) noexcept
{
    writeStringSlot (ID::foregroundProcess, src, length);
}

/**
 * @brief Records an OSC 8 hyperlink span into `hyperlinkMap`.
 *
 * Emplaces or overwrites the entry keyed by `id`.  Bumps `hyperlinksGeneration`
 * with a release store so `flushHyperlinks()` observes the completed write.
 * Sets `needsFlush` so the timer wakes on the next tick.
 *
 * @note READER THREAD — lock-free, noexcept.
 */
void State::storeHyperlink (const juce::Identifier& id, const char* uri, int uriLength,
                             int row, int startCol, int endCol) noexcept
{
    // READER THREAD
    auto it { hyperlinkMap.find (id) };

    if (it == hyperlinkMap.end())
    {
        hyperlinkMap.emplace (id, std::make_unique<HyperlinkEntry>());
        it = hyperlinkMap.find (id);
    }

    auto* entry { it->second.get() };
    const int len { juce::jmin (uriLength, maxStringLength - 1) };
    std::memcpy (entry->uri, uri, static_cast<size_t> (len));
    entry->uri[len] = '\0';
    entry->row      = row;
    entry->startCol = startCol;
    entry->endCol   = endCol;

    fetchAdd (*getRawParam (ID::hyperlinksGeneration), 1.0f, std::memory_order_release);
    getRawParam (ID::needsFlush)->store (1.0f, std::memory_order_release);
}

/**
 * @brief Clears all entries in `hyperlinkMap` and bumps the generation.
 *
 * `flushHyperlinks()` detects the generation change and removes all
 * HYPERLINKS children from the ValueTree.
 *
 * @note READER THREAD — lock-free, noexcept.
 */
void State::clearHyperlinks() noexcept
{
    // READER THREAD
    hyperlinkMap.clear();
    fetchAdd (*getRawParam (ID::hyperlinksGeneration), 1.0f, std::memory_order_release);
    getRawParam (ID::needsFlush)->store (1.0f, std::memory_order_release);
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
    if (getRawParam (ID::pasteEchoRemaining)->load (std::memory_order_relaxed) <= 0.0f)
    {
        getRawParam (ID::snapshotDirty)->store (1.0f, std::memory_order_release);
    }
}

// MESSAGE THREAD
void State::setPasteEchoGate (int bytes) noexcept
{
    getRawParam (ID::pasteEchoRemaining)->store (static_cast<float> (bytes), std::memory_order_release);
}

// READER THREAD
void State::consumePasteEcho (int bytes) noexcept
{
    auto* gate { getRawParam (ID::pasteEchoRemaining) };

    if (gate->load (std::memory_order_relaxed) > 0.0f)
    {
        const float remaining { fetchSub (*gate, static_cast<float> (bytes), std::memory_order_acq_rel)
                                - static_cast<float> (bytes) };

        if (remaining <= 0.0f)
        {
            gate->store (0.0f, std::memory_order_relaxed);
            setSnapshotDirty();
        }
    }
}

// READER THREAD
void State::clearPasteEchoGate() noexcept
{
    auto* gate { getRawParam (ID::pasteEchoRemaining) };

    if (gate->exchange (0.0f, std::memory_order_acq_rel) > 0.0f)
    {
        setSnapshotDirty();
    }
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
    return getRawParam (ID::snapshotDirty)->exchange (0.0f, std::memory_order_acquire) != 0.0f;
}

// READER THREAD
void State::setSyncOutput (bool active) noexcept
{
    getRawParam (ID::syncOutputActive)->store (active ? 1.0f : 0.0f, std::memory_order_release);

    if (not active)
        setSnapshotDirty();
}

bool State::isSyncOutputActive() const noexcept
{
    return getRawParam (ID::syncOutputActive)->load (std::memory_order_relaxed) != 0.0f;
}

// MESSAGE THREAD
void State::setModalType (ModalType type) noexcept
{
    storeAndFlush (ID::modalType, static_cast<float> (type));
    setFullRebuild();
    setSnapshotDirty();
}

ModalType State::getModalType() const noexcept { return static_cast<ModalType> (getRawValue<int> (ID::modalType)); }

bool State::isModal() const noexcept { return getModalType() != ModalType::none; }

void State::setHintPage (int page) noexcept
{
    storeAndFlush (ID::hintPage, static_cast<float> (page));
}

int State::getHintPage() const noexcept { return getRawValue<int> (ID::hintPage); }

void State::setHintTotalPages (int total) noexcept
{
    storeAndFlush (ID::hintTotalPages, static_cast<float> (total));
}

int State::getHintTotalPages() const noexcept { return getRawValue<int> (ID::hintTotalPages); }

// MESSAGE THREAD
void State::setHintOverlay (const LinkSpan* data, int count) noexcept
{
    hintOverlayData  = data;
    hintOverlayCount = count;
    setSnapshotDirty();
}

const LinkSpan* State::getHintOverlayData() const noexcept { return hintOverlayData; }

int State::getHintOverlayCount() const noexcept { return hintOverlayCount; }

// --- Selection state convenience wrappers ---

void State::setSelectionType (int type) noexcept
{
    storeAndFlush (ID::selectionType, static_cast<float> (type));
    setFullRebuild();
    setSnapshotDirty();
}

int State::getSelectionType() const noexcept { return getRawValue<int> (ID::selectionType); }

void State::setSelectionCursor (int row, int col) noexcept
{
    storeAndFlush (ID::selectionCursorRow, static_cast<float> (row));
    storeAndFlush (ID::selectionCursorCol, static_cast<float> (col));
    setFullRebuild();
    setSnapshotDirty();
}

int State::getSelectionCursorRow() const noexcept { return getRawValue<int> (ID::selectionCursorRow); }

int State::getSelectionCursorCol() const noexcept { return getRawValue<int> (ID::selectionCursorCol); }

void State::setSelectionAnchor (int row, int col) noexcept
{
    storeAndFlush (ID::selectionAnchorRow, static_cast<float> (row));
    storeAndFlush (ID::selectionAnchorCol, static_cast<float> (col));
    setFullRebuild();
    setSnapshotDirty();
}

int State::getSelectionAnchorRow() const noexcept { return getRawValue<int> (ID::selectionAnchorRow); }

int State::getSelectionAnchorCol() const noexcept { return getRawValue<int> (ID::selectionAnchorCol); }

void State::setDragAnchor (int row, int col) noexcept
{
    storeAndFlush (ID::dragAnchorRow, static_cast<float> (row));
    storeAndFlush (ID::dragAnchorCol, static_cast<float> (col));
}

int State::getDragAnchorRow() const noexcept { return getRawValue<int> (ID::dragAnchorRow); }

int State::getDragAnchorCol() const noexcept { return getRawValue<int> (ID::dragAnchorCol); }

void State::setDragActive (bool active) noexcept
{
    storeAndFlush (ID::dragActive, active ? 1.0f : 0.0f);
    setFullRebuild();
    setSnapshotDirty();
}

bool State::isDragActive() const noexcept { return getRawValue<bool> (ID::dragActive); }

void State::requestSyncResize() noexcept
{
    getRawParam (ID::syncResizePending)->store (1.0f, std::memory_order_relaxed);
}

bool State::consumeSyncResize() noexcept
{
    return getRawParam (ID::syncResizePending)->exchange (0.0f, std::memory_order_relaxed) != 0.0f;
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
ActiveScreen State::getScreen() const noexcept { return getRawValue<ActiveScreen> (ID::activeScreen); }

/**
 * @brief Returns the current terminal column count.
 * @return Number of columns (e.g. 80 or 132).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getCols() const noexcept { return getRawValue<int> (ID::cols); }

/**
 * @brief Returns the number of rows visible in the terminal viewport.
 * @return Number of visible rows (e.g. 24).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getVisibleRows() const noexcept { return getRawValue<int> (ID::visibleRows); }

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
bool State::getMode (const juce::Identifier& id) const noexcept { return getRawValue<bool> (modeKey (id)); }

/**
 * @brief Returns the cursor row for the specified screen buffer.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based row index within the visible area.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getCursorRow (ActiveScreen s) const noexcept { return getRawValue<int> (screenKey (s, ID::cursorRow)); }

/**
 * @brief Returns the cursor column for the specified screen buffer.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based column index.
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getCursorCol (ActiveScreen s) const noexcept { return getRawValue<int> (screenKey (s, ID::cursorCol)); }

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
bool State::isWrapPending (ActiveScreen s) const noexcept { return getRawValue<bool> (screenKey (s, ID::wrapPending)); }

/**
 * @brief Returns the top row of the scrolling region for the specified screen.
 * @param s  Target screen (`normal` or `alternate`).
 * @return Zero-based row index of the first scrolling row (DECSTBM top).
 * @note READER THREAD — `memory_order_relaxed` load, lock-free, noexcept.
 */
int State::getScrollTop (ActiveScreen s) const noexcept { return getRawValue<int> (screenKey (s, ID::scrollTop)); }

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

int State::getCursorShape (ActiveScreen s) const noexcept { return getRawValue<int> (screenKey (s, ID::cursorShape)); }

float State::getCursorColorR (ActiveScreen s) const noexcept
{
    return getRawValue<float> (screenKey (s, ID::cursorColorR));
}

float State::getCursorColorG (ActiveScreen s) const noexcept
{
    return getRawValue<float> (screenKey (s, ID::cursorColorG));
}

float State::getCursorColorB (ActiveScreen s) const noexcept
{
    return getRawValue<float> (screenKey (s, ID::cursorColorB));
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
juce::ValueTree State::get() noexcept { return state; }

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
    return jreng::ValueTree::getValueFromChildWithID (state, paramId);
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
    auto param { jreng::ValueTree::getChildWithID (modesNode, id.toString()) };
    bool result { false };

    if (param.isValid())
    {
        result = static_cast<bool> (param.getProperty (ID::value));
    }

    return result;
}

/**
 * @brief Returns the currently active screen buffer from the ValueTree
 *        (post-flush value).
 *
 * Navigates to the `activeScreen` PARAM child of the root SESSION node and
 * returns its `value` property cast to `ActiveScreen`.  Returns
 * `ActiveScreen::normal` if the parameter node is not found.
 *
 * @return `ActiveScreen::normal` or `ActiveScreen::alternate`.
 * @note MESSAGE THREAD only — reads from the ValueTree (post-flush values).
 */
ActiveScreen State::getActiveScreen() const noexcept
{
    auto param { jreng::ValueTree::getChildWithID (state, ID::activeScreen.toString()) };
    ActiveScreen result { normal };

    if (param.isValid())
    {
        result = static_cast<ActiveScreen> (static_cast<int> (param.getProperty (ID::value)));
    }

    return result;
}

/**
 * @brief Reads the kitty keyboard flags from the ValueTree (post-flush value).
 *
 * Determines the active screen via `getActiveScreen()`, navigates to the
 * corresponding screen child node (`NORMAL` or `ALTERNATE`), and reads
 * the `keyboardFlags` parameter.  Returns 0 (legacy mode) if the parameter
 * is not found.
 *
 * @return The active keyboard enhancement flags (0 = legacy mode).
 * @note MESSAGE THREAD only — reads from the ValueTree (post-flush values).
 */
uint32_t State::getTreeKeyboardFlags() const noexcept
{
    const auto scr { getActiveScreen() };
    auto screenNode { state.getChildWithName (scr == normal ? ID::NORMAL : ID::ALTERNATE) };
    auto param { jreng::ValueTree::getChildWithID (screenNode, ID::keyboardFlags.toString()) };
    uint32_t result { 0 };

    if (param.isValid())
    {
        result = static_cast<uint32_t> (static_cast<int> (param.getProperty (ID::value)));
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
    auto param { jreng::ValueTree::getChildWithID (state, ID::scrollOffset.toString()) };

    if (param.isValid())
    {
        param.setProperty (ID::value, offset, nullptr);
        setFullRebuild();
        setSnapshotDirty();
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
    auto param { jreng::ValueTree::getChildWithID (state, ID::scrollOffset.toString()) };
    int result { 0 };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (ID::value));
    }

    return result;
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
    flushHyperlinks();

    const int interval { anythingUpdated ? 1000 / flushHz : 1000 / idleHz };
    tickCursorBlink (interval);
    startTimer (interval);

    if (onFlush != nullptr)
        onFlush();
}

/**
 * @brief Advances the cursor blink counter and toggles the phase.
 *
 * Reads the current cursor position from the active screen's atomics and
 * compares against the previous flushed position.  If the cursor moved,
 * the blink phase resets to "on" (visible) — standard terminal behaviour
 * where typing makes the cursor solid until the next blink interval.
 *
 * DECSCUSR shape parity determines whether blinking is active:
 * - Shape 0: uses the `cursor.blink` config setting.
 * - Odd shapes (1, 3, 5): blink enabled.
 * - Even shapes (2, 4, 6): steady (always visible).
 *
 * @param elapsedMs  Milliseconds since the previous timer tick.
 * @note MESSAGE THREAD — called from `timerCallback()` only.
 */
void State::tickCursorBlink (int elapsedMs) noexcept
{
    const ActiveScreen scr { getScreen() };
    const int row { getCursorRow (scr) };
    const int col { getCursorCol (scr) };
    const int shape { getCursorShape (scr) };

    const int prevRow { jreng::toInt (getRawParam (ID::prevFlushedCursorRow)->load (std::memory_order_relaxed)) };
    const int prevCol { jreng::toInt (getRawParam (ID::prevFlushedCursorCol)->load (std::memory_order_relaxed)) };
    const bool cursorMoved { row != prevRow or col != prevCol };
    getRawParam (ID::prevFlushedCursorRow)->store (static_cast<float> (row), std::memory_order_relaxed);
    getRawParam (ID::prevFlushedCursorCol)->store (static_cast<float> (col), std::memory_order_relaxed);

    // Shape 0 defers to config; odd = blink; even = steady.
    const bool blinkEnabled { getRawParam (ID::cursorBlinkEnabled)->load (std::memory_order_relaxed) != 0.0f };
    const bool blinkActive { shape == 0 ? blinkEnabled : (shape % 2 != 0) };

    // Movement or steady mode resets to visible.
    if (cursorMoved or not blinkActive)
    {
        getRawParam (ID::cursorBlinkOn)->store (1.0f, std::memory_order_relaxed);
        getRawParam (ID::cursorBlinkElapsed)->store (0.0f, std::memory_order_relaxed);
    }
    else
    {
        const float elapsed { getRawParam (ID::cursorBlinkElapsed)->load (std::memory_order_relaxed)
                              + static_cast<float> (elapsedMs) };
        getRawParam (ID::cursorBlinkElapsed)->store (elapsed, std::memory_order_relaxed);

        const float interval { getRawParam (ID::cursorBlinkInterval)->load (std::memory_order_relaxed) };

        if (elapsed >= interval)
        {
            const bool wasOn { getRawParam (ID::cursorBlinkOn)->load (std::memory_order_relaxed) != 0.0f };
            getRawParam (ID::cursorBlinkOn)->store (wasOn ? 0.0f : 1.0f, std::memory_order_relaxed);
            getRawParam (ID::cursorBlinkElapsed)->store (0.0f, std::memory_order_relaxed);
            setFullRebuild();
            setSnapshotDirty();
        }
    }
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
 * @note MESSAGE THREAD — called once from the constructor.  After this call
 *       `parameterMap` is immutable and safe to read from any thread without
 *       a lock.
 */
void State::buildParameterMap() noexcept
{
    jreng::ValueTree::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& node) -> bool
        {
            if (node.getType() == ID::PARAM and node.getProperty (ID::value).isDouble())
            {
                const auto paramId { node.getProperty (ID::id).toString() };
                const auto parent { node.getParent() };
                const bool isRoot { parent.getType() == ID::SESSION };
                const juce::Identifier key { isRoot ? juce::Identifier { paramId }
                                                    : buildParamKey (parent.getType(), paramId) };
                parameterMap[key] =
                    std::make_unique<std::atomic<float>> (static_cast<float> (node.getProperty (ID::value)));
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
    for (auto& [id, slotPtr] : stringMap)
    {
        auto* slot { slotPtr.get() };

        const juce::Identifier genId { id == ID::title ? ID::titleGeneration
                                       : id == ID::cwd ? ID::cwdGeneration
                                                       : ID::foregroundProcessGeneration };

        const juce::Identifier lastGenId { id == ID::title ? ID::titleLastFlushedGeneration
                                           : id == ID::cwd ? ID::cwdLastFlushedGeneration
                                                           : ID::foregroundProcessLastFlushedGeneration };

        auto* genAtom { getRawParam (genId) };
        auto* lastGenAtom { getRawParam (lastGenId) };

        const float gen { genAtom->load (std::memory_order_acquire) };
        const float lastGen { lastGenAtom->load (std::memory_order_relaxed) };

        if (gen != lastGen)
        {
            char local[maxStringLength];
            std::memcpy (local, slot->buffer, maxStringLength);

            const float gen2 { genAtom->load (std::memory_order_acquire) };

            if (gen2 != gen)
                std::memcpy (local, slot->buffer, maxStringLength);

            lastGenAtom->store (gen2, std::memory_order_relaxed);
            state.setProperty (id, juce::String::fromUTF8 (local), nullptr);
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
