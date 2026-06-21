# ORACLE — Rendering Optimization Research
**Date:** June 21, 2026
**Status:** Incomplete. Needs source verification from git history.

---

## OBJECTIVE

Look into past commits in `~/Documents/Poems/dev/end` to determine whether
a GPU instanced-draw rendering path using a lock-free snapshot mailbox was
previously implemented. If prior art exists, assess what can be salvaged and
fitted into END's current jam-based architecture.

This is a RESEARCH task. Read source first. Do not assume.

---

## WHAT WAS VERIFIED FROM SOURCE (ground truth)

### Current glyph pipeline (read from `~/Documents/Poems/dev/jam/`)

The glyph rendering pipeline composites on the CPU via SIMD, then blits
through `juce::Graphics`. When an OpenGL context is attached (END's default),
JUCE's internal `CachedImageList` re-uploads the entire render target as a GL
texture every frame via `glTexSubImage2D` because the pixel data changes every
frame.

**The inner loop builds a `glyph::Render::Quad` per glyph, uses it once for
CPU compositing, then discards it.** The Quad struct contains screen position,
glyph size, atlas UV coordinates, and foreground RGBA — it is GPU instance
buffer data that is currently thrown away.

Key files read and verified:
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics.h` — stateless compositor: push/drawGlyphs/pop
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics.cpp` — frame lifecycle, drawGlyphs overloads
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics_cells.cpp` — cell path with constraints, bg, decorations
- `jam_graphics/fonts/font/glyph/jam_glyph_render.h` — `Quad`, `Background`, `SnapshotBase` (all trivially copyable, static_assert'd)
- `jam_graphics/fonts/font/glyph/jam_simd_blend.h` — NEON + SSE2 + scalar: `blendSrcOver4`, `blendMonoTinted4`, `fillOpaque4`
- `jam_graphics/fonts/font/glyph/jam_glyph_atlas.h` — CPU atlas, owns Packer via unique_ptr
- `jam_graphics/fonts/font/glyph/jam_glyph_packer.h` — LRU cache + rasterize + shelf pack + writePixels
- `jam_graphics/fonts/font/glyph/jam_lru_glyph_cache.h` — HashMap LRU, frame-based 10% eviction
- `jam_graphics/fonts/font/glyph/jam_atlas_packer.h` — shelf-based 2D bin packer
- `jam_graphics/fonts/font/glyph/jam_glyph_key.h` — Atlas::Key (glyphIndex + fontFace + fontSize + span)
- `jam_graphics/fonts/font/glyph/jam_atlas_glyph.h` — Atlas::Region (UV rect + dims + bearings + type)
- `jam_graphics/fonts/font/glyph/jam_glyph_constraint.h` — Nerd Font icon constraint per codepoint
- `jam_graphics/fonts/font/glyph/jam_box_drawing.h` — procedural rasterizer for box/block/braille
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h` — layout engine: CodeModel → Entry[] → Run[]
- `jam_graphics/fonts/font/jam_font.h` — immutable value type, resolves cellW/cellH/baseline once
- `jam_graphics/fonts/typeface/jam_typeface.h` — platform font handles, HarfBuzz shaping, fallback
- `jam_gui/code_view/jam_code_view.cpp` — ContentView::paint: shape visible rows, push/drawGlyphs/pop

### Lock-free infrastructure (read from source)

**`jam::Mailbox<SnapshotType>`** (read from uploaded `jam_mailbox.h`):
Single atomic pointer exchange. `write()` returns old buffer. `read()` returns
latest-or-nullptr. `acq_rel` ordering. Zero blocking on either side.

**`jam::Model`** (read from uploaded `jam_model.cpp`):
APVTS pattern. Atomic parameter writes, CAS-gated flush, timer-driven ValueTree sync.

**`CellFifo`** (read from `~/Documents/Poems/dev/end/Source/terminal/CellFifo.h`):
Lock-free SPSC with seqlock epochs, drop-oldest, producer never stalls.

### JUCE OpenGL drawImage internals (read from JUCE source)

When `pop()` calls `g.drawImageAt()` through a JUCE OpenGL context:
- `CachedImageList::getTextureFor()` checks `textureNeedsReloading` flag
- Flag is set by `ImagePixelData::Listener` whenever pixel data changes
- Since the render target changes every frame → full `texture.loadImage()` → full `glTexSubImage2D` every frame
- This is the bottleneck: uploading the entire composited viewport image to the GPU that could have drawn the quads directly

### Ghostty comparison (read from `~/Documents/Poems/dev/ghostty/`)

Read and verified: `src/renderer/shaders/shaders.metal`, `src/font/Atlas.zig`,
`src/font/SharedGrid.zig`, `src/renderer/Thread.zig`, `src/renderer/cell.zig`,
`src/font/Glyph.zig`.

Ghostty uses GPU instanced draw (one draw call per atlas type). Instance data
is ~3MB at 8K. END uploads ~132MB of composited pixels at 8K. Ghostty uses
RwLock + BlockingQueue(64) on the hot path. END has zero locks.

10M sequence benchmark (from end commit message `bcef69c`):
`seq 1 10000000` — 6.41s user, 12.230 total. Side-by-side with Ghostty:
0–150ms variance, END sometimes faster.

### END's current GL pipeline (read from `~/Documents/Poems/dev/end/Source/shader/`)

`shader::Controller` implements `juce::OpenGLRenderer`. Currently used for
Shadertoy post-processing shaders only. Owns a fullscreen Quad (VAO+VBO),
shader program management, FBO pairs for multi-pass. Attached/detached via
`gpu` config event.

---

## WHAT WAS NOT VERIFIED (do not treat as fact)

- The contents of the old `GLAtlasRenderer` files — never read, only saw filenames in git log
- Whether the old code used Mailbox, instanced draw, or any specific rendering model
- Whether the old code is compatible with the current jam architecture
- Any specific git commit hash as "the right one" — the git log was provided by ARCHITECT but file contents were never read
- How the old GL renderer interacted with the atlas, font pipeline, or component lifecycle

---

## TASK FOR NEXT ORACLE SESSION

1. **Read the git history** of `~/Documents/Poems/dev/end` — you have shell access, use `git log` and `git show` to find and read the actual GL rendering source code

2. **Search for**: any implementation that builds Quad/instance data on the message thread and delivers it to a GL thread for instanced draw — whether via Mailbox, atomic exchange, or any other lock-free mechanism

3. **Assess what exists**: read the actual code, understand what it did, what worked, what was removed and why

4. **Determine what's reusable**: given that jam's `glyph::Render::Quad` and `SnapshotBase` already exist and are trivially copyable, and `jam::Mailbox` already exists, can the old GL draw path be adapted to consume SnapshotBase data?

5. **Respect BLESSED**: any proposed optimization must maintain zero locks on the hot path, preserve the CPU fallback (gpu=false), and not duplicate the rendering codepath

6. **Do not assume** — if you can't find the code or it doesn't exist, say so

---

## REFERENCE: Key data structures (verified)

`glyph::Render::Quad` — 7 floats: screenPosition(2), glyphSize(2), textureCoordinates(4 as Rectangle), foregroundRGBA(4). Trivially copyable.

`glyph::Render::Background` — screenBounds(4 as Rectangle) + backgroundRGBA(4). Trivially copyable.

`glyph::Render::SnapshotBase` — HeapBlock<Quad> mono/emoji + HeapBlock<Background> backgrounds + counts/capacities. Grow-only via ensureCapacity().

`jam::Mailbox<T>` — single `std::atomic<T*>` slot, `acq_rel` exchange both directions.
