# PLAN: Terminal Rendering — Grid→Buffer, GST→DST

**RFC:** RFC-texteditor-rewrite.md
**Date:** 2026-05-22
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)

## Overview

Replace `Grid` with `jam::Buffer<jam::Cell>` and `GridSizeTransition` with `jam::DiscreteStateTransition<jam::Cell>`. Same ownership, same listener pattern, same init sequence. Only the objects and render types change. jam modules already have DST, Buffer with ring ops, Row deleted, TextEditor accepts `Block<Cell>`.

## Preconditions (already done in jam)

- `jam::Buffer<ElementType>` — ring-addressed storage with `advanceHead`, `reverseHead`, `getHead`, `setHead`, `getBlock`, `clear`, `copyFrom`, `setSize`
- `jam::DiscreteStateTransition<ElementType>` — generic SST in jam_core: `addTrigger`, `set`, `prepare`, `flush`, `process`, `onStop`, `previous`, `previousHead`, `liveRows`
- `jam::TextEditor` — accepts `setText(Block<Cell>)`, has `getWrapWidth()`, `TextEditorViewport` with reentrant guard
- `jam::Row` deleted, `Block<Row>` shape overloads deleted from Arrangement
- `jam_core.h` includes DST after Block

## Validation Gate

Each step validated by @Auditor against:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (coding standards)
- Locked PLAN decisions (no deviation, no scope drift)

## Ownership Map (unchanged from 396c195)

```
Session         owns    Buffer<Cell> buffer         (was: Grid grid)
Processor       holds   Buffer<Cell>& buffer        (was: Grid& grid)
Processor       owns    DST<Cell> transitioner      (was: GridSizeTransition gridResize)
Video           holds   Buffer<Cell>& buffer        (was: Grid& grid)
Screen          holds   Buffer<Cell>& buffer        (was: Grid& grid)
Display         passes  buffer& to Screen           (was: grid& to Screen)
State           owns    all dimensions, cursor, scroll, numRows (unchanged)
```

## Grid→Buffer Method Mapping

| Grid method | Buffer equivalent | Notes |
|---|---|---|
| `setSize(rows, cols, scrollback)` | `buffer.setSize(2, ringSize, cols)` | Caller computes `ringSize = nextPow2((scrollback + rows) * 2)` |
| `getWritePointer(screen, row)` | `buffer.getWritePointer(screen, row.value)` | Returns `Cell*` (was `Row*`) |
| `getBlock(screen, scrollOff, vpRows)` | `buffer.getBlock(screen, startRow, vpRows.value)` | Caller computes `startRow` from numRows - scrollOffset |
| `scrollUp` (full-screen) | `buffer.advanceHead(screen, count)` + `buffer.clear(screen, row)` | Caller increments numRows via State atomic |
| `scrollUp` (partial region) | `buffer.copyFrom` loop + `buffer.clear` | No head change — same as Grid's partial path |
| `scrollDown` (full-screen) | `buffer.reverseHead(screen, count)` + `buffer.clear` | |
| `scrollDown` (partial region) | `buffer.copyFrom` loop + `buffer.clear` | |
| `clear(screen)` | `buffer.clear(screen)` | Caller resets numRows via State atomic |
| `clear(screen, row)` | `buffer.clear(screen, row.value)` | |
| `clear(screen, row, startCol, n)` | `buffer.clear(screen, row.value, startCol.value, n.value)` | |
| `isAllocated()` | `buffer.getNumRows() > 0` | |
| `getNumRows(screen)` | `state.loadValue(screenId, id::numRows)` | State is SSOT — no Grid shadow |
| `getViewportRows()` | `state.loadValue(id::SESSION, id::visibleRows)` | State is SSOT |
| `getRingMask()` | not needed — Buffer manages ring internally | |
| `getHeadPosition(screen)` | `buffer.getHead(screen)` | |
| `getBuffer()` | not needed — DST holds `Buffer&` directly | |
| `resizeHeight(rows, cursorRow)` | inline in DST trigger | Logic preserved, operates on buffer + State |
| `reflow` / `reflowFrom` | DELETED | Render-time wrapping replaces storage reflow |
| `getRow(screen, absIdx)` | DELETED | Only consumed by reflow |
| `physicalRow` | not needed — Buffer maps internally | |

## numRows Tracking (Grid dissolution)

Grid owned `numRows[screen]` — incremented by `scrollUp`, read by Processor to sync State.

With Grid dissolved: State is SSOT for `numRows`. Video (READER thread) writes via `state.storeValue(screenId, id::numRows, value)` after scroll operations. Same cross-thread atomic pattern used for cursor, modes, etc.

`scrollbackLines` stays as config constant — read from `lua::Engine::nexus.terminal.scrollbackLines` at Session construction, passed to Processor, stored locally.

---

## Steps

### Step 1: Session — Grid→Buffer

**Scope:** `Session.h`, `Session.cpp`

