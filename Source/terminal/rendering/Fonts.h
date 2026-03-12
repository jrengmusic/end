/**
 * @file Fonts.h
 * @brief Font management system for the terminal emulator.
 *
 * Fonts is the single point of authority for all font handles, HarfBuzz shaping
 * fonts, and glyph metrics used by the terminal renderer.  It abstracts over two
 * platform backends behind a unified API:
 *
 * - **macOS** (`Fonts.mm`) — CoreText + HarfBuzz via `hb_coretext_font_create`.
 *   Font objects are `CTFontRef` values stored as `void*`.
 * - **Linux / Windows** (`Fonts.cpp`) — FreeType + HarfBuzz via
 *   `hb_ft_font_create_referenced`.  Font objects are `FT_Face` values.
 *
 * ### Font handles managed
 * | Handle          | Purpose                                      |
 * |-----------------|----------------------------------------------|
 * | mainFont        | Primary monospace face (user-selected)       |
 * | emojiFont       | Color emoji face (Apple Color Emoji / Noto)  |
 * | nerdFont        | Nerd Font icons loaded from BinaryData       |
 * | identityFont    | Display Mono — used for FontCollection slot 0|
 *
 * Each font handle has a paired HarfBuzz shaping font (`shapingFont`,
 * `emojiShapingFont`, `nerdShapingFont`, `identityShapingFont`) used by
 * `shapeText()` and `shapeEmoji()`.
 *
 * ### Zoom / resize
 * `setSize()` recreates all font handles at the new point size, clears the
 * fallback font cache, and updates the live `FontCollection` entries so that
 * subsequent shaping picks up the new metrics immediately.
 *
 * ### HiDPI / Retina
 * `getDisplayScale()` queries the primary display's device-pixel ratio.  All
 * physical pixel dimensions are scaled by this factor; logical dimensions are
 * kept in CSS/point space.
 *
 * @note All methods must be called on the **MESSAGE THREAD**.
 *
 * @see FontCollection
 * @see GlyphAtlas
 * @see GlyphConstraint
 */

#pragma once

#include <JuceHeader.h>


#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#if JUCE_MAC
// hb-coretext.h included in Fonts.mm
#else
    #include <hb-ft.h>
#endif
#include <array>
#include <unordered_map>

/**
 * @struct Fonts
 * @brief Platform-agnostic font manager: loading, shaping, rasterization.
 *
 * Owns all font resources for the terminal renderer.  Constructed once per
 * terminal view with the user's preferred family name and initial point size.
 * The same instance is reused across zoom operations via `setSize()`.
 *
 * @par Platform dispatch
 * The header is shared; the implementation is split:
 * - `Fonts.mm` — compiled only on macOS (`JUCE_MAC`).
 * - `Fonts.cpp` — compiled on Linux and Windows.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods must be called from the JUCE message
 * thread.  No internal locking is performed.
 *
 * @see FontCollection
 * @see GlyphAtlas
 */
// MESSAGE THREAD
struct Fonts : public jreng::Context<Fonts>
{
    // =========================================================================
    // Inner types
    // =========================================================================

    /**
     * @enum Style
     * @brief SGR font style variants.
     *
     * Maps directly to the four slots in the `faces` array on the FreeType
     * backend.  On macOS, style selection is handled by CoreText font
     * descriptors; the enum is accepted by the API but only `regular` is
     * currently populated.
     */
    enum class Style
    {
        regular,    ///< Normal weight, upright.
        bold,       ///< Heavier stroke weight (SGR 1).
        italic,     ///< Oblique / italic variant (SGR 3).
        boldItalic  ///< Bold + italic combined (SGR 1 + 3).
    };

    /**
     * @struct Face
     * @brief A paired FreeType face and its HarfBuzz shaping font.
     *
     * Used by the FreeType backend to keep `FT_Face` and `hb_font_t*` in sync.
     * On macOS this struct is not used; CoreText handles font objects directly.
     *
     * @note Both pointers may be `nullptr` if the corresponding style variant
     *       was not found on disk (e.g. no bold face for a custom font).
     */
    struct Face
    {
        FT_Face    face    { nullptr }; ///< FreeType face handle; nullptr if not loaded.
        hb_font_t* hbFont  { nullptr }; ///< HarfBuzz font wrapping @p face; nullptr if not loaded.
    };

