/**
 * @file FontCollection.h
 * @brief O(1) codepoint-to-font-slot dispatch table for fallback font resolution.
 *
 * FontCollection maintains a flat `int8_t[0x110000]` lookup table that maps
 * every Unicode codepoint to the index of the font slot that can render it.
 * The table is populated once at font-load time by iterating each font's cmap
 * (via FreeType or CoreText), and thereafter every codepoint lookup is a single
 * array read — O(1) with no branching.
 *
 * ## Slot layout
 *
 * Up to 32 font slots are supported (`maxFonts`).  Slot 0 is conventionally
 * the identity / primary font.  Slots 1–31 are fallback fonts added in
 * priority order via `addFont()`.  `populateFromCmap()` fills the table for
 * one slot at a time; it only writes a slot index if the entry is still
 * `unresolved`, so earlier (higher-priority) slots win.
 *
 * ## Sentinel values
 *
 * | Value        | Meaning                                          |
 * |--------------|--------------------------------------------------|
 * | `unresolved` | Codepoint not yet assigned to any slot (-1)      |
 * | `notFound`   | Codepoint absent from all loaded fonts (-2)      |
 * | 0 … 31       | Index into `entries[]` for the owning font slot  |
 *
 * ## Platform backends
 *
 * - **macOS** (`FontCollection.mm`) — uses `CTFontCopyCharacterSet` +
 *   `CFCharacterSetIsLongCharacterMember` to enumerate supported codepoints.
 * - **Linux / Windows** (`FontCollection.cpp`) — uses `FT_Get_First_Char` /
 *   `FT_Get_Next_Char` to walk the FreeType cmap.
 *
 * ## Usage
 *
 * @code
 * FontCollection fc;
 * fc.addFont ({ ctFont, hbFont, false });   // slot 0
 * fc.populateFromCmap (0);
 *
 * int8_t slot = fc.resolve (0x1F600);       // emoji codepoint
 * if (slot >= 0)
 * {
 *     const FontCollection::Entry* e = fc.getEntry (slot);
 *     // use e->hbFont for shaping
 * }
 * @endcode
 *
 * @note FontCollection is a `jreng::Context<FontCollection>` singleton.
 *       Retrieve the live instance via `FontCollection::getContext()`.
 *
 * @see Fonts
 * @see GlyphAtlas
 */

#pragma once

#include <JuceHeader.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <array>

/**
 * @struct FontCollection
 * @brief Flat-array codepoint-to-font-slot resolver with O(1) lookup.
 *
 * Owns a 1 MiB `int8_t` heap block indexed by Unicode codepoint.  Each byte
 * holds the slot index of the font that covers that codepoint, or a sentinel
 * value (`unresolved` / `notFound`).  Up to 32 font slots are stored in a
 * fixed-size `std::array<Entry, maxFonts>`.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all methods must be called from the JUCE message thread.
 * No internal locking is performed.
 *
 * @see Fonts
 * @see GlyphAtlas
 */
struct FontCollection : jreng::Context<FontCollection>
{
    // =========================================================================
    // Inner types
    // =========================================================================

    /**
     * @struct Entry
     * @brief A single font slot: platform handle + HarfBuzz shaping font.
     *
     * On macOS the platform handle is a `CTFontRef` stored as `void*`.
     * On other platforms it is an `FT_Face`.  Both are owned externally
     * (by `Fonts`) and must outlive the `FontCollection`.
     */
    struct Entry
    {
#if JUCE_MAC
        void* ctFont { nullptr };       ///< CoreText font reference (macOS only); nullptr if not loaded.
#else
        FT_Face ftFace { nullptr };     ///< FreeType face handle (non-macOS); nullptr if not loaded.
#endif
        hb_font_t* hbFont { nullptr };  ///< HarfBuzz shaping font wrapping the platform handle; nullptr if not loaded.
        bool hasColorGlyphs { false };  ///< True if this font contains COLR/CBDT color glyph tables (e.g. emoji).
    };

    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr int maxFonts    { 32 };       ///< Maximum number of font slots (indices 0–31).
    static constexpr int unresolved  { -1 };       ///< Sentinel: codepoint not yet assigned to any slot.
    static constexpr int notFound    { -2 };       ///< Sentinel: codepoint absent from all loaded fonts.
    static constexpr int UNICODE_MAX { 0x110000 }; ///< One past the last valid Unicode codepoint.

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Constructs the collection and allocates the lookup table.
     *
     * Allocates a `UNICODE_MAX`-byte heap block and fills it with `unresolved`
     * (-1) so that every codepoint starts in the unresolved state.
     */
    FontCollection();

