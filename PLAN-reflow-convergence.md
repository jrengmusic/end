# PLAN: Grid Reflow Convergence — TETRIS SmoothStateTransition Conformance

**RFC:** RFC-reflow-convergence.md (pre-refactor reference; codebase + ARCHITECT discussion is ground of truth)
**Date:** 2026-05-21
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / jam framework

## Overview

Redesign GridResize to conform to TETRIS SmoothStateTransition. GridResize is a dumb transition machine — owns timer, holds scratch buffer, interpolates dimensions per tick, writes animated values to bare atomic references. Processor tells GridResize when to start. Screen renders reactively from State (unaware of animation). Grid owns all reflow computation. SIGWINCH deferred to final tick. Fix scrollOffset=0 live-mode guard.

## Completed (keep as-is)

- `Row::dead` bit (jam_row.h:39) — tombstone flag for reflow
- Reflow primitives: `reflowDead`, `reflowMove`, `reflowJoin`, `reflowSplit` (Grid.cpp)
- `reflowScreen` outer loop — tmux-faithful dispatch (Grid.cpp)
- Cursor wrap/unwrap with sentinel — `wx=-1` for cursor-past-end (Grid.cpp)
- `Grid::resizeHeight` — tmux screen_resize_y translation (Grid.h/cpp)
- `Grid::reflow` body — scratch buffer, normal screen reflow, alternate screen move-only (Grid.cpp)

## TETRIS SmoothStateTransition Mapping

### Roles

| TETRIS (DSP) | Terminal |
|---|---|
| DSP core (trivially copyable, owns calc/process) | Grid (Row is trivially copyable, owns reflow/resizeHeight) |
| SmoothStateTransition (dumb worker: holds previous+current, owns crossfade timer, told to start by ProcessorChain) | GridResize (dumb worker: holds scratch buffer, owns animation timer, told to start by Processor) |
| ProcessorChain (orchestrator: tells SST to start via `.set()`, calls SST.process() per sample via processBlock) | Processor (orchestrator: tells GridResize to start via `.start()`, ignores VT changes during transition) |
| `std::atomic<float> gainReduction` (DSP writes, UI reads, no coupling) | Atomic refs for cols/visibleRows/numRows/scrollOffset/cursor (GridResize writes, State flushes, Screen reads) |

### SST chain (DSP)

```
parameterChanged → ProcessorChain::parameterChanged
    parameters.get(id, value)         ← registered lambda
    → highPass.set("SLOPE", value)    ← ProcessorChain TELLS SST to start

processBlock → for each sample:
    highPass.process(sample)          ← continuous external driver
    SST internally:
        if transitioning: blend previous + current, advanceCrossfade
        else: current.process(sample)
```

SST owns the crossfade. ProcessorChain tells it when to start. processBlock drives it continuously. SST writes to shared data (audio buffer) directly — no callback, no std::function.

### GridResize chain (Terminal)

```
Display::resized → state.setValue(cols, rows) → flush → Processor::valueTreePropertyChanged
    Processor: if not gridResize.isInTransition():
        gridResize.start(oldDims, newDims)    ← Processor TELLS GridResize to start
    else:
        gridResize.updateTarget(newDims)      ← coalesce

GridResize owns timer → timerCallback:          ← self-driven (no processBlock equivalent)
    interpolate currentDims toward target
    grid.reflowFrom(scratch, currentDims)       ← reflow scratch→live
    video.setDimensions/resize(currentDims)
    write animated values to atomic refs:       ← like DSP writing std::atomic<float>
        cols, visibleRows, numRows[0], numRows[1],
        scrollOffset, cursorRow, cursorCol
    State flush picks them up → Screen renders  ← Screen unaware of animation

final tick:
    reflow to targetDims (exact)
    write final values to atomic refs
    tty->platformResize(targetDims)             ← SIGWINCH, once
    isTransitioning = false
```

GridResize owns the transition. Processor tells it when to start. GridResize writes to bare atomic references — no State object, no callback, no std::function.

### What each class knows

| Class | Knows about | Does NOT know about |
|---|---|---|
| GridResize | Grid, Video, atomic refs, TTY* | State, Processor, Screen, Display |
| Processor | State, Grid, Video, GridResize | Screen, Display, TTY (delegates to GridResize) |
| Grid | jam::Buffer, jam::Row | State, Video, GridResize, Processor |
| Screen | State (reads VT), Grid (reads getBlock) | GridResize, animation, Processor |

