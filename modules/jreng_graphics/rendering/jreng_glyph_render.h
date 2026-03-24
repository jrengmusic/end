#pragma once

namespace jreng::Glyph
{

/**
 * @struct Render
 * @brief Namespace-struct grouping all render types.
 *
 * `Render` is a plain struct used as a namespace to group the types that
 * cross the MESSAGE THREAD / RENDER THREAD boundary:
 *
 * - `Render::Background`    — a coloured rectangle (cell background or block char).
 * - `Render::Quad`          — a positioned, textured glyph instance.
 * - `Render::SnapshotBase`  — the generic per-frame arrays of quads + backgrounds.
 *
 * @see jreng::SnapshotBuffer
 */
struct Render
{

/**
 * @struct Background
 * @brief A coloured rectangle to be drawn as a cell background or block element.
 *
 * Trivially copyable.  Coordinates are in physical (HiDPI-scaled) pixel space.
 *
 * @note `static_assert` below verifies trivial copyability.
 *
 * @see Render::SnapshotBase::backgrounds
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
 * @struct Quad
 * @brief A positioned, textured glyph instance for rendering.
 *
 * Describes one glyph to be drawn: its screen position, size, atlas texture
 * coordinates, and foreground colour.  Arrays of `Quad` are consumed by the
 * renderer backend (GL instanced draw or `juce::Graphics` blit).
 *
 * @note Renamed from `Glyph` to `Quad` to avoid collision with
 *       `jreng::Typeface::Glyph` (the shaped glyph output from HarfBuzz).
 *       In the render context this type is a positioned quad with texture coords.
 *
 * @par Thread contract
 * - **MESSAGE THREAD**: builds and writes `Quad` instances
 *   into `Render::SnapshotBase`.
 * - **RENDER THREAD**: reads `Quad` instances from the acquired snapshot
 *   (immutable after publish).
 *
 * @note `static_assert` below verifies trivial copyability.
 *
 * @see Render::SnapshotBase::mono
 * @see Render::SnapshotBase::emoji
 */
struct Quad
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
 * @struct SnapshotBase
 * @brief A complete rendered frame: glyph instances + background quads.
 *
 * Owns three `HeapBlock` arrays — `mono`, `emoji`, and `backgrounds` — that
 * hold all draw calls for one frame.  Capacity is grown on demand via
 * `ensureCapacity()` and never shrunk, so allocations stabilise after a few
 * frames.
 *
 * Two instances are owned internally by `jreng::SnapshotBuffer` and
 * recycled via atomic pointer exchange to avoid per-frame allocation.
 *
 * App-level `Snapshot` types inherit from `SnapshotBase` and add
 * domain-specific fields (e.g. cursor state for the terminal renderer).
 *
 * @see jreng::SnapshotBuffer
 */
struct SnapshotBase
{
    juce::HeapBlock<Quad>        mono;              ///< Monochrome glyph instances (regular + bold + italic).
    juce::HeapBlock<Quad>        emoji;             ///< Colour emoji glyph instances.
    juce::HeapBlock<Background>  backgrounds;       ///< Background colour quads (non-default bg + selection overlay).
    int                          monoCount        { 0 }; ///< Number of valid entries in @p mono.
    int                          emojiCount       { 0 }; ///< Number of valid entries in @p emoji.
    int                          backgroundCount  { 0 }; ///< Number of valid entries in @p backgrounds.
    int                          monoCapacity     { 0 }; ///< Allocated capacity of @p mono in elements.
    int                          emojiCapacity    { 0 }; ///< Allocated capacity of @p emoji in elements.
    int                          backgroundCapacity { 0 }; ///< Allocated capacity of @p backgrounds in elements.

    /**
     * @brief Grows the backing arrays to at least the requested capacities.
     *
     * Each array is reallocated only if the requested count exceeds the
     * current capacity.  Existing data is not preserved across reallocation.
     *
     * @param monoNeeded   Minimum number of `Quad` slots required for mono.
     * @param emojiNeeded  Minimum number of `Quad` slots required for emoji.
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

static_assert (std::is_trivially_copyable_v<Render::Background>, "jreng::Glyph::Render::Background must be trivially copyable");
static_assert (std::is_trivially_copyable_v<Render::Quad>,       "jreng::Glyph::Render::Quad must be trivially copyable");

} // namespace jreng::Glyph
