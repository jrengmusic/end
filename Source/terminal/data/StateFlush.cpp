/**
 * @file StateFlush.cpp
 * @brief Timer-driven flush pass: copies dirty atomics into the ValueTree.
 *
 * ## Flush mechanism overview
 *
 * The three functions in this file form the write-back path from the atomic
 * parameter store to the `juce::ValueTree` that the UI observes:
 *
 * ```
 * timerCallback()          [MESSAGE THREAD / TIMER, ~60–120 Hz]
 *   └─ flush()             [MESSAGE THREAD]
 *        ├─ flushRootParams()   — session-level PARAMs (activeScreen, cols, …)
 *        └─ flushGroupParams()  — per-group PARAMs (MODES, NORMAL, ALTERNATE)
 *
 * refresh()                [MESSAGE THREAD / onVBlank]
 *   └─ flush()             [MESSAGE THREAD]  (same pass, driven by VBlank)
 * ```
 *
 * ## Why a separate translation unit?
 *
 * The flush logic is isolated here so that `State.cpp` stays focused on
 * construction, key-building, and the reader-thread setters.  The flush pass
 * iterates the ValueTree and touches every parameter, making it the most
 * "expensive" operation in the class — keeping it separate makes profiling
 * and future optimisation easier.
 *
 * ## Memory ordering
 *
 * `flush()` opens with an `exchange (false, memory_order_acquire)` on
 * `needsFlush`.  This acquire fence synchronises with the `memory_order_release`
 * store in `storeAndFlush()` (reader thread), guaranteeing that all relaxed
 * atomic stores made before the release are visible to the flush pass.
 *
 * Individual atomic loads inside `flushRootParams()` and `flushGroupParams()`
 * therefore use `memory_order_relaxed` — the acquire on `needsFlush` already
 * provides the necessary ordering.
 *
 * @see State.h   — full architecture overview and thread ownership table.
 * @see State.cpp — construction, key-building, and reader-thread setters.
 */

#include "State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief MESSAGE THREAD flush — snap-copies `slot.buffer` to `node[property]` when generation advanced.
 *
 * Loads `slot.generation` before and after the `memcpy`.  If the value changed
 * (torn write in flight), repeats the copy once.  Writes to the ValueTree only
 * when `generation` has advanced past `slot.lastFlushedGeneration`.
 *
 * @param slot      Source `StringSlot`.
 * @param node      ValueTree node to write the property on.
 * @param property  Property identifier to set on `node`.
 * @return `true` if the ValueTree was updated; `false` if generation had not changed.
 * @note MESSAGE THREAD — noexcept.
 */
// MESSAGE THREAD
bool State::flushSlot (StringSlot& slot, juce::ValueTree& node, const juce::Identifier& property) noexcept
{
    const auto gen { slot.generation.load (std::memory_order_acquire) };

    if (gen == slot.lastFlushedGeneration)
        return false;

    char local[maxStringLength];
    std::memcpy (local, slot.buffer, maxStringLength);

    const auto gen2 { slot.generation.load (std::memory_order_acquire) };

    if (gen2 != gen)
        std::memcpy (local, slot.buffer, maxStringLength);

    slot.lastFlushedGeneration = gen2;
    node.setProperty (property, juce::String::fromUTF8 (local), nullptr);
    return true;
}

/**
 * @brief Flushes PARAM nodes that are direct children of the root SESSION node.
 *
 * Iterates the immediate children of `state` (the SESSION root).  For each
 * child whose type is `ID::PARAM`, it looks up the parameter's atomic slot in
 * `parameterMap` using the raw `id` string as the key (root-level params have
 * no parent-type prefix).  If found, the current atomic value is written back
 * to the ValueTree's `value` property.
 *
 * Parameters handled here:
 * - `activeScreen`   — which screen buffer is active (normal / alternate).
 * - `cols`           — terminal width; written by `setDimensions()` on the message thread.
 * - `visibleRows`    — terminal height; written by `setDimensions()` on the message thread.
 * - `scrollbackUsed` — scrollback depth; written by the reader thread via `setScrollbackUsed()`.
 * - All other root-level session PARAMs.
 *
 * `scrollOffset` is intentionally skipped: it is stored as an integer
 * `juce::var` (not a `double`), so `buildParameterMap()` never registered it
 * in `parameterMap`.  It is owned exclusively by the message thread and
 * written directly to the ValueTree via `setScrollOffset()`.
 *
 * @note MESSAGE THREAD — called from `flush()` only.  The acquire fence on
 *       `needsFlush` in `flush()` ensures all reader-thread stores are visible
 *       before this function reads the atomics.
 */
// MESSAGE THREAD
void State::flushRootParams() noexcept
{
    for (int i { 0 }; i < state.getNumChildren(); ++i)
    {
        auto child { state.getChild (i) };
        if (child.getType() == ID::PARAM)
        {
            const auto paramId { child.getProperty (ID::id).toString() };
            const auto it { parameterMap.find (juce::Identifier { paramId }) };
            if (it != parameterMap.end())
            {
                child.setProperty (ID::value, it->second.get()->load (std::memory_order_relaxed), nullptr);
            }
        }
    }
}

