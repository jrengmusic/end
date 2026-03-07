/**
 * @file Screen.h
 * @brief Render coordinator: cell cache, snapshot builder, and OpenGL presenter.
 *
 * `Screen` is the central render coordinator for the terminal emulator.  It
 * sits between the data layer (`Grid`, `State`) and the GPU layer
 * (`Render::OpenGL`) and is responsible for:
 *
 * 1. **Cell cache** — a flat `hotCells` / `coldGraphemes` mirror of the
 *    visible grid, updated incrementally via dirty-row tracking.
 * 2. **Per-row render cache** — `cachedMono`, `cachedEmoji`, `cachedBg`
 *    arrays that store pre-built `Render::Glyph` and `Render::Background`
 *    instances for each row.  Only dirty rows are rebuilt on each frame.
 * 3. **Snapshot builder** — `buildSnapshot()` / `updateSnapshot()` pack the
 *    per-row caches into a contiguous `Render::Snapshot` and publish it to
 *    the `Render::Mailbox`.
 * 4. **OpenGL presenter** — owns a `Render::OpenGL` instance that reads
 *    snapshots from the mailbox and draws them to the attached component.
 *
 * ## Double-buffered snapshots
 *
 * Two `Render::Snapshot` objects (`snapshotA`, `snapshotB`) are owned by
 * `Screen` and recycled via the `Render::Mailbox`.  The MESSAGE THREAD writes
 * to `writeSnapshot` and publishes it; the GL THREAD acquires the latest
 * snapshot via `Render::Mailbox::acquire()`.  The mailbox returns the
 * previously published pointer for reuse, keeping allocations stable.
 *
 * ## Scroll optimisation
 *
 * When `Grid::consumeScrollDelta()` returns a positive value, `Screen`
 * shifts the per-row caches upward by the scroll amount using `memmove`,
 * adjusts Y positions of cached glyph instances, and marks only the newly
 * exposed rows as dirty.  This avoids reshaping the entire grid on every
 * scroll event.
 *
 * ## Thread contract
 *
 * | Method                  | Thread         |
 * |-------------------------|----------------|
 * | `render()`              | MESSAGE THREAD |
 * | `setViewport()` etc.    | MESSAGE THREAD |
 * | `Render::Mailbox::publish()` | MESSAGE THREAD |
 * | `Render::Mailbox::acquire()` | GL THREAD  |
 * | `Render::OpenGL::renderOpenGL()` | GL THREAD |
 *
 * @see Grid
 * @see State
 * @see Render::Mailbox
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
#include "GLShaderCompiler.h"
#include "GLVertexLayout.h"
#include "ScreenSelection.h"
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

// BlockGeometry stays outside Render ...

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
 * - `Render::Mailbox`    — lock-free single-slot exchange for snapshots.
 * - `Render::OpenGL`     — the JUCE OpenGL renderer that consumes snapshots.
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
// READER THREAD - Builds snapshot
// GL THREAD - Reads snapshot (immutable)
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
 * Two `Snapshot` instances (`snapshotA`, `snapshotB`) are owned by `Screen`
 * and recycled through the `Mailbox` to avoid per-frame allocation.
 *
 * @see Render::Mailbox
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

    juce::Point<int>            cursorPosition;    ///< Cursor position in grid coordinates (col, row).
    bool                        cursorVisible { false }; ///< True if the cursor should be drawn this frame.

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
};

/**
 * @class Mailbox
 * @brief Lock-free single-slot snapshot exchange between MESSAGE THREAD and GL THREAD.
 *
 * The mailbox holds at most one `Snapshot*` at a time.  The MESSAGE THREAD
 * calls `publish()` to deposit a new snapshot and receives the old pointer
 * back for reuse.  The GL THREAD calls `acquire()` to take the latest
 * snapshot (replacing the slot with `nullptr`).
 *
 * This design guarantees:
 * - The GL THREAD always sees the most recent snapshot (no queue lag).
 * - No snapshot is ever freed — `Screen` owns both `snapshotA` and
 *   `snapshotB`; the mailbox only exchanges pointers.
 * - No locks are needed; the exchange is a single `std::atomic::exchange`.
 *
 * @par Thread contract
 * - `publish()` — **MESSAGE THREAD** only.
 * - `acquire()` — **GL THREAD** only.
 * - `hasSnapshot()` — any thread (informational only).
 *
 * @code
 * // MESSAGE THREAD
 * Snapshot* old = mailbox.publish (newSnapshot);
 * // old is now safe to reuse for the next frame
 *
 * // GL THREAD
 * Snapshot* snap = mailbox.acquire();
 * if (snap != nullptr) { draw (*snap); }
 * @endcode
 *
 * @see Render::Snapshot
 * @see Screen::updateSnapshot()
 */
