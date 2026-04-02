# PLAN: Font Metrics Fixes

**Date:** 2026-04-02
**From:** COUNSELOR analysis session

Two independent issues. Each gets its own sprint with a separate COUNSELOR.

---

## Issue A: calcMetrics missing render DPI restore

**Bug:** `jreng_typeface_metrics.cpp:214` — `FT_Set_Char_Size(face, 0, height26_6, baseDpi, baseDpi)` sets the face to 96 DPI. The documented step 8 (line 171-173) that restores to `baseDpi * displayScale` does not exist in code.

**Call chain:**
```
setFontSize(pt)
  font.setSize(pt)          -> face at renderDpi (96 * scale)
  calc()
    calcMetrics(pt)          -> face at baseDpi (96) -- NEVER RESTORED
```

**Effect:** All subsequent `FT_Load_Glyph` calls rasterize at 96 DPI. On scale > 1.0, glyphs are `1/scale` the expected physical size. Severity scales with DPI factor:
- 125% (ROG Ally): 20% too small — subtle
- 175% (UTM): 43% too small — obvious

**Fix:** After reading logical metrics at baseDpi, restore face to renderDpi:
```cpp
// After line 245 (physBaseline assignment), before closing brace:
const FT_UInt renderDpi { static_cast<FT_UInt> (static_cast<float> (baseDpi) * displayScale) };
FT_Set_Char_Size (face, 0, height26_6, renderDpi, renderDpi);
```

**Also affects:** `shapeASCII()` at `jreng_typeface_shaping.cpp:116` reads `face->size->metrics.max_advance` — after the fix this will correctly return physical-resolution advance.

**Validation:** Must be debugged on the Windows UTM VM. Steps:
1. Add temporary debug output in `calcMetrics` — print `displayScale`, `baseDpi`, `renderDpi`, `logicalCellW`, `logicalCellH`, `physCellW`, `physCellH`
2. Confirm `displayScale` returns 1.75 on UTM
3. Apply the DPI restore fix
4. Confirm glyph bitmaps now match cell dimensions visually
5. Test on both CPU and GPU renderer paths
6. Remove debug output

**Files to modify:**
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:196-254` — add render DPI restore after metric computation

---

## Issue B: Cell aspect ratio — wide and short vs other terminals

**Observation:** Cell aspect ratio appears wider and shorter than Kitty, Ghostty, WezTerm.

**Data from competitor source code (FreeType paths):**

### Cell height source
| Terminal | Source | Includes line gap? |
|----------|--------|--------------------|
| **end** | `face->size->metrics.height` | Maybe not — FreeType doc: "ascender minus descender" |
| **Kitty** | `FT_MulFix(face->height, y_scale)` | Yes — `face->height` is design-unit line height including gap |
| **Ghostty** | `hhea.ascender - hhea.descender + hhea.lineGap` (table read) | Yes — explicit line_gap |
| **WezTerm** | `face->height * y_scale / 64` | Yes — same as Kitty, design-unit height |

### Line spacing config
| Terminal | Config key | Default |
|----------|-----------|---------|
| **end** | None | No adjustment possible |
| **Kitty** | `modify_font cell_height` | 0 (px/pt/% additive) |
| **Ghostty** | `adjust-cell-height` | 0 (additive integer) |
| **WezTerm** | `line_height` | 1.0 (multiplier) |

### Rounding
| Terminal | Width | Height |
|----------|-------|--------|
| **end** | `ceil` | `ceil` |
| **Kitty** | `ceil` | `ceil` |
| **Ghostty** | `round` | `round` |
| **WezTerm** | `ceil` | `ceil` |

**Root cause candidates:**
1. `face->size->metrics.height` may exclude line gap that Kitty/WezTerm/Ghostty include via `face->height`
2. No user-configurable line spacing to compensate

**Investigation needed:** Compare actual numeric values of `face->size->metrics.height` vs `FT_MulFix(face->height, y_scale)` for Display Mono Book at the same point size. The delta is the missing line gap.

**This requires a separate COUNSELOR sprint** — needs numeric comparison with the embedded font, decision on which metric source to use, and whether to add a `cell_height` / `line_height` config key.
