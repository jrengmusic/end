# RFC — Terminal::Image — Universal Image Subsystem
Date: 2026-04-28
Status: Ready for COUNSELOR handoff

## Problem Statement

Inline images (Sixel, iTerm2) currently live as cells in Grid's text buffer (`LAYOUT_IMAGE` / `LAYOUT_IMAGE_CONT` flags on Cell). This creates:

- **`imageProtectionActive` hack**: per-batch boolean on Grid that prevents text from overwriting image cells within the same `Parser::process()` batch. A workaround for images living where they don't belong.
- **Image/text coupling**: image cells and text cells share the same buffer. Erase, scroll, overwrite all require awareness of image cells. Grid's job is text — images are a renderer concern.
- **No animation support**: decoders hardcode frame 0 (`CGImageSourceCreateImageAtIndex(source, 0)` on macOS, `decoder->GetFrame(0, &frame)` on Windows). GIF animation from chafa works only because chafa drives its own frame loop, emitting N independent sixel sequences. END has no native animation capability.
- **Protocol coupling**: Kitty's graphics protocol (stateful, bidirectional) leaked state into Grid internals — the `ImageCell` sidecar and U+10EEEE render path stripped in Sprint 42 were symptoms. The correct boundary: protocol delivers data, renderer owns presentation.

Sprint 42 proved the unified decode pipeline works. This RFC extracts images from Grid entirely, establishing `Terminal::Image` as the image subsystem with correct architecture.

## Research Summary

### Current Implementation (What Dies)

**Cell flags** (`Cell.h:259-305`):
- `LAYOUT_IMAGE` (0x10) — head cell, `codepoint` repurposed as imageId.
- `LAYOUT_IMAGE_CONT` (0x02) — continuation cells.
- `isImage()`, `isImageContinuation()` accessors.

**Grid image write** (`Grid.cpp:716-754`):
- `activeWriteImage()` — sets `imageProtectionActive = true`, writes head cell (`LAYOUT_IMAGE`, codepoint=imageId) + continuation cells (`LAYOUT_IMAGE_CONT`) into Grid's text buffer.
- `imageProtectionActive` (`Grid.h:969`) — per-batch boolean, cleared at start of every `Parser::process()` (`Parser.cpp:111`).

**Grid image protection** (`Grid.cpp:562`, `ParserVT.cpp:213`):
- `activeWriteCell()` slow path and ground fast path both check: if `imageProtectionActive` and target cell `isImage()`, skip the write.

**Grid image ID reservation** (`Grid.h`):
- `reserveImageId()` — atomic counter on Grid for unique image IDs.

**Grid decoded image store** (`Grid.h:906`):
- `decodedImages` — `unordered_map<uint32_t, PendingImage>`, READER writes, MESSAGE reads via `getDecodedImage()` / `releaseDecodedImage()`.

**Renderer image path** (`ScreenRenderCell.cpp:233-303`):
- `processCellForSnapshot()` checks `isImage()` / `isImageContinuation()`.
- Head cell: looks up atlas, stages if needed, emits `ImageQuad`.
- Continuation cell: emits background only.
- `cachedImages` / `imageCacheCount` per-row image quad cache.

**GL draw** (`ScreenGL.cpp:124-163`):
- `drawImages()` — iterates snapshot `ImageQuad` array, converts to `Render::Glyph`, draws with atlas texture.

**Decoder backends** (`ImageDecodeMac.mm:33`, `ImageDecodeWin.cpp:40`):
- macOS: `CGImageSourceCreateImageAtIndex(source, 0)` — frame 0 only.
- Windows: `decoder->GetFrame(0, &frame)` — frame 0 only.
- Frame count and delay metadata ignored on both platforms.

### What This Kills

