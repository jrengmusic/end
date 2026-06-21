# RFC — Rendering Optimization: GPU Instanced Draw + CPU Background Fill

Date: 2026-06-21
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END's glyph rendering pipeline (`jam::glyph::Graphics`) composites on the CPU via SIMD into a `SoftwareImageType` render target, then blits through `juce::Graphics`. When an OpenGL context is attached, JUCE's ShaderContext uploads the entire software image as a GL texture every frame via its internal texture cache. The per-pixel CPU work and full-image GPU upload scale with viewport pixel count, not glyph count.

Two independent optimizations:
1. **CPU background fill** — replace scalar per-pixel loop with vectorized `std::fill_n`
2. **GPU instanced draw** — bypass the software image entirely; draw instanced quads directly into JUCE's component FBO during `paint()`

---

## Research Summary

### Git Archaeology

The original END (commit `de22c5c`) had a Snapshot+Mailbox+instanced-draw architecture. This RFC supersedes that design — JUCE's native OpenGL paint pipeline makes it unnecessary. The key insight: when an OpenGLContext is attached, `paint(juce::Graphics& g)` already executes on the GL thread with the component FBO bound. `glyph::Graphics::pop()` can issue `glDrawArraysInstanced` directly into that FBO.

### JUCE 8 OpenGL Rendering Contract (Verified from Source)

`juce_OpenGLContext.cpp`, `CachedImage::renderFrame()`:

1. `renderOpenGL()` executes FIRST — draws to default framebuffer
2. JUCE acquires MessageManager lock on the GL thread
3. Component `paint()` renders dirty regions into `cachedImageFrameBuffer` FBO via `OpenGLGraphicsContext` (ShaderContext — GPU-accelerated 2D)
4. FBO texture composites ON TOP with premultiplied alpha blending
5. `paint()` executes with GL context current, FBO bound — raw GL calls are valid

**Cross-platform:** When OpenGLContext is attached, D2D (Windows) and CoreGraphics (macOS) are bypassed entirely. Pure GL on both platforms.

### Benchmark Results (Apple M4, GL 4.1 Metal)

Standalone benchmark at `~/Documents/Poems/dev/bench/`.

**100K quads (all visible — simulates 5K fullscreen terminal):**

| Mode | ms/frame | Description |
|------|----------|-------------|
| CPU (scalar bg + SIMD blend + drawImageAt) | 38 ms | Current path |
| GPU renderOpenGL (instanced draw + glFinish) | 7 ms | Manual offload to renderOpenGL |
| **GPU-in-paint (instanced draw into FBO + glFinish)** | **~6 ms** | Draw inside paint(), no extra composite |

**1M quads (viewport-clipped — proves CPU clipping is efficient):**

| Mode | ms/frame |
|------|----------|
| CPU (with std::fill_n bg) | 12.9 ms |
| GPU renderOpenGL | 21.6 ms |
| **GPU-in-paint** | **19.0 ms** |

**Key findings:**
- GPU-in-paint is **12% faster** than renderOpenGL (eliminates one compositing pass)
- CPU with viewport clipping is efficient — invisible quads cost near-zero (pixel loops skip immediately)
- In real terminal usage, grid = viewport (all quads visible). The 100K test is the fair comparison: GPU is **5× faster**
- GPU timing includes `glFinish()` — pessimistic. Without forced sync, CPU-visible cost is just VBO upload (~0.1ms)
- CPU scales with pixel count. GPU scales with quad count. At higher DPI the gap widens.

---

## Principles and Rationale

### Why GPU-in-paint (not Mailbox/Snapshot)

The original proto (`de22c5c`) used Mailbox to cross from MESSAGE THREAD to GL THREAD. This was necessary when component painting and GL rendering were separate phases.

