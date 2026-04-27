/**
 * @file Screen.cpp
 * @brief Screen lifecycle, configuration, geometry, and render entry point.
 *
 * This translation unit implements:
 * - `Screen` constructor / destructor and `calc()`.
 * - Viewport, font, theme, and selection configuration.
 * - Grid geometry helpers (`getCellBounds`, `cellAtPoint`).
 * - `render()` — the top-level per-frame entry point.
 * - `allocateRenderCache()` — per-row glyph and background cache allocation.
 *
 * The rendering pipeline (glyph shaping, snapshot building) is split across:
 * - `ScreenRender.cpp`   — `buildSnapshot()`, `processCellForSnapshot()`, `buildCellInstance()`.
 * - `ScreenSnapshot.cpp` — `updateSnapshot()`.
 *
 * @see Screen.h
 * @see ScreenRender.cpp
 * @see ScreenSnapshot.cpp
 */

#include "Screen.h"
#include "../../lua/Engine.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

/**
 * @brief Constructs the screen.
 *
 * Initialises `Resources`, calls `calc()` to derive cell dimensions,
 * then calls `reset()` to zero the cache dimension sentinels.
 */
template<typename Renderer>
Screen<Renderer>::Screen (jam::Font& fontToUse,
                          jam::Glyph::Packer& packerToUse,
                          typename Renderer::Atlas& atlasToUse,
                          Terminal::ImageAtlas& imageAtlasToUse)
    : font (fontToUse)
    , packer (packerToUse)
    , atlasRef (atlasToUse)
    , imageAtlas (imageAtlasToUse)
    , resources()
    , baseFontSize (lua::Engine::getContext()->dpiCorrectedFontSize())
{
    textRenderer.setPacker (packer);
    calc();
    reset();
}

template<typename Renderer>
Screen<Renderer>::~Screen()
{
    textRenderer.closeContext();
}

/**
 * @brief Recomputes cell dimensions and grid size from the current font metrics.
 *
 * Calls `Fonts::calcMetrics()` with `baseFontSize`.  If the metrics are valid,
 * updates all cell dimension fields and recomputes `numCols` / `numRows` from
 * the current viewport size.  Does nothing if the viewport has zero area.
 */
template<typename Renderer>
void Screen<Renderer>::calc() noexcept
{
    const jam::Typeface::Metrics fm { font.getResolvedTypeface()->calcMetrics (baseFontSize) };

    if (fm.isValid())
    {
        const int adjustedPhysCellH { static_cast<int> (static_cast<float> (fm.physCellH) * lineHeightMultiplier) };
        const int extraPixels { adjustedPhysCellH - fm.physCellH };
        const int adjustedPhysBaseline { fm.physBaseline + extraPixels / 2 };

        physCellWidth = static_cast<int> (static_cast<float> (fm.physCellW) * cellWidthMultiplier);
        physCellHeight = adjustedPhysCellH;
        physBaseline = adjustedPhysBaseline;

        if (physCellWidth > 0 and physCellHeight > 0 and glViewportWidth > 0 and glViewportHeight > 0)
        {
            numCols = glViewportWidth / physCellWidth;
            numRows = glViewportHeight / physCellHeight;
        }

        const float scale { jam::Typeface::getDisplayScale() };

        cellWidth = scale > 0.0f ? static_cast<int> (static_cast<float> (physCellWidth) / scale) : fm.logicalCellW;
        cellHeight = scale > 0.0f ? static_cast<int> (static_cast<float> (physCellHeight) / scale) : fm.logicalCellH;
        baseline = scale > 0.0f ? static_cast<int> (static_cast<float> (physBaseline) / scale) : fm.logicalBaseline;

        if (physCellWidth > 0 and physCellHeight > 0 and onPhysCellDimensionsChanged)
        {
            onPhysCellDimensionsChanged (physCellWidth, physCellHeight);
        }
    }
}

/**
 * @brief Updates the render viewport and recomputes grid dimensions.
 *
 * Stores the new bounds, scales them to physical pixels, and calls `calc()`.
 *
 * @param bounds  New viewport bounds in logical pixel space.
 */
