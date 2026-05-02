/**
 * @file Screen.h
 * @brief Render coordinator: cell cache, snapshot builder, and GPU/CPU presenter.
 *
 * `Screen` is the central render coordinator for the terminal emulator.  It
 * sits between the data layer (`Grid`, `State`) and the GPU layer
 * and is responsible for:
 *
 * 1. **Cell processing** — delegates to `Terminal::Renderer::Glyph`, which owns
 *    per-row caches and produces `Render::Glyph` / `Render::Background` quads.
 * 2. **Snapshot builder** — `buildSnapshot()` / `updateSnapshot()` pack the
 *    per-row caches into a contiguous `Render::Snapshot` and publish it via
 *    `jam::SnapshotBuffer`.
 * 3. **GPU/CPU presenter** — reads snapshots from the snapshot buffer and draws
 *    them to the attached component.
 *
 * ## Double-buffered snapshots
 *
 * `jam::SnapshotBuffer<Render::Snapshot>` owns two snapshot instances
 * internally and manages double-buffer rotation.  The MESSAGE THREAD writes
 * to `getWriteBuffer()` and calls `write()`; the GL THREAD calls `read()`
 * to acquire the latest snapshot.  Lock-free via atomic pointer exchange.
 *
 * ## Thread contract
 *
 * | Method                  | Thread         |
 * |-------------------------|----------------|
 * | `render()`              | MESSAGE THREAD |
 * | `setViewport()` etc.    | MESSAGE THREAD |
 * | `GLSnapshotBuffer::write()`  | MESSAGE THREAD |
 * | `GLSnapshotBuffer::read()`   | GL THREAD  |
 * | `renderOpenGL()`             | GL THREAD |
 *
 * @see Grid
 * @see State
 * @see jam::SnapshotBuffer
 * @see Render::Snapshot
 * @see ScreenSelection
 * @see FontCollection
 */

#pragma once
#include <JuceHeader.h>
#include <jam_tui/jam_tui.h>
#include <array>
#include "../data/Cell.h"
#include "../logic/Grid.h"
#include "../data/Palette.h"
#include "ScreenSelection.h"
#include "../selection/LinkSpan.h"
#include "../../lua/Engine.h"
#include "Render.h"
#include "Glyph.h"

namespace Terminal
{ /*____________________________________________________________________________*/

class LinkManager;

/**
 * @class ScreenBase
 * @brief Non-template interface for renderer-agnostic Screen operations.
 *
 * Provides the minimal API needed by UI event handlers (InputHandler,
 * MouseHandler) without exposing the renderer template parameter.
 */
class ScreenBase
{
public:
    virtual ~ScreenBase() = default;

    virtual int getNumRows() const noexcept = 0;
    virtual int getNumCols() const noexcept = 0;
    virtual juce::Point<int> cellAtPoint (int x, int y) const noexcept = 0;

protected:
    ScreenBase() = default;
    ScreenBase (const ScreenBase&) = delete;
    ScreenBase& operator= (const ScreenBase&) = delete;
};

/**
 * @class Screen
 * @brief Render coordinator: owns cell cache, snapshot builder, and GL presenter.
 *
 * `Screen` is the single object responsible for translating the terminal data
 * model (`Grid` + `State`) into GPU-ready `Render::Snapshot` frames.  It is
 * constructed once per terminal view and lives on the **MESSAGE THREAD**.
 *
 * ## Responsibilities
 *
 * - **Viewport management**: `setViewport()` recomputes `numCols` / `numRows`
 *   from the physical cell dimensions returned by `Fonts::calcMetrics()`.
 * - **Cell processing**: delegated to the composed `Terminal::Renderer::Glyph`
 *   member, which owns per-row caches and processes cells each frame.
 * - **Snapshot publication**: `updateSnapshot()` packs the per-row caches into
 *   a `Render::Snapshot` and publishes it via `jam::SnapshotBuffer`.
 * - **Selection overlay**: forwarded to `glyph.setSelection()` for per-cell
 *   highlight quad emission.
 *
 * @par Thread context
 * All public methods except `getSnapshotBuffer()` must be called on the
 * **MESSAGE THREAD**.  The GL thread methods run on the **GL THREAD**
 * and communicate only through the `jam::SnapshotBuffer`.
 *
 * @see Grid
 * @see State
 * @see jam::SnapshotBuffer
 * @see Render::Snapshot
 * @see ScreenSelection
 * @see FontCollection
 * @see Fonts
 * @see gl::GlyphAtlas
 */
template<typename Context>
class Screen : public ScreenBase
{
public:
    // =========================================================================
    // Inner types
    // =========================================================================