    /**
     * @struct Glyph
     * @brief Positioned glyph produced by HarfBuzz shaping.
     *
     * Coordinates are in **pixels** (not 26.6 fixed-point).  The renderer
     * uses `xAdvance` to advance the pen position and `xOffset`/`yOffset` for
     * mark attachment and kerning corrections.
     *
     * @par Layout
     * @code
     * pen.x += g.xOffset;   // apply mark offset
     * draw at (pen.x, pen.y + g.yOffset);
     * pen.x += g.xAdvance;  // advance to next cluster
     * @endcode
     */
    struct Glyph
    {
        uint32_t glyphIndex { 0 };    ///< Font-internal glyph ID (not a Unicode codepoint).
        float    xOffset    { 0.0f }; ///< Horizontal offset from pen position, in pixels.
        float    yOffset    { 0.0f }; ///< Vertical offset from baseline, in pixels.
        float    xAdvance   { 0.0f }; ///< Horizontal advance to next glyph, in pixels.
    };

    /**
     * @struct Metrics
     * @brief Cell geometry derived from the active font at a given height.
     *
     * Provides three coordinate spaces for the same cell dimensions:
     * - **Logical** — CSS/point pixels, used for layout calculations.
     * - **Physical** — device pixels after HiDPI scaling, used for rasterization.
     * - **Fixed** — 26.6 fixed-point integers, used for FreeType / HarfBuzz calls.
     *
     * @note Call `isValid()` before using any field; an invalid Metrics means
     *       font loading failed and all values are zero.
     *
     * @see Fonts::calcMetrics
     */
    struct Metrics
    {
        int logicalCellW   { 0 }; ///< Cell width in logical (CSS) pixels.
        int logicalCellH   { 0 }; ///< Cell height in logical (CSS) pixels.
        int logicalBaseline{ 0 }; ///< Ascent from top of cell in logical pixels.
        int physCellW      { 0 }; ///< Cell width in physical (device) pixels.
        int physCellH      { 0 }; ///< Cell height in physical (device) pixels.
        int physBaseline   { 0 }; ///< Ascent from top of cell in physical pixels.
        int fixedCellWidth { 0 }; ///< Cell width in 26.6 fixed-point units.
        int fixedCellHeight{ 0 }; ///< Cell height in 26.6 fixed-point units.
        int fixedBaseline  { 0 }; ///< Ascent in 26.6 fixed-point units.

        /**
         * @brief Returns true when the metrics contain valid (non-zero) dimensions.
         * @return `true` if both `logicalCellW` and `logicalCellH` are positive.
         */
        bool isValid() const noexcept { return logicalCellW > 0 && logicalCellH > 0; }
    };

    // =========================================================================
    // Constants
    // =========================================================================

    /**
     * @brief FreeType / HarfBuzz 26.6 fixed-point scale factor.
     *
     * Multiply a pixel value by this to obtain a 26.6 integer; divide a 26.6
     * integer by this to obtain pixels.  Value is 64 (2^6).
     */
    static constexpr int ftFixedScale { 64 };

    /**
     * @brief Base DPI used when setting FreeType character sizes.
     *
     * macOS uses 72 DPI (CoreText point = 1 CSS pixel at 1x).
     * Linux/Windows use 96 DPI (standard screen DPI).
     * The actual render DPI is `baseDpi * getDisplayScale()`.
     */
#if JUCE_MAC
    static constexpr FT_UInt baseDpi { 72 };
#else
    static constexpr FT_UInt baseDpi { 96 };
#endif

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Constructs the font manager and loads all font handles.
     *
     * Calls `initialize()` which registers embedded fonts, creates the HarfBuzz
     * scratch buffer, and calls `loadFaces()` to populate all font handles at
     * the given point size.
     *
     * @param userFamilyName  Preferred font family name (e.g. "JetBrains Mono").
     *                        If empty or not found, falls back to the platform
     *                        default monospace font, then to the embedded
     *                        Display Mono.
     * @param pointSize       Initial font size in CSS points.
     */
    Fonts (const juce::String& userFamilyName, float pointSize);

