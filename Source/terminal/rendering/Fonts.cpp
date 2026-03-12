/**
 * @file Fonts.cpp
 * @brief Linux / Windows FreeType + HarfBuzz implementation of the Fonts font manager.
 *
 * This file is compiled on all platforms **except macOS** (`!JUCE_MAC`).  It
 * implements the `Fonts` struct declared in `Fonts.h` using:
 *
 * - **FreeType** (`FT_Face`) for font loading, metrics, and rasterization.
 * - **HarfBuzz** (`hb_ft_font_create_referenced`) for Unicode shaping.
 * - **fontconfig** (Linux only) for font discovery by family name.
 *
 * ### Companion files (also compiled on non-macOS)
 * | File                | Responsibility                                    |
 * |---------------------|---------------------------------------------------|
 * | `FontsMetrics.cpp`  | `calcMetrics()` — FreeType cell geometry          |
 * | `FontsShaping.cpp`  | `shapeText()`, `shapeEmoji()`, static utilities   |
 *
 * ### Font loading sequence (`loadFaces`)
 * 1. Initialize `FT_Library` via `FT_Init_FreeType`.
 * 2. Resolve the primary font file path via `resolveFontPath()`:
 *    - Try `discoverFont (userFamily)`.
 *    - Fall back to the platform default monospace font.
 *    - Fall back to `juce::Font::getDefaultMonospacedFontName()`.
 * 3. Load the regular face with `FT_New_Face`; fall back to the embedded
 *    Display Mono TTF via `FT_New_Memory_Face` if the file path is empty or
 *    loading fails.
 * 4. Set character size via `FT_Set_Char_Size` using `baseDpi * displayScale`.
 * 5. Create the HarfBuzz shaping font via `hb_ft_font_create_referenced`.
 * 6. Load the emoji face from the system emoji font path.
 * 7. Load the Nerd Font face from `BinaryData::SymbolsNerdFontRegular_ttf` via
 *    `FT_New_Memory_Face`.
 * 8. Register the regular face and Nerd Font face with `FontCollection` at
 *    slots 0 and 1 respectively.
 *
 * ### Resize (`setSize`)
 * Calls `FT_Set_Char_Size` on all four style faces, `emojiFace`, and `nfFace`.
 * Destroys and recreates `nerdShapingFont` via `hb_ft_font_create_referenced`
 * (HarfBuzz reads metrics from the `FT_Face` at shape time, so the other
 * shaping fonts update automatically).  Updates `FontCollection` slot 1.
 *
 * ### Rasterization
 * Implemented in `FontsShaping.cpp` via `rasterizeToImage()`.  Calls
 * `FT_Load_Glyph` with `FT_LOAD_RENDER | FT_LOAD_COLOR` for emoji or
 * `FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT` for monochrome glyphs.
 *
 * @note All methods run on the **MESSAGE THREAD**.
 *
 * @see Fonts.h
 * @see FontCollection
 * @see GlyphAtlas
 */

#include "Fonts.h"

#if JUCE_MAC
// CoreText implementation in Fonts.mm
#else

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC
    #include <CoreText/CoreText.h>
#endif
#if JUCE_LINUX
    #include <fontconfig/fontconfig.h>
#endif

#include <BinaryData.h>
#include "FontCollection.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

/**
 * @brief Constructs the font manager and loads all FreeType font handles.
 *
 * Stores the user family name and point size, then calls `initialize()` which
 * initializes the FreeType library, creates the HarfBuzz scratch buffer, and
 * loads all font handles via `loadFaces()`.
 *
 * @param userFamilyName  Preferred font family (e.g. "JetBrains Mono").
 * @param pointSize       Initial font size in CSS points.
 */
Fonts::Fonts (const juce::String& userFamilyName, float pointSize)
    : userFamily (userFamilyName), fontSize (pointSize)
{
    initialize();
}

