/**
 * @file Screen.h
 * @brief Render coordinator: cell cache, snapshot builder, and OpenGL presenter.
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
 *    `jreng::GLSnapshotBuffer`.
 * 3. **OpenGL presenter** — reads snapshots from the snapshot buffer and draws
 *    them to the attached component.
 *
 * ## Double-buffered snapshots
 *
 * `jreng::GLSnapshotBuffer<Render::Snapshot>` owns two snapshot instances
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
 * @see jreng::GLSnapshotBuffer
 * @see Render::Snapshot
 * @see ScreenSelection
 * @see FontCollection
 */

#pragma once
#include <JuceHeader.h>
#include <array>
#include "../data/Cell.h"
#include "../logic/Grid.h"
#include "GlyphAtlas.h"
#include "Fonts.h"
#include "../data/Palette.h"
#include "ScreenSelection.h"
#include "../selection/LinkSpan.h"
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct BlockGeometry
 * @brief Normalised geometry descriptor for a Unicode block-element character.
 *
 * Each entry in `BLOCK_TABLE` describes one block-element codepoint
 * (U+2580–U+2593) as a rectangle in normalised cell space [0, 1] × [0, 1],
 * plus an optional alpha override.  The renderer scales these values by the
 * physical cell dimensions to produce pixel-space quads.
 *
 * @note `static_assert` below verifies trivial copyability for safe GPU upload.
 *
 * @see BLOCK_TABLE
 * @see Screen::buildBlockRect()
 */
struct BlockGeometry
{
    float x;     ///< Normalised X offset from the left edge of the cell [0, 1].
    float y;     ///< Normalised Y offset from the top edge of the cell [0, 1].
    float w;     ///< Normalised width as a fraction of the cell width [0, 1].
    float h;     ///< Normalised height as a fraction of the cell height [0, 1].
    float alpha; ///< Alpha override: negative means use the foreground colour's alpha; otherwise [0, 1].
};

static_assert (std::is_trivially_copyable_v<BlockGeometry>, "BlockGeometry must be trivially copyable");

/// @brief First Unicode codepoint in the block-element range (U+2580 UPPER HALF BLOCK).
static constexpr uint32_t BLOCK_FIRST { 0x2580 };

/// @brief Last Unicode codepoint in the block-element range (U+2593 DARK SHADE).
static constexpr uint32_t BLOCK_LAST  { 0x2593 };

/**
 * @brief Lookup table mapping block-element codepoints to normalised geometry.
 *
 * Indexed by `codepoint - BLOCK_FIRST`.  Contains 20 entries covering
 * U+2580–U+2593.  Each entry is a `BlockGeometry` describing the filled
 * rectangle within the cell in normalised [0, 1] space.
 *
 * @see BlockGeometry
 * @see Screen::buildBlockRect()
 */
static constexpr std::array<BlockGeometry, 20> BLOCK_TABLE
{{
    { 0.0f, 0.0f,   1.0f,   0.5f,   -1.0f },
    { 0.0f, 0.875f, 1.0f,   0.125f, -1.0f },
    { 0.0f, 0.75f,  1.0f,   0.25f,  -1.0f },
    { 0.0f, 0.625f, 1.0f,   0.375f, -1.0f },
    { 0.0f, 0.5f,   1.0f,   0.5f,   -1.0f },
    { 0.0f, 0.375f, 1.0f,   0.625f, -1.0f },
    { 0.0f, 0.25f,  1.0f,   0.75f,  -1.0f },
    { 0.0f, 0.125f, 1.0f,   0.875f, -1.0f },
    { 0.0f, 0.0f,   1.0f,   1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.875f, 1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.75f,  1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.625f, 1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.5f,   1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.375f, 1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.25f,  1.0f,   -1.0f },
    { 0.0f, 0.0f,   0.125f, 1.0f,   -1.0f },
    { 0.5f, 0.0f,   0.5f,   1.0f,   -1.0f },
    { 0.0f, 0.0f,   1.0f,   1.0f,   0.25f },
    { 0.0f, 0.0f,   1.0f,   1.0f,   0.50f },
    { 0.0f, 0.0f,   1.0f,   1.0f,   0.75f },
}};

/**
 * @brief Returns true if @p codepoint is a Unicode block-element character.
 *
 * Tests whether @p codepoint falls in the range [BLOCK_FIRST, BLOCK_LAST]
 * (U+2580–U+2593).  Block characters are rendered as GPU quads rather than
 * rasterised glyphs.
 *
 * @param codepoint  Unicode scalar value to test.
 * @return           `true` if the codepoint is a block element.
 *
 * @see BLOCK_TABLE
 * @see Screen::buildBlockRect()
 */