    /**
     * @brief Destroys all font handles and releases HarfBuzz resources.
     *
     * On macOS: calls `CFRelease` on all `CTFontRef` handles and
     * `hb_font_destroy` on all shaping fonts.  Also releases every entry in
     * `fallbackFontCache`.
     *
     * On Linux/Windows: calls `FT_Done_Face` on all `FT_Face` handles,
     * `hb_font_destroy` on all shaping fonts, and `FT_Done_FreeType` on the
     * library instance.
     */
    ~Fonts();

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Returns the FreeType face for the given style.
     *
     * On macOS this always returns `nullptr`; CoreText does not expose
     * `FT_Face` handles.  On Linux/Windows, falls back to the regular face if
     * the requested style was not loaded.
     *
     * @param style  Desired style variant.
     * @return FreeType face handle, or `nullptr` on macOS.
     */
    FT_Face getFace (Style style) noexcept;

    /**
     * @brief Returns the FreeType face for the emoji font.
     *
     * On macOS this always returns `nullptr`.  On Linux/Windows, returns the
     * `emojiFace` loaded from the system emoji font path.
     *
     * @return FreeType emoji face handle, or `nullptr` on macOS.
     */
    FT_Face getEmojiFace() noexcept;

    /**
     * @brief Returns the platform-native font handle for the given style.
     *
     * On macOS: returns the `CTFontRef` cast to `void*`.
     * On Linux/Windows: returns the `FT_Face` cast to `void*`.
     *
     * This opaque handle is stored in `GlyphKey::fontFace` to key glyph cache
     * entries without exposing platform types to the atlas.
     *
     * @param style  Desired style variant.
     * @return Opaque font handle; never `nullptr` if `isValid()` is true.
     */
    void* getFontHandle (Style style) noexcept;

    /**
     * @brief Returns the platform-native font handle for the emoji font.
     *
     * On macOS: returns the `CTFontRef` for Apple Color Emoji cast to `void*`.
     * On Linux/Windows: returns the `FT_Face` for the emoji font cast to `void*`.
     *
     * @return Opaque emoji font handle, or `nullptr` if no emoji font was found.
     */
    void* getEmojiFontHandle() noexcept;

    /**
     * @brief Returns the HarfBuzz shaping font for the given style.
     *
     * On macOS: returns `shapingFont` (wraps `mainFont` via `hb_coretext_font_create`).
     * On Linux/Windows: returns the `hb_font_t*` from the matching `Face` slot,
     * falling back to the regular face if the requested style is not loaded.
     *
     * @param style  Desired style variant.
     * @return HarfBuzz font pointer; `nullptr` if font loading failed.
     */
    hb_font_t* getHbFont (Style style) noexcept;

    /**
     * @brief Returns the pixels-per-em value for the given style.
     *
     * On macOS: reads `CTFontGetSize` from `mainFont`.
     * On Linux/Windows: reads `face->size->metrics.x_ppem` from the FreeType face.
     *
     * @param style  Desired style variant.
     * @return Pixels per em at the current point size and display scale,
     *         or `0.0f` if the font is not loaded.
     */
    float getPixelsPerEm (Style style) noexcept;

    // =========================================================================
    // Resize
    // =========================================================================

    /**
     * @brief Resizes all font handles to a new point size.
     *
     * This is the zoom handler — called when the user presses Cmd+/Cmd-/Cmd+0.
     * It performs a full resize of every managed font:
     *
     * **macOS:** Creates new `CTFontRef` copies via `CTFontCreateCopyWithAttributes`
     * for `mainFont`, `emojiFont`, `identityFont`, and `nerdFont`.  Destroys and
     * recreates all paired HarfBuzz shaping fonts.  Clears `fallbackFontCache`
     * (all cached fallback `CTFontRef` values are released).  Updates the live
     * `FontCollection` entries (slots 0 and 1) so subsequent shaping uses the
     * new size.
     *
     * **Linux/Windows:** Calls `FT_Set_Char_Size` on all four style faces,
     * `emojiFace`, and `nfFace`.  Destroys and recreates `nerdShapingFont` via
     * `hb_ft_font_create_referenced`.  Updates `FontCollection` slot 1.
     *
     * @param pointSize  New font size in CSS points.
     *
     * @note HarfBuzz shaping fonts on the FreeType backend do not need explicit
     *       recreation because `hb_ft_font_create_referenced` reads metrics
     *       directly from the `FT_Face` at shape time — except for `nerdShapingFont`
     *       which is recreated to pick up the new size reliably.
     */
    void setSize (float pointSize) noexcept;

