/**
 * @file Screen.cpp
 * @brief Screen lifecycle, configuration, geometry, and grid-population implementation.
 *
 * This translation unit implements:
 * - `Screen` constructor / destructor and `calc()`.
 * - Viewport, font, theme, and selection configuration.
 * - Grid geometry helpers (`getCellBounds`, `cellAtPoint`).
 * - `render()` — the top-level per-frame entry point.
 * - `populateFromGrid()` — copies dirty rows from `Grid` into the cell cache.
 * - `allocateRowCache()` and `applyScrollOptimization()`.
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
 * then calls `reset()` to zero-initialise the cell cache.
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
 * The next `render()` call will detect the change via `hadSelection` and
 * mark all rows dirty.
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
 * Stores a non-owning pointer to the active `LinkSpan` array.  The next
 * `render()` call detects the transition via `hadHintOverlay` and marks all
 * rows dirty so the overlay or its absence is reflected in the next frame.
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
 * Stores a non-owning pointer to the clickable link span array.  The next
 * `render()` call detects the transition via `hadLinkUnderlay` and marks all
 * rows dirty so the underlines or their absence is reflected in the next frame.
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
 * @brief Resets the cell cache to default cells and clears row counts.
 *
 * Fills `hotCells` with default-constructed `Cell` values and `coldGraphemes`
 * with default `Grapheme` values.  Resets `cacheRows`, `cacheCols`, and
 * `bgCacheCols` to zero so the next `render()` call reallocates the caches.
 */
void Screen::reset() noexcept
{
    const Cell defaultCell {};
    std::fill (hotCells.get(), hotCells.get() + hotCellCount, defaultCell);
    std::fill (coldGraphemes.get(), coldGraphemes.get() + hotCellCount, Grapheme {});
    cacheRows = 0;
    cacheCols = 0;
    bgCacheCols = 0;
}

/**
 * @brief Returns true if @p row is marked dirty in the 256-bit bitmask.
 *
 * The bitmask is stored as four `uint64_t` words.  Row @p row maps to bit
 * `(row & 63)` of word `(row >> 6)`.
 *
 * @param dirty  Four-word dirty bitmask (256 bits, one per row).
 * @param row    Row index to test (0–255).
 * @return       `true` if bit @p row is set.
 */
bool Screen::isRowDirty (const uint64_t dirty[4], int row) noexcept
{
    return (dirty[row >> 6] & (uint64_t { 1 } << (row & 63))) != 0;
}

/**
 * @brief Allocates per-row render caches for @p rows rows and @p cols columns.
 *
 * Each row gets `cols * 2` glyph slots (to accommodate wide characters and
 * ligatures) and `cols * 2` background slots (for bg + selection overlay).
 * All arrays are zero-initialised.
 *
 * @param rows  Number of visible rows.
 * @param cols  Number of visible columns.
 */
void Screen::allocateRowCache (int rows, int cols) noexcept
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

    cacheRows = rows;
    cacheCols = cols;
    bgCacheCols = cols * 3;
}

/**
 * @brief Shifts per-row caches upward by @p scroll rows.
 *
 * Uses `memmove` to shift `hotCells`, `coldGraphemes`, `cachedMono`,
 * `cachedEmoji`, `cachedBg`, and the count arrays.  After shifting, adjusts
 * the Y screen positions of all cached glyph and background instances in the
 * retained rows by `-scroll * physCellHeight`.  Clears the newly exposed rows
 * at the bottom.
 *
 * @param rows    Number of visible rows.
 * @param cols    Number of visible columns.
 * @param scroll  Number of rows scrolled upward (positive).
 * @param dirty   Dirty bitmask (currently unused; reserved for future use).
 *
 * @note The `dirty` parameter is intentionally unused; the caller marks the
 *       newly exposed rows dirty after this function returns.
 */
