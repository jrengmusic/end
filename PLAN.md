# PLAN.md — Text Rendering Pipeline: Extraction, Decoupling, CPU Fallback

**Project:** END
**Date:** 2026-03-24
**Author:** COUNSELOR
**Status:** Active — Plan 4 next

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Seven sequential plans. Each plan is self-contained — delivers working, testable output before the next begins. No plan assumes the next will happen.

| Plan | Objective | Status |
|------|-----------|--------|
| 0 | Vendor FreeType as JUCE module | Done (Sprint 115) |
| 1 | Extract glyph pipeline into `jreng_graphics` module | Done (Sprint 115) |
| 2 | Replace END's rendering with `jreng_graphics` (destructive move) | Done (Sprint 115) |
| 2.5 | Decouple GL from glyph pipeline, rewire Screen to Font API | Done (Sprint 117) |
| 2.6 | Rendering pipeline optimization — metrics cache, dirty rows, CG pooling | Done (Sprint 118) |
| 3 | CPU rendering backend — `GraphicsTextRenderer` + template `Screen` | Done (Sprint 119) |
| 4 | END runtime GPU/CPU switching | Done (Sprint 121) |

### Surface API

`jreng::TextLayout` — drop-in replacement for `juce::TextLayout`.

- **Input:** `juce::AttributedString` (no custom string type)
- **Rendering:** `jreng::TextLayout::draw<GraphicsContext> (context, area)` — template duck-typing
- **GL backend:** `GLTextRenderer` satisfies duck-type — instanced quad rendering
- **Graphics backend:** Plan 3 — `GraphicsTextRenderer` satisfies duck-type — `juce::Graphics` blit rendering

### Architecture (current — post-Plan 2.6)

Two rendering paths exist:

**TextLayout path** (UI components: LookAndFeel, overlays):
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
TextLayout::draw<GraphicsContext> (context, area)
        |                              |
        v                              v
  setFont() + drawGlyphs()      setFont() + drawGlyphs()
    GLTextRenderer                 GraphicsTextRenderer (Plan 3)
   (jreng_opengl)                 (jreng_graphics)
```

**Screen path** (terminal grid):
```
Grid + State
      |
      v
Screen::buildSnapshot()  (MESSAGE THREAD)
      |
      v
Render::Snapshot  (Quad[] + Background[] + cursor)
      |
      v
SnapshotBuffer::write() → read()
      |
      v
Renderer::drawBackgrounds() + drawQuads()
      |                              |
      v                              v
  GLTextRenderer                 GraphicsTextRenderer (Plan 3)
  (GL THREAD)                    (MESSAGE THREAD paint())
```

### Module Dependency (current)

```
jreng_core       (Mailbox, SnapshotBuffer — lock-free utilities) [Plan 3 moves these here]
     ^
     |
jreng_graphics   (Font, Atlas, Typeface, TextLayout, Render types, GraphicsTextRenderer)
     ^
     |
jreng_opengl     (GLTextRenderer, GLRenderer, shaders, GL context)
     ^
     |
END app          (Screen<Renderer>, Terminal::Component, MainComponent)
```

`jreng_graphics` has zero GL dependency. `jreng_opengl` depends on `jreng_graphics`.

### Renderer Duck-Type Contract

Both `GLTextRenderer` and `GraphicsTextRenderer` satisfy this implicit interface via C++ template duck-typing. No virtual dispatch.

```cpp
// Required by TextLayout::draw<GraphicsContext>:
void setFont (jreng::Font& font) noexcept;
void drawGlyphs (const uint16_t* glyphCodes,
                 const juce::Point<float>* positions,
                 int count) noexcept;

// Required by Screen<Renderer>:
void createContext() noexcept;            // GL: compile shaders. Graphics: no-op.
void closeContext() noexcept;             // GL: release GPU. Graphics: no-op.
bool isReady() const noexcept;            // GL: shaders compiled. Graphics: always true.
void uploadStagedBitmaps (jreng::Typeface&) noexcept;  // GL: glTexSubImage2D. Graphics: copy to juce::Image.
void setViewportSize (int w, int h) noexcept;
void push (int x, int y, int w, int h, int fullHeight) noexcept;
void pop() noexcept;
void drawQuads (const Render::Quad* data, int count, bool isEmoji) noexcept;
void drawBackgrounds (const Render::Background* data, int count) noexcept;
static constexpr int getAtlasDimension() noexcept;  // GL: 4096, CPU: 2048
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

