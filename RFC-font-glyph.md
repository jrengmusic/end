# RFC — jam::Font + jam::glyph Module Architecture

Date: 2026-05-13
Status: Ready for COUNSELOR handoff

## Problem Statement

Font-derived rendering geometry (cellWidth, cellHeight, baseline, fontSize) is computed identically at 4 sites, carried as 4 loose scalars through State, and consumed by Screen, CaretComponent, TextEditor independently via 4 separate getters. There is no `jam::Font` type. The `jam::Glyph` struct is a dead namespace anchor — never instantiated at runtime. The shaping layer (`shapeASCII`, `shapeFallback`) enforces monospace by discarding per-glyph advances from HarfBuzz, making the pipeline incompatible with proportional fonts. Cell carries fields (`layout`, `width`) that are codepoint-derivable and belong in the shaping layer.

The design question: what is the correct type system for font identity, glyph rendering, and cell content? Answer: `jam::Font` is the single rendering descriptor (analog of `juce::FontOptions` + `juce::Font`, merged). `jam::glyph` is the namespace for rendering infrastructure. Cell is pure content. Glyph is Font's private implementation detail.

## Research Summary

### Current architecture — duplication and loose scalars

The maxAdvance scan + logCellW/logCellH/logBase derivation is duplicated verbatim at 4 call sites:
- `TerminalDisplay.cpp:66–113` (authoritative — writes to State via `setCellMetrics`)
- `Panes.cpp:55–103` (window sizing)
- `MainComponent.cpp:151–188` (popup sizing)
- `MainComponent.cpp:491–544` (resize overlay)

All follow the same formula: iterate codepoints 32–127, measure `typeface->getAdvanceWidth(cp) * fontSize`, take max, derive `logCellW`, `logCellH`, `logBase` via `jam::toInt(..., true)`.

The 4 values travel as loose scalars: 4 parameters to `setCellMetrics()`, 4 separate `Parameter<int/float>` entries in State, 4 separate getter calls at consumption sites. No bundle type exists.

### Current Glyph struct — dead namespace anchor

`jam::Glyph` (`jam_graphics/jam_glyph.h`) has 6 fields (`codepoint`, `glyphIndex`, `fontHandle`, `style`, `advanceWidth`, `isEmoji`) but zero instantiation sites anywhere in jam or END. It exists solely as a namespace anchor for `Glyph::Atlas`, `Glyph::Graphics`, `Glyph::ShapedText`, `Glyph::Constraint`, `Glyph::Render`.

The fields decompose cleanly:
- Cell-origin (redundant): `codepoint`, `style`
- Font/shaping-internal: `fontHandle`, `glyphIndex`, `advanceWidth`, `isEmoji`

### Current shaping — monospace enforcement in Typeface

Monospace is enforced at the Typeface shaping layer, not the layout layer:

| Path | Advance source | Enforces monospace? |
|------|---------------|-------------------|
| `shapeASCII` (count==1, cp<128) | `face->size->metrics.max_advance >> 6` | **Yes** — ignores per-glyph horiAdvance |
| `shapeFallback` (.notdef from primary) | `primaryFace->size->metrics.max_advance >> 6` | **Yes** — ignores fallback face advance |
| `shapeHarfBuzz` (general path) | `glyphPos[i].x_advance / ftFixedScale` | No — real HarfBuzz advance |
| `shapeEmoji` | `glyphPos[i].x_advance / ftFixedScale` | No — real HarfBuzz advance |

`buildArrangements()` in ShapedText reads only `glyphIndex` from the `GlyphRun` output — `xAdvance`, `xOffset`, `yOffset` are discarded entirely. All positioning uses integer cell columns (`currentCol += pen.width`).

### JUCE Font model — deprecated double layer

| JUCE type | Role | Status |
|-----------|------|--------|
| `juce::FontOptions` | Descriptor/builder (name, style, size, features) | Active |
| `juce::Font` | Resolved wrapper (COW, mutable, lazy typeface) | Deprecated constructors |
| `juce::Typeface` | Font data + shaping (HarfBuzz) | Active |
| `juce::GlyphArrangement` | Bulk layout engine | Active |
| `juce::PositionedGlyph` | Per-glyph positioned data | Active |

### jam analog — single layer

| jam type (new) | JUCE analog | Role |
|---------------|-------------|------|
| `jam::Font` | `FontOptions` + `Font` merged | Single rendering descriptor |
| `jam::Typeface` | `juce::Typeface` | Font data + shaping (exists) |
| `jam::glyph::Arrangement` | `juce::GlyphArrangement` | Bulk layout engine |
| `jam::Font::Glyph` | `juce::PositionedGlyph` | Per-glyph shaped identity (private) |

