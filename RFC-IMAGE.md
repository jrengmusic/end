# RECOMMENDATION — Inline Image Rendering
Date: 2026-04-04
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END needs inline image rendering for Sixel (DCS `Pq...ST`) and iTerm2 (OSC 1337) protocols,
as specified in SPEC.md Phase 4. The question was how to integrate image cells into the
existing `Cell` / `Grid` / `Screen<Renderer>` / `Render::Snapshot` pipeline without
violating BLESSED or touching module boundaries unnecessarily.

---

## Research Summary

### Codebase ground truth (all read directly — no assumptions)

**Cell** (`Source/terminal/data/Cell.h`)
- 16 bytes, trivially copyable, `static_assert` enforced.
- `layout` byte current usage: `0x01` WIDE_CONT, `0x04` EMOJI, `0x08` GRAPHEME.
- Free bits in `layout`: `0x02`, `0x10`, `0x20`, `0x40`, `0x80`.
- `reserved` byte: always 0, documented as future-extension slot — not used.
- `codepoint` field: `uint32_t`, valid range U+0000–U+10FFFF.

**Grapheme side-table** (`Source/terminal/data/Cell.h`, `Source/terminal/logic/Grid.h`)
- `getCellKey(row, col)` exists in Cell.h but is NOT used by Grid for indexing.
- Grid uses **physical co-indexing**: `buffer.graphemes[physicalRow * cols + col]`.
- Both `cells` and `graphemes` are parallel `HeapBlock`s of size `totalRows × cols`,
  allocated together in `initBuffer()`, same lifetime as the Buffer.
- `LAYOUT_GRAPHEME` flag on the Cell signals a valid entry at the same physical offset.

**Grid** (`Source/terminal/logic/Grid.h`)
- Ring buffer, power-of-two `totalRows`, `head` pointer, `rowMask` for modular indexing.
- `Buffer` struct owns: `HeapBlock<Cell> cells`, `HeapBlock<RowState> rowStates`,
  `HeapBlock<Grapheme> graphemes`.
- Cell write API: `activeWriteCell`, `activeWriteGrapheme`, `directRowPtr` + `markRowDirty`.
- Read API (MESSAGE THREAD): `activeVisibleRow`, `activeVisibleGraphemeRow`,
  `scrollbackRow`, `scrollbackGraphemeRow`.

**Screen render pipeline** (`Source/terminal/rendering/ScreenRender.cpp`)
- `buildSnapshot()` consumes dirty bits, iterates rows, calls `processCellForSnapshot()`
  per cell. Leading/trailing blank columns are trimmed per row.
- `processCellForSnapshot()` dispatch order:
  1. Hint label override (substitutes cell content for flash-jump labels).
  2. Colour resolution (`resolveCellColors`).
  3. Background quad if bg ≠ default.
  4. `ligatureSkip` decrement.
  5. If `hasContent()`: block char → `buildBlockRect`, else → `buildCellInstance`.
  6. Selection overlay quad.
  7. Link underlay quad.
- `LAYOUT_WIDE_CONT` is NOT explicitly guarded — wide CONT cells have `codepoint == 0`
  so `hasContent()` skips them naturally. Background is still emitted if bg ≠ default.

**Snapshot** (`Source/terminal/rendering/Screen.h`, `ScreenSnapshot.cpp`)
- `Render::Snapshot` inherits `jreng::Glyph::Render::SnapshotBase`.
- `SnapshotBase::ensureCapacity(mono, emoji, bg)` — fixed 3-param, in `jreng_graphics`.
  Cannot be extended without touching the module. Image capacity lives on the derived
  `Render::Snapshot`, not the base.
- `Render::Snapshot` already adds cursor/scroll fields directly — same pattern for images.

**Per-row render cache** (`Screen.h`)
- `cachedMono`, `cachedEmoji`: `[row * maxGlyphsPerRow + index]`, `maxGlyphsPerRow = cacheCols * 2`.
- `cachedBg`: `[row * bgCacheCols + index]`, `bgCacheCols = cacheCols * 3`.
- A fourth per-row cache (`cachedImages`) is needed for image quads.

