/**
 * @file Glyph.cpp
 * @brief Glyph renderer lifecycle, cache management, row processing, and snapshot packing.
 */

#include "Glyph.h"
#include "../logic/Grid.h"
#include "../selection/LinkManager.h"

namespace Terminal::Renderer
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

// =========================================================================
// Lifecycle
// =========================================================================

template <typename Context>
Glyph<Context>::Glyph (jam::Font& fontToUse, jam::Glyph::Packer& packerToUse) noexcept
    : font (fontToUse)
    , packer (packerToUse)
{
}

// =========================================================================
// Configuration
// =========================================================================

template <typename Context>
void Glyph<Context>::setGeometry (int physCellW, int physCellH, int physBase, float fontSize) noexcept
{
    physCellWidth = physCellW;
    physCellHeight = physCellH;
    physBaseline = physBase;
    baseFontSize = fontSize;
}

template <typename Context>
void Glyph<Context>::setLigatures (bool enabled) noexcept
{
    ligatureEnabled = enabled;
}

template <typename Context>
void Glyph<Context>::setTheme (const Theme* t) noexcept
{
    theme = t;
}

template <typename Context>
void Glyph<Context>::setSelection (const ScreenSelection* sel) noexcept
{
    selection = sel;
}

template <typename Context>
void Glyph<Context>::setLinkManager (const LinkManager* lm) noexcept
{
    linkManager = lm;
}

template <typename Context>
void Glyph<Context>::setLinkUnderlay (const LinkSpan* links, int count) noexcept
{
    linkUnderlay = links;
    linkUnderlayCount = count;
}

// =========================================================================
// Cache lifecycle
// =========================================================================

template <typename Context>
void Glyph<Context>::ensureCache (int rows, int cols) noexcept
{
    if (cacheRows != rows or cacheCols != cols)
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

        cacheRows = rows;
        cacheCols = cols;
        bgCacheCols = cols * 3;
        maxGlyphsPerRow = maxGlyphs;
    }
}

template <typename Context>
void Glyph<Context>::resetCache() noexcept
{
    cacheRows = 0;
    cacheCols = 0;
    bgCacheCols = 0;
}

// =========================================================================
// Per-frame processing
// =========================================================================

template <typename Context>
void Glyph<Context>::beginFrame() noexcept
{
    hintOverlay      = linkManager != nullptr ? linkManager->getActiveHintsData() : nullptr;
    hintOverlayCount = linkManager != nullptr ? linkManager->getActiveHintsCount() : 0;
}

template <typename Context>
void Glyph<Context>::processRow (int row, const Cell* rowCells, int cols,
                                  const Grapheme* rowGraphemes, Grid& grid,
                                  bool fullRebuild) noexcept
{
    if (rowCells != nullptr)
    {
        Cell* prevRow { previousCells.get()
                        + static_cast<size_t> (row) * static_cast<size_t> (cacheCols) };

        if (fullRebuild
            or std::memcmp (rowCells, prevRow, static_cast<size_t> (cols) * sizeof (Cell)) != 0)
        {
            monoCount[row]  = 0;
            emojiCount[row] = 0;
            bgCount[row]    = 0;
            ligatureSkip    = 0;

            // Leading/trailing blank trim
            int firstCol { 0 };

            while (firstCol < cols
                   and rowCells[firstCol].codepoint == 0
                   and rowCells[firstCol].bg.isTheme()
                   and rowCells[firstCol].style == 0)
            {
                ++firstCol;
            }

            int lastCol { cols - 1 };

            while (lastCol >= firstCol
                   and rowCells[lastCol].codepoint == 0
                   and rowCells[lastCol].bg.isTheme()
                   and rowCells[lastCol].style == 0)
            {
                --lastCol;
            }

            for (int c { firstCol }; c <= lastCol; ++c)
            {
                processCell (rowCells[c], rowCells, rowGraphemes, c, row, grid);
            }

            // Store current row for next-frame comparison.
            std::memcpy (prevRow, rowCells, static_cast<size_t> (cols) * sizeof (Cell));
        }
    }
}

// =========================================================================
// Snapshot packing
// =========================================================================

template <typename Context>
void Glyph<Context>::packSnapshot (Render::Snapshot& snapshot, int rows,
                                    const uint64_t* frameDirtyBits) noexcept
{
    int totalMono  { 0 };
    int totalEmoji { 0 };
    int totalBg    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        if (isRowIncludedInSnapshot (r, frameDirtyBits))
        {
            totalMono  += monoCount[r];
            totalEmoji += emojiCount[r];
            totalBg    += bgCount[r];
        }
    }

    snapshot.ensureCapacity (totalMono, totalEmoji, totalBg);

    int monoOffset  { 0 };
    int emojiOffset { 0 };
    int bgOffset    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        if (isRowIncludedInSnapshot (r, frameDirtyBits))
        {
            if (monoCount[r] > 0)
            {
                std::memcpy (snapshot.mono.get() + monoOffset,
                             cachedMono.get() + r * maxGlyphsPerRow,
                             static_cast<size_t> (monoCount[r]) * sizeof (Render::Glyph));
                monoOffset += monoCount[r];
            }

            if (emojiCount[r] > 0)
            {
                std::memcpy (snapshot.emoji.get() + emojiOffset,
                             cachedEmoji.get() + r * maxGlyphsPerRow,
                             static_cast<size_t> (emojiCount[r]) * sizeof (Render::Glyph));
                emojiOffset += emojiCount[r];
            }

            if (bgCount[r] > 0)
            {
                std::memcpy (snapshot.backgrounds.get() + bgOffset,
                             cachedBg.get() + r * bgCacheCols,
                             static_cast<size_t> (bgCount[r]) * sizeof (Render::Background));
                bgOffset += bgCount[r];
            }
        }
    }

    snapshot.monoCount       = totalMono;
    snapshot.emojiCount      = totalEmoji;
    snapshot.backgroundCount = totalBg;

    packer.publishStagedBitmaps();
}

template class Glyph<jam::Glyph::GLContext>;
template class Glyph<jam::Glyph::GraphicsContext>;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal::Renderer
