# RFC — jam::TextEditor Foundation

Date: 2026-05-06
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END's rendering architecture has been fighting JUCE's OpenGL integration from inception. JAM built a complete parallel GL rendering pipeline — command recording (`gl::GraphicsContext`), command replay (`gl::Renderer`), path tessellation (`GLPath`), stencil clip management, custom 2D shaders (`flat_colour`, `image`) — that duplicates what JUCE provides for free when `OpenGLContext` is attached with `setComponentPaintingEnabled(true)`.

This foundational error cascades: manual viewport management, fake scrollbar, manual coordinate transforms, manual component tree walking with `jam::gl::Component*` casts. Every feature built on this foundation inherits the fight.

The one thing JUCE does NOT do fast enough: text rendering. END's glyph atlas pipeline — shelf-packed atlas, LRU cache, HarfBuzz shaping, FreeType/CoreText rasterization, instanced GL draw, SIMD/NEON CPU compositing — is proven faster than JUCE's built-in text rendering. This is the sole divergence point.

**The mandate: use JUCE fully. Diverge only at the lowest level where we need control — the glyph rendering pipeline.**

---

## Research Summary

### What JUCE Provides (That We Reimplemented)

**`OpenGLContext` with `setComponentPaintingEnabled(true)`:**
- Replaces software renderer with `OpenGLGraphicsContext` for the entire component tree
- Every `Component::paint()` call is GL-accelerated automatically
- Shapes, fills, images, clips — all through JUCE's internal GL shader pipeline
- Frame order: `renderOpenGL()` → `paintComponent()` (tree into FBO) → `drawComponentBuffer()` (FBO composite) → `swapBuffers()`
- Single shared GL thread (`SharedResourcePointer<RenderThread>`) services all contexts
- MessageManager locked during `renderOpenGL()` when `componentPaintingEnabled=true` — safe to read all message-thread data

**`juce::TextEditor` viewport architecture:**
- `Viewport` containing `TextHolderComponent` — the scroll pattern
- `TextHolderComponent::paint()` → `drawContent()` — single rendering entry point
- `checkLayout()`: paragraph heights → viewport sizing → scrollbar visibility (3-iteration convergence loop)
- Word-wrap width = `contentHolder.getWidth() - leftIndent - rightEdgeSpace`, auto-recalculated on resize via `visibleAreaChanged()` reentrant guard
- Selection: character-index `Range<int>`, drag state machine, `indexAtPosition()` hit-test delegating to `ShapedText::getTextIndexForCaret()`
- `CaretComponent` as child of `TextHolderComponent` — positioned in holder-local coordinates
- `ShapedText` (JUCE 8, HarfBuzz-backed): layout (line breaks, paragraph heights, metrics) + rendering (glyph IDs + positions via `accessTogetherWith`)

**`juce::Font` / `juce::FontOptions` (JUCE 8):**
- Immutable font identity with fluent API
- Fallback family list, typeface pointer bypass
- `TypefaceMetrics` for ascent/descent/height
- Internal HarfBuzz shaping (same glyph IDs as direct HarfBuzz — both are font-level identifiers from the same `.ttf`)
- `createSystemFallback()` for codepoint-based fallback resolution

### What JAM Built (The Fight)

| Component | What It Does | JUCE Equivalent |
|---|---|---|
| `jam::gl::GraphicsContext` | Records draw commands as `std::vector<Command>` | JUCE's internal `OpenGLGraphicsContext` |
| `jam::gl::Graphics` | Mirrors `juce::Graphics` API over GraphicsContext | `juce::Graphics` with GL-backed context |
| `jam::gl::Renderer` | Replays commands with custom shaders | JUCE's internal FBO rendering |
| `jam::gl::AtlasRenderer` | Extends Renderer with glyph atlas text pass | — (this is the divergence) |
| `jam::gl::Component` | Component base with GL lifecycle hooks | `juce::Component` (hooks unnecessary with `componentPaintingEnabled`) |
| `jam::gl::Viewport` | Viewport with auto-sync content width | `juce::Viewport` |
| `jam::GLPath` | Ear-clip tessellation + triangle strip stroke | JUCE's internal path tessellation |
| `jam::GLOverlay` | Overlay layer synced to target component | Child `juce::Component` |
| `jam::GLVignette` | Gradient edge effect | `paint()` with `ColourGradient` |
| `jam::gl::ShaderCompiler` | Stateless shader factory | `juce::OpenGLShaderProgram` directly |
| `jam::gl::VertexLayout` | Vertex attribute setup | Inline GL calls |
| `jam::Font` | Font identity wrapping juce + jam typeface | `juce::Font` / `juce::FontOptions` |
| `jam::Typeface` (full) | FT/CT handles + HarfBuzz shaping + metrics + registry + font discovery | `juce::Font` + `juce::Typeface` for identity/shaping/metrics |