inline bool isBlockChar (uint32_t codepoint) noexcept
{
    return codepoint >= BLOCK_FIRST and codepoint <= BLOCK_LAST;
}

/// @brief Alias for the active colour theme type from Config.
using Theme = Config::Theme;

/**
 * @struct Render
 * @brief Namespace-struct grouping all GPU-facing render types.
 *
 * `Render` is a plain struct used as a namespace to group the types that
 * cross the MESSAGE THREAD / GL THREAD boundary:
 *
 * - `Render::Background` — a coloured rectangle (cell background or block char).
 * - `Render::Glyph`      — a positioned, textured glyph instance.
 * - `Render::Snapshot`   — a complete frame: arrays of glyphs + backgrounds.
 * - `jreng::GLSnapshotBuffer` — double-buffered lock-free snapshot exchange.
 *
 * @see Screen
 */
struct Render
{

/**
 * @struct Background
 * @brief A coloured rectangle to be drawn as a cell background or block element.
 *
 * Trivially copyable for direct GPU upload.  Coordinates are in physical
 * (HiDPI-scaled) pixel space.
 *
 * @note `static_assert` below verifies trivial copyability.
 *
 * @see Render::Snapshot::backgrounds
 */
struct Background
{
    juce::Rectangle<float> screenBounds;  ///< Rectangle in physical pixel space (origin at top-left of viewport).
    float backgroundColorR;               ///< Red channel of the background colour [0, 1].
    float backgroundColorG;               ///< Green channel of the background colour [0, 1].
    float backgroundColorB;               ///< Blue channel of the background colour [0, 1].
    float backgroundColorA;               ///< Alpha channel of the background colour [0, 1].
};

/**
 * @struct Glyph
 * @brief A positioned, textured glyph instance for GPU instanced drawing.
 *
 * Describes one glyph to be drawn: its screen position, size, atlas texture
 * coordinates, and foreground colour.  Arrays of `Glyph` are uploaded to the
 * instance VBO and drawn with a single `glDrawArraysInstanced` call.
 *
 * @par Thread contract
 * - **READER THREAD** (MESSAGE THREAD): builds and writes `Glyph` instances
 *   into `Render::Snapshot`.
 * - **GL THREAD**: reads `Glyph` instances from the acquired snapshot
 *   (immutable after publish).
 *
 * @note `static_assert` below verifies trivial copyability for GPU upload.
 *
 * @see Render::Snapshot::mono
 * @see Render::Snapshot::emoji
 */
struct Glyph
{
    juce::Point<float>      screenPosition;      ///< Top-left corner of the glyph bitmap in physical pixel space.
    juce::Point<float>      glyphSize;           ///< Width and height of the glyph bitmap in physical pixels.
    juce::Rectangle<float>  textureCoordinates;  ///< UV rectangle within the glyph atlas texture [0, 1].
    float                   foregroundColorR;    ///< Red channel of the glyph foreground colour [0, 1].
    float                   foregroundColorG;    ///< Green channel of the glyph foreground colour [0, 1].
    float                   foregroundColorB;    ///< Blue channel of the glyph foreground colour [0, 1].
    float                   foregroundColorA;    ///< Alpha channel of the glyph foreground colour [0, 1].
};

/**
 * @struct Snapshot
 * @brief A complete rendered frame: glyph instances + background quads.
 *
 * Owns three `HeapBlock` arrays — `mono`, `emoji`, and `backgrounds` — that
 * hold all draw calls for one frame.  Capacity is grown on demand via
 * `ensureCapacity()` and never shrunk, so allocations stabilise after a few
 * frames.
 *
 * Two `Snapshot` instances are owned internally by `jreng::GLSnapshotBuffer`
 * and recycled via atomic pointer exchange to avoid per-frame allocation.
 *
 * @see jreng::GLSnapshotBuffer
 * @see Screen::updateSnapshot()
 */
struct Snapshot
{
    juce::HeapBlock<Glyph>      mono;              ///< Monochrome glyph instances (regular + bold + italic).
    juce::HeapBlock<Glyph>      emoji;             ///< Colour emoji glyph instances.
    juce::HeapBlock<Background> backgrounds;       ///< Background colour quads (non-default bg + selection overlay).
    int                         monoCount      { 0 }; ///< Number of valid entries in @p mono.
    int                         emojiCount     { 0 }; ///< Number of valid entries in @p emoji.
    int                         backgroundCount{ 0 }; ///< Number of valid entries in @p backgrounds.
    int                         monoCapacity   { 0 }; ///< Allocated capacity of @p mono in elements.
    int                         emojiCapacity  { 0 }; ///< Allocated capacity of @p emoji in elements.
    int                         backgroundCapacity { 0 }; ///< Allocated capacity of @p backgrounds in elements.

