/**
 * @file jreng_font.h
 * @brief Lightweight font value type mirroring juce::Font.
 *
 * `jreng::Font` carries size, style, and a reference to the backing
 * `jreng::Typeface`.  It is copyable, lightweight, and designed to be
 * set on a graphics context via `setFont()` — identical to how
 * `juce::Font` is used with `juce::Graphics`.
 *
 * @see jreng::Typeface
 */

#pragma once

namespace jreng
{

/**
 * @struct Font
 * @brief Lightweight font descriptor: size, style, and typeface reference.
 *
 * Mirrors `juce::Font` — a value type that describes which font to use
 * for rendering.  The heavy resources (FreeType handles, HarfBuzz shapers,
 * atlas cache) live in `Typeface`; `Font` is just the selector.
 *
 * @par Usage
 * @code
 * jreng::Font font (typeface, 14.0f, Typeface::Style::bold);
 * g.setFont (font);
 * g.drawGlyphs (codes, positions, count);
 * @endcode
 *
 * @par Copyable
 * Font holds a reference to Typeface (which must outlive the Font)
 * plus scalar state.  Copies are shallow and cheap.
 */
struct Font
{
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Font with default size and regular style.
     * @param typeface  Backing typeface resource; must outlive this Font.
     */
    explicit Font (Typeface& typeface) noexcept
        : typeface (typeface)
    {
    }

    /**
     * @brief Construct a Font with a specific size.
     * @param typeface  Backing typeface resource.
     * @param size      Font size in logical (CSS) pixels.
     */
    Font (Typeface& typeface, float size) noexcept
        : typeface (typeface), size (size)
    {
    }

    /**
     * @brief Construct a Font with size and style.
     * @param typeface  Backing typeface resource.
     * @param size      Font size in logical (CSS) pixels.
     * @param style     Style variant (regular, bold, italic, boldItalic).
     */
    Font (Typeface& typeface, float size, Typeface::Style style) noexcept
        : typeface (typeface), size (size), style (style)
    {
    }

    // =========================================================================
    // Mutation
    // =========================================================================

    /** @brief Set the font size in logical pixels. */
    void setSize (float newSize) noexcept { size = newSize; }

    /** @brief Set the style variant. */
    void setStyle (Typeface::Style newStyle) noexcept { style = newStyle; }

    /** @brief Set whether this font targets the emoji atlas. */
    void setEmoji (bool isEmoji) noexcept { emoji = isEmoji; }

    /**
     * @brief Apply face context from a glyph run.
     *
     * When shaping produces a glyph run from a non-primary face (e.g. Nerd
     * Font, fallback font), the run carries internal face context that Font
     * needs for correct atlas lookup.  Call this after shaping to transfer
     * that context into the Font.
     *
     * @param run  Glyph run from Typeface::shapeText() or shapeEmoji().
     */
    void applyGlyphRun (const Typeface::GlyphRun& run) noexcept
    {
        faceHandle = run.fontHandle;
    }

    // =========================================================================
    // Accessors — for graphics context configuration
    // =========================================================================

    /** @brief Font size in logical pixels. */
    float getSize () const noexcept { return size; }

    /** @brief Current style variant. */
    Typeface::Style getStyle () const noexcept { return style; }

    /** @brief Whether this font targets the emoji atlas. */
    bool isEmoji () const noexcept { return emoji; }

    /** @brief The backing typeface resource. */
    Typeface& getTypeface () noexcept { return typeface; }

    /** @brief The backing typeface resource (const). */
    const Typeface& getTypeface () const noexcept { return typeface; }

    // =========================================================================
    // Glyph access
    // =========================================================================

    /**
     * @brief Look up or rasterize a glyph using a default (inactive) constraint and span 1.
     * @param glyphCode  Font-internal glyph index from HarfBuzz shaping.
     * @return Pointer to the atlas Region, or nullptr if rasterization failed.
     * @note **MESSAGE THREAD**.
     */
    jreng::Glyph::Region* getGlyph (uint16_t glyphCode) noexcept;

    /**
     * @brief Look up or rasterize a glyph with an explicit constraint and span.
     * @param glyphCode   Font-internal glyph index.
     * @param constraint  Nerd Font scaling / alignment constraint.
     * @param span        Number of cells the glyph spans horizontally.
     * @return Pointer to the atlas Region, or nullptr if rasterization failed.
     * @note **MESSAGE THREAD**.
     */
    jreng::Glyph::Region* getGlyph (uint16_t glyphCode,
                                     const jreng::Glyph::Constraint& constraint,
                                     uint8_t span) noexcept;

    // =========================================================================
    // Atlas delegation
    // =========================================================================

    /**
     * @brief Drain staged bitmaps from the atlas for GL upload.
     * Delegates to Typeface::consumeStagedBitmaps().
     * @note **GL THREAD**.
     */
    void consumeStagedBitmaps (juce::HeapBlock<jreng::Glyph::StagedBitmap>& out,
                                int& outCount) noexcept;

    /**
     * @brief Returns true if any bitmaps are queued for GL upload.
     * Delegates to Typeface::hasStagedBitmaps().
     * @note **GL THREAD**.
     */
    bool hasStagedBitmaps() const noexcept;

private:
    Typeface& typeface;
    float size { 0.0f };
    Typeface::Style style { Typeface::Style::regular };
    bool emoji { false };
    void* faceHandle { nullptr }; ///< Override font handle from GlyphRun; nullptr = derive from style/emoji.
};

} // namespace jreng
