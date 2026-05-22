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

Content stored as logical paragraphs (`ParagraphStorage` with raw `String`). Wrapping computed lazily inside `detail::ShapedText` using `wordWrapWidth`. On resize, only cached `ShapedText` is invalidated — the `String` is never touched.

- `TextEditorViewport::visibleAreaChanged()` detects width changes, triggers `checkLayout()` under reentrant guard (juce_TextEditor.cpp:173-186)
- `checkLayout()` (juce_TextEditor.cpp:964-996) iterates paragraphs, recomputes total height from `ShapedText::getLineMetricsForGlyphRange()`, resizes content holder
- Content model (`ParagraphStorage::text`) and display model (`ShapedText`) are fully separated

**Key pattern:** backing store is width-agnostic. Display computes wrapping FROM content at current viewport width. Never writes back to storage.

### Zed WrapMap (~/Documents/Poems/dev/zed/)

Content lives upstream in a rope (`language::Rope`). `WrapMap` (crates/editor/src/display_map/wrap_map.rs) holds a `SumTree<Transform>` — alternating isomorphic (pass-through) and wrap-sentinel nodes.

- `Transform::wrap(indent)` (wrap_map.rs:1192-1218) emits zero-input synthetic newline — wrap point consumes no input bytes, expands output by one display row
- `WrapMap::set_wrap_width()` (wrap_map.rs:178-267) calls `rewrap()` — rebuilds entire `SumTree<Transform>`. Large documents spawn background async rebuild with fast approximate `interpolate()` until complete
- Content never touched. WrapMap only stores the transform tree.

**Key pattern:** wrapping is a transform layer — zero-input sentinels inserted between content segments. Rebuild on resize, discard old transform.

### Terminal emulators

No terminal emulator implements lossless reflow that survives arbitrary resize cycles. All conflate the content model (buffer) with the display model (grid). Wrapping mutates the grid, which IS the buffer — resize = mutation = loss.

### DSP analogy

Terminal reflow maps to oversampling:

| DSP concept | Terminal equivalent |
|---|---|
| Signal samples | Content cells (non-whitespace) |
| Zeros | Whitespace cells |
| Sample rate boundary | Soft wrap at column width |
| Upsample | Upsize — remove soft-wrap boundaries (join), expand whitespace (zero-stuff/interpolate) |
| Downsample | Downsize — impose new wrap boundaries (decimate) |
| Boundary marker | `Row::wrapped` — distinguishes signal (hard newline) from imposed boundary (soft wrap) |

### Existing codebase: jam::Row at d516dd6

`jam::Row` existed and compiled at commit d516dd6. Proven data structure with:

- FAM struct packing cells + metadata in single allocation
- `FlexType = Cell` trait for Buffer stride computation
- `Block<Row>` non-owning view with ring addressing
- `TextEditor::setText(Block<Row>)` — working API
- `glyph::Arrangement::shape(Block<Row>)` — stops at `usedCols`
- `Row::wrapped` (soft wrap flag) and `Row::dead` (reflow tombstone)

Removed in 06d03fd under now-invalidated assumption. Restoration, not invention.

---

## Principles and Rationale

### Core insight

END's terminal rendering IS a TextEditor. The buffer is the document. The viewport is the view. Wrapping belongs in the reflow step, not the renderer.

END uses a **SYNC model**: reflow computes new layout, writes it back to buffer, then SIGWINCH fires. Buffer and PTY are always in sync. This is necessary because Video writes cursor-addressed cells — buffer column count must match PTY width.

### Why SYNC write-back

1. TextEditor stays dumb — one buffer row = one display row. No runtime wrap computation on every paint.
2. Buffer always matches PTY width — Video writes at width X, buffer IS width X. No dual model. **No shadow state (SSOT).**
3. No reference width tracking — each resize is a forward operation from current buffer state. **No persistent machinery state (Stateless).**
4. Shell redraws active viewport after SIGWINCH — those rows are overwritten anyway.

### Why jam::Row instead of Buffer\<Cell\>

`jam::Row` packs cells + per-row metadata into a single FAM struct. **One owner for cells and their row metadata (Bound).** `Buffer<Row>` is a single buffer — no parallel `Buffer<Cell>` + `Buffer<Row>` with synchronized ring state. **No duplicate truth (SSOT).**

### Why DST in TextEditor

DST is preserverance + transition animation. It captures snapshot, creates target, animates values. This is a display concern — the component that renders content owns the transition between display states. **One responsibility (Encapsulation).**

