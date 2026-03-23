# PLAN.md — Text Rendering Pipeline: Extraction, Decoupling, CPU Fallback

**Project:** END
**Date:** 2026-03-23
**Author:** COUNSELOR
**Status:** Active — Plan 2.6 next

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Seven sequential plans. Each plan is self-contained — delivers working, testable output before the next begins. No plan assumes the next will happen.

| Plan | Objective | Status |
|------|-----------|--------|
| 0 | Vendor FreeType as JUCE module | Done (Sprint 115) |
| 1 | Extract glyph pipeline into `jreng_glyph` module | Done (Sprint 115) |
| 2 | Replace END's rendering with `jreng_glyph` (destructive move) | Done (Sprint 115) |
| 2.5 | Decouple GL from glyph pipeline, rewire Screen to Font API | Done (Sprint 117) |
| **2.6** | **Rendering pipeline optimization — rasterization + dirty rows** | **Next** |
| 3 | CPU rendering backend via `juce::Graphics` | Blocked on 2.6 |
| 4 | END end-to-end CPU rendering fallback | Blocked on 3 |

### Surface API

`jreng::TextLayout` — drop-in replacement for `juce::TextLayout`.

- **Input:** `juce::AttributedString` (no custom string type)
- **Rendering:** `jreng::TextLayout::draw (jreng::Glyph::Renderer&, area)` — backend-agnostic
- **GL backend:** `GLRenderer` implements `Glyph::Renderer` — instanced quad rendering
- **CPU backend:** Plan 3 implements `Glyph::Renderer` — `juce::Graphics` blit rendering

### Architecture (post-Plan 2.5)

```
juce::AttributedString
        |
        v
TextLayout::createLayout (text, maxWidth, font)
        |
        v
   layout data (lines, runs, positioned glyphs)
        |
        v
TextLayout::draw (renderer, area)
        |                         |
        v                         v
  setFont() + drawGlyphs()   setFont() + drawGlyphs()
    GLRenderer                  CPURenderer (Plan 3)
   (jreng_opengl)
```

### Module Dependency (post-Plan 2.5)

```
jreng_glyph  (Font, Atlas, TextLayout, Glyph::Renderer interface)
     ^
     |
jreng_opengl (GLRenderer implements Glyph::Renderer)
     ^
     |
jreng_core
```

`jreng_glyph` has zero GL dependency. `jreng_opengl` depends on `jreng_glyph` (DIP: high-level module defines interface, low-level implements).

### Rendering Interface (mirrors juce::LowLevelGraphicsContext::drawGlyphs)

```cpp
namespace jreng::Glyph
{
    struct Renderer
    {
        virtual ~Renderer() = default;
        virtual void setFont (jreng::Font& font, Font::Style style, bool isEmoji) = 0;
        virtual void drawGlyphs (const uint16_t* glyphCodes,
                                 const juce::Point<float>* positions,
                                 int count) = 0;
        virtual void drawBackgrounds (const juce::Rectangle<float>* areas,
                                      const juce::Colour* colours,
                                      int count) = 0;
    };
}
```

---

## PLAN 0: Vendor FreeType as JUCE Module — DONE

Completed Sprint 115. `modules/jreng_freetype/` wraps vendored FreeType 2.13.3.
HarfBuzz: using JUCE's internal copy (ARCHITECT decision).

---

## PLAN 1: Extract Glyph Pipeline (`jreng_glyph`) — DONE

Completed Sprint 115. `modules/jreng_glyph/` contains Font, Atlas, shaping, rendering, TextLayout.

---

## PLAN 2: Replace END Rendering with `jreng_glyph` — DONE

Completed Sprint 115 (destructive move — original code deleted).

---

## PLAN 2.5: Decouple GL from `jreng_glyph`

