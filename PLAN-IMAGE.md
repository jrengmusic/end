# PLAN: Inline Image Rendering

**RFC:** RFC-IMAGE.md
**Date:** 2026-04-09
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (LANGUAGE.md)

## Overview

Add Sixel (DCS) and iTerm2 (OSC 1337) inline image rendering to END's terminal. Images are decoded to RGBA, staged into a shelf-packed atlas, stored as cell references in Grid, and rendered as instanced quads alongside glyph rendering. No new shaders. No module boundary changes.

## Language / Framework Constraints

C++ / JUCE is the reference implementation. All BLESSED principles enforced as written. JUCE-specific: `juce::Image` with `SoftwareImageType` for CPU atlas mirror, `HeapBlock` for contiguous buffers, `CriticalSection` for staging queue.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Cell Flags + ImageCell Struct

**Scope:** `Source/terminal/data/Cell.h`

**Action:**
- Add `static constexpr uint8_t LAYOUT_IMAGE { 0x10 };` and `LAYOUT_IMAGE_CONT { 0x02 };`
- Add `bool isImage() const noexcept` and `bool isImageContinuation() const noexcept`
- Add `struct ImageCell { uint16_t offsetX; uint16_t offsetY; };` with `static_assert(sizeof(ImageCell) == 4)` and `static_assert(trivially_copyable)`
- Zero changes to Cell struct layout. `static_assert(sizeof(Cell) == 16)` must still hold.

**Validation:**
- `sizeof(Cell) == 16` assertion compiles
- `sizeof(ImageCell) == 4` assertion compiles
- No existing Cell usage affected (grep `LAYOUT_` confirms no collision with 0x10 or 0x02)
- NAMES: `ImageCell` is a noun, `isImage`/`isImageContinuation` are boolean accessors (Rule 1)

---

### Step 2: Grid::Buffer imageCells HeapBlock

**Scope:** `Source/terminal/logic/Grid.h`, `Source/terminal/logic/Grid.cpp`

**Action:**
- Add `juce::HeapBlock<ImageCell> imageCells;` to `Grid::Buffer` (co-indexed with cells)
- Allocate in `initBuffer()`: `imageCells.allocate(totalRows * cols, true);`
- Add public API mirroring grapheme pattern exactly:
  - `void activeWriteImageCell (int row, int col, const ImageCell& ic) noexcept;`
  - `const ImageCell* activeVisibleImageRow (int row) const noexcept;`
  - `const ImageCell* scrollbackImageRow (int visibleRow, int scrollOffset) const noexcept;`
- Mirror `graphemes` handling in `reflow()`, `clearBuffer()`, and ring-buffer copy paths
- Update `getStateInformation` / `setStateInformation`: memcpy `imageCells` per buffer (totalRows x cols x sizeof(ImageCell)), both normal and alternate screens

**Validation:**
- Every `graphemes` operation in Grid has a corresponding `imageCells` operation
- Serialization round-trip: `getStateInformation` → `setStateInformation` produces identical imageCells
- No change to existing Grid behavior (imageCells all zero-initialized, unused until decoder writes them)
- BLESSED-B: imageCells HeapBlock has same lifetime as Buffer (co-allocated, co-destroyed)
- BLESSED-S (SSOT): single allocation site, single physical-row math

---

### Step 3: ImageAtlas

**Scope:** `Source/terminal/rendering/ImageAtlas.h` (new), `Source/terminal/rendering/ImageAtlas.cpp` (new), `Source/terminal/rendering/Screen.h` (Resources)

**Action:**
- Create `Terminal::ImageAtlas` class:
  - `uint32_t stage (const uint8_t* rgba, int widthPx, int heightPx) noexcept;` — READER THREAD, copies to staging queue, returns imageId > 0
  - `void consumeStagedUploads() noexcept;` — MESSAGE THREAD, drains queue into atlas
  - `const ImageRegion* lookup (uint32_t imageId) const noexcept;` — pure lookup
  - `void release (uint32_t imageId) noexcept;` — marks for LRU eviction
  - `void evictIfNeeded (int budgetBytes) noexcept;` — evicts LRU entries
  - `const juce::Image& getCPUAtlas() const noexcept;` — CPU-side mirror
- Create `Terminal::ImageRegion` struct: `juce::Rectangle<float> uv`, `int widthPx`, `int heightPx`, `GLuint glTextureHandle`
- Internal: `jreng::Glyph::AtlasPacker` (shelf-packed 4096x4096), `GLuint glTexture`, `juce::Image cpuAtlas` (ARGB SoftwareImageType), LRU map (imageId -> ImageRegion + frame counter), staging queue with CriticalSection
- Add `ImageAtlas` to `Screen::Resources`
- Call `consumeStagedUploads()` from `renderOpenGL` / `renderPaint` before snapshot packing
- Add `end.lua` config key: `image.atlas_budget` (default 32MB)

