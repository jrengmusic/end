/**
 * @file jreng_font_registry.cpp
 * @brief Font::Registry implementation — lifecycle, lookup, and FreeType cmap population.
 *
 * This translation unit provides:
 * - Constructor / destructor (platform-agnostic).
 * - `resolve()` — O(1) codepoint-to-slot lookup.
 * - `addFont()` / `getEntry()` — slot management.
 * - `populateFromCmap()` — FreeType backend (non-macOS).
 *
 * On macOS the `populateFromCmap()` implementation lives in
 * `jreng_font_registry.mm` and uses CoreText instead of FreeType.
 *
 * @see jreng_font.h
 * @see jreng_font_registry.mm
 */

// Included via unity build (jreng_glyph.cpp) — jreng_glyph.h already in scope

// MESSAGE THREAD

namespace jreng
{ /*____________________________________________________________________________*/

/**
 * @brief Allocates and initialises the lookup table.
 *
 * Calls `calloc` to zero-allocate `unicodeMax` bytes, then fills the block
 * with the `unresolved` sentinel (-1) via `memset`.  The zero-then-fill
 * pattern ensures the OS has committed the pages before the memset runs.
 */
Font::Registry::Registry()
{
    lookupTable.calloc (static_cast<size_t> (unicodeMax));
    memset (lookupTable.get(), unresolved, static_cast<size_t> (unicodeMax));
}

/**
 * @brief Destroys the registry.
 *
 * `HeapBlock` frees the lookup table automatically.  Platform font handles
 * stored in `entries` are owned by `Font` and are not released here.
 */
Font::Registry::~Registry()
{
}

/**
 * @brief Resolves a codepoint to a font slot index.
 *
 * Performs a bounds check then a single array read.  Returns `notFound` for
 * any codepoint >= `unicodeMax` (i.e. out of the valid Unicode range).
 *
 * @param codepoint  Unicode scalar value (0 … 0x10FFFF).
 * @return           Slot index (0–31), `unresolved` (-1), or `notFound` (-2).
 */
int8_t Font::Registry::resolve (uint32_t codepoint) const noexcept
{
    int8_t result { notFound };

    if (codepoint < static_cast<uint32_t> (unicodeMax))
    {
        result = lookupTable[codepoint];
    }

    return result;
}

/**
 * @brief Appends a font entry to the next available slot.
 *
 * Stores @p entry at `entries[entryCount]` and increments `entryCount`.
 * Silently drops the entry if `entryCount` has already reached `maxFonts`.
 *
 * @param entry  Font entry to register.  The platform handle and `hbFont`
 *               must remain valid for the lifetime of this `Registry`.
 */
void Font::Registry::addFont (Entry entry) noexcept
{
    if (entryCount < maxFonts)
    {
        entries.at (static_cast<size_t> (entryCount)) = entry;
        ++entryCount;
    }
}

/**
 * @brief Returns a mutable pointer to the entry at @p slotIndex.
 *
 * @param slotIndex  Slot index to retrieve (0 … `entryCount - 1`).
 * @return           Pointer to the `Entry`, or `nullptr` if out of range.
 */
Font::Registry::Entry* Font::Registry::getEntry (int slotIndex) noexcept
{
    Entry* result { nullptr };

    if (slotIndex >= 0 and slotIndex < entryCount)
    {
        result = &entries.at (static_cast<size_t> (slotIndex));
    }

    return result;
}

/**
 * @brief Returns a read-only pointer to the entry at @p slotIndex.
 *
 * @param slotIndex  Slot index to retrieve (0 … `entryCount - 1`).
 * @return           Const pointer to the `Entry`, or `nullptr` if out of range.
 */
const Font::Registry::Entry* Font::Registry::getEntry (int slotIndex) const noexcept
{
    const Entry* result { nullptr };

    if (slotIndex >= 0 and slotIndex < entryCount)
    {
        result = &entries.at (static_cast<size_t> (slotIndex));
    }

    return result;
}

#if JUCE_MAC
// CoreText implementation in jreng_font_registry.mm
#else

/**
 * @brief Populates the lookup table from a FreeType cmap (non-macOS).
 *
 * Walks the cmap of the `FT_Face` stored in `entries[slotIndex]` using
 * `FT_Get_First_Char` / `FT_Get_Next_Char`.  For each codepoint that maps to
 * a non-zero glyph index, writes @p slotIndex into the lookup table if the
 * entry is still `unresolved`.  Already-resolved entries are left unchanged so
 * that higher-priority (lower-index) slots win.
 *
 * @param slotIndex  Index of the slot to populate (0 … `entryCount - 1`).
 *                   Out-of-range values are silently ignored.
 *
 * @note This is an O(n) scan over the font's cmap and should only be called
 *       during font loading, not on the render path.
 *
 * @see jreng_font_registry.mm  — CoreText equivalent for macOS.
 */
void Font::Registry::populateFromCmap (int slotIndex) noexcept
{
    if (slotIndex >= 0 and slotIndex < entryCount)
    {
        const Entry& entry { entries.at (static_cast<size_t> (slotIndex)) };

        if (entry.ftFace != nullptr)
        {
            FT_Face face { entry.ftFace };
            FT_ULong charcode { 0 };
            FT_UInt glyphIndex { 0 };

            charcode = FT_Get_First_Char (face, &glyphIndex);

            while (glyphIndex != 0)
            {
                if (charcode < static_cast<FT_ULong> (unicodeMax))
                {
                    if (lookupTable[charcode] == unresolved)
                    {
                        lookupTable[charcode] = static_cast<int8_t> (slotIndex);
                    }
                }

                charcode = FT_Get_Next_Char (face, charcode, &glyphIndex);
            }
        }
    }
}

#endif

} // namespace jreng