    // =========================================================================
    // Shaping
    // =========================================================================

    /**
     * @struct ShapeResult
     * @brief Output of a HarfBuzz shaping call.
     *
     * Points into `shapingBuffer` — valid only until the next call to any
     * `shape*` method.  Callers must consume the result immediately.
     *
     * @note `fontHandle` is set by `shapeFallback()` to identify which fallback
     *       `CTFontRef` was used, so the renderer can pass the correct handle to
     *       `GlyphAtlas::getOrRasterize`.  It is `nullptr` for normal shaping.
     */
    struct ShapeResult
    {
        const Glyph* glyphs     { nullptr }; ///< Pointer into the internal shaping buffer.
        int          count      { 0 };       ///< Number of valid Glyph entries.
        void*        fontHandle { nullptr }; ///< Fallback font handle (macOS only); nullptr for primary font.
    };

    /**
     * @brief Shapes a sequence of Unicode codepoints using the primary font.
     *
     * Dispatch order:
     * 1. Single ASCII codepoint (< 128) → `shapeASCII()` (fast path, no HarfBuzz).
     * 2. Multi-codepoint or non-ASCII → `shapeHarfBuzz()`.
     * 3. If HarfBuzz returns all `.notdef` glyphs → `shapeFallback()` (macOS only).
     *
     * @param style       Font style to use for shaping.
     * @param codepoints  Pointer to an array of UTF-32 codepoints.
     * @param count       Number of codepoints in the array.
     * @return ShapeResult pointing into the internal shaping buffer.
     *         `count == 0` means no glyphs could be shaped.
     *
     * @note The returned pointer is invalidated by the next call to any
     *       `shape*` method.
     */
    ShapeResult shapeText (Style style,
                           const uint32_t* codepoints,
                           size_t count) noexcept;

    /**
     * @brief Shapes a sequence of Unicode codepoints using the emoji font.
     *
     * Uses `emojiShapingFont` directly — no fallback chain.  Intended for
     * codepoints already classified as emoji by the cell layout flags.
     *
     * @param codepoints  Pointer to an array of UTF-32 codepoints.
     * @param count       Number of codepoints in the array.
     * @return ShapeResult pointing into the internal shaping buffer.
     *         `count == 0` if `emojiShapingFont` is null or shaping produced
     *         no glyphs.
     *
     * @note The returned pointer is invalidated by the next call to any
     *       `shape*` method.
     */
    ShapeResult shapeEmoji (const uint32_t* codepoints,
                            size_t count) noexcept;

    // =========================================================================
    // Validity
    // =========================================================================

    /**
     * @brief Returns true when the font system was successfully initialized.
     *
     * On macOS: checks that `mainFont` (a `CTFontRef`) is non-null.
     * On Linux/Windows: checks that `library` (an `FT_Library`) is non-null.
     *
     * @return `true` if at least the primary font handle is available.
     */
#if JUCE_MAC
    bool isValid() const noexcept { return mainFont != nullptr; }
#else
    bool isValid() const noexcept { return library != nullptr; }
#endif

    // =========================================================================
    // Static utilities
    // =========================================================================

    /**
     * @brief Returns the device-pixel ratio of the primary display.
     *
     * Queries `juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()`.
     * Returns `1.0f` if no display is available (headless / unit test context).
     *
     * @return Display scale factor (e.g. `2.0f` on Retina, `1.0f` on standard).
     */
    static float getDisplayScale() noexcept;

    /**
     * @brief Registers embedded Display Mono font data with the platform font system.
     *
     * On macOS: registers `DisplayMonoBook_ttf`, `DisplayMonoMedium_ttf`, and
     * `DisplayMonoBold_ttf` from BinaryData via `CTFontManagerRegisterGraphicsFont`.
     * This makes them available to CoreText by family name ("Display Mono").
     *
     * On Linux/Windows: no-op — embedded fonts are loaded directly via
     * `FT_New_Memory_Face` in `loadFaces()`.
     *
     * @note Safe to call multiple times; registration is guarded by a static flag.
     */
    static void registerEmbeddedFonts();