| Component | Removed |
|---|---|
| `Cell::LAYOUT_IMAGE` | Flag deleted |
| `Cell::LAYOUT_IMAGE_CONT` | Flag deleted |
| `Cell::isImage()` | Accessor deleted |
| `Cell::isImageContinuation()` | Accessor deleted |
| `Grid::activeWriteImage()` | Method deleted |
| `Grid::imageProtectionActive` | Field deleted |
| `Grid::Writer::clearImageProtection()` | Method deleted |
| `Grid::Writer::isImageProtected()` | Method deleted |
| `Grid::reserveImageId()` | Moved to ImageAtlas (already owns nextImageId) |
| `Grid::decodedImages` | Moved to State (pending image handoff) |
| `Grid::storeDecodedImage()` | Replaced by State pending image write |
| `Grid::getDecodedImage()` | Replaced by State pending image read |
| `Grid::releaseDecodedImage()` | Deleted |
| Image protection checks in `activeWriteCell()` | Deleted |
| Image protection checks in ground fast path | Deleted |
| `LAYOUT_IMAGE` / `LAYOUT_IMAGE_CONT` branches in `processCellForSnapshot()` | Replaced by ValueTree-driven compositing |

### What Stays

| Component | Status |
|---|---|
| `ImageAtlas` | Unchanged — pixel storage, staging, GL upload, LRU eviction |
| `ImageRegion` | Unchanged — UV + dimensions |
| `DecodedImage` / `PendingImage` | Unchanged — decoder output format |
| `SixelDecoder` | Unchanged — produces DecodedImage |
| `ITerm2Decoder` | Unchanged — produces DecodedImage |
| `KittyDecoder` | Unchanged — produces DecodedImage |
| `drawImages()` in ScreenGL | Adapted — reads from ValueTree-derived quads instead of cell-derived quads |

### Established Patterns (What We Follow)

**Terminal::State as SSOT.** State is always the sole source of truth for any state machine. Lock-free. READER always writes to atomics, consumer always reads from MESSAGE thread. Everything on State is metadata. All components are event-driven, reacting to what changes on State. Orchestrator classes listen and react. This is the established architecture for title, cwd, cursor position, screen parameters — image follows the same contract.

**ValueTree as the storage layer.** State's ValueTree is the SSOT that MESSAGE thread reads from. READER writes to raw atomic buffers on State, `timerCallback` flushes to ValueTree at 60/120 Hz, listeners react. This infrastructure is proven in JUCE's audio domain: APVTS handles 48,000 Hz sample rate × 8x oversampling = 384,000 parameter operations/second under 1.3ms hard real-time deadlines in production DAWs. A terminal at 60-120 Hz with a few dozen image nodes is orders of magnitude less demanding. ValueTree overhead is a non-concern — empirically proven by decades of commercially shipped audio products.

**ImageAtlas ownership**: already on Screen (via MainComponent → Tabs → Panes → Display → Screen). Unchanged — Screen owns the pixel storage, State owns the metadata.

**Decoder pipeline**: Decoder → DecodedImage → staging → atlas. The decoder doesn't know or care about the render target. Only the write target changes: Grid cells → State ValueTree.

## Principles and Rationale

### Why images don't belong in Grid

Grid is the text buffer. Its job: store characters, scroll them, erase them, reflow them on resize. Images are not characters. Embedding images as cells forces:
- Protection hacks (imageProtectionActive) — text must not overwrite images within a batch
- Cell repurposing (codepoint as imageId) — breaks the semantic contract of the field
- Continuation cell overhead — N×M cells marked LAYOUT_IMAGE_CONT, each consuming 16 bytes for zero information
- Renderer branching — every cell in processCellForSnapshot checks image flags

Extracting images to their own subsystem eliminates all four. Grid becomes pure text. The renderer composites text and images as separate layers.

### Protocol boundary

Sixel and iTerm2 prove the correct architecture: protocol delivers pixel data, terminal decides rendering. These protocols are stateless and unidirectional — they worked first try with BLESSED because they respect separation of concerns.

Kitty's graphics protocol violated this boundary — stateful, bidirectional, baking placement semantics into the wire format. Sprint 42 stripped Kitty's state leak. This RFC completes the extraction: images are a renderer concern, not a grid/protocol concern.

### BLESSED mapping

