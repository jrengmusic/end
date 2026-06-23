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
struct Position : public jam::Bimap<Position>
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

    /** @brief Returns a fused Validator for the Position value set.
     *
     *  check  — accepts any string present in the Position bimap.
     *  create — registers a ParameterText on the model for the given property.
     */
    static jam::lua::Validator getValidator()
    {
        return jam::lua::Validator {
            [] (const juce::var& v)
            {
                return v.isString() and getInstance()->contains (v.toString());
            },
            [] (jam::Model& model, juce::ValueTree& tree, const juce::Identifier& id, const juce::var& value)
            {
                model.createAndAddParameter<jam::ParameterText> (tree, id, value.toString());
            }
        };
    }
};

//==============================================================================
/**
 * @brief Bimap for multi-file drop separator mode — "space" or "newline".
 *
 * Used by init.terminal.drop_multifiles.
 *   0 → "space"   — join paths with spaces (default)
 *   1 → "newline" — join paths with newlines
 *
 * Registered in Application CONTEXT before config::Model construction.
 */
struct DropMode : public jam::Bimap<DropMode>
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

    /** @brief Returns a fused Validator for the DropMode value set.
     *
     *  check  — accepts any string present in the DropMode bimap.
     *  create — registers a ParameterText on the model for the given property.
     */
    static jam::lua::Validator getValidator()
    {
        return jam::lua::Validator {
            [] (const juce::var& v)
            {
                return v.isString() and getInstance()->contains (v.toString());
            },
            [] (jam::Model& model, juce::ValueTree& tree, const juce::Identifier& id, const juce::var& value)
            {
                model.createAndAddParameter<jam::ParameterText> (tree, id, value.toString());
            }
        };
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

//==============================================================================
namespace file
{
/*____________________________________________________________________________*/

/**
 * @brief Registry of lua config files — 3 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<Config\>.
 *
 * Maps each enum key to its Identifier stem (e.g. init → "init") and
 * resolves the on-disk filename via getName(). All keys produce "stem.lua".
 *
 * Owns the user config directory and the lua extension. The live instance
 * is owned by end::Application — static get() resolves through
 * jam::Instance\<Config\>.
 *
 * getPath() is the canonical child-path resolver used by Themes and Shaders
 * to seed their own static path members.
 */
struct Config : public jam::Bimap<Config>
{
    /** @brief Integer keys for all lua config section files. */
    enum
    {
        display,///< Application config (display.lua).
        popup,///< Popup terminal definitions.
        keys,///< Key bindings.
    };

    /** @brief Populates the bimap with all 3 entries. */
    Config()
    {
        map = {
            { Config::display, IDref::display },
            { Config::popup,   IDref::popup   },
            { Config::keys,    IDref::keys    },
        };
    }

    /**
     * @brief Returns the int→Identifier-stem map from the live context instance.
     *
     * Caller must ensure a file::Config owner is live (end::Application is).
     */
    static const auto& get() noexcept { return getInstance()->map; }

    /**
     * @brief Returns the filename for the given enum key (always "stem.lua").
     *
     * @param key  One of the Config enum values.
     * @return     Filename string including extension (e.g. "init.lua").
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
    const juce::String& getDefault() const noexcept override { return map.at (Config::display); }
};

//==============================================================================
/**
 * @brief Registry of theme directory files — 2 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<Themes\>.
 *
 * Maps each enum key to its Identifier stem and resolves the on-disk
 * filename via getName(). All keys produce "stem.lua".
 *
 * Owns the themes root directory path (seeded from File::getPath).
 * The live instance is owned by end::Application — static get() resolves
 * through jam::Instance\<Themes\>.
 */
struct Themes : public jam::Bimap<Themes>
{
    /** @brief Integer keys for theme lua files. */
    enum
    {
        theme,///< Visual properties (colours, fonts, metrics, graphics).
        whelmed,///< Whelmed markdown renderer styles.
    };

    /** @brief Populates the bimap with both entries. */
    Themes()
    {
        map = {
            { Themes::theme,   IDref::theme   },
            { Themes::whelmed, IDref::whelmed },
        };
    }

    /** @brief Returns the int→Identifier-stem map from the live context instance. */
    static const auto& get() noexcept { return getInstance()->map; }

    /**
     * @brief Returns the filename for the given enum key (always "stem.lua").
     *
     * @param key  One of the Themes enum values.
     * @return     Filename string including extension (e.g. "theme.lua").
     */
    static const juce::String getName (int key) noexcept
    {
        return jam::Format::toFileName (get().at (key), extension);
    }

    /**
     * @brief Returns the theme subdirectory for the given theme name.
     *
     * @param themeName  Theme directory name (e.g. "gfx").
     * @return           Resolved path: ~/.config/end/themes/@p themeName/
     */
    static const juce::File getPath (const juce::String& themeName) noexcept
    {
        return path.getChildFile (themeName);
    }

    /** @brief Themes root directory: ~/.config/end/themes/ */
    inline static const juce::File path { Config::getPath (IDref::themes) };
    /** @brief Lua extension for theme files. */
    inline static const juce::String extension { "lua" };

private:
    const juce::String& getDefault() const noexcept override { return map.at (Themes::theme); }
};

//==============================================================================
/**
 * @brief Registry of theme SVG graphics assets — 4 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<Flex\>.
 *
 * Maps each enum key to its Identifier stem. The on-disk filename is
 * stem + ".svg" (via getName()). The filesystem is the manifest: present
 * files are loaded into the FLEX ValueTree child nested under THEME,
 * absent files are skipped.
 *
 * Lives inside the active theme subdirectory under flex/ — no
 * independent root path. Theme resolves the directory and passes it to
 * the scan.
 *
 * The live instance is owned by end::Application — static get() resolves
 * through jam::Instance\<Flex\>.
 */
struct Flex : public jam::Bimap<Flex>
{
    /** @brief Integer keys for known SVG graphics assets. */
    enum
    {
        tabBar,///< Tab bar background.
        tabHighlight,///< Tab highlight overlay.
        tabButtonNormalOn,///< Tab button active state.
        resizerBar,///< Pane resizer bar.
    };

    /** @brief Populates the bimap with all 4 entries. */
    Flex()
    {
        map = {
            { Flex::tabBar,            IDref::tabBar            },
            { Flex::tabHighlight,      IDref::tabHighlight      },
            { Flex::tabButtonNormalOn, IDref::tabButtonNormalOn },
            { Flex::resizerBar,        IDref::resizerBar        },
        };
    }

    /** @brief Returns the int→Identifier-stem map from the live context instance. */
    static const auto& get() noexcept { return getInstance()->map; }

    /**
     * @brief Returns the filename for the given enum key ("stem.svg").
     *
     * @param key  One of the Flex enum values.
     * @return     Filename string including extension (e.g. "tab_bar.svg").
     */
    static const juce::String getName (int key) noexcept
    {
        return jam::Format::toFileName (get().at (key), extension);
    }

    /** @brief SVG extension for flex files. */
    inline static const juce::String extension { "svg" };

private:
    const juce::String& getDefault() const noexcept override { return map.at (Flex::tabBar); }
};

//==============================================================================
/**
 * @brief Registry of Shadertoy pass file names — 6 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<Shaders\>.
 *
 * Maps each enum key to its Identifier stem which IS the on-disk filename
 * (no extension — Shadertoy bare-stem convention). The filesystem is the
 * manifest: present files are loaded, absent files are skipped.
 *
 * Enum order equals render order: common (shared library, prepended),
 * buffers A–D (intermediate BufferChannel passes), image (main output, last).
 * jam::Bimap::map is std::map — iteration follows ascending key order,
 * so no manual ordering array is needed.
 *
 * Owns the shaders root directory path (seeded from File::getPath).
 * The live instance is owned by end::Application — static get() resolves
 * through jam::Instance\<Shaders\>.
 */
struct Shaders : public jam::Bimap<Shaders>
{
    /** @brief Integer keys for Shadertoy pass types.
     *
     *  Enum order = render order: common (skipped), buffers first, image
     *  last. Bimap::map is std::map — iteration follows ascending key order.
     *  loadShaders iterates the bimap directly; no manual ordering array needed.
     */
    enum
    {
        bufferA,///< Intermediate BufferChannel pass A.
        bufferB,///< Intermediate BufferChannel pass B.
        bufferC,///< Intermediate BufferChannel pass C.
        bufferD,///< Intermediate BufferChannel pass D.
        common,///< Shared library code, prepended to all passes.
        image,///< Main output pass (always present). Last = renders last.
    };

    /** @brief Populates the bimap — render order matches enum key order. */
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

    /**
     * @brief Returns the shader project subdirectory for the given name.
     *
     * @param shadersName  Shader project directory name (e.g. "singularity").
     * @return             Resolved path: ~/.config/end/shaders/@p shadersName/
     */
    static const juce::File getPath (const juce::String& shadersName) noexcept
    {
        return path.getChildFile (shadersName);
    }

    /** @brief Shaders root directory: ~/.config/end/shaders/ */
    inline static const juce::File path { Config::getPath (IDref::shaders) };

private:
    const juce::String& getDefault() const noexcept override { return map.at (Shaders::image); }
};

//==============================================================================
struct BufferChannel : public jam::Bimap<BufferChannel>
{
    /** @brief Populates the bimap — render order matches enum key order. */
    BufferChannel()
    {
        map = {
            { Shaders::bufferA, IDref::iChannel0 },
            { Shaders::bufferB, IDref::iChannel1 },
            { Shaders::bufferC, IDref::iChannel2 },
            { Shaders::bufferD, IDref::iChannel3 },
        };
    }

    /** @brief Returns the int→Identifier-stem map from the live context instance. */
    static const auto& get() noexcept { return getInstance()->map; }

private:
    const juce::String& getDefault() const noexcept override
    {
        return map.at (Shaders::bufferA);
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace file

//==============================================================================
namespace end
{
/*____________________________________________________________________________*/
struct Map
{
    end::Position position;
    end::DropMode dropMode;
    file::Config file;
    file::Themes themes;
    file::Flex flex;
    file::Shaders shaders;
    file::BufferChannel fbo;
    jam::map::WindowFX window;
    jam::map::Segment segment;
    jam::map::ButtonState button;
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
