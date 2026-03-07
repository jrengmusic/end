# SPEC: Font Fallback, NF Constraint System, and Box Drawing

**Status:** DRAFT
**Date:** 2026-03-05
**Author:** COUNSELOR (for ARCHITECT review)

---

## Problem Statement

Display Mono currently bundles NF glyphs via `build_monolithic.py` + NF patcher. The NF patcher scales icons to fit Display Mono's em-square metrics, producing icons that are visually smaller than the same icons in other terminals (Kitty, Ghostty, WezTerm). The terminal has no runtime control over icon sizing because the scaling is baked into the font file.

Additionally, END has no font fallback chain. Missing glyphs (CJK, exotic Unicode) render as tofu. Box drawing characters (U+2500-U+259F) are rendered from the font rather than procedurally, causing alignment artifacts at fractional cell sizes.

## Solution Overview

Three subsystems:

1. **Font Fallback Chain** -- ordered collection of font faces, OS-level fallback discovery
2. **NF Constraint System** -- per-glyph scaling/positioning table applied at rasterization time
3. **Procedural Box Drawing** -- pixel-perfect box/block characters rendered to the atlas programmatically

---

## 1. Font Fallback Chain

### Architecture

```
FontCollection
  slot 0: Display Mono (embedded, BinaryData)     -- text, Noto symbols, Noto emoji mono
  slot 1: Symbols Nerd Font (embedded, BinaryData) -- NF icons (PUA + standard overrides)
  slot 2..N: OS fallback fonts (discovered at runtime) -- CJK, exotic scripts
  slot N+1: Apple Color Emoji (macOS) / Noto Color Emoji (Linux) -- color emoji
```

### New struct: FontCollection

Lives in `Source/terminal/rendering/FontCollection.h`.

Follows the same `.h` / `.cpp` / `.mm` platform split as `GlyphAtlas`:
- `.mm` contains macOS CoreText fallback discovery (`CTFontCreateForString`)
- `.cpp` contains `#if JUCE_MAC` (empty) / `#else` Linux fontconfig discovery / `#endif`
- `.h` contains the struct definition and cache

```cpp
// MESSAGE THREAD
struct FontCollection
{
    struct Entry
    {
    #if JUCE_MAC
        void* ctFont { nullptr };       // CTFontRef (non-owning, Fonts owns lifetime)
    #else
        FT_Face ftFace { nullptr };     // Non-owning, Fonts owns lifetime
    #endif
        hb_font_t* hbFont { nullptr };  // Non-owning, Fonts owns lifetime
        bool hasColorGlyphs { false };
    };

    static_assert (std::is_trivially_copyable_v<Entry>);

    static constexpr int MAX_FALLBACK_FONTS { 32 };

    // Returns the first entry that has a glyph for this codepoint.
    // Returns non-owning pointer into internal array, valid for FontCollection lifetime.
    // Returns nullptr if no font has it.
    Entry* getFontForCodepoint (uint32_t codepoint) noexcept;

    // Add a non-owning font reference at the next available slot.
    void addFont (Entry entry) noexcept;

    int getCount() const noexcept { return entryCount; }

private:
    std::array<Entry, MAX_FALLBACK_FONTS> entries;
    int entryCount { 0 };

    // Lazily populated. Cleared on font size change or font reload.
    std::unordered_map<uint32_t, int> resolveCache;
};
```

**Glyph existence check:**
- macOS: `CTFontGetGlyphsForCharacters (font, &unichar, &glyph, 1)` -- returns false if missing
- Linux: `FT_Get_Char_Index (face, codepoint)` -- returns 0 if missing

### Fallback Discovery (macOS)

On cache miss after exhausting all loaded fonts:

```objc
// Worker thread (not render thread)
CTFontRef fallback = CTFontCreateForString (primaryCtFont, cfString, range);
```