### What END Proved Works (The Divergence)

**Glyph atlas pipeline** — the sole area where JUCE's built-in rendering is insufficient:

1. **Rasterization** — platform-native: CoreText `CTFontDrawGlyphs` into pooled `CGBitmapContext` (macOS), FreeType `FT_Render_Glyph` with `FT_LOAD_TARGET_LIGHT` (Linux/Windows). Constrained rendering for Nerd Font icons (scale/translate into cell bounds). Embolden via fill+stroke (CT) or `FT_Outline_Embolden` (FT). Oversize rescale for glyphs exceeding cell width.
2. **Atlas allocation** — `AtlasPacker`: shelf-packing, 4096×4096 (GL) or 2048×2048 (CPU). O(S) first-fit scan. O(1) reset. Two independent packers: mono (R8) and emoji (RGBA8).
3. **LRU cache** — `LRUCache`: 19K mono / 4K emoji capacity. Frame-stamped access. Partial-sort eviction (oldest 10%).
4. **Staged upload** — `Mailbox<StagedBatch>`: lock-free `std::atomic` pointer exchange. Double-buffered batches. Message thread stages → GL thread consumes via `glTexSubImage2D`. Zero mutex.
5. **GPU rendering** — `Glyph::GLContext`: instanced quad draw (`glDrawArraysInstanced`). Per-glyph instance data: `Render::Quad` (screenPos, glyphSize, textureUV, fgColour — 10 floats). Three shader programs: mono (R8 alpha × colour), emoji (RGBA passthrough), background (solid fill). Single VAO, static quad VBO, dynamic instance VBO.
6. **CPU rendering** — `Glyph::GraphicsContext`: SIMD/NEON compositing into pixel buffer. `blendMonoTinted4` (mono atlas + fg tint, 4px/iter), `blendSrcOver4` (premultiplied src-over, 4px/iter), `fillOpaque4` (opaque fill, 4px/iter). SSE2 on x86_64, NEON on ARM64, scalar fallback. Same API surface as `GLContext` (duck-typed).
7. **Packer** — `Glyph::Packer`: central orchestrator. `getOrRasterize(Key) → Region*`. Key = `{glyphIndex, fontFace, fontSize, span}`. Dispatches to platform rasterizer, allocates atlas rect, stages bitmap, caches in LRU. Thread model: all operations message-thread only, cross-thread boundary is solely the `Mailbox`.

**This pipeline is completely self-contained.** It has zero dependency on JAM's GL rendering pipeline (`gl::Renderer`, `gl::GraphicsContext`, command recording/replay). It can operate standalone inside any `paint()` call where GL is active (GPU path) or where a pixel buffer is available (CPU path).

### Font Identity: JUCE Native

`jam::Font` exists to bridge `juce::Font` → `jam::Typeface`. With `jam::Typeface` shrunk to rasterizer-only, this bridge is unnecessary.

| Need | JUCE Native | jam Equivalent | Disposition |
|---|---|---|---|
| Font identity | `juce::Font` / `juce::FontOptions` | `jam::Font` | **Delete jam::Font** |
| Metrics | `juce::Typeface` / `TypefaceMetrics` | `jam::Typeface::Metrics` | **Delete** |
| Text shaping | `ShapedText` (internal HarfBuzz) | `jam::Typeface::shapeText()` | **Delete** |
| Line breaking | `ShapedText` word-wrap | `jam::Shaper::findLineBreaks()` | **Delete** |
| Caret/selection geometry | `ShapedText::getGlyphsBounds()` | `jam::Shaper::getCaretRectangle()` | **Delete** |
| Fallback resolution | `juce::Font` fallback list | `jam::Typeface::findSuitableFontForText()` | **Delete** |
| **Bitmap rasterization** | **Not exposed** | `jam::Typeface` FT_Face/CTFontRef | **Keep, shrink to rasterizer** |

