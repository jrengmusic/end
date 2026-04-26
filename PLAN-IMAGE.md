## Handoff: SKiT Image Protocol (Sixel + Kitty + iTerm2)

**From:** COUNSELOR
**Date:** 2026-04-26
**Status:** Ready for Implementation

### Key Decisions
- **Terminal::Session owns Processor** — creates PTY + Processor, wires all 6 callbacks internally. Callers get Processor via getProcessor(). ARCHITECT decided.
- **Grid+State snapshot replaces raw byte history** — daemon Processor is real (processes all bytes), serializes Grid+State on reattach. No Loader thread. ARCHITECT decided after tmux research showed virtual-grid-redraw pattern.
- **Popup is standalone** — creates Terminal::Session directly. No Nexus involvement. ARCHITECT: "Popup has NOTHING TO DO WITH NEXUS."
- **Terminal layer Nexus-free** — Input, Mouse, Display route through Processor callbacks (writeInput, onResize). No Nexus::Session dependency in Terminal namespace.
- **Image rendering design** — side-table (ImageCell) mirroring Grapheme pattern. Single shelf-packed RGBA8 atlas. Emoji shader reused for drawImages. Decoders are pure Grid writers + staging producers.
- **SKiT — three protocols** — Sixel (DCS q), Kitty (APC G), iTerm2 (OSC 1337). ARCHITECT decided: all three, core Kitty first, extended deferred.
- **Lock-free atomic batch handoff** — Packer std::mutex replaced with Mailbox<StagedBatch> pattern. No mutex anywhere. ARCHITECT decided.
- **Hybrid lazy buffers** — DCS/APC/OSC accumulation uses Parser-lifetime HeapBlock, lazy allocation on first image sequence, geometric growth. BLESSED-B compliant. ARCHITECT decided.
- **ImageAtlas same B as GlyphAtlas** — owned by MainComponent, passed by reference down Tabs->Panes->Display->Screen. MESSAGE->GL channel (same as glyphs). READER THREAD decoders produce raw RGBA into transfer buffer, MESSAGE THREAD stages to atlas. Same topology, same Mailbox pattern. ARCHITECT decided.
- **Kitty core scope** — transmit (direct base64, t=d), display (a=T/a=p), delete (a=d), query (a=q), chunked (m=0/m=1), compression (o=z). Extended (shm, temp file, animation, unicode placeholders, z-indexing, relative placements) deferred.

### Open Questions
1. **Sixel aspect ratio** — honour DCS intro params or ignore? Most modern terminals ignore. ARCHITECT decides at Step 8.
2. **Large Sixel decode time** — if reader thread blocks too long, consider async decode. Monitor at Step 11.
3. **Nexus image restore** — after snapshot restore, imageIds reference images not in new client's atlas. Re-stage mechanism or accept blank until app redraws. ARCHITECT decides at Step 11.

---

# PLAN: SKiT Image Protocol

**RFC:** RFC-IMAGE.md
**Date:** 2026-04-26
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (LANGUAGE.md)

## Overview

Add Sixel (DCS), Kitty (APC), and iTerm2 (OSC 1337) inline image rendering to END's terminal. Images are decoded to RGBA, staged into a shelf-packed atlas via lock-free atomic batch handoff, stored as cell references in Grid, and rendered as instanced quads alongside glyph rendering. No new shaders. No new concurrency primitives. No mutex.

## Language / Framework Constraints

C++ / JUCE is the reference implementation. All BLESSED principles enforced as written. JUCE provides: image decoding (PNG/JPEG/GIF via `ImageFileFormat::loadFrom`), base64 (`MemoryBlock::fromBase64Encoding`), zlib (`GZIPDecompressorInputStream`), pixel manipulation (`Image::BitmapData`, `Image::convertedToFormat`, `Image::rescaled`), memory (`HeapBlock`, `MemoryBlock`, `MemoryOutputStream`). jam provides: atlas packing (`jam::Glyph::AtlasPacker`), staged upload pattern (`StagedBitmap`), GL texture creation (`GLContext::createAtlasTexture`), CPU atlas mirror (`GraphicsAtlas` pattern), lock-free mailbox (`jam::Mailbox`).

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 0: Packer Lock-Free Migration

