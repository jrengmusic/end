/**
 * @file ParserSGR.cpp
 * @brief SGR (Select Graphic Rendition) dispatch for the VT terminal parser.
 *
 * This file implements the SGR subsystem of the Parser class.  SGR sequences
 * (`ESC [ <params> m`) control all visual text attributes: style flags (bold,
 * italic, underline, blink, inverse, strike) and foreground/background colors
 * (ANSI 16-color palette, 256-color palette, and 24-bit RGB).
 *
 * ## SGR parameter table
 *
 * | Code(s)     | Effect                                      |
 * |-------------|---------------------------------------------|
 * | 0           | Reset all attributes to default             |
 * | 1           | Bold (increased intensity)                  |
 * | 2           | Dim (decreased intensity / faint)           |
 * | 3           | Italic                                      |
 * | 4           | Underline                                   |
 * | 5           | Blink (slow)                                |
 * | 6           | Blink (rapid) — treated as slow blink       |
 * | 7           | Inverse (swap fg/bg)                        |
 * | 9           | Strikethrough                               |
 * | 21          | Doubly underlined / bold off (xterm)        |
 * | 22          | Normal intensity (bold off)                 |
 * | 23          | Italic off                                  |
 * | 24          | Underline off                               |
 * | 25          | Blink off                                   |
 * | 27          | Inverse off                                 |
 * | 29          | Strikethrough off                           |
 * | 30–37       | Set foreground: ANSI colors 0–7             |
 * | 38;5;N      | Set foreground: 256-color palette index N   |
 * | 38;2;R;G;B  | Set foreground: 24-bit RGB                  |
 * | 39          | Reset foreground to default                 |
 * | 40–47       | Set background: ANSI colors 0–7             |
 * | 48;5;N      | Set background: 256-color palette index N   |
 * | 48;2;R;G;B  | Set background: 24-bit RGB                  |
 * | 49          | Reset background to default                 |
 * | 90–97       | Set foreground: bright ANSI colors 8–15     |
 * | 100–107     | Set background: bright ANSI colors 8–15     |
 *
 * ## Extended color encoding
 *
 * Two syntaxes are accepted for 256-color and 24-bit RGB colors:
 *
 * @par Semicolon syntax (ECMA-48 / xterm)
 * @code
 * ESC [ 38 ; 5 ; N m          — 256-color palette, index N
 * ESC [ 38 ; 2 ; R ; G ; B m  — 24-bit RGB
 * @endcode
 *
 * @par Colon sub-separator syntax (ISO 8613-6 / ITU T.416)
 * @code
 * ESC [ 38 : 5 : N m          — 256-color palette, index N
 * ESC [ 38 : 2 : : R : G : B m — 24-bit RGB (empty slot for color space ID)
 * @endcode
 *
 * Both syntaxes are handled by `parseExtendedColor()`, which reads the
 * sub-type from `params.values[i+1]` and advances the index accordingly.
 *
 * @note All functions in this file run on the READER THREAD only.
 *
 * @see Pen    — the drawing attribute struct mutated by SGR
 * @see Color  — color representation (palette index vs. 24-bit RGB)
 * @see CSI    — parameter accumulator supplying the SGR parameter list
 * @see Cell   — style bit flags (BOLD, ITALIC, UNDERLINE, BLINK, INVERSE, STRIKE)
 */

#include "Parser.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Parses an extended color sub-sequence (256-palette or 24-bit RGB).
 *
 * Called when the SGR loop encounters code 38 (foreground) or 48 (background).
 * Reads the sub-type from `params.values[i+1]` and constructs the appropriate
 * Color value, advancing `i` past the consumed sub-parameters.
 *
 * @par Sub-type 5 — 256-color palette
 * @code
 * // ESC [ 38 ; 5 ; N m
 * // params.values: [38, 5, N, ...]
 * //                  i  i+1 i+2
 * // Consumes i+1 and i+2; advances i by 2.
 * @endcode
 *
 * @par Sub-type 2 — 24-bit RGB
 * @code
 * // ESC [ 38 ; 2 ; R ; G ; B m
 * // params.values: [38, 2, R, G, B, ...]
 * //                  i  i+1 i+2 i+3 i+4
 * // Consumes i+1 through i+4; advances i by 4.
 * @endcode
 *
 * @param params  The full CSI parameter set for the current SGR sequence.
 * @param i       Index of the 38/48 code in `params.values`.  Updated in-place
 *                to point past the last consumed sub-parameter so the caller's
 *                loop can continue from the correct position.
 *
 * @return A Color value with the appropriate mode tag set:
 *         - `Color::palette` for 256-color (sub-type 5)
 *         - `Color::rgb`     for 24-bit RGB (sub-type 2)
 *         - Default-constructed `Color {}` if the sub-type is unrecognised
 *           or there are insufficient parameters.
 *
 * @note READER THREAD only.
 *
 * @see Color::palette
 * @see Color::rgb
 * @see handleSGR()
 */