For CJK range (U+4E00-U+9FFF), use `CTFontCreateForString` directly (locale-aware, picks PingFang/Hiragino based on system language). For other codepoints, same API.

Exclude `LastResort` font by checking PostScript name.

**Startup cascade list** (built once, used as pre-sorted fallback candidates):
```objc
CFArrayRef cascade = CTFontCopyDefaultCascadeListForLanguages (primaryCtFont, languages);
```

### Fallback Discovery (Linux)

Use fontconfig `FcFontMatch` with a charset pattern containing the missing codepoint.

### Integration with Existing Code

`Fonts` struct gains a `FontCollection collection` member. `Fonts::getFontHandle (Style)` remains for the primary font. New method:

```cpp
// Returns the best font entry for this codepoint, searching the fallback chain.
// Non-owning pointer, valid for Fonts lifetime. Returns nullptr if no font has it.
FontCollection::Entry* Fonts::getFontForCodepoint (uint32_t codepoint, Style style) noexcept;
```

`buildCellInstance` in `ScreenRender.cpp` calls `getFontForCodepoint` instead of `getFontHandle` to determine which font renders each cell. The null check follows the existing pattern in `buildCellInstance` (ScreenRender.cpp:224-229).

---

## 2. NF Constraint System

### Why

NF icons are designed as a collection with varying natural sizes. Without constraints, each icon scales independently based on its own bounding box, producing inconsistent sizes. The constraint system normalizes scaling across icon groups and controls alignment/padding per-glyph.

### Constraint Struct

Lives in `Source/terminal/rendering/GlyphConstraint.h`.

```cpp
struct GlyphConstraint
{
    enum class ScaleMode : uint8_t
    {
        none,           // Do not resize
        fit,            // Scale DOWN only to fit cell, preserve aspect ratio
        cover,          // Scale to fill cell, preserve aspect ratio
        adaptiveScale,  // fit if wider than 1 cell, cover if narrower (default for most NF icons)
        stretch         // Stretch to fill both axes (no AR preservation)
    };

    enum class Align : uint8_t
    {
        none,       // Do not move
        start,      // Left or bottom edge
        end,        // Right or top edge
        center      // Center in cell
    };

    enum class HeightRef : uint8_t
    {
        cell,       // Use full cell height (for powerline/box that must tile)
        icon        // Use icon height (ascender to descender, for most NF icons)
    };

    ScaleMode scaleMode { ScaleMode::none };
    Align alignH { Align::none };
    Align alignV { Align::none };
    HeightRef heightRef { HeightRef::cell };

    float padTop { 0.0f };
    float padLeft { 0.0f };
    float padRight { 0.0f };
    float padBottom { 0.0f };

    // Scale group: this glyph's bbox relative to its group's union bbox.
    // Default 1.0/1.0/0.0/0.0 = glyph IS the group (no grouping).
    // 1.0 = full group dimension, 0.0 = no offset from group origin.
    float relativeWidth { 1.0f };
    float relativeHeight { 1.0f };
    float relativeX { 0.0f };
    float relativeY { 0.0f };

    // Cap on (scaled_width / scaled_height) after scaling.
    // Prevents over-wide powerline arrows on tall cells.
    // 0.0 = no cap applied.
    float maxAspectRatio { 0.0f };

    // Max cells this glyph may span horizontally (1 or 2).
    uint8_t maxCellSpan { 2 };

    bool isActive() const noexcept
    {
        return scaleMode != ScaleMode::none;
    }
};

static_assert (std::is_trivially_copyable_v<GlyphConstraint>);
```

### Generated Constraint Table

**Generator script:** `gen/nf_constraint_gen.py`

**Input:**
- `___display___/FontPatcher/font-patcher` -- parsed via AST to extract `setup_patch_set()` attribute dicts and `ScaleRules`
- `___display___/FontPatcher/src/glyphs/` -- source glyph fonts for bounding box measurement (via fontTools)
- Symbols Nerd Font Regular `.ttf` (the actual NF font we embed) -- for measuring final glyph bounding boxes in scale groups