    /**
     * @brief Destroys the collection.
     *
     * The `HeapBlock` is freed automatically.  Platform font handles stored in
     * `entries` are owned by `Fonts` and are not released here.
     */
    ~FontCollection() override;

    // =========================================================================
    // Lookup
    // =========================================================================

    /**
     * @brief Resolves a Unicode codepoint to a font slot index.
     *
     * Performs a single array read into the lookup table.  Returns the slot
     * index of the font that covers @p codepoint, or a sentinel value if the
     * codepoint is out of range or not covered by any loaded font.
     *
     * @param codepoint  Unicode scalar value to look up (0 … 0x10FFFF).
     * @return           Slot index (0–31) if covered; `unresolved` (-1) if the
     *                   table has not been populated for this codepoint; or
     *                   `notFound` (-2) if @p codepoint ≥ `UNICODE_MAX`.
     *
     * @note This method is O(1) and safe to call on the hot rendering path.
     */
    int8_t resolve (uint32_t codepoint) const noexcept;

    // =========================================================================
    // Mutation
    // =========================================================================

    /**
     * @brief Appends a font entry to the next available slot.
     *
     * Stores @p entry at `entries[entryCount]` and increments `entryCount`.
     * Does nothing if `entryCount` has already reached `maxFonts`.
     *
     * @param entry  Font entry to add.  The platform handle and `hbFont`
     *               pointer must remain valid for the lifetime of this
     *               `FontCollection`.
     *
     * @note Call `populateFromCmap()` after adding each entry to register its
     *       codepoint coverage in the lookup table.
     */
    void addFont (Entry entry) noexcept;

    /**
     * @brief Populates the lookup table from the cmap of one font slot.
     *
     * Iterates every codepoint covered by the font at @p slotIndex and writes
     * @p slotIndex into the lookup table for each codepoint that is still
     * `unresolved`.  Already-resolved entries are left unchanged, so
     * higher-priority (lower-index) slots win.
     *
     * @par Platform dispatch
     * - **macOS** (`FontCollection.mm`): uses `CTFontCopyCharacterSet` +
     *   `CFCharacterSetIsLongCharacterMember`.
     * - **Other** (`FontCollection.cpp`): uses `FT_Get_First_Char` /
     *   `FT_Get_Next_Char`.
     *
     * @param slotIndex  Index of the slot to populate (0 … `entryCount - 1`).
     *                   Out-of-range values are silently ignored.
     *
     * @note This is an O(n) operation over the font's cmap and should only be
     *       called during font loading, not on the render path.
     */
    void populateFromCmap (int slotIndex) noexcept;

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Returns a mutable pointer to the entry at @p slotIndex.
     *
     * @param slotIndex  Slot index to retrieve (0 … `entryCount - 1`).
     * @return           Pointer to the `Entry`, or `nullptr` if @p slotIndex
     *                   is out of range.
     */
    Entry* getEntry (int slotIndex) noexcept;

    /**
     * @brief Returns a read-only pointer to the entry at @p slotIndex.
     *
     * @param slotIndex  Slot index to retrieve (0 … `entryCount - 1`).
     * @return           Const pointer to the `Entry`, or `nullptr` if
     *                   @p slotIndex is out of range.
     */
    const Entry* getEntry (int slotIndex) const noexcept;

    /**
     * @brief Returns the number of font slots currently registered.
     *
     * @return Number of entries added via `addFont()` (0 … `maxFonts`).
     */
    int getCount() const noexcept { return entryCount; }

private:
    // =========================================================================
    // Data
    // =========================================================================

    juce::HeapBlock<int8_t>        lookupTable; ///< Flat codepoint→slot table; UNICODE_MAX bytes.
    std::array<Entry, maxFonts>    entries;     ///< Fixed-size array of font slot entries.
    int                            entryCount { 0 }; ///< Number of valid entries in @p entries.
};