static Color parseExtendedColor (const CSI& params, uint8_t& i) noexcept
{
    Color result {};

    if (i + 1 < params.count)
    {
        const auto subType { params.values.at (i + 1) };

        if (subType == 5 and i + 2 < params.count)
        {
            result = Color { static_cast<uint8_t> (params.values.at (i + 2)), 0, 0, Color::palette };
            i += 2;
        }
        else if (subType == 2 and i + 4 < params.count)
        {
            result = Color { static_cast<uint8_t> (params.values.at (i + 2)),
                             static_cast<uint8_t> (params.values.at (i + 3)),
                             static_cast<uint8_t> (params.values.at (i + 4)),
                             Color::rgb };
            i += 4;
        }
    }

    return result;
}

namespace
{
    /**
     * @brief Sets a style bit on the pen for SGR attribute-on codes (1–9).
     *
     * Maps the raw SGR code to the corresponding `Cell` style flag and ORs it
     * into `p.style`.  Codes not listed in the switch are silently ignored.
     *
     * | Code | Attribute set         |
     * |------|-----------------------|
     * | 1    | `Cell::BOLD`          |
     * | 3    | `Cell::ITALIC`        |
     * | 4    | `Cell::UNDERLINE`     |
     * | 5    | `Cell::BLINK`         |
     * | 6    | `Cell::BLINK` (rapid) |
     * | 7    | `Cell::INVERSE`       |
     * | 9    | `Cell::STRIKE`        |
     *
     * @param p     The pen whose style field is modified.
     * @param code  The SGR attribute code (1–9).
     *
     * @note READER THREAD only.
     *
     * @see resetSGRStyle() for the corresponding attribute-off path
     * @see Cell::BOLD, Cell::ITALIC, Cell::UNDERLINE, Cell::BLINK,
     *      Cell::INVERSE, Cell::STRIKE
     */
    inline void applySGRStyle (Pen& p, uint16_t code) noexcept
    {
        switch (code)
        {
            case 1:  p.style |= Cell::BOLD;      break;
            case 3:  p.style |= Cell::ITALIC;    break;
            case 4:  p.style |= Cell::UNDERLINE; break;
            case 5:
            case 6:  p.style |= Cell::BLINK;     break;
            case 7:  p.style |= Cell::INVERSE;   break;
            case 9:  p.style |= Cell::STRIKE;    break;
            default: break;
        }
    }

    /**
     * @brief Clears a style bit on the pen for SGR attribute-off codes (21–29).
     *
     * Maps the raw SGR code to the corresponding `Cell` style flag and ANDs its
     * complement into `p.style`.  Codes not listed in the switch are silently
     * ignored.
     *
     * | Code | Attribute cleared     |
     * |------|-----------------------|
     * | 21   | `Cell::BOLD`          |
     * | 22   | `Cell::BOLD`          |
     * | 23   | `Cell::ITALIC`        |
     * | 24   | `Cell::UNDERLINE`     |
     * | 25   | `Cell::BLINK`         |
     * | 27   | `Cell::INVERSE`       |
     * | 29   | `Cell::STRIKE`        |
     *
     * @note Codes 21 and 22 both clear `Cell::BOLD`.  Code 21 is specified as
     *       "doubly underlined" in ECMA-48 but xterm uses it as bold-off, which
     *       is the de-facto standard in modern terminals.
     *
     * @param p     The pen whose style field is modified.
     * @param code  The SGR attribute-off code (21–29).
     *
     * @note READER THREAD only.
     *
     * @see applySGRStyle() for the corresponding attribute-on path
     */
    inline void resetSGRStyle (Pen& p, uint16_t code) noexcept
    {
        switch (code)
        {
            case 21:
            case 22: p.style &= static_cast<uint8_t> (~Cell::BOLD);      break;
            case 23: p.style &= static_cast<uint8_t> (~Cell::ITALIC);    break;
            case 24: p.style &= static_cast<uint8_t> (~Cell::UNDERLINE); break;
            case 25: p.style &= static_cast<uint8_t> (~Cell::BLINK);     break;
            case 27: p.style &= static_cast<uint8_t> (~Cell::INVERSE);   break;
            case 29: p.style &= static_cast<uint8_t> (~Cell::STRIKE);    break;
            default: break;
        }
    }
}

