# PLAN: Fix Active Prompt Misalignment After Pane Close

**RFC:** N/A (bug fix)
**Date:** 2026-04-05
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

`Grid::reflow()` tracks and repositions `cursorRow` through the reflow algorithm but ignores three OSC 133 absolute-row markers: `promptRow`, `outputBlockTop`, `outputBlockBottom`. After reflow (triggered by pane close), these markers hold stale absolute row values, causing the active prompt to render at the wrong position with row artifacts. Fix by tracking all three markers through the same reflow pass that already tracks the cursor.

## Validation Gate
Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output is correct and BLESSED-compliant.

## Steps

### Step 1: Extend `countOutputRows()` to track prompt and output block markers

**Scope:** `Source/terminal/logic/GridReflow.cpp` — `countOutputRows()` function (lines 509-541)

**Action:**
Add three new marker-tracking parameter pairs to `countOutputRows()`, parallel to the existing `cursorLinear`/`outCursorOutputRow`/`outCursorNewCol` pattern:

1. Add parameters: `promptLinear`, `outPromptOutputRow`, `blockTopLinear`, `outBlockTopOutputRow`, `blockBottomLinear`, `outBlockBottomOutputRow`
2. Inside the logical-line walk loop (after the existing cursor tracking block at lines 526-533), add three parallel tracking blocks — one for each marker. Each uses the same pattern:
   - Check if `markerLinear >= r and markerLinear < r + runLen`
   - Compute `flatOffset = (markerLinear - r) * wp.oldCols`
   - Set `*outMarkerOutputRow = total + flatOffset / newCols`
3. Markers only need row tracking (no column), so no `outNewCol` needed for them
4. Initialize all three `*outMarkerOutputRow` to -1 at function start
5. Guard: if marker linear value is -1 (unset), skip tracking for that marker

**Validation:** Function signature extended, all three markers tracked in the walk loop, no early returns, existing cursor tracking unchanged.

### Step 2: Extend `Grid::reflow()` to pass markers and update State

**Scope:** `Source/terminal/logic/GridReflow.cpp` — `Grid::reflow()` function (lines 638-704)

**Action:**

1. Before the `countOutputRows()` call, read the three markers from State:
   ```
   const int promptRow     { state.getPromptRow() };
   const int blockTop      { state.getOutputBlockTop() };
   const int blockBottom   { state.getOutputBlockBottom() };
   ```

2. Compute linear positions (same pattern as cursor):
   ```
   const int promptLinear      { promptRow >= 0 ? promptRow : -1 };
   const int blockTopLinear    { blockTop >= 0 ? blockTop : -1 };
   const int blockBottomLinear { blockBottom >= 0 ? blockBottom : -1 };
   ```
   Note: these are already absolute (`scrollbackUsed + visibleRow`), which is the linear index. No conversion needed.

3. Declare output variables:
   ```
   int promptOutputRow     { -1 };
   int blockTopOutputRow   { -1 };
   int blockBottomOutputRow { -1 };
   ```

4. Pass all to `countOutputRows()`.

5. After the cursor update block (lines 684-702), add the marker update block. For each marker, if `markerOutputRow >= 0`:
   - Compute new absolute row: apply the same `rowsToSkip` subtraction
   - For `pinToBottom`: `newAbsolute = markerOutputRow - rowsToSkip + newVisibleRows - contentExtent + scClamped`
   - For non-pinToBottom: `newAbsolute = markerOutputRow - rowsToSkip`
   - Clamp to `>= 0`
   - Write back via `state.setPromptRow()` / `state.setOutputBlockStart()` — wait, `setOutputBlockStart` resets `outputBlockBottom` and sets `outputScanActive`. We need direct store instead.

**Design decision needed:** `State::setOutputBlockStart()` has side effects (resets bottom, activates scan). For reflow, we need raw stores. Options:
- (A) Add `State::setOutputBlockTopRaw(int)` and `State::setOutputBlockBottomRaw(int)` — direct `storeAndFlush` without side effects
- (B) Use existing `storeAndFlush` directly from Grid (Grid already has `state` reference)
- (C) Add a single `State::reflowMarkers(promptRow, blockTop, blockBottom)` method

Recommend **(A)** — minimal, explicit, follows existing naming pattern (`setPromptRow` is already a raw store). ARCHITECT decides.

**Validation:** All three markers read before reflow, passed through count, updated in State after reflow. No side effects on outputScanActive. Cursor tracking unchanged.

### Step 3: Add raw setters to State (if option A approved)

**Scope:** `Source/terminal/data/State.h` and `Source/terminal/data/State.cpp`

**Action:**
Add two methods:
```cpp
void setOutputBlockTopRaw (int row) noexcept;
void setOutputBlockBottomRaw (int row) noexcept;
```
Implementation: single `storeAndFlush` call each, same pattern as `setPromptRow`.

**Validation:** Methods exist, no side effects, follow existing naming and code style.

## BLESSED Alignment

- **B (Bound):** Marker values are owned by State, updated atomically under `resizeLock` — same ownership as cursor
- **L (Lean):** No new functions beyond 2 raw setters. countOutputRows grows by ~15 lines for 3 parallel tracking blocks
- **E (Explicit):** Raw setters named explicitly to distinguish from side-effect versions. No magic values — -1 sentinel already established
- **S (SSOT):** Markers stored in State only — reflow updates the single source, no shadow state
- **S (Stateless):** Grid remains stateless machinery — reads markers, reflows, writes back
- **E (Encapsulation):** Grid communicates with State via API only. Raw setters are State's API
- **D (Deterministic):** Same reflow input produces same marker positions — deterministic

## Risks / Open Questions

1. **Raw setter naming (Step 3):** Option A/B/C — ARCHITECT to decide
2. **outputScanActive state during reflow:** Reflow runs under `resizeLock` on message thread. The reader thread sets `outputScanActive` via OSC 133 C/D. If scan is active during reflow, the bottom marker is still being extended. The reflow should preserve the current `outputScanActive` state unchanged. Current plan does this (raw setters don't touch `outputScanActive`).
3. **Marker falls outside linearRows:** If a marker's absolute row exceeds `linearRows` (beyond last content), `countOutputRows` won't find it in the walk. Fallback: leave it at -1, then in reflow() set marker to -1 (invalidate). This is correct — if the prompt row was beyond content, it's meaningless after reflow anyway.
