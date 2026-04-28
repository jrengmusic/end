/**
 * @file Screen.h
 * @brief Render coordinator: cell cache, snapshot builder, and GPU/CPU presenter.
 *
 * `Screen` is the central render coordinator for the terminal emulator.  It
 * sits between the data layer (`Grid`, `State`) and the GPU layer
 * and is responsible for:
 *
 * 1. **Per-row render cache** — `cachedMono`, `cachedEmoji`, `cachedBg`
 *    arrays that store pre-built `Render::Glyph` and `Render::Background`
 *    instances for each row.  All rows are rebuilt on every frame by reading
 *    directly from `Grid`.
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
#include <array>
#include "../data/Cell.h"
#include "../logic/Grid.h"
#include "../data/Palette.h"
#include "ScreenSelection.h"
#include "../selection/LinkSpan.h"
#include "../../lua/Engine.h"
#include "ImageAtlas.h"

namespace Terminal
{ /*____________________________________________________________________________*/

class LinkManager;

/**
 * @struct BlockGeometry
 * @brief Normalised geometry descriptor for a Unicode block-element character.
 *
 * Each entry in `blockTable` describes one block-element codepoint
 * (U+2580–U+2593) as a rectangle in normalised cell space [0, 1] × [0, 1],
 * plus an optional alpha override.  The renderer scales these values by the
 * physical cell dimensions to produce pixel-space quads.
 *
 * @note `static_assert` below verifies trivial copyability for safe GPU upload.
 *
 * @see blockTable
 * @see Screen::buildBlockRect()
 */
struct BlockGeometry
{
    float x;///< Normalised X offset from the left edge of the cell [0, 1].
    float y;///< Normalised Y offset from the top edge of the cell [0, 1].
    float w;///< Normalised width as a fraction of the cell width [0, 1].
    float h;///< Normalised height as a fraction of the cell height [0, 1].
    float alpha;///< Alpha override: negative means use the foreground colour's alpha; otherwise [0, 1].
};

static_assert (std::is_trivially_copyable_v<BlockGeometry>, "BlockGeometry must be trivially copyable");

/// @brief First Unicode codepoint in the box-drawing range (U+2500 BOX DRAWINGS LIGHT HORIZONTAL).
static constexpr uint32_t boxDrawingFirst { 0x2500 };

/// @brief Last Unicode codepoint in the box-drawing range (U+259F QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT).
static constexpr uint32_t boxDrawingLast { 0x259F };

/// @brief First Unicode codepoint in the block-element range (U+2580 UPPER HALF BLOCK).
static constexpr uint32_t blockFirst { 0x2580 };

/// @brief Last Unicode codepoint in the block-element range (U+2593 DARK SHADE).
static constexpr uint32_t blockLast { 0x2593 };

/**
 * @brief Lookup table mapping block-element codepoints to normalised geometry.
 *
 * Indexed by `codepoint - blockFirst`.  Contains 20 entries covering
 * U+2580–U+2593.  Each entry is a `BlockGeometry` describing the filled
 * rectangle within the cell in normalised [0, 1] space.
 *
 * @see BlockGeometry
 * @see Screen::buildBlockRect()
 */
static constexpr std::array<BlockGeometry, 20> blockTable {
    {
     { 0.0f, 0.0f, 1.0f, 0.5f, -1.0f },   { 0.0f, 0.875f, 1.0f, 0.125f, -1.0f },
     { 0.0f, 0.75f, 1.0f, 0.25f, -1.0f }, { 0.0f, 0.625f, 1.0f, 0.375f, -1.0f },
     { 0.0f, 0.5f, 1.0f, 0.5f, -1.0f },   { 0.0f, 0.375f, 1.0f, 0.625f, -1.0f },
     { 0.0f, 0.25f, 1.0f, 0.75f, -1.0f }, { 0.0f, 0.125f, 1.0f, 0.875f, -1.0f },
     { 0.0f, 0.0f, 1.0f, 1.0f, -1.0f },   { 0.0f, 0.0f, 0.875f, 1.0f, -1.0f },
     { 0.0f, 0.0f, 0.75f, 1.0f, -1.0f },  { 0.0f, 0.0f, 0.625f, 1.0f, -1.0f },
     { 0.0f, 0.0f, 0.5f, 1.0f, -1.0f },   { 0.0f, 0.0f, 0.375f, 1.0f, -1.0f },
     { 0.0f, 0.0f, 0.25f, 1.0f, -1.0f },  { 0.0f, 0.0f, 0.125f, 1.0f, -1.0f },
     { 0.5f, 0.0f, 0.5f, 1.0f, -1.0f },   { 0.0f, 0.0f, 1.0f, 1.0f, 0.25f },
     { 0.0f, 0.0f, 1.0f, 1.0f, 0.50f },   { 0.0f, 0.0f, 1.0f, 1.0f, 0.75f },
     }
};