JUCE 8's `ShapedText` uses HarfBuzz internally. Same font file → same glyph IDs as direct HarfBuzz. `ShapedText` provides glyph IDs + positions. The atlas rasterizes those IDs via FreeType/CoreText. Compatible pipeline, zero duplication.

`jam::Typeface` shrinks to **rasterizer only** — owns FreeType/CoreText handles for the same font `juce::Font` references. Sole job: `(glyphID, fontHandle, size) → pixel buffer`. No shaping, no metrics, no style management, no registry. Could be absorbed into `Glyph::Packer` as a private implementation detail.

---

## Principles and Rationale

### BLESSED Compliance

**B — Bound:** `OpenGLContext` owned by top-level window. Glyph resources (Packer, atlas textures) owned by `MainComponent`, injected into `TextEditor`. No floating resources. RAII throughout.

**L — Lean:** Deleting the entire JAM GL pipeline removes thousands of lines of code that duplicate JUCE. `jam::TextEditor` is a surgical modification of the existing fork, not a new system. The glyph pipeline is already lean — shelf packer, LRU cache, staged upload, all bounded.

**E — Explicit:** Dual renderer selection is explicit: `OpenGLContext::getCurrentContext() != nullptr` → GPU path. Detach context → CPU path. No hidden mode flags. `drawContent()` is the single rendering entry point with a clear branch.

**S — SSOT:** `juce::Font` is the single font identity. `ShapedText` is the single source for layout (line breaks, paragraph heights, glyph positions). Grid is the single source for terminal content. `TextEditor` never pokes the grid — it receives styled text and displays it.

**S — Stateless:** `TextEditor` is a dumb painter. Give it text, it gives you viewport + scrollbar + selection + reflow. It holds no opinion about where the text came from. Terminal, Whelmed, anything — same component.

**E — Encapsulation:** `TextEditor` is ignorant of terminals, cells, ANSI. The Grid→TextEditor bridge is the caller's responsibility. The glyph atlas is `TextEditor`'s private rendering detail. JUCE handles all 2D painting. The divergence (atlas) is encapsulated at the lowest possible level.

**D — Deterministic:** Same text input → same visual output. GPU and CPU paths produce identical glyph placement (both use `ShapedText` positions). Atlas rasterization is deterministic per `(glyphID, font, size)`.

### Why Not Fork `juce::TextEditor` at the Source Level

The fork already exists: `jam::gl::TextEditor` is a verbatim copy with three mechanical changes (namespace, base class, viewport type). The path forward is to **fix the fork**, not create a new one:

1. Base class: `jam::gl::Component` → `juce::Component`
2. Internal viewport: `jam::gl::Viewport` → `juce::Viewport`
3. `drawContent()`: inject dual-path rendering (GPU atlas / CPU SIMD)

### Why Not `renderOpenGL()` for Text

Using `renderOpenGL()` for glyph atlas rendering creates a layering problem: `renderOpenGL()` fires before `paintComponent()`. Text would render under selection highlights and cursor, with no way to interleave. By rendering inside `drawContent()` (which runs during `paintComponent()`), the layering is correct: backgrounds → text → selection → cursor. All within the same FBO, same coordinate space.

The GPU atlas draws inside JUCE's paint FBO are straightforward: save GL state, bind atlas shader/textures/VBO, draw instances, restore state. This pattern is established in JUCE's own `OpenGLDemo.h` example.

---

## Architecture

### Top Level

```
OpenGLContext attached to top-level window
├── setComponentPaintingEnabled(true)    ← JUCE renders entire component tree via GL
├── setContinuousRepainting(false)       ← event-driven, not continuous
│
└── Component tree (all plain juce::Component)
    ├── Tabs
    │   └── Panes
    │       ├── Terminal::Display
    │       │   └── jam::TextEditor (Screen)    ← dual-renderer text viewport
    │       │       ├── Viewport + TextHolderComponent
    │       │       ├── CaretComponent
    │       │       └── ScrollBar
    │       └── Whelmed::Component
    │           └── jam::TextEditor             ← same SSOT component
    ├── StatusBarOverlay
    ├── MessageOverlay
    └── ActionList (modal)
```

