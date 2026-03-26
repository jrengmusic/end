/**
 * @file jreng_typeface.mm
 * @brief macOS CoreText + HarfBuzz implementation of the Typeface font manager.
 *
 * This file is compiled **only on macOS** (`JUCE_MAC`).  It implements the
 * `jreng::Typeface` struct declared in `jreng_font.h` using:
 *
 * - **CoreText** (`CTFontRef`) for font loading, metrics, and rasterization.
 * - **HarfBuzz** (`hb_coretext_font_create`) for Unicode shaping.
 *
 * ### Font loading sequence (`loadFaces`)
 * 1. Register embedded Display Mono TTFs with CoreText via
 *    `CTFontManagerRegisterGraphicsFont` (once, guarded by a static flag).
 * 2. Create `mainFont` from the user-supplied family name via
 *    `CTFontDescriptorCreateWithNameAndSize`; fall back to "Display Mono" if
 *    the family is not found.
 * 3. Create `emojiFont` for "Apple Color Emoji".
 * 4. Create `identityFont` for "Display Mono" and register it with
 *    `Registry` at slot 0.
 * 5. Load `nerdFont` (Symbols Nerd Font) from BinaryData via
 *    `CGDataProviderCreateWithData` + `CTFontCreateWithGraphicsFont` and
 *    register it with `Registry` at slot 1.
 *
 * ### Resize (`setSize`)
 * Creates new `CTFontRef` copies via `CTFontCreateCopyWithAttributes` for
 * every managed font, destroys and recreates all paired HarfBuzz shaping fonts,
 * clears `fallbackFontCache`, and updates the live `Registry` entries.
 *
 * ### Rasterization (`rasterizeToImage`)
 * Draws glyphs into a `CGBitmapContext` using `CTFontDrawGlyphs`.  Color emoji
 * use an ARGB context; monochrome glyphs use a grayscale context and are
 * converted to white-on-transparent ARGB.
 *
 * @note All methods run on the **MESSAGE THREAD**.
 *
 * @see jreng_font.h
 * @see jreng::Typeface::Registry
 * @see jreng::Glyph::Atlas
 */

// Included via unity build (jreng_glyph.mm) — jreng_glyph.h already in scope
#include <BinaryData.h>

#include <algorithm>

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

#include <hb-coretext.h>

// ============================================================================
// Helpers
// ============================================================================

namespace jreng
{

/**
 * @brief Casts an opaque `void*` font handle back to a `CTFontRef`.
 *
 * All CoreText font handles are stored as `void*` in the shared header to
 * avoid exposing CoreText types.  This helper restores the type safely.
 *
 * @param ptr  Opaque pointer previously obtained from a `CTFontRef`.
 * @return The original `CTFontRef`.
 */
static CTFontRef toCTFont (void* ptr) noexcept
{
    return static_cast<CTFontRef> (ptr);
}

/**
 * @brief Looks up the `CGGlyph` index for a Unicode codepoint in a CTFont.
 *
 * Converts the codepoint to UTF-16 (handling surrogate pairs for codepoints
 * above U+FFFF) and calls `CTFontGetGlyphsForCharacters`.
 *
 * @param font       CoreText font to query.
 * @param codepoint  Unicode scalar value (U+0000–U+10FFFF).
 * @return `CGGlyph` index, or `0` if the font has no glyph for this codepoint.
 */
static CGGlyph glyphForCodepoint (CTFontRef font, uint32_t codepoint) noexcept
{
    UniChar utf16[2] {};
    int utf16Len { 0 };

    if (codepoint <= 0xFFFF)
    {
        utf16[0] = static_cast<UniChar> (codepoint);
        utf16Len = 1;
    }
    else
    {
        const uint32_t cp { codepoint - 0x10000 };
        utf16[0] = static_cast<UniChar> (0xD800 + (cp >> 10));
        utf16[1] = static_cast<UniChar> (0xDC00 + (cp & 0x3FF));
        utf16Len = 2;
    }

    CGGlyph glyphs[2] {};
    CTFontGetGlyphsForCharacters (font, utf16, glyphs, utf16Len);
    return glyphs[0];
}

} // namespace jreng

// ============================================================================
// Constructor / Destructor
// ============================================================================

/**
 * @brief Constructs the font manager and loads all CoreText font handles.
 *
 * Stores the user family name and point size, then calls `initialize()` which
 * registers embedded fonts, creates the HarfBuzz scratch buffer, and loads
 * all font handles via `loadFaces()`.
 *
 * @param fontRegistry    Font slot registry populated during `loadFaces()`.
 * @param userFamilyName  Preferred font family (e.g. "JetBrains Mono").
 * @param pointSize       Initial font size in CSS points.
 */
jreng::Typeface::Typeface (Registry& fontRegistry,
                   const juce::String& userFamilyName,
                   float pointSize,
                   jreng::Glyph::AtlasSize atlasSize,
                   bool shouldBeMonospace)
    : registry (fontRegistry), userFamily (userFamilyName), fontSize (pointSize), atlas (atlasSize), isMonospace (shouldBeMonospace)
{
    initialize();
}

/**
 * @brief Releases all CoreText and HarfBuzz resources.
 *
 * Release order (reverse of creation):
 * 1. All `CTFontRef` values in `fallbackFontCache`.
 * 2. `scratchBuffer` (HarfBuzz).
 * 3. `nerdShapingFont` → `nerdFont`.
 * 4. `identityShapingFont` → `identityFont`.
 * 5. `emojiShapingFont` → `shapingFont`.
 * 6. `emojiFont` → `mainFont`.
 */