JUCE's actual architecture makes them the **same thing**: `paint()` IS GL rendering when a context is attached. The GL thread runs component painting with the FBO bound. Drawing instanced quads inside `pop()` is:
- Simpler (no Mailbox, no Snapshot, no double-buffer, no thread crossing)
- Faster (one fewer compositing pass — text is IN the FBO, not behind it)
- Orthogonal to shader::Controller (Shadertoy is untouched, unaware)

### Why this respects current architecture

- **No new GL context.** Uses JUCE's existing context (attached by shader::Controller via `gpu` config event).
- **No thread contract change.** `paint()` already runs on the GL thread when context is attached. No new cross-thread communication.
- **No lock.** No Mailbox means no atomic exchange. All rendering happens within a single `paint()` call.
- **shader::Controller untouched.** It continues doing Shadertoy in `renderOpenGL()`. Completely orthogonal.
- **CodeView untouched.** Still calls `push()` / `drawGlyphs()` / `pop()`. The dispatch is internal to `glyph::Graphics`.
- **CPU fallback preserved.** `gpu=false` → no OpenGL context → `pop()` takes the CPU SIMD path. Zero behavioral change for non-GL consumers.
- **All JAM consumers benefit.** Any component painting into a GL-backed `juce::Graphics` gets the optimization transparently.

---

## Scaffold

### Architecture

```
CodeView::paint(g)         ← g backed by JUCE's OpenGLGraphicsContext (FBO bound, GL current)
  └─ glyph::Graphics::push()
  └─ glyph::Graphics::drawGlyphs()    ← builds Quad array (unchanged)
  └─ glyph::Graphics::pop(g)
       ├─ [no GL context]: CPU SIMD blend + g.drawImageAt (today's path, optimized bg fill)
       └─ [GL context active]: save GL state → VBO upload → glDrawArraysInstanced → restore GL state
```

shader::Controller is unaware. CodeView is unaware. The optimization is encapsulated in `glyph::Graphics`.

### Change 1: CPU Background Fill Optimization (JAM)

**File:** `jam_graphics/fonts/font/glyph/jam_glyph_graphics_cells.cpp`

Replace scalar per-pixel background fill:
```cpp
// Before: O(cells × cellH × cellW) individual pixel stores
for (int dy = 0; dy < physCellHeight; ++dy)
{
    auto* row = ...;
    for (int dx = 0; dx < cellW; ++dx)
        row[bx + dx] = pixel;
}
```

With vectorized row fill:
```cpp
// After: O(cells × cellH) fill_n calls — compiler vectorizes to SIMD memset
for (int dy = 0; dy < physCellHeight; ++dy)
{
    auto* rowStart = reinterpret_cast<uint32_t*> (targetData.getLinePointer (cellY + dy)) + cellX;
    std::fill_n (rowStart, fillWidth, pixel);
}
```

Bounds check: `fillWidth = juce::jmin (cellW * span, physW - cellX)`. Clipping is already the existing behavior.

### Change 2: GPU Instanced Draw in pop() (JAM)

**File:** `jam_graphics/fonts/font/glyph/jam_glyph_graphics.h` / `.cpp`

Add to `glyph::Graphics`:

**Members (GL resources, lazy-initialized):**
```cpp
struct GLResources
{
    GLuint vao { 0 };
    GLuint quadVBO { 0 };
    GLuint instanceVBO { 0 };
    GLuint bgInstanceVBO { 0 };
    GLuint monoAtlasTexture { 0 };
    GLuint emojiAtlasTexture { 0 };
    std::unique_ptr<juce::OpenGLShaderProgram> monoShader;
    std::unique_ptr<juce::OpenGLShaderProgram> emojiShader;
    std::unique_ptr<juce::OpenGLShaderProgram> bgShader;
    bool ready { false };
};

std::unique_ptr<GLResources> gl;
```

**Detection in pop():**
```cpp
void glyph::Graphics::pop (juce::Graphics& g, int drawX, int drawY) noexcept
{
    if (juce::OpenGLContext::getCurrentContext() != nullptr)
        popGL (g, drawX, drawY);    // GPU path
    else
        popSoftware (g, drawX, drawY);  // CPU path (today's code)
}
```