void Screen::applyScrollOptimization (int rows, int cols, int scroll, uint64_t dirty[4]) noexcept
{
    const size_t rowBytes { static_cast<size_t> (cols) * sizeof (Cell) };
    const size_t graphemeRowBytes { static_cast<size_t> (cols) * sizeof (Grapheme) };
    const size_t shiftRows { static_cast<size_t> (rows - scroll) };

    std::memmove (hotCells.get(),
                  hotCells.get() + static_cast<size_t> (scroll) * static_cast<size_t> (cols),
                  shiftRows * rowBytes);
    std::memmove (coldGraphemes.get(),
                  coldGraphemes.get() + static_cast<size_t> (scroll) * static_cast<size_t> (cols),
                  shiftRows * graphemeRowBytes);

    const Cell defaultCell {};
    for (int r { rows - scroll }; r < rows; ++r)
    {
        const size_t rowBase { static_cast<size_t> (r) * static_cast<size_t> (cols) };
        std::fill (hotCells.get() + rowBase, hotCells.get() + rowBase + static_cast<size_t> (cols), defaultCell);
        std::fill (coldGraphemes.get() + rowBase, coldGraphemes.get() + rowBase + static_cast<size_t> (cols), Grapheme {});
    }

    const int maxGlyphs { cacheCols * 2 };
    const size_t glyphRowBytes { static_cast<size_t> (maxGlyphs) * sizeof (Render::Glyph) };
    const size_t bgRowBytes { static_cast<size_t> (bgCacheCols) * sizeof (Render::Background) };

    std::memmove (cachedMono.get(), cachedMono.get() + static_cast<size_t> (scroll) * static_cast<size_t> (maxGlyphs), shiftRows * glyphRowBytes);
    std::memmove (cachedEmoji.get(), cachedEmoji.get() + static_cast<size_t> (scroll) * static_cast<size_t> (maxGlyphs), shiftRows * glyphRowBytes);
    std::memmove (cachedBg.get(), cachedBg.get() + static_cast<size_t> (scroll) * static_cast<size_t> (bgCacheCols), shiftRows * bgRowBytes);
    std::memmove (monoCount.get(), monoCount.get() + scroll, shiftRows * sizeof (int));
    std::memmove (emojiCount.get(), emojiCount.get() + scroll, shiftRows * sizeof (int));
    std::memmove (bgCount.get(), bgCount.get() + scroll, shiftRows * sizeof (int));

    const float yShift { static_cast<float> (scroll * physCellHeight) };
    for (int r { 0 }; r < rows - scroll; ++r)
    {
        const int base { r * maxGlyphs };
        for (int i { 0 }; i < monoCount[r]; ++i)
            cachedMono[base + i].screenPosition.y -= yShift;
        for (int i { 0 }; i < emojiCount[r]; ++i)
            cachedEmoji[base + i].screenPosition.y -= yShift;
        const int bgBase { r * bgCacheCols };
        for (int i { 0 }; i < bgCount[r]; ++i)
        {
            auto& bounds { cachedBg[bgBase + i].screenBounds };
            bounds = bounds.withY (bounds.getY() - yShift);
        }
    }

    for (int r { rows - scroll }; r < rows; ++r)
    {
        monoCount[r] = 0;
        emojiCount[r] = 0;
        bgCount[r] = 0;
    }

    (void) dirty;
}



/**
 * @brief Performs one full render cycle: update cache, build snapshot, trigger repaint.
 *
 * Called once per frame from the terminal view on the **MESSAGE THREAD**.
 *
 * @par Steps
 * 1. Allocates / resizes `hotCells` and `coldGraphemes` if the grid size changed.
 * 2. Consumes dirty rows and scroll delta from `Grid`.
 * 3. If the cache dimensions changed, reallocates caches and marks all rows dirty.
 * 4. If a partial scroll occurred, applies the scroll optimisation and marks
 *    the newly exposed rows dirty.
 * 5. If a full scroll occurred (≥ rows), marks all rows dirty.
 * 6. If a selection is active or was active last frame, marks all rows dirty.
 * 7. If the view is scrolled or was scrolled last frame, marks all rows dirty.
 * 8. Calls `populateFromGrid()` to copy dirty rows from `Grid`.
 * 9. Calls `buildSnapshot()` to rebuild dirty rows and publish the snapshot.
 *
 * @param state  Current terminal state (cursor, dimensions, scroll offset).
 * @param grid   Terminal grid providing cell data and dirty tracking.
 */
