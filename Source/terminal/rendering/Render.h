/**
 * @file Render.h
 * @brief Shared GPU-facing render types for the terminal rendering pipeline.
 *
 * Extracted from Screen.h to break the circular dependency introduced by the
 * upcoming Glyph class extraction.  All types here cross the MESSAGE THREAD /
 * GL THREAD boundary and are consumed by both `Screen` and future render
 * subsystems without pulling in the full `Screen` template.
 *
 * ## Contents
 *
 * - `BlockGeometry`  — normalised geometry descriptor for Unicode block-element characters.
 * - `blockTable`     — lookup table mapping block-element codepoints to `BlockGeometry`.
 * - `isBlockChar()`  — predicate for the block-element codepoint range.
 * - `Theme`          — alias for the active colour theme type from `lua::Engine`.
 * - `Render`         — namespace-struct grouping `Background`, `Glyph`, `ImageQuad`, `Snapshot`.
 *
 * @see Screen
 * @see BlockGeometry
 * @see Render::Snapshot
 */

#pragma once
#include <JuceHeader.h>
#include <array>
#include "../../lua/Engine.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct BlockGeometry
 * @brief Normalised geometry descriptor for a Unicode block-element character.
 *
 * Each entry in `blockTable` describes one block-element codepoint
 * (U+2580–U+2593) as a rectangle in normalised cell space [0, 1] × [0, 1],
 * plus an optional alpha override.  The renderer scales these values by the
 * physical cell dimensions to produce pixel-space quads.
 *
 * @note `static_assert` below verifies trivial copyability for safe GPU upload.
 *
 * @see blockTable
 * @see Screen::buildBlockRect()
 */
struct BlockGeometry
{
    float x;///< Normalised X offset from the left edge of the cell [0, 1].
    float y;///< Normalised Y offset from the top edge of the cell [0, 1].
    float w;///< Normalised width as a fraction of the cell width [0, 1].
    float h;///< Normalised height as a fraction of the cell height [0, 1].
    float alpha;///< Alpha override: negative means use the foreground colour's alpha; otherwise [0, 1].
};

static_assert (std::is_trivially_copyable_v<BlockGeometry>, "BlockGeometry must be trivially copyable");

/// @brief First Unicode codepoint in the box-drawing range (U+2500 BOX DRAWINGS LIGHT HORIZONTAL).
static constexpr uint32_t boxDrawingFirst { 0x2500 };

/// @brief Last Unicode codepoint in the box-drawing range (U+259F QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT).
static constexpr uint32_t boxDrawingLast { 0x259F };

/// @brief First Unicode codepoint in the block-element range (U+2580 UPPER HALF BLOCK).
static constexpr uint32_t blockFirst { 0x2580 };

/// @brief Last Unicode codepoint in the block-element range (U+2593 DARK SHADE).
static constexpr uint32_t blockLast { 0x2593 };

/**
 * @brief Lookup table mapping block-element codepoints to normalised geometry.
 *
 * Indexed by `codepoint - blockFirst`.  Contains 20 entries covering
 * U+2580–U+2593.  Each entry is a `BlockGeometry` describing the filled
 * rectangle within the cell in normalised [0, 1] space.
 *
 * @see BlockGeometry
 * @see Screen::buildBlockRect()
 */
static constexpr std::array<BlockGeometry, 20> blockTable {
    {
     { 0.0f, 0.0f, 1.0f, 0.5f, -1.0f },   { 0.0f, 0.875f, 1.0f, 0.125f, -1.0f },
     { 0.0f, 0.75f, 1.0f, 0.25f, -1.0f }, { 0.0f, 0.625f, 1.0f, 0.375f, -1.0f },
     { 0.0f, 0.5f, 1.0f, 0.5f, -1.0f },   { 0.0f, 0.375f, 1.0f, 0.625f, -1.0f },
     { 0.0f, 0.25f, 1.0f, 0.75f, -1.0f }, { 0.0f, 0.125f, 1.0f, 0.875f, -1.0f },
     { 0.0f, 0.0f, 1.0f, 1.0f, -1.0f },   { 0.0f, 0.0f, 0.875f, 1.0f, -1.0f },
     { 0.0f, 0.0f, 0.75f, 1.0f, -1.0f },  { 0.0f, 0.0f, 0.625f, 1.0f, -1.0f },
     { 0.0f, 0.0f, 0.5f, 1.0f, -1.0f },   { 0.0f, 0.0f, 0.375f, 1.0f, -1.0f },
     { 0.0f, 0.0f, 0.25f, 1.0f, -1.0f },  { 0.0f, 0.0f, 0.125f, 1.0f, -1.0f },
     { 0.5f, 0.0f, 0.5f, 1.0f, -1.0f },   { 0.0f, 0.0f, 1.0f, 1.0f, 0.25f },
     { 0.0f, 0.0f, 1.0f, 1.0f, 0.50f },   { 0.0f, 0.0f, 1.0f, 1.0f, 0.75f },
     }
};