**Action:**
- `Grid grid` → `jam::Buffer<jam::Cell> buffer`
- `getGrid()` → `getBuffer()` returning `jam::Buffer<jam::Cell>&`
- Pass `buffer` to Processor constructor (same reference pattern)
- Add `scrollbackLines` member — read from lua config at construction, passed to Processor
- Remove `#include "Grid.h"` — add `<jam_core/jam_core.h>` if not already included

**Validation:** Session compiles with Buffer<Cell>. Grid no longer referenced. Does not compile alone — Processor/Video/Screen still reference Grid.

---

### Step 2: Processor — Grid&→Buffer&, GST→DST

**Scope:** `Processor.h`, `Processor.cpp`

**Action:**

**Header:**
- `Grid& grid` → `jam::Buffer<jam::Cell>& buffer`
- `GridSizeTransition gridResize` → `jam::DiscreteStateTransition<jam::Cell> transitioner`
- Constructor: `Processor(jam::Buffer<jam::Cell>& buffer, TextBuffer& textBuffer, cell cols, cell rows, int scrollbackLines, const juce::String& uuid)`
- `getGrid()` → `getBuffer()` returning `jam::Buffer<jam::Cell>&`
- Store `scrollbackLines` as member (int)
- Remove `#include "GridSizeTransition.h"`, remove `#include "Grid.h"`

**Constructor body — same init sequence as 396c195:**
1. `state.setDimensions(cols, rows)` (unchanged)
2. `registerEvents()` (unchanged)
3. `parser = make_unique<Parser>(video)` (unchanged)
4. `state.get().addListener(this)` (unchanged)
5. Cold start: compute `ringSize`, call `buffer.setSize(2, ringSize, cols.value)`, then `video.setDimensions(cols, rows)`, `video.resize(cols, rows)` — same as GST.allocate() did
6. `transitioner.prepare()` (was `gridResize.prepare(scrollbackLines)`)

**DST trigger registration (in constructor or registerEvents):**
- Register `id::resize` trigger via `transitioner.addTrigger<cell, cell>(id::resize, [this](cell cols, cell rows) { ... })` — lambda contains resizeHeight logic (from Grid.resizeHeight) + buffer.setSize for column change + video.setDimensions + video.loadScreenState + video.resize + State sync + fire `id::resizeTick` event
- `transitioner.onStop = [this]() { events.get(id::resizeEnd); }` — SIGWINCH delivery (was advanceCrossfade firing resizeEnd)
- Set `transitioner.liveRows` before each `transitioner.set()` call

