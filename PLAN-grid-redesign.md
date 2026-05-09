# PLAN: Grid Redesign -- AudioBuffer Analog

**RFC:** RFC-grid-redesign.md
**Date:** 2026-05-09
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE
**Supersedes:** PLAN-viewport-redesign.md, PLAN-nude-screen.md

## Overview

Grid becomes an AudioBuffer -- live frame buffer of jam::Cell records. Parser writes in-place. Display reads dirty rows + scroll-off. Screen stores scrollback as jam::Cells, renders via Glyph pipeline. Command FIFO eliminated.

## Current --> Target

| Component | Current | Target |
|-----------|---------|--------|
| Grid | Command FIFO (AbstractFifo + HeapBlock\<Command\>) | Cell buffer (HeapBlock\<char, true\>, row pointers, SIMD-aligned, ring) |
| Grid owner | Processor (value) | Session (value) |
| State owner | Processor (value) | Session (value) |
| Parser --> Grid | push(Command) | getWritePointer(row) -- write Cell in-place |
| Display --> Screen | grid.drain() --> screen.makeLayout(batch) | consumeDirtyRows() + consumeScrolledRows() --> tell Screen |
| Screen | Applies Commands in makeLayout, owns Cells, renders | Receives rows from Display, stores Cells, renders |
| Command | Type + Cell + fillBg + param + row + col | Eliminated |
| Spatial logic | Split: Parser emits coords, Screen applies | Parser owns: cursor, wrap, scroll on Grid |
| Scrollback | Screen grows Cells indefinitely | Screen stores scroll-off rows from Display |

## Ownership Chain (Target)

```
Session
  +-- Grid              (value -- the AudioBuffer)
  +-- State             (value -- the APVTS)
  +-- Processor         (unique_ptr)
        +-- Parser      (unique_ptr, receives Grid& and State&)
```

## Steps

### Step 1: Parser UTF-8 Fast Path

**Scope:** ParserVT.cpp

**Action:**
- Extend `processGroundChunk()` to decode full UTF-8 codepoints (currently ASCII 0x20-0x7E only).
- Non-ASCII printable bytes currently fall through to the DFA slow path. Move UTF-8 multi-byte decode into the fast path.
- Still emits Commands (Command FIFO not eliminated yet). UTF-8 decode logic survives the Step 4 transition -- only emission target changes.

**Files:**
- `Source/terminal/logic/ParserVT.cpp` -- processGroundChunk extension

**Validation:** Compiles. UTF-8 text renders correctly. No regression on ASCII.

### Step 2: Grid Class

**Scope:** New Grid implementation replacing FIFO

**Action:**
- Replace FIFO with AudioBuffer-pattern cell buffer.
- Single `HeapBlock<char, true>` allocation: `Cell** rowPointers + Cell[] data`. SIMD-aligned (Cell count per row rounded to multiple of 4).
- Ring buffer indexing -- head advances on scroll, no memmove. Capacity: `visibleRows + scrollMargin`.
- Dual screen (normal + alternate) -- internal swap via `setScreen()`.
- Dirty tracking: `std::atomic<uint64_t>` bitmask per buffer, consumed via `consumeDirtyRows()`.
- Scroll-off capture: `getNumScrolledRows()`, `getScrolledReadPointer()`, `consumeScrolledRows()`.
- Terminal-specific: `scrollUp(int n)` -- advances ring head, clears new bottom rows, marks dirty.
- Full API per RFC scaffold (setSize, getNumRows, getNumCols, getReadPointer, getWritePointer, getCell, setCell, clear, hasBeenCleared, setScreen, isAlternateScreen).

**Files:**
- `Source/terminal/logic/Grid.h` -- complete rewrite
- `Source/terminal/logic/Grid.cpp` -- new implementation

**Validation:** Compiles. API matches RFC scaffold.

### Step 3: Ownership -- Grid + State --> Session

**Scope:** Session, Processor

**Action:**
- Session owns Grid (value) and State (value) as members, constructed before Processor.
- Processor constructor changes: receives `Grid&` and `State&` (no longer owns them).
- Parser constructor unchanged (already receives `Grid&` and `State&`).
- Processor still exposes `getGrid()` and `getState()` -- returns references to Session-owned objects.
- Display unchanged -- still gets refs from Processor.
- Behavior identical -- pure ownership restructure.

**Files:**
- `Source/terminal/logic/Session.h` -- add Grid, State as value members
- `Source/terminal/logic/Session.cpp` -- construct Grid + State, pass refs to Processor
- `Source/terminal/logic/Processor.h` -- Grid& and State& instead of value members
- `Source/terminal/logic/Processor.cpp` -- update constructor

**Validation:** Compiles. Same runtime behavior. Ownership chain matches RFC.

### Step 4: Parser -- Grid In-Place Writes

**Scope:** Parser.h, ParserVT.cpp, ParserCSI.cpp, ParserESC.cpp, ParserEdit.cpp

This is the core architectural change. Parser stops emitting Commands, writes Grid directly.