### jam::TextEditor

**What it is:** A faithful recreation of `juce::TextEditor`'s viewport/scrollbar/selection/reflow logic with a dual-path rendering backend. Reusable by any consumer. Lives in `jam_gui` module.

**Base class:** `juce::Component` (not `jam::gl::Component`).

**Internal viewport:** `juce::Viewport` (not `jam::gl::Viewport`).

**Document model:** Retained from `juce::TextEditor` — `ParagraphStorage`, `ParagraphsModel`, `ShapedText` (lazy), `RangedValues<Font>`, `RangedValues<Colour>`. Drives layout, line-breaking, word-wrap, paragraph heights. This is what makes viewport sizing, scrollbar visibility, and reflow work.

**Input API:** `setText(text, styles)` — caller provides text with font/colour spans. TextEditor never knows where the text came from.

**Rendering — `drawContent()` dual path:**

```
drawContent(juce::Graphics& g)
│
├── For each visible paragraph:
│   ├── Draw backgrounds        → g.fillRect()  [GL or CPU — automatic]
│   └── Draw selection          → g.fillRect()  [GL or CPU — automatic]
│
├── GPU path (glAtlasRenderer != nullptr && OpenGLContext::getCurrentContext()):
│   │   Save JUCE's GL state
│   │   Bind atlas shader + textures + instance VBO
│   │   For each visible paragraph:
│   │     ShapedText::accessTogetherWith → glyph IDs + positions
│   │     → Build Render::Quad instances
│   │     → glDrawArraysInstanced
│   │   Restore GL state
│   │
│   └── Glyph pipeline: Packer → LRU → AtlasPacker → StagedBatch → Mailbox → GLContext
│
└── CPU path (no GL context or no atlas renderer):
        For each visible paragraph:
          ShapedText::accessTogetherWith → glyph IDs + positions
          → SIMD composite into pixel buffer (blendMonoTinted4 / blendSrcOver4)
          → g.drawImageAt() to blit result
```

**Hot-swap:** Attach `OpenGLContext` → GPU path. Detach → CPU path. Zero code change at the TextEditor level. The rendering backend follows the context automatically.

**Glyph atlas integration:** `TextEditor` receives an optional atlas renderer reference (injected, not owned). When present and GL active → GPU path. When absent or no GL → CPU path. The atlas renderer encapsulates: `Glyph::Packer`, `Glyph::GLContext` (GPU) or `Glyph::GraphicsContext` (CPU), atlas textures, staged upload.

### Terminal Integration

```
Grid (SSOT)
  ↓ read (VBlank dirty check)
Terminal::Screen (bridge)
  ↓ setText(styledText)
jam::TextEditor (dumb viewport)
  ↓ drawContent() with dual renderer
Pixels on screen
```

`Terminal::Screen` extends `jam::TextEditor`. Its sole job: read Grid → convert cells to styled text (ANSI attrs → `juce::Font` + `juce::Colour` spans) → `setText()`. Screen never mutates Grid. Grid never knows about Screen. Resize, reflow, selection, scrollbar — all handled by TextEditor.

`Terminal::Display` owns Screen. VBlank fires → checks `state.consumeSnapshotDirty()` → calls `screen.updateFromGrid(grid)` → Screen reads Grid and calls its own `setText()`.

### Whelmed Integration

Same `jam::TextEditor`. Different text source (markdown blocks). Different styling (Whelmed LookAndFeel). Same viewport, scrollbar, selection, reflow. Same dual renderer.

---

## What to Delete

### JAM GL Pipeline (The Fight)

