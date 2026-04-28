/**
 * @file Cell.h
 * @brief Core terminal cell data structures: Cell, Grapheme, Pen, RowState.
 *
 * A terminal screen is a 2-D grid of Cell values.  Each Cell is exactly
 * 16 bytes and trivially copyable, so the entire grid can be copied with a
 * single memcpy and stored in a juce::MemoryBlock without serialisation.
 *
 * ### Cell memory layout (16 bytes, little-endian)
 * ```
 * Offset  Size  Field
 * ------  ----  -----
 *  0       4    codepoint  – Unicode scalar value (U+0000 … U+10FFFF)
 *  4       1    style      – SGR attribute bits (BOLD | ITALIC | …)
 *  5       1    layout     – Geometry flags (WIDE_CONT | EMOJI | GRAPHEME | HYPERLINK)
 *  6       1    width      – Display columns occupied (1 or 2)
 *  7       1    reserved   – Padding, always 0
 *  8       4    fg         – Foreground Color (see Color.h)
 * 12       4    bg         – Background Color (see Color.h)
 * ```
 *
 * ### Encoding rules
 * - A **normal** cell has `width == 1` and `layout == 0`.
 * - A **wide** (CJK / fullwidth) character occupies two columns.  The left
 *   column stores the codepoint with `width == 2`; the right column stores
 *   `codepoint == 0` with `LAYOUT_WIDE_CONT` set.
 * - An **emoji** cell has `LAYOUT_EMOJI` set.  The renderer may apply
 *   colour-emoji font selection for this cell.
 * - A **grapheme cluster** (base + combining marks) stores the base codepoint
 *   in `Cell::codepoint` and sets `LAYOUT_GRAPHEME`.  The extra codepoints
 *   are stored in a parallel `Grapheme` entry keyed by `getCellKey()`.
 */

#pragma once

#include <JuceHeader.h>
#include <array>
#include "Color.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct Cell
 * @brief A single terminal grid cell — 16 bytes, trivially copyable.
 *
 * Cell is the atomic unit of the terminal screen buffer.  Its fixed size and
 * trivial-copyability allow the renderer to diff rows with memcmp and copy
 * entire screens with memcpy.
 *
 * @note `sizeof(Cell) == 16` and `std::is_trivially_copyable_v<Cell>` are
 *       enforced by static_assert immediately after the struct definition.
 */
struct Cell
{
    /** @name Style bit-flags (stored in Cell::style)
     *  Correspond to ANSI/VT SGR (Select Graphic Rendition) attributes.
     *  Multiple flags may be OR-ed together.
     * @{ */

    /** SGR 1 — render text with a heavier stroke weight. */
    static constexpr uint8_t BOLD      { 0x01 };

    /** SGR 3 — render text in an oblique / italic variant. */
    static constexpr uint8_t ITALIC    { 0x02 };

    /** SGR 4 — draw a line beneath the glyph baseline. */
    static constexpr uint8_t UNDERLINE { 0x04 };

    /** SGR 9 — draw a horizontal line through the glyph midpoint. */
    static constexpr uint8_t STRIKE    { 0x08 };

    /** SGR 5 — the cell should blink at the terminal's blink rate. */
    static constexpr uint8_t BLINK     { 0x10 };

    /** SGR 7 — swap foreground and background colours when rendering. */
    static constexpr uint8_t INVERSE   { 0x20 };

    /** SGR 2 — render text with reduced intensity (dim / faint). */
    static constexpr uint8_t DIM       { 0x40 };

    /** @} */

    /** @name Data members
     * @{ */

    /**
     * @brief Unicode scalar value of the primary codepoint.
     *
     * Valid range: U+0000 … U+10FFFF.  A value of 0 means the cell is empty
     * (space / blank).  For grapheme clusters, this holds the base character;
     * combining marks are stored in the associated Grapheme entry.
     */
    uint32_t codepoint { 0 };

    /**
     * @brief SGR attribute bit-field.
     *
     * A bitmask of the BOLD, ITALIC, UNDERLINE, STRIKE, BLINK, and INVERSE
     * constants defined above.  Use the `is*()` accessors rather than
     * testing bits directly.
     */
    uint8_t style { 0 };

    /**
     * @brief Geometry / cluster bit-field.
     *
     * A bitmask of the LAYOUT_WIDE_CONT, LAYOUT_EMOJI, LAYOUT_GRAPHEME, and
     * LAYOUT_HYPERLINK constants defined below.  Controls how the renderer
     * measures and draws this cell.
     */
    uint8_t layout { 0 };