// THREAD CONTRACT:
//   - publish(): MESSAGE THREAD only — returns old pointer for reuse
//   - acquire(): GL THREAD only — returns latest, or nullptr
//   - Screen owns all Snapshot instances (snapshotA, snapshotB)
class Mailbox
{
public:
    Mailbox() = default;
    ~Mailbox() = default;

    /**
     * @brief Deposits a new snapshot and returns the previous one.
     *
     * Atomically replaces the current slot with @p snapshot and returns
     * whatever was there before (may be `nullptr` if the GL THREAD already
     * consumed it).
     *
     * @param snapshot  Pointer to the newly built snapshot.  Must not be
     *                  `nullptr`.
     * @return          The previously published snapshot pointer (for reuse),
     *                  or `nullptr` if the GL THREAD has already consumed it.
     *
     * @note **MESSAGE THREAD** only.
     */
    // MESSAGE THREAD: publish new snapshot, get back old one for reuse
    Snapshot* publish (Snapshot* snapshot) noexcept
    {
        return current.exchange (snapshot, std::memory_order_acq_rel);
    }

    /**
     * @brief Takes the latest snapshot from the mailbox.
     *
     * Atomically replaces the current slot with `nullptr` and returns the
     * pointer that was there.  Returns `nullptr` if no snapshot has been
     * published since the last `acquire()`.
     *
     * @return  Pointer to the latest snapshot, or `nullptr` if none is
     *          available.
     *
     * @note **GL THREAD** only.
     */
    // GL THREAD: acquire latest snapshot
    Snapshot* acquire() noexcept
    {
        return current.exchange (nullptr, std::memory_order_acq_rel);
    }

    /**
     * @brief Returns true if a snapshot is waiting in the mailbox.
     *
     * Informational only — the result may be stale by the time the caller
     * acts on it.
     *
     * @return `true` if the current slot is non-null.
     */
    bool hasSnapshot() const noexcept
    {
        return current.load (std::memory_order_acquire) != nullptr;
    }

private:
    std::atomic<Snapshot*> current { nullptr }; ///< The single snapshot slot; exchanged atomically.
};

/**
 * @class OpenGL
 * @brief JUCE OpenGL renderer that consumes `Render::Snapshot` frames.
 *
 * Implements `juce::OpenGLRenderer` and is attached to a `juce::Component`
 * via `attachTo()`.  On each vsync the JUCE OpenGL thread calls
 * `renderOpenGL()`, which:
 *
 * 1. Acquires the latest snapshot from the `Render::Mailbox`.
 * 2. Uploads any staged atlas bitmaps to GPU textures.
 * 3. Draws background quads via the background shader.
 * 4. Draws mono glyph instances via the mono shader.
 * 5. Draws emoji glyph instances via the emoji shader.
 *
 * @par Thread context
 * All `juce::OpenGLRenderer` callbacks (`newOpenGLContextCreated`,
 * `renderOpenGL`, `openGLContextClosing`) run on the **GL THREAD**.
 * `setResources()`, `setViewport()`, `attachTo()`, `detach()`, and
 * `triggerRepaint()` are called from the **MESSAGE THREAD**.
 *
 * @see Render::Mailbox
 * @see GlyphAtlas
 */
// GL THREAD
class OpenGL : public juce::OpenGLRenderer
{
public:
    OpenGL();
    ~OpenGL() override;

