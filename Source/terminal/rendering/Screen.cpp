/**
 * @file Screen.cpp
 * @brief Screen lifecycle, configuration, geometry, and render entry point.
 *
 * This translation unit implements:
 * - `Screen` constructor / destructor and `calc()`.
 * - Viewport, font, theme, and selection configuration.
 * - Grid geometry helpers (`getCellBounds`, `cellAtPoint`).
 * - `render()` — the top-level per-frame entry point.
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
template<typename Context>
Screen<Context>::Screen (jam::Font& fontToUse,
                          jam::Glyph::Packer& packerToUse,
                          typename Context::Atlas& atlasToUse)
    : font (fontToUse)
    , packer (packerToUse)
    , atlasRef (atlasToUse)
    , resources()
    , glyph (fontToUse, packerToUse)
    , baseFontSize (lua::Engine::getContext()->dpiCorrectedFontSize())
{
    textRenderer.setPacker (packer);
    calc();
    reset();
}

template<typename Context>
Screen<Context>::~Screen()
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
template<typename Context>
void Screen<Context>::calc() noexcept
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

        const float scale { jam::Typeface::getDisplayScale() };
        metrics = jam::tui::Metrics { physCellWidth, physCellHeight, scale };

        if (physCellWidth > 0 and physCellHeight > 0 and glViewportWidth > 0 and glViewportHeight > 0)
        {
            const auto gridRect { metrics.gridSize (glViewportWidth, glViewportHeight) };
            numCols = gridRect.getWidth();
            numRows = gridRect.getHeight();
        }

        cellWidth = scale > 0.0f ? static_cast<int> (static_cast<float> (physCellWidth) / scale) : fm.logicalCellW;
        cellHeight = scale > 0.0f ? static_cast<int> (static_cast<float> (physCellHeight) / scale) : fm.logicalCellH;
        baseline = scale > 0.0f ? static_cast<int> (static_cast<float> (physBaseline) / scale) : fm.logicalBaseline;

        if (physCellWidth > 0 and physCellHeight > 0 and onPhysCellDimensionsChanged)
        {
            onPhysCellDimensionsChanged (physCellWidth, physCellHeight);
        }

        glyph.setGeometry (physCellWidth, physCellHeight, physBaseline, baseFontSize);
    }
}

/**
 * @brief Updates the render viewport and recomputes grid dimensions.
 *
 * Stores the new bounds, scales them to physical pixels, and calls `calc()`.
 *
 * @param bounds  New viewport bounds in logical pixel space.
 */
template<typename Context>
void Screen<Context>::setViewport (const juce::Rectangle<int>& bounds) noexcept
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
template<typename Context>
int Screen<Context>::getNumCols() const noexcept
{
    return numCols.value;
}

/// @return `numRows` as computed by `calc()`.
template<typename Context>
int Screen<Context>::getNumRows() const noexcept
{
    return numRows.value;
}

/// @return `cellWidth` in logical pixels.
template<typename Context>
int Screen<Context>::getCellWidth() const noexcept
{
    return cellWidth;
}

/// @return `cellHeight` in logical pixels.
template<typename Context>
int Screen<Context>::getCellHeight() const noexcept
{
    return cellHeight;
}