- **B (Bound)**: State owns image metadata (ValueTree nodes). Screen owns ImageAtlas (pixel storage). Grid owns nothing image-related. Clear ownership, clear destruction. RAII lifecycle.
- **L (Lean)**: No continuation cells (N×M × 16 bytes eliminated). No per-cell image flag checks in renderer hot path. Image metadata is sparse ValueTree nodes — only exists when images exist.
- **E (Explicit)**: Image is its own subsystem, not a cell attribute. No codepoint repurposing. No hidden imageProtectionActive flag. ValueTree properties are named and visible.
- **S (SSOT)**: State ValueTree is the sole truth for image metadata. ImageAtlas is the sole truth for pixel data. No shadow state between Grid and renderer. No parallel structs.
- **S (Stateless)**: Renderer reads ValueTree per frame — no persistent image state cached outside State. Screen holds no image metadata. Atlas is pure pixel storage with no opinions.
- **E (Encapsulation)**: Decoders produce data. State bridges threads via ValueTree. Screen renders. Each has one job. Unidirectional: READER → State atomics → flush → ValueTree → listeners react → Screen renders.
- **D (Deterministic)**: Same ValueTree state + same atlas → same output. No batch-scoped protection flags creating timing-dependent behavior.

### Dual backend survival

Image metadata lives on ValueTree — renderer-agnostic. Any rendering backend (GL, Metal, Vulkan, CPU software) reads the same ValueTree properties and resolves pixels from ImageAtlas. ImageAtlas already supports dual backends (GL texture + CPU atlas). Swapping the renderer changes nothing about image data or lifecycle. This is the same architectural property that makes the rest of END's rendering backend-swappable.

## Scaffold

### 1. Image as ValueTree Nodes on State

Images are ValueTree child nodes under an `IMAGES` parent, following the same pattern as all other State metadata.

```
SESSION (root)
  ├── SCREEN_NORMAL
  ├── SCREEN_ALTERNATE
  ├── IMAGES
  │   ├── IMAGE { gridRow, gridCol, cellCols, cellRows, imageId }
  │   ├── IMAGE { gridRow, gridCol, cellCols, cellRows, imageId, frameCount, currentFrame, ... }
  │   └── ...
  └── ...
```

**READER writes** (decoder completion):
- Parser decodes image → writes raw image metadata to atomic buffer on State → bumps generation atomic (`memory_order_release`).

**State::timerCallback flushes** (MESSAGE thread, 60/120 Hz):
- Checks image generation. If advanced, propagates raw buffer to ValueTree: adds/updates `IMAGE` children under `IMAGES` node with properties `gridRow`, `gridCol`, `cellCols`, `cellRows`, `imageId`.

**Listeners react** (MESSAGE thread):
- Screen (or renderer) listens to `IMAGES` node. On change, rebuilds image quads from ValueTree properties + ImageAtlas lookup. Same event-driven pattern as cursor, title, screen parameters.

**Animation properties** (on animated IMAGE nodes):
- `frameCount` — number of frames (1 = static, >1 = animated)
- `currentFrame` — current frame index (advanced by timerCallback)
- `frameStartMs` — timestamp when current frame started displaying
- Frame imageIds and delays stored as ValueTree properties or binary data

**Animation tick** (inside State::timerCallback, MESSAGE thread):
- For each IMAGE node where `frameCount > 1`: check `now - frameStartMs >= delays[currentFrame]`. If elapsed, advance `currentFrame`, update `frameStartMs`, update `imageId` property to `imageIds[currentFrame]`. Listeners fire, renderer picks up the new frame.
- No separate timer. State's existing timerCallback at 60/120 Hz handles cursor blink, state flush, AND image animation. One timer for everything.

### 2. Parser Write Target Change