    juce::Point<int>            cursorPosition;           ///< Cursor position in grid coordinates (col, row).
    bool                        cursorVisible  { false }; ///< True if DECTCEM cursor mode is on.
    int                         cursorShape    { 0 };     ///< DECSCUSR Ps value (0 = user glyph, 1–6 = geometric).
    float                       cursorColorR   { -1.0f }; ///< OSC 12 red override (0–255), or -1 if no override.
    float                       cursorColorG   { -1.0f }; ///< OSC 12 green override (0–255), or -1 if no override.
    float                       cursorColorB   { -1.0f }; ///< OSC 12 blue override (0–255), or -1 if no override.
    int                         scrollOffset   { 0 };     ///< Lines scrolled back (0 = live view; cursor hidden when > 0).
    bool                        cursorBlinkOn  { true };  ///< Current blink phase (true = visible half of cycle).
    bool                        cursorFocused  { false }; ///< True if the terminal component has keyboard focus.
    Glyph                       cursorGlyph;                    ///< Pre-built glyph instance for user cursor (shape 0).
    bool                        hasCursorGlyph    { false };   ///< True when cursorGlyph is valid (shape 0 or cursor.force).
    bool                        cursorGlyphIsEmoji { false };  ///< True when cursorGlyph lives in the emoji (RGBA) atlas.
    float cursorDrawColorR { 1.0f };  ///< Final resolved cursor colour red [0, 1] (theme or OSC 12).
    float cursorDrawColorG { 1.0f };  ///< Final resolved cursor colour green [0, 1] (theme or OSC 12).
    float cursorDrawColorB { 1.0f };  ///< Final resolved cursor colour blue [0, 1] (theme or OSC 12).

    int                         gridWidth  { 0 }; ///< Grid width in columns at the time of snapshot.
    int                         gridHeight { 0 }; ///< Grid height in rows at the time of snapshot.

    /**
     * @brief Grows the backing arrays to at least the requested capacities.
     *
     * Each array is reallocated only if the requested count exceeds the
     * current capacity.  Existing data is not preserved across reallocation.
     *
     * @param monoNeeded   Minimum number of `Glyph` slots required for mono.
     * @param emojiNeeded  Minimum number of `Glyph` slots required for emoji.
     * @param bgNeeded     Minimum number of `Background` slots required.
     *
     * @note Called on the **MESSAGE THREAD** from `updateSnapshot()` before
     *       writing glyph data.
     */
    void ensureCapacity (int monoNeeded, int emojiNeeded, int bgNeeded) noexcept
    {
        if (monoNeeded > monoCapacity)
        {
            mono.allocate (static_cast<size_t> (monoNeeded), false);
            monoCapacity = monoNeeded;
        }
        if (emojiNeeded > emojiCapacity)
        {
            emoji.allocate (static_cast<size_t> (emojiNeeded), false);
            emojiCapacity = emojiNeeded;
        }
        if (bgNeeded > backgroundCapacity)
        {
            backgrounds.allocate (static_cast<size_t> (bgNeeded), false);
            backgroundCapacity = bgNeeded;
        }
    }

    void resize() noexcept {}
};

/**______________________________END OF NAMESPACE______________________________*/
};  // struct Render

static_assert (std::is_trivially_copyable_v<Render::Background>, "Render::Background must be trivially copyable");
static_assert (std::is_trivially_copyable_v<Render::Glyph>, "Render::Glyph must be trivially copyable for GPU upload");

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal

namespace Terminal
{ /*____________________________________________________________________________*/

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
 *   a `Render::Snapshot` and publishes it via `jreng::GLSnapshotBuffer`.
 * - **Selection overlay**: a `ScreenSelection*` pointer is checked per cell in
 *   `processCellForSnapshot()` to emit selection highlight quads.
 *
 * @par Thread context
 * All public methods except `getSnapshotBuffer()` must be called on the
 * **MESSAGE THREAD**.  The GL thread methods run on the **GL THREAD**
 * and communicate only through the `jreng::GLSnapshotBuffer`.
 *
 * @see Grid
 * @see State
 * @see jreng::GLSnapshotBuffer
 * @see Render::Snapshot
 * @see ScreenSelection
 * @see FontCollection
 * @see Fonts
 * @see GlyphAtlas
 */
class Screen
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
        Resources()
        {
        }