### Cell field analysis — what's derivable from codepoint

| Cell field | Codepoint-derivable? | Needed by VT logic (Video)? | Disposition |
|-----------|---------------------|---------------------------|-------------|
| `codepoint` | — | yes | stays |
| `style` | no (SGR context) | yes | stays |
| `fg`, `bg` | no (SGR context) | yes | stays |
| `width` | yes (`charPropsFor`) | yes (cursor advance) | stays in Cell for VT, shaping also derives |
| `layout` (EMOJI) | yes (`isEmojiPresentation`) | no (render-only) | moves to shaping |
| `layout` (WIDE_CONT) | from neighbor | yes (grid structure) | stays |
| `layout` (GRAPHEME) | no (needs UAX#29) | yes | stays |

### Monospace vs proportional — no mathematical difference

```
monospace:      position[i] = sum(advances[0..i-1])  where all advances = cellW
proportional:   position[i] = sum(advances[0..i-1])  where advances vary
```

Monospace is proportional with a constant advance. `col * cellW` is the shortcut for `sum(cellW, cellW, ...)`. The shaping layer should produce actual per-glyph advances; for monospace fonts, these are naturally uniform.

## Principles and Rationale

### Why jam::Font

Font-derived rendering geometry belongs in one type. Consumers call `setFont(font)` — one call, one concept. No loose scalars, no manual arithmetic, no 4-site duplication. Font is the analog of `juce::FontOptions` + `juce::Font` merged into a single terminal-aware type.

### Why Font::Glyph is private

Glyph is the shaped product of Font × Cell. Its fields (`fontHandle`, `glyphIndex`, `advanceWidth`, `isEmoji`) are shaping artifacts — implementation details of how Font renders a Cell. No consumer needs them. `font.getImage(atlas, cell)` is the public API; Glyph is internal.

### Why glyph is a namespace

The rendering infrastructure (Arrangement, Graphics, Atlas, Constraint) needs an organizational home. `jam::glyph` (lowercase, namespace convention) replaces the dead `jam::Glyph` struct. These types are rendering pipeline machinery, not Font's private state — they orchestrate how Font's output reaches the screen.

### Why Cell reduces

Cell is content: what character, what style, what color. Rendering geometry (width detection, emoji detection) belongs in the shaping layer where it's derived from codepoints. Cell carries what the VT logic (Video) needs for cursor advancement and grid structure. Anything purely render-side moves to shaping.

### Why shapeASCII fix

`shapeASCII` uses `max_advance` instead of per-glyph advance — a monospace enforcement baked into Typeface. For proportional support, shaping must produce actual advances. For monospace fonts, per-glyph advances are naturally equal to `max_advance`. The fix removes the enforcement; the math stays correct for both cases.

### BLESSED mapping

- **B (Bound):** Font is value type, non-owning `Typeface*`. Glyph is ephemeral (constructed at render time, consumed immediately). Atlas at application lifetime in `Typeface::Context`. Every lifetime clear.
- **L (Lean):** 4 duplication sites → 1 constructor. No boilerplate at call sites. No dead struct.
- **E (Explicit):** Named type replaces 4 loose scalars. `setFont()` replaces 4 setter/getter patterns. No magic values.
- **S (SSOT):** Font IS font truth. One metric computation. One type. Cell IS content truth. No overlap.
- **S (Stateless):** Font is immutable value. Components receive, don't track. Arrangement takes Font per `shape()` call, stores nothing.
- **E (Encapsulation):** Glyph is private to Font. Shaping is Font's internal concern. Components see `setFont()` only. `glyph` namespace scopes implementation.
- **D (Deterministic):** Same Typeface + same fontSize = same Font = same metrics. Same Font + same Cell = same render. Always.

## Scaffold

### Module structure

```
jam_fonts/                                    ← existing module (binary assets + font types)
  ├── jam_fonts.h                             ← module header (updated: adds jam_graphics dependency)
  ├── jam_fonts.cpp                           ← unity build (updated: includes jam_font/ files)
  ├── segment/                                ← existing binary asset subdirectory
  │   ├── jam_fonts_segment.h
  │   └── jam_fonts_segment.cpp
  ├── display/                                ← existing TTF assets
  ├── display_mono/                           ← existing TTF assets
  └── jam_font/                               ← NEW subdirectory (font type + glyph namespace)
      ├── jam_font.h                          ← jam::Font class
      ├── jam_font.cpp                        ← Font implementation (metric resolution, getImage)
      └── glyph/                              ← jam::glyph namespace
          ├── jam_glyph_arrangement.h          ← glyph::Arrangement (was Glyph::ShapedText)
          ├── jam_glyph_arrangement.cpp
          ├── jam_glyph_graphics.h             ← glyph::Graphics (moved from jam_graphics)
          ├── jam_glyph_graphics.cpp
          ├── jam_glyph_atlas.h                ← glyph::Atlas (moved from jam_graphics)
          ├── jam_glyph_atlas.cpp
          ├── jam_glyph_constraint.h           ← glyph::Constraint (moved from jam_graphics)
          └── jam_glyph_constraint_table.cpp
```

`jam_fonts` module dependencies updated: `juce_graphics`, `jam_core`, `jam_graphics` (for Typeface), `jam_tui` (for Cell).

### jam::Font — class definition

```cpp
namespace jam
{

struct Font
{
    // =========================================================================
    // Construction — resolves metrics from Typeface + configuration
    // =========================================================================

    Font() = default;

    /** Primary constructor. Resolves cellWidth, cellHeight, baseline from
     *  typeface metrics and fontSize via maxAdvance scan (ASCII 32–127). */
    Font (Typeface* typeface, float fontSize) noexcept;

    /** Extended constructor with display multipliers. */
    Font (Typeface* typeface, float fontSize,
          float cellWidthMultiplier, float lineHeightMultiplier) noexcept;

    // =========================================================================
    // Styling — returns modified copy (FontOptions pattern)
    // =========================================================================

    /** Returns a Font with the given style flags applied.
     *  Used by TUI Graphics pen when writing styled cells. */
    [[nodiscard]] Font withStyle (uint8_t styleFlags) const noexcept;

    // =========================================================================
    // Rendering — single cell
    // =========================================================================

    /** Renders a single cell to an image via the glyph pipeline.
     *  Shapes the cell internally, produces a Glyph, rasterizes through atlas. */
    juce::Image getImage (glyph::Atlas& atlas, const Cell& cell) const noexcept;

    // =========================================================================
    // Input properties (styling / configuration)
    // =========================================================================

    Typeface*  typeface              { nullptr };  ///< Non-owning. Lives in Typeface::Context.
    float      fontSize             { 0.0f };     ///< Points.
    float      cellWidthMultiplier  { 1.0f };     ///< User config: display.font.cellWidth.
    float      lineHeightMultiplier { 1.0f };     ///< User config: display.font.lineHeight.
    uint8_t    styleFlags           { 0 };        ///< Active SGR style (for pen/writing use).

    // =========================================================================
    // Resolved properties (computed in constructor)
    // =========================================================================

    int   cellWidth   { 0 };   ///< Logical pixels. Max advance across ASCII 32–127.
    int   cellHeight  { 0 };   ///< Logical pixels. ceil(ascent + descent + leading) * fontSize.
    int   baseline    { 0 };   ///< Logical pixels. ceil(ascent * fontSize).

private:
    // =========================================================================
    // Internal — shaped character identity
    // =========================================================================

    struct Glyph
    {
        uint32_t codepoint    { 0 };
        uint16_t glyphIndex   { 0 };
        void*    fontHandle   { nullptr };
        uint8_t  style        { 0 };
        uint8_t  advanceWidth { 1 };
        bool     isEmoji      { false };
    };

    void resolveMetrics() noexcept;  ///< maxAdvance scan + metric derivation.
};

} // namespace jam
```

### jam::glyph namespace

```cpp
namespace jam
{
namespace glyph
{

// =========================================================================
// Arrangement — bulk layout engine (was Glyph::ShapedText)
// =========================================================================
//
// Shapes Cells through a Font, producing positioned draw runs.
// Stores accumulated float positions from glyph advances.
// Monospace: positions form a regular grid. Proportional: positions vary.

class Arrangement { /* ... */ };

// =========================================================================
// Graphics — glyph compositor (moved from jam_graphics)
// =========================================================================

class Graphics { /* ... */ };

// =========================================================================
// Atlas — rasterized glyph cache (moved from jam_graphics)
// =========================================================================
//
// Type definition here. Instance lives in Typeface::Context (application lifetime).

class Atlas { /* ... */ };

// =========================================================================
// Constraint — Nerd Font constraint table (moved from jam_graphics)
// =========================================================================

struct Constraint { /* ... */ };

} // namespace glyph
} // namespace jam
```

### Arrangement — key changes from ShapedText

```cpp
namespace jam::glyph
{

class Arrangement
{
public:
    /** Shapes cells through font, producing positioned draw runs.
     *  @param cells    Content to shape.
     *  @param font     Rendering descriptor (typeface + metrics).
     *  @param wrapColumns  Column wrap width (0 = no wrap). */
    void shape (const Cells& cells, const Font& font, int wrapColumns = 0) noexcept;

    int getNumDrawRuns() const noexcept;
    const Run& getDrawRun (int index) const noexcept;
    int getNumLines() const noexcept;
    bool isValid() const noexcept;

    struct Run
    {
        juce::HeapBlock<uint16_t>       glyphCodes;
        juce::HeapBlock<juce::Point<float>> positions;  ///< Accumulated pixel positions (not cell coords).
        juce::HeapBlock<uint32_t>       codepoints;
        juce::HeapBlock<uint8_t>        spans;
        juce::HeapBlock<uint8_t>        styles;
        juce::HeapBlock<juce::Colour>   bgColours;
        int            count      { 0 };
        void*          fontHandle { nullptr };
        juce::Colour   colour;
        bool           isEmoji    { false };
    };

private:
    /* ... */
};

} // namespace jam::glyph
```

Key change: `Run` stores `positions` as `juce::Point<float>` (accumulated pixel positions from glyph advances), replacing `cols[]`/`rows[]` integer cell coordinates. Position computation moves from the renderer (`col * cellW`) into the Arrangement during `shape()`. Renderer reads positions directly.

### Typeface shaping fix

```cpp
// shapeASCII — BEFORE (monospace enforcement):
g.xAdvance = static_cast<float> (face->size->metrics.max_advance >> 6);

// shapeASCII — AFTER (actual per-glyph advance):
FT_Load_Glyph (face, glyphIndex, FT_LOAD_DEFAULT);
g.xAdvance = static_cast<float> (face->glyph->metrics.horiAdvance >> 6);

// shapeFallback — BEFORE:
g.xAdvance = static_cast<float> (primaryFace->size->metrics.max_advance >> 6);

// shapeFallback — AFTER:
FT_Load_Glyph (resolvedFace, glyphIndex, FT_LOAD_DEFAULT);
g.xAdvance = static_cast<float> (resolvedFace->glyph->metrics.horiAdvance >> 6);
```

For monospace fonts (e.g., Display Mono), `horiAdvance == max_advance` for all ASCII glyphs. The fix produces identical output for terminal use. For proportional fonts, it produces actual per-glyph advances.

### Cell — reduced

```cpp
namespace jam
{

struct Cell
{
    // =========================================================================
    // Content
    // =========================================================================

    uint32_t     codepoint { 0 };       ///< Unicode scalar value. 0 = empty.
    uint8_t      style     { 0 };       ///< SGR style flags (bold, italic, underline, etc.).
    uint8_t      width     { 1 };       ///< Display columns (1 or 2). VT logic needs this.
    uint8_t      layout    { 0 };       ///< Grid structure flags (WIDE_CONT, GRAPHEME).
    uint8_t      reserved  { 0 };       ///< Alignment padding.
    juce::Colour fg;                    ///< Foreground. Alpha 0 = theme sentinel.
    juce::Colour bg;                    ///< Background. Alpha 0 = theme sentinel.

    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr uint8_t BOLD       = 0x01;
    static constexpr uint8_t ITALIC     = 0x02;
    static constexpr uint8_t UNDERLINE  = 0x04;
    static constexpr uint8_t STRIKE     = 0x08;
    static constexpr uint8_t BLINK      = 0x10;
    static constexpr uint8_t INVERSE    = 0x20;
    static constexpr uint8_t DIM        = 0x40;

    static constexpr uint8_t LAYOUT_WIDE_CONT = 0x01;
    static constexpr uint8_t LAYOUT_GRAPHEME  = 0x08;
    // LAYOUT_EMOJI removed — detected by shaping from codepoint.

    // =========================================================================
    // Queries
    // =========================================================================

    bool hasContent() const noexcept { return codepoint > 0; }
    static Cell erase (juce::Colour bg) noexcept;
};

static_assert (sizeof (Cell) == 16);

} // namespace jam
```

`LAYOUT_EMOJI` removed — emoji detection moves to shaping layer (`charPropsFor(codepoint).isEmojiPresentation()`). `width` and `LAYOUT_WIDE_CONT` stay — VT logic (Video) needs them for cursor advancement and grid structure. `LAYOUT_GRAPHEME` stays — requires UAX#29 context, not codepoint-derivable.

### Consumer API changes

```cpp
// CaretComponent
void setFont (const jam::Font& font) noexcept;  ///< Replaces setGlyphImage pattern.

// TextEditor
void setFont (const jam::Font& font) noexcept;  ///< Replaces setFont(juce::Font).

// Screen — holds Font, passes to Arrangement and CaretComponent
// Display::resized() constructs Font, passes down via screen.setFont(font).
```

### Ownership chain

```
Typeface::Context (application lifetime)
  ├── Owner<Typeface>           — font data + shaping
  └── glyph::Atlas              — shared rasterized cache

jam::Font (value type, per-resize)
  ├── Typeface*                 — non-owning
  ├── fontSize, multipliers     — input configuration
  ├── cellWidth, cellHeight, baseline — resolved metrics
  └── Font::Glyph              — private, ephemeral shaped identity

jam::glyph (namespace, rendering infrastructure)
  ├── glyph::Arrangement        — bulk layout engine, takes Font per shape()
  ├── glyph::Graphics           — compositor
  ├── glyph::Atlas              — type def (instance in Typeface::Context)
  └── glyph::Constraint         — Nerd Font constraint table
```

### Data flow

```
Display::resized()
  │
  │  jam::Font font (typeface, fontSize, cellWidthMul, lineHeightMul)
  │    └── resolveMetrics(): maxAdvance scan → cellWidth, cellHeight, baseline
  │
  ├── screen.setFont (font)
  │     ├── caret->setFont (font)
  │     └── arrangement.shape (cells, font, cols)
  │           └── per cell: typeface->shapeText() → xAdvance (real, not max_advance)
  │                         accumulate positions from advances
  │
  └── state.setCellMetrics (font)          ← cross-thread fan-out for Video/Skit

Panes / MainComponent popup / overlay:
  jam::Font font (typeface, fontSize)      ← local, for window sizing
  int windowWidth = font.cellWidth * cols
```

## BLESSED Compliance Checklist

- [x] Bounds — Font value type, non-owning Typeface*. Glyph ephemeral. Atlas application-lifetime. Clear lifetimes.
- [x] Lean — 4 duplication sites → 1 constructor. Dead Glyph struct eliminated. No boilerplate at call sites.
- [x] Explicit — Named type replaces loose scalars. setFont() replaces 4 getter/setter patterns. No magic.
- [x] SSOT — Font IS font truth. Cell IS content truth. No overlap. One metric computation.
- [x] Stateless — Font immutable value. Components receive, don't track. Arrangement stateless between shape() calls.
- [x] Encapsulation — Glyph private to Font. glyph namespace scopes infrastructure. Components see setFont() only.
- [x] Deterministic — Same inputs = same outputs. Always.

## Open Questions

None. All decisions resolved during ORACLE session.

## Handoff Notes

- **jam_fonts module gains dependencies** on `jam_graphics` (Typeface) and `jam_tui` (Cell). Currently depends only on `juce_graphics` + `jam_core`. This is a significant dependency expansion — the module evolves from pure binary assets to binary assets + font type system.
- **Glyph::Render stays in jam_graphics.** It contains render-thread boundary types (Background, Quad, SnapshotBase) — rendering infrastructure, not font/glyph pipeline. No move needed.
- **jam::Typeface stays in jam_graphics.** Font references it via non-owning pointer. No circular dependency — `jam_fonts` depends on `jam_graphics`, not vice versa.
- **shapeASCII performance.** Current path is O(1) — `FT_Get_Char_Index` + read `max_advance`. Fix adds `FT_Load_Glyph` per call (loads glyph metrics from the font). For monospace terminal, consider caching per-glyph advances in a 128-entry lookup table on `FT_Set_Char_Size`. Implementation detail for COUNSELOR/SURGEON.
- **RFC-text-editor.md updated concurrently** to reflect the new Font/glyph foundation. TextEditor uses `jam::Font`, `jam::glyph::Arrangement`, accumulated float positions. Both RFCs are the foundation for COUNSELOR planning.
- **Constraint table** (10,470 NF entries, generated code) moves file location but content is unchanged. The `getConstraint(uint32_t)` API is codepoint-only — no Font/Cell dependency.
