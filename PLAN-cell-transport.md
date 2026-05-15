# PLAN: Cell Transport — 8-byte Packed Cell + Direct Grid Read

**RFC:** RFC-cell-transport.md
**Date:** 2026-05-15
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation — no overrides)

## Overview

Unified Cell and Grid transport rewrite. Cell shrinks from 16B to 8B via shared Stamp and Grapheme tables (jam::Context). Buffer<Grapheme> eliminated. Grid eliminates scrollOff FIFO — reader thread owns Grid under ScopedLock, Display reads Grid directly under ScopedTryLock. Zero cross-thread cell copy for visible rows. Scroll-off rows drain to Screen's scrollback under lock, cache-hot. Combined effect: halved transport bandwidth, eliminated cache-cold scrollOff writes, eliminated AbstractFifo atomics.

## Architecture

### Cell — packed u64

```
Bit 63                                                    Bit 0
[  padding (23)  |  styleId (16)  |  wide (2)  |  contentTag (2)  |  codepoint (21)  ]
```

| Field | Bits | Range | Purpose |
|-------|------|-------|---------|
| `codepoint` | 21 | U+0000-U+10FFFF | Unicode scalar, or Grapheme entry index when contentTag == 1 |
| `contentTag` | 2 | 0-3 | 0 = codepoint, 1 = grapheme index, 2-3 reserved |
| `wide` | 2 | 0-3 | 0 = narrow, 1 = wide, 2 = spacerTail, 3 = spacerHead |
| `styleId` | 16 | 0-65535 | Index into Terminal::Stamp table |
| padding | 23 | - | Reserved |

8 bytes. Trivially copyable. static_assert enforced.

### Terminal::Stamp — jam::Context<Stamp>

Shared deduped style table. App-lifetime. Video (reader) inserts, renderer (message) reads. Entries immutable once inserted. Ordering guaranteed by ScopedLock — by the time Display reads Grid, all Stamp entries for visible cells exist.

### Terminal::Grapheme — jam::Context<Grapheme>

Shared deduped grapheme cluster table. Same lifetime and thread-safety as Stamp. Full cluster (base + extras, up to 8 codepoints). Cell's codepoint field holds the grapheme entry index when contentTag == 1.

### Grid — Direct Read, No FIFO

Grid owns 1 Buffer<Cell> (2 channels, viewport-sized, power-of-two, bitmask). Reader thread writes under ScopedLock. Display reads under ScopedTryLock.

Ring per channel: `head[channel]`. scrollUp = `head = (head + 1) & ringMask`. Bitmask, not modulo.

No AbstractFifo. No scrollOff buffers. No State::scrolledRows. No Buffer<Grapheme>.

Scroll-off rows in the ring staging (power-of-two minus viewport) are read by Display under lock and drained to Screen's scrollback. Lossy during extreme throughput — same as old design.

### Display — ScopedTryLock

```
READER THREAD                      MESSAGE THREAD

ScopedLock(grid.lock)              ScopedTryLock(grid.lock)
  parser -> Video -> Grid              |
  scrollUp: head++, clear row         +- visible: Block over Grid (zero copy)
                                      +- staging: drain to Screen (memcpy, cache-hot)
                                      +- caret position from State
                                      +- repaint
```

No Screen::live buffer. Display constructs Block<Cell> directly over Grid's ring. Valid for lock duration.

## Contract

- Cell is 8 bytes — static_assert enforced
- Buffer<Grapheme> does not exist — graphemes in Context table
- No scrollOff buffer — Grid ring is the only cell storage
- No AbstractFifo — ScopedLock/ScopedTryLock synchronization
- No State::scrolledRows — staging drain count computed from ring heads under lock
- Reader never copies cells — scrollUp is head advance + one row clear
- Display never copies visible rows — reads via Block under lock
- Display copies scroll-off rows to Screen under lock — cache-hot, batched per vblank
- Stamp and Grapheme tables are append-only, immutable entries, no locking needed for reads
- Grid ring power-of-two — bitmask, not modulo
- ARCHITECT builds. Agents never run build commands.
- ARCHITECT runs git. Agents never run git commands.

## Steps

### Step 1: Terminal::Stamp — Context Table
**Scope:** `Source/terminal/data/Stamp.h` (new), `Source/terminal/data/Stamp.cpp` (new)
**Action:** Write Stamp as `jam::Context<Stamp>`. Entry struct: `juce::Colour fg`, `juce::Colour bg`, `uint8_t flags`. `getOrInsert(const Entry&)` returns `uint16_t`. `get(uint16_t)` returns `const Entry&`. Linear scan dedup. Add to includes.
**Validation:** Compiles. Context pattern matches jam::Context<T> usage. BLESSED S (SSOT — each unique style stored once). NAMES.md compliant.

### Step 2: Terminal::Grapheme — Context Table
**Scope:** `Source/terminal/data/Grapheme.h` (new), `Source/terminal/data/Grapheme.cpp` (new)
**Action:** Write Grapheme as `jam::Context<Grapheme>`. Entry struct: `std::array<char32_t, 8> codepoints`, `uint8_t count`. Full cluster including base. `getOrInsert(const Entry&)` returns `uint32_t` (21-bit, stored in Cell codepoint field). `get(uint32_t)` returns `const Entry&`. Linear scan dedup.
**Validation:** Compiles. Replaces jam::Grapheme sidecar. BLESSED L (lean — no per-cell parallel buffer).

