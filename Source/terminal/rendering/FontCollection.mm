/**
 * @file FontCollection.mm
 * @brief FontCollection::populateFromCmap — CoreText backend (macOS only).
 *
 * This Objective-C++ translation unit provides the macOS implementation of
 * `populateFromCmap()`.  It is compiled only when `JUCE_MAC` is defined; the
 * FreeType equivalent lives in `FontCollection.cpp`.
 *
 * ## Algorithm
 *
 * CoreText exposes a font's full Unicode coverage as a `CFCharacterSetRef`
 * via `CTFontCopyCharacterSet()`.  This implementation iterates every
 * codepoint in the range [0, `UNICODE_MAX`) and tests membership with
 * `CFCharacterSetIsLongCharacterMember()`.  For each covered codepoint that
 * is still `unresolved` in the lookup table, the slot index is written.
 *
 * @note The `CFCharacterSetRef` is released via `CFRelease()` after the scan
 *       to avoid a memory leak.
 *
 * @see FontCollection.h
 * @see FontCollection.cpp  — FreeType equivalent for non-macOS platforms.
 */

#include "FontCollection.h"

#if JUCE_MAC

#include <CoreText/CoreText.h>

/**
 * @brief Populates the lookup table from a CoreText character set (macOS).
 *
 * Copies the character set from the `CTFontRef` stored in
 * `entries[slotIndex]` and iterates all Unicode codepoints [0, `UNICODE_MAX`).
 * For each codepoint that is a member of the character set and is still
 * `unresolved` in the lookup table, writes @p slotIndex.  Already-resolved
 * entries are left unchanged so that higher-priority (lower-index) slots win.
 *
 * @param slotIndex  Index of the slot to populate (0 … `entryCount - 1`).
 *                   Out-of-range values are silently ignored.
 *
 * @note `CTFontCopyCharacterSet` returns a retained `CFCharacterSetRef` that
 *       must be released with `CFRelease()`.  This function releases it before
 *       returning.
 *
 * @note This is an O(UNICODE_MAX) scan (~1 M iterations) and should only be
 *       called during font loading, not on the render path.
 *
 * @see FontCollection.cpp  — FreeType equivalent for non-macOS platforms.
 */
void FontCollection::populateFromCmap (int slotIndex) noexcept
{
    if (slotIndex >= 0 and slotIndex < entryCount)
    {
        const Entry& entry { entries.at (static_cast<size_t> (slotIndex)) };

        if (entry.ctFont != nullptr)
        {
            CTFontRef font { static_cast<CTFontRef> (entry.ctFont) };
            CFCharacterSetRef charSet { CTFontCopyCharacterSet (font) };

            if (charSet != nullptr)
            {
                for (uint32_t cp { 0 }; cp < static_cast<uint32_t> (UNICODE_MAX); ++cp)
                {
                    if (CFCharacterSetIsLongCharacterMember (charSet, cp))
                    {
                        if (lookupTable[cp] == unresolved)
                        {
                            lookupTable[cp] = static_cast<int8_t> (slotIndex);
                        }
                    }
                }

                CFRelease (charSet);
            }
        }
    }
}

#endif
