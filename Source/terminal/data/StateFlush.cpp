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
 * ```
 *
 * ## Why a separate translation unit?
 *
 * The flush logic is isolated here so that `State.cpp` stays focused on
 * construction, key-building, and the reader-thread API.  The flush pass
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
 * @see State.cpp — construction, key-building, and reader-thread API.
 */

#include "State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

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
 * - `activeScreen`  — which screen buffer is active (normal / alternate).
 * - `cols`          — terminal width in characters.
 * - `visibleRows`   — terminal height in characters.
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
 * 2. **`flushRootParams()`** — writes session-level PARAMs (`activeScreen`,
 *    `cols`, `visibleRows`) back to the ValueTree.
 *
 * 3. **Group loop** — iterates the children of SESSION and calls
 *    `flushGroupParams()` for each group node (MODES, NORMAL, ALTERNATE).
 *    Non-group children (i.e. root-level PARAMs already handled above) are
 *    skipped via the `isGroup` guard.
 *
 * 4. Returns `true` to signal to `timerCallback()` that the ValueTree was
 *    modified, prompting it to schedule the next tick at the higher 120 Hz
 *    rate.
 *
 * @return `true` if `needsFlush` was set and the ValueTree was updated;
 *         `false` if nothing had changed and the flush was skipped entirely.
 * @note MESSAGE THREAD — called from `timerCallback()` only.
 */
// MESSAGE THREAD
bool State::flush() noexcept
{
    bool result { false };
    if (getRawParam (ID::needsFlush)->exchange (0.0f, std::memory_order_acquire) != 0.0f)
    {
        // Flush group params (MODES, NORMAL, ALTERNATE) BEFORE root params.
        // Root params include activeScreen, whose ValueTree change fires
        // synchronous listeners that read cursorRow/cursorCol from the
        // target screen.  Those values must already be up-to-date.
        for (int c { 0 }; c < state.getNumChildren(); ++c)
        {
            auto child { state.getChild (c) };
            const bool isGroup { child.getType() == ID::MODES or child.getType() == ID::NORMAL or child.getType() == ID::ALTERNATE };

            if (isGroup)
            {
                flushGroupParams (child);
            }
        }

        flushRootParams();

        result = true;
    }

    return result;
}

/**
 * @brief Synchronises OSC 8 hyperlink spans from `hyperlinkMap` into the
 *        HYPERLINKS ValueTree node.
 *
 * Compares `hyperlinksGeneration` (acquire load) against
 * `hyperlinksLastFlushedGeneration`.  When they differ, removes all existing
 * HYPERLINKS children and rebuilds them from `hyperlinkMap`.  Each child node
 * has type `ID::HYPERLINK` and carries `ID::uri`, `ID::row`, `ID::startCol`,
 * and `ID::endCol`.
 *
 * @note MESSAGE THREAD — called from `timerCallback()` only.
 */
// MESSAGE THREAD
void State::flushHyperlinks() noexcept
{
    auto* genAtom     { getRawParam (ID::hyperlinksGeneration) };
    auto* lastGenAtom { getRawParam (ID::hyperlinksLastFlushedGeneration) };

    const float gen     { genAtom->load (std::memory_order_acquire) };
    const float lastGen { lastGenAtom->load (std::memory_order_relaxed) };

    if (gen != lastGen)
    {
        lastGenAtom->store (gen, std::memory_order_relaxed);

        juce::ValueTree hyperlinksNode { state.getChildWithName (ID::HYPERLINKS) };

        if (hyperlinksNode.isValid())
        {
            hyperlinksNode.removeAllChildren (nullptr);

            for (const auto& [id, entry] : hyperlinkMap)
            {
                juce::ValueTree child { ID::HYPERLINK };
                child.setProperty (ID::uri,      juce::String::fromUTF8 (entry->uri), nullptr);
                child.setProperty (ID::row,      entry->row,      nullptr);
                child.setProperty (ID::startCol, entry->startCol, nullptr);
                child.setProperty (ID::endCol,   entry->endCol,   nullptr);

                hyperlinksNode.appendChild (child, nullptr);
            }

            // Signal listeners once after the full rebuild by bumping the node's
            // value property.  This fires valueTreePropertyChanged on any listener
            // attached to hyperlinksNode, triggering a single re-scan.
            hyperlinksNode.setProperty (ID::value, gen, nullptr);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