/**
 * @brief Returns true if @p codepoint is a Unicode block-element character.
 *
 * Tests whether @p codepoint falls in the range [blockFirst, blockLast]
 * (U+2580–U+2593).  Block characters are rendered as GPU quads rather than
 * rasterised glyphs.
 *
 * @param codepoint  Unicode scalar value to test.
 * @return           `true` if the codepoint is a block element.
 *
 * @see blockTable
 * @see Screen::buildBlockRect()
 */
inline bool isBlockChar (uint32_t codepoint) noexcept { return codepoint >= blockFirst and codepoint <= blockLast; }

/// @brief Alias for the active colour theme type from lua::Engine.
using Theme = lua::Engine::Theme;

/**
 * @struct Render
 * @brief Namespace-struct grouping all GPU-facing render types.
 *
 * `Render` is a plain struct used as a namespace to group the types that
 * cross the MESSAGE THREAD / GL THREAD boundary:
 *
 * - `Render::Background` — alias for `jam::Glyph::Render::Background`.
 * - `Render::Glyph`      — alias for `jam::Glyph::Render::Quad` (kept for
 *                          minimal consumer churn; the canonical module name
 *                          is `Quad` to avoid collision with `jam::Typeface::Glyph`).
 * - `Render::Snapshot`   — terminal-specific snapshot: inherits the generic
 *                          `jam::Glyph::Render::SnapshotBase` arrays and
 *                          adds cursor state fields.
 * - `jam::SnapshotBuffer` — double-buffered lock-free snapshot exchange.
 *
 * @see Screen
 * @see jam::Glyph::Render
 */
struct Render
{
    /// @brief Alias for the module-level coloured rectangle type.
    using Background = jam::Glyph::Render::Background;

    /// @brief Alias for the module-level positioned quad type.
    /// @note The canonical module name is `jam::Glyph::Render::Quad`; this
    ///       alias preserves existing consumer code at `Terminal::Render::Glyph`.
    using Glyph = jam::Glyph::Render::Quad;

    /**
 * @struct ImageQuad
 * @brief Render instance for one image cell: screen bounds + atlas UV.
 *
 * Trivially copyable.  Packed into `Snapshot::images` by `updateSnapshot()`
 * and drawn by `drawImages()` as textured quads over the image atlas.
 */
    struct ImageQuad
    {
        juce::Rectangle<float> screenBounds;///< Destination rectangle in viewport pixels.
        juce::Rectangle<float> uvRect;///< Normalised source rectangle in the image atlas.
    };

    static_assert (std::is_trivially_copyable_v<ImageQuad>, "ImageQuad must be trivially copyable");

    /**
 * @struct Snapshot
 * @brief A complete rendered frame: glyph instances + background quads + cursor state.
 *
 * Inherits the generic `jam::Glyph::Render::SnapshotBase` which owns the
 * three `HeapBlock` arrays (`mono`, `emoji`, `backgrounds`) and
 * `ensureCapacity()`.  Terminal-specific cursor state fields are added here.
 *
 * Two `Snapshot` instances are owned internally by `jam::SnapshotBuffer`
 * and recycled via atomic pointer exchange to avoid per-frame allocation.
 *
 * @see jam::Glyph::Render::SnapshotBase
 * @see jam::SnapshotBuffer
 * @see Screen::updateSnapshot()
 */
    struct Snapshot : jam::Glyph::Render::SnapshotBase
    {
        juce::HeapBlock<ImageQuad> images;///< Packed image quad instances for this frame.
        int imageCount { 0 };///< Number of valid entries in `images`.
        int imageCapacity { 0 };///< Allocated capacity of `images`.

