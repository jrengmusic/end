#pragma once
#include <JuceHeader.h>

namespace end
{
/*____________________________________________________________________________*/

struct Boolean : public jam::Map::Instance<Boolean>
{
    /**
     * @brief Populates the bimap with all known display mode entries.
     */
    Boolean()
    {
        map = {
            { false, "false" },
            { true,  "true"  }
        };
    }

    const juce::String& getDefault() const noexcept override { return map.at (false); }

    static const auto& get() noexcept { return getContext()->map; }

    static const bool get (const juce::String& value) noexcept
    {
        return jam::Map::getKey (get()).at (value);
    }

    /**
     * @brief Looks up the string for a given integer key.
     *
     * @param key The integer key (e.g. Boolean::false).
     * @return The corresponding mode string.
     */
    static const juce::String get (int key) noexcept { return get().at (key); }
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