**Goal:** `jreng_glyph` becomes rendering-agnostic. All GL code consolidates in `jreng_opengl`. Font owns Atlas. `TextLayout::draw()` follows JUCE's `drawGlyphs` pattern — parallel spans of glyph IDs + positions, font set per-run. `Glyph::Renderer` is the abstract target.

### Design Decisions (ARCHITECT approved)

1. **TextLayout is pure layout.** No rendering code. `draw()` iterates lines/runs, calls `renderer.setFont()` + `renderer.drawGlyphs()` per run.
2. **Font owns Atlas.** Atlas is Font's internal rasterization cache. Rendering layer never touches Atlas directly.
3. **GLRenderer is THE renderer.** All GL rendering goes through GLRenderer. It gains instanced text rendering and implements `Glyph::Renderer`.
4. **Parallel spans (SOA).** `drawGlyphs(uint16_t[], Point<float>[], count)` — matches JUCE pattern, cache-friendly.
5. **uint16_t glyph codes.** TrueType/OpenType glyph indices max at 65535. Aligns with JUCE's `drawGlyphs(Span<const uint16_t>, ...)`.
6. **Per-run context via `setFont()`.** Style, emoji flag, constraint — all per-run, not per-glyph. NF icons always get their own run.
7. **Staged bitmap queue = cross-thread mailbox.** Font stages bitmaps (message thread). Renderer drains queue at frame start (GL thread). Queue is the communication channel.
8. **`Render::Quad` / `Render::Background` become internal to GL implementation.** Never cross the `Glyph::Renderer` interface boundary.
9. **`StagedBitmap::AtlasKind` → `Glyph::Atlas::Type { mono, emoji }`.** Remove `StagedBitmap::format` — redundant. Renderer derives pixel format from Type.

---

### Step 2.5.0 — Atlas::Type + remove format

Rename `StagedBitmap::AtlasKind` → `Glyph::Atlas::Type`. Remove `StagedBitmap::format` field.

**Files:**
- `atlas_impl/jreng_glyph_atlas.h` — define `enum class Type { mono, emoji }` in Atlas
- `atlas/jreng_staged_bitmap.h` — remove `AtlasKind` enum, remove `format` field, change `kind` → `type` typed `Atlas::Type`
- `atlas_impl/jreng_glyph_atlas.cpp` — update `stageForUpload()`: remove `staged.format` assignment, use `staged.type`
- `render/jreng_gl_text_renderer.cpp` — update `uploadStagedBitmaps()`: derive GL format from `staged.type` instead of `staged.format`

**Validate:** END builds. Atlas staging and upload work identically.

---

### Step 2.5.1 — Font absorbs Atlas

Atlas becomes Font's private member. Rendering layer stops touching Atlas.

**Font gains:**
- Private `Glyph::Atlas atlas` member
- `setSize()` calls `atlas.clear()`
- `consumeStagedBitmaps()` — delegates to internal atlas (cross-thread mailbox drain)
- `hasStagedBitmaps()` — delegates to internal atlas
- `getOrRasterize()` — delegates to internal atlas
- `getOrRasterizeBoxDrawing()` — delegates to internal atlas
- `advanceFrame()` — delegates to `atlas.advanceFrame()`
- `setEmbolden()` — delegates to `atlas.setEmbolden()`
- `setDisplayScale()` — delegates to `atlas.setDisplayScale()`
- `getCacheStats()` — delegates to `atlas.getCacheStats()`
- `atlasDimension()` — delegates to `Atlas::atlasDimension()`

**END changes:**
- `Screen::Resources` loses `glyphAtlas` member
- All `resources.glyphAtlas.*` calls → `font.*`
- `Main.cpp` / `MainComponent` no longer threads Atlas separately

**Validate:** END builds. Atlas is private to Font. Rendering unchanged.

---

### Step 2.5.2 — Define `Glyph::Renderer` interface

Abstract rendering target in `jreng_glyph`. Zero GL dependency.

