/**
 * @file jreng_atlas_packer.h
 * @brief Shelf-based rectangle packer for the 4096×4096 glyph atlas texture.
 *
 * AtlasPacker implements the classic "shelf" (strip) bin-packing algorithm for
 * allocating rectangular regions within a fixed-size 2D texture atlas.  It is
 * used by `GlyphAtlas` to assign atlas sub-regions to newly rasterized glyphs.
 *
 * ### Algorithm
 * Rectangles are packed into horizontal shelves.  Each shelf has a fixed height
 * equal to the first rectangle placed on it.  Subsequent rectangles are placed
 * on the first shelf whose height is ≥ the requested height and that has
 * sufficient remaining horizontal space.  If no existing shelf fits, a new shelf
 * is opened immediately below the last one.
 *
 * @par Complexity
 * - `allocate()`: O(S) where S is the number of open shelves (typically < 200).
 * - `reset()`: O(1) — just zeroes the shelf count.
 *
 * @par Limitations
 * - No deallocation of individual regions; the entire atlas is reset via
 *   `reset()` when the LRU cache is cleared.
 * - Shelf height is fixed at the height of the first glyph placed on it, so
 *   tall glyphs on a short shelf will not fit and will open a new shelf.
 * - The shelf array grows by doubling from an initial capacity of 64.
 *
 * @note All allocation must happen on the **MESSAGE THREAD**.  Read-only access
 *       to `atlasWidth` / `atlasHeight` is safe from any thread after
 *       construction.
 *
 * @see GlyphAtlas
 * @see LRUCache
 */

#pragma once

namespace jreng::Glyph
{

/**
 * @struct AtlasPacker
 * @brief Shelf-based 2D bin packer for a fixed-size texture atlas.
 *
 * Maintains a dynamic array of `Shelf` records.  Each shelf occupies a
 * horizontal strip of the atlas at a fixed Y offset and height.  Rectangles
 * are placed left-to-right within the first shelf that can accommodate them.
 *
 * @par Thread context
 * - **MESSAGE THREAD**: `allocate()`, `reset()`, `growShelves()`.
 * - **ANY THREAD** (read-only): `atlasWidth`, `atlasHeight` after construction.
 *
 * @see GlyphAtlas
 */
// MESSAGE THREAD - Allocation
// ANY THREAD - Read-only access after allocation
struct AtlasPacker
{
    /**
     * @struct Shelf
     * @brief One horizontal strip within the atlas.
     *
     * A shelf is opened when a rectangle cannot fit on any existing shelf.
     * Its `height` is fixed at the height of the first rectangle placed on it.
     * `currentX` advances right as rectangles are packed.
     */
    struct Shelf
    {
        /** @brief Y coordinate of the shelf's top edge in texels. */
        int y { 0 };

        /**
         * @brief Height of this shelf in texels.
         *
         * Set to the height of the first rectangle placed on the shelf and
         * never changed thereafter.
         */
        int height { 0 };

        /**
         * @brief X coordinate of the next free position on this shelf.
         *
         * Starts at 0 and advances by the width of each allocated rectangle.
         */
        int currentX { 0 };
    };

    /**
     * @brief Construct a packer for a texture of the given dimensions.
     *
     * Allocates an initial shelf array of 64 entries (zero-initialized).
     * The initial capacity of 64 covers typical glyph variety before the
     * first grow.
     *
     * @param width  Atlas texture width in texels.
     * @param height Atlas texture height in texels.
     */
    AtlasPacker (int width, int height) noexcept
        : atlasWidth (width), atlasHeight (height)
    {
        shelfCapacity = 64; // Initial shelf count; covers typical glyph variety before first grow
        shelves.allocate (static_cast<size_t> (shelfCapacity), true);
    }