**GL / CPU draw path** (`ScreenGL.cpp`)
- `renderOpenGL` and `renderPaint`: `drawBackgrounds` → `drawQuads(mono)` →
  `drawQuads(emoji)` → `drawCursor`. Image draw call slots in after emoji.
- Both backends expose identical duck-type API (`drawBackgrounds`, `drawQuads`).
  A new `drawImages` method is added to both.

**Atlas pattern** (`modules/jreng_graphics/glyph/`)
- `AtlasPacker`: shelf-based bin packer, fixed 4096×4096 atlas, O(S) allocation.
- `Region`: UV rect + pixel dimensions + bearing. `ImageRegion` mirrors this.
- `GraphicsContext`: owns `sharedMonoAtlas` / `sharedEmojiAtlas` as `juce::Image`
  instances that mirror the GL atlas layout. CPU blits via locked `BitmapData`.
  One locked `BitmapData` per frame for all blits — memory bandwidth optimised.

**SPEC.md confirmed:**
- Draw call budget: `3–4 (background + mono + emoji + images)`.
- Image cells store image ID + position within image, similar to wide-char continuation.
- Decode on message thread (async if large). Upload via staged bitmap queue.
- Scrollback: images scroll with content as cell references.

---

## Resolved Design Decisions

**1. LRU eviction: fixed cap, 32MB, configurable in `end.lua`**

Single shelf-packed RGBA8 atlas (mirrors `AtlasPacker` / glyph atlas architecture).
Fixed 32MB cap — dynamic VRAM query adds complexity with no benefit at END's scale.
Configurable same as `scrollback` lines. Eviction unit: shelf region, not whole atlas.

Rationale for single atlas on CPU path: `GraphicsContext` composites via `BitmapData`
pixel walks — one locked `BitmapData` per frame for all image blits vs. N lock/unlock
cycles for N separate images. Same reason glyph atlas is one `juce::Image`.
On GL: fewer texture binds, one instanced draw call for all image quads per frame.

**2. Staged upload queue: on `ImageAtlas`, same pattern as `Typeface::consumeStagedBitmaps`**

`ImageAtlas` owns a `StagedImageUpload` queue. READER THREAD decodes RGBA and enqueues.
MESSAGE THREAD drains in the GL frame setup alongside `uploadStagedBitmaps` —
`ImageAtlas::consumeStagedUploads()` called from `renderOpenGL` / `renderPaint`
before snapshot packing. Assigns `imageId` at drain time.

**3. Atlas: single shelf-packed RGBA8, mirrors glyph atlas exactly**

`AtlasPacker` algorithm, same shelf-packing, same LRU eviction pattern.
`ImageRegion` mirrors `jreng::Glyph::Region`: UV rect + pixel dimensions.
LRU cache keyed by `uint32_t imageId`.

**4. `drawImages` shader: reuse emoji shader**

Emoji shader: RGBA8 texture + UV sampling + instanced quads — structurally identical
to image quads. `drawImages` binds the image atlas texture (single atlas → single
texture handle per session) and issues the same instanced draw call. No new shader.

**5. CPU-path image pool: `ImageAtlas` owns `juce::Image` atlas mirror**

`ImageAtlas` owns both the GL texture handle AND a `juce::Image` (ARGB,
`SoftwareImageType`) as the CPU-side atlas mirror. Exact same pattern as
`GraphicsContext::sharedEmojiAtlas`. Eviction clears both atomically.
`drawImages` on `GraphicsContext` reads from `ImageAtlas::getCPUAtlas()` via
`BitmapData`, same compositing path as emoji glyphs.

---

## Principles & Rationale