/**
 * @brief Releases all FreeType and HarfBuzz resources.
 *
 * Release order (reverse of creation):
 * 1. `scratchBuffer` (HarfBuzz).
 * 2. All four style faces: `hb_font_destroy` then `FT_Done_Face`.
 * 3. `emojiShapingFont` then `emojiFace`.
 * 4. `nerdShapingFont` then `nfFace`.
 * 5. `library` via `FT_Done_FreeType`.
 *
 * @note `FT_Done_FreeType` invalidates all `FT_Face` handles created from
 *       this library, so faces must be destroyed before the library.
 */
Fonts::~Fonts()
{
    if (scratchBuffer != nullptr)
    {
        hb_buffer_destroy (scratchBuffer);
    }

    for (int i { 0 }; i < 4; ++i)
    {
        if (faces.at (i).hbFont != nullptr)
        {
            hb_font_destroy (faces.at (i).hbFont);
        }

        if (faces.at (i).face != nullptr)
        {
            FT_Done_Face (faces.at (i).face);
        }
    }

    if (emojiShapingFont != nullptr)
    {
        hb_font_destroy (emojiShapingFont);
    }

    if (emojiFace != nullptr)
    {
        FT_Done_Face (emojiFace);
    }

    if (nerdShapingFont != nullptr)
    {
        hb_font_destroy (nerdShapingFont);
    }

    if (nfFace != nullptr)
    {
        FT_Done_Face (nfFace);
    }

    if (library != nullptr)
    {
        FT_Done_FreeType (library);
    }
}

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief No-op on Linux/Windows — embedded fonts are loaded via FT_New_Memory_Face.
 *
 * FreeType has no system-wide font registration mechanism.  Embedded fonts
 * (Display Mono variants) are loaded directly in `loadFaces()` via
 * `FT_New_Memory_Face` when the file-system path is unavailable.
 */
void Fonts::registerEmbeddedFonts()
{
    // FreeType has no system-wide registration.
    // Embedded fonts are loaded via FT_New_Memory_Face in loadFaces().
}

/**
 * @brief Initializes the FreeType library and loads all font handles.
 *
 * Calls `FT_Init_FreeType` to create the library instance.  On success,
 * creates the HarfBuzz scratch buffer and calls `loadFaces()`.  If
 * `FT_Init_FreeType` fails, the instance remains invalid (`isValid()` returns
 * false) and no further initialization is attempted.
 */
void Fonts::initialize()
{
    if (FT_Init_FreeType (&library) == 0)
    {
        scratchBuffer = hb_buffer_create();
        loadFaces();
    }
}

/**
 * @brief Resolves the absolute file path for the user's preferred font.
 *
 * Tries font discovery in this order:
 * 1. `discoverFont (userFamily)` — user-specified family.
 * 2. Platform default monospace:
 *    - macOS: "Menlo"
 *    - Linux: "DejaVu Sans Mono"
 *    - Windows: "Consolas"
 * 3. `juce::Font::getDefaultMonospacedFontName()` — JUCE fallback.
 *
 * @return Absolute path to a font file, or an empty string if no font was found.
 */
juce::String Fonts::resolveFontPath()
{
    juce::String fontPath { discoverFont (userFamily) };

    if (fontPath.isEmpty())
    {
        #if JUCE_MAC
            fontPath = discoverFont ("Menlo");
        #elif JUCE_LINUX
            fontPath = discoverFont ("DejaVu Sans Mono");
        #elif JUCE_WINDOWS
            fontPath = discoverFont ("Consolas");
        #endif
    }

    if (fontPath.isEmpty())
    {
        fontPath = discoverFont (juce::Font::getDefaultMonospacedFontName());
    }

    return fontPath;
}

// ============================================================================
// Embedded font lookup
// ============================================================================

/**
 * @brief Returns a pointer to embedded TTF data for a known Display Mono variant.
 *
 * Matches the family name (case-insensitive) against the three embedded Display
 * Mono variants in BinaryData and returns the corresponding data pointer and
 * size.
 *
 * @param family  Font family name to look up.
 * @param size    Output: size of the returned data in bytes; set to 0 if not found.
 * @return Pointer to the embedded TTF data, or `nullptr` if the family is not
 *         one of the known Display Mono variants.
 */