### Step 3: Cell Rewrite — 8-byte Packed u64
**Scope:** `jam_tui/cell/jam_cell.h`
**Action:** Rewrite Cell as packed u64. Accessors: `codepoint()`, `contentTag()`, `wide()`, `styleId()`. Factory: `Cell::make(codepoint, contentTag, wide, styleId)`. `Cell::erase(uint16_t styleId)`. static_assert sizeof == 8. static_assert trivially copyable. Remove old fields (style, layout, width, reserved, fg, bg). Remove LAYOUT_WIDE_CONT, LAYOUT_GRAPHEME constants — replaced by `wide` and `contentTag` enums.
**Validation:** Compiles. 8 bytes. Bit layout matches RFC. No old Cell API remains. BLESSED E (explicit — named accessors for packed fields).

### Step 4: Video Rewrite — Stamp/Grapheme Integration
**Scope:** `Source/terminal/logic/Video.h`, `Video.cpp`, `VideoSGR.cpp`, all Video*.cpp
**Action:** Replace `jam::Cell pen` / `jam::Cell stamp` with internal pen members: `juce::Colour fg`, `juce::Colour bg`, `uint8_t flags`. SGR handlers write to pen members (same resolution logic — palette256At, parseExtendedColor). `print()`: calls `Stamp::getContext()->getOrInsert({pen.fg, pen.bg, pen.flags})` to get styleId, writes packed Cell to Grid. Grapheme path: builds full cluster, calls `Grapheme::getContext()->getOrInsert(cluster)` to get index, writes Cell with contentTag=1 and codepoint=index. `scrollUpAndFill`: builds erase Cell from current stamp styleId. `SavedCursor`: stores `fg`, `bg`, `flags` instead of `jam::Cell pen`. Remove `lastPrintRow`/`lastPrintCol` — grapheme sidecar gone, grapheme tracked via contentTag.
**Validation:** Compiles. Video produces 8-byte Cells. Stamp/Grapheme tables populated correctly. No jam::Cell pen anywhere. BLESSED S (Stateless — Video is dumb worker, tables are SSOT).

### Step 5: Grid Rewrite — Ring + Lock, No FIFO
**Scope:** `Source/terminal/logic/Grid.h`, `Grid.cpp`
**Action:** Grid members: 1 `Buffer<Cell>` (2 channels), `int head[2]`, `int ringMask`, `juce::CriticalSection lock`. No Buffer<Grapheme>. No scrollOff buffers. No AbstractFifo. `setSize(numRows, numCols)`: rounds numRows to next power of two, sets ringMask = powerOfTwo - 1, allocates `cells.setSize(2, powerOfTwo, numCols)`. Ring mapping: `physicalRow(screen, row) = (head[screen] + row) & ringMask`. `scrollUp` full-screen: head advance + clear recycled row. `scrollUp` partial: per-row copyFrom. `scrollDown` analogous. Remove all grapheme pointer methods. Remove scrollOff methods (prepareScrollOffRead, getScrollOffReadPointer, getGraphemeScrollOffReadPointer, advanceScrollOff). Add `getLock()` returning `CriticalSection&`. Add `getScrolledCount(int screen)` and `resetScrolledCount(int screen)` — simple int counter incremented by scrollUp, read/reset by Display under lock. Add `getViewportRows()` returning the logical viewport row count (pre-power-of-two rounding).
**Validation:** Compiles. Zero grapheme references. Zero AbstractFifo. Zero scrollOff buffer. CriticalSection exposed. Ring uses bitmask. BLESSED B (bound — lock is the coordination point). BLESSED S (SSOT — Grid is the only cell storage).

### Step 6: Processor — ScopedLock
**Scope:** `Source/terminal/logic/Processor.h`, `Processor.cpp`
**Action:** `process()`: wrap parser->process + video.flushState + video.flushResponses in `juce::ScopedLock(grid.getLock())`. Remove `State::scrolledRows` atomic and `addScrolledRows`/`consumeScrolledRows`. Remove `resizePending`/`pendingCols`/`pendingRows` atomics — resize now applied directly under lock (Grid is protected). `resized()` takes ScopedLock, calls grid.setSize + video.setDimensions directly (both reader and message thread go through the lock).
**Validation:** Compiles. Reader thread holds lock during process(). No scrolledRows atomic. No pending resize atomics. BLESSED B (threads bound — lock is explicit coordination).

