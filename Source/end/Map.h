#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

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

//==============================================================================
namespace config
{
/*____________________________________________________________________________*/

/**
 * @brief Registry of lua config files — 8 section keys.
 *        CRTP-derived from jam::Map::Instance<File>.
 *
 * Maps each enum key to its Identifier stem (e.g. config → "config") and
 * resolves the on-disk filename via getName(). All keys produce "stem.lua".
 *
 * Owns the user config directory and the lua extension. The live instance
 * is owned by end::Application — static get() resolves through Context<File>.
 */
struct File : public jam::Map::Instance<File>
{
    /** @brief Integer keys for all lua config section files. */
    enum
    {
        config,///< Master config section — written but never overlaid.
        whelmed,///< Whelmed renderer styles.
        nexus,///< Nexus GPU probe / runner settings.
        display,///< Display/window properties.
        graphics,///< SVG asset path registry.
        actions,///< Action registry.
        popups,///< Popup registry.
        keys,///< Key bindings.
    };

    /** @brief Populates the bimap with all 8 entries. */
    File()
    {
        map = {
            { File::config,   IDref::config   },
            { File::whelmed,  IDref::whelmed  },
            { File::nexus,    IDref::nexus    },
            { File::display,  IDref::display  },
            { File::graphics, IDref::graphics },
            { File::actions,  IDref::actions  },
            { File::popups,   IDref::popups   },
            { File::keys,     IDref::keys     },
        };
    }

    /**
     * @brief Returns the int→Identifier-stem map from the live context instance.
     *
     * Caller must ensure a config::File owner is live (end::Application is).
     */
    static const auto& get() noexcept { return getContext()->map; }

    /**
     * @brief Returns the filename for the given enum key (always "stem.lua").
     *
     * @param key  One of the File enum values.
     * @return     Filename string including extension (e.g. "display.lua").
     */
    static const juce::String getName (int key) noexcept
    {
        return jam::Format::toFileName (get().at (key), extension);
    }

    /** @brief Returns the config directory child path. */
    static const juce::File getPath (juce::StringRef child) noexcept
    {
        return path.getChildFile (child);
    }

    /** @brief User config directory: ~/.config/end/ */
    inline static const juce::File path {
        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end")
    };
    /** @brief Bare lua extension (no wildcard) for file watcher checks. */
    inline static const juce::String extension { "lua" };

private:
    const juce::String& getDefault() const noexcept override { return map.at (File::config); }
};

//==============================================================================
/**
 * @brief Registry of SVG graphics assets — 3 keys.
 *        CRTP-derived from jam::Map::Instance<Graphics>.
 *
 * Maps each enum key to its Identifier stem (e.g. tabBar → "tab_bar") and
 * resolves the on-disk filename via getName(). All keys produce "stem.svg".
 *
 * Owns the graphics subdirectory and the svg extension. The live instance
 * is owned by end::Application — static get() resolves through Context<Graphics>.
 */
struct Graphics : public jam::Map::Instance<Graphics>
{
    /** @brief Integer keys for all SVG graphics assets. */
    enum
    {
        tabBar,///< Tab bar background SVG asset.
        tabInactive,///< Inactive tab button SVG asset.
        tabActive,///< Active tab indicator SVG asset.
    };

    /** @brief Populates the bimap with all 3 entries. */
    Graphics()
    {
        map = {
            { Graphics::tabBar,      IDref::tabBar      },
            { Graphics::tabInactive, IDref::tabInactive },
            { Graphics::tabActive,   IDref::tabActive   },
        };
    }

    /**
     * @brief Returns the int→Identifier-stem map from the live context instance.
     *
     * Caller must ensure a config::Graphics owner is live (end::Application is).
     */
    static const auto& get() noexcept { return getContext()->map; }

    /**
     * @brief Returns the filename for the given enum key (always "stem.svg").
     *
     * @param key  One of the Graphics enum values.
     * @return     Filename string including extension (e.g. "tab_bar.svg").
     */
    static const juce::String getName (int key) noexcept
    {
        return jam::Format::toFileName (get().at (key), extension);
    }

    /** @brief Returns the graphics directory child path. */
    static const juce::File getPath (juce::StringRef child) noexcept
    {
        return path.getChildFile (child);
    }

    /** @brief SVG asset subdirectory: ~/.config/end/graphics/ */
    inline static const juce::File path { File::getPath (jam::IDref::graphics) };
    /** @brief Bare svg extension (no wildcard) for file watcher checks. */
    inline static const juce::String extension { "svg" };

private:
    const juce::String& getDefault() const noexcept override { return map.at (Graphics::tabBar); }
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
