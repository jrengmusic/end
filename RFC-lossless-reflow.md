# RFC — Lossless Terminal Reflow

Date: 2026-05-22
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END's terminal rendering destroys buffer content on resize. `Buffer<Cell>::setSize()` resets all ring head positions unconditionally — column changes scramble content. The previous tmux-model reflow (`jam::Row` with Grid) achieved lossless content but had three unresolved issues:

1. **No expansion on upsize** — content stayed at original write width, trailing empty space
2. **Downsize forced height resize** — created unnecessary scrollback history even when viewport rows were empty
3. **Fake viewport** — scroll mechanics reimplemented instead of delegating to juce::Viewport

The move to `Buffer<Cell>` (commit 06d03fd) was motivated by the assumption that juce::TextEditor text wrapping required a flat 2D cell grid. This assumption was wrong — TextEditor stays dumb (one buffer row = one display row), and wrapping belongs in the reflow step, not the renderer.

**Objective:** Restore `jam::Row` with a new reflow algorithm that is deterministic, lossless, and handles both upsize (expansion via unwrap + padding) and downsize (wrapping). Buffer content survives any resize cycle. Real juce::Viewport for normal screen scrolling.

---

## Research Summary

### juce::TextEditor (~/Documents/Poems/juce/)

Content stored as logical paragraphs (`ParagraphStorage` with raw `String`). Wrapping computed lazily inside `detail::ShapedText` using `wordWrapWidth`. On resize, only cached `ShapedText` is invalidated — the `String` is never touched. Lossless by construction.

- `TextEditorViewport::visibleAreaChanged()` detects width changes, triggers `checkLayout()` under reentrant guard
- `checkLayout()` iterates paragraphs, recomputes total height from `ShapedText::getLineMetricsForGlyphRange()`, resizes content holder
- Content model and display model are fully separated — wrapping is a display artifact, never written back to storage

**Key pattern:** backing store doesn't know about wrapping. Display computes wrapping FROM content. Never writes back.

### Zed WrapMap (~/Documents/Poems/dev/zed/)

Content lives upstream in a rope (`language::Rope`). `WrapMap` holds a `SumTree<Transform>` — alternating isomorphic (pass-through) and wrap-sentinel (zero-input synthetic newline + indent) nodes.

- `Transform::wrap(indent)` emits `TransformSummary { input: zero, output: {lines: Point(1, indent)} }` — wrap point consumes no input but expands output by one display row
- `WrapMap::set_wrap_width()` calls `rewrap()` — rebuilds entire `SumTree<Transform>` from `TabSnapshot` chunks
- Large documents: background async rebuild with fast approximate `interpolate()` until complete
- Content never touched. WrapMap only stores the transform tree.

**Key pattern:** wrapping is a transform layer — zero-input sentinels inserted between content segments. Rebuild on resize, discard old transform.

### Terminal emulators (all lossy)

No terminal emulator implements lossless reflow that survives arbitrary resize cycles. All conflate the content model (buffer) with the display model (grid). Wrapping mutates the grid, which IS the buffer — resize = mutation = loss.

### DSP analogy (ARCHITECT's insight)

Terminal reflow maps to oversampling:

- **Content cells** = signal samples
- **Whitespace cells** = zeros in the signal
- **Soft wrap boundaries** = sample rate boundaries imposed by column width
- **Upsize** = upsample — remove soft-wrap boundaries (join), expand whitespace (zero-stuff/interpolate)
- **Downsize** = downsample — impose new wrap boundaries (decimate)
- **Reference width** = original sample rate
- **Row::wrapped** = the boundary marker that distinguishes signal (hard newline) from imposed boundary (soft wrap)

---

## Principles and Rationale

### Core insight

END's terminal rendering IS a TextEditor. The buffer is the document. The viewport is the view. Wrapping is a display-time concern that should never mutate the backing store.

However, unlike a pure text editor model where display never writes back, END uses a **SYNC model**: reflow computes the new layout, writes it back to the buffer, then SIGWINCH fires. Buffer and PTY are always in sync. This is necessary because Video (the VT command processor) writes cursor-addressed cells into the buffer — the buffer's column count must match what the PTY thinks the grid width is.