        juce::Point<int> cursorPosition;///< Cursor position in grid coordinates (col, row).
        bool cursorVisible { false };///< True if DECTCEM cursor mode is on.
        int cursorShape { 0 };///< DECSCUSR Ps value (0 = user glyph, 1–6 = geometric).
        float cursorColorR { -1.0f };///< OSC 12 red override (0–255), or -1 if no override.
        float cursorColorG { -1.0f };///< OSC 12 green override (0–255), or -1 if no override.
        float cursorColorB { -1.0f };///< OSC 12 blue override (0–255), or -1 if no override.
        int scrollOffset { 0 };///< Lines scrolled back (0 = live view; cursor hidden when > 0).
        bool cursorBlinkOn { true };///< Current blink phase (true = visible half of cycle).
        bool cursorFocused { false };///< True if the terminal component has keyboard focus.
        Glyph cursorGlyph;///< Pre-built glyph instance for user cursor (shape 0).
        bool hasCursorGlyph { false };///< True when cursorGlyph is valid (shape 0 or cursor.force).
        bool cursorGlyphIsEmoji { false };///< True when cursorGlyph lives in the emoji (RGBA) atlas.
        float cursorDrawColorR { 1.0f };///< Final resolved cursor colour red [0, 1] (theme or OSC 12).
        float cursorDrawColorG { 1.0f };///< Final resolved cursor colour green [0, 1] (theme or OSC 12).
        float cursorDrawColorB { 1.0f };///< Final resolved cursor colour blue [0, 1] (theme or OSC 12).
        int gridWidth { 0 };///< Grid width in columns at the time of snapshot.
        int gridHeight { 0 };///< Grid height in rows at the time of snapshot.
        uint64_t dirtyRows[4] {};///< Bitmask of rows that changed this frame (bit per row, max 256 rows).
        int scrollDelta { 0 };///< Lines scrolled up since last frame (positive = scrolled up).
        int physCellHeight { 0 };///< Physical (HiDPI-scaled) cell height in pixels.
    };