/**
 * @brief Returns true if @p codepoint is a Unicode block-element character.
 *
 * Tests whether @p codepoint falls in the range [blockFirst, blockLast]
 * (U+2580–U+2593).  Block characters are rendered as GPU quads rather than
 * rasterised glyphs.
 *
 * @param codepoint  Unicode scalar value to test.
 * @return           `true` if the codepoint is a block element.
 *
 * @see blockTable
 * @see Screen::buildBlockRect()
 */
inline bool isBlockChar (uint32_t codepoint) noexcept { return codepoint >= blockFirst and codepoint <= blockLast; }

/// @brief Alias for the active colour theme type from lua::Engine.
using Theme = lua::Engine::Theme;

/**
 * @struct Render
 * @brief Namespace-struct grouping all GPU-facing render types.
 *
 * `Render` is a plain struct used as a namespace to group the types that
 * cross the MESSAGE THREAD / GL THREAD boundary:
 *
 * - `Render::Background` — alias for `jam::Glyph::Render::Background`.
 * - `Render::Glyph`      — alias for `jam::Glyph::Render::Quad` (kept for
 *                          minimal consumer churn; the canonical module name
 *                          is `Quad` to avoid collision with `jam::Typeface::Glyph`).
 * - `Render::Snapshot`   — terminal-specific snapshot: inherits the generic
 *                          `jam::Glyph::Render::SnapshotBase` arrays and
 *                          adds cursor state fields.
 * - `jam::SnapshotBuffer` — double-buffered lock-free snapshot exchange.
 *
 * @see Screen
 * @see jam::Glyph::Render
 */
struct Render
{
    /// @brief Alias for the module-level coloured rectangle type.
    using Background = jam::Glyph::Render::Background;

    /// @brief Alias for the module-level positioned quad type.
    /// @note The canonical module name is `jam::Glyph::Render::Quad`; this
    ///       alias preserves existing consumer code at `Terminal::Render::Glyph`.
    using Glyph = jam::Glyph::Render::Quad;

    /**
 * @struct Snapshot
 * @brief A complete rendered frame: glyph instances + background quads + cursor state.
 *
 * Inherits the generic `jam::Glyph::Render::SnapshotBase` which owns the
 * three `HeapBlock` arrays (`mono`, `emoji`, `backgrounds`) and
 * `ensureCapacity()`.  Terminal-specific cursor state fields are added here.
 *
 * Two `Snapshot` instances are owned internally by `jam::SnapshotBuffer`
 * and recycled via atomic pointer exchange to avoid per-frame allocation.
 *
 * @see jam::Glyph::Render::SnapshotBase
 * @see jam::SnapshotBuffer
 * @see Screen::updateSnapshot()
 */
    struct Snapshot : jam::Glyph::Render::SnapshotBase
    {

        juce::Point<int> cursorPosition;///< Cursor position in grid coordinates (col, row).
        bool cursorVisible { false };///< True if DECTCEM cursor mode is on.
        int cursorShape { 0 };///< DECSCUSR Ps value (0 = user glyph, 1–6 = geometric).
        float cursorColorR { -1.0f };///< OSC 12 red override (0–255), or -1 if no override.
        float cursorColorG { -1.0f };///< OSC 12 green override (0–255), or -1 if no override.
        float cursorColorB { -1.0f };///< OSC 12 blue override (0–255), or -1 if no override.
        int scrollOffset { 0 };///< Lines scrolled back (0 = live view; cursor hidden when > 0).
        bool cursorBlinkOn { true };///< Current blink phase (true = visible half of cycle).
        bool cursorFocused { false };///< True if the terminal component has keyboard focus.
        Glyph cursorGlyph;///< Pre-built glyph instance for user cursor (shape 0).
        bool hasCursorGlyph { false };///< True when cursorGlyph is valid (shape 0 or cursor.force).
        bool cursorGlyphIsEmoji { false };///< True when cursorGlyph lives in the emoji (RGBA) atlas.
        float cursorDrawColorR { 1.0f };///< Final resolved cursor colour red [0, 1] (theme or OSC 12).
        float cursorDrawColorG { 1.0f };///< Final resolved cursor colour green [0, 1] (theme or OSC 12).
        float cursorDrawColorB { 1.0f };///< Final resolved cursor colour blue [0, 1] (theme or OSC 12).
        int gridWidth { 0 };///< Grid width in columns at the time of snapshot.
        int gridHeight { 0 };///< Grid height in rows at the time of snapshot.
        uint64_t dirtyRows[4] {};///< Bitmask of rows that changed this frame (bit per row, max 256 rows).
        int scrollDelta { 0 };///< Lines scrolled up since last frame (positive = scrolled up).
        int physCellHeight { 0 };///< Physical (HiDPI-scaled) cell height in pixels.
    };

    /**______________________________END OF NAMESPACE______________________________*/
};// struct Render

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