```cpp
namespace jreng::Glyph
{
    struct Renderer
    {
        virtual ~Renderer() = default;
        virtual void setFont (jreng::Font& font, Font::Style style, bool isEmoji) = 0;
        virtual void drawGlyphs (const uint16_t* glyphCodes,
                                 const juce::Point<float>* positions,
                                 int count) = 0;
        virtual void drawBackgrounds (const juce::Rectangle<float>* areas,
                                      const juce::Colour* colours,
                                      int count) = 0;
    };
}
```

**Files:**
- New: `render/jreng_glyph_renderer.h`
- Update: `jreng_glyph.h` — add include

**Validate:** END builds. Additive only.

---

### Step 2.5.3 — Refactor TextLayout

TextLayout captures `Font&` at `createLayout()`. `draw()` takes `Glyph::Renderer&`.

**Before:**
```cpp
void createLayout (const juce::AttributedString&, float maxWidth);
void draw (GLTextRenderer&, Atlas&, Font&, Rectangle<float>) const;
```

**After:**
```cpp
void createLayout (const juce::AttributedString&, float maxWidth, Font& font);
void draw (Glyph::Renderer& target, juce::Rectangle<float> area) const;
```

Inside `draw()`: iterate lines/runs, call `target.setFont(font, style, isEmoji)` then `target.drawGlyphs(glyphCodes, positions, count)` per run. TextLayout no longer builds `Render::Quad` arrays.

**Keep old draw() temporarily** for backward compat until Step 2.5.5.

**Validate:** END builds. Old path still works.

---

### Step 2.5.4 — GLRenderer implements `Glyph::Renderer`

GLTextRenderer folds into GLRenderer. GLRenderer gains instanced text rendering.

**What moves from `jreng_glyph` to `jreng_opengl`:**
- Glyph + background shaders (source strings + .vert/.frag files)
- Instanced quad setup: static unit quad VBO, instance VBO, VAO
- Atlas texture creation + `glTexSubImage2D` upload logic
- `Render::Quad`, `Render::Background` structs (become GLRenderer-internal types)

**GLRenderer gains:**
- Private: mono/emoji/background shader programs, mono/emoji atlas textures (GLuint), instance VBO, quad VBO
- Implements `Glyph::Renderer`: `setFont()` caches font ref + style + emoji flag. `drawGlyphs()` does atlas lookup → build quads → instanced draw. `drawBackgrounds()` builds background quads → draw.
- `uploadStagedGlyphs (Font&)` — drains Font's staged bitmap queue, uploads to atlas textures. Derives GL format from `Atlas::Type`.
- Text resource lifecycle in existing `newOpenGLContextCreated()` / `openGLContextClosing()`

**Dependency change:**
- `jreng_opengl.h` — add `jreng_glyph` to dependencies
- `jreng_glyph.h` — remove `jreng_opengl` and `juce_opengl` from dependencies

**Validate:** GLRenderer implements `Glyph::Renderer`. Text renders through new path.

---

### Step 2.5.5 — Rewire END's Screen

Screen uses `Glyph::Renderer&` instead of `GLTextRenderer`.

**Changes:**
- `Screen.h` — replace `GLTextRenderer textRenderer` member with `Glyph::Renderer& glyphRenderer` reference
- `ScreenGL.cpp` — `glyphRenderer.setFont(...)` / `drawGlyphs(...)` / `drawBackgrounds(...)` instead of `textRenderer.drawQuads(...)` / `drawBackgrounds(...)`
- `MainComponent` passes `GLRenderer&` (which is-a `Glyph::Renderer`) to Screen
- GL lifecycle (`contextCreated`/`contextClosing`) managed by GLRenderer — Screen no longer calls these
- GL state (viewport, blend) managed by GLRenderer internally

**Validate:** END builds, renders identically. No visual change.

---

### Step 2.5.6 — Remove dead code, clean dependencies