**Validation:**
- Follows `AtlasPacker` / `consumeStagedBitmaps` pattern exactly
- Thread safety: `stage()` takes lock, `consumeStagedUploads()` takes same lock, no concurrent mutation
- BLESSED-B: atlas owns GL texture + CPU image, destroys both in destructor
- BLESSED-E: `lookup()` returns nullptr for evicted/unknown IDs, never crashes
- BLESSED-S (Stateless): `lookup` is pure, `stage` is append-only
- NAMES: `ImageAtlas`, `ImageRegion`, `stage`, `lookup`, `release` — nouns for things, verbs for actions

---

### Step 4: ImageQuad + Snapshot + Per-Row Cache

**Scope:** `Source/terminal/rendering/Screen.h`, `Source/terminal/rendering/ScreenSnapshot.cpp`

**Action:**
- Add `struct Render::ImageQuad { juce::Rectangle<float> screenBounds; juce::Rectangle<float> uvRect; GLuint glTexture; };` — trivially copyable
- Add to `Render::Snapshot`: `juce::HeapBlock<ImageQuad> images;`, `int imageCount { 0 };`, `int imageCapacity { 0 };`
- Add per-row image cache to Screen private members: `juce::HeapBlock<ImageQuad> cachedImages;`, `juce::HeapBlock<int> imageCacheCount;`, `int maxImagesPerRow { 0 };`
- Allocate in `allocateRenderCache(rows, cols)`: `maxImagesPerRow = cols;`
- Update `updateSnapshot`: pack `cachedImages` into `snapshot.images` (same memcpy pattern as mono/emoji/bg)

**Validation:**
- `static_assert(trivially_copyable<ImageQuad>)`
- Cache allocation mirrors cachedMono/cachedEmoji/cachedBg pattern exactly
- Snapshot packing mirrors existing mono/emoji/bg packing
- BLESSED-L: no new abstraction, extends existing Render namespace types

---

### Step 5: processCellForSnapshot Image Branch

**Scope:** `Source/terminal/rendering/ScreenRender.cpp`

**Action:**
- Add `const ImageCell* rowImageCells` parameter to `processCellForSnapshot`
- Insert image branch at the top of the function, BEFORE hint label block:
  - `if (cell.isImageContinuation())` → emit background quad only
  - `else if (cell.isImage())` → lookup atlas, emit ImageQuad
  - `else` → all existing text/hint/selection/underlay paths
- Positive nesting. No early returns.
- Update `buildSnapshot` caller: pass `activeVisibleImageRow(r)` / `scrollbackImageRow(r, offset)`. Reset `imageCacheCount[r]` per dirty row.

**Validation:**
- No early returns (JRENG)
- Image CONT cells with non-default bg still emit background quads
- Image head cells bypass colour resolution, shaping, selection (per RFC)
- `hasContent()` unaffected — head cells have imageId > 0 (true), CONT cells have codepoint 0 (false, but handled before hasContent check)
- Existing text rendering completely unchanged in the `else` branch
- BLESSED-E: `LAYOUT_IMAGE` / `LAYOUT_IMAGE_CONT` flags make dispatch explicit

---

### Step 6: drawImages — GL + CPU Backends

**Scope:** `Source/terminal/rendering/ScreenGL.cpp`

**Action:**
- Add `drawImages` to GL backend: bind image atlas GL texture, instanced draw reusing emoji shader (RGBA8 + UV sampling). Single `glBindTexture` + single instanced draw call.
- Add `drawImages` to CPU backend (`GraphicsContext`): lock `getCPUAtlas()` BitmapData once, blit each `uvRect` sub-region to `screenBounds` via SIMD compositing (same path as `compositeEmojiGlyph`). One BitmapData lock per frame.
- Call `drawImages` in both `renderOpenGL` and `renderPaint` after `drawQuads(emoji)`, before `drawCursor`.

**Validation:**
- Draw call count: background + mono + emoji + images + cursor = 5 max (within SPEC budget of 3-4 + images)
- GL: no new shader, no new vertex format — reuses emoji instanced pipeline
- CPU: no new compositing path — reuses existing SIMD emoji blit
- BLESSED-L: one method per backend, no helpers
- Performance: measure frame time delta with 0 images vs 10 images

---

### Step 7: Sixel Decoder

**Scope:** `Source/terminal/logic/SixelDecoder.h` (new), `Source/terminal/logic/SixelDecoder.cpp` (new), `Source/terminal/logic/Parser.cpp` (DCS dispatch)

**Action:**
- Create `Terminal::SixelDecoder`:
  - `decode (const uint8_t* data, size_t length)` → RGBA pixel buffer + width + height
  - Parse DCS `Pq...ST`: intro params (aspect ratio, background), colour registers (`#` HLS/RGB), sixel data (`!` repeat, `$` CR, `-` NL)
  - Palette: up to 256 entries
  - Output: contiguous RGBA8 buffer