### Why SYNC write-back instead of view-only

1. TextEditor stays dumb — one buffer row = one display row. No runtime wrap computation on every paint.
2. Buffer always matches PTY width — Video writes at width X, buffer IS width X. No dual model.
3. No reference width tracking — each resize is a forward operation from current buffer state.
4. Shell redraws active viewport after SIGWINCH — those rows are overwritten anyway.

### Why jam::Row instead of Buffer\<Cell\>

`jam::Row` packs cells + per-row metadata (wrapped flag, usedCols, dead tombstone) into a single FAM struct. `Buffer<Row>` is a single buffer — no parallel `Buffer<Cell>` + `Buffer<Row>` with synchronized ring state. The data structure already existed and was proven lossless at commit d516dd6. It was removed in 06d03fd under a now-invalidated assumption.

### Rejected alternatives

- **View-only wrapping (no write-back):** Requires TextEditor to understand wrapping. Requires reference width per row. Creates split brain between buffer and PTY width. More complex display layer.
- **Buffer\<Cell\> + separate Buffer\<Row\>:** Two buffers with synchronized ring state. Fragile. One bit of metadata doesn't justify a parallel buffer.
- **Cell padding bits for row metadata:** Couples row metadata to cell internals. Per-cell type shouldn't carry per-row semantics.

---

## Design

### 1. jam::Row — restored from d516dd6 with current mental model

Location: `~/Documents/Poems/dev/jam/jam_fonts/cell/jam_row.h`

```cpp
struct Row
{
    using FlexType = Cell;

    uint16_t usedCols { 0 };
    uint8_t flags { 0 };

    static constexpr uint8_t wrapped { 1 << 0 };  // soft wrap — continues on next row
    static constexpr uint8_t dead    { 1 << 1 };  // reflow tombstone

    Cell cells[];  // FAM, sized at allocation time by Buffer
};
```

- `FlexType = Cell` tells `Buffer<Row>` to compute row stride as `sizeof(Row) + alignedCols * sizeof(Cell)`
- `usedCols` — rightmost non-blank column + 1. Updated by Video on each cell write.
- `wrapped` — set by Video in `resolveWrapPending()` when cursor wraps at right margin. Default 0 = terminates (hard newline or empty row).
- `dead` — reflow tombstone. Rows consumed by unwrap/join are marked dead and excluded from reflow output.
- `cells[]` — C99 flexible array member. Cells inline with metadata. No separate allocation.

### 2. Buffer\<Row\> — FlexType stride restoration

Location: `~/Documents/Poems/dev/jam/jam_core/buffer/jam_buffer.h`

Current `Buffer<ElementType>` computes row stride as `alignedCols * sizeof(ElementType)`. This assumes fixed-size elements. `jam::Row` has a FAM — `sizeof(Row)` is just the header.

**Modification:** Restore FlexType-aware stride computation. When `ElementType` has a `FlexType` member type, compute stride as:

```
sizeof(ElementType) + alignedCols * sizeof(typename ElementType::FlexType)
```

This was the pattern at d516dd6 before Row deletion. All existing Buffer APIs (`getWritePointer`, `getReadPointer`, `clear`, `copyFrom`, `advanceHead`, `reverseHead`) work unchanged — they operate on row-sized memory regions using `rowStrideBytes`. Only the stride computation changes.

`getWritePointer(channel, row)` returns `ElementType*` (`Row*`). Callers access cells via `row->cells[col]` and metadata via `row->flags`, `row->usedCols`.

`clear(channel, row)` zeroes `rowStrideBytes` — zeros both Row metadata and all cells in the FAM. Default-constructed Row has `flags = 0` (terminates), `usedCols = 0`.

Block\<Row\> follows the same pattern — non-owning view with ring addressing, `getRowPointer(row)` returns `Row*`.

### 3. Video changes — READER thread

Location: `~/Documents/Poems/dev/end/Source/terminal/Video.cpp`, `Video.h`

Three changes. All on the READER thread.

**a. Cell write path (`print()`, Video.cpp:467-585):**

Current: `buffer.getWritePointer(scr, writeRow)[writeCol] = glyph`

New: `auto* row = buffer.getWritePointer(scr, writeRow); row->cells[writeCol] = glyph`

