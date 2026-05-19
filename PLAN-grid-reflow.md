# PLAN: Grid Reflow — Content-Preserving Resize

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-05-19
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)
**Reference:** tmux `grid_reflow()` — structural and flow conformance; implementation uses jam/JUCE APIs

## Overview

Implement content-preserving resize for `Terminal::Grid`. Currently `Grid::setSize()` destroys all content on resize (blank screen). New `Grid::reflow()` follows tmux's row-by-row dispatch pattern (move/join/split) to migrate content to a resized ring buffer. Prerequisite: `Row::usedCols` must be maintained by Video (currently never set).

## Language / Framework Constraints

C++ / JUCE — MANIFESTO.md enforced as written. `jam::Buffer::copyFrom()` for row migration. `jam::Row::wrapped` flag already set by `Video::resolveWrapPending()`. No verbatim tmux C translation — use framework APIs where they exist.

## tmux Structural Conformance

The following tmux patterns are adopted at the structural/flow level:

1. **Row-by-row dispatch** — main loop walks all rows (history + viewport), dispatches each to one of three operations based on `usedCols` vs new width and `wrapped` flag:
   - `usedCols <= newCols` and not wrapped → **move** (copy as-is)
   - `usedCols <= newCols` and wrapped → **join** (append next row's cells into this row)
   - `usedCols > newCols` → **split** (break into multiple rows at new width)

2. **Target accumulation** — reflow writes into a scratch buffer sequentially (row 0, 1, 2, ...). At the end, compute head and numRows from total rows written, copy scratch → live buffer.

3. **Cursor wrap/unwrap** — before reflow, convert cursor `(col, row)` to paragraph-relative `(wx, wy)` where `wy` counts hard newlines and `wx` counts columns within the logical line. After reflow, unwrap back to `(col, row)` in the new layout. If cursor falls into history, reset to `(0, 0)`.

4. **Split-tail join** — when a split produces a last fragment shorter than new width AND the original row was wrapped, attempt to join the next row's cells into that fragment (tmux: `grid_reflow_join` called from `grid_reflow_split` with `already=1`).

5. **Partial consumption** — join may consume only part of the next row (cells that fit). Remaining cells stay on the source row for the main loop to process on its next iteration.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 0: Maintain Row::usedCols in Video

**Scope:** `Source/terminal/logic/Video.cpp`, `Source/terminal/logic/Video.h`
**Action:**
- `Row::usedCols` is currently NEVER SET (always 0). Reflow needs it to know where content ends per row. Same role as tmux's `cellused`.
- In `Video::print()` — after writing a cell at `writeCol`, update: `row->usedCols = juce::jmax(row->usedCols, static_cast<uint16_t>(writeCol + 1))`. High-water mark, grows only.
- In grapheme/combining path — same update after writing the base cell.
- Clear operations (`Grid::clear()`) already memset the Row to zero, which zeros `usedCols` — correct behavior (cleared row has no content).
- `usedCols` is NOT reduced by erase operations (same as tmux `cellused`) — it tracks the highest written position.
**Validation:** After this step, `usedCols` reflects the content extent of every row written by Video. Clear resets it to 0. Erase does not shrink it.

### Step 1: Grid API — Add reflow() declaration, isAllocated() and getNumRows()

**Scope:** `Source/terminal/logic/Grid.h`
**Action:**
- Add `bool isAllocated() const noexcept` — returns `ringMask > 0`. Place in "Asked" section.
- Add `int getNumRows (int screen) const noexcept` — returns `numRows.at(screen)`. Place in "Asked" section.
- Add `std::array<int, 2> reflow (int newViewportRows, int newCols, int scrollbackLines) noexcept` — in "Told" section after `setSize()`. Returns new numRows per screen. Grid does NOT write State — Processor reads the return value and tells Grid (setNumRows) and State.
- Doxygen: documents tmux-conformant row-by-row dispatch (move/join/split), content preservation, return value semantics.
**Validation:** Declarations match contract. TETRIS intact — return value, not internal write.

### Step 2: Grid reflow() — tmux-conformant row-by-row dispatch

**Scope:** `Source/terminal/logic/Grid.cpp`
**Action:** Implement `Grid::reflow()` following tmux structural flow:

1. **Save old state:** `oldRingMask`, `oldHead`, `oldNumRows`, `oldViewportRows`, `oldNumCols` (from `buffer.getNumCols()`).
2. **Allocate scratch:** local `jam::Buffer<jam::Row> scratch` with new ring size and new col count. Sequential write index `writeIdx` starts at 0.
3. **Per-screen dispatch (screen 0 and 1):**
   Walk all old rows (history + viewport) from oldest to newest. For each row, read via `buffer.getReadPointer(screen, physicalIdx)` using old ring arithmetic.

   **Dispatch (tmux conformant):**
   - `usedCols == newCols` → **move**: copy row to scratch at `writeIdx`, preserve wrap flag as-is.
   - `usedCols > newCols` → **split**: write cells `[0..newCols)` to scratch row with `wrapped` flag. Continue writing remaining cells in chunks of `newCols`. Last chunk: set `wrapped` only if original row had it. After split, if last chunk has room and original was wrapped → attempt **join** with next source row (split-tail join).
   - `usedCols < newCols` and `wrapped` → **join**: copy this row's cells to scratch row. Append cells from next source row(s) until either: new width reached, or a non-wrapped row consumed, or source exhausted. Handle partial consumption — if next row not fully consumed, remaining cells stay for main loop. Set `wrapped` flag only if last consumed row was wrapped.
   - `usedCols < newCols` and not `wrapped` → **move**: copy row to scratch at `writeIdx`, no wrap flag.
   - `usedCols == 0` (empty row, not wrapped) → **move**: copy empty row.

   **Row writing:** Use `scratch.getWritePointer(screen, writeIdx)` to get target Row*. Copy cells via `std::memcpy` on the `cells[]` FAM region up to `juce::jmin(usedCols, newCols)` cells. Set `usedCols` and `flags` on target row. Increment `writeIdx`.

4. **Compute head and numRows:**
   - `totalNewRows = writeIdx` (per screen).
   - If `totalNewRows < newViewportRows`: pad scratch with empty rows to fill viewport.
   - `newHead = juce::jmax(0, totalNewRows - newViewportRows)`.
   - `newNumRows = juce::jmin(newHead, scrollbackLines)`.
   - `head[screen] = newHead & newRingMask`.

5. **Finalize:** Call `buffer.setSize(2, newRingSize, newCols, false, true, false)`. Copy scratch rows → buffer row by row via `buffer.copyFrom(screen, physicalRow, scratch, screen, physicalRow)`. Set `ringMask`, `viewportRows`, `scrollbackLines`, `head`, `numRows`. Return `{newNumRows[0], newNumRows[1]}`.

6. **Degenerate case:** If `newCols == oldNumCols`, skip join/split — move all rows, only adjust for new viewport height.

**Constraints:**
- `jam::Buffer::copyFrom()` for all row transfers — no manual stride arithmetic.
- `Row::usedCols` set correctly on every output row.
- `Row::wrapped` flag set/cleared per tmux rules: split chunks get wrapped (except last if original wasn't), join result gets wrapped only if last consumed source was wrapped.
- Ring capacity: if `writeIdx` exceeds ring size, oldest rows overwritten via `& newRingMask` — natural eviction.
**Validation:** Ring arithmetic correct. Content preserved. tmux dispatch logic matched structurally. BLESSED compliant.

### Step 3: Cursor tracking — wrap/unwrap around reflow

**Scope:** `Source/terminal/logic/Grid.h`, `Source/terminal/logic/Grid.cpp`
**Action:**
- Add static helpers (file-scope `static` in Grid.cpp):
  - `wrapCursorPosition(buffer, head, numRows, viewportRows, ringMask, cursorCol, cursorRow) → (wx, wy)` — converts viewport-relative cursor to paragraph-relative coordinates. `wy` = hard newline count from row 0 to cursor row. `wx` = column offset within the logical line (accumulates cols across wrapped continuations). If cursor past content end, `wx` = max sentinel.
  - `unwrapCursorPosition(buffer, head, numRows, viewportRows, ringMask, newCols, wx, wy) → (col, row)` — walks reflowed rows counting hard newlines to find paragraph `wy`, then walks within that paragraph to find column `wx`. Returns viewport-relative position.
- `Grid::reflow()` takes additional parameters: `int cursorRow, int cursorCol` (viewport-relative, from Video). Returns `struct ReflowResult { std::array<int,2> numRows; int cursorRow; int cursorCol; }`.
- Before dispatch: wrap cursor for active screen. After finalize: unwrap cursor. If cursor falls into history (absolute row < newHead), reset to `(0, 0)`.
**Validation:** Cursor survives reflow at correct logical position. Falls-to-history handled. Paragraph counting matches tmux wrap/unwrap semantics.

### Step 4: Processor::prepare() — Route first-call vs resize

**Scope:** `Source/terminal/logic/Processor.cpp`
**Action:**
- In `prepare()`: check `grid.isAllocated()`.
  - If `false` (first call): call `grid.setSize()` as before. Reset `numRows` to {0,0}.
  - If `true` (subsequent resize): call `grid.reflow(newViewportRows, newCols, scrollbackLines, video.getCursorRow(), video.getCursorCol())`. From the returned `ReflowResult`:
    - Set `this->numRows` from result.
    - Call `grid.setNumRows()` and `state.setNumRows()` for both screens.
    - Pass `result.cursorRow`, `result.cursorCol` to Video — Video needs a `setCursorPosition(row, col)` or Processor sets via existing cursor state path.
- `video.setDimensions()` and `video.resize()` still called after both paths (cursor clamp, tab stops, scroll region reset).
**Validation:** First-call path unchanged. Resize path preserves content and cursor. TETRIS intact. numRows synced across Grid, Processor, State.

### Step 5: Audit

**Scope:** All files modified in Steps 0-4
**Action:** @Auditor validates against MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions.
**Validation:** Zero findings required for completion.

## BLESSED Alignment

- **B (Bound):** Scratch buffer local to reflow(), dies on return. No dangling refs.
- **L (Lean):** reflow() follows tmux decomposition: move/join/split as distinct code paths. No god method.
- **E (Explicit):** `isAllocated()`, `reflow()`, `ReflowResult` — semantic names. `usedCols` — explicit content extent. No magic values.
- **S (SSOT):** reflow() returns numRows + cursor. Processor is sole writer to Grid (setNumRows) and State. No shadow state. usedCols is SSOT for row content extent.
- **S (Stateless):** Grid remains dumb storage. reflow() is a one-shot migration. Scratch is transient.
- **E (Encapsulation):** Grid handles content migration. Processor orchestrates. Video maintains usedCols. Each owns its domain.
- **D (Deterministic):** Same content + same new dimensions = same output.

## Risks / Open Questions

None — all decisions resolved with ARCHITECT.