DST's trigger/events mechanism (`addTrigger`, `onStop`) is the existing API for wiring actions. Display wires Processor's reflow into these events at construction — same pattern as Video's events → Processor handlers. **Established pattern reuse (Encapsulation).**

### Rejected alternatives

- **View-only wrapping (no write-back):** Split brain between buffer width and PTY width. Shadow state. **SSOT violation.**
- **Buffer\<Cell\> + separate Buffer\<Row\>:** Two ring buffers with synchronized head positions. **SSOT violation — same truth in two places that can drift.**
- **Cell padding bits for row metadata:** Per-cell type carrying per-row semantics. **Encapsulation violation — Cell's one job is cell data.**
- **DST stays in Processor:** Display transition is a component concern, not a pipeline concern. Processor would hold display machinery. **Encapsulation violation.**
- **Manual lambda / new callback mechanism for DST wiring:** DST already has `addTrigger` and `onStop`. **Inventing a new pattern where one exists — Encapsulation anti-pattern.**

---

## Design

### 1. jam::Row — restore from d516dd6

Location: `~/Documents/Poems/dev/jam/jam_fonts/cell/jam_row.h`

```cpp
struct Row
{
    using FlexType = Cell;

    uint16_t usedCols { 0 };
    uint8_t flags { 0 };

    static constexpr uint8_t wrapped { 1 << 0 };
    static constexpr uint8_t dead    { 1 << 1 };

    Cell cells[];
};
```

| Field | Set by | Purpose |
|---|---|---|
| `FlexType = Cell` | Compile-time | Tells `Buffer<Row>` stride: `sizeof(Row) + alignedCols * sizeof(Cell)` |
| `usedCols` | Video::print() | Rightmost non-blank column + 1. Content boundary for shaper and reflow. |
| `wrapped` | Video::resolveWrapPending() | Soft wrap — cursor wrapped at right margin. Default 0 = terminates. |
| `dead` | reflow() | Tombstone — row consumed by unwrap/join, excluded from reflow output. |
| `cells[]` | Video (all cell write paths) | C99 FAM. Cells inline with metadata. Sized at allocation by Buffer. |

### 2. Buffer\<Row\> — FlexType stride restore

Location: `~/Documents/Poems/dev/jam/jam_core/buffer/jam_buffer.h`

Current `Buffer<ElementType>` computes `rowStrideBytes = alignedCols * sizeof(ElementType)`. Wrong for FAM types — `sizeof(Row)` is just the header.

Restore FlexType-aware stride from d516dd6:

```cpp
template<typename T, typename = void>
struct has_flex_type : std::false_type {};

template<typename T>
struct has_flex_type<T, std::void_t<typename T::FlexType>> : std::true_type {};

template<typename T>
inline constexpr bool hasFlexType = has_flex_type<T>::value;
```

In `setSize()`:

```cpp
if constexpr (hasFlexType<ElementType>)
    newRowStride = sizeof(ElementType)
                 + static_cast<size_t>(newAlignedCols) * sizeof(typename ElementType::FlexType);
else
    newRowStride = static_cast<size_t>(newAlignedCols) * sizeof(ElementType);
```

All Buffer APIs unchanged — `getWritePointer`, `getReadPointer`, `clear`, `copyFrom`, `advanceHead`, `reverseHead` operate on `rowStrideBytes`. Only the stride computation changes.

`getWritePointer(channel, row)` returns `Row*`. Callers access cells via `row->cells[col]`, metadata via `row->flags`, `row->usedCols`.

`clear(channel, row)` zeroes `rowStrideBytes` — zeros Row metadata AND all cells in FAM. Default Row: `flags = 0` (terminates), `usedCols = 0`.

`Block<Row>` follows same pattern — non-owning view, ring addressing, `getRowPointer(row)` returns `Row*`.

### 3. Video changes — READER thread

Location: `~/Documents/Poems/dev/end/Source/terminal/Video.cpp`, `Video.h`

**a. Cell write path (`print()`, Video.cpp:467-585):**

```cpp
// Current:
buffer.getWritePointer(scr, writeRow)[writeCol] = glyph;

// New:
auto* row = buffer.getWritePointer(scr, writeRow);
row->cells[writeCol] = glyph;
row->usedCols = static_cast<uint16_t>(
    juce::jmax(static_cast<int>(row->usedCols), writeCol.value + charWidth));
```

Same change applies to: wide char SPACER_TAIL write, grapheme cluster extension (`lastWriteRow`/`lastWriteCol` path).