After each cell write, update usedCols: `row->usedCols = juce::jmax(row->usedCols, static_cast<uint16_t>(writeCol + charWidth))`

Same change applies to wide char SPACER_TAIL write and grapheme cluster extension.

**b. Soft wrap flag (`resolveWrapPending()`, Video.cpp:407-433):**

One line added before advancing cursor. When autoWrap fires:

```cpp
auto* completedRow = buffer.getWritePointer(activeScreen, cursorRow.value);
completedRow->flags |= Row::wrapped;
```

This is the ONLY place `wrapped` is set. Default state (0) = terminates. `executeLineFeed()` does not need to clear it — new/cleared rows default to 0.

**c. All other cell access paths:**

Every `buffer.getWritePointer(scr, row)` currently returns `Cell*` and indexes with `[col]`. Change to return `Row*` and index with `->cells[col]`:

- Erase paths: ED (VideoEdit.cpp:65-198), EL (VideoEdit.cpp:226-291), ECH, ICH, DCH
- Scroll fills: `scrollUpAndFill()` (Video.cpp:296-340)
- Scroll down partial-region row copy loop (VideoCSI.cpp:511-555)
- DECALN fill (VideoESC.cpp:238-257)
- Line insert/delete shift (VideoEdit.cpp shiftLines)

`buffer.clear(scr, row)` zeroes the entire Row including metadata — no change needed for erase paths that use clear. Erase paths that fill with `Cell::erase(eraseStyleId())` need to write to `row->cells[col]` and reset `row->usedCols` and `row->flags` as appropriate.

### 4. Reflow algorithm — static function in Processor.cpp

Location: `~/Documents/Poems/dev/end/Source/terminal/Processor.cpp` (alongside existing `resizeHeight`)

```cpp
static void reflow (jam::Buffer<jam::Row>& buffer,
                    terminal::State& state,
                    terminal::Video& video,
                    int scrollbackLines,
                    cell oldCols,
                    cell newCols,
                    cell newRows) noexcept;
```

**Downsize (newCols < oldCols) — wrap:**

For each scrollback row (above active viewport):
1. Read row. If `row->usedCols <= newCols` and not `wrapped`: fits, no change.
2. If `row->usedCols > newCols` or `wrapped` (logical line continues): reconstruct logical line by joining consecutive `wrapped` rows.
3. Split logical line at `newCols` boundaries. Each split point creates a new row marked `wrapped` (continues). Last segment inherits the original row's wrap state.
4. Wide character at wrap boundary: if a 2-cell character would straddle `newCols - 1` and `newCols`, leave column `newCols - 1` empty (SPACER_HEAD), push the wide char to the next row. Cell's `wide(2 bits)` field identifies these.
5. Write reflowed rows back.

**Upsize (newCols > oldCols) — unwrap + pad:**

For each scrollback row:
1. If `wrapped`: join with next row(s) until a non-wrapped row is found. This reconstructs the logical line.
2. If the reconstructed logical line fits in `newCols`: write as single row, clear `wrapped` flag. Mark consumed source rows as `dead`.
3. If the logical line exceeds `newCols`: re-wrap at `newCols` (same as downsize logic).
4. For non-wrapped rows (terminates) with `usedCols == oldCols` (full-width): scan for interior whitespace runs (consecutive space codepoint cells between content cells). Distribute `newCols - oldCols` extra cells proportionally into whitespace runs. This expands layout padding (OMP right-pinned prompts, `ls` column spacing).
5. For non-wrapped rows shorter than `oldCols`: no change (short line, trailing empty space).

**Padding algorithm detail:**

Interior whitespace = consecutive cells with space codepoint (0x20) between the first and last non-space cell in the row. Leading whitespace (indentation) is excluded — only runs with content cells on both sides are expanded.

Distribution is proportional: each whitespace run receives `extra * (runLength / totalWhitespaceLength)` additional space cells, with remainder distributed left-to-right.

On downsize, padding is not "reversed" — the row is simply wrapped at the new column boundary like any other content. Each resize is a forward operation from current buffer state.

**Active viewport rows:** Not reflowed. The PTY owns these rows. SIGWINCH fires after reflow, shell redraws them at the new width.