- Delete from `jreng_glyph`: `render/jreng_gl_text_renderer.h`, `render/jreng_gl_text_renderer.cpp`, `render/jreng_glyph_shaders.h`, `render/jreng_glyph_render.h`
- Delete shader source files from `jreng_glyph/shaders/`
- Remove old `TextLayout::draw(GLTextRenderer&, ...)` overload
- Update `jreng_glyph.h` — remove `jreng_opengl` and `juce_opengl` from dependencies
- Update `jreng_glyph.cpp` / `.mm` — remove deleted files from unity build

**Validate:** END builds. `grep -r "juce::gl\|GL_\|OpenGL\|jreng_opengl" modules/jreng_glyph/` returns nothing. `jreng_glyph` is fully rendering-agnostic.

---

## PLAN 2.6: Rendering Pipeline Optimization

**Gate:** Must complete before Plan 3. CPU rendering will amplify any inefficiency — fix on the GL path first so the CPU path inherits a clean baseline.

**Goal:** Eliminate redundant per-frame work in the rendering pipeline. Two confirmed bottlenecks: (1) `calcMetrics()` called per cell every frame despite metrics only changing on font size change, (2) full grid rebuild every frame despite dirty-row tracking infrastructure already existing.

### Benchmark context

`seq 1 10000000` on iMac 5K 2015, -O3:

| Terminal | Wall |
|----------|------|
| kitty | 13.55s |
| END | **12.39s** |

END wins throughput. The optimization targets are per-frame CPU waste, not throughput.

### Investigation results (confirmed by code analysis)

| Target | Finding | Status |
|--------|---------|--------|
| Snapshot buffer latency | `GLMailbox` is lock-free atomic exchange. One-frame latency is inherent and acceptable. | No change needed |
| Full redraw every frame | `buildSnapshot()` rebuilds ALL rows unconditionally. `Grid::dirtyRows[4]` bitmask exists but is never consumed by the render path. | **Fix: Step 2.6.1** |
| `calcMetrics()` per cell | `Font::getGlyph()` calls `typeface.calcMetrics(size)` per cell per frame. 4,800 calls/frame, each doing `FT_Set_Char_Size` x2 + 96 `FT_Load_Glyph`. 100% waste on cache hits. | **Fix: Step 2.6.0** |
| `glBufferData` per frame | Standard orphaning pattern. Modern drivers handle it well. | Low priority |
| CGBitmapContext per glyph | macOS rasterization creates/destroys CGBitmapContext + CGColorSpace per glyph. | **Fix: Step 2.6.2** |
| Blend state on opaque backgrounds | Minor. | Deferred |

---

### Step 2.6.0 — Cache `calcMetrics()` on Typeface

**Problem:** `Font::getGlyph()` calls `typeface.calcMetrics(size)` on every invocation. For 120x40 terminal: 4,800 calls per frame. Each call does `FT_Set_Char_Size` twice + iterates 96 ASCII glyphs in `measureMaxCellWidth`. On steady-state cache hits, this is pure waste — metrics never change between calls at the same size.

**Fix:** Cache the `Metrics` result on Typeface, keyed by size. `calcMetrics()` returns the cached result when size matches. Invalidate only on size change.

**Files:**
- `modules/jreng_graphics/fonts/jreng_typeface.h` — add cached Metrics member + cached size
- `modules/jreng_graphics/fonts/jreng_typeface.cpp` — `calcMetrics()` returns cached result when size matches, recomputes and caches on size change

**Validate:** Build succeeds. `Font::getGlyph()` no longer triggers FreeType calls on cache hits. Verify: font size change still recomputes metrics correctly.

---

### Step 2.6.1 — Dirty-row skipping in `buildSnapshot()`

**Problem:** `buildSnapshot()` processes all rows, all columns, every frame. Grid already has `dirtyRows[4]` atomic bitmask with `consumeDirtyRows()` and `markRowDirty()` / `markAllDirty()` / `batchMarkDirty()`. This infrastructure is unused by the render path.