### Step 7: Display Rewrite — ScopedTryLock + Direct Read
**Scope:** `Source/component/TerminalDisplay.h`, `TerminalDisplay.cpp`
**Action:** `onVBlank()`: `ScopedTryLock lock(grid.getLock())`. If not locked, skip frame. Under lock: (1) read scrolled count from Grid, drain staging rows to `screen.append()`, reset count. (2) Construct `Block<Cell>` over Grid's ring for visible rows — zero copy. Shape and paint from Block directly (or pass Block to Screen for shaping). (3) Read caret position from State. Remove all `grid.getScrollOffReadPointer` / `grid.getGraphemeScrollOffReadPointer` / `grid.prepareScrollOffRead` / `grid.advanceScrollOff` calls. Remove `screen.updateVisibleRow` calls — no Screen::live buffer. Remove grapheme row reads.
**Validation:** Compiles. ScopedTryLock — Display never blocks. No scrollOff drain. No live buffer copy. Visible rows read via Block (zero copy). BLESSED E (Encapsulation — Display mediates, doesn't own data).

### Step 8: Screen Cleanup — Remove Live Buffer
**Scope:** `Source/terminal/rendering/Screen.h`, `Screen.cpp`
**Action:** Remove `Buffer<Cell> live`. Remove `Buffer<Grapheme> liveGrapheme`. Remove `updateVisibleRow()`. Remove `setLiveDimensions()`. Keep `append()` for scrollback accumulation (remove grapheme parameter). Keep `setActiveScreen()`, `setVisibleRow()` for alternate screen.
**Validation:** Compiles. No live buffer. No grapheme buffer. Screen is scrollback storage only. BLESSED L (lean — removed dead storage).

### Step 9: TextEditor Cleanup — Remove Grapheme Buffers
**Scope:** `jam_gui/text_editor/jam_text_editor.h`, `.cpp`
**Action:** Remove `Buffer<Grapheme> graphemeNormal`. Remove `Buffer<Grapheme> graphemeAlternate`. Remove `setLiveGraphemeBuffer()`. Remove `liveGraphemeBuffer` pointer. Remove grapheme parameters from `appendRow()` and `setVisibleRow()`. All grapheme resolution happens at shape time via Grapheme::getContext()->get(index).
**Validation:** Compiles. No grapheme buffers. No grapheme parameters. BLESSED S (SSOT — graphemes in Context table only).

### Step 10: buildArrangements — Table Lookups
**Scope:** `jam_fonts/jam_font/glyph/jam_glyph_arrangement_shape.cpp`, `jam_glyph_arrangement.h`, `.cpp`
**Action:** `buildArrangements`: read `cell.styleId()` → `Stamp::getContext()->get(id)` for fg/bg/flags. Read `cell.contentTag()` — if 1, read `cell.codepoint()` as grapheme index → `Grapheme::getContext()->get(index)` for full cluster, shape all codepoints. Remove `Block<Grapheme>*` parameter from shape methods. Remove `graphemes` parameter from buildArrangements. Update `resolveStyle()` to read from Stamp entry flags instead of `pen.style`. Update `shapeCodepoint()` for new Cell API.
**Validation:** Compiles. Shapes from Stamp/Grapheme tables. No grapheme Block parameter. BLESSED S (SSOT — style and grapheme from tables).

### Step 11: Cleanup — Delete Dead Types
**Scope:** `jam_tui/cell/jam_grapheme.h`, `jam_tui/cell/jam_grapheme.cpp`, `jam_tui.h`
**Action:** Delete jam::Grapheme struct. Remove from jam_tui module includes. Verify zero remaining references. Delete PLAN-cell-transport.md. Delete RFC-cell-transport.md.
**Validation:** Compiles. No Grapheme references anywhere. No dead files. BLESSED L (lean — no dead code).

## BLESSED Alignment

- **B (Bound):** Cell is 8 bytes, fixed lifetime. CriticalSection bounds Grid access. Context tables are app-lifetime singletons. No ambiguous ownership.
- **L (Lean):** Cell halved. Grapheme buffer eliminated. ScrollOff buffer eliminated. Transport bandwidth halved.
- **E (Explicit):** Packed Cell has named accessors. contentTag discriminates codepoint vs grapheme index. ScopedLock/ScopedTryLock — explicit coordination.
- **S (SSOT):** Each unique style stored once in Stamp. Each unique grapheme stored once in Grapheme. Grid is the only cell storage — no scrollOff copy.
- **S (Stateless):** Video writes cells, doesn't track transport. Grid is dumb buffer. Tables are append-only.
- **E (Encapsulation):** Cell internals packed, accessed via API. Stamp/Grapheme tables expose getOrInsert/get only. Grid lock is internal.
- **D (Deterministic):** Same input → same packed Cell. Same style → same styleId. Stamp/Grapheme dedup is deterministic (linear scan, insertion order).

## Risks

1. **Stamp table growth:** Unbounded during session. Pathological SGR input (unique RGB per cell) could grow to 65536 entries. Linear scan dedup O(n) at high entry count. Mitigation: monitor in practice; hash map upgrade if needed (decision deferred — YAGNI).
2. **Scrollback lossy during extreme throughput:** Grid ring is viewport-sized (power-of-two). During `seq 1 M`, rows scroll past between vblanks faster than Display drains. Screen's scrollback accumulates what Display captures. Same behavior as old design. Acceptable per ARCHITECT.
3. **SavedCursor changes:** DECSC/DECRC stores pen state. Pen representation changes from jam::Cell to fg/bg/flags. Must be validated against VT spec edge cases (cursor save during alternate screen switch etc.).