| File/Class | Reason |
|---|---|
| `jam::gl::GraphicsContext` | Command recording — JUCE does this |
| `jam::gl::Graphics` | Wrapper — redundant |
| `jam::gl::Renderer` | Command replay — JUCE does this |
| `jam::gl::AtlasRenderer` | Replay + atlas — atlas moves into TextEditor |
| `jam::gl::Component` | GL base — `juce::Component` suffices |
| `jam::gl::Viewport` | GL viewport — `juce::Viewport` suffices |
| `jam::gl::ShaderCompiler` | Tied to replay pipeline |
| `jam::gl::VertexLayout` | Tied to replay pipeline |
| `jam::GLPath` | Tessellation — JUCE does this |
| `jam::GLOverlay` | Overlay — child Component suffices |
| `jam::GLVignette` | Effect — `paint()` with gradient |

### Font System Duplication

| File/Class | Reason |
|---|---|
| `jam::Font` | Use `juce::Font` / `juce::FontOptions` natively |
| `jam::Typeface` (shaping, metrics, registry, discovery) | Use `juce::Font` + `ShapedText` |
| `jam::Shaper` (line breaks, caret, selection geometry) | Use `ShapedText` |

**Retained from `jam::Typeface`:** FreeType/CoreText handle ownership + bitmap rasterization. Absorbed into `Glyph::Packer` as private implementation detail.

### END Render Infrastructure

| Component | Reason |
|---|---|
| `MainComponent::setRenderer()` + render traversal lambda | No manual component walk — JUCE paints the tree |
| `jam::gl::Component*` cast/walk | Gone with the pipeline |
| `MainComponentActions` renderer construction | No renderer objects |
| Modal/Popup separate `jam::gl::Renderer` | Plain components in tree, JUCE paints them |

---

## What to Keep

### Glyph Atlas Pipeline (The Divergence)

| Component | Role | Location |
|---|---|---|
| `Glyph::Packer` | Orchestrator: `getOrRasterize(Key) → Region*` | `jam_graphics/fonts/` |
| `Glyph::GLContext` | GPU: instanced draw, atlas texture upload | `jam_graphics/opengl/renderers/` |
| `Glyph::GraphicsContext` | CPU: SIMD composite into pixel buffer | `jam_graphics/rendering/` |
| `Glyph::Shaders` | GLSL source (mono/emoji/background vert+frag) | `jam_graphics/opengl/renderers/` |
| `AtlasPacker` | Shelf-packing allocator | `jam_graphics/fonts/` |
| `LRUCache` | Frame-stamped glyph cache with partial-sort eviction | `jam_graphics/fonts/` |
| `StagedBitmap` / `StagedBatch` | CPU→GPU staged upload data | `jam_graphics/fonts/` |
| `Mailbox<T>` | Lock-free atomic pointer exchange | `jam_core/concurrency/` |
| `Render::Quad` / `Render::Background` | Instance data formats | `jam_graphics/rendering/` |
| `jam_simd_blend.h` | SSE2/NEON blend kernels | `jam_graphics/rendering/` |
| FreeType/CoreText rasterization | Platform bitmap generation (absorbed into Packer) | `jam_graphics/fonts/` |

### TextEditor Logic (From Existing Fork)

| Component | Role |
|---|---|
| `Viewport` + `TextHolderComponent` | Scroll architecture, scrollbar |
| `checkLayout()` | Layout → viewport sizing → scrollbar visibility |
| Selection (`Range<int>`, `DragType`, `CaretState`) | Text selection |
| `CaretComponent` | Cursor visual |
| `indexAtPosition()` / mouse handling | Hit-testing, interaction |
| `ParagraphStorage` / `ParagraphsModel` | Document model for layout |
| `ShapedText` | Line breaking, heights, glyph positions |
| `RangedValues<Font>` / `RangedValues<Colour>` | Styled text spans |

### Terminal Pipeline (Untouched)

| Component | Role |
|---|---|
| `State` / `Grid` | SSOT terminal buffer |
| `Parser` / `Processor` | ANSI parsing |
| `Display` VBlank polling | Dirty-check-driven refresh |
| LookAndFeel colour system | ANSI → colour mapping |

---

## Feature Breaks

Fixing the foundation breaks everything built on it. All are in scope.

1. **Render traversal** — `MainComponent`'s lambda that casts to `jam::gl::Component*` and walks the tree → deleted. JUCE's built-in tree painting replaces it entirely.

2. **Modal/popup GL context sharing** — `NativeSharedContextOwner`, `NativeContextResource`, `setSharedRenderer()` → gone. Single `OpenGLContext` with `componentPaintingEnabled(true)` handles the entire window.

