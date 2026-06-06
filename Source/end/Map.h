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

    static const juce::String get (int key) noexcept { return get().at (key); }
};

//==============================================================================
struct TabOrientation : public jam::Map::Instance<TabOrientation>
{
    TabOrientation()
    {
        map = {
            { jam::button::Tab::Orientation::left,   "left"   },
            { jam::button::Tab::Orientation::bottom, "bottom" },
            { jam::button::Tab::Orientation::top,    "top"    },
            { jam::button::Tab::Orientation::right,  "right"  },
        };
    }

    const juce::String& getDefault() const noexcept override { return map.at (false); }

    static const auto& get() noexcept { return getContext()->map; }

    static int get (const juce::String& value) noexcept
    {
        return jam::Map::getKey (get()).at (value);
    }

    static const juce::String& get (int key) noexcept { return getContext()->map.at (key); }
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