**Scope:** `jam_graphics/fonts/jam_glyph_packer.h`, `jam_graphics/fonts/jam_glyph_packer.cpp`

**Action:**
- Replace `std::mutex uploadMutex` + `HeapBlock<StagedBitmap> uploadQueue` + `uploadCount` + `uploadCapacity` with:
  - `jam::Mailbox<StagedBatch> uploadMailbox;`
  - `std::array<StagedBatch, 2> uploadBatches;`
  - `int writeSlot { 0 };`
- `StagedBatch` struct: `HeapBlock<StagedBitmap> items`, `int count`, `int capacity`. Methods: `append(StagedBitmap&&)`, `absorb(StagedBatch&)`, `reset()`. Geometric growth (min 16, then x2).
- `stageForUpload()` — MESSAGE THREAD, no lock: appends to `uploadBatches[writeSlot]`
- Add `publishStagedBitmaps()` — MESSAGE THREAD: atomically publishes write batch via `uploadMailbox.write()`, flips writeSlot. On returned non-null pointer: absorb returned batch into new write slot (prevents data loss if GL behind)
- `consumeStagedBitmaps()` — GL THREAD, no lock: returns `uploadMailbox.read()` (nullptr if nothing new). Caller resets batch after processing.
- Remove `hasStagedBitmaps()` — dead code (zero call sites confirmed by Pathfinder)
- Fix `clear()` — currently zeroes `uploadCount` without lock (latent race). Lock-free version: `uploadBatches[writeSlot].reset()` — safe because MESSAGE THREAD owns write slot exclusively

**Publish call site in END:**
- `ScreenSnapshot.cpp` — call `packer.publishStagedBitmaps()` after all `getOrRasterize` / `getOrRasterizeBoxDrawing` calls complete, before `snapshotBuffer.write()`

**Validation:**
- Zero `std::mutex` / `std::lock_guard` remaining in Packer
- Existing glyph rendering visually identical (regression test)
- No data loss under rapid text scrolling (GL thread falling behind producer)
- BLESSED-B: StagedBatch owns HeapBlock, clear lifetime via double-buffer swap
- BLESSED-L: reuses existing Mailbox primitive, no new concurrency types
- BLESSED-S (SSOT): one publish point per frame, one consume point per frame

---

### Step 1: Parser Hybrid Buffers

**Scope:** `Source/terminal/logic/Parser.h`, `Source/terminal/logic/ParserESC.cpp`

**Action:**
- Add to Parser private members:
  - `juce::HeapBlock<uint8_t> dcsBuffer;` — DCS payload accumulation (Sixel)
  - `int dcsBufferSize { 0 };`
  - `int dcsBufferCapacity { 0 };`
  - `juce::HeapBlock<uint8_t> apcBuffer;` — APC payload accumulation (Kitty)
  - `int apcBufferSize { 0 };`
  - `int apcBufferCapacity { 0 };`
- Replace fixed `uint8_t oscBuffer[OSC_BUFFER_CAPACITY]` with:
  - `juce::HeapBlock<uint8_t> oscBuffer;`
  - `int oscBufferSize { 0 };`
  - `int oscBufferCapacity { 0 };`
  - Preserve `OSC_BUFFER_CAPACITY { 512 }` as initial allocation size for non-image OSC sequences
- Add private `appendToBuffer(HeapBlock<uint8_t>&, int&, int&, uint8_t)` — geometric growth (min 4096, then x2). Single helper for all three buffers.
- Wire `dcsPut()`: call `appendToBuffer(dcsBuffer, ...)` instead of discarding
- Wire `dcsHook()`: record `finalByte` for dispatch in `dcsUnhook()`. If `finalByte == 'q'`: lazy-allocate dcsBuffer (initial 64KB for typical Sixel)
- Wire `dcsUnhook()`: dispatch accumulated dcsBuffer to SixelDecoder (Step 8)
- Add APC state machine hooks (mirroring DCS pattern):
  - `apcStart()` — first byte `'G'` → lazy-allocate apcBuffer (initial 8KB for typical Kitty chunk)
  - `apcPut(uint8_t)` — `appendToBuffer(apcBuffer, ...)`
  - `apcEnd()` — dispatch accumulated apcBuffer to KittyDecoder (Step 10)