**b. Soft wrap flag (`resolveWrapPending()`, Video.cpp:407-433):**

One line added. When `autoWrap` fires, before advancing cursor:

```cpp
auto* completedRow = buffer.getWritePointer(activeScreen, cursorRow.value);
completedRow->flags |= jam::Row::wrapped;
```

This is the ONLY place `wrapped` is set. Default 0 = terminates. `executeLineFeed()` does not need to clear it — new/cleared rows default to 0.

Codebase reference — `wrapPending` cleared by (Video.h:366, all documented in Pathfinder report): BS, HT, CR, `cursorGoToNextLine`, all cursor movement methods, `shiftLines`, `setCursorColumn`, `cursorForwardTab`, `cursorBackTab`, `resetCursor`, `resize`, `loadScreenState`. None of these need Row flag changes — they clear the deferred-wrap signal, not the row's structural metadata.

**c. All other cell access paths (mechanical `Cell*` → `Row*` → `->cells[]`):**

| File | Lines | Paths |
|---|---|---|
| VideoEdit.cpp | 65-198 | ED modes 0/1/2/3 |
| VideoEdit.cpp | 226-291 | EL modes 0/1/2 |
| VideoEdit.cpp | shiftLines | IL, DL, ICH, DCH |
| Video.cpp | 296-340 | `scrollUpAndFill()` full-screen + partial-region |
| VideoCSI.cpp | 511-555 | Scroll down partial-region row copy |
| VideoESC.cpp | 238-257 | DECALN fill |

`buffer.clear(scr, row)` zeroes entire Row including metadata — no change for erase paths using clear. Erase paths filling with `Cell::erase(eraseStyleId())` write to `row->cells[col]` and reset `row->usedCols` / `row->flags`.

### 4. Reflow algorithm — static function

Location: `~/Documents/Poems/dev/end/Source/terminal/Processor.cpp`

```cpp
static void reflow (jam::Buffer<jam::Row>& buffer,
                    terminal::State& state,
                    terminal::Video& video,
                    int scrollbackLines,
                    cell oldCols,
                    cell newCols,
                    cell newRows) noexcept;
```

Replaces `resizeHeight()` (Processor.cpp:32-102). Height adjustment absorbed into unified reflow.

**Downsize (newCols < oldCols) — wrap:**

For each scrollback row (above active viewport):

1. If `row->usedCols <= newCols` and not `wrapped`: fits, no change.
2. If content exceeds `newCols` or `wrapped`: reconstruct logical line by joining consecutive `wrapped` rows.
3. Split logical line at `newCols` boundaries. Each split creates a new row marked `wrapped`. Last segment inherits original row's wrap state.
4. Wide char at wrap boundary: 2-cell char straddling `newCols - 1` and `newCols` → leave column `newCols - 1` empty (SPACER_HEAD), push wide char to next row. Cell's `wide(2 bits)` identifies these.
5. Write reflowed rows. Mark consumed source rows `dead`.

**Upsize (newCols > oldCols) — unwrap + pad:**

For each scrollback row:

1. If `wrapped`: join with next row(s) until non-wrapped found → logical line reconstructed.
2. If logical line fits in `newCols`: write as single row, clear `wrapped`. Mark consumed source rows `dead`.
3. If logical line exceeds `newCols`: re-wrap at `newCols` (same as downsize).
4. Non-wrapped rows (terminates) with `usedCols == oldCols` (full-width): **pad** — scan for interior whitespace runs, distribute `newCols - oldCols` extra cells proportionally.
5. Non-wrapped rows shorter than `oldCols`: no change (short line).

**Padding algorithm:**

Interior whitespace = consecutive cells with space codepoint (0x20) between the first and last non-space cell. Leading whitespace (indentation) excluded — only runs with content cells on BOTH sides are expanded.

Distribution: each whitespace run receives `extra * (runLength / totalWhitespaceLength)` additional space cells. Remainder distributed left-to-right.

On downsize, padding is NOT reversed — row is wrapped at new column boundary like any content. Each resize is a forward operation from current buffer state.

**Active viewport rows:** Not reflowed. PTY owns them. Shell redraws after SIGWINCH.

**Alternate screen (channel 1):** Not reflowed. Apps redraw on SIGWINCH.

**Working area:** DST's existing snapshot buffer (`previous`) — captures current content before `setSize()` clears the live buffer.

### 5. DST — moves from Processor to TextEditor