```cpp
// ParserOSCExt.cpp — handleOsc1337() — replace Grid cell writes with State metadata
void Parser::handleOsc1337 (const uint8_t* data, int dataLength) noexcept
{
    // ... decode same as before ...

    if (image.isValid())
    {
        const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
        const int cursorRow    { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int cursorCol    { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
        const int absRow       { state.getRawValue<int> (ID::scrollbackUsed) + cursorRow };

        const int cellCols { (image.width  + cellW - 1) / cellW };
        const int cellRows { (image.height + cellH - 1) / cellH };

        // Reserve imageId — ImageAtlas owns the counter
        const uint32_t imageId { imageAtlas.reserveImageId() };

        // Write image metadata to State (not cells to Grid)
        state.addImage (absRow, cursorCol, cellCols, cellRows, imageId);

        // Cursor advance — same as before
        cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
        state.setCursorCol (scr, 0);

        // Store decoded image for MESSAGE thread staging
        PendingImage pending;
        pending.imageId = imageId;
        pending.rgba    = std::move (image.rgba);
        pending.width   = image.width;
        pending.height  = image.height;

        state.storePendingImage (std::move (pending));
    }
}
```

Sixel decoder path (`ParserDCS.cpp`) follows the same pattern.

### 3. Renderer — ValueTree Listener

```cpp
// Screen listens to IMAGES ValueTree node
// On valueTreeChildAdded / valueTreePropertyChanged:
void rebuildImageQuads (const juce::ValueTree& imagesNode,
                        const ImageAtlas& atlas,
                        int visibleBase, int visibleRows,
                        int physCellWidth, int physCellHeight,
                        Render::Snapshot& snapshot) noexcept
{
    for (int i { 0 }; i < imagesNode.getNumChildren(); ++i)
    {
        const auto img { imagesNode.getChild (i) };
        const uint32_t imageId { static_cast<uint32_t> (static_cast<int> (img.getProperty (ID::imageId))) };

        if (imageId != 0)
        {
            const int gridRow  { img.getProperty (ID::gridRow) };
            const int gridCol  { img.getProperty (ID::gridCol) };
            const int cellCols { img.getProperty (ID::cellCols) };
            const int cellRows { img.getProperty (ID::cellRows) };
            const int viewRow  { gridRow - visibleBase };

            if (viewRow >= -cellRows and viewRow < visibleRows)
            {
                const ImageRegion* region { atlas.lookup (imageId) };

                if (region != nullptr)
                {
                    Render::ImageQuad iq;
                    iq.screenBounds = {
                        gridCol * physCellWidth,
                        viewRow * physCellHeight,
                        region->widthPx,
                        region->heightPx
                    };
                    iq.uvRect = region->uv;

                    snapshot.addImage (iq);
                }
            }
        }
    }
}
```

No cell scanning. No `processCellForSnapshot` image branches. ValueTree children iterated directly. Event-driven — rebuilds only when IMAGES node changes.

### 4. Decoder Multi-Frame Extraction

**macOS** (`ImageDecodeMac.mm`):
```cpp
DecodedImageSequence loadAnimatedImageNative (const uint8_t* data, size_t size) noexcept
{
    DecodedImageSequence result;

    // ... create CGImageSource same as before ...

    const size_t frameCount { CGImageSourceGetCount (source) };
    result.frameCount = static_cast<int> (juce::jmin (frameCount,
                                          static_cast<size_t> (maxAnimationFrames)));

    for (int i { 0 }; i < result.frameCount; ++i)
    {
        CGImageRef cgImage { CGImageSourceCreateImageAtIndex (source, i, nullptr) };

        if (cgImage != nullptr)
        {
            // ... extract RGBA same as existing frame 0 path ...
            result.frames[i] = std::move (decodedFrame);

            // Extract frame delay
            CFDictionaryRef props { CGImageSourceCopyPropertiesAtIndex (source, i, nullptr) };
            if (props != nullptr)
            {
                CFDictionaryRef gifProps { static_cast<CFDictionaryRef> (
                    CFDictionaryGetValue (props, kCGImagePropertyGIFDictionary)) };

                if (gifProps != nullptr)
                {
                    CFNumberRef delayRef { static_cast<CFNumberRef> (
                        CFDictionaryGetValue (gifProps, kCGImagePropertyGIFDelayTime)) };

                    if (delayRef != nullptr)
                    {
                        double delay { 0.0 };
                        CFNumberGetValue (delayRef, kCFNumberDoubleType, &delay);
                        result.delays[i] = juce::jmax (1, static_cast<int> (delay * 1000.0));
                    }
                }
                CFRelease (props);
            }
            CGImageRelease (cgImage);
        }
    }

    return result;
}
```

