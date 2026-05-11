#include "State.h"

namespace Terminal
{

/**
 * @brief Copies all dirty atom values into the ValueTree in a single pass.
 *
 * ### Flush sequence
 *
 * 1. Test-and-clear `needsFlush` (acquire).
 * 2. MODES group — mode atoms flushed before screen atoms.
 * 3. Screen groups — cursor/keyboard data flushed before activeScreen fires
 *    ValueTree listeners that read those values.
 * 4. SESSION group — root atoms including activeScreen (fires listeners),
 *    plus TEXT atoms (Atom<const char*>) that flush to direct SESSION properties.
 * 5. displayName computation from flushed title / cwd / foreground.
 *
 * All groups are nested AnyMaps so forEach<AtomBase> operates on uniform
 * Atom entries — no UB from mixed-type iteration.
 *
 * @return `true` if needsFlush was set and the ValueTree was updated.
 * @note MESSAGE THREAD.
 */
bool State::flush() noexcept
{
    if (needsFlushAtom->exchangeAcquire (0) != 0)
    {
        // Flush MODES first — mode values must be current before screen atoms.
        params.get<jam::AnyMap> (ID::MODES)->forEach<AtomBase> (
            [] (const juce::Identifier&, AtomBase& atom) { atom.flush(); });

        // Flush screen groups — cursor data must be current before activeScreen
        // fires ValueTree listeners that read cursor position from the VT nodes.
        auto* screenCtx { map::Screen::getContext() };

        for (const auto& [index, screenName] : screenCtx->get())
        {
            const juce::Identifier screenId { screenName };
            params.get<jam::AnyMap> (screenId)->forEach<AtomBase> (
                [] (const juce::Identifier&, AtomBase& atom) { atom.flush(); });
        }

        // Flush SESSION group last — contains activeScreen which fires listeners,
        // and TEXT atoms (Atom<const char*>) which flush to direct SESSION properties.
        params.get<jam::AnyMap> (ID::SESSION)->forEach<AtomBase> (
            [] (const juce::Identifier&, AtomBase& atom) { atom.flush(); });

        // displayName from flushed title / cwd / foreground.
        const auto foreground { get().getProperty (ID::foregroundProcess).toString() };
        const auto cwdPath    { get().getProperty (ID::cwd).toString() };
        juce::String name;

        if (foreground.isNotEmpty())
        {
            name = foreground;
        }
        else if (cwdPath.isNotEmpty())
        {
            name = juce::File (cwdPath).getFileName();
        }

        if (name.isNotEmpty())
            get().setProperty (App::ID::displayName, name, nullptr);

        return true;
    }

    return false;
}

bool State::refresh() noexcept { return flush(); }

} // namespace Terminal