## PLAN 2.6: Rendering Pipeline Optimization — DONE

Completed Sprint 118. Three optimizations:
- **Metrics caching** on Typeface (`cachedMetricsSize` / `cachedMetrics`). `calcMetrics()` returns cached result when size matches.
- **Dirty-row skipping** in `buildSnapshot()`. Consumes `grid.consumeDirtyRows()` bitmask, skips clean rows.
- **CGBitmapContext pooling** on macOS. `ensurePooledContext()` reuses persistent mono/emoji contexts.

---

## PLAN 3: CPU Rendering Backend — `GraphicsTextRenderer` + Template `Screen`

**Goal:** `juce::Graphics`-based rendering backend for both TextLayout (UI) and Screen (terminal grid). Same call sites as GL path. Template `Screen<Renderer>` enables compile-time backend selection with zero runtime overhead.

**Covers:**
1. Move rendering-agnostic infrastructure out of `jreng_opengl`
2. `GraphicsTextRenderer` in `jreng_graphics` — duck-type compatible with `GLTextRenderer`
3. `Screen<Renderer>` template — identical rendering code, swappable backend

### Design Decisions (ARCHITECT approved)

1. **Template duck-typing, not virtual dispatch.** `Screen<Renderer>` and `TextLayout::draw<GraphicsContext>` use compile-time polymorphism. Zero vtable overhead.
2. **`GraphicsTextRenderer` lives in `jreng_graphics`.** All CPU rendering code stays in the GL-free module.
3. **`juce::Image` atlas mirrors GL atlas.** Same atlas packing, same UV coordinates. `GraphicsTextRenderer` drains the same staged bitmap queue — copies pixels to `juce::Image` instead of `glTexSubImage2D`.
4. **`Render::Quad`, `Render::Background`, `SnapshotBase` move to `jreng_graphics`.** Pure geometry + colour PODs with zero GL dependency.
5. **`Mailbox` and `SnapshotBuffer` move to `jreng_core`.** Lock-free atomic utilities with zero GL dependency. Renamed from `GLMailbox` / `GLSnapshotBuffer`.
6. **GL state consolidation.** `glViewport`, `glEnable(GL_BLEND)`, `glBlendFunc`, `glDisable(GL_BLEND)` move into `GLTextRenderer::beginFrame()` / `endFrame()`. Screen never touches GL directly.
7. **Mono glyph tinting.** Mono atlas stores alpha coverage. `GraphicsTextRenderer::drawQuads()` blits glyph alpha tinted with `Render::Quad::foregroundColor`. Emoji atlas stores full ARGB — direct blit.

---

### Step 3.0 — Move snapshot infrastructure out of `jreng_opengl`

Move rendering-agnostic types and utilities so `Screen` and `GraphicsTextRenderer` can live outside `jreng_opengl`.

**Move to `jreng_core`:**
- `jreng_opengl/context/jreng_gl_mailbox.h` → `jreng_core/concurrency/jreng_mailbox.h` — rename `GLMailbox` → `Mailbox`
- `jreng_opengl/context/jreng_gl_snapshot_buffer.h` → `jreng_core/concurrency/jreng_snapshot_buffer.h` — rename `GLSnapshotBuffer` → `SnapshotBuffer`

**Move to `jreng_graphics`:**
- `jreng_opengl/renderers/jreng_glyph_render.h` → `jreng_graphics/rendering/jreng_glyph_render.h` — `Render::Quad`, `Render::Background`, `SnapshotBase`

**Update consumers:**
- `jreng_opengl.h` — remove moved includes, add `jreng_graphics` dependency (already exists)
- `jreng_core.h` — add new includes
- `jreng_graphics.h` — add new include
- `Screen.h` — `jreng::GLSnapshotBuffer` → `jreng::SnapshotBuffer`
- All files referencing `GLMailbox` → `Mailbox`, `GLSnapshotBuffer` → `SnapshotBuffer`