jreng::Typeface::~Typeface()
{
    for (auto& pair : fallbackFontCache)
    {
        if (pair.second != nullptr)
        {
            CFRelease (jreng::toCTFont (pair.second));
        }
    }

    if (scratchBuffer != nullptr)
    {
        hb_buffer_destroy (scratchBuffer);
    }

    if (nerdShapingFont != nullptr)
    {
        hb_font_destroy (nerdShapingFont);
    }

    if (nerdFont != nullptr)
    {
        CFRelease (jreng::toCTFont (nerdFont));
    }

    if (identityShapingFont != nullptr)
    {
        hb_font_destroy (identityShapingFont);
    }

    if (identityFont != nullptr)
    {
        CFRelease (jreng::toCTFont (identityFont));
    }

    if (emojiShapingFont != nullptr)
    {
        hb_font_destroy (emojiShapingFont);
    }

    if (shapingFont != nullptr)
    {
        hb_font_destroy (shapingFont);
    }

    if (emojiFont != nullptr)
    {
        CFRelease (jreng::toCTFont (emojiFont));
    }

    if (mainFont != nullptr)
    {
        CFRelease (jreng::toCTFont (mainFont));
    }
}

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Registers embedded Display Mono fonts with CoreText (once).
 *
 * Iterates over the three Display Mono variants in BinaryData
 * (`DisplayMonoBook_ttf`, `DisplayMonoMedium_ttf`, `DisplayMonoBold_ttf`),
 * creates a `CGDataProvider` for each, converts it to a `CGFont`, and
 * registers it via `CTFontManagerRegisterGraphicsFont`.
 *
 * After registration, CoreText can resolve "Display Mono" by family name
 * without any file-system access.
 *
 * @note Guarded by a `static bool registered` flag — safe to call multiple
 *       times; registration happens exactly once per process.
 */
void jreng::Typeface::registerEmbeddedFonts()
{
    static bool registered { false };

    if (not registered)
    {
        registered = true;

        struct EmbeddedFont { const char* data; int size; };
        const std::array<EmbeddedFont, 3> fonts {{
            { BinaryData::DisplayMonoBook_ttf,   BinaryData::DisplayMonoBook_ttfSize },
            { BinaryData::DisplayMonoMedium_ttf, BinaryData::DisplayMonoMedium_ttfSize },
            { BinaryData::DisplayMonoBold_ttf,   BinaryData::DisplayMonoBold_ttfSize }
        }};

        for (const auto& font : fonts)
        {
            auto dataProvider { CGDataProviderCreateWithData (nullptr,
                                                              font.data,
                                                              static_cast<size_t> (font.size),
                                                              nullptr) };

            if (dataProvider != nullptr)
            {
                auto cgFont { CGFontCreateWithDataProvider (dataProvider) };
                CGDataProviderRelease (dataProvider);

                if (cgFont != nullptr)
                {
                    CFErrorRef error { nullptr };
                    CTFontManagerRegisterGraphicsFont (cgFont, &error);

                    if (error != nullptr)
                        CFRelease (error);

                    CGFontRelease (cgFont);
                }
            }
        }
    }
}

/**
 * @brief Initializes the CoreText backend and loads all font handles.
 *
 * Calls `registerEmbeddedFonts()` to make Display Mono available by name,
 * creates the HarfBuzz scratch buffer, then calls `loadFaces()`.
 */
void jreng::Typeface::initialize()
{
    registerEmbeddedFonts();
    scratchBuffer = hb_buffer_create();
    loadFaces();
}

/**
 * @brief No-op on macOS — font path resolution is not used by CoreText.
 *
 * CoreText resolves fonts by family name, not file path.  This method exists
 * to satisfy the shared interface; the FreeType backend uses it to locate font
 * files on disk.
 *
 * @return Always returns an empty string on macOS.
 */
juce::String jreng::Typeface::resolveFontPath()
{
    return {};
}

/**
 * @brief Loads all CoreText font handles at the current point size.
 *
 * ### Loading sequence
 * 1. **mainFont** — `CTFontDescriptorCreateWithNameAndSize` for `userFamily`
 *    (or "Menlo" if empty).  Falls back to "Display Mono" if the descriptor
 *    fails.  Creates `shapingFont` via `hb_coretext_font_create`.
 * 2. **emojiFont** — `CTFontCreateWithName` for the family returned by
 *    `discoverEmojiFont()` ("Apple Color Emoji").  Creates `emojiShapingFont`.
 * 3. **identityFont** — `CTFontCreateWithName` for "Display Mono".  Creates
 *    `identityShapingFont`.  Registers with `Registry` at slot 0 and
 *    calls `populateFromCmap (0)`.
 * 4. **nerdFont** — Loaded from `BinaryData::SymbolsNerdFontRegular_ttf` via
 *    `CGDataProviderCreateWithData` + `CTFontCreateWithGraphicsFont`.  Creates
 *    `nerdShapingFont`.  Registers with `Registry` at slot 1 and calls
 *    `populateFromCmap (1)`.
 *
 * All font sizes are scaled by `getDisplayScale()` to produce physical pixels.
 *
 * @note Called once from `initialize()`.  Subsequent size changes go through
 *       `setSize()` which updates handles in-place.
 */
void jreng::Typeface::loadFaces()
{
    const juce::String familyName { userFamily.isNotEmpty() ? userFamily : juce::String ("Menlo") };
    const float displayScale { getDisplayScale() };
    const CGFloat scaledSize { static_cast<CGFloat> (fontSize * displayScale) };

    auto cfName { familyName.toCFString() };
    auto descriptor { CTFontDescriptorCreateWithNameAndSize (cfName, scaledSize) };
    CFRelease (cfName);

    if (descriptor != nullptr)
    {
        auto font { CTFontCreateWithFontDescriptor (descriptor, scaledSize, nullptr) };
        CFRelease (descriptor);

        if (font != nullptr)
        {
            mainFont = const_cast<void*> (static_cast<const void*> (font));
            shapingFont = hb_coretext_font_create (font);
        }
    }

    if (mainFont == nullptr)
    {
        auto fallbackName { CFSTR ("Display Mono") };
        auto font { CTFontCreateWithName (fallbackName, scaledSize, nullptr) };

        if (font != nullptr)
        {
            mainFont = const_cast<void*> (static_cast<const void*> (font));
            shapingFont = hb_coretext_font_create (font);
        }
    }

    const juce::String emojiFamily { discoverEmojiFont() };
    auto cfEmojiName { emojiFamily.toCFString() };
    auto emojiFontRef { CTFontCreateWithName (cfEmojiName, scaledSize, nullptr) };
    CFRelease (cfEmojiName);

    if (emojiFontRef != nullptr)
    {
        emojiFont = const_cast<void*> (static_cast<const void*> (emojiFontRef));
        emojiShapingFont = hb_coretext_font_create (emojiFontRef);
    }

    auto dmFont { CTFontCreateWithName (CFSTR ("Display Mono"), scaledSize, nullptr) };

    if (dmFont != nullptr)
    {
        identityFont = const_cast<void*> (static_cast<const void*> (dmFont));
        identityShapingFont = hb_coretext_font_create (dmFont);

        Registry::Entry dmEntry;
        dmEntry.ctFont = identityFont;
        dmEntry.hbFont = identityShapingFont;
        registry.addFont (dmEntry);
        registry.populateFromCmap (0);
    }

    auto nfDataProvider { CGDataProviderCreateWithData (nullptr,
                                                        BinaryData::SymbolsNerdFontRegular_ttf,
                                                        static_cast<size_t> (BinaryData::SymbolsNerdFontRegular_ttfSize),
                                                        nullptr) };

    if (nfDataProvider != nullptr)
    {
        auto nfCgFont { CGFontCreateWithDataProvider (nfDataProvider) };
        CGDataProviderRelease (nfDataProvider);

        if (nfCgFont != nullptr)
        {
            auto nfFontRef { CTFontCreateWithGraphicsFont (nfCgFont, scaledSize, nullptr, nullptr) };
            CGFontRelease (nfCgFont);

            if (nfFontRef != nullptr)
            {
                nerdFont = const_cast<void*> (static_cast<const void*> (nfFontRef));
                nerdShapingFont = hb_coretext_font_create (nfFontRef);

                Registry::Entry nfEntry;
                nfEntry.ctFont = nerdFont;
                nfEntry.hbFont = nerdShapingFont;
                registry.addFont (nfEntry);
                registry.populateFromCmap (1);
            }
        }
    }
}