    // =========================================================================
    // Metrics and rasterization
    // =========================================================================

    /**
     * @brief Calculates cell geometry metrics for a given cell height.
     *
     * Queries the active font for ascent, descent, leading, and space-glyph
     * advance width, then fills a `Metrics` struct with logical, physical, and
     * fixed-point values.
     *
     * On macOS: uses `CTFontGetAscent`, `CTFontGetDescent`, `CTFontGetLeading`,
     * and `CTFontGetAdvancesForGlyphs` on a temporary scaled copy of `mainFont`.
     *
     * On Linux/Windows: uses `FT_Face::size->metrics` fields.
     *
     * @param heightPx  Desired cell height in logical (CSS) pixels.
     * @return Populated `Metrics` struct; `isValid()` returns false if the font
     *         is not loaded.
     */
    Metrics calcMetrics (float heightPx) noexcept;

    /**
     * @brief Rasterizes a single Unicode codepoint to a JUCE image.
     *
     * Used by `GlyphAtlas` to produce bitmaps for atlas upload.  Font selection
     * priority:
     * 1. Primary font (`mainFont` / regular face).
     * 2. Emoji font — if the primary has no glyph for `codepoint`.
     * 3. Nerd Font — if neither primary nor emoji has a glyph.
     * 4. Solid white rectangle — special-case for U+2588 FULL BLOCK when no
     *    glyph is available.
     *
     * The returned image dimensions are a multiple of `physCellW` × `physCellH`
     * to support wide (double-width) glyphs.
     *
     * @param codepoint  Unicode scalar value to rasterize.
     * @param fontSize   Cell height in logical pixels (used to scale the font).
     * @param isColor    Set to `true` if the emoji font was used (ARGB output);
     *                   `false` for monochrome glyphs (white-on-transparent ARGB).
     * @return ARGB `juce::Image` containing the rendered glyph, or an invalid
     *         image if rasterization failed.
     *
     * @note The image is in physical (device) pixels.  Callers must not scale it
     *       further before uploading to the GPU atlas.
     */
    juce::Image rasterizeToImage (uint32_t codepoint, float fontSize, bool& isColor) noexcept;

private:
    // =========================================================================
    // Fixed-point helpers
    // =========================================================================

    /**
     * @brief Converts a 26.6 fixed-point value to pixels, rounding up.
     * @param v26_6  Value in FreeType 26.6 fixed-point format.
     * @return Ceiling pixel count.
     */
    static int ceil26_6ToPx (int v26_6) noexcept;

    /**
     * @brief Converts a floating-point pixel value to 26.6 fixed-point, rounding.
     * @param px  Pixel value.
     * @return Nearest 26.6 fixed-point integer.
     */
    static int roundFloatPxTo26_6 (float px) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initializes the font backend and loads all faces.
     *
     * On macOS: calls `registerEmbeddedFonts()`, creates the HarfBuzz scratch
     * buffer, then calls `loadFaces()`.
     *
     * On Linux/Windows: calls `FT_Init_FreeType`, creates the HarfBuzz scratch
     * buffer, then calls `loadFaces()`.  If `FT_Init_FreeType` fails, the
     * instance remains invalid (`isValid()` returns false).
     */
    void initialize();

    /**
     * @brief Loads all font handles at the current `fontSize`.
     *
     * **macOS:** Creates `CTFontRef` handles for `mainFont`, `emojiFont`,
     * `identityFont` (Display Mono), and `nerdFont` (Symbols Nerd Font from
     * BinaryData).  Registers Display Mono and Nerd Font with `FontCollection`
     * at slots 0 and 1 respectively.
     *
     * **Linux/Windows:** Resolves the font file path via `resolveFontPath()`,
     * loads the regular face with `FT_New_Face`, loads the emoji face from the
     * system emoji font path, and loads the Nerd Font face from BinaryData via
     * `FT_New_Memory_Face`.  Registers faces with `FontCollection`.
     *
     * @note Called once from `initialize()`.  Not called again on resize;
     *       `setSize()` updates existing handles in-place.
     */
    void loadFaces();

