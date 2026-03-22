/**
 * @file jreng_text_layout.h
 * @brief Drop-in replacement for juce::TextLayout using jreng::Font (HarfBuzz) shaping
 *        and libunibreak (UAX #14) line breaking.
 *
 * TextLayout accepts a juce::AttributedString, shapes each attribute run through
 * jreng::Font::shapeText(), breaks lines via libunibreak's set_linebreaks_utf32(),
 * and outputs positioned glyphs for either the GL instanced renderer or (stub)
 * juce::Graphics.
 *
 * ### Coordinate system
 * All positions are in logical (CSS) pixels.  Physical-pixel conversion is the
 * caller's responsibility (multiply by the display scale before passing to GL).
 *
 * ### Thread context
 * **MESSAGE THREAD** — createLayout() calls Font::shapeText() which is MESSAGE THREAD
 * only.  draw(GLTextRenderer&) is called on the **GL THREAD** after layout is built.
 *
 * @see jreng::Font
 * @see jreng::Glyph::Atlas
 * @see jreng::Glyph::GLTextRenderer
 */

#pragma once

namespace jreng
{

/**
 * @class TextLayout
 * @brief Shapes and lays out an AttributedString using HarfBuzz and libunibreak.
 *
 * Mirrors the public surface of juce::TextLayout so it can be used as a
 * drop-in replacement.  Internally it delegates to jreng::Font::shapeText()
 * for glyph shaping and libunibreak's set_linebreaks_utf32() for UAX #14
 * compliant line-break opportunity detection.
 *
 * @par Typical usage
 * @code
 * jreng::TextLayout layout;
 * layout.createLayout (attributedString, font, maxWidth);
 *
 * // GL THREAD:
 * layout.draw (renderer, atlas, font, areaPx);
 * @endcode
 */
class TextLayout
{
public:
    // =========================================================================
    // Inner types — mirror juce::TextLayout structure
    // =========================================================================

    /**
     * @struct Glyph
     * @brief A single shaped and positioned glyph within a Run.
     */
    struct Glyph
    {
        int   glyphCode { 0 };         ///< Font-internal glyph ID from HarfBuzz.
        juce::Point<float> anchor;     ///< Position relative to the line origin (baseline).
        float width     { 0.0f };      ///< Horizontal advance width in logical pixels.
    };

    /**
     * @struct Run
     * @brief A sequence of shaped glyphs sharing a single font and colour.
     *
     * Corresponds to one juce::AttributedString::Attribute.  The @p style field
     * is resolved from the attribute's juce::Font bold/italic flags.
     */
    struct Run
    {
        juce::Font         font;                             ///< Source JUCE font for this attribute run.
        juce::Colour       colour;                           ///< Text colour for this run.
        juce::Array<Glyph> glyphs;                          ///< Shaped and positioned glyphs.
        juce::Range<int>   stringRange;                      ///< Character range in the original string.
        Font::Style        style { Font::Style::regular };  ///< Resolved style from bold/italic flags.

        /**
         * @brief Returns the [minX, maxX) extent of all glyphs in this run.
         * @return Horizontal range in the run's local coordinate space.
         */
        juce::Range<float> getRunBoundsX() const noexcept;
    };

    /**
     * @struct Line
     * @brief A single typeset line containing one or more Runs.
     *
     * @p lineOrigin is the baseline origin of this line in layout space (Y
     * increases downward from the top of the layout).
     */
    struct Line
    {
        juce::OwnedArray<Run> runs;          ///< Ordered runs for this line.
        juce::Range<int>      stringRange;   ///< Character range spanned by this line.
        juce::Point<float>    lineOrigin;    ///< Baseline origin in layout space.
        float ascent  { 0.0f };             ///< Ascent above baseline in logical pixels.
        float descent { 0.0f };             ///< Descent below baseline in logical pixels.
        float leading { 0.0f };             ///< Extra spacing below descent in logical pixels.

        /**
         * @brief Returns the bounding rectangle of this line in layout space.
         *
         * The rectangle's top edge is lineOrigin.y - ascent; its height is
         * ascent + descent + leading.
         *
         * @return Line bounds in layout-space logical pixels.
         */
        juce::Rectangle<float> getLineBounds() const noexcept;
    };

    // =========================================================================
    // Construction
    // =========================================================================

    TextLayout()  = default;
    ~TextLayout() = default;

    // =========================================================================
    // Layout creation
    // =========================================================================

    /**
     * @brief Shape and lay out @p text, wrapping at @p maxWidth.
     *
     * Converts the string to UTF-32, queries libunibreak for break opportunities,
     * then shapes each attribute run via Font::shapeText().  The resulting glyphs
     * are wrapped into Line objects according to the break opportunities and
     * @p maxWidth.
     *
     * @param text      Attributed string to lay out.
     * @param font      jreng::Font instance used for shaping and metrics.
     * @param maxWidth  Maximum line width in logical pixels before wrapping.
     *
     * @note **MESSAGE THREAD** only.
     */
    void createLayout (const juce::AttributedString& text,
                       jreng::Font& font,
                       float maxWidth) noexcept;