**Why side-table, not union Cell::Pixel**
Grid already has a working parallel HeapBlock pattern (Grapheme). Reusing it is
**SSOT** and **Lean** — no new pattern, no struct change, no `static_assert` risk.
Side-table entries are co-allocated with cells and travel through the ring buffer
automatically including scrollback.

**Why imageId in `cell.codepoint`**
`codepoint` is `uint32_t`. IDs start at 1 so `hasContent()` returns true for head
cells. `LAYOUT_IMAGE` flag in `layout` makes the reinterpretation **Explicit** at
every read site. No struct change.

**Why `LAYOUT_IMAGE_CONT` mirrors `LAYOUT_WIDE_CONT`**
CONT cells must skip the image quad path AND the glyph path. Explicit flag is required
because CONT cells could have non-default `bg` (transparent image edge) — they still
emit background quads. The flag must be checked before `hasContent()`.

**Why images live on `Render::Snapshot`, not `SnapshotBase`**
`SnapshotBase::ensureCapacity` is fixed 3-param in `jreng_graphics` — shared with
WHELMED. **Lean**: don't touch the module for a terminal-specific concern.
`Render::Snapshot` already adds cursor fields outside the base. Same pattern.

---

## Scaffold

### 1. New layout flags (Cell.h — additive only)

```cpp
static constexpr uint8_t LAYOUT_IMAGE      { 0x10 }; // head cell of an image span
static constexpr uint8_t LAYOUT_IMAGE_CONT { 0x02 }; // continuation cell (0x02 currently free)

bool isImage()             const noexcept { return (layout & LAYOUT_IMAGE)      != 0; }
bool isImageContinuation() const noexcept { return (layout & LAYOUT_IMAGE_CONT) != 0; }
```

No struct change. `static_assert(sizeof(Cell) == 16)` still holds.
`imageId` stored in `cell.codepoint` when `LAYOUT_IMAGE` is set. `imageId > 0` always.

### 2. ImageCell side-table entry (Cell.h — new struct)

```cpp
struct ImageCell
{
    uint16_t offsetX; // pixel offset within source image for this cell's top-left (X)
    uint16_t offsetY; // pixel offset within source image for this cell's top-left (Y)
};
static_assert(std::is_trivially_copyable_v<ImageCell>);
static_assert(sizeof(ImageCell) == 4);
```

### 3. Grid::Buffer extension (Grid.h / Grid.cpp)

```cpp
// In struct Buffer:
juce::HeapBlock<ImageCell> imageCells; // totalRows x cols, co-indexed with cells
```

`initBuffer()`:
```cpp
buffer.imageCells.allocate(static_cast<size_t>(totalRows) * static_cast<size_t>(numCols), true);
```

Same physical row math as graphemes. Public API mirrors grapheme API exactly:
```cpp
void            activeWriteImageCell    (int row, int col, const ImageCell& ic) noexcept;
const ImageCell* activeReadImageCell    (int row, int col) const noexcept;
const ImageCell* activeVisibleImageRow  (int row) const noexcept;
const ImageCell* scrollbackImageRow     (int visibleRow, int scrollOffset) const noexcept;
```

`Grid::Writer` gets the same passthrough wrappers. `reflow()` and `clearBuffer()`
mirror every existing `graphemes` operation for `imageCells`.

**Serialization**: `Grid::getStateInformation` / `setStateInformation` must include
`imageCells` alongside `cells`, `graphemes`, and `rowStates`. Same memcpy pattern —
`totalRows × cols × sizeof(ImageCell)` per buffer. Both normal and alternate screens.

### 4. ImageAtlas (Source/terminal/rendering/ImageAtlas.h/.cpp)