**Validate:** END builds. No behavior change. `grep -r "GLMailbox\|GLSnapshotBuffer" modules/` returns nothing.

---

### Step 3.1 — Consolidate GL state into `GLTextRenderer`

Move all GL state management from `ScreenGL.cpp` into the renderer. Screen never touches GL directly after this step.

**GLTextRenderer gains:**
- `beginFrame (int x, int y, int w, int h, int fullHeight) noexcept` — `glViewport(x, fullHeight - y - h, w, h)`, `setViewportSize(w, h)`, `glEnable(GL_BLEND)`, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
- `endFrame() noexcept` — `glDisable(GL_BLEND)`

**ScreenGL.cpp `renderOpenGL` becomes:**
```cpp
if (textRenderer.isReady())
{
    const int x { originX + glViewportX };
    const int y { originY + glViewportY };
    textRenderer.beginFrame (x, y, glViewportWidth, glViewportHeight, fullHeight);
    textRenderer.uploadStagedBitmaps (font);

    Render::Snapshot* snapshot { resources.snapshotBuffer.read() };
    if (snapshot != nullptr)
    {
        // ... drawBackgrounds, drawQuads, drawCursor — unchanged
    }

    textRenderer.endFrame();
}
```

**Files:**
- `modules/jreng_opengl/renderers/jreng_gl_text_renderer.h` — add `beginFrame`, `endFrame`
- `modules/jreng_opengl/renderers/jreng_gl_text_renderer.cpp` — implement
- `Source/terminal/rendering/ScreenGL.cpp` — remove raw GL calls, use `beginFrame`/`endFrame`

**Validate:** END builds. Identical rendering. No raw `juce::gl::` calls remain in Screen.

---

### Step 3.2 — `GraphicsTextRenderer` in `jreng_graphics`

New class satisfying the same duck-type contract as `GLTextRenderer`. All rendering via `juce::Graphics`.

**Class:** `jreng::Glyph::GraphicsTextRenderer` in `jreng_graphics/rendering/`

**Data:**
- `juce::Image monoAtlas` — `SingleChannel`, `atlasDimension() x atlasDimension()`
- `juce::Image emojiAtlas` — `ARGB`, `atlasDimension() x atlasDimension()`
- `juce::Graphics* graphics { nullptr }` — bound per-frame via `setGraphicsContext()`
- `jreng::Font* currentFont { nullptr }`
- `int viewportWidth { 0 }, viewportHeight { 0 }`

**Duck-type methods:**
- `contextCreated()` — allocate `juce::Image` atlases
- `contextClosing()` — release atlases
- `isReady()` — `return monoAtlas.isValid()`
- `uploadStagedBitmaps (Typeface&)` — drain `typeface.consumeStagedBitmaps()`, copy pixel data into `juce::Image` atlas via `Image::BitmapData` write access
- `setViewportSize (int w, int h)` — store dimensions
- `beginFrame (int x, int y, int w, int h, int fullHeight)` — store viewport offset for coordinate translation
- `endFrame()` — no-op
- `drawQuads (const Render::Quad*, int, bool isEmoji)` — for each quad: compute source rect from `textureCoordinates` × atlas dimension, dest rect from `screenPosition` + `glyphSize`. Mono: tint alpha with foreground colour. Emoji: direct blit.
- `drawBackgrounds (const Render::Background*, int)` — for each bg: `g.setColour()`, `g.fillRect(screenBounds)`
- `setFont (Font&)` — store font pointer
- `drawGlyphs (const uint16_t*, const Point<float>*, int)` — for each glyph: `currentFont->getGlyph(code)` → `Region*`, compute source/dest rects, blit from atlas
- `setGraphicsContext (juce::Graphics&)` — CPU-specific: bind paint target
- `static constexpr int atlasDimension()` — `return Atlas::atlasDimension()`

