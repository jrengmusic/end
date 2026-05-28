# PLAN: SPSC Pipeline — State-Machine-Driven Terminal Rendering

**RFC:** none -- objective from ARCHITECT prompt + session discussion
**Date:** 2026-05-28
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides per LANGUAGE.md)

## Overview

Replace the flush-based Video-to-TextLineArray pipeline with a state-machine-driven architecture. Video writes to its own Buffer<Row> and sets per-row flush-dirty Parameters in State. CellFifo (AbstractFifo + HeapBlock<Cell>) on Processor handles lock-free cross-thread history commit. TextLineArray is a generic document buffer with range operations — no terminal semantics. Two independent dirty tracking systems (flush-dirty and shape-dirty) as Parameter<int> in State eliminate all manual arithmetic and shadow state.

## Key Architecture Decisions (locked)

### Mental Model

```
Video owns Buffer<Row> (cols x visibleRows)
|
|  VT state machine write surface
|  cursor addressing, character writes, scroll shifting
|  sets per-row flush-dirty Parameter in State after writes
|
+-- pushLine event: departing/completed row cells -> CellFifo (reader thread)

CellFifo (Processor, SPSC ring)
|
|  AbstractFifo + HeapBlock<Cell>, pre-allocated
|  reader thread pushes raw cell rows + flags
|  message thread drains, joins continued rows into logical TextLines
|
+-- drain -> TextLineArray.add() on screenDirty tick

TextLineArray (generic document buffer)
|
|  deque<TextLine>, scrollbackLines cap
|  add(TextLine&&) -- append, cap enforced
|  remove(juce::Range<int>) -- remove by index range
|  No terminal semantics. Processor manages what to add/remove.
|
+-- TextEditor reads for rendering

State (terminal parameter store)
|
|  Per-row flush-dirty Parameters (viewport-sized, N = visibleRows)
|  Per-row shape-dirty Parameters (viewport-sized, N = visibleRows)
|  Recreated on resize
|  screenDirty = aggregate repaint trigger
|
+-- flush-dirty: "Video wrote this row" -- consumed by commit path
+-- shape-dirty: "this visible TLA row needs reshaping" -- consumed by TextEditor
```

### Coordinate Spaces (no reconciliation needed)

- **Video/Buffer<Row>**: viewport-relative (row, col). Video writes here. Flush-dirty tracks here.
- **TextLineArray**: chronological logical lines. Renderer reads here. Shape-dirty tracks visible range.
- **CellFifo**: one-way bridge. Rows leave viewport space, enter document space. No mapping.

### Active Prompt

Processor tracks `activePromptRowCount` -- how many tail entries in TextLineArray are the active prompt. On each screenDirty:

1. `remove(Range(total - activePromptRowCount, total))` -- remove old prompt
2. Drain CellFifo -> `add` committed logical lines
3. Read remaining flush-dirty rows from Video -> `add` as new prompt rows
4. Update `activePromptRowCount`

Active prompt is arbitrary size: 1 row (simple prompt), 2+ rows (OMP multi-line), N rows (long command with auto-wrap). Determined by flush-dirty rows remaining after CellFifo drain.

On non-scroll LF: pushLine fires for the row cursor leaves -> CellFifo captures it. Auto-wrapped rows above cursor are NOT pushed to CellFifo -- they remain as active prompt rows in TextLineArray. When user hits enter (LF), the cursor row goes to CellFifo, and the wrapped rows above it are already in TextLineArray -- they stay, promoted to history by the next `remove` + `add` cycle.

On resize: `remove` active prompt. History untouched. Shell redraws after SIGWINCH. Fresh prompt arrives.

### Two Independent Dirty Systems

1. **Flush-dirty** -- N Parameter<int> on viewport node, fixed to Buffer<Row> dimensions.
   - Video sets on cell write (reader thread atomic store)
   - Consumed by screenDirty handler (message thread CAS)
   - Answers: "which Buffer<Row> rows did Video touch since last flush?"
   - Recreated on resize

2. **Shape-dirty** -- N Parameter<int> on TextEditor node, viewport-sized.
   - Set when add() introduces new entries or active prompt changes
   - Consumed by TextEditor (shapes only dirty visible rows, CAS clears)
   - Answers: "which visible TextLineArray rows need reshaping?"
   - Sliding window: scroll exposes unshaped lines, marks them dirty

Both are Parameter<int> in State. Both CAS-gated. Both consumed independently. No coupling.

### Normal vs Alternate Screen