    /**
     * @brief Number of terminal columns this cell occupies (1 or 2).
     *
     * Fullwidth (CJK) characters set this to 2 on the left column.  The
     * right continuation column stores 0 here and sets LAYOUT_WIDE_CONT.
     * All other cells use the default value of 1.
     */
    uint8_t width { 1 };

    /**
     * @brief Reserved padding byte — always 0.
     *
     * Ensures `sizeof(Cell) == 16` and provides a future-extension slot.
     * Must not be written by any code path other than zero-initialisation.
     */
    uint8_t reserved { 0 };

    /**
     * @brief Foreground (text) colour.
     *
     * Interpreted after applying the INVERSE flag: when INVERSE is set the
     * renderer swaps fg and bg at draw time without mutating the stored values.
     */
    Color fg;

    /**
     * @brief Background (fill) colour.
     *
     * See fg for INVERSE semantics.
     */
    Color bg;

    /** @} */

    /** @name Style accessors
     * @{ */

    /**
     * @brief Returns true when the cell holds a visible character.
     * @return `true` if `codepoint != 0`.
     * @note A cell with `codepoint == 0` is treated as an empty space by the
     *       renderer regardless of the style or colour fields.
     */
    bool hasContent() const noexcept
    {
        return codepoint != 0;
    }

    /**
     * @brief Returns true when the BOLD style flag is set.
     * @return `true` if the BOLD bit is set in `style`.
     */
    bool isBold() const noexcept
    {
        return (style & BOLD) != 0;
    }

    /**
     * @brief Returns true when the ITALIC style flag is set.
     * @return `true` if the ITALIC bit is set in `style`.
     */
    bool isItalic() const noexcept
    {
        return (style & ITALIC) != 0;
    }

    /**
     * @brief Returns true when the UNDERLINE style flag is set.
     * @return `true` if the UNDERLINE bit is set in `style`.
     */
    bool isUnderline() const noexcept
    {
        return (style & UNDERLINE) != 0;
    }

    /**
     * @brief Returns true when the STRIKE (strikethrough) style flag is set.
     * @return `true` if the STRIKE bit is set in `style`.
     */
    bool isStrike() const noexcept
    {
        return (style & STRIKE) != 0;
    }

    /**
     * @brief Returns true when the BLINK style flag is set.
     * @return `true` if the BLINK bit is set in `style`.
     */
    bool isBlink() const noexcept
    {
        return (style & BLINK) != 0;
    }

    /**
     * @brief Returns true when the INVERSE (reverse video) style flag is set.
     * @return `true` if the INVERSE bit is set in `style`.
     * @note The renderer swaps fg/bg at draw time; the stored colour values
     *       are never mutated by inversion.
     */
    bool isInverse() const noexcept
    {
        return (style & INVERSE) != 0;
    }

    /**
     * @brief Returns true when the DIM (faint) style flag is set.
     * @return `true` if the DIM bit is set in `style`.
     */
    bool isDim() const noexcept
    {
        return (style & DIM) != 0;
    }

    /** @} */

    /** @name Layout bit-flags (stored in Cell::layout)
     * @{ */

    /**
     * @brief Marks the right-hand continuation column of a wide character.
     *
     * When a fullwidth codepoint is written, the left column stores the
     * codepoint with `width == 2` and the right column stores `codepoint == 0`
     * with this flag set.  The renderer skips continuation cells.
     */
    static constexpr uint8_t LAYOUT_WIDE_CONT { 0x01 };

    /**
     * @brief Marks the cell as containing an emoji codepoint.
     *
     * Signals the renderer to prefer a colour-emoji font for this cell.
     * May be combined with LAYOUT_GRAPHEME when the emoji is a cluster
     * (e.g. base + variation selector + ZWJ sequence).
     */
    static constexpr uint8_t LAYOUT_EMOJI    { 0x04 };

    /**
     * @brief Marks the cell as the head of a grapheme cluster.
     *
     * When set, the cell's extra combining codepoints are stored in a
     * Grapheme entry retrieved via getCellKey (row, col).  The base
     * codepoint remains in Cell::codepoint.
     */
    static constexpr uint8_t LAYOUT_GRAPHEME { 0x08 };

    /** @brief Layout flag: this cell carries an OSC 8 hyperlink (linkId in sidecar). */
    static constexpr uint8_t LAYOUT_HYPERLINK { 0x20 };

    /** @} */

    /** @name Layout accessors
     * @{ */

    /**
     * @brief Returns true when this cell is the right-hand half of a wide character.
     * @return `true` if LAYOUT_WIDE_CONT is set in `layout`.
     * @note The renderer must skip this cell; its visual content is owned by
     *       the preceding column.
     */
    bool isWideContinuation() const noexcept
    {
        return (layout & LAYOUT_WIDE_CONT) != 0;
    }