static const char* embeddedFontForFamily (const juce::String& family, int& size)
{
    const juce::String lower { family.toLowerCase() };
    const char* result { nullptr };

    if (lower == "display mono" or lower == "display mono book")
    {
        size = BinaryData::DisplayMonoBook_ttfSize;
        result = BinaryData::DisplayMonoBook_ttf;
    }
    else if (lower == "display mono medium")
    {
        size = BinaryData::DisplayMonoMedium_ttfSize;
        result = BinaryData::DisplayMonoMedium_ttf;
    }
    else if (lower == "display mono bold")
    {
        size = BinaryData::DisplayMonoBold_ttfSize;
        result = BinaryData::DisplayMonoBold_ttf;
    }
    else
    {
        size = 0;
    }

    return result;
}

// ============================================================================
// Face loading
// ============================================================================

/**
 * @brief Loads all FreeType font handles at the current point size.
 *
 * ### Loading sequence
 * 1. **Regular face** — resolves the font file path via `resolveFontPath()` and
 *    calls `FT_New_Face`.  If the path is empty or loading fails, falls back to
 *    `embeddedFontForFamily (userFamily)`, then to the embedded Display Mono
 *    Book TTF via `FT_New_Memory_Face`.
 * 2. Sets character size via `FT_Set_Char_Size` using
 *    `baseDpi * getDisplayScale()` as the render DPI.
 * 3. Creates the HarfBuzz shaping font via `hb_ft_font_create_referenced` and
 *    stores it in `faces[Style::regular]`.
 * 4. **Emoji face** — resolves the emoji font path via `discoverEmojiFont()`,
 *    loads with `FT_New_Face`, sets character size, creates `emojiShapingFont`.
 * 5. **FontCollection** — if a `FontCollection` context is available:
 *    - Registers the regular face at slot 0 and calls `populateFromCmap (0)`.
 *    - Loads the Nerd Font from `BinaryData::SymbolsNerdFontRegular_ttf` via
 *      `FT_New_Memory_Face`, sets character size, creates `nerdShapingFont`,
 *      registers at slot 1, calls `populateFromCmap (1)`.
 *
 * @note Only the `Style::regular` slot is populated.  Bold/italic variants are
 *       not currently loaded on the FreeType backend.
 */
void Fonts::loadFaces()
{
    const FT_UInt renderDpi { static_cast<FT_UInt> (static_cast<float> (baseDpi) * getDisplayScale()) };
    const FT_F26Dot6 size26_6 { static_cast<FT_F26Dot6> (fontSize * ftFixedScale) };

    FT_Face face { nullptr };
    const juce::String fontPath { resolveFontPath() };

    if (fontPath.isNotEmpty())
    {
        FT_New_Face (library, fontPath.toRawUTF8(), 0, &face);
    }

    if (face == nullptr)
    {
        int dataSize { 0 };
        const char* data { embeddedFontForFamily (userFamily, dataSize) };

        if (data == nullptr)
        {
            data = embeddedFontForFamily ("Display Mono", dataSize);
        }

        if (data != nullptr)
        {
            FT_New_Memory_Face (library, reinterpret_cast<const FT_Byte*> (data), dataSize, 0, &face);
        }
    }

    if (face != nullptr)
    {
        FT_Set_Char_Size (face, 0, size26_6, renderDpi, renderDpi);
        faces.at (static_cast<int> (Style::regular)).face = face;
        faces.at (static_cast<int> (Style::regular)).hbFont = hb_ft_font_create_referenced (face);
    }

    const juce::String emojiPath { discoverEmojiFont() };
    if (emojiPath.isNotEmpty())
    {
        FT_New_Face (library, emojiPath.toRawUTF8(), 0, &emojiFace);

        if (emojiFace != nullptr)
        {
            FT_Set_Char_Size (emojiFace, 0, size26_6, renderDpi, renderDpi);
            emojiShapingFont = hb_ft_font_create_referenced (emojiFace);
        }
    }

    auto* fc { FontCollection::getContext() };

    if (fc != nullptr)
    {
        if (faces.at (static_cast<int> (Style::regular)).face != nullptr)
        {
            FontCollection::Entry dmEntry;
            dmEntry.ftFace = faces.at (static_cast<int> (Style::regular)).face;
            dmEntry.hbFont = faces.at (static_cast<int> (Style::regular)).hbFont;
            fc->addFont (dmEntry);
            fc->populateFromCmap (0);
        }

        FT_New_Memory_Face (library,
                            reinterpret_cast<const FT_Byte*> (BinaryData::SymbolsNerdFontRegular_ttf),
                            BinaryData::SymbolsNerdFontRegular_ttfSize,
                            0,
                            &nfFace);

        if (nfFace != nullptr)
        {
            FT_Set_Char_Size (nfFace, 0, size26_6, renderDpi, renderDpi);
            nerdShapingFont = hb_ft_font_create_referenced (nfFace);

            FontCollection::Entry nfEntry;
            nfEntry.ftFace = nfFace;
            nfEntry.hbFont = nerdShapingFont;
            fc->addFont (nfEntry);
            fc->populateFromCmap (1);
        }
    }
}