        GlyphAtlas       glyphAtlas;       ///< Glyph atlas: rasterises and caches glyphs on the GPU.
        jreng::GLSnapshotBuffer<Render::Snapshot> snapshotBuffer; ///< Double-buffered snapshot exchange between MESSAGE THREAD and GL THREAD.
        Theme            terminalColors;   ///< Active colour theme (ANSI palette + default fg/bg/selection).
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
     * @note **MESSAGE THREAD**.
     */
    Screen();

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
     * @brief Enables or disables synthetic bold (embolden) rendering.
     *
     * Forwards to `GlyphAtlas::setEmbolden()` and clears the atlas if the
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
    // GL lifecycle (called by Terminal::Component from GLComponent overrides)
    // =========================================================================

    /**
     * @brief Called when the GL context is first created.
     *
     * Called by `Terminal::Component::glContextCreated()`.  Compiles shaders
     * and creates GPU buffers.
     *
     * @note **GL THREAD**.
     */
    void glContextCreated();

    /**
     * @brief Called when the GL context is closing.
     *
     * Called by `Terminal::Component::glContextClosing()`.  Releases all
     * GPU resources.
     *
     * @note **GL THREAD**.
     */
    void glContextClosing();

    /**
     * @brief Renders a frame to the GL context with viewport offset.
     *
     * Called by `Terminal::Component::renderGL()`.  Draws backgrounds,
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
    void render (const State& state, Grid& grid) noexcept;

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
    int getNumCols() const noexcept;

    /**
     * @brief Returns the number of visible rows.
     *
     * @return `numRows` as computed by `calc()`.
     *
     * @note **MESSAGE THREAD**.
     */
    int getNumRows() const noexcept;

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
    juce::Point<int> cellAtPoint (int x, int y) const noexcept;

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

    /**
     * @brief Sets the hint label overlay for Open File mode rendering.
     *
     * Stores a non-owning pointer to the active `LinkSpan` array.  When
     * non-null, `processCellForSnapshot()` overrides cell content at hint
     * label positions with the hint character and `Theme::hintLabelBg` /
     * `Theme::hintLabelFg` colours.  Pass `nullptr` (with `count` 0) to clear
     * the overlay.
     *
     * Because every row is rebuilt from `Grid` on every frame, the overlay is
     * reflected immediately on the next `render()` call.
     *
     * @param spans  Pointer to the `LinkSpan` array, or `nullptr`.
     * @param count  Number of elements in @p spans; ignored when @p spans is `nullptr`.
     *
     * @note **MESSAGE THREAD**.
     * @see LinkSpan
     */
    void setHintOverlay (const LinkSpan* spans, int count) noexcept;

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
     * @brief Returns a mutable reference to the glyph atlas.
     *
     * @return Reference to `resources.glyphAtlas`.
     *
     * @note **MESSAGE THREAD**.
     */
    GlyphAtlas& getGlyphAtlas() noexcept;

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
     * @see jreng::GLSnapshotBuffer
     */
    jreng::GLSnapshotBuffer<Render::Snapshot>& getSnapshotBuffer() noexcept;

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
     * `jreng::GLSnapshotBuffer::write()`.
     *
     * @param state      Current terminal state (cursor position, screen type).
     * @param rows       Number of visible rows.
     * @param maxGlyphs  Maximum glyph slots per row (`cacheRows` cols × 2).
     *
     * @note **MESSAGE THREAD**.
     */
    void updateSnapshot (const State& state, int rows, int maxGlyphs) noexcept;

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
    void buildSnapshot (const State& state, Grid& grid) noexcept;

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
    void processCellForSnapshot (const Cell& cell, const Cell* rowCells,
                                 const Grapheme* rowGraphemes, int col, int row) noexcept;

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
                            int col, int row,
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
    int tryLigature (const Cell* rowCells, int col, int row, Fonts::Style style, void* fontHandle,
                     const juce::Colour& foreground) noexcept;

    /**
     * @brief Builds a `Render::Background` quad for a block-element character.
     *
     * Looks up the `BlockGeometry` for @p codepoint in `BLOCK_TABLE` and
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
     * @see BLOCK_TABLE
     * @see isBlockChar()
     */
    Render::Background buildBlockRect (uint32_t codepoint, int col, int row, const juce::Colour& foreground) const noexcept;

    /**
     * @brief Selects the `Fonts::Style` variant for a cell based on its SGR attributes.
     *
     * @param cell  Cell whose `isBold()` and `isItalic()` flags are tested.
     * @return      `boldItalic`, `bold`, `italic`, or `regular`.
     */
    static Fonts::Style selectFontStyle (const Cell& cell) noexcept;

    // =========================================================================
    // GL thread methods
    // =========================================================================

