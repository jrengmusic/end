#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

//==============================================================================
/**
 * @brief Bimap for multi-file drop separator mode — "space" or "newline".
 *
 * Used by init.terminal.drop_multifiles.
 *   0 → "space"   — join paths with spaces (default)
 *   1 → "newline" — join paths with newlines
 *
 * Registered in ENDApplication CONTEXT before ConfigModel construction.
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
        return jam::lua::Validator { [] (const juce::var& v)
                                     {
                                         return v.isString()
                                                and getInstance()->contains (v.toString());
                                     },
                                     [] (jam::Model& model,
                                         juce::ValueTree& tree,
                                         const juce::Identifier& id,
                                         const juce::var& value)
                                     {
                                         model.createAndAddParameter<jam::ParameterText> (
                                             tree, id, value.toString());
                                     } };
    }
};

//==============================================================================
/**
 * @brief Bimap for the glyph atlas mono rasterization backend — "edge_table",
 *        "freetype", or "native".
 *
 * Used by graphics.font_rasterizer config field. Integer keys deliberately
 * mirror jam::GlyphAtlas::Backend so the looked-up key can be forwarded
 * directly to jam::GlyphAtlas::setRasterization() via a static_cast, without a
 * secondary mapping:
 *   0 → "edge_table" — juce::Typeface::getLayersForGlyph() coverage, unhinted (default)
 *   1 → "freetype"   — autofit-hinted, stem-darkened FreeType rasterization
 *   2 → "native"     — OS-native font-smoothing (CoreText / DirectWrite)
 *
 * Registered in ENDApplication CONTEXT before ConfigModel construction.
 */
struct FontRasterizerBackend : public jam::Bimap<FontRasterizerBackend>
{
    /** @brief Integer keys mirroring jam::GlyphAtlas::Backend's enumerator order. */
    enum
    {
        edgeTable,///< Unhinted EdgeTable coverage (default).
        freetype,///< Autofit-hinted, stem-darkened FreeType rasterization.
        native,///< OS-native font-smoothing rasterization.
    };

    /** @brief Populates the bimap with all three entries. */
    FontRasterizerBackend()
    {
        map = {
            { FontRasterizerBackend::edgeTable, "edge_table" },
            { FontRasterizerBackend::freetype,  "freetype"   },
            { FontRasterizerBackend::native,    "native"     },
        };
    }

    const juce::String& getDefault() const noexcept override
    {
        return map.at (FontRasterizerBackend::edgeTable);
    }

    static const auto& get() noexcept { return getInstance()->map; }

    static int get (const juce::String& value) noexcept
    {
        return jam::Map::getKey (get()).at (value);
    }

    static const juce::String& get (int key) noexcept { return getInstance()->map.at (key); }

    /** @brief Returns a fused Validator for the FontRasterizerBackend value set.
     *
     *  check  — accepts any string present in the FontRasterizerBackend bimap.
     *  create — registers a ParameterText on the model for the given property.
     */
    static jam::lua::Validator getValidator()
    {
        return jam::lua::Validator { [] (const juce::var& v)
                                     {
                                         return v.isString()
                                                and getInstance()->contains (v.toString());
                                     },
                                     [] (jam::Model& model,
                                         juce::ValueTree& tree,
                                         const juce::Identifier& id,
                                         const juce::var& value)
                                     {
                                         model.createAndAddParameter<jam::ParameterText> (
                                             tree, id, value.toString());
                                     } };
    }
};

//==============================================================================
/**
 * @brief Bimap for the terminal caret geometry — "block", "underline", or "bar".
 *
 * Used by theme.lua cursor.style. Integer
 * keys deliberately mirror jam::CaretShape's enumerator order so the
 * looked-up key can be forwarded directly to jam::CodeView::setCaretShape()
 * via a static_cast, without a secondary mapping:
 *   0 → "block"     — DECSCUSR 0/1/2 (default)
 *   1 → "underline" — DECSCUSR 3/4
 *   2 → "bar"       — DECSCUSR 5/6
 *
 * Registered in ENDApplication CONTEXT before ConfigModel construction.
 */
