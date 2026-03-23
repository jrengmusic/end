/**
 * @file jreng_box_drawing.h
 * @brief Procedural rasterizer for box drawing, block elements, and braille.
 *
 * BoxDrawing provides a fully self-contained, allocation-free rasterizer for
 * three Unicode character ranges:
 *
 * | Range           | Description                        | Method              |
 * |-----------------|------------------------------------|---------------------|
 * | U+2500–U+257F   | Box drawing characters             | `drawLines()`,      |
 * |                 |                                    | `drawDashedLine()`, |
 * |                 |                                    | `drawDoubleLines()`,|
 * |                 |                                    | `drawRoundedCorner()`,|
 * |                 |                                    | `drawDiagonal()`    |
 * | U+2580–U+259F   | Block elements                     | `drawBlockElement()`|
 * | U+2800–U+28FF   | Braille patterns                   | `drawBraille()`     |
 *
 * ### Design principles
 * - **No allocations**: all methods operate on a caller-supplied `uint8_t*`
 *   buffer of `w × h` bytes.
 * - **Integer arithmetic** for straight lines and block fills (`fillRect()`).
 * - **Anti-aliased Bresenham** for diagonal lines (`drawDiagonal()`): distance
 *   to the line is computed analytically and mapped to alpha.
 * - **SDF-based smoothstep** for rounded corners (`drawRoundedCorner()`):
 *   a signed-distance field of a rounded rectangle drives a `smoothstep()`
 *   anti-aliasing kernel.
 * - **Cell-relative metrics**: line thickness is derived from `cellWidth` via
 *   `lightThickness()` / `heavyThickness()`, so the output scales correctly
 *   at any font size.
 *
 * ### Coordinate system
 * The output buffer uses a top-left origin with Y increasing downward, matching
 * OpenGL's `glTexSubImage2D` convention after the atlas upload.  Pixel (x, y)
 * maps to `buf[y * w + x]`.
 *
 * ### TABLE and HALF_TABLE
 * The 76-entry `TABLE` maps box-drawing codepoints U+2500–U+254B to `Lines`
 * descriptors (up/right/down/left weights).  The 12-entry `HALF_TABLE` covers
 * the half-line characters U+2574–U+257F.  Both tables are `constexpr` and
 * stored in the binary's read-only data segment.
 *
 * @note All methods are `static` and `noexcept`.  `BoxDrawing` has no instance
 *       state and is never instantiated.
 *
 * @see jreng::Glyph::Atlas::getOrRasterizeBoxDrawing()
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>

namespace jreng::Glyph
{

struct BoxDrawing
{
    /**
     * @enum Weight
     * @brief Line weight for box-drawing strokes.
     *
     * - `none`   — no line on this side.
     * - `light`  — standard thin line (≈ 1/8 of cell width).
     * - `heavy`  — bold line (≈ 2× light thickness).
     * - `double_` — two parallel light lines (handled by `drawDoubleLines()`).
     */
    enum class Weight : uint8_t { none, light, heavy, double_ };

    /**
     * @struct Lines
     * @brief Four-directional line weight descriptor for a box-drawing character.
     *
     * Each field specifies the weight of the line segment extending from the
     * cell centre toward the named edge.  Used by `drawLines()` to render
     * standard box-drawing characters.
     *
     * @note `std::is_trivially_copyable` is asserted below.
     */
    struct Lines
    {
        /** @brief Weight of the line segment extending upward from the cell centre. */
        Weight up    { Weight::none };

        /** @brief Weight of the line segment extending rightward from the cell centre. */
        Weight right { Weight::none };

        /** @brief Weight of the line segment extending downward from the cell centre. */
        Weight down  { Weight::none };

        /** @brief Weight of the line segment extending leftward from the cell centre. */
        Weight left  { Weight::none };
    };

    static_assert (std::is_trivially_copyable_v<Lines>);

    /**
     * @brief Return `true` if the codepoint is handled by this rasterizer.
     *
     * Covers box drawing (U+2500–U+257F), block elements (U+2580–U+259F), and
     * braille patterns (U+2800–U+28FF).
     *
     * @param codepoint Unicode codepoint to test.
     * @return `true` if `rasterize()` can render this codepoint.
     *
     * @see rasterize()
     */
    static bool isProcedural (uint32_t codepoint) noexcept
    {
        return (codepoint >= 0x2500 and codepoint <= 0x257F)
            or (codepoint >= 0x2580 and codepoint <= 0x259F)
            or (codepoint >= 0x2800 and codepoint <= 0x28FF);
    }

    /**
     * @brief Rasterize a box-drawing, block-element, or braille codepoint.
     *
     * Clears `buf` to zero, then dispatches to the appropriate sub-renderer
     * based on the codepoint range:
     *
     * - **U+2500–U+257F** (box drawing):
     *   - U+256D–U+2570 (`idx` 0x6D–0x70): `drawRoundedCorner()`
     *   - U+2504–U+250B (`idx` 0x04–0x0B): `drawDashedLine()`
     *   - U+2571–U+2573 (`idx` 0x71–0x73): `drawDiagonal()`
     *   - U+2500–U+254B (`idx` 0x00–0x4B): `drawLines()` via `TABLE`
     *   - U+2550–U+256C (`idx` 0x50–0x6C): `drawDoubleLines()`
     *   - U+2574–U+257F (`idx` 0x74–0x7F): `drawLines()` via `HALF_TABLE`
     * - **U+2580–U+259F** (block elements): `drawBlockElement()`
     * - **U+2800–U+28FF** (braille): `drawBraille()`
     *
     * @param codepoint Unicode codepoint to rasterize.
     * @param w         Output bitmap width in pixels (= cell width).
     * @param h         Output bitmap height in pixels (= cell height).
     * @param buf       Output buffer of `w × h` bytes (R8 greyscale).
     *                  Must be pre-allocated by the caller.
     *
     * @note The buffer is zeroed at the start of this function regardless of
     *       the codepoint.
     * @see isProcedural()
     * @see GlyphAtlas::getOrRasterizeBoxDrawing()
     */
    static void rasterize (uint32_t codepoint, int w, int h, uint8_t* buf) noexcept
    {
        std::memset (buf, 0, static_cast<size_t> (w) * static_cast<size_t> (h));

        if (codepoint >= 0x2500 and codepoint <= 0x257F)
        {
            const uint32_t idx { codepoint - 0x2500 };

            if (idx >= 0x6D and idx <= 0x70)
            {
                drawRoundedCorner (idx, w, h, buf);
            }
            else if (idx >= 0x04 and idx <= 0x0B)
            {
                drawDashedLine (idx, w, h, buf);
            }
            else if (idx >= 0x71 and idx <= 0x73)
            {
                drawDiagonal (idx, w, h, buf);
            }
            else if (idx <= 0x4B)
            {
                const Lines& lines { TABLE.at (idx) };
                drawLines (lines, w, h, buf);
            }
            else if (idx >= 0x50 and idx <= 0x6C)
            {
                drawDoubleLines (idx, w, h, buf);
            }
            else if (idx >= 0x74 and idx <= 0x7F)
            {
                const Lines& lines { HALF_TABLE.at (idx - 0x74) };
                drawLines (lines, w, h, buf);
            }
            else
            {
                const Lines fallback { Weight::light, Weight::light, Weight::light, Weight::light };
                drawLines (fallback, w, h, buf);
            }
        }

        if (codepoint >= 0x2580 and codepoint <= 0x259F)
        {
            drawBlockElement (codepoint, w, h, buf);
        }

        if (codepoint >= 0x2800 and codepoint <= 0x28FF)
        {
            drawBraille (codepoint, w, h, buf);
        }
    }