    void compileShaders();
    void createBuffers();
    static GLuint createAtlasTexture (int width, int height, GLenum internalFormat, GLenum format) noexcept;
    void uploadStagedBitmaps();
    void drawInstances (const Render::Glyph* data, int count, bool isEmoji);
    void drawBackgrounds (const Render::Background* data, int count);
    void drawCursor (const Render::Snapshot& snapshot);

    // =========================================================================
    // Data
    // =========================================================================

    std::unique_ptr<juce::OpenGLShaderProgram> monoShader;
    std::unique_ptr<juce::OpenGLShaderProgram> emojiShader;
    std::unique_ptr<juce::OpenGLShaderProgram> backgroundShader;
    GLuint monoAtlasTexture  { 0 };
    GLuint emojiAtlasTexture { 0 };
    GLuint vao         { 0 };
    GLuint quadVBO     { 0 };
    GLuint instanceVBO { 0 };
    int glViewportX      { 0 };
    int glViewportY      { 0 };
    int glViewportWidth  { 0 };
    int glViewportHeight { 0 };

    int cellWidth    { 0 };  ///< Logical cell width in CSS/point pixels.
    int cellHeight   { 0 };  ///< Logical cell height in CSS/point pixels.
    int baseline     { 0 };  ///< Logical baseline offset from cell top in CSS/point pixels.
    int physCellWidth  { 0 }; ///< Physical (HiDPI-scaled) cell width in pixels.
    int physCellHeight { 0 }; ///< Physical (HiDPI-scaled) cell height in pixels.
    int physBaseline   { 0 }; ///< Physical (HiDPI-scaled) baseline offset in pixels.
    int numCols      { 0 };  ///< Number of visible columns (viewportWidth / cellWidth).
    int numRows      { 0 };  ///< Number of visible rows (viewportHeight / cellHeight).
    int viewportX    { 0 };  ///< Logical pixel X origin of the viewport.
    int viewportY    { 0 };  ///< Logical pixel Y origin of the viewport.
    int viewportWidth  { 0 }; ///< Logical pixel width of the viewport.
    int viewportHeight { 0 }; ///< Logical pixel height of the viewport.
    float baseFontSize { 14.0f }; ///< Current font size in points; updated by setFontSize().

    Resources resources;           ///< All shared rendering resources (fonts, atlas, mailbox, theme).
    bool ligatureEnabled { false }; ///< True if HarfBuzz ligature shaping is active.
    bool debugMode       { false }; ///< True if debug rendering overlays are enabled.
    int  ligatureSkip    { 0 };     ///< Number of cells to skip after a ligature was emitted.

    // Per-row render cache (rebuilt every frame for every row)
    juce::HeapBlock<Render::Glyph>      cachedMono;   ///< Mono glyph instances; layout: [row][0 … maxGlyphs-1].
    juce::HeapBlock<Render::Glyph>      cachedEmoji;  ///< Emoji glyph instances; layout: [row][0 … maxGlyphs-1].
    juce::HeapBlock<Render::Background> cachedBg;     ///< Background quads; layout: [row][0 … bgCacheCols-1].
    juce::HeapBlock<int>                monoCount;    ///< Number of valid mono glyphs per row.
    juce::HeapBlock<int>                emojiCount;   ///< Number of valid emoji glyphs per row.
    juce::HeapBlock<int>                bgCount;      ///< Number of valid background quads per row.
    int cacheRows    { 0 }; ///< Number of rows the per-row caches were allocated for.
    int cacheCols    { 0 }; ///< Number of columns the per-row caches were allocated for.
    int bgCacheCols  { 0 }; ///< Background slots per row (= cacheCols * 3, for bg + selection + underlay).

    const ScreenSelection* selection   { nullptr }; ///< Non-owning pointer to the active selection; nullptr if none.

    const LinkSpan* hintOverlay      { nullptr }; ///< Non-owning pointer to the active hint label spans; nullptr if none.
    int             hintOverlayCount { 0 };       ///< Number of valid elements in @p hintOverlay.

    const LinkSpan* linkUnderlay      { nullptr }; ///< Non-owning pointer to always-on click-mode link spans for underline rendering; nullptr if none.
    int             linkUnderlayCount { 0 };       ///< Number of valid elements in @p linkUnderlay.

    bool selectionModeActive   { false }; ///< True when vim-style selection mode is active (hides terminal cursor).
    int  selectionCursorRow    { 0 };     ///< Selection cursor row in visible-grid coordinates (0 = top visible row).
    int  selectionCursorCol    { 0 };     ///< Selection cursor column (0-based).
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