struct CursorShape : public jam::Bimap<CursorShape>
{
    /** @brief Integer keys mirroring jam::CaretShape's enumerator order. */
    enum
    {
        block,///< DECSCUSR 0/1/2 (default).
        underline,///< DECSCUSR 3/4.
        bar,///< DECSCUSR 5/6.
    };

    /** @brief Populates the bimap with all three entries. */
    CursorShape()
    {
        map = {
            { CursorShape::block,     "block"     },
            { CursorShape::underline, "underline" },
            { CursorShape::bar,       "bar"       },
        };
    }

    const juce::String& getDefault() const noexcept override { return map.at (CursorShape::block); }

    static const auto& get() noexcept { return getInstance()->map; }

    static int get (const juce::String& value) noexcept
    {
        return jam::Map::getKey (get()).at (value);
    }

    static const juce::String& get (int key) noexcept { return getInstance()->map.at (key); }

    /** @brief Returns a fused Validator for the CursorShape value set.
     *
     *  check  — accepts any string present in the CursorShape bimap.
     *  create — registers a ParameterText on the model for the given property.
     */
    static jam::lua::Validator getValidator()
    {
        return jam::lua::Validator { [] (const juce::var& v)
                                     {
                                         return v.isString()
                                                and getInstance()->contains (v.toString());
                                     },
                                     [] (jam::Model& model,
                                         juce::ValueTree& tree,
                                         const juce::Identifier& id,
                                         const juce::var& value)
                                     {
                                         model.createAndAddParameter<jam::ParameterText> (
                                             tree, id, value.toString());
                                     } };
    }
};

//==============================================================================
/**
 * @brief Registry of lua config files — 3 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<FileConfig\>.
 *
 * Maps each enum key to its Identifier stem (e.g. init → "init") and
 * resolves the on-disk filename via getName(). All keys produce "stem.lua".
 *
 * Owns the user config directory and the lua extension. The live instance
 * is owned by ENDApplication — static get() resolves through
 * jam::Instance\<FileConfig\>.
 *
 * getPath() is the canonical child-path resolver used by FileThemes and FileShaders
 * to derive their own static path members.
 */
struct FileConfig : public jam::Bimap<FileConfig>
{
    /** @brief Integer keys for all lua config section files. */
    enum
    {
        display,///< Application config (display.lua).
        popup,///< Popup terminal definitions.
        keys,///< Key bindings.
    };

    /** @brief Populates the bimap with all 3 entries. */
    FileConfig()
    {
        map = {
            { FileConfig::display, IDref::display },
            { FileConfig::popup,   IDref::popup   },
            { FileConfig::keys,    IDref::keys    },
        };
    }

    /**
     * @brief Returns the int→Identifier-stem map from the live context instance.
     *
     * Caller must ensure a FileConfig owner is live (ENDApplication is).
     */
    static const auto& get() noexcept { return getInstance()->map; }

    /**
     * @brief Returns the filename for the given enum key (always "stem.lua").
     *
     * @param key  One of the FileConfig enum values.
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
    static inline const juce::File path {
        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end")
    };
    /** @brief Bare lua extension (no wildcard) for file watcher checks. */
    static inline const juce::String extension { "lua" };

private:
    const juce::String& getDefault() const noexcept override
    {
        return map.at (FileConfig::display);
    }
};

//==============================================================================
/**
 * @brief Registry of theme directory files — 2 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<FileThemes\>.
 *
 * Maps each enum key to its Identifier stem and resolves the on-disk
 * filename via getName(). All keys produce "stem.lua".
 *
 * Owns the themes root directory path (derived from FileConfig::getPath).
 * The live instance is owned by ENDApplication — static get() resolves
 * through jam::Instance\<FileThemes\>.
 */
