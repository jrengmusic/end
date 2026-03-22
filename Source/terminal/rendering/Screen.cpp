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
#include "../../config/Config.h"


namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

/**
 * @brief Constructs the screen.
 *
 * Initialises `Resources`, calls `calc()` to derive cell dimensions,
 * then calls `reset()` to zero the cache dimension sentinels.
 */
Screen::Screen()
    : resources()
    , baseFontSize (Config::getContext()->getFloat (Config::Key::fontSize))
{
    calc();
    reset();
}

Screen::~Screen() = default;

/**
 * @brief Recomputes cell dimensions and grid size from the current font metrics.
 *
 * Calls `Fonts::calcMetrics()` with `baseFontSize`.  If the metrics are valid,
 * updates all cell dimension fields and recomputes `numCols` / `numRows` from
 * the current viewport size.  Does nothing if the viewport has zero area.
 */
void Screen::calc() noexcept
{
    const Fonts::Metrics fm { Fonts::getContext()->calcMetrics (baseFontSize) };

    if (fm.isValid())
    {
        physCellWidth  = fm.physCellW;
        physCellHeight = fm.physCellH;
        physBaseline   = fm.physBaseline;

        if (physCellWidth > 0 and physCellHeight > 0 and glViewportWidth > 0 and glViewportHeight > 0)
        {
            numCols = glViewportWidth / physCellWidth;
            numRows = glViewportHeight / physCellHeight;
        }

        const float scale { Fonts::getDisplayScale() };

        cellWidth  = scale > 0.0f ? static_cast<int> (static_cast<float> (physCellWidth)  / scale) : fm.logicalCellW;
        cellHeight = scale > 0.0f ? static_cast<int> (static_cast<float> (physCellHeight) / scale) : fm.logicalCellH;
        baseline   = scale > 0.0f ? static_cast<int> (static_cast<float> (physBaseline)   / scale) : fm.logicalBaseline;
    }
}

/**
 * @brief Updates the render viewport and recomputes grid dimensions.
 *
 * Stores the new bounds, scales them to physical pixels, and calls `calc()`.
 *
 * @param bounds  New viewport bounds in logical pixel space.
 */
void Screen::setViewport (const juce::Rectangle<int>& bounds) noexcept
{
    viewportX = bounds.getX();
    viewportY = bounds.getY();
    viewportWidth = bounds.getWidth();
    viewportHeight = bounds.getHeight();

    const float scale { Fonts::getDisplayScale() };
    glViewportX      = static_cast<int> (static_cast<float> (bounds.getX())      * scale);
    glViewportY      = static_cast<int> (static_cast<float> (bounds.getY())      * scale);
    glViewportWidth  = static_cast<int> (static_cast<float> (bounds.getWidth())  * scale);
    glViewportHeight = static_cast<int> (static_cast<float> (bounds.getHeight()) * scale);

    calc();
}

/// @return `numCols` as computed by `calc()`.
int Screen::getNumCols() const noexcept { return numCols; }

/// @return `numRows` as computed by `calc()`.
int Screen::getNumRows() const noexcept { return numRows; }

/// @return `cellWidth` in logical pixels.
int Screen::getCellWidth() const noexcept { return cellWidth; }

/// @return `cellHeight` in logical pixels.
int Screen::getCellHeight() const noexcept { return cellHeight; }

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
juce::Rectangle<int> Screen::getCellBounds (int col, int row) const noexcept
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
        const float scale { Fonts::getDisplayScale() };
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
juce::Point<int> Screen::cellAtPoint (int x, int y) const noexcept
{
    juce::Point<int> result { 0, 0 };

    if (physCellWidth > 0 and physCellHeight > 0)
    {
        const float scale { Fonts::getDisplayScale() };
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
void Screen::setSelection (const ScreenSelection* sel) noexcept
{
    selection = sel;
}

/**
 * @brief Sets the hint label overlay for Open File mode rendering.
 *
 * Stores a non-owning pointer to the active `LinkSpan` array.  Because every
 * row is rebuilt from `Grid` on every frame, the overlay is reflected
 * immediately on the next `render()` call.
 *
 * @param spans  Pointer to the `LinkSpan` array, or `nullptr` to clear.
 * @param count  Number of elements in @p spans.
 */
void Screen::setHintOverlay (const LinkSpan* spans, int count) noexcept
{
    hintOverlay      = spans;
    hintOverlayCount = count;
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
void Screen::setLinkUnderlay (const LinkSpan* spans, int count) noexcept
{
    linkUnderlay      = spans;
    linkUnderlayCount = count;
}

/**
 * @brief Returns a mutable reference to the glyph atlas.
 *
 * @return Reference to `resources.glyphAtlas`.
 */
GlyphAtlas& Screen::getGlyphAtlas() noexcept
{
    return resources.glyphAtlas;
}

/**
 * @brief Returns a read-only reference to the active colour theme.
 *
 * @return Const reference to `resources.terminalColors`.
 */
const Theme& Screen::getTheme() const noexcept
{
    return resources.terminalColors;
}

jreng::GLSnapshotBuffer<Render::Snapshot>& Screen::getSnapshotBuffer() noexcept
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
void Screen::setFontSize (float pointSize) noexcept
{
    baseFontSize = pointSize;
    Fonts::getContext()->setSize (pointSize);
    resources.glyphAtlas.clear();
    calc();
}

/**
 * @brief Enables or disables HarfBuzz ligature shaping.
 *
 * @param enabled  `true` to enable ligatures.
 */
void Screen::setLigatures (bool enabled) noexcept
{
    ligatureEnabled = enabled;
}

/**
 * @brief Enables or disables synthetic bold (embolden) rendering.
 *
 * Forwards to `GlyphAtlas::setEmbolden()` and clears the atlas if the value
 * changed, so all glyphs are re-rasterised with the new setting.
 *
 * @param enabled  `true` to enable embolden.
 */
void Screen::setEmbolden (bool enabled) noexcept
{
    if (enabled != resources.glyphAtlas.getEmbolden())
    {
        resources.glyphAtlas.setEmbolden (enabled);
        resources.glyphAtlas.clear();
    }
}

/**
 * @brief Replaces the active colour theme.
 *
 * @param theme  New theme to apply immediately.
 */
void Screen::setTheme (const Theme& theme) noexcept
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
void Screen::setSelectionCursor (bool active, int row, int col) noexcept
{
    selectionModeActive = active;
    selectionCursorRow  = row;
    selectionCursorCol  = col;
}

/// @brief Toggles debug rendering mode.
void Screen::toggleDebug() noexcept { debugMode = not debugMode; }

/// @return `true` if debug rendering mode is active.
bool Screen::isDebugMode() const noexcept { return debugMode; }

/// @return `true` if a new snapshot is waiting in the mailbox.
bool Screen::hasNewSnapshot() const noexcept { return resources.snapshotBuffer.isReady(); }

/**
 * @brief Resets cache dimension sentinels to force reallocation on the next frame.
 *
 * Zeroes `cacheRows`, `cacheCols`, and `bgCacheCols` so the next `render()` call
 * reallocates the per-row glyph and background caches.
 */
void Screen::reset() noexcept
{
    cacheRows   = 0;
    cacheCols   = 0;
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
void Screen::allocateRenderCache (int rows, int cols) noexcept
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

    cacheRows   = rows;
    cacheCols   = cols;
    bgCacheCols = cols * 3;
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
void Screen::render (const State& state, Grid& grid) noexcept
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