    /**
     * @brief Called by JUCE when the OpenGL context is first created.
     *
     * Compiles shaders, creates VAO/VBO buffers, and sets `contextReady`.
     * Called on the **GL THREAD**.
     */
    void newOpenGLContextCreated() override;

    /**
     * @brief Called by JUCE on every vsync to render one frame.
     *
     * Acquires the latest snapshot, uploads staged atlas bitmaps, and issues
     * draw calls for backgrounds, mono glyphs, and emoji glyphs.
     * Called on the **GL THREAD**.
     */
    void renderOpenGL() override;

    /**
     * @brief Called by JUCE when the OpenGL context is about to be destroyed.
     *
     * Releases GPU resources (textures, VAO, VBOs, shaders).
     * Called on the **GL THREAD**.
     */
    void openGLContextClosing() override;

    /**
     * @brief Wires up the mailbox and atlas pointers used during rendering.
     *
     * Must be called before the OpenGL context is created.  Both pointers
     * must remain valid for the lifetime of this `OpenGL` instance.
     *
     * @param mailbox  Pointer to the `Render::Mailbox` owned by `Screen::Resources`.
     * @param atlas    Pointer to the `GlyphAtlas` owned by `Screen::Resources`.
     *
     * @note **MESSAGE THREAD**.
     */
    void setResources (Mailbox* mailbox, GlyphAtlas* atlas) noexcept
    {
        snapshotMailbox = mailbox;
        glyphAtlas = atlas;
    }

    /**
     * @brief Updates the OpenGL viewport to match the component bounds.
     *
     * Scales @p bounds by `Fonts::getDisplayScale()` to convert from logical
     * to physical pixels, then stores the result for use in `renderOpenGL()`.
     *
     * @param bounds  Component bounds in logical (CSS/point) pixel space.
     *
     * @note **MESSAGE THREAD**.
     */
    void setViewport (const juce::Rectangle<int>& bounds) noexcept
    {
        const float scale { Fonts::getDisplayScale() };
        viewportX      = static_cast<int> (static_cast<float> (bounds.getX())      * scale);
        viewportY      = static_cast<int> (static_cast<float> (bounds.getY())      * scale);
        viewportWidth  = static_cast<int> (static_cast<float> (bounds.getWidth())  * scale);
        viewportHeight = static_cast<int> (static_cast<float> (bounds.getHeight()) * scale);
    }

    /**
     * @brief Attaches the OpenGL context to @p target and starts rendering.
     *
     * @param target  Component to render into.
     *
     * @note **MESSAGE THREAD**.
     */
    void attachTo (juce::Component& target) noexcept;

    /**
     * @brief Detaches the OpenGL context from the current component.
     *
     * @note **MESSAGE THREAD**.
     */
    void detach() noexcept;

    /**
     * @brief Requests a repaint on the next vsync.
     *
     * @note **MESSAGE THREAD**.
     */
    void triggerRepaint() noexcept;

    /**
     * @brief Returns true if the OpenGL context is currently attached.
     *
     * @return `true` if attached to a component.
     *
     * @note **MESSAGE THREAD**.
     */
    bool isAttached() const noexcept;

    /**
     * @brief Atomically reads and clears the context-ready flag.
     *
     * Returns `true` once after `newOpenGLContextCreated()` has run.  Used
     * by the message thread to detect when the GL context is ready for use.
     *
     * @return `true` if the context became ready since the last call.
     *
     * @note **MESSAGE THREAD**.
     */
    bool consumeContextReady() noexcept { return contextReady.exchange (false); }

private:
    /**
     * @brief Compiles the mono, emoji, and background GLSL shader programs.
     *
     * Called from `newOpenGLContextCreated()` on the **GL THREAD**.
     */
    void compileShaders();

    /**
     * @brief Creates the VAO, quad VBO, and instance VBO.
     *
     * Called from `newOpenGLContextCreated()` on the **GL THREAD**.
     */
    void createBuffers();

    /**
     * @brief Uploads any pending atlas bitmaps to the mono/emoji GPU textures.
     *
     * Called from `renderOpenGL()` on the **GL THREAD**.
     */
    void uploadStagedBitmaps();