// ============================================================================
// Accessors
// ============================================================================

/**
 * @brief Returns `nullptr` — FreeType faces are not used on macOS.
 *
 * CoreText manages font objects internally; there is no `FT_Face` to expose.
 * This stub satisfies the shared `Font` interface.
 *
 * @param  (unused) Style variant.
 * @return Always `nullptr` on macOS.
 */
FT_Face jreng::Typeface::getFace (Style) noexcept
{
    return nullptr;
}

/**
 * @brief Returns `nullptr` — FreeType emoji face is not used on macOS.
 *
 * @return Always `nullptr` on macOS.
 */
FT_Face jreng::Typeface::getEmojiFace() noexcept
{
    return nullptr;
}

/**
 * @brief Returns the `CTFontRef` for the given style as an opaque `void*`.
 *
 * On macOS all style variants share `mainFont`; bold/italic selection is
 * handled by CoreText font descriptors at load time.  The style parameter is
 * accepted but ignored.
 *
 * @param  (unused) Style variant.
 * @return `mainFont` cast to `void*`, or `nullptr` if not loaded.
 */
void* jreng::Typeface::getFontHandle (Style) noexcept
{
    return mainFont;
}

/**
 * @brief Returns the `CTFontRef` for Apple Color Emoji as an opaque `void*`.
 *
 * @return `emojiFont` cast to `void*`, or `nullptr` if the emoji font was not
 *         found during `loadFaces()`.
 */
void* jreng::Typeface::getEmojiFontHandle() noexcept
{
    return emojiFont;
}

/**
 * @brief Returns the HarfBuzz shaping font for the given style.
 *
 * On macOS all style variants share `shapingFont` (wraps `mainFont`).
 * The style parameter is accepted but ignored.
 *
 * @param  (unused) Style variant.
 * @return `shapingFont`, or `nullptr` if font loading failed.
 */
hb_font_t* jreng::Typeface::getHbFont (Style) noexcept
{
    return shapingFont;
}

/**
 * @brief Returns the physical pixels-per-em for the given style.
 *
 * Reads `CTFontGetSize` from `mainFont`, which returns the scaled point size
 * (i.e. `fontSize * displayScale`).
 *
 * @param  (unused) Style variant.
 * @return Pixels per em at the current size and display scale, or `0.0f` if
 *         `mainFont` is null.
 */
float jreng::Typeface::getPixelsPerEm (Style) noexcept
{
    float result { 0.0f };

    if (mainFont != nullptr)
    {
        result = static_cast<float> (CTFontGetSize (jreng::toCTFont (mainFont)));
    }

    return result;
}

// ============================================================================
// Size
// ============================================================================

/**
 * @brief Resizes all CoreText font handles to a new point size.
 *
 * Called when the user zooms in/out (Cmd+/Cmd-/Cmd+0).  Performs a full
 * resize of every managed font handle:
 *
 * 1. Destroys `shapingFont` and `emojiShapingFont`.
 * 2. Replaces `mainFont` with a new `CTFontRef` via
 *    `CTFontCreateCopyWithAttributes`; recreates `shapingFont`.
 * 3. Replaces `emojiFont` similarly; recreates `emojiShapingFont`.
 * 4. Replaces `identityFont` similarly; recreates `identityShapingFont`.
 * 5. Destroys `nerdShapingFont`, replaces `nerdFont`, recreates
 *    `nerdShapingFont`.
 * 6. Releases all entries in `fallbackFontCache` and clears the map — cached
 *    fallback fonts are sized at the old point size and must be discarded.
 * 7. Updates `Registry` slots 0 (`identityFont`) and 1 (`nerdFont`) so
 *    subsequent shaping uses the new handles.
 *
 * @param pointSize  New font size in CSS points.
 *
 * @note The physical size passed to CoreText is `pointSize * getDisplayScale()`.
 */