/// @return `baseFontSize` in points (includes zoom).
template<typename Context>
float Screen<Context>::getBaseFontSize() const noexcept
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
template<typename Context>
juce::Rectangle<int> Screen<Context>::getCellBounds (int col, int row) const noexcept
{
    juce::Rectangle<int> result {};

    if (numCols.value > 0 and numRows.value > 0)
    {
        const int clampedCol { juce::jlimit (0, numCols.value - 1, col) };
        const int clampedRow { juce::jlimit (0, numRows.value - 1, row) };

        // Derive logical position from the physical-pixel grid used by the
        // GL renderer (row * physCellHeight) divided back by display scale.
        // This eliminates per-row rounding drift at fractional scales (e.g.
        // 150%) between the JUCE component overlay and GL-rendered glyphs.
        const int x { viewportX + metrics.cellToPixelX (jam::literals::Cell { clampedCol }) };
        const int y { viewportY + metrics.cellToPixelY (jam::literals::Cell { clampedRow }) };
        const int w { metrics.cellPixelWidth() };
        const int h { metrics.cellPixelHeight() };

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
template<typename Context>
juce::Point<int> Screen<Context>::cellAtPoint (int x, int y) const noexcept
{
    juce::Point<int> result { 0, 0 };

    if (physCellWidth > 0 and physCellHeight > 0)
    {
        const auto cell { metrics.pixelToCell (x - viewportX, y - viewportY) };
        const int col { juce::jlimit (0, numCols.value - 1, cell.x) };
        const int row { juce::jlimit (0, numRows.value - 1, cell.y) };
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
template<typename Context>
void Screen<Context>::setSelection (const ScreenSelection* sel) noexcept
{
    glyph.setSelection (sel);
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
template<typename Context>
void Screen<Context>::setLinkUnderlay (const LinkSpan* spans, int count) noexcept
{
    glyph.setLinkUnderlay (spans, count);
}

/**
 * @brief Returns a read-only reference to the active colour theme.
 *
 * @return Const reference to `resources.terminalColors`.
 */
template<typename Context>
const Theme& Screen<Context>::getTheme() const noexcept
{
    return resources.terminalColors;
}

template<typename Context>
jam::SnapshotBuffer<Render::Snapshot>& Screen<Context>::getSnapshotBuffer() noexcept
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
template<typename Context>
void Screen<Context>::setFontSize (float pointSize) noexcept
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
template<typename Context>
void Screen<Context>::setLineHeight (float multiplier) noexcept
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
template<typename Context>
void Screen<Context>::setCellWidth (float multiplier) noexcept
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
template<typename Context>
void Screen<Context>::setLigatures (bool enabled) noexcept
{
    glyph.setLigatures (enabled);
}

/**
 * @brief Enables or disables synthetic bold (embolden) rendering.
 *
 * Forwards to `gl::GlyphAtlas::setEmbolden()` and clears the atlas if the value
 * changed, so all glyphs are re-rasterised with the new setting.
 *
 * @param enabled  `true` to enable embolden.
 */
template<typename Context>
void Screen<Context>::setEmbolden (bool enabled) noexcept
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
template<typename Context>
void Screen<Context>::setTheme (const Theme& theme) noexcept
{
    resources.terminalColors = theme;
    glyph.setTheme (&resources.terminalColors);
}

/// @brief Sets whether this pane is in selection mode for the current frame.
template<typename Context>
void Screen<Context>::setSelectionActive (bool active) noexcept
{
    selectionModeActive = active;
}

/// @brief Toggles debug rendering mode.
template<typename Context>
void Screen<Context>::toggleDebug() noexcept
{
    debugMode = not debugMode;
}

/// @return `true` if debug rendering mode is active.
template<typename Context>
bool Screen<Context>::isDebugMode() const noexcept
{
    return debugMode;
}

/// @return `true` if a new snapshot is waiting in the mailbox.
template<typename Context>
bool Screen<Context>::hasNewSnapshot() const noexcept
{
    return resources.snapshotBuffer.isReady();
}

/**
 * @brief Resets cache dimension sentinels to force reallocation on the next frame.
 *
 * Delegates to `glyph.resetCache()` so the next `render()` call
 * reallocates per-row glyph and background caches.
 */
template<typename Context>
void Screen<Context>::reset() noexcept
{
    glyph.resetCache();
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
template<typename Context>
void Screen<Context>::render (State& state, Grid& grid) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };

    if (cols > 0 and rows > 0)
    {
        glyph.ensureCache (rows, cols);

        buildSnapshot (state, grid);
    }
}

template class Screen<jam::Glyph::GLContext>;
template class Screen<jam::Glyph::GraphicsContext>;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
