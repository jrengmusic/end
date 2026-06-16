#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace end
{
/*____________________________________________________________________________*/

//==============================================================================
/**
 * @brief Bimap for component position — "top", "bottom", "left", "right", or "center".
 *
 * Used by theme.tab.orientation, theme.status_bar.position,
 * theme.action_list.position, and any future positional config fields.
 *
 * Integer keys deliberately mirror jam::button::Bar::Orientation so that
 * orientation values can be forwarded directly to Bar::setOrientation():
 *   0 → "top"
 *   1 → "right"
 *   2 → "bottom"
 *   3 → "left"
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
        right,///< Position at right.
        bottom,///< Position at bottom (default).
        left,///< Position at left.
        center,///< Centered position.
    };

    /** @brief Populates the bimap with all five entries. */
    Position()
    {
        map = {
            { Position::top,    "top"    },
            { Position::right,  "right"  },
            { Position::bottom, "bottom" },
            { Position::left,   "left"   },
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
 * Used by end.terminal.drop_multifiles.
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
 * @brief Registry of lua config files — 4 section keys.
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
        config,///< Application config (end.lua).
        end,
        popups,///< Popup terminal definitions.
        keys,///< Key bindings.
    };

    /** @brief Populates the bimap with all 4 entries. */
    File()
    {
        map = {
            { File::config, IDref::config },
            { File::end,    IDref::end    },
            { File::popups, IDref::popups },
            { File::keys,   IDref::keys   },
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
     * @return     Filename string including extension (e.g. "end.lua").
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

    //==========================================================================
    /**
     * @brief Registry of theme directory files — 2 section keys.
     *        Nested inside File. CRTP-derived from jam::Map::Instance<Theme>.
     *
     * Maps each enum key to its Identifier stem and resolves the on-disk
     * filename via getName(). All keys produce "stem.lua".
     *
     * Owns the themes root directory path. The live instance is owned by
     * end::Application — static get() resolves through Context<Theme>.
     */
    struct Theme : public jam::Map::Instance<Theme>
    {
        /** @brief Integer keys for theme lua files. */
        enum
        {
            theme,///< Visual properties (colours, fonts, metrics, graphics).
            whelmed,///< Whelmed markdown renderer styles.
        };

        /** @brief Populates the bimap with both entries. */
        Theme()
        {
            map = {
                { Theme::theme,   IDref::theme   },
                { Theme::whelmed, IDref::whelmed },
            };
        }

        /** @brief Returns the int→Identifier-stem map from the live context instance. */
        static const auto& get() noexcept { return getInstance()->map; }

        /** @brief Returns the filename for the given enum key (always "stem.lua").
         *  @param key  One of the Theme enum values.
         *  @return     Filename string including extension (e.g. "theme.lua").
         */
        static const juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (get().at (key), extension);
        }

        /** @brief Returns the theme subdirectory for the given theme name.
         *  @param themeName  Theme directory name (e.g. "gfx").
         *  @return           Resolved path: ~/.config/end/themes/@p themeName/
         */
        static const juce::File getPath (const juce::String& themeName) noexcept
        {
            return path.getChildFile (themeName);
        }

        /** @brief Themes root directory: ~/.config/end/themes/ */
        inline static const juce::File path { File::getPath (IDref::themes) };
        /** @brief Lua extension for theme files. */
        inline static const juce::String extension { "lua" };

    private:
        const juce::String& getDefault() const noexcept override { return map.at (Theme::theme); }
    };

    //==========================================================================
    /**
     * @brief Registry of Shadertoy pass file names — deterministic discovery.
     *        Nested inside File. CRTP-derived from jam::Map::Instance<Shaders>.
     *
     * Maps each enum key to its Identifier stem which IS the on-disk filename
     * (no extension — Shadertoy convention). The filesystem is the manifest:
     * present files are loaded, absent files are skipped.
     *
     * Owns the shaders root directory path. The live instance is owned by
     * end::Application — static get() resolves through Context<Shaders>.
     */
    struct Shaders : public jam::Map::Instance<Shaders>
    {
        /** @brief Integer keys for Shadertoy pass types. */
        enum
        {
            common,///< Shared library code, prepended to all passes.
            image,///< Main output pass (always present).
            bufferA,///< Intermediate FBO pass A.
            bufferB,///< Intermediate FBO pass B.
            bufferC,///< Intermediate FBO pass C.
            bufferD,///< Intermediate FBO pass D.
        };

        /** @brief Populates the bimap with all Shadertoy pass entries. */
        Shaders()
        {
            map = {
                { Shaders::common,  IDref::common  },
                { Shaders::image,   IDref::image   },
                { Shaders::bufferA, IDref::bufferA },
                { Shaders::bufferB, IDref::bufferB },
                { Shaders::bufferC, IDref::bufferC },
                { Shaders::bufferD, IDref::bufferD },
            };
        }

        /** @brief Returns the int→Identifier-stem map from the live context instance. */
        static const auto& get() noexcept { return getInstance()->map; }

        /** @brief Returns the shader project subdirectory for the given name.
         *  @param shadersName  Shader project directory name (e.g. "sea-at-night").
         *  @return             Resolved path: ~/.config/end/shaders/@p shadersName/
         */
        static const juce::File getPath (const juce::String& shadersName) noexcept
        {
            return path.getChildFile (shadersName);
        }

        /** @brief Shaders root directory: ~/.config/end/shaders/ */
        inline static const juce::File path { File::getPath (IDref::shaders) };

    private:
        const juce::String& getDefault() const noexcept override { return map.at (Shaders::image); }
    };

private:
    const juce::String& getDefault() const noexcept override { return map.at (File::config); }
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config

//==============================================================================
namespace end
{
/*____________________________________________________________________________*/
struct Map
{
    end::Position position;
    end::DropMode dropMode;
    config::File file;
    config::File::Theme theme;
    config::File::Shaders shaders;
    jam::map::WindowFX window;
    jam::map::Segment segment;
    jam::map::ButtonState button;
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