void jreng::Typeface::setSize (float pointSize) noexcept
{
    fontSize = pointSize;
    const float displayScale { getDisplayScale() };
    const CGFloat scaledSize { static_cast<CGFloat> (fontSize * displayScale) };

    if (shapingFont != nullptr)
    {
        hb_font_destroy (shapingFont);
        shapingFont = nullptr;
    }

    if (emojiShapingFont != nullptr)
    {
        hb_font_destroy (emojiShapingFont);
        emojiShapingFont = nullptr;
    }

    if (mainFont != nullptr)
    {
        auto newFont { CTFontCreateCopyWithAttributes (jreng::toCTFont (mainFont),
                                                       scaledSize,
                                                       nullptr,
                                                       nullptr) };
        CFRelease (jreng::toCTFont (mainFont));
        mainFont = const_cast<void*> (static_cast<const void*> (newFont));

        if (newFont != nullptr)
        {
            shapingFont = hb_coretext_font_create (newFont);
        }
    }

    if (emojiFont != nullptr)
    {
        auto newEmojiFont { CTFontCreateCopyWithAttributes (jreng::toCTFont (emojiFont),
                                                            scaledSize,
                                                            nullptr,
                                                            nullptr) };
        CFRelease (jreng::toCTFont (emojiFont));
        emojiFont = const_cast<void*> (static_cast<const void*> (newEmojiFont));

        if (newEmojiFont != nullptr)
        {
            emojiShapingFont = hb_coretext_font_create (newEmojiFont);
        }
    }

    if (identityShapingFont != nullptr)
    {
        hb_font_destroy (identityShapingFont);
        identityShapingFont = nullptr;
    }

    if (identityFont != nullptr)
    {
        auto newIdentityFont { CTFontCreateCopyWithAttributes (jreng::toCTFont (identityFont),
                                                               scaledSize,
                                                               nullptr,
                                                               nullptr) };
        CFRelease (jreng::toCTFont (identityFont));
        identityFont = const_cast<void*> (static_cast<const void*> (newIdentityFont));

        if (newIdentityFont != nullptr)
        {
            identityShapingFont = hb_coretext_font_create (newIdentityFont);
        }
    }

    if (nerdShapingFont != nullptr)
    {
        hb_font_destroy (nerdShapingFont);
        nerdShapingFont = nullptr;
    }

    if (nerdFont != nullptr)
    {
        auto newNerdFont { CTFontCreateCopyWithAttributes (jreng::toCTFont (nerdFont),
                                                           scaledSize,
                                                           nullptr,
                                                           nullptr) };
        CFRelease (jreng::toCTFont (nerdFont));
        nerdFont = const_cast<void*> (static_cast<const void*> (newNerdFont));

        if (newNerdFont != nullptr)
        {
            nerdShapingFont = hb_coretext_font_create (newNerdFont);
        }
    }

    for (auto& pair : fallbackFontCache)
    {
        if (pair.second != nullptr)
        {
            CFRelease (jreng::toCTFont (pair.second));
        }
    }
    fallbackFontCache.clear();

    auto* dmEntry { registry.getEntry (0) };

    if (dmEntry != nullptr)
    {
        dmEntry->ctFont = identityFont;
        dmEntry->hbFont = identityShapingFont;
    }

    auto* nfEntry { registry.getEntry (1) };

    if (nfEntry != nullptr)
    {
        nfEntry->ctFont = nerdFont;
        nfEntry->hbFont = nerdShapingFont;
    }
}

// ============================================================================
// Font Discovery
// ============================================================================

/**
 * @brief Dispatches font discovery to the macOS CoreText implementation.
 *
 * On macOS this simply calls `discoverFontMac (familyName)`.
 *
 * @param familyName  Font family name to look up.
 * @return Absolute POSIX path to the font file, or empty if not found.
 */
juce::String jreng::Typeface::discoverFont (const juce::String& familyName)
{
    return discoverFontMac (familyName);
}

/**
 * @brief Resolves a font family name to an absolute file path using CoreText.
 *
 * Creates a `CTFontDescriptor` for the family name, instantiates a temporary
 * `CTFontRef`, then extracts the file URL via `kCTFontURLAttribute` and
 * converts it to a POSIX path string.
 *
 * @param familyName  Font family name to look up (e.g. "Menlo").
 * @return Absolute POSIX path (e.g. `/System/Library/Fonts/Menlo.ttc`), or
 *         empty if the family is not installed.
 */