**Current state (ARCHITECTURE.md Processor.h:357):** Processor owns `jam::DiscreteStateTransition transitioner`. Processor registers `addTrigger<cell, cell>(id::resizeStart, ...)` and `transitioner.onStop`. Processor calls `transitioner.set()` from `valueTreePropertyChanged`.

**New state:** TextEditor owns DST. TextEditor exposes DST's events map (`addTrigger`, `onStop`). Display wires handlers at construction time.

**DST facts from jam_core (commit d516dd6):**
- `captureSnapshot()` — copies live content to `previous` buffer. Preserverance.
- `set()` — sets target, captures snapshot, fires trigger, starts animation timer.
- `addTrigger<Args...>(id, handler)` — registers handler fired on transition start.
- `onStop` — callback fired when animation completes.
- Timer: 16ms tick, 200ms transition. Animates `crossfadePosition` 0→1.
- `pendingChanges` — coalesces rapid inputs. Latest target wins.
- `prepare()` — cold-start. Skips snapshot on first transition.

**What DST does in TextEditor:**
- Preserves content snapshot before resize (existing `captureSnapshot()`)
- Animates dimension values from old to target
- Coalesces rapid resize events during drag (pending changes)
- Visual crossfade between old layout and new layout
- Fires trigger handler on transition start (reflow + buffer mutation)
- Fires `onStop` on completion (state sync + SIGWINCH)

### 6. Resize sequence — SYNC model

```
1. Display::resized()
   → screen.setBounds(contentBounds)

2. TextEditor::resized() → viewport->setBounds() → visibleAreaChanged()
   → writes visibleWidth, visibleHeight, scrollbarVisible to TE state node

3. Display reads TE state node (visibleWidth, visibleHeight, scrollbarVisible)
   → computes Cell::Rectangle via jam::Bounds
   → calls screen.DST.set(newCols, newRows)

4. TextEditor DST:
   a. captureSnapshot() — preserves current buffer content in DST's `previous`
   b. Fires trigger (wired by Display at construction):
      - reflow(buffer, state, video, scrollbackLines, oldCols, newCols, newRows)
      - buffer.setSize(2, ringSize, newCols) — buffer resized, heads reset
      - Write reflowed content from DST snapshot back to buffer
      - video.setDimensions(newCols, newRows)
      - video.resize(newCols, newRows)
   c. Starts animation timer (16ms tick, 200ms transition)
   d. Timer ticks animate crossfade / dimension interpolation

5. TextEditor DST onStop (wired by Display at construction):
   → state.setDimensions(newCols, newRows)
   → tty->platformResize(cols, rows, pixelWidth, pixelHeight) — SIGWINCH

6. Shell receives SIGWINCH
   → redraws active viewport at new width
   → new output written to buffer via Video at new column count
```

`state.setDimensions()` called in `onStop` — after buffer matches new dimensions. Video reads cols/visibleRows from State for cursor addressing.

### 7. Display wiring — construction time

Display owns `Screen` (which IS `jam::TextEditor`) and holds `Processor&`.

At construction, Display wires TextEditor's DST events:

- **Trigger:** lambda capturing `processor` reference. Calls `reflow()` static function, `buffer.setSize()`, writes reflowed content back, `video.setDimensions()`, `video.resize()`.
- **onStop:** lambda capturing `processor` reference. Calls `state.setDimensions()`, `tty->platformResize()`.

Same pattern as: Video exposes events → Processor registers handlers (Processor.cpp:303-611). Owner wires events on the owned object. No reverse dependencies.

TextEditor does not know about Processor. Processor does not know about TextEditor. Display bridges them. **Unidirectional layer flow preserved (Encapsulation).**

### 8. Removed from Processor

| Removed | Location |
|---|---|
| `jam::DiscreteStateTransition transitioner` member | Processor.h:357 |
| `transitioner.addTrigger<cell, cell>(id::resizeStart, ...)` | Processor.cpp:147-191 |
| `transitioner.onStop` callback | Processor.cpp:194-197 |
| `transitioner.set(id::resizeStart, ...)` in valueTreePropertyChanged | Processor.cpp:779-783 |
| `transitioner.prepare()` cold-start | Processor.cpp:199 |
| `transitioner.liveRows` reset | Processor.cpp:781 |
| `static void resizeHeight(...)` | Processor.cpp:32-102 |
| `id::resizeEnd` event handler | Processor.cpp:597-611 |
| cols/visibleRows dimension-change detection in `valueTreePropertyChanged` | Processor.cpp:769-785 |

Processor's `valueTreePropertyChanged` retains: shell integration (outputBlockTop, promptRow), cell pixel changes (cellWidth, cellHeight), displayName computation (foregroundProcess, cwd). Only resize path removed.