private:
    /**
     * @brief Fill an axis-aligned rectangle in the bitmap with a constant value.
     *
     * Clips `x0`, `y0`, `x1` to the bitmap bounds before writing.  `y1` is
     * not clipped (callers are responsible for keeping it within `h`).
     *
     * @param buf    Output buffer (R8, row-major, stride = `stride`).
     * @param stride Row stride in pixels (= bitmap width).
     * @param x0     Left edge (inclusive), clamped to ≥ 0.
     * @param y0     Top edge (inclusive), clamped to ≥ 0.
     * @param x1     Right edge (exclusive), clamped to ≤ `stride`.
     * @param y1     Bottom edge (exclusive).
     * @param value  Pixel value to write (0–255).
     */
    static void fillRect (uint8_t* buf, int stride, int x0, int y0, int x1, int y1, uint8_t value) noexcept
    {
        x0 = std::max (x0, 0);
        y0 = std::max (y0, 0);
        x1 = std::min (x1, stride);

        for (int y { y0 }; y < y1; ++y)
        {
            for (int x { x0 }; x < x1; ++x)
            {
                buf[y * stride + x] = value;
            }
        }
    }

    /**
     * @brief Compute the light (thin) line thickness for the given cell width.
     *
     * Returns `max(1, cellWidth / 8)`.  At typical terminal font sizes
     * (8–16 px wide cells) this yields 1–2 px.
     *
     * @param cellWidth Cell width in pixels.
     * @return Light line thickness in pixels (≥ 1).
     */
    static int lightThickness (int cellWidth) noexcept
    {
        return std::max (1, cellWidth / 8);
    }

    /**
     * @brief Compute the heavy (bold) line thickness for the given cell width.
     *
     * Returns `max(2, lightThickness(cellWidth) * 2)`.
     *
     * @param cellWidth Cell width in pixels.
     * @return Heavy line thickness in pixels (≥ 2).
     */
    static int heavyThickness (int cellWidth) noexcept
    {
        return std::max (2, lightThickness (cellWidth) * 2);
    }

    /**
     * @brief Render the four directional line segments described by `lines`.
     *
     * Each non-`none` direction is rendered as a filled rectangle centred on
     * the cell midpoint `(cx, cy)`.  The rectangle extends from the midpoint
     * to the respective cell edge, with thickness determined by the weight.
     *
     * @par Coordinate convention
     * - Up:    `y` range `[0, cy + t/2]`
     * - Down:  `y` range `[cy - t/2, h]`
     * - Left:  `x` range `[0, cx + t/2]`
     * - Right: `x` range `[cx - t/2, w]`
     *
     * @param lines Line weight descriptor.
     * @param w     Bitmap width in pixels.
     * @param h     Bitmap height in pixels.
     * @param buf   Output buffer (R8, `w × h` bytes).
     */
    static void drawLines (const Lines& lines, int w, int h, uint8_t* buf) noexcept
    {
        const int cx { w / 2 };
        const int cy { h / 2 };
        const int lt { lightThickness (w) };
        const int ht { heavyThickness (w) };

        if (lines.up != Weight::none)
        {
            const int t { lines.up == Weight::heavy ? ht : lt };
            fillRect (buf, w, cx - t / 2, 0, cx - t / 2 + t, cy + t / 2, 255);
        }

        if (lines.down != Weight::none)
        {
            const int t { lines.down == Weight::heavy ? ht : lt };
            fillRect (buf, w, cx - t / 2, cy - t / 2, cx - t / 2 + t, h, 255);
        }

        if (lines.left != Weight::none)
        {
            const int t { lines.left == Weight::heavy ? ht : lt };
            fillRect (buf, w, 0, cy - t / 2, cx + t / 2, cy - t / 2 + t, 255);
        }

        if (lines.right != Weight::none)
        {
            const int t { lines.right == Weight::heavy ? ht : lt };
            fillRect (buf, w, cx - t / 2, cy - t / 2, w, cy - t / 2 + t, 255);
        }
    }

    /**
     * @brief Render a dashed horizontal or vertical line (U+2504–U+250B).
     *
     * Draws 3 or 4 evenly spaced dashes with equal gap lengths.  The first
     * dash is offset by half a dash length from the cell edge to centre the
     * pattern visually.
     *
     * @par Index mapping
     * | idx  | Codepoint | Direction  | Weight | Dashes |
     * |------|-----------|------------|--------|--------|
     * | 0x04 | U+2504 ┄  | horizontal | light  | 3      |
     * | 0x05 | U+2505 ┅  | horizontal | heavy  | 3      |
     * | 0x06 | U+2506 ┆  | vertical   | light  | 3      |
     * | 0x07 | U+2507 ┇  | vertical   | heavy  | 3      |
     * | 0x08 | U+2508 ┈  | horizontal | light  | 4      |
     * | 0x09 | U+2509 ┉  | horizontal | heavy  | 4      |
     * | 0x0A | U+250A ┊  | vertical   | light  | 4      |
     * | 0x0B | U+250B ┋  | vertical   | heavy  | 4      |
     *
     * @param idx Box-drawing table index (codepoint − 0x2500).
     * @param w   Bitmap width in pixels.
     * @param h   Bitmap height in pixels.
     * @param buf Output buffer (R8, `w × h` bytes).
     */
    static void drawDashedLine (uint32_t idx, int w, int h, uint8_t* buf) noexcept
    {
        const int cx { w / 2 };
        const int cy { h / 2 };
        const bool horizontal { idx == 0x04 or idx == 0x05 or idx == 0x08 or idx == 0x09 };
        const bool heavy { idx == 0x05 or idx == 0x07 or idx == 0x09 or idx == 0x0B };
        const int numDashes { (idx == 0x04 or idx == 0x05 or idx == 0x06 or idx == 0x07) ? 3 : 4 };
        const int t { heavy ? heavyThickness (w) : lightThickness (w) };

        if (horizontal)
        {
            const int dashLen { w / (numDashes * 2) };
            const int gapLen { dashLen };
            const int y0 { cy - t / 2 };
            const int y1 { y0 + t };
            int x { dashLen / 2 };

            for (int d { 0 }; d < numDashes and x < w; ++d)
            {
                const int end { std::min (x + dashLen, w) };
                fillRect (buf, w, x, y0, end, y1, 255);
                x = end + gapLen;
            }
        }
        else
        {
            const int dashLen { h / (numDashes * 2) };
            const int gapLen { dashLen };
            const int x0 { cx - t / 2 };
            const int x1 { x0 + t };
            int y { dashLen / 2 };

            for (int d { 0 }; d < numDashes and y < h; ++d)
            {
                const int end { std::min (y + dashLen, h) };
                fillRect (buf, w, x0, y, x1, end, 255);
                y = end + gapLen;
            }
        }
    }

    /**
     * @brief Render anti-aliased diagonal lines (U+2571–U+2573).
     *
     * For each pixel, computes the perpendicular distance to the diagonal
     * line(s) and maps it to an alpha value using a linear ramp over a
     * half-pixel AA band.  The result is max-blended with any existing pixel
     * value so that crossing diagonals (U+2573 ╳) combine correctly.
     *
     * @par Index mapping
     * | idx  | Codepoint | Lines drawn                    |
     * |------|-----------|--------------------------------|
     * | 0x71 | U+2571 ╱  | Bottom-left to top-right       |
     * | 0x72 | U+2572 ╲  | Top-left to bottom-right       |
     * | 0x73 | U+2573 ╳  | Both diagonals                 |
     *
     * @param idx Box-drawing table index (codepoint − 0x2500).
     * @param w   Bitmap width in pixels.
     * @param h   Bitmap height in pixels.
     * @param buf Output buffer (R8, `w × h` bytes).
     */
    static void drawDiagonal (uint32_t idx, int w, int h, uint8_t* buf) noexcept
    {
        const int t { lightThickness (w) };
        const float halfT { static_cast<float> (t) * 0.5f };
        const float fw { static_cast<float> (w) };
        const float fh { static_cast<float> (h) };

        for (int py { 0 }; py < h; ++py)
        {
            for (int px { 0 }; px < w; ++px)
            {
                const float x { static_cast<float> (px) + 0.5f };
                const float y { static_cast<float> (py) + 0.5f };
                float alpha { 0.0f };

                if (idx == 0x71 or idx == 0x73)
                {
                    const float dist { std::abs ((x / fw + y / fh - 1.0f) * fw * fh / std::sqrt (fw * fw + fh * fh)) };

                    if (dist < halfT + 0.5f)
                    {
                        alpha = std::max (alpha, std::min (1.0f, halfT + 0.5f - dist));
                    }
                }

                if (idx == 0x72 or idx == 0x73)
                {
                    const float dist { std::abs ((x / fw - y / fh) * fw * fh / std::sqrt (fw * fw + fh * fh)) };

                    if (dist < halfT + 0.5f)
                    {
                        alpha = std::max (alpha, std::min (1.0f, halfT + 0.5f - dist));
                    }
                }

                if (alpha > 0.0f)
                {
                    const int existing { buf[py * w + px] };
                    buf[py * w + px] = static_cast<uint8_t> (std::max (existing, static_cast<int> (alpha * 255.0f)));
                }
            }
        }
    }

    /**
     * @brief Render double-line box-drawing characters (U+2550–U+256C).
     *
     * Each double-line character is rendered as two parallel light-weight
     * lines separated by a gap of `max(1, lt)` pixels.  The outer and inner
     * rail positions are computed relative to the cell centre:
     * - Horizontal rails: `outerH = cy - gap`, `innerH = cy + gap`
     * - Vertical rails:   `outerV = cx - gap`, `innerV = cx + gap`
     *
     * Corner and junction characters are handled case-by-case to correctly
     * connect the rail pairs.
     *
     * @param idx Box-drawing table index (codepoint − 0x2500), range 0x50–0x6C.
     * @param w   Bitmap width in pixels.
     * @param h   Bitmap height in pixels.
     * @param buf Output buffer (R8, `w × h` bytes).
     */
    static void drawDoubleLines (uint32_t idx, int w, int h, uint8_t* buf) noexcept
    {
        const int cx { w / 2 };
        const int cy { h / 2 };
        const int lt { lightThickness (w) };
        const int gap { std::max (1, lt) };

        const int outerH { cy - gap };
        const int innerH { cy + gap };
        const int outerV { cx - gap };
        const int innerV { cx + gap };

        switch (idx)
        {
            case 0x50: // ═
                fillRect (buf, w, 0, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x51: // ║
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                break;
            case 0x52: // ╒  down=light, right=double
                fillRect (buf, w, cx - lt / 2, cy, cx - lt / 2 + lt, h, 255);
                fillRect (buf, w, cx, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, cx, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x53: // ╓  down=double, right=light
                fillRect (buf, w, outerV - lt / 2, cy, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, cy, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, cx, cy - lt / 2, w, cy - lt / 2 + lt, 255);
                break;
            case 0x54: // ╔  down=double, right=double
                fillRect (buf, w, outerV - lt / 2, cy, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, cy, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, cx, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, innerV, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x55: // ╕  down=light, left=double
                fillRect (buf, w, cx - lt / 2, cy, cx - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, cx, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, cx, innerH - lt / 2 + lt, 255);
                break;
            case 0x56: // ╖  down=double, left=light
                fillRect (buf, w, outerV - lt / 2, cy, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, cy, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, cy - lt / 2, cx, cy - lt / 2 + lt, 255);
                break;
            case 0x57: // ╗  down=double, left=double
                fillRect (buf, w, outerV - lt / 2, cy, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, cy, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, cx, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, outerV, innerH - lt / 2 + lt, 255);
                break;
            case 0x58: // ╘  up=light, right=double
                fillRect (buf, w, cx - lt / 2, 0, cx - lt / 2 + lt, cy, 255);
                fillRect (buf, w, cx, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, cx, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x59: // ╙  up=double, right=light
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, cx, cy - lt / 2, w, cy - lt / 2 + lt, 255);
                break;
            case 0x5A: // ╚  up=double, right=double
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, cx, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, innerV, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x5B: // ╛  up=light, left=double
                fillRect (buf, w, cx - lt / 2, 0, cx - lt / 2 + lt, cy, 255);
                fillRect (buf, w, 0, outerH - lt / 2, cx, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, cx, innerH - lt / 2 + lt, 255);
                break;
            case 0x5C: // ╜  up=double, left=light
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, 0, cy - lt / 2, cx, cy - lt / 2 + lt, 255);
                break;
            case 0x5D: // ╝  up=double, left=double
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, 0, outerH - lt / 2, cx, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, outerV, innerH - lt / 2 + lt, 255);
                break;
            case 0x5E: // ╞  up=light, down=light, right=double
                fillRect (buf, w, cx - lt / 2, 0, cx - lt / 2 + lt, h, 255);
                fillRect (buf, w, cx, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, cx, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x5F: // ╟  up=double, down=double, right=light
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, cx, cy - lt / 2, w, cy - lt / 2 + lt, 255);
                break;
            case 0x60: // ╠  up=double, down=double, right=double
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, innerV, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x61: // ╡  up=light, down=light, left=double
                fillRect (buf, w, cx - lt / 2, 0, cx - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, cx, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, cx, innerH - lt / 2 + lt, 255);
                break;
            case 0x62: // ╢  up=double, down=double, left=light
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, cy - lt / 2, cx, cy - lt / 2 + lt, 255);
                break;
            case 0x63: // ╣  up=double, down=double, left=double
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, outerV, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, outerV, innerH - lt / 2 + lt, 255);
                break;
            case 0x64: // ╤  down=light, left=double, right=double
                fillRect (buf, w, cx - lt / 2, cy, cx - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x65: // ╥  down=double, left=light, right=light
                fillRect (buf, w, outerV - lt / 2, cy, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, cy, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, cy - lt / 2, w, cy - lt / 2 + lt, 255);
                break;
            case 0x66: // ╦  down=double, left=double, right=double
                fillRect (buf, w, outerV - lt / 2, cy, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, cy, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, outerV, innerH - lt / 2, innerV, innerH - lt / 2 + lt, 255);
                break;
            case 0x67: // ╧  up=light, left=double, right=double
                fillRect (buf, w, cx - lt / 2, 0, cx - lt / 2 + lt, cy, 255);
                fillRect (buf, w, 0, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x68: // ╨  up=double, left=light, right=light
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, 0, cy - lt / 2, w, cy - lt / 2 + lt, 255);
                break;
            case 0x69: // ╩  up=double, left=double, right=double
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, cy, 255);
                fillRect (buf, w, 0, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, outerV, innerH - lt / 2, innerV, innerH - lt / 2 + lt, 255);
                break;
            case 0x6A: // ╪  up=light, down=light, left=double, right=double
                fillRect (buf, w, cx - lt / 2, 0, cx - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            case 0x6B: // ╫  up=double, down=double, left=light, right=light
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, cy - lt / 2, w, cy - lt / 2 + lt, 255);
                break;
            case 0x6C: // ╬  up=double, down=double, left=double, right=double
                fillRect (buf, w, outerV - lt / 2, 0, outerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, innerV - lt / 2, 0, innerV - lt / 2 + lt, h, 255);
                fillRect (buf, w, 0, outerH - lt / 2, outerV, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, innerV, outerH - lt / 2, w, outerH - lt / 2 + lt, 255);
                fillRect (buf, w, 0, innerH - lt / 2, outerV, innerH - lt / 2 + lt, 255);
                fillRect (buf, w, innerV, innerH - lt / 2, w, innerH - lt / 2 + lt, 255);
                break;
            default:
                break;
        }
    }

    /**
     * @brief Cubic Hermite smoothstep interpolation.
     *
     * Returns 0 for `x ≤ edge0`, 1 for `x ≥ edge1`, and a smooth cubic
     * interpolation in between.  Used by `drawRoundedCorner()` to produce
     * anti-aliased stroke edges.
     *
     * @param edge0 Lower edge of the transition band.
     * @param edge1 Upper edge of the transition band.
     * @param x     Input value.
     * @return Smoothly interpolated value in [0, 1].
     *
     * @note Returns 0 or 1 immediately when `edge0 == edge1` to avoid
     *       division by zero.
     */
    static float smoothstep (float edge0, float edge1, float x) noexcept
    {
        float result { x < edge0 ? 0.0f : 1.0f };

        if (edge0 != edge1)
        {
            const float t { std::clamp ((x - edge0) / (edge1 - edge0), 0.0f, 1.0f) };
            result = t * t * (3.0f - 2.0f * t);
        }

        return result;
    }

    /**
     * @brief Render an SDF-based rounded corner (U+256D–U+2570).
     *
     * Computes a signed-distance field of a rounded rectangle centred at the
     * cell midpoint.  The SDF value drives a `smoothstep()` anti-aliasing
     * kernel to produce a smooth stroke of `lightThickness()` width.
     *
     * Each corner is rendered by shifting the sample coordinates so that the
     * arc centre falls at the appropriate cell corner:
     * - 0x6D (╭): top-left corner — shift `(-cx, -cy)`
     * - 0x6E (╮): top-right corner — shift `(+cx, -cy)`
     * - 0x6F (╯): bottom-right corner — shift `(+cx, +cy)`
     * - 0x70 (╰): bottom-left corner — shift `(-cx, +cy)`
     *
     * @param idx Box-drawing table index (codepoint − 0x2500), range 0x6D–0x70.
     * @param w   Bitmap width in pixels.
     * @param h   Bitmap height in pixels.
     * @param buf Output buffer (R8, `w × h` bytes).
     *
     * @see smoothstep()
     */
    static void drawRoundedCorner (uint32_t idx, int w, int h, uint8_t* buf) noexcept
    {
        const float cx { static_cast<float> (w) / 2.0f };
        const float cy { static_cast<float> (h) / 2.0f };
        const float thickness { static_cast<float> (lightThickness (w)) };
        const float radius { std::min (cx, cy) };
        const float bx { cx - radius };
        const float by { cy - radius };

        float xShift { 0.0f };
        float yShift { 0.0f };

        switch (idx)
        {
            case 0x6D:
                xShift = -cx;
                yShift = -cy;
                break;
            case 0x6E:
                xShift = +cx;
                yShift = -cy;
                break;
            case 0x6F:
                xShift = +cx;
                yShift = +cy;
                break;
            case 0x70:
                xShift = -cx;
                yShift = +cy;
                break;
            default:
                break;
        }

        const float halfStroke { thickness * 0.5f };

        for (int py { 0 }; py < h; ++py)
        {
            for (int px { 0 }; px < w; ++px)
            {
                const float sampleX { static_cast<float> (px) + 0.5f + xShift };
                const float sampleY { static_cast<float> (py) + 0.5f + yShift };
                const float posX { sampleX - cx };
                const float posY { sampleY - cy };
                const float qx { std::abs (posX) - bx };
                const float qy { std::abs (posY) - by };
                const float dx { std::max (qx, 0.0f) };
                const float dy { std::max (qy, 0.0f) };
                const float dist { std::sqrt (dx * dx + dy * dy) + std::min (std::max (qx, qy), 0.0f) - radius };
                const float aa { (qx > 1e-7f and qy > 1e-7f) ? 0.5f : 0.0f };
                const float outer { halfStroke - dist };
                const float inner { -halfStroke - dist };
                const float alpha { smoothstep (-aa, aa, outer) - smoothstep (-aa, aa, inner) };

                if (alpha > 0.0f)
                {
                    const int existing { buf[py * w + px] };
                    buf[py * w + px] = static_cast<uint8_t> (std::max (existing, static_cast<int> (alpha * 255.0f)));
                }
            }
        }
    }

    /**
     * @brief Fill the entire bitmap with a uniform shade value.
     *
     * Used for shade block elements (U+2591 ░, U+2592 ▒, U+2593 ▓).
     *
     * @param w     Bitmap width in pixels.
     * @param h     Bitmap height in pixels.
     * @param buf   Output buffer (R8, `w × h` bytes).
     * @param alpha Shade value: 64 (light), 128 (medium), 191 (dark).
     */
    static void drawShade (int w, int h, uint8_t* buf, uint8_t alpha) noexcept
    {
        std::memset (buf, alpha, static_cast<size_t> (w) * static_cast<size_t> (h));
    }

    /**
     * @brief Render a block element character (U+2580–U+259F).
     *
     * Block elements are rendered as one or more filled rectangles.  The cell
     * is divided into halves (`halfW = w/2`, `halfH = h/2`) and eighths
     * (`h/8`, `w/8`) for the fractional block characters.
     *
     * @par Character groups
     * - U+2580–U+2588: Upper/lower/full block and vertical eighths.
     * - U+2589–U+258F: Left fractional blocks (7/8 down to 1/8 width).
     * - U+2590: Right half block.
     * - U+2591–U+2593: Shade blocks (light / medium / dark).
     * - U+2594–U+2595: Upper 1/8 and right 1/8 blocks.
     * - U+2596–U+259F: Quadrant combinations.
     *
     * @param codepoint Unicode codepoint (U+2580–U+259F).
     * @param w         Bitmap width in pixels.
     * @param h         Bitmap height in pixels.
     * @param buf       Output buffer (R8, `w × h` bytes).
     */
    static void drawBlockElement (uint32_t codepoint, int w, int h, uint8_t* buf) noexcept
    {
        const int halfW { w / 2 };
        const int halfH { h / 2 };

        switch (codepoint)
        {
            case 0x2580: fillRect (buf, w, 0, 0, w, halfH, 255); break;
            case 0x2581: fillRect (buf, w, 0, h * 7 / 8, w, h, 255); break;
            case 0x2582: fillRect (buf, w, 0, h * 6 / 8, w, h, 255); break;
            case 0x2583: fillRect (buf, w, 0, h * 5 / 8, w, h, 255); break;
            case 0x2584: fillRect (buf, w, 0, h * 4 / 8, w, h, 255); break;
            case 0x2585: fillRect (buf, w, 0, h * 3 / 8, w, h, 255); break;
            case 0x2586: fillRect (buf, w, 0, h * 2 / 8, w, h, 255); break;
            case 0x2587: fillRect (buf, w, 0, h * 1 / 8, w, h, 255); break;
            case 0x2588: fillRect (buf, w, 0, 0, w, h, 255); break;
            case 0x2589: fillRect (buf, w, 0, 0, w * 7 / 8, h, 255); break;
            case 0x258A: fillRect (buf, w, 0, 0, w * 6 / 8, h, 255); break;
            case 0x258B: fillRect (buf, w, 0, 0, w * 5 / 8, h, 255); break;
            case 0x258C: fillRect (buf, w, 0, 0, w * 4 / 8, h, 255); break;
            case 0x258D: fillRect (buf, w, 0, 0, w * 3 / 8, h, 255); break;
            case 0x258E: fillRect (buf, w, 0, 0, w * 2 / 8, h, 255); break;
            case 0x258F: fillRect (buf, w, 0, 0, w * 1 / 8, h, 255); break;
            case 0x2590: fillRect (buf, w, halfW, 0, w, h, 255); break;
            case 0x2591: drawShade (w, h, buf, 64); break;
            case 0x2592: drawShade (w, h, buf, 128); break;
            case 0x2593: drawShade (w, h, buf, 191); break;
            case 0x2594: fillRect (buf, w, 0, 0, w, h * 1 / 8, 255); break;
            case 0x2595: fillRect (buf, w, w * 7 / 8, 0, w, h, 255); break;
            case 0x2596: fillRect (buf, w, 0, halfH, halfW, h, 255); break;
            case 0x2597: fillRect (buf, w, halfW, halfH, w, h, 255); break;
            case 0x2598: fillRect (buf, w, 0, 0, halfW, halfH, 255); break;
            case 0x2599:
                fillRect (buf, w, 0, 0, halfW, halfH, 255);
                fillRect (buf, w, 0, halfH, halfW, h, 255);
                fillRect (buf, w, halfW, halfH, w, h, 255);
                break;
            case 0x259A:
                fillRect (buf, w, 0, 0, halfW, halfH, 255);
                fillRect (buf, w, halfW, halfH, w, h, 255);
                break;
            case 0x259B:
                fillRect (buf, w, 0, 0, halfW, halfH, 255);
                fillRect (buf, w, halfW, 0, w, halfH, 255);
                fillRect (buf, w, 0, halfH, halfW, h, 255);
                break;
            case 0x259C:
                fillRect (buf, w, 0, 0, halfW, halfH, 255);
                fillRect (buf, w, halfW, 0, w, halfH, 255);
                fillRect (buf, w, halfW, halfH, w, h, 255);
                break;
            case 0x259D: fillRect (buf, w, halfW, 0, w, halfH, 255); break;
            case 0x259E:
                fillRect (buf, w, halfW, 0, w, halfH, 255);
                fillRect (buf, w, 0, halfH, halfW, h, 255);
                break;
            case 0x259F:
                fillRect (buf, w, halfW, 0, w, halfH, 255);
                fillRect (buf, w, 0, halfH, halfW, h, 255);
                fillRect (buf, w, halfW, halfH, w, h, 255);
                break;
            default: break;
        }
    }

    /**
     * @brief Render a braille pattern character (U+2800–U+28FF).
     *
     * The 8-bit pattern encoded in the low byte of the codepoint (codepoint −
     * U+2800) maps directly to the 8 braille dots in the standard 2×4 grid:
     *
     * @code
     *   Bit 0 (dot 1) → col 0, row 0
     *   Bit 1 (dot 2) → col 0, row 1
     *   Bit 2 (dot 3) → col 0, row 2
     *   Bit 3 (dot 4) → col 1, row 0
     *   Bit 4 (dot 5) → col 1, row 1
     *   Bit 5 (dot 6) → col 1, row 2
     *   Bit 6 (dot 7) → col 0, row 3
     *   Bit 7 (dot 8) → col 1, row 3
     * @endcode
     *
     * Each dot is rendered as a filled square of `max(1, cellW/4) × max(1,
     * cellH/8)` pixels, centred within its grid cell.
     *
     * @param codepoint Unicode codepoint (U+2800–U+28FF).
     * @param w         Bitmap width in pixels.
     * @param h         Bitmap height in pixels.
     * @param buf       Output buffer (R8, `w × h` bytes).
     */
    static void drawBraille (uint32_t codepoint, int w, int h, uint8_t* buf) noexcept
    {
        const uint8_t pattern { static_cast<uint8_t> (codepoint - 0x2800) };
        const int cellW { w / 2 };
        const int cellH { h / 4 };
        const int dotW { std::max (1, cellW / 2) };
        const int dotH { std::max (1, cellH / 2) };

        static constexpr std::array<int, 8> dotCol {{ 0, 0, 0, 1, 1, 1, 0, 1 }};
        static constexpr std::array<int, 8> dotRow {{ 0, 1, 2, 0, 1, 2, 3, 3 }};

        for (int i { 0 }; i < 8; ++i)
        {
            if (pattern & static_cast<uint8_t> (1 << i))
            {
                const int col { dotCol.at (i) };
                const int row { dotRow.at (i) };
                const int x0 { col * cellW + (cellW - dotW) / 2 };
                const int y0 { row * cellH + (cellH - dotH) / 2 };
                fillRect (buf, w, x0, y0, x0 + dotW, y0 + dotH, 255);
            }
        }
    }

    /**
     * @brief Lookup table mapping box-drawing indices 0x00–0x4B to `Lines` descriptors.
     *
     * Index `i` corresponds to Unicode codepoint U+2500 + i.  Entries for
     * dashed characters (0x04–0x0B) are present but ignored by `rasterize()`,
     * which dispatches those to `drawDashedLine()` instead.
     *
     * @see rasterize()
     * @see drawLines()
     */
    static constexpr std::array<Lines, 76> TABLE
    {{
        { Weight::none,  Weight::light, Weight::none,  Weight::light }, // 0x2500 ─
        { Weight::none,  Weight::heavy, Weight::none,  Weight::heavy }, // 0x2501 ━
        { Weight::light, Weight::none,  Weight::light, Weight::none  }, // 0x2502 │
        { Weight::heavy, Weight::none,  Weight::heavy, Weight::none  }, // 0x2503 ┃
        { Weight::none,  Weight::light, Weight::none,  Weight::light }, // 0x2504 ┄
        { Weight::none,  Weight::heavy, Weight::none,  Weight::heavy }, // 0x2505 ┅
        { Weight::light, Weight::none,  Weight::light, Weight::none  }, // 0x2506 ┆
        { Weight::heavy, Weight::none,  Weight::heavy, Weight::none  }, // 0x2507 ┇
        { Weight::none,  Weight::light, Weight::none,  Weight::light }, // 0x2508 ┈
        { Weight::none,  Weight::heavy, Weight::none,  Weight::heavy }, // 0x2509 ┉
        { Weight::light, Weight::none,  Weight::light, Weight::none  }, // 0x250A ┊
        { Weight::heavy, Weight::none,  Weight::heavy, Weight::none  }, // 0x250B ┋
        { Weight::none,  Weight::light, Weight::light, Weight::none  }, // 0x250C ┌
        { Weight::none,  Weight::heavy, Weight::light, Weight::none  }, // 0x250D ┍
        { Weight::none,  Weight::light, Weight::heavy, Weight::none  }, // 0x250E ┎
        { Weight::none,  Weight::heavy, Weight::heavy, Weight::none  }, // 0x250F ┏
        { Weight::none,  Weight::none,  Weight::light, Weight::light }, // 0x2510 ┐
        { Weight::none,  Weight::none,  Weight::light, Weight::heavy }, // 0x2511 ┑
        { Weight::none,  Weight::none,  Weight::heavy, Weight::light }, // 0x2512 ┒
        { Weight::none,  Weight::none,  Weight::heavy, Weight::heavy }, // 0x2513 ┓
        { Weight::light, Weight::light, Weight::none,  Weight::none  }, // 0x2514 └
        { Weight::light, Weight::heavy, Weight::none,  Weight::none  }, // 0x2515 ┕
        { Weight::heavy, Weight::light, Weight::none,  Weight::none  }, // 0x2516 ┖
        { Weight::heavy, Weight::heavy, Weight::none,  Weight::none  }, // 0x2517 ┗
        { Weight::light, Weight::none,  Weight::none,  Weight::light }, // 0x2518 ┘
        { Weight::light, Weight::none,  Weight::none,  Weight::heavy }, // 0x2519 ┙
        { Weight::heavy, Weight::none,  Weight::none,  Weight::light }, // 0x251A ┚
        { Weight::heavy, Weight::none,  Weight::none,  Weight::heavy }, // 0x251B ┛
        { Weight::light, Weight::light, Weight::light, Weight::none  }, // 0x251C ├
        { Weight::light, Weight::heavy, Weight::light, Weight::none  }, // 0x251D ┝
        { Weight::heavy, Weight::light, Weight::light, Weight::none  }, // 0x251E ┞
        { Weight::light, Weight::light, Weight::heavy, Weight::none  }, // 0x251F ┟
        { Weight::heavy, Weight::light, Weight::heavy, Weight::none  }, // 0x2520 ┠
        { Weight::heavy, Weight::heavy, Weight::light, Weight::none  }, // 0x2521 ┡
        { Weight::light, Weight::heavy, Weight::heavy, Weight::none  }, // 0x2522 ┢
        { Weight::heavy, Weight::heavy, Weight::heavy, Weight::none  }, // 0x2523 ┣
        { Weight::light, Weight::none,  Weight::light, Weight::light }, // 0x2524 ┤
        { Weight::light, Weight::none,  Weight::light, Weight::heavy }, // 0x2525 ┥
        { Weight::heavy, Weight::none,  Weight::light, Weight::light }, // 0x2526 ┦
        { Weight::light, Weight::none,  Weight::heavy, Weight::light }, // 0x2527 ┧
        { Weight::heavy, Weight::none,  Weight::heavy, Weight::light }, // 0x2528 ┨
        { Weight::heavy, Weight::none,  Weight::light, Weight::heavy }, // 0x2529 ┩
        { Weight::light, Weight::none,  Weight::heavy, Weight::heavy }, // 0x252A ┪
        { Weight::heavy, Weight::none,  Weight::heavy, Weight::heavy }, // 0x252B ┫
        { Weight::none,  Weight::light, Weight::light, Weight::light }, // 0x252C ┬
        { Weight::none,  Weight::light, Weight::light, Weight::heavy }, // 0x252D ┭
        { Weight::none,  Weight::heavy, Weight::light, Weight::light }, // 0x252E ┮
        { Weight::none,  Weight::heavy, Weight::light, Weight::heavy }, // 0x252F ┯
        { Weight::none,  Weight::light, Weight::heavy, Weight::light }, // 0x2530 ┰
        { Weight::none,  Weight::light, Weight::heavy, Weight::heavy }, // 0x2531 ┱
        { Weight::none,  Weight::heavy, Weight::heavy, Weight::light }, // 0x2532 ┲
        { Weight::none,  Weight::heavy, Weight::heavy, Weight::heavy }, // 0x2533 ┳
        { Weight::light, Weight::light, Weight::none,  Weight::light }, // 0x2534 ┴
        { Weight::light, Weight::light, Weight::none,  Weight::heavy }, // 0x2535 ┵
        { Weight::light, Weight::heavy, Weight::none,  Weight::light }, // 0x2536 ┶
        { Weight::light, Weight::heavy, Weight::none,  Weight::heavy }, // 0x2537 ┷
        { Weight::heavy, Weight::light, Weight::none,  Weight::light }, // 0x2538 ┸
        { Weight::heavy, Weight::light, Weight::none,  Weight::heavy }, // 0x2539 ┹
        { Weight::heavy, Weight::heavy, Weight::none,  Weight::light }, // 0x253A ┺
        { Weight::heavy, Weight::heavy, Weight::none,  Weight::heavy }, // 0x253B ┻
        { Weight::light, Weight::light, Weight::light, Weight::light }, // 0x253C ┼
        { Weight::light, Weight::light, Weight::light, Weight::heavy }, // 0x253D ┽
        { Weight::light, Weight::heavy, Weight::light, Weight::light }, // 0x253E ┾
        { Weight::light, Weight::heavy, Weight::light, Weight::heavy }, // 0x253F ┿
        { Weight::heavy, Weight::light, Weight::light, Weight::light }, // 0x2540 ╀
        { Weight::light, Weight::light, Weight::heavy, Weight::light }, // 0x2541 ╁
        { Weight::heavy, Weight::light, Weight::heavy, Weight::light }, // 0x2542 ╂
        { Weight::heavy, Weight::light, Weight::light, Weight::heavy }, // 0x2543 ╃
        { Weight::heavy, Weight::heavy, Weight::light, Weight::light }, // 0x2544 ╄
        { Weight::light, Weight::light, Weight::heavy, Weight::heavy }, // 0x2545 ╅
        { Weight::light, Weight::heavy, Weight::heavy, Weight::light }, // 0x2546 ╆
        { Weight::light, Weight::heavy, Weight::light, Weight::heavy }, // 0x2547 ╇
        { Weight::heavy, Weight::light, Weight::heavy, Weight::heavy }, // 0x2548 ╈
        { Weight::heavy, Weight::heavy, Weight::heavy, Weight::light }, // 0x2549 ╉
        { Weight::heavy, Weight::heavy, Weight::light, Weight::heavy }, // 0x254A ╊
        { Weight::heavy, Weight::heavy, Weight::heavy, Weight::heavy }, // 0x254B ╋
    }};

    /**
     * @brief Lookup table for half-line characters U+2574–U+257F.
     *
     * Index `i` corresponds to Unicode codepoint U+2574 + i.  These characters
     * have a line segment on only one side of the cell centre.
     *
     * @see rasterize()
     * @see drawLines()
     */
    static constexpr std::array<Lines, 12> HALF_TABLE
    {{
        { Weight::none,  Weight::none,  Weight::none,  Weight::light }, // 0x2574 ╴
        { Weight::light, Weight::none,  Weight::none,  Weight::none  }, // 0x2575 ╵
        { Weight::none,  Weight::light, Weight::none,  Weight::none  }, // 0x2576 ╶
        { Weight::none,  Weight::none,  Weight::light, Weight::none  }, // 0x2577 ╷
        { Weight::none,  Weight::none,  Weight::none,  Weight::heavy }, // 0x2578 ╸
        { Weight::heavy, Weight::none,  Weight::none,  Weight::none  }, // 0x2579 ╹
        { Weight::none,  Weight::heavy, Weight::none,  Weight::none  }, // 0x257A ╺
        { Weight::none,  Weight::none,  Weight::heavy, Weight::none  }, // 0x257B ╻
        { Weight::none,  Weight::heavy, Weight::none,  Weight::light }, // 0x257C ╼
        { Weight::light, Weight::none,  Weight::heavy, Weight::none  }, // 0x257D ╽
        { Weight::none,  Weight::light, Weight::none,  Weight::heavy }, // 0x257E ╾
        { Weight::heavy, Weight::none,  Weight::light, Weight::none  }, // 0x257F ╿
    }};
};

} // namespace jreng::Glyph