```cpp
namespace Terminal
{

struct ImageRegion
{
    juce::Rectangle<float> uv;   // normalised UV rect within atlas [0,1]
    int widthPx;                  // source image width in pixels
    int heightPx;                 // source image height in pixels
    GLuint glTextureHandle;       // GL texture name (image atlas texture)
};

class ImageAtlas
{
public:
    // Enqueues RGBA pixels for staged upload. Returns assigned imageId (> 0).
    // Called on READER THREAD — pixels copied into staging queue, not uploaded yet.
    uint32_t stage(const uint8_t* rgba, int widthPx, int heightPx) noexcept;

    // Drains staged uploads into the atlas texture (GL + juce::Image mirror).
    // Called on MESSAGE THREAD in renderOpenGL/renderPaint before snapshot packing.
    void consumeStagedUploads() noexcept;

    // Returns nullptr if imageId evicted or not yet uploaded.
    const ImageRegion* lookup(uint32_t imageId) const noexcept;

    // Marks imageId as unused. LRU eviction candidates.
    void release(uint32_t imageId) noexcept;

    // Evicts LRU entries until atlas usage < budgetBytes. Called once per frame.
    void evictIfNeeded(int budgetBytes) noexcept;

    // CPU-side atlas image for GraphicsContext path.
    const juce::Image& getCPUAtlas() const noexcept;

private:
    uint32_t             nextId     { 1 };
    jreng::Glyph::AtlasPacker packer { atlasSize, atlasSize };
    GLuint               glTexture  { 0 };
    juce::Image          cpuAtlas;   // ARGB SoftwareImageType, mirrors GL atlas
    // LRU map: imageId -> ImageRegion + last-used frame counter
    static constexpr int atlasSize  { 4096 };
};

} // namespace Terminal
```

Owned by `Screen::Resources` alongside `snapshotBuffer` and `terminalColors`.

### 5. ImageQuad (Screen.h, inside struct Render)

```cpp
struct ImageQuad
{
    juce::Rectangle<float> screenBounds; // physical px, cell-aligned top-left + size
    juce::Rectangle<float> uvRect;       // normalised [0,1] sub-rect within atlas
    GLuint                 glTexture;    // atlas GL texture handle
};
static_assert(std::is_trivially_copyable_v<ImageQuad>);
```

### 6. Render::Snapshot extension (Screen.h)

```cpp
// Added to struct Snapshot : jreng::Glyph::Render::SnapshotBase:
juce::HeapBlock<ImageQuad> images;
int   imageCount    { 0 };
int   imageCapacity { 0 };
```

### 7. Per-row image cache (Screen.h private members)

```cpp
juce::HeapBlock<ImageQuad> cachedImages;   // [row * maxImagesPerRow + index]
juce::HeapBlock<int>       imageCacheCount; // per-row
int                        maxImagesPerRow { 0 }; // = cacheCols
```

`allocateRenderCache(rows, cols)`:
```cpp
maxImagesPerRow = cols;
cachedImages.allocate   (static_cast<size_t>(rows * maxImagesPerRow), false);
imageCacheCount.allocate(static_cast<size_t>(rows), true);
```

### 8. processCellForSnapshot — image branch

Inserted at the very top of `processCellForSnapshot`, BEFORE the hint label block.
Image cells must never be overridden by hint labels.