**Alternate screen (channel 1):** Not reflowed. Alternate screen apps (vim, htop) redraw entirely on SIGWINCH. All alternate screen rows are effectively terminates — no reflow logic applied.

### 5. SYNC model — resize sequence

The complete resize sequence, triggered by Display::resized():

```
1. Display::resized()
   → compute new cell grid from viewport pixel bounds via Cell::Rectangle
   → state.setDimensions(cols, rows)

2. Processor::valueTreePropertyChanged
   → detects cols/visibleRows change
   → transitioner.set(id::resizeStart, newCols, newRows)

3. DST quiet timer (50ms debounce)
   → on settle, fires trigger lambda

4. DST trigger (Processor's registered lambda):
   a. Capture reflowed content: reflow(buffer, state, video, scrollbackLines, oldCols, newCols, newRows)
   b. buffer.setSize(2, ringSize, newCols) — buffer resized, heads reset
   c. Write reflowed content back to buffer
   d. video.setDimensions(newCols, newRows)
   e. video.resize(newCols, newRows)
   f. State sync (numRows, scrollOffset, cursor position)

5. DST onStop:
   → fires id::resizeEnd
   → tty->platformResize(cols, rows, pixelWidth, pixelHeight) — SIGWINCH

6. Shell receives SIGWINCH
   → redraws active viewport at new width
   → new output written to buffer via Video at new column count
```

Buffer and PTY are always in sync after step 4b-c. SIGWINCH in step 5 tells the shell to redraw into a buffer that already has the correct width.

### 6. DST role

`jam::DiscreteStateTransition` remains in Processor (owned, same as today). Its role:

**Retained:**
- SIGWINCH debounce via quiet timer (50ms)
- Snapshot capture before destructive setSize (existing `captureSnapshot()` pattern)
- Pending change coalescing (latest resize wins during drag)
- SIGWINCH delivery on settle (`onStop` callback)

**Changed:**
- Snapshot is now a reflowed Block<Row>, not a raw buffer copy
- The trigger lambda performs reflow + setSize + write-back instead of raw destructive resize
- Visual crossfade between old layout and new layout (display transition) moves toward TextEditor internals as a future enhancement — not in scope for this RFC

**Removed:**
- `resizeHeight()` static function — absorbed into `reflow()`. Height adjustment (scrollback push/pull) is part of the unified reflow, not a separate step.

### 7. Screen changes — MESSAGE thread

Location: `~/Documents/Poems/dev/end/Source/terminal/component/Screen.cpp`, `Screen.h`

Screen IS jam::TextEditor (direct inheritance). Changes:

- `setText(Block<Row>)` replaces `setText(Block<Cell>)` — TextEditor already had this API at d516dd6
- `Block<Row>` constructed from `Buffer<Row>` in `valueTreePropertyChanged` — same ring addressing, same non-owning view
- `ContentView::shapeVisibleContent()` accesses cells via `row->cells[]` instead of flat `Cell*` — this was the working pattern at d516dd6
- `glyph::Arrangement::shape(Block<Row>, font, ...)` — the Block<Row> overload already existed, stops at `usedCols`

No wrapping logic in Screen. No wrapping logic in TextEditor. One buffer row = one display row. TextEditor renders what it's given.

### 8. Session ownership

Location: `~/Documents/Poems/dev/end/Source/terminal/Session.h`

Session owns `jam::Buffer<jam::Row>` (replaces `jam::Buffer<jam::Cell>`). Single buffer. Processor references it. Same ownership pattern as today — Session outlives Processor.

### 9. Cross-thread contract preservation

Per ARCHITECTURE.md §Cross-Thread Data Contract:

- **READER thread** writes cells via `buffer.getWritePointer(scr, row)->cells[col]` and sets `row->flags`, `row->usedCols`. Same thread, same lock-free path. No new synchronization.
- **MESSAGE thread** reads via `Block<Row>` constructor from Buffer — non-owning view, no copy. Same pattern as today with Block<Cell>.
- **Reflow** runs on MESSAGE thread inside DST trigger (same as current resize logic). No READER thread access during reflow — DST's quiet timer ensures the resize gesture has settled.
- **Scalar state** (numRows, scrollOffset, cursor position) flows through State atomics → ValueTree. Unchanged.