- **Normal**: TextLineArray = append-only history + mutable active prompt at tail. Processor manages active prompt via remove/add. CellFifo commits departed rows. Resize: history untouched, prompt removed, shell redraws.
- **Alternate**: TextLineArray = live buffer. Flush-dirty rows written directly via remove/add. Zero scrollback. Sized to viewport. Resize: cleared, rebuilt.

### What Gets Rendered

TextEditor reads TextLineArray exclusively. One source. TextLineArray contains both history and active prompt -- TextEditor doesn't know the difference. Renders everything via getWrappedLines projection.

## Language / Framework Constraints

C++ / JUCE -- MANIFESTO.md enforced as written. juce::AbstractFifo for SPSC index management. juce::HeapBlock for contiguous pre-allocated cell storage. Parameter<int> for dirty tracking (existing APVTS-analog infrastructure). No new synchronization primitives.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Fix wrapping boundary (jam)

**Scope:** `jam/jam_graphics/fonts/font/glyph/jam_glyph_arrangement_shape.cpp:355`
**Action:** Change `currentCol >= wrapColumns` to `currentCol > wrapColumns`. A line with exactly `wrapColumns` cells fits on one row -- the `>=` wraps it incorrectly to 2.
**Validation:** Confirmed by diagnostic logging in prior session.

### Step 2: TextLine -- HeapBlock<Cell> replaces vector<Cell> (jam)

**Scope:** `jam/jam_graphics/detail/jam_text_line.h`, all call sites in jam and end
**Action:** Replace `std::vector<Cell> cells` with `juce::HeapBlock<Cell> cells` + `int cellCount { 0 }`. Update:
- `getWrappedLines`: use `cellCount` instead of `cells.size()`
- Move constructor/assignment: HeapBlock is movable, transfer cellCount
- All call sites: `cells.size()` -> `cellCount`, `cells.data()` -> `cells.getData()`, `cells.resize(n)` -> `cells.allocate(n, true)` + `cellCount = n`, `cells.at(i)` -> `cells[i]`
**Validation:** All call sites updated. No vector<Cell> references remain.

### Step 3: TextLineArray -- generic document buffer (jam)

**Scope:** `jam/jam_graphics/detail/jam_text_line_array.h`
**Action:** Rewrite TextLineArray as a generic document buffer with no terminal semantics:
- `add(TextLine&&)` -- append to back, pop front if over scrollbackLines cap
- `remove(juce::Range<int>)` -- remove lines at index range
- `setCapacity(int)` -- set scrollback cap, trim excess
- `clear()` -- remove all
- `operator[](int)` -- indexed read access
- `totalRows()` -- line count
- Remove: `pushHistory`, `pushLine`, `flushLine`, `historyCount`, `visibleRows`, `liveRows`, `setCapacity(int, int)`, `setLiveRows`, `popBack`
**Validation:** Pure container. No terminal coupling. Alternate screen managed by Processor via remove/add -- no dedicated API.

### Step 4: CellFifo -- SPSC ring buffer (end)

**Scope:** new file `Source/terminal/CellFifo.h`
**Action:** Create CellFifo class:
- Owns `juce::HeapBlock<Cell> buffer` + `juce::AbstractFifo fifo`
- Per-entry layout in ring: `[int32_t cellCount | uint8_t flags | Cell cells[cellCount]]`
  - flags bit 0: isContinued (flexWrap), bit 1: isJustified
- `pushRow(const Cell* cells, int count, uint8_t flags) noexcept` -- writer (reader thread). `fifo.write()` header + cells. No heap allocation.
- `drainInto(jam::TextLineArray& target) noexcept` -- consumer (message thread). Reads all ready entries. Joins consecutive isContinued rows into one logical TextLine. Calls `target.add()` for each complete logical line.
- `reset(int capacityInCells) noexcept` -- reallocate ring. Called while processing suspended.
- `getNumReady() const noexcept` -- forwarded from AbstractFifo.
- Processor owns CellFifo as private member.
**Validation:** Lock-free SPSC. Zero heap allocation on push. drain joins continued rows (replaces continuedLine accumulator). AbstractFifo handles wrap-around.

### Step 5: Per-row flush-dirty Parameters in State (end)