```cpp
// Image cells are checked first — must never be overridden by hint labels or text paths.
// Positive nesting, no early returns (JRENG-CODING-STANDARD).

if (cell.isImageContinuation())
{
    // CONT cell: emit background quad if non-default. No text, no image quad.
    const juce::Colour bg { resolveBackground (cell.bg, resources.terminalColors) };

    if (bg != resources.terminalColors.defaultBackground)
    {
        cachedBg[row * bgCacheCols + bgCount[row]++] = {
            { static_cast<float> (col * physCellWidth),
              static_cast<float> (row * physCellHeight),
              static_cast<float> (physCellWidth),
              static_cast<float> (physCellHeight) },
            bg.getFloatRed(), bg.getFloatGreen(), bg.getFloatBlue(), bg.getFloatAlpha()
        };
    }
}
else if (cell.isImage())
{
    // Head cell: emit one ImageQuad. No text paths.
    const uint32_t imageId { cell.codepoint };
    const ImageRegion* reg { resources.imageAtlas.lookup (imageId) };

    if (reg != nullptr and rowImageCells != nullptr)
    {
        const ImageCell& ic { rowImageCells[col] };
        const float fw { static_cast<float> (reg->widthPx) };
        const float fh { static_cast<float> (reg->heightPx) };
        const float uL { ic.offsetX / fw };
        const float uT { ic.offsetY / fh };
        const float uR { (ic.offsetX + physCellWidth)  / fw };
        const float uB { (ic.offsetY + physCellHeight) / fh };

        cachedImages[row * maxImagesPerRow + imageCacheCount[row]++] = {
            { static_cast<float> (col * physCellWidth),
              static_cast<float> (row * physCellHeight),
              static_cast<float> (physCellWidth),
              static_cast<float> (physCellHeight) },
            { uL, uT, uR - uL, uB - uT },
            reg->glTexture
        };
    }
}
else
{
    // Normal cell — existing hint label / colour / shaping / selection / underlay paths.
    // ... (all existing processCellForSnapshot logic lives here)
}
```

`processCellForSnapshot` gains `const ImageCell* rowImageCells` parameter alongside
`const Grapheme* rowGraphemes`. `buildSnapshot` passes `activeVisibleImageRow(r)` /
`scrollbackImageRow(r, offset)`. `imageCacheCount[r]` reset to 0 per dirty row.

### 9. updateSnapshot extension (ScreenSnapshot.cpp)

After existing base `ensureCapacity` call and before `resources.snapshotBuffer.write()`:

```cpp
int totalImages { 0 };
for (int r { 0 }; r < rows; ++r)
    if (isRowIncludedInSnapshot(r))
        totalImages += imageCacheCount[r];

if (totalImages > snapshot.imageCapacity)
{
    snapshot.images.allocate(static_cast<size_t>(totalImages), false);
    snapshot.imageCapacity = totalImages;
}

int imgOffset { 0 };
for (int r { 0 }; r < rows; ++r)
{
    if (isRowIncludedInSnapshot(r) and imageCacheCount[r] > 0)
    {
        std::memcpy(snapshot.images.get() + imgOffset,
                    cachedImages.get() + r * maxImagesPerRow,
                    static_cast<size_t>(imageCacheCount[r]) * sizeof(ImageQuad));
        imgOffset += imageCacheCount[r];
    }
}
snapshot.imageCount = totalImages;
```

### 10. GL / CPU draw (ScreenGL.cpp)

In both `renderOpenGL` and `renderPaint`, after `drawQuads(emoji)`:

```cpp
if (snapshot->imageCount > 0)
    textRenderer.drawImages(snapshot->images.get(), snapshot->imageCount);
```

**`GLContext::drawImages`**: binds image atlas GL texture, issues instanced draw using
emoji shader (RGBA8 + UV sampling). `ImageQuad::glTexture` is the same handle for all
quads (single atlas). One `glBindTexture` + one instanced draw call per frame.

**`GraphicsContext::drawImages`**: locks `ImageAtlas::getCPUAtlas()` BitmapData once,
iterates `ImageQuad` array, blits each `uvRect` sub-region to `screenBounds` via
SIMD-composited pixel writes — same path as `compositeEmojiGlyph`. One `BitmapData`
lock for all image blits per frame.

### 11. Decoder boundary (Source/terminal/logic/)

`SixelDecoder` and `ITerm2Decoder` are pure Grid writers + staging producers.