### 10. Layer separation preservation

Per ARCHITECTURE.md §Layer Separation Rules:

```
Terminal / Logic (Video → Buffer<Row>)        writes cells + Row metadata on reader thread
Terminal / Data  (State / Buffer<Row>)         pure types, atomic storage, timer flush
Terminal / Component (Screen / TextEditor)     reads Block<Row>, renders. No buffer mutation.
Reflow (static function in Processor.cpp)      reads buffer, computes new layout, writes back
DST (jam_core)                                 timing, debounce, snapshot lifecycle
```

- Rendering (Screen/TextEditor) NEVER calls Video or Buffer mutators — preserved
- Video NEVER calls UI/Component code — preserved
- Reflow runs in Processor's DST trigger on MESSAGE thread — same thread context as current resize
- No new layer violations introduced

---

## Scaffold

### jam::Row (restore from d516dd6)

```cpp
// jam_fonts/cell/jam_row.h
#pragma once

namespace jam
{ /*____________________________________________________________________________*/

struct Row
{
    using FlexType = Cell;

    uint16_t usedCols { 0 };
    uint8_t flags { 0 };

    static constexpr uint8_t wrapped { 1 << 0 };
    static constexpr uint8_t dead    { 1 << 1 };

    Cell cells[];
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace jam
```

### Buffer FlexType stride (restore in setSize)

```cpp
// In Buffer::setSize(), stride computation:
if constexpr (hasFlexType<ElementType>)
{
    // FAM element: header + aligned flex elements per row
    newAlignedCols = (newNumCols + alignment - 1) & ~(alignment - 1);
    newRowStride = sizeof(ElementType) + static_cast<size_t>(newAlignedCols) * sizeof(typename ElementType::FlexType);
}
else
{
    // Fixed-size element: aligned elements per row
    newAlignedCols = (newNumCols + alignment - 1) & ~(alignment - 1);
    newRowStride = static_cast<size_t>(newAlignedCols) * sizeof(ElementType);
}
```

FlexType detection via SFINAE or `requires` clause:

```cpp
template<typename T, typename = void>
struct has_flex_type : std::false_type {};

template<typename T>
struct has_flex_type<T, std::void_t<typename T::FlexType>> : std::true_type {};

template<typename T>
inline constexpr bool hasFlexType = has_flex_type<T>::value;
```

### Video::resolveWrapPending — wrapped flag

```cpp
// Video.cpp, resolveWrapPending(), before cursor advance:
if (autoWrap)
{
    auto* completedRow = buffer.getWritePointer(activeScreen, cursorRow.value);
    completedRow->flags |= jam::Row::wrapped;

    // ... existing scroll/cursor logic
}
```

### Video::print — usedCols tracking

```cpp
// Video.cpp, print(), after cell write:
auto* row = buffer.getWritePointer(scr, writeRow);
row->cells[writeCol] = glyph;
row->usedCols = static_cast<uint16_t>(juce::jmax(static_cast<int>(row->usedCols), writeCol.value + charWidth));
```

### Reflow function signature

```cpp
// Processor.cpp, static function
static void reflow (jam::Buffer<jam::Row>& buffer,
                    terminal::State& state,
                    terminal::Video& video,
                    int scrollbackLines,
                    cell oldCols,
                    cell newCols,
                    cell newRows) noexcept
{
    // 1. For each scrollback row (not active viewport):
    //    - Reconstruct logical lines by joining wrapped rows
    //    - Re-wrap/pad logical lines at newCols
    //    - Mark consumed rows as dead
    //
    // 2. Height adjustment (absorbs former resizeHeight):
    //    - Shrink: eat empty rows below cursor, push to scrollback
    //    - Grow: pull from scrollback, fill with blank rows
    //
    // 3. Write reflowed content to a working area,
    //    ready for write-back after setSize
}
```

---

## BLESSED Compliance Checklist