### 9. Screen changes

Location: `~/Documents/Poems/dev/end/Source/terminal/component/Screen.cpp`, `Screen.h`

- `setText(Block<Row>)` replaces `setText(Block<Cell>)` — API existed at d516dd6
- `Block<Row>` constructed from `Buffer<Row>` in `valueTreePropertyChanged`:
  ```cpp
  const jam::Block<jam::Row> block (buffer, activeScreen, startRow, totalRows);
  setText (block);
  ```
- `ContentView::shapeVisibleContent()` calls `content.getSubBlock(firstVisibleRow, visRowCount)` and `arrangement.shape(subBlock, font, ...)` — Block<Row> overload stops at `usedCols`
- No wrapping logic in Screen. No wrapping logic in TextEditor. One buffer row = one display row.

### 10. Session ownership

Location: `~/Documents/Poems/dev/end/Source/terminal/Session.h`

`jam::Buffer<jam::Row>` replaces `jam::Buffer<jam::Cell>`. Single buffer. Processor references it. Session outlives Processor. **Clear ownership, deterministic lifecycle (Bound).**

### 11. Cross-thread contract preservation

Per ARCHITECTURE.md §Cross-Thread Data Contract:

| Thread | Access | Change |
|---|---|---|
| READER | `buffer.getWritePointer(scr, row)->cells[col]`, `row->flags`, `row->usedCols` | `Cell*` → `Row*` access, plus wrapped/usedCols writes. Same lock-free path. |
| MESSAGE (render) | `Block<Row>` constructor from Buffer — non-owning view, no copy | `Block<Cell>` → `Block<Row>`. Same pattern. |
| MESSAGE (reflow) | DST trigger handler — reflow + setSize + write-back | Runs in TextEditor's DST on MESSAGE thread. Same thread context as current Processor resize. |
| Scalar state | State atomics → ValueTree. numRows, scrollOffset, cursor position. | Unchanged. |

No new synchronization primitives. No new cross-thread paths.

### 12. Layer separation preservation

Per ARCHITECTURE.md §Layer Separation Rules:

```
Terminal / Logic (Video → Buffer<Row>)           writes cells + Row metadata on READER thread
Terminal / Data  (State / Buffer<Row>)            pure types, atomic storage, timer flush
Terminal / Component (Screen / TextEditor / DST)  reads Block<Row>, renders. Owns display transition.
Display                                           bridges TextEditor ↔ Processor. Wires DST events.
Reflow (static in Processor.cpp)                  pure function: buffer + dims → reflowed content
```

- Rendering NEVER calls Video or Buffer mutators — preserved. DST trigger handler (wired by Display) calls `reflow()` static function — this is the sole buffer mutation path on MESSAGE thread during resize.
- Video NEVER calls Component code — preserved.
- Terminal headers NEVER include Component headers — preserved. `reflow()` is a static function in Processor.cpp, not a TextEditor method.
- DST moves from Logic to Component layer — display transition is a component concern.

---

## Scaffold

### jam::Row

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

### Buffer FlexType stride

```cpp
// jam_core/buffer/jam_buffer.h — in setSize(), stride computation

template<typename T, typename = void>
struct has_flex_type : std::false_type {};

template<typename T>
struct has_flex_type<T, std::void_t<typename T::FlexType>> : std::true_type {};

template<typename T>
inline constexpr bool hasFlexType = has_flex_type<T>::value;

// In setSize():
if constexpr (hasFlexType<ElementType>)
{
    newAlignedCols = (newNumCols + alignment - 1) & ~(alignment - 1);
    newRowStride = sizeof(ElementType)
                 + static_cast<size_t>(newAlignedCols) * sizeof(typename ElementType::FlexType);
}
else
{
    newAlignedCols = (newNumCols + alignment - 1) & ~(alignment - 1);
    newRowStride = static_cast<size_t>(newAlignedCols) * sizeof(ElementType);
}
```

### Video::resolveWrapPending

```cpp
// Video.cpp:407-433, inside if (autoWrap):
auto* completedRow = buffer.getWritePointer(activeScreen, cursorRow.value);
completedRow->flags |= jam::Row::wrapped;
```

### Video::print — usedCols

```cpp
// Video.cpp, print(), after cell write:
auto* row = buffer.getWritePointer(scr, writeRow);
row->cells[writeCol] = glyph;
row->usedCols = static_cast<uint16_t>(
    juce::jmax(static_cast<int>(row->usedCols), writeCol.value + charWidth));
```