template<typename Renderer>
void Screen<Renderer>::setViewport (const juce::Rectangle<int>& bounds) noexcept
{
    viewportX = bounds.getX();
    viewportY = bounds.getY();
    viewportWidth = bounds.getWidth();
    viewportHeight = bounds.getHeight();

    const float scale { jam::Typeface::getDisplayScale() };
    glViewportX = static_cast<int> (static_cast<float> (bounds.getX()) * scale);
    glViewportY = static_cast<int> (static_cast<float> (bounds.getY()) * scale);
    glViewportWidth = static_cast<int> (static_cast<float> (bounds.getWidth()) * scale);
    glViewportHeight = static_cast<int> (static_cast<float> (bounds.getHeight()) * scale);

    calc();
}

/// @return `numCols` as computed by `calc()`.
template<typename Renderer>
int Screen<Renderer>::getNumCols() const noexcept
{
    return numCols;
}

/// @return `numRows` as computed by `calc()`.
template<typename Renderer>
int Screen<Renderer>::getNumRows() const noexcept
{
    return numRows;
}

/// @return `cellWidth` in logical pixels.
template<typename Renderer>
int Screen<Renderer>::getCellWidth() const noexcept
{
    return cellWidth;
}

/// @return `cellHeight` in logical pixels.
template<typename Renderer>
int Screen<Renderer>::getCellHeight() const noexcept
{
    return cellHeight;
}

/// @return `baseFontSize` in points (includes zoom).
template<typename Renderer>
float Screen<Renderer>::getBaseFontSize() const noexcept
{
    return baseFontSize;
}

/**
 * @brief Returns the logical pixel bounds of the cell at (@p col, @p row).
 *
 * Clamps both indices to the valid grid range.  Returns an empty rectangle
 * if `numCols` or `numRows` is zero.
 *
 * @param col  Column index (0-based).
 * @param row  Row index (0-based).
 * @return     Bounds in logical pixel space relative to the viewport origin.
 */
template<typename Renderer>
juce::Rectangle<int> Screen<Renderer>::getCellBounds (int col, int row) const noexcept
{
    juce::Rectangle<int> result {};

    if (numCols > 0 and numRows > 0)
    {
        const int clampedCol { juce::jlimit (0, numCols - 1, col) };
        const int clampedRow { juce::jlimit (0, numRows - 1, row) };

        // Derive logical position from the physical-pixel grid used by the
        // GL renderer (row * physCellHeight) divided back by display scale.
        // This eliminates per-row rounding drift at fractional scales (e.g.
        // 150%) between the JUCE component overlay and GL-rendered glyphs.
        const float scale { jam::Typeface::getDisplayScale() };
        const int x { viewportX + static_cast<int> (static_cast<float> (clampedCol * physCellWidth) / scale) };
        const int y { viewportY + static_cast<int> (static_cast<float> (clampedRow * physCellHeight) / scale) };
        const int w { static_cast<int> (static_cast<float> (physCellWidth) / scale) };
        const int h { static_cast<int> (static_cast<float> (physCellHeight) / scale) };

        result = { x, y, juce::jmax (1, w), juce::jmax (1, h) };
    }

    return result;
}

/**
 * @brief Returns the grid cell that contains the logical pixel point (@p x, @p y).
 *
 * Converts the logical offset from the viewport origin to physical pixels by
 * multiplying by the display scale, then divides by `physCellWidth` /
 * `physCellHeight` — the same physical grid the GL renderer uses.  This
 * matches the inverse of `getCellBounds()` and eliminates per-row rounding
 * drift at fractional DPI scales (e.g. 125%, 150%) where
 * `physCellHeight / scale ≠ cellHeight` due to integer truncation.
 *
 * @param x  Logical pixel X coordinate (component-local).
 * @param y  Logical pixel Y coordinate (component-local).
 * @return   Grid cell (col, row) clamped to [0, numCols-1] × [0, numRows-1].
 */
template<typename Renderer>
juce::Point<int> Screen<Renderer>::cellAtPoint (int x, int y) const noexcept
{
    juce::Point<int> result { 0, 0 };

    if (physCellWidth > 0 and physCellHeight > 0)
    {
        const float scale { jam::Typeface::getDisplayScale() };
        const int physX { static_cast<int> (static_cast<float> (x - viewportX) * scale) };
        const int physY { static_cast<int> (static_cast<float> (y - viewportY) * scale) };
        const int col { juce::jlimit (0, numCols - 1, physX / physCellWidth) };
        const int row { juce::jlimit (0, numRows - 1, physY / physCellHeight) };
        result = { col, row };
    }

    return result;
}