### Row is trivially copyable

`jam::Row`: uint16_t usedCols, uint8_t flags, Cell cells[] (FAM). `jam::Buffer::copyFrom` copies full rows. Scratch snapshot is memcpy-fast per row — maps to SST's `previous = current` trivial copy.

### scrollOffset = tmux hscrolled

- Both: user scroll position into history (0 = live, >0 = scrolled back)
- During reflow: adjusted when rows inserted/deleted before scroll position
- Guard: scrollOffset=0 (live mode) must stay 0
- Grid::reflow owns the computation, takes scrollOffset as in/out parameter

## Validation Gate

Each step validated by @Auditor against:
- MANIFESTO.md (BLESSED)
- NAMES.md
- JRENG-CODING-STANDARD.md
- This PLAN (no deviation)

## Steps

### Step 1: Fix scrollOffset=0 guard

**Scope:** `Grid.cpp` — reflowJoin, reflowSplit
**Action:** Guard all scrollOffset adjustments with `if (scrollOffset > 0)`. Live mode (scrollOffset=0) never incremented.
- reflowJoin (~line 300): wrap adjustment in `if (scrollOffset > 0)`
- reflowSplit (~line 360): `if (scrollOffset > 0 and yy <= scrollOffset)`
**Validation:** scrollOffset=0 stays 0. scrollOffset>0 adjusts per tmux hscrolled logic.

### Step 2: Width-aware reflow primitives

**Scope:** `Grid.cpp` — reflowSplit, reflowJoin

**Problem:** Current reflow treats every cell as width-1. WIDE cells (CJK, emoji, fullwidth forms) occupy 2 columns as a WIDE+SPACER_TAIL pair. Cell-count chunking in reflowSplit tears pairs apart. Cell-count fit check in reflowJoin allows buffer overrun.

**Cell.wide() values:** NARROW(0), WIDE(1), SPACER_TAIL(2), SPACER_HEAD(3). SPACER_HEAD is defined but never written by Video — it exists for exactly this reflow boundary case.

**Action — reflowSplit:**

Replace cell-count chunking with display-width accumulation:
```
int chunkCells = 0;
int chunkWidth = 0;
while (srcOffset + chunkCells < usedCols)
    const auto& cell = srcRow->cells[srcOffset + chunkCells]
    cellWidth = (cell.wide() == WIDE) ? 2 : (cell.wide() == SPACER_TAIL) ? 0 : 1
    if (chunkWidth + cellWidth > newCols) break
    chunkWidth += cellWidth
    ++chunkCells
```

At boundary when 1 column remains and next cell is WIDE:
- Insert SPACER_HEAD in the last column of current chunk (padding)
- WIDE+SPACER_TAIL pair starts the next chunk as a unit

When dest width is 1 column: WIDE cells cannot fit. Replace with narrow empty cell (destroy). This matches Ghostty/Kitty behavior.

**Action — reflowJoin:**

Replace `if (at + 1 > newCols)` with width-aware check:
```
cellWidth = (srcCell.wide() == WIDE) ? 2 : (srcCell.wide() == SPACER_TAIL) ? 0 : 1
if (at + cellWidth > newCols) break
```

Copy WIDE+SPACER_TAIL as atomic pair — never split.

Strip SPACER_HEAD from target row end before appending (the spacer was padding from a previous split that no longer applies at the new width).

**usedCols maintenance:** After SPACER_HEAD insertion, usedCols includes the spacer. After SPACER_HEAD stripping, usedCols decrements.

**Grapheme integrity:** CONTENT_GRAPHEME cells reference `jam::Grapheme` sidecar. Grapheme is a `SharedResource` (process-wide singleton via `Context<Grapheme>`). Indices survive reflow — no sidecar migration needed. memcpy preserves the 21-bit index in the codepoint field.

**Action — glyph shaper:**

The shaper (`jam_glyph_arrangement_shape.cpp:226`) skips SPACER_TAIL: `if (pen.wide() != jam::Cell::SPACER_TAIL)`. SPACER_HEAD is NOT handled — a SPACER_HEAD cell would pass the check and attempt to shape codepoint 0.

Add SPACER_HEAD to the skip condition:
```
if (pen.wide() != jam::Cell::SPACER_TAIL and pen.wide() != jam::Cell::SPACER_HEAD)
```