**popGL() — instanced draw into JUCE FBO:**
```cpp
void glyph::Graphics::popGL (juce::Graphics&, int drawX, int drawY) noexcept
{
    if (not gl) initGL();
    if (not gl->ready) return;

    // Save JUCE ShaderContext GL state
    GLint prevProgram, prevVAO, prevArrayBuffer;
    GLboolean prevBlend;
    glGetIntegerv (GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
    glGetBooleanv (GL_BLEND, &prevBlend);

    GLint viewport[4];
    glGetIntegerv (GL_VIEWPORT, viewport);
    float vpW = static_cast<float> (viewport[2]);
    float vpH = static_cast<float> (viewport[3]);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray (gl->vao);

    // Draw backgrounds
    // ... glBufferData(backgrounds) + glDrawArraysInstanced ...

    // Draw mono glyphs
    // ... glBufferData(mono quads) + glDrawArraysInstanced ...

    // Draw emoji glyphs
    // ... glBufferData(emoji quads) + glDrawArraysInstanced ...

    glBindVertexArray (0);

    // Restore JUCE state
    glUseProgram (prevProgram);
    glBindVertexArray (prevVAO);
    glBindBuffer (GL_ARRAY_BUFFER, prevArrayBuffer);
    if (prevBlend) glEnable (GL_BLEND); else glDisable (GL_BLEND);
}
```

**Quad accumulation (already happens in drawGlyphs):**

The current `drawGlyphs()` already builds a `Render::Quad` per glyph — currently consumed immediately for CPU compositing. In GPU mode, instead of compositing each quad inline, accumulate them into the `SnapshotBase` arrays (mono, emoji, backgrounds). `popGL()` then uploads the accumulated arrays in one shot.

This means `drawGlyphs()` has two inner paths:
- CPU mode: build Quad → composite immediately (today)
- GPU mode: build Quad → append to SnapshotBase arrays (deferred)

The Quad construction is identical — same atlas lookup, same position calculation, same color. Only the consumption differs.

**initGL() — lazy one-time setup:**
```cpp
void glyph::Graphics::initGL() noexcept
{
    gl = std::make_unique<GLResources>();
    auto* ctx = juce::OpenGLContext::getCurrentContext();
    // Compile shaders (5 constexpr GLSL strings)
    // Create VAO + VBOs
    // Create atlas textures (R8 mono + RGBA8 emoji)
    gl->ready = true;
}
```

**Atlas GL texture sync:**

When `glyph::Atlas::writePixels()` writes a newly-rasterized glyph into the CPU atlas image, it also sets a dirty flag. On the next `popGL()`, dirty regions are uploaded to the GL atlas texture via `glTexSubImage2D` — incremental, only new glyphs, zero-cost after warmup.

Since `writePixels()` and `popGL()` both run on the same thread (MESSAGE/GL thread during paint), no mutex is needed. The atlas is written during `drawGlyphs()`, uploaded at the start of `popGL()`, before the draw calls.

### GLSL Shaders (5 constexpr strings)

Byte-compatible with `jam::glyph::Render::Quad` (verified: offset 0=screenPos, 8=glyphSize, 16=texCoords, 32=color):

- `glyph.vert` — pixel→NDC via uViewportSize, per-instance screenPosition/glyphSize/texCoords/color
- `glyph_mono.frag` — samples R8 atlas, alpha × instance color
- `glyph_emoji.frag` — samples RGBA8 atlas directly
- `background.vert` — pixel→NDC, per-instance position/size/color
- `background.frag` — passthrough color

### GL Resource Lifecycle