    /**
     * @brief Shape and lay out @p text, clipping at @p maxWidth and @p maxHeight.
     *
     * Identical to createLayout(text, font, maxWidth) but stops adding lines once
     * the accumulated height exceeds @p maxHeight.
     *
     * @param text      Attributed string to lay out.
     * @param font      jreng::Font instance used for shaping and metrics.
     * @param maxWidth  Maximum line width in logical pixels before wrapping.
     * @param maxHeight Maximum total layout height in logical pixels.
     *
     * @note **MESSAGE THREAD** only.
     */
    void createLayout (const juce::AttributedString& text,
                       jreng::Font& font,
                       float maxWidth,
                       float maxHeight) noexcept;

    // =========================================================================
    // Drawing
    // =========================================================================

    /**
     * @brief Render all lines via the GL instanced renderer.
     *
     * For each glyph in each run in each line, looks up (or rasterizes) the
     * glyph in @p atlas and emits a Glyph::Render::Quad.  Mono and emoji quads
     * are separated and submitted to @p renderer via drawQuads().
     *
     * Caller must have called renderer.setViewportSize() before this.
     *
     * @param renderer  GL renderer that owns the atlas textures and shaders.
     * @param atlas     Glyph atlas for rasterization and UV lookup.
     * @param font      Font used during layout; provides getFontHandle() per style.
     * @param area      Bounding rectangle in physical pixels (layout origin mapped
     *                  to area.getTopLeft()).
     *
     * @note **GL THREAD** only.
     */
    void draw (jreng::Glyph::GLTextRenderer& renderer,
               jreng::Glyph::Atlas& atlas,
               jreng::Font& font,
               juce::Rectangle<float> area) const noexcept;

    /**
     * @brief CPU draw path — stub for Plan 3.
     *
     * Currently a no-op.  Plan 3 will implement JUCE Graphics-based fallback
     * rendering.
     *
     * @param g     JUCE Graphics context.
     * @param area  Target rectangle in component space.
     */
    void draw (juce::Graphics& g,
               juce::Rectangle<float> area) const noexcept;

    // =========================================================================
    // Accessors
    // =========================================================================

    /** @brief Returns the total layout width in logical pixels. */
    float getWidth()    const noexcept { return width; }

    /** @brief Returns the total layout height in logical pixels. */
    float getHeight()   const noexcept { return height; }

    /** @brief Returns the number of lines in this layout. */
    int   getNumLines() const noexcept { return lines.size(); }

    /**
     * @brief Returns a reference to the line at @p index.
     * @param index  Zero-based line index; must be < getNumLines().
     */
    Line& getLine (int index) const noexcept { return *lines.getUnchecked (index); }

    // =========================================================================
    // Static convenience
    // =========================================================================

    /**
     * @brief Returns the bounding box of @p text shaped by @p font with no
     *        width constraint.
     *
     * Creates a temporary TextLayout with a very large maxWidth and returns the
     * resulting bounds.
     *
     * @param text  Attributed string to measure.
     * @param font  Font to use for shaping and metrics.
     * @return Tight bounding rectangle in logical pixels.
     */
    static juce::Rectangle<float> getStringBounds (const juce::AttributedString& text,
                                                    jreng::Font& font);

    /**
     * @brief Returns the total advance width of @p text shaped by @p font.
     *
     * Convenience wrapper around getStringBounds().
     *
     * @param text  Attributed string to measure.
     * @param font  Font to use for shaping and metrics.
     * @return Total advance width in logical pixels.
     */
    static float getStringWidth (const juce::AttributedString& text,
                                 jreng::Font& font);

private:
    // =========================================================================
    // Private data
    // =========================================================================

    juce::OwnedArray<Line> lines;  ///< Ordered lines produced by createLayout().
    float width  { 0.0f };        ///< Total layout width in logical pixels.
    float height { 0.0f };        ///< Total layout height in logical pixels.

    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Maps juce::Font bold/italic flags to jreng::Font::Style.
     *
     * | Bold  | Italic | Result       |
     * |-------|--------|--------------|
     * | false | false  | regular      |
     * | true  | false  | bold         |
     * | false | true   | italic       |
     * | true  | true   | boldItalic   |
     *
     * @param juceFont  JUCE font whose bold/italic flags are inspected.
     * @return Corresponding Font::Style enum value.
     */
    static Font::Style resolveStyle (const juce::Font& juceFont) noexcept;

    /**
     * @brief Recomputes @p width and @p height from the current line array.
     *
     * Unions all line bounds to produce the tight layout bounding box.
     * Called at the end of createLayout().
     */
    void recalculateSize() noexcept;

    /**
     * @brief Core layout implementation shared by both createLayout overloads.
     *
     * @param text       Attributed string to lay out.
     * @param font       jreng::Font for shaping and metrics.
     * @param maxWidth   Maximum line width before wrapping.
     * @param maxHeight  Maximum total height; use a very large value to disable.
     */
    void createLayoutInternal (const juce::AttributedString& text,
                               jreng::Font& font,
                               float maxWidth,
                               float maxHeight) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextLayout)
};

} // namespace jreng