**Windows** (`ImageDecodeWin.cpp`): equivalent using `IWICBitmapDecoder::GetFrameCount()` + `GetFrame(i)` + WIC metadata reader for delay.

**Static images**: `frameCount == 1`, no delays. Single IMAGE ValueTree node with one imageId. No animation tick needed.

### 5. Erase Sequence Handling

When Parser processes erase sequences (ED, EL, ECH), it clears image entries that overlap the erased region via State's raw buffer:

```cpp
// In erase handlers (ParserEdit.cpp)
state.clearImagesInRegion (topRow, leftCol, bottomRow, rightCol);
```

State bumps the image generation atomic. Next flush removes the corresponding IMAGE children from ValueTree. Listeners fire, renderer updates.

### 6. Scroll Handling

IMAGE nodes store absolute grid rows (scrollback-aware). The renderer translates: `viewRow = gridRow - visibleBase`. Images scroll naturally with text content — the coordinate system handles it. No special scroll handler needed.

Images that scroll past scrollback capacity become unreachable. Their ValueTree nodes are reclaimed during flush when detected out of bounds. Atlas LRU eviction reclaims pixel data for unreferenced imageIds.

### 7. Alt Screen

When switching to alternate screen, IMAGE nodes referencing normal screen rows are outside the visible coordinate space — renderer skips them naturally. When switching back, they reappear. No clearing needed.

For apps on alt screen emitting images: IMAGE nodes use alt-screen coordinates. On alt screen exit, those nodes reference rows that no longer exist — reclaimed during next flush.

### 8. Native Image Preview (Open File / Hyperlink Click)

Two user-initiated paths lead to image preview:

- **Modal open file**: user enters open-file mode → selects image file → preview
- **Hyperlink click**: user activates hyperlink pointing to image file → preview

Both are MESSAGE thread — no cross-thread barrier. The path is simpler than the decoder path:

1. User activates image (MESSAGE thread)
2. Load file → decode (MESSAGE thread). Multi-frame for GIF → `DecodedImageSequence`.
3. `ImageAtlas::stage()` — already MESSAGE thread. For animated GIF: stage all frames, get N imageIds.
4. Add `IMAGE` child to `IMAGES` ValueTree node **directly** — no atomic buffer, no flush cycle. Already on MESSAGE thread. Set `preview: true` property.
5. Set State `preview` property on ValueTree → listeners fire → renderer enters split viewport mode.
6. Any key → clear State `preview` property → listener fires → renderer restores full viewport. IMAGE node removed. Atlas LRU reclaims pixel data.

**Fully event-driven.** No manual boolean tracking. No `if (isPreviewActive)` scattered through code. State property changes on ValueTree, listeners react:
- Key handler sees State `preview` property, routes any key to "clear preview." Orchestrator tells, never asks.
- Renderer sees State `preview` property, splits viewport. On clear, restores.
- No component knows about any other component. All react to State.

#### Split Viewport Rendering

Preview uses a **split viewport** — not a floating overlay, not a clip:

- **Left portion**: active terminal buffer rendered with **visual-only reflow** to the narrower display width. Grid stays full width in memory. PTY not notified (no SIGWINCH). Scrollback untouched. The reflow is purely presentational — the renderer wraps existing content to `splitCol` width for display purposes only. No mutation of Grid, no mutation of live state.
- **Right portion**: bordered panel containing the preview image. Border drawn with END's native box rendering. Image scaled to fit panel with aspect ratio preserved. Rendered from ImageAtlas.

The terminal VT100 screen model stays pure: normal/alternate only. Preview is UI state, not terminal state. Grid doesn't know the split exists. The split is a renderer concern.

```
SESSION (root)
  ├── SCREEN_NORMAL          ← VT100 spec
  ├── SCREEN_ALTERNATE       ← VT100 spec
  ├── IMAGES
  │   ├── IMAGE { gridRow, gridCol, ... }              ← inline (decoder)
  │   └── IMAGE { preview: true, imageId, ... }        ← native preview
  └── preview: false         ← UI state flag
```