// ============================================================================
// Accessors
// ============================================================================

/**
 * @brief Returns the FreeType face for the given style.
 *
 * Falls back to the regular face if the requested style slot is null (i.e. the
 * bold/italic variant was not loaded).
 *
 * @param style  Desired style variant.
 * @return FreeType face handle; falls back to `faces[regular].face`.
 *         Returns `nullptr` only if the regular face itself failed to load.
 */
FT_Face Fonts::getFace (Style style) noexcept
{
    const auto idx { static_cast<int> (style) };
    FT_Face result { faces.at (static_cast<int> (Style::regular)).face };

    if (faces.at (idx).face != nullptr)
    {
        result = faces.at (idx).face;
    }

    return result;
}

/**
 * @brief Returns the HarfBuzz shaping font for the given style.
 *
 * Falls back to the regular shaping font if the requested style slot is null.
 *
 * @param style  Desired style variant.
 * @return HarfBuzz font pointer; falls back to `faces[regular].hbFont`.
 *         Returns `nullptr` only if the regular face failed to load.
 */
hb_font_t* Fonts::getHbFont (Style style) noexcept
{
    const auto idx { static_cast<int> (style) };
    hb_font_t* result { faces.at (static_cast<int> (Style::regular)).hbFont };

    if (faces.at (idx).hbFont != nullptr)
    {
        result = faces.at (idx).hbFont;
    }

    return result;
}

/**
 * @brief Returns the FreeType face for the emoji font.
 *
 * @return `emojiFace`, or `nullptr` if the emoji font was not found during
 *         `loadFaces()`.
 */
FT_Face Fonts::getEmojiFace() noexcept
{
    return emojiFace;
}

/**
 * @brief Returns the FreeType face for the given style as an opaque `void*`.
 *
 * Casts the result of `getFace (style)` to `void*` for use as a `GlyphKey`
 * font handle in the atlas cache.
 *
 * @param style  Desired style variant.
 * @return `FT_Face` cast to `void*`.
 */
void* Fonts::getFontHandle (Style style) noexcept
{
    return static_cast<void*> (getFace (style));
}

/**
 * @brief Returns the emoji FreeType face as an opaque `void*`.
 *
 * @return `emojiFace` cast to `void*`, or `nullptr` if not loaded.
 */
void* Fonts::getEmojiFontHandle() noexcept
{
    return static_cast<void*> (emojiFace);
}

/**
 * @brief Returns the physical pixels-per-em for the given style.
 *
 * Reads `face->size->metrics.x_ppem` from the FreeType face, which reflects
 * the current character size set by `FT_Set_Char_Size`.
 *
 * @param style  Desired style variant.
 * @return Pixels per em at the current size and display scale, or `0.0f` if
 *         the face is null.
 */
float Fonts::getPixelsPerEm (Style style) noexcept
{
    const FT_Face face { getFace (style) };
    float result { 0.0f };

    if (face != nullptr)
    {
        result = static_cast<float> (face->size->metrics.x_ppem);
    }

    return result;
}

