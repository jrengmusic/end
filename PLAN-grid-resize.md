# PLAN: GridResize — Coalesced Resize Lifecycle

**RFC:** none — objective from ARCHITECT session
**Date:** 2026-05-19
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)
**Reference:** kuassa::dsp::SmoothStateTransition (structural analog), TETRIS.md

## Overview

Dedicated `GridResize` class manages the resize lifecycle: coalesces rapid dimension changes, reflows Grid content, triggers SIGWINCH. Processor delegates resize handling to GridResize. `process()` becomes pure — bytes only. Removes `callbackLock`, `suspended`, `suspendProcessing`, `displayReady`, cellSize detection from Processor.

## Steps

### Step 1: GridResize class

**Scope:** `Source/terminal/logic/GridResize.h`, `Source/terminal/logic/GridResize.cpp` (new files)
**Action:**

GridResize inherits `juce::Timer` (coalesce mechanism). Owns pending dimensions. When timer fires (dimensions stable), calls `prepare()` sequence.

```
GridResize
├── pendingCols, pendingRows (cell)
├── pendingCellWidth, pendingCellHeight (int)
├── hasPending (bool)
├── coalesceMs (static constexpr int, ~50ms)
├── References: Grid&, Video&, State&, TTY* (nullable)
│
├── set (cell cols, cell rows) — stores pending, starts timer
├── setCellSize (int w, int h) — stores pending cell size
├── setTTY (TTY*) — wires TTY for SIGWINCH
├── timerCallback() — applies pending: reflow Grid, resize Video, SIGWINCH TTY
└── apply() — the actual prepare sequence (extracted from Processor::prepare)
```

`apply()` contains the logic currently in `Processor::prepare()`:
- If Grid allocated: reflow, setNumRows, sync State
- Else: setSize (first allocation)
- setDimensions + resize on Video
- platformResize on TTY

`set()` stores pending and (re)starts the timer. Multiple rapid calls coalesce — only the last dimensions are applied when the timer fires.

`setCellSize()` stores pending cell dimensions. Applied in `apply()` or immediately if no resize pending.

### Step 2: Processor — delegate to GridResize

**Scope:** `Source/terminal/logic/Processor.h`, `Source/terminal/logic/Processor.cpp`
**Action:**
- Add `GridResize gridResize;` member (constructed with Grid&, Video&, State&)
- Remove: `callbackLock`, `suspended`, `suspendProcessing()`, `isSuspended()`, `displayReady`, `scrollbackLines`, `prepare()`
- `process()` becomes pure:
  ```
  void process (const char* data, int length) noexcept
  {
      jassert (parser != nullptr);
      if (video.getCols().value > 0 and video.getVisibleRows().value > 0)
      {
          parser->process (data, length);
          video.flush();
          video.flushResponses();
      }
      state.consumePasteEcho (length);
  }
  ```
  No lock. No suspended check. No cellSize detection. No displayReady check. Pure.
- `valueTreePropertyChanged`: dimension change → `gridResize.set(cols, rows)`. cellSize change → `gridResize.setCellSize(w, h)`. One-liner setters.
- Constructor: initial `gridResize.apply()` instead of `prepare()`.
- `setTTY()`: forwards to `gridResize.setTTY(tty.get())`.

### Step 3: Remove stale Processor API

**Scope:** `Source/terminal/logic/Processor.h`
**Action:**
- Remove `prepare()` from public API
- Remove `suspendProcessing()`, `isSuspended()`
- Remove `callbackLock`, `suspended`, `displayReady` from private
- Keep `scrollbackLines` on GridResize, not Processor

### Step 4: Audit

**Scope:** All modified/new files
**Action:** @Auditor validates against all contracts.

## BLESSED Alignment

- **B (Bound):** GridResize owns resize lifecycle. Timer is RAII (Timer::stopTimer in destructor).
- **L (Lean):** GridResize ~60 lines. process() ~10 lines. No god objects.
- **E (Explicit):** `set()`, `setCellSize()`, `apply()` — semantic names. No magic.
- **S (SSOT):** Pending dimensions live on GridResize only. No shadow state.
- **S (Stateless):** Processor pure. GridResize holds only transient pending state.
- **E (Encapsulation):** Processor delegates resize. GridResize encapsulates the lifecycle.
- **D (Deterministic):** Same dimensions → same reflow result.

## Risks / Open Questions

None — all decisions resolved with ARCHITECT.
