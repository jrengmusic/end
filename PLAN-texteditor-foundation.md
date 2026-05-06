# PLAN: TextEditor Foundation

**RFC:** RFC-texteditor-foundation.md
**Date:** 2026-05-06
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)

## Overview

Replace the custom JAM GL rendering pipeline with JUCE-native component painting (`OpenGLContext::setComponentPaintingEnabled(true)`). `jam::TextEditor` becomes a `juce::Component` with dual-path glyph atlas rendering inside `drawContent()`. JAM GL pipeline deleted. Screen validated as a derived TextEditor rendering dummy text via glyph atlas (both GPU and CPU). Grid→Screen wiring is a separate follow-up step — only after TextEditor is proven as a fully working dumb painter.

## Language / Framework Constraints

C++ / JUCE — MANIFESTO enforced as written per LANGUAGE.md.

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions.

## Steps

### Step 1: Foundation Swap (Atomic)

**Scope:** `jam/jam_gui/text_editor/jam_text_editor.h`, `jam_text_editor.cpp`, `jam_text_editor_model.cpp`, `Source/terminal/rendering/Screen.h`, `Screen.cpp`, `Source/MainComponent.h`, `MainComponent.cpp`
**Action:**
1. `jam::gl::TextEditor` base class: `jam::gl::Component` → `juce::Component`
2. Internal viewport: `std::unique_ptr<jam::gl::Viewport>` → `std::unique_ptr<juce::Viewport>`
3. Remove GL lifecycle hooks from TextEditor (`glContextCreated`, `glContextClosing`)
4. Screen: remove GL lifecycle overrides, keep existing API surface
5. MainComponent: replace `window->setRenderer(make_unique<AtlasRenderer>(...))` + tree-walk lambda with `OpenGLContext` owned by MainComponent, `setComponentPaintingEnabled(true)`, `setContinuousRepainting(false)`, attached to peer
6. Remove `setRenderables()`, `dynamic_cast<jam::gl::Component*>` walker
7. Keep namespace `jam::gl` temporarily to avoid consumer explosion
**Validation:** Compiles. No renderer objects. No GL lifecycle hooks. OpenGLContext with componentPaintingEnabled drives the tree.

### Step 2: drawContent() Dual-Path Rendering

**Scope:** `jam/jam_gui/text_editor/jam_text_editor.h`, `jam_text_editor.cpp`
**Action:**
1. No explicit injection API — Packer discovered via `jam::Typeface` from the current `juce::Font` (set via `setFont()`)
2. `drawContent()` background/selection: `g.fillRect()` — JUCE handles GL/CPU automatically
3. GPU path (`OpenGLContext::getCurrentContext() != nullptr`): save GL state → bind atlas shaders/textures/VBO → ShapedText→glyph IDs→`Render::Quad` instances → `glDrawArraysInstanced` → restore GL state
4. CPU path: ShapedText→glyph IDs→SIMD composite into pixel buffer → `g.drawImageAt()`
5. Layering: backgrounds → text → selection → cursor (all within same FBO)
**Validation:** Text renders via atlas in both GPU and CPU modes. Backgrounds/selection via JUCE. Correct layering.

### Step 3: Font System Collapse — Typeface Registry

**Scope:** `jam/jam_graphics/fonts/jam_font.h`, `jam_typeface.h/.mm/.cpp`, all consumers
**Action:**
1. `jam::Typeface` becomes `juce::Typeface` subclass — carries FT_Face/CTFontRef handles (per-instance)
2. Static registry owns everything: `static Owner<Typeface>` for instances, `static Glyph::Packer` (shared), `static GlyphAtlas` (GL textures), `static GraphicsAtlas` (CPU images)
3. `registerTypeface()` takes `unique_ptr<Typeface>`, `Owner` holds it. `findTypeface()` returns non-owning pointer. All resources contained — nothing floats outside the registry
4. `juce::Font` with `jam::Typeface` Ptr is the single font identity — `setFont()` wires everything (TextEditor extracts `jam::Typeface` → static Packer/atlas access, zero injection API)
5. Delete `jam::Font` — replaced by `juce::Font` / `juce::FontOptions` at all sites
6. Remove from `jam::Typeface`: `shapeText()`, `shapeEmoji()`, `shapeFallback()`, `getMetrics()`, `Metrics` struct, `resolveFromJuceFont()`, `findSuitableFontForText()`, `GlyphRun`, `Face` HarfBuzz members
7. Screen constructor: `jam::Font&` → `juce::Font`
8. MainComponent: drop `jam::Font font`, `packer`, `glyphAtlas`, `graphicsAtlas` members — all owned by Typeface registry now
**Validation:** All font identity via `juce::Font`. `jam::Typeface : juce::Typeface` is rasterizer-only. Registry owns all instances + Packer + atlas stores. No `jam::Font` references remain. No manual font/glyph resources outside the registry.