void Screen::render (const State& state, Grid& grid) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };

    if (cols > 0 and rows > 0)
    {
        const size_t needed { static_cast<size_t> (cols) * static_cast<size_t> (rows) };

        if (needed != hotCellCount)
        {
            hotCells.allocate (needed, true);
            coldGraphemes.allocate (needed, true);
            hotCellCount = needed;
            cacheRows = 0;
            cacheCols = 0;
        }

        uint64_t dirty[4] { 0, 0, 0, 0 };
        grid.consumeDirtyRows (dirty);
        const int scroll { grid.consumeScrollDelta() };

        if (cacheRows != rows or cacheCols != cols)
        {
            allocateRowCache (rows, cols);
            std::fill (std::begin (dirty), std::end (dirty), ~uint64_t { 0 });
        }
        else if (scroll > 0 and scroll < rows)
        {
            applyScrollOptimization (rows, cols, scroll, dirty);

            for (int r { rows - scroll }; r < rows; ++r)
            {
                dirty[r >> 6] |= (uint64_t { 1 } << (r & 63));
            }
        }
        else if (scroll >= rows)
        {
            std::fill (std::begin (dirty), std::end (dirty), ~uint64_t { 0 });
        }

        const bool hasSelection { selection != nullptr };

        if (hasSelection or hadSelection)
        {
            std::fill (std::begin (dirty), std::end (dirty), ~uint64_t { 0 });
        }

        hadSelection = hasSelection;

        const bool isScrolled { state.getScrollOffset() > 0 };

        if (isScrolled or wasScrolled)
        {
            std::fill (std::begin (dirty), std::end (dirty), ~uint64_t { 0 });
        }

        wasScrolled = isScrolled;

        const bool hasHintOverlay { hintOverlay != nullptr and hintOverlayCount > 0 };

        if (hasHintOverlay or hadHintOverlay)
        {
            std::fill (std::begin (dirty), std::end (dirty), ~uint64_t { 0 });
        }

        hadHintOverlay = hasHintOverlay;

        const bool hasLinkUnderlay { linkUnderlay != nullptr and linkUnderlayCount > 0 };

        if (hasLinkUnderlay or hadLinkUnderlay)
        {
            std::fill (std::begin (dirty), std::end (dirty), ~uint64_t { 0 });
        }

        hadLinkUnderlay = hasLinkUnderlay;

        populateFromGrid (state, grid, dirty);
        buildSnapshot (state, dirty);
    }
}

/**
 * @brief Copies dirty rows from `Grid` into the `hotCells` / `coldGraphemes` cache.
 *
 * For each row marked dirty in @p dirty, selects the appropriate row pointer
 * from `Grid` (scrollback or active, depending on `state.getScrollOffset()`)
 * and copies cells and graphemes into the flat cache arrays.  Rows not marked
 * dirty are left unchanged.
 *
 * @param state  Current terminal state (provides `getCols()`, `getVisibleRows()`,
 *               `getScrollOffset()`).
 * @param grid   Source grid (provides `scrollbackRow()`, `activeVisibleRow()`,
 *               and their grapheme equivalents).
 * @param dirty  256-bit dirty bitmask; only rows with their bit set are copied.
 */
void Screen::populateFromGrid (const State& state, const Grid& grid, const uint64_t dirty[4]) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };
    const int offset { state.getScrollOffset() };

    for (int row { 0 }; row < rows; ++row)
    {
        if (isRowDirty (dirty, row))
        {
            const Cell* rowCells { offset > 0
                ? grid.scrollbackRow (row, offset)
                : grid.activeVisibleRow (row) };
            const Grapheme* rowGraphemes { offset > 0
                ? grid.scrollbackGraphemeRow (row, offset)
                : grid.activeVisibleGraphemeRow (row) };

            if (rowCells != nullptr)
            {
                const size_t rowBase { static_cast<size_t> (row) * static_cast<size_t> (cols) };

                for (int col { 0 }; col < cols; ++col)
                {
                    const size_t idx { rowBase + static_cast<size_t> (col) };
                    hotCells[idx] = rowCells[col];

                    if (rowCells[col].hasGrapheme() and rowGraphemes != nullptr)
                    {
                        coldGraphemes[idx] = rowGraphemes[col];
                    }
                }
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