/**
 * @brief Dispatches an SGR sequence and applies all parameter groups to the pen.
 *
 * Iterates over every parameter in `params` and applies the corresponding
 * attribute change to `pen`.  A single SGR sequence may contain multiple
 * parameter groups separated by semicolons, e.g. `ESC[1;32;48;5;200m` sets
 * bold, green foreground, and a 256-color background in one call.
 *
 * @par Dispatch logic (per parameter code)
 *
 * | Code range  | Action                                                  |
 * |-------------|---------------------------------------------------------|
 * | (empty)     | Reset pen to default (`Pen {}`)                         |
 * | 0           | Reset pen to default (`Pen {}`)                         |
 * | 1–9         | Set style attribute via `applySGRStyle()`               |
 * | 21–29       | Clear style attribute via `resetSGRStyle()`             |
 * | 30–37       | Set fg to ANSI palette color 0–7                        |
 * | 38          | Set fg to extended color (256 or RGB) via sub-params    |
 * | 39          | Reset fg to default (`Color {}`)                        |
 * | 40–47       | Set bg to ANSI palette color 0–7                        |
 * | 48          | Set bg to extended color (256 or RGB) via sub-params    |
 * | 49          | Reset bg to default (`Color {}`)                        |
 * | 90–97       | Set fg to bright ANSI palette color 8–15                |
 * | 100–107     | Set bg to bright ANSI palette color 8–15                |
 *
 * @par ANSI 16-color palette mapping
 * @code
 * // Normal colors (30–37 fg, 40–47 bg) → palette indices 0–7
 * p.fg = Color { code - 30, 0, 0, Color::palette };
 *
 * // Bright colors (90–97 fg, 100–107 bg) → palette indices 8–15
 * p.fg = Color { code - 90 + 8, 0, 0, Color::palette };
 * @endcode
 *
 * @param params  The finalised CSI parameter set for the SGR sequence
 *                (final byte 'm').  An empty set (`count == 0`) is treated
 *                identically to a single parameter of 0 (full reset).
 *
 * @note READER THREAD only.
 *
 * @see applySGR()        — public entry point that calls this then `calc()`
 * @see parseExtendedColor() — handles 38;5;N and 38;2;R;G;B sub-sequences
 * @see applySGRStyle()   — sets individual style bits
 * @see resetSGRStyle()   — clears individual style bits
 * @see Pen               — the drawing attribute struct being mutated
 */
void Parser::handleSGR (const CSI& params) noexcept
{
    if (params.count == 0)
    {
        pen = Pen {};
    }
    else
    {
        auto& p { pen };

        for (uint8_t i { 0 }; i < params.count; ++i)
        {
            const auto code { params.values.at (i) };

            if (code == 0)
            {
                p = Pen {};
            }
            else if (code >= 1 and code <= 9)
            {
                applySGRStyle (p, code);
            }
            else if (code >= 21 and code <= 29)
            {
                resetSGRStyle (p, code);
            }
            else if (code >= 30 and code <= 37)
            {
                p.fg = Color { static_cast<uint8_t> (code - 30), 0, 0, Color::palette };
            }
            else if (code == 38)
            {
                p.fg = parseExtendedColor (params, i);
            }
            else if (code == 39)
            {
                p.fg = Color {};
            }
            else if (code >= 40 and code <= 47)
            {
                p.bg = Color { static_cast<uint8_t> (code - 40), 0, 0, Color::palette };
            }
            else if (code == 48)
            {
                p.bg = parseExtendedColor (params, i);
            }
            else if (code == 49)
            {
                p.bg = Color {};
            }
            else if (code >= 90 and code <= 97)
            {
                p.fg = Color { static_cast<uint8_t> (code - 90 + 8), 0, 0, Color::palette };
            }
            else if (code >= 100 and code <= 107)
            {
                p.bg = Color { static_cast<uint8_t> (code - 100 + 8), 0, 0, Color::palette };
            }
        }
    }
}

/**
 * @brief Public SGR entry point: applies attributes and synchronises cached state.
 *
 * Delegates attribute application to `handleSGR()`, then calls `calc()` to
 * propagate any geometry-dependent changes.  This is the method invoked by
 * `csiDispatch()` when the final byte 'm' is received.
 *
 * @par Call sequence
 * @code
 * // Inside csiDispatch(), final byte 'm':
 * applySGR (csi);
 * // Equivalent to:
 * handleSGR (csi);   // mutate pen
 * calc();            // sync scrollBottom and other cached state
 * @endcode
 *
 * @param params  The finalised CSI parameter set for the SGR sequence.
 *
 * @note READER THREAD only.
 *
 * @see handleSGR() — the inner implementation that mutates `pen`
 * @see calc()      — synchronises internal cached geometry after any state change
 */
void Parser::applySGR (const CSI& params) noexcept
{
    handleSGR (params);
    calc();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