- Wire APC hooks in Parser VT state machine: `ESC _` enters APC state, `ESC \` terminates
- Wire OSC 1337 to use dynamic oscBuffer: on `oscDispatch()` command 1337, the existing oscBuffer contains the full payload for iTerm2Decoder (Step 9)

**Validation:**
- All three buffers: lazy allocation (zero cost when unused), geometric growth, Parser-lifetime ownership
- Existing OSC sequences (titles, clipboard, hyperlinks) still work identically
- DCS/APC accumulation tested with synthetic payloads
- BLESSED-B: buffers owned by Parser, destroyed with Parser
- BLESSED-L: one `appendToBuffer` helper, no duplication across DCS/APC/OSC
- BLESSED-E: explicit lazy allocation on sequence-type detection
- No existing Parser behavior changed for non-image sequences

---

### Step 2: Cell Flags + ImageCell Struct

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

### Step 3: Grid::Buffer imageCells HeapBlock

**Scope:** `Source/terminal/logic/Grid.h`, `Source/terminal/logic/Grid.cpp`

**Action:**
- Add `juce::HeapBlock<ImageCell> imageCells;` to `Grid::Buffer` (co-indexed with cells)
- Allocate in `initBuffer()`: `imageCells.allocate(totalRows * cols, true);`
- Add public API mirroring grapheme pattern exactly:
  - `void activeWriteImageCell (int row, int col, const ImageCell& ic) noexcept;`
  - `const ImageCell* activeVisibleImageRow (int row) const noexcept;`
  - `const ImageCell* scrollbackImageRow (int visibleRow, int scrollOffset) const noexcept;`
- Add `void activeWriteImage (uint32_t imageId, int widthPx, int heightPx, int cellWidthPx, int cellHeightPx) noexcept;` — shared cell-writing path for all 3 decoders. Computes cell span, writes LAYOUT_IMAGE head + LAYOUT_IMAGE_CONT continuations, writes ImageCell offsets. Lives on Grid::Writer (READER THREAD, consistent with activeWriteCell/activeWriteGrapheme).
- Mirror `graphemes` handling in `reflow()`, `clearBuffer()`, and ring-buffer copy paths
- Update `getStateInformation` / `setStateInformation`: memcpy `imageCells` per buffer (totalRows x cols x sizeof(ImageCell)), both normal and alternate screens

**Validation:**
- Every `graphemes` operation in Grid has a corresponding `imageCells` operation
- Serialization round-trip: `getStateInformation` -> `setStateInformation` produces identical imageCells
- No change to existing Grid behavior (imageCells all zero-initialized, unused until decoder writes them)
- BLESSED-B: imageCells HeapBlock has same lifetime as Buffer (co-allocated, co-destroyed)
- BLESSED-S (SSOT): single allocation site, single physical-row math

---

### Step 4: ImageAtlas

**Scope:** `Source/terminal/rendering/ImageAtlas.h` (new), `Source/terminal/rendering/ImageAtlas.cpp` (new), `Source/MainComponent.h` (ownership), `Source/terminal/rendering/Screen.h` (reference)

**Action:**
- Create `Terminal::ImageAtlas` class — same B as GlyphAtlas:
  - Owned by value in `MainComponent` (alongside `packer`, `glyphAtlas`, `graphicsAtlas`)
  - Passed by reference: MainComponent -> Tabs -> Panes -> Display -> Screen
  - `uint32_t stage (const uint8_t* rgba, int widthPx, int heightPx) noexcept;` — MESSAGE THREAD, packs into atlas, appends to write batch, returns imageId > 0
  - `void publishStagedUploads() noexcept;` — MESSAGE THREAD, publishes batch via Mailbox (called alongside `packer.publishStagedBitmaps()` before `snapshotBuffer.write()`)
  - `StagedBatch* consumeStagedUploads() noexcept;` — GL THREAD, returns batch from Mailbox (nullptr if nothing new)
  - `const ImageRegion* lookup (uint32_t imageId) const noexcept;` — pure lookup
  - `void release (uint32_t imageId) noexcept;` — marks for LRU eviction
  - `void evictIfNeeded (int budgetBytes) noexcept;` — evicts LRU entries
  - `const juce::Image& getCPUAtlas() const noexcept;` — CPU-side mirror
- Create `Terminal::ImageRegion` struct: `juce::Rectangle<float> uv`, `int widthPx`, `int heightPx`, `GLuint glTextureHandle`
- Internal:
  - `jam::Glyph::AtlasPacker` (shelf-packed 4096x4096) — MESSAGE THREAD only, same thread safety as glyph AtlasPacker
  - `GLuint glTexture` + `juce::Image cpuAtlas` (ARGB SoftwareImageType)
  - LRU map (imageId -> ImageRegion + frame counter)
  - `jam::Mailbox<StagedBatch> uploadMailbox` — MESSAGE->GL, same topology as glyph Mailbox
  - `std::array<StagedBatch, 2> uploadBatches` + `int writeSlot { 0 }`
- `StagedBatch` for images: same struct as glyph StagedBatch (Step 0), with `StagedBitmap::Type::image`
- READER->MESSAGE transfer: decoders produce raw RGBA + dimensions into a transfer buffer (atomic handoff via Mailbox or SnapshotBuffer). MESSAGE THREAD consumes in `buildSnapshot`, decodes if needed, calls `stage()`.
- GL THREAD: consume + upload in `renderOpenGL` / `renderPaint` before snapshot read
- Add `end.lua` config key: `image.atlas_budget` (default 32MB)

**Validation:**
- Same B as GlyphAtlas: MainComponent owns, reference flows down, no ownership transfer
- Same channel topology: MESSAGE->GL via Mailbox, same as glyph pipeline
- Lock-free: Mailbox atomic exchange only, zero mutex
- Same StagedBatch/Mailbox pattern proven in Step 0 (glyph pipeline)
- BLESSED-B: atlas owns GL texture + CPU image, destroys both in destructor. MainComponent lifetime.
- BLESSED-E: `lookup()` returns nullptr for evicted/unknown IDs, never crashes
- BLESSED-S (Stateless): `lookup` is pure, `stage` is append-only to write batch
- BLESSED-S (SSOT): AtlasPacker MESSAGE THREAD only — same thread safety guarantee as glyph packer
- NAMES: `ImageAtlas`, `ImageRegion`, `stage`, `lookup`, `release` — nouns for things, verbs for actions

---

### Step 5: ImageQuad + Snapshot + Per-Row Cache

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

### Step 6: processCellForSnapshot Image Branch

**Scope:** `Source/terminal/rendering/ScreenRender.cpp`

**Action:**
- Add `const ImageCell* rowImageCells` parameter to `processCellForSnapshot`
- Insert image branch at the top of the function, BEFORE hint label block:
  - `if (cell.isImageContinuation())` -> emit background quad only
  - `else if (cell.isImage())` -> lookup atlas, emit ImageQuad
  - `else` -> all existing text/hint/selection/underlay paths
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

### Step 7: drawImages — GL + CPU Backends

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

### Step 8: Sixel Decoder

**Scope:** `Source/terminal/logic/SixelDecoder.h` (new), `Source/terminal/logic/SixelDecoder.cpp` (new)

**Action:**
- Create `Terminal::SixelDecoder`:
  - `decode (const uint8_t* data, size_t length)` -> RGBA pixel buffer + width + height
  - Parse DCS `Pq...ST`: intro params (aspect ratio, background), colour registers (`#` HLS/RGB), sixel data (`!` repeat, `$` CR, `-` NL)
  - Palette: up to 256 entries
  - Output: contiguous RGBA8 buffer
