/**
 * @file Charset.h
 * @brief Character set tables for VT terminal emulation.
 *
 * This file provides character set mappings used by the VT100/VT220 terminal
 * emulation to translate bytes in the 0x60-0x7E range to Unicode characters.
 * The DEC (Digital Equipment Corporation) terminal standard defines multiple
 * character sets (G0-G3) that can be designated and switched at runtime.
 *
 * Character Set Designation Sequences:
 * - ESC ( B - Designate G0 as US ASCII
 * - ESC ( 0 - Designate G0 as DEC Special Graphics (line drawing)
 * - ESC ) B - Designate G1 as US ASCII
 * - ESC ) 0 - Designate G1 as DEC Special Graphics
 *
 * The parser uses the lineDrawing flag to determine whether to apply
 * the DEC Line Drawing mapping when processing characters in the 0x60-0x7E range.
 *
 * @see Parser.h for character set switching implementation
 * @see VT220 Terminal Emulation specification
 */

#pragma once

#include <array>
#include <cstdint>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief DEC Special Character and Line Drawing Set (VT100).
 *
 * This table maps bytes 0x60-0x7E (96-126 decimal) to Unicode box-drawing
 * characters and symbols used by the VT100 terminal for drawing lines,
 * corners, and special graphical elements.
 *
 * Activated by ESC ( 0 (designate G0 as DEC Special Graphics)
 * Deactivated by ESC ( B (designate G0 as US ASCII)
 *
 * Mapping table index: codepoint - 0x60
 *
 * Example characters:
 * - 0x60 (`) -> ▒ (U+2592) - Shade
 * - 0x6A (j) -> ┘ (U+2518) - Lower right corner
 * - 0x6C (l) -> ┌ (U+250C) - Upper left corner
 * - 0x71 (q) -> ─ (U+2500) - Horizontal line
 */
inline constexpr std::array<uint32_t, 32> DEC_LINE_DRAWING
{
    0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0, 0x00B1, 0x2424,
    0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA, 0x23BB,
    0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C, 0x2502,
    0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7, 0x0020, 0x0020
};

/**
 * @brief Translates a codepoint using the active character set.
 *
 * This function applies character set mapping when line drawing mode is active.
 * When lineDrawing is true and the codepoint falls within 0x60-0x7E, the
 * corresponding DEC Special Graphics character is returned.
 *
 * @param codepoint The Unicode codepoint to translate (input byte value)
 * @param lineDrawing True if DEC Line Drawing mode is active
 * @return The translated codepoint (may be unchanged if not in mapped range)
 *
 * @note Thread safety: This function is constexpr and has no side effects.
 *       Safe to call from any thread context.
 *
 * @par Example
 * @code
 * uint32_t c1 = translateCharset(0x6A, false); // Returns 0x6A ('j')
 * uint32_t c2 = translateCharset(0x6A, true);  // Returns 0x2518 (┘)
 * @endcode
 */
inline uint32_t translateCharset (uint32_t codepoint, bool lineDrawing) noexcept
{
    uint32_t result { codepoint };

    if (lineDrawing and codepoint >= 0x60 and codepoint <= 0x7E)
    {
        result = DEC_LINE_DRAWING.at (codepoint - 0x60);
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