**valueTreePropertyChanged — same dispatch as 396c195:**
- `id::cols` / `id::visibleRows` → set liveRows, call `transitioner.set(id::resize, cols, rows)` (was `gridResize.set(cols, rows)`)
- `id::cellWidth` / `id::cellHeight` → handled via DST coalescing or direct (same as GST.setCellSize pattern)
- Cold start path (first dimension set): `transitioner.set` fires immediately without animation (DST's `isReady=false` path — same as GST)

**registerEvents — adapt Grid references:**
- `id::scrollUp` handler: was `grid.getNumRows(screen)` → `state.loadValue(screenId, id::numRows)` (State is SSOT)
- `id::clearBuffer` handler: was `grid.clear(screen)` → `buffer.clear(screen)`
- `id::resizeTick` handler: State writes unchanged (numRows, scrollOffset, cursor)
- `id::resizeEnd` handler: `tty->platformResize()` (unchanged)

**Validation:** Processor compiles with Buffer&, DST, no Grid/GST references. Does not compile alone — Video/Screen still reference Grid.

---

### Step 3: Video — Grid&→Buffer&, Row*→Cell*

**Scope:** `Video.h`, `Video.cpp`, `VideoEdit.cpp`, `VideoESC.cpp`, `VideoCSI.cpp`

**Action:**

**Header:**
- `Grid& grid` → `jam::Buffer<jam::Cell>& buffer`
- Constructor: `Video(jam::Buffer<jam::Cell>& buffer, jam::Function::Map<juce::Identifier, void>& events)`
- Add `State& state` reference (for numRows atomic writes during scroll) — or pass from Processor if Video doesn't already hold State
- Add `int scrollbackLines` member (for numRows cap during scrollUp)

**Scroll operations — inline Grid logic using Buffer primitives:**

`scrollUp(screen, top, bottom, count)`:
- Full-screen (top == 0 and bottom == viewportRows - 1): `buffer.advanceHead(screen, count)` + clear new rows + increment numRows via `state.storeValue` capped at scrollbackLines
- Partial region: `buffer.copyFrom` loop within region + clear vacated rows
- Same logic as Grid.cpp scrollUp, using Buffer API

`scrollDown(screen, top, bottom, count)`:
- Full-screen: `buffer.reverseHead(screen, count)` + clear new rows
- Partial region: `buffer.copyFrom` loop + clear

**Cell* migration — mechanical at every getWritePointer callsite:**

Pattern: `grid.getWritePointer(scr, row)` returned `Row*`, accessed `row->cells[col]`.
Now: `buffer.getWritePointer(screen, row.value)` returns `Cell*`, accessed `cells[col]`.

All `row->usedCols` writes deleted. All `row->flags |= wrapped` writes deleted.

`grid.clear(...)` → `buffer.clear(...)` at all call sites.

**Validation:** Video compiles with Buffer&, Cell*. No Grid, Row, usedCols, flags references. Scroll operations use Buffer primitives.

---

### Step 4: Screen + Display — Grid&→Buffer&

**Scope:** `Screen.h`, `Screen.cpp`, `Display.h`, `Display.cpp`

**Action:**

**Screen.h:**
- `Grid& grid` → `jam::Buffer<jam::Cell>& buffer`
- Constructor: `Screen(State& state, jam::Buffer<jam::Cell>& buffer)`

**Screen.cpp:**
- Constructor: same as 396c195 but with `buffer` reference
- `valueTreePropertyChanged`: `grid.getBlock(activeScreen, scrollOffset, viewportRows)` → `buffer.getBlock(activeScreen, startRow, viewportRows.value)` where `startRow = numRows - scrollOffset` (both read from State)
- `grid.isAllocated()` → `buffer.getNumRows() > 0`
- `setText(block)` — Block<Cell> flows from buffer.getBlock (jam TextEditor already accepts Block<Cell>)
- Remove `setViewportMode(ViewportMode::proportional)` — jam TextEditor no longer has this
- Remove `setScrollRange(numRows)` — jam TextEditor no longer has this
- Remove `attach(scrollValue)` — jam TextEditor no longer has this

**Display.h:**
- Constructor unchanged: `Display(terminal::Processor& processor)`

**Display.cpp:**
- Screen construction: `screen(Display::createAndAttachState(...), processorToUse.getBuffer())` (was `getGrid()`)

**Validation:** **[COMPILATION GATE]** Full project compiles. No Grid, GridSizeTransition, Row references anywhere in END. Buffer<Cell> flows from Session → Processor → Video (write) and Session → Processor → Display → Screen (read via getBlock → Block<Cell> → setText).

---

### Step 5: Delete Grid + GridSizeTransition files

**Scope:** `Source/terminal/Grid.h`, `Source/terminal/Grid.cpp`, `Source/terminal/GridSizeTransition.h`, `Source/terminal/GridSizeTransition.cpp`

**Action:**
- Delete all four files
- Remove from CMakeLists.txt / Projucer / build system
- Verify no remaining `#include` references

**Validation:** **[COMPILATION GATE]** Full project compiles and links. No orphan includes. Clean build.

---

### Step 6: ARCHITECTURE.md update

**Scope:** `ARCHITECTURE.md`

**Action:**
- Update Module Map: remove Grid.h/cpp, GridSizeTransition.h/cpp
- Update Key Data Types: `jam::Buffer<jam::Cell>` replaces Grid, `jam::DiscreteStateTransition<jam::Cell>` replaces GridSizeTransition
- Update Data Flow: Buffer<Cell> storage, Block<Cell> render path
- Update ownership: Session owns Buffer, Processor owns DST
- Remove all Row, reflow, GridSizeTransition references

**Validation:** ARCHITECTURE.md reflects codebase. No stale references.

---

## BLESSED Alignment

- **B (Bound):** Buffer owned by Session (unchanged). DST owned by Processor (same as GST). References passed down. Lifetimes explicit.
- **L (Lean):** ~500+ lines deleted (reflow, Row, Grid class, GST class). DST is generic in jam_core. Net deletion.
- **E (Explicit):** `buffer.getBlock(screen, startRow, numRows)` — all params visible. `buffer.advanceHead(screen, count)` — no hidden ring math.
- **S (SSOT):** numRows in State (was shadow-tracked on Grid). Dimensions in State. Cursor in State.
- **S (Stateless):** Buffer is dumb storage. DST is generic state machine. Video writes cells, doesn't track history.
- **E (Encapsulation):** Buffer: storage. Video: VT writes + scroll. Screen: render. Processor: lifecycle + DST. Display: dimensions.
- **D (Deterministic):** Same Block<Cell> + same wrapColumns → same glyphs. No reflow edge cases.

## Risks / Open Questions

1. **Video scroll — State access:** Video currently has no State& reference. It fires events that Processor handles. With Grid dissolved, Video needs `state.storeValue` for numRows during scrollUp. Options: (a) Video gets State& reference, (b) Video fires event with numRows delta and Processor writes. Current GST pattern has Grid track numRows internally — which Video-to-State pattern does ARCHITECT prefer?

2. **resizeHeight logic in DST trigger:** Grid.resizeHeight is ~50 lines of scrollback pull/push logic. This goes into the DST trigger lambda. The lambda may exceed 30-line Lean limit — may need extraction to a static helper. Decision deferred to execution.

3. **getBlock startRow computation:** Grid.getBlock computed `startRow = head - numRows + scrollOffset` internally. With Buffer, caller computes `startRow`. Screen reads numRows and scrollOffset from State, computes startRow, passes to `buffer.getBlock`. Verify arithmetic matches Grid's absolute-index formula.