    /**
     * @brief Issues an instanced draw call for an array of glyph instances.
     *
     * Uploads @p data to the instance VBO and calls `glDrawArraysInstanced`.
     *
     * @param data     Pointer to the glyph instance array.
     * @param count    Number of instances to draw.
     * @param isEmoji  If `true`, uses the emoji shader and texture; otherwise
     *                 uses the mono shader and texture.
     *
     * @note **GL THREAD**.
     */
    void drawInstances (const Glyph* data, int count, bool isEmoji);

    /**
     * @brief Issues draw calls for an array of background quads.
     *
     * @param data   Pointer to the background array.
     * @param count  Number of quads to draw.
     *
     * @note **GL THREAD**.
     */
    void drawBackgrounds (const Background* data, int count);

    Mailbox*    snapshotMailbox { nullptr }; ///< Pointer to the shared mailbox; not owned.
    GlyphAtlas* glyphAtlas      { nullptr }; ///< Pointer to the shared glyph atlas; not owned.

    juce::OpenGLContext openGLContext; ///< JUCE OpenGL context managing the platform GL surface.

    // OWNED - snapshot acquired from mailbox via acquire()
    // Screen owns the snapshots; GL thread just borrows a pointer
    Snapshot* currentSnapshot { nullptr }; ///< Most recently acquired snapshot; borrowed from Screen.

    std::unique_ptr<juce::OpenGLShaderProgram> monoShader;       ///< Shader for monochrome glyph instances.
    std::unique_ptr<juce::OpenGLShaderProgram> emojiShader;      ///< Shader for colour emoji glyph instances.
    std::unique_ptr<juce::OpenGLShaderProgram> backgroundShader; ///< Shader for background colour quads.

    GLuint monoAtlasTexture  { 0 }; ///< GPU texture ID for the monochrome glyph atlas.
    GLuint emojiAtlasTexture { 0 }; ///< GPU texture ID for the colour emoji atlas.

    GLuint vao         { 0 }; ///< Vertex array object.
    GLuint quadVBO     { 0 }; ///< VBO holding the unit-quad vertex data (shared across instances).
    GLuint instanceVBO { 0 }; ///< VBO holding per-instance glyph data (updated each frame).

    int viewportX      { 0 }; ///< Physical pixel X origin of the render viewport.
    int viewportY      { 0 }; ///< Physical pixel Y origin of the render viewport.
    int viewportWidth  { 0 }; ///< Physical pixel width of the render viewport.
    int viewportHeight { 0 }; ///< Physical pixel height of the render viewport.