    /**______________________________END OF NAMESPACE______________________________*/
};// struct Render

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
 * - **Per-row render cache**: `cachedMono`, `cachedEmoji`, `cachedBg` store
 *   pre-built glyph/background instances per row.  `buildSnapshot()` rebuilds
 *   all rows every frame by reading directly from `Grid`.
 * - **Snapshot publication**: `updateSnapshot()` packs the per-row caches into
 *   a `Render::Snapshot` and publishes it via `jam::SnapshotBuffer`.
 * - **Selection overlay**: a `ScreenSelection*` pointer is checked per cell in
 *   `processCellForSnapshot()` to emit selection highlight quads.
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
template<typename Renderer>
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
     * @param font        Font spec carrying resolved typeface; provides metrics, shaping, and rasterisation.
     * @param packer      Glyph packer; owns the atlas and rasterization.
     * @param atlas       Renderer-specific atlas store; `jam::gl::GlyphAtlas` for the GL
     *                    path, `jam::GraphicsAtlas` for the CPU path.  Passed through
     *                    to `uploadStagedBitmaps()` each frame.
     * @param imageAtlas  Inline image atlas; `publishStagedUploads()` is called each frame
     *                    on the MESSAGE THREAD; `consumeStagedUploads()` on the GL THREAD.
     *
     * @note **MESSAGE THREAD**.
     */
    Screen (jam::Font& font,
            jam::Glyph::Packer& packer,
            typename Renderer::Atlas& atlas,
            Terminal::ImageAtlas& imageAtlas);

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
     * When @p active is true, the normal terminal cursor is suppressed and
     * replaced with a block cursor drawn at (@p col, @p row) in visible-grid
     * coordinates using the theme's `selectionCursorColour`.  Call with
     * @p active `false` to restore normal cursor rendering.
     *
     * Must be called before `render()` on the **MESSAGE THREAD** so that the
     * values are visible to `updateSnapshot()` in the same frame.
     *
     * @param active  True to enable selection-mode cursor override.
     * @param row     Visible row index (0 = topmost visible row).
     * @param col     Column index (0-based).
     *
     * @note **MESSAGE THREAD**.
     */
    void setSelectionCursor (bool active, int row, int col) noexcept;

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
     * 1. Reallocates per-row render caches if the grid dimensions changed.
     * 2. Calls `buildSnapshot()` to process all rows and pack the snapshot.
     *
     * @param state  Current terminal state (cursor, dimensions, scroll offset).
     * @param grid   Terminal grid providing cell data.
     *
     * @note **MESSAGE THREAD**.
     */
    void render (State& state, Grid& grid) noexcept;

    /**
     * @brief Resets the render cache dimensions to force reallocation on the next frame.
     *
     * Resets `cacheRows` and `bgCacheCols` to zero so the next `render()` call
     * reallocates the per-row glyph and background caches.
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
    void setLinkManager (const LinkManager* lm) noexcept { linkManager = lm; }

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
        for (int r { 0 }; r < numRows; ++r)
        {
            for (int c { 0 }; c < numCols; ++c)
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
     * @brief Allocates per-row render caches for @p rows rows and @p cols columns.
     *
     * Allocates `cachedMono`, `cachedEmoji`, `cachedBg`, `monoCount`,
     * `emojiCount`, and `bgCount`.  Each row gets `cols * 2` glyph slots and
     * `cols * 3` background slots.
     *
     * @param rows  Number of visible rows.
     * @param cols  Number of visible columns.
     *
     * @note **MESSAGE THREAD**.
     */
    void allocateRenderCache (int rows, int cols) noexcept;

    /**
     * @brief Packs per-row caches into a `Render::Snapshot` and publishes it.
     *
     * Totals the per-row counts, ensures snapshot capacity, copies glyph and
     * background data via `memcpy`, sets cursor state, and calls
     * `jam::SnapshotBuffer::write()`.
     *
     * @param state      Current terminal state (cursor position, screen type).
     * @param grid       Terminal grid; read-only access for image rendering.
     * @param rows       Number of visible rows.
     * @param maxGlyphs  Maximum glyph slots per row (`cacheRows` cols × 2).
     *
     * @note **MESSAGE THREAD**.
     */
    void updateSnapshot (const State& state, Grid& grid, int rows, int maxGlyphs) noexcept;

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

    /**
     * @brief Processes one cell and appends its contributions to the row caches.
     *
     * Resolves foreground and background colours, emits a background quad if
     * the background is non-default, dispatches to block-char / box-drawing /
     * glyph rendering, and emits a selection overlay quad if the cell is
     * selected.
     *
     * @param cell         The cell to render.
     * @param rowCells     Pointer to the start of the current row in `Grid` (for lookahead).
     * @param rowGraphemes Grapheme sidecar row pointer for the current row (may be `nullptr`).
     * @param col          Column index of the cell.
     * @param row          Row index of the cell.
     *
     * @note **MESSAGE THREAD**.
     * @see buildCellInstance()
     * @see buildBlockRect()
     */
    void processCellForSnapshot (const Cell& cell,
                                 const Cell* rowCells,
                                 const Grapheme* rowGraphemes,
                                 int col,
                                 int row,
                                 Grid& grid) noexcept;

    /**
     * @brief Shapes and rasterises one cell's glyph(s) into the row cache.
     *
     * Handles box-drawing (procedural rasterisation), FontCollection fallback
     * lookup, ligature shaping, and standard HarfBuzz shaping.  Appends
     * `Render::Glyph` instances to `cachedMono` or `cachedEmoji`.
     *
     * @param cell       The cell to render.
     * @param grapheme   Optional grapheme cluster for multi-codepoint cells; may be `nullptr`.
     * @param rowCells   Pointer to the start of the current row in `Grid` (for lookahead).
     * @param col        Column index.
     * @param row        Row index.
     * @param foreground Resolved foreground colour.
     *
     * @note **MESSAGE THREAD**.
     * @see tryLigature()
     * @see FontCollection::resolve()
     */
    void buildCellInstance (const Cell& cell,
                            const Grapheme* grapheme,
                            const Cell* rowCells,
                            int col,
                            int row,
                            const juce::Colour& foreground) noexcept;

    /**
     * @brief Attempts to shape a 2- or 3-character ligature starting at @p col.
     *
     * Tries lengths 3 then 2.  If HarfBuzz produces fewer glyphs than input
     * codepoints, the sequence is a ligature: emits the shaped glyphs and
     * returns the number of subsequent cells to skip.
     *
     * @param rowCells    Pointer to the start of the current row in `Grid`.
     * @param col         Starting column.
     * @param row         Row index.
     * @param style       Font style for shaping.
     * @param fontHandle  Platform font handle.
     * @param foreground  Resolved foreground colour.
     * @return            Number of cells to skip after this one (0 if no ligature found).
     *
     * @note **MESSAGE THREAD**.
     */
    int tryLigature (const Cell* rowCells,
                     int col,
                     int row,
                     jam::Typeface::Style style,
                     const juce::Colour& foreground) noexcept;

    /**
     * @brief Builds a `Render::Background` quad for a block-element character.
     *
     * Looks up the `BlockGeometry` for @p codepoint in `blockTable` and
     * scales it by the physical cell dimensions.  Uses @p foreground as the
     * fill colour (with the geometry's alpha override if non-negative).
     *
     * @param codepoint   Block-element codepoint (U+2580–U+2593).
     * @param col         Column index.
     * @param row         Row index.
     * @param foreground  Resolved foreground colour used as the block fill.
     * @return            A fully populated `Render::Background` quad.
     *
     * @note **MESSAGE THREAD**.
     * @see blockTable
     * @see isBlockChar()
     */
    Render::Background
    buildBlockRect (uint32_t codepoint, int col, int row, const juce::Colour& foreground) const noexcept;

    /**
     * @brief Selects the `Fonts::Style` variant for a cell based on its SGR attributes.
     *
     * @param cell  Cell whose `isBold()` and `isItalic()` flags are tested.
     * @return      `boldItalic`, `bold`, `italic`, or `regular`.
     */
    static jam::Typeface::Style selectFontStyle (const Cell& cell) noexcept;

    /**
     * @brief Returns true if row @p r should be included in the current snapshot.
     *
     * GPU path (`GLContext`): always returns `true` — all rows are packed
     * because the framebuffer is cleared each frame.
     *
     * CPU path (`GraphicsContext`): returns `true` only if the row's bit
     * is set in `frameDirtyBits`.  Non-dirty rows retain correct content on the
     * persistent render target; re-drawing them causes alpha accumulation.
     *
     * @param r  Row index (0-based).
     * @return   `true` if the row must be packed into the snapshot.
     *
     * @note **MESSAGE THREAD**.
     */
    inline bool isRowIncludedInSnapshot (int r) const noexcept
    {
        bool result { true };

        if constexpr (std::is_same_v<Renderer, jam::Glyph::GraphicsContext>)
        {
            const int word { r >> 6 };
            const uint64_t bit { static_cast<uint64_t> (1) << (r & 63) };
            result = (frameDirtyBits[word] & bit) != 0;
        }

        return result;
    }

    // =========================================================================
    // GL thread methods
    // =========================================================================

    void drawCursor (const Render::Snapshot& snapshot);
    void drawImages (const Render::Snapshot& snapshot);

    // =========================================================================
    // Data
    // =========================================================================

    Renderer textRenderer;///< Owns all GL resources for instanced glyph and background rendering.
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
    int numCols { 0 };///< Number of visible columns (viewportWidth / cellWidth).
    int numRows { 0 };///< Number of visible rows (viewportHeight / cellHeight).
    int viewportX { 0 };///< Logical pixel X origin of the viewport.
    int viewportY { 0 };///< Logical pixel Y origin of the viewport.
    int viewportWidth { 0 };///< Logical pixel width of the viewport.
    int viewportHeight { 0 };///< Logical pixel height of the viewport.
    float baseFontSize { 14.0f };///< Current font size in points (includes zoom); updated by setFontSize().
    float lineHeightMultiplier { 1.0f };///< User line-height multiplier from config; 1.0 = no adjustment.
    float cellWidthMultiplier { 1.0f };///< User cell-width multiplier from config; 1.0 = no adjustment.

    jam::Font& font;///< Font spec carrying resolved typeface; provides metrics, shaping, and rasterisation.
    jam::Glyph::Packer& packer;///< Glyph packer; owns the atlas and rasterization.
    typename Renderer::Atlas& atlasRef;///< Renderer-specific atlas store; passed through to uploadStagedBitmaps().
    Terminal::ImageAtlas& imageAtlas;///< Inline image atlas; staged uploads published each frame on MESSAGE THREAD.
    Resources resources;///< All shared rendering resources (atlas, mailbox, theme).
    bool ligatureEnabled { false };///< True if HarfBuzz ligature shaping is active.
    bool debugMode { false };///< True if debug rendering overlays are enabled.
    int ligatureSkip { 0 };///< Number of cells to skip after a ligature was emitted.

    // Per-row render cache (rebuilt every frame for every row)
    juce::HeapBlock<Render::Glyph> cachedMono;///< Mono glyph instances; layout: [row][0 … maxGlyphs-1].
    juce::HeapBlock<Render::Glyph> cachedEmoji;///< Emoji glyph instances; layout: [row][0 … maxGlyphs-1].
    juce::HeapBlock<Render::Background> cachedBg;///< Background quads; layout: [row][0 … bgCacheCols-1].
    juce::HeapBlock<int> monoCount;///< Number of valid mono glyphs per row.
    juce::HeapBlock<int> emojiCount;///< Number of valid emoji glyphs per row.
    juce::HeapBlock<int> bgCount;///< Number of valid background quads per row.
    juce::HeapBlock<Render::ImageQuad> cachedImages;///< Image quads; layout: [row * cacheCols + imageIndex].
    juce::HeapBlock<int> imageCacheCount;///< Number of valid image quads per row.
    juce::HeapBlock<Terminal::Cell>
        previousCells;///< Previous-frame cells for memcmp skip; layout: [row * cacheCols + col].
    int previousCursorRow { -1 };
    int cacheRows { 0 };///< Number of rows the per-row caches were allocated for.
    int cacheCols { 0 };///< Number of columns the per-row caches were allocated for.
    int bgCacheCols { 0 };///< Background slots per row (= cacheCols * 3, for bg + selection + underlay).
    int maxGlyphsPerRow { 0 };///< Maximum glyph slots per row (= cacheCols * 2); set in allocateRenderCache().

    const ScreenSelection* selection { nullptr };///< Non-owning pointer to the active selection; nullptr if none.

    const LinkManager* linkManager { nullptr };///< Non-owning pointer to LinkManager for direct hint overlay access; nullptr if none.

    const LinkSpan* hintOverlay { nullptr };///< Pulled from LinkManager each frame by buildSnapshot(); nullptr if none.
    int hintOverlayCount { 0 };///< Number of valid elements in @p hintOverlay; pulled from LinkManager each frame.

    const LinkSpan* linkUnderlay {
        nullptr
    };///< Non-owning pointer to always-on click-mode link spans for underline rendering; nullptr if none.
    int linkUnderlayCount { 0 };///< Number of valid elements in @p linkUnderlay.

    bool selectionModeActive { false };///< True when vim-style selection mode is active (hides terminal cursor).
    int selectionCursorRow { 0 };///< Selection cursor row in visible-grid coordinates (0 = top visible row).
    int selectionCursorCol { 0 };///< Selection cursor column (0-based).

    uint64_t frameDirtyBits[4] {};///< Dirty row bitmask for current frame, set in buildSnapshot.
    int frameScrollDelta { 0 };///< Scroll delta for current frame, set in buildSnapshot.
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
