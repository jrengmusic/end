# PLAN: Terminal Rendering Architecture Rewrite

**RFC:** RFC-texteditor-rewrite.md
**Date:** 2026-05-22
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides per LANGUAGE.md)

## Overview

Replace `Buffer<Row>` with `Buffer<Cell>`, delete tmux storage-mutating reflow (~470 lines), extract generic `jam::DiscreteStateTransition` from `GridSizeTransition`, simplify `TextEditor` to single viewport mode with juce::Viewport subclass. Three active regressions deleted with their cause. Net deletion ~500+ lines.

## Language / Framework Constraints

C++ / JUCE reference implementation. All BLESSED principles enforced as written. JUCE Viewport reentrant guard pattern adopted from `juce_TextEditor.cpp:164-199`.

## Validation Gate

Each step validated by @Auditor against:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (coding standards)
- Locked PLAN decisions (no deviation, no scope drift)

**Compilation groups noted.** Steps 1-3 form one compilation unit — validated together after Step 3.

---

## Steps

### Step 1: Delete Row + Transform Grid

**Scope:** `jam_fonts/cell/jam_row.h`, `Source/terminal/Grid.h`, `Source/terminal/Grid.cpp`

**Action:**

1. Delete `jam_fonts/cell/jam_row.h` entirely.

2. Grid.h changes:
   - Buffer type: `jam::Buffer<jam::Row>` -> `jam::Buffer<jam::Cell>`
   - `getWritePointer(int screen, cell row)` returns `jam::Cell*` (was `jam::Row*`)
   - `getBlock(int screen, cell scrollOffset, cell viewportRows)` returns `jam::Block<jam::Cell>` (was `Block<jam::Row>`)
   - Delete: `reflow()` (line 99), `reflowFrom()` (line 124), `getRow()` (line 214), `getBuffer()` (line 248)
   - Add: `resizeCols (cell newCols, cell scrollbackLines)`

3. Grid.cpp changes:
   - Delete all static reflow helpers: `wrapCursorPosition`, `unwrapCursorPosition`, `reflowDead`, `reflowMove`, `reflowJoin`, `reflowSplit`, `reflowScreen` (~470 lines)
   - Delete `reflow()`, `reflowFrom()`, `getRow()`, `getBuffer()` implementations
   - Update all remaining method bodies: `Row*` -> `Cell*` in returns, `buffer.clear` calls (stride changes automatically via Buffer template)
   - Implement `resizeCols()`: allocate new buffer at new stride, copy `min(oldCols, newCols)` cells per row for all live rows (history + viewport, both screens), pad with `Cell::erase()` on grow, update `ringMask`, reallocate `blockPointers`

**Validation:** Grid.h/cpp has no `jam::Row` references. No reflow methods. Buffer type is `Cell`. `resizeCols` implemented. (Does not compile alone — Row consumers remain.)

---

### Step 2: Video Mechanical Migration

**Scope:** `Source/terminal/Video.cpp`, `VideoEdit.cpp`, `VideoESC.cpp`, `VideoCSI.cpp`

**Action:**

Mechanical `Row*` -> `Cell*` at every `getWritePointer()` callsite. Pattern:

```
// Before
jam::Row* const row { grid.getWritePointer (scr, cell (writeRow)) };
row->cells[writeCol] = glyph;
row->usedCols = juce::jmax (row->usedCols, uint16_t (writeCol + charWidth));

// After
jam::Cell* const cells { grid.getWritePointer (scr, cell (writeRow)) };
cells[writeCol] = glyph;
// usedCols write deleted
```

Exhaustive callsites (from Pathfinder):

| File | Lines | Change |
|---|---|---|
| Video.cpp | 310,337 | scroll fill: `row->cells[col]` -> `cells[col]` |
| Video.cpp | 379-380 | resolveWrapPending: delete `row->flags \|= jam::Row::wrapped`. Keep cursor advance + scroll. |
| Video.cpp | 443 | grapheme cluster: `&row->cells[lastWriteCol]` -> `&cells[lastWriteCol]` |
| Video.cpp | 528-530,540 | print main write: `row->cells` -> `cells`, delete `usedCols` write |
| VideoCSI.cpp | 528,531 | scrollDown fill: `row->cells[c]` -> `cells[c]` |
| VideoESC.cpp | 249,252,254 | DECALN: `rowPtr->cells[col]` -> `cells[col]`, delete `usedCols` write |
| VideoEdit.cpp | 83-278 | eraseInDisplay/eraseInLine (12+ sites): `row->cells[c]` -> `cells[c]` |
| VideoEdit.cpp | 380-393 | shiftLines fill: same pattern |
| VideoEdit.cpp | 430-442 | shiftCellsRight: `row->cells` memmove -> `cells` memmove, delete `usedCols` write |
| VideoEdit.cpp | 469 | removeCells: `row->cells` memmove -> `cells` memmove |
| VideoEdit.cpp | 506-509 | eraseCells: `row->cells[c]` -> `cells[c]` |