- **Created:** lazily on first `popGL()` call (GL context guaranteed current)
- **Destroyed:** when `glyph::Graphics` destructor runs, OR when GL context closes (need a context-closing notification — JUCE's `OpenGLContext` provides `removeExtensionFunctions` but no public teardown hook. Alternative: check `isAttached()` and release on next pop, or use a weak reference to the context.)
- **Shared:** one set of GL resources per `glyph::Graphics` instance. Each CodeView has its own `glyph::Graphics` — each gets its own VAO/VBOs. Shaders could be shared (static), but per-instance is simpler and the cost is negligible (compiled once, kept forever).

---

## BLESSED Compliance Checklist

- [x] **Bounds** — All GL calls happen within `paint()` on the GL thread. Same thread boundary as today. No new cross-thread surface.
- [x] **Lean** — Two changes: one line for bg fill optimization, one new code path in `pop()`. No new types, no new abstractions beyond `GLResources` member struct.
- [x] **Explicit** — GPU/CPU dispatch via `OpenGLContext::getCurrentContext() != nullptr`. No config flag needed — presence of GL context IS the signal.
- [x] **SSOT** — One Quad struct layout consumed by both paths. One atlas, one font pipeline. GPU and CPU build identical Quad data.
- [x] **Stateless** — Shaders are pure functions. GLResources are context-bound constants (compiled once). No mutable state between frames beyond the VBO upload (overwritten every frame).
- [x] **Encapsulation** — Entirely within `glyph::Graphics`. CodeView unaware. shader::Controller unaware. All JAM consumers get it transparently.
- [x] **Deterministic** — Same Quad data → same pixels. Detection is deterministic (`getCurrentContext()` is non-null iff GL is attached). No timing-dependent behavior.

---

## Resolved Decisions

1. **GL resource teardown** — Context closes = driver cleans up. When OpenGLContext detaches, the driver releases all GL objects created in that context. No explicit cleanup in `glyph::Graphics`. The context owns all objects. Lazy re-creation on next GL-mode `pop()` if context reattaches.

2. **Atlas texture size** — 4096×4096, matching the CPU atlas. R8 (16MB) + RGBA8 (64MB) = 80MB GPU. Same as original proto, same as current CPU allocation.

3. **Shader version** — `#version 410 core`. Matches shader::Controller's context requirement (GL 4.1). No 4.1 features used, but consistency with the context version avoids surprises.

4. **Text decorations (underline, strikethrough)** — Draw as thin Background quads in the instanced draw. Same draw call as cell backgrounds, zero extra cost. One code path for both CPU and GPU (Quad packing is shared, only consumption differs).

5. **Cursor** — No change. Cursor is drawn by CodeView after `pop()` returns. In GPU mode it paints into the same FBO — composites correctly on top of instanced-draw text. No action required.

---

## Handoff Notes

- **Benchmark app** at `~/Documents/Poems/dev/bench/` — standalone, not part of END. All three modes (CPU, GPU-renderOpenGL, GPU-in-paint) available for continued profiling.
- **shader::Controller is untouched** by this optimization. It continues to own the GL context and render Shadertoy. The glyph instanced draw is orthogonal — it happens inside JUCE's component paint phase, not in renderOpenGL().
- **No Mailbox, no Snapshot, no double-buffer.** The original proto's architecture is superseded. JUCE's native paint-on-GL-thread makes thread-crossing machinery unnecessary.
- **`glyph::Graphics` is JAM-wide.** The GPU optimization benefits any JAM consumer with an attached OpenGLContext (END, potentially TIT, CAKE). The CPU fallback is unchanged for non-GL consumers.
- **The "Open Seam — Font / Atlas GL-Thread Binding" in ARCHITECTURE.md is resolved:** font/atlas operations stay on MESSAGE THREAD (same thread as paint when GL attached). Atlas GL textures sync incrementally during `popGL()`. No cross-thread atlas access.
- The `kuassa::gl::Renderer` base class pattern is relevant for shader::Controller's own refactoring (separate concern) but NOT for the glyph optimization. glyph::Graphics doesn't inherit any GL renderer — it just issues GL calls when it detects a GL context during pop().