**Mono glyph tinting strategy:**
The mono atlas `juce::Image` is `SingleChannel` (8-bit alpha). For each mono quad:
1. Read source sub-rect from `monoAtlas` via `BitmapData`
2. Blit to destination with foreground colour tint: `pixel = fg_colour * glyph_alpha`
3. Implementation: `g.reduceClipRegion()` with glyph image as mask + `g.setColour(fg)` + `g.fillAll()`, or direct pixel compositing via `BitmapData` — ARCHITECT decides during implementation.

**Files:**
- New: `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.h`
- New: `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp`
- Update: `jreng_graphics.h` — add include

**Validate:** Builds. `TextLayout::draw (graphicsRenderer, area)` compiles and draws glyphs via `juce::Graphics`.

---

### Step 3.3 — Template `Screen<Renderer>`

`Screen` becomes a class template parameterised on the renderer type.

**Changes to `Screen.h`:**
- `class Screen` → `template <typename Renderer> class Screen`
- Member `jreng::Glyph::GLTextRenderer textRenderer` → `Renderer textRenderer`
- `renderOpenGL (int, int, int)` uses `textRenderer.beginFrame()` / `endFrame()` (Step 3.1 contract)
- `glContextCreated()` / `glContextClosing()` delegate to `textRenderer.contextCreated()` / `textRenderer.contextClosing()` — works for both (GL does real work, Graphics no-ops)

**Changes to Screen .cpp files:**
- `ScreenGL.cpp`, `ScreenRender.cpp`, `Screen.cpp` — implementation stays in .cpp with explicit template instantiation at bottom:
  ```cpp
  template class Screen<jreng::Glyph::GLTextRenderer>;
  template class Screen<jreng::Glyph::GraphicsTextRenderer>;
  ```

**Changes to `Terminal::Render`:**
- `Render::Glyph`, `Render::Background`, `Render::Snapshot` aliases unchanged — they reference types now in `jreng_graphics`

**Validate:** END builds with `Screen<GLTextRenderer>`. Identical rendering. `Screen<GraphicsTextRenderer>` compiles (not yet wired to a paint path — that is Plan 4).

---

### Step 3.4 — Force Graphics path, validate end-to-end

Temporarily disable GL rendering (comment out, non-destructive). Wire `Screen<GraphicsTextRenderer>` as the exclusive rendering path. ARCHITECT tests full terminal functionality and measures performance.

**Changes (non-destructive — comments only, no code deletion):**
- `MainComponent` / `Terminal::Component` — comment out `GLRenderer` attachment, GL context setup, GL render loop
- Wire `Screen<GraphicsTextRenderer>` instead of `Screen<GLTextRenderer>`
- Terminal rendering driven by `Component::paint (juce::Graphics&)` on message thread
- `Screen::renderOpenGL` replaced by equivalent paint-path call: `textRenderer.setGraphicsContext(g)` → `beginFrame` → `uploadStagedBitmaps` → draw snapshot → `endFrame`

**Validation:**
1. Terminal launches without GL context
2. Text rendering: mono glyphs, bold, italic, emoji, ligatures — visually correct
3. Backgrounds: cell backgrounds, selection overlay, block characters
4. Cursor: all shapes (block, underline, bar), blink, focus/unfocus
5. Font size change: atlas images repopulate, metrics recompute
6. Scrolling: rapid scroll (`seq 1 10000000`), scrollback navigation
7. Performance: `time seq 1 10000000` — measure wall time vs GL baseline (12.39s)
8. Resize: grid recomputes, no artifacts
9. UI chrome: LookAndFeel overlays render via `TextLayout::draw<GraphicsTextRenderer>`

**After validation:** Restore GL path (uncomment). Both paths confirmed working. Plan 4 adds runtime switching.

**Scope:** Each step is standalone. ARCHITECT builds and tests after each step before proceeding.

---

## PLAN 4: END Runtime GPU/CPU Switching

**Goal:** User-configurable rendering engine via `gpu.acceleration`. Hot-reloadable. Auto-fallback on machines without GPU. GPU = glassmorphism + GL render loop. CPU = opaque + SIMD compositing via `paint()`.

### Config

```lua
END = {
    gpu = {
        acceleration = "auto",  -- "auto" | "true" | "false"
    },
}
```

