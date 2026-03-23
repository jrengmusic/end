# PLAN.md — Text Rendering Pipeline: Extraction, Decoupling, CPU Fallback

**Project:** END
**Date:** 2026-03-23
**Author:** COUNSELOR
**Status:** Active — Plan 2.5 next

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Seven sequential plans. Each plan is self-contained — delivers working, testable output before the next begins. No plan assumes the next will happen.

| Plan | Objective | Status |
|------|-----------|--------|
| 0 | Vendor FreeType as JUCE module | Done (Sprint 115) |
| 1 | Extract glyph pipeline into `jreng_glyph` module | Done (Sprint 115) |
| 2 | Replace END's rendering with `jreng_glyph` (destructive move) | Done (Sprint 115) |
| **2.5** | **Decouple GL from `jreng_glyph`, consolidate in `jreng_opengl`** | **Next** |
| **2.6** | **Rendering pipeline optimization** | **Gate for Plan 3** |
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

**Goal:** Investigate and resolve rendering latency. END's data throughput beats kitty by ~200ms on `time seq 1 1M`, but rendering feels sluggish. The bottleneck is between "data ready" and "pixels on screen."

**Investigation targets (prioritize by impact):**

1. **Snapshot buffer latency** — `GLSnapshotBuffer` double-buffer introduces 1-frame lag. Kitty renders directly in the GL callback. Measure: timestamp delta between snapshot write and GL draw.
2. **Full redraw every frame** — does Screen rebuild the entire instance buffer every frame, or only dirty cells? If full rebuild: thousands of atlas lookups + quad builds per frame on static content.
3. **`glBufferData` per frame** — uploading entire instance VBO every frame via `GL_DYNAMIC_DRAW`. For mostly-static content, persistent mapped buffer or dirty-region update avoids the stall.
4. **Message thread rasterization blocking** — `getOrRasterize()` on message thread during scrolling can stall snapshot production while GL thread waits.
5. **Blend state on opaque backgrounds** — `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA` on every quad including fully opaque backgrounds. Backgrounds could skip blending.

**Scope:** Profile, measure, fix. Each optimization is a standalone step with before/after measurement. No speculative optimization — data first.

**Validate:** Measurable improvement in perceived rendering responsiveness. `time seq 1 1M` visual output feels as fluid as kitty.

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
