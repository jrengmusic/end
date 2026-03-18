# PLAN.md — Text Rendering Pipeline Extraction & CPU Fallback

**Project:** END  
**Date:** 2026-03-19  
**Author:** COUNSELOR  
**Status:** Draft — awaiting ARCHITECT approval

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Five sequential plans. Each plan is self-contained — delivers working, testable output before the next begins. No plan assumes the next will happen.

| Plan | Objective | Module |
|------|-----------|--------|
| 0 | Vendor FreeType and HarfBuzz as JUCE modules | `jreng_freetype`, `jreng_harfbuzz` |
| 1 | Extract generalizable glyph pipeline into reusable module | `jreng_text` |
| 2 | Replace END's rendering pipeline with Plan 1 module | END `Source/terminal/rendering/` |
| 3 | CPU rendering backend for glyph atlas pipeline via `juce::Graphics` | `jreng_text` (CPU backend) |
| 4 | END end-to-end CPU rendering fallback when GPU unavailable | END `Source/` |

### Surface API

`jreng::TextLayout` — drop-in replacement for `juce::TextLayout`.

- **Input:** `juce::AttributedString` (no custom string type — use JUCE's familiar API)
- **GL output:** `jreng::TextLayout::draw (jreng::GLGraphics&, area)` — instanced quad rendering
- **CPU output:** `jreng::TextLayout::draw (juce::Graphics&, area)` — `juce::Image` blit rendering
- **Shared pipeline:** HarfBuzz shaping, GlyphAtlas caching, FontCollection dispatch

---

## PLAN 0: Vendor FreeType & HarfBuzz as JUCE Modules

**Goal:** Self-contained font dependencies. No external build deps, no piggybacking on JUCE internals.

**Current state:**
- FreeType: git submodule at `Library/freetype/`, built via CMake `add_subdirectory`. Works.
- HarfBuzz: using JUCE's internal copy at `${JUCE_PATH}/modules/juce_graphics/fonts/harfbuzz`. Fragile — JUCE doesn't guarantee internal API stability.

**Licenses:** FreeType License (BSD-like) + HarfBuzz MIT. Both allow vendoring.

---

### Step 0.1 — Create `jreng_freetype` module

Wrap the existing vendored FreeType source (`Library/freetype/`) as a JUCE module at `modules/jreng_freetype/`. The module header exposes FreeType's public API (`ft2build.h`, `freetype.h`). The module `.cpp` compiles FreeType source files.

**Discuss with ARCHITECT:** Should we keep the git submodule at `Library/freetype/` and have the module reference it? Or copy the source into the module directory? Submodule keeps upstream tracking. Copy is fully self-contained.

**Validate:** END builds with `jreng_freetype` instead of CMake `add_subdirectory`. FreeType API accessible via module include.

---

### Step 0.2 — Create `jreng_harfbuzz` module

Vendor HarfBuzz source as a JUCE module at `modules/jreng_harfbuzz/`. HarfBuzz provides an amalgamation build (`harfbuzz.cc`) — single file compilation.

**Discuss with ARCHITECT:** HarfBuzz version — use the same version JUCE bundles (for compatibility), or latest stable?

**Validate:** END builds with `jreng_harfbuzz` instead of JUCE's internal HarfBuzz include path. Shaping produces identical output.

---

### Step 0.3 — Remove old dependencies

Remove `Library/freetype/` submodule reference from CMakeLists.txt. Remove JUCE internal HarfBuzz include path. All FreeType/HarfBuzz access goes through `jreng_freetype` and `jreng_harfbuzz` modules.

**Validate:** END builds cleanly. No references to `Library/freetype/` or `${JUCE_PATH}/modules/juce_graphics/fonts/harfbuzz`.

---

## PLAN 1: Extract Universal Glyph Pipeline (`jreng_text`)

**Goal:** Generalize END's working GL text rendering into a reusable JUCE module. Provides `jreng::TextLayout` as drop-in replacement for `juce::TextLayout` — accepts `juce::AttributedString`, renders via GL or CPU.

**Module location:** `modules/jreng_text/`

**Dependencies:** `jreng_opengl`, `jreng_core`, `jreng_freetype`, `jreng_harfbuzz`, `juce_graphics`, `juce_opengl`, CoreText (macOS)

### Principle: Extract, Don't Rewrite

Every component listed below already works in END. The task is to **move** code, **remove** terminal-specific assumptions, and **add** the proportional layout path. No rewriting of working logic.

LIFESTAR Lean: extract the minimum. LIFESTAR SSOT: one definition, referenced by END and future consumers (WHELMED).

---

### Step 1.1 — Create module skeleton

Create `modules/jreng_text/` with module header, `.cpp`, `.mm` files. Empty module that compiles and links. Add to CMakeLists.txt.

**Validate:** END builds with the empty module linked. No functional change.

---

### Step 1.2 — Extract AtlasPacker

Move `AtlasPacker` from `Source/terminal/rendering/GlyphAtlas.h` into `modules/jreng_text/atlas/`. Zero changes to logic — pure shelf-packing algorithm with no dependencies.

**Files:**
- `jreng_atlas_packer.h` — class definition

**Validate:** END builds, uses `jreng_text::AtlasPacker` instead of the inline definition. Atlas packing behavior identical.

**Discuss with ARCHITECT:** Namespace — `jreng::text` or flat `jreng`? Follow existing module pattern (`jreng::GLGraphics` is in namespace `jreng`).

---

### Step 1.3 — Extract LRUGlyphCache

Move `LRUGlyphCache` from `GlyphAtlas.h` into `modules/jreng_text/atlas/`. Pure LRU cache — `unordered_map<Key, Entry>` with frame-counter eviction.

**Files:**
- `jreng_lru_glyph_cache.h` — template or concrete class

**Discuss with ARCHITECT:** Should `GlyphKey` be generalized? Current key has `cellSpan` (terminal-specific). Options:
- (a) Remove `cellSpan` from the module's key, add it as an END-specific extension
- (b) Keep `cellSpan` as a generic "span" field (useful for wide glyphs in any context)
- (c) Template the cache on key type

**Validate:** END builds, uses module's LRU cache. Cache behavior identical.

---

### Step 1.4 — Extract StagedBitmap upload queue

Move `StagedBitmap` and the upload queue protocol from `GlyphAtlas.h` into `modules/jreng_text/atlas/`.

**Files:**
- `jreng_staged_bitmap.h` — `StagedBitmap` struct + queue (mutex-protected `HeapBlock`)

**Validate:** END builds, cross-thread staging works identically.

---

### Step 1.5 — Extract GlyphAtlas core

Move the atlas management (dual mono/emoji atlases, rasterization dispatch, cache lookup, staging) into the module. This is the largest extraction step.

**What moves:**
- Atlas texture management (two atlases: greyscale R8 + color RGBA)
- `getOrRasterize()` — cache lookup → rasterize → stage for upload
- `consumeStagedBitmaps()` — GL thread consumer
- `advanceFrame()` — LRU counter update
- `clear()` — invalidation

**What stays in END:**
- `GlyphConstraint` / NF icon outline transform (terminal-specific)
- `BoxDrawing` procedural rasterizer (terminal-specific, but discuss with ARCHITECT)
- Cell-span logic (terminal-specific width calculation)

**Files:**
- `jreng_glyph_atlas.h` / `.cpp` / `.mm`

**Discuss with ARCHITECT:**
- Should `BoxDrawing` move to the module? It's useful for any fixed-cell renderer (code editors, spreadsheets). But it has cell-relative thickness.
- Should `GlyphConstraint` stay in END or become an optional module feature?
- The rasterization callback pattern: module provides atlas + cache, consumer provides the rasterization function. This decouples NF icon transforms from the atlas.

**Validate:** END builds, glyph caching and atlas upload work identically. No visual change.

---

### Step 1.6 — Extract FontCollection

Move the flat codepoint-to-font-slot lookup table into the module.

**What moves:**
- `HeapBlock<int8_t>` lookup table (1.1M entries)
- `Entry` struct (font handle + HarfBuzz font + color flag)
- `resolve(codepoint)` — O(1) lookup
- `populateFromCmap(slotIndex)` — cmap iteration

**What stays in END:**
- Slot-0/Slot-1 convention (Display Mono + Nerd Font)
- Font loading from BinaryData

**Files:**
- `jreng_font_collection.h` / `.cpp` / `.mm`

**Validate:** END builds, codepoint resolution works identically.

---

### Step 1.7 — Extract shaping pipeline

Move HarfBuzz shaping from `FontsShaping.cpp` into the module.

**What moves:**
- `shapeHarfBuzz()` — full HarfBuzz pipeline (buffer → shape → read back)
- `shapeEmoji()` — emoji-specific shaping
- `shapeFallback()` — cmap-only fallback
- `ShapeResult` / `Glyph` types
- Scratch buffer management (`hb_buffer_t` reuse)

**What stays in END:**
- `shapeASCII()` fast path (terminal-specific: assumes fixed `max_advance`)
- `calcMetrics()` (terminal-specific: derives fixed cell dimensions)
- Font lifecycle management (loading, BinaryData, platform dispatch)

**Files:**
- `jreng_text_shaper.h` / `.cpp`

**Discuss with ARCHITECT:** Should `shapeASCII` move as an optimization available to any monospace consumer? It's a valid optimization for any fixed-width font context.

**Validate:** END builds, shaping produces identical glyph indices and advances.

---

### Step 1.8 — Extract instanced quad renderer

Move the GL instanced rendering pattern from `ScreenGL.cpp` into the module.

**What moves:**
- Unit quad VBO setup
- Instance VBO upload (`glBufferData`)
- `drawInstances()` — instanced draw call with per-instance attributes
- `drawBackgrounds()` — same pattern for background quads
- Glyph vertex/fragment shaders
- Background vertex/fragment shaders
- `Render::Glyph` and `Render::Background` structs (renamed to module-level types)

**Files:**
- `jreng_text_renderer.h` / `.cpp`
- `shaders/` — glyph and background shaders (moved from `Source/terminal/rendering/shaders/`)

**Validate:** END builds, GL rendering produces identical output.

---

### Step 1.9 — Add proportional text layout engine (`jreng::TextLayout`)

This is the **only new code** in Plan 1. Everything before was extraction.

**What it does:**
- Takes a `juce::AttributedString` (styled runs with font, size, color, bold/italic)
- For each run: resolve font via `FontCollection`, shape via `TextShaper` (HarfBuzz)
- Accumulate glyph advances (variable width — not fixed cell width)
- Line wrap at `maxWidth` (word boundary or character boundary)
- Output: array of positioned glyph instances ready for the instanced renderer

**API surface — drop-in replacement for `juce::TextLayout`:**
```cpp
namespace jreng
{
    class TextLayout
    {
    public:
        void createLayout (const juce::AttributedString& text, float maxWidth);
        void draw (GLGraphics& g, juce::Rectangle<float> area) const;
        void draw (juce::Graphics& g, juce::Rectangle<float> area) const;
        float getHeight() const;
        int getNumLines() const;
    };
}
```

**Input:** `juce::AttributedString` — no custom string type. Users build text with JUCE's familiar API, render with our pipeline.

**Two draw overloads:**
- `draw (GLGraphics&)` — GL instanced quad rendering (Plan 1)
- `draw (juce::Graphics&)` — CPU `juce::Image` blit rendering (Plan 3, stubbed in Plan 1)

**Discuss with ARCHITECT:**
- Line breaking: simple word-wrap, or full Unicode line break algorithm (UAX #14)?
- Bidirectional text: defer to future sprint or handle now?

**Validate:** Standalone test — create a `jreng::TextLayout` with a multi-styled `juce::AttributedString`, render it in a test window. Compare visual output with `juce::TextLayout` for the same string.

---

### Step 1.10 — Module documentation and API review

Write module header documentation. Review public API surface — ensure nothing terminal-specific leaked. Ensure all types follow NAMING-CONVENTION.

**Validate:** @auditor reviews module API against LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO.

---

## PLAN 2: Replace END Rendering with `jreng_text`

**Goal:** END's `Screen` consumes `jreng_text` module instead of owning its own atlas, shaping, and GL rendering code. Zero visual change.

### Principle: Consume, Don't Duplicate

After Plan 1, END has two copies of everything — the extracted module and the original code. Plan 2 removes the original code and wires END to the module. LIFESTAR SSOT: one definition.

---

### Step 2.1 — Wire GlyphAtlas

Replace `Screen::Resources::glyphAtlas` with `jreng::GlyphAtlas` from the module. Update all call sites.

**Discuss with ARCHITECT:** How to handle `GlyphConstraint` and `BoxDrawing` — they're END-specific but called from within `getOrRasterize()`. Options:
- (a) Rasterization callback: module calls a user-provided function for custom rasterization
- (b) Subclass: END extends `GlyphAtlas` with terminal-specific rasterization
- (c) Composition: END wraps `GlyphAtlas` and intercepts specific codepoints

**Validate:** END builds, all glyphs render correctly. Atlas packing, LRU, staging identical.

---

### Step 2.2 — Wire FontCollection and TextShaper

Replace END's `FontCollection` and shaping calls with module equivalents.

**Validate:** END builds, shaping produces identical output.

---

### Step 2.3 — Wire instanced renderer

Replace END's `drawInstances()` / `drawBackgrounds()` with module's `TextRenderer`.

**Validate:** END builds, GL rendering identical.

---

### Step 2.4 — Remove dead code

Delete the original implementations from `Source/terminal/rendering/` that are now in the module. Keep only END-specific code (cell cache, grid coordinate mapping, dirty tracking, ligature detection, `GlyphConstraint`, `BoxDrawing`).

**Validate:** END builds, no dead code, no duplicate definitions. @auditor verifies SSOT.

---

### Step 2.5 — Move shaders

Move glyph/background shaders from `Source/terminal/rendering/shaders/` to `modules/jreng_text/shaders/`. Update CMakeLists.txt BinaryData entries.

**Validate:** END builds, shaders load from module.

---

## PLAN 3: CPU Rendering Backend (`jreng_text` + `jreng_graphics`)

**Goal:** Render the glyph atlas pipeline via `juce::Graphics` instead of OpenGL. Identical surface API — same `GlyphAtlas`, same `FontCollection`, same `TextShaper`. Different output: `juce::Image` instead of GL textures.

### Principle: Same Data, Different Surface

The glyph atlas already rasterizes to CPU bitmaps before GPU upload. The CPU backend skips the upload and draws directly from the bitmap cache.

---

### Step 3.1 — CPU glyph cache

Create `jreng::CPUGlyphCache` — stores rasterized glyph bitmaps as `juce::Image` objects instead of atlas texture regions. Same `GlyphKey`, same LRU eviction, same rasterization pipeline. No atlas packing needed — each glyph is an independent `juce::Image`.

**Files:**
- `jreng_cpu_glyph_cache.h` / `.cpp`

**Discuss with ARCHITECT:** Should this share the `LRUGlyphCache` template with a different value type? Or separate implementation?

**Validate:** Standalone test — rasterize ASCII glyphs into `juce::Image` objects, verify pixel content matches GL atlas bitmaps.

---

### Step 3.2 — CPU text renderer

Create `jreng::CPUTextRenderer` — draws positioned glyphs via `juce::Graphics::drawImageAt()` and backgrounds via `juce::Graphics::fillRect()`.

**API surface mirrors GL renderer:**
```cpp
namespace jreng
{
    struct CPUTextRenderer
    {
        void drawBackgrounds (juce::Graphics& g,
                              const Background* backgrounds, int count);
        void drawGlyphs (juce::Graphics& g,
                         const CPUGlyphCache& cache,
                         const Glyph* glyphs, int count);
    };
}
```

**Validate:** Standalone test — render "Hello World" with `CPUTextRenderer` in a `juce::Component::paint()`. Compare visual output with GL renderer.

---

### Step 3.3 — Implement `jreng::TextLayout::draw (juce::Graphics&)`

The `draw (juce::Graphics&)` overload was stubbed in Plan 1 Step 1.9. Now implement it using `CPUGlyphCache` + `CPUTextRenderer`.

`jreng::TextLayout` is a single class with two `draw()` overloads — not two separate classes. `createLayout()` is shared (same shaping, same layout). Only the final draw call differs.

**Validate:** Standalone test — render attributed string via `jreng::TextLayout::draw (juce::Graphics&)`, compare with `draw (GLGraphics&)` output.

---

### Step 3.4 — Double-buffered CPU rendering

Create `jreng::CPUSnapshotBuffer` — renders to an offscreen `juce::Image`, flips to screen in `paint()`. Same double-buffer pattern as `GLSnapshotBuffer` but for `juce::Image`.

**Validate:** Flicker-free rendering in a test component.

---

## PLAN 4: END CPU Rendering Fallback

**Goal:** END detects GPU availability at startup. If GL context creation fails, falls back to CPU rendering. Full terminal functionality — same features, same config, no blur.

### Principle: Same Business Logic, Different Surface

The terminal data model (Grid, Parser, Session, TTY) doesn't change. Only the rendering layer switches between `ScreenGL` and `ScreenCPU`.

---

### Step 4.1 — Detect GPU availability

At startup, attempt `juce::OpenGLContext` creation. If it fails, set a flag. No crash, no assert — graceful detection.

**Discuss with ARCHITECT:** Where does the flag live? Options:
- (a) `Config` key (user can force CPU mode)
- (b) `AppState` runtime flag
- (c) Static function in `jreng_text` module

**Validate:** On UTM/no-GPU, detection returns "no GPU". On physical machine, returns "GPU available".

---

### Step 4.2 — Create ScreenCPU

New class `Terminal::ScreenCPU` — same interface as `Terminal::Screen` but renders via `juce::Graphics` in `paint()` instead of GL.

**What it does:**
- Reads `ScreenSnapshot` (same data as GL path)
- Uses `CPUGlyphCache` for glyph bitmaps
- Uses `CPUTextRenderer` for drawing
- Renders to `juce::Image` double buffer
- Paints result in `paint()`

**What it doesn't do:**
- No `OpenGLContext`
- No `enableGLTransparency()`
- No GL shaders
- No atlas textures

**Validate:** END builds with `ScreenCPU`. Terminal renders correctly in a test harness with CPU rendering forced.

---

### Step 4.3 — Wire renderer selection in MainComponent

`MainComponent::initialiseTabs()` checks GPU flag. If GPU available: attach `GLRenderer`, create `Screen` (GL). If no GPU: skip `GLRenderer`, create `ScreenCPU`.

**Discuss with ARCHITECT:** Should this be a runtime switch or compile-time? Runtime is more flexible (user can force CPU mode in config). Compile-time is simpler.

**Validate:** On physical machine: GL rendering (existing behavior). On UTM: CPU rendering (new behavior). Same terminal functionality on both.

---

### Step 4.4 — Skip DWM blur on CPU path

When GPU is unavailable, skip all DWM blur calls in `applyDwmGlass()`. Window is opaque with tint from config background colour. No `WS_EX_LAYERED` stripping, no `DwmExtendFrameIntoClientArea`, no accent policy.

**Validate:** On UTM: opaque window, no crash, terminal renders. On physical machine: blur works as before.

---

### Step 4.5 — Integration test

Full end-to-end test on all environments:

| Environment | Expected |
|---|---|
| macOS Apple Silicon | GL rendering + CGS blur |
| Windows 11 (GPU) | GL rendering + acrylic blur |
| Windows 10 (GPU) | GL rendering + blur-behind |
| Windows 11 UTM (no GPU) | CPU rendering + opaque window |
| Windows 10 (no GPU) | CPU rendering + opaque window |

**Validate:** All environments produce a working terminal with correct rendering.

---

## Rules for Execution

1. **Always invoke @pathfinder first** — before any code change, discover existing patterns
2. **Validate each step before proceeding** — ARCHITECT builds and tests
3. **Never assume, never decide** — any discrepancy between plan and actual code, discuss with ARCHITECT
4. **No new types without ARCHITECT approval** — check if existing types can be reused
5. **Follow all contracts** — JRENG-CODING-STANDARD, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO
6. **Incremental execution** — one step at a time, ARCHITECT confirms before next step
7. **ARCHITECT runs all git commands** — agents prepare changes only
