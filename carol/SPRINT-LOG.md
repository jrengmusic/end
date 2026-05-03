# SPRINT-LOG

## Sprint 3: Crossplatform Font-from-Binary Helper — FreeType Symmetric with CoreText ✅

**Date:** 2026-05-03
**Duration:** ~00:20

### Agents Participated
- SURGEON — diagnosis, direct edits

### Files Modified (1 total)

- `jam/jam_graphics/fonts/jam_typeface.cpp:227` — new `static FT_Face makeFaceFromBinary (FT_Library, const void*, int)` helper; mirrors `makeCTFontFromBinary` from the macOS side; wraps `FT_New_Memory_Face` + `FT_Byte*` cast
- `jam/jam_graphics/fonts/jam_typeface.cpp:279` — `loadFaces()`: `FT_New_Memory_Face` call replaced with `makeFaceFromBinary`
- `jam/jam_graphics/fonts/jam_typeface.cpp:542` — `setFontFamily()`: same
- `jam/jam_graphics/fonts/jam_typeface.cpp:319` — `addFallbackFont()`: stub implemented; `makeFaceFromBinary` + `FT_Set_Char_Size` + `userFallbackFonts.push_back`

### Alignment Check
- [x] BLESSED — S (SSOT): `FT_New_Memory_Face` now exists once (helper); L (Lean): no new abstraction layers, helper is a direct mechanical extraction; Encapsulation: font binary loading stays inside Typeface
- [x] NAMES.md — `makeFaceFromBinary` is verb-noun, consistent with `makeCTFontFromBinary` (cross-platform naming symmetry)
- [x] MANIFESTO.md — SSOT enforced; YAGNI respected (helper does exactly one thing, no speculative parameters)

### Problems Solved
- `FT_New_Memory_Face` + `reinterpret_cast<const FT_Byte*>` block was duplicated verbatim in `loadFaces` and `setFontFamily` — SSOT violation; now eliminated
- `addFallbackFont` was a TODO stub (`juce::ignoreUnused`) — now implemented, symmetric with macOS `addFallbackFont`
- Non-macOS now fully consistent with macOS: both platforms have a `makeFaceFromBinary` / `makeCTFontFromBinary` helper and a working `addFallbackFont`

### Debts Paid
None

### Debts Deferred
None

---

## Sprint 2: Embedded Font Priority — Display Mono from Binary on macOS ✅

**Date:** 2026-05-03
**Duration:** ~01:30

### Agents Participated
- SURGEON — diagnosis, surgical plan, direct edits, audit response
- Pathfinder (×2) — surveyed `jam_typeface.mm` methods; confirmed `JamFontsBinaryData.h` include and symbol names via `jam_typeface.cpp` reference
- Engineer (×1) — initial implementation of 3-method scaffold (embeddedFontForFamily, loadFaces, setFontFamily)
- Auditor (×1) — identified C1 (mainFont fallback still system lookup) and C2 (3-site CGDataProvider duplication)

### Files Modified (1 total)

- `jam/jam_graphics/fonts/jam_typeface.mm:226` — new `static CTFontRef makeCTFontFromBinary (const void*, int, CGFloat)` helper; eliminates all CGDataProvider→CGFont→CTFont duplication
- `jam/jam_graphics/fonts/jam_typeface.mm:804` — `embeddedFontForFamily()`: replaced no-op stub with Display Mono family dispatch (mirrors `jam_typeface.cpp:184–210`); doxygen updated
- `jam/jam_graphics/fonts/jam_typeface.mm:262` — `loadFaces()`: embedded-first path (via helper) for Display Mono families; system descriptor lookup in `else` for other families; `mainFont == nullptr` fallback routes through `embeddedFontForFamily("Display Mono")` + helper (no system lookup); doxygen updated
- `jam/jam_graphics/fonts/jam_typeface.mm:548` — `setFontFamily()`: same embedded-first treatment as `loadFaces()`
- `jam/jam_graphics/fonts/jam_typeface.mm:391` — `addFallbackFont()`: refactored to use `makeCTFontFromBinary` helper; body reduced from 22 lines to 9

### Alignment Check
- [x] BLESSED — B: no new resources float free; E: `jassert` on fallback data pointer, no silent fail; S (SSOT): `makeCTFontFromBinary` eliminates 3-site duplication; D: embedded binary produces identical results across machines
- [x] NAMES.md — `makeCTFontFromBinary` is verb-noun, describes operation and return; `fallbackData`/`fallbackSize` consistent with `embeddedData`/`embeddedSize`
- [x] MANIFESTO.md — Lean: helper extracted because same logic appeared 3+ times; Encapsulation: font resolution stays inside Typeface; SSOT: embedded binary is now the authoritative source for Display Mono on macOS

### Problems Solved
- macOS `embeddedFontForFamily()` was a no-op stub — system-installed Display Mono (stale, different build) was winning over the embedded canonical binary
- `mainFont == nullptr` fallback still called `CTFontCreateWithName(CFSTR("Display Mono"))` — system lookup, violating RFC locked principle "no system-installed copy ever overrides the embedded canonical font"
- `CGDataProvider → CGFont → CTFont` chain was inlined identically in `loadFaces`, `setFontFamily`, and `addFallbackFont` — SSOT violation; extracted to `makeCTFontFromBinary`
- macOS now symmetric with non-macOS: `embeddedFontForFamily()` dispatches Display Mono variants, system lookup only for unknown families

### Debts Paid
None

### Debts Deferred
None

---

## Sprint 1: SKiT Image Preview — Overlay Architecture + Display Decomposition ✅

**Date:** 2026-05-02
**Duration:** ~04:00

### Agents Participated
- COUNSELOR — audit triage, ARCHITECTURE.md updates, delegation, fact-checking
- Engineer (×4) — Overlay implementation, Display decomposition, stale doxygen fixes, config ref + getContentBounds
- Auditor (×1) — comprehensive audit of all SKiT changes + refactoring opportunities
- Pathfinder (×8) — codebase surveys (Display methods, ARCHITECTURE.md state, config stability, stale refs, git state)

### Files Modified (50+)

**New files:**
- `Source/terminal/rendering/Overlay.h` — jam::gl::Component, owns juce::Image, border, animation timer
- `Source/terminal/rendering/Overlay.cpp` — paintGL/paint unconditional render, Timer animation
- `Source/component/TerminalDisplayPreview.cpp` — extracted preview lifecycle (handleOpenImage, activatePreview, dismissPreview, consumePendingPreview)
- `Source/terminal/rendering/Glyph.cpp/h` — glyph rendering extracted from Screen
- `Source/terminal/rendering/GlyphCell.cpp` — per-cell snapshot processing
- `Source/terminal/rendering/GlyphShape.cpp` — box-drawing/block-element shape rendering
- `Source/terminal/rendering/Render.h` — shared GPU-facing render types extracted from Screen.h

**Major rewrites:**
- `Source/component/TerminalDisplay.h` — config ref member, getContentBounds decl, 3 new private method decls (processDirtySnapshot, rebuildSelectionFromState, consumePendingPreview), padding inits from config ref, Overlay/preview members
- `Source/component/TerminalDisplay.cpp` — onVBlank decomposed to 18 lines, getContentBounds impl, resized() uses getContentBounds, all getContext() replaced with config ref, ImageDecode include moved to preview file
- `Source/terminal/rendering/Screen.h` — Image ref removed, numCols/numRows to Cell, Metrics member, 3-arg constructor
- `Source/terminal/rendering/Screen.cpp` — calc() uses Metrics, getCellBounds/cellAtPoint use Metrics
- `Source/terminal/rendering/ScreenRender.cpp` — image.drainPending removed, stale doxygen fixed
- `Source/terminal/rendering/ScreenGL.cpp` — image render/preview scissor blocks removed
- `Source/terminal/rendering/ScreenSnapshot.cpp` — image.packInlineQuads removed
- `Source/MainComponent.cpp` — recursive GL walker for child jam::gl::Components
- `Source/terminal/logic/ParserDCS.cpp` — handleSkitFilepath shared helper extracted, Metrics member
- `Source/terminal/logic/ParserOSCExt.cpp` — same handleSkitFilepath pattern
- `Source/terminal/logic/Processor.h` — stale TerminalDisplay.h include removed (layer violation fix)
- `Source/terminal/data/State.h/cpp` — preview state flags
- `Source/component/Panes.cpp` — cellsFromRect migrated to Metrics
- `ARCHITECTURE.md` — 7 targeted edits: header, module map, module inventory (jam_tui), data flow (preview pipeline), decision (Overlay), glossary

**Stale doxygen fixed:**
- `Source/terminal/rendering/ScreenRender.cpp:8,20,25` — ScreenRenderCell → GlyphCell
- `Source/terminal/rendering/GlyphShape.cpp:13,34` — ScreenRenderCell → GlyphCell
- `Source/terminal/logic/SixelDecoder.h:9,26,41,60` — ImageAtlas → State::addImageNode
- `Source/terminal/logic/KittyDecoder.h:29` — ImageAtlas → State::addImageNode
- `Source/terminal/logic/ParserDCS.cpp:24` — ImageAtlas FIFO → onImageDecoded

**Deleted:**
- `Source/terminal/rendering/ImageAtlas.h/cpp` — entire atlas/FIFO subsystem
- `Source/terminal/rendering/Image.h/cpp/ImageRender.cpp` — entire Image renderer
- `Source/terminal/rendering/Preview.h/cpp` — 960-line god object replaced by 140-line Overlay
- `Source/terminal/rendering/ScreenRenderCell.cpp` — moved to GlyphCell.cpp
- `Source/terminal/rendering/ScreenRenderGlyph.cpp` — moved to GlyphShape.cpp
- `PLAN-skit.md`, `RFC-SKiT.md`, `RFC-cell-metrics.md` — objectives complete

### Alignment Check
- [x] BLESSED principles followed (Overlay is Lean, Bound, Explicit, SSOT)
- [x] NAMES.md adhered (Overlay, Metrics, getContentBounds, handleSkitFilepath — all ARCHITECT-approved)
- [x] MANIFESTO.md principles applied (no FIFO/atlas for one image, jam modules as SSOT, established patterns followed)

### Problems Solved
- Preview god object (~960 lines, handrolled GL) replaced with Overlay (~140 lines, jam::gl::Component)
- Display→Screen pattern replicated for Display→Overlay — side-by-side layout with automatic Screen reflow via PTY resize
- TerminalDisplay.cpp onVBlank decomposed from 139 lines to 18 lines (3 named helpers)
- TerminalDisplay preview lifecycle extracted to TerminalDisplayPreview.cpp (Screen file decomposition pattern)
- Config context stored as reference member — eliminates 10 getContext() calls, 2 defensive null guards collapsed
- getContentBounds() SSOT — eliminates duplicated padding arithmetic in resized() and applyZoom()
- Processor.h layer violation fixed — terminal/logic no longer includes component/TerminalDisplay.h
- Stale doxygen references to deleted files fixed across 6 files
- ARCHITECTURE.md updated to reflect Overlay architecture (was stale: 8+ ImageAtlas references, zero Overlay)
- Cell metrics consolidated in jam::tui::Metrics — replaces ~15 duplicated conversion sites
- handleSkitFilepath extracted as shared parser helper (was triplicated ~50 lines each)

### Debts Paid
None

### Debts Deferred
None

---

## Handoff to COUNSELOR: Image Subsystem — Terminal::Image

**From:** COUNSELOR
**Date:** 2026-04-29
**Status:** In Progress — BROKEN, multiple issues

### Context
Executing RFC-IMAGE.md — extract images from Grid entirely into a separate layer: image metadata on State's IMAGES ValueTree, pixel data on ImageAtlas with READER FIFO submission. Grid becomes pure text. Multi-frame GIF decode with disposal composition. Animation tick on VBlank. Native image preview with split viewport.

### Completed
- Step 1: ImageAtlas READER-facing 16-slot SPSC FIFO (`submitDecoded` / `drainPending`)
- Step 2: State IMAGES ValueTree node (IMAGE children, `addImageNode` / `removeImageNode`, erase bounding box via parameterMap, scrollback reclamation in `flushImages`)
- Step 3: Parser `onImageDecoded` callback, wired in TerminalDisplay to ImageAtlas
- Step 4: Multi-frame decoder extraction (`ImageSequence` struct, `loadImageSequenceNative` with GIF disposal composition on both macOS/Windows, `ImageDecodeGif.h` parser)
- Step 5: Decoder write target switch (all three protocols use `onImageDecoded` callback, erase signals in ED 2/3 only)
- Step 6+7: ValueTree-driven image quad building in `updateSnapshot`, cell-based rendering removed (`isImage`/`isImageContinuation` branches, `cachedImages`/`imageCacheCount` deleted)
- Step 8: Grid cleanup (LAYOUT_IMAGE/LAYOUT_IMAGE_CONT flags, `activeWriteImage`, `imageProtectionActive`, `reserveImageId`, `decodedImages` — all deleted. Grid is pure text.)
- Step 9: Animation tick on Screen VBlank path (`tickImageAnimation`)
- Step 10: Native image preview (split viewport, GL scissor, adaptive ratio, key dismiss, MessageOverlay callback chain removed)
- Step 11: ARCHITECTURE.md updated
- Fix: EraseRegion FIFO replaced with parameterMap bounding box (BLESSED compliance)
- Fix: `linkUriCount` stray member replaced with parameterMap entry
- Fix: `addImageNode` overlap removal (new image replaces old)
- Fix: Image layer decoupled from cell grid (removed stale-image cell check, removed cell clearing in decoders, removed queueImageErase from EL/ECH/ED 0/1)

### Known Issues (BROKEN)
1. **Image persistence:** last shown image persists when fzf switches to text preview — erase signal (ED 2/3 only) doesn't fire for per-line content changes. Image layer lifecycle needs rethinking.
2. **Image persistence on exit:** last shown image persists when fzf is dismissed/exits. Alt screen switch doesn't clear IMAGE nodes.
3. **Images only cleared by `clear`:** only ED 2/3 triggers `queueImageErase`. No other mechanism removes stale inline images.
4. **GIF animation broken:** loading forever, not animating, JUCE assertion failure in `ImageAtlas.cpp:490` — likely `jassert(sub.frameCount > 0)` in `drainPending` or atlas staging overflow for multi-frame images.
5. **Native image preview broken:** needs redesign — the split viewport / GL scissor approach has fundamental issues.

### Key Decisions (Locked)
- ImageAtlas owns pixel lifecycle end-to-end (READER FIFO submission, MESSAGE drain + stage, GL upload)
- No `reserveImageId` — atlas assigns ID at staging time
- Scrollback reclamation during `flush()` — explicit, deterministic
- Erase: any overlap removes entire image
- FIFO: 16 slots, each holds full image sequence (N frames + delays)
- Parser access: `std::function` callback (no layer violation Logic→Rendering)
- Frame data on ValueTree: binary blob (`juce::MemoryBlock`) — packed imageId + delay arrays
- Preview split ratio: adaptive based on image aspect ratio
- Animation tick: Screen VBlank path (NOT State timer — State timer is flush only)
- Image layer decoupled from cell grid — no cell content checks for image lifecycle
- All State values go through parameterMap → ValueTree. No stray atomics, no invented primitives.

### Files Modified (30+)
- `Source/terminal/rendering/ImageAtlas.h/cpp` — READER FIFO (submitDecoded/drainPending), Submission struct, thread contract table updated
- `Source/terminal/data/Identifier.h` — IMAGES, IMAGE, imageId, gridRow, gridCol, cellCols, cellRows, frameCount, currentFrame, frameStartMs, frameData, widthPx, heightPx, eraseTop/Left/Bottom/Right, linkUriCount, preview, splitCol, isPreview
- `Source/terminal/data/State.h/cpp` — IMAGES ValueTree node, addImageNode/removeImageNode, queueImageErase (parameterMap bbox), setScrollbackCapacity, setPreview/isPreviewActive/getSplitCol, linkUriCount→parameterMap
- `Source/terminal/data/StateFlush.cpp` — flushImages (erase bbox drain + scrollback reclamation)
- `Source/terminal/data/Cell.h` — LAYOUT_IMAGE/LAYOUT_IMAGE_CONT deleted
- `Source/terminal/logic/Grid.h/cpp` — activeWriteImage, imageProtectionActive, reserveImageId, decodedImages all deleted
- `Source/terminal/logic/GridAccess.cpp` — decoded image storage implementations deleted
- `Source/terminal/logic/Parser.h` — onImageDecoded callback added
- `Source/terminal/logic/ParserDCS.cpp` — dcsUnhook/apcEnd switched to onImageDecoded
- `Source/terminal/logic/ParserOSCExt.cpp` — handleOsc1337 switched to onImageDecoded + ImageSequence
- `Source/terminal/logic/ParserEdit.cpp` — queueImageErase in ED 2/3 only
- `Source/terminal/logic/ParserVT.cpp` — image protection guard removed
- `Source/terminal/logic/Parser.cpp` — clearImageProtection call removed
- `Source/terminal/logic/ITerm2Decoder.h/cpp` — returns ImageSequence, uses loadImageSequenceNative
- `Source/terminal/logic/ImageDecode.h/cpp` — ImageSequence struct, loadImageSequenceNative declaration
- `Source/terminal/logic/ImageDecodeMac.mm` — multi-frame GIF extraction with disposal composition
- `Source/terminal/logic/ImageDecodeWin.cpp` — multi-frame GIF extraction with disposal composition
- `Source/terminal/logic/ImageDecodeGif.h` — GIF metadata parser (static, platform-independent)
- `Source/terminal/logic/SixelDecoder.h` — stale doxygen updated
- `Source/terminal/logic/KittyDecoder.h` — stale doxygen updated
- `Source/terminal/rendering/Screen.h/cpp` — cachedImages/imageCacheCount removed, tickImageAnimation added, Snapshot gains previewActive/previewSplitCol
- `Source/terminal/rendering/ScreenRender.cpp` — drainPending in buildSnapshot, tickImageAnimation impl
- `Source/terminal/rendering/ScreenSnapshot.cpp` — ValueTree-driven image quad building
- `Source/terminal/rendering/ScreenRenderCell.cpp` — isImage/isImageContinuation branches removed
- `Source/terminal/rendering/ScreenGL.cpp` — GL scissor for preview split
- `Source/component/TerminalDisplay.h/cpp` — onImageDecoded wired, handleOpenImage atlas-based, onShowImagePreview chain removed
- `Source/component/Panes.h/cpp` — onShowImagePreview removed
- `Source/component/Tabs.h/cpp` — onShowImagePreview removed
- `Source/MainComponent.cpp` — onShowImagePreview wiring removed
- `Source/terminal/logic/Input.cpp` — preview dismiss on any key
- `ARCHITECTURE.md` — image subsystem documented

### Open Questions
1. **Image lifecycle for non-ED erases:** how should images be cleared when the program overwrites them with text without explicit ED 2/3? The image layer is decoupled from cells — cell content checks were removed as a layer violation. Need a different signaling mechanism.
2. **Alt screen exit:** when a program exits alt screen (mode 1049l), IMAGE nodes from the alt screen should be cleared. Not currently implemented.
3. **GIF assertion:** `ImageAtlas.cpp:490` assertion in drainPending — likely frameCount or staging issue with multi-frame images. Needs debugging.
4. **Preview redesign:** the split viewport approach (GL scissor + adaptive ratio) needs rethinking. The current implementation is broken.

### Next Steps
1. Debug GIF assertion (`ImageAtlas.cpp:490`) — likely the most straightforward fix
2. Add alt screen exit cleanup — clear IMAGE nodes when mode 1049l switches back to normal screen
3. Design correct image lifecycle mechanism for non-ED text overwrites that respects layer separation
4. Redesign native image preview
5. Run full audit before logging sprint

---

## Sprint 44: Unified StringSlot seqlock + URI table migration + hint overlay direct read ✅

**Date:** 2026-04-29
**Duration:** ~1:30

### Agents Participated
- COUNSELOR: plan update, delegation, audit triage, doc fixes
- Engineer: Step 1 (unified StringSlot), Step 2 (LinkManager string type), Step 3 (hint overlay removal), PLAN deletion
- Auditor: comprehensive audit of sprint changes
- Pathfinder: hint overlay data flow exploration, TerminalDisplay wiring discovery
- Explore: flushStrings verification, setLinkManager wiring check, stale reference grep

### Files Modified (11 total)
- `Source/terminal/data/State.h` — StringSlot gains `generation`/`lastFlushedGeneration`; `LinkUri` struct removed; `linkUriTable` type→StringSlot[]; `linkUriCount`→plain uint32_t; `getLinkUri`→juce::String; `writeSlot`/`flushSlot`/`readSlot` added; `writeStringSlot` removed; `hintOverlayData`/`hintOverlayCount`/`setHintOverlay`/`getHintOverlayData`/`getHintOverlayCount` removed; stale doxygen fixed
- `Source/terminal/data/State.cpp` — 6 generation `addParam` calls removed; `writeSlot`/`readSlot` implemented; `writeStringSlot` deleted; `setTitle`/`setCwd`/`setForegroundProcess`→delegate to `writeSlot`; `registerLinkUri`→uses `writeSlot`; `getLinkUri`→returns `juce::String` via `readSlot`; `flushStrings`→uses `flushSlot` loop; `setHintOverlay`/`getHintOverlayData`/`getHintOverlayCount` removed
- `Source/terminal/data/StateFlush.cpp` — `flushSlot` implementation added
- `Source/terminal/data/Identifier.h` — 6 generation identifiers removed (titleGeneration, cwdGeneration, foregroundProcessGeneration + lastFlushed variants)
- `Source/terminal/selection/LinkManagerScan.cpp:137-153` — `const char*`→`juce::String`; removed `fromUTF8`/`String()` wraps
- `Source/terminal/selection/LinkManager.h` — stale `activeHints`/`setHintOverlay` doxygen updated (4 sites)
- `Source/terminal/rendering/Screen.h` — `class LinkManager` forward decl; `const LinkManager*` member; `setLinkManager` setter
- `Source/terminal/rendering/ScreenRender.cpp` — `#include LinkManager.h`; hint overlay reads from LinkManager directly
- `Source/terminal/logic/Input.cpp:355-387` — 3 `setHintOverlay` calls removed; Space path→`setFullRebuild`+`setSnapshotDirty`
- `Source/component/TerminalDisplay.cpp` — `setHintOverlay` call removed; `setLinkManager` wired in `switchRenderer`
- `ARCHITECTURE.md:391` — Cross-Thread Data Contract updated for intrinsic seqlock + `readSlot` URI path

**Deleted files (1):**
- `PLAN-hyperlinks-valuetree.md` — objective complete

### Alignment Check
- [x] BLESSED principles followed (SSOT: one type, three functions; Explicit: generation intrinsic; Bound: StringSlot self-contained; Lean: 12 identifiers/addParams eliminated)
- [x] NAMES.md adhered (no improvised names)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced

### Problems Solved
- **URI table thread safety:** bare C array + stray atomic replaced by StringSlot[] with intrinsic seqlock. READER writes lock-free, MESSAGE reads via snap-copy. No data race.
- **Scattered generation tracking:** 6 external parameterMap entries collapsed into `StringSlot::generation` intrinsic member. One type, one write function, one flush function, one read function.
- **Hint overlay dangling pointer risk:** raw pointer relay through State eliminated. Screen reads directly from `const LinkManager*`. No intermediary, no dangling risk.
- **Type duplication:** `LinkUri` struct eliminated — identical to `StringSlot` minus the seqlock. Unified into single type.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 43: Kitty graphics protocol wiring + image cell protection + Lean audit sweep ✅

**Date:** 2026-04-28
**Duration:** ~4:00

### Agents Participated
- COUNSELOR: RFC consumption, plan writing, diagnostic analysis, audit triage, delegation
- Pathfinder: current Kitty/APC/image pipeline state survey, write path discovery, loadImageNative survey
- Engineer: all code implementation — apcEnd wiring, KittyDecoder fixes, image protection, diagnostic instrumentation/removal, SSOT extractions, Lean file splits, linker fixes
- Auditor: comprehensive 3-sprint audit against MANIFESTO/NAMES/JRENG-CODING-STANDARD
- Oracle: deep analysis of image rendering pipeline (snapshot → atlas → GL)
- Explore: diagnostic log analysis (5 rounds), line counts, build file investigation

### Files Modified (28 total)

**New files (12):**
- `Source/terminal/logic/ImageDecode.h` — shared image decode header (loadImageNative + swizzleARGBToRGBA)
- `Source/terminal/logic/ImageDecode.cpp` — platform-independent BGRA→RGBA swizzle
- `Source/terminal/logic/ImageDecodeMac.mm` — macOS native image decode (CoreGraphics + ImageIO)
- `Source/terminal/logic/ImageDecodeWin.cpp` — Windows native image decode (WIC) + Linux stub
- `Source/terminal/logic/KittyDecoderDecode.cpp` — parseKittyParams, decodePayload, response builders
- `Source/terminal/logic/SixelDecoderParse.cpp` — Sixel parsing helpers (defaultPalette, parseParams, hlsToRgb, growBuffer, writeSixelByte)
- `Source/terminal/logic/ParserOSC.cpp` — OSC dispatch + handleOscTitle/Cwd/Clipboard/Notification/777/CursorColor
- `Source/terminal/logic/ParserOSCExt.cpp` — handleOsc8, handleOsc133, handleOsc1337
- `Source/terminal/logic/ParserDCS.cpp` — dcsHook, dcsPut, dcsUnhook, apcEnd, setPhysCellDimensions
- `Source/terminal/logic/ParserAction.cpp` — performAction, performEntryAction, appendToBuffer, UTF-8 helpers
- `Source/terminal/rendering/ScreenRenderCell.cpp` — processCellForSnapshot (per-cell snapshot handler)
- `Source/terminal/rendering/ScreenRenderGlyph.cpp` — buildCellInstance, tryLigature, glyph helpers

**Modified files (16):**
- `Source/terminal/logic/Parser.h` — added apcBuffer/apcBufferSize/apcBufferCapacity, kittyDecoder member, #include KittyDecoder.h, updated apcPut/apcString/setPhysCellDimensions doxygen
- `Source/terminal/logic/Parser.cpp` — wired apcPut to appendToBuffer, apcString entry action, clearImageProtection at process() entry, split to ~236 lines
- `Source/terminal/logic/ParserESC.cpp` — split to ~330 lines (ESC dispatch + cursor save/restore only)
- `Source/terminal/logic/ParserVT.cpp:213` — image cell protection guard in directRowPtr fast path
- `Source/terminal/logic/Grid.h` — imageProtectionActive flag, isImageProtected(), Writer::clearImageProtection()/isImageProtected()
- `Source/terminal/logic/Grid.cpp` — image protection in activeWriteCell, imageProtectionActive=true in activeWriteImage, diagnostic removal
- `Source/terminal/logic/GridErase.cpp` — diagnostic removal only
- `Source/terminal/logic/KittyDecoder.h` — KittyParams promoted to private struct, accumulateChunk/finalizeChunk declarations, Result::placementCols/placementRows
- `Source/terminal/logic/KittyDecoder.cpp` — stale accumulator fix (m=1 else-if), c=/r= propagation, M-5 DRY extraction, split to ~346 lines
- `Source/terminal/logic/ITerm2Decoder.cpp` — loadImageNative/swizzle extracted, #include ImageDecode.h, reduced to ~148 lines
- `Source/terminal/logic/SixelDecoder.cpp` — split to ~315 lines
- `Source/terminal/rendering/ImageAtlas.h` — stageImpl private declaration
- `Source/terminal/rendering/ImageAtlas.cpp` — stage/stageWithId consolidated via stageImpl
- `Source/terminal/rendering/ScreenRender.cpp` — split to ~181 lines, per-method template instantiation
- `Source/terminal/rendering/ScreenRenderGlyph.cpp` — per-method template instantiations (replacing whole-class)
- `Source/Main.cpp` — diagnostic Log::Scope removal

**Deleted files (3):**
- `PLAN-shaders.md` — stale, never implemented
- `PLAN-fix-kitty.md` — objective complete
- `RFC-fix-kitty.md` — consumed by completed plan

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (no improvised names — all new names follow existing patterns)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced (brace init, and/or/not tokens, no early returns, no anon namespaces)

### Problems Solved
- **Kitty images not rendering:** apcEnd was empty stub. Wired KittyDecoder → activeWriteImage → storeDecodedImage identical to Sixel/iTerm2 pipeline
- **Stale chunk accumulator:** fzf multi-chunk Kitty transmissions could inherit stale metadata from prior image. Added else-if reset guard on m=1 path
- **Image cells overwritten by text:** fzf writes text padding over Kitty image area (isImage not set for Kitty in fzf). Root cause found via 5-round diagnostic instrumentation. Fixed with per-process-batch imageProtectionActive flag — text skips image cells within same process() call, erase always clears
- **Image persistence on preview switch:** per-batch flag auto-clears at next process() call, allowing text to replace image on subsequent output
- **loadImageNative SSOT violation:** identical ~140-line function duplicated in KittyDecoder.cpp and ITerm2Decoder.cpp. Extracted to shared platform TUs (ImageDecodeMac.mm, ImageDecodeWin.cpp)
- **Lean violations (5 files):** KittyDecoder 933→346, SixelDecoder 514→315, ParserESC 1164→330, Parser 634→236, ScreenRender 1101→181
- **DRY violations:** ImageAtlas stage/stageWithId → stageImpl; KittyDecoder a=t/a=T → accumulateChunk/finalizeChunk

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 42: SKiT image pipeline unification + ImageCell removal + Kitty clean slate ✅

**Date:** 2026-04-28
**Duration:** ~6:00 (across multiple compactions)

### Agents Participated
- COUNSELOR: architecture analysis, pipeline unification direction, delegation
- Pathfinder: ImageCell usage survey, Kitty handling survey, overlay removal survey
- Engineer: decoder integration (60 files), ImageCell removal (7 files), Kitty wiring strip (8 files)

### Commits
- `3f23c9b` — SKiT decoder integration, overlay removal, Display→ValueTree::Listener, Processor ChangeBroadcaster removal
- `52c2f77` — ImageCell sidecar removal, Kitty wiring strip, erase protection removal, diagnostic log cleanup

### Files Modified (60+ total, key changes below)

**New files (decoder infrastructure):**
- `Source/terminal/logic/SixelDecoder.cpp/.h` — Sixel image decoder
- `Source/terminal/logic/KittyDecoder.cpp/.h` — Kitty image decoder (retained, wiring stripped)
- `Source/terminal/logic/ITerm2Decoder.cpp/.h` — iTerm2 image decoder
- `Source/terminal/rendering/ImageAtlas.cpp/.h` — GPU image atlas for decoded images

**Display architecture:**
- `Source/component/TerminalDisplay.h` — changed from ChangeListener on Processor to ValueTree::Listener on State
- `Source/component/TerminalDisplay.cpp` — ValueTree listener wiring, repaint on valueTreePropertyChanged
- `Source/terminal/logic/Processor.h` — removed ChangeBroadcaster inheritance
- `Source/terminal/logic/Processor.cpp` — removed sendChangeMessage

**Overlay removal (replaced by cell-based pipeline):**
- `Source/terminal/logic/Grid.h/.cpp` — removed Overlay struct, setOverlay, getOverlay, clearOverlay, hasOverlayImage, resetOverlayFlag, isOverlayFresh
- `Source/terminal/data/State.h/.cpp` — removed overlay state methods (setOverlayImageId, setOverlayRow, setOverlayCol, etc.)
- `Source/terminal/data/StateFlush.cpp` — removed overlay staleness/clear from flush()
- `Source/terminal/data/Identifier.h` — removed overlay identifiers
- `Source/terminal/rendering/ScreenSnapshot.cpp` — removed overlay rendering block

**ImageCell sidecar removal (dead code):**
- `Source/terminal/data/Cell.h` — removed ImageCell struct + static_asserts
- `Source/terminal/logic/Grid.h/.cpp` — removed HeapBlock<ImageCell> from Buffer, imageCellPtr, activeWriteImageCell, activeVisibleImageRow, scrollbackImageRow, serialization
- `Source/terminal/logic/GridErase.cpp` — removed ImageCell fill; removed erase protection (image cells erase like text)
- `Source/terminal/logic/GridReflow.cpp` — removed all ImageCell handling (~15 sites)
- `Source/terminal/rendering/Screen.h` — removed rowImageCells param from processCellForSnapshot
- `Source/terminal/rendering/ScreenRender.cpp` — removed rowImageCells param; removed dead virtual placement block

**Kitty wiring strip (clean slate):**
- `Source/terminal/logic/ParserESC.cpp` — gutted apcEnd() to empty body
- `Source/terminal/logic/Parser.h/.cpp` — removed kittyDecoder field, apcBuffer, apcBufferSize, apcBufferCapacity
- `Source/terminal/logic/SixelDecoder.h` — removed Kitty-only PendingImage fields
- `Source/terminal/logic/ITerm2Decoder.cpp` — removed all diagnostic logs
- `Source/Main.cpp` — removed diagnostic log scope

### Alignment Check
- [x] BLESSED principles followed — S (SSOT): overlay was shadow state alongside grid cells; ImageCell duplicated what Cell.codepoint provides; both removed. S (Stateless): manual boolean tracking (hasOverlayImage) eliminated; erase protection removed. E (Encapsulation): Display listens to State's ValueTree (established pattern), not ChangeBroadcaster (invented pattern). Unified pipeline — same path for text and image glyphs.
- [x] NAMES.md adhered — no new names introduced
- [x] MANIFESTO.md principles applied — L (Lean): net -504 lines in cleanup commit alone. YAGNI: virtual placement dead code, overlay machinery removed.

### Problems Solved
- **Overlay architecture replaced**: overlay was a separate layer (Overlay struct on Grid, overlay state on State, overlay rendering in ScreenSnapshot) creating shadow state divergence. Replaced with cell-based pipeline — images are just cells with LAYOUT_IMAGE flag and codepoint=imageId.
- **Display notification architecture**: TerminalDisplay was ChangeListener on Processor (invented pattern). Changed to ValueTree::Listener on State (established JUCE pattern). sendChangeMessage/ChangeBroadcaster removed from Processor.
- **ImageCell sidecar dead code**: offsetX/offsetY written by decoders, carried through reflow/serialize, passed to renderer — never dereferenced. Renderer uses cell.codepoint only. Entire sidecar removed.
- **Image persistence after fzf dismiss**: erase protection (isImage skip in GridErase) prevented ED from clearing image cells. Removed — image cells erase identically to text cells.
- **Kitty wiring pollution**: Kitty-specific code paths stripped for clean slate. KittyDecoder files retained for future re-wiring through unified pipeline.
- **Sixel/iTerm2 confirmed working** through unified cell pipeline after all removals.

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 41: scrollOffset reset on screen switch + ED 3 conformance ✅

**Date:** 2026-04-27
**Duration:** ~1:00

### Agents Participated
- SURGEON: diagnosis, fix design, oversight, overlay restore
- Pathfinder: codebase survey (alt-screen switch path, scrollback/scrollOffset flow, ED handler)
- Engineer: StateFlush.cpp + ParserEdit.cpp implementation

### Files Modified (2 total)
- `Source/terminal/data/StateFlush.cpp:209-225` — in `flush()`, after `flushRootParams()`: (1) capture prevActiveScreen from ValueTree before flush, reset `scrollOffset` to 0 if activeScreen changed; (2) enforce `scrollOffset <= scrollbackUsed` invariant — clamp when scrollback shrinks beneath current offset
- `Source/terminal/logic/ParserEdit.cpp:134-136` — ED case 3: removed non-standard `writer.eraseRowRange(0, visibleRows-1)`. ED 3 (`CSI 3 J`) per XTerm spec = erase scrollback only; visible content is untouched

### Alignment Check
- [x] BLESSED principles followed — State is SSOT; invariant enforced in flush(), not in display layer; no shadow state; no cross-thread writes
- [x] NAMES.md adhered — no new names introduced; `jam::ValueTree::getChildWithID` pattern reused from `setScrollOffset()`
- [x] MANIFESTO.md principles applied — S (SSOT): scrollOffset reset belongs in State::flush(), not TerminalDisplay; E (Explicit): invariant clamp is explicit; B (Bound): message-thread-only path throughout

### Problems Solved
- **scrollback appears cleared on vim exit**: `scrollOffset` was never reset on `activeScreen` transitions. After vim (`?1049l`) switched END to normal screen, if user had scrolled back, the view stayed locked to the old offset — live CC content invisible. Fix: `flush()` detects `activeScreen` change post-flush and resets `scrollOffset` to 0.
- **scrollOffset > scrollbackUsed corruption**: when scrollback is cleared (e.g., ED 3 from any app), `scrollOffset` could exceed `scrollbackUsed`, causing `scrollbackRow()` to read logically-deleted ring buffer cells. Fix: `flush()` clamps `scrollOffset` to `scrollbackUsed` after every flush pass.
- **ED 3 non-standard visible erase**: case 3 in `eraseInDisplay()` was calling `eraseRowRange(0, visibleRows-1)` in addition to `clearScrollback()`. XTerm ED 3 = scrollback only. The visible erase wiped the active screen unnecessarily on any `CSI 3 J` sequence.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 40: Migrate lua::Engine to clean-room jam::lua API

**Date:** 2026-04-26

### Agents Participated
- COUNSELOR (jam session): API design, PLAN authorship, migration coordination — END migration driven from jam as producer
- Engineer (x3 parallel): Engine.h/cpp migration, EngineParse.cpp migration, EnginePatch/CMake cleanup
- Engineer: EngineParse.cpp 3-way file split
- Auditor (via jam session): validated all consumer changes

### Files Modified (5 modified, 2 new)
- `Source/lua/Engine.h:818,1246` — `jam::lua::protected_function` → `Function`, `jam::lua::state` → `State`
- `Source/lua/Engine.cpp:68-201,237-250` — State/openLibraries/script(File)/setFunction (void + raw int(lua_State*) overloads), Result replaces try/catch + script_pass_on_error, juce::StringArray replaces std::unordered_set<std::string>, registerApiTable rewritten
- `Source/lua/EngineParse.cpp` — rewritten: keys/popups/actions only (214 lines, was 787). All sol types → jam::lua::Value, .optional<juce::String>() replaces sol::optional<std::string>, forEach replaces structured binding iteration
- `Source/lua/EngineParseDisplay.cpp` — NEW: 6 static display helpers + parseDisplay (357 lines)
- `Source/lua/EngineParseConfig.cpp` — NEW: parseNexus + parseWhelmed (253 lines)
- `Source/lua/EnginePatch.cpp:11` — removed vestigial `#include <jam_lua/jam_lua.h>`
- `CMakeLists.txt:128-129` — removed sol include path (`target_include_directories` for jam_lua/)

### Alignment Check
- [x] BLESSED principles followed (E: explicit .to/.optional conversions, no implicit; L: 787-line file split into 3)
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Full consumer migration from sol3 to jam::lua clean-room API (~144 usage sites)
- Eliminated std::string from Lua boundary — juce::String native throughout
- EngineParse.cpp Lean violation (787 lines) resolved via 3-way split

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 39: Fix Action List Search Esc + Lean Decomposition

**Date:** 2026-04-26

### Agents Participated
- COUNSELOR: `/pay` planning, delegation, audit orchestration
- Pathfinder (x2): action list codebase survey, search box mechanism discovery
- Engineer (x2): Esc bug fix (selectRow visibility + clearSearch callback), Lean decomposition (ActionList.cpp file split + selectRow extraction)
- Auditor: full sprint audit (4 findings: 2 stale doxygen, 2 pre-existing L violations, all resolved)

### Files Modified (5 total)
- `Source/action/KeyHandler.h:29` — added `clearSearch` callback to `Callbacks` struct
- `Source/action/KeyHandler.cpp:26` — Esc search handler calls `clearSearch()` instead of `dismiss()` when no visible rows
- `Source/action/ActionList.h:51-54,147-155,157-161` — updated `keyPressed` doxygen (Esc IS handled by KeyHandler), added `findSelectableRow` declaration, updated `selectRow` doxygen (mentions non-visible rows)
- `Source/action/ActionList.cpp` — wired `clearSearch` lambda in Callbacks aggregate init; trimmed to lifecycle/config/build/dispatch only (289 lines, was 563)
- `Source/action/ActionListSelection.cpp` — NEW: filterRows, layoutRows, findSelectableRow (extracted from selectRow), selectRow, executeSelected, visibleRowCount, getSelectedIndex (201 lines)
- `Source/action/ActionListBinding.cpp` — NEW: valueTreePropertyChanged, binding mode methods (107 lines)

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (clearSearch, findSelectableRow — both approved)
- [x] MANIFESTO.md principles applied
- [x] Lean: ActionList.cpp 563 → 3 files (289/201/107); selectRow 51 → findSelectableRow (29) + selectRow (28)

### Problems Solved
- Action list Esc from search mode: `selectRow(1)` hardcoded index 1 which could be hidden after filtering → merged visibility into skip loop so `selectRow` scans past hidden rows. Esc with no results now clears search text (restoring all rows) instead of dismissing the action list.
- Lean violations: ActionList.cpp split into 3 files following Engine*.cpp pattern; selectRow decomposed into findSelectableRow + selectRow.

### Debts Paid
- `DEBT-20260426T101948` — action list search Esc fixed (selectRow visibility skip + clearSearch callback)
- `DEBT-20260420T062717` — vim exit kills pane (paid in prior work, not this sprint)

### Debts Deferred
- `DEBT-20260411T100058` — mermaid rendering broken (deferred by ARCHITECT, markdown needs work first)

## Sprint 38: Tab Rename, Drag-Reorder, SVG Button Graphics

**Date:** 2026-04-26

### Agents Participated
- COUNSELOR: led planning, orchestrated all steps, PLAN-tabs.md authoring
- BRAINSTORMER: RFC-tabs.md (pre-flight, prior session)
- Pathfinder: codebase discovery (tab architecture, jam::SVG/XML APIs, config dir SSOT audit)
- Engineer: jam fork (6 files), migration, rename feature, SVG loading, Label refactor, SSOT refactor, Lean splits, audit fixes
- Auditor: full sprint audit (21 findings, all resolved)
- Librarian: jam::SVG/XML research

### Files Modified (32 total)

**jam framework (8 files)**
- `jam_gui/layout/jam_tab_bar_button.h` — NEW: `jam::TabBarButton` with drag-reorder, embedded `juce::Label`, `showRenameEditor()`, `onRenameCommit` callback
- `jam_gui/layout/jam_tab_bar_button.cpp` — NEW: implementation (drag mouse handling, label layout with vertical transform, text truncation, rename editor)
- `jam_gui/layout/jam_tabbed_button_bar.h` — NEW: `jam::TabbedButtonBar` with own `LookAndFeelMethods`, `onTabMoved`/`onTabRightClicked` callbacks
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — NEW: implementation (forked from JUCE, `moveTab` fires callback)
- `jam_gui/layout/jam_tabbed_component.h` — NEW: `jam::TabbedComponent` with content-component management stripped
- `jam_gui/layout/jam_tabbed_component.cpp` — NEW: implementation (simplified `addTab`, `clearTabs`, `changeCallback`)
- `jam_gui/jam_gui.h:95-97` — added 3 tab layout includes
- `jam_gui/jam_gui.cpp:17-19` — added 3 tab layout .cpp includes

**END project (24 files)**
- `Source/component/LookAndFeel.h:38-39,94-115,283-290` — added `jam::TabbedButtonBar::LookAndFeelMethods` inheritance, all jam:: method declarations, SVG path members, `loadTabButtonSvg()`, `drawTabButtonCore` signature
- `Source/component/LookAndFeel.cpp` — SVG loading via `jam::SVG::getPath`, `drawTabButtonCore` with SVG 3-slice path rendering, colour IDs → `jam::`, `getConfigPath()` SSOT
- `Source/component/LookAndFeelTab.cpp` — NEW: extracted tab button drawing methods (drawTabButtonCore, all draw/font/bestWidth overloads)
- `Source/component/LookAndFeelMenu.cpp` — NEW: extracted popup menu drawing methods
- `Source/component/Tabs.h:43,61,202-213,336,392-393` — base class → `jam::TabbedComponent`, orientation types → `jam::`, added `showRenameEditor`/`renameActiveTab`/`popupMenuClickOnTab`
- `Source/component/Tabs.cpp` — `onTabMoved` wiring, `valueChanged` with `userTabName` override, `renameActiveTab`/`showRenameEditor`/`popupMenuClickOnTab` implementations
- `Source/component/TabsActions.cpp` — NEW: extracted config/zoom/split/focus/restore methods
- `Source/component/TabsClose.cpp` — NEW: extracted `closeActiveTab`/`closeSession`
- `Source/AppIdentifier.h:84` — added `userTabName` identifier
- `Source/MainComponent.cpp:128,449-456` — wired `renameTab` callback, orientation refs → `jam::`
- `Source/MainComponentActions.cpp:286-297` — registered `rename_tab` action (modal, category Tabs)
- `Source/lua/Engine.h:298-299,857-859,1152,1180` — `buttonSvg` config field, `renameTab` callback, `keyMappingCount` 23, `rename_tab` key mapping, `getConfigPath()` declaration
- `Source/lua/Engine.cpp:30,72,178-182` — `getConfigPath()` implementation, `rename_tab` Lua API, SSOT config dir replacement
- `Source/lua/EngineParse.cpp:321-322` — `button_svg` parsing
- `Source/lua/EngineDefaults.cpp:284` — SSOT config dir replacement
- `Source/lua/EnginePatch.cpp:26` — SSOT config dir replacement
- `Source/terminal/logic/Session.cpp:64` — SSOT config dir replacement
- `Source/terminal/tty/WindowsTTY.cpp:236` — SSOT config dir replacement
- `Source/Main.cpp:176,512` — SSOT config dir replacement
- `Source/AppState.cpp:509,515,521` — SSOT config dir replacement
- `Source/config/default_keys.lua:76-77` — added `rename_tab = "shift+t"` binding

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (userTabName, showRenameEditor, renameActiveTab, getConfigPath, buttonSvg — all approved)
- [x] MANIFESTO.md principles applied
- [x] Lean splits: LookAndFeel.cpp → 3 files (196/187/100), Tabs.cpp → 3 files (~300/206/118)
- [x] SSOT: getConfigPath() eliminates 11 hardcoded config dir constructions
- [x] Audit: 21 findings, all resolved (C-style casts, dead code, raw new, Lean splits)

### Problems Solved
- Tab rename: userTabName property on TAB node overrides auto-computed displayName unconditionally. Inline label editor on button (not a separate overlay). Right-click, action, and Lua API triggers.
- Tab drag-reorder: live reposition during drag via jam::TabBarButton mouse handling + onTabMoved callback mirroring panes vector and ValueTree.
- SVG tab button: user-provided SVG with 6 named `<g>` groups, paths extracted via jam::SVG::getPath, rendered with theme colours (shape from SVG, colour from END). 3-slice stretching.
- jam fork: juce::TabbedComponent content-component management stripped (END bypasses it). Own LookAndFeelMethods with jam:: types. Label child on TabBarButton with setTransform for vertical orientation.
- SSOT: lua::Engine::getConfigPath() as single source for ~/.config/end/ path.

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 37: Unified Lua Config Engine

**Date:** 2026-04-26

### Agents Participated
- **COUNSELOR:** Claude Sonnet — RFC consumption, PLAN authoring, architecture decisions, delegation, audit orchestration
- **Pathfinder** (x3) — Codebase discovery: Config/Whelmed::Config/Scripting::Engine audit (all call sites, key constants, member inventory), template file content, include dependency mapping
- **Engineer** (x10) — All code: Engine scaffold, 7 Lua templates, writeDefaults, initDefaults, full parse pipeline (EngineParse/EnginePatch), consumer migration (44 files), old file deletion, audit fixes (Lean splits, early return, parsed flag, stale docs)
- **Auditor** (x2) — Step 1 scaffold audit, full sprint audit (24 findings: 2C/5H/10M/7L, all resolved)
- **Explore** (x4) — ActionList Scripting refs, Config include sites, stale reference grep, luaEngine member position

### Files Modified (62 total)

**New (10):**
- `Source/lua/Engine.h` — unified lua::Engine class: 6 typed module structs (Nexus, Display, Whelmed, Keys, Popup, Action), Theme, DisplayCallbacks, PopupCallbacks, SelectionKeys, full public API
- `Source/lua/Engine.cpp` — core lifecycle: constructor, destructor, load, reload, registerApiTable, registerActions, buildKeyMap, fileChanged, END table validation
- `Source/lua/EngineDefaults.cpp` — initDefaults (all colour/platform defaults), writeDefaults (7 template helpers), colourToHex/colourToWhelmedHex
- `Source/lua/EngineConfig.cpp` — parseColour (unified), buildTheme, dpiCorrectedFontSize, getHandler, isClickableExtension
- `Source/lua/EngineParse.cpp` — parseNexus, parseDisplay (6 sub-parsers), parseWhelmed, parseKeys, parseSelectionKeys, parsePopups, parseActions
- `Source/lua/EnginePatch.cpp` — patchKey (targets keys.lua), getActionLuaKey, getShortcutString
- `Source/config/default_nexus.lua` — nexus module template (gpu, daemon, shell, terminal, hyperlinks)
- `Source/config/default_display.lua` — display module template (window, colours, cursor, font, tab, pane, overlay, menu, action_list, status_bar, popup border)
- `Source/config/default_keys.lua` — keys module template (prefix, bindings, selection keys, END-GENERATED marker)
- `Source/config/default_popups.lua` — popups module template (defaults + entries)

**Modified templates (3):**
- `Source/config/default_end.lua` — rewritten as thin require() entry point
- `Source/config/default_actions.lua` — renamed from default_action.lua, api.* instead of display.*
- `Source/config/default_whelmed.lua` — `WHELMED = {` → `return {`

**Migrated consumers (44):**
- `Source/Main.cpp` — lua::Engine luaEngine as first member, rewired onReload
- `Source/MainComponent.h/cpp` — lua::Engine& ref replaces Scripting::Engine unique_ptr + Config cached ref
- `Source/MainComponentActions.cpp` — luaEngine.reload() replaces config.reload() + Whelmed reload
- `Source/AppState.cpp` — all Config calls → typed struct access
- `Source/action/ActionList.h/cpp` — lua::Engine& replaces Scripting::Engine&
- `Source/component/LookAndFeel.h/cpp` — Config + Whelmed calls → lua::Engine
- `Source/component/TerminalDisplay.h/cpp` — Config + Scripting → lua::Engine
- `Source/component/Tabs.cpp` — Config::zoomMin/Max/Step → lua::Engine
- `Source/component/Panes.cpp`, `Popup.h`, `Dialog.h`, `ModalWindow.h`, `StatusBarOverlay.h`, `MessageOverlay.h`, `LoaderOverlay.h` — Config → lua::Engine
- `Source/terminal/data/State.cpp`, `logic/Grid.cpp`, `logic/Session.cpp`, `logic/Mouse.cpp`, `logic/Input.h` — Config/Scripting → lua::Engine
- `Source/terminal/rendering/Screen.h/cpp` — Config → lua::Engine
- `Source/terminal/selection/LinkDetector.h`, `LinkManager.cpp` — Config → lua::Engine
- `Source/nexus/Nexus.cpp` — Config → lua::Engine
- `Source/whelmed/Component.cpp`, `InputHandler.h/cpp`, `Parser.cpp`, `Screen.cpp`, `Tokenizer.cpp` — Whelmed::Config/Scripting → lua::Engine

**Stale docs fixed (18 files):**
- `Source/Cursor.h`, `MainComponent.h/cpp`, `AppState.h`, `Session.h/cpp`, `LookAndFeel.h/cpp`, `TerminalDisplay.h`, `Popup.h`, `Tabs.h`, `MessageOverlay.h`, `Action.h`, `Main.cpp`, `Engine.h`

**Project docs (3):**
- `ARCHITECTURE.md` — lua/ section replaces scripting/ + config/Config, stale refs fixed
- `SPEC.md:121` — Scripting::Engine → lua::Engine
- `PLAN-lua-engine.md` — deleted (objective complete)

**Deleted (13):**
- `Source/config/Config.h`, `Config.cpp`, `WhelmedConfig.h`, `WhelmedConfig.cpp`
- `Source/config/default_action.lua`
- `Source/config/KeyBinding.h`, `KeyBinding.cpp`, `ModalKeyBinding.h`, `ModalKeyBinding.cpp`
- `Source/scripting/Scripting.h`, `Scripting.cpp`, `ScriptingParse.cpp`, `ScriptingPatch.cpp`

### Alignment Check
- [x] BLESSED principles followed — B: lua::Engine sole owner of Lua state; L: functions split to <30 lines, files split to <300 lines; E: typed member access, no string indirection, unified parseColour; S(SSOT): one values store, one colour parser; S(Stateless): no facades; E(Encap): same semantic access pattern; D: one load path
- [x] NAMES.md adhered — six module structs (Nexus, Display, Whelmed, Keys, Popup, Action) approved by ARCHITECT; all field names match Lua table keys
- [x] MANIFESTO.md principles applied — no early returns, positive nesting, RAII lifecycle, `not`/`and`/`or` tokens

### Problems Solved
- Three Lua states unified into one — enables cross-file require(), eliminates duplicate infrastructure
- Whelmed hot-reload gap fixed — any .lua change triggers total reload (whelmed.lua was previously unwatched)
- Divergent colour parsers unified — Config's #RRGGBBAA and Whelmed's bare RRGGBBAA handled by one parseColour
- Status bar font keys moved from colours namespace to status_bar namespace (fixes pre-existing misplacement)
- String-keyed generic getters eliminated — compile-time typed struct access prevents runtime key mismatches
- Old config format detection — unrecognized root keys in END table surface error via MessageOverlay

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 36: Move DWM Platform Code to jam ✅

**Date:** 2026-04-26

### Agents Participated
- COUNSELOR: Analysis, delegation, verification
- Pathfinder: DWM code discovery across END and jam
- Librarian: timeBeginPeriod / JUCE timer research
- Engineer: Code migration

### Files Modified (3 total)
- `Source/Main.cpp` — removed `applyForceDwmRegistry` function, `dwmForceEffectEnabled` constant, redundant `timeBeginPeriod(1)`/`timeEndPeriod(1)` calls, redundant `<windows.h>`/`winmm.lib`/extern declarations; two call sites updated to `jam::BackgroundBlur::applyForceEffectRegistry`
- `jam: jam_style/background_blur/jam_background_blur.h:85-96` — added `applyForceEffectRegistry` declaration to `BackgroundBlur`
- `jam: jam_style/background_blur/jam_background_blur.cpp:340-372` — added `applyForceEffectRegistry` implementation, reuses existing `dwmCornerRound` constant

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- DWM registry hack (`ForceEffectMode`) lived in application entry point instead of jam's DWM plumbing layer — moved to `BackgroundBlur` where all other DWM concerns live
- Redundant `timeBeginPeriod(1)` — JUCE already calls it at static init via `HiResCounterHandler` in `juce_SystemStats_windows.cpp:424`
- Redundant `<windows.h>`, `winmm.lib` pragma, extern declarations — all provided by JUCE

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 35: Unified Build Script + LTO ✅

**Date:** 2026-04-26

### Agents Participated
- COUNSELOR: Analysis, size investigation, delegation
- Pathfinder: Binary size investigation, CMake config discovery
- Engineer: builds.sh creation

### Files Modified (3 total)
- `builds.sh` (new) — unified cross-platform build script replacing both build.bat and install.sh; absorbs MSVC env sourcing (vswhere, vcvarsall, compiler pin, VS cmake/ninja PATH) into bash; default Release, `debug` flag for Debug
- `build.bat` (deleted) — absorbed into builds.sh
- `install.sh` (deleted) — absorbed into builds.sh

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Two separate build entry points (build.bat for Windows, install.sh for cross-platform) with duplicated logic — unified into single builds.sh
- Default build type was Debug, changed to Release (END is a shipped tool, Release is the common case)

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 34: Typeface Architecture Refactor + Emoji/NF Rendering Fix ✅

**Date:** 2026-04-25 — 2026-04-26

### Agents Participated
- **COUNSELOR:** Claude Sonnet — Architecture analysis, ODE investigation, fix delegation, audit orchestration
- **Pathfinder** (via COUNSELOR) — Codebase discovery: NF/emoji render path trace, dispatch table analysis, BinaryData location, font name verification, dependency graph mapping
- **Engineer** (via COUNSELOR) — All code: typeface strip, jam::Font creation, addFallbackFont API, fallback wiring, jam::Font integration across 14 END files, audit fixes, ODE instrumentation/cleanup
- **Auditor** (via COUNSELOR) — Full sprint audit: 15 findings (1C, 5H, 8M, 5L), all resolved
- **Librarian** — Not invoked
- **Oracle** — Not invoked

### Files Modified (2 repos)

**JAM (16 files modified, 2 deleted)**
- `jam_graphics/fonts/jam_typeface.h` — Stripped Registry, Packer, atlas wrappers, NF members, isMonospace. Added `addFallbackFont(const void*, int)`, `userFallbackFonts` member, `emojiFontFamily` member, `getUserFamily()`. Fixed `&&`→`and` in Metrics::isValid
- `jam_graphics/fonts/jam_typeface.mm` — Stripped registerEmbeddedFonts, discoverEmojiFont, rasterizeToImage, BinaryData include, NF loading, Registry. Constructor accepts emojiFontFamily. Added addFallbackFont with CGDataProvider→CGFont→CTFont path. Modified shapeFallback with user fallback loop. Destructor handles userFallbackFonts cleanup (skip aliased cache entries). setSize resizes userFallbackFonts
- `jam_graphics/fonts/jam_typeface.cpp` — Same strip as .mm for FreeType path. Added addFallbackFont stub
- `jam_graphics/fonts/jam_typeface_shaping.cpp` — Removed `isMonospace` check. Fixed stale `jreng_*` doxygen filenames, removed `@see Registry`, `[]`→`insert_or_assign`
- `jam_graphics/fonts/jam_font.h` — Rewritten: new Font value type with fluent builders, `using Style = jam::Typeface::Style`, findSuitableFontForText
- `jam_graphics/fonts/jam_font.cpp` — Rewritten: findSuitableFontForText delegates to juce::Font
- `jam_graphics/fonts/jam_text_layout.h` — Fixed Font construction in draw template, CPU draw takes Packer&. Fixed stale `@file`
- `jam_graphics/fonts/jam_glyph_packer.cpp` — Replaced `jam::Typeface::ftFixedScale` with file-scope static. Fixed stale `@file`
- `jam_graphics/rendering/jam_glyph_graphics_context.h` — setPacker, uploadStagedBitmaps takes Packer&
- `jam_graphics/rendering/jam_glyph_graphics_context.cpp` — Same
- `jam_gui/opengl/renderers/jam_gl_context.h` — Packer& instead of Typeface&
- `jam_gui/opengl/renderers/jam_gl_context.cpp` — Same
- `jam_gui/opengl/context/jam_gl_atlas_renderer.h` — Takes Packer& instead of Typeface&
- `jam_gui/opengl/context/jam_gl_atlas_renderer.cpp` — Same
- `jam_gui/opengl/context/jam_gl_graphics.cpp` — getHeight() instead of getSize(), getResolvedTypeface()
- `jam_core/binary_data/jam_binary_data.h` — Added BinaryData::createTypeface
- ~~`jam_graphics/fonts/jam_typeface_registry.cpp`~~ — DELETED
- ~~`jam_graphics/fonts/jam_typeface_registry.mm`~~ — DELETED

**END (18 files modified)**
- `Source/Main.cpp` — Removed DisplayMono/DisplayProp/NerdFont structs, removed BinaryData.h and JamFontsBinaryData.h includes, removed Log::Scope
- `Source/MainComponent.h` — `std::shared_ptr<jam::Typeface> typeface` → `jam::Font font`
- `Source/MainComponent.cpp` — Constructor: Typeface built in body block, Font assembled via withResolvedTypeface. Display Mono + NF registered as fallback fonts. All `typeface->` → `font.getResolvedTypeface()->`. Font sync on setFontFamily/setFontSize
- `Source/terminal/logic/Processor.h` — `createDisplay(jam::Typeface&)` → `createDisplay(jam::Font&)`
- `Source/terminal/logic/Processor.cpp` — Signature update
- `Source/terminal/rendering/Screen.h` — Constructor and member: `jam::Typeface&` → `jam::Font&`
- `Source/terminal/rendering/Screen.cpp` — Constructor, calcMetrics/setSize via getResolvedTypeface()
- `Source/terminal/rendering/ScreenRender.cpp` — All shaping/font handle calls via getResolvedTypeface()
- `Source/terminal/rendering/ScreenSnapshot.cpp` — Same
- `Source/component/TerminalDisplay.h` — Constructor and member: `jam::Typeface&` → `jam::Font&`
- `Source/component/TerminalDisplay.cpp` — Constructor, calcMetrics via getResolvedTypeface()
- `Source/component/Tabs.h` — Constructor and member: `jam::Typeface&` → `jam::Font&`
- `Source/component/Tabs.cpp` — Constructor update
- `Source/component/Panes.h` — Constructor, cellsFromRect, member: `jam::Typeface&` → `jam::Font&`
- `Source/component/Panes.cpp` — Constructor, cellsFromRect via getResolvedTypeface()
- `test/emoji_test.sh` — Shebang `#!/usr/bin/env bash` → `#!/bin/zsh`
- `ARCHITECTURE.md` — Font Architecture section updated
- `carol/SPRINT-LOG.md` — This entry

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **NF icons not rendering:** `CTFontCreateForString` returns LastResort for PUA codepoints. Fix: `addFallbackFont` API creates CTFontRef directly from binary data via CGDataProvider path, bypassing name-based CoreText lookup
- **Emoji not rendering:** Test script shebang `#!/usr/bin/env bash` → macOS bash 3.2 lacks `\U` printf support → literal text output. Fix: shebang to `#!/bin/zsh`
- **E+000/E+001 from wrong font:** Display Mono registered as first fallback (before NF) ensures reserved PUA glyphs always come from Display Mono
- **Typeface god object:** Stripped Registry, Packer, atlas delegation, font discovery, BinaryData references from jam::Typeface. Now: font handle management only
- **Module boundary violation:** Module (`jam_graphics`) no longer includes project-level BinaryData or knows about NF fonts
- **jam::Font not consumed:** Wired jam::Font as the renderer interface throughout END, mirroring juce::Font/Typeface relationship
- **User fallback font leak:** Destructor skips aliased fallbackFontCache entries, releases userFallbackFonts separately
- **setSize zoom bug:** userFallbackFonts resized via CTFontCreateCopyWithAttributes on zoom

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 33: MSVC Build Regression Fixes + Audit ✅

**Date:** 2026-04-25

### Agents Participated
- **COUNSELOR:** Claude Sonnet — Build error analysis, audit orchestration, fix delegation
- **Pathfinder** (via COUNSELOR) — CMakeLists diff, KeyMapping git history, brace-init pattern inventory, private static member access investigation
- **Engineer** (via COUNSELOR) — All code fixes: brace-init MSVC ambiguity, KeyMapping refactor, uint8 qualification, taper narrowing, audit resolution (early returns, anonymous namespace, enum class, boolean flag, std::array, formatting)
- **Auditor** (via COUNSELOR) — Full session audit: 13 findings across END and jam, all resolved

### Files Modified (2 repos)

**END (5 files modified)**
- `Source/scripting/Scripting.h:245-270` — KeyMapping: removed raw pointer accessors, refactored to `static constexpr std::array<KeyMapping, 22> keyMappings` inline; fixed `} //` formatting
- `Source/scripting/Scripting.cpp:149,210,247` — `jam::lua::error` brace-init via `.get<jam::lua::error>()`; fixed `} //` formatting
- `Source/scripting/ScriptingParse.cpp:68-238` — 21 sites: `.get<jam::lua::optional<T>>()` on sol table proxy brace-init; early return fix in `parse` lambda
- `Source/scripting/ScriptingPatch.cpp:121-168` — `keyMappings` direct access via `.at()`; early return fix in `getShortcutString()`

**JAM (3 files modified)**
- `jam_style/system_colour/jam_system_colour.cpp:13-55` — `uint8` → `juce::uint8`; refactored 5 if-return branches to lookup table with `static` helper; eliminated DRY violation
- `jam_core/utilities/jam_taper.h:15-242` — plain enum → `enum class Type`; early returns → single exit point in 5 functions; copy-assignment → brace-init; `[]` → `.at()`; fixed latent bug in `inverseAnti()` map key formula
- `jam_data_structures/parameter/jam_parameter_layout.cpp:17-113` — Updated `Taper::Type::` enum class references; early return fix in `getParameterName`; eliminated `isUsingDecibelTaper` boolean flag

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- MSVC C2440 brace-init ambiguity: sol table proxy → `jam::lua::optional<T>` and `jam::lua::error` — resolved via explicit `.get<>()`
- MSVC C2248 private nested type at file scope — resolved by refactoring to `static constexpr` inline member
- MSVC C2061 unqualified `uint8` — resolved with `juce::` prefix
- MSVC C4244 double→float narrowing in taper lambdas — resolved with `static_cast<FloatType>`
- Latent bug: `inverseAnti()` used wrong map key formula `(logTaper/5)-1` instead of enum key directly

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 32: Lua-Scriptable Custom Actions + Action Binding Migration ✅

**Date:** 2026-04-24

### Agents Participated
- **COUNSELOR:** Claude Sonnet — RFC analysis, PLAN authoring, migration orchestration, audit resolution, doc updates
- **Pathfinder** (via COUNSELOR) — Codebase surveys: Config schema patterns, Action::Registry API, splitRect/PaneManager ratio flow, ActionList remap wiring, file watcher status
- **Engineer** (via COUNSELOR) — All code implementation: Config key additions, splitActiveWithRatio extraction, Scripting::Engine subsystem, default_action.lua template, Config/Action migration, MainComponent wiring, Input/Whelmed selection key migration, file watcher, auto-reload callbacks, PaneManager ratio fix, audit fixes (early return, file split, rename)
- **Auditor** (via COUNSELOR) — Full sprint audit: 14 findings (4 high, 9 medium, 1 low), all resolved

### Files Modified (2 repos)

**END (26 files — 4 created, 22 modified)**
- `Source/scripting/Scripting.h` — NEW: Engine class, Context\<Engine\>, DisplayCallbacks/PopupCallbacks/SelectionKeys structs, KeyMapping static table
- `Source/scripting/Scripting.cpp` — NEW: Engine core (load, registerActions, buildKeyMap, fileChanged watcher callback)
- `Source/scripting/ScriptingParse.cpp` — NEW: parseKeys, parsePopups, parseActions, parseSelectionKeys, writeDefaults
- `Source/scripting/ScriptingPatch.cpp` — NEW: patchKey (action.lua file patching), getActionLuaKey, getShortcutString
- `Source/config/default_action.lua` — NEW: default action.lua template with platform placeholders, comprehensive docs, OOTB split-thirds examples
- `Source/config/Config.h` — Added autoReload, actionListPosition, statusBarPosition keys; removed ~40 keys* constants, PopupEntry, getPopups
- `Source/config/Config.cpp` — Added auto_reload/relocated schema entries; removed keys/popups schema, loadPopups, clearPopups
- `Source/config/default_end.lua` — Added auto_reload + status_bar sections; removed keys + popups sections; relocated action_list_position
- `Source/action/Action.h` — Added Binding struct; changed buildKeyMap to accept external bindings; removed configKeyForAction, reload
- `Source/action/Action.cpp` — Removed actionKeyTable, anonymous namespace, Config dependency; rewrote buildKeyMap
- `Source/action/ActionList.cpp` — Migrated from Config to Scripting::Engine (patchKey, getShortcutString, getPrefixString)
- `Source/action/ActionRow.cpp` — Migrated configKeyForAction to Engine::getActionLuaKey
- `Source/MainComponent.h` — Added Scripting::Engine member
- `Source/MainComponent.cpp` — Engine construction, DisplayCallbacks/PopupCallbacks wiring, onActionReload/onConfigReload with RELOADED message
- `Source/MainComponentActions.cpp` — Removed registerPopupActions; wired Engine::registerActions + buildKeyMap
- `Source/component/Panes.h` — Added splitActiveWithRatio (public), ratio param on splitAt + splitActive
- `Source/component/Panes.cpp` — splitActiveWithRatio impl, ratio threaded through splitActive → splitAt → PaneManager
- `Source/component/Tabs.h` — Added splitActiveWithRatio forwarding
- `Source/component/Tabs.cpp` — splitActiveWithRatio impl
- `Source/terminal/logic/Input.h` — buildKeyMap accepts Scripting::Engine::SelectionKeys
- `Source/terminal/logic/Input.cpp` — Migrated from Config reads to Engine selection keys
- `Source/component/TerminalDisplay.cpp` — Passes Engine::getSelectionKeys() to Input::buildKeyMap
- `Source/whelmed/InputHandler.h` — buildKeyMap accepts Scripting::Engine::SelectionKeys
- `Source/whelmed/InputHandler.cpp` — Migrated from Config reads to Engine selection keys
- `Source/whelmed/Component.cpp` — Passes Engine::getSelectionKeys() to InputHandler::buildKeyMap
- `ARCHITECTURE.md` — Updated module map, inventory, action registry section for Scripting::Engine
- `SPEC.md` — Updated keybinding + popup sections to reference action.lua

**JAM (2 files)**
- `jam_gui/layout/jam_pane_manager.h` — Added ratio parameter to split() (default 0.5)
- `jam_gui/layout/jam_pane_manager.cpp` — Uses ratio parameter instead of hardcoded 0.5

### Alignment Check
- [x] BLESSED — B: Engine owns Lua state + watcher (RAII). L: Scripting split into 3 files under 300 lines. E: Callbacks structs explicit, no early returns. S: action.lua is SSOT for all bindings. S: Engine stateless beyond Lua VM + parsed list. E: Lua state private, clean API boundary. D: deterministic load order.
- [x] NAMES.md — No improvised names. displayCallbacks, popupCallbacks, keyMappings, selectionKeys all semantic.
- [x] MANIFESTO.md — No pattern invention. Follows existing Config/Action/Context patterns.

### Problems Solved
- **Lua-scriptable custom actions** — users define actions in `~/.config/end/action.lua` composed from display API (split_with_ratio, tabs, pane focus)
- **Action binding SSOT** — all keybindings, popups, and custom actions in one file instead of split across end.lua Config + Action::Registry
- **Custom split ratios** — splitActiveWithRatio enables arbitrary pane ratios (e.g. equal thirds via 0.333 + 0.5)
- **PaneManager ratio bug** — ratio parameter was lost at PaneManager::split (hardcoded 0.5), now propagated from Lua through to ValueTree
- **Auto-reload** — jam::File::Watcher hot-reloads action.lua and end.lua on save, gated by auto_reload config key
- **Platform-conditional defaults** — default_action.lua uses %%placeholder%% substitution for macOS cmd vs Linux/Windows ctrl

### Debts Paid
None.

### Debts Deferred
None. Zero-debt rule observed.

**Status:** ✅ Complete — Lua scripting system operational, all audit findings resolved

---

## Sprint 31: JAM Namespace Conformance to Kuassa Convention ✅

**Date:** 2026-04-23

### Agents Participated
- COUNSELOR: divergence analysis, plan, delegation, verification
- Pathfinder: codebase survey (jam, kuassa, END), namespace spread analysis, binary data config discovery
- Oracle: deep file-by-file diff of kuassa vs jam modules, ModalWindow/style::Manager/MapType comparison
- Engineer: namespace lowercasing, GL re-namespace, ModalWindow cleanup, applyFontsTo restore, MapType import, BinaryCodec struct conversion, END reference updates, stale doxygen sweep
- Auditor: Phase 1 verification, final 7-point audit

### Files Modified (2 repos, ~112 files)

**JAM (87 files)**
- `Jam.cmake:34` — NAMESPACE "jam::Fonts" → "jam::fonts"
- `jam_animation/common/jam_animation_common.h` — namespace Animation → animation
- `jam_animation/scrolling_text/jam_scrolling_text.h` — namespace Animation → animation
- `jam_animation/scrolling_text/jam_scrolling_text.cpp` — namespace Animation → animation
- `jam_graphics/colours/jam_colours_names.h` — namespace Colours → colours
- `jam_graphics/colours/jam_colours_names.cpp` — namespace Colours → colours
- `jam_graphics/colours/jam_colours_utilities.h` — namespace Colours → colours
- `jam_graphics/graphics/jam_graphics_fader.h` — namespace Graphics → graphics
- `jam_graphics/graphics/jam_graphics_perimeter.h` — namespace Graphics → graphics
- `jam_graphics/graphics/jam_graphics_segment.h` — namespace Graphics → graphics
- `jam_graphics/graphics/jam_graphics_rotary.h` — namespace Graphics → graphics
- `jam_graphics/graphics/jam_graphics_rotary.cpp` — namespace Graphics → graphics
- `jam_graphics/graphics/jam_graphics_utilities.h` — namespace Graphics → graphics
- `jam_graphics/utilities/jam_graphics_utilities.h` — namespace Graphics → graphics
- `jam_graphics/vignette/jam_graphics_vignette.h` — namespace Graphics → graphics
- `jam_graphics/rendering/jam_graphics_atlas.h` — doxygen GL refs updated
- `jam_graphics/glyph/jam_glyph_key.h` — doxygen updated
- `jam_graphics/glyph/jam_atlas_glyph.h` — doxygen updated
- `jam_graphics/glyph/jam_atlas_packer.h` — doxygen updated
- `jam_graphics/fonts/jam_typeface.cpp` — jam::Fonts → jam::fonts
- `jam_graphics/fonts/jam_typeface.mm` — jam::Fonts → jam::fonts
- `jam_graphics/fonts/jam_atlas_packer.h` — doxygen updated
- `jam_graphics/fonts/jam_glyph_key.h` — doxygen updated
- `jam_graphics/fonts/jam_atlas_glyph.h` — doxygen updated
- `jam_graphics/fonts/jam_box_drawing.h` — doxygen updated
- `jam_graphics/fonts/jam_lru_glyph_cache.h` — doxygen updated
- `jam_graphics/fonts/jam_text_layout.h` — doxygen updated
- `jam_graphics/blur/background_blur/jam_background_blur.cpp` — doxygen updated
- `jam_gui/menu/jam_menu.h` — namespace Menu → menu, self-refs Menu:: → menu::
- `jam_gui/mouse/jam_mouse_enter_parent.h` — namespace Mouse → mouse
- `jam_gui/mouse/jam_mouse_events.h` — namespace Mouse → mouse
- `jam_gui/style_window/jam_style_window.h` — namespace Style::Window → style::window
- `jam_gui/style_window/jam_style_window.cpp` — namespace Style::Window → style::window
- `jam_gui/style_window/jam_style_window.mm` — namespace Style::Window → style::window
- `jam_gui/window/jam_window.h` — GL forward decls → gl::, doxygen updated
- `jam_gui/window/jam_window.cpp` — gl:: namespace refs
- `jam_gui/window/jam_modal_window.h` — removed style::getSafeMetrics ctor, doxygen updated
- `jam_gui/window/jam_modal_window.cpp` — removed style::getSafeMetrics ctor impl
- `jam_gui/opengl/context/jam_gl_renderer.h` — GLRenderer → gl::Renderer
- `jam_gui/opengl/context/jam_gl_renderer.cpp` — gl:: namespace
- `jam_gui/opengl/context/jam_gl_renderer_mac.mm` — gl:: namespace
- `jam_gui/opengl/context/jam_gl_component.h` — GLComponent → gl::Component, setGLRenderer → setRenderer
- `jam_gui/opengl/context/jam_gl_component.cpp` — gl:: namespace
- `jam_gui/opengl/context/jam_gl_graphics.h` — GLGraphics → gl::Graphics
- `jam_gui/opengl/context/jam_gl_graphics.cpp` — gl:: namespace
- `jam_gui/opengl/context/jam_gl_vertex_layout.h` — GLVertexLayout → struct gl::VertexLayout
- `jam_gui/opengl/context/jam_gl_vertex_layout.cpp` — gl:: namespace
- `jam_gui/opengl/context/jam_gl_shader_compiler.h` — GLShaderCompiler → struct gl::ShaderCompiler
- `jam_gui/opengl/context/jam_gl_shader_compiler.cpp` — gl:: namespace
- `jam_gui/opengl/context/jam_glyph_atlas.h` — GlyphAtlas → gl::GlyphAtlas
- `jam_gui/opengl/context/jam_glyph_atlas.cpp` — gl:: namespace
- `jam_gui/opengl/context/jam_gl_atlas_renderer.h` — gl:: type refs
- `jam_gui/opengl/context/jam_gl_atlas_renderer.cpp` — gl:: type refs
- `jam_gui/opengl/context/jam_native_context_resource.h` — gl::Component, doxygen updated
- `jam_gui/opengl/context/jam_gl_overlay.h` — gl::Renderer
- `jam_gui/opengl/context/jam_native_shared_context_owner.h` — doxygen updated
- `jam_gui/opengl/renderers/jam_gl_path.h` — GLVertex → gl::Vertex, GLTexVertex → gl::TexVertex
- `jam_gui/opengl/renderers/jam_gl_path.cpp` — gl:: refs
- `jam_gui/opengl/renderers/jam_gl_vignette.h` — gl::Component, gl::Graphics
- `jam_gui/opengl/renderers/jam_gl_context.h` — gl::GlyphAtlas, doxygen updated
- `jam_gui/opengl/renderers/jam_gl_context.cpp` — gl:: refs
- `jam_gui/opengl/renderers/jam_glyph_shaders.h` — namespace Glyph::Shaders → struct Shaders
- `jam_gui/opengl/component/jam_gl_frequency_grid.h` — gl::Component, gl::Graphics
- `jam_gui/component/comboBox/jam_comboBox.h` — Menu:: → menu::
- `jam_gui/jam_gui.h` — include comment updated
- `jam_gui/jam_gui.cpp` — include comment updated
- `jam_core/map/jam_map.h` — namespace Map → struct Map with static methods
- `jam_core/binary_codec/jam_binary_codec.h` — namespace binaryCodec → struct BinaryCodec
- `jam_core/binary_codec/jam_binary_codec.cpp` — struct BinaryCodec:: method defs
- `jam_core/binary_data/jam_binary_data.h` — doc comment updated
- `jam_core/function_map/jam_function_map.h` — added TypeEntry + MapType from kuassa
- `jam_data_structures/style_manager/jam_style_manager.h` — namespace Style → style, added applyFontsTo
- `jam_data_structures/style_manager/jam_style_manager.cpp` — style:: refs, applyFontsTo impl
- `jam_fonts/segment/jam_fonts_segment.h` — namespace Fonts::Segment → fonts::segment
- `jam_fonts/segment/jam_fonts_segment.cpp` — namespace fonts::segment
- `jam_debug/console/jam_console.cpp` — jam::Fonts → jam::fonts
- `jam_look_and_feel/graphics/knob_v1/jam_look_and_feel_graphics_knob_v1.h` — style::Manager
- `jam_look_and_feel/graphics/knob_v1/jam_look_and_feel_graphics_knob_v1.cpp` — style::, colours::
- `jam_look_and_feel/graphics/toggle_push/jam_look_and_feel_graphics_toggle_push.cpp` — colours::
- `jam_look_and_feel/graphics/toggle_slide/jam_look_and_feel_graphics_toggle_slide.cpp` — colours::
- `jam_look_and_feel/popup_text_box/jam_look_and_feel_popup_text_box.h` — style::getSafeFont
- `jam_look_and_feel/seven_segment/jam_look_and_feel_seven_segment.h` — style::, fonts::segment
- `jam_look_and_feel/seven_segment/jam_look_and_feel_seven_segment.cpp` — style::Manager
- `jam_look_and_feel/theme/jam_look_and_feel_theme.h` — style::Manager
- `jam_look_and_feel/theme/jam_look_and_feel_theme.cpp` — style::, style::window::
- `jam_look_and_feel/vari_display/jam_vari_display.h` — style::, fonts::segment
- `jam_look_and_feel/vari_display/jam_vari_display.cpp` — style::Manager

**END (25 files)**
- `Source/Main.cpp:552-568` — jam::Fonts → jam::fonts
- `Source/MainComponent.h` — jam::gl::GlyphAtlas
- `Source/MainComponent.cpp` — jam::gl::Component
- `Source/MainComponentActions.cpp` — jam::gl::Renderer
- `Source/interprocess/EncoderDecoder.h` — using Codec = jam::BinaryCodec
- `Source/interprocess/Channel.cpp` — Codec:: prefix
- `Source/interprocess/Daemon.cpp` — Codec:: prefix
- `Source/interprocess/Link.cpp` — Codec:: prefix
- `Source/component/PaneComponent.h` — jam::gl::Component, doxygen
- `Source/component/Popup.h` — jam::gl::Renderer, doxygen
- `Source/component/Popup.cpp` — jam::gl::Renderer
- `Source/component/ModalWindow.h` — jam::gl::Renderer
- `Source/component/ModalWindow.cpp` — jam::gl::Renderer
- `Source/component/TerminalDisplay.h` — jam::gl::GlyphAtlas, doxygen
- `Source/component/TerminalDisplay.cpp` — jam::gl::GlyphAtlas
- `Source/component/Tabs.h` — jam::gl::GlyphAtlas
- `Source/component/Tabs.cpp` — jam::gl::GlyphAtlas
- `Source/component/Panes.h` — jam::gl::GlyphAtlas
- `Source/component/Panes.cpp` — jam::gl::GlyphAtlas
- `Source/terminal/logic/Processor.h` — jam::gl::GlyphAtlas
- `Source/terminal/logic/Processor.cpp` — jam::gl::GlyphAtlas
- `Source/terminal/rendering/Screen.h` — doxygen updated
- `Source/terminal/rendering/Screen.cpp` — doxygen updated
- `Source/terminal/rendering/ScreenRender.cpp` — doxygen updated
- `Source/terminal/rendering/ScreenSnapshot.cpp` — (no changes, already clean)

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered — namespace lowercase, struct/class PascalCase
- [x] MANIFESTO.md principles applied

### Problems Solved
- JAM sub-namespaces diverged from kuassa upstream (PascalCase vs lowercase) — conformed all to lowercase
- GL classes used flat GL-prefix naming instead of kuassa's `gl::` namespace — re-namespaced to `jam::gl::`
- Closed single-block namespaces (Map, Shaders, BinaryCodec, ShaderCompiler, VertexLayout) — converted to structs with static methods per contract
- ModalWindow coupled to style::Manager via getSafeMetrics ctor — removed, caller provides explicit params
- JAM missing `applyFontsTo` from kuassa — restored
- JAM missing `MapType`/`TypeEntry` from kuassa — imported
- BinaryCodec struct `using` declarations invalid in END — replaced with type alias + qualified calls
- `.mm` file missed in Phase 1 namespace rename — fixed
- `Menu::` self-references in jam_menu.h and jam_comboBox.h not updated — fixed
- JUCE binary data namespace `jam::Fonts` not lowercased — updated Jam.cmake + all call sites

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 30: Windows MSVC Build Regression Fix ✅

**Date:** 2026-04-21

### Agents Participated
- COUNSELOR: diagnosis, planning, delegation
- Pathfinder: Windows macro collision analysis, Kuassa pattern discovery, JUCE module include-order tracing
- Engineer: code edits across jam and END

### Files Modified (2 repos)

**JAM (11 files)**
- `Jam.cmake:42-59` — MSVC per-module warning suppression (/wd4005, /wd4804, /wd4297) + global /wd4661
- `jam_core/utilities/jam_platform.h:15-30` — NOMINMAX + #undef interface/small/ALTERNATE/MessageBox (single protection point)
- `jam_core/identifier/jam_identifier.h:54-57` — removed push/pop block, macros dead globally via jam_platform.h
- `jam_graphics/fonts/jam_typeface.cpp:69-73` — surgical #define interface before dwrite_2.h, #undef after
- `jam_graphics/blur/background_blur/jam_background_blur.cpp:1-4` — removed redundant <windows.h> + manual undefs, uses jam_platform.h
- `jam_gui/style_window/jam_style_window.cpp:1-3` — same as above
- `jam_gui/component/browser/jam_native_file_chooser.cpp:3-6` — jam_platform.h + surgical #define interface for shlobj.h
- `jam_core/gpu_probe/jam_gpu_probe.cpp:15` — <windows.h> → jam_platform.h
- `jam_debug/console/jam_console.cpp:25,46,182` — #ifdef → #if JUCE_WINDOWS (3 sites)
- `jam_tui/input/jam_tui_input.h:6-10` — NOMINMAX block → jam_platform.h
- `jam_tui/input/jam_tui_input.cpp:11-13` — <windows.h> → jam_platform.h
- `jam_tui/metrics/jam_tui_metrics.h:8-13` — NOMINMAX block → jam_platform.h

**END (2 files)**
- `CMakeLists.txt:286-291` — if(NOT MSVC) guard on -Wno-deprecated-declarations
- `CMakeLists.txt:293-300` — if(NOT MSVC) guard on -Wno-macro-redefined

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- MSVC C2236: `interface` macro (`#define interface struct`) colliding with jam X-macro identifier `X(interface, "interface")` — killed globally in jam_platform.h, surgically re-enabled for DWrite/shlobj includes
- MSVC C4003: `min`/`max` macros colliding with jam struct fields — NOMINMAX in jam_platform.h
- MSVC C4141: double `__declspec(novtable)` from `#define interface struct __declspec(novtable)` — simplified to `#define interface struct`
- MSVC C2653: `MessageBox` macro clobbering `jam::MessageBox` namespace — added #undef MessageBox
- MSVC D8021: clang-only flags (-Wno-macro-redefined, -Wno-deprecated-declarations) rejected by cl.exe — guarded with if(NOT MSVC)
- Inconsistent Windows header protection (3 patterns: NOMINMAX, post-include #undef, none) — unified to single jam_platform.h protection point
- MSVC vendored-code warnings (C4005, C4804, C4297, C4661) — suppressed in Jam.cmake, available to all consumers

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 29: Release Build Fix + jam_fonts/jam_debug Module Extraction ✅

**Date:** 2026-04-21

### Agents Participated
- COUNSELOR: planning, delegation, namespace collision diagnosis
- Pathfinder: JAM module structure survey, font reference discovery
- Engineer: module creation, file moves, code edits, warning suppression

### Files Modified (2 repos)

**END (9 files)**
- `CMakeLists.txt:268-279` — added jam_fonts, jam_debug to jam_add_modules
- `CMakeLists.txt:285-296` — set_source_files_properties for jam_lua, jam_freetype warning suppression
- `CMakeLists.txt:422` — GL_SILENCE_DEPRECATION (Darwin-scoped)
- `Source/Main.cpp:47` — added #include <JamFontsBinaryData.h>
- `Source/Main.cpp:551-567` — BinaryData::Display* → jam::Fonts::Display*
- `Source/fonts/Display-{Book,Medium,Bold}.ttf` — moved to jam_fonts/display/
- `Source/fonts/DisplayMono-{Book,Medium,Bold}.ttf` — moved to jam_fonts/display_mono/
- `Source/interprocess/EncoderDecoder.h` — replaced with `using jam::BinaryCodec::` re-exports, `#include <JuceHeader.h>`
- `Source/interprocess/EncoderDecoder.cpp` — stripped to `encodePdu` only

**JAM (16 files)**
- `Jam.cmake:28-39` — auto-glob TTF/OTF → juce_add_binary_data (namespace jam::Fonts, header JamFontsBinaryData.h)
- `jam_core/jam_core.h:77` — removed #if JUCE_DEBUG guard from debug include, then removed debug include entirely
- `jam_core/jam_core.cpp:16-19` — removed debug/jam_log.cpp + debug/jam_debug.cpp includes
- `jam_core/debug/` — entire directory deleted (moved to jam_debug)
- `jam_core/gpu_probe/jam_gpu_probe.mm:14` — #ifndef guard on GL_SILENCE_DEPRECATION
- `jam_fonts/jam_fonts.h` — removed open_sans include
- `jam_fonts/jam_fonts.cpp` — removed open_sans include
- `jam_fonts/open_sans/` — deleted (2 files)
- `jam_fonts/display_mono/` — created with DisplayMono{Book,Medium,Bold}.ttf
- `jam_fonts/display/` — created with Display{Book,Medium,Bold}.ttf
- `jam_debug/` — new module: jam_debug.h, jam_debug.cpp, jam_log.cpp, build_info/, console/, valueTree_monitor/
- `jam_debug/console/jam_console.cpp:37,58-64` — JamFontsBinaryData.h include + jam::Fonts::DisplayMonoBook for console font
- `jam_graphics/fonts/jam_typeface.cpp:76,248-259` — JamFontsBinaryData.h include + BinaryData::DisplayMono → jam::Fonts::
- `jam_graphics/fonts/jam_typeface.mm:43,232-234,252` — JamFontsBinaryData.h include + jam::Fonts:: namespace + CTFontManager pragma suppression
- `jam_core/binary_codec/jam_binary_codec.h` — created, 9 read/write primitives in jam::BinaryCodec
- `jam_core/binary_codec/jam_binary_codec.cpp` — created, implementation
- `jam_core/jam_core.h` — added binary_codec include
- `jam_core/jam_core.cpp` — added binary_codec amalgamation include

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (jam::Fonts — established namespace)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Release build failure: jam_core.h gated jam_debug.h include on JUCE_DEBUG, hiding Log struct from jam_log.cpp
- Release build failure: jam_core/debug/fonts/ binary data CPPs included inside JUCE_DEBUG guard in jam_debug.cpp
- Linker error: BinaryData::SymbolsNerdFontRegular_ttf not found due to two BinaryData.h headers (namespace collision) — resolved with JamFontsBinaryData.h
- Linker error: jam::Fonts::InputMono* symbols missing after font file deletion
- Module extraction: debug instruments (console, log, VTM) isolated from jam_core into standalone jam_debug module
- Module extraction: Display/DisplayMono fonts moved from END binary data to jam_fonts module with CMake auto-glob
- Warning suppression: GL_SILENCE_DEPRECATION, CTFontManager, vendored Lua tmpnam, vendored FreeType ONE_PIXEL
- Extracted jam::BinaryCodec to jam_core — 9 binary serialization primitives from END's Interprocess::EncoderDecoder

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 28: Post-JAM Stabilization — Atlas Symmetry, Context Dual-Impl, Style::Manager Drop, Phase 4 Cleanup ✅

**Date:** 2026-04-21
**Duration:** ~full session (multi-day span; session ran through multiple compacts)

### Agents Participated
- COUNSELOR: session lead, multiple Decision Gate cycles, audit orchestration, clean-sweep direction, commit message draft
- Pathfinder: 12+ surveys — reload-chain investigation (pre-compact), window race tracing, DEC 1049 site analysis, Parser↔State topology map, Context jam-vs-kuassa diff, Config surface + consumer map, Style::Manager surface, BackgroundBlur glass path, GlyphAtlas↔Context thread analysis, jam project-type macro check, popup/modal setupWindow chain, shell-exit VBlank UAF, Tabs addNewTab quit chain
- Engineer: 20+ execution passes — saveCursor/restoreCursor extraction, ?1049 dispatch, resize→AppState, clean-sweep batches (ODE drain, forwarder refactor, setWindowSize dedup, doc drift), Dialog + whelmed Config migrations, Main.cpp Style::Manager drop (245 lines), DisplayMono/Prop restoration, EndColourId.cpp deletion + option 2a setColourIdMap migration (jam + kuassa), jam_context.h dual-impl, GlyphAtlas Context drop + reference threading (18 files), GraphicsAtlas extraction + symmetric Renderer::Atlas alias, Option 5 default-walker relocation, ModalWindow opacity+blur ctor overload + delegation, Session shell-exit wire, explicit markAtlasDirty restore, Phase 4 module deletion, many surgical doc/stub fixes
- Auditor: 2 passes — mid-session (M1-M7, Mi1-Mi7) and final (B1, M1-M5, m1-m10) — drove multiple clean-sweep rounds to resolution
- ARCHITECT: locked BLESSED-proper direction at every Decision Gate, rejected invented abstractions, redirected from name-invention multiple times, corrected thread_local-context assumption, chose option 2a for ColourIdMap migration, chose option B for TextLayout atlas injection, chose symmetric-atlas (path B) over Typeface re-absorption

### Files Modified (major)

**END Source:**
- `Source/MainComponent.cpp` / `.h` — setWindowSize at resized() (SSOT), setRenderer getRenderer→setRenderables forwarder, markAtlasDirty restoration, GraphicsAtlas member, doc drift cleanups, dead setCloseCallback removal
- `Source/Main.cpp` — rebuildStyleManager deleted (~245 lines), DisplayMono/DisplayProp structs restored, shell-exit onAllSessionsExited wire added then reverted (double-quit)
- `Source/component/Dialog.cpp` / `.h` — Font::getStringWidthFloat → juce::TextLayout::getStringWidth, Config-direct migration, stale setupWindow doc rewrite
- `Source/component/LookAndFeel.cpp` — Style::Manager consumers → Config direct, WhelmedConfig include
- `Source/component/Tabs.cpp` / `.h`, `Panes.cpp` / `.h`, `TerminalDisplay.cpp` / `.h` — GraphicsAtlas threaded through ctor chain
- `Source/component/ModalWindow.cpp` — Terminal::ModalWindow passes Config opacity/blur to new jam overload, doc trimmed
- `Source/terminal/logic/Parser.h` — saveCursor/restoreCursor method decls
- `Source/terminal/logic/ParserESC.cpp` — method impls + ESC 7/8 updates + anonymous namespace → static
- `Source/terminal/logic/ParserCSI.cpp` — ?47/?1047/?1049 dispatch differentiation, anonymous namespace → static
- `Source/terminal/logic/Processor.h` / `.cpp` — createDisplay gains GraphicsAtlas&
- `Source/terminal/logic/Session.cpp` — tty->onExit invokes processor->onShellExited() before Session::onExit
- `Source/terminal/rendering/Screen.h` / `.cpp`, `ScreenGL.cpp` — atlasRef typed typename Renderer::Atlas&, uploadStagedBitmaps calls updated
- `Source/terminal/tty/TTY.h` / `.cpp`, `UnixTTY.h` / `.cpp` — ODE instrumentation drained
- `Source/whelmed/Parser.cpp`, `Tokenizer.cpp`, `Screen.cpp`, `Component.cpp`, `State.cpp` — Style::Manager → Whelmed::Config direct
- `Source/EndColourId.cpp` — deleted (migrated to jam setColourIdMap runtime registration)
- `Source/whelmed/ColourIds.h` — deleted (zero consumers)
- `Source/config/Config.h` / `.cpp` — getStyleMetrics removed
- `CMakeLists.txt:285` — FreeType status message updated to `${JAM_ROOT}/jam_freetype`
- `end/modules/jreng_*` (7 directories) — deleted; `end/modules/` removed
- `ARCHITECTURE.md` (48 refs), `SPEC.md` (18 refs) — jreng → jam rename; glyph_packer entry corrected
- `PLAN-jam-migration.md` — Phase 3.6 ongoing, Phase 4 complete
- `DEBT.md` — whitespace tidied

**jam:**
- `jam_core/context/jam_context.h` — dual-impl (`JUCE_STANDALONE_APPLICATION` → single global pointer; else → kuassa-verbatim thread_local LIFO)
- `jam_gui/window/jam_window.h` / `.cpp` — Window::setRenderables forwarder, attachRendererToContent no longer installs walker (Option 5)
- `jam_gui/window/jam_modal_window.h` / `.cpp` — explicit (opacity, blurRadius) ctor overload; Style::Manager ctor delegates via C++ ctor delegation; dead setupWindow(Component&) removed
- `jam_gui/opengl/context/jam_gl_atlas_renderer.h` / `.cpp` — GLAtlasRenderer ctor takes GlyphAtlas&
- `jam_gui/opengl/context/jam_glyph_atlas.h` / `.cpp` — drops jam::Context<T> inheritance
- `jam_gui/opengl/renderers/jam_gl_renderer.h` / `.cpp` — ctor installs default walker rooted at openGLContext.getTargetComponent()
- `jam_gui/opengl/renderers/jam_gl_context.h` / `.cpp` — uploadStagedBitmaps gains GlyphAtlas&, Atlas type alias
- `jam_graphics/fonts/jam_typeface.h` / `.cpp` — CPU atlas methods + members removed (moved to GraphicsAtlas)
- `jam_graphics/fonts/jam_text_layout.h` / `.cpp` — draw() gains GraphicsAtlas&
- `jam_graphics/rendering/jam_graphics_atlas.h` / `.cpp` — new class, CPU-side atlas owner symmetric to GlyphAtlas
- `jam_graphics/rendering/jam_glyph_graphics_context.h` / `.cpp` — uploadStagedBitmaps gains GraphicsAtlas&, Atlas type alias, @file tag corrected
- `jam_graphics/jam_graphics.h` / `.cpp` — new header includes
- `jam_data_structures/style_manager/jam_style_manager.h` / `.cpp` — setColourIdMap static method added; getColourIdMap defined internally

**kuassa (option 2a migration):**
- `___lib___/kuassa_data_structures/style_manager/kuassa_style_manager.h` / `.cpp` — parallel fork of jam's setColourIdMap pattern
- `jreng-filter-strip/Source/PluginEditorColourId.cpp` — deleted
- `jreng-filter-strip/Source/PluginEditor.cpp:119` — initialiseTheme calls ku::Style::Manager::setColourIdMap with 100 ColourId entries preserved
- `jreng-filter-strip/CMakeLists.txt` — PluginEditorColourId.cpp removed from explicit SOURCES list

### Alignment Check
- [x] BLESSED principles applied (SSOT enforced for AppState window size + atlas ownership; Encapsulation preserved; Explicit atlas + opacity + blur passed through ctor chains; Stateless — atlas Ptrs now lifetime-bound to owner; Bound — GraphicsAtlas RAII)
- [x] NAMES.md: every new name approved at Decision Gate (`jam::GraphicsAtlas`, `setColourIdMap`, `Atlas` type alias, `saveCursor`/`restoreCursor`, ModalWindow ctor overload parameters, `graphicsAtlas` member, `atlasRef`)
- [x] JRENG-CODING-STANDARD: `not`/`and`/`or`, brace init, no early returns, `.at()` container access, no anonymous namespaces (Mi2 converted to `static`)
- [x] MANIFESTO: all Auditor findings resolved before sprint log (2 audit rounds, full clean sweep)

### Problems Solved
- Reload (Cmd+R) destroys text — explicit markAtlasDirty restoration in setRenderer (listener-only path missed same-type reloads)
- CPU→GPU switch crash on reload — Context thread_local boundary exposed; fix via GlyphAtlas Context drop + reference threading + Context dual-impl on JUCE_STANDALONE_APPLICATION
- Popup window lost glass — jam::ModalWindow::setupWindow depended on Style::Manager; explicit (opacity, blur) ctor overload takes values from Config directly
- Standalone shell-exit crash (`:q` / `exit`) — Display::onVBlank race with State destruction; fix via wiring Processor::onShellExited to destroy Display before Session teardown
- DEC 1049 scrollback garbage — added cursor save/restore at ?1049h/?1049l boundary (?47/?1047 unchanged)
- Resize → wrong split dimensions — Tabs::computeContentRect reads AppState which stayed stale during live resize; fix via MainComponent::resized writing live dimensions to AppState as SSOT
- Style::Manager architectural rot — dropped entirely from END; 9 consumer files migrated to direct Config reads; EndColourId.cpp migrated to jam-owned setColourIdMap (kuassa migrated in parallel)
- GlyphAtlas/CPU-atlas asymmetry — GraphicsAtlas extracted, Typeface fully atlas-agnostic, Renderer::Atlas type alias for symmetric templated consumption
- Phase 4 JAM migration cleanup — end/modules/jreng_* (7 directories) deleted
- ODE instrumentation drain (96 lines across 15 files)

### Debts Paid
- `DEBT-20260420T070224` — Resize → pane stale dimension on split (Tabs::computeContentRect now reads live AppState via MainComponent::resized)
- `DEBT-20260420T062722` — Claude Code scrollback garbage (DEC 1049 cursor save/restore via new Parser::saveCursor / restoreCursor methods + differentiated dispatch)

### Debts Deferred
- `DEBT-20260420T062717` — Ctrl+C vim dead pane — instrumented mid-session but bug did not reproduce under ODE harness on ARCHITECT's macOS environment; instrumentation drained per ODE protocol; bug likely remains in daily-usage observation
- `DEBT-20260411T100058` — mermaid rendering broken — not addressed this sprint

---

## Handoff to SURGEON: END→JAM Migration — Parity Stuck on Reload-Text-Disappears

**From:** COUNSELOR
**Date:** 2026-04-20

### Problem

END→JAM migration (PLAN-jam-migration.md) is code-complete through Phase 3.5. Phase 3.6 functional parity testing surfaced two bugs:

1. **Terminal popup window opaque (was: should be glass)** — RESOLVED. Root cause: terminal popup's `popup.show` caller arg was `*getTopLevelComponent()` (Terminal::Window) instead of `*this` (MainComponent). Window has its own `backgroundColourId` set to `transparentBlack` by JUCE DocumentWindow base ctor; MainComponent has no own colour and inherits the configured `windowColour` from `Terminal::LookAndFeel` via global default. Fixed at `MainComponentActions.cpp:504`.

2. **Cmd+R reload makes text disappear** — ACTIVE. Sprint 27 (commit `dd3c2ce`) explicitly fixed this exact bug via `markAtlasDirty` → `setRenderables` lambda → `consumeAtlasDirty` → `rebuildAtlas` (zeros GL handles). Pathfinder confirmed the post-migration `jam_gl_context.cpp` and surrounding code are byte-identical to `dd3c2ce` (modulo `jreng_*` → `jam_*` rename). Clean build done — bug persists. The Sprint 27 mechanism is in place but not producing the expected behavior. Suspected: GL context lifecycle (`Terminal::Display::glContextCreated` firing on new context, which calls `processor.getGrid().markAllDirty()` triggering full re-stage) is not actually firing on the reload path post-migration. Needs runtime instrumentation to verify.

### Recommended Solution

**Step 1:** Instrument the reload chain to identify the specific gap.

Add `jam::debug::Log::write(...)` traces (the existing diagnostic primitive added during migration) to:
- `MainComponent::setRenderer` entry — confirm reload calls it
- The `setRenderables` lambda (`MainComponent.cpp:128-129`) — confirm `consumeAtlasDirty` returns true and `rebuildAtlas` fires
- `Terminal::Display::glContextCreated` — confirm it fires after reload's renderer replacement
- `processor.getGrid().markAllDirty()` — confirm it runs
- `jam_gl_context.cpp::uploadStagedBitmaps` — confirm `stagedCount` is non-zero on the first paint after reload (if zero, packer was never told to re-stage)

If `stagedCount` is zero after reload — Pathfinder's earlier diagnosis stands: `Typeface::Packer` (CPU cache) doesn't know textures were destroyed; needs `typeface.clearAtlas()` call in the reload path. Sprint 27 may have implicitly relied on `glContextCreated`'s `markAllDirty` to force re-stage, but that may not be firing.

**Step 2:** Based on trace findings, either:
- (a) Fix the broken link in the existing chain (e.g., `glContextCreated` not firing → fix the GL context lifecycle wiring)
- (b) Add explicit `typeface.clearAtlas()` call in `MainComponent::setRenderer` (message thread) before `markAtlasDirty()` — bypass the implicit chain entirely

**Step 3:** Drain Auditor findings (B1, C1-C5, D1-D5 — see Auditor report below).

**Step 4:** ARCHITECT functional parity smoke test on the cleaned, fixed build. On green → Phase 4 cleanup (delete `end/modules/jreng_*`).

### Files to Modify

**Active bug (instrument first, fix second):**
- `Source/MainComponent.cpp` — instrument `setRenderer`, `setRenderables` lambda. Possibly add `typeface.clearAtlas()` call.
- `Source/component/TerminalDisplay.cpp` — instrument `glContextCreated`. Verify it fires on new context.
- `jam/jam_gui/opengl/context/jam_gl_context.cpp` — instrument `uploadStagedBitmaps` to log `stagedCount`.

**Audit cleanup (BLOCKING per ODE protocol):**
- 15 `// ODE` lines + the `Log::init` line — grep recipe: `grep -rn "// ODE" ~/Documents/Poems/dev/end ~/Documents/Poems/dev/jam`
- `~/Documents/Poems/dev/end/ode.log` — delete file at project root

**Audit cleanup (CLEANUP):**
- `Source/Main.cpp:134` — hardcoded absolute path `/Users/jreng/...` to ode.log (covered by ODE drain)
- `CMakeLists.txt:285` — stale `message(STATUS "FreeType: modules/jreng_freetype...")` — module is now `${JAM_ROOT}/jam_freetype`
- `Source/component/MainComponent.h:50` — comment-as-explanation, NAMES.md infraction
- `PLAN-jam-migration.md` — update status (Phases 0-3.5 done; 3.6 + 4 deferred) or convert to sprint receipt

**Audit cleanup (DOC):**
- `Source/component/Dialog.h:7,38-50,153-160` — stale doc references obsolete `Terminal::ModalWindow::setupWindow`
- `Source/component/LookAndFeel.cpp:29,247-252` — stale "via Config" / "via AppState" references; now via Style::Manager / ENDApplication

**Decision Gate items (audit-discovered emergent decisions not in original PLAN — ARCHITECT triage):**
- `jam::AnyMap`, `Style::Manager::createFromMetrics/clear/setMetrics`, `Config::getStyleMetrics`, `ENDApplication::rebuildStyleManager`
- `EndColourId.cpp` + `Source/whelmed/ColourIds.h` (mirrors JFS `PluginEditorColourId.cpp` pattern)
- `jam::NativeContextResource` (still active — Dialog + ActionList depend on it; ARCHITECT once called it "wrong abstraction" but it wasn't retired)
- `jam_log.cpp` (file-logger primitive — ARCHITECT directed its creation)
- `jam_lua` module (PLAN had 10 modules; jam_lua is the 11th, added mid-sprint)
- `Terminal::Window` extraction (PLAN Step 1.8.4 said absorb into jam::Window; instead extracted to END subclass per ARCHITECT correction)
- Whelmed Style::Manager migration scope (11 files, broader than PLAN Step 3.2's mechanical rename)

**Documentation drift (recommend DEBT.md capture — own sprint):**
- `ARCHITECTURE.md` — 48 jreng references; codebase is SSOT, full rewrite needed
- `SPEC.md` — 9 jreng references in jreng_text/jreng_graphics/jreng_gui module-organization sections

### Acceptance Criteria

- [ ] Reload (Cmd+R) preserves all text on screen — no blank atlas
- [ ] All ODE instrumentation removed; ode.log file deleted
- [ ] Stale comments and doc-blocks updated
- [ ] PLAN-jam-migration.md status reconciled
- [ ] ARCHITECT functional parity smoke test green: terminal flows, glass blur (macOS Apple Silicon CALayer residue absent via GLRoot+setRenderables), DWM glass (Windows), modal popups, mermaid rendering, hot reload, multi-window
- [ ] Phase 4 cleanup: `end/modules/jreng_*` (all 7 directories) deleted
- [ ] No residual `jreng::` namespace or `<jreng_*/...>` includes anywhere in END source

### Notes

**JAM migration scope summary:**
- 11 jam_* modules created/consolidated under `~/Documents/Poems/dev/jam/`: jam_core, jam_data_structures, jam_freetype, jam_javascript, jam_markdown, jam_graphics, jam_animation, jam_gui, jam_fonts, jam_look_and_feel, jam_lua
- New jam-side primitives: `jam::AnyMap`, `jam::debug::Log`, `jam::Style::Manager::createFromMetrics/clear/setMetrics/setAppearance(LAF&)`, `jam::NativeContextResource`
- END source: 352 namespace rename sites across 51 files, 4 explicit include rewrites, `Source/Gpu.{h,cpp,mm}` deleted (replaced by jam::GpuProbe), `Library/lua` + `Library/sol` deleted (moved to jam_lua), 11 Whelmed files migrated to Style::Manager
- New END files: `EndColourId.cpp` (Style::Manager ColourId map mirroring JFS pattern), `Source/whelmed/ColourIds.h` (Whelmed ColourId enums), `Source/component/TerminalWindow.{h,cpp}` (END subclass of jam::Window adding resize tracking)

**JFS (jreng-filter-strip) is the BLESSED reference for plugin patterns. kuassa/jam relationship: jam was forked from kuassa; kuassa-verbatim wherever possible. END uses jam as foundation.**

**ARCHITECT-locked architectural decisions throughout sprint:**
- Universal `jam_` prefix; `jam::` namespace only; no `jm` alias
- jam::Window owns native shared context (extension on top of verbatim kuassa::Window)
- ENDApplication owns Style::Manager (not AppState); Style::Manager mutated in place via clear/setMetrics/register*, NEVER destroyed
- Config + jam::GpuProbe → AppState → all renderer consumers (`AppState::setRendererType` calls `jam::BackgroundBlur::setEnabled` on render-mode change)
- LAF reads colours/fonts from Style::Manager only (never Config directly)
- `Terminal::LookAndFeel` houses END's `WindowsTitleBarLookAndFeel` behavior (relocated from jam::Window during the verbatim revert)
- Popup `getNativeSharedContext` flows through `jam::Window` → its glRenderer

**Test environment:** Apple M4 macOS (per ode.log earlier in sprint).

**Known divergences from PLAN that ARCHITECT confirmed:**
- jam_lua addition (Phase 1 expanded from 10 to 11 modules)
- Terminal::Window extraction (PLAN said absorb resize tracking into jam::Window; reverted to verbatim kuassa, extracted END-side)
- Style::Manager API extensions (createFromMetrics, clear, setMetrics, setAppearance(LAF&))

---

## Sprint 27: GlyphAtlas Singleton + Typeface deGL + Config-Reload Rebuild ✅

**Date:** 2026-04-19
**Duration:** ~03:30

### Agents Participated
- COUNSELOR: session lead, foundation-scope framing (ARCHITECT "proper foundation now"), multiple Decision Gate escalations (atomic placement, layer boundary, Context pattern adoption, TextLayout drop-in constraint), plan drafting and iterative correction, compile-failure triage (include order, Atlas::Type straggler, stale texture IDs on reload).
- Pathfinder: 6 surveys — popup close→atlas destroy path, GL context sharing hypothesis, atlas ownership structure (Typeface/Glyph::Atlas/GLContext/GLAtlasRenderer mapping), reload-trigger chain, popup renderer construction + MainComponent wiring + jreng::Window API, commit-style reference (git log).
- Engineer: 6 execution passes — Step 0 (class + file rename, 100+ refs, git mv), Step 1 (GlyphAtlas creation), Step 2 (Typeface setFontFamily/setFontSize on both platforms), Step 3 (strip GL handles from Typeface), Steps 4-7 chained (GLContext/GLAtlasRenderer/MainComponent/popup wiring), Context-pattern refactor (abandoned ctor threading after Screen<Renderer> template collision), Steps 8-12 (AppState properties + atomic + listener + two consumers), ARCHITECTURE.md update.
- Auditor: 2 validation passes — post-Step 0 (git mv status + build system), post-Steps 4-7 (ctor parameter order discrepancy flagged — amended PLAN rather than swap), final end-to-end sweep (grep sanity + contract compliance + doc drift).
- ARCHITECT: locked BLESSED-proper direction at every Decision Gate — rejected atomic-on-Typeface, rejected atomic-on-GlyphAtlas (kept AppState as state-machine home per State.h precedent), rejected CPU-atlas refactor (kept scope honest — CPU path has no bug, jreng_graphics cannot reach jreng_gui), chose GlyphAtlas location (jreng_gui/opengl/context/), approved Context<T> pattern adoption when template threading hit Screen<Renderer> wall, directed pre-existing macOS setSize cachedMetricsSize bug fixed in-sprint.

### Files Modified (~25)

**New files:**
- `modules/jreng_gui/opengl/context/jreng_glyph_atlas.h` — `jreng::GlyphAtlas` declaration, inherits `jreng::Context<GlyphAtlas>`, 89 lines, pure GL resource holder (monoAtlas/emojiAtlas handles + atlasSize).
- `modules/jreng_gui/opengl/context/jreng_glyph_atlas.cpp` — `rebuildAtlas()` impl (glDeleteTextures + zero non-zero handles).
- `PLAN-atlas-ownership.md` — project-root plan document, 14 steps, all gates resolved before execution.

**Renamed (git mv):**
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.{h,cpp,mm}` → `jreng_glyph_packer.{h,cpp,mm}` — class `jreng::Glyph::Atlas` → `jreng::Glyph::Packer`. Rename touches 9 source files (typeface.h, typeface.mm, typeface.cpp, typeface_shaping.cpp, glyph_constraint.h, box_drawing.h, staged_bitmap.h, text_layout.h, gl_context.h).

**Modified — Typeface (pure machinery):**
- `modules/jreng_graphics/fonts/jreng_typeface.h` — removed glMonoAtlas/glEmojiAtlas members + 5 public GL accessors (getGl*/setGl*/resetGlAtlas); `atlas` member → `packer`; added setFontFamily/setFontSize declarations; ctor param `atlasSize` retained (CPU packer still needs it).
- `modules/jreng_graphics/fonts/jreng_typeface.mm` — `setFontFamily` implementation (CoreText: tear down primary font handles, reload via descriptor-create, clear fallbackFontCache, cachedMetricsSize = -1, packer.clear); `setFontSize` delegates to `setSize` + `packer.clear`; `setSize` now resets `cachedMetricsSize = -1.0f` (parity with FreeType path, pre-existing asymmetry closed).
- `modules/jreng_graphics/fonts/jreng_typeface.cpp` — `setFontFamily` implementation (FreeType: teardown/reload sequence from `loadFaces` regular-face block, registry slot 0 update); `setFontSize` analogous.

**Modified — jreng_gui module:**
- `modules/jreng_gui/jreng_gui.h` — umbrella include order: `jreng_glyph_atlas.h` before `jreng_gl_context.h` (GLContext references GlyphAtlas via Context).
- `modules/jreng_gui/jreng_gui.cpp` — umbrella .cpp include for `jreng_glyph_atlas.cpp`.
- `modules/jreng_gui/opengl/renderers/jreng_gl_context.h` — `GLContext() = default` (ctor threading abandoned post Context-pattern adoption); removed `GlyphAtlas&` member; doxygen updated.
- `modules/jreng_gui/opengl/renderers/jreng_gl_context.cpp` — `uploadStagedBitmaps` uses `auto& atlas { *jreng::GlyphAtlas::getContext() }` local alias; read/write handles via atlas; `Atlas::Type::emoji` references → `Packer::Type::emoji`.
- `modules/jreng_gui/opengl/context/jreng_gl_atlas_renderer.h` — ctor `explicit GLAtlasRenderer(jreng::Typeface&)`; removed `managedTypefaces` + `glyphAtlasRef`; `typefaceRef` retained; `~GLAtlasRenderer() = default` preserved (no dtor-driven teardown).
- `modules/jreng_gui/opengl/context/jreng_gl_atlas_renderer.cpp` — ctor mem-init simplified; `uploadStagedBitmaps` hoisted out of per-command loop (once-per-frame, correct since staging is per-typeface).

**Modified — App layer (Source/):**
- `Source/AppIdentifier.h` — `App::ID::fontFamily` + `App::ID::fontSize` Identifiers added.
- `Source/AppState.h` — added `getFontFamily`/`setFontFamily`/`getFontSize`/`setFontSize` public API; added `markAtlasDirty`/`consumeAtlasDirty`; added `std::atomic<bool> atlasDirty { false }` private member.
- `Source/AppState.cpp` — getter/setter impls mirroring `getWindowWidth`/`setWindowSize` pattern; atomic methods use release-store / acquire-exchange.
- `Source/MainComponent.h:159` — `jreng::GlyphAtlas glyphAtlas;` value member after `typeface`.
- `Source/MainComponent.cpp` — `glyphAtlas.setAtlasSize(compact)` in ctor; `appState.getWindow().addListener(this)` / `removeListener(this)` ctor/dtor pair; `applyConfig` writes font family/size/renderer to AppState and calls `setRenderer`; `setRenderer` calls `typeface.setAtlasSize` + `glyphAtlas.setAtlasSize` + `markAtlasDirty` (renderer replacement unconditionally invalidates GL texture IDs); `setRenderables` lambda consumes atlasDirty → `glyphAtlas.rebuildAtlas()`; `valueTreePropertyChanged` body dispatches WINDOW property changes to typeface setters + `markAtlasDirty`.
- `Source/MainComponentActions.cpp:500` — popup GLAtlasRenderer ctor simplified to `(typeface)` (shared GlyphAtlas via Context, no param).
- `Source/terminal/rendering/ScreenGL.cpp` — `#include "../../AppState.h"`; `Screen<Renderer>::renderPaint` checks `consumeAtlasDirty()` → calls `GlyphAtlas::getContext()->rebuildAtlas()` (message-thread safe no-op in CPU-only runtime where handles are 0).
- `Source/component/Tabs.cpp:833-866` — `computeContentRect` reads window dimensions from AppState (SSOT) instead of `getLocalBounds()`; removed `jassert(not base.isEmpty())` startup-path failure.

**Documentation:**
- `ARCHITECTURE.md` — L125 (jreng_glyph_packer rename), L796/1008/1095 (Glyph::Packer references in constraint/NF docs), new GlyphAtlas entry under Context pattern section, new APVTS subsection for fontFamily/fontSize/atlasDirty signal path.

### Alignment Check
- [x] BLESSED — Bound (GlyphAtlas single owner, MainComponent); Lean (one new class, dead `managedTypefaces` removed, 13-branch listener within MANIFESTO-L); Explicit (writer → ValueTree → listener → atomic → consumer, every hop visible); SSOT (handles live once on GlyphAtlas; font values on AppState ValueTree once); Stateless (Typeface no longer holds GL state, GlyphAtlas is dumb resource holder); Encapsulation (Typeface no longer knows GL, consumers tell GlyphAtlas to rebuild); Deterministic (config → AppState → listener → atomic → consumer → rebuild).
- [x] NAMES.md — `GlyphAtlas`, `Packer`, `atlasDirty` (noun-adjective per State.h `snapshotDirty` Rule 5), `fontFamily`/`fontSize` (match existing Config::Key naming), `markAtlasDirty`/`consumeAtlasDirty` (verb-form, State.h `setFullRebuild`/`consumeFullRebuild` pattern).
- [x] JRENG-CODING-STANDARD — `not`/`and`/`or`, positive nested checks, brace-init, braces on new line, no early returns, memory-order acquire/release explicit.
- [x] CONTRACT adherence — all Decision Gates honored, plan locked before execution, no improvisation.

### Problems Solved
- **Popup close destroys shared atlas** — fixed at architectural level: popup's GLAtlasRenderer shares MainComponent-owned GlyphAtlas via `jreng::Context<>` singleton; popup destruction touches no atlas state.  Reverts the 491db0a dtor-driven `detachContext` workaround.
- **Cmd+R reload makes text disappear** — `setRenderer` creates a new `juce::OpenGLContext`; the old context's texture IDs become invalid on teardown, but `GlyphAtlas` retained the stale numbers.  Fixed by unconditional `appState.markAtlasDirty()` in `setRenderer`; first frame of the new context consumes the signal and runs `rebuildAtlas` → zeroes handles → `uploadStagedBitmaps` allocates fresh textures.
- **Font family / size hot-reload** — new `Typeface::setFontFamily` / `setFontSize` tear down and rebuild platform-specific font handles, clear fallback cache, invalidate `cachedMetricsSize`, and drop stale CPU-rasterized glyphs via `packer.clear()`.  Works on both GPU and CPU paths (CPU consumer hook in `Screen::renderPaint`).
- **Typeface BLESSED-S violation** — Typeface was machinery holding persistent GL resource state.  Stripped GL handles + accessors; Typeface is now pure rasterization + shaping.
- **macOS setSize cachedMetricsSize asymmetry** — FreeType `setSize` reset the metrics cache; macOS did not.  Closed the gap (pre-existing bug, fixed in-sprint per ARCHITECT direction).
- **Glyph::Atlas name conflation** — old name referred to CPU packer + LRU + staging queue, not the GPU atlas.  Renamed to `Glyph::Packer`; `GlyphAtlas` now names the GPU resource owner.
- **Tabs::computeContentRect startup assertion** — `getLocalBounds()` was empty before JUCE layout ran; replaced with `AppState` dimension read (SSOT).

### Debts Paid
- None. The atlas-destruction and reload-disappearance bugs were surfaced and fixed same-session; no DEBT.md entry existed for them. `DEBT-20260418T111325` (cmd+r reload → blank text) was already superseded by 491db0a but that fix was architecturally wrong; Sprint 27 supersedes the superseding.

### Debts Deferred
- None introduced this sprint. `DEBT-20260411T100058` (mermaid rendering broken) remains on the ledger from Sprint 24's deferral; not scoped for this sprint.

---

## Sprint 26: Instant-Kill Terminal Pane Close ✅

**Date:** 2026-04-19
**Duration:** ~00:30

### Agents Participated
- COUNSELOR: session lead, root-cause framing (two bugs: Session::stop() ordering + close() graceful-first), option triage (SIGKILL-first vs detach-reader vs two-phase), SSOT hoist decision on Auditor findings.
- Pathfinder: terminal teardown survey — identified `UnixTTY::close()` 5 s `stopThread(5000)` join + SIGTERM grace loop as hang vector; flagged `Session::stop()` ordering (processor.reset before tty->close) as UAF window with reader thread mid-`processWithLock`.
- Engineer: reordered `Session::stop()`; rewrote `UnixTTY::close()` (SIGKILL-first) and `WindowsTTY::close()` (TerminateProcess-first) with named constants `instantKillJoinTimeoutMs` + `instantKillChildWaitMs`; removed now-unused `killTimeoutIterations` / `killPollInterval`; updated doxygen (close() docs + WindowsTTY file-level shutdown sequence).
- Auditor: PASS with two Low concerns — stale UnixTTY.h file-level shutdown doc + duplicated `instantKillJoinTimeoutMs` across UnixTTY.h and WindowsTTY.cpp (SSOT).
- ARCHITECT: approved SIGKILL-first direction, approved hoist fix-all for both audit findings.

### Files Modified (4)

- `Source/terminal/logic/Session.cpp:539–555` — swapped `processor.reset()` / `tty->close()` ordering so the reader thread is joined before Processor destruction; comment rewritten with correct rationale (reader-thread UAF, not State timer — timer is message-thread bound).
- `Source/terminal/tty/TTY.h:110,349` — hoisted `inline static constexpr int instantKillJoinTimeoutMs { 500 }` into `TTY::Constants`; updated abstract `close()` doxygen ("up to 5 seconds" → "bounded by instantKillJoinTimeoutMs").
- `Source/terminal/tty/UnixTTY.h:26–30, 237` — file-level "Destructor / shutdown sequence" rewritten for SIGKILL-first flow; removed per-class `instantKillJoinTimeoutMs` member (now inherited from TTY).
- `Source/terminal/tty/UnixTTY.cpp:200–238` — `close()` body reordered: signal exit → SIGKILL child + blocking waitpid → close master → `stopThread(instantKillJoinTimeoutMs)`. Removed SIGTERM poll loop + `killTimeoutIterations` / `killPollInterval` constants. Doxygen rewritten.
- `Source/terminal/tty/WindowsTTY.cpp:42–48, 211–215, 888–953` — `close()` body reordered: signal exit → `TerminateProcess` + `WaitForSingleObject(instantKillChildWaitMs)` → `ClosePseudoConsole` → `stopThread(instantKillJoinTimeoutMs)` → handle cleanup. File-level `### Shutdown sequence` block synced to new ordering. Removed local duplicate of `instantKillJoinTimeoutMs` (kept Windows-specific `instantKillChildWaitMs`).

### Alignment Check
- [x] BLESSED principles followed — **B** (Bound): per-pane ownership strictly preserved; SIGKILL targets this pane's pid only; master fd / pseudo console / reader thread are per-pane; no cross-instance effect. Removed SIGTERM grace — was a defensive guard with no named threat per Manifesto "Guard Rule". **L** (Lean): net deletion (two constants, one poll loop, one duplicate constant); close() bodies shrank on both platforms. **E** (Explicit): named constants replace magic `5000` / `2000`; new doxygen states the sequence, the rationale, and the thread context on every touched function; positive nested checks preserved; no early returns introduced. **S** (SSOT): `instantKillJoinTimeoutMs` now lives exactly once in `TTY.h` and is inherited by both platform subclasses. **S** (Stateless): no new machinery state. **E** (Encapsulation): close() remains the single public teardown API; platforms implement the contract, owner calls it and does not inspect. **D** (Deterministic): pane close completes within a bounded 500 ms ceiling regardless of in-flight burst size.
- [x] NAMES.md adhered — `instantKillJoinTimeoutMs`, `instantKillChildWaitMs`: semantic, no type suffix, verb+noun form. ARCHITECT-approved per Rule -1 (implicit via `/go` acceptance of Engineer plan naming).
- [x] MANIFESTO.md / JRENG-CODING-STANDARD.md — alternative tokens (`not`, `and`, `or`); brace init; positive nested checks; explicit nullptr / INVALID_HANDLE_VALUE comparisons; no magic numbers introduced (two existing 5000 / 2000 magics deleted); no anonymous namespaces; no `namespace detail`; function-call spacing matches existing style in each file.

### Problems Solved
- **Pane close hangs on large in-flight burst** — closing a terminal pane (e.g. long Claude Code session) could hang up to 5 s. Root cause: `UnixTTY::close()` called `stopThread(5000)` immediately after closing master; reader thread stuck inside `Processor::processWithLock` on a large chunk could not observe `threadShouldExit()` in time, and SIGTERM grace added another 500 ms before SIGKILL finally delivered. Fix: SIGKILL / TerminateProcess the child first — child death makes the master fd EOF / the overlapped ReadFile complete with broken-pipe immediately, reader exits, bounded 500 ms join is a ceiling, not an expected wait.
- **Use-after-free window on pane close** — `Session::stop()` destroyed `Processor` before calling `tty->close()`. The TTY `onData` callback captures `procRawPtr` at `Session.cpp:261`; if the reader thread was mid-`processWithLock` when stop() ran, reset of Processor would invalidate `procRawPtr` while the reader was still dereferencing it. Old comment justified the order by "State timer" concerns, but the State timer is a `juce::Timer` — it runs on the message thread (same as stop()), so it cannot fire mid-shutdown. The reader-thread UAF is the real hazard. Fix: call `tty->close()` first (joins reader) → then `processor.reset()` (safe, no more callers). Callbacks already nulled at 543–545, so no late dispatches on the message thread either.
- **SSOT duplication of bounded-join timeout** (audit finding) — `instantKillJoinTimeoutMs` was declared in both `UnixTTY.h` and `WindowsTTY.cpp` with identical value and identical meaning. Hoisted into `TTY.h` as an inline `static constexpr` — single truth, inherited unqualified by both subclasses.

### Debts Paid
- None. Bug was surfaced and fixed same-session; no DEBT.md entry existed for it.

### Debts Deferred
- None. `DEBT-20260411T100058` (mermaid rendering broken) remains on the ledger from Sprint 24's deferral; not scoped for this sprint.

---

## Sprint 25: Reload Atlas Reset + CPU Pane Repaint + Overlay Z-order + Standalone Save Contract ✅

**Date:** 2026-04-19
**Duration:** ~04:00

### Agents Participated
- COUNSELOR: session lead. Initial sprint surface (renderer-type guard, save/load narrowing, onContextReady, switchRenderer idempotency) was over-scoped and wrong on multiple fronts; reverted in full to HEAD after ARCHITECT /stop, then re-applied minimal targeted fixes grounded in ODE runtime evidence. Decision-gate discipline failures (proposing hedges for ARCHITECT-already-decided questions, bailing toward SPEC-violating single-atlas-size shortcut) were surfaced by ARCHITECT and corrected.
- Pathfinder: surveys — (1) Cmd+R reload path end-to-end (keybind → Action::Registry → config.reload → onReload → MainComponent::applyConfig → setRenderer → window->setRenderer → tabs->switchRenderer → Display::switchRenderer → onVBlank → Screen::render → GLSnapshotBuffer → uploadStagedBitmaps), (2) jreng_animator.h diff vs Kuassa's `___lib___/kuassa_animation/fade/kuassa_toggle_fade.h` (byte-identical — confirmed animator NOT the bug), (3) ModalWindow.cpp:74 entanglement check (single-terminal modal, not the active-only starvation pattern), (4) MessageOverlay CPU-vs-GPU paint trigger divergence survey, (5) CPU non-active pane repaint starvation trace, (6) `MessageOverlay::setBounds` git blame → commit 1a63528 (Apr 9) regression.
- Engineer: many delegations — initial (reverted) sprint edits, full HEAD revert + reapply two minimal fixes, two ODE instrumentation passes (`juce::FileLogger` at project-root `end-debug.log`, `threadTag()` helpers at 6 sites), both instrumentation passes stripped within sprint per ODE protocol, `detachContext()` protected base-method addition, CPU non-active repaint fix (Display::onVBlank self-repaint + MainComponent lambda simplification), MessageOverlay z-order + bounds, standalone save/load contract split, audit cleanup batch (dead code, doxygen sync, schema fixes, ARCHITECTURE.md state-persistence section).
- Auditor: two passes — post-revert intermediate (caught lingering pre-sprint virtual-dispatch hazard + doc gaps) and final pre-log (caught stray `public:` in animator header, dead `end.state` references across 7 files, `getStateFile()` dead fallback, `AppIdentifier.h` `port` schema drift, MessageOverlay doxygen gaps, ARCHITECTURE.md state-persistence drift).
- ARCHITECT: diagnostic log inspection, /stop + /ode protocol invocations, contract reassertion ("daemon must restore session fully"), naming correction (`setOnContextReady` → `onContextReady` matching codebase pattern), evidence-grounded redirect from patch-chasing to ODE diagnosis, revert directive when sprint over-scoped.

### Files Modified (15 source + 1 docs)

**Primary fixes (DEBT target + regression + starvation):**
- `modules/jreng_gui/opengl/context/jreng_gl_renderer.h` — new `protected: void detachContext()` + doxygen explaining subclass-dtor contract and virtual-dispatch-during-base-destructor rationale; removed unused public `void detach()`.
- `modules/jreng_gui/opengl/context/jreng_gl_renderer.cpp` — `GLRenderer::detachContext()` impl (delegates to `openGLContext.detach()`); removed `GLRenderer::detach()` def.
- `modules/jreng_gui/opengl/context/jreng_gl_atlas_renderer.cpp` — `~GLAtlasRenderer()` replaced `= default` with body calling `detachContext()` at top; `contextClosing()` unchanged (its existing `typeface->resetGlAtlas()` now fires via derived vtable); doxygen explains the fix.
- `modules/jreng_gui/opengl/context/jreng_gl_atlas_renderer.h` — file-level doxygen notes destructor path through `detachContext()` → derived `contextClosing()` → atlas reset.
- `Source/component/TerminalDisplay.cpp::onVBlank()` — `repaint()` on `this` after `visitScreen(s.render(...))`; each Display triggers its own JUCE paint invalidation. Doxygen updated.
- `Source/MainComponent.cpp::initialiseTabs()` — `tabs->onRepaintNeeded` lambda no longer calls `terminal->repaint()` (Display self-repaints in onVBlank); retains `statusBarOverlay->updateHintInfo()` + `window->triggerRepaint()`.
- `Source/MainComponent.cpp::resized()` — unconditional `messageOverlay->setBounds(getLocalBounds())` added after `tabs->setBounds`; doxygen added. Restores pre-commit-1a63528 behavior.
- `Source/MainComponent.cpp::showMessageOverlay()` — removed redundant `setBounds` line (now owned by `resized()`); doxygen clarifies geometry ownership.
- `Source/MainComponent.cpp::initialiseTabs()` — removed redundant `tabs->setBounds(getLocalBounds())` (resized() fires immediately after via valueTreeChildAdded).
- `Source/component/MessageOverlay.h::showMessage()` + `showResize()` — `toFront(false)` after `toggleFade(this, true, fadeInMs)` for consistent sibling z-order; `@note` doxygen added to both.

**Standalone save/load contract split:**
- `Source/Main.cpp::initialise()` — standalone branch: removed `appState.load()` and `hadState` guard; only `loadWindowState()` when `Config::Key::windowSaveSize` is set. Daemon paths untouched. Also removed the now-obsolete file-level doxygen reference to `end.state`.
- `Source/Main.cpp::systemRequestedQuit()` — standalone branch: removed `appState.save()` call; window size persisted via existing `saveWindowState()` earlier in same method. Doxygen updated: "standalone mode persists only window size; sessions die with the window by design."
- `Source/AppState.cpp::getStateFile()` — removed dead no-UUID fallback branch (all call sites are daemon-gated post-sprint); body simplified to always return `nexus/<uuid>.display` path.
- `Source/AppState.cpp` — `save()` and `load()` doxygen updated to reflect daemon-only contract.
- `Source/AppState.h` — 4 doxygen sites updated (file-level, `save()`, `load()`, `getStateFile()`).

**Audit sync (doc + dead refs):**
- `Source/AppIdentifier.h` — file-level schema comment rewrote: `window.state` schema (standalone), `<uuid>.display` schema (daemon); corrected prior claim that `<uuid>.display` carries `port` (save() strips port before writing; port lives in `<uuid>.nexus`).
- `Source/MainComponent.h:10` — doxygen reference to `end.state` replaced with `window.state` (standalone) / `<uuid>.display` (daemon client).
- `Source/MainComponent.cpp` — inline comment at tab-restore site updated from "from `end.state`" to "from `<uuid>.display` when present (daemon client mode)."
- `Source/component/Tabs.h::onRepaintNeeded` doxygen — added `StatusBarOverlay::updateHintInfo()` refresh to the stated responsibilities (pre-existing gap surfaced by audit).
- `Source/config/Config.h:14` + `Source/config/Config.cpp:18` — state-path documentation row split: `window.state` (standalone) + `<uuid>.display` (daemon client).
- `ARCHITECTURE.md:751-760` — Platform Configuration table + prose rewritten to reflect post-sprint contract (window.state for standalone, `<uuid>.display` for daemon, `<uuid>.nexus` for port).
- `modules/jreng_gui/animation/jreng_animator.h` — stray `public:` access-specifier inside `struct Animator` removed (struct is already public by default).

### Alignment Check
- [x] BLESSED principles followed — `B` (Bound): base `~GLRenderer()` safety-net detach retained, idempotent with derived's explicit `detachContext()`; standalone/daemon persistence contracts cleanly bounded. `L` (Lean): net deletion (dead public `detach()`, dead fallback branch, redundant setBounds + tabs->setBounds, end.state machinery). `E` (Explicit): new virtual-dispatch contract documented on protected `detachContext()`; each Display owns its repaint trigger explicitly. `S` (SSOT): typeface atlas handles always zero when owning renderer dies; each Display the single truth for its own paint invalidation. `S` (Stateless): no new machinery state. `E` (Encapsulation): Display owns paint lifecycle, not a global lambda picking active; MessageOverlay owns z-order via its own `toFront()` call. `D` (Deterministic): reload + renderer-type swap now produces consistent render output regardless of mode or reload count.
- [x] NAMES.md adhered — `detachContext` (protected, subclass-hook semantic; ARCHITECT-approved per Rule -1); `onContextReady` was attempted mid-sprint and reverted when the real root cause (virtual-dispatch trap) made the callback dead weight.
- [x] MANIFESTO.md / JRENG-CODING-STANDARD.md — alternative tokens (`not`, `and`, `or`); brace init; `noexcept` retained; positive nested checks; no early returns; no anonymous namespaces; no `namespace detail`; no magic numbers introduced.
- [x] ODE instrumentation removed — two passes added and stripped within sprint; `end-debug.log` deleted; final grep confirms zero `juce::Logger::writeToLog` / `threadTag()` / `FileLogger` residue in touched files.

### Problems Solved
- **Cmd+R reload made all text disappear** (DEBT target) — root cause was C++ virtual-dispatch-during-base-destructor. `~GLRenderer()` calls `openGLContext.detach()` which fires `openGLContextClosing()` → virtual `contextClosing()` — but at that point the derived portion is destroyed and dispatch lands on the base's empty override instead of `GLAtlasRenderer::contextClosing()` (which resets typeface atlas handles). Stale handles persisted across renderer instances; next GL context's `uploadStagedBitmaps()` saw non-zero handles, skipped texture creation, and uploaded to invalid IDs. Fix: protected `detachContext()` on base, derived destructor calls it at top of body — detach fires while derived vtable is still active, `contextClosing()` correctly dispatches to derived, handles reset. Base destructor's own `detach()` retained as idempotent safety net.
- **CPU-mode non-active panes blank on session restore** — `tabs->onRepaintNeeded` lambda only called `getActiveTerminal()->repaint()`, starving non-active Display components in CPU mode (where each JUCE component must receive explicit `repaint()` to have `paint(Graphics&)` invoked). GPU mode masked this because `GLRenderer::renderComponent` iterates all renderables per GL frame. Fix: Display self-repaints in `onVBlank` after render; MainComponent lambda simplified to window triggerRepaint + status bar hint only.
- **MessageOverlay invisible on launch / non-resize reload** — commit 1a63528 (Apr 9) gated `showMessageOverlay()` behind `isUserResizing()`, which inadvertently dragged the `messageOverlay->setBounds(getLocalBounds())` call inside the gate. Bounds stayed 0×0 on non-resize code paths, making every `showMessage()` paint invisibly. Fix: unconditional `setBounds` in `resized()`; `showMessageOverlay()` retains only the ruler-metrics computation + `showResize()` trigger.
- **MessageOverlay z-order inconsistency in CPU mode** — second cmd+r reload painted overlay underneath siblings (CPU mode respects sibling declaration order literally; GPU GL-composites mask this). Fix: `toFront(false)` after fade-in in both `showMessage()` and `showResize()`.
- **Standalone mode was persisting full state** — prior contract saved WINDOW+TABS+zoom+renderer to `end.state` on standalone quit, then restored all on next launch. Per ARCHITECT contract: standalone persists only window size; sessions die with window by design. Fix: standalone quit writes only `window.state` via `saveWindowState()`; launch loads only `loadWindowState()` (when configured); `appState.save()/load()` gated to daemon mode only; dead fallback branch of `getStateFile()` removed.
- **Sprint scope over-reach** (process) — initial sprint interpreted ARCHITECT's "DROP EVERYTHING, only store and restore WINDOW size" as a literal full gut of `save()/load()` (which also broke daemon layout restore), added an `onContextReady` callback + `switchRenderer` idempotency fix as speculative mechanism before diagnosing. After ARCHITECT /stop and contract re-assertion, reverted entire sprint surface to HEAD, re-applied minimal targeted fixes grounded in ODE-instrumentation runtime evidence.

### Debts Paid
- `DEBT-20260418T111325` — mac Cmd+R reload made all text disappear. Resolved via `~GLAtlasRenderer` destructor calling `GLRenderer::detachContext()` at top of body, making `contextClosing()` virtual dispatch land on the derived vtable while it is still active and correctly reset typeface atlas handles. See Files Modified entries in `jreng_gl_atlas_renderer.{h,cpp}` + `jreng_gl_renderer.{h,cpp}`.

### Debts Deferred
- None. `DEBT-20260411T100058` (mermaid rendering broken) remains on the ledger from Sprint 24's deferral; not scoped for this sprint.

---

## Sprint 24: Ctrl+Q Confirmation Dialog + Display Font Resolution ✅

**Date:** 2026-04-17
**Duration:** ~02:30

### Agents Participated
- COUNSELOR: session lead, `/pay` ordering (newest-first → DEBT-20260412T234116), per-question decision discipline (kill-all semantics, Dialog naming, button widget choice, message copy, confirmation key), TTF metadata root-cause triage, handoff authorship to the `___display___` font project
- Pathfinder: surveyed Ctrl+Q binding (Config.cpp:204, Action.cpp:23, MainComponentActions.cpp:73), daemon-mode detection (AppState.cpp:138-151), session save/restore (AppState.cpp:375-448), existing modal infra (Terminal::Popup + ModalWindow, MessageOverlay, LoaderOverlay), font-role dispatch pattern at ActionRow.cpp:27,33 + LookAndFeel.cpp:541-559
- Engineer: six delegations — (1) scaffold `Terminal::Dialog` + `getTextButtonFont` font-role dispatch + wire quit action, (2) add `window.confirmation_on_exit` gate + content-driven sizing + explicit child-LAF propagation via `lookAndFeelChanged`, (3) diagnostic logging instrumentation to prove font dispatch, (4) strip logging + set `actionListNameFamily` default to `"Display Bold"`, (5) TTF `name` table inspection of Display {Book,Medium,Bold}.ttf + Display Mono Bold.ttf via fontTools — confirmed Display Prop metadata broken (uppercase subfamily, missing ID 16/17), (6) split `actionListName*`/`actionListShortcut*` config into separate family + style keys, chain `.withStyle(...)` in all five font-construction sites
- ARCHITECT: diagnostic runtime logs pasted back, TTF rebuilt with correct `name` table, spotted pre-existing `Display Mono Bold` misconfiguration (shortcut font was also substituted silently)

### Files Modified (9 Source + 3 binary + 2 docs)
- NEW `Source/component/Dialog.h` — `Terminal::Dialog : juce::Component` with `onYes`/`onNo`/`onDismiss` callbacks, `keyPressed` for Y/N, `parentHierarchyChanged` + `visibilityChanged` + `lookAndFeelChanged` overrides; `static constexpr` paddings; LookAndFeel-derived font via `jreng::ID::font = jreng::ID::name` tagging on messageLabel + yesButton + noButton
- NEW `Source/component/Dialog.cpp` — content-driven `getPreferredWidth/Height` (measures text width in Display font + button widths + padding), `resized` lays out message-above-buttons, `callAsync` + `SafePointer` deferred focus grab, explicit `setLookAndFeel(&laf)` on each child in `lookAndFeelChanged` for deterministic dispatch
- `Source/MainComponentActions.cpp:74-135` — `"quit"` action: `if (not popup.isActive())` guard → `if (not config.getBool(windowConfirmationOnExit))` direct-quit path → `else` Popup+Dialog path with `daemonMode` branch (Save/Yes → systemRequestedQuit; No/kill-all → loop closeActiveTab + delete state file + quit) vs standalone (Yes → quit; No → dismiss only)
- `Source/component/LookAndFeel.cpp:499-517` — rewrote `getTextButtonFont` to dispatch via `button.getProperties()[jreng::ID::font]` (name role → Display family + style + size; fallback → tab font)
- `Source/component/LookAndFeel.cpp:555-567` — `getLabelFont` name + keyPress branches chain `.withStyle(...)` using new style keys
- `Source/component/LookAndFeel.cpp:58` — added `setColour (juce::Label::textColourId, fg)` so Dialog messageLabel inherits terminal foreground
- `Source/config/Config.h:313,431,440` — new `windowConfirmationOnExit`, `actionListNameStyle`, `actionListShortcutStyle` keys with Doxygen
- `Source/config/Config.cpp:146,255-260` — registered the three new keys; family defaults reverted from `"Display Bold"`/`"Display Mono Bold"` to bare `"Display"`/`"Display Mono"` with separate `"Bold"` style
- `Source/config/default_end.lua` — `confirmation_on_exit` entry under `window = {}` table; `name_font_style` / `shortcut_font_style` entries under action-list section
- `___display___/fonts/Display/{Book,Medium,Bold}.ttf` — name-table rebuilt: Bold is RIBBI (ID 1=`Display`, ID 2=`Bold`); Book/Medium non-RIBBI (ID 1=`Display Book`/`Display Medium`, ID 2=`Regular`); all three populate ID 16=`Display` + ID 17=`{Book,Medium,Bold}`
- `___display___/carol/SPRINT-LOG.md` — handoff appended: rebuild `fonts/Display/*.ttf` metadata (completed by ARCHITECT)

### Alignment Check
- [x] BLESSED principles followed (no early returns, positive checks, `static constexpr` for all dimensions, no magic numbers, explicit LAF propagation)
- [x] NAMES.md adhered (`Dialog`, `onYes`/`onNo`/`onDismiss`, `messageLabel`/`yesButton`/`noButton`, `getPreferredWidth/Height`, `windowConfirmationOnExit`, `actionListNameStyle`, `actionListShortcutStyle` — all self-describing)
- [x] MANIFESTO.md principles applied (Explicit Encapsulation — Dialog exposes only callbacks, children stay private; Single Responsibility — Dialog is generic 2-choice body, quit semantics live in MainComponentActions)
- [x] Code contract — no early returns, no logging in final diff (diagnostics stripped per sprint protocol after root cause found), no magic numbers

### Problems Solved
- **Ctrl+Q had no confirmation** — raw `systemRequestedQuit()` on Ctrl+Q offered no chance to abort. Now gated by `window.confirmation_on_exit = true` (default) with mode-aware copy: daemon asks about session persistence ("Save this session?" — Yes = save + quit client / daemon persists; No = close all tabs → daemon exits via onAllSessionsExited); standalone asks explicit "Are you sure you wanna quit?".
- **Action::List `name` font was always substituted** — pre-existing bug discovered mid-sprint. Config had `actionListNameFamily = "Display Bold"`, but `juce::FontOptions().withName(...)` matches TTF family (ID 1) only. TTF ID 1 = `Display`, not `Display Bold`. JUCE silently fell back to a default face. Same bug affected `actionListShortcutFamily = "Display Mono Bold"`.
- **Display Prop TTF metadata was broken** — subfamily encoded `BOLD`/`MEDIUM`/`BOOK` (uppercase); no typographic preferred family (ID 16/17) entries. Rebuilt TTFs now encode Bold as RIBBI, populate ID 16/17, make family lookup deterministic across JUCE/DirectWrite.
- **Dialog messageLabel rendered in default white** — `juce::Label::textColourId` was never wired to terminal foreground in `Terminal::LookAndFeel::setColours()`. Added the one-line colour registration; all labels now inherit `coloursForeground` by default (explicit per-component `setColour` still wins where set, e.g. Action::List rows).

### Debts Paid
- `DEBT-20260412T234116` — Ctrl+Q confirmation dialog, daemon + standalone modes, Save/Kill-all semantics per Files Modified entries in `MainComponentActions.cpp` + `Dialog.{h,cpp}`

### Debts Deferred
- `DEBT-20260411T100058` — mermaid rendering broken (stuck, no loader overlay) — ARCHITECT scoped this sprint to the dialog entry only ("one by one. focus on dialog"). Stays on ledger for next sprint.

---

## Sprint 23: Tab Label Crossplatform TUI/cwd + displayName SSOT ✅

**Date:** 2026-04-17
**Duration:** ~02:00

### Agents Participated
- COUNSELOR: session lead, `/pay` targeting, contract audit per ARCHITECT challenge ("including whelmed? crossplatform? SSOT? BLESSED?"), scope discipline (Rule 7), post-audit remediation plan
- Pathfinder: three surveys — (1) Windows tab-label codepath + foregroundPid + OSC 7 path forms, (2) tab-label binding topology for multi-pane tabs + Whelmed focus gap, (3) OSC 133 command-state tracking + foregroundProcess clearing semantics + displayName identifier scopes across codebase
- Researcher: reference-terminal survey at `~/Documents/Poems/dev/TERMINAL` — Windows Terminal (OSC 0/2 only, no OSC 7, no process walk), Alacritty (OSC only, no Windows process walk), WezTerm (hybrid: OSC title first + CreateToolhelp32Snapshot/th32ParentProcessID walk + youngest-by-start_time + NtQueryInformationProcess PEB read for cwd), ConPTY API constraints (`GetConsoleProcessList` is attached-process-only, unusable from terminal side), OSC 7 path-form variance survey (MSYS2 `/c/...`, powershell backslash + no separator, wezterm forward-slash variants)
- Engineer: three delegations — (1) WindowsTTY process-tree walk with TTL cache + OSC 7 parser `/C:/...` branch + powershell integration fix, (2) displayName SSOT unification across Terminal/Whelmed via PaneComponent + TTY::getShellPid virtual chain + Session::onFlush PID-based clear + flushStrings redundant compare removal, (3) 14-finding audit remediation (shellProgram dead-state deletion, WindowsTTY helper decomposition, ParserESC DRY collapse + modern cast, thread doxygen corrections, State/Parser doc fixes, ARCHITECTURE.md sync)
- Auditor: pre-log audit returned 14 findings — 8 sprint-introduced + 6 pre-existing in touched files; all 14 resolved per CAROL clean-sweep rule

### Files Modified
- `Source/terminal/tty/WindowsTTY.cpp:120` — `static constexpr int64_t foregroundQueryCacheTtlMs { 500 }` named constant
- `Source/terminal/tty/WindowsTTY.cpp:131,176` — new `static` file-local helpers `collectDescendantPids` (BFS via Process32FirstW/NextW filtering by `th32ParentProcessID` from shell root) and `findYoungestDescendant` (GetProcessTimes + CompareFileTime across candidates)
- `Source/terminal/tty/WindowsTTY.cpp:1020,1037-1040` — `getForegroundPid` decomposed to 38-line orchestrator: cache check → snapshot → `collectDescendantPids` → `findYoungestDescendant` → cache write; `getShellPid` override returns `childPid`
- `Source/terminal/tty/WindowsTTY.h` — `mutable DWORD cachedForegroundPid { 0 }` + `mutable int64_t lastForegroundQueryTimeMs { 0 }` members (memoization, MESSAGE THREAD only); `getShellPid` override declaration
- `Source/terminal/tty/TTY.h` — `virtual int getShellPid() const noexcept { return 0; }` base declaration; thread doxygen on `getForegroundPid` corrected ("Any thread. Called from Session::onFlush (message thread)")
- `Source/terminal/tty/UnixTTY.h,cpp` — `getShellPid` override returning `static_cast<int> (childProcess)`
- `Source/terminal/logic/Session.h:211,222` — `Session::getForegroundPid` + `Session::getShellPid` doxygen corrected to MESSAGE THREAD
- `Source/terminal/logic/Session.cpp:283-307` — `onFlush` lambda: `if (fgPid == shellPid)` clears foregroundProcess via `setForegroundProcess ("", 0)`; else existing name-write path
- `Source/terminal/logic/Session.cpp:386-397,400` — `Session::getShellPid` passthrough to `tty->getShellPid()` with corrected thread doxygen
- `Source/terminal/logic/Session.cpp:238,339` — dead `setProperty (Terminal::ID::shellProgram, ...)` writes deleted from both Session constructors
- `Source/terminal/logic/ParserESC.cpp:452-496` — OSC 7 Windows branches collapsed: one block handles both `/c/...` MSYS and `/C:/...` native forms via conditional `:` insertion; `static_cast<int>` replaces C-style cast
- `Source/terminal/data/Identifier.h` — deleted duplicate `Terminal::ID::displayName` and dead `Terminal::ID::shellProgram` identifiers
- `Source/terminal/data/State.cpp:1266-1282` — removed `shell` local + `foreground != shell` compare; writes `App::ID::displayName`
- `Source/terminal/data/State.h:1461` — `flushStrings` doxygen updated: "foreground process (when non-empty) → cwd leaf name"; shell-filter note relocated to Session::onFlush
- `Source/component/Tabs.cpp:124,175` — `tabName.referTo` reads `App::ID::displayName` at new-tab creation and in `globalFocusChanged`
- `Source/component/Tabs.cpp:171-183` — `globalFocusChanged` restructured: outer `dynamic_cast<PaneComponent*>` rebinds tabName via `pane->getValueTree().getPropertyAsValue(App::ID::displayName, ...)` (covers Whelmed); inner `dynamic_cast<Terminal::Display*>` preserves terminal-only side effects
- `Source/terminal/logic/Parser.{h,cpp}` — stale `Session::resized()` doxygen references corrected to actual caller
- `Source/terminal/shell/powershell_integration.ps1:15` — OSC 7 emits `file://$HOST/$($PWD.Path -replace '\\','/')` so payload is `file://HOST/C:/Users/...` (parser-compatible)
- `ARCHITECTURE.md` — line 676 identifier swap; lines 680-689 displayName Computation section rewritten for PID-based filter at Session layer + App-scoped identifier; line 1186 glossary entry updated; line 1205 `shellProgram` glossary entry deleted

### Alignment Check
- [x] BLESSED — `B` (Bound): cache members owned by WindowsTTY, lifecycle matches; `getShellPid` owned by TTY. `L` (Lean): `getForegroundPid` 90→38 lines via helper decomposition; ParserESC DRY collapse; shellProgram dead state removed. `E` (Explicit): PID comparison replaces string-match heuristic; named constant `foregroundQueryCacheTtlMs`; `static_cast` throughout; positive nested checks; `not/and/or` tokens. `S` (SSOT): one `displayName` identifier (App::ID) for both Terminal + Whelmed; one at-prompt decision point (Session::onFlush). `S` (Stateless): Session tells State to clear; State doesn't infer. `E` (Encapsulation): PaneComponent::getValueTree() virtual reused; no new pattern invented. `D` (Deterministic): same PID → same outcome across platforms
- [x] NAMES.md — Rule -1 honored. New names ARCHITECT-approved: `getShellPid` (verb+noun, semantic), `cachedForegroundPid` + `lastForegroundQueryTimeMs` + `foregroundQueryCacheTtlMs` (semantic memoization labels), `collectDescendantPids` + `findYoungestDescendant` (verb+noun, action-clear). All alphabetic English, non-redundant with type
- [x] JRENG-CODING-STANDARD.md — brace init, `not/and/or`, positive nested checks, `.at()` where applicable, `static_cast` modern, explicit `nullptr`/`INVALID_HANDLE_VALUE` checks, no early returns, `noexcept` on all new methods, `override` consistently, no anonymous namespaces (file-local helpers use `static` linkage per Rule)

### Problems Solved
- **Windows ConPTY has no `tcgetpgrp` equivalent** — `WindowsTTY::getForegroundPid` originally returned shell `childPid` unconditionally, making tab-label `foreground != shell` name-compare at `State.cpp:1271` meaningless. Fix: walk the process tree with `CreateToolhelp32Snapshot` + BFS from `childPid` via `th32ParentProcessID`, pick descendant with latest `CreationTime` via `GetProcessTimes` + `CompareFileTime`. TTL cache bounded to 500ms prevents the 120Hz snapshot cost from `State::timerCallback → onFlush`. WezTerm-validated heuristic
- **PowerShell integration emitted malformed OSC 7** — `file://$HOST$($PWD.Path)` had no separator between hostname and `C:\Users\...` drive + raw backslashes. Parser third-slash finder never matched; `setCwd` never fired. Fix emits `file://HOST/C:/Users/...` via path separator + `-replace '\\','/'`
- **OSC 7 parser only handled MSYS form** — `/c/Users/...` mapped to `C:/Users/...`, but native Windows `/C:/Users/...` (drive+colon) fell through to non-Windows path. Added sibling branch; later collapsed per DRY with MSYS branch into one conditional
- **TUI name stuck after command exits** (ARCHITECT: "it always show running tui. we also want cwd") — `foregroundProcess` was never cleared when walk fell back to shell PID. Guard `if (fgNameLen > 0)` at `Session.cpp:293` skipped setForegroundProcess on query failure, leaving stale "nvim" name indefinitely. Fix: Session::onFlush now compares `fgPid == shellPid` (PID-based, crossplatform); when equal (at-prompt case on both Unix via `tcgetpgrp == shell pgid` and Windows via walk fallback), explicitly writes empty foregroundProcess. `State::flushStrings` `name.isNotEmpty()` guard then correctly falls to cwd leaf
- **Two identifier declarations for one concept** (ARCHITECT: "i thought tab name is single source? why whelmed on different valuetree?") — `Terminal::ID::displayName` + `App::ID::displayName` both `juce::Identifier{"displayName"}` but declared in separate namespaces. Shadow state / SSOT violation. Deleted Terminal scope; App::ID::displayName is now the sole pane-label identifier. Terminal `State::flushStrings` writes it; Whelmed `Component::openFile` already wrote it; `Tabs.cpp` reads it
- **Whelmed panes didn't participate in tab-label binding** — `globalFocusChanged` dynamic_cast to `Terminal::Display*` failed for Whelmed, so `tabName.referTo` never re-bound when Whelmed gained focus. Fix: outer `dynamic_cast<PaneComponent*>` handles the rebind generically via `getValueTree()` virtual; inner Terminal cast retained for terminal-only side effects
- **Shadow state** (Audit Finding #1) — `Terminal::ID::shellProgram` was still written by Session constructors after the PID-comparison fix removed the only reader. Deleted identifier + both writes; ARCHITECTURE.md glossary entry removed
- **getForegroundPid monolithic** (Audit Finding #2) — 90 lines / 5+ nested branches flagged as Lean smell. Decomposed to two named `static` helpers + 38-line orchestrator (cache + snapshot + delegate). Single responsibility each
- **OSC 7 Windows branches duplicated** (Audit Finding #3) — two near-identical branches differing by `:` presence. Collapsed under one guard with conditional colon insertion; `static_cast<int>` replaces C-style `(int)` (Finding #4)
- **Thread doxygen stale** (Audit Findings #5–#8) — `getForegroundPid` / `getShellPid` claimed READER THREAD on base (`TTY.h:229`), Session declaration (`Session.h:211,222`), and Session definition (`Session.cpp:400`). Actual invocation path: `State::timerCallback` (juce::Timer → MESSAGE THREAD) → `state.onFlush` lambda. All four sites corrected
- **State.h flushStrings doc stale** (Audit Finding #9) — claimed "foreground process (when different from shell)"; after filter relocation, corrected to "foreground process (when non-empty) → cwd leaf name" with shell-filter note pointing to Session::onFlush
- **ARCHITECTURE.md drift** (Audit Findings #10–#13) — four sites referenced deleted identifier or old filter logic. All synced to current codebase-as-SSOT
- **Parser stale `Session::resized()` reference** (Audit Finding #14) — method does not exist; both `Parser.h:189` and `Parser.cpp:172` corrected to actual caller

### Debts Paid
- `DEBT-20260411T125015` — tab label now updates crossplatform from active terminal pane's foreground TUI (when running) or cwd leaf (at prompt); Windows ConPTY foreground detection via process-tree walk; OSC 7 parser + PowerShell integration produce valid cwd signal; displayName identifier unified so Whelmed panes participate in tab binding

### Debts Deferred
- None

---

## Sprint 22: Selection Bleed Isolation ✅

**Date:** 2026-04-16
**Duration:** ~00:20

### Agents Participated
- COUNSELOR: session lead, `/pay` targeting, approach selection (identity gate vs per-pane state), Whelmed constraint verification before approval
- Pathfinder: two surveys — (1) selection state ownership + render path + pane architecture + shared ValueTree suspect, (2) pane identity accessor confirmation + Whelmed selection path + Whelmed split capability
- Engineer: one surgical edit, identity-gate swap

### Files Modified
- `Source/component/TerminalDisplay.cpp:601` — `isActivePane` computation changed from pane-type check (`AppState::getActivePaneType() == App::ID::paneTypeTerminal`) to pane-identity check (`getComponentID() == AppState::getActivePaneID()`). Matches the identity pattern already in use at line 579 for the focus-toFront gate. Sibling terminal panes now fail the gate; selection render branch at line 616 skipped entirely, preventing phantom highlights from stale `selectionAnchorRow/Col` in inactive panes

### Alignment Check
- [x] BLESSED — `B` (Bound): active pane UUID is the sole owner of "who currently shows selection"; `L` (Lean): one-line change, no new members/methods/helpers; `E` (Explicit): positive check, alternative tokens preserved, no early return; `S` (SSOT): `AppState::getActivePaneID()` is the single truth; `S` (Stateless): no new state; `E` (Encapsulation): reuses existing identity accessor pattern already present in same callback; `D` (Deterministic): identical inputs yield identical render branch decisions
- [x] NAMES.md — no new names introduced; reuses `getComponentID()` and `getActivePaneID()`
- [x] JRENG-CODING-STANDARD — alternative tokens, brace init, positive check all preserved

### Problems Solved
- Selection highlight bleeding into sibling terminal panes when one pane had an active selection. Root cause: `isActivePane` gated on pane *type*, not pane *identity*, so every terminal pane in a split passed the gate and painted its own stale `selectionAnchor/selectionCursor` coords while the global `AppState::selectionType` was non-none. Copy payload was always correct because extraction reads from the active pane's `State` only — bleed was render-only. Whelmed unaffected: separate `paint()` codepath, no `onVBlank` participation, structurally excluded, and cannot be split (one instance maximum per `createWhelmed` in-place swap)

### Debts Paid
- `DEBT-20260416T091057` — selection highlight now isolated to active pane via identity gate at `TerminalDisplay.cpp:601`

### Debts Deferred
- None

---

## Sprint 21: Ledger Drain + Pane-0 SIGWINCH + Ctrl+C Kitty Keymap ✅

**Date:** 2026-04-16
**Duration:** ~03:00

### Agents Participated
- COUNSELOR: session lead, `/pay` ordering, diagnostic strategy, ODE-style runtime instrumentation coordination, post-fix verification against `ctrlc.log` evidence
- Pathfinder: four surveys — file-open dispatch path, Whelmed loader lifecycle, TTY abstraction + resize emit site, kitty keyboard encoder + per-session mode state
- Oracle: root-cause analysis of pane-0 spurious SIGWINCH, pinpointed `Display::resized()` unconditional `onResize` with three candidate gate layers
- Engineer: 7 delegations — path quoting, bracketed-paste wrap for opener, loader guard + addChildComponent, 4× `const` return restore, TTY Template Method refactor, `modifierCode` JUCE_MAC guard, audit cleanup pass (DIAG strip + visibility tightening + doxygen fixes)
- Auditor: pre-log audit, 6 findings surfaced and resolved in single cleanup pass

### Files Modified
- `Source/terminal/selection/LinkManager.cpp:213-228` — `LinkManager::dispatch()` now wraps path in `"..."` for shell quoting and wraps full opener command in `\x1b[200~…\x1b[201~\r` when `state.getMode(ID::bracketedPaste)` is true. Mirrors `Processor::encodePaste` bracketed-paste pattern; `\r` placed after close marker so readline inserts before executing
- `Source/whelmed/Component.cpp:43` — `addAndMakeVisible(loaderOverlay)` → `addChildComponent(loaderOverlay)`; overlay starts hidden
- `Source/whelmed/Component.cpp:126-132` — `openFile()` extracts `const int totalBlocks` from state once, gates `loaderOverlay.show()` + `toFront()` on `if (initialBatch < totalBlocks)` — no async parser work means no loader
- `Source/whelmed/Component.h:62` — dead `int totalBlocks { 0 };` member removed (all reads went through `state.getProperty(App::ID::totalBlocks)`)
- `modules/jreng_gui/window/jreng_background_blur.cpp:132,137,172,259` — `const bool` return type restored on `isDwmAvailable`, `enable`, `applyDwmGlass`, `enableWindowTransparency` definitions to match header declarations (build fix)
- `Source/terminal/tty/TTY.h:343-370` — Template Method: `platformResize(int,int)` promoted to non-virtual public with same-dim suppression gate; new pure virtual protected `doPlatformResize` hook; new protected `rememberDimensions(int,int) noexcept` for subclass priming; new private `lastResizeCols{-1}`, `lastResizeRows{-1}`
- `Source/terminal/tty/TTY.cpp:103-120` — `platformResize` body (gate + hook dispatch + dim record) and `rememberDimensions` body
- `Source/terminal/tty/WindowsTTY.h:220,285` — override renamed `platformResize` → `doPlatformResize`, moved to `protected:`
- `Source/terminal/tty/WindowsTTY.cpp:755,1462,1498` — `open()` calls `rememberDimensions(cols, rows)` before `startThread()`; override body renamed; stale `READER THREAD` doxygen → `MESSAGE THREAD`
- `Source/terminal/tty/UnixTTY.h:163,226` — override renamed and moved to `protected:`
- `Source/terminal/tty/UnixTTY.cpp:190,366` — matching `open()` priming and doxygen fix
- `Source/terminal/data/Keyboard.h:541-596` — `modifierCode()` `metaBit` contribution now `#if JUCE_MAC`. Doxygen rewritten to describe JUCE's `commandModifier == ctrlModifier` aliasing on Windows/Linux and why the bit is macOS-only

### Alignment Check
- [x] BLESSED — `B` (Bound): TTY owns PTY size truth via `lastResizeCols/Rows`; `L` (Lean): all edits scoped; `E` (Explicit): platform `#if` justified by actual JUCE semantic divergence, positive checks throughout; `S` (SSOT): TTY is single authority on "what size did we tell the kernel"; `S` (Stateless): no new machinery state, `lastResize*` are calculation-input shadows; `E` (Encapsulation): Template Method keeps gate in one place, subclass hook is pure; `D` (Deterministic): same-dim resize produces no side effect
- [x] NAMES.md — Rule -1 honored. New names: `doPlatformResize` (subclass hook), `rememberDimensions` (explicit priming action), `lastResizeCols`/`lastResizeRows` (semantic state labels). All verb/noun alignment consistent
- [x] JRENG-CODING-STANDARD.md — alt tokens, brace init, no early returns, const return types restored on blur methods, no magic numbers (bracketed-paste open/close use `static constexpr const char[]` mirroring existing pattern)
- [x] ODE protocol — instrumented `TTY::platformResize`, `WindowsTTY::write`, `TTY::run` (PTY-READ), `TerminalDisplay::keyPressed`, `State::pushKeyboardMode`/`popKeyboardMode`/`getKeyboardFlags` to `ctrlc.log` at project root; runtime evidence confirmed `modifier=13` double-count and revealed `win32InputMode` branch bypass explained the "random kitty" apparent race; all instrumentation stripped and log files deleted same sprint

### Problems Solved
- **Windows open-file path separator loss** (`DEBT-20260416T083624`) — `LinkManager::dispatch` was concatenating `opener + " " + path + "\r"` with no shell quoting. Path `C:\Users\jreng\...` reached shell as raw bytes; shell interpreted backslashes as escapes, collapsing to `UsersjrengDocuments...`. Fix quotes the path inside `"..."` so every shell on Windows (cmd, pwsh, bash, zsh) preserves the literals
- **File-open command echoed char-by-char in terminal** (mid-sprint surface, not in DEBT.md) — PTY line discipline's ECHO flag is intrinsic; no master-side suppression available. Wrap in bracketed paste so readline inserts atomically
- **Whelmed loader visible on short documents** (`DEBT-20260411T095809`) — `addAndMakeVisible` made overlay visible at construction before any `show()` guard could fire. `Parser::run` only increments `blockCount` for blocks past `startBlock`; when `initialBatch == totalBlocks` the async thread does no work, `valueTreePropertyChanged` never fires, `hide()` never called. Fix: default-hidden via `addChildComponent`, `show()` only when async work is needed
- **Windows build failure** (`const`-return mismatch on 4 `BackgroundBlur` methods) — header declared `static const bool` on all four Windows methods; `.cpp` definitions had `bool` — `error C2373: different type modifiers`. Restored per NAMES.md Rule 5 (consistency with existing header pattern for this class)
- **Spurious SIGWINCH on pane 0 mid-nvim session** (Ctrl+C race surface) — root cause: `Display::resized()` unconditionally forwarded `onResize` to `Session::resize` → `tty->platformResize` even when dims were unchanged from what the Session was constructed with. Pane 0's first deferred `resized()` (it's the only pane `setVisible(true)` at creation) landed AFTER nvim had launched, emitting a redundant SIGWINCH. Template Method gate in base TTY silently no-ops same-dim calls; subclass `open()` primes the cache so the first post-layout resize is correctly suppressed
- **Ctrl+C bypass of user's nvim `<C-c>` → `:qa` mapping** (surfaced after SIGWINCH fix) — kitty keyboard protocol Ctrl+C was encoded as `ESC[99;13u` instead of `ESC[99;5u`. Modifier `13 = 1+4+8` = base + ctrl + super; modifier `5 = 1+4` = base + ctrl. nvim received an unrecognised modifier combination and fell through to its default alert instead of firing the user's mapping. Root cause: `modifierCode` added `metaBit` on `isCommandDown()`; JUCE aliases `commandModifier` to `ctrlModifier` on Windows/Linux, so every plain Ctrl+X double-counted. Platform-gated with `#if JUCE_MAC` — Cmd on macOS correctly maps to Super/Meta, Windows/Linux no longer double-counts
- **Apparent "random 1 of 5 tabs" broken Ctrl+C** — runtime evidence in `ctrlc.log` showed `Processor::encodeKeyPress` has two branches: `win32InputMode` ON uses `Keyboard::encodeWin32Input` (direct `0x03` encoding, bypasses `getKeyboardFlags`); `win32InputMode` OFF falls to `Keyboard::map` which reads kitty flags. The "randomness" was `win32InputMode` toggle, not a kitty state race. No fix needed

### Debts Paid
- `DEBT-20260416T083624` — Windows open-file path separator preserved via shell quoting (`LinkManager.cpp:214`)
- `DEBT-20260411T095809` — Whelmed loader no longer rendered when content fits viewport (`Component.cpp:128-132` + `:43`)

### Debts Deferred
None.

---

## Sprint 20: Bracketed Paste CRLF Normalization ✅

**Date:** 2026-04-16
**Duration:** ~00:10

### Agents Participated
- COUNSELOR: session lead, symptom triage, reference-terminal convention recall (xterm / alacritty / wezterm / kitty), fix-location selection, post-edit verification
- Pathfinder: bracketed-paste surface survey — mode toggle sites, clipboard entry point, PTY write path, normalization audit (none found)
- Engineer: single delegation — 3-line change inside `Processor::encodePaste` bracketed branch

### Files Modified
- `Source/terminal/logic/Processor.cpp:137-138` — inside `if (bracketed)` branch: introduced `const juce::String normalized { text.replace ("\r\n", "\n").replace ("\r", "\n") }` and swapped `text` → `normalized` in the wrapper concatenation. Two-step replace: CRLF first, then lone CR. Non-bracketed else branch untouched.

### Alignment Check
- [x] BLESSED — `B` (Bound): change scoped to one branch of one method; `L` (Lean): 2 lines, no helpers, no new names; `E` (Explicit): `const` local, literal control bytes, replace chain reads as the algorithm; `S` (SSOT): paste normalization lives where bracketed-paste semantics already live (`encodePaste`); `S` (Stateless): no state added; `E` (Encapsulation): Processor owns paste encoding, no caller poking; `D` (Deterministic): same input always yields same output
- [x] NAMES.md — Rule -1 honored: no new names. `normalized` is a local intermediate, direct transformation label
- [x] MANIFESTO.md / JRENG-CODING-STANDARD.md — no early returns, positive checks, brace style preserved, no magic numbers (`"\r\n"` / `"\r"` / `"\n"` are the ASCII bytes they name), existing doxygen unchanged

### Problems Solved
- **CRLF doubling on pasted multi-line content**: pasting C++ code from nvim into Claude Code inside END produced a blank line between every source line. Root cause: Windows clipboard CRLF passed verbatim through bracketed-paste wrapper; readline consumers render `\r` and `\n` as two separate breaks. Fix collapses to single LF — matches xterm/alacritty/wezterm/kitty convention.

### Debts Paid
None — symptom surfaced mid-session, not previously logged to DEBT.md.

### Debts Deferred
None.

---

## Sprint 19: Mac-only Line-Wrap Bug — LC_CTYPE Fix ✅

**Date:** 2026-04-16
**Duration:** ~03:00

### Agents Participated
- COUNSELOR: session lead, prior-sprint handoff intake, hypothesis triage (parser shadow-state ruled out, locale identified), reference-terminal survey (kitty / wezterm / ghostty env + DA + integration), root-cause framing, BLESSED-grounded option presentation
- Pathfinder: parser cursor/wrapPending shadow-state survey across Parser.cpp / ParserVT.cpp / ParserCSI.cpp / ParserOps.cpp / ParserESC.cpp / Terminal::State (×1 deep pass); reference-terminal child-env / DA / OSC 133 / terminfo / CPR survey for kitty + wezterm + ghostty (×3 parallel); macOS NSLocale / AppleLocale system probe (×1)
- Engineer: 6 delegations — (1) PS1 probe at precmd, (2) move probe to preexec, (3) move probe to zle-line-init with widget chain, (4) add LANG/LC_CTYPE/locale dump to probe, (5) remove forced LANG in UnixTTY child setup, (6) LC_CTYPE setenv in child + doxygen step renumber, (7) full DIAG strip across Parser.cpp / ParserVT.cpp / UnixTTY.cpp / Main.cpp / zsh_end_integration.zsh + log-file deletion

### Files Modified
- `Source/terminal/tty/UnixTTY.cpp:49-51,89-92` — added doxygen step 8 (`Set LC_CTYPE=en_US.UTF-8 if not already set, ensure UTF-8 character classification, works around inherited invalid LANG=UTF-8 on macOS`); added `if (getenv ("LC_CTYPE") == nullptr) { setenv ("LC_CTYPE", "en_US.UTF-8", 1); }` after `COLORTERM` setenv; removed previous LANG-forcing block at former lines 88-91; removed all DIAG `bytes.log`/`output.log` hex appenders from `read()` / `write()`
- `Source/Main.cpp:175` — removed `std::setlocale (LC_CTYPE, "en_US.UTF-8")` workaround at start of `ENDApplication::initialise` (child-side LC_CTYPE is the correct locus)
- `Source/terminal/logic/Parser.cpp:100-113,170-186` — removed `std::once_flag diagOnce` log-deletion block from constructor; removed 17-line state dump tail from `Parser::process()`
- `Source/terminal/logic/ParserVT.cpp:214-216,623-625` — removed `screen.log` 3-line append from `GroundOps::flushPrintRun` (fast-path) and `Parser::print` (slow path)
- `Source/terminal/shell/zsh_end_integration.zsh` — full rewrite: `_end_precmd` (OSC 7 + OSC 133;A), `_end_preexec` (OSC 133;C), `add-zsh-hook precmd _end_precmd`, `add-zsh-hook preexec _end_preexec`. Migrated from `precmd_functions+=()` / `preexec_functions+=()` to `add-zsh-hook` (zsh 5.9 standard, robust against framework array-clobber)
- Deleted: `bytes.log`, `output.log`, `screen.log`, `state.log` (project root) and `/tmp/end-ps1.log`

### Alignment Check
- [x] BLESSED — `B` (Bound): scoped to child pre-exec, no new ownership; `L` (Lean): 4-line LC_CTYPE guard, no helpers, no constants table; `E` (Explicit): positive `nullptr ==` check, named env var, named locale string, doxygen step describes intent + workaround target; `S` (SSOT): LC_CTYPE outranks LANG via setlocale precedence — single point of truth for character classification, user's LANG remains untouched; `S` (Stateless): no machinery state added; `E` (Encapsulation): localized to `runChildProcess`, no caller poking; `D` (Deterministic): same input (LC_CTYPE unset) yields same output every spawn
- [x] NAMES.md — Rule -1 honored: no new names introduced. `LC_CTYPE` and `en_US.UTF-8` are POSIX standards, not invented
- [x] MANIFESTO.md / JRENG-CODING-STANDARD.md — `nullptr ==` comparison, brace style on new line, no early returns, doxygen step uses verb-form description

### Problems Solved
- **Mac-only line-wrap bug (DEBT-20260415T123816)**: typing on row=1 with multi-line OMP prompt caused overflow UP to row=0 (OMP decoration row) instead of wrap DOWN to row=2. Root cause: macOS launchd inherits `LANG=UTF-8` (invalid locale name) into END's process environment. Without LC_CTYPE override, child shell's `setlocale` falls through to LANG, fails validation, falls back to "C" (= US-ASCII). zsh's prompt-width counter then byte-counts UTF-8 chars: `╰─ ` (`\xe2\x95\xb0 \xe2\x94\x80 \x20`) = 7 bytes = 7 "cells" in zsh's broken model. zsh's reposition math (`CSI A + CSI 20 D` from physical (1, 27) → physical (0, 7)) targeted byte-offset 7 of zsh's frame, which mapped to the decoration row. Setting LC_CTYPE=en_US.UTF-8 in child env restores zsh's UTF-8 multi-byte mode → `╰─ ` correctly counted as 3 cells → reposition math correct
- **Parser-shadow-state hypothesis (prior counselor's framing)**: ruled out via comprehensive Pathfinder survey. No persistent cursor/wrapPending/scroll shadow exists on Parser; only `GroundOps::Cursor c` stack-local in `processGroundChunk` (written back to State unconditionally on `consumed > 0`) and `savedCursor[2]` for DECSC/DECRC (not on this path). Bug was locale-side, not parser-side
- **`LANG=en_US.UTF-8` force-setting in child** (added by prior counselor): removed. Forcing LANG overrides user's intentional message/numeric/time locale — incorrect surface

### Debts Paid
- `DEBT-20260415T123816` — character went up to previous row when row is full — resolved via LC_CTYPE setenv in child pre-exec (`UnixTTY.cpp:89-92`)

### Debts Deferred
None — no out-of-scope findings pushed to DEBT.md during this sprint.

---

## Handoff to COUNSELOR: Mac-only input wrap bug (row=1 typing overflows UP to row=0 OMP row)

**From:** COUNSELOR
**Date:** 2026-04-16
**Status:** Blocked — root cause not conclusively identified after extended session

### Context
DEBT-20260415T123816: on Mac only, with an Oh-My-Posh multi-line prompt:
- Row 0: OMP prompt line
- Row 1: active input row (cursor here after prompt render)
When user types enough chars on row=1 to approach the right margin, the next char overflows **UP to row=0** (overwriting the OMP line) instead of wrapping **down to row=2**. Same zsh/OMP config works correctly on other terminals. ARCHITECT confirmed bug is parsing-side, not rendering.

### Completed
- Reproduced bug with live instrumentation
- Dumped three raw byte streams: `bytes.log` (shell→us), `output.log` (us→shell), `screen.log` (grid writes)
- Added `state.log` (Terminal::State snapshot per tty chunk)
- Verified our parser correctly executes every CSI command shell emits (no Terminal::State shadow-state violation found)
- Traced the exact byte sequence: shell emits `CSI A + CSI 20 D` (or `CSI A + CSI C 29`) from `(1, 27)` to reposition at `(0, 7)` — shell's input_start in its internal linear model
- Ruled out several hypotheses (each tested and reverted):
  - CR-commits-pending-wrap (didn't fix)
  - U+EB06 width-2 override (didn't fix)
  - Per-char width-table disagreement with macOS wcwidth (no WIDTH DIFF entries once locale set)
- Applied one change that was wrong but not reverted yet: `setenv ("LANG", "en_US.UTF-8", 1)` in `UnixTTY.cpp:90` (was `"UTF-8"` which is invalid locale). ARCHITECT confirmed this alone does NOT fix the bug.
- Also kept: `std::setlocale (LC_CTYPE, "en_US.UTF-8")` at `Main.cpp:175` (app startup) — legitimate locale setting, not a bug fix.

### Remaining
- Actual fix. Session ended without identifying the parser-side root cause.
- Strip all `// DIAG` instrumentation across 4 files (listed below)
- Remove 4 log files from project root: `bytes.log`, `screen.log`, `output.log`, `state.log`
- Run full audit after real fix lands

### Key Decisions
- **BLESSED hunt:** ARCHITECT forced the framing "there IS a Terminal::State / Parser shadow state violation — find it." COUNSELOR could not locate it. May be true and COUNSELOR is blind to it, or the violation sits elsewhere (e.g., inter-process: our child env setup distorts shell's internal PS1-length calc).
- **Refused to handoff to SURGEON**: ARCHITECT explicitly rejected that move — insisted session understand the problem before handoff. Understanding was not achieved.
- Shell's internal model says `input_start = (0, 7)` (linear). Our render puts actual cursor at `(1, 3)`. Cols=30, visibleRows=13. Parser's `Terminal::State` tracks `(1, 3)` correctly throughout; the divergence lives entirely in the shell's own PS1-length calculation, which terminals other than ours somehow drive to the correct value with the SAME zsh/starship config.

### Files Modified (all contain `// DIAG` instrumentation that must be stripped)
- `Source/terminal/logic/Parser.cpp` — `OdeLoggerGuard` replaced by simpler `std::once_flag diagOnce` block in ctor that deletes 4 log files; state dump block at end of `Parser::process()`
- `Source/terminal/logic/ParserVT.cpp` — `screen.log` appends in `flushPrintRun` (fast-path cell write) and in `Parser::print` (slow-path `activeWriteCell`)
- `Source/terminal/tty/UnixTTY.cpp` — `bytes.log` hex appender in `UnixTTY::read`; `output.log` hex appender in `UnixTTY::write`. **NON-DIAG change at line 90**: `setenv("LANG", "en_US.UTF-8", 1)` replacing `"UTF-8"`. Decide whether to keep (it's a latent bugfix regardless — `LANG=UTF-8` is not a valid locale name).
- `Source/Main.cpp` — `std::setlocale (LC_CTYPE, "en_US.UTF-8")` at start of `ENDApplication::initialise`. Comment reads `// force UTF-8 LC_CTYPE so child shell inherits correct locale`. Did not fix the bug; keep or revert per ARCHITECT.

### Open Questions
- Which specific terminal capability / byte sequence / env var makes zsh's zle compute `input_start` correctly on other Mac terminals? COUNSELOR exhausted reasonable hypotheses without evidence. Candidates not tested:
  - DA (Device Attributes) response — ours is minimal `\x1b[?62;4c`. xterm's is richer. Shell didn't query DA in this session, so probably not the cause.
  - Terminfo capabilities advertised via TERM=xterm-256color — standard, no reason this differs.
  - Shell integration OSC 133;B (end-of-prompt marker) — our zsh integration script does not emit it. If added (via `zle-line-init` widget), parser could record "input start row" at 133;B firing and clamp CUU to not cross it.
- Is there actually a parser-side shadow state COUNSELOR missed? Re-read with fresh eyes recommended.

### Next Steps
1. Fresh COUNSELOR session re-reads Parser.cpp / ParserVT.cpp / ParserCSI.cpp / ParserOps.cpp / ParserESC.cpp looking specifically for: any write to cursor/wrapPending/scroll margins NOT via `state.setCursor*` / `state.setWrapPending` / `state.setScroll*`. Any local cursor cache that persists beyond one `processGroundChunk` call. Any parser member field that duplicates `Terminal::State` cursor data.
2. If no shadow state found, test OSC 133;B emission (modify `Source/terminal/shell/zsh_end_integration.zsh` to emit `\033]133;B\007` at end of PS1 via `zle-line-init`) AND extend `handleOsc133` in `ParserESC.cpp:849` to record input-start-row. Then clamp CUU to not cross this row.
3. Reference log files (preserved from bug repro): `bytes.log`, `output.log`, `screen.log`, `state.log` at project root. Delete only after next session extracts what it needs.

---

## Sprint 18: Cross-Instance Window-Size Persistence via ~/.config/end/window.state ✅

**Date:** 2026-04-15
**Duration:** brief session

### Agents Participated
- COUNSELOR: session lead, scope gating (ARCHITECT clarified "NEW INSTANCE ONLY" precedence rule after two plan iterations), NAMES Rule -1 gate on `windowSaveSize` / `getWindowState` / `saveWindowState` / `loadWindowState`
- Pathfinder: HEAD survey of window-size path (Config defaults, AppState WINDOW ValueTree, MainComponent ctor sizing, systemRequestedQuit save, daemon vs standalone state files, jreng::Window API surface)
- Engineer: single delegation — six-site implementation across Config / AppState / Main / lua template

### Files Modified
- `Source/config/Config.h:308-309` — added `Key::windowSaveSize` identifier ("window.save_size") adjacent to `windowForceDwm` with doxygen brief
- `Source/config/Config.cpp:145` — registered default value `"true"` for `Key::windowSaveSize` alongside other `window.*` keys
- `Source/config/default_end.lua:274-281` — added `save_size = "%%window_save_size%%"` line inside the `window = { ... }` block with doc comment spelling out the read/write gating and the "restored sessions ignore this" caveat
- `Source/AppState.h:187-209` — declared `saveWindowState()` and `loadWindowState()` next to existing `save()` / `load()`; doxygen @note MESSAGE THREAD, behavior spec for each
- `Source/AppState.h:244` — declared `juce::File getWindowState() const` next to `getStateFile()` / `getNexusFile()`
- `Source/AppState.cpp:486-490` — `getWindowState()` returns `~/.config/end/window.state` (sibling of end.state, not nested under nexus/)
- `Source/AppState.cpp:492-504` — `saveWindowState()` creates parent dir, serializes `state.getChildWithName(App::ID::WINDOW)` XML via `createXml()->writeTo()`, positive nested guard on window validity + xml creation
- `Source/AppState.cpp:506-528` — `loadWindowState()` parses file, validates `parsed.getType() == App::ID::WINDOW`, copies `width` / `height` properties into current WINDOW node; silent no-op on missing file / parse failure / type mismatch
- `Source/Main.cpp:324-328` — standalone branch: `const bool hadState { appState.getStateFile().existsAsFile() }` captured BEFORE `appState.load()`; post-load conditional `if (not hadState and cfg->getBool (Config::Key::windowSaveSize)) appState.loadWindowState();`
- `Source/Main.cpp:338-342` — daemon-client branch: identical capture-before-load + conditional loadWindowState() pattern, placed after `setInstanceUuid` / `setDaemonMode` so `getStateFile()` resolves the `<uuid>.display` path correctly
- `Source/Main.cpp:476-477` — inside existing `if (mainWindow != nullptr)` guard in `systemRequestedQuit`, added `if (Config::getContext()->getBool (Config::Key::windowSaveSize)) appState.saveWindowState();` — runs after existing `setWindowSize` so the current resize is persisted before XML dump

### Alignment Check
- [x] BLESSED — `B` (Bound): new file I/O scoped to AppState, no new owners; `L` (Lean): three small methods + one config key, WINDOW ValueTree reused (no parallel size store); `E` (Explicit): config key gates read and write symmetrically, positive nested checks throughout, no early returns, doxygen @note MESSAGE THREAD on every new AppState method; `S` (SSOT): WINDOW ValueTree remains the sole in-memory truth for size — window.state is a serialization sink, end.state WINDOW overrides it on load; `S` (Stateless): no new members, no flags, no machinery state; `E` (Encapsulation): Main calls AppState verbs (`saveWindowState` / `loadWindowState` / `getStateFile`), never pokes internals; `D` (Deterministic): precedence rule is a pure function of file existence + config bool
- [x] NAMES.md — Rule -1 gated through ARCHITECT; `windowSaveSize` (bool, is-prefix-free adjective matches existing `windowForceDwm`/`windowAlwaysOnTop` pattern), `getWindowState` (verb + noun, returns juce::File), `saveWindowState` / `loadWindowState` (symmetric verbs matching existing `save()` / `load()` family), `window.save_size` (lua dot-convention, `save_size` snake_case inside window table)
- [x] MANIFESTO.md / JRENG-CODING-STANDARD.md — `not`/`and`/`or` alternative tokens (Main.cpp:327,341), brace init (`const bool hadState { ... }`, `const juce::File file { ... }`), positive nested checks in both saveWindowState and loadWindowState, no early returns, no anonymous namespaces, no `namespace detail`

### Problems Solved
- **Cross-instance size memory**: new END instance (no prior session) now inherits the last-closed size from any peer instance via the shared `window.state` file, instead of always falling back to `Config::Key::windowWidth` / `windowHeight` defaults
- **Precedence clarity**: explicit rule — `end.state` / `<uuid>.display` WINDOW (session restore) > `window.state` (cross-instance) > Config defaults. Restored sessions NEVER consult `window.state` for reading, per ARCHITECT directive
- **Config-driven opt-out**: `window.save_size = "false"` fully disables the feature (no reads, no writes, no stale file creation). Existing `window.state` left alone when disabled — never auto-deleted
- **Symmetric file I/O**: save and load both gated by the same config key; write happens on every quit regardless of daemon/standalone, matching ARCHITECT's "always save on quit" requirement

### Debts Paid
None — this sprint introduces a new feature, does not close any DEBT.md entry.

### Debts Deferred
None — no out-of-scope findings surfaced during this sprint.

---

## Sprint 17: jreng::Window Self-Managed Dual Backend + jreng_opengl Dissolved into jreng_gui ✅

**Date:** 2026-04-14
**Duration:** full session

### Agents Participated
- COUNSELOR: session lead, RFC-less plan grounded in two-edit probe baseline, four-step PLAN, decision-gate orchestration, audit triage
- Pathfinder: HEAD survey (×3 — initial GL surface, pane/renderer/font lifecycle, jreng_opengl module footprint), git-log + DEBT.md inventory
- Librarian: JUCE OpenGL design (Context↔Renderer cardinality, setNativeSharedContext / wglShareLists semantics, setComponentPaintingEnabled mechanism, nesting prohibition)
- Engineer: ~9 delegations (S0 module dissolution, S1 dead-code removal, S2 API extension, S3 atomic migration, focus-grab fixes, audit-fix batch, GLOverlay rename)
- Auditor: full S0–S4 audit with entanglement scope (1 Critical, 2 Major, 7 Minor — all resolved)

### Files Modified

**Module restructure (S0):**
- `modules/jreng_opengl/` deleted entirely; 27 files relocated via `git mv` to `modules/jreng_gui/opengl/{context,renderers,shaders}/`
- `modules/jreng_gui/jreng_gui.h` — added `juce_opengl, jreng_graphics` deps; appended absorbed module includes prefixed with `opengl/`
- `modules/jreng_gui/jreng_gui.cpp` — appended absorbed `.cpp` unity-build includes
- `modules/jreng_gui/jreng_gui.mm` — added `opengl/context/jreng_gl_renderer_mac.mm` under JUCE_MAC
- `Builds/Ninja/END_App_artefacts/JuceLibraryCode/JuceHeader.h` — removed jreng_opengl include
- `CMakeLists.txt:44` — removed jreng_opengl from JUCE_MODULES
- `END.jucer` — removed jreng_opengl module entry
- `ARCHITECTURE.md`, `SPEC.md`, `PLAN-WHELMED.md` — prose path updates

**Whelmed font dead-code removal (S1):**
- `Source/MainComponent.h:158,161,164` — deleted whelmedBodyFont + whelmedCodeFont members; collapsed glRenderer NSDMI initializer to `{ &typeface }`
- `Source/MainComponent.cpp:60-69,374` — deleted both Typeface ctor inits; updated initialiseTabs to two-arg Tabs ctor
- `Source/component/Tabs.h/.cpp` — dropped whelmed params from ctor signature + members; updated internal Panes ctor call
- `Source/component/Panes.h/.cpp` — dropped whelmed params from ctor signature + members
- Net: −22 LOC, zero behavior change (Whelmed::Component reads own Whelmed::Config keys)

**jreng::Window API extension (S2):**
- `modules/jreng_gui/window/jreng_window.h/.cpp` — added private `glRenderer` (uptr) + `nativeSharedContext` (void*) members; public `setRenderer`, `setNativeSharedContext`, `getNativeSharedContext`, `triggerRepaint`; private `attachRendererToContent` helper enforcing F9 pre-attach order
- `modules/jreng_gui/opengl/context/jreng_gl_renderer.h/.cpp` — added `setNativeSharedContext(void*)`, `getNativeContext()`, `isAttached()` passthroughs

**Atomic migration (S3):**
- `modules/jreng_gui/window/jreng_window.h/.cpp` — deleted `setGpuRenderer(bool)` + `gpuRenderer` member; swapped `visibilityChanged` GPU-branch trigger to `if (glRenderer != nullptr)`; inserted `attachRendererToContent()` call before `setGlass`
- `Source/Main.cpp:343-359` — captured `mainComponent` ptr, deleted `setGpuRenderer` call, inserted `mainComponent->applyConfig()` after `setGlass` (decision P)
- `Source/MainComponent.h:164` — deleted `glRenderer` member
- `Source/MainComponent.cpp` — deleted `applyConfig()` from ctor body; rewrote `setRenderer` to `make_unique<GLAtlasRenderer>({&typeface})` + iterator config + `window->setRenderer(std::move(...))`; deleted `glRenderer.detach()` from dtor; deleted `glRenderer.setComponentIterator` block from `initialiseTabs`; rewrote `tabs->onRepaintNeeded` to call `window->triggerRepaint()` via dynamic_cast lookup
- `Source/component/Popup.h/.cpp` — collapsed two `show()` overloads into single 5-arg form taking `std::unique_ptr<jreng::GLRenderer>`; extracts shared-context handle from caller's top-level Window
- `Source/component/ModalWindow.h/.cpp` — deleted `ContentView` struct entirely; collapsed two ctors into single ctor accepting renderer uptr + shared-context handle; routed through base Window's `setNativeSharedContext` + `setRenderer`; removed `setGpuRenderer(false)`/`setOpaque(true)`/`setBackgroundColour` block; relocated Display::onRepaintNeeded wiring
- `Source/MainComponentActions.cpp:154,435` — both popup call sites updated to construct appropriate renderer subclass conditional on AppState::getRendererType
- `Source/component/TerminalDisplay.cpp:500-513` — added `parentHierarchyChanged` override that grabs keyboard focus on Display itself (async via `MessageManager::callAsync` + `SafePointer<Display>` guard, deferring past `enterModalState`); removed external focus-grab from ModalWindow

**Focus + iterator patches:**
- `Source/component/ModalWindow.cpp` — deleted async focus-grab block (moved into Display); added per-frame iterator wiring for GLComponent content via `setRenderable` static helper

**Doxygen (S4):**
- `modules/jreng_gui/window/jreng_window.h` — `@par Render backend` section documenting dual-backend contract
- `modules/jreng_gui/opengl/context/jreng_gl_component.h` — `@class GLComponent` doxygen formalizing paint()-baseline + paintGL()-optimization

**Audit fixes:**
- `modules/jreng_gui/opengl/context/jreng_gl_renderer.h/.cpp` — renamed `setComponentIterator` → `setRenderables` (instance, plural); added public static `setRenderable(GLRenderer&, GLComponent&)` single-content convenience
- `modules/jreng_gui/opengl/context/jreng_gl_overlay.h` — full rename to match (`setComponentIterator` → `setRenderables`); plugin API breaking change accepted by ARCHITECT
- `Source/MainComponent.cpp` — uses `setRenderables`
- `Source/component/ModalWindow.cpp` — uses `jreng::GLRenderer::setRenderable(*renderer, *glContent)` static helper
- `modules/jreng_gui/window/jreng_window.cpp:104-110` — `getNativeSharedContext` rewritten to positive nested + single return (Critical fix)
- `modules/jreng_gui/window/jreng_window.cpp:82-84` — orphan `// Renderer type` banner deleted
- `modules/jreng_gui/window/jreng_window.h:24,116,191,210` — stale macOS-only `@note` replaced; "after S3 trigger swap" sprint-state parentheticals stripped (3 sites)
- `Source/component/Tabs.h:65-67` — onRepaintNeeded doxygen updated to `Window::triggerRepaint()` with CPU-mode no-op note
- `ARCHITECTURE.md:479` — `modules/jreng_opengl/` → `modules/jreng_gui/opengl/context/`

### Alignment Check
- [x] BLESSED principles followed — `B` (Bound): Window owns renderer via unique_ptr, RAII detach in dtor; `L` (Lean): net deletion, ContentView gone, two ctors collapse to one, two show overloads to one, setGpuRenderer/gpuRenderer/MainComponent::glRenderer all deleted; `E` (Explicit): caller intent in unique_ptr present/null, all new methods documented with @param/@note, positive nested checks, no early returns; `S` (SSOT): render mode = renderer presence on Window, no parallel boolean; `S` (Stateless): orchestrator TELLs setRenderer, Window self-manages attach lifecycle; `E` (Encapsulation): GL wiring lives inside jreng_gui/window/, callers see only Window verbs; `D` (Deterministic): same content + same renderer-presence yields same render path
- [x] NAMES.md adhered — `setRenderer`, `setNativeSharedContext`, `getNativeSharedContext`, `getNativeContext`, `triggerRepaint`, `attachRendererToContent`, `isAttached`, `setRenderables`, `setRenderable`, `bindContentToRender` (rejected during deliberation in favor of setRenderable), `glRenderer`, `nativeSharedContext` — all gated through ARCHITECT approval per Rule -1; all verbs-for-functions, nouns-for-members, no type-encoding suffixes, consistent with JUCE's `setNativeSharedContext`/`getRawContext` naming
- [x] MANIFESTO.md / JRENG-CODING-STANDARD.md — alternative tokens (`not`, `and`, `or`); brace init; `noexcept`; positive nested checks; no early returns; no anonymous namespaces; no `namespace detail`
- [x] Sprint 16 failure filter (F1-F10) honored — F1/F9: jreng::GLRenderer ctor's 5 config calls (setPixelFormat, setOpenGLVersionRequired, setRenderer(this), setComponentPaintingEnabled(false), setContinuousRepainting(false)) untouched throughout sprint; F4: only ARCHITECT ran build.bat; F5: S3 atomic batch; F7: CPU branch body in Window::visibilityChanged unchanged from HEAD; F10: exactly one attachTo site (Window::attachRendererToContent)

### Problems Solved
- **Action::List CPU-mode rendering**: probe-validated baseline (plain juce::Component on glass via setComponentPaintingEnabled(true) rasterizing JUCE tree into GL FBO) relocated from Terminal::ModalWindow::ContentView into jreng::Window itself
- **API encapsulation**: callers no longer touch attachTo, setComponentPaintingEnabled, setSharedRenderer, setNativeSharedContext on the renderer object — Window does it
- **Mode-check duplication eliminated**: AppState::getRendererType reads remain at construction sites only; the GPU-vs-CPU branch in visibilityChanged lives ONCE in Window
- **Module dependency hygiene**: dissolving jreng_opengl into jreng_gui/opengl/ eliminates the rejected jreng_gui→jreng_opengl dep entirely (Sprint 16 F6)
- **Whelmed coupling broken**: MainComponent no longer holds zombie whelmed Typeface scaffolding; Whelmed::Component's font lifecycle is fully self-contained via Whelmed::Config
- **Display popup focus**: relocated from ModalWindow's async grab into Terminal::Display::parentHierarchyChanged (deferred via MessageManager::callAsync + SafePointer to clear modal-state-entry timing)
- **Iterator API confusion**: setComponentIterator (named after parameter type) renamed to setRenderables (named after semantic role); new static setRenderable convenience wraps the single-content modal case

### Debts Paid
None — none of the 4 open DEBT.md entries map to this sprint's scope.

### Debts Deferred
None — no out-of-scope findings pushed to DEBT.md during this sprint.

---

## Sprint 16: GOBLOK — Window-Owned GL Context Refactor (FAILED, REVERTED)

**Date:** 2026-04-14
**Duration:** full session
**Outcome:** Hard reset to HEAD. No code shipped. This entry is the filter for next COUNSELOR.

### Agents Participated
- COUNSELOR: session lead, PLAN-window-context-ownership.md, repeated micro-fix cycles
- Pathfinder: HEAD/current survey, GL lifecycle trace (multiple rounds)
- Librarian: JUCE `OpenGLContext` internals, D2D backend, Windows peer `computeTransparencyKind`, PopupMenu paint path, DWM DComp
- Oracle: two deep-analysis rounds — both produced plausible hypotheses acted on without runtime verification
- Researcher: DWM acrylic + GDI alpha docs, Windows Terminal AtlasEngine / DirectComposition patterns
- Engineer: ~12 delegations — additive Window API, GLRenderer decoupling, downstream migration, multiple reactive micro-fixes
- Auditor: one full-sprint audit, caught control-flow, jassert idiom, and stale-dep findings

### Files Modified (0 shipped — all reverted)
- `PLAN-window-context-ownership.md` (root) — deleted by revert
- `Source/component/ConfirmationDialog.cpp/.h` (new, untracked) — deleted by revert
- Touched but reverted: `Source/Main.cpp`, `Source/MainComponent.cpp/.h`, `Source/MainComponentActions.cpp`, `Source/action/ActionList.cpp/.h`, `Source/component/LookAndFeel.cpp/.h`, `Source/component/ModalWindow.cpp/.h`, `Source/component/Popup.cpp/.h`, `Source/config/Config.cpp/.h`, `Source/config/default_end.lua`, `Source/interprocess/Link.cpp/.h`, `Source/nexus/Nexus.cpp/.h`, `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp`, `modules/jreng_graphics/glyph/jreng_atlas_glyph.h`, `modules/jreng_gui/jreng_gui.h`, `modules/jreng_gui/window/jreng_background_blur.cpp`, `modules/jreng_gui/window/jreng_window.cpp/.h`, `modules/jreng_opengl/context/jreng_gl_overlay.h`, `modules/jreng_opengl/context/jreng_gl_renderer.cpp/.h`

### Alignment Check
- [x] Changes reverted — repo matches HEAD `08c396f`
- [x] Failure filter captured for next sprint
- [ ] Objective not achieved
- [ ] DEBT-20260412T234116 (confirmation dialog on Ctrl+Q) remains UNPAID — reverted with sprint

### Problems Solved
None.

### Problems Caused
- Wasted session tokens and ARCHITECT time
- Six+ reactive micro-fixes on a broken foundation
- Author's disposable revert script blew away own Sprint 16 entry on first write (uncommitted); had to be rewritten under `/log`

### Debts Paid
None.

### Debts Deferred
None. DEBT-20260412T234116 (confirmation dialog) was attempted in scope, reverted, remains open in DEBT.md.

---

### Causal chain (hypothesis — not runtime-verified)

Attempted refactor: move `juce::OpenGLContext` ownership from `jreng::GLRenderer` into `jreng::Window` as `std::optional<juce::OpenGLContext>`. Establish Context-is-per-HWND / Renderer-is-callback contract.

**Root cause of persistent blank-window + focus-loss:** Step 2 deleted HEAD's `GLRenderer` ctor context configuration (`setPixelFormat(stencilBits=8)`, `setOpenGLVersionRequired(openGL3_2)`, `setContinuousRepainting(false)`). Default-emplaced Context in new `Window::setGpuRenderer(true)` lacked them. GLSL 3.2+ shaders failed to compile silently; `flatColourShader` stayed null; `renderOpenGL` skipped its render block; blank. Focus loss likely secondary GL child-HWND mis-init.

Late restoration via `GLRenderer::configureContext(...)` called from `MainComponent::setRenderer` did NOT resolve the symptom when ARCHITECT rebuilt. Additional contributing factors (attach target choice, DWM acrylic coexistence with `WS_EX_LAYERED`, content-vs-window paint pipeline) were hypothesized repeatedly but never definitively isolated. Session ended with revert.

---

### FILTER FOR NEXT COUNSELOR — F1–F10

**F1. READ HEAD BEFORE CHANGING IT.** Before Step 1 of any ownership/lifecycle refactor, `git show HEAD:<file>` on every touched file AND every caller. Diff HEAD vs. current mentally. Enumerate every HEAD config call; guarantee a migration path for each. GL example: HEAD `GLRenderer` ctor does FIVE things (`setPixelFormat`, `setOpenGLVersionRequired`, `setRenderer`, `setComponentPaintingEnabled`, `setContinuousRepainting`). All five must be migrated, not just the obvious one.

**F2. NEVER DELEGATE UNDERSTANDING.** `@Oracle`/`@Librarian` produce hypotheses, not truth. Treat their output as a lead to verify by reading source yourself. Do not approve fixes based on agent diagnosis alone.

**F3. WHEN ARCHITECT SAYS "FROM STEP 1", REVERT TO STEP 1 AND VERIFY.** Not a diagnostic mystery — a direct instruction that the foundation is broken. First response: `git stash` / revert, rebuild, confirm symptom clears or persists. Only proceed if foundation is confirmed clean. Micro-fixing on top of a broken foundation is the canonical wrong move.

**F4. DO NOT ACCEPT "BUILD SUCCEEDED" WITHOUT EVIDENCE.** END uses `build.bat` (MSVC via Windows cmd). Bash-based Engineer sessions cannot invoke it. Engineer claims of "build succeeded" from bash are unverified. Only ARCHITECT's `build.bat` run counts.

**F5. GATE VALIDATION ON COMPILATION, NOT STATIC REVIEW.** Steps that remove public API must gate on ARCHITECT's build before the next step. Auditor static review is necessary but not sufficient. "Build breaks here, fixed in later step" is a signal to BATCH those steps, not proceed on faith.

**F6. BLESSED SIMPLICITY FIRST.** First instincts this sprint (all rejected by ARCHITECT): `Window::setComponentIterator`, `Window::attachRenderer(jreng::GLRenderer&)`-with-jreng-type, `jreng_gui → jreng_opengl` module dependency, `GLOverlay::setHostWindow`, `dynamic_cast`-based modal iterator. Default to the simplest shape satisfying BLESSED. If it feels clever or elegant, it is probably adding coupling. Caller uses JUCE native API on returned observers when possible — wrappers are rarely needed.

**F7. DWM ACRYLIC + GDI CONTENT IS ARCHITECTURALLY INCOMPATIBLE.** `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})` + `ACCENT_ENABLE_ACRYLICBLURBEHIND` makes DWM composite per-pixel alpha. GDI writes RGB without alpha → pixels land `alpha=0` → composited fully transparent → invisible. Only alpha-aware pipelines survive (GL with `setComponentPaintingEnabled(true)`, D2D via DComp composition surface). `juce::PopupMenu` renders on glass because its window has no native title bar → JUCE `computeTransparencyKind` returns `perPixelTransparent` → `UpdateLayeredWindow` preserves alpha. `juce::DocumentWindow` with `setUsingNativeTitleBar(true)` cannot take that path — JUCE asserts it (`juce_Windowing_windows.cpp:2620`).

**F8. `juce::OpenGLContext::getContextAttachedTo(Component&)` DOES NOT WALK PARENT CHAIN.** Returns context directly attached to the queried component; `nullptr` otherwise. Do not use it to discover an ancestor Window's context from a child component.

**F9. JUCE PRE-ATTACH CONSTRAINTS ARE ASSERTED.** `setRenderer`, `setNativeSharedContext`, `setComponentPaintingEnabled`, `setPixelFormat`, `setOpenGLVersionRequired` — all assert `nativeContext == nullptr`. Must be called BEFORE `attachTo`. Any API exposing attach/configure separately must document and `jassert` the ordering; orchestrator must respect it.

**F10. ONE HWND = ONE OpenGLContext.** Double-attach during refactor (e.g. residual `glRenderer.attachTo(*this)` coexisting with new `window.attachRenderer(glRenderer)`) is UB. During Context ownership relocation, exhaustively grep every `attachTo` site AFTER removal and confirm exactly one context attaches per HWND.

---

## Sprint 15: Action List Window — Glass Modal + Plain Component Refactor

**Date:** 2026-04-13
**Duration:** ~10:00

### Agents Participated
- COUNSELOR: session lead, /pay planning, architecture analysis, DWM glass root cause investigation, PLAN-window-glass-modes.md
- Pathfinder: codebase survey (action list, popup, window module, glass pipeline)
- Librarian: JUCE internals — DWM glass + software rendering incompatibility (computeTransparencyKind, StretchDIBits alpha)
- Researcher: JUCE Windows peer rendering pipeline, child HWND patterns, DWM composition research
- Engineer: all code implementation (~25 delegations)
- Auditor: full BLESSED/NAMES/CODING-STANDARD sweep (19 findings)

### Files Modified (80+ total)

**jreng::Window glass modes:**
- `modules/jreng_gui/window/jreng_window.h` — `setGlass(Colour, float blur)` (removed opacity param), `setGpuRenderer(bool)`, `gpuRenderer` member
- `modules/jreng_gui/window/jreng_window.cpp` — GPU/CPU branching in `visibilityChanged()`, DWM rounded corners decoupled from glass, CPU path = solid opaque + rounded corners

**setGlass API change:**
- `modules/jreng_gui/window/jreng_window.h/.cpp` — 2-param signature, colour alpha carries tint
- `Source/Main.cpp:348,396` — callers updated to `.withAlpha(opacity)`
- `Source/component/ModalWindow.cpp:78` — caller updated
- `Source/Gpu.h` — `resolveOpacity` removed (dead code)

**Module rename:**
- `modules/jreng_gui/glass/` renamed to `modules/jreng_gui/window/`
- `modules/jreng_gui/jreng_gui.h/.cpp/.mm` — include paths updated
- `modules/jreng_gui/window/jreng_child_window.h/.cpp/.mm` — deleted (unused)

**Terminal::ModalWindow (extracted from Popup::Window):**
- `Source/component/ModalWindow.h` — new file, GL and non-GL constructors, ContentView (GL only), setupWindow shared helper
- `Source/component/ModalWindow.cpp` — new file, full implementation

**Popup refactor:**
- `Source/component/Popup.h` — removed ContentView + Window structs, uses Terminal::ModalWindow, `setTerminalSession` setter (was public member), non-GL `show()` overload
- `Source/component/Popup.cpp` — removed ContentView + Window impl, non-GL show() implementation

**Action::List refactor (ModalWindow subclass -> plain Component):**
- `Source/action/ActionList.h` — plain `juce::Component` + `juce::ValueTree::Listener`, full doxygen, `onActionRun` callback, minimum size 600x400
- `Source/action/ActionList.cpp` — self-contained construction, `valueTreePropertyChanged`, direct member access, size clamped at minimum
- `Source/action/ActionRow.h` — removed NameLabel/ShortcutLabel type tags, plain juce::Label with ID::font property
- `Source/action/ActionRow.cpp` — property-based font dispatch via `getProperties().set(ID::font, ...)`
- `Source/action/LookAndFeel.h/.cpp` — deleted (merged into Terminal::LookAndFeel)
- `Source/action/KeyHandler.h/.cpp` — removed dismiss callback from Callbacks struct

**Font dispatch:**
- `Source/component/LookAndFeel.h` — `getLabelFont` override, `separatorAlpha` constant, ColourIds JUCE convention comment
- `Source/component/LookAndFeel.cpp` — `getLabelFont` implementation (property-based dispatch via ID::font/ID::name/ID::keyPress)
- `modules/jreng_core/identifier/jreng_identifier_core.h` — added `keyPress` identifier

**MainComponent:**
- `Source/MainComponent.h` — removed `actionList` member
- `Source/MainComponentActions.cpp` — uses `popup.show()` non-GL overload, `onActionRun` + `onDismiss` wired to `popup.dismiss()`
- `Source/Main.cpp` — `setGpuRenderer` call on main window

**Escape/dismiss restore + close-on-run:**
- `Source/action/KeyHandler.h` — restored `dismiss` callback in Callbacks struct
- `Source/action/KeyHandler.cpp` — restored Escape in key tables (search mode → select row 1, navigation mode → dismiss)
- `Source/action/ActionList.h` — `onDismiss` + `onActionRun` callbacks, minimum size 600x400
- `Source/action/ActionList.cpp` — KeyHandler wired with dismiss, executeSelected fires `onActionRun` on close-on-run
- `Source/component/ModalWindow.cpp` — `keyPressed` returns false (content owns all keys), non-GL constructor pre-sets opaque background before setVisible to prevent white flash

**Audit fixes (19 findings):**
- `modules/jreng_gui/window/jreng_background_blur.h` — removed `const` on return-by-value, space before `//`
- `modules/jreng_gui/window/jreng_background_blur.cpp` — C-style casts -> static_cast, brace init, explicit bitwise bool, enum class ACCENT_STATE, extracted `applyAccentPolicy`+`buildAbgrColorref` helpers, named DWM constants
- `modules/jreng_gui/window/jreng_window.cpp` — named DWM constants
- `Source/component/LookAndFeel.cpp` — JUCE API comment on raw new, extracted `applyVerticalTabTransform`+`truncateTabText`+`drawPopupMenuSeparator`+`drawSubmenuArrow` helpers

**Codebase-wide:**
- 63 files: `}//` -> `} //` spacing fix

### Alignment Check
- [x] BLESSED principles followed — E (Explicit): setGlass no magic opacity threshold, named DWM constants, property-based font dispatch; S (SSOT): AppState is renderer type SSOT, font config read once in getLabelFont, no duplicated ABGR conversion; E (Encapsulation): jreng::Window self-manages GPU/CPU mode, Action::List is plain component, Terminal::ModalWindow encapsulates glass setup; B (Bound): ModalWindow owns content lifecycle, RAII cleanup; L (Lean): extracted helpers for 30-line compliance, deleted dead LookAndFeel/ChildWindow files
- [x] NAMES.md adhered — `onActionRun`, `setGpuRenderer`, `gpuRenderer`, `minimumWidth`/`minimumHeight`, `setupWindow`, `hasChildPeer` (removed), `ContentView` (follows Popup pattern)
- [x] MANIFESTO.md principles applied — no early returns, alternative tokens, brace init, const, .at() access, explicit nullptr checks

### Problems Solved
- **Action list rendered solid white on Windows:** Root cause was Action::LookAndFeel replacing WindowsTitleBarLookAndFeel (which suppressed title bar paint). Fixed by merging into Terminal::LookAndFeel with property-based font dispatch.
- **DWM glass + software rendering incompatible:** JUCE software renderer writes Image::RGB with zero alpha via StretchDIBits. DWM reads alpha=0 as transparent. GL renderer writes correct alpha. Resolution: CPU path uses solid opaque background + DWM rounded corners. GPU path unchanged.
- **Action::List architecture:** Refactored from ModalWindow subclass to plain Component hosted via Popup::show(). Eliminated window construction complexity, content sizing chicken-and-egg problems, and L&F conflicts.
- **Escape two-state behavior restored:** Search mode Escape → selection mode. Selection mode Escape → dismiss. Broken by premature removal of KeyHandler dismiss callback, restored.
- **White flash on open:** Pre-set opaque background colour before setVisible in non-GL ModalWindow constructor.

### Debts Paid
- `DEBT-20260411T083120` — action list on windows rendered solid white — resolved via Terminal::ModalWindow extraction + jreng::Window GPU/CPU modes + plain component refactor

### Debts Deferred
- None

## Sprint 14: Fallback Advance + Zoom + Non-Destructive Reflow + Daemon Lifecycle

**Date:** 2026-04-12 / 2026-04-13
**Duration:** ~20:00

### Agents Participated
- COUNSELOR: session lead, SSOT analysis, zoom architecture (multiple iterations), daemon lifecycle design, ODE intervention
- Pathfinder: zoom call-graph trace, windowsGlyphScale consumer mapping, NF init desync root cause, AppState zoom persistence check, calcMetrics cache analysis
- Researcher: fallback advance handling across 4 reference terminals (local source), zoom handling in reference terminals, tmux daemon lifecycle patterns
- Engineer: all code implementation (~30 delegations)
- Auditor: comprehensive audit of Sprints 10-14 (BLESSED, SSOT, stale docs, coding standard)

### Files Modified (30+ total)

**Fallback advance (Phase 3 + Phase 2):**
- `modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp:306-320` — force `xAdvance = primaryFace->size->metrics.max_advance >> 6` for all fallback glyphs; doxygen updated
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.cpp:449-490` — oversize rescale when bitmap.width > effectiveWidth on scalable faces; FT_Set_Char_Size restore (not FT_Set_Pixel_Sizes); vertical centering via originalBitmapTop/Rows

**SSOT windowsGlyphScale:**
- `modules/jreng_graphics/fonts/jreng_typeface.h` — `computeRenderDpi` declaration, `getBaseFontSize` declaration
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:86-111` — `computeRenderDpi` definition, `windowsGlyphScale` constant
- `modules/jreng_graphics/fonts/jreng_typeface.cpp:300,511,942` — all `FT_Set_Char_Size` sites routed through `computeRenderDpi`; `windowsGlyphScale` applied to text faces only (slots 0-3), NF/emoji/fallback use full `baseDpi * displayScale`; `cachedMetricsSize` invalidated in `setSize`; 26.6 conversion unified to `roundFloatPxTo26_6`

**Typeface init fix:**
- `Source/MainComponent.cpp:57` — pass `dpiCorrectedFontSize()` to Typeface constructor instead of raw `config.getFloat(Key::fontSize)`

**Zoom (wezterm Mode A):**
- `Source/component/TerminalDisplay.cpp:700-740` — `applyZoom` reads SSOT (Config x AppState), computes ratio from `font.calcMetrics`, resizes window proportionally; font size from `getBaseFontSize` (pre-zoom) vs SSOT (post-zoom)
- `Source/component/TerminalDisplay.h:68-70,420-432` — updated `@par Zoom` and `applyZoom` doxygen
- `Source/component/Tabs.cpp:491-533` — `Config::zoomStep` constant, `resetZoom` uses `Config::zoomMin`
- `Source/component/Tabs.h:252-276` — updated zoom method doxygen
- `Source/config/Config.h:75` — `static constexpr float zoomStep { 0.25f }`
- `Source/config/Config.h` — removed `Config::Key::windowZoom`
- `Source/config/Config.cpp` — removed `addKey(Key::windowZoom, ...)`
- `Source/config/default_end.lua` — removed `zoom` entry from window table
- `Source/AppState.cpp:79,494` — `getWindowZoom` fallback and `initDefaults` use `Config::zoomMin`
- `Source/component/PaneComponent.h:74-79` — `virtual void applyZoom(float) noexcept = 0`
- `Source/whelmed/Component.h` — no-op `applyZoom` override

**Non-destructive reflow:**
- `Source/terminal/logic/GridReflow.cpp` — `reflowPass()` replaces duplicate `countOutputRows`/`writeReflowedContent` (SSOT); `effectiveLen = flatLen` unconditionally (no hard-line truncation); `maxOffset = flatLen - 1` (no cursor clamping); `pinToBottom` removed; `emitLogicalLine` dead code deleted; file-level doxygen updated

**Screen:**
- `Source/terminal/rendering/Screen.h` — `getBaseFontSize()` accessor
- `Source/terminal/rendering/Screen.cpp` — `getBaseFontSize()` implementation; `calc()` unchanged; `setFontSize` with `calc()` restored

**Daemon lifecycle:**
- `Source/interprocess/Daemon.cpp:134-143` — removed `onAllSessionsExited` from `removeConnection`; `stop()` no longer calls `releasePlatformProcessCleanup`
- `Source/interprocess/DaemonWindows.cpp` — `JOB_OBJECT_LIMIT_BREAKAWAY_OK` on daemon job; `CREATE_BREAKAWAY_FROM_JOB` on daemon spawn
- `Source/nexus/Nexus.cpp:168` — `attachedLink->sendRemove(uuid)` in `Nexus::remove`
- `Source/Main.cpp` — GUI Job Object with `KILL_ON_JOB_CLOSE + BREAKAWAY_OK` for OpenConsole cleanup

**Keyboard protocol rename:**
- `Source/terminal/data/Keyboard.h` — `disambiguate`, `allKeys`, `progressiveKeyCodes` (removed all external terminal references)
- `Source/terminal/data/State.h`, `State.cpp`, `Identifier.h`, `Parser.h`, `ParserCSI.cpp`, `ParserVT.cpp` — doxygen updated
- `ARCHITECTURE.md`, `README.md` — protocol references updated
- `carol/SPRINT-LOG.md` — historical references updated
- `gen/gen_char_props.py` — references updated

**Cleanup:**
- Deleted `PLAN-fix-zoom.md`, `test-fallback-advance.sh`
- Dead `Display::increaseZoom/decreaseZoom/resetZoom` removed
- All stale doxygen fixed per audit

### Alignment Check
- [x] BLESSED principles followed — D (Deterministic): non-destructive reflow, zoom roundtrip preserves content; S (SSOT): single `reflowPass`, zoom from AppState, `computeRenderDpi` for all DPI sites, `Config::zoomStep` constant, no shadow copies; E (Encapsulation): `windowsGlyphScale` text-only, zoom doesn't poke grid; L (Lean): dead code removed (Display zoom triad, emitLogicalLine, Config::Key::windowZoom)
- [x] NAMES.md adhered — `computeRenderDpi`, `getBaseFontSize`, `zoomStep`, `reflowPass`, `disambiguate`, `allKeys`, `progressiveKeyCodes` all follow naming rules; `zoomFactor`/`setZoomFactor` introduced and removed within same sprint
- [x] MANIFESTO.md principles applied — no early returns; alternative tokens; brace init; const where practical

### Problems Solved
- **Fallback advance:** Proportional system fallback fonts broke monospace grid. Forced cell-width advance matching all 4 reference terminal implementations. Oversize bitmaps rescaled via FT_Set_Char_Size (not FT_Set_Pixel_Sizes) with vertical centering.
- **Zoom determinism:** Zoom in+out now preserves grid content. Wezterm Mode A: window resizes to maintain cols/rows. No grid mutation from zoom. SIGWINCH fires with same dimensions — harmless.
- **Non-destructive reflow:** Hard lines wrap on shrink, unwrap on grow. Single `reflowPass` eliminates count/write SSOT violation. `pinToBottom` removed — cursor stays content-relative.
- **windowsGlyphScale text-only:** Optical weight correction (0.90) now applies only to regular text faces. NF icons, emoji, and fallback symbols render at full DPI.
- **Typeface init desync:** Constructor received raw font size (12pt) while Screen used DPI-corrected (9.6pt). Guard skip left NF face at wrong size until first zoom. Fixed by passing `dpiCorrectedFontSize()` to constructor.
- **calcMetrics cache stale:** `setSize()` didn't invalidate cache. Post-zoom `calcMetrics` returned stale `physCellW/H`. Fixed with `cachedMetricsSize = -1` in `setSize`.
- **26.6 SSOT:** `setSize` truncated, `calcMetrics` rounded. Unified to `roundFloatPxTo26_6` at all 3 sites.
- **Daemon lifecycle:** `removeConnection` no longer kills daemon on client disconnect. `Nexus::remove` sends `killSession` IPC. Session restoration preserved.
- **OpenConsole cleanup:** GUI Job Object with `KILL_ON_JOB_CLOSE` ensures ConPTY children die on GUI exit. `CREATE_BREAKAWAY_FROM_JOB` lets daemon outlive GUI.
- **Config::Key::windowZoom removed:** Zoom is AppState-only (SSOT). `resetZoom` = `Config::zoomMin` (1.0). No config/state conflation.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 13: Arrow Glyphs + Ligature Span Fix

**Date:** 2026-04-13
**Duration:** ~02:30

### Agents Participated
- COUNSELOR: session lead, rendering pipeline trace, root-cause analysis (oversize rescale + span=1 hardcode)
- Pathfinder: codebase survey (font pipeline, rendering dispatch)
- Engineer: build_fonts.py execution, build_monolithic.py execution, GSUB/HarfBuzz verification

### Files Modified (7 total)
- `___display___/sheet_config.py` — added ARROWS list (U+2190-U+2194)
- `___display___/generate_unified_sheets.py` — added arrow row calculation, guide/cell emission blocks, updated total_rows and print output
- `___display___/svg-mono/GlyphSheet_Display-MONO-Book.svg` — height 3622→3875, 5 arrow guide blocks, 5 arrow cell groups with artwork
- `___display___/svg-mono/GlyphSheet_Display-MONO-Medium.svg` — same as Book
- `___display___/svg-mono/GlyphSheet_Display-MONO-Bold.svg` — same as Book (rgb() style convention preserved)
- `Source/terminal/rendering/ScreenRender.cpp:962` — tryLigature: span 0 → `static_cast<uint8_t>(tryLen)` so oversize rescale uses correct multi-cell width
- `Source/terminal/rendering/ScreenRender.cpp:259-261` — emitShapedGlyphsToCache: route through span-aware `getGlyph` when `span > 1` even without active constraint
### Alignment Check
- [x] BLESSED principles followed — SSOT (sheet_config.py drives generator and manual edits identically), Deterministic (HarfBuzz verified: lig_002D_003E glyph_id=107 independent of uni2192 glyph_id=102), Encapsulation (span flows through existing getGlyph API, no new surface)
- [x] NAMES.md adhered — no new names introduced
- [x] MANIFESTO.md principles applied — no early returns, positive checks, alternative tokens

### Problems Solved
- **Arrow glyphs missing from Display Mono:** OS fallback arrows rendered ugly on Windows (proportional advance, wrong style). Added 5 arrow codepoints to GlyphSheets, drew artwork in all 3 weights, rebuilt Display Mono + Monolith.
- **Ligature oversize rescale bug:** `tryLigature` passed `span=0` to `emitShapedGlyphsToCache`, which called `font.getGlyph(glyphIndex)` (single-arg overload hardcoding `span=1`). Oversize rescale in `rasterizeGlyph` computed `effectiveWidth = cellWidth * 1`, shrinking 2-cell ligature bitmaps to 1 cell. Previously invisible because OS-fallback arrows looked different; exposed when Display Mono arrows matched the rescaled ligature. Fix: pass `tryLen` as span, route through span-aware `getGlyph` overload.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 12: Daemon Lifecycle — Clean Start, Clean Kill, tmux Alignment

**Date:** 2026-04-12
**Duration:** ~02:00

### Agents Participated
- COUNSELOR: session lead, PLAN authorship, 5-step delegation, bug root-cause analysis (killSession path missing quit trigger)
- Pathfinder: daemon/nexus infrastructure survey (2 invocations — lifecycle + detailed line-level)
- Engineer: all code implementation (Steps 1–5)
- Auditor: post-implementation validation (found orphaned App::ID::connected — resolved)

### Files Modified (8 total)
- `Source/interprocess/Message.h:56` — added `killDaemon = 0x18` PDU enum value
- `Source/interprocess/Daemon.h:123-133` — declared `killAll()` public method
- `Source/interprocess/Daemon.cpp:144-165` — implemented `killAll()`: snapshot session list, iterate `nexus.remove()`, fire `onAllSessionsExited`
- `Source/interprocess/Channel.cpp:108,219-236` — added `case Message::killDaemon` → `daemon.killAll()`; fixed `killSession` handler to broadcast sessions and fire `onAllSessionsExited` when last session removed
- `Source/Main.cpp:189-295,429-436,487-640` — added `clientLock` (InterProcessLock) member; restructured `initialise()` with kill/kill-all CLI dispatch; rewrote `resolveNexusInstance()` with lock-based claim; removed `setConnected(false)` from `systemRequestedQuit()`
- `Source/AppState.h:19,33-35,155-169` — removed `setConnected`/`isConnected` declarations and doc references
- `Source/AppState.cpp:154-167,390,410,449-450` — removed `setConnected`/`isConnected` implementations; removed `connected` property from `load()`/`save()` docs
- `Source/AppIdentifier.h:12,83` — removed `App::ID::connected` identifier and schema comment
- `Source/interprocess/Link.cpp:140-156` — removed `setConnected` calls from `connectionMade()`/`connectionLost()`

### Alignment Check
- [x] BLESSED principles followed — SSOT (InterProcessLock IS the connected truth, no shadow boolean), Bound (lock has RAII lifecycle, auto-releases on crash), Lean (net code reduction — removed connected flag, added minimal killAll/killDaemon), Stateless (removed manual `connected` boolean flag from AppState), Encapsulation (daemon handles its own shutdown via killAll), Deterministic (lock acquire is atomic OS operation, no TOCTOU)
- [x] NAMES.md adhered — `killDaemon` (PDU), `killAll` (method), `clientLock` (member), `KillConn` (function-local struct) all ARCHITECT-approved
- [x] MANIFESTO.md principles applied — no early returns, positive nesting, alternative tokens, brace init throughout

### Problems Solved
- **Crash-unsafe connected flag:** `.display` XML `connected=true` persisted after crash, blocking future clients from claiming daemon. Replaced with `juce::InterProcessLock` (named by UUID) — OS auto-releases on crash. `resolveNexusInstance()` now uses `enter(0)` for non-blocking lock-based claim.
- **No explicit daemon kill command:** Added `killDaemon` PDU + `Daemon::killAll()`. CLI: `end --nexus kill <uuid>` and `end --nexus kill-all` — ephemeral fire-and-forget connections.
- **ctrl+w on last pane leaves daemon alive:** `Channel::messageReceived` killSession handler only called `nexus.remove()` which checked `Nexus::onAllSessionsExited` (nullptr in daemon mode). Fixed by adding `daemon.broadcastSessions()` + `daemon.onAllSessionsExited` check after removal — mirrors the `wireOnExit` path.
- **Dead code:** Removed all `connected` flag infrastructure (AppState, AppIdentifier, Link, Main) — ~30 lines of dead code eliminated.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 10: Windows Font Rendering + Box Drawing AA + Daemon Cleanup

**Date:** 2026-04-11 / 2026-04-12
**Duration:** ~16:00

### Agents Participated
- COUNSELOR: session lead, planning, multi-wave delegation, role drift correction mid-session
- Pathfinder: codebase surveys (config key layout, daemon lifecycle, rounded corner primitive search, terminal vs popup vs DWM rounded corner disambiguation, box drawing helpers, fallback glyph sizing path)
- Engineer: all code implementation across the waves listed below
- Researcher: reference terminal / JUCE rounded rectangle prior art, fallback glyph scaling prior art

### Files Modified

**Config / font routing (Ask A — `font.desktop_scale`):**
- `Source/config/Config.h:200-202` — added `Config::Key::fontDesktopScale { "font.desktop_scale" }` with windows-only semantic doc
- `Source/config/Config.h:645-647` — declared `Config::dpiCorrectedFontSize() const noexcept`
- `Source/config/Config.cpp:104` — `addKey (Key::fontDesktopScale, "false", { T::string })`
- `Source/config/Config.cpp:985-1003` — implemented `Config::dpiCorrectedFontSize()`: returns `Key::fontSize`, on Windows divides by `jreng::Typeface::getDisplayScale()` when `fontDesktopScale == "false"`
- `Source/config/default_end.lua:88-93` — `desktop_scale = "%%font_desktop_scale%%"` entry under `font` table with docs
- `Source/component/TerminalDisplay.h:554-558` — deleted stub `dpiCorrectedFontSize`
- `Source/component/TerminalDisplay.cpp:700,726` — route through `Config::getContext()->dpiCorrectedFontSize()`
- `Source/MainComponent.cpp:320` — same
- `Source/MainComponentActions.cpp:391` — same
- `Source/component/Panes.cpp:57` — same
- `Source/terminal/rendering/Screen.cpp:40` — `baseFontSize` initialised via `Config::getContext()->dpiCorrectedFontSize()` (eliminates transient pre-applyScreenSettings mismatch)
- `Source/config/Config.cpp:698-760` — BLESSED cleanup of `Config::load` lambdas (removed 3 early returns inside `sol::table::for_each`, restructured as nested positive checks with `isTableVal`/`isSpecialGroup`/`isPadding` locals)
- `Source/config/Config.cpp:847-858` — BLESSED cleanup of `patchKey` (collapsed mutable `needsQuotes` flag into `const bool isNumberType`)

**Windows glyph scale (Ask B — `windowsGlyphScale`):**
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:86-88` — `static constexpr float windowsGlyphScale { 0.90f }` inside `namespace jreng` (JUCE_WINDOWS only)
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:252-257` — `#if JUCE_WINDOWS` branch multiplies final render DPI by `windowsGlyphScale`; logical metrics pass untouched, so cell dimensions stay at the unscaled size while FreeType rasterises the glyph outline slightly smaller

**Daemon cleanup (DEBT-20260411T101344):**
- `Source/interprocess/Daemon.cpp:134-145` — after `connections.remove(index)`, if `connections.isEmpty() and onAllSessionsExited != nullptr`, fire the callback; reuses the existing shell-exit quit path

**Box drawing (DEBT-20260411T083315):**
- `modules/jreng_graphics/fonts/jreng_box_drawing.h:235-260` — `lightThickness(w, embolden)` doubles base thickness when embolden is true; `heavyThickness` delegates through (effective ×4 under embolden, correct — embolden is orthogonal to light/heavy weight)
- `modules/jreng_graphics/fonts/jreng_box_drawing.h` — threaded `bool embolden` through `BoxDrawing::rasterize`, `drawLines`, `drawDashedLine`, `drawDiagonal`, `drawDoubleLines`, `drawRoundedCorner`; block elements and braille unchanged (they don't use thickness)
- `modules/jreng_graphics/fonts/jreng_box_drawing.h:680-749` — `drawRoundedCorner` replaced with 4× supersampled SDF + 4×4 box-filter downsample. `adjustedHxSs` / `adjustedHySs` derived from `drawLines`' actual line-range centres (`cxNative - lt/2 + lt * 0.5f`, times `supersampleFactor`) so the arc's attachment pole tangents the straight stroke at the same pixel column. `aaSs = supersampleFactor * 0.5f` widens the AA band in SS space. `juce::HeapBlock<uint8_t>` scratch buffer at `4w × 4h`, unconditional max-blend into `buf` on downsample.
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.cpp:232` — `Atlas::getOrRasterizeBoxDrawing` passes `embolden` member to `BoxDrawing::rasterize`

### Alignment Check
- [x] BLESSED principles followed — SSOT (one `dpiCorrectedFontSize` source in Config, referenced by 5 call sites; single `windowsGlyphScale` constexpr; single `lightThickness`/`heavyThickness` source with embolden parameter), Lean (one method per concept, no speculative helpers), Explicit (no magic numbers — `windowsGlyphScale`, `supersampleFactor`, `renderDpiUnit` are named constexpr), Stateless (daemon quit gate is derived from connection list state, not a flag), Encapsulation (Box drawing, font metrics, and config are separate concerns touched through their public APIs), Bound (ScopedFaceSize experiment was reverted on failure — no lingering ownership concerns)
- [x] NAMES.md adhered — all new names ARCHITECT-approved: `Config::Key::fontDesktopScale`, `Config::dpiCorrectedFontSize`, `windowsGlyphScale`, `platformCellWidthCorrection` (superseded by `windowsGlyphScale`), `supersampleFactor`, embolden parameter on box drawing helpers
- [x] MANIFESTO.md principles applied — no early returns introduced; all new code uses positive nesting and alternative tokens; brace initialisation throughout; `const` where practical; no new helper types introduced at class scope; file-local `static` for utility symbols

### Problems Solved
- **`font.desktop_scale` (Windows-only):** Windows desktop scale slider previously ballooned the configured font size because JUCE's `getDisplayScale()` returns the OS scale. Users wanted 12pt to stay 12pt physical on any scale setting. Implemented upstream in `Config::dpiCorrectedFontSize` as an optional divide-by-scale when the key is `"false"`; default is `"false"` (persistent), user can opt into OS-scale-following by setting `"true"`. No-op on macOS/Linux.
- **`windowsGlyphScale`:** Display Mono visually felt heavier on Windows than macOS because FreeType's rasteriser at the full render DPI produced slightly wider strokes than CoreText at the equivalent size. Applied a downstream 0.90× scale to the final `FT_Set_Char_Size` render DPI only; cell metrics remain at unscaled size. Net effect: glyph bitmap is slightly smaller within the (unchanged) cell, matching macOS visual weight.
- **Embolden x2 for box drawing:** Embolden config flag previously only affected font glyphs via `FT_Outline_Embolden`. Now also doubles `lightThickness` / `heavyThickness` used by all procedural box-drawing helpers, so rounded corners and straight lines inherit the same weight adjustment as the surrounding font.
- **Rounded corner AA (DEBT-20260411T083315):** Original SDF at native resolution was visibly jaggy at small cell sizes. Applied 4× supersampling pattern: render SDF into a `4w × 4h` scratch buffer with `aa = supersampleFactor * 0.5f` (wider AA band in SS space), then box-filter down to native. Visual quality improved significantly per ARCHITECT's visual verification. Minor sub-pixel drift on even cell widths remains inherent to integer pixel grids.
- **Daemon cleanup on last close (DEBT-20260411T101344):** Daemon process previously outlived the GUI after the last terminal was closed. 1:1 daemon↔GUI lifecycle established by having `Daemon::removeConnection` trigger the existing `onAllSessionsExited` quit path when the connection list drains empty. `Main.cpp` wiring to `appState.deleteNexusFile(); quit()` unchanged.

### Failed Attempt (Not in Sprint Scope)
- **Fallback glyph advance width** (RFC-FALLBACK-ADVANCE.md): attempted to fix the "arrow too close to comma" visual bug where proportional system fallback fonts (Segoe UI Symbol) broke the monospace grid. Three iterations: (1) `xAdvance = cellAdvance` with centering xOffset — shifted further; (2) per-glyph FreeType rescale via `ScopedFaceSize` RAII, new `renderScale` field on `Glyph` and `Key` — all text rendered as dots (unconditional guard); (3) conditional guard + `shapeASCII`/`shapeHarfBuzz` renderScale init — artifacts remained, still shifted. Root cause of residual artifacts not diagnosed within the session's diagnostic budget. COUNSELOR's mental model of the shaping→rasterization interaction was incomplete; each fix iteration compounded the problem. **Full revert of all 8 touched files via `git show HEAD:<path>` readback, restoring shaping pipeline to HEAD state.** RFC-FALLBACK-ADVANCE.md stays open for a future sprint with deeper pre-investigation.

### Debts Paid
- `DEBT-20260411T101344` — daemon not killed on last terminal close; fixed via `Source/interprocess/Daemon.cpp:134-145` (`removeConnection` fires `onAllSessionsExited` on empty connection list)
- `DEBT-20260411T083315` — rounded rectangle misalignment; diminished via `modules/jreng_graphics/fonts/jreng_box_drawing.h:680-749` (4× supersample SDF, attachment-aware `adjustedHx/Hy`). Residual sub-pixel drift on even cell widths is mathematical, not algorithmic

### Debts Deferred
- None (no ARCHITECT-commanded mid-sprint deferrals)

---

## Sprint 9: Nexus Architectural Refactor + CWD Propagation Fix + Audit Cleanup

**Date:** 2026-04-11

### Agents Participated
- COUNSELOR: session lead, planning, 6-wave audit-driven fix sprint, delegation
- Pathfinder: codebase surveys (Nexus call sites, file layout, caller migration scope)
- Engineer: all code implementation (Waves 1–6 fix sprint, ~15 delegations)
- Auditor: comprehensive post-refactor audit (2 critical, 12 high, 5 medium, 4 low)
- MACHINIST: stale file deletion + .gitignore update (Wave 5)

### Files Modified (50+ total)

**Config:**
- `Source/config/Config.h` — `Key::nexus` → `Key::daemon`
- `Source/config/Config.cpp:146` — `addKey(Key::daemon, ...)`
- `Source/config/default_end.lua:59` — `daemon = "%%daemon%%"`

**Nexus (new class, replaces Nexus::Session):**
- `Source/nexus/Nexus.h` — NEW — session manager class (`jreng::Context<Nexus>`), `unordered_map<uuid, Terminal::Session>`, attach(Daemon&)/attach(Link&), three create overloads (with TTY / no TTY / routing)
- `Source/nexus/Nexus.cpp` — NEW — implementation. Client branch uses `Link::sendCreateSession` + factory (SSOT + Encapsulation fix)
- `Source/nexus/Session.h/cpp` — DELETED
- `Source/nexus/SessionFanout.cpp` — DELETED

**Interprocess (new namespace, IPC layer):**
- `Source/interprocess/Daemon.h/cpp` — moved from nexus/, namespace `Interprocess`, absorbs broadcast/subscriber registry from SessionFanout, `wireSessionCallbacks` split into `wireOnBytes`/`wireOnStateFlush`/`wireOnExit` helpers, `installPlatformProcessCleanup`/`releasePlatformProcessCleanup`
- `Source/interprocess/Daemon.mm` — moved, namespace `Interprocess`, added POSIX no-op platform cleanup
- `Source/interprocess/DaemonWindows.cpp` — NEW — extracted Windows platform helpers (hideDockIcon, spawnDaemon, Job Object setup) to parallel `Daemon.mm` (H9 resolution, Daemon.cpp back under 300 code lines)
- `Source/interprocess/Link.h/cpp` — moved from nexus/, namespace `Interprocess`, added typed `sendCreateSession(cwd, uuid, cols, rows)` method
- `Source/interprocess/Channel.h/cpp` — moved from nexus/, namespace `Interprocess`, constructor takes `Nexus&` (not `Nexus::Session&`)
- `Source/interprocess/EncoderDecoder.h/cpp` — moved and renamed from `Wire.h/cpp`
- `Source/interprocess/Message.h` — moved from nexus/

**Terminal layer:**
- `Source/terminal/logic/Session.h` — added `process`, `getStateInformation`, `setStateInformation` public API; `shouldTrackCwdFromOs` private member; deleted dead `onDrainComplete` public callback
- `Source/terminal/logic/Session.cpp` — two `create` factory overloads (with TTY / no TTY), `applyShellIntegration` sets `CHERE_INVOKING=1` unconditionally; onFlush gates getCwd on `shouldTrackCwdFromOs`
- `Source/terminal/tty/WindowsTTY.h` — added `getCwd` override declaration
- `Source/terminal/tty/WindowsTTY.cpp` — implemented `getCwd` via PEB query (NtQueryInformationProcess + ReadProcessMemory), split into 4 file-local static helpers (queryPebAddress, readProcessParametersAddress, readCurrentDirectoryUnicodeString, readAndConvertWidePath); Job Object setup moved out to DaemonWindows.cpp

**Application:**
- `Source/Main.cpp` — rewired ENDApplication: `unique_ptr<Nexus>`, `unique_ptr<Interprocess::Daemon>`, `unique_ptr<Interprocess::Link>`; daemon branch constructs+attaches; client branch constructs Link+attaches; standalone appends SESSIONS ValueTree; `shutdown()` orders teardown (link → daemon → mainWindow → nexus)
- `Source/AppState.h/cpp` — `isDaemonMode`/`setDaemonMode` (renamed from Nexus variant)
- `Source/AppIdentifier.h:79` — `App::ID::nexusMode` → `App::ID::daemonMode` (C++ identifier + XML string key)
- `Source/MainComponent.h/cpp` — include path update (`nexus/Nexus.h`)
- `Source/MainComponentActions.cpp` — `isDaemonMode` rename call sites
- `Source/component/Tabs.cpp` — `Nexus::getContext()->remove(uuid)` (2 sites), `isDaemonMode` renames (6 sites)
- `Source/component/Panes.cpp` — `Nexus::getContext()->create(...)` + `.getProcessor()` (2 sites)

**Diagnostics (temporary, removed before sprint close):**
- `Source/TeardownLog.h` — ADDED then DELETED (diagnostic logger for CWD trace + shutdown hang investigation)
- Temporary `TEARDOWN_LOG` calls in 7 files — added then removed

**Docs:**
- `ARCHITECTURE.md` — full rewrite: new module map with `interprocess/` tree, new Nexus+Interprocess section, layer separation diagram, communication contracts table, PDU kind table, data flow for all three configurations, Windows Job Object subsection
- `.gitignore` — added `# Diagnostic logs` section with `*.log` pattern
- `PLAN-nexus-refactor.md` — DELETED (all 7 steps executed)
- `teardown.log` — DELETED (30MB leftover)

### Alignment Check
- [x] BLESSED principles followed — Bound (clear ownership chain ENDApplication→Nexus→Terminal::Session), Lean (Nexus=pure container, Interprocess=IPC only, all files under 300 code lines, all functions under 30), Explicit (no mode flags, attach IS the mode; no early returns; no magic values), SSOT (one session map, one create factory with typed overloads, one Link::sendCreateSession method), Stateless (Nexus has attachment pointers but is dumb about IPC internals), Encapsulation (Terminal layer does not include Nexus or Interprocess headers; Nexus does not include Interprocess implementations), Deterministic (same create call produces same Session regardless of mode)
- [x] NAMES.md adhered — all new names ARCHITECT-approved: `Nexus`, `Interprocess::{Daemon,Link,Channel,EncoderDecoder,Message}`, `Terminal::Session::process/getStateInformation/setStateInformation`, `shouldTrackCwdFromOs`, `Link::sendCreateSession`, `Daemon::wireOnBytes/wireOnStateFlush/wireOnExit`, `Daemon::installPlatformProcessCleanup/releasePlatformProcessCleanup`, `App::ID::daemonMode`
- [x] MANIFESTO.md principles applied — zero stale `Nexus::Session` references, zero early returns, positive nesting, alternative tokens, brace init, `const` everywhere, `jassert` preconditions, no anonymous namespaces (static file-local where needed), no `namespace detail`

### Problems Solved

**Architectural rot:**
- `Nexus::Session` was a three-mode dispatcher (standalone/daemon/client) with null-check mode routing (`link != nullptr`, `daemon != nullptr`). Replaced with `Nexus` class owning sessions + `Interprocess::` transport layer; mode determined by `attach(Daemon&)` vs `attach(Link&)` vs nothing. Mode IS the attached object.
- `Terminal::Session` had two personalities (full with TTY, "remote" without TTY). Unified under single class with two `create` factory overloads.
- Nexus::Session was reaching into Terminal::Session::Processor internals for state sync. Session now exposes `process`, `getStateInformation`, `setStateInformation` — callers talk to Session, not Processor.

**CWD propagation (Explorer launch case):**
- Root cause: MSYS2 `/etc/post-install/05-home-dir.post` sources via `/etc/profile` when shell launched with `-l`; if `CHERE_INVOKING` unset AND `SHLVL<=1`, it executes `cd "$HOME"` BEFORE first prompt. END spawned shells at the intended cwd via `CreateProcessW lpCurrentDirectory`, but MSYS2 overrode it 60ms later during shell init. Vim-launched END inherited SHLVL≥2 from nested context, so the cd was skipped there — which is why "from vim works."
- Fix: `seedEnv.set("CHERE_INVOKING", "1")` unconditionally in `Terminal::Session::applyShellIntegration`. Cross-platform safe (harmless on macOS/Linux).
- Verified via diagnostic TEARDOWN_LOG sweep across the chain: Parser OSC 7 → State.setCwd → flushStrings → ValueTree → onStateFlush → stateUpdate PDU → Link handler → client ValueTree → pwdValue → getPwd → split. Chain was intact; the shell was just cd-ing away from the intended dir.

**OpenConsole.exe surviving daemon death:**
- Root cause: OpenConsole.exe is spawned internally by `conpty.dll::CreatePseudoConsole` — it's a grandchild of the daemon. If the daemon process is killed externally (taskkill, crash), destructors never run, `ClosePseudoConsole` never fires, OpenConsole.exe orphans.
- Fix: Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` assigned to the daemon process in `installPlatformProcessCleanup()`. All children (including OpenConsole.exe grandchildren) killed by kernel when job handle closes — even on crash.

**BLESSED audit findings (15 total, all resolved):**
- C1 — Nexus.cpp client branch used raw `make_unique<Terminal::Session>` constructor instead of `Terminal::Session::create` factory (SSOT). Fixed by routing through factory.
- C2 — Nexus hand-rolling `createSession` PDU payload (Encapsulation). Added typed `Interprocess::Link::sendCreateSession` method.
- H1 — dead `Nexus::create(cols, rows, uuid)` overload. Resolved by C1/C2 fix which now uses it with extended signature.
- H2 — dead `Terminal::Session::onDrainComplete` public member. Removed.
- H3 — stale `Nexus::Session` doxygen references across 13+ files. Swept.
- H4 — ARCHITECTURE.md Nexus section described deleted class. Fully rewritten.
- H5 — stale `PLAN-nexus-refactor.md` at repo root. Deleted.
- H6 — `teardown.log` 30MB leftover. Deleted; `*.log` added to `.gitignore`.
- H7 — `Nexus::create(cwd, uuid, cols, rows)` > 30 lines. Shrunk to 23 via C1/C2 fix.
- H8 — `Daemon::wireSessionCallbacks` 78 lines. Split into 3 helpers.
- H9 — `Daemon.cpp` 340 code lines (>300). Extracted Windows platform code to `DaemonWindows.cpp` → 212 lines.
- H10 — `WindowsTTY::getCwd` 158 lines, 10-level nesting. Split into 4 file-local helpers, flat body.
- H11 — `App::ID::nexusMode` → `App::ID::daemonMode` (identifier + XML string key).
- H12 — 3 defensive `Nexus::getContext() != nullptr` guards in `Link.cpp` without named threat. Replaced with `jassert`.

### Debts Paid
- None (no formal DEBT.md entries existed at sprint start)

### Debts Deferred
- None explicitly pushed to DEBT.md this sprint. One pre-existing observation flagged during audit: `Source/config/Config.cpp:705,713,743` contain three early returns in the Lua loader root-iteration lambda (pre-Sprint 9 debt). Not fixed this sprint — out of scope.

## Sprint 8: BLESSED Cleanup — CWD SSOT, Config, Nexus Rename, Instance Isolation

**Date:** 2026-04-10

### Agents Participated
- COUNSELOR: session lead, planning, delegation, discussion
- Engineer: all code implementation (35+ delegations)
- Pathfinder: codebase exploration, git history, blast radius surveys
- Auditor: PLAN-nexus-cleanup validation, comprehensive final audit (22 findings, all resolved)
- Librarian: juce::var bool-vs-string behavior research

### Files Modified (35+ total)
- `modules/jreng_gui/glass/jreng_window.cpp` — middle-click drag only (removed DocumentWindow fall-through)
- `Source/nexus/Session.cpp` — openTerminal decomposed (dispatcher + openTerminalRemote + openTerminalLocal), inline PDU, remote Terminal::Session, manual encode replaced with writeString, session vocabulary
- `Source/nexus/Session.h` — openTerminal, removed dead forEachAttached, member renames (daemon/link), forward decls, session vocabulary
- `Source/nexus/SessionFanout.cpp` — broadcastSessions (was broadcastProcessorList), Channel refs, operator[] comment
- `Source/nexus/Daemon.h` (was Server.h) — class rename, folded hideDockIcon/spawnDaemon, removed lockfile wrappers
- `Source/nexus/Daemon.cpp` (was Server.cpp) — class rename, Windows spawnDaemon impl
- `Source/nexus/Daemon.mm` (new) — macOS hideDockIcon/spawnDaemon, merged macOS+Linux spawnDaemon (SSOT)
- `Source/nexus/Link.h` (was Client.h) — class rename, removed dead connectToHost/onPdu/handleUnknown, perProbeTimeoutMs constant
- `Source/nexus/Link.cpp` (was Client.cpp) — class rename, port from .nexus file, handleSessions/handleSessionKilled, removed dead fgConsumed, sendPdu delegates to encodePdu
- `Source/nexus/Channel.h` (was ServerConnection.h) — class rename, Daemon& ref, removed dead getId/id member
- `Source/nexus/Channel.cpp` (was ServerConnection.cpp) — class rename, broadcastSessions, minimal createSession PDU, sendPdu delegates to encodePdu
- `Source/nexus/Message.h` — createSession/killSession/detachSession/sessionKilled/sessions, removed dead shutdown enum
- `Source/nexus/Wire.h` — doc updates, encodePdu extracted (SSOT), removed redundant static from constexpr
- `Source/nexus/Wire.cpp` — encodePdu implementation
- `Source/nexus/NexusDaemon.h` — DELETED (folded into Daemon)
- `Source/nexus/NexusDaemon.mm` — DELETED
- `Source/nexus/NexusDaemon_win.cpp` — DELETED
- `Source/terminal/logic/Session.h` — Terminal::Session::create factory, applyShellIntegration static, remote constructor
- `Source/terminal/logic/Session.cpp` — create factory, applyShellIntegration impl, remote constructor, shellProgram write, onFlush guards
- `Source/terminal/tty/WindowsTTY.h` — getForegroundPid, getProcessName, childPid member
- `Source/terminal/tty/WindowsTTY.cpp` — getForegroundPid/getProcessName impl, spawnProcess pidOut
- `Source/config/Config.h` — Type enum (removed boolean), gpu key flattened
- `Source/config/Config.cpp` — boolean defaults as strings, getBool toString, validateAndStore, writeDefaults, patchKey
- `Source/config/WhelmedConfig.h` — Type enum unified, dead loader keys removed
- `Source/config/WhelmedConfig.cpp` — dead loader keys, scroll key names fixed, dead isBool branch
- `Source/config/default_end.lua` — gpu flattened, booleans quoted
- `Source/AppIdentifier.h` — SESSIONS/SESSION identifiers, removed instanceID
- `Source/AppState.h` — instanceUuid, connected, port, getNexusFile, getStateFile (.display), setConnected (no save), dtor trivial
- `Source/AppState.cpp` — split state files, save/load, setPort read-modify-write, dtor = default, setConnected property only
- `Source/Main.cpp` — startup scan extracted to resolveNexusInstance(), daemon UUID via CLI, systemRequestedQuit file decisions, shutdown order (nexus before mainWindow), spawnDaemon(uuid), stale comments fixed
- `Source/terminal/data/State.h` — "ServerConnection" → "Channel" in doxygen
- `ARCHITECTURE.md` — nexus/ module map added, state.xml → end.state/uuid.display
- `README.md` — state.lua → end.state (XML format)
- `PLAN-nexus-cleanup.md` — DELETED (completed)
- `Source/MainComponent.h` — removed onNexusConnected, sessionsNode member
- `Source/MainComponent.cpp` — inlined onNexusConnected into valueTreeChildAdded, implemented valueTreeChildRemoved (closeSession), SESSIONS/SESSION refs
- `Source/MainComponentActions.cpp` — save on tab mutation (nexus mode)
- `Source/component/Tabs.h` — closeSession declaration
- `Source/component/Tabs.cpp` — closeSession impl, save on mutation, removed file ops
- `Source/component/Panes.h` — doc updates
- `Source/component/Panes.cpp` — openTerminal call sites updated

### Alignment Check
- [x] BLESSED principles followed — SSOT for CWD (Terminal::State → AppState → new terminal), Bound (AppState created first destroyed last, daemon lifecycle explicit), Lean (removed wrappers, dead code, folded files), Explicit (names match reality), Encapsulation (AppState dumb storage, Main owns file I/O)
- [x] NAMES.md adhered — Daemon/Link/Channel/createSession/killSession/sessions approved by ARCHITECT
- [x] MANIFESTO.md principles applied — no early returns, positive checks, brace init, alternative tokens

### Problems Solved
- Window drag: left-click collision with text selection (Windows) — middle-click only
- Byte trails: duplicate DA/DSR response from client Processor — removed setHostWriter from display-only Processor
- CWD not propagating (Windows): shell integration env injection lost during Nexus arch shift — ported applyShellIntegration to Terminal::Session::create
- CWD overwrite: WindowsTTY::getCwd returning stale PEB data overwrote OSC 7 — removed getCwd, added onFlush guards
- CWD in nexus client: handleStateUpdate overwrote initial CWD with empty — added isNotEmpty guard
- CWD SSOT: Terminal::Session::create single entry point for all modes, daemon resolves shell/args from own config
- Config inconsistency: booleans as strings, gpu table flattened, Type enum unified
- Instance collision: per-UUID lockfile/state in nexus/ directory, startup scan with probe
- Session restoration: separated .nexus (daemon) and .display (client) ownership
- Stale .display cleanup: MainComponent owns .display lifecycle, valueTreeChildRemoved handles daemon-killed sessions
- shellProgram never written to State: fixed displayName always showing "zsh"

### Audit Sweep (22 findings, all resolved)
- Dead code: Message::shutdown, forEachAttached, Channel::getId/id, fgConsumed
- SSOT: sendPdu extracted to Wire::encodePdu, spawnDaemon merged macOS+Linux, manual encode replaced with writeString
- Stale docs: State.h, Main.cpp, MainComponent.cpp, ARCHITECTURE.md, README.md
- BLESSED L: openTerminal decomposed, initialise() scan extracted to resolveNexusInstance()
- Low: magic 200ms named, operator[] commented, redundant static removed, PLAN-nexus-cleanup.md deleted

### Technical Debt / Follow-up
- PLAN-IMAGE.md execution pending (9 steps for inline image rendering)
- WHELMED Mermaid still broken

## Sprint 7: BLESSED Audit — Production Quality

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Directed audit resolution, prioritized findings
- Auditor: Comprehensive 2-critical/5-violation/6-stale/4-improvement sweep across 30+ files
- Engineer: All fixes — early returns, SSOT extraction, doc updates, dead code removal

### Files Modified (15 total)
- `Source/terminal/logic/Processor.h` — setScrollOffsetClamped declaration
- `Source/terminal/logic/Processor.cpp` — setStateInformation early returns→positive nesting, setScrollOffsetClamped definition, removed Nexus::logLine from process(), removed Log.h include
- `Source/terminal/logic/Input.h/cpp` — deleted local setScrollOffsetClamped, routes to Processor
- `Source/component/TerminalDisplay.h/cpp` — deleted local setScrollOffsetClamped, routes to Processor
- `Source/component/Tabs.cpp` — addNewTab no-args delegates to parameterized overload
- `Source/nexus/Session.h` — deleted createClientSession declaration, added buildProcessorListPayload, stale docs fixed
- `Source/nexus/Session.cpp` — createClientSession inlined into create(), stale docs fixed
- `Source/nexus/SessionFanout.cpp` — broadcastProcessorList payload extracted to buildProcessorListPayload
- `Source/nexus/Client.cpp` — stale Loader doc references updated
- `Source/action/Action.cpp` — TODO removed
- `ARCHITECTURE.md` — filenames updated, Session description corrected, Grid snapshot section added, GlassWindow→Window
- `SPEC.md` — terminal state serialization marked Done
- `README.md` — roadmap updated, Nexus daemon mention added
- `PLAN-nexus.md` — deleted

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- 5 early returns in setStateInformation eliminated (BLESSED-E, JRENG)
- createClientSession deleted as separate method (dead pattern)
- setScrollOffsetClamped duplicated in Display + Input → single Processor method (SSOT)
- broadcastProcessorList duplicated payload → extracted helper (SSOT)
- addNewTab duplication → no-args delegates to parameterized (SSOT)
- Nexus::logLine in Processor::process hot path removed (Lean)
- 6 stale Loader doc references fixed
- ARCHITECTURE.md brought current with Sprint 4-6 architecture
- SPEC.md terminal state serialization marked implemented
- PLAN-nexus.md deleted (superseded)
- TODO in Action.cpp removed

### Technical Debt / Follow-up
- None deferred — all audit findings resolved

## Sprint 6: Terminal Owns Processor, Nexus Decoupled

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Architecture analysis, Nexus contamination discovery, staged execution direction
- Pathfinder: Grid/State internals, call site mapping, Nexus reference sweep
- Researcher: tmux reattach architecture (virtual grid redraw pattern)
- Librarian: JUCE ComponentBoundsConstrainer API
- Engineer: Serialization, Terminal::Session ownership, Nexus removal from Terminal layer, ServerConnection inline, popup fix

### Files Modified (25+ total)
- `Source/terminal/logic/Session.h` — owns Processor, getProcessor(), onStateFlush callback, uuid parameter
- `Source/terminal/logic/Session.cpp` — creates Processor in ctor, wires all 6 callbacks (onBytes, onDrainComplete, onFlush, setHostWriter, writeInput, onResize), stop() destroys Processor before TTY
- `Source/terminal/logic/Processor.h` — getStateInformation/setStateInformation, writeInput callback, onResize callback, deleted onLoadingStarted/onLoadingFinished
- `Source/terminal/logic/Processor.cpp` — serialization implementations
- `Source/terminal/logic/Grid.h/cpp` — getStateInformation/setStateInformation (dual buffer memcpy)
- `Source/terminal/logic/Input.cpp` — Nexus::Session→processor.writeInput (2 sites)
- `Source/terminal/logic/Mouse.cpp` — Nexus::Session→processor.writeInput (6 sites)
- `Source/component/TerminalDisplay.h/cpp` — removed Nexus::Session dependency, LoaderOverlay, titleBarHeight; sendInput→writeInput, sendResize→onResize
- `Source/component/Panes.h/cpp` — deleted buildTerminal/teardownTerminal/CreatedTerminal, direct Session::create/remove calls
- `Source/component/Tabs.cpp` — teardownTerminal→Session::remove, first-leaf sub-rect descent, cellsFromRect physical pixels, computeContentRect jassert
- `Source/component/Popup.h/cpp` — owns Terminal::Session directly, no Nexus, no buildTerminal
- `Source/MainComponent.h/cpp` — removed titleBarHeight from getContentRect, resize overlay via Window::isUserResizing
- `Source/MainComponentActions.cpp` — popup creates Terminal::Session directly
- `Source/nexus/Session.h/cpp` — single create() (no mode helpers), deleted processors map, deleted createLocalSession/createDaemonSession, client-side Processor wired with writeInput/onResize
- `Source/nexus/SessionFanout.cpp` — attach uses getStateInformation snapshot, terminalSessions lookups
- `Source/nexus/ServerConnection.h/cpp` — handle* methods inlined into messageReceived, unified createProcessor PDU
- `Source/nexus/Client.h/cpp` — createSession (renamed), deleted attachSession, handleStateUpdate
- `Source/nexus/Message.h` — createProcessor=0x10, stateUpdate=0x22, deleted 3 dead PDUs
- `Source/Main.cpp` — jreng::Window
- `modules/jreng_gui/glass/jreng_window.h/cpp` — renamed from GlassWindow, ComponentBoundsConstrainer inheritance
- `modules/jreng_gui/glass/jreng_modal_window.h/cpp` — GlassWindow→Window
- `modules/jreng_gui/jreng_gui.h/cpp` — updated module includes
- Deleted: `Source/nexus/Loader.h`, `Source/nexus/Loader.cpp`, `Source/nexus/Phrases.h`

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Terminal::Session owns Processor — single owner, deterministic lifecycle (B)
- Grid+State snapshot replaces raw byte history replay — eliminates dim-garble bug class
- Terminal layer decoupled from Nexus — Input, Mouse, Display use Processor callbacks
- Popup creates Terminal::Session directly — ephemeral terminal, no Nexus routing
- ServerConnection handle* methods inlined — no unnecessary helpers
- buildTerminal/teardownTerminal/CreatedTerminal deleted — wrong placement, raw pointer struct
- Unified createProcessor PDU — 3 dead PDUs deleted
- Double titleBarHeight subtraction — getContentRect and Display::resized
- cellsFromRect physical-pixel SSOT — eliminates 1-row divergence with Screen::calc
- First-leaf sub-rect descent — correct dims for split-pane restore
- Resize overlay via ComponentBoundsConstrainer — no flags, no timers
- Client-side Processor writeInput/onResize wired for IPC routing

### Technical Debt / Follow-up
- createClientSession still exists in Nexus::Session — should be eliminated (END creates its own Processor)
- Nexus::logLine referenced from Terminal::Session.cpp and Processor.cpp — logging dependency
- Stale doc comments referencing Nexus in Terminal headers
- forEachAttached template in Session.h — verify callers exist
- Session::create still has envID resolution logic that may belong elsewhere

## Sprint 5: Grid Snapshot Architecture

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Architecture analysis, tmux research, flow tracing, directed all changes
- Pathfinder: Grid/State internals discovery, destruction order analysis, dims divergence tracing
- Researcher: tmux reattach architecture research (virtual grid redraw vs raw byte replay)
- Librarian: JUCE ComponentBoundsConstrainer API research
- Engineer: Serialization implementation, daemon rewire, Loader deletion, Window rename

### Files Modified (20+ total)
- `Source/terminal/logic/Processor.h` — getStateInformation/setStateInformation declarations, deleted onLoadingStarted/onLoadingFinished
- `Source/terminal/logic/Processor.cpp` — serialization implementations delegating to Grid + State
- `Source/terminal/logic/Grid.h` — getStateInformation/setStateInformation declarations
- `Source/terminal/logic/Grid.cpp` — dual-buffer serialization (cells, graphemes, rowStates, ring metadata)
- `Source/terminal/data/Cell.h` — (read only, trivially copyable confirmed)
- `Source/nexus/Session.h` — deleted Loader includes/members, updated startLoading doc
- `Source/nexus/Session.cpp` — daemon Processor real (processWithLock in onBytes), startLoading calls setStateInformation, deleted Loader machinery, createClientSession initial cwd write
- `Source/nexus/SessionFanout.cpp` — attach sends getStateInformation snapshot, merged attachAndSync+attachConnection into attach(uuid, target, sendHistory, cols, rows)
- `Source/nexus/ServerConnection.h/cpp` — handleCreateProcessor (unified), hasSession check, attach with dims, deleted handleAttachProcessor
- `Source/nexus/Client.h/cpp` — createSession (renamed from spawnSession), deleted attachSession, handleStateUpdate for cwd/fgProcess
- `Source/nexus/Message.h` — createProcessor=0x10, stateUpdate=0x22, deleted 3 dead PDUs
- `Source/component/TerminalDisplay.h` — deleted LoaderOverlay member, titleBarHeight member
- `Source/component/TerminalDisplay.cpp` — deleted loading overlay wiring, removed titleBarHeight from resized
- `Source/component/Tabs.cpp` — Tabs::restore first-leaf sub-rect descent, cellsFromRect physical-pixel math, removed AppState fallback from computeContentRect
- `Source/component/Panes.cpp` — cellsFromRect uses physical pixels (Screen::calc SSOT)
- `Source/MainComponent.h/cpp` — removed getContentRect titleBarHeight subtraction, resize overlay via Window::isUserResizing
- `Source/Main.cpp` — jreng::GlassWindow → jreng::Window
- `modules/jreng_gui/glass/jreng_window.h/cpp` — renamed from jreng_glass_window, added ComponentBoundsConstrainer inheritance, resizeStart/resizeEnd/isUserResizing
- `modules/jreng_gui/glass/jreng_modal_window.h/cpp` — GlassWindow → Window
- `modules/jreng_gui/jreng_gui.h/cpp` — updated module includes
- Deleted: `Source/nexus/Loader.h`, `Source/nexus/Loader.cpp`, `Source/nexus/Phrases.h`

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Raw byte history replay replaced with Grid+State snapshot (eliminates dim-garble class of bugs)
- Daemon Processor real — parses all bytes, maintains live Grid+State for snapshot
- Unified createProcessor PDU — daemon decides create-vs-attach, deleted 3 dead PDUs
- cellsFromRect/Screen::calc SSOT — physical-pixel math eliminates 1-row divergence
- Double titleBarHeight subtraction in getContentRect and Display::resized
- First-leaf dims computed from actual sub-rect (split tree descent), not full content rect
- Resize overlay driven by ComponentBoundsConstrainer (no flags, no timers)
- GlassWindow → Window rename (domain naming)
- Loader thread + LoaderOverlay + loading callbacks deleted (entire subsystem)

### Technical Debt / Follow-up
- Session::create mode helpers (createClientSession/createDaemonSession/createLocalSession) to be unified into one path — byte source is configuration, not architecture
- Generic naming (daemon/client) to be purged — domain is Nexus/END
- startLoading naming is stale — now calls setStateInformation, not loading
- Daemon stub Processor concept eliminated but processors map still exists on daemon side

## Sprint 4: Nexus Parity — Loader, CWD, DSR, Shutdown

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Root cause analysis, flow tracing, directed all fixes
- Pathfinder: Byte path tracing, destruction order analysis, cwd flow discovery
- Engineer: Per-pane overlay, stateUpdate PDU, Phrases, shutdown fix

### Files Modified (10 total)
- `Source/terminal/logic/Processor.h:292-297` — added onLoadingStarted/onLoadingFinished callbacks
- `Source/nexus/Phrases.h` — new, random verb pool for loading messages
- `Source/component/TerminalDisplay.h` — added LoaderOverlay member, include
- `Source/component/TerminalDisplay.cpp` — wired onLoadingStarted/onLoadingFinished, addChildComponent, resized
- `Source/MainComponent.h` — removed global LoaderOverlay, loadingNode, getLoaderOverlay
- `Source/MainComponent.cpp` — removed LOADING ValueTree listener, overlay management, nexus-connect op
- `Source/nexus/Message.h` — added stateUpdate (0x22) PDU kind
- `Source/nexus/Session.h` — moved loaders to last member (destruction order)
- `Source/nexus/Session.cpp` — wired onFlush in createDaemonSession (cwd+fgProcess relay), flushResponses in feedBytes, loaders.clear() in dtor, initial cwd write in createClientSession
- `Source/nexus/Client.h` — declared handleStateUpdate
- `Source/nexus/Client.cpp` — implemented handleStateUpdate, added switch case

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Global LoaderOverlay replaced with per-pane overlay (Encapsulation — each pane manages its own loading state)
- Nexus cwd tracking: daemon relays getCwd via stateUpdate PDU, identical flow to standalone (BLESSED violation — standalone/nexus divergence eliminated)
- DSR responses dropped in client mode: flushResponses() never called after process() in feedBytes (nvim startup degraded)
- Shutdown crash: Loader threads accessing freed Processors due to wrong member destruction order (use-after-free)

### Technical Debt / Follow-up
- None deferred

## Sprint 3: BLESSED Audit Sweep

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Led audit analysis, directed all passes, wrote cleanup plan
- Pathfinder: Codebase discovery, file-size scoping, direct-access sweep
- Auditor: Comprehensive BLESSED/NAMES/JRENG audit (14 dimensions)
- Engineer: Encapsulation refactor, decomposition, file extraction, rename/relocate

### Files Modified (18 total)
- `Source/terminal/logic/Processor.h` — private members, reference getters, bundled methods (processWithLock, setHostWriter)
- `Source/terminal/logic/Processor.cpp` — bundled method definitions
- `Source/terminal/logic/Input.h` — new (relocated from component/InputHandler.h, renamed InputHandler to Terminal::Input)
- `Source/terminal/logic/Input.cpp` — new (relocated from component/InputHandler.cpp, all access via getters)
- `Source/terminal/logic/Mouse.h` — new (relocated from component/MouseHandler.h, renamed MouseHandler to Terminal::Mouse)
- `Source/terminal/logic/Mouse.cpp` — new (relocated from component/MouseHandler.cpp, all access via getters)
- `Source/component/InputHandler.h` — deleted
- `Source/component/InputHandler.cpp` — deleted
- `Source/component/MouseHandler.h` — deleted
- `Source/component/MouseHandler.cpp` — deleted
- `Source/component/TerminalDisplay.h` — updated includes, member types to Terminal::Input/Mouse
- `Source/component/TerminalDisplay.cpp` — all direct Processor access replaced with getters
- `Source/component/Panes.cpp` — processor.uuid to processor.getUuid()
- `Source/component/Tabs.h` — added Tabs::restore declaration
- `Source/component/Tabs.cpp` — added Tabs::restore definition (recursive walk, no intermediate types)
- `Source/MainComponent.h` — deleted broken TabRestoreEntry method declarations
- `Source/MainComponent.cpp` — deleted anon namespace, file-scope constants, collect/replay methods; initialiseTabs calls Tabs::restore
- `Source/MainComponentActions.cpp` — new (6 register*Actions method definitions extracted)
- `Source/nexus/Session.h` — 3 private mode helpers declared, setHostWriter
- `Source/nexus/Session.cpp` — create() decomposed into createClientSession/createDaemonSession/createLocalSession, single-exit
- `Source/nexus/Client.h` — removed processorUuids shadow state, stale docs fixed
- `Source/nexus/Client.cpp` — switch dispatch with 5 handler methods, shadow state eliminated, processor->getUuid()
- `Source/nexus/Loader.h` — onFinished takes no args, loaderJoinTimeoutMs constant
- `Source/nexus/Loader.cpp` — uuid captured by value in onFinished lambda (UseAfterFree fix)
- `Source/nexus/ServerConnection.cpp` — processor.uuid to processor.getUuid()
- `Source/nexus/Message.h` — stale doc comment corrected
- `Source/AppState.h` — dead ensureProcessorsNode removed, getProcessorsNode/getLoadingNode added
- `Source/AppState.cpp` — dead method removed, accessor implementations
- `Source/Main.cpp` — nexus construction reordered after mainWindow for listener availability

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Processor encapsulation: 100+ direct member accesses routed through getters/bundled API
- UseAfterFree in Loader::onFinished lambda (const ref to destroyed Loader::uuid)
- ProcessorList arrival race (connected=true before processorList PDU round-trip)
- AppState ctor regression (load() vs initDefaults() ordering)
- MainComponent listener bootstrap (lazy processorsNode/loadingNode assignment)
- Broken MainComponent.h (TabRestoreEntry referenced but only defined in anon-ns)
- Anonymous namespace violation in MainComponent.cpp eliminated
- File-scope statics (fake VT100 fallback constants) eliminated
- InputHandler/MouseHandler naming and location (component/ to terminal/logic/)
- Session::create 5 early returns eliminated via single-exit with mode helpers
- Client::messageReceived if-else chain replaced with switch dispatch

### Technical Debt / Follow-up
- None deferred — all audit findings resolved or accepted as KEEP with BLESSED justification

## Handoff to COUNSELOR: Nexus Byte-Forward Architecture — Session Continuity + Final BLESSED Pass

**From:** COUNSELOR
**Date:** 2026-04-08
**Status:** In Progress — baseline works, residual bugs identified, env var unification still pending

### Context
Multi-day session that started mid-execution of PLAN-nexus.md (state-replication architecture) and pivoted to a byte-forward architecture after repeated BLESSED violations and functional failures exposed the state-replication approach as wrong. ARCHITECT locked the new invariant:

> **The ONLY divergence between standalone and nexus paths is the ANSI byte data source** (standalone: reader thread from local TTY; nexus client: message thread from IPC). Every other branch on `isNexusMode` or mode-aware logic is a BLESSED violation.

Late in the session ARCHITECT also demolished the "extra/override/shell/var" garbage naming in env-var handling — the right model is `Terminal::Session::env` as a plain `std::map<String, String>` with `getEnv(name)` / `setEnv(name, value)`, no "extra", no "override".

### Completed
**Byte-forward refactor (landed, built, partially working):**
- `Terminal::History` (`Source/terminal/data/History.{h,cpp}`) — fixed-capacity byte ring buffer, `append` / `snapshot`, juce::HeapBlock + CriticalSection, bounded by `terminalScrollbackLines * bytesPerLineEstimate`
- `Terminal::Session` (`Source/terminal/logic/Session.{h,cpp}`) — owns TTY + History. `onBytes` callback forwards bytes to consumer. Replaces Processor's TTY ownership.
- `Terminal::Processor` stripped to pure pipeline: State + Grid + Parser. `encodeKeyPress`/`encodeMouseEvent`/`encodeFocusEvent` const encoders. `process(bytes, len)` feeds parser.
- `Nexus::Session::create` routes internally by mode: local/daemon constructs Terminal::Session + wires onBytes; client constructs Processor only, registers with Client
- New wire messages: `Message::output` (daemon → client live bytes), `Message::history` (daemon → client snapshot on attach)
- Deleted entirely: `Nexus::StateInfo`, `Nexus::DirtyRow`, `Grid::writeDirtyRowsToStream`, `Grid::readDirtyRowsFromStream`, `Grid::writeScrollbackRow`, `Grid::setSeqno`, `Processor::getStateInformation`, `Processor::setStateInformation`, `Processor::HostedTag`, `Processor::startShell`, `Processor::writeToPty`, `Processor::addExtraEnv`, `Processor::getShellEnvVar`, `Processor::hasShellExited`, `Processor::onShellExited`, `Processor::setShellProgram`, `Processor::setWorkingDirectory`

**Daemon persistence fixes:**
- `Nexus::Session::create` now idempotent: on duplicate uuid, returns existing Processor without spawning a new shell or creating a dangling reference
- `Main.cpp` nexus client branch probes existing lockfile port via `juce::StreamingSocket` with 200ms timeout; only spawns daemon if port is dead or lockfile absent. Session continuity across GUI restarts now works.
- Daemon log stays alive across GUI close/reopen cycles

**State setter API cleanup (BLESSED-E encapsulation):**
- `Terminal::State::get()` is now const-only (non-const overload removed)
- `Terminal::State::setId(uuid)` added as proper setter — replaces three `state.get().setProperty(jreng::ID::id, ...)` violations
- Single uuid SSOT: `Processor` ctor now takes uuid explicitly, no internal generation. `Session::create` generates once, flows through ctor → member → map key → `state.setId`

**Unified tab/split restoration walker (no isNexusMode branch):**
- `MainComponent::initialiseTabs` refactored to a single code path. Walks the saved TABS tree, extracts each TAB's full pane hierarchy (splits + ratios), then rebuilds via `addNewTab` + `splitAt` + ratio restoration
- `Panes::splitAt(targetUuid, newUuid, cwd, direction, isVertical)` new public method — allows restoration to place panes at specific positions with specific uuids
- `Panes::splitImpl` now delegates to `splitAt` with `AppState::getActivePaneID()` as target and empty uuid hint
- Deleted: `findFirstPane`, the `if (isNexusMode) … else …` branch in initialiseTabs
- `Tabs::getActivePanes()` made public so the walker can reach the live Panes after `addNewTab`

**Parser response wiring (partial):**
- `Nexus::Session::create` client branch now wires `proc->parser->writeToHost = [this, uuid] (data, len) { client->sendInput(uuid, data, len); }` — DSR/DA/CPR replies flow from client parser → IPC → daemon PTY. Fixes "Did not detect DSR response" in vim/neovim. `[this, uuid]` capture with no raw pointer extraction.

**Dimension fix on history replay:**
- `Message::history` wire format now carries `cols (uint16) + rows (uint16)` after the uuid prefix
- Daemon's `Session::sendHistorySnapshot` writes its current dims from the stub Processor's grid
- Client's `Client::messageReceived` history branch parses cols/rows, calls `processor->resized(cols, rows)` before feeding history bytes to `process()` — prevents parser from replaying cursor-positioning escapes at wrong dimensions

**Other byte-forward supporting changes (from earlier in session):**
- `Nexus::Client::getContext()` removed from external visibility; Client is private to `Nexus::Session`
- Client/Server owned as `unique_ptr<...>` by Session (three-mode tag struct ctors: default/DaemonTag/ClientTag)
- `Nexus::Wire.h` consolidated binary codec helpers (SSOT)
- `Nexus::Server::getLockfile()` single source of truth for lockfile path
- `Nexus::wireMagicHeader` single source of truth for IPC magic
- `Terminal::Processor::defaultCols` / `defaultRows` single source of truth for default terminal geometry
- `Terminal::State::setDimensions` called from Processor::setStateInformation (before refactor deleted it)
- `Terminal::History::bytesPerLineEstimate` = 256 (single derivation from `terminalScrollbackLines`)
- All `juce::StringArray` replacing `std::vector<juce::String>` in nexus files
- All known silent-fail `if (ptr != nullptr)` double-guards removed; `jassert` + unconditional body
- Log file split: `end-nexus-client.log` (GUI) and `end-nexus-daemon.log` (daemon)
- JUCE FileLogger explicit `shutdownLog()` in `ENDApplication::shutdown` to avoid static-lifetime leak detector

### Remaining
**Confirmed broken / needs fix after last test run:**

1. **Rendering artifacts on nexus reconnect (PARTIALLY FIXED, needs verification):** Dimension fix on `Message::history` just landed but was NOT yet built/tested. ARCHITECT should rebuild and retest:
   - vim session persistence (should now render at correct dims)
   - btop rendering (should resolve if it was the dimension issue)
   - "Active prompt ghost / shadow" on first pane (likely dimension-related, verify)
   - "Last active tab always broken" on reconnect (likely dimension-related, verify)

2. **Env var feature (`getShellEnvVar` / `addExtraEnv`) still broken in nexus mode:** ARCHITECT's final demolition:
   - HEAD implementation reads live shell PATH via `tcgetpgrp` + `sysctl(KERN_PROCARGS2)` (macOS) / `/proc/<pid>/environ` (Linux) in `Terminal::Session::getShellEnvVar` → `UnixTTY::getEnvVar`. **Pure OS process introspection**, no shell integration, no OSC.
   - Current byte-forward refactor stubbed `Terminal::Display::getShellEnvVar` and `addExtraEnv` to no-op — feature completely broken in both modes. Popup PATH inheritance dead.
   - **ARCHITECT-locked fix direction (not yet implemented):** restore `Terminal::Session::getEnv(name)` and `Terminal::Session::setEnv(name, value)` (renamed from garbage `getShellEnvVar`/`addExtraEnv`), mirror on `Nexus::Session::getEnv(uuid, name)` / `setEnv(uuid, name, value)` for IPC routing. Non-nexus: direct call into local Terminal::Session. Nexus client: IPC round-trip (sync) to daemon.
   - Naming cleanup: no "extra", no "override", no "Shell", no "Var". `env` is a `std::map<String,String>` member, `getEnv` / `setEnv` are the accessors. Child shell's env constructed from this map at `execve` time.
   - MainComponent popup path becomes mode-agnostic: `Nexus::Session::getContext()->getEnv(activeUuid, "PATH")` + `setEnv(newUuid, "PATH", value)`. The `isNexusMode` guard at `MainComponent.cpp:577` is deleted.

3. **Deferred tab init mode branch (Option A locked, not yet implemented):** `MainComponent.cpp:75` has `if (not appState.isNexusMode()) initialiseTabs()`. Fix: always defer `initialiseTabs` until `nexus->onConnected` fires. Non-nexus `Nexus::Session` fires `onConnected` synchronously in its default ctor; client-mode `Nexus::Session` fires it on `Client::connectionMade`. Both modes subscribe the same way, `MainComponent::onNexusConnected` is the single handler.

4. **Verification after all fixes land:**
   - `grep -rn "isNexusMode" Source/` should only have legitimate hits: Main.cpp daemon-spawn branch, AppState getter/setter. NOTHING in MainComponent, Panes, Tabs, Display, Processor, State, Grid, Parser.
   - Session persistence smoke: run vim, quit, reopen → vim still there at correct dims
   - Split persistence smoke: split pane, quit, reopen → splits restored in the same layout
   - Popup PATH inheritance smoke (after env fix): custom PATH in shell A, popup from shell A inherits it

### Key Decisions
**Byte-forward architecture (ARCHITECT-locked, replacing original PLAN-nexus.md state-replication approach):**
- Daemon owns PTY + History (byte ring buffer). No Parser, State, Grid on daemon side.
- Client owns full pipeline (Processor + State + Grid + Parser + Display).
- Only the ANSI byte source differs between modes. Standalone: local TTY reader thread feeds `onBytes` → `Processor::process`. Nexus client: IPC `Message::output` → `Processor::process`. Identical sequence.
- Initial attach sends `Terminal::History::snapshot()` via `Message::history` with cols/rows. Client resizes processor, then pipes the whole byte chunk through its parser to reconstruct state.
- Scrollback preserved via daemon's bounded byte history. Single config `Config::Key::terminalScrollbackLines` drives both Grid row ring and History byte ring (multiplier `bytesPerLineEstimate = 256`).
- Rationale: JUCE's `get/setStateInformation` is one-shot save/load, not continuous push. `ValueTreeSynchroniser` exists but is not used in `juce_audio_processors` anywhere. Production terminal muxes (tmux/zellij/wezterm/mosh) all have the server owning the parser, never forward raw bytes as primary GUI path. END's byte-forward model is closest to tmux control mode + wezterm's `ClientPane` but simpler because END has one client per session, so independent client parsing is fine.

**`Nexus::Session` ownership and mode selection:**
- Three construction modes via tag struct: default (non-nexus local), `DaemonTag`, `ClientTag`. No bool flags.
- Daemon: owns `std::map<uuid, unique_ptr<Terminal::Session>>` (PTY side) + stub Processor map for uuid tracking + Server via `unique_ptr`
- Client: owns Client via `unique_ptr`, routes IPC messages. Processors owned by Panes via Display.
- Non-nexus: owns both Terminal::Session map and Processor map, wires them directly via onBytes lambda.
- `Session::create(shell, args, cwd, uuid)` is idempotent on duplicate uuid (returns existing Processor, no shell respawn).

**Display is passive reader holding `Terminal::Processor&`:**
- Reads `processor.state` and `processor.grid` directly (JUCE AudioProcessorEditor pattern).
- Subscribes as `juce::ChangeListener` — Processor inherits `juce::ChangeBroadcaster`, fires `sendChangeMessage()` on `process()` completion so UI thread coalesces and repaints.
- Display input handlers call `processor.encodeX(...)` then `Nexus::Session::getContext()->sendInput(processor.uuid, bytes, len)` — Session routes internally.

**Naming discipline (BLESSED-E Explicit):**
- `get()` on containers is `const`-only when no caller legitimately mutates through it
- `state.get().setProperty(id, …)` is a violation — add proper `State::setId(uuid)` setter
- No "extra", no "override", no "Shell", no "Var" redundant prefixes. Just `env`, `getEnv`, `setEnv`.
- Tag structs over bool flags for ctor disambiguation.
- `.get()` on smart pointers is only for true non-owning boundary (JUCE API). Never for capturing in lambdas where `this` + member dereference works.

**Layer rules:**
- `Source/terminal/` cannot include `Source/nexus/*` headers (except pure data struct `nexus/StateInfo.h` — but that's now deleted)
- `Source/nexus/` can include `Source/terminal/` freely
- `Source/component/` can include both

### Files Modified
Byte-forward refactor landed across:
- `Source/terminal/data/History.h`, `History.cpp` — new
- `Source/terminal/logic/Session.h`, `Session.cpp` — new
- `Source/terminal/logic/Processor.h`, `Processor.cpp` — stripped
- `Source/terminal/logic/Grid.h`, `Grid.cpp` — stream methods deleted
- `Source/terminal/data/State.h`, `State.cpp` — `setId` added, `get()` const-only, stale subscriber methods dead code
- `Source/nexus/Session.h`, `Session.cpp` — three-mode tag ctor, idempotent `create`, `sendHistorySnapshot` with cols/rows, local/daemon/client routing
- `Source/nexus/SessionFanout.cpp` — handleAsyncUpdate rewritten (no stateInfo)
- `Source/nexus/Server.h`, `Server.cpp` — `getLockfile()` public SSOT
- `Source/nexus/ServerConnection.h`, `ServerConnection.cpp` — attach now sends history
- `Source/nexus/Client.h`, `Client.cpp` — `output`/`history` dispatch, no stateInfo path, hostedProcessors routing
- `Source/nexus/Message.h` — `output`/`history` added, `stateInfo`/`listSessions*`/`getRenderDelta`/`renderDelta` deleted
- `Source/nexus/Wire.h` — binary codec helpers + `wireMagicHeader`
- `Source/nexus/Log.h`, `Log.cpp` — `initLog(filename)` + `shutdownLog()`, separate daemon/client logs
- `Source/nexus/StateInfo.h` — deleted
- `Source/component/Panes.h`, `Panes.cpp` — `splitAt` new public method, `buildTerminal` + `teardownTerminal` via `Nexus::Session`, `hostedProcessors` moved to Client
- `Source/component/Tabs.h` — `getActivePanes` public
- `Source/component/TerminalDisplay.h`, `TerminalDisplay.cpp` — Processor& reference, ChangeListener, `getShellEnvVar`/`addExtraEnv` stubbed to no-op (broken — see Remaining)
- `Source/component/InputHandler.h`, `InputHandler.cpp` — member renamed `session → processor`, `handleKey` restructured without `bool handled` flag
- `Source/component/MouseHandler.h`, `MouseHandler.cpp` — member renamed, accesses via `processor.state` / `processor.grid`
- `Source/component/Popup.h`, `Popup.cpp` — `onTeardown` callback removed (dead), `removePopupSession` uses `teardownTerminal`
- `Source/Main.cpp` — three startup branches, StreamingSocket probe before spawnDaemon, `Nexus::shutdownLog` in shutdown
- `Source/MainComponent.h`, `Source/MainComponent.cpp` — `onNexusConnected` deferred init, unified restoration walker, popup path still has broken env guard
- `Source/AppState.h`, `Source/AppState.cpp` — `clientMode → nexusMode` rename, `isNexusMode/setNexusMode`
- `Source/AppIdentifier.h` — `clientMode → nexusMode`
- `Source/terminal/data/Identifier.h` — subscriber identifiers (now dead)
- Various sibling files for stale comment updates and rename cascades

### Open Questions
1. **`Terminal::Session::getEnv` / `setEnv` API (ARCHITECT-locked naming, not yet implemented):** method signatures are agreed but the actual wiring hasn't been written. Needs:
   - Restore the OS-process-introspection code (`tcgetpgrp` + `sysctl(KERN_PROCARGS2)` macOS / `/proc/environ` Linux) that HEAD had in `UnixTTY::getEnvVar`. Was not moved over in the byte-forward refactor.
   - Decide on sync vs async IPC round-trip in nexus client mode. Popup construction is a user-initiated action so brief blocking on the message thread is probably acceptable UX.
   - Add new Message kinds: `Message::getEnv` (request), `Message::getEnvResponse` (reply), `Message::setEnv` (fire-and-forget).
2. **Async IPC round-trip pattern**: the existing JUCE `InterprocessConnection` is message-based. To do a synchronous-looking `getEnv(uuid, name) -> String` on the client side we'd need either (a) a blocking wait with a timeout, (b) keep the API async with a callback. Not yet decided.
3. **btop rendering**: is it downstream of the dimension fix or a separate bug? Verify after rebuild.
4. **`Processor::onShellExited`** — deleted in the byte-forward refactor. On daemon side, `Terminal::Session::onExit` lambda now handles the "shell died" lifecycle (callAsync → remove → broadcastProcessorList → onAllSessionsExited). On non-nexus, same mechanism. Verify this chain still fires correctly — no known bug but was refactored heavily.

### Next Steps
1. **ARCHITECT rebuild and test current working tree** — the dimension fix on `Message::history` and parser writeToHost wiring both just landed but were not built/tested. Share logs after test, especially daemon + client log for the reconnect scenario with vim running.
2. **If vim/btop render correctly** after step 1 → proceed to env var unification (item 2 in Remaining). If still broken → diagnose with fresh logs before any more code changes.
3. **Deferred tab init unification**: delete the `isNexusMode` guard at `MainComponent.cpp:75`. Always defer `initialiseTabs` until `nexus->onConnected` fires. Requires `Nexus::Session` default-ctor (non-nexus) to fire `onConnected` immediately from its constructor body.
4. **Env var unification**: implement `Terminal::Session::getEnv` / `setEnv` (restore OS introspection from HEAD), `Nexus::Session::getEnv(uuid, name)` / `setEnv(uuid, name, value)` with mode routing, IPC message kinds for the daemon round-trip, MainComponent popup path updated to the mode-agnostic two-line pattern.
5. **Final `isNexusMode` audit**: grep should show only Main.cpp daemon-spawn branch and AppState getter/setter. Anything else is a BLESSED violation.
6. **Split persistence verification**: the unified restoration walker just landed. Verify two-pane split survives quit/relaunch with correct layout.

---

## Handoff to COUNSELOR: Nexus Refactor — PLAN-nexus.md Revision Complete

**From:** COUNSELOR
**Date:** 2026-04-07
**Status:** Ready for Implementation — PLAN-nexus.md fully locked, all decisions made, no open items

### Context
Session consumed RFC-NEXUS.md and the previous COUNSELOR handoff (Nexus Processor/Display Architecture Fix). Ran extended ARCHITECT discussion to resolve every naming, ownership, and API design question before writing the execution plan. All discussion was in service of producing PLAN-nexus.md with zero ambiguity so @Engineer can execute step-by-step without further clarification rounds.

Key framing shift during the session: the wire payload is **processor state serialized for a proxy**, not a "delta" or "frame." This renamed the API from the BRAINSTORMER-proposed `applyDelta` (and COUNSELOR's intermediate `processFrame` misstep) to JUCE-literal `getStateInformation` / `setStateInformation`, mirroring `juce::AudioProcessor`'s exact preset save/restore API.

### Completed

**PLAN-nexus.md fully revised and written.** 16 sequential steps, all decisions locked, per-step @Auditor checklist, forbidden-token table, BLESSED alignment notes, dependency graph. Ready for @Engineer consumption.

**Pathfinder recons completed (3 sweeps):**
1. Initial rename scope + Grid grapheme API + PTY timing + Client threading
2. Grid writer API audit for proxy-mode setStateInformation correctness
3. Final sweep: State setters, AppState::isClientMode consumers, Client lifetime, lockfile protocol, per-subscriber seqno tracking today, wire serialization pattern, InputHandler/MouseHandler refs, LoaderOverlay API, Cell::LAYOUT_GRAPHEME, HeapBlock API

**Librarian recons completed (2 sweeps):**
1. JUCE AudioProcessor/Editor API naming + DAW out-of-process bridging precedent (Apple AUv3, Reaper, Bitwig, Ardour, GoF Remote Proxy)
2. JUCE `InterprocessConnectionServer` ownership contract (verified base class is ownership-opaque, contradicting BRAINSTORMER's initial reading)

**Researcher recon completed:**
- Wezterm `ClientPane` + `apply_changes_to_surface` + `GetPaneRenderChangesResponse` architecture
- Tmux architecture (confirmed unusable as precedent — tmux sends ANSI escapes directly via fd-passing, doesn't replicate grid state)

**Contract documents read and applied:**
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- JRENG-CODING-STANDARD.md (C++ coding standards)
- CAROL.md (protocol)

### Remaining
Everything. Plan is the deliverable of this session; execution is the next session's work.

Execution order per PLAN-nexus.md dependency graph:
1. Step 0 — annotation sweep (READER THREAD/MESSAGE THREAD → PRODUCER/CONSUMER CONTEXT, codebase-wide)
2. Steps 1-3 — mechanical renames (Session→Processor, Host→Session, Pdu→Message)
3. Step 7 — StateInfo type definition (before Step 6 because Step 6 Grid methods return DirtyRow)
4. Step 6 — Grid additions (writeScrollbackRow, setSeqno, collectDirtyRows) + buildDelta deletion
5. Steps 4, 5 — Bug 1 and Bug 2 fixes
6. Step 8 — Terminal::Processor two-mode (the largest architectural step)
7. Steps 9, 10 — tell-don't-ask protocol + multi-subscriber seqno
8. Steps 11a/b/c — consumer updates (Panes, Tabs+Popup, Main+MainComponent)
9. Steps 12, 13 — Bug 3 wiring + Bug 4/5/6/7 verification
10. Step 15 — InputHandler/MouseHandler cascade (can run parallel with 11)
11. Step 14 — async startup + LoaderOverlay
12. Step 16 — final cleanup + forbidden-token sweep

### Key Decisions

**Architecture (L1-L18 in PLAN-nexus.md):**
- **L1:** One `Terminal::Processor` class, two ctors (local, hosted). No Proxy/Mirror/Stub subclass. Proxy-ness is structural (constructor choice). Matches JUCE `AudioProcessor` standalone/plugin pattern.
- **L2:** `state` and `grid` are public members on Processor. Display reads `processor.state.*` / `processor.grid.*` directly. Exact 1:1 with `AudioProcessorEditor::processor` at `juce_AudioProcessorEditor.h:68`. No unnecessary getters.
- **L3:** IPC transfer methods named `getStateInformation` / `setStateInformation` — literal JUCE convention. Mode-neutral (same method on real and proxy Processor).
- **L4:** Wire payload type `Nexus::StateInfo` (replaces `Nexus::GridDelta`). Names content, not mechanism.
- **L5:** `StateInfo` uses `juce::HeapBlock<DirtyRow>` + `HeapBlock<Cell>` / `HeapBlock<Grapheme>`. Two-pass count-then-fill. No `std::vector` in wire path (project convention for trivially copyable).
- **L6:** `Grid::buildDelta` deleted. Replaced by `Grid::collectDirtyRows(sinceSeqno)`. Grid no longer reads State or Nexus types. Full StateInfo assembly moves to Processor.
- **L7:** Two new Grid APIs mandatory — `writeScrollbackRow` (no existing public API for scrollback writes, blocks proxy) and `setSeqno` (seqno is private atomic, proxy needs to align). Pathfinder confirmed both gaps in pass 2.
- **L8:** `GridDelta.cpp` deleted. Logic moves to `Processor::getStateInformation`.
- **L9:** `Pdu.h` → `Message.h`. `enum class PduKind` → `enum class Nexus::Message` (flat, no nested Kind). JUCE vocabulary, drops OSI jargon.
- **L10:** `jreng::Owner<ServerConnection>` on `Nexus::Server`. `connections.add(std::make_unique<ServerConnection>(session)).get()` one-liner. Librarian verified JUCE base is ownership-opaque — without this, current code leaks.
- **L11:** Server owns connections; Session has non-owning broadcast set `std::vector<ServerConnection*> attached` populated by `connectionMade`/`connectionLost`. Separation of ownership and broadcast intent.
- **L12:** `Client` uses `InterprocessConnection(true, ...)` — `callbacksOnMessageThread = true`. No raw-this lambdas. JUCE SafeAction handles lifetime.
- **L13:** Explicit `Processor::startShell()` lifecycle method. `resized()` becomes pure SIGWINCH. `callAsync` deferral at `Session.cpp:251-281` deleted. Fixes Bug 3 by construction.
- **L14:** Multi-subscriber (N>1 clients) in scope. Per-subscriber seqno tracking as ValueTree children under `ID::subscribers` on sidecar Processor's State. Migration from `Host::lastFanoutSeqno` (per-session watermark shared across subscribers — incorrect for N>1).
- **L15:** `LoaderOverlay.show(0, "Connecting to nexus")` during connect wait, `hide()` on `connectionMade`. Indeterminate mode renders spinner without progress bar.
- **L16:** Thread annotation rename scope: **entire codebase**. `// READER THREAD` / `// MESSAGE THREAD` → `// PRODUCER CONTEXT` / `// CONSUMER CONTEXT`. Honest across both modes.
- **L17:** `AppState::isClientMode` **kept**. Used at 4 construction points (Panes 3x, Tabs 1x) + 2 startup sites. Smaller blast radius than factory injection. ARCHITECT ruling.
- **L18:** No wire protocol backwards compatibility. Single-binary deployment.

**Naming (final):**
- `Terminal::Session` → `Terminal::Processor`
- `Nexus::Host` → `Nexus::Session`
- `Nexus::HostFanout` → `Nexus::SessionFanout`
- `Nexus::RemoteSession` → absorbed (file deleted)
- `Nexus::GridDelta` → `Nexus::StateInfo`
- `Nexus::CellRange` → `Nexus::DirtyRow`
- `Grid::buildDelta` → `Grid::collectDirtyRows`
- New: `Processor::getStateInformation`, `setStateInformation`, `startShell`
- New: `Grid::writeScrollbackRow`, `setSeqno`
- `Source/nexus/Pdu.h` → `Source/nexus/Message.h`
- `enum class PduKind` → `enum class Nexus::Message` (flat)
- `Message::renderDelta` → `Message::stateInfo`
- `spawnSession/attachSession/detachSession/sessionExited` → `spawnProcessor/attachProcessor/detachProcessor/processorExited`
- `getRenderDelta`, `listSessions`, `listSessionsResponse` → deleted (tell-don't-ask)
- New: `Message::processorList` (push on connectionMade)
- `// READER THREAD` / `// MESSAGE THREAD` → `// PRODUCER CONTEXT` / `// CONSUMER CONTEXT`

**Grid return type (L6 follow-up, locked):** `Grid::collectDirtyRows` returns `juce::HeapBlock<Nexus::DirtyRow>`. Grid.h includes `nexus/StateInfo.h`. No intermediate Terminal::ChangedRow conversion.

**Key rationale highlights:**
- **Why `getStateInformation`/`setStateInformation` and not `processFrame`:** `processBlock`-style verbs (`process`) are for *computing on* work units. The proxy doesn't compute — it *adopts* state. JUCE's `getStateInformation`/`setStateInformation` is the preset save/restore API, which is exactly what cross-process proxy sync is semantically (continuous restore instead of one-shot).
- **Why `StateInfo` is not called "Delta" or "Frame":** only 1/10 fields are incremental (dirtyRows). 9/10 are absolute snapshots (cursor, title, cwd, geometry, etc.). Naming it "Delta" privileged the transmission optimization over the semantic payload. "StateInfo" matches JUCE method names exactly.
- **Why public `state`/`grid` members:** exact replication of `AudioProcessorEditor::processor` public member pattern. No getters where JUCE has none. NAMES.md forbids dead getters.
- **Why multi-subscriber tracking in ValueTree children on State:** project idiom is "all state lives in Terminal::State as ValueTree." External maps in Session/Client would violate SSOT.
- **Why JUCE `OwnedArray` was rejected in favor of `jreng::Owner`:** jreng::Owner is the project's modern smart-pointer-based equivalent to the pre-C++11 OwnedArray. Jules Storer (JUCE) has publicly said smart pointers are preferred for new code.

### Files Modified

- `PLAN-nexus.md` — fully rewritten (file previously contained an earlier plan draft; now reflects all locked decisions)
- `carol/SPRINT-LOG.md` — this handoff entry prepended

No source code modified. All code work is delegated to @Engineer in next session per the plan's step-by-step structure.

### Open Questions
None. All decisions locked. The only open item surfaced during plan-writing (Grid return type a/b) was resolved by ARCHITECT as (a) and the plan updated accordingly.

### Next Steps

**Immediate (next COUNSELOR session):**
1. ARCHITECT activates COUNSELOR with context: read PLAN-nexus.md, carol/SPRINT-LOG.md (this entry), and optionally RFC-NEXUS.md for background.
2. COUNSELOR invokes @Pathfinder for Step 0 enumeration (every `// READER THREAD` and `// MESSAGE THREAD` annotation site codebase-wide). Pathfinder returns the full list.
3. COUNSELOR delegates Step 0 execution to @Engineer with the Pathfinder-verified site list.
4. @Auditor verifies Step 0 against the checklist (zero `// READER THREAD` / `// MESSAGE THREAD` survivors, comments-only diff).
5. Proceed to Step 1 (mechanical Terminal::Session → Terminal::Processor rename) only after Step 0 is signed off.

**Gating discipline:** execute ONE STEP AT A TIME. @Auditor signs off before next step begins. ARCHITECT may gate at any step boundary. Do not batch multiple steps in one Engineer pass — the incremental validation is the whole point of the 16-step decomposition.

**Recovery hint if a step fails:** PLAN-nexus.md step dependency graph shows which steps must be reverted together. Any failure in Step 8 (Terminal::Processor two-mode) is the highest-risk rollback — it depends on Steps 1, 2, 6, 7 being in place.

### Commit Message (for current uncommitted changes)

```
docs(nexus): lock PLAN-nexus.md execution plan for Nexus refactor

Rewrite PLAN-nexus.md with all ARCHITECT-locked decisions from extended
COUNSELOR discussion. Supersedes the earlier draft. 16 sequential steps
covering: thread-annotation sweep, Terminal::Session → Terminal::Processor
rename, Nexus::Host → Nexus::Session rename, Pdu.h → Message.h rename,
StateInfo type replacing GridDelta (HeapBlock-based), Grid API additions
(writeScrollbackRow, setSeqno, collectDirtyRows), Bug 1 ServerConnection
ownership via jreng::Owner, Bug 2 Client threading via callbacksOnMessageThread,
Terminal::Processor two-mode absorbing RemoteSession, tell-don't-ask protocol,
multi-subscriber ValueTree seqno tracking, split consumer updates, Bug 3
startShell lifecycle, async startup with LoaderOverlay, final cleanup.

API naming matches juce::AudioProcessor 1:1 (getStateInformation /
setStateInformation, public state/grid members per AudioProcessorEditor).
Wire vocabulary matches JUCE InterprocessConnection (Message, messageReceived).

Ready for Engineer execution; step-by-step with per-step Auditor checklist
and forbidden-token table for validation gating.

Pre-flight recons logged: 3 Pathfinder sweeps, 2 Librarian sweeps, 1
Researcher sweep (wezterm/tmux), covering JUCE AudioProcessor source,
InterprocessConnectionServer ownership contract, wezterm ClientPane
precedent, Terminal::State setter API, existing Grid API gaps.

Also prepend handoff entry to carol/SPRINT-LOG.md for session continuity.
```

---

## Handoff to COUNSELOR: Nexus Processor/Display Architecture Fix

**From:** BRAINSTORMER
**Date:** 2026-04-07
**Status:** Ready for Implementation — RFC complete, all decisions made

### Context
BRAINSTORMER session to diagnose why `nexus = true` produces a blank window while `nexus = false` works. Evolved into a full architectural audit and redesign. The previous implementation was poisoned by wezterm/tmux hand-rolled IPC patterns that fought JUCE's threading model. BRAINSTORMER analyzed all JUCE IPC source code, both working JUCE IPC examples, and every nexus source file. Found 7 bugs. Designed the fix architecture based on JUCE's actual AudioProcessor/AudioProcessorEditor pattern (read from source, not docs).

### Completed
- Full root cause analysis: 7 bugs identified with evidence from JUCE source
- RFC-NEXUS.md written — single authoritative document, no contradictions
- Naming correction: Terminal::Session → Terminal::Processor, Nexus::Host → Nexus::Session, RemoteSession absorbed
- Architecture: one Processor class with two construction modes (local/hosted), ChangeBroadcaster/ChangeListener pattern
- UX model defined: tmux-like session persistence, exit hierarchy, sidecar lifecycle
- All ARCHITECT decisions captured: RAII cleanup, signal/clock separation, tell-don't-ask, async startup, coalesced fan-out
- Analyzed JUCE source: InterprocessConnection, InterprocessConnectionServer, ConnectedChildProcess, AudioProcessor, AudioProcessorEditor, ChangeBroadcaster

### Remaining
- Rename sprint: Terminal::Session → Terminal::Processor across entire codebase (mechanical, first priority)
- Fix all 7 bugs per RFC architecture
- Verify Grid API has `activeVisibleGraphemeRow()` for Bug 7 fix (or add it)
- Background connect thread design for fresh-spawn case
- Shell exit mechanism: Processor sets atomic `exited` flag + `sendChangeMessage()`, Session detects in `handleAsyncUpdate()`

### Key Decisions
- **One Processor, two modes:** Local ctor (owns PTY) vs hosted ctor (shadow Grid + IPC). Display holds `Processor&` always. No proxy class. No interface. No dual-path routing.
- **ChangeBroadcaster:** Processor extends it. Display extends ChangeListener. JUCE-sanctioned, event-driven, lifetime-safe.
- **callbacksOnMessageThread = true** on both Client and ServerConnection. JUCE SafeAction protects everything.
- **Server owns connections:** `jreng::Owner<ServerConnection>`. Cleanup in connectionLost. No timer, no polling.
- **Tell, don't ask:** Sidecar pushes unsolicited. `getRenderDelta` and `listSessions` PDUs eliminated. Attach triggers full snapshot push.
- **Signal/clock separation:** ChangeBroadcaster → setSnapshotDirty (signal). VBlank → render (clock).
- **Sidecar notification:** AsyncUpdater coalesces N dirty Processors into one batched fan-out. ChangeBroadcaster per-Processor on GUI side.
- **Display RAII:** Destructor calls `processor.removeChangeListener(this)` + `processor.displayBeingDeleted(this)`.
- **Sidecar runs real Processors:** Same class, local constructor. ChangeBroadcaster with no Display listeners — Nexus::Session listens instead.
- **Rename same sprint, first priority.**

### Files Modified
- `RFC-NEXUS.md` — NEW. Complete architecture RFC. Single authority for COUNSELOR. Supersedes PLAN-mux-server.md "Remaining Work" section.

### Open Questions
- None for ARCHITECT. All decisions made.
- COUNSELOR implementation details: background connect thread lifetime management, Grid grapheme row API verification.

### Next Steps
1. COUNSELOR reads RFC-NEXUS.md (single document, no external dependencies)
2. Write PLAN for rename sprint + fix sprint (can be combined per ARCHITECT decision)
3. Rename first: Terminal::Session → Terminal::Processor, Nexus::Host → Nexus::Session, delete RemoteSession
4. Fix Bug 1 (connection ownership) + Bug 2 (callbacksOnMessageThread) — prerequisites
5. Fix Bug 3 (blank screen timing) — unblocked by 1+2
6. Fix Bugs 4+5 (InputHandler/MouseHandler) — independent
7. Fix Bug 6 (splitImpl) — two missing calls
8. Fix Bug 7 (graphemes) — after Grid API verification

### Recommended Commit Message
```
docs(nexus): RFC for Processor/Display architecture fix

Root cause analysis of blank-screen bug found 7 issues:
connection leaks, unsafe threading, missing input handlers,
deferred PTY timing, grapheme copy omission.

Redesign based on JUCE AudioProcessor/Editor pattern:
- One Processor class, two modes (local PTY / hosted IPC)
- ChangeBroadcaster/ChangeListener (JUCE-sanctioned)
- Tell-don't-ask: sidecar pushes, client declares intent
- Nexus::Session as listener-orchestrator
- Terminal::Session → Terminal::Processor rename

All JUCE IPC source audited. All architect decisions captured.
```

---


## Handoff to COUNSELOR: Nexus Mux Server

**From:** COUNSELOR
**Date:** 2026-04-06
**Status:** In Progress — client-mode terminal rendering not working

### Context
Implementing a mux server architecture for END (Ephemeral Nexus Display) that follows the PluginProcessor/PluginEditor pattern from JUCE audio plugins. Session (Processor) owns state and lives independently in a background daemon; Display (Editor) is ephemeral UI that connects as a client. Goal: sessions survive window close, relaunch reconnects.

Architecture: `end --nexus` runs headless daemon. `end` (with `nexus = true` in config) spawns daemon if needed, connects as client, shows UI. `nexus = false` reverts to single-process behavior.

### Completed
- Nexus::Host — session pool, Context<Host>, owns all Sessions
- Terminal::Component renamed to Terminal::Display across entire codebase
- Session::createDisplay() — Processor creates Editor pattern
- SeqNo + GridDelta — monotonic version tracking on Grid, delta builder
- Pdu.h — PduKind enum for message vocabulary
- JUCE InterprocessConnectionServer/Connection — replaced hand-rolled AF_UNIX IPC
- Nexus::Server — TCP localhost, port lockfile at ~/.config/end/end.port
- Nexus::ServerConnection — per-client PDU dispatch (spawn, input, resize, attach, detach, getRenderDelta)
- Nexus::Client — InterprocessConnection + Context<Client>, connects to daemon
- Nexus::RemoteSession — shadow Grid+State for client-mode Display
- Terminal::Display dual-mode — local (reads Session directly) vs client (reads RemoteSession)
- Push fan-out — AsyncUpdater on Host, onHostDataReceived fires triggerAsyncUpdate from reader thread, handleAsyncUpdate builds deltas and pushes to subscribers
- Headless daemon — end --nexus, hideDockIcon, onAllSessionsExited quits
- NexusDaemon.h/.mm/_win.cpp — posix_spawn (macOS/Linux), CreateProcess (Windows)
- nexus = true/false config key in end.lua
- AppState stores clientMode — no manual boolean flags
- Session restoration — state.xml layout matching in client mode
- Step 10 eliminated — JUCE TCP localhost is cross-platform

### Remaining
1. **Connection drops (critical):** `connectionLost()` fires unexpectedly during client operation. Root cause unknown. Currently connectionLost is an empty body (crash was fixed — callAsync with dangling `this` captured when Client was being destroyed). Need to investigate WHY the TCP connection drops between UI and daemon on localhost.
2. **Client-mode terminal blank (critical):** Delta delivery chain has issues. Push fan-out is implemented but connection instability may prevent deltas from arriving. Even when connection stays up, the terminal renders blank. Need to verify: does spawnSession create a real PTY in the daemon? Does the PTY produce output? Does handleAsyncUpdate fire? Does the delta reach the client? Does applyDelta work?
3. **5-second blocking poll (UX):** Main.cpp::initialise() blocks message thread for up to 5 seconds polling for daemon readiness. Unacceptable. Need async startup — spawn daemon, show window immediately, connect in background, attach when ready.
4. **connectionLost safe cleanup:** When connection genuinely drops (not shutdown), remoteSessions should be cleaned up. Current empty body leaks. Need safe cleanup that doesn't crash on shutdown (the callAsync + dangling this problem).

### Key Decisions
- **Processor/Editor pattern:** Session is the Processor (always alive, owns state), Display is the Editor (ephemeral, created/destroyed). Nexus::Host is the DAW Host. This mirrors JUCE audio plugin architecture exactly.
- **Naming:** END = Ephemeral Nexus Display. Nexus IS the architecture. Terminal::Display (not Component/View).
- **JUCE IPC over TCP localhost:** Replaced hand-rolled AF_UNIX with JUCE InterprocessConnectionServer/Connection. Cross-platform, battle-tested framing, managed threading. Eliminated Step 10 (Windows AF_UNIX).
- **Config-driven:** `nexus = true` (default) enables daemon. `nexus = false` single-process. User never types --nexus.
- **AppState for clientMode:** Stored in ValueTree WINDOW node. All routing uses AppState::getContext()->isClientMode(). No manual boolean passing.
- **Push not poll:** AsyncUpdater for dirty notification. Reader thread triggers, message thread builds delta and fans out. VBlank is for rendering only.
- **UUID in renderDelta payload:** Every renderDelta carries the session UUID in the payload itself. No serial matching.

### Files Modified (new + modified, 39 total)

**New files (Source/nexus/):**
- `Source/nexus/Host.h/cpp` — Nexus::Host session pool
- `Source/nexus/Server.h/cpp` — InterprocessConnectionServer wrapper
- `Source/nexus/ServerConnection.h/cpp` — per-client connection handler
- `Source/nexus/Client.h/cpp` — client-side InterprocessConnection
- `Source/nexus/RemoteSession.h/cpp` — shadow Grid+State
- `Source/nexus/GridDelta.h` — GridDelta/CellRange value types
- `Source/nexus/HostFanout.cpp` — subscriber registry, push fan-out, serializeGridDelta
- `Source/nexus/Pdu.h` — PduKind enum
- `Source/nexus/NexusDaemon.h` — hideDockIcon, spawnDaemon declarations
- `Source/nexus/NexusDaemon.mm` — macOS/Linux implementations
- `Source/nexus/NexusDaemon_win.cpp` — Windows implementation

**Renamed files:**
- `Source/component/TerminalComponent.h/cpp` → `Source/component/TerminalDisplay.h/cpp`

**New file (terminal):**
- `Source/terminal/logic/GridDelta.cpp` — Grid::buildDelta() implementation

**Modified files:**
- `Source/Main.cpp` — three launch paths (--nexus, nexus=true client, nexus=false single), daemon spawn, shutdown order
- `Source/MainComponent.h/cpp` — initialiseTabs client-mode branch
- `Source/AppState.h/cpp` — setClientMode/isClientMode
- `Source/AppIdentifier.h` — App::ID::clientMode
- `Source/component/Panes.h/cpp` — dual-mode createTerminal, closePane with Host::remove
- `Source/component/Tabs.h/cpp` — addNewTab(cwd, uuid) overload, closeActiveTab session cleanup
- `Source/component/Popup.h/cpp` — removePopupSession helper, session cleanup on dismiss
- `Source/config/Config.h/cpp` — Key::nexus, addKey, Lua reader
- `Source/config/default_end.lua` — nexus = true default
- `Source/terminal/logic/Grid.h/cpp` — seqno, rowSeqnos, markRowDirty/batchMarkDirty tracking
- `Source/terminal/logic/Session.h/cpp` — createDisplay, onHostShellExited, onHostDataReceived
- `Source/terminal/rendering/Screen.h` — comment updates
- `Source/terminal/selection/LinkSpan.h` — comment updates
- Various .h files — Terminal::Component → Terminal::Display in comments

### Open Questions
- Why does InterprocessConnection drop on localhost? Is it a JUCE bug, a timing issue, or a configuration problem (magic number mismatch, read timeout)?
- Should the daemon use a different JUCE application model? Currently uses START_JUCE_APPLICATION with no window. Does NSApplication (created by JUCE) interfere with headless operation?
- Is the probe loop (temporary Client objects connecting/disconnecting rapidly) destabilizing the daemon's ServerConnection accept/teardown cycle?

### Next Steps
1. Investigate connectionLost — add file-based logging (not DBG, since MainComponent may not exist) to trace when and why connections drop
2. Verify the daemon is functioning: does spawnSession create a PTY? Does the shell start? Does handleAsyncUpdate fire in the daemon?
3. Consider simplifying startup: instead of probe loop, have the daemon write a "ready" signal to the lockfile after Server::start() succeeds
4. Fix the 5-second blocking poll — make it async
5. Once connection is stable, verify full delta delivery chain end-to-end

### Recommended Commit Message
```
feat(nexus): mux server architecture — daemon + client mode (WIP)

Processor/Editor pattern: Session owns state (daemon), Display is
ephemeral UI (client). JUCE InterprocessConnection for IPC.

- Nexus::Host session pool with Context<Host>
- Terminal::Component → Terminal::Display rename
- Session::createDisplay() (Processor creates Editor)
- SeqNo + GridDelta for incremental state transfer
- Server/ServerConnection (JUCE IPC, TCP localhost)
- Client + RemoteSession (shadow Grid for client-mode Display)
- Push fan-out via AsyncUpdater
- end --nexus headless daemon, nexus=true/false config
- NexusDaemon cross-platform process launch
- AppState clientMode routing

WIP: client-mode terminal rendering not functional yet.
Connection stability issues under investigation.
```

## Sprint 2: Fix macOS Titlebar Flash on Window Creation

**Date:** 2026-04-06
**Duration:** ~30m

### Agents Participated
- COUNSELOR: Root cause analysis, plan, orchestration
- Pathfinder: Codebase exploration (GlassWindow init sequence, BackgroundBlur API, renderer probe timing)
- Engineer: Code changes (configureWindowChrome, sync/async separation, dead code removal)
- Auditor: Validation (BLESSED compliance, dead code detection, idempotency check)

### Files Modified (4 total)
- `modules/jreng_gui/glass/jreng_background_blur.h:24` -- Added `configureWindowChrome` declaration; removed dead `hideWindowButtons` declaration
- `modules/jreng_gui/glass/jreng_background_blur.mm:162-184` -- Added `configureWindowChrome` implementation (synchronous NSWindow title hiding, style mask, button hiding); removed duplicated title/style mutations from `applyBackgroundBlur` and `applyNSVisualEffect`; removed dead `hideWindowButtons` implementation
- `modules/jreng_gui/glass/jreng_background_blur.cpp:210-212` -- Removed dead `hideWindowButtons` Windows stub
- `modules/jreng_gui/glass/jreng_glass_window.cpp:115` -- `visibilityChanged()` calls `configureWindowChrome` synchronously before `triggerAsyncUpdate()`; removed redundant `isBlurApplied = true` from `handleAsyncUpdate()`; removed redundant `hideWindowButtons` call from `setGlass()`; updated file and class doc comments

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **macOS titlebar flash on first show**: When GlassWindow became visible, the native titlebar was briefly rendered before being hidden by the async blur handler. Root cause: titlebar hiding was bundled with blur application in `handleAsyncUpdate()`, deferred to the next message pump cycle. Only blur requires async deferral (window server needs the window fully presented for CoreGraphics blur APIs). Fix: extract NSWindow chrome mutations (setTitleVisibility, setTitlebarAppearsTransparent, FullSizeContentView mask, button hiding) into `BackgroundBlur::configureWindowChrome()`, called synchronously in `visibilityChanged()` before `triggerAsyncUpdate()`. Blur remains async.
- **SSOT violation**: Title/style mutations were duplicated in `applyBackgroundBlur()` and `applyNSVisualEffect()`. Extracted to single `configureWindowChrome()` called once synchronously.
- **Dead code**: `hideWindowButtons()` had zero callers after refactor — removed from header, macOS implementation, and Windows stub.

### Technical Debt / Follow-up
- Pre-existing early returns in `BackgroundBlur::enable()`, `applyBackgroundBlur()`, `applyNSVisualEffect()`, `enableWindowTransparency()` violate BLESSED E (Explicit). Not introduced by this sprint. New code (`configureWindowChrome`) is compliant.

---

## Sprint 1: Fix Active Prompt Misalignment After Pane Close

**Date:** 2026-04-06
**Duration:** ~4h

### Agents Participated
- COUNSELOR: Requirements analysis, root cause investigation, plan, orchestration
- Pathfinder: Codebase exploration (resize path tracing, reflow architecture, rendering pipeline, buffer struct analysis)
- Engineer: Code changes (height-only fast path, diagnostics, cleanup)
- Auditor: Validation (alternate buffer overflow, BLESSED compliance)
- Oracle: (not invoked)

### Files Modified (2 total)
- `Source/terminal/logic/GridReflow.cpp:119-170` -- Height-only resize fast path with pinToBottom branching: when only visibleRows changes (cols unchanged, both buffers large enough), skip full reflow and adjust buffer metadata in-place. pinToBottom: head unchanged, scrollbackUsed decreases, cursor moves down (viewport expands from top). Not pinToBottom: head advances by heightDelta, scrollbackUsed unchanged, cursor stays (viewport expands from bottom). Sync State::scrollbackUsed after both paths. Existing full reflow path unchanged (now `else if`). Cursor fallback in full reflow: use `totalOutputRows - 1` and preserve `cursorCol`.
- `Source/terminal/data/State.h:373-378` -- Updated `setScrollbackUsed` doc comment to reflect dual-thread usage (READER THREAD or MESSAGE THREAD).

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **Active prompt 1-row misalignment after pane close**: When a split pane was closed and the surviving terminal reclaimed height, the active prompt rendered 1 row above its correct position. Root cause: the full reflow algorithm (designed for column-width changes that re-wrap content) produced a 1-row viewport shift when only height changed, due to the cursor fallback inflating `contentExtent` by 1 and floor-division rounding losses (107/2=52, 52*2=104!=107) compounding through double reflow (shrink+expand). Fix: bypass the full reflow for height-only changes — adjust viewport metadata directly (scrollbackUsed, allocatedVisibleRows, cursor) without touching the ring buffer content. Content layout is identical when cols are unchanged.
- **Prompt at wrong position after external window resize**: Height-only fast path initially assumed pinToBottom unconditionally (cursor + heightDelta). When the cursor was NOT at the bottom (e.g., fresh launch, Hammerspoon resize), the prompt slid to the middle instead of staying at its content position. Fix: branch on pinToBottom — when true, viewport expands from top (head stays, scrollback revealed); when false, viewport expands from bottom (head advances, cursor stays).
- **State::scrollbackUsed stale after reflow**: After full reflow, `Buffer::scrollbackUsed` was updated but `State::scrollbackUsed` (atomic) was not synced. Shell's OSC 133;A computed `promptRow = staleScrollbackUsed + cursorRow` producing wrong absolute row. Fix: sync State from Buffer after reflow.
- **SSOT violation**: Initial approach tracked OSC 133 markers (promptRow, blockTop, blockBottom) through the reflow, making Grid a second writer for values owned by the shell via parser. Removed all marker tracking — only the shell (via OSC 133) writes prompt/block markers. Grid writes only what it owns: cursor position, scrollbackUsed.

### Technical Debt / Follow-up
- None. PLAN-fix-active-prompt.md at project root can be removed (plan is superseded by the actual implementation).