### Step 4: Consumer Cleanup

**Scope:** `Source/component/Popup.h`, `Source/terminal/rendering/Overlay.h/.cpp`, modal/glass components
**Action:**
1. All components that extend `jam::gl::Component` → extend `juce::Component`
2. Remove GL lifecycle hooks from Overlay, Popup, any modal components
3. Remove all `jam::gl::Component` references from END source tree
**Validation:** All END components are plain `juce::Component`. No GL hooks outside TextEditor's `drawContent()`.

### Step 5: Delete JAM GL Pipeline

**Scope:** `jam/jam_graphics/opengl/context/`, `jam/jam_graphics/opengl/renderers/jam_gl_path.*`, `jam/jam_gui/` module headers
**Action:**
1. Delete: `gl::GraphicsContext`, `gl::Graphics`, `gl::Renderer`, `gl::AtlasRenderer`, `gl::Component`, `gl::Viewport`, `gl::ShaderCompiler`, `gl::VertexLayout`, `GLPath`, `GLOverlay`, `GLVignette`
2. Delete: `jam::Font` (`jam_font.h`), `jam::Shaper` if present
3. Delete: `jam::Typeface` shaping/metrics/registry code (files or portions)
4. Update `jam_graphics` and `jam_gui` module header files — remove deleted includes
5. Remove `MainComponentActions` renderer-related code if still present
**Validation:** Dead code removed. Modules compile. No dangling references.

### Step 6: Namespace Rename

**Scope:** `jam/jam_gui/text_editor/`, all consumers
**Action:**
1. `jam::gl::TextEditor` → `jam::TextEditor`
2. Update all consumers: Screen, Display, any other references
3. Update `jam_gui` module namespace declarations
**Validation:** TextEditor lives in `jam` namespace. All references updated. Compiles.

### Step 7: Screen Validation (Dummy Text)

**Scope:** `Source/terminal/rendering/Screen.h`, `Screen.cpp`
**Action:**
1. Screen extends `jam::TextEditor`, sets dummy text via `setText()` in `render()`
2. Verify glyph atlas rendering works for both GPU path (GL attached) and CPU path (GL detached)
3. Verify viewport, scrollbar, reflow all function correctly as inherited TextEditor behavior
4. Screen remains a stub — no Grid wiring. Dummy text only.
**Validation:** Screen renders dummy text via atlas. Both GPU and CPU paths produce correct output. Viewport scrolls. Resize reflows.

### Step 8: Screen Grid→TextEditor Bridge (FUTURE — not this sprint)

**Scope:** `Source/terminal/rendering/Screen.h`, `Screen.cpp`
**Action:**
1. Replace stub `render()`: read Grid cells → convert ANSI attrs to `juce::Font` + `juce::Colour` styled spans → `setText(styledText)`
2. `resolveColour()` already exists — use it for the bridge
3. CellMetrics: keep on Screen (bridge between cell-grid world and TextEditor world)
4. Screen never mutates Grid. Display's `onVBlank` flow unchanged.
**Validation:** Terminal output renders correctly. Scrollback visible. Reflow on resize works.
**Gate:** Only after Step 7 proves TextEditor is a fully working dumb painter.

## BLESSED Alignment

- **B:** OpenGLContext owned by MainComponent. Typeface registry (`Owner<Typeface>` + Packer + atlas stores) is self-contained static — no manual resources outside. RAII. No floating resources.
- **L:** Thousands of lines deleted (entire GL command pipeline). TextEditor is surgical modification of existing fork.
- **E:** Dual renderer = single branch on GL context presence in `drawContent()`. No hidden modes.
- **S (SSOT):** `juce::Font` for identity. `ShapedText` for layout. Grid for terminal content. No shadow state.
- **S (Stateless):** TextEditor is a dumb painter. No opinion about text source.
- **E (Encapsulation):** TextEditor ignorant of terminals. Atlas is TextEditor's private rendering detail. JUCE handles all 2D painting.
- **D:** Same input → same output. Both paths use ShapedText positions. Atlas rasterization deterministic per key.

## Risks / Open Questions

1. **Naming:** `jam::gl::TextEditor` → `jam::TextEditor` — approved.
2. **ShapedText glyph ID compatibility:** RFC note 3 — JUCE 8 `ShapedText` uses HarfBuzz internally. Same glyph IDs expected. Verify during Step 2.
3. **GL state save/restore:** Step 2 highest-risk — atlas draws inside JUCE's paint FBO. Pattern from JUCE `OpenGLDemo.h` line 844.