// ============================================================================
// Size
// ============================================================================

/**
 * @brief Resizes all FreeType font handles to a new point size.
 *
 * Called when the user zooms in/out (Cmd+/Cmd-/Cmd+0).  Performs:
 *
 * 1. Calls `FT_Set_Char_Size` on all four style faces (skipping null slots).
 * 2. Calls `FT_Set_Char_Size` on `emojiFace`.
 * 3. Calls `FT_Set_Char_Size` on `nfFace`.
 * 4. Destroys `nerdShapingFont` and recreates it via
 *    `hb_ft_font_create_referenced (nfFace)`.
 * 5. Updates `FontCollection` slot 1 with the new `nfFace` and
 *    `nerdShapingFont` pointers.
 *
 * @param pointSize  New font size in CSS points.
 *
 * @note HarfBuzz shaping fonts created with `hb_ft_font_create_referenced`
 *       read metrics directly from the `FT_Face` at shape time, so the regular
 *       and emoji shaping fonts update automatically when `FT_Set_Char_Size` is
 *       called.  Only `nerdShapingFont` is explicitly recreated to ensure
 *       reliable metric updates.
 *
 * @note The render DPI is `baseDpi * getDisplayScale()`.
 */
void Fonts::setSize (float pointSize) noexcept
{
    fontSize = pointSize;
    const FT_UInt renderDpi { static_cast<FT_UInt> (static_cast<float> (baseDpi) * getDisplayScale()) };
    const FT_F26Dot6 size26_6 { static_cast<FT_F26Dot6> (fontSize * ftFixedScale) };

    for (int i { 0 }; i < 4; ++i)
    {
        if (faces.at (i).face != nullptr)
        {
            FT_Set_Char_Size (faces.at (i).face, 0, size26_6, renderDpi, renderDpi);
        }
    }

    if (emojiFace != nullptr)
    {
        FT_Set_Char_Size (emojiFace, 0, size26_6, renderDpi, renderDpi);
    }

    if (nfFace != nullptr)
    {
        FT_Set_Char_Size (nfFace, 0, size26_6, renderDpi, renderDpi);
    }

    if (nerdShapingFont != nullptr)
    {
        hb_font_destroy (nerdShapingFont);
        nerdShapingFont = nullptr;
    }

    if (nfFace != nullptr)
    {
        nerdShapingFont = hb_ft_font_create_referenced (nfFace);
    }

    auto* ctx { FontCollection::getContext() };

    if (ctx != nullptr)
    {
        auto* nfEntry { ctx->getEntry (1) };

        if (nfEntry != nullptr)
        {
            nfEntry->ftFace = nfFace;
            nfEntry->hbFont = nerdShapingFont;
        }
    }
}

// ============================================================================
// Font Discovery
// ============================================================================

/**
 * @brief Dispatches font discovery to the platform-specific implementation.
 *
 * - macOS: `discoverFontMac (familyName)` (CoreText).
 * - Linux: `discoverFontLinux (familyName)` (fontconfig).
 * - Windows: returns empty string (not yet implemented).
 *
 * @param familyName  Font family name to look up.
 * @return Absolute path to the font file, or empty if not found.
 */
juce::String Fonts::discoverFont (const juce::String& familyName)
{
    #if JUCE_MAC
        return discoverFontMac (familyName);
    #elif JUCE_LINUX
        return discoverFontLinux (familyName);
    #elif JUCE_WINDOWS
        return discoverFontWindows (familyName);
    #else
        (void) familyName;
        return {};
    #endif
}

#if JUCE_MAC
/**
 * @brief Resolves a font family name to an absolute file path using CoreText.
 *
 * Creates a `CTFontDescriptor` for the family name, instantiates a temporary
 * `CTFontRef`, then extracts the file URL via `kCTFontURLAttribute` and
 * converts it to a POSIX path string.
 *
 * @note This overload exists in `Fonts.cpp` for the case where the FreeType
 *       backend is compiled on macOS (e.g. for testing).  In production the
 *       macOS build uses `Fonts.mm` exclusively.
 *
 * @param familyName  Font family name to look up.
 * @return Absolute POSIX path to the font file, or empty if not found.
 */