struct FileThemes : public jam::Bimap<FileThemes>
{
    /** @brief Integer keys for theme lua files. */
    enum
    {
        theme,///< Visual properties (colours, fonts, metrics, graphics).
        whelmed,///< Whelmed markdown renderer styles.
    };

    /** @brief Populates the bimap with both entries. */
    FileThemes()
    {
        map = {
            { FileThemes::theme,   IDref::theme   },
            { FileThemes::whelmed, IDref::whelmed },
        };
    }

    /** @brief Returns the int→Identifier-stem map from the live context instance. */
    static const auto& get() noexcept { return getInstance()->map; }

    /**
     * @brief Returns the filename for the given enum key (always "stem.lua").
     *
     * @param key  One of the FileThemes enum values.
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
    static inline const juce::File path { FileConfig::getPath (IDref::themes) };
    /** @brief Lua extension for theme files. */
    static inline const juce::String extension { "lua" };

private:
    const juce::String& getDefault() const noexcept override { return map.at (FileThemes::theme); }
};

//==============================================================================
/**
 * @brief Registry of theme SVG graphics assets — 4 section keys.
 *        Top-level registry. CRTP-derived from jam::Bimap\<FileFlex\>.
 *
 * Maps each enum key to its Identifier stem. The on-disk filename is
 * stem + ".svg" (via getName()). The filesystem is the manifest: present
 * files are loaded into the FLEX ValueTree child nested under THEME,
 * absent files are skipped.
 *
 * Lives inside the active theme subdirectory under flex/ — no
 * independent root path. FileThemes resolves the directory and passes it to
 * the scan.
 *
 * The live instance is owned by ENDApplication — static get() resolves
 * through jam::Instance\<FileFlex\>.
 */
struct FileFlex : public jam::Bimap<FileFlex>
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
    FileFlex()
    {
        map = {
            { FileFlex::tabBar,            IDref::tabBar            },
            { FileFlex::tabHighlight,      IDref::tabHighlight      },
            { FileFlex::tabButtonNormalOn, IDref::tabButtonNormalOn },
            { FileFlex::resizerBar,        IDref::resizerBar        },
        };
    }

    /** @brief Returns the int→Identifier-stem map from the live context instance. */
    static const auto& get() noexcept { return getInstance()->map; }

    /**
     * @brief Returns the filename for the given enum key ("stem.svg").
     *
     * @param key  One of the FileFlex enum values.
     * @return     Filename string including extension (e.g. "tab_bar.svg").
     */
    static const juce::String getName (int key) noexcept
    {
        return jam::Format::toFileName (get().at (key), extension);
    }

    /** @brief SVG extension for flex files. */
    static inline const juce::String extension { "svg" };

private:
    const juce::String& getDefault() const noexcept override { return map.at (FileFlex::tabBar); }
};

//==============================================================================
/**
 * @brief Shader project directory resolver — plain static utility, not a
 *        jam::Bimap\<T\> registry (no fixed pass-name set exists to map:
 *        ConfigShader::loadFromPath enumerates every regular (non-hidden)
 *        file in each project directory directly, keyed by its extensionless
 *        stem -- Common/Image special-cased, every other stem a named
 *        buffer pass in lexicographic order).
 *
 * Owns the shaders root directory path (derived from FileConfig::getPath). No
 * live instance required — every member is static, so nothing needs
 * constructing in Map/ENDApplication's CONTEXT.
 */
struct FileShaders
{
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
    static inline const juce::File path { FileConfig::getPath (IDref::shaders) };
};

//==============================================================================
struct Map
{
    jam::Position position;
    DropMode dropMode;
    FileConfig file;
    FileThemes themes;
    FileFlex flex;
    FontRasterizerBackend fontRasterizerBackend;
    CursorShape cursorShape;
    jam::map::ImageResample imageResample;
    jam::map::WindowFX window;
    jam::map::Segment segment;
    jam::map::ButtonState button;
    jam::map::MouseButton mouseButton;
};
