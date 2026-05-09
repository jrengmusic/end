/**
 * @file Cell.h
 * @brief Terminal cell supporting structures: RowState, getCellKey.
 *
 * The grid stores jam::Cell directly so that all colours are resolved ARGB
 * values at write time — no deferred palette lookup at render time.
 *
 * ### Cell memory layout (16 bytes, little-endian)
 * ```
 * Offset  Size  Field
 * ------  ----  -----
 *  0       4    codepoint  – Unicode scalar value (U+0000 … U+10FFFF)
 *  4       1    style      – SGR attribute bits (BOLD | ITALIC | …)
 *  5       1    layout     – Geometry flags (WIDE_CONT | EMOJI | GRAPHEME)
 *  6       1    width      – Display columns occupied (1 or 2)
 *  7       1    reserved   – Padding, always 0
 *  8       4    fg         – Foreground juce::Colour (ARGB, resolved at write time)
 * 12       4    bg         – Background juce::Colour (ARGB, resolved at write time)
 * ```
 *
 * ### Theme sentinel
 * A fully transparent colour (`juce::Colour{}`, alpha == 0) means "use theme
 * default".  The renderer checks `getAlpha() == 0` and substitutes the active
 * UI theme colour.  This sentinel is safe because a genuinely transparent cell
 * colour is functionally invisible and not a valid rendered state.
 *
 * ### Encoding rules
 * - A **normal** cell has `width == 1` and `layout == 0`.
 * - A **wide** (CJK / fullwidth) character occupies two columns.  The left
 *   column stores the codepoint with `width == 2`; the right column stores
 *   `codepoint == 0` with `LAYOUT_WIDE_CONT` set.
 * - An **emoji** cell has `LAYOUT_EMOJI` set.
 * - A **grapheme cluster** stores the base codepoint in `jam::Cell::codepoint`
 *   and sets `LAYOUT_GRAPHEME`.  Extra codepoints live in the grapheme side-table.
 *
 * @see jam::Cell     — canonical cell type (jam_pen.h)
 * @see jam::Grapheme — grapheme sidecar (jam_grapheme.h)
 */

#pragma once

#include <JuceHeader.h>
#include <array>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct RowState
 * @brief Per-row metadata flags packed into a single byte.
 *
 * Stored alongside each row in the screen buffer to track line-level
 * properties that are not per-cell.
 */
struct RowState
{
    /**
     * @brief Packed bit-field of row-level flags.
     *
     * Bit 1 (0x02) — double-width: each cell occupies two display columns
     *                (VT100 DECDWL mode).
     */
    uint8_t bits { 0 };

    /**
     * @brief Returns true when the row is rendered in VT100 double-width mode.
     * @return `true` if bit 1 of `bits` is set.
     * @note In double-width mode the renderer stretches each cell to two
     *       display columns; only the left half of the terminal is usable.
     */
    bool isDoubleWidth() const noexcept
    {
        return (bits & 0x02) != 0;
    }

    /**
     * @brief Sets or clears the double-width flag for this row.
     * @param value `true` to enable double-width rendering, `false` to clear.
     */
    void setDoubleWidth (bool value) noexcept
    {
        bits = static_cast<uint8_t> ((bits & ~0x02) | (static_cast<uint8_t> (value) << 1));
    }
};

static_assert (std::is_trivially_copyable_v<RowState>,
               "RowState must be trivially copyable");

/**
 * @brief Encodes a (row, col) grid coordinate into a 32-bit lookup key.
 *
 * Used as the key into the grapheme side-table: the upper 16 bits hold the
 * row index and the lower 16 bits hold the column index.
 *
 * @param row  Zero-based row index (0 … 65535).
 * @param col  Zero-based column index (0 … 65535).
 * @return A 32-bit key unique to the (row, col) pair within a 65536-column grid.
 * @note Both row and col are truncated to 16 bits; grids larger than 65535
 *       columns or rows are not supported.
 */
inline uint32_t getCellKey (int row, int col) noexcept
{
    return (static_cast<uint32_t> (row) << 16) | static_cast<uint32_t> (col);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
