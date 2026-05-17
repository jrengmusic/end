# PLAN: Viewport — Screen Self-Management via State Event Chain

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-05-17
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation — no overrides)

## Overview

Screen becomes self-managing — listens to State's DISPLAY ValueTree node, owns Grid reading, dirty row tracking, cell dimension updates, history drain, activeScreen/cursor sync. Display reduces to node graft, bounds setting, and Grid handoff. VBlank eliminated — all rendering driven by State's existing event→atomic→flush→VT callback chain.

## Architecture

### Event Chain (existing, no new pattern)

```
[reader thread]
Video mutates Grid
  -> events.get(ID::xxx, value)
    -> Processor lambda
      -> state.storeValue() / state.setXxx()
        -> Parameter<int>::store() [atomic]

[message thread, flush timer]
Parameter::flush()
  -> VT setProperty()
    -> valueTreePropertyChanged fires on listeners
      -> Screen reads State, copies dirty rows from Grid
```

### Dirty Row Signal

Video fires `ID::dirtyRow` event per row touched (print, scroll, erase). Processor routes to State. Screen reads exact row indices via State callback, copies only those rows from Grid via setText. Same event→atomic→flush→VT chain as historyRows.

### Screen Ownership

Screen (VT listener on State) owns:
- Grid reference — reads dirty rows and history from Grid directly
- Cell dimension updates — computes cellArea from getLocalBounds(), writes cols/visibleRows to DISPLAY node
- History drain — tracks previousHistoryRows, drains delta from Grid on historyRows change
- normalCount/normalHead — written to DISPLAY node on State (not shadow members)
- activeScreen sync — reads from State callback
- Cursor position sync — reads from State callback
- Resize debounce — lastCols/lastRows on DISPLAY node

### Display Reduction

Display keeps:
- DISPLAY node graft into State + cell metrics write (applyConfig)
- Grid ownership (passes reference to Screen)
- Screen bounds management (setBounds in resized)
- KeyListener forwarding

Display loses:
- valueTreePropertyChanged body (history drain, activeScreen, cursor — all move to Screen)
- onVBlank / VBlankAttachment — eliminated entirely
- updateDimensions — moves to Screen
- previousHistoryRows, lastCols, lastRows members
- Direct screen.setText/setActiveScreen/setCaretPosition calls

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: State — dirtyRows Bitset Registration
**Scope:** `Source/terminal/data/Identifier.h`, `Source/terminal/data/State.h`, `Source/terminal/data/State.cpp`
**Action:** Add `ID::dirtyRows` identifier. Add `State::markDirtyRow(int row)` (OR's bit into atomic bitset) and `State::consumeDirtyRows()` (reads and clears, returns bitset). Atomic bitset — Video OR's per-row bits on writer thread, Screen reads and clears on message thread. Register on DISPLAY group.
**Validation:** BLESSED S (SSOT — dirty signal on State, not stray atomic). NAMES (dirtyRows — noun, semantic, plural for bitset).

### Step 2: Video + Processor — dirtyRow Event Wiring
**Scope:** `Source/terminal/logic/Video.cpp`, `Source/terminal/logic/Processor.cpp`
**Action:** Video fires `events.get(ID::dirtyRow, activeScreen, cursorRow.value)` after each Grid write in `print()`. Video fires `events.get(ID::dirtyRow, ...)` for each row touched in `scrollUpAndFill`, `scrollDownAndFill`, erase operations. Processor registers `ID::dirtyRow` lambda routing to `state.setDirtyRow()`. Same pattern as `ID::historyRows`.
**Validation:** BLESSED E (Encapsulation — no new pattern, extends existing event chain). Every Grid mutation produces a State signal.

### Step 3: Screen — VT Listener + Grid Reference
**Scope:** `Source/terminal/rendering/Screen.h`, `Source/terminal/rendering/Screen.cpp`
**Action:** Screen adds `juce::ValueTree::Listener` inheritance. Constructor takes `Terminal::State&` and `Terminal::Grid&` references. Screen registers as listener on State's VT. `valueTreePropertyChanged` reads dirtyRow from State, copies that row from Grid via inherited setText. Reads activeScreen, cursor position from State — calls inherited setActiveScreen/setCaretPosition. History drain: reads historyRows delta, copies range from Grid. Resize: Screen::resized() computes cellArea from getLocalBounds(), writes cols/visibleRows to DISPLAY node via State.
**Validation:** BLESSED E (Encapsulation — Screen owns its rendering pipeline end-to-end). BLESSED S (SSOT — normalCount on DISPLAY node, not shadow member). BLESSED S (Stateless — Screen reads State, no internal tracking beyond what DISPLAY node holds).

### Step 4: Display — Strip Ownership
**Scope:** `Source/component/TerminalDisplay.h`, `Source/component/TerminalDisplay.cpp`
**Action:** Remove `onVBlank()` method and `VBlankAttachment` member. Remove `valueTreePropertyChanged` body (keep override if Display still listens for other reasons, otherwise remove Listener inheritance). Remove `previousHistoryRows`, `lastCols`, `lastRows` members. Remove `updateDimensions()`. Screen constructor now receives State and Grid refs — Display passes them. Display::resized() calls `screen.setBounds(contentBounds)` only — Screen handles dimension update internally via its own resized(). Remove `snapshotDirty`/`consumeSnapshotDirty` — no vblank consumer.
**Validation:** BLESSED L (Lean — Display stripped to essential responsibility). BLESSED E (Encapsulation — Display sets bounds, Screen self-manages).

### Step 5: normalCount on DISPLAY Node
**Scope:** `Source/terminal/rendering/Screen.cpp`, `Source/terminal/data/Identifier.h`
**Action:** Add `ID::normalCount` and `ID::normalHead` identifiers. Screen seeds DISPLAY node with normalCount/normalHead properties (0 default) — same pattern Display uses for cellWidth/cellHeight. Screen updates normalCount on DISPLAY node after each history drain setText. TextEditor::calc() already reads normalCount — once non-zero, ContentView grows, viewport scroll activates.
**Validation:** BLESSED S (SSOT — normalCount lives on State's DISPLAY node). calc() contract honored — normalCount drives content height.

## BLESSED Alignment

- **B (Bound):** Screen owns Grid ref for its lifetime. State is the coordination point — no stray atomics.
- **L (Lean):** VBlank eliminated. Display stripped. No duplicate tracking state.
- **E (Explicit):** Every Grid mutation fires a dirtyRow event. Screen reads exact row indices. No silent writes.
- **S (SSOT):** normalCount, lastCols, lastRows, previousHistoryRows all on DISPLAY node in State. Zero shadow state.
- **S (Stateless):** Screen is event-driven — reads State on callback, copies from Grid, no internal tracking.
- **E (Encapsulation):** Screen owns rendering pipeline. Display owns bounds and config. Clear separation.
- **D (Deterministic):** Same dirtyRow signal -> same Grid row copied -> same render. Event chain is deterministic.

## Risks / Open Questions

None. dirtyRows stored as atomic bitset on State — Video OR's bits per row touched, Screen reads and clears. No coalescing loss. Parameter is type-agnostic.