**Fix:** `buildSnapshot()` consumes the dirty bitmask. Only dirty rows run through `processCellForSnapshot()` (color resolution, shaping, atlas lookup). Clean rows retain their cached `cachedMono[r]`, `cachedEmoji[r]`, `cachedBg[r]` arrays from the previous frame.

**Dirty-all triggers (already handled by existing Grid API):**
- Scroll: `markAllDirty()` or `batchMarkDirty()`
- Resize: `markAllDirty()`
- Scroll offset change: needs to mark all dirty
- Selection overlay change: needs to dirty affected rows

**Expected impact:** During typical usage (typing, cursor movement), 1-3 rows dirty per frame. 90-98% reduction in per-frame `processCellForSnapshot()` calls.

**Files:**
- `Source/terminal/rendering/ScreenRender.cpp` — `buildSnapshot()` consumes `dirtyRows`, skips clean rows
- Possibly `Source/terminal/data/State.cpp` or `Source/component/TerminalComponent.cpp` — ensure scroll offset changes mark all rows dirty

**Validate:** Build succeeds. Terminal renders correctly. Static content (e.g. vim buffer) does not trigger per-cell processing. Scrolling still works (marks all dirty). Selection still works.

---

### Step 2.6.2 — Pool CGBitmapContext on macOS

**Problem:** Every glyph rasterization on macOS creates and destroys a `CGBitmapContext` + `CGColorSpaceRef`. During cache-cold frames (first display, font size change), this is ~285+ CoreGraphics allocations for 95 ASCII characters.

**Fix:** Pool two persistent contexts: one DeviceGray (mono glyphs), one DeviceRGB (emoji). Resize only when cell dimensions change (font size change). Reuse across rasterizations.

**Files:**
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.mm` — pool CGBitmapContext + CGColorSpace, resize on cell dimension change

**Validate:** Build succeeds on macOS. Glyph rasterization produces identical output. Font size change triggers context resize.

---

### Step 2.6.3 — Measure and validate

After steps 2.6.0-2.6.2:
1. `time seq 1 10000000` — wall time unchanged or improved
2. Open `nvim` with large file — observe static content. Verify: steady-state frame cost is minimal (only dirty rows processed)
3. Font size change — verify metrics recompute, atlas repopulates, contexts resize
4. Rapid scrolling — verify all rows marked dirty, no visual artifacts
5. Selection — verify affected rows re-rendered

**Scope:** Each step is standalone. ARCHITECT builds and tests after each step before proceeding.

---

---

## PLAN 3: CPU Rendering Backend

**Blocked on:** Plan 2.6 (rendering optimization)

**Goal:** Implement `Glyph::Renderer` for `juce::Graphics`. Same `TextLayout::draw()` call — different backend. CPU glyph cache stores `juce::Image` per glyph. `drawGlyphs()` calls `g.drawImageAt()`. `drawBackgrounds()` calls `g.fillRect()`.

**Scope unchanged from original Plan 3.** Implementation details deferred until Plan 2.6 completes and the optimized pipeline establishes the baseline.

---

## PLAN 4: END CPU Rendering Fallback

**Blocked on:** Plan 3

**Goal:** END detects GPU availability at startup. Falls back to CPU rendering. Full terminal functionality, no blur.

**Scope unchanged from original Plan 4.** Implementation details deferred until Plan 3 completes.

---

## Rules for Execution

1. **Always invoke @pathfinder first** — before any code change, discover existing patterns
2. **Validate each step before proceeding** — ARCHITECT builds and tests
3. **Never assume, never decide** — discrepancy between plan and code, discuss with ARCHITECT
4. **No new types without ARCHITECT approval** — check if existing types can be reused
5. **Follow all contracts** — JRENG-CODING-STANDARD, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO
6. **Incremental execution** — one step at a time, ARCHITECT confirms before next step
7. **ARCHITECT runs all git commands** — agents prepare changes only
8. **When uncertain — STOP and ASK**