- READER THREAD: Parser `dcsUnhook()` calls `decode()`, produces raw RGBA + dimensions. Calls `Grid::Writer::activeWriteImage()` to write cell span (LAYOUT_IMAGE head + LAYOUT_IMAGE_CONT continuations + ImageCell offsets). Writes decoded RGBA to atomic transfer buffer (same pattern as Grid dirty rows / State parameterMap — READER writes data + sets atomic flag, MESSAGE consumes).
- MESSAGE THREAD: during `buildSnapshot`, checks atomic flag, consumes decoded RGBA, calls `imageAtlas.stage(rgba, w, h)` -> imageId. Atlas packing + staging happens here.
- Edge cases: clip if wider than terminal cols, scroll if taller than visible rows

**Validation:**
- Test with `libsixel` output (`img2sixel` piped to END)
- Images render in correct cell positions
- Images scroll with content in scrollback
- Images survive grid resize (reflow handles imageCells)
- BLESSED-B: same thread topology as glyph pipeline (READER produces data, MESSAGE stages, GL uploads)
- BLESSED-E (Encapsulation): decoder produces RGBA. Never touches atlas or renderer.
- NAMES: `SixelDecoder` — noun (Rule 1), `decode` — verb

---

### Step 9: iTerm2 Decoder (OSC 1337)