| Value | Behavior |
|-------|----------|
| `"auto"` | Detect GPU capability before creating renderer. GPU if available, CPU fallback. |
| `"true"` | Force GPU. If unavailable, error in MessageOverlay, fallback to CPU. |
| `"false"` | Force CPU. No GL context created. |

### Design Decisions (ARCHITECT approved)

1. **`std::variant<Screen<GLTextRenderer>, Screen<GraphicsTextRenderer>>`** in `Terminal::Component`. Runtime selection, both compiled in. Zero virtual dispatch — `std::visit` with visitor.
2. **Hot-reload:** Config change triggers variant swap. Grid/State/Session survive — they are independent of Screen. Render cache rebuilds automatically via `markAllDirty()`. No serialization needed.
3. **GPU detection before renderer creation.** `"auto"` probes GL capability at startup, not on failure. No trial-and-error.
4. **MessageOverlay feedback.** After reload, show renderer status: `"RELOADED (GPU)"` or `"RELOADED (CPU)"`. On auto-fallback: `"GPU unavailable — using CPU renderer"`.
5. **GLRenderer lifecycle.** `MainComponent` owns `GLRenderer`. Attached only when GPU path is active. Detached on switch to CPU. Re-attached on switch back to GPU. `attachTo()` / `detach()` are safe to call at any time.
6. **Opacity follows renderer.** GPU: `setOpaque(false)`, glassmorphism via NSView/DWM. CPU: `setOpaque(true)`, `setBufferedToImage(true)`, bg fill from LookAndFeel.
7. **Popup has own GLRenderer** (`Popup.cpp:54`). Popup follows the same `gpu.acceleration` setting independently.

### Architecture

```
Config::Key::gpuAcceleration
        |
        v
MainComponent::applyConfig()
        |
        v
    resolveRenderer()  — "auto" probes GPU, returns enum { gpu, cpu }
        |
        v
    if changed:
        GPU → CPU: glRenderer.detach(), all terminals switchToCPU()
        CPU → GPU: glRenderer.attachTo(*this), all terminals switchToGPU()
        |
        v
Terminal::Component::switchRenderer (RendererType type)
        |
        v
    screen.emplace<Screen<GLTextRenderer>>() or <GraphicsTextRenderer>()
    setOpaque() / setBufferedToImage() toggle
    grid.markAllDirty() + state.setSnapshotDirty()
    rewire onRepaintNeeded (glRenderer.triggerRepaint vs terminal->repaint)
```

### Screen Variant

```cpp
// Terminal::Component member:
using ScreenVariant = std::variant<
    Screen<jreng::Glyph::GLTextRenderer>,
    Screen<jreng::Glyph::GraphicsTextRenderer>>;

ScreenVariant screen;
```

All call sites use `std::visit`:
```cpp
std::visit ([&] (auto& s) { s.render (state, grid); }, screen);
std::visit ([&] (auto& s) { s.renderPaint (g, 0, 0, h); }, screen);
```

`renderPaint` is CPU-only. `renderOpenGL` is GPU-only. The visitor dispatches to the active alternative. Calling the wrong method on the wrong alternative is a compile error — the duck-type contract ensures both have `render()`, but only `GraphicsTextRenderer` has `setGraphicsContext` and only `GLTextRenderer` has GL lifecycle methods.

---

### Step 4.0 — Config key + GPU detection

Add `Config::Key::gpuAcceleration` with default `"auto"`. Add `resolveRenderer()` utility that returns `enum class RendererType { gpu, cpu }` based on config value + GPU capability probe.

**GPU probe:** Attempt `juce::OpenGLContext` creation on a dummy component. If `openGLContextCreated` fires successfully, GPU is available. Tear down immediately. Cache result — probe once at startup, re-probe on explicit `"auto"` reload.

**Files:**
- `Source/config/Config.h` — add `Key::gpuAcceleration`
- `Source/config/Config.cpp` — add `initKeys` entry, Lua loader for `gpu` table
- New: `Source/component/RendererType.h` — `enum class RendererType { gpu, cpu }` + `resolveRenderer()` utility

**Validate:** Config loads `gpu.acceleration`. `resolveRenderer()` returns correct type. No rendering changes yet.

---

### Step 4.1 — Screen variant in Terminal::Component