```
// READER THREAD:
decode(rawBytes) -> RGBA buffer + widthPx + heightPx
imageAtlas.stage(rgba, w, h)  -> imageId   // copies to staging queue, returns id

// Compute cell span:
cellCols = ceil(widthPx  / physCellWidth)
cellRows = ceil(heightPx / physCellHeight)

// Write cells:
for each (visibleRow, col) in [imageStartRow .. +cellRows) x [imageStartCol .. +cellCols):
    cell.codepoint = (row == imageStartRow && col == imageStartCol) ? imageId : 0
    cell.layout    = (head cell) ? LAYOUT_IMAGE : LAYOUT_IMAGE_CONT
    cell.bg        = defaultBackground
    grid.activeWriteCell(visibleRow, col, cell)
    grid.activeWriteImageCell(visibleRow, col, {
        offsetX: uint16_t((col - imageStartCol) * physCellWidth),
        offsetY: uint16_t((row - imageStartRow) * physCellHeight)
    })

// MESSAGE THREAD (next frame setup):
imageAtlas.consumeStagedUploads()  // uploads RGBA into atlas, activates imageId
```

Decoders never touch the renderer. Renderer never sees Sixel or iTerm2.
Protocol is an ingestion format only.

---

## BLESSED Compliance Checklist

- [x] **Bounds** — `imageCells` HeapBlock co-indexed with `cells` in `Buffer`. Same
  lifetime, same `initBuffer` allocation, same physical row math. No dangling refs.
- [x] **Lean** — No new abstraction layers. Side-table mirrors Grapheme exactly.
  `SnapshotBase` untouched. Decoders are Grid writers + staging producers only.
  Emoji shader reused — no new shader.
- [x] **Explicit** — `LAYOUT_IMAGE` / `LAYOUT_IMAGE_CONT` flags make every reinterpretation
  of `cell.codepoint` as `imageId` visible at the read site. `ImageCell` is a named
  type. `ImageRegion` is a named type. No silent field aliasing.
- [x] **SSOT** — `cell.codepoint` is the imageId. `imageCells[physRow * cols + col]`
  holds the pixel offset. `ImageAtlas` is the sole texture registry (GL + CPU).
  One place each.
- [x] **Stateless** — `ImageAtlas::lookup` is a pure lookup. Decoders are stateless
  per image. `processCellForSnapshot` image branch is stateless given row pointers
  and atlas.
- [x] **Encapsulation** — Decoder never sees renderer. Renderer never sees decoder.
  `ImageAtlas` boundary: stage/consume/lookup/release. `SnapshotBase` not modified.
- [x] **Deterministic** — Same imageId + same `ImageCell` offset + same cell geometry
  always produces the same `ImageQuad`. No hidden state in the render path.

---

## Open Questions

None. All decisions settled by ARCHITECT.

---

## Handoff Notes

- **No source files modified in this session.** Design only. COUNSELOR implements.
- `Cell.h`: additive only — two `static constexpr uint8_t` flags, two `bool` accessors.
  Zero risk to existing paths. `static_assert(sizeof(Cell) == 16)` unaffected.
- `Grid::Buffer`: mirror every `graphemes` operation for `imageCells` in `initBuffer`,
  `reflow`, and `clearBuffer`. Mechanical — no logic changes.
- `processCellForSnapshot` image branch: inserted at the very top, BEFORE hint label
  block. CONT cells return before `hasContent()` is tested — required because CONT
  cells have `codepoint == 0` and would otherwise be skipped by `hasContent()` without
  clearing the image quad path. Head cells return after emitting one `ImageQuad`,
  bypassing colour resolution, shaping, selection, and underlay entirely.
- `buildSnapshot` leading/trailing blank trim: head cells have `imageId > 0` so
  `hasContent()` is true — not trimmed. CONT cells have `codepoint == 0` — may be
  trimmed at row edges with default bg. Acceptable: CONT cells emit no quads.
- `previousCells` memcmp skip: works correctly — `imageId` change or `layout` byte
  change detected by memcmp, row rebuilt. No special handling needed.
- `drawImages` reuses the emoji shader (instanced RGBA8 + UV sampling). Verify
  instanced draw approach aligns with existing GL pipeline before implementation.
- WHELMED image rendering (Phase 5): `ImageAtlas` and `ImageQuad` designed for eventual
  extraction into `jreng_text`, but extraction is out of scope for this feature.