/**
 * @brief Sets the active text selection for overlay rendering.
 *
 * Stores a non-owning pointer.  Pass `nullptr` to clear the selection.
 *
 * @param sel  Pointer to the active `ScreenSelection`, or `nullptr`.
 */
template<typename Renderer>
void Screen<Renderer>::setSelection (const ScreenSelection* sel) noexcept
{
    selection = sel;
}

/**
 * @brief Sets the always-on link underlay for click-mode underline rendering.
 *
 * Stores a non-owning pointer to the clickable link span array.  Because every
 * row is rebuilt from `Grid` on every frame, the underlines are reflected
 * immediately on the next `render()` call.
 *
 * @param spans  Pointer to the `LinkSpan` array, or `nullptr` to clear.
 * @param count  Number of elements in @p spans.
 */
template<typename Renderer>
void Screen<Renderer>::setLinkUnderlay (const LinkSpan* spans, int count) noexcept
{
    linkUnderlay = spans;
    linkUnderlayCount = count;
}

/**
 * @brief Returns a read-only reference to the active colour theme.
 *
 * @return Const reference to `resources.terminalColors`.
 */
template<typename Renderer>
const Theme& Screen<Renderer>::getTheme() const noexcept
{
    return resources.terminalColors;
}

template<typename Renderer>
jam::SnapshotBuffer<Render::Snapshot>& Screen<Renderer>::getSnapshotBuffer() noexcept
{
    return resources.snapshotBuffer;
}

/**
 * @brief Changes the font size and invalidates the glyph atlas.
 *
 * Calls `Fonts::setSize()`, clears the atlas (forcing re-rasterisation of all
 * glyphs at the new size), and calls `calc()` to recompute cell dimensions.
 * Does nothing if @p pointSize equals the current size.
 *
 * @param pointSize  New font size in points.
 */
template<typename Renderer>
void Screen<Renderer>::setFontSize (float pointSize) noexcept
{
    if (pointSize != baseFontSize)
    {
        baseFontSize = pointSize;
        font.getResolvedTypeface()->setSize (pointSize);
        packer.setDisplayScale (jam::Typeface::getDisplayScale());
        packer.clear();
        calc();
    }
}

/**
 * @brief Sets the line-height multiplier and recomputes cell dimensions.
 *
 * @param multiplier  Line-height multiplier (clamped to 0.5–3.0 by config).
 */
template<typename Renderer>
void Screen<Renderer>::setLineHeight (float multiplier) noexcept
{
    if (multiplier != lineHeightMultiplier)
    {
        lineHeightMultiplier = multiplier;
        calc();
    }
}

/**
 * @brief Sets the cell-width multiplier and recomputes cell dimensions.
 *
 * @param multiplier  Cell-width multiplier (clamped to 0.5–3.0 by config).
 */
template<typename Renderer>
void Screen<Renderer>::setCellWidth (float multiplier) noexcept
{
    if (multiplier != cellWidthMultiplier)
    {
        cellWidthMultiplier = multiplier;
        calc();
    }
}

/**
 * @brief Enables or disables HarfBuzz ligature shaping.
 *
 * @param enabled  `true` to enable ligatures.
 */
template<typename Renderer>
void Screen<Renderer>::setLigatures (bool enabled) noexcept
{
    ligatureEnabled = enabled;
}

/**
 * @brief Enables or disables synthetic bold (embolden) rendering.
 *
 * Forwards to `gl::GlyphAtlas::setEmbolden()` and clears the atlas if the value
 * changed, so all glyphs are re-rasterised with the new setting.
 *
 * @param enabled  `true` to enable embolden.
 */
template<typename Renderer>
void Screen<Renderer>::setEmbolden (bool enabled) noexcept
{
    if (enabled != packer.getEmbolden())
    {
        packer.setEmbolden (enabled);
        packer.clear();
    }
}

/**
 * @brief Replaces the active colour theme.
 *
 * @param theme  New theme to apply immediately.
 */
template<typename Renderer>
void Screen<Renderer>::setTheme (const Theme& theme) noexcept
{
    resources.terminalColors = theme;
}