#### Preview Flow

```
activate image
  → State::preview = true (ValueTree property)
  → State::splitCol = viewportCols / 2 (ValueTree property)
  → IMAGE node added with preview: true + imageId(s)
  → listeners fire
  → renderer: split viewport, visual reflow left, image panel right
  → input handler: routes all keys to "dismiss preview"

any key
  → State::preview = false (ValueTree property)
  → IMAGE preview node removed
  → listeners fire
  → renderer: full viewport restored instantly
  → atlas: LRU reclaims preview image data
```

Zero mutation of Grid. Zero mutation of PTY. Zero SIGWINCH. Ephemeral — exists only in State properties and renderer output. Dismiss discards everything.

#### Animation in Preview

Animated GIF preview works for free:
- All frames staged into atlas during step 3 (N imageIds)
- IMAGE node holds frame data (imageIds + delays)
- `State::timerCallback` advances `currentFrame` on the IMAGE node — same path as inline animated images
- Renderer reads current imageId from the node, draws current frame in the preview panel
- On dismiss: all frame imageIds released, atlas LRU reclaims

No special animation code for preview. Same timerCallback, same ValueTree listener, same atlas. Preview is just another IMAGE node with a flag.

## BLESSED Compliance Checklist

- [x] **Bound** — State owns image metadata (ValueTree). Screen owns ImageAtlas (pixels). Grid owns nothing. One owner per resource. RAII lifecycle.
- [x] **Lean** — No continuation cells (N×M × 16 bytes eliminated). No per-cell image flags. Image metadata exists only when images exist (ValueTree children, not fixed arrays). Zero cost when no images on screen.
- [x] **Explicit** — ValueTree properties are named (`gridRow`, `gridCol`, `imageId`). No codepoint repurposing. No hidden protection flags. Animation state is visible properties on the node.
- [x] **SSOT** — State ValueTree = sole truth for positions. ImageAtlas = sole truth for pixels. No shadow state. No parallel structs. ValueTree IS the image data, not a copy of it.
- [x] **Stateless** — Renderer reads ValueTree per frame. Screen caches no image metadata. Atlas is pure pixel storage. Animation state lives on the ValueTree node, advanced by timerCallback — not tracked by any external object.
- [x] **Encapsulation** — Decoders produce data (unchanged). State bridges threads (ValueTree). Screen renders (atlas + listener). Unidirectional: READER → atomics → flush → ValueTree → listener → render. User-initiated preview: MESSAGE → ValueTree directly → listener → render. Both paths converge at ValueTree. Protocol boundary respected. No component knows about any other — all react to State.
- [x] **Deterministic** — Same ValueTree state + same atlas = same output. No batch-scoped flags. No timing-dependent protection hacks.

## Open Questions

1. **Atlas pressure from animation**: A 60-frame GIF at 800×600 scaled to display size (e.g. 400×300) = 60 × 400 × 300 × 4 = 28.8 MB — close to the 32 MB atlas budget. Strategy: scale to fit display size before staging. Large animations may need frame-on-demand loading instead of all-upfront staging. COUNSELOR should evaluate.

2. **Scrollback image reclamation**: IMAGE nodes with `gridRow` past scrollback capacity should be removed from ValueTree and atlas entries released. Detected during flush cycle or scroll events. Current atlas LRU eviction partially handles pixel reclamation, but explicit ValueTree node removal is cleaner.

3. **`reserveImageId()` ownership**: Currently on Grid. Should move to ImageAtlas (which already owns `nextImageId` counter). ImageAtlas is the natural owner — it assigns IDs in `stage()`.

4. **GIF disposal model**: Full GIF spec includes per-frame compose modes (blend vs replace), per-frame offsets, and background disposal. Current design stages raw decoded frames. Some GIFs with complex disposal will render incorrectly without pre-compositing frames during decode. Wezterm solves this via `image` crate's `AnimationDecoder` which pre-composes. COUNSELOR should evaluate whether native decoders (CGImageSource, WIC) handle disposal automatically or if pre-composition is needed.