**Scope:** `Source/terminal/logic/ITerm2Decoder.h` (new), `Source/terminal/logic/ITerm2Decoder.cpp` (new)

**Action:**
- Create `Terminal::ITerm2Decoder`:
  - `decode (const uint8_t* data, size_t length)` -> RGBA pixel buffer + width + height
  - Parse `\x1b]1337;File=<params>:<base64>\x07`: extract `name`, `size`, `width`, `height`, `preserveAspectRatio`, `inline`
  - `juce::MemoryBlock::fromBase64Encoding` for base64 decode (established pattern from `handleOscClipboard`)
  - `juce::ImageFileFormat::loadFrom(data, size)` for PNG/JPEG/GIF decode
  - `juce::Image::convertedToFormat(Image::ARGB)` for uniform RGBA8
  - `juce::Image::rescaled()` if width/height params present
- READER THREAD: Parser `oscDispatch()` case 1337 calls `decode()` using dynamic oscBuffer. Calls `Grid::Writer::activeWriteImage()` for cell span. Writes decoded RGBA to atomic transfer buffer.
- MESSAGE THREAD: consumes atomic transfer buffer, calls `imageAtlas.stage()`. Same path as Sixel.

**Validation:**
- Test with `imgcat` or manual `printf` escape sequence
- PNG and JPEG images render correctly
- Shares ImageAtlas + cell-writing path with Sixel (SSOT)
- BLESSED-B: same READER->MESSAGE->GL topology
- BLESSED-E (Encapsulation): decoder produces RGBA. Never touches atlas or renderer.
- NAMES: `ITerm2Decoder` — noun
- JUCE does the heavy lifting: base64, image decode, format conversion, scaling

---

### Step 10: Kitty Decoder — Core

**Scope:** `Source/terminal/logic/KittyDecoder.h` (new), `Source/terminal/logic/KittyDecoder.cpp` (new)