SPACER_HEAD renders as empty space (same visual as SPACER_TAIL — the column is blank padding where a WIDE char didn't fit).

**Validation:** WIDE+SPACER_TAIL never torn apart. SPACER_HEAD inserted at boundary when WIDE doesn't fit. SPACER_HEAD skipped by glyph shaper. 1-column dest handles WIDE destruction. usedCols accurate after all operations. Grapheme indices preserved.

### Step 3: Grid::reflowFrom — external source buffer

**Scope:** `Grid.h`, `Grid.cpp`
**Action:** Add `reflowFrom` that reads from an external source buffer. Enables per-tick animation without copy-restore cycles.

```cpp
std::array<int, 2> reflowFrom (const jam::Buffer<jam::Row>& source,
                                const std::array<int, 2>& sourceHead,
                                const std::array<int, 2>& sourceNumRows,
                                int sourceRingMask, int sourceViewportRows,
                                int newViewportRows, int newCols, int scrollbackLines,
                                int& cursorRow, int& cursorCol, int& scrollOffset) noexcept;
```

Internally: allocates scratch at target ring size, reflows source→scratch using existing primitives, copies scratch→live buffer. Updates Grid's head/numRows/ringMask/viewportRows.

Existing `reflow()` becomes a thin wrapper: calls `reflowFrom(buffer, head, numRows, ringMask, viewportRows.value, ...)`.

**Validation:** Existing reflow behavior unchanged. reflowFrom reads from external source. Same tmux-faithful primitives.

### Step 4: Reshape GridResize — dumb transition machine with atomic refs

**Scope:** `GridResize.h`, `GridResize.cpp`
**Action:** Strip GridResize to conform to SST role. Owns timer and transition state. Writes animated values to bare atomic references. No State, no callback, no std::function.

**Remove:**
- `State& state` member and constructor parameter
- All State reads/writes
- `set()` / coalesce pattern — replaced by `start()` / `updateTarget()`
- `setScrollbackLines()` — passed as parameter to start()
- `hasPendingDimensions` / `hasPendingCellSize` — Processor decides when to call

**Keep:**
- `Grid& grid` — tells Grid to reflowFrom
- `Video& video` — tells Video setCellSize/setDimensions/resize/loadScreenState
- `TTY* tty` — SIGWINCH on final tick (GridResize's responsibility, like SST owning crossfade completion)
- `juce::Timer` inheritance — owns animation timer

**Add — atomic refs (set at construction, written per tick):**
- `Parameter<int>& normalNumRowsParam`
- `Parameter<int>& alternateNumRowsParam`
- `Parameter<int>& scrollOffsetParam`
- `Parameter<int>& cursorRowParam`
- `Parameter<int>& cursorColParam`

NOT written per tick: cols, visibleRows. These stay at their pre-animation values during the transition. Screen reads viewportRows from State but getBlock returns a Block whose numRows/numCols reflect the actual grid content — Screen uses Block dimensions for rendering, not State dimensions for layout. cols/visibleRows are written ONCE on final tick to match the target dimensions. This avoids Processor misidentifying intermediate writes as new external resize events.

These are references to State's Parameter<int> slots. GridResize writes via `param.store(value)` — same mechanism Video uses on READER thread. State flush propagates to VT. Screen renders.

**Add — snapshot ("previous" in SST):**
- `jam::Buffer<jam::Row> scratch`
- `std::array<int, 2> snapshotHead`
- `std::array<int, 2> snapshotNumRows`
- `int snapshotRingMask`
- `int snapshotViewportRows`

**Add — animation state:**
- `cell startCols, startRows`
- `cell targetCols, targetRows`
- `cell currentCols, currentRows`
- `int ticksRemaining { 0 }`
- `bool isTransitioning { false }`

**API:**

```cpp
// Told by Processor to start transition. Snapshots grid, starts timer.
void start (cell fromCols, cell fromRows, cell toCols, cell toRows,
            int scrollbackLines, int scrollOffset,
            int cursorRow, int cursorCol,
            bool cursorVisible, uint32_t keyboardFlags) noexcept;

// Told by Processor to update target during active transition. Coalesces.
void updateTarget (cell cols, cell rows) noexcept;

// Query — Processor checks before starting new transition.
bool isInTransition() const noexcept;

// First allocation — no animation (cold start, SST isReady=false equivalent).
void allocate (cell cols, cell rows, int scrollbackLines) noexcept;

// Cell pixel size — delegates to Video.
void setCellSize (int cellWidth, int cellHeight) noexcept;

// Wire TTY for SIGWINCH.
void setTTY (TTY* ttyToUse) noexcept;
```

**timerCallback (self-driven, like SST process but timer-based):**

**No reader-thread pause needed.** Reader writes during animation are at old dimensions — they'd render garbled even if preserved. Each tick's reflowFrom overwrites the live buffer with coherent reflowed content from the snapshot at the current interpolated dimensions. Reader writes between ticks are harmlessly overwritten by the next tick. After final tick, SIGWINCH fires, shell redraws at correct dimensions, reader output becomes coherent again. The animation IS the gap filler — same as SST crossfade filling the audio stream during parameter change. No lock, no pause, no race concern.

```
interpolate currentCols/currentRows toward target (cellsPerTick step)
grid.resizeHeight(currentRows, cursorRowCell)
numRows = grid.reflowFrom(scratch, ..., currentRows, currentCols, scrollbackLines,
                           cursorRow, cursorCol, scrollOffset)
video.setDimensions(currentCols, currentRows)
video.loadScreenState(cursorRow, cursorCol, cursorVisible, ...)
video.resize(currentCols, currentRows)

// Write per-tick animated values to atomic refs
normalNumRowsParam.store(numRows[0])
alternateNumRowsParam.store(numRows[1])
scrollOffsetParam.store(scrollOffset)
cursorRowParam.store(cursorRow)
cursorColParam.store(cursorCol)
// cols/visibleRows NOT written per tick — avoids Processor re-trigger

if currentDims == targetDims:                  ← final tick
    // Write final cols/visibleRows ONCE — Processor sees definitive target
    colsParam.store(targetCols.value)
    visibleRowsParam.store(targetRows.value)
    stopTimer()
    isTransitioning = false
    if tty running: tty->platformResize(targetCols, targetRows, pixelW, pixelH)
```

**Animation constants:**
- `static constexpr int tickIntervalMs { 16 }` — ~60fps
- `static constexpr int cellsPerTick { 2 }` — variable duration from delta

**Validation:** No State include. No callback. No std::function. Writes to atomic refs only. Owns timer. SIGWINCH on final tick. Conforms to SST pattern.

### Step 5: Processor orchestration — tells GridResize when to start

**Scope:** `Processor.h`, `Processor.cpp`
**Action:** Processor becomes the orchestrator that tells GridResize to start/coalesce.

**Constructor:**
- `gridResize` constructed with `Grid&, Video&, Parameter<int>& refs...` — Processor passes refs from State's parameter slots
- Initial allocation: `gridResize.allocate(cols, rows, scrollbackLines)` — cold start, immediate, no animation
- `gridResize.setTTY(tty.get())` — same as current, in Processor::setTTY()

**valueTreePropertyChanged (cols/visibleRows change):**

```
if not grid.isAllocated():
    gridResize.allocate(cols, rows, scrollbackLines)

else if not gridResize.isInTransition():
    read scrollOffset, cursorVisible, keyboardFlags from State VT nodes
    read cursorRow, cursorCol from Video
    gridResize.start(oldCols, oldRows, newCols, newRows,
                     scrollbackLines, scrollOffset,
                     cursorRow, cursorCol, cursorVisible, keyboardFlags)

else:
    gridResize.updateTarget(newCols, newRows)     ← coalesce
```

**Guard:** GridResize does NOT write cols/visibleRows during animation (only on final tick). So Processor's cols/visibleRows listener only fires from external sources (Display::resized). When `gridResize.isInTransition()` and new dims arrive, Processor calls `gridResize.updateTarget()` — clean coalesce, no ambiguity.

**Remove:** Direct calls to `gridResize.set()`, `gridResize.setScrollbackLines()`. All replaced by `start()` parameters.

**Validation:** Processor only tells GridResize to start/coalesce. Does not drive ticks. Does not write animated values. Does not own animation timer. Clean orchestrator.

### Step 6: Audit + cleanup

**Scope:** All modified files
**Action:**
- @Auditor full sweep against CONTRACT
- Replace magic screen indices (`0`, `1`) with `Map::Screen::normal`, `Map::Screen::alternate` across Grid.cpp, GridResize.cpp, and any new code. Pre-existing violation — all screen index literals must use Map::Screen named constants.
- Remove dead code: `getGridResize()` accessor (zero callers), old `set()`/`apply()` remnants
- Verify Video thread annotations (setDimensions/setCellSize documented READER, called on MESSAGE — pre-existing)
- Update ARCHITECTURE.md Grid/GridResize/Processor sections
- Delete PLAN-reflow-convergence.md (objective complete)

**Validation:** Zero findings. Docs current. No dead code.

## BLESSED Alignment

- **Bound:** Scratch buffer owned by GridResize, lifecycle bound to transition. Row trivially copyable — memcpy-safe. Timer started/stopped explicitly. Atomic refs are non-owning references — Parameter<int> lifetime owned by State, which outlives GridResize.
- **Lean:** Three clean responsibilities: Grid computes reflow, GridResize transitions dims, Processor orchestrates lifecycle. No god objects. GridResize is minimal — snapshot, step, allocate.
- **Explicit:** All start() parameters visible. Animation state visible (isTransitioning, ticksRemaining). Animated values written to named atomic refs — not hidden behind State abstraction. No magic — cellsPerTick and tickIntervalMs are named constants.
- **SSOT:** State atomics are SSOT for all terminal parameters. GridResize writes current animated values to the same atomics. Grid is SSOT for cell content. Scratch is SSOT for pre-resize snapshot. No shadow state.
- **Stateless:** GridResize holds only transition mechanics — no terminal semantics. Screen renders from State, unaware of animation. GridResize does not interpret the values it writes — just passes reflow output to atomic refs.
- **Encapsulation:** Processor tells GridResize (start). GridResize tells Grid (reflowFrom) and Video (setDimensions, resize). GridResize writes atomic refs (Store). Screen reads State (VT flush). Unidirectional. No reverse dependencies. No layer violations. No poking internals.
- **Deterministic:** Same snapshot + same dims = same reflow output. Animation is deterministic — fixed interpolation from start to target.

## TETRIS Alignment

- **T (Thread Separation):** GridResize.timerCallback() on MESSAGE thread. Atomic ref writes are lock-free (same mechanism as Video on READER thread — Parameter<int>::store is atomic). Grid modified on MESSAGE thread during resize; reader thread writes during normal operation (pre-existing concurrency — not introduced here).
- **E (Encapsulation):** Private state. Atomic refs are bare references — no State object coupling. start() takes all needed values as parameters.
- **T (Trivially Copyable):** Row IS trivially copyable. Scratch snapshot is memcpy via Buffer::copyFrom. GridResize itself is NOT trivially copyable (owns Buffer) — this is the cold-state/hot-state split. Buffer is cold (allocation), Row content is hot (data).
- **R (Reference Processing):** Grid::reflowFrom modifies live buffer directly. Cursor/scrollOffset modified in place. Atomic refs written per tick.
- **S (SmoothStateTransition):** GridResize IS SST for terminal resize. Processor tells it to start (like ProcessorChain calling `highPass.set()`). GridResize owns transition internally (like SST owns crossfade). Writes to shared atomic state (like DSP writing `std::atomic<float>`). No callback. No std::function.

## Risks / Open Questions

- **Video thread annotations:** setDimensions() and setCellSize() documented READER THREAD only but called on MESSAGE thread. Pre-existing. ARCHITECT decision needed.
- **Reader writes during animation:** Reader thread continues writing at old dimensions. These writes are harmlessly overwritten by each tick's reflowFrom. No pause or lock needed — the animation fills the gap, same as SST crossfade.
- **Per-tick reflow cost:** O(history + viewport) memcpys per tick at ~60fps. Sub-millisecond for typical scrollback. Profile if needed.
- **Scratch buffer memory:** Duplicates grid content during animation. ~16MB worst case (10K rows, 200 cols). Transient — freed on animation completion.
- **Parameter<int> ref access:** GridResize needs references to specific Parameter<int> slots in State. Processor must extract and pass them at construction. State's parameter map must expose these — verify API exists.
- **Screen viewportRows during animation:** Screen reads viewportRows from State (unchanged during animation). Block returned by getBlock carries actual numRows/numCols from the grid. Screen must use Block dimensions for rendering content, not State dimensions. Verify Screen::setText/calc uses Block extents.