**Validation:** No `jam::Row` references in any Video*.cpp file. No `usedCols` writes. No `flags` writes. (Does not compile alone — Block<Row> consumers remain.)

---

### Step 3: Type Cascade — Arrangement + TextEditor + Screen

**Scope:** `jam_glyph_arrangement.h`, `jam_glyph_arrangement.cpp`, `jam_text_editor.h`, `jam_text_editor_content_view.cpp`, `Source/terminal/component/Screen.cpp`

**Action:**

1. Arrangement (jam_glyph_arrangement.h):
   - Delete `Block<Row>` shape overloads: array (line 171) and single (line 178)
   - Keep `Block<Cell>` overloads unchanged (lines 133, 152)

2. Arrangement (jam_glyph_arrangement.cpp):
   - Delete `Block<Row>` extractor lambda and `shapeImpl` instantiation
   - Keep `Block<Cell>` path unchanged

3. TextEditor (jam_text_editor.h):
   - `jam::Block<jam::Row> content` -> `jam::Block<jam::Cell> content`
   - `setText (jam::Block<jam::Row>)` -> `setText (jam::Block<jam::Cell>)` (both overloads)

4. content_view (jam_text_editor_content_view.cpp):
   - Shape calls now use `Block<Cell>` overload (type change automatic from #3)

5. Screen.cpp:
   - `grid.getBlock()` returns `Block<Cell>` (from Step 1). `setText()` accepts `Block<Cell>` (from #3). Types align mechanically.

**Validation:** **[COMPILATION GATE]** Full project compiles. All `jam::Row` references eliminated across entire codebase. `Block<Cell>` flows from Grid -> Screen -> TextEditor -> Arrangement. `jam_row.h` file does not exist.

---

### Step 4: TextEditor Cleanup — Single Viewport Mode

**Scope:** `jam_text_editor.h`, `jam_text_editor.cpp`, `jam_text_editor_content_view.cpp`, `Source/terminal/component/Screen.h`, `Screen.cpp`

**Action:**

**TextEditor removals:**
- `ViewportMode` enum (h:96)
- `setViewportMode()` (cpp:133-161)
- `setScrollRange()` (cpp:163-178)
- `attach (juce::Value)` (cpp:180-184)
- `scrollOffsetId`, `scrollCapacityId` from property enum (h:26-27)
- `scrollbarVisibleId` from property enum (h:48)
- `viewportMode` member (h:142)
- `std::unique_ptr<jam::Scrollbar> scrollbar` member (h:140)
- Proportional branch in `calc()` (cpp:98-101)
- Proportional branch in `shapeVisibleContent()` (content_view:47-49)
- Scrollbar bounds reservation in `resized()` (cpp:78-79)
- `setCaretShape (int decscusr)` (h:69) — terminal VT vocabulary, moves to Screen

**TextEditor additions:**
- `TextEditorViewport` inner struct — `juce::Viewport` subclass with `bool reentrant` guard + `juce::ScopedValueSetter`. Adopted from `juce_TextEditor.cpp:164-199`. `visibleAreaChanged` override checks `getWrapWidth() != lastWrapWidth`, guards with reentrant flag, calls `owner.checkLayout()`.
- `getWrapWidth()` -> `viewport->getMaximumVisibleWidth()`
- `checkLayout()` — computes content height, sets ContentView size, juce::Viewport auto-manages scrollbar. Reentrant guard prevents oscillation.
- Constructor creates `TextEditorViewport` (replacing raw `juce::Viewport`)

**Screen adaptation:**
- Remove `setViewportMode (ViewportMode::proportional)` call (Screen.cpp constructor)
- Remove `setScrollRange (numRows)` call (Screen.cpp valueTreePropertyChanged)
- Remove `attach (scrollValue)` calls (Screen.cpp constructor + valueTreePropertyChanged)
- Add `setCaretShape (int decscusr)` method to Screen.h/cpp (moved from TextEditor — terminal DECSCUSR vocabulary)
- Compute wrapColumns via `jam::Cell::Rectangle (font.bounds, juce::Rectangle<int> { 0, 0, getWrapWidth(), 1 }).getWidth().value` — no manual division. Same pattern as Display.cpp:155.

**Validation:** TextEditor compiles with single viewport mode. No `ViewportMode` enum. No `jam::Scrollbar` usage in TextEditor. `TextEditorViewport` with reentrant guard present. Screen compiles without proportional API calls. `setCaretShape` on Screen. wrapColumns passed to arrangement.

---

### Step 5: Create jam::DiscreteStateTransition

**Scope:** NEW: `jam_gui/transition/jam_discrete_state_transition.h`, `jam_gui/transition/jam_discrete_state_transition.cpp`

**Action:**

Create `jam::DiscreteStateTransition` class per RFC Pillar 2 — generic SST for discrete state changes. Same TETRIS contract as `kuassa::dsp::SmoothStateTransition`.

Class structure:
- `class DiscreteStateTransition : private juce::Timer`
- `addTrigger<Args...> (const juce::Identifier& name, FunctionType&& callback)` — registered via `jam::Function::Map<juce::Identifier, void>`
- `set<Args...> (const juce::Identifier& name, Args... args)` — fires trigger via `applyChange`, or queues as pending if transitioning (coalescing: `pendingChanges.clear()` + add)
- `process() noexcept` — called from `timerCallback()`. Calls `advanceCrossfade()`. If still transitioning, fires tick trigger (if registered).
- `flush() noexcept` — immediate apply, no animation
- `prepare() noexcept` — cold start: `isReady = false`. First `applyChange` fires immediately without animation, sets `isReady = true`.
- `setCrossfadeTimeMs (double timeMs) noexcept` — configure timing, recalculate increment
- `setTickTrigger (const juce::Identifier& tickId)` — register which trigger fires per tick during transition (for repaint)
- `setSettledTrigger (const juce::Identifier& settledId)` — register which trigger fires on completion
- `isInTransition() const noexcept`, `getCrossfadePosition() const noexcept` — query state

Private:
- `applyChange<Args...> (name, args...)` — calls `triggers.get(name, args...)`. If `isReady`, starts transition (position=0, timer starts). If not ready, sets `isReady = true` (cold start).
- `advanceCrossfade() noexcept` — position += increment. On completion (>=1.0): stop timer, `isTransitioning = false`, fire settled trigger, drain pending.
- `updateCrossfadeIncrement() noexcept` — `increment = tickIntervalMs / transitionTimeMs`
- `timerCallback() override { process(); }`

Members:
- `jam::Function::Map<juce::Identifier, void> triggers`
- `jam::Function::Vector<void> pendingChanges`
- `bool isReady { false }`, `bool isTransitioning { false }`
- `double crossfadePosition { 1.0 }`, `double crossfadeIncrement { 0.0 }`, `double transitionTimeMs { 200.0 }`
- `juce::Identifier settledId`, `juce::Identifier tickId`
- `static constexpr int tickIntervalMs { 16 }`

**Validation:** DST compiles. No domain-specific references (no Grid, Video, terminal types). Generic trigger registration via `Function::Map`. Same SST lifecycle as `SmoothStateTransition`: prepare -> set -> snapshot+mutate -> crossfade -> settle -> drain pending. Coalescing (latest wins).

---

### Step 6: Delete GridSizeTransition + Wire Screen/Processor

**Scope:** `Source/terminal/GridSizeTransition.h` (DELETE), `GridSizeTransition.cpp` (DELETE), `Source/terminal/component/Screen.h`, `Screen.cpp`, `Source/terminal/Processor.h`, `Processor.cpp`, `Source/terminal/Identifier.h`

**Action:**

**Delete GridSizeTransition:**
- Delete `Source/terminal/GridSizeTransition.h`
- Delete `Source/terminal/GridSizeTransition.cpp`

**CONTRACT CONSTRAINTS (Step 6):**
- State is SSOT for all dimensions, cursor, scroll. No shadow state on Screen.
- READER → atomics. MESSAGE → State set/getValue. Unidirectional, lock-free.
- Tick repaint flows through State (`id::snapshotDirty`) → `valueTreePropertyChanged` → repaint. No direct `repaint()` from callbacks.
- Interpolation: `jam::Value::map (crossfadePosition, previousCols, targetCols)`. No manual arithmetic.
- Previous cols/rows derived from snapshot metadata — no separate `startCols`/`startRows` members.
- Follow existing patterns verbatim. `jam::Function::Map` for triggers (SST pattern). ValueTree reactive for repaint.

**Processor — remove GridSizeTransition:**
- Delete `GridSizeTransition gridResize` member (Processor.h:351)
- Delete `gridResize` construction from constructor (Processor.cpp:40)
- Delete `gridResize.prepare(scrollbackLines)`, `gridResize.allocate(cols, rows)` from constructor
- Delete dimension-change handling from `valueTreePropertyChanged` (Processor.cpp:617-642): `gridResize.set()`, `gridResize.allocate()`, `gridResize.setCellSize()` — all gone
- Keep `id::resizeEnd` event handler registration for SIGWINCH delivery (`tty->platformResize()`)
- Make `events` member public (not a getter — same access pattern as Video/Skit/GridSizeTransition)

**Screen — add DST ownership (Screen.h):**
- `jam::DiscreteStateTransition transitioner` member
- `jam::Buffer<jam::Cell> previous` snapshot buffer — SST `previous` state. State preserverance: lossless, non-negotiable.
- Snapshot metadata: `std::array<int, 2> previousHead { 0, 0 }`, `std::array<int, 2> previousNumRows { 0, 0 }`, `int previousRingMask { 0 }`, `int previousViewportRows { 0 }`, `int previousCols { 0 }`
- No `startCols`/`startRows` — previous dimensions derived from snapshot metadata (`previousCols`, `previousViewportRows`)
- `void captureSnapshot()` — private method
- `jam::Function::Map<juce::Identifier, void>& events` reference member (public on Processor)

**Screen — constructor (Screen.cpp):**
- Accept `jam::Function::Map& events` parameter (Display passes from `processor.events`)
- Register DST triggers:
  - `id::resize` trigger `<cell, cell>`: `captureSnapshot()` (previous = current, captures previousCols/previousViewportRows) -> `grid.resizeHeight(rows, cursorRow)` -> `grid.resizeCols(cols, scrollbackLines)` -> cursor clamp via `state.storeValue` -> numRows sync via `state.setValue` for both screens -> `scrollOffset` reset to 0 -> write `id::snapshotDirty` to State (triggers repaint via reactive path)
  - `id::resizeEnd` trigger `<>`: calls `events.get(id::resizeEnd)` for SIGWINCH delivery
- `transitioner.setSettledTrigger(id::resizeEnd)`
- `transitioner.prepare()` — cold start

**DST tick repaint — unidirectional reactive path:**
- DST `process()` (timer, 16ms, message thread) → advances crossfade → writes `id::snapshotDirty` to State (message thread direct VT write)
- State `valueTreePropertyChanged` fires on Screen → Screen detects `transitioner.isInTransition()` → renders from `previous` at interpolated wrapColumns → repaint
- No direct `repaint()` calls from DST. No manual lambda callbacks for repaint. Standard reactive ValueTree path.

**Screen — captureSnapshot (SST contract: previous = current):**
- Captures `previousCols` from Grid's current logical column count (read from State: `id::cols`)
- Same ring metadata logic as GridSizeTransition.cpp:213-233: `previous.setSize(2, ringSize, previousCols, false, true, false)`, copy physical rows via `previous.copyFrom` per screen
- Lossless cell-level memcpy. Content at old dimensions preserved in `previous` for the entire transition duration.

**Screen — render path during transition (analogous to SST crossfade):**
- Audio SST: `previous.process(sample)` + `current.process(sample)` + blend
- Terminal DST: shape `previous` at interpolated wrapColumns. Dimension interpolation IS the crossfade.
- Interpolation: `int interpolatedCols { jam::Value::map (crossfadePosition, previousCols, targetCols) }` where `targetCols` read from State SSOT (`id::cols`)
- Build `Block<Cell>` from `previous` buffer using snapshot ring metadata (previousHead, previousNumRows, previousRingMask)
- `Arrangement::shape (previousBlock, font, interpolatedCols, lineOffset)`
- After settle: shell has received SIGWINCH, redraws at target dims → live grid updated → Screen renders from live grid

**Screen — rewrite valueTreePropertyChanged:**
- Dimension change (`id::cols` or `id::visibleRows`): `transitioner.set(id::resize, cols, rows)` → return
- Cell size change (`id::cellWidth` or `id::cellHeight`): propagate through DST if coalescing needed, or direct
- `id::snapshotDirty` change while `transitioner.isInTransition()`: render from `previous` at interpolated cols via `jam::Value::map`, repaint
- `id::snapshotDirty` change while not transitioning: read live grid, `setText(Block<Cell>)`, `setCaretPosition`, repaint
- Normal flush (non-dimension, non-snapshot): read live grid, `setText(Block<Cell>)`, `setCaretPosition`, repaint

**Display — pass events to Screen:**
- Screen constructor gains `events&` parameter
- Display passes `processor.events` to Screen construction (public member, no getter)

**Identifier.h:**
- Add `id::resize` if not present (DST trigger name for dimension change)
- Keep `id::resizeEnd` (repurposed: DST settled trigger for SIGWINCH)
- Keep `id::snapshotDirty` (existing — repaint signal during transition, written by DST process())

**Validation:** **[COMPILATION GATE]** Full project compiles and runs. No `GridSizeTransition` references anywhere. DST owned by Screen. Resize lifecycle: Display::resized() → State dimensions → Screen::valueTreePropertyChanged → transitioner.set(id::resize) → registered trigger fires (snapshot → grid mutation → state sync → snapshotDirty) → crossfade → per-tick snapshotDirty → reactive repaint from previous at interpolated cols → settle → resizeEnd → SIGWINCH. No shadow state. No manual arithmetic. No direct repaint calls. Unidirectional data flow through State.

---

## BLESSED Alignment

- **B (Bound):** `previous` owned by Screen for transition lifetime. Grid owned by Session. DST owns no domain state — all in registered triggers. Lifetimes explicit.
- **L (Lean):** ~500+ lines deleted (reflow helpers, Row, proportional mode, GridSizeTransition hardwiring). DST ~60 lines generic. Net deletion.
- **E (Explicit):** `Arrangement::shape(block, font, wrapColumns, lineOffset)` — all params visible. Wrap columns derived from `Cell::Rectangle(font.bounds, pixelRect).getWidth()` — no manual arithmetic. DST triggers registered with named Identifiers and typed args. State reads via `jam::ValueTree::getValueFromChildWithID` — existing battle-tested API.
- **S (SSOT):** scrollOffset in State. Cursor in State. Dimensions in State. Viewport position derived.
- **S (Stateless):** Arrangement produces glyph positions fresh each frame. No reflow state. Grid is dumb ring. DST is a state machine with no domain knowledge.
- **E (Encapsulation):** Grid: ring storage. Arrangement: shaping. Screen: render + resize coord. DST: transition lifecycle. Video: VT. Display: dimensions. One job each.
- **D (Deterministic):** Same `Block<Cell>` + same `wrapColumns` -> same glyph positions. No tmux history interaction. No ring-full edge cases.

## Resolved Questions

1. **events& wiring:** `Processor::events` made public. Same pattern as Video/Skit/GridSizeTransition — all receive reference at construction. Display passes `processor.events` to Screen constructor.
2. **wrapColumns source:** `font.bounds.width` — protected member of TextEditor, already accessed by content_view (content_view.cpp:42). Screen inherits TextEditor.
3. **Snapshot during transition:** Dimension interpolation IS the crossfade. `jam::Value::map(crossfadePosition, previousCols, targetCols)` → shape `previous` at interpolated cols. 1:1 analogy with audio SST `blend(outPrev, outCurr, pos)`.
4. **No shadow state:** Previous cols/rows derived from snapshot metadata (`previousCols`, `previousViewportRows`). Target cols from State SSOT (`id::cols`). No `startCols`/`startRows` members.
5. **Tick repaint:** Unidirectional through State (`id::snapshotDirty`) → `valueTreePropertyChanged` → repaint. No direct repaint() calls.

## Risks / Open Questions

1. **setCaretShape consumers:** Verify no jam-level consumers call `TextEditor::setCaretShape()` before removing. If found, keep on TextEditor and add override on Screen.
2. **Block<Cell> from previous snapshot:** Screen must build `Block<Cell>` from `previous` buffer using snapshot ring metadata (previousHead, previousNumRows, previousRingMask). Verify `blockPointers` allocation for snapshot or use a local array.
