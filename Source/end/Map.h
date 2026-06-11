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

    static const auto& get() noexcept { return getInstance()->map; }

    static const bool get (const juce::String& value) noexcept
    {
        return jam::Map::getKey (get()).at (value);
    }

    static const juce::String get (int key) noexcept { return get().at (key); }
};

//==============================================================================
/**
 * @brief Bimap for the GPU rendering backend selection.
 *
 * Maps integer keys to the three valid gpu config values:
 *   0 → "auto"   — GPU if available, CPU fallback (default)
 *   1 → "true"   — Force GPU rendering
 *   2 → "false"  — Force CPU rendering
 *
 * Registered in Application CONTEXT before config::Model construction.
 */
struct GpuMode : public jam::Map::Instance<GpuMode>
{
    /** @brief Integer keys for all GPU mode entries. */
    enum
    {
        automatic,///< Use GPU if available, CPU fallback.
        enabled,///< Force GPU rendering.
        disabled,///< Force CPU rendering.
    };

    /** @brief Populates the bimap with all three entries. */
    GpuMode()
    {
        map = {
            { GpuMode::automatic, "auto"  },
            { GpuMode::enabled,   "true"  },
            { GpuMode::disabled,  "false" },
        };
    }

    const juce::String& getDefault() const noexcept override { return map.at (GpuMode::automatic); }

    static const auto& get() noexcept { return getInstance()->map; }
};

//==============================================================================
/**
 * @brief Bimap for component position — "top", "bottom", "left", "right", or "center".
 *
 * Used by display.tab.orientation, display.status_bar.position,
 * display.action_list.position, and any future positional config fields.
 *
 * Integer keys deliberately mirror jam::button::Bar::Orientation so that
 * orientation values can be forwarded directly to Bar::setOrientation():
 *   0 → "top"
 *   1 → "bottom" (default)
 *   2 → "left"
 *   3 → "right"
 *   4 → "center"
 *
 * Registered in Application CONTEXT before config::Model construction.
 */
struct Position : public jam::Map::Instance<Position>
{
    /** @brief Integer keys for all position entries.
     *  Keys 0–3 are intentionally aligned with jam::button::Bar::Orientation
     *  so that Position::get(string) can be forwarded to Bar::setOrientation()
     *  without a secondary mapping.
     */
    enum
    {
        top,///< Position at top.
        bottom,///< Position at bottom (default).
        left,///< Position at left.
        right,///< Position at right.
        center,///< Centered position.
    };

    /** @brief Populates the bimap with all five entries. */
    Position()
    {
        map = {
            { Position::top,    "top"    },
            { Position::bottom, "bottom" },
            { Position::left,   "left"   },
            { Position::right,  "right"  },
            { Position::center, "center" },
        };
    }

    const juce::String& getDefault() const noexcept override { return map.at (Position::bottom); }

    static const auto& get() noexcept { return getInstance()->map; }

    static int get (const juce::String& value) noexcept
    {
        return jam::Map::getKey (get()).at (value);
    }

    static const juce::String& get (int key) noexcept { return getInstance()->map.at (key); }
};

//==============================================================================
/**
 * @brief Bimap for multi-file drop separator mode — "space" or "newline".
 *
 * Used by nexus.terminal.drop_multifiles.
 *   0 → "space"   — join paths with spaces (default)
 *   1 → "newline" — join paths with newlines
 *
 * Registered in Application CONTEXT before config::Model construction.
 */
struct DropMode : public jam::Map::Instance<DropMode>
{
    /** @brief Integer keys for all drop mode entries. */
    enum
    {
        space,///< Join dropped paths with spaces (default).
        newline,///< Join dropped paths with newlines.
    };

    /** @brief Populates the bimap with both entries. */
    DropMode()
    {
        map = {
            { DropMode::space,   "space"   },
            { DropMode::newline, "newline" },
        };
    }

    const juce::String& getDefault() const noexcept override { return map.at (DropMode::space); }

    static const auto& get() noexcept { return getInstance()->map; }
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
    static const auto& get() noexcept { return getInstance()->map; }

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
 * Maps each enum key to its Identifier stem and resolves the on-disk
 * filename via getName(). All keys produce "stem.svg":
 *   tabBar          → tab_bar.svg
 *   tabIndicator    → tab_indicator.svg
 *   tabButtonNormal → tab_button_normal.svg  (default slot for the tab button)
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
        tabIndicator,///< Sliding tab indicator SVG asset.
        tabButtonNormal,///< Default (normal-state) tab button SVG asset stem.
    };

    /** @brief Populates the bimap with all 3 entries. */
    Graphics()
    {
        map = {
            { Graphics::tabBar,          IDref::tabBar          },
            { Graphics::tabIndicator,    IDref::tabIndicator    },
            { Graphics::tabButtonNormal, IDref::tabButtonNormal },
        };
    }

    /**
     * @brief Returns the int→Identifier-stem map from the live context instance.
     *
     * Caller must ensure a config::Graphics owner is live (end::Application is).
     */
    static const auto& get() noexcept { return getInstance()->map; }

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

//==============================================================================
namespace end
{
/*____________________________________________________________________________*/
struct Map
{
    end::Boolean boolMap;
    end::GpuMode gpuModeMap;
    end::Position positionMap;
    end::DropMode dropModeMap;
    config::File file;
    config::Graphics graphics;
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