/**
 * @brief Flushes PARAM children of a single group node (MODES, NORMAL, or ALTERNATE).
 *
 * Iterates the PARAM children of `group`.  For each PARAM, the compound key
 * is built via `buildParamKey (parentType, paramId)` — the same format used
 * during `buildParameterMap()` — and the corresponding atomic is looked up in
 * `parameterMap`.  If found, the current float value is written to the
 * ValueTree's `value` property.
 *
 * Writing to the ValueTree here triggers any attached
 * `juce::ValueTree::Listener::valueTreePropertyChanged()` callbacks
 * synchronously on the message thread, which is how UI components learn that
 * a parameter has changed.
 *
 * @param group  A group-level ValueTree child of SESSION.  Expected to be one
 *               of: the MODES node, the NORMAL screen node, or the ALTERNATE
 *               screen node.  Passing any other node is a no-op (no PARAM
 *               children will match `parameterMap`).
 * @note MESSAGE THREAD — called from `flush()` only.
 */
// MESSAGE THREAD
void State::flushGroupParams (juce::ValueTree& group) noexcept
{
    const auto parentType { group.getType() };
    for (int i { 0 }; i < group.getNumChildren(); ++i)
    {
        auto param { group.getChild (i) };
        if (param.getType() == ID::PARAM)
        {
            const auto paramId { param.getProperty (ID::id).toString() };
            const juce::Identifier key { buildParamKey (parentType, paramId) };
            const auto it { parameterMap.find (key) };
            if (it != parameterMap.end())
            {
                param.setProperty (ID::value, it->second.get()->load (std::memory_order_relaxed), nullptr);
            }
        }
    }
}

/**
 * @brief Signals that the caller is about to read State values.
 *
 * Ensures all pending atomic writes from the reader thread are
 * visible in the ValueTree before the caller proceeds.
 *
 * @return `true` if the ValueTree was updated (new READER data arrived); `false` otherwise.
 * @note MESSAGE THREAD only.
 */
bool State::refresh() noexcept { return flush(); }

/**
 * @brief Copies all dirty atomic values into the ValueTree in a single pass.
 *
 * ## Flush sequence
 *
 * 1. **Test-and-clear `needsFlush`** with `memory_order_acquire`.
 *    - If `false`: nothing changed since the last flush — return immediately.
 *    - If `true`: proceed.  The acquire fence synchronises with the reader
 *      thread's `memory_order_release` store in `storeAndFlush()`, making all
 *      preceding relaxed atomic stores visible here.
 *
 * 2. **Group loop** — iterates the children of SESSION and calls
 *    `flushGroupParams()` for each group node (MODES, NORMAL, ALTERNATE).
 *    Non-group children (i.e. root-level PARAMs handled below) are
 *    skipped via the `isGroup` guard.
 *
 * 3. **`flushRootParams()`** — writes session-level PARAMs (`activeScreen`)
 *    back to the ValueTree.
 *
 * 4. **Scrollback clamping** — clamps `scrollOffset` to `scrollbackUsed` if
 *    the scrollback shrank (e.g. on resize).
 *
 * 5. Returns `true` to signal to `timerCallback()` that the ValueTree was
 *    modified, prompting it to schedule the next tick at the higher 120 Hz
 *    rate.
 *
 * @return `true` if `needsFlush` was set and the ValueTree was updated;
 *         `false` if nothing had changed and the flush was skipped entirely.
 * @note MESSAGE THREAD — called from `timerCallback()` and `refresh()`.
 */
// MESSAGE THREAD
bool State::flush() noexcept
{
    if (bool needsFlush { getRawParam (ID::needsFlush)->exchange (0.0f, std::memory_order_acquire) != 0.0f })
    {
        // Flush group params (MODES, NORMAL, ALTERNATE) BEFORE root params.
        // Root params include activeScreen, whose ValueTree change fires
        // synchronous listeners that read cursorRow/cursorCol from the
        // target screen.  Those values must already be up-to-date.
        for (int c { 0 }; c < state.getNumChildren(); ++c)
        {
            auto child { state.getChild (c) };
            const bool isGroup { child.getType() == ID::MODES or child.getType() == ID::NORMAL
                                 or child.getType() == ID::ALTERNATE };

            if (isGroup)
            {
                flushGroupParams (child);
            }
        }

        const auto activeScreenNode { jam::ValueTree::getChildWithID (state, ID::activeScreen.toString()) };
        const float prevActiveScreen { activeScreenNode.isValid()
                                           ? static_cast<float> (activeScreenNode.getProperty (ID::value))
                                           : 0.0f };

        flushRootParams();

        const float newActiveScreen { activeScreenNode.isValid()
                                          ? static_cast<float> (activeScreenNode.getProperty (ID::value))
                                          : 0.0f };
        if (newActiveScreen != prevActiveScreen and getScrollOffset() != 0)
            setScrollOffset (0);

        {
            const auto scrollbackNode { jam::ValueTree::getChildWithID (state, ID::scrollbackUsed.toString()) };
            const int flushedScrollback { scrollbackNode.isValid()
                                              ? static_cast<int> (scrollbackNode.getProperty (ID::value))
                                              : 0 };
            const int currentOffset { getScrollOffset() };
            if (currentOffset > flushedScrollback)
                setScrollOffset (flushedScrollback);
        }

        return needsFlush;
    }

    return false;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