    std::atomic<bool> contextReady { false }; ///< Set to true in newOpenGLContextCreated(); cleared by consumeContextReady().
};

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
 * - **Cell cache**: `hotCells` / `coldGraphemes` mirror the visible grid.
 *   `populateFromGrid()` copies only dirty rows from `Grid`.
 * - **Per-row render cache**: `cachedMono`, `cachedEmoji`, `cachedBg` store
 *   pre-built glyph/background instances per row.  `buildSnapshot()` rebuilds
 *   only dirty rows.
 * - **Scroll optimisation**: `applyScrollOptimization()` shifts caches by the
 *   scroll delta, avoiding a full rebuild on scroll events.
 * - **Snapshot publication**: `updateSnapshot()` packs the per-row caches into
 *   a `Render::Snapshot` and publishes it via `Render::Mailbox`.
 * - **Selection overlay**: a `ScreenSelection*` pointer is checked per cell in
 *   `processCellForSnapshot()` to emit selection highlight quads.
 *
 * @par Thread context
 * All public methods except `getSnapshotMailbox()` must be called on the
 * **MESSAGE THREAD**.  The `Render::OpenGL` sub-object runs its callbacks on
 * the **GL THREAD** and communicates only through the `Render::Mailbox`.
 *
 * @see Grid
 * @see State
 * @see Render::Mailbox
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
     * accessed by both `Screen` (MESSAGE THREAD) and `Render::OpenGL`
     * (GL THREAD) through carefully controlled interfaces.
     */
    struct Resources
    {
        /**
         * @brief Constructs all resources for the given font family and size.
         *
         * @param fontFamily  Font family name passed to `Fonts`.
         * @param pointSize   Initial font size in points.
         */
        Resources (const juce::String& fontFamily, float pointSize)
            : fonts (fontFamily, pointSize)
        {
        }

        GlyphAtlas       glyphAtlas;       ///< Glyph atlas: rasterises and caches glyphs on the GPU.
        Render::Mailbox  snapshotMailbox;  ///< Lock-free snapshot exchange between MESSAGE THREAD and GL THREAD.
        Fonts            fonts;            ///< Font manager: loading, shaping, and metrics.
        Theme            terminalColors;   ///< Active colour theme (ANSI palette + default fg/bg/selection).
    };

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Constructs the screen with the given font family and point size.
     *
     * Initialises `Resources`, calls `calc()` to derive cell dimensions, wires
     * the GL renderer to the mailbox and atlas, then calls `reset()`.
     *
     * @param fontFamily  Font family name (e.g. "JetBrains Mono").
     * @param pointSize   Initial font size in points.
     *
     * @note **MESSAGE THREAD**.
     */
    Screen (const juce::String& fontFamily, float pointSize);

    /**
     * @brief Destroys the screen and releases all resources.
     *
     * @note **MESSAGE THREAD**.  The GL renderer must be detached before
     *       destruction to avoid use-after-free on the GL THREAD.
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
     * Stores the new bounds, forwards them to the GL renderer, and calls
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

    // =========================================================================
    // OpenGL attachment
    // =========================================================================

    /**
     * @brief Attaches the OpenGL renderer to @p component.
     *
     * @param component  JUCE component to render into.
     *
     * @note **MESSAGE THREAD**.
     */
    void attachTo (juce::Component& component);

    /**
     * @brief Detaches the OpenGL renderer from its current component.
     *
     * @note **MESSAGE THREAD**.
     */
    void detach();

    /**
     * @brief Returns true if the OpenGL renderer is attached to a component.
     *
     * @return `true` if attached.
     *
     * @note **MESSAGE THREAD**.
     */
    bool isAttached() const noexcept;

    /**
     * @brief Atomically reads and clears the GL context-ready flag.
     *
     * @return `true` once after the GL context has been created.
     *
     * @note **MESSAGE THREAD**.
     */
    bool consumeContextReady() noexcept;

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
     * @return `true` if `Render::Mailbox::hasSnapshot()` returns true.
     *
     * @note **MESSAGE THREAD** (informational; result may be stale).
     */
    bool hasNewSnapshot() const noexcept;

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Performs one full render cycle: update cache, build snapshot, trigger repaint.
     *
     * Called once per frame from the terminal view on the **MESSAGE THREAD**.
     * Steps:
     * 1. Allocates / resizes `hotCells` and `coldGraphemes` if the grid size changed.
     * 2. Consumes dirty rows and scroll delta from `Grid`.
     * 3. Applies scroll optimisation or marks all rows dirty as needed.
     * 4. Marks all rows dirty if a selection is active or the view is scrolled.
     * 5. Calls `populateFromGrid()` to copy dirty rows from `Grid`.
     * 6. Calls `buildSnapshot()` to rebuild dirty rows and pack the snapshot.
     * 7. Calls `Render::OpenGL::triggerRepaint()`.
     *
     * @param state  Current terminal state (cursor, dimensions, scroll offset).
     * @param grid   Terminal grid providing cell data and dirty tracking.
     *
     * @note **MESSAGE THREAD**.
     */
    void render (const State& state, Grid& grid) noexcept;

    /**
     * @brief Resets the cell cache to default cells and clears row counts.
     *
     * Fills `hotCells` with default-constructed `Cell` values and
     * `coldGraphemes` with default `Grapheme` values.  Resets `cacheRows`,
     * `cacheCols`, and `bgCacheCols` to zero.
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
     * @brief Returns a mutable reference to the font manager.
     *
     * @return Reference to `resources.fonts`.
     *
     * @note **MESSAGE THREAD**.
     */
    Fonts& getFonts() noexcept;

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
     * @brief Returns a mutable reference to the snapshot mailbox.
     *
     * @return Reference to `resources.snapshotMailbox`.
     *
     * @note The mailbox is thread-safe; `publish()` is MESSAGE THREAD,
     *       `acquire()` is GL THREAD.
     */
    Render::Mailbox& getSnapshotMailbox() noexcept;

    /**
     * @brief Returns a read-only reference to the snapshot mailbox.
     *
     * @return Const reference to `resources.snapshotMailbox`.
     */
    const Render::Mailbox& getSnapshotMailbox() const noexcept;

