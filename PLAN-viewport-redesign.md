# PLAN: Viewport Redesign

**RFC:** RFC-viewport-redesign.md
**Date:** 2026-05-08
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE
**Supersedes:** PLAN-terminal-rendering-pipeline.md Steps 10–12

## Overview

Replace Grid-as-persistent-store with Grid-as-FIFO. Parser becomes pure VT state machine pushing CellOps through lock-free SPSC ring. Screen (jam::TextEditor subclass) owns all cell content, scrollback, and layout. Display orchestrates via ValueTree::Listener — event-driven, tell-only, no poking internals.

## Resolved Decisions

| Q | Decision |
|---|----------|
| Q1 Alt screen | `std::array<LineRing, 2>` indexed by `State::activeScreen`. Direct lookup. |
| Q2 Mutation | Screen self-managed. Owns cells, applies ops, calls `setText(Cells&&)`. |
| Q3 Tab stops | State APVTS. Parser reads atomics. |
| Q4 Scroll region | State ValueTree. Screen reads at drain time. |
| Q5 Capacity | `cols × visibleRows × 2`, backpressure on overflow. |

## Architecture Constraints (ARCHITECT-stated)

- Fully event-driven — Display listens (ValueTree::Listener), orchestrates
- Always tell, never ask — no poking internals
- No manual boolean, no manual lambda, no manual state tracking, no shadow state
- Parser has ZERO knowledge of Screen, viewport, scrollback, UI
- Lock-free — no mutex, no CriticalSection
- Screen cells = bounded FIFO limited by scrollback capacity
- Rendering: persistent renderTarget image, append-only dirty tracking

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions.

## Steps

### Step 1: CellOp

**Scope:** terminal/data/CellOp.h (new)
**Action:** Define `CellOp` struct — `Type` enum (Print, LineFeed, CarriageReturn, Tab, Backspace, EraseInLine, EraseInDisplay, InsertLines, DeleteLines, InsertChars, DeleteChars, EraseChars, SetScreen, ClearScrollback, SetTabStop, ClearTabStop, ClearAllTabStops), `jam::Cell cell`, `juce::Colour fillBg`, `int param`.
**Validation:** Compiles standalone. No dependencies beyond Cell.h and juce::Colour.

### Step 2: Grid → FIFO + Parser refactor

**Scope:** Grid.h (rewrite), Grid.cpp (delete/rewrite), Parser.h, Parser.cpp, ParserEdit.cpp, ParserVT.cpp, ParserCSI.cpp, ParserESC.cpp, ParserOps.cpp, Processor.h/cpp

**Action — Grid:**
- Replace Grid class with FIFO transport: `juce::AbstractFifo` + `juce::HeapBlock<CellOp>`.
- Public API: `push(const CellOp&)` (reader thread), `drain(CellOp*, int maxOps)` (message thread), `isEmpty()`, `resize(int cols, int visibleRows)` (reallocates to `cols × visibleRows × 2`).
- Delete: Writer, Row, Lines, TerminalLine, dirty tracking, resizeLock, text extraction, serialization.
- Delete: GridAccess.cpp, GridErase.cpp, GridReflow.cpp, GridReflowHelpers.cpp, GridScroll.cpp, GridSerialize.cpp.

**Action — Parser:**
- Replace `Grid::Writer writer` with `Grid& grid` (FIFO reference).
- Delete `rowMapping`, `rowCells()`, `rowGraphemes()`, `rowLinkIds()`, `updateRowLength()`, `rebuildRowMapping()`.
- All write sites (~15) → `grid.push(CellOp{...})`:
  - `print` → Print, `lineFeed` → LineFeed, `CR` → CarriageReturn
  - `shiftLines` → InsertLines/DeleteLines, `shiftCellsRight` → InsertChars
  - `removeCells` → DeleteChars, `eraseCells` → EraseChars
  - `eraseInLine` → EraseInLine, `eraseInDisplay` → EraseInDisplay
  - `setScreen` → SetScreen, tab changes → SetTabStop/ClearTabStop
- `resize()` simplifies: clamp cursors + reset scroll region via State. No rowMapping rebuild.

**Action — Processor:**
- Replace `Grid grid` (persistent store) with `Grid grid` (FIFO).
- Constructor: capacity from config or initial `cols × visibleRows × 2`.
- `resized()`: `grid.resize(cols, rows)` (realloc FIFO) + pass to Parser via State atomics.

**Note:** Grid replacement and Parser refactor are an atomic pair — compilation breaks between them. Single engineering pass.

**Validation:** Compiles. Parser pushes CellOps. Grid is AbstractFifo + HeapBlock only. No Writer, no rowMapping, no resizeLock.

### Step 3: Screen line storage

**Scope:** Screen.h, Screen.cpp

**Action:**
- Screen owns scrollback-bounded line ring:
  - Line type (same shape as old TerminalLine): `HeapBlock<Cell>`, `HeapBlock<Grapheme>`, length, capacity.
  - Ring buffer, capacity = `visibleRows + scrollbackCapacity` (from config).
  - `std::array<Ring, 2>` indexed by `state.getActiveScreen()` — direct lookup, no branching.
- Screen owns `cols` and `visibleRows` — set by Display.
- Screen reads cursor from State ValueTree for rendering.
- Remove dummy preview content — screen starts empty.

**Name gate:** Line type and ring type names require ARCHITECT approval.

**Validation:** Compiles. Screen has internal storage. No functional change yet.

### Step 4: Screen op application