    /**
     * @brief Double the shelf array capacity.
     *
     * Allocates a new `HeapBlock` of `shelfCapacity * 2` entries, copies the
     * existing shelves into it, and replaces the old block.  Called
     * automatically by `allocate()` when `shelfCount >= shelfCapacity`.
     *
     * @note MESSAGE THREAD only.
     */
    void growShelves() noexcept
    {
        const int newCapacity { shelfCapacity * 2 };
        juce::HeapBlock<Shelf> grown;
        grown.allocate (static_cast<size_t> (newCapacity), true);
        std::memcpy (grown.get(), shelves.get(), static_cast<size_t> (shelfCount) * sizeof (Shelf));
        shelves = std::move (grown);
        shelfCapacity = newCapacity;
    }

    /**
     * @brief Allocate a `width × height` rectangle in the atlas.
     *
     * Scans existing shelves for one whose height is ≥ `height` and that has
     * at least `width` pixels of horizontal space remaining.  If found, the
     * rectangle is placed at `(shelf.currentX, shelf.y)` and `currentX` is
     * advanced.
     *
     * If no existing shelf fits, a new shelf is opened at the Y position
     * immediately below the last shelf.  If the new shelf would exceed
     * `atlasHeight`, an empty rectangle is returned to signal atlas-full.
     *
     * @param width  Requested rectangle width in texels.
     * @param height Requested rectangle height in texels.
     * @return The allocated `juce::Rectangle<int>` (x, y, width, height) in
     *         atlas texel coordinates, or an empty rectangle if the atlas is
     *         full.
     *
     * @note MESSAGE THREAD only.
     * @see reset()
     */
    juce::Rectangle<int> allocate (int width, int height) noexcept
    {
        juce::Rectangle<int> result;

        int fittingShelf { -1 };

        for (int i { 0 }; i < shelfCount; ++i)
        {
            jassert (i >= 0 and i < shelfCount);
            const auto& shelf { shelves[i] };

            if (fittingShelf == -1 and height <= shelf.height and shelf.currentX + width <= atlasWidth)
            {
                fittingShelf = i;
            }
        }

        if (fittingShelf != -1)
        {
            jassert (fittingShelf >= 0 and fittingShelf < shelfCount);
            auto& shelf { shelves[fittingShelf] };
            result = juce::Rectangle<int> { shelf.currentX, shelf.y, width, height };
            shelf.currentX += width;
        }
        else
        {
            int shelfY { 0 };

            if (shelfCount > 0)
            {
                jassert (shelfCount - 1 >= 0 and shelfCount - 1 < shelfCount);
                const auto& lastShelf { shelves[shelfCount - 1] };
                shelfY = lastShelf.y + lastShelf.height;
            }

            if (shelfY + height <= atlasHeight)
            {
                if (shelfCount >= shelfCapacity)
                {
                    growShelves();
                }

                jassert (shelfCount >= 0 and shelfCount < shelfCapacity);
                shelves[shelfCount].y        = shelfY;
                shelves[shelfCount].height   = height;
                shelves[shelfCount].currentX = width;
                ++shelfCount;

                result = juce::Rectangle<int> { 0, shelfY, width, height };
            }
        }

        return result;
    }

    /**
     * @brief Reset the packer to empty, discarding all shelf state.
     *
     * Sets `shelfCount` to zero.  The shelf array is not deallocated; its
     * capacity is preserved for the next allocation cycle.  Called by
     * `GlyphAtlas::clear()` when the LRU cache is invalidated.
     *
     * @note MESSAGE THREAD only.
     */
    void reset() noexcept
    {
        shelfCount = 0;
    }

    /** @brief Atlas texture width in texels. */
    int atlasWidth { 0 };

    /** @brief Atlas texture height in texels. */
    int atlasHeight { 0 };

    /**
     * @brief Heap-allocated array of shelf records.
     *
     * Capacity starts at 64 and doubles via `growShelves()` as needed.
     * Only indices `[0, shelfCount)` are valid.
     */
    juce::HeapBlock<Shelf> shelves;

    /** @brief Number of shelves currently in use. */
    int shelfCount { 0 };

    /** @brief Allocated capacity of the `shelves` array in entries. */
    int shelfCapacity { 0 };
};

} // namespace jreng::Glyph