**Scope:** `Source/terminal/State.h`, `Source/terminal/State.cpp`, `Source/terminal/Identifier.h`
**Action:**
- Add `id::rowDirty` identifier
- State creates N Parameter<int> for flush-dirty tracking under viewport node (N = initial visibleRows)
- `State::setRowDirty(int row) noexcept` -- atomic store, sets Parameter needsUpdate. Reader thread.
- `State::isRowDirty(int row) noexcept` -- CAS-gated read, returns true and clears. Message thread.
- `State::rebuildRowParameters(int newVisibleRows) noexcept` -- recreate on resize.
- Flush-dirty Parameters flush alongside existing parameters in State::refresh(). screenDirty fires if any row dirty.
**Validation:** Same Parameter<int> pattern as existing terminal parameters. CAS-gated. Recreated on resize.

### Step 6: Video sets flush-dirty via events (end)

**Scope:** `Source/terminal/Video.cpp`, `Source/terminal/ProcessorEvents.cpp`
**Action:**
- Register `id::rowDirty` event: `events.add<int>(id::rowDirty, [&state](int row) { state.setRowDirty(row); })`
- Video fires `events.get(id::rowDirty, row)` after writing cells to a row
- Fire once per row per operation, not per cell. Batch cell writes then fire.
**Validation:** Every Video cell write marks the row dirty. Events map decoupling -- Video has no State dependency.

### Step 7: pushLine fires into CellFifo (end)

**Scope:** `Source/terminal/Video.cpp` (executeLineFeed, scrollUpAndFill), `Source/terminal/ProcessorEvents.cpp`
**Action:**
- pushLine event signature: `(int screen, int row)` -- screen index + row index
- scrollUpAndFill: fire `pushLine(scr, 0)` before shift (add row param to existing call)
- executeLineFeed non-scroll path: fire `pushLine(scr, cRow)` for row cursor is leaving
- pushLine handler: reads row from Video's Buffer<Row> at given index, writes cells + flags into `cellFifo.pushRow()`. No callAsync. No TextLine construction. No heap allocation.
- Normal screen only (gate on screen == Map::Screen::normal)
- Remove: continuedLine member, hasActiveLine, all callAsync in pushLine handler
**Validation:** Every completed line (scroll or newline) captured in CellFifo. Lock-free push. Zero per-line allocation.

### Step 8: screenDirty handler -- drain + active prompt + alternate (end)

**Scope:** `Source/terminal/Processor.cpp` (valueTreePropertyChanged), `Source/terminal/Processor.h`
**Action:**
- Processor private member: `int activePromptRowCount { 0 }`
- screenDirty handler for NORMAL screen:
  1. Remove old active prompt: `textLineArrays[0].remove(Range(total - activePromptRowCount, total))`
  2. Drain CellFifo: `cellFifo.drainInto(textLineArrays[0])` -- commits queued rows as logical lines. Returns number of logical lines committed.
  3. Selection adjustment: for each logical line committed in step 2, shift selection anchor/cursor rows by -1 (same logic as current pushLine callAsync, but batched on message thread). Read/write TextEditor selection properties on State's ValueTree. If anchors go negative, clear selection.
  4. Read remaining flush-dirty rows from State (rows not captured by CellFifo = active prompt area). For each dirty row: read from Video's Buffer<Row>, build TextLine, `textLineArrays[0].add(std::move(line))`.
  5. Update `activePromptRowCount` = number of rows added in step 4
  6. No historyCount store
- screenDirty handler for ALTERNATE screen:
  - Check flush-dirty flags. For each dirty row: read from Video, build TextLine. Remove old row: `remove(Range(r, r+1))`. Insert... actually for alternate, simpler: `remove(Range(0, total))` then re-add all dirty rows? No -- only dirty rows changed. Use indexed overwrite.
  - Better: alternate screen TextLineArray sized to visibleRows at init. On screenDirty, for each dirty row: build TextLine from Video, replace at index via `remove(Range(r, r+1))` then insert... Range remove + add won't preserve index. Need indexed overwrite.
  - Decision: add `set(int index, TextLine&&)` to TextLineArray for alternate screen indexed overwrite. OR keep alternate screen as full remove/add cycle (cheap at viewport size).
- Display unchanged: calls `session.getScreen().setText(processor.getTextLineArray())` on screenDirty.
**Validation:** Normal: remove old prompt, drain, add new prompt. Alternate: dirty-only update. One activePromptRowCount counter on Processor -- managed per tick, not accumulated.

### Step 9: Screen caret positioning (end)

