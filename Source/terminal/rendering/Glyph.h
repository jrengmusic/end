/**
 * @file Glyph.h
 * @brief Text rendering subsystem: cell processing, glyph shaping, and per-row caching.
 *
 * `Terminal::Renderer::Glyph` owns all glyph-specific data and methods extracted
 * from `Screen`.  It processes terminal cells into GPU-ready glyph and background
 * quads, maintains per-row caches, and packs its output into `Render::Snapshot`.
 *
 * ## Ownership
 * Owned by `Screen` as a value member.  Lifetime bound to Screen.
 *
 * ## Thread contract
 * All methods: **MESSAGE THREAD** only.
 *
 * @see Screen
 * @see Render::Snapshot
 */
#pragma once
#include <JuceHeader.h>
#include "../data/Cell.h"
#include "../selection/LinkSpan.h"
#include "Render.h"
#include "ScreenSelection.h"

namespace Terminal
{
class Grid;
class LinkManager;
struct Grapheme;
}

namespace Terminal::Renderer
{ /*____________________________________________________________________________*/

/**
 * @class Glyph
 * @brief Text rendering: cell processing, glyph shaping, per-row quad caching.
 *
 * Reads terminal cells from `Grid`, resolves colours against the active `Theme`,
 * shapes glyphs via `jam::Glyph::Packer`, and stores the results in per-row
 * caches.  `packSnapshot()` copies the caches into a `Render::Snapshot` for
 * cross-thread handoff to the GL/CPU presenter.
 *
 * @tparam Context  Render context type — `jam::Glyph::GLContext` or
 *                  `jam::Glyph::GraphicsContext`.
 */
template <typename Context>
class Glyph
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Constructs the glyph renderer.
     *
     * @param font    Font spec carrying resolved typeface.
     * @param packer  Glyph packer; owns atlas and rasterisation.
     */
    Glyph (jam::Font& font, jam::Glyph::Packer& packer) noexcept;

    ~Glyph() = default;


    // =========================================================================
    // Configuration (called by Screen on MESSAGE THREAD)
    // =========================================================================

    /**
     * @brief Updates physical cell dimensions and font size.
     *
     * Called by Screen::calc() after viewport or font changes.
     */
    void setGeometry (int physCellW, int physCellH, int physBase, float fontSize) noexcept;

    /** @brief Enables or disables HarfBuzz ligature shaping. */
    void setLigatures (bool enabled) noexcept;

    /** @brief Sets the active colour theme for colour resolution. */
    void setTheme (const Theme* theme) noexcept;

    /** @brief Sets the active selection for overlay rendering. */
    void setSelection (const ScreenSelection* sel) noexcept;


    /** @brief Sets the LinkManager for per-frame hint overlay access. */
    void setLinkManager (const LinkManager* lm) noexcept;

    /** @brief Sets the link underlay spans for underline rendering. */
    void setLinkUnderlay (const LinkSpan* links, int count) noexcept;

    // =========================================================================
    // Cache lifecycle
    // =========================================================================

    /**
     * @brief Ensures per-row caches are allocated for the given dimensions.
     *
     * Allocates only if @p rows or @p cols differ from the current cache size.
     *
     * @param rows  Number of visible rows.
     * @param cols  Number of visible columns.
     */
    void ensureCache (int rows, int cols) noexcept;

    /**
     * @brief Zeroes cache dimension sentinels, forcing reallocation on next ensureCache.
     */
    void resetCache() noexcept;

    // =========================================================================
    // Per-frame processing (called by Screen::buildSnapshot)
    // =========================================================================

    /**
     * @brief Pulls per-frame overlay data from LinkManager.
     *
     * Must be called once per frame before processRow.
     */
    void beginFrame() noexcept;

    /**
     * @brief Processes one dirty row: memcmp skip, blank trim, cell iteration.
     *
     * Handles the full per-row pipeline:
     * 1. memcmp against previousCells — skips if unchanged (unless fullRebuild)
     * 2. Resets mono/emoji/bg counts for the row
     * 3. Trims leading/trailing blank cells
     * 4. Calls processCell for each non-blank cell
     * 5. Updates previousCells for next-frame comparison
     *
     * @param row           Row index (0-based visible row).
     * @param rowCells      Pointer to the row's cells in Grid.
     * @param cols          Number of columns.
     * @param rowGraphemes  Grapheme sidecar row pointer (may be nullptr).
     * @param grid          Terminal grid (for image staging).
     * @param fullRebuild   If true, skip memcmp optimisation.
     */
    void processRow (int row, const Cell* rowCells, int cols,
                     const Grapheme* rowGraphemes, Grid& grid, bool fullRebuild) noexcept;