### Reflow signature

```cpp
// Processor.cpp
static void reflow (jam::Buffer<jam::Row>& buffer,
                    terminal::State& state,
                    terminal::Video& video,
                    int scrollbackLines,
                    cell oldCols,
                    cell newCols,
                    cell newRows) noexcept;
```

---

## BLESSED Compliance Checklist

- [x] **B (Bound):** Buffer<Row> owned by Session — one owner. DST owned by TextEditor — one owner. Row owns cells via FAM — no separate allocation. Reflow scoped to DST trigger. RAII throughout. No resource floats free.
- [x] **L (Lean):** Row: 3 fields + FAM. One bit discriminator. Reflow: one static function. Video: 2 new writes + mechanical access path. No god objects. No speculative abstractions. YAGNI: no "future-proof" metadata fields.
- [x] **E (Explicit):** `Row::wrapped` — named flag, set in ONE place (`resolveWrapPending`). `Row::dead` — named tombstone. `Row::usedCols` — named boundary. All reflow parameters in function signature. No magic values. No early returns.
- [x] **S (SSOT):** Buffer<Row> is THE content store. Row metadata is THE wrap discriminator. No parallel buffer. No shadow state between buffer and display. No duplicate dimension tracking.
- [x] **S (Stateless):** Reflow: pure function from current buffer state to new state. TextEditor: stateless renderer, content set per frame via Block<Row>. DST: transient transition state only. No persistent machinery state.
- [x] **E (Encapsulation):** Buffer doesn't know reflow. TextEditor doesn't know Processor — DST events wired by Display. Video doesn't know display. Reflow doesn't know SIGWINCH. Display bridges without either side knowing the other. Unidirectional layer flow. Established event wiring pattern reused.
- [x] **D (Deterministic):** Same buffer + same dimensions = same reflow output. Wrap/unwrap reversible via `Row::wrapped`. Padding: arithmetic from cell content, proportional distribution. BLESSE followed → D is the result.

---

## Open Questions

None. All design questions resolved in the ORACLE session.

---

## Handoff Notes

### For COUNSELOR

**Restoration (from d516dd6, not invention):**
- `jam::Row` struct — `jam_fonts/cell/jam_row.h`
- Buffer FlexType stride computation — `jam_core/buffer/jam_buffer.h`
- `TextEditor::setText(Block<Row>)` — `jam_gui/text_editor/jam_text_editor.h`
- `glyph::Arrangement::shape(Block<Row>)` — stops at `usedCols`
- `Block<Row>` ring-addressed view — `jam_core/buffer/jam_block.h`
- SPACER_HEAD skip in glyph shaper — reflow boundary padding for wide chars

**New code:**
- `reflow()` static function in Processor.cpp — replaces `resizeHeight()`. Wrap/unwrap/pad algorithm. Working area: DST's existing snapshot buffer (`previous`).
- Display DST wiring — construction-time `addTrigger` + `onStop` on TextEditor's DST.
- Display::resized() calls `screen.DST.set(cols, rows)` after reading TE state for scrollbar visibility.

**Modifications:**
- Video: `Cell*` → `Row*` access paths, `wrapped` flag in `resolveWrapPending`, `usedCols` in `print`. All READER thread. Mechanical.
- Session: `Buffer<jam::Row>` replaces `Buffer<jam::Cell>`.
- Screen: `Block<Row>` replaces `Block<Cell>` in `valueTreePropertyChanged`.
- Processor: DST removed. Resize path in `valueTreePropertyChanged` removed. `resizeHeight()` removed. `id::resizeEnd` handler removed. Shell integration / cell pixel / displayName listeners unchanged.
- TextEditor: owns DST. Exposes DST events. `visibleAreaChanged()` unchanged — still writes visibleWidth/visibleHeight/scrollbarVisible.
- Display: sole author of cell dimensions. Reads TE state (visibleWidth, visibleHeight, scrollbarVisible), computes Cell::Rectangle, triggers DST. Wires trigger (reflow + setSize + write-back + video sync) and onStop (state.setDimensions + SIGWINCH).

**Do NOT touch:**
- Ownership: Session owns Buffer. Processor references it.
- Init sequence: Processor constructor. Display's `createAndAttachState`.
- State listener patterns: ValueTree::Listener on State.
- Cross-thread contract: READER writes atomics, MESSAGE reads ValueTree.

**Build note:** NEVER run cmake/ninja/make/xcodebuild. Building is ARCHITECT only.