**Action:**
- Create `Terminal::KittyDecoder`:
  - Parse APC `G` command: comma-separated `key=value` pairs before `;`, base64 payload after `;`
  - Key parsing: `a` (action), `f` (format), `t` (transmission), `s`/`v` (pixel size), `i`/`p` (image/placement ID), `m` (chunking), `o` (compression), `d` (delete sub-action), `q` (query quiet flag)
  - Core actions:
    - `a=T` / `a=t` — transmit (+ optional display). Direct base64 only (`t=d`). Formats: `f=24` RGB, `f=32` RGBA (default), `f=100` PNG
    - `a=p` — display previously transmitted image by ID
    - `a=d` — delete placements (sub-actions: `a/A` all, `i/I` by id, `c/C` at cursor)
    - `a=q` — query support (respond with OK/error)
  - Chunked transmission: `m=1` accumulates, `m=0` finalizes. Chunks append to a per-image accumulation buffer keyed by imageId.
  - Compression: `o=z` -> `juce::GZIPDecompressorInputStream` with `zlibFormat` over `MemoryInputStream`
  - Image decode: `f=100` -> `juce::ImageFileFormat::loadFrom`. `f=24/32` -> raw pixel buffer, `Image::convertedToFormat(ARGB)` if RGB
  - Response: write `\x1b_Gi=<id>;OK\x1b\\` or error string back to PTY via `writeInput` callback
- Image storage: decoded images held in KittyDecoder keyed by Kitty image ID, for deferred display (`a=p`). Cleared on `a=d` delete.
- READER THREAD: Parser `apcEnd()` dispatches to KittyDecoder. Decoder parses command, decodes pixels. Calls `Grid::Writer::activeWriteImage()` for cell span. Writes decoded RGBA to atomic transfer buffer.
- MESSAGE THREAD: consumes atomic transfer buffer, calls `imageAtlas.stage()`. Same path as Sixel/iTerm2.

**Validation:**
- Test with `kitty +kitten icat` (uses `a=T`, `m=0/1`, `f=100`)
- Test with `chafa --format=kitty` and `timg -pk`
- Chunked transmission reassembles correctly across multiple APC sequences
- Delete by ID clears correct placements
- Query responds correctly (tools use this to detect Kitty support)
- BLESSED-B: same READER->MESSAGE->GL topology. Per-image accumulation buffer owned by KittyDecoder, freed on finalize or delete.
- BLESSED-E (Encapsulation): decoder produces RGBA. Never touches atlas or renderer.
- BLESSED-S (SSOT): single cell-writing path shared with Sixel/iTerm2
- NAMES: `KittyDecoder` — noun, `decode` — verb

---

### Step 11: Polish + Edge Cases

**Scope:** Cross-cutting

**Action:**
- LRU eviction: verify atlas doesn't grow unbounded under sustained image output
- Scrollback: verify images scroll correctly, atlas releases when cells scroll past capacity
- Resize: verify image cells reflow with grid (imageCells in `reflow()`)
- Selection: verify selection overlay renders on top of image cells
- Nexus snapshot: verify `getStateInformation` / `setStateInformation` round-trips images correctly (imageCells serialized, imageId references valid after restore via atlas re-staging)
- Performance: measure frame time with 10+ visible images. Target: 60fps.
- Config: verify `image.atlas_budget` respects configured value
- DA1 response: verify `4` (Sixel) capability already declared in ParserCSI.cpp:806. Add Kitty graphics response to `a=q` query.