/**
 * @brief Updates the selection-mode cursor state used for rendering.
 *
 * When @p active is true, `updateSnapshot()` will suppress the normal terminal
 * cursor and emit a block cursor at (@p col, @p row) using the theme's
 * `selectionCursorColour`.  Calling with @p active `false` restores the
 * normal terminal cursor.
 *
 * @param active  True to enable selection-mode cursor override.
 * @param row     Visible row index in the current viewport (0 = top row).
 * @param col     Column index (0-based).
 *
 * @note **MESSAGE THREAD** — call before `render()` each frame.
 */
template<typename Renderer>
void Screen<Renderer>::setSelectionCursor (bool active, int row, int col) noexcept
{
    selectionModeActive = active;
    selectionCursorRow = row;
    selectionCursorCol = col;
}

/// @brief Toggles debug rendering mode.
template<typename Renderer>
void Screen<Renderer>::toggleDebug() noexcept
{
    debugMode = not debugMode;
}

/// @return `true` if debug rendering mode is active.
template<typename Renderer>
bool Screen<Renderer>::isDebugMode() const noexcept
{
    return debugMode;
}

/// @return `true` if a new snapshot is waiting in the mailbox.
template<typename Renderer>
bool Screen<Renderer>::hasNewSnapshot() const noexcept
{
    return resources.snapshotBuffer.isReady();
}

/**
 * @brief Resets cache dimension sentinels to force reallocation on the next frame.
 *
 * Zeroes `cacheRows`, `cacheCols`, and `bgCacheCols` so the next `render()` call
 * reallocates the per-row glyph and background caches.
 */
template<typename Renderer>
void Screen<Renderer>::reset() noexcept
{
    cacheRows = 0;
    cacheCols = 0;
    bgCacheCols = 0;
}

/**
 * @brief Allocates per-row render caches for @p rows rows and @p cols columns.
 *
 * Each row gets `cols * 2` glyph slots (to accommodate wide characters and
 * ligatures) and `cols * 3` background slots (for bg + selection + underlay).
 * All arrays are zero-initialised.
 *
 * @param rows  Number of visible rows.
 * @param cols  Number of visible columns.
 */
template<typename Renderer>
void Screen<Renderer>::allocateRenderCache (int rows, int cols) noexcept
{
    const int maxGlyphs { cols * 2 };
    const size_t glyphSlots { static_cast<size_t> (rows) * static_cast<size_t> (maxGlyphs) };
    const size_t bgSlots { static_cast<size_t> (rows) * static_cast<size_t> (cols) * 3 };

    cachedMono.allocate (glyphSlots, true);
    cachedEmoji.allocate (glyphSlots, true);
    cachedBg.allocate (bgSlots, true);
    monoCount.allocate (static_cast<size_t> (rows), true);
    emojiCount.allocate (static_cast<size_t> (rows), true);
    bgCount.allocate (static_cast<size_t> (rows), true);
    previousCells.allocate (static_cast<size_t> (rows) * static_cast<size_t> (cols), true);

    // Image cell sidecar: one ImageQuad per cell per row maximum.
    cachedImages.allocate (static_cast<size_t> (rows) * static_cast<size_t> (cols), true);
    imageCacheCount.allocate (static_cast<size_t> (rows), true);

    cacheRows = rows;
    cacheCols = cols;
    bgCacheCols = cols * 3;
    maxGlyphsPerRow = cols * 2;
}

/**
 * @brief Performs one full render cycle: build snapshot from Grid, trigger repaint.
 *
 * Called once per frame from the terminal view on the **MESSAGE THREAD**.
 *
 * @par Steps
 * 1. Reallocates per-row render caches if the grid dimensions changed.
 * 2. Calls `buildSnapshot()` to process all rows and publish the snapshot.
 *
 * @param state  Current terminal state (cursor, dimensions, scroll offset).
 * @param grid   Terminal grid providing cell data.
 */
template<typename Renderer>
void Screen<Renderer>::render (State& state, Grid& grid) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };

    if (cols > 0 and rows > 0)
    {
        if (cacheRows != rows or cacheCols != cols)
        {
            allocateRenderCache (rows, cols);
        }

        buildSnapshot (state, grid);
    }
}

template class Screen<jam::Glyph::GLContext>;
template class Screen<jam::Glyph::GraphicsContext>;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