5. **DecodedImage handoff**: Currently `Grid::decodedImages` (unordered_map, READER writes, MESSAGE reads). Moves to State with the same cross-thread pattern — READER stores pending image in raw buffer, MESSAGE retrieves for atlas staging during flush. Storage mechanism (ring buffer, map, fixed array) is implementation detail for COUNSELOR.

6. **Animation frame storage on ValueTree**: Frame imageIds and delays for animated images need to be stored on the IMAGE node. Options: individual indexed properties (`frameId0`, `frameId1`, ...), or binary blob property (`juce::MemoryBlock` containing the arrays). Binary blob is leaner for many frames. COUNSELOR should decide.

7. **Preview split ratio**: Fixed 50% split, or adaptive based on image aspect ratio? A tall narrow image wastes space in a 50% panel. An adaptive split that sizes the panel to the image (with min/max bounds) would be more elegant. COUNSELOR should evaluate.

## Handoff Notes

- This RFC depends on RFC-HYPERLINKS being implemented first. The hyperlink refactor establishes the cell-flag + State-table pattern and validates the "Grid is pure text" principle.
- The decoder multi-frame extraction (Section 4) is the only new capability. Everything else is reorganization of existing working code — same pipeline, different write target.
- `drawImages()` in ScreenGL stays almost identical — it already draws from an ImageQuad array. The source changes from cell-derived quads to ValueTree-derived quads, but the GL path is unchanged.
- SKiT protocols (Sixel, iTerm2, Kitty) are respected byte-for-byte. No protocol changes. No decoder changes (except multi-frame extraction). Apps see identical behavior.
- The `imageProtectionActive` hack dying is the clearest architectural win. It was a per-batch workaround for a boundary violation. With images out of Grid, the problem it solved doesn't exist.
- `Terminal::Image` is not a class or namespace — it is the image metadata on State's ValueTree (`IMAGES` node + `IMAGE` children) plus ImageAtlas on Screen. The "subsystem" is just ValueTree nodes and an atlas. No god object. No new abstraction layer.
- Animation is driven by State's existing `timerCallback` (60/120 Hz) — the same timer that handles cursor blink and state flush. No separate animation timer. One timer for everything.
- Native image preview (Section 8) replaces `MessageOverlay::showImage()` for the open-file and hyperlink-click paths. MessageOverlay continues to serve its original purpose (transient status messages — reload, errors). The preview path is MESSAGE thread only — no cross-thread complexity, no flush latency. ValueTree write is direct.

### Comparative Analysis

Evaluated against wezterm, kitty, and ghostty image architectures:

| Aspect | END (this RFC) | Wezterm | Kitty | Ghostty |
|---|---|---|---|---|
| Image/grid separation | Full — Grid is pure text | Cell-embedded (`ImageCell` per cell) | Mostly separate (`GraphicsManager`) | Full — cells are text-only |
| SSOT | ValueTree = sole truth | UV crops shadow state across N×M cells | 3 maps tracking same image | Clean (ImageStorage) |
| Renderer coupling | Decoupled — ValueTree listener | Tight — renderer reads cell attrs | Moderate — layer rebuild from placement state | Moderate — renderer-specific state |
| Animation | timerCallback frame advance (designed) | Instant-based (battle-tested) | Protocol-driven state machine | Not implemented |
| Dual backend | ValueTree is renderer-agnostic | Tied to glyph cache impl | OpenGL-aware render data | Renderer-specific |
| Per-cell overhead | Zero | ImageCell per covered cell | Minimal (unicode placeholders only) | Zero |
| Protocol support | Sixel + iTerm2 + Kitty (partial) | Sixel + iTerm2 + Kitty | Kitty (native) | Kitty only |

END's design achieves Ghostty's architectural cleanliness with Wezterm's protocol coverage, using JUCE's ValueTree as proven infrastructure. The ValueTree approach is validated by JUCE's audio domain where it handles 384,000 parameter operations/second under 1.3ms hard real-time deadlines — orders of magnitude more demanding than terminal rendering at 60-120 Hz.