    /**
     * @brief Resolves the absolute file path for the user's preferred font.
     *
     * Calls `discoverFont (userFamily)`.  If that returns empty, falls back to
     * the platform default monospace font, then to `juce::Font::getDefaultMonospacedFontName()`.
     *
     * @return Absolute path to a font file, or an empty string if no font was found.
     *
     * @note Only used by the FreeType backend; CoreText resolves fonts by name.
     */
    juce::String resolveFontPath();

    /**
     * @brief Discovers the file path for a font family by name.
     *
     * Dispatches to the platform-specific implementation:
     * - macOS: `discoverFontMac()`
     * - Linux: `discoverFontLinux()`
     * - Windows: returns empty string (not yet implemented).
     *
     * @param familyName  Font family name to search for (e.g. "JetBrains Mono").
     * @return Absolute path to the font file, or empty if not found.
     */
    juce::String discoverFont (const juce::String& familyName);

    /**
     * @brief Returns the family name or path of the system emoji font.
     *
     * - macOS: returns `"Apple Color Emoji"` (resolved by CoreText by name).
     * - Linux: returns the path to `"Noto Color Emoji"` via fontconfig.
     * - Windows: returns empty string.
     *
     * @return Family name (macOS) or file path (Linux) of the emoji font.
     */
    juce::String discoverEmojiFont();

    // =========================================================================
    // Shaping internals
    // =========================================================================

    /**
     * @brief Fast path for single ASCII codepoints (U+0000–U+007F).
     *
     * Bypasses HarfBuzz entirely: looks up the glyph index directly via
     * `CTFontGetGlyphsForCharacters` (macOS) or `FT_Get_Char_Index` (Linux),
     * then queries the advance width.  Writes one `Glyph` into `shapingBuffer`.
     *
     * @param codepoint  ASCII codepoint (must be < 128).
     * @return ShapeResult with `count == 1` on success, `count == 0` if the
     *         glyph is not in the font.
     */
    ShapeResult shapeASCII (uint32_t codepoint) noexcept;

    /**
     * @brief Shapes codepoints using HarfBuzz.
     *
     * Resets `scratchBuffer`, adds the codepoints as UTF-32, guesses segment
     * properties, and calls `hb_shape`.  Copies the resulting glyph info and
     * positions into `shapingBuffer` (growing it if needed).  Returns an empty
     * result if all output glyphs are `.notdef` (glyph index 0).
     *
     * @param style       Font style — selects the HarfBuzz font via `getHbFont`.
     * @param codepoints  UTF-32 codepoint array.
     * @param count       Number of codepoints.
     * @return ShapeResult; `count == 0` if all glyphs are `.notdef`.
     */
    ShapeResult shapeHarfBuzz (Style style, const uint32_t* codepoints, size_t count) noexcept;

    /**
     * @brief macOS-only fallback shaper using CoreText font substitution.
     *
     * For each codepoint that `mainFont` cannot render, queries
     * `CTFontCreateForString` to find a system font that can.  Results are
     * cached in `fallbackFontCache` (keyed by codepoint) to avoid repeated
     * CoreText lookups.  Fonts identified as "LastResort" are rejected.
     *
     * The `ShapeResult::fontHandle` is set to the fallback `CTFontRef` so the
     * renderer can pass the correct handle to `GlyphAtlas::getOrRasterize`.
     *
     * @param codepoints  UTF-32 codepoint array.
     * @param count       Number of codepoints.
     * @return ShapeResult; `count == 0` if no codepoint could be shaped.
     */
    ShapeResult shapeFallback (const uint32_t* codepoints, size_t count) noexcept;

    // =========================================================================
    // Platform-specific font discovery
    // =========================================================================

    #if JUCE_MAC
    /**
     * @brief Discovers a font file path on macOS using CoreText.
     *
     * Creates a `CTFontDescriptor` for the family name, instantiates a
     * `CTFontRef`, then extracts the file URL via `kCTFontURLAttribute`.
     *
     * @param familyName  Font family name to look up.
     * @return Absolute POSIX path to the font file, or empty if not found.
     */
    juce::String discoverFontMac (const juce::String& familyName);
    #endif

    #if JUCE_LINUX
    /**
     * @brief Discovers a font file path on Linux using fontconfig.
     *
     * Builds an `FcPattern` requesting the given family with `FC_MONO` spacing,
     * runs `FcFontMatch`, and extracts the `FC_FILE` property from the result.
     *
     * @param familyName  Font family name to look up.
     * @return Absolute path to the font file, or empty if not found.
     */
    juce::String discoverFontLinux (const juce::String& familyName);
    #endif

