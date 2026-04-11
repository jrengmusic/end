# RFC — Fallback Glyph Advance Width Break

**Status:** Open — handoff to COUNSELOR
**Date:** 2026-04-11
**Scope:** `modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp`
**Severity:** Visible terminal grid break on any codepoint that falls through to system fallback

---

## Observation

On Windows, characters like `→` (U+2192), `↔` (U+2194), and other arrows render with the next character crashing into them — the glyph is too close to its right-hand neighbour. Visible in END's own terminal output when running with Display Mono.

## Evidence Chain

1. **Display Mono** (`___display___/svg-mono/GlyphSheet_Display-MONO-*.svg`) — does not carry arrow glyphs. `build_fonts.py:416-477` iterates `<g id="_XXXX">` cells; there were no `_2190`–`_2194` cells at time of discovery. *(This sprint added empty cells for designer fill-in; the runtime-present gap is separate.)*

2. **Symbols Nerd Font** (`Source/fonts/SymbolsNerdFont-Regular.ttf`) — carries NF icons, not Unicode arrows. `FT_Get_Char_Index` returns 0 for U+2190–U+2194.

3. **`BoxDrawing` procedural rasterizer** (`modules/jreng_graphics/fonts/jreng_box_drawing.h:110-115`):
   ```cpp
   static bool isProcedural (uint32_t codepoint) noexcept
   {
       return (codepoint >= 0x2500 and codepoint <= 0x257F)
           or (codepoint >= 0x2580 and codepoint <= 0x259F)
           or (codepoint >= 0x2800 and codepoint <= 0x28FF);
   }
   ```
   Covers box drawing, block elements, and braille. Arrows are outside the range.

4. **System fallback path** (`modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp:269-303`) — on cache miss, calls `discoverFallbackFace(cp)` which on Windows queries DirectWrite's `IDWriteFactory2::GetSystemFontFallback()` + `MapCharacters()` to find a system font covering the codepoint (`modules/jreng_graphics/fonts/jreng_typeface.cpp:773-968`). DirectWrite picks a proportional system font (Segoe UI Symbol, Cambria Math, etc.), `FT_New_Face` loads it, `FT_Set_Char_Size` sizes it.

## Root Cause

`modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp:308-313`:

```cpp
float advance { static_cast<float> (activeFace->size->metrics.x_ppem) };

if (FT_Load_Glyph (activeFace, activeIndex, FT_LOAD_DEFAULT) == 0)
{
    advance = static_cast<float> (activeFace->glyph->metrics.horiAdvance) / static_cast<float> (ftFixedScale);
}
```

The advance is read from whatever face `FT_Load_Glyph` landed on — primary, emoji, NF, or system fallback. For the fallback path, that face is a proportional font whose `horiAdvance` has no relationship to END's monospace cell width. The terminal cell grid is broken: the glyph renders at its native advance (typically narrower than the cell), and the next character is placed immediately after it, overlapping or crashing the grid.

## Scope of Impact

Not arrow-specific. Any codepoint absent from:
- Primary face (Display Mono)
- Emoji face
- NF face (`SymbolsNerdFontRegular`)
- `BoxDrawing` procedural ranges

...will route through `shapeFallback` → `discoverFallbackFace` → proportional system font → wrong advance. Arrows were the visible trigger; the class of bug covers every Unicode range END doesn't ship.

## Proposed Fix Direction

When `activeFace != primaryFace` (i.e., the glyph came from a fallback source), override the advance with the primary face's monospace cell advance. The glyph itself is still rendered from the fallback face — only the horizontal advance is clamped to the cell width so the terminal grid stays intact.

The primary face's cell advance is already computed at `jreng_typeface_metrics.cpp` (scans printable ASCII for `max_advance`); the value is the authoritative cell width END uses for everything else. `shapeFallback` should read it and apply it on fallback glyphs.

Left-side bearing: glyph's native LSB can stay (left-aligned in cell), or center the glyph by computing `(cell_advance - glyph_width) / 2` and adjusting the offset. Design choice for COUNSELOR.

## BLESSED Alignment

- **S (SSOT):** cell advance is already owned by `jreng_typeface_metrics.cpp`. Current code duplicates the "where does advance come from" decision across code paths — primary path trusts the metrics, fallback path trusts the fallback face. That's a shadow state violation — fallback code should reference the same SSOT.
- **D (Deterministic):** same codepoint, same cell, same advance — regardless of which face sourced the glyph. Currently the advance depends on which DirectWrite-selected system font happens to cover the codepoint — non-deterministic across systems and language packs.

## Files for COUNSELOR's Attention

- `modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp:236-340` — `shapeFallback` implementation, advance logic at 308-313
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp` — cell advance SSOT
- `modules/jreng_graphics/fonts/jreng_typeface.cpp:773-968` — Windows fallback face discovery (for context; not the bug site)
- `modules/jreng_graphics/fonts/jreng_box_drawing.h:110-115` — procedural codepoint coverage (for context)

## Out of Scope Here

Adding arrow glyphs to Display Mono (handled in the current sprint by populating empty cells in `GlyphSheet_Display-MONO-{Book,Medium,Bold}.svg` for the designer to fill). That fix removes the **symptom** for arrows specifically; this RFC targets the **class of bug** so any future missing codepoint renders with correct cell advance.

---
**End of RFC**