juce::String Fonts::discoverFontMac (const juce::String& familyName)
{
    juce::String result;

    auto cfFamilyName { familyName.toCFString() };
    auto descriptor { CTFontDescriptorCreateWithNameAndSize (cfFamilyName, fontSize) };
    CFRelease (cfFamilyName);

    if (descriptor != nullptr)
    {
        auto fontRef { CTFontCreateWithFontDescriptor (descriptor, fontSize, nullptr) };
        CFRelease (descriptor);

        if (fontRef != nullptr)
        {
            auto urlRef { static_cast<CFURLRef> (CTFontCopyAttribute (fontRef, kCTFontURLAttribute)) };
            CFRelease (fontRef);

            if (urlRef != nullptr)
            {
                auto pathRef { CFURLCopyFileSystemPath (urlRef, kCFURLPOSIXPathStyle) };
                CFRelease (urlRef);

                if (pathRef != nullptr)
                {
                    std::array<char, 1024> buffer {};
                    CFStringGetCString (pathRef, buffer.data(), static_cast<CFIndex> (buffer.size()), kCFStringEncodingUTF8);
                    CFRelease (pathRef);
                    result = juce::String (buffer.data());
                }
            }
        }
    }

    return result;
}
#endif

#if JUCE_LINUX
/**
 * @brief Resolves a font family name to an absolute file path using fontconfig.
 *
 * Builds an `FcPattern` requesting the given family with `FC_MONO` spacing,
 * applies `FcConfigSubstitute` and `FcDefaultSubstitute` for normalization,
 * then calls `FcFontMatch` to find the best match.  Extracts the `FC_FILE`
 * property from the result.
 *
 * @param familyName  Font family name to look up (e.g. "DejaVu Sans Mono").
 * @return Absolute path to the font file (e.g. `/usr/share/fonts/...`), or
 *         empty if fontconfig cannot find a match.
 */
juce::String Fonts::discoverFontLinux (const juce::String& familyName)
{
    juce::String path;

    auto config { FcInitLoadConfigAndFonts() };

    if (config != nullptr)
    {
        auto pattern { FcPatternCreate() };
        FcPatternAddString (pattern, FC_FAMILY, reinterpret_cast<const FcChar8*> (familyName.toRawUTF8()));
        FcPatternAddInteger (pattern, FC_SPACING, FC_MONO);
        FcConfigSubstitute (config, pattern, FcMatchPattern);
        FcDefaultSubstitute (pattern);

        FcResult fcResult;
        auto matched { FcFontMatch (config, pattern, &fcResult) };
        FcPatternDestroy (pattern);

        if (matched != nullptr)
        {
            FcChar8* file { nullptr };

            if (FcPatternGetString (matched, FC_FILE, 0, &file) == FcResultMatch)
            {
                path = juce::String (reinterpret_cast<const char*> (file));
            }

            FcPatternDestroy (matched);
        }

        FcConfigDestroy (config);
    }

    return path;
}
#endif

#if JUCE_WINDOWS
/**
 * @brief Discovers a font file path on Windows via the font registry.
 *
 * Enumerates `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`
 * looking for an entry whose name starts with the requested family.
 * The registry value is either an absolute path or a filename relative
 * to the system Fonts directory.
 *
 * @param familyName  Font family name to look up (e.g. "Cascadia Code").
 * @return Absolute path to the font file, or empty if not found.
 */
