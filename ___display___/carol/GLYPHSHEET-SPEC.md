# GLYPHSHEET FORMAT SPEC v2

**Status:** ACTIVE
**Date:** 2026-03-03

## Overview

A GlyphSheet is an SVG file containing a grid of cells. Each cell holds one glyph (single character or ligature). Designers draw glyph artwork inside cells using vector editors. A build script parses the SVG to extract glyph artwork and compile TTF fonts.

## Design Principles

1. **Self-contained cells** -- each cell `<g>` carries its own identity (`id`) and geometry (`<rect>`). No external lookup needed.
2. **Guides are disposable** -- all visual aids (labels, reflines, centerlines) live in a separate locked layer. Delete them entirely and fonts still build correctly.
3. **XML-native parsing** -- `ElementTree` walks `<g>` elements, reads `<rect>` attributes. No regex. No coordinate math. No heuristics.
4. **Editor-safe** -- relies only on `id` attribute and standard SVG elements. No `data-*` attributes (stripped by Affinity Designer and Plain SVG export).

## SVG Structure

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="W" height="H">

  <!-- Background -->
  <rect width="W" height="H" fill="rgb(20,20,20)"/>

  <!-- Guides layer (lockable, hideable, deletable) -->
  <g id="guides">
    <!-- Per-cell: label + reflines + centerline -->
    <text x="42" y="78" style="font-size:7px;fill:rgb(100,100,100)">A</text>
    <path d="M 40,101 L 160,101" style="fill:none;stroke:rgb(167,139,250);stroke-width:0.5"/>
    <path d="M 40,141 L 160,141" style="fill:none;stroke:rgb(52,211,153);stroke-width:0.5"/>
    <path d="M 40,246 L 160,246" style="fill:none;stroke:rgb(250,204,21);stroke-width:0.5"/>
    <path d="M 40,287 L 160,287" style="fill:none;stroke:rgb(248,113,113);stroke-width:0.5"/>
    <path d="M 100,70 L 100,313" style="fill:none;stroke:rgb(68,68,68);stroke-width:0.5px;stroke-dasharray:2,3"/>
    <!-- ... more labels, reflines, centerlines ... -->
  </g>

  <!-- Cell groups (structural truth) -->
  <g id="_0041">
    <rect x="40" y="70" width="120" height="243"
      style="fill:none;stroke:rgb(50,50,50);stroke-width:0.5"/>
    <!-- glyph artwork drawn by designer goes here -->
  </g>

  <g id="_0042">
    <rect x="170" y="70" width="120" height="243"
      style="fill:none;stroke:rgb(50,50,50);stroke-width:0.5"/>
  </g>

  <!-- Ligature cell (2-cell, wider rect) -->
  <g id="_002D_003E">
    <rect x="40" y="2600" width="250" height="243"
      style="fill:none;stroke:rgb(50,50,50);stroke-width:0.5"/>
  </g>

  <!-- Ligature cell (3-cell) -->
  <g id="_003C_003D_003E">
    <rect x="40" y="3860" width="380" height="243"
      style="fill:none;stroke:rgb(50,50,50);stroke-width:0.5"/>
  </g>

</svg>
```

## Cell Identity

The `id` attribute on each `<g>` uses a leading `_` prefix followed by zero-padded 4-digit uppercase hex Unicode codepoints. The prefix ensures XML-safe ids (XML ids cannot start with a digit) and survives vector editor round-trips (Affinity Designer, Inkscape).

| Character | id |
|---|---|
| `!` | `_0021` |
| `A` | `_0041` |
| `->` | `_002D_003E` |
| `<=>` | `_003C_003D_003E` |

**Delimiter:** `_` (underscore) separates codepoints. First `_` is the prefix.

**Parsing:**
```python
parts = [p for p in g.get("id").split("_") if p]
char = "".join(chr(int(p, 16)) for p in parts)
```

Single part = single character. Multiple parts = ligature.

## Cell Geometry

Each `<g>` contains exactly one `<rect>` as its first child. This rect is the **true coordinate source**.

| Attribute | Meaning |
|---|---|
| `x` | Cell left edge (px) |
| `y` | Cell top edge (px) |
| `width` | Cell width (px) -- `CELL_W` for single, `N*CELL_W + (N-1)*GUTTER` for ligature |
| `height` | Cell height (px) -- always `CELL_H` |

The parser reads these directly. No grid math from config needed at parse time.

**Scale derivation:** The parser still needs `CAPLINE_Y`, `BASELINE_Y`, and `CAP_H` from config to compute the SVG-to-font-units scale factor. But cell position comes from the rect, not from grid math.

## Glyph Artwork

Everything inside a cell `<g>` that is NOT the cell `<rect>` is glyph artwork.

Glyph artwork elements:
- `<path>` with `fill:rgb(224,224,224)` -- filled outlines (most glyphs)
- `<rect>` with `fill:rgb(224,224,224)` -- rectangular glyphs (-, _, |, .)

The parser collects all non-cell-rect children as glyph data.

## Guides Layer

The `<g id="guides">` layer contains all visual aids:
- **Labels** -- `<text>` elements showing the character name
- **Reflines** -- horizontal `<path>` elements (capline, x-height, baseline, descender)
- **Centerlines** -- vertical dashed `<path>` elements

The guides layer is:
- **Lockable** in vector editors (prevents accidental edits)
- **Hideable** (declutters the workspace)
- **Deletable** (fonts build without it)

The build script ignores the guides layer entirely.

## Parser Algorithm

```
1. Parse SVG with xml.etree.ElementTree
2. For each direct child <g> of <svg>:
   a. Read id attribute
   b. Skip if id == "guides" or id is not valid hex codepoint(s)
   c. Split id by "_", convert each part: chr(int(part, 16))
   d. Find first <rect> child -- this is the cell rect
   e. Read x, y, width, height from rect attributes
   f. Collect all other children as glyph artwork:
      - <path> with d attribute -> ("path", d_string)
      - <rect> (non-cell) -> ("rect", (x, y, w, h))
   g. Store: char -> { rect: (x, y, w, h), artwork: [...] }
3. Compute scale from config: CAP_H / (BASELINE_Y - CAPLINE_Y)
4. For each glyph, compute transform from cell rect position + scale
5. Return glyphs dict and scale
```

## Validation

The build script should warn on:
- `<g>` with invalid id (not hex codepoints)
- `<g>` with no `<rect>` child
- `<g>` with `<rect>` but no artwork (empty cell -- not an error, just info)
- Ungrouped `<path>` elements with `fill:rgb(224,224,224)` outside any cell `<g>` (designer drew outside group)

## Generator Output

`generate_unified_sheets.py` produces:
1. Background `<rect>`
2. `<g id="guides">` containing all labels, reflines, centerlines
3. One `<g id="_XXXX">` per character, each containing one `<rect>`
4. One `<g id="_XXXX_YYYY">` per 2-cell ligature
5. One `<g id="_XXXX_YYYY_ZZZZ">` per 3-cell ligature

## Migration

Existing SVGs have flat structure (no groups). Migration script:
1. Parse existing SVG
2. Build cell map from `<text>` labels (existing pattern)
3. Assign glyph artwork `<path>` elements to cells by bounding box center (proven pattern from `transfer_glyphs.py`)
4. Rebuild SVG in new grouped format
5. Verify glyph count matches before/after