    /**
     * @struct Resources
     * @brief Aggregates all shared rendering resources owned by `Screen`.
     *
     * Constructed once in the `Screen` initialiser list.  All members are
     * accessed by both `Screen` (MESSAGE THREAD) and the GL thread methods
     * through carefully controlled interfaces.
     */
    struct Resources
    {
        Resources() = default;

        jam::SnapshotBuffer<Render::Snapshot>
            snapshotBuffer;///< Double-buffered snapshot exchange between MESSAGE THREAD and GL THREAD.
        Theme terminalColors;///< Active colour theme (ANSI palette + default fg/bg/selection).
    };

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Constructs the screen.
     *
     * Initialises `Resources`, calls `calc()` to derive cell dimensions,
     * then calls `reset()`.
     *
     * @param font    Font spec carrying resolved typeface; provides metrics, shaping, and rasterisation.
     * @param packer  Glyph packer; owns the atlas and rasterization.
     * @param atlas   Context-specific atlas store; `jam::gl::GlyphAtlas` for the GL
     *                path, `jam::GraphicsAtlas` for the CPU path.  Passed through
     *                to `uploadStagedBitmaps()` each frame.
     *
     * @note **MESSAGE THREAD**.
     */
    Screen (jam::Font& font,
            jam::Glyph::Packer& packer,
            typename Context::Atlas& atlas);

    /**
     * @brief Destroys the screen and releases all resources.
     *
     * @note **MESSAGE THREAD**.  Call `glContextClosing()` before destroying
     *       to release GL resources on the correct thread.
     */
    ~Screen();