- [x] **B (Bound):** Buffer<Row> owned by Session. Row owns cells via FAM. DST owned by Processor. Reflow is scoped to DST trigger. No resource floats free. RAII throughout.
- [x] **L (Lean):** Row is 3 fields + FAM. One bit discriminator. Reflow is one static function. Video changes are 2 code paths. No god objects. No speculative abstractions.
- [x] **E (Explicit):** `Row::wrapped` — explicit soft wrap flag. `Row::dead` — explicit tombstone. `Row::usedCols` — explicit content boundary. No magic values. All parameters visible in reflow signature. No early returns.
- [x] **S (SSOT):** Buffer<Row> is THE content store. Row metadata is THE wrap discriminator. No shadow state. No duplicate between buffer and display. No parallel Buffer<Row> for metadata.
- [x] **S (Stateless):** Reflow is a pure function from current buffer state to new buffer state. TextEditor is stateless renderer — content set per frame via Block<Row>. No persistent display state. No history tracking.
- [x] **E (Encapsulation):** Buffer doesn't know about reflow. TextEditor doesn't know about wrapping. Video doesn't know about display. Reflow doesn't know about SIGWINCH. Each layer has one job. Unidirectional flow preserved. No layer violations.
- [x] **D (Deterministic):** Same buffer + same target dimensions = same reflow output. Always. Wrap/unwrap is reversible via Row::wrapped. Padding is arithmetic from cell content. Non-determinism impossible when BLESSE is followed.

---

## Open Questions

1. **SPACER_HEAD in glyph shaper:** Commit d516dd6 added "SPACER_HEAD skip in glyph shaper (reflow boundary padding)." This was a rendering concern for wide characters at wrap boundaries. Confirm this pattern carries forward or needs revision with the new reflow algorithm.

2. **Reflow working area:** The reflow function needs a scratch space to build the reflowed content before `setSize()` clears the buffer. Options: (a) use DST's existing snapshot buffer (`previous`), (b) allocate a temporary, (c) build in-place with careful ring manipulation. ARCHITECT decides.

3. **usedCols maintenance on erase:** When ED/EL erases cells, should `usedCols` be recalculated (scan for rightmost non-blank) or left stale? Stale is cheaper but may affect reflow accuracy for padding detection. Recalculation is O(cols) per erase.

4. **Alternate screen reflow:** Currently specified as "not reflowed." Confirm alternate screen rows should have `Row::wrapped = 0` always, or whether alternate screen content should have Row metadata tracked for potential future use.

5. **Padding threshold:** Should interior whitespace expansion apply to ALL whitespace runs, or only runs above a minimum length (e.g., 2+ spaces)? Single spaces between words are content; multi-space runs are layout. ARCHITECT decides the threshold.

---

## Handoff Notes

### For COUNSELOR

- **jam::Row exists at d516dd6** — not new code, restoration. The struct, FlexType trait, Block<Row> overloads, and glyph::Arrangement shape(Block<Row>) all existed and compiled. Start from that commit's jam state.
- **Buffer<Row> FlexType stride existed at d516dd6** — the Buffer template had FAM-aware stride computation. Removed in 06d03fd. Restore it.
- **TextEditor setText(Block<Row>) existed at d516dd6** — the API is proven. Screen used it. ContentView shaped from it.
- **Video changes are mechanical** — `Cell*` → `Row*`, `[col]` → `->cells[col]`, plus two new writes (wrapped flag, usedCols). No architectural change to Video.
- **Reflow is the new code** — the static function in Processor.cpp. This is the implementation work. The wrap/unwrap logic is straightforward (join wrapped rows, split at new width). Padding (interior whitespace expansion) is the nuanced part.
- **DST trigger lambda replacement** — current trigger in Processor constructor (lines 147-191) is replaced with reflow + setSize + write-back. Same lambda shape, different body.
- **resizeHeight() absorbed** — the existing static function (Processor.cpp:32-102) is absorbed into the unified reflow. Height adjustment happens as part of reflow, not as a separate step.
- **Cross-thread safety is unchanged** — reflow runs on MESSAGE thread in DST trigger, same as current resize. READER thread writes cells and Row metadata. No new synchronization needed.
- **Do NOT touch:** ownership (Session owns Buffer), init sequence (Processor constructor), state listener patterns (ValueTree::Listener on State). These were cleaned up in prior sprints and are correct.
- **Build note:** NEVER run cmake/ninja/make/xcodebuild. Building is ARCHITECT only.