**Output:** `gen/GlyphConstraintTable.cpp`

```cpp
// GENERATED FILE -- do not edit
// Generated by gen/nf_constraint_gen.py from NF patcher v3.4.0

#include "GlyphConstraint.h"

// Declared in GlyphConstraint.h
GlyphConstraint getGlyphConstraint (uint32_t codepoint) noexcept
{
    // C++17: no designated initializers. Each arm returns a populated struct.
    // Fields not listed use default values from GlyphConstraint{}.
    switch (codepoint)
    {
        // Powerline arrow tips
        case 0xe0b0:
        {
            GlyphConstraint c;
            c.scaleMode = GlyphConstraint::ScaleMode::stretch;
            c.alignH = GlyphConstraint::Align::start;
            c.alignV = GlyphConstraint::Align::center;
            c.heightRef = GlyphConstraint::HeightRef::cell;
            c.padLeft = -0.03f;
            c.padRight = -0.03f;
            c.maxAspectRatio = 0.7f;
            c.maxCellSpan = 1;
            return c;
        }

        // ... ~277 switch arms covering ~9000 codepoints ...

        default:
            return {};  // scaleMode = none, no constraint
    }
}
```

**Generator algorithm** (mirrors Ghostty's `nerd_font_codegen.py`):

1. Parse `font-patcher` via `ast` module, extract `setup_patch_set()` method
2. Build symbol table from variable assignments, resolve `self.patch_set` list
3. For each patch set with `ScaleRules.ScaleGroups`:
   - Open the embedded Symbols NF font via fontTools
   - Measure each glyph's bounding box with `BoundsPen`
   - Compute union bounding box per group
   - Compute `relativeWidth/Height/X/Y` for each member
4. Translate NF patcher attributes to `GlyphConstraint` fields:
   - `"pa"` without `"!"` -> `adaptiveScale`
   - `"pa"` with `"!"` or overlap > 0 -> `cover`
   - `"xy"` -> `stretch`
   - `"^"` absent -> `heightRef = icon`
   - `"1"` present -> `maxCellSpan = 1`
   - `overlap` -> negative padding (capped at 0.01 vertically)
   - `ypadding` -> symmetric top/bottom padding
   - `xy-ratio` -> `maxAspectRatio`
5. Group codepoints by identical constraint, coalesce ranges
6. Emit C++ switch statement

### Constraint Application

Applied in `GlyphAtlas::rasterizeGlyph` BEFORE rasterization. The atlas stores the already-constrained bitmap.

**New parameters to `rasterizeGlyph`:**

```cpp
AtlasGlyph rasterizeGlyph (const GlyphKey& key, void* fontHandle,
                            bool isEmoji, const GlyphConstraint& constraint,
                            int cellWidth, int cellHeight, int iconHeight) noexcept;
```

**Algorithm** (same as Ghostty's `face.zig::renderGlyph`):

1. Load glyph, get natural bounding box (`width`, `height`, `bearingX`, `bearingY`)
2. If `constraint.isActive()`:
   a. Reconstruct scale group bounding box from `relative*` fields
   b. Compute target box from cell dimensions minus padding
   c. Compute scale factors based on `ScaleMode` enum (fit/cover/stretch/adaptiveScale)
   d. Apply `maxAspectRatio` cap
   e. Scale the glyph outline points (CoreText: use `CGAffineTransform`; FreeType: `FT_Outline_Transform`)
   f. Compute aligned position within cell based on `Align` enums
3. Rasterize at the constrained size
4. Store in atlas with adjusted bearings

**GlyphKey change:**

```cpp
struct GlyphKey
{
    uint32_t glyphIndex { 0 };
    void* fontFace { nullptr };
    float fontSize { 0.0f };
    uint8_t cellSpan { 0 };  // 0 = no constraint, 1 or 2 = constrained cell span
};
```

`cellSpan` in the key ensures the cache distinguishes between single-cell and double-cell renders of the same glyph.

### Cell Span Detection

When rendering a cell, check if the next cell is whitespace (space or empty). If so, `cellSpan = 2` (icon may span 2 cells). Otherwise `cellSpan = 1`.

This is the mechanism that makes NF icons larger when followed by whitespace -- exactly the behavior observed in Kitty/Ghostty.

In `buildCellInstance`:

```cpp
const GlyphConstraint constraint { getGlyphConstraint (cell.codepoint) };
uint8_t cellSpan { 1 };

if (constraint.isActive() and constraint.maxCellSpan >= 2)
{
    // Check if next cell is whitespace
    if (col + 1 < cols)
    {
        // HeapBlock has no .at() -- index validated by col + 1 < cols guard above
        const Cell& next { hotCells[row * cols + col + 1] };

        if (not next.hasContent() or next.codepoint == 0x20)
        {
            cellSpan = 2;
        }
    }
}
```

### Symbols Nerd Font Embedding

**Font file:** Download `SymbolsNerdFont-Regular.ttf` (NOT Mono -- the Regular variant's metrics are not pre-adjusted, giving our constraint system full control).

**Location:** `Source/fonts/SymbolsNerdFont-Regular.ttf`

**CMake:** Already globs `Source/fonts/*.ttf` for BinaryData (line 227-230 of CMakeLists.txt). Adding the file is sufficient.

**Registration:**
- macOS: `CTFontManagerRegisterGraphicsFont` (same as Display Mono)
- Linux: `FT_New_Memory_Face` from BinaryData

**Fallback chain position:** Slot 1 (after Display Mono, before OS system fonts).

---

## 3. Procedural Box Drawing

### Why

Box drawing characters (U+2500-U+259F) must align pixel-perfectly at cell boundaries. Font-rendered box glyphs have fractional-pixel offsets and inconsistent stroke widths that cause visible gaps at cell edges. All mature terminals (Kitty, Ghostty) render these procedurally.

### Current State

END already has `BLOCK_TABLE` in `Screen.h` covering U+2580-U+2593 (block elements) rendered as colored rectangles via the background shader. This needs to be extended to cover the full box drawing range.

### Scope

| Range | Name | Count | Rendering |
|-------|------|-------|-----------|
| U+2500-U+257F | Box Drawing | 128 | Procedural lines to atlas |
| U+2580-U+259F | Block Elements | 32 | Already done (BLOCK_TABLE) |
| U+2800-U+28FF | Braille Patterns | 256 | Procedural dots to atlas |
| U+E0B0-U+E0BF | Powerline Separators | 16 | Procedural to atlas (triangles, arcs) |
| U+1FB00-U+1FBAE | Symbols for Legacy Computing | 175 | Procedural to atlas |

### Architecture

New file: `Source/terminal/rendering/BoxDrawing.h` / `BoxDrawing.cpp`

```cpp
struct BoxDrawing
{
    // Returns true if this codepoint should be rasterized procedurally.
    static bool isProcedural (uint32_t codepoint) noexcept;

    // Rasterize a procedural glyph into a pixel buffer.
    // Buffer is cellWidth x cellHeight, single channel (grayscale).
    // Returns false if codepoint is not a procedural glyph.
    static bool rasterize (uint32_t codepoint,
                           uint8_t* buffer,
                           int cellWidth, int cellHeight,
                           int strokeWeight) noexcept;
};
```

**Integration:** In `buildCellInstance`, before font resolution:

```cpp
if (BoxDrawing::isProcedural (cell.codepoint))
{
    // Rasterize to atlas via BoxDrawing::rasterize, skip font shaping entirely
}
```

Procedural glyphs go into the mono atlas like any other glyph. They use a special `GlyphKey` with `fontFace = nullptr` and `glyphIndex = codepoint` to distinguish from font-rendered glyphs.

### Powerline Separators

Powerline arrow tips (U+E0B0-U+E0B3) and rounded arcs (U+E0B4-U+E0B7) are rendered procedurally for pixel-perfect tiling. These are simple geometric shapes (triangles, half-circles) that must exactly fill the cell with optional overlap for seamless tiling.

Procedural rendering takes priority over the NF constraint system. `BoxDrawing::isProcedural` is checked first in the render pipeline. The constraint table also marks these as `stretch` with overlap, but procedural rendering is more precise and avoids font rasterization entirely.

---

## 4. Ligature Rendering

### Why

Display Mono has 12 ligatures registered as `calt` (contextual alternates) in the GSUB table:
- 2-char: `->`, `=>`, `==`, `!=`, `>=`, `<=`, `&&`, `||`, `::`, `<<`, `>>`
- 3-char: `<=>`

HarfBuzz reads the GSUB table and substitutes ligature glyphs automatically during `hb_shape`. But the current renderer shapes each cell independently (`buildCellInstance` calls `shapeText` with a single codepoint). HarfBuzz never sees `->` as a sequence -- it sees `-` then `>` in separate calls. Ligatures require shaping consecutive cells together.

### Current Problem

```
Cell[col=5] codepoint='-'  -> shapeText("-")  -> glyph for '-'
Cell[col=6] codepoint='>'  -> shapeText(">")  -> glyph for '>'
```

HarfBuzz needs to see both codepoints in one call to trigger the `calt` substitution.

### Solution: Ligature Run Detection in buildSnapshot

In `buildSnapshot`, before calling `processCellForSnapshot` per-cell, scan each dirty row for ligature-eligible runs. A ligature run is a sequence of consecutive ASCII cells (same style, not emoji, not wide) that should be shaped together.

**Algorithm:**

1. Walk cells left-to-right in the row
2. Accumulate consecutive ASCII cells with matching style into a run buffer
3. When the run breaks (different style, emoji, wide char, end of row):
   - If `ligatureEnabled` and run length >= 2: shape the entire run via `shapeHarfBuzz`
   - If run length == 1 or ligatures disabled: shape per-cell (existing path)
4. HarfBuzz returns the shaped glyphs. If a ligature fired, the output glyph count is less than the input codepoint count (e.g., 2 codepoints `->` produce 1 ligature glyph).
5. The ligature glyph occupies the first cell's position. The second cell is skipped (no glyph emitted, but background is still rendered).

**Key detail:** The ligature glyph's `xAdvance` from HarfBuzz spans the full width of all consumed cells. The renderer must not advance `currentX` by one cell per glyph -- it must use the HarfBuzz-reported `xAdvance`.

### Integration Point

The change is in `buildSnapshot` / `processCellForSnapshot`. Instead of the current per-cell loop:

```cpp
for (int c { 0 }; c < cols; ++c)
{
    processCellForSnapshot (hotCells[index], c, r);
}
```

The new loop detects runs and shapes them together when ligatures are enabled. When ligatures are disabled, the loop is identical to the current one (no overhead).

### `ligatureEnabled` Flag

Already exists:
- `Screen.h:336` -- `bool ligatureEnabled { false };`
- `Screen.cpp:111-113` -- `setLigatures (bool enabled)`
- `Config.h:11` -- `font.ligatures` config key

Currently the flag is set but never read during rendering. This spec connects it.

### Edge Cases

- **Mixed styles within a potential ligature:** `=` in bold + `>` in regular -- do NOT form a ligature. Style must match across the run.
- **Cursor on ligature cell:** If cursor is on the second cell of a `->` ligature, the ligature should still render. Cursor overlay is independent.
- **Selection across ligature:** Selection highlighting is per-cell, independent of ligature shaping. No special handling needed.
- **Wide characters breaking runs:** A wide (CJK) character or its continuation cell breaks the run.
- **Emoji breaking runs:** An emoji cell breaks the run.

---

## 5. Rendering Pipeline Changes

### Current Flow

```
for each cell in row:
    Cell -> selectFontStyle -> getFontHandle -> shapeText (1 codepoint)
                                                    |
                                          emitShapedGlyphsToCache
                                                    |
                                          GlyphAtlas::getOrRasterize
                                                    |
                                          rasterizeGlyph (font metrics only)
                                                    |
                                          atlas bitmap -> GPU
```

### New Flow

```
for each dirty row:
    scan for ligature runs (consecutive ASCII, same style)
        |
    for each run or single cell:
        |
        BoxDrawing::isProcedural? --yes--> BoxDrawing::rasterize -> atlas -> GPU
            |
            no
            |
        getGlyphConstraint (codepoint) -> constraint
            |
        getFontForCodepoint (codepoint, style) -> FontCollection::Entry
            |
        shapeText / shapeHarfBuzz (single codepoint or ligature run)
            |
        emitShapedGlyphsToCache (with constraint + cell dimensions)
            |
        GlyphAtlas::getOrRasterize (with constraint)
            |
        rasterizeGlyph (constraint applied before rasterization)
            |
        atlas bitmap (already at correct size/position) -> GPU
```

### Cell Layout Flag

No new `Cell.layout` bit needed. The constraint table lookup (`getGlyphConstraint`) is a switch statement that returns `ScaleMode::none` for non-NF codepoints. The switch is effectively free (single branch prediction miss on first encounter, then predicted correctly). Adding a layout bit would duplicate information already in the constraint table, violating SSOT.

---

## 5. File Inventory

### New Files

| File | Purpose |
|------|---------|
| `Source/terminal/rendering/FontCollection.h` | Ordered fallback font collection |
| `Source/terminal/rendering/FontCollection.cpp` | Resolution cache, fallback discovery |
| `Source/terminal/rendering/FontCollection.mm` | macOS CoreText fallback discovery |
| `Source/terminal/rendering/GlyphConstraint.h` | Constraint struct definition |
| `Source/terminal/rendering/BoxDrawing.h` | Procedural glyph detection |
| `Source/terminal/rendering/BoxDrawing.cpp` | Procedural glyph rendering |
| `gen/nf_constraint_gen.py` | Constraint table generator |
| `gen/GlyphConstraintTable.cpp` | Generated constraint switch table |
| `Source/fonts/SymbolsNerdFont-Regular.ttf` | Embedded NF font |

### Modified Files

| File | Changes |
|------|---------|
| `Fonts.h` | Add `FontCollection collection` member, `getFontForCodepoint` method |
| `Fonts.cpp` / `Fonts.mm` | Register Symbols NF, populate collection |
| `GlyphAtlas.h` | `GlyphKey` gains `cellSpan`, `rasterizeGlyph` gains constraint params |
| `GlyphAtlas.cpp` | FreeType constraint application (outline transform) |
| `GlyphAtlas.mm` | CoreText constraint application (CGAffineTransform) |
| `ScreenRender.cpp` | Procedural glyph path, constraint width detection, use `getFontForCodepoint`, ligature run detection |
| `Screen.h` | Keep `BLOCK_TABLE` as-is (block elements stay in background shader) |
| `CMakeLists.txt` | No change needed (already globs `.ttf`) |

### Removed from build_monolithic.py

The NF patcher step is removed from `___display___/build_monolithic.py`. Display Mono keeps:
- Base glyphs (98 + 12 ligatures)
- Noto Sans Symbols 2 (1248 glyphs)
- Noto Emoji monotone (87 glyphs)

Total: ~1,400 glyphs (down from ~12,000).

---

## 6. Implementation Order

### Phase 1: NF Constraint System (highest impact)

1. Write `gen/nf_constraint_gen.py` -- generate constraint table from NF patcher data
2. Write `GlyphConstraint.h` -- constraint struct
3. Add `gen/GlyphConstraintTable.cpp` to build
4. Embed `SymbolsNerdFont-Regular.ttf` in `Source/fonts/`
5. Modify `GlyphAtlas` to accept and apply constraints at rasterization
6. Modify `GlyphKey` to include `cellSpan`
7. Modify `ScreenRender.cpp` to look up constraints and pass to atlas
8. Test: OMP prompt with NF icons should render at correct size

### Phase 2: Font Fallback Chain

1. Write `FontCollection.h/.cpp/.mm`
2. Integrate into `Fonts` -- populate collection on init
3. Implement `getFontForCodepoint` with cache
4. Implement macOS fallback discovery (`CTFontCreateForString`)
5. Modify `buildCellInstance` to use `getFontForCodepoint`
6. Test: CJK characters should render from system fonts

### Phase 3: Ligature Rendering

1. Add ligature run detection in `buildSnapshot` row loop
2. Shape runs via `shapeHarfBuzz` when `ligatureEnabled` is true
3. Handle ligature glyph placement (first cell gets glyph, subsequent cells skipped)
4. Test: `->`, `=>`, `==`, `!=`, `<=>`  should render as single ligature glyphs
5. Test: ligatures disabled via config should render as separate characters

### Phase 4: Procedural Box Drawing

1. Write `BoxDrawing.h/.cpp` -- line rendering for U+2500-U+257F
2. Integrate into `buildCellInstance` (procedural path before font path)
3. Add Braille patterns (U+2800-U+28FF)
4. Add Powerline separators (U+E0B0-U+E0BF) -- optional, NF constraint may suffice
5. Test: `box_drawing_test.sh` output should show seamless lines

### Phase 5: Cleanup

1. Remove NF patcher step from `build_monolithic.py`
2. Rebuild Display Mono without NF glyphs
3. Verify all NF icons come from Symbols NF via constraint system
4. Update config defaults if needed

---

## 7. Edge Cases

### Constraint + Zoom

When zoom is active, `cellWidth` and `cellHeight` passed to the constraint system are the zoomed values. The constraint scales relative to the cell, so zoom works correctly without special handling. The `GlyphKey` already includes `fontSize` which changes with zoom, so cached glyphs are invalidated.

### Constraint + Display Scale (Retina)

Constraints operate in physical pixels. `cellWidth`/`cellHeight` passed to the constraint system are `physCellWidth`/`physCellHeight` (already scaled by display factor).

### Missing NF Font

If `SymbolsNerdFont-Regular.ttf` is not in BinaryData (build error), NF codepoints fall through to OS font discovery. Most will render as tofu since PUA codepoints are not in system fonts. This is acceptable degradation -- the font should always be embedded.

### Ambiguous Width Characters

Default to single-width. Config option `ambiguous_width_as_wide` (future) can override.

### Color Emoji in NF Range

Some NF codepoints overlap with Unicode emoji (e.g., U+2665 Heart). If the cell has `LAYOUT_EMOJI` flag (already exists in `Cell.h`), skip NF constraint and render as color emoji. The emoji check happens before constraint lookup in the render pipeline.

---

## 8. Success Criteria

- [ ] OMP prompt NF icons render at the same visual size as in Kitty/Ghostty
- [ ] Powerline separators tile seamlessly with no gaps
- [ ] CJK characters render from system fonts (not tofu)
- [ ] Color emoji render correctly (not affected by constraint system)
- [ ] Box drawing lines are pixel-perfect at cell boundaries
- [ ] No regression in ASCII text rendering performance
- [ ] Constraint table covers all NF 3.4.0 codepoints (~9000)
- [ ] Font fallback cache prevents repeated OS font discovery calls
- [ ] Ligatures `->`, `=>`, `==`, `!=`, `<=>`  render as single glyphs when enabled
- [ ] Ligatures disabled via `font.ligatures = false` renders separate characters
- [ ] Mixed-style sequences do not form ligatures