**Validation:**
- Full @Auditor sweep against MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md
- Performance targets from SPEC.md (inline images don't drop below 60fps)
- No regression in existing terminal rendering (text, emoji, box drawing, selection, hints)

---

## Dependency Chain

```
Step 0 (Packer lock-free) ──────────────────────────────► Step 4 (ImageAtlas follows pattern)
Step 1 (Parser buffers) ────────────────────────────────► Step 8, 9, 10 (decoders need buffers)
Step 2 (Cell flags) ► Step 3 (Grid imageCells) ► Step 4 (ImageAtlas)
                                                ► Step 5 (Snapshot/Cache)
                                                     ► Step 6 (processCellForSnapshot)
                                                          ► Step 7 (drawImages)
                                                               ► Step 8  (Sixel)  ─┐
                                                               ► Step 9  (iTerm2) ─┼► Step 11 (Polish)
                                                               ► Step 10 (Kitty)  ─┘
```

Steps 0 and 1 are independent — can execute in parallel.
Steps 2-7 are sequential (rendering pipeline build-up).
Steps 8, 9, and 10 are independent — can execute in parallel.

## JUCE + jam Leverage

| Need | JUCE / jam provides | Replaces |
|---|---|---|
| PNG/JPEG/GIF decode | `ImageFileFormat::loadFrom(data, size)` | Kitty's `png_from_data`, custom decoders |
| Base64 decode | `MemoryBlock::fromBase64Encoding` | Manual base64 |
| Zlib decompress | `GZIPDecompressorInputStream` | Kitty's `inflate_zlib` |
| Image scaling | `Image::rescaled()` | Manual resampling |
| Format conversion | `Image::convertedToFormat(ARGB)` | Manual pixel conversion |
| Pixel access | `Image::BitmapData` | Raw pointer math |
| Atlas packing | `jam::Glyph::AtlasPacker` | Kitty's per-texture approach |
| Staged upload | `StagedBitmap` + `Mailbox<StagedBatch>` | Kitty's `upload_to_gpu` + disk cache (939 lines) |
| GL texture | `GLContext::createAtlasTexture()` | Manual GL setup |
| CPU mirror | `GraphicsAtlas` pattern | N/A (Kitty has no CPU fallback) |
| Lock-free transfer | `jam::Mailbox` | Kitty's mutex / WezTerm's mutex |
| Memory buffers | `HeapBlock`, `MemoryBlock`, `MemoryOutputStream` | Manual malloc/realloc |

## BLESSED Alignment

- **B (Bound):** ImageAtlas owned by MainComponent (same as GlyphAtlas), reference flows down. imageCells HeapBlock co-allocated with cells in Buffer. StagedBatch owns HeapBlock, recycled via double-buffer. Parser hybrid buffers owned by Parser. Thread topology: READER->MESSAGE->GL (same as glyph pipeline). No dangling references.
- **L (Lean):** No new abstraction layers. No new concurrency primitives. No new shaders. Side-table mirrors Grapheme. Emoji shader reused. SnapshotBase untouched. JUCE/jam do the heavy lifting.
- **E (Explicit):** LAYOUT_IMAGE / LAYOUT_IMAGE_CONT flags make every codepoint->imageId reinterpretation visible. Named types (ImageCell, ImageRegion, ImageQuad). Lazy buffer allocation is explicit (on first image sequence). No magic values.
- **S (SSOT):** imageId in cell.codepoint. Pixel offset in imageCells. ImageAtlas sole texture registry. One cell-writing path shared by all three decoders. One place each.
- **S (Stateless):** lookup is pure. Decoders are stateless per image (produce RGBA, never touch atlas). processCellForSnapshot image branch is stateless given row pointers and atlas. Mailbox is pure plumbing.
- **E (Encapsulation):** Decoder produces RGBA — never sees atlas or renderer. Renderer never sees decoder. ImageAtlas boundary: stage/publish/consume/lookup/release. Decoders in terminal/logic/, atlas in MainComponent, rendering in terminal/rendering/. SnapshotBase not modified.
- **D (Deterministic):** Same imageId + ImageCell offset + cell geometry -> same ImageQuad. MESSAGE->GL channel is SPSC with deterministic ordering. No hidden state.

## Risks / Open Questions

- **Sixel aspect ratio:** DCS intro params specify pixel aspect ratio. Need to confirm whether to honour it or ignore (most modern terminals ignore). ARCHITECT to decide at Step 8.
- **Large image decode time:** Sixel images can be megabytes. Decoding on reader thread may block byte processing. If this becomes an issue, consider async decode with a staging buffer. Monitor in Step 11.
- **Nexus image restore:** After snapshot restore, imageIds in cells reference images that don't exist in the new client's atlas. Need a re-staging mechanism or accept blank images until the app redraws. ARCHITECT to decide at Step 11.
- **Kitty extended features (deferred):** shared memory (`t=s`), temp file (`t=t/f`), animation (`a=f/a/c`), unicode placeholders (`U+10EEEE`), z-indexing, relative placements. Each is incremental on top of core. Unicode placeholders is the most architecturally significant (enables Kitty images inside tmux).