juce::String Fonts::discoverFontWindows (const juce::String& familyName)
{
    juce::String path;
    const juce::String target { familyName.toLowerCase() };

    HKEY hKey { nullptr };
    const LONG result { RegOpenKeyExW (HKEY_LOCAL_MACHINE,
                                       L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                                       0, KEY_READ, &hKey) };

    if (result == ERROR_SUCCESS and hKey != nullptr)
    {
        DWORD index { 0 };
        wchar_t valueName[512];
        DWORD valueNameLen { 512 };
        BYTE valueData[512];
        DWORD valueDataLen { 512 };
        DWORD valueType { 0 };

        while (RegEnumValueW (hKey, index, valueName, &valueNameLen,
                              nullptr, &valueType, valueData, &valueDataLen) == ERROR_SUCCESS)
        {
            if (valueType == REG_SZ)
            {
                const juce::String entryName { juce::String (valueName).toLowerCase() };

                if (entryName.startsWith (target))
                {
                    const juce::String filename { juce::String (reinterpret_cast<const wchar_t*> (valueData)) };

                    if (juce::File::isAbsolutePath (filename))
                    {
                        path = filename;
                    }
                    else
                    {
                        wchar_t winDir[MAX_PATH];
                        GetWindowsDirectoryW (winDir, MAX_PATH);
                        path = juce::String (winDir) + "\\Fonts\\" + filename;
                    }

                    break;
                }
            }

            ++index;
            valueNameLen = 512;
            valueDataLen = 512;
        }

        RegCloseKey (hKey);
    }

    return path;
}
#endif

/**
 * @brief Returns the file path of the system emoji font.
 *
 * Dispatches to `discoverFont` with the platform-appropriate family name:
 * - macOS: "Apple Color Emoji" (resolved by CoreText).
 * - Linux: "Noto Color Emoji" (resolved by fontconfig).
 * - Windows: returns empty string (not yet implemented).
 *
 * @return Absolute path to the emoji font file, or empty if not found.
 */
juce::String Fonts::discoverEmojiFont()
{
    #if JUCE_MAC
        return discoverFont ("Apple Color Emoji");
    #elif JUCE_LINUX
        return discoverFont ("Noto Color Emoji");
    #elif JUCE_WINDOWS
        return discoverFont ("Segoe UI Emoji");
    #else
        return {};
    #endif
}

/**
 * @brief Rasterizes a Unicode codepoint to a JUCE ARGB image using FreeType.
 *
 * ### Font selection priority
 * 1. Regular face (`getFace (Style::regular)`).
 * 2. `emojiFace` — if the regular face has no glyph for `codepoint`.
 * 3. `nfFace` — if neither regular nor emoji has a glyph.
 * 4. Solid white rectangle — special-case for U+2588 FULL BLOCK.
 *
 * ### Rendering
 * - **Color emoji** (`FT_PIXEL_MODE_BGRA`): `FT_LOAD_RENDER | FT_LOAD_COLOR`.
 *   Pixels are in pre-multiplied BGRA order; copied directly to JUCE ARGB.
 * - **Monochrome glyphs** (`FT_PIXEL_MODE_GRAY`): `FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT`.
 *   Each grayscale byte becomes the alpha channel of a white pixel in the
 *   output ARGB image.
 *
 * The output image width is a multiple of `physCellW` to support wide glyphs.
 * Glyph pixels are placed at `(bitmap_left, physBaseline - bitmap_top)` within
 * the image, clipped to the image bounds.
 *
 * @param codepoint  Unicode scalar value to rasterize.
 * @param fontSize   Cell height in logical pixels (used to query metrics).
 * @param isColor    Set to `true` if the emoji face was used; `false` otherwise.
 * @return ARGB `juce::Image` in physical pixels, or an invalid image on failure.
 */