**Action:**
- Parser tracks cursor internally: writeRow, writeCol (within visible grid bounds).
- `print()`: construct Cell from codepoint + pen --> `grid.getWritePointer(writeRow)[writeCol] = cell`. Advance writeCol. Wrap at cols: writeCol = 0, writeRow++. If writeRow >= visibleRows --> `grid.scrollUp()`, writeRow = visibleRows - 1.
- `lineFeed()`: if writeRow at bottom of scroll region --> `grid.scrollUp()`. Else writeRow++.
- `carriageReturn()`: writeCol = 0.
- `tab()`: advance writeCol to next tab stop.
- `backspace()`: writeCol-- (clamp to 0).
- CSI cursor moves (CUP, CUU/CUD/CUF/CUB): update writeRow/writeCol, clamp to bounds.
- Erase: `grid.clear(row, startCol, numCols)`.
- Insert/delete lines: memmove via Grid getWritePointer within scroll region.
- Insert/delete chars: memmove within row via getWritePointer.
- `processGroundChunk()`: UTF-8 decode (from Step 1) --> Cell construction --> Grid write.
- All `grid.push()` calls eliminated.
- Scroll regions (DECSTBM): Parser tracks top/bottom margins. scrollUp operates within margins.
- Wide chars: advance cursor by Cell width, not by 1.

**Files:**
- `Source/terminal/logic/Parser.h` -- cursor members (writeRow, writeCol), remove Command emission
- `Source/terminal/logic/ParserVT.cpp` -- print() writes Grid, processGroundChunk writes Grid
- `Source/terminal/logic/ParserCSI.cpp` -- cursor moves, erases, insert/delete on Grid
- `Source/terminal/logic/ParserESC.cpp` -- scroll, cursor save/restore on Grid
- `Source/terminal/logic/ParserEdit.cpp` -- insert/delete lines/chars on Grid

**Validation:** Parser writes correct cells to Grid. Basic sequences work (echo, ls, clear, cursor moves).

### Step 5: Display -- Grid Reads + Screen Wire

**Scope:** TerminalDisplay.cpp, Screen.h, Screen.cpp

**Action:**
- `onVBlank()` rewritten:
  1. `grid.getNumScrolledRows()` -- for each scroll-off row: `grid.getScrolledReadPointer(i)` --> `screen.appendScrollbackRow(ptr, cols)`. Then `grid.consumeScrolledRows(n)`.
  2. `grid.consumeDirtyRows()` -- for each dirty bit: `grid.getReadPointer(row)` --> `screen.updateVisibleRow(row, ptr, cols)`.
  3. State: cursor position/visibility --> `screen.setCaretPosition(index)`.
- Screen API changes:
  - `makeLayout(const Command*, int)` eliminated.
  - New: `appendScrollbackRow(const Cell*, int cols)` -- copies row into scrollback Cells.
  - New: `updateVisibleRow(int row, const Cell*, int cols)` -- updates visible region Cells.
- Screen Cells layout: `[scrollback rows | visible rows]`. Total size = `(scrollbackCount + visibleRows) * cols`.
- Viewport auto-scroll to bottom after content change.

**Files:**
- `Source/component/TerminalDisplay.cpp` -- rewrite onVBlank
- `Source/terminal/rendering/Screen.h` -- appendScrollbackRow, updateVisibleRow (replace makeLayout)
- `Source/terminal/rendering/Screen.cpp` -- implement row receive, remove Command switch

**Validation:** Terminal renders. Scrollback accumulates. Viewport scrolls.

### Step 6: Cleanup

**Scope:** Multiple files

**Action:**
- Delete `Source/terminal/data/Command.h`.
- Remove `#include "Command.h"` from all files.
- Remove old Grid FIFO code (AbstractFifo, HeapBlock\<Command\>).
- Delete superseded PLANs: PLAN-viewport-redesign.md, PLAN-nude-screen.md.
- Delete superseded RFCs: RFC-viewport-redesign.md, RFC-atlas-owns-packer.md (if stale).

**Validation:** Clean compile. No Command references. No dead code.

## Architecture Constraints (RFC)

- Grid = AudioBuffer, not storage. Screen owns visual storage.
- Parser reads/writes Grid in-place (processBlock pattern).
- Lock-free -- no mutex, no CriticalSection.
- MVC is fractal -- Display is V+C, Screen is V.
- Scrollback is visual concern -- Screen's JUCE Viewport, not Grid.
- Session --> Grid, Session --> Processor --> Parser(Grid&) ownership chain.
- Grid ring buffer for scroll-off -- encapsulated, Parser/Display don't see it.
- Dirty tracking via atomic\<uint64_t\> bitmask -- consumed by Display on VBlank.

## BLESSED Alignment

- **B:** Session owns Grid+State (RAII). Parser borrows Grid& (reference). Clear ownership.
- **L:** Grid is one class, one allocation. Screen is ~200 lines.
- **E:** getWritePointer/getReadPointer -- no magic. consumeDirtyRows -- explicit sync.
- **S(SSOT):** Grid = SSOT for live cells. State = SSOT for cursor/modes. Cells = SSOT for scrollback.
- **S(Stateless):** Grid is dumb buffer. Parser writes. Display reads.
- **E(Encap):** Ring buffer, dirty tracking, dual screen -- all internal to Grid.
- **D:** Same bytes --> same Grid content --> same Screen content --> same pixels.

## Risks

1. **Step 4 size** -- Parser rewrite spans all Parser*.cpp files. Must be atomic (can't half-migrate from Commands to Grid writes).
2. **Scroll regions** -- DECSTBM adds complexity to Parser scroll logic. Parser must track top/bottom margins.
3. **Alternate screen** -- LineFeed behavior differs (normal: ring advance, alternate: memmove within fixed buffer). Grid::scrollUp must handle both modes.
4. **Thread safety** -- Parser writes Grid on reader thread, Display reads on message thread. Dirty tracking + scroll-off consumption must use correct atomic patterns.
5. **Wide chars** -- cursor advance by Cell width (1 or 2), not always 1.