- Integrate with Parser DCS dispatch: final byte `q` → collect payload, call decode
- After decode: `imageAtlas.stage(rgba, w, h)` → imageId. Compute cell span (ceil(width/physCellWidth), ceil(height/physCellHeight)). Write image cells to Grid via `activeWriteCell` + `activeWriteImageCell`.
- Edge cases: clip if wider than terminal cols, scroll if taller than visible rows

**Validation:**
- Test with `libsixel` output (`img2sixel` piped to END)
- Images render in correct cell positions
- Images scroll with content in scrollback
- Images survive grid resize (reflow handles imageCells)
- BLESSED-E (Encapsulation): decoder writes Grid + stages atlas. Never touches renderer.
- NAMES: `SixelDecoder` — noun (Rule 1), `decode` — verb

---

### Step 8: iTerm2 Decoder (OSC 1337)

**Scope:** `Source/terminal/logic/ITerm2Decoder.h` (new), `Source/terminal/logic/ITerm2Decoder.cpp` (new), `Source/terminal/logic/ParserESC.cpp` (OSC dispatch)

**Action:**
- Create `Terminal::ITerm2Decoder`:
  - `decode (const uint8_t* data, size_t length)` → RGBA pixel buffer + width + height
  - Parse `\x1b]1337;File=<params>:<base64>\x07`: extract `name`, `size`, `width`, `height`, `preserveAspectRatio`, `inline`
  - Base64 decode → `juce::Image::createFromMemory` (handles PNG/JPEG/GIF) → RGBA8
  - Scale to requested cell dimensions if width/height params present
- Integrate with Parser OSC dispatch: command 1337 → collect payload, call decode
- Same cell-writing path as Sixel: stage → compute span → write cells

**Validation:**
- Test with `imgcat` or manual `printf` escape sequence
- PNG and JPEG images render correctly
- Shares ImageAtlas + cell-writing path with Sixel (SSOT)
- BLESSED-E (Encapsulation): decoder never touches renderer
- NAMES: `ITerm2Decoder` — noun

---

### Step 9: Polish + Edge Cases

**Scope:** Cross-cutting

**Action:**
- LRU eviction: verify atlas doesn't grow unbounded under sustained image output
- Scrollback: verify images scroll correctly, atlas releases when cells scroll past capacity
- Resize: verify image cells reflow with grid (imageCells in `reflow()`)
- Selection: verify selection overlay renders on top of image cells
- Nexus snapshot: verify `getStateInformation` / `setStateInformation` round-trips images correctly (imageCells serialized, imageId references valid after restore via atlas re-staging)
- Performance: measure frame time with 10+ visible images. Target: 60fps.
- Config: verify `image.atlas_budget` respects configured value

**Validation:**
- Full @Auditor sweep against MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md
- Performance targets from SPEC.md (inline images don't drop below 60fps)
- No regression in existing terminal rendering (text, emoji, box drawing, selection, hints)

---

## Dependency Chain

```
Step 1 (Cell flags) ─► Step 2 (Grid imageCells) ─► Step 3 (ImageAtlas)
                                                  ─► Step 4 (Snapshot/Cache)
                                                       ─► Step 5 (processCellForSnapshot)
                                                            ─► Step 6 (drawImages)
                                                                 ─► Step 7 (Sixel)
                                                                 ─► Step 8 (iTerm2)
                                                                      ─► Step 9 (Polish)
```

Steps 7 and 8 are independent — can execute in parallel.

## BLESSED Alignment

- **B (Bound):** imageCells HeapBlock co-allocated with cells in Buffer. ImageAtlas owns GL texture + CPU image, destroys both. No dangling references.
- **L (Lean):** No new abstraction layers. Side-table mirrors Grapheme. Emoji shader reused. SnapshotBase untouched.
- **E (Explicit):** LAYOUT_IMAGE / LAYOUT_IMAGE_CONT flags make every codepoint→imageId reinterpretation visible. Named types (ImageCell, ImageRegion, ImageQuad). No magic values.
- **S (SSOT):** imageId in cell.codepoint. Pixel offset in imageCells. ImageAtlas sole texture registry. One place each.
- **S (Stateless):** lookup is pure. Decoders are stateless per image. processCellForSnapshot image branch is stateless given row pointers and atlas.
- **E (Encapsulation):** Decoder never sees renderer. Renderer never sees decoder. ImageAtlas boundary: stage/consume/lookup/release. SnapshotBase not modified.
- **D (Deterministic):** Same imageId + ImageCell offset + cell geometry → same ImageQuad. No hidden state.

## Risks / Open Questions

- **Sixel aspect ratio:** DCS intro params specify pixel aspect ratio. Need to confirm whether to honour it or ignore (most modern terminals ignore). ARCHITECT to decide at Step 7.
- **Large image decode time:** Sixel images can be megabytes. Decoding on reader thread may block byte processing. If this becomes an issue, consider async decode with a staging buffer. Monitor in Step 9.
- **Nexus image restore:** After snapshot restore, imageIds in cells reference images that don't exist in the new client's atlas. Need a re-staging mechanism or accept blank images until the app redraws. ARCHITECT to decide at Step 9.