    Screen (const Screen&) = delete;
    Screen& operator= (const Screen&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Updates the render viewport and recomputes grid dimensions.
     *
     * Stores the new bounds, scales them to physical pixels, and calls
     * `calc()` to recompute `numCols` / `numRows`.
     *
     * @param bounds  New viewport bounds in logical pixel space.
     *
     * @note **MESSAGE THREAD**.
     */
    void setViewport (const juce::Rectangle<int>& bounds) noexcept;

    /**
     * @brief Changes the font size and invalidates the glyph atlas.
     *
     * Calls `Fonts::setSize()`, clears the atlas, and calls `calc()` to
     * recompute cell dimensions.  Does nothing if @p pointSize equals the
     * current size.
     *
     * @param pointSize  New font size in points.
     *
     * @note **MESSAGE THREAD**.
     */
    void setFontSize (float pointSize) noexcept;

    /**
     * @brief Sets the line-height multiplier and recomputes cell dimensions.
     *
     * Values greater than 1.0 increase cell height (more line spacing).
     * Values less than 1.0 decrease it (tighter spacing).
     * Extra space is split evenly above and below the text baseline.
     *
     * @param multiplier  Line-height multiplier (clamped to 0.5–3.0 by config).
     * @note **MESSAGE THREAD**.
     */
    void setLineHeight (float multiplier) noexcept;

    /**
     * @brief Sets the cell-width multiplier and recomputes cell dimensions.
     *
     * Values less than 1.0 narrow the cell (more columns fit).
     * Values greater than 1.0 widen it (fewer columns).
     *
     * @param multiplier  Cell-width multiplier (clamped to 0.5–3.0 by config).
     * @note **MESSAGE THREAD**.
     */
    void setCellWidth (float multiplier) noexcept;

    /**
     * @brief Enables or disables HarfBuzz ligature shaping.
     *
     * When enabled, `tryLigature()` is called for ASCII sequences before
     * falling back to single-codepoint shaping.
     *
     * @param enabled  `true` to enable ligatures.
     *
     * @note **MESSAGE THREAD**.
     */
    void setLigatures (bool enabled) noexcept;

    /**
     * @brief Callback invoked by `calc()` whenever the physical cell dimensions change.
     *
     * Set by `Terminal::Display::initialise()` to forward the new cell dimensions
     * to `Parser::setPhysCellDimensions()`.  This allows the READER THREAD Sixel
     * decoder to use the correct cell grid dimensions when writing image cells.
     *
     * Called on the **MESSAGE THREAD** as part of `calc()`.
     *
     * @note Set before the first `render()` call and not changed thereafter.
     */
    std::function<void (int widthPx, int heightPx)> onPhysCellDimensionsChanged;

    /**
     * @brief Enables or disables synthetic bold (embolden) rendering.
     *
     * Forwards to `gl::GlyphAtlas::setEmbolden()` and clears the atlas if the
     * value changed.
     *
     * @param enabled  `true` to enable embolden.
     *
     * @note **MESSAGE THREAD**.
     */
    void setEmbolden (bool enabled) noexcept;

    /**
     * @brief Replaces the active colour theme.
     *
     * @param theme  New theme to apply immediately.
     *
     * @note **MESSAGE THREAD**.
     */
    void setTheme (const Theme& theme) noexcept;

    /**
     * @brief Updates the selection-mode cursor state used for rendering.
     *
     * When @p active is true, `updateSnapshot()` reads selection cursor
     * position from `State::getSelectionCursorRow/Col()` and renders a block
     * cursor at the converted visible-grid position.  Call with `false` to
     * restore normal cursor rendering.
     *
     * @param active  True to enable selection-mode cursor override.
     *
     * @note **MESSAGE THREAD**.
     */
    void setSelectionActive (bool active) noexcept;

    // =========================================================================
    // GL lifecycle (called by Terminal::Display from gl::Component overrides)
    // =========================================================================

    /**
     * @brief Called when the GL context is first created.
     *
     * Called by `Terminal::Display::glContextCreated()`.  Compiles shaders
     * and creates GPU buffers.
     *
     * @note **GL THREAD**.
     */
    void glContextCreated();

    /**
     * @brief Called when the GL context is closing.
     *
     * Called by `Terminal::Display::glContextClosing()`.  Releases all
     * GPU resources.
     *
     * @note **GL THREAD**.
     */
    void glContextClosing();

    /**
     * @brief Renders a frame to the GL context with viewport offset.
     *
     * Called by `Terminal::Display::paintGL()`.  Draws backgrounds,
     * mono glyphs, and emoji glyphs using the physical pixel origin offset
     * so the GL viewport accounts for the tab bar.
     *
     * @param originX     Physical pixel X offset of the component relative to the
     *                    top-level window.
     * @param originY     Physical pixel Y offset of the component relative to the
     *                    top-level window.
     * @param fullHeight  Full window height in physical pixels.
     *
     * @note **GL THREAD**.
     */
    void renderOpenGL (int originX, int originY, int fullHeight);

    /**
     * @brief Returns true if the GL context is ready for rendering.
     *
     * Used for lazy initialisation of tabs created after the shared GL context
     * was already established.  When this returns false, the caller should
     * invoke `glContextCreated()` before rendering.
     *
     * @return `true` if the GL context is ready for rendering.
     *
     * @note **GL THREAD**.
     */
    bool isGLContextReady() const noexcept;

    /**
     * @brief Renders a frame via juce::Graphics (CPU rendering path).
     *
     * Binds the graphics context to the renderer, then executes the same
     * render pipeline as `renderOpenGL()`.  Used when the terminal is
     * driven by `Component::paint()` instead of the GL render loop.
     *
     * @param g           The graphics context from `Component::paint()`.
     * @param originX     Physical pixel X offset.
     * @param originY     Physical pixel Y offset.
     * @param fullHeight  Full window height in physical pixels.
     *
     * @note **MESSAGE THREAD**.
     */
    void renderPaint (juce::Graphics& g, int originX, int originY, int fullHeight);

    // =========================================================================
    // Debug / state queries
    // =========================================================================

    /**
     * @brief Toggles debug rendering mode.
     *
     * @note **MESSAGE THREAD**.
     */
    void toggleDebug() noexcept;

    /**
     * @brief Returns true if debug rendering mode is active.
     *
     * @return `true` if debug mode is on.
     *
     * @note **MESSAGE THREAD**.
     */
    bool isDebugMode() const noexcept;

    /**
     * @brief Returns true if a new snapshot is waiting in the mailbox.
     *
     * @return `true` if the snapshot buffer has a pending snapshot.
     *
     * @note **MESSAGE THREAD** (informational; result may be stale).
     */
    bool hasNewSnapshot() const noexcept;

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Performs one full render cycle: build snapshot from Grid, trigger repaint.
     *
     * Called once per frame from the terminal view on the **MESSAGE THREAD**.
     * Steps:
     * 1. Ensures `glyph` caches are allocated for the current grid dimensions.
     * 2. Calls `buildSnapshot()` to process all rows and pack the snapshot.
     *
     * @param state  Current terminal state (cursor, dimensions, scroll offset).
     * @param grid   Terminal grid providing cell data.
     *
     * @note **MESSAGE THREAD**.
     */
    void render (State& state, Grid& grid) noexcept;

    /**
     * @brief Resets the render cache to force full reallocation on the next frame.
     *
     * Delegates to `glyph.resetCache()` so the next `render()` call
     * reallocates per-row glyph and background caches.
     *
     * @note **MESSAGE THREAD**.
     */
    void reset() noexcept;


    // =========================================================================
    // Grid geometry
    // =========================================================================

    /**
     * @brief Returns the number of visible columns.
     *
     * @return `numCols` as computed by `calc()`.
     *
     * @note **MESSAGE THREAD**.
     */
    int getNumCols() const noexcept override;

    /**
     * @brief Returns the number of visible rows.
     *
     * @return `numRows` as computed by `calc()`.
     *
     * @note **MESSAGE THREAD**.
     */
    int getNumRows() const noexcept override;

    /**
     * @brief Returns the logical cell width in pixels.
     *
     * @return `cellWidth` in logical (CSS/point) pixels.
     *
     * @note **MESSAGE THREAD**.
     */
    int getCellWidth() const noexcept;

    /**
     * @brief Returns the logical cell height in pixels.
     *
     * @return `cellHeight` in logical (CSS/point) pixels.
     *
     * @note **MESSAGE THREAD**.
     */
    int getCellHeight() const noexcept;

    /**
     * @brief Returns the current font size in points (includes zoom).
     *
     * Used by Display::applyZoom() to derive old font metrics for window resize
     * ratio computation without caching a shadow copy of the font size.
     *
     * @return `baseFontSize` as set by the most recent `setFontSize()` call.
     *
     * @note **MESSAGE THREAD**.
     */
    float getBaseFontSize() const noexcept;

    /**
     * @brief Returns the logical pixel bounds of the cell at (@p col, @p row).
     *
     * Clamps @p col and @p row to the valid grid range before computing the
     * rectangle.  Returns an empty rectangle if `numCols` or `numRows` is zero.
     *
     * @param col  Column index (0-based).
     * @param row  Row index (0-based).
     * @return     Bounds in logical pixel space relative to the viewport origin.
     *
     * @note **MESSAGE THREAD**.
     */
    juce::Rectangle<int> getCellBounds (int col, int row) const noexcept;

    /**
     * @brief Returns the grid cell that contains the logical pixel point (@p x, @p y).
     *
     * Converts pixel coordinates to grid coordinates by dividing by cell
     * dimensions and clamping to the valid range.
     *
     * @param x  Logical pixel X coordinate.
     * @param y  Logical pixel Y coordinate.
     * @return   Grid cell (col, row) clamped to [0, numCols-1] × [0, numRows-1].
     *
     * @note **MESSAGE THREAD**.
     */
    juce::Point<int> cellAtPoint (int x, int y) const noexcept override;

    /**
     * @brief Sets the active text selection for overlay rendering.
     *
     * Stores a non-owning pointer to @p sel.  Pass `nullptr` to clear the
     * selection.  When non-null, all rows are marked dirty on the next
     * `render()` call so the overlay is redrawn.
     *
     * @param sel  Pointer to the active selection, or `nullptr`.
     *
     * @note **MESSAGE THREAD**.
     * @see ScreenSelection
     */
    void setSelection (const ScreenSelection* sel) noexcept;

    /** @brief Stores a non-owning reference to the LinkManager for direct hint overlay access. MESSAGE THREAD. */
    void setLinkManager (const LinkManager* lm) noexcept { glyph.setLinkManager (lm); }

    /**
     * @brief Sets the always-on link underlay for click-mode underline rendering.
     *
     * Stores a non-owning pointer to the clickable link span array.  When
     * non-null, `processCellForSnapshot()` emits a thin `Render::Background`
     * underline quad at the bottom of each cell that falls within a span.
     * Pass `nullptr` (with `count` 0) to clear the underlay.
     *
     * Because every row is rebuilt from `Grid` on every frame, the underlines
     * are reflected immediately on the next `render()` call.
     *
     * @param spans  Pointer to the `LinkSpan` array, or `nullptr`.
     * @param count  Number of elements in @p spans; ignored when @p spans is `nullptr`.
     *
     * @note **MESSAGE THREAD**.
     * @see LinkSpan
     */
    void setLinkUnderlay (const LinkSpan* spans, int count) noexcept;

    /**
     * @brief Invokes @p callback for every cell in the visible grid.
     *
     * Iterates rows then columns and calls:
     * @code
     * callback (col, row, getCellBounds (col, row));
     * @endcode
     *
     * @tparam Callback  Callable with signature `void(int col, int row, juce::Rectangle<int>)`.
     * @param  callback  Function to invoke for each cell.
     *
     * @note **MESSAGE THREAD**.
     */
    template<typename Callback>
    void forEachCell (Callback&& callback) const
    {
        for (int r { 0 }; r < numRows.value; ++r)
        {
            for (int c { 0 }; c < numCols.value; ++c)
            {
                callback (c, r, getCellBounds (c, r));
            }
        }
    }

    // =========================================================================
    // Resource accessors
    // =========================================================================

    /**
     * @brief Returns a read-only reference to the active colour theme.
     *
     * @return Const reference to `resources.terminalColors`.
     *
     * @note **MESSAGE THREAD**.
     */
    const Theme& getTheme() const noexcept;

    /**
     * @brief Returns a mutable reference to the double-buffered snapshot exchange.
     *
     * Used by the GL thread methods on the **GL THREAD** to acquire the latest
     * snapshot published by `render()` on the MESSAGE THREAD.
     *
     * @return Reference to `resources.snapshotBuffer`.
     *
     * @note Thread-safe: the snapshot buffer uses atomic pointer exchange.
     * @see jam::SnapshotBuffer
     */
    jam::SnapshotBuffer<Render::Snapshot>& getSnapshotBuffer() noexcept;

private:
    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Recomputes cell dimensions and grid size from the current font metrics.
     *
     * Calls `Fonts::calcMetrics()` and updates `cellWidth`, `cellHeight`,
     * `baseline`, `physCellWidth`, `physCellHeight`, `physBaseline`,
     * `numCols`, and `numRows`.
     *
     * @note **MESSAGE THREAD**.
     */
    void calc() noexcept;

    /**
     * @brief Packs per-row caches into a `Render::Snapshot` and publishes it.
     *
     * Delegates glyph packing to `glyph.packSnapshot()`, sets cursor state,
     * and calls `jam::SnapshotBuffer::write()`.
     *
     * @param state  Current terminal state (cursor position, column count, screen type).
     * @param grid   Terminal grid; passed for context (dimensions, scroll state).
     * @param rows   Number of visible rows.
     *
     * @note **MESSAGE THREAD**.
     */
    void updateSnapshot (const State& state, Grid& grid, int rows) noexcept;

    // =========================================================================
    // Private render pipeline
    // =========================================================================

    /**
     * @brief Rebuilds all rows in the per-row caches from `Grid` and calls `updateSnapshot()`.
     *
     * Iterates all visible rows, reads cells directly from `Grid` (scrollback
     * or active, depending on `state.getScrollOffset()`), resets the row's
     * glyph/bg counts, and calls `processCellForSnapshot()` for every cell.
     * Then calls `updateSnapshot()` to publish the result.
     *
     * @param state  Current terminal state.
     * @param grid   Source grid (read directly; no intermediate cache).
     *
     * @note **MESSAGE THREAD**.
     */
    void buildSnapshot (State& state, Grid& grid) noexcept;

    // =========================================================================
    // GL thread methods
    // =========================================================================

    void drawCursor (const Render::Snapshot& snapshot);

    // =========================================================================
    // Data
    // =========================================================================

    Context textRenderer;///< Owns all GL resources for instanced glyph and background rendering.
    int glViewportX { 0 };
    int glViewportY { 0 };
    int glViewportWidth { 0 };
    int glViewportHeight { 0 };

    int cellWidth { 0 };///< Logical cell width in CSS/point pixels.
    int cellHeight { 0 };///< Logical cell height in CSS/point pixels.
    int baseline { 0 };///< Logical baseline offset from cell top in CSS/point pixels.
    int physCellWidth { 0 };///< Physical (HiDPI-scaled) cell width in pixels.
    int physCellHeight { 0 };///< Physical (HiDPI-scaled) cell height in pixels.
    int physBaseline { 0 };///< Physical (HiDPI-scaled) baseline offset in pixels.
    jam::literals::Cell numCols { 0 };///< Number of visible columns (viewportWidth / cellWidth).
    jam::literals::Cell numRows { 0 };///< Number of visible rows (viewportHeight / cellHeight).
    jam::tui::Metrics metrics {};///< Pixel↔cell conversion helper; rebuilt by calc() whenever physCellWidth/Height change.
    int viewportX { 0 };///< Logical pixel X origin of the viewport.
    int viewportY { 0 };///< Logical pixel Y origin of the viewport.
    int viewportWidth { 0 };///< Logical pixel width of the viewport.
    int viewportHeight { 0 };///< Logical pixel height of the viewport.
    float baseFontSize { 14.0f };///< Current font size in points (includes zoom); updated by setFontSize().
    float lineHeightMultiplier { 1.0f };///< User line-height multiplier from config; 1.0 = no adjustment.
    float cellWidthMultiplier { 1.0f };///< User cell-width multiplier from config; 1.0 = no adjustment.

    jam::Font& font;///< Font spec carrying resolved typeface; provides metrics, shaping, and rasterisation.
    jam::Glyph::Packer& packer;///< Glyph packer; owns the atlas and rasterization.
    typename Context::Atlas& atlasRef;///< Context-specific atlas store; passed through to uploadStagedBitmaps().
    Resources resources;///< All shared rendering resources (atlas, mailbox, theme).
    Terminal::Renderer::Glyph<Context> glyph;///< Text rendering subsystem; owns glyph caches and cell processing.
    bool debugMode { false };///< True if debug rendering overlays are enabled.

    int previousCursorRow { -1 };

    bool selectionModeActive { false };///< Per-frame flag: true when this pane is in selection mode.

    uint64_t frameDirtyBits[4] {};///< Dirty row bitmask for current frame, set in buildSnapshot.
    int frameScrollDelta { 0 };///< Scroll delta for current frame, set in buildSnapshot.
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
