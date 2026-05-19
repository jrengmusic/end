# PLAN: Resize Data Flow — APVTS-Conformant

**RFC:** none — objective from ARCHITECT session
**Date:** 2026-05-19
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)
**Reference:** Kuassa jreng-filter-strip APVTS pattern, JUCE AudioProcessor::suspendProcessing

## Overview

Fix resize data flow to match APVTS pattern. Display writes State only (via `setValue`). Processor detects dimension change in `valueTreePropertyChanged`, brackets `prepare()` with `suspendProcessing`. Events made private — Session uses Processor API.

## Steps

### Step 1: Processor — add suspendProcessing (1:1 AudioProcessor analog)

**Scope:** `Source/terminal/logic/Processor.h`, `Source/terminal/logic/Processor.cpp`
**Action:**
- Private: `juce::CriticalSection callbackLock;` + `bool suspended { false };`
- Public: `void suspendProcessing (bool shouldBeSuspended) noexcept` — acquires callbackLock, sets suspended.
- Public: `bool isSuspended() const noexcept` — returns suspended.
- `process()`: acquire `ScopedLock sl (callbackLock)` at top, check `isSuspended()`, skip parser/video/flush if suspended.

### Step 2: Add pixel dimension parameters to State

**Scope:** `Source/terminal/data/Parameters.xml`, `Source/terminal/data/Identifier.h`, `Source/terminal/data/State.h`, `Source/terminal/data/State.cpp`
**Action:**
- Parameters.xml: add `pixelWidth` (int, 0) and `pixelHeight` (int, 0) to root STATE group.
- Identifier.h: add `ID::pixelWidth`, `ID::pixelHeight`.
- State.h/cpp: add `loadPixelWidth()`, `loadPixelHeight()` — atomic loaders matching existing `loadCellWidth` pattern.

### Step 3: Display writes State only — remove all direct Processor event fires

**Scope:** `Source/component/TerminalDisplay.cpp`
**Action:**
- `updateDimensions()`: remove `processor.events.get(ID::terminalResize, ...)`. Keep `state.setValue(ID::cols, ...)` and `state.setValue(ID::visibleRows, ...)`. Add `state.setValue(ID::pixelWidth, contentBounds.getWidth())` and `state.setValue(ID::pixelHeight, contentBounds.getHeight())`.
- `writeToPty()`: change `processor.events.get(ID::writeInput, ...)` → `processor.writeInput(data, len)` (new Processor API).
- `keyPressed()`: change `processor.events.get(ID::writeInput, ...)` → `processor.writeInput(encoded.toRawUTF8(), len)`.

### Step 4: Processor API — writeInput, events private

**Scope:** `Source/terminal/logic/Processor.h`, `Source/terminal/logic/Processor.cpp`
**Action:**
- Move `events` from public to private.
- Add public `void writeInput (const char* data, int length) noexcept` — forwards to `events.get(ID::writeInput, data, length)` if registered.
- Session already calls `events.add<>()` in its constructor — Session needs a registration API instead. Add `void registerWriteInputHandler (std::function<void(const char*, int)> handler) noexcept` and `void registerResizeHandler (std::function<void(int, int, int, int)> handler) noexcept`.
- Session::open() calls these instead of touching events directly.

### Step 5: Processor detects resize in valueTreePropertyChanged

**Scope:** `Source/terminal/logic/Processor.cpp`
**Action:**
- In `valueTreePropertyChanged`: detect `cols` or `visibleRows` change (compare against current Video dimensions via `video.getCols()` / `video.getVisibleRows()`).
- When changed: `suspendProcessing(true)` → `prepare(newRows, newCols, scrollbackLines)` → `suspendProcessing(false)`.
- Fire SIGWINCH: read `pixelWidth`/`pixelHeight` from State, call resize handler (registered by Session in Step 4).
- Remove `terminalResize` event registration from Session — Processor handles resize internally.

### Step 6: Audit

**Scope:** All files modified in Steps 1-5
**Action:** @Auditor validates against all contracts.

## BLESSED Alignment

- **E (Encapsulation):** Display tells State, Processor reacts. No layer violations.
- **S (SSOT):** State is sole channel. No shadow state, no direct events.
- **B (Bound):** callbackLock owns suspend lifecycle. CriticalSection is RAII.
- **D (Deterministic):** suspendProcessing ensures mutual exclusion — no race between prepare and process.