**Scope:** Screen.h, Screen.cpp

**Action:**
- `void applyOps(const CellOp* ops, int count)` — processes batch into active line ring:
  - Print: write cell at cursor position (from State)
  - LineFeed: advance row, scroll at region bottom (scrollTop/scrollBottom from State)
  - CarriageReturn: col → 0
  - Tab: advance to next tab stop (from State)
  - Erase ops: fill cells with fillBg
  - Insert/Delete lines/chars: shift within scroll region or line
  - SetScreen: switch active ring index
  - ClearScrollback: clear scrollback in active ring
  - Tab stop ops: forward to State
- After batch: build `jam::Cells` from visible portion → `setText(Cells&&)`.
- Scrollback trim: oldest lines fall off when ring exceeds capacity.
- **Scroll boundary postcondition:** after every `applyOps()` call and after every scroll event, Screen recomputes `isAtTop` and `isAtBottom` from its own scroll offset and total content height. These drive scroll indicator rendering and clamp scroll input at boundaries. Screen-owned derived state — no external query needed.

**Validation:** Compiles. Screen processes CellOps into line storage and renders via TextEditor. Scroll boundaries correct at content edges.

### Step 5: Display wiring

**Scope:** TerminalDisplay.h, TerminalDisplay.cpp

**Action:**
- `onVBlank()`: `state.consumeSnapshotDirty()` → drain Grid FIFO → `screen.applyOps(batch, count)`.
- **Drain loop:** drain in batches (e.g. 256 ops per `drain()` call), loop until FIFO empty or per-frame op cap reached. Cap prevents a single VBlank from blocking the message thread under high-volume output. Remaining ops drain on next VBlank.
- Remove `ScopedTryLock` on `grid.getResizeLock()` — no lock. FIFO drain is message-thread-only.
- Display as `ValueTree::Listener` for State changes (cursor, modes, active screen).
- **Resize debounce:** `resized()` computes new cols/rows from pixel bounds, compares against `lastDimensions` (Display-owned `std::pair<int,int>`). SIGWINCH sent only when grid dimensions actually change. Pixel-level size changes during pane drag that do not alter cell count produce no PTY notification. `lastDimensions` is Display's own private comparison state — not a mirror of State or Screen.
- `resized()`: on dimension change → `state.setDimensions()` → tell Screen new dimensions → SIGWINCH.
- Display tells Screen — Screen never asks Display.

**Validation:** Live terminal renders. Characters appear on keystroke. `ls` output visible. Resize works without spurious SIGWINCH. Scrollback accumulates. High-volume output does not block message thread.

### Step 6: Tab stops → State

**Scope:** State.h, State.cpp, Parser tab stop sites

**Action:**
- Add tab stop table to State via StringSlot (seqlock): `char[256]` bit vector, one byte per column.
- Setters (reader thread): `setTabStop(int col)`, `clearTabStop(int col)`, `clearAllTabStops()`, `initTabStops(int cols)`.
- Reader (message thread): `readTabStops()` — snap-copy via seqlock.
- Parser: delete `tabStops` member and `initializeTabStops()`. Call State setters instead.
- Screen reads tab stops from State at drain time.

**Validation:** Tab key advances correctly. HTS/TBC escape sequences work. Tab stops survive across ops.

### Step 7: Cleanup

**Scope:** Multiple files

**Action:**
- Delete dead Grid includes from: LinkManager.h/cpp, LinkManagerScan.cpp.
- Migrate text extraction (selection, LinkManager) from old Grid to Screen's line ring.
- Delete old TerminalLine, Lines<> template if fully replaced.
- Remove `lookAndFeelChanged()` test from Screen.h (yellowgreen test — confirm with ARCHITECT).
- Update ARCHITECTURE.md to reflect new topology.

**Validation:** Clean compile. No dead includes. No dead code. ARCHITECTURE.md matches codebase.

## Deferred

- **Cursor (PLAN-terminal-rendering-pipeline Step 10):** Cursor as CaretComponent. Depends on Grid wiring (this plan). Do after Step 5.
- **State serialization:** `getStateInformation`/`setStateInformation` currently on Grid. Needs migration to Screen. Separate sprint.
- **Incremental content feed (old Step 12):** Superseded — Screen's append-only dirty tracking + renderTarget achieves same goal without per-paragraph API.

## BLESSED Alignment

- **B:** Grid FIFO owns HeapBlock. Screen owns line rings. No cross-thread calls. CriticalSection eliminated.
- **L:** Grid = AbstractFifo + HeapBlock. CellOp = flat POD. No speculative abstraction.
- **E:** CellOp carries all context at emission (fillBg, param). No implicit state. No magic.
- **S(SSOT):** Screen line ring = SSOT for cells. State = SSOT for cursor/modes/tabs. Grid empties after drain.
- **S(Stateless):** Grid has no memory after drain. Parser holds no render state.
- **E(Encap):** Parser → Grid → Screen. Each layer ignorant of the others. ValueTree::Listener, not internals. Tell, don't ask.
- **D:** Same bytes → same CellOps → same Screen content. Resize doesn't change cell sequence.

## Risks

1. **Step 2 size** — Grid + Parser refactor is the largest step (~15 write sites). Atomic pair, cannot split.
2. **LinkManager/Selection** — Currently depend on Grid for text extraction. Step 7 migration scope may expand.
3. **State serialization** — Deferred. Terminal state save/restore broken until migrated.
4. **New type names** — ScreenLine, LineRing (Step 3) need ARCHITECT approval per NAMES.md.