juce::Image Fonts::rasterizeToImage (uint32_t codepoint, float fontSize, bool& isColor) noexcept
{
    juce::Image result;
    isColor = false;

    FT_Face face { getFace (Style::regular) };

    if (face != nullptr)
    {
        const Metrics metrics { calcMetrics (fontSize) };

        if (metrics.isValid())
        {
            FT_UInt glyphIndex { FT_Get_Char_Index (face, codepoint) };
            bool useEmojiFont { false };

            if (glyphIndex == 0 and emojiFace != nullptr)
            {
                const FT_UInt emojiGlyphIndex { FT_Get_Char_Index (emojiFace, codepoint) };

                if (emojiGlyphIndex != 0)
                {
                    face = emojiFace;
                    glyphIndex = emojiGlyphIndex;
                    useEmojiFont = true;
                    isColor = true;
                }
            }

            if (glyphIndex == 0 and not useEmojiFont and nfFace != nullptr)
            {
                const FT_UInt nfGlyphIndex { FT_Get_Char_Index (nfFace, codepoint) };

                if (nfGlyphIndex != 0)
                {
                    face = nfFace;
                    glyphIndex = nfGlyphIndex;
                }
            }

            if (glyphIndex == 0 and codepoint == 0x2588)
            {
                result = juce::Image (juce::Image::ARGB, metrics.physCellW, metrics.physCellH, false);
                juce::Image::BitmapData data (result, juce::Image::BitmapData::writeOnly);

                for (int y { 0 }; y < metrics.physCellH; ++y)
                {
                    for (int x { 0 }; x < metrics.physCellW; ++x)
                    {
                        data.setPixelColour (x, y, juce::Colours::white);
                    }
                }
            }
            else if (glyphIndex != 0)
            {
                const int loadFlags { useEmojiFont
                    ? (FT_LOAD_RENDER | FT_LOAD_COLOR)
                    : (FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT) };

                if (FT_Load_Glyph (face, glyphIndex, loadFlags) == 0)
                {
                    const FT_Bitmap& bitmap { face->glyph->bitmap };

                    if (bitmap.width > 0 and bitmap.rows > 0)
                    {
                        const int advance { static_cast<int> (face->glyph->metrics.horiAdvance / ftFixedScale) };
                        const int offsetX { face->glyph->bitmap_left };
                        const int glyphRight { offsetX + static_cast<int> (bitmap.width) };
                        const int effectiveW { std::max (advance, glyphRight) };
                        const int cellSpan { std::max (1, (effectiveW + metrics.physCellW - 1) / metrics.physCellW) };
                        const int imageW { cellSpan * metrics.physCellW };

                        result = juce::Image (juce::Image::ARGB, imageW, metrics.physCellH, true);
                        juce::Image::BitmapData data (result, juce::Image::BitmapData::writeOnly);

                        const int offsetY { metrics.physBaseline - face->glyph->bitmap_top };

                        if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
                        {
                            for (unsigned int y { 0 }; y < bitmap.rows; ++y)
                            {
                                const int destY { offsetY + static_cast<int> (y) };

                                if (destY >= 0 and destY < metrics.physCellH)
                                {
                                    const unsigned char* src { bitmap.buffer + static_cast<ptrdiff_t> (y) * std::abs (bitmap.pitch) };

                                    for (unsigned int x { 0 }; x < bitmap.width; ++x)
                                    {
                                        const int destX { offsetX + static_cast<int> (x) };

                                        if (destX >= 0 and destX < imageW)
                                        {
                                            const uint8_t b { src[x * 4] };
                                            const uint8_t g { src[x * 4 + 1] };
                                            const uint8_t r { src[x * 4 + 2] };
                                            const uint8_t a { src[x * 4 + 3] };
                                            data.setPixelColour (destX, destY, juce::Colour (r, g, b, a));
                                        }
                                    }
                                }
                            }
                        }
                        else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
                        {
                            for (unsigned int y { 0 }; y < bitmap.rows; ++y)
                            {
                                const int destY { offsetY + static_cast<int> (y) };

                                if (destY >= 0 and destY < metrics.physCellH)
                                {
                                    const unsigned char* src { bitmap.buffer + static_cast<ptrdiff_t> (y) * std::abs (bitmap.pitch) };

                                    for (unsigned int x { 0 }; x < bitmap.width; ++x)
                                    {
                                        const int destX { offsetX + static_cast<int> (x) };

                                        if (destX >= 0 and destX < imageW)
                                        {
                                            const uint8_t alpha { src[x] };
                                            data.setPixelColour (destX, destY,
                                                                 juce::Colour (static_cast<uint8_t> (255),
                                                                               static_cast<uint8_t> (255),
                                                                               static_cast<uint8_t> (255),
                                                                               alpha));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

#endif