**Scope:** `Source/terminal/component/Screen.cpp`, `Source/terminal/Identifier.h`
**Action:**
- Caret positioning reads cursor (row, col) from State (packed cursor, existing).
- Active prompt rows are the last `activePromptRowCount` entries in TextLineArray. Video's cursorRow maps to the last active prompt row. Caret document row = sum of all projected rows before the active prompt region + cursor's visual row offset within the active prompt.
- Processor stores projected-row offset of active prompt start in State (one Parameter<int>) on each screenDirty tick. Screen reads it for caret placement.
- Remove `id::historyCount` dependency.
**Validation:** Caret correctly positioned for single-line and multi-line prompts. No historyCount.

### Step 10: Per-row shape-dirty Parameters in State (end + jam)

**Scope:** `Source/terminal/State.h`, `jam/jam_gui/text_editor/jam_text_editor.h`, `jam/jam_gui/text_editor/jam_text_editor.cpp`
**Action:**
- State creates N Parameter<int> for shape-dirty tracking on TextEditor node (N = viewport visible rows)
- Set dirty when: add() introduces new entries, active prompt changes, scroll exposes new lines
- TextEditor::setText reads shape-dirty flags. Shapes only dirty visible rows. Clears after shaping.
- `State::rebuildRowParameters` recreates both flush-dirty and shape-dirty sets on resize.
- Arrangement::shape gets startLine/endLine range -- shapes only visible + dirty rows.
**Validation:** O(viewport) shaping regardless of history depth. Idle: zero work. Typing: 1-2 rows. seq 10M: visible dirty only.

### Step 11: Processor resize path (end)

**Scope:** `Source/terminal/Processor.cpp` (resizeTextLineArray, resizeVideo)
**Action:**
- `resizeVideo`: `cellFifo.reset(newCapacity)` + `video.setWinsize()`. Ring cleared.
- Normal screen: `textLineArrays[0].remove(Range(total - activePromptRowCount, total))` to clear active prompt. `setCapacity(scrollbackLines)`. History untouched. `activePromptRowCount = 0`.
- Alternate screen: `textLineArrays[1].clear()` then rebuild at new size on first flush.
- `State::rebuildRowParameters(newVisibleRows)` -- recreate both dirty parameter sets.
- Shell redraws after SIGWINCH. Fresh prompt arrives via normal flush path.
**Validation:** Resize never destroys history. Ring reset clean. Parameters recreated. Content preserved.

### Step 12: Remove dead code + update docs (end + jam)

**Scope:** Both repos
**Action:**
- Remove `id::historyCount` from Identifier.h and all store/read sites
- Remove old flush path (full-visible-row copy loop in Processor vTPC)
- Remove continuedLine, hasActiveLine from Processor
- Remove old setCapacity(int, int), pushHistory, flushLine, liveRows, visibleRows
- Update doxygen on all modified classes
- Update ARCHITECTURE.md to reflect new pipeline
**Validation:** Clean compile. No dead references. Documentation matches implementation.

## BLESSED Alignment

- **B (Bound):** CellFifo owns HeapBlock + AbstractFifo. TextLineArray owns deque. Video owns Buffer<Row>. State owns Parameters. RAII lifecycle.
- **L (Lean):** CellFifo ~100 lines. TextLineArray ~60 lines. No god objects. YAGNI: single add, no batch.
- **E (Explicit):** Dirty flags are first-class Parameters. activePromptRowCount is one counter, set per tick, not accumulated. No hidden state.
- **S (SSOT):** TextLineArray sole rendered source. State sole dirty truth. No shadow state.
- **S (Stateless):** No accumulators, no boolean flags. Processor sets activePromptRowCount per tick from dirty flags -- derived, not tracked.
- **E (Encapsulation):** TextLineArray is a generic container -- no terminal semantics. CellFifo encapsulates SPSC. State encapsulates dirty tracking. Unidirectional flow.
- **D (Deterministic):** Same bytes -> same TextLineArray. CAS-deterministic dirty flags. FIFO-ordered ring.

## Risks / Open Questions

- **CellFifo capacity sizing:** Worst case seq 10M at 10ms tick: ~100K rows x ~80 cells x 8B + headers = ~64MB. Overflow policy: drop oldest or dynamic growth? ARCHITECT decision needed.
- **Partial-region scroll:** DECSTBM non-full-screen scroll regions don't fire pushLine (Video.cpp line 336). Investigation needed.
- **Alternate screen indexed overwrite:** Step 8 needs `set(int, TextLine&&)` or full remove/add cycle. If `set` added, TextLineArray stays generic (indexed write is a normal container op). ARCHITECT decision.
- **HeapBlock<Cell> in TextLine:** No `.at()` bounds checking. Acceptable per ARCHITECT's prior ruling on HeapBlock.