3. **All `jam::gl::Renderer` consumers** — `MainComponent`, `MainComponentActions`, `ModalWindow`, `Popup` → rewrite. No renderer objects exist.

4. **Glyph pipeline integration point** — moves from standalone `AtlasRenderer::renderText()` to being called from inside `TextEditor::drawContent()`. `Glyph::GLContext` methods invoked within JUCE's paint FBO, not in a separate render pass.

5. **Atlas/Packer/Font propagation chain** — `MainComponent → Panes → Tabs → Display → Screen` carrying atlas/packer/font references simplifies. TextEditor receives atlas renderer once. Screen doesn't need atlas internals.

6. **`Display::onVBlank` flow** — currently calls `screen.render(grid, cursor)` which writes dummy text. Changes to: Grid → styled text → `textEditor.setText()`.

7. **CellMetrics** — currently on Screen. Moves to the Grid→TextEditor bridge. TextEditor doesn't know about cells.

8. **`jam::Font` / `jam::Typeface` consumers** — all sites using `jam::Font` for identity/shaping → switch to `juce::Font`. Rasterization sites → use Packer's internal handles.

9. **Overlay** (`jam::gl::Component` child of Display) — becomes plain `juce::Component`. No GL lifecycle hooks needed.

10. **END's own rendering files** (`Fonts.h/mm/cpp`, `FontsMetrics.cpp`, `FontsShaping.cpp`, `FontCollection.h/mm/cpp`) — the shaping/metrics portions are superseded by `juce::Font` + `ShapedText`. Rasterization portions absorbed into Packer. Evaluate per-file.

---

## BLESSED Compliance Checklist

- [x] **B — Bound:** OpenGLContext owned by window. Atlas resources owned by MainComponent, injected. RAII throughout. No floating resources.
- [x] **L — Lean:** Deletes thousands of lines of duplicated GL pipeline. TextEditor is a surgical modification of existing fork.
- [x] **E — Explicit:** Dual renderer selection is a single clear branch on GL context presence. No hidden modes. drawContent() is the single entry point.
- [x] **S — SSOT:** juce::Font for identity. ShapedText for layout. Grid for terminal content. No shadow state.
- [x] **S — Stateless:** TextEditor is a dumb painter. No opinion about text source. Caller feeds, TextEditor displays.
- [x] **E — Encapsulation:** TextEditor ignorant of terminals. Atlas is TextEditor's private detail. JUCE handles all 2D painting. Divergence at lowest possible level.
- [x] **D — Deterministic:** Same input → same output. Both paths use ShapedText positions. Atlas rasterization is deterministic per key.

---

## Open Questions

None. All architectural decisions made by ARCHITECT during this session.

---

## Handoff Notes

1. **The existing `jam::gl::TextEditor` IS the starting point** — it's a verbatim fork of `juce::TextEditor`. Three changes: base class (`juce::Component`), viewport type (`juce::Viewport`), rendering path (`drawContent()` dual dispatch).

2. **GL state save/restore inside `drawContent()`** is the established JUCE pattern — see `OpenGLDemo.h` line 844. Not a hack, not fighting.

3. **ShapedText glyph IDs = HarfBuzz glyph IDs = atlas glyph IDs.** JUCE 8's ShapedText uses HarfBuzz internally. Same font file → same glyph indices. Verify during implementation but architecturally sound.

4. **The duck-type contract between `Glyph::GLContext` and `Glyph::GraphicsContext`** (identical method signatures: `push`, `pop`, `uploadStagedBitmaps`, `drawQuads`, `drawBackgrounds`) is the existing dual-renderer interface. Formalize into a proper abstract base or keep as duck-type — COUNSELOR's call.

5. **SPEC.md §End Game** already envisions this: "Both END and WHELMED share a common GL text rendering module (shared via `jam_graphics` module)." This RFC realizes that vision through `jam::TextEditor`.

6. **Scope boundary:** Up to terminal rendering on plain `juce::Component` with dual hot-swappable rendering. Terminal protocol features (parser, grid, PTY) are untouched. Whelmed integration is a separate sprint — the foundation enables it but doesn't implement it.