    #if JUCE_WINDOWS
    /**
     * @brief Discovers a font file path on Windows via the font registry.
     *
     * Enumerates `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`
     * looking for an entry whose name starts with the requested family.
     *
     * @param familyName  Font family name to look up (e.g. "Cascadia Code").
     * @return Absolute path to the font file, or empty if not found.
     */
    juce::String discoverFontWindows (const juce::String& familyName);
    #endif

    // =========================================================================
    // Platform-specific font handles
    // =========================================================================

#if JUCE_MAC
    /** @name macOS font handles (CTFontRef stored as void*)
     *  All handles are `CTFontRef` values cast to `void*` to avoid exposing
     *  CoreText types in the shared header.
     * @{ */

    void* mainFont          { nullptr }; ///< Primary monospace CTFontRef (user-selected family).
    void* emojiFont         { nullptr }; ///< Apple Color Emoji CTFontRef.
    hb_font_t* shapingFont  { nullptr }; ///< HarfBuzz font wrapping mainFont.
    hb_font_t* emojiShapingFont { nullptr }; ///< HarfBuzz font wrapping emojiFont.
    void* identityFont      { nullptr }; ///< Display Mono CTFontRef (FontCollection slot 0).
    hb_font_t* identityShapingFont { nullptr }; ///< HarfBuzz font wrapping identityFont.
    void* nerdFont          { nullptr }; ///< Symbols Nerd Font CTFontRef (FontCollection slot 1).
    hb_font_t* nerdShapingFont { nullptr }; ///< HarfBuzz font wrapping nerdFont.

    /**
     * @brief Cache of CoreText fallback fonts keyed by Unicode codepoint.
     *
     * Populated lazily by `shapeFallback()`.  Each entry maps a codepoint to
     * the `CTFontRef` (as `void*`) that can render it, or `nullptr` if no
     * suitable font was found.  Cleared entirely by `setSize()`.
     */
    std::unordered_map<uint32_t, void*> fallbackFontCache;

    /** @} */
#else
    /** @name FreeType font handles (Linux / Windows)
     * @{ */

    FT_Library library { nullptr }; ///< FreeType library instance; owns all FT_Face handles.

    /**
     * @brief Array of FreeType faces for the four style variants.
     *
     * Indexed by `static_cast<int>(Style::*)`.  Slots for bold/italic/boldItalic
     * may be null if the font family does not provide those variants.
     */
    std::array<Face, 4> faces;

    FT_Face    emojiFace        { nullptr }; ///< FreeType face for the system emoji font.
    hb_font_t* emojiShapingFont { nullptr }; ///< HarfBuzz font wrapping emojiFace.
    FT_Face    nfFace           { nullptr }; ///< FreeType face for Symbols Nerd Font (from BinaryData).
    hb_font_t* nerdShapingFont  { nullptr }; ///< HarfBuzz font wrapping nfFace.

    /** @} */
#endif

    // =========================================================================
    // Shared state
    // =========================================================================

    /**
     * @brief Reusable HarfBuzz buffer for all shaping calls.
     *
     * Reset and reused on every `shapeHarfBuzz()` / `shapeEmoji()` call to
     * avoid per-call allocation.  Created in `initialize()`, destroyed in the
     * destructor.
     */
    hb_buffer_t* scratchBuffer { nullptr };

    /**
     * @brief Heap-allocated output buffer for shaped glyphs.
     *
     * Grown on demand (never shrunk) to hold the largest glyph run seen so far.
     * All `shape*` methods write into this buffer and return a pointer to it.
     *
     * @warning The pointer returned by `shapeText()` / `shapeEmoji()` is
     *          invalidated by the next shaping call.
     */
    juce::HeapBlock<Glyph> shapingBuffer;

    /** @brief Current capacity of `shapingBuffer` in number of `Glyph` elements. */
    int shapingBufferCapacity { 0 };

    /** @brief User-requested font family name, stored for resize operations. */
    juce::String userFamily;

    /** @brief Current font size in CSS points. Updated by `setSize()`. */
    float fontSize { 0.0f };
};