    /**
     * @brief Returns true when the cell contains an emoji codepoint.
     * @return `true` if LAYOUT_EMOJI is set in `layout`.
     */
    bool isEmoji() const noexcept
    {
        return (layout & LAYOUT_EMOJI) != 0;
    }

    /**
     * @brief Returns true when the cell is the head of a grapheme cluster.
     * @return `true` if LAYOUT_GRAPHEME is set in `layout`.
     * @note When true, look up the associated Grapheme via getCellKey() to
     *       obtain the full sequence of codepoints.
     */
    bool hasGrapheme() const noexcept
    {
        return (layout & LAYOUT_GRAPHEME) != 0;
    }

    /** @brief True if this cell carries an OSC 8 hyperlink. */
    bool hasHyperlink() const noexcept { return (layout & LAYOUT_HYPERLINK) != 0; }

    /** @} */
};

static_assert (std::is_trivially_copyable_v<Cell>, "Cell must be trivially copyable");
static_assert (sizeof (Cell) == 16, "Cell must be 16 bytes");

/**
 * @struct Grapheme
 * @brief Extra codepoints for a grapheme cluster whose head is stored in a Cell.
 *
 * Unicode grapheme clusters can consist of a base character followed by one or
 * more combining marks (e.g. emoji with skin-tone modifier + ZWJ + another
 * emoji).  When a Cell has `LAYOUT_GRAPHEME` set, the renderer fetches the
 * corresponding Grapheme from the grid's grapheme side-table using
 * `getCellKey (row, col)` as the key.
 *
 * The base codepoint is always in `Cell::codepoint`; `extraCodepoints` holds
 * only the combining / joining codepoints that follow it.
 *
 * @note Trivially copyable so the side-table can be stored in a MemoryBlock.
 */
struct Grapheme
{
    /**
     * @brief Up to 7 additional Unicode codepoints following the base character.
     *
     * Unused slots are zero-filled.  The number of valid entries is given by
     * `count`.  Sequences longer than 7 combining codepoints are truncated
     * (extremely rare in practice).
     */
    std::array<uint32_t, 7> extraCodepoints { 0, 0, 0, 0, 0, 0, 0 };

    /**
     * @brief Number of valid entries in `extraCodepoints` (0 … 7).
     *
     * A value of 0 means the cluster consists solely of the base codepoint
     * stored in the associated Cell — the Grapheme entry is effectively empty.
     */
    uint8_t count { 0 };
};

static_assert (std::is_trivially_copyable_v<Grapheme>, "Grapheme must be trivially copyable");


/**
 * @struct Pen
 * @brief Current drawing state applied to newly written cells.
 *
 * The Pen is the terminal's "current attribute" register.  When the parser
 * writes a character to the grid it stamps the active Pen's style, fg, and bg
 * onto the destination Cell.  SGR escape sequences mutate the Pen; they do not
 * retroactively alter already-written cells.
 *
 * @note Trivially copyable so it can be saved/restored in a MemoryBlock
 *       (e.g. for DECSC / DECRC cursor-save sequences).
 */
struct Pen
{
    /**
     * @brief Active SGR attribute bitmask.
     *
     * Uses the same bit constants as Cell (BOLD, ITALIC, UNDERLINE, STRIKE,
     * BLINK, INVERSE).  Copied verbatim into Cell::style when a character is
     * written.
     */
    uint8_t style { 0 };

    /**
     * @brief Active foreground colour.
     *
     * Copied into Cell::fg when a character is written.
     */
    Color fg;

    /**
     * @brief Active background colour.
     *
     * Copied into Cell::bg when a character is written.
     */
    Color bg;
};

static_assert (std::is_trivially_copyable_v<Pen>,
               "Pen must be trivially copyable for MemoryBlock storage");

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
     * Bit 0 (0x01) — wrapped: the logical line continues on the next row.\n
     * Bit 1 (0x02) — double-width: each cell occupies two display columns
     *                (VT100 DECDWL mode).
     */
    uint8_t bits { 0 };

    /**
     * @brief Returns true when this row is a soft-wrapped continuation.
     * @return `true` if bit 0 of `bits` is set.
     * @note A wrapped row must not be broken by a newline when reflowing text.
     */
    bool isWrapped() const noexcept
    {
        return (bits & 0x01) != 0;
    }

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
     * @brief Sets or clears the soft-wrap flag for this row.
     * @param value `true` to mark the row as wrapped, `false` to clear.
     */
    void setWrapped (bool value) noexcept
    {
        bits = static_cast<uint8_t> ((bits & ~0x01) | (static_cast<uint8_t> (value) << 0));
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