Replace `Screen<GraphicsTextRenderer> screen` with `ScreenVariant screen`. Add `switchRenderer(RendererType)` method. Update all `screen.*` call sites to use `std::visit`.

**Call sites to update:**
- `onVBlank()` — `screen.render(state, grid)`
- `paint()` — `screen.renderPaint(g, ...)`
- `renderGL()` — `screen.renderOpenGL(...)`
- `glContextCreated()` — `screen.glContextCreated()`
- `glContextClosing()` — `screen.glContextClosing()`
- `resized()` — `screen.setViewport(...)`
- `applyConfig()` — `screen.setLigatures(...)`, `screen.setEmbolden(...)`, `screen.setTheme(...)`
- `initialise()` — `screen.setFontSize(...)`
- All other `screen.*` calls

**`switchRenderer(RendererType)` method:**
1. Emplace new variant alternative (destroys old Screen, constructs new)
2. GPU: `setOpaque(false)`, remove `setBufferedToImage`
3. CPU: `setOpaque(true)`, `setBufferedToImage(true)`
4. Reapply settings from config (ligatures, embolden, theme, font size)
5. `grid.markAllDirty()`, `state.setSnapshotDirty()`

**Files:**
- `Source/component/TerminalComponent.h` — `ScreenVariant`, `switchRenderer()`
- `Source/component/TerminalComponent.cpp` — all `screen.*` → `std::visit`

**Validate:** Builds. Hardcode CPU variant. Terminal works identically to current state.

---

### Step 4.2 — MainComponent wiring + repaint path

Wire `MainComponent::applyConfig()` to detect renderer change and propagate to all terminals. Wire `onRepaintNeeded` callback to use `glRenderer.triggerRepaint()` for GPU or `terminal->repaint()` for CPU.

**Changes:**
- `MainComponent::applyConfig()` — call `resolveRenderer()`, compare to current, if changed: attach/detach `glRenderer`, iterate all terminals via `tabs->switchRenderer(type)`
- `Tabs::switchRenderer(RendererType)` — iterates panes, calls `Terminal::Component::switchRenderer()` on each
- `onRepaintNeeded` callback — branch on active renderer type
- `MainComponent::initialiseTabs()` — use `resolveRenderer()` for initial setup instead of hardcoded CPU
- Uncomment GL code paths — `glContextCreated()`, `glContextClosing()`, `renderGL()`, gated by variant type

**Files:**
- `Source/MainComponent.cpp` — `applyConfig()`, `initialiseTabs()`, `onRepaintNeeded`
- `Source/component/Tabs.h/cpp` — `switchRenderer()` forwarding
- `Source/component/Panes.h/cpp` — `switchRenderer()` forwarding
- `Source/component/TerminalComponent.cpp` — uncomment GL paths, gate by variant

**Validate:** Launch with `gpu.acceleration = "false"` → CPU path. Change to `"true"` → reload → GPU path. Change back → CPU. No crash, no artifacts.

---

### Step 4.3 — MessageOverlay renderer feedback

Show renderer status after reload and on auto-fallback.

**Changes:**
- After config reload in `MainComponent.cpp`: `messageOverlay->showMessage("RELOADED (GPU)", 1000)` or `"RELOADED (CPU)"` based on active renderer
- On auto-fallback (GPU requested but unavailable): `messageOverlay->showMessage("GPU unavailable — CPU renderer", 3000)`
- On startup with `"auto"`: silent (no overlay unless fallback occurred)

**Files:**
- `Source/MainComponent.cpp` — reload path

**Validate:** Reload with each config value. Correct overlay message shown.

---

### Step 4.4 — Popup renderer alignment

Popup has its own `GLRenderer`. Align with `gpu.acceleration` setting.

**Changes:**
- `Popup::ContentView::initialiseGL()` — respect `gpu.acceleration`. If CPU, skip GL attachment, use `paint()` path.
- Popup's `Terminal::Component` uses same `ScreenVariant` — follows global setting.

**Files:**
- `Source/component/Popup.h/cpp`

**Validate:** Open popup in CPU mode — renders via paint. GPU mode — renders via GL.

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
