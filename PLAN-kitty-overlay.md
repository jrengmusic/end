# PLAN: Kitty Overlay — Grid-Persistent Image

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-04-28
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (reference implementation)

## Overview

Move Kitty overlay storage from State (transient atomic/ValueTree params) to Grid (persistent struct). Overlay persists like glyphs — cleared only when `activeWriteCell` writes to the overlay's row range. Removes all flush-timing complexity.

## Validation Gate

Each step validated by @Auditor against: MANIFESTO.md (BLESSED), NAMES.md, JRENG-CODING-STANDARD.md, and this locked PLAN.

## Steps

### Step 1: Add Overlay struct to Grid::Buffer

**Scope:** `Grid.h`
**Action:**
- Add to `Grid::Buffer` (private struct, line ~839):
  ```cpp
  struct Overlay
  {
      uint32_t imageId { 0 };
      int row { 0 };
      int col { 0 };
      int rowSpan { 0 };
  };

  Overlay overlay;
  ```
- Add public methods on Grid:
  - `void setOverlay (uint32_t imageId, int row, int col, int rowSpan) noexcept` — sets `activeBuffer().overlay`
  - `const Overlay& getOverlay() const noexcept` — returns `activeBuffer().overlay`
  - `void clearOverlay() noexcept` — zeroes `activeBuffer().overlay.imageId`
- Add `setOverlay` / `clearOverlay` passthrough on `Grid::Writer`

**Validation:** Struct is POD, trivially copyable. No new pattern — follows existing Buffer member style. Name `Overlay` gated by ARCHITECT.

### Step 2: Clear overlay on cell write to overlay rows

**Scope:** `Grid.cpp` — `activeWriteCell`, `activeWriteRun`
**Action:**
- In `activeWriteCell(row, col, cell)`: after bounds check, before the cell write:
  ```cpp
  if (buffer.overlay.imageId != 0
      and row >= buffer.overlay.row
      and row < buffer.overlay.row + buffer.overlay.rowSpan)
  {
      buffer.overlay.imageId = 0;
  }
  ```
- Same check in `activeWriteRun(row, startCol, cells, count)`.
- ED/erase functions (`clearRow`, `eraseCellRange`, `eraseCell`) do NOT clear overlay — erasing underneath doesn't replace content.

**Validation:** Hot path — 4 integer comparisons, branch-predicted false. No allocation. No new function calls. Erase paths untouched.

### Step 3: Set overlay from Kitty APC

**Scope:** `ParserESC.cpp` — `apcEnd()`
**Action:**
- Replace `state.setOverlayImageId/Row/Col` calls with `writer.setOverlay(imageId, cursorRow, cursorCol, cellRows)`:
  ```cpp
  writer.setOverlay (imageId, cursorRow, cursorCol, cellRows);
  ```
- `cellRows` is already computed (line 1146): `(kittyResult.image.height + cellH - 1) / cellH`
- Keep `writer.storeDecodedImage(std::move(pending))` — image pixel data stays in Grid's `decodedImages` map.

**Validation:** One call replaces three. `rowSpan` enables row-range check in Step 2. No State dependency for overlay.

### Step 4: Clear overlay on screen switch

**Scope:** `ParserEdit.cpp` — `setScreen()`
**Action:**
- Replace `state.setOverlayImageId(0)` with `writer.clearOverlay()` at line 558.

**Validation:** Same behavior, different storage.

### Step 5: Renderer reads overlay from Grid

**Scope:** `ScreenSnapshot.cpp` — `updateSnapshot()`
**Action:**
- Replace the overlay block (lines ~155-196) that reads `state.getOverlayImageId/Row/Col` with reading `grid.getOverlay()`:
  ```cpp
  const auto& overlay { grid.getOverlay() };
  if (overlay.imageId != 0)
  {
      // atlas lookup + on-demand staging (same as current)
      // ImageQuad position from overlay.row/col * physCellWidth/Height
  }
  ```
- `updateSnapshot` already takes `const Grid&` — no signature change.

**Validation:** Reads Grid directly under resize lock (VBlank holds `ScopedTryLock`). Same thread safety as cell reads.

### Step 6: Remove overlay from State

**Scope:** `State.h`, `State.cpp`, `StateFlush.cpp`, `Identifier.h`
**Action:**
- Remove: `setOverlayImageId`, `setOverlayRow`, `setOverlayCol`, `getOverlayImageId`, `getOverlayRow`, `getOverlayCol`
- Remove: `addParam(state, ID::overlayImageId/overlayRow/overlayCol, 0.0f)`
- Remove: `state.setProperty(ID::prevFlushedOverlayId, 0.0f, nullptr)` from constructor
- Remove: staleness block in `flush()` (lines 177-191)
- Remove: identifiers `overlayImageId`, `overlayRow`, `overlayCol`, `prevFlushedOverlayId` from `Identifier.h`

**Validation:** No remaining references to overlay in State. grep confirms.

## BLESSED Alignment

- **B (Bound):** Overlay owned by Grid::Buffer. Lifecycle tied to buffer. RAII.
- **L (Lean):** Struct is 4 members. No helpers beyond set/get/clear.
- **E (Explicit):** Clear signal is `activeWriteCell` overlap — visible, deterministic.
- **S (SSOT):** Overlay lives in one place (Grid). No shadow state in State.
- **S (Stateless):** Grid is persistent data, not machinery tracking state for the caller.
- **E (Encapsulation):** Parser tells Grid (via Writer). Renderer reads Grid. Unidirectional.
- **D (Deterministic):** Same input always produces same overlay state.

## Follow-Up Sprint: Sixel/iTerm2 Unification

Once Kitty overlay works, migrate Sixel and iTerm2 to the same Grid overlay pipeline:
- Replace cell-based image rendering (ImageCell sidecar, cell flags, activeWriteImage) with Grid overlays
- All three protocols: decode RGBA → store in decodedImages → set overlay at cursor position
- Overlays use buffer-relative rows → scroll naturally with content
- Eliminate ImageCell struct, `activeWriteImage`, per-cell image tile rendering
- Single renderer path: draw visible overlays as quads

## Risks / Open Questions

1. **Name `Overlay`** — needs ARCHITECT approval per NAMES.md Rule -1.
2. **`rowSpan` computation** — currently `(height + cellH - 1) / cellH`. If cell dimensions change between set and clear, stale rowSpan could miss/over-clear. Low risk — resize resets grid anyway.