private:
    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Returns true if @p row is marked dirty in the 256-bit bitmask.
     *
     * @param dirty  Four-word dirty bitmask (256 bits, one per row).
     * @param row    Row index to test (0–255).
     * @return       `true` if bit @p row is set.
     */
    static bool isRowDirty (const uint64_t dirty[4], int row) noexcept;

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
     * `cols * 2` background slots.
     *
     * @param rows  Number of visible rows.
     * @param cols  Number of visible columns.
     *
     * @note **MESSAGE THREAD**.
     */
    void allocateRowCache (int rows, int cols) noexcept;

    /**
     * @brief Packs per-row caches into a `Render::Snapshot` and publishes it.
     *
     * Totals the per-row counts, ensures snapshot capacity, copies glyph and
     * background data via `memcpy`, sets cursor state, and calls
     * `Render::Mailbox::publish()`.  The returned pointer is stored as
     * `writeSnapshot` for the next frame.
     *
     * @param state      Current terminal state (cursor position, screen type).
     * @param rows       Number of visible rows.
     * @param maxGlyphs  Maximum glyph slots per row (`cacheCols * 2`).
     *
     * @note **MESSAGE THREAD**.
     */
    void updateSnapshot (const State& state, int rows, int maxGlyphs) noexcept;

    /**
     * @brief Shifts per-row caches upward by @p scroll rows.
     *
     * Uses `memmove` to shift `hotCells`, `coldGraphemes`, `cachedMono`,
     * `cachedEmoji`, `cachedBg`, and the count arrays.  Adjusts Y positions
     * of cached glyph instances by `-scroll * physCellHeight`.  Clears the
     * newly exposed rows at the bottom.
     *
     * @param rows    Number of visible rows.
     * @param cols    Number of visible columns.
     * @param scroll  Number of rows scrolled (positive = scroll up).
     * @param dirty   Dirty bitmask (unused; reserved for future use).
     *
     * @note **MESSAGE THREAD**.
     */
    void applyScrollOptimization (int rows, int cols, int scroll, uint64_t dirty[4]) noexcept;

    // =========================================================================
    // Private render pipeline
    // =========================================================================

    /**
     * @brief Copies dirty rows from `Grid` into the `hotCells` / `coldGraphemes` cache.
     *
     * For each row marked dirty in @p dirty, reads the row pointer from `Grid`
     * (scrollback or active, depending on `state.getScrollOffset()`) and
     * copies cells and graphemes into the flat cache arrays.
     *
     * @param state  Current terminal state.
     * @param grid   Source grid.
     * @param dirty  256-bit dirty bitmask.
     *
     * @note **MESSAGE THREAD**.
     */
    void populateFromGrid (const State& state, const Grid& grid, const uint64_t dirty[4]) noexcept;

    /**
     * @brief Rebuilds dirty rows in the per-row caches and calls `updateSnapshot()`.
     *
     * Iterates all rows; for each dirty row, resets the row's glyph/bg counts
     * and calls `processCellForSnapshot()` for every cell.  Then calls
     * `updateSnapshot()` to publish the result.
     *
     * @param state  Current terminal state.
     * @param dirty  256-bit dirty bitmask.
     *
     * @note **MESSAGE THREAD**.
     */
    void buildSnapshot (const State& state, const uint64_t dirty[4]) noexcept;

    /**
     * @brief Processes one cell and appends its contributions to the row caches.
     *
     * Resolves foreground and background colours, emits a background quad if
     * the background is non-default, dispatches to block-char / box-drawing /
     * glyph rendering, and emits a selection overlay quad if the cell is
     * selected.
     *
     * @param cell  The cell to render.
     * @param col   Column index of the cell.
     * @param row   Row index of the cell.
     *
     * @note **MESSAGE THREAD**.
     * @see buildCellInstance()
     * @see buildBlockRect()
     */
    void processCellForSnapshot (const Cell& cell, int col, int row) noexcept;

    /**
     * @brief Shapes and rasterises one cell's glyph(s) into the row cache.
     *
     * Handles box-drawing (procedural rasterisation), FontCollection fallback
     * lookup, ligature shaping, and standard HarfBuzz shaping.  Appends
     * `Render::Glyph` instances to `cachedMono` or `cachedEmoji`.
     *
     * @param cell       The cell to render.
     * @param grapheme   Optional grapheme cluster for multi-codepoint cells; may be `nullptr`.
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
                            int col, int row,
                            const juce::Colour& foreground) noexcept;

    /**
     * @brief Attempts to shape a 2- or 3-character ligature starting at @p col.
     *
     * Tries lengths 3 then 2.  If HarfBuzz produces fewer glyphs than input
     * codepoints, the sequence is a ligature: emits the shaped glyphs and
     * returns the number of subsequent cells to skip.
     *
     * @param col         Starting column.
     * @param row         Row index.
     * @param style       Font style for shaping.
     * @param fontHandle  Platform font handle.
     * @param foreground  Resolved foreground colour.
     * @return            Number of cells to skip after this one (0 if no ligature found).
     *
     * @note **MESSAGE THREAD**.
     */
    int tryLigature (int col, int row, Fonts::Style style, void* fontHandle,
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
    // Data
    // =========================================================================

    Render::OpenGL glRenderer; ///< OpenGL renderer; runs its callbacks on the GL THREAD.

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

    juce::HeapBlock<Cell>     hotCells;      ///< Flat mirror of the visible grid cells (rows × cols).
    size_t                    hotCellCount { 0 }; ///< Number of elements in hotCells (rows × cols).
    juce::HeapBlock<Grapheme> coldGraphemes; ///< Grapheme cluster sidecar co-indexed with hotCells.

    // Per-row render cache
    juce::HeapBlock<Render::Glyph>      cachedMono;   ///< Mono glyph instances; layout: [row][0 … maxGlyphs-1].
    juce::HeapBlock<Render::Glyph>      cachedEmoji;  ///< Emoji glyph instances; layout: [row][0 … maxGlyphs-1].
    juce::HeapBlock<Render::Background> cachedBg;     ///< Background quads; layout: [row][0 … bgCacheCols-1].
    juce::HeapBlock<int>                monoCount;    ///< Number of valid mono glyphs per row.
    juce::HeapBlock<int>                emojiCount;   ///< Number of valid emoji glyphs per row.
    juce::HeapBlock<int>                bgCount;      ///< Number of valid background quads per row.
    int cacheRows    { 0 }; ///< Number of rows the per-row caches were allocated for.
    int cacheCols    { 0 }; ///< Number of columns the per-row caches were allocated for.
    int bgCacheCols  { 0 }; ///< Background slots per row (= cacheCols * 2, for bg + selection overlay).

    // Double-buffered snapshots for persistent reuse
    Render::Snapshot  snapshotA;              ///< First snapshot buffer; alternates with snapshotB.
    Render::Snapshot  snapshotB;              ///< Second snapshot buffer; alternates with snapshotA.
    Render::Snapshot* writeSnapshot { nullptr }; ///< Points to whichever snapshot is being written this frame.

    const ScreenSelection* selection   { nullptr }; ///< Non-owning pointer to the active selection; nullptr if none.
    bool                   hadSelection { false };  ///< True if a selection was active on the previous frame (forces full dirty).
    bool                   wasScrolled  { false };  ///< True if the view was scrolled on the previous frame (forces full dirty).
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