    // =========================================================================
    // Snapshot packing
    // =========================================================================

    /**
     * @brief Packs per-row glyph caches into the snapshot and publishes staged bitmaps.
     *
     * Counts quads per row (respecting isRowIncludedInSnapshot), ensures snapshot
     * capacity, copies cached data via memcpy, sets mono/emoji/bg counts on the
     * snapshot, and calls packer.publishStagedBitmaps().
     *
     * @param snapshot       Write buffer of the snapshot to pack into.
     * @param rows           Number of visible rows.
     * @param frameDirtyBits Dirty row bitmask (for CPU-path row filtering).
     */
    void packSnapshot (Render::Snapshot& snapshot, int rows,
                       const uint64_t* frameDirtyBits) noexcept;

private:
    // =========================================================================
    // Private render pipeline
    // =========================================================================

    /**
     * @brief Processes one cell and appends glyph/background quads to row caches.
     *
     * Resolves colours, emits background quad, dispatches to block-char or glyph
     * shaping, emits selection overlay and link underlay quads.
     */
    void processCell (const Cell& cell, const Cell* rowCells,
                      const Grapheme* rowGraphemes, int col, int row,
                      Grid& grid) noexcept;

    /**
     * @brief Shapes and rasterises one cell's glyphs into the row cache.
     *
     * Handles box-drawing, FontCollection fallback, ligature shaping,
     * and standard HarfBuzz shaping.
     */
    void buildCellInstance (const Cell& cell, const Grapheme* grapheme,
                            const Cell* rowCells, int col, int row,
                            const juce::Colour& foreground) noexcept;

    /**
     * @brief Attempts to shape a 2- or 3-character ligature starting at col.
     *
     * @return Number of cells to skip (0 if no ligature found).
     */
    int tryLigature (const Cell* rowCells, int col, int row,
                     jam::Typeface::Style style,
                     const juce::Colour& foreground) noexcept;

    /**
     * @brief Builds a background quad for a block-element character.
     */
    Render::Background buildBlockRect (uint32_t codepoint, int col, int row,
                                        const juce::Colour& foreground) const noexcept;

    /**
     * @brief Selects font style variant from cell SGR attributes.
     */
    static jam::Typeface::Style selectFontStyle (const Cell& cell) noexcept;

    /**
     * @brief Returns true if row r should be included in the current snapshot.
     *
     * GL path: always true. CPU path: only if row is dirty.
     */
    inline bool isRowIncludedInSnapshot (int r, const uint64_t* dirtyBits) const noexcept
    {
        bool result { true };

        if constexpr (std::is_same_v<Context, jam::Glyph::GraphicsContext>)
        {
            const int word { r >> 6 };
            const uint64_t bit { static_cast<uint64_t> (1) << (r & 63) };
            result = (dirtyBits[word] & bit) != 0;
        }

        return result;
    }

    // =========================================================================
    // Data
    // =========================================================================

    jam::Font& font;
    jam::Glyph::Packer& packer;

    int physCellWidth { 0 };
    int physCellHeight { 0 };
    int physBaseline { 0 };
    float baseFontSize { 14.0f };

    bool ligatureEnabled { false };
    int ligatureSkip { 0 };

    // Per-row render cache
    juce::HeapBlock<Render::Glyph> cachedMono;
    juce::HeapBlock<Render::Glyph> cachedEmoji;
    juce::HeapBlock<Render::Background> cachedBg;
    juce::HeapBlock<int> monoCount;
    juce::HeapBlock<int> emojiCount;
    juce::HeapBlock<int> bgCount;
    juce::HeapBlock<Terminal::Cell> previousCells;
    int cacheRows { 0 };
    int cacheCols { 0 };
    int bgCacheCols { 0 };
    int maxGlyphsPerRow { 0 };

    // Selection and overlays
    const ScreenSelection* selection { nullptr };
    const LinkManager* linkManager { nullptr };
    const LinkSpan* hintOverlay { nullptr };
    int hintOverlayCount { 0 };
    const LinkSpan* linkUnderlay { nullptr };
    int linkUnderlayCount { 0 };

    // Theme (non-owning pointer, set by Screen)
    const Theme* theme { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Glyph)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal::Renderer