juce::String jreng::Typeface::discoverFontMac (const juce::String& familyName)
{
    juce::String result;

    auto cfFamilyName { familyName.toCFString() };
    auto descriptor { CTFontDescriptorCreateWithNameAndSize (cfFamilyName, static_cast<CGFloat> (fontSize)) };
    CFRelease (cfFamilyName);

    if (descriptor != nullptr)
    {
        auto fontRef { CTFontCreateWithFontDescriptor (descriptor, static_cast<CGFloat> (fontSize), nullptr) };
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

/**
 * @brief Returns the CoreText family name for the system emoji font.
 *
 * On macOS the emoji font is always "Apple Color Emoji", which CoreText can
 * resolve by name without a file path.
 *
 * @return `"Apple Color Emoji"`.
 */
juce::String jreng::Typeface::discoverEmojiFont()
{
    return "Apple Color Emoji";
}

// ============================================================================
// Metrics
// ============================================================================

/**
 * @brief Calculates cell geometry metrics for a given cell height using CoreText.
 *
 * Creates a temporary scaled copy of `mainFont` at `heightPx` points, then
 * queries:
 * - `CTFontGetAscent` — distance from baseline to top of cell.
 * - `CTFontGetDescent` — distance from baseline to bottom of cell.
 * - `CTFontGetLeading` — inter-line gap (clamped to ≥ 0).
 * - `CTFontGetAdvancesForGlyphs` for the space glyph — cell width.
 *
 * Falls back to `CTFontGetSize` for cell width if the space glyph advance is
 * zero or negative.
 *
 * Fills all three coordinate spaces (logical, physical, fixed-point) in the
 * returned `Metrics` struct.
 *
 * @param heightPx  Desired cell height in logical (CSS) pixels.
 * @return Populated `Metrics`; `isValid()` returns false if `mainFont` is null.
 */
jreng::Typeface::Metrics jreng::Typeface::calcMetrics (float heightPx) noexcept
{
    Metrics metrics;

    if (heightPx == cachedMetricsSize)
    {
        metrics = cachedMetrics;
    }
    else
    {
        if (mainFont != nullptr)
        {
            auto scaledFont { CTFontCreateCopyWithAttributes (jreng::toCTFont (mainFont),
                                                              static_cast<CGFloat> (heightPx),
                                                              nullptr,
                                                              nullptr) };

            if (scaledFont != nullptr)
            {
                const float displayScale { getDisplayScale() };

                const float ascent  { static_cast<float> (CTFontGetAscent (scaledFont)) };
                const float descent { static_cast<float> (CTFontGetDescent (scaledFont)) };
                const float leading { std::max (0.0f, static_cast<float> (CTFontGetLeading (scaledFont))) };
                const float lineH   { ascent + descent + leading };

                CGGlyph spaceGlyph { jreng::glyphForCodepoint (scaledFont, static_cast<uint32_t> (' ')) };
                float cellW { 0.0f };

                if (spaceGlyph != 0)
                {
                    CGSize advance {};
                    CTFontGetAdvancesForGlyphs (scaledFont, kCTFontOrientationHorizontal, &spaceGlyph, &advance, 1);
                    cellW = static_cast<float> (advance.width);
                }

                if (cellW <= 0.0f)
                {
                    cellW = static_cast<float> (CTFontGetSize (scaledFont));
                }

                CFRelease (scaledFont);

                metrics.logicalCellW    = static_cast<int> (ceilf (cellW));
                metrics.logicalCellH    = static_cast<int> (ceilf (lineH));
                metrics.logicalBaseline = static_cast<int> (ceilf (ascent));

                metrics.fixedCellWidth  = static_cast<int> (cellW  * static_cast<float> (ftFixedScale));
                metrics.fixedCellHeight = static_cast<int> (lineH  * static_cast<float> (ftFixedScale));
                metrics.fixedBaseline   = static_cast<int> (ascent * static_cast<float> (ftFixedScale));

                metrics.physCellW    = static_cast<int> (static_cast<float> (metrics.logicalCellW)    * displayScale);
                metrics.physCellH    = static_cast<int> (static_cast<float> (metrics.logicalCellH)    * displayScale);
                metrics.physBaseline = static_cast<int> (static_cast<float> (metrics.logicalBaseline) * displayScale);
            }
        }

        cachedMetricsSize = heightPx;
        cachedMetrics = metrics;
    }

    return metrics;
}

// ============================================================================
// Rasterization
// ============================================================================

/**
 * @brief Rasterizes a Unicode codepoint to a JUCE ARGB image using CoreText.
 *
 * ### Font selection priority
 * 1. `mainFont` — primary monospace font.
 * 2. `emojiFont` — Apple Color Emoji, if `mainFont` has no glyph.
 * 3. `nerdFont` — Symbols Nerd Font, if neither primary nor emoji has a glyph.
 * 4. Solid white rectangle — special-case for U+2588 FULL BLOCK when no font
 *    has a glyph.
 *
 * ### Rendering
 * - **Color emoji** (Apple Color Emoji): ARGB `CGBitmapContext` with
 *   `kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host`.  Pixels are
 *   un-premultiplied when copied to the JUCE image.
 * - **Monochrome glyphs**: Grayscale `CGBitmapContext` with antialiasing and
 *   font smoothing enabled.  Each grayscale byte becomes the alpha channel of
 *   a white pixel in the output ARGB image.
 *
 * The output image width is a multiple of `physCellW` to support wide glyphs
 * (e.g. CJK characters that occupy two terminal columns).
 *
 * @param codepoint  Unicode scalar value to rasterize.
 * @param heightPx   Cell height in logical pixels (used to scale the font).
 * @param isColor    Set to `true` if the emoji font was used; `false` otherwise.
 * @return ARGB `juce::Image` in physical pixels, or an invalid image on failure.
 *
 * @note The physical size is `heightPx * getDisplayScale()`.
 */
juce::Image jreng::Typeface::rasterizeToImage (uint32_t codepoint, float heightPx, bool& isColor) noexcept
{
    juce::Image result;
    isColor = false;

    if (mainFont != nullptr)
    {
        const float displayScale { getDisplayScale() };
        const CGFloat physicalSize { static_cast<CGFloat> (heightPx * displayScale) };

        auto scaledFont { CTFontCreateCopyWithAttributes (jreng::toCTFont (mainFont),
                                                          physicalSize,
                                                          nullptr,
                                                          nullptr) };

        if (scaledFont != nullptr)
        {
            CGGlyph cgGlyph { jreng::glyphForCodepoint (scaledFont, codepoint) };
            bool useEmojiFont { false };

            if (cgGlyph == 0 and emojiFont != nullptr)
            {
                auto scaledEmoji { CTFontCreateCopyWithAttributes (jreng::toCTFont (emojiFont),
                                                                    physicalSize,
                                                                    nullptr,
                                                                    nullptr) };

                if (scaledEmoji != nullptr)
                {
                    const CGGlyph emojiGlyph { jreng::glyphForCodepoint (scaledEmoji, codepoint) };

                    if (emojiGlyph != 0)
                    {
                        CFRelease (scaledFont);
                        scaledFont = scaledEmoji;
                        cgGlyph = emojiGlyph;
                        useEmojiFont = true;
                        isColor = true;
                    }
                    else
                    {
                        CFRelease (scaledEmoji);
                    }
                }
            }

            if (cgGlyph == 0 and not useEmojiFont and nerdFont != nullptr)
            {
                auto scaledNF { CTFontCreateCopyWithAttributes (jreng::toCTFont (nerdFont),
                                                                 physicalSize,
                                                                 nullptr,
                                                                 nullptr) };

                if (scaledNF != nullptr)
                {
                    const CGGlyph nfGlyph { jreng::glyphForCodepoint (scaledNF, codepoint) };

                    if (nfGlyph != 0)
                    {
                        CFRelease (scaledFont);
                        scaledFont = scaledNF;
                        cgGlyph = nfGlyph;
                    }
                    else
                    {
                        CFRelease (scaledNF);
                    }
                }
            }

            const Metrics metrics { calcMetrics (heightPx) };

            if (cgGlyph == 0 and codepoint == 0x2588 and metrics.isValid())
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
            else if (cgGlyph != 0 and metrics.isValid())
            {
                CGRect boundingRect {};
                CTFontGetBoundingRectsForGlyphs (scaledFont,
                                                 kCTFontOrientationHorizontal,
                                                 &cgGlyph,
                                                 &boundingRect,
                                                 1);

                CGSize advance {};
                CTFontGetAdvancesForGlyphs (scaledFont,
                                            kCTFontOrientationHorizontal,
                                            &cgGlyph,
                                            &advance,
                                            1);

                const int glyphW { static_cast<int> (ceilf (static_cast<float> (boundingRect.size.width))) };
                const int glyphH { static_cast<int> (ceilf (static_cast<float> (boundingRect.size.height))) };

                if (glyphW > 0 and glyphH > 0)
                {
                    const int offsetX    { static_cast<int> (floorf (static_cast<float> (boundingRect.origin.x))) };
                    const int glyphRight { offsetX + glyphW };
                    const int advancePx  { static_cast<int> (ceilf (static_cast<float> (advance.width))) };
                    const int effectiveW { std::max (advancePx, glyphRight) };
                    const int cellSpan   { std::max (1, (effectiveW + metrics.physCellW - 1) / metrics.physCellW) };
                    const int imageW     { cellSpan * metrics.physCellW };
                    const int imageH     { metrics.physCellH };

                    if (useEmojiFont)
                    {
                        auto colorSpace { CGColorSpaceCreateDeviceRGB() };

                        if (colorSpace != nullptr)
                        {
                            const size_t bytesPerRow { static_cast<size_t> (imageW) * 4 };
                            auto context { CGBitmapContextCreate (nullptr,
                                                                  static_cast<size_t> (imageW),
                                                                  static_cast<size_t> (imageH),
                                                                  8,
                                                                  bytesPerRow,
                                                                  colorSpace,
                                                                  kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host) };

                            if (context != nullptr)
                            {
                                CGContextClearRect (context, CGRectMake (0, 0, imageW, imageH));
                                CGContextSetShouldAntialias (context, true);

                                const CGFloat drawX { static_cast<CGFloat> (offsetX >= 0 ? 0 : -offsetX) };
                                const CGFloat drawY { static_cast<CGFloat> (imageH - metrics.physBaseline) };
                                CGPoint position { drawX, drawY };

                                CTFontDrawGlyphs (scaledFont, &cgGlyph, &position, 1, context);

                                const uint8_t* pixels { static_cast<const uint8_t*> (CGBitmapContextGetData (context)) };

                                if (pixels != nullptr)
                                {
                                    result = juce::Image (juce::Image::ARGB, imageW, imageH, true);
                                    juce::Image::BitmapData data (result, juce::Image::BitmapData::writeOnly);

                                    for (int y { 0 }; y < imageH; ++y)
                                    {
                                        const uint8_t* srcRow { pixels + static_cast<ptrdiff_t> (y) * static_cast<ptrdiff_t> (bytesPerRow) };

                                        for (int x { 0 }; x < imageW; ++x)
                                        {
                                            const uint8_t* px { srcRow + x * 4 };
                                            const uint8_t a { px[3] };
                                            const uint8_t r { px[2] };
                                            const uint8_t g { px[1] };
                                            const uint8_t b { px[0] };
                                            data.setPixelColour (x, y, juce::Colour (r, g, b, a));
                                        }
                                    }
                                }

                                CGContextRelease (context);
                            }

                            CGColorSpaceRelease (colorSpace);
                        }
                    }
                    else
                    {
                        auto colorSpace { CGColorSpaceCreateDeviceGray() };

                        if (colorSpace != nullptr)
                        {
                            auto context { CGBitmapContextCreate (nullptr,
                                                                  static_cast<size_t> (imageW),
                                                                  static_cast<size_t> (imageH),
                                                                  8,
                                                                  static_cast<size_t> (imageW),
                                                                  colorSpace,
                                                                  kCGImageAlphaNone) };

                            if (context != nullptr)
                            {
                                CGContextSetGrayFillColor (context, 0.0, 1.0);
                                CGContextFillRect (context, CGRectMake (0, 0, imageW, imageH));

                                CGContextSetGrayFillColor (context, 1.0, 1.0);
                                CGContextSetShouldAntialias (context, true);
                                CGContextSetShouldSmoothFonts (context, true);

                                const CGFloat drawX { static_cast<CGFloat> (offsetX >= 0 ? 0 : -offsetX) };
                                const CGFloat drawY { static_cast<CGFloat> (imageH - metrics.physBaseline) };
                                CGPoint position { drawX, drawY };

                                CTFontDrawGlyphs (scaledFont, &cgGlyph, &position, 1, context);

                                const uint8_t* pixels { static_cast<const uint8_t*> (CGBitmapContextGetData (context)) };

                                if (pixels != nullptr)
                                {
                                    result = juce::Image (juce::Image::ARGB, imageW, imageH, true);
                                    juce::Image::BitmapData data (result, juce::Image::BitmapData::writeOnly);

                                    for (int y { 0 }; y < imageH; ++y)
                                    {
                                        const uint8_t* srcRow { pixels + static_cast<ptrdiff_t> (y * imageW) };

                                        for (int x { 0 }; x < imageW; ++x)
                                        {
                                            const uint8_t alpha { srcRow[x] };
                                            data.setPixelColour (x, y,
                                                                 juce::Colour (static_cast<uint8_t> (255),
                                                                               static_cast<uint8_t> (255),
                                                                               static_cast<uint8_t> (255),
                                                                               alpha));
                                        }
                                    }
                                }

                                CGContextRelease (context);
                            }

                            CGColorSpaceRelease (colorSpace);
                        }
                    }
                }
            }

            CFRelease (scaledFont);
        }
    }

    return result;
}

// ============================================================================
// Shaping
// ============================================================================

/**
 * @brief Fast-path shaper for single ASCII codepoints (U+0000–U+007F).
 *
 * Bypasses HarfBuzz entirely.  Looks up the glyph index via
 * `glyphForCodepoint`, queries the advance width via
 * `CTFontGetAdvancesForGlyphs`, and writes a single `Glyph` into
 * `shapingBuffer` (allocating 16 slots if the buffer is empty).
 *
 * @param codepoint  ASCII codepoint (must be < 128).
 * @return GlyphRun with `count == 1` on success, `count == 0` if the glyph
 *         is not in `mainFont`.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeASCII (uint32_t codepoint) noexcept
{
    GlyphRun result;

    if (mainFont != nullptr)
    {
        CGGlyph cgGlyph { jreng::glyphForCodepoint (jreng::toCTFont (mainFont), codepoint) };

        if (cgGlyph != 0)
        {
            if (shapingBufferCapacity < 1)
            {
                shapingBuffer.allocate (16, false);
                shapingBufferCapacity = 16;
            }

            CGSize advance {};
            CTFontGetAdvancesForGlyphs (jreng::toCTFont (mainFont), kCTFontOrientationHorizontal, &cgGlyph, &advance, 1);

            Glyph& g { shapingBuffer[0] };
            g.glyphIndex = static_cast<uint32_t> (cgGlyph);
            g.xOffset    = 0.0f;
            g.yOffset    = 0.0f;
            g.xAdvance   = static_cast<float> (advance.width);

            result = { shapingBuffer.get(), 1 };
        }
    }

    return result;
}

/**
 * @brief Shapes codepoints using HarfBuzz with the CoreText shaping font.
 *
 * Resets `scratchBuffer`, adds the UTF-32 codepoints, guesses segment
 * properties (script, language, direction), and calls `hb_shape` with the
 * HarfBuzz font selected by `getHbFont (style)`.
 *
 * Copies glyph info and positions from the HarfBuzz output into `shapingBuffer`
 * (growing it if needed).  HarfBuzz positions are in 26.6 fixed-point and are
 * divided by `ftFixedScale` to produce pixel values.
 *
 * Returns an empty result if all output glyphs have index 0 (`.notdef`),
 * indicating the font cannot render any of the codepoints.
 *
 * @param style       Font style — selects the HarfBuzz font via `getHbFont`.
 * @param codepoints  UTF-32 codepoint array.
 * @param count       Number of codepoints.
 * @return GlyphRun; `count == 0` if all glyphs are `.notdef`.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeHarfBuzz (Style style,
                                                      const uint32_t* codepoints, size_t count) noexcept
{
    GlyphRun result;
    hb_font_t* font { getHbFont (style) };

    if (font != nullptr and scratchBuffer != nullptr)
    {
        hb_buffer_reset (scratchBuffer);
        hb_buffer_add_utf32 (scratchBuffer, codepoints, static_cast<int> (count), 0, static_cast<int> (count));
        hb_buffer_guess_segment_properties (scratchBuffer);
        hb_shape (font, scratchBuffer, nullptr, 0);

        unsigned int glyphCount { 0 };
        hb_glyph_info_t*     glyphInfo { hb_buffer_get_glyph_infos (scratchBuffer, &glyphCount) };
        hb_glyph_position_t* glyphPos  { hb_buffer_get_glyph_positions (scratchBuffer, &glyphCount) };

        if (glyphCount > 0)
        {
            const bool hasValidGlyph { std::any_of (glyphInfo, glyphInfo + glyphCount,
                [] (const hb_glyph_info_t& info) { return info.codepoint != 0; }) };

            if (hasValidGlyph)
            {
                if (static_cast<int> (glyphCount) > shapingBufferCapacity)
                {
                    shapingBuffer.allocate (static_cast<size_t> (glyphCount), false);
                    shapingBufferCapacity = static_cast<int> (glyphCount);
                }

                for (unsigned int i { 0 }; i < glyphCount; ++i)
                {
                    Glyph& g { shapingBuffer[i] };
                    g.glyphIndex = glyphInfo[i].codepoint;
                    g.xOffset    = glyphPos[i].x_offset  / static_cast<float> (ftFixedScale);
                    g.yOffset    = glyphPos[i].y_offset  / static_cast<float> (ftFixedScale);
                    g.xAdvance   = glyphPos[i].x_advance / static_cast<float> (ftFixedScale);
                }

                result = { shapingBuffer.get(), static_cast<int> (glyphCount) };
            }
        }
    }

    return result;
}

/**
 * @brief Fallback shaper using CoreText font substitution (macOS only).
 *
 * For each codepoint that `mainFont` cannot render (glyph index 0), queries
 * `CTFontCreateForString` to let CoreText select an appropriate system font.
 * Results are cached in `fallbackFontCache` (keyed by codepoint) to avoid
 * repeated CoreText lookups on subsequent frames.
 *
 * Fonts identified as "LastResort" (CoreText's last-resort placeholder) are
 * rejected and stored as `nullptr` in the cache to prevent future lookups.
 *
 * The `GlyphRun::fontHandle` is set to the fallback `CTFontRef` so the
 * renderer can pass the correct handle to `Atlas::getOrRasterize`.
 *
 * @param codepoints  UTF-32 codepoint array.
 * @param count       Number of codepoints.
 * @return GlyphRun; `count == 0` if no codepoint could be shaped.
 *
 * @note `fallbackFontCache` is cleared by `setSize()` because cached fonts are
 *       sized at the old point size.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeFallback (const uint32_t* codepoints, size_t count) noexcept
{
    GlyphRun result;

    if (mainFont != nullptr)
    {
        const int needed { static_cast<int> (count) };

        if (needed > shapingBufferCapacity)
        {
            shapingBuffer.allocate (static_cast<size_t> (needed), false);
            shapingBufferCapacity = needed;
        }

        int written { 0 };
        void* lastFallbackHandle { nullptr };

        for (size_t i { 0 }; i < count; ++i)
        {
            const uint32_t cp { codepoints[i] };
            CGGlyph cgGlyph { jreng::glyphForCodepoint (jreng::toCTFont (mainFont), cp) };
            CTFontRef useFont { jreng::toCTFont (mainFont) };

            if (cgGlyph == 0)
            {
                auto cacheIt { fallbackFontCache.find (cp) };

                if (cacheIt != fallbackFontCache.end())
                {
                    if (cacheIt->second != nullptr)
                    {
                        CTFontRef cached { jreng::toCTFont (cacheIt->second) };
                        const CGGlyph cachedGlyph { jreng::glyphForCodepoint (cached, cp) };

                        if (cachedGlyph != 0)
                        {
                            cgGlyph = cachedGlyph;
                            useFont = cached;
                            lastFallbackHandle = cacheIt->second;
                        }
                    }
                }
                else
                {
                    UniChar utf16[2] {};
                    CFIndex utf16Len { 0 };

                    if (cp <= 0xFFFF)
                    {
                        utf16[0] = static_cast<UniChar> (cp);
                        utf16Len = 1;
                    }
                    else
                    {
                        UTF32Char longChar { static_cast<UTF32Char> (cp) };
                        CFStringGetSurrogatePairForLongCharacter (longChar, utf16);
                        utf16Len = 2;
                    }

                    auto cfStr { CFStringCreateWithCharacters (kCFAllocatorDefault, utf16, utf16Len) };

                    if (cfStr != nullptr)
                    {
                        CTFontRef candidate { CTFontCreateForString (jreng::toCTFont (mainFont),
                                                                     cfStr,
                                                                     CFRangeMake (0, utf16Len)) };
                        CFRelease (cfStr);

                        if (candidate != nullptr)
                        {
                            bool isLastResort { false };
                            auto psName { CTFontCopyPostScriptName (candidate) };

                            if (psName != nullptr)
                            {
                                std::array<char, 64> nameBuf {};
                                CFStringGetCString (psName, nameBuf.data(),
                                                    static_cast<CFIndex> (nameBuf.size()),
                                                    kCFStringEncodingUTF8);
                                CFRelease (psName);

                                isLastResort = (juce::String (nameBuf.data()) == "LastResort");
                            }

                            if (not isLastResort)
                            {
                                const CGGlyph candidateGlyph { jreng::glyphForCodepoint (candidate, cp) };

                                if (candidateGlyph != 0)
                                {
                                    void* candidateHandle { const_cast<void*> (static_cast<const void*> (candidate)) };
                                    fallbackFontCache.emplace (cp, candidateHandle);
                                    cgGlyph = candidateGlyph;
                                    useFont = candidate;
                                    lastFallbackHandle = candidateHandle;
                                }
                                else
                                {
                                    CFRelease (candidate);
                                    fallbackFontCache.emplace (cp, nullptr);
                                }
                            }
                            else
                            {
                                CFRelease (candidate);
                                fallbackFontCache.emplace (cp, nullptr);
                            }
                        }
                        else
                        {
                            fallbackFontCache.emplace (cp, nullptr);
                        }
                    }
                }
            }

            if (cgGlyph != 0)
            {
                CGSize advance {};
                CTFontGetAdvancesForGlyphs (useFont, kCTFontOrientationHorizontal, &cgGlyph, &advance, 1);

                Glyph& g { shapingBuffer[written] };
                g.glyphIndex = static_cast<uint32_t> (cgGlyph);
                g.xOffset    = 0.0f;
                g.yOffset    = 0.0f;
                g.xAdvance   = static_cast<float> (advance.width);
                ++written;
            }
        }

        if (written > 0)
        {
            result = { shapingBuffer.get(), written, lastFallbackHandle };
        }
    }

    return result;
}

/**
 * @brief Shapes a sequence of codepoints using the primary font with fallback.
 *
 * Dispatch order:
 * 1. Single ASCII codepoint (< 128) → `shapeASCII()` (no HarfBuzz overhead).
 * 2. Multi-codepoint or non-ASCII → `shapeHarfBuzz()`.
 * 3. If HarfBuzz returns `count == 0` (all `.notdef`) → `shapeFallback()`.
 *
 * @param style       Font style for HarfBuzz shaping.
 * @param codepoints  UTF-32 codepoint array; must not be null.
 * @param count       Number of codepoints; must be > 0.
 * @return GlyphRun; `count == 0` if no glyphs could be shaped.
 *
 * @note The returned pointer into `shapingBuffer` is invalidated by the next
 *       call to any `shape*` method.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeText (Style style,
                                                  const uint32_t* codepoints,
                                                  size_t count) noexcept
{
    GlyphRun result;

    if (codepoints != nullptr and count > 0 and mainFont != nullptr)
    {
        if (count == 1 and codepoints[0] < 128)
        {
            result = shapeASCII (codepoints[0]);
        }
        else
        {
            result = shapeHarfBuzz (style, codepoints, count);

            if (result.count == 0)
            {
                result = shapeFallback (codepoints, count);
            }
        }
    }

    return result;
}

/**
 * @brief Shapes emoji codepoints using the Apple Color Emoji HarfBuzz font.
 *
 * Uses `emojiShapingFont` directly — no fallback chain.  Intended for
 * codepoints already classified as emoji by the cell layout flags.
 *
 * HarfBuzz positions are converted from 26.6 fixed-point to pixels by dividing
 * by `ftFixedScale`.
 *
 * @param codepoints  UTF-32 codepoint array; must not be null.
 * @param count       Number of codepoints; must be > 0.
 * @return GlyphRun; `count == 0` if `emojiShapingFont` is null or shaping
 *         produced no glyphs.
 *
 * @note The returned pointer into `shapingBuffer` is invalidated by the next
 *       call to any `shape*` method.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeEmoji (const uint32_t* codepoints, size_t count) noexcept
{
    GlyphRun result;

    if (codepoints != nullptr and count > 0 and emojiShapingFont != nullptr)
    {
        hb_buffer_reset (scratchBuffer);
        hb_buffer_add_utf32 (scratchBuffer, codepoints, static_cast<int> (count), 0, static_cast<int> (count));
        hb_buffer_guess_segment_properties (scratchBuffer);
        hb_shape (emojiShapingFont, scratchBuffer, nullptr, 0);

        unsigned int glyphCount { 0 };
        hb_glyph_info_t*     glyphInfo { hb_buffer_get_glyph_infos (scratchBuffer, &glyphCount) };
        hb_glyph_position_t* glyphPos  { hb_buffer_get_glyph_positions (scratchBuffer, &glyphCount) };

        if (glyphCount > 0)
        {
            if (static_cast<int> (glyphCount) > shapingBufferCapacity)
            {
                shapingBuffer.allocate (static_cast<size_t> (glyphCount), false);
                shapingBufferCapacity = static_cast<int> (glyphCount);
            }

            for (unsigned int i { 0 }; i < glyphCount; ++i)
            {
                Glyph& g { shapingBuffer[i] };
                g.glyphIndex = glyphInfo[i].codepoint;
                g.xOffset    = glyphPos[i].x_offset  / static_cast<float> (ftFixedScale);
                g.yOffset    = glyphPos[i].y_offset  / static_cast<float> (ftFixedScale);
                g.xAdvance   = glyphPos[i].x_advance / static_cast<float> (ftFixedScale);
            }

            result = { shapingBuffer.get(), static_cast<int> (glyphCount) };
        }
    }

    return result;
}

// ============================================================================
// Static utilities
// ============================================================================

/**
 * @brief Converts a 26.6 fixed-point value to pixels, rounding up (ceiling).
 *
 * Used to convert FreeType / HarfBuzz metric values to integer pixel counts
 * without truncation.
 *
 * @param v26_6  Value in FreeType 26.6 fixed-point format.
 * @return Ceiling pixel count: `(v26_6 + 63) >> 6`.
 */
int jreng::Typeface::ceil26_6ToPx (int v26_6) noexcept
{
    return (v26_6 + ftFixedScale - 1) >> 6;
}

/**
 * @brief Converts a floating-point pixel value to 26.6 fixed-point, rounding.
 *
 * Multiplies by 64 and adds 0.5 before truncating to produce a rounded result.
 *
 * @param px  Pixel value.
 * @return Nearest 26.6 fixed-point integer: `(int)(px * 64 + 0.5)`.
 */
int jreng::Typeface::roundFloatPxTo26_6 (float px) noexcept
{
    return static_cast<int> (px * static_cast<float> (ftFixedScale) + 0.5f);
}

/**
 * @brief Returns the device-pixel ratio of the primary display.
 *
 * Queries `juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()`.
 * Returns `1.0f` if no display is available (headless / unit test context).
 *
 * @return Display scale factor (e.g. `2.0f` on Retina, `1.0f` on standard).
 */
float jreng::Typeface::getDisplayScale() noexcept
{
    const auto* display { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() };
    float scale { 1.0f };

    if (display != nullptr)
    {
        scale = static_cast<float> (display->scale);
    }

    return scale;
}

#include "jreng_typeface_registry.mm"

#endif
