# PLAN: jam::TextEditor Refactor — Content Ownership to Borrowed Block<Row>

**RFC:** RFC-text-editor-refactor.md
**Date:** 2026-05-20
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides — reference implementation)

## Execution Discipline

**Delete first. Implement second. Compiler is the checklist.**

Phase 1 deletes all old code. The codebase will not compile. That's expected — compiler errors are the exact list of what needs implementing. Phase 2 implements the new code. When the compiler is clean, the plan is done.

No fallback. No legacy. No backward compatibility. No coexistence of old and new paths.

## Overview

Remove `jam::Buffer<jam::Cell>` ownership from `jam::TextEditor`. TextEditor becomes a stateless renderer over borrowed `Block<Row>`, synchronized via its own `juce::ValueTree` node grafted into the caller's State tree. Selection is TextEditor's responsibility — selection identifiers migrate from `terminal::id` on State to TextEditor's grafted node. Input/Mouse write selection directly to TextEditor's node.

Two viewport modes: proportional (terminal — viewport == visible rows, interactive scrollbar) and absolute (whelmed — real juce::Viewport). Both modes render scrollbar identically via Viewport LookAndFeel — SSOT for scrollbar rendering and UX regardless of mode. Scrollbar metrics are user-config driven.

Grid exposes `Block<Row>` via `getBlock()` — Grid owns the pointer array. `glyph::Arrangement` gains a `Block<Row>` overload that stops shaping at `usedCols` per row. Selection anchors on TextEditor's node are adjusted by Processor on scroll via the established event/atomic pattern.

Grid's viewport rendering and scrolling work. Wrapping and reflow are broken. This refactor establishes the rendering foundation before fixing reflow.

## Language / Framework Constraints

C++/JUCE reference implementation. All BLESSED principles enforced as written. No LANGUAGE.md overrides.

## Constraints (RFC Handoff Notes)

- All event wiring uses `jam::Function::Map<juce::Identifier, void>` — no manual lambdas, no manual callbacks outside the established pattern.
- `registerNodeAtomics()` requires properties pre-seeded on the node before `appendChild()`. TextEditor constructor seeds all properties with defaults.
- Existing `shape(Block<Cell>)` stays for any consumer without Row context — the Row overload is additive.
- TextEditor's existing TETRIS contract (`calc()` on every setter) is the foundation — this refactor extends it, does not change it.
- Selection anchor adjustment co-locates in existing `id::scrollUp` handler — same place as `numRows` update. Atomic, same transaction.

## Data Flow (RFC §6)

```
READER thread:
  Video -> Grid cells + State atomics (screenDirty counter++)
  Video::scrollUpAndFill() -> events.get(id::scrollUp, screen, count)
    -> Processor handler: adjusts numRows, adjusts selection anchors
       via storeValue() into TextEditor's grafted node

Timer flush (60/120 Hz):
  jam::ValueTree::flush() -> dirty atomics -> ValueTree properties
    -> TextEditor's node properties updated

MESSAGE thread:
  Screen::valueTreePropertyChanged()
    -> reads Grid -> calls textEditor.setText(Block<Row>)
  TextEditor::valueTreePropertyChanged() (on own node)
    -> calc() -> selection geometry, scroll indicator -> repaint()
  ContentView::paint()
    -> reads stored Block<Row> -> arrangement.shape(Block<Row>) -> drawGlyphRuns()
```

No VBlank in the codebase — render trigger is timer-driven flush only.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Phase 1: DELETE — Break Everything

Codebase will not compile after this phase. Compiler errors are the implementation checklist.

### Step 1: Delete TextEditor old storage and API

**Scope:** `~/Documents/Poems/dev/jam/jam_gui/text_editor/jam_text_editor.h` + `.cpp` + `_content_view.cpp`

- `jam::Owner<jam::Buffer<jam::Cell>> screen` — the entire owned buffer storage. Gone.
- `int activeScreen` member
- `int filledRows` member
- `void setText (const jam::Block<jam::Cell>&, juce::Range<int>) noexcept` — old Cell-based copy path
- `void setActiveScreen (int) noexcept`
- `void clear() noexcept`
- `void setScrollbackLines (int)`
- `mouseWheelMove` override
- Buffer allocation in `resized()` — `screen.at(0)->setSize(...)`, `screen.at(1)->setSize(...)`. All of it.
- `ContentView::shapeVisibleContent()` old path — `Block<Cell>(*owner.screen.at(N), 0)` construction and `activeScreen == 1` branch. Entire old shaping path.
- Dead `#include` for `jam::Buffer` or `jam::Owner`

### Step 2: Delete drag transient state from State machine

**Scope:** `Source/terminal/Identifier.h` + `Source/terminal/Mouse.cpp` + `Source/terminal/State.h` + `Source/terminal/State.cpp`

- `terminal::id::dragAnchorRow` — from Identifier.h
- `terminal::id::dragAnchorCol` — from Identifier.h
- `terminal::id::dragActive` — from Identifier.h
- Drag parameter registrations from State
- All `state.storeValue(id::dragAnchorRow/Col/Active, ...)` calls in Mouse.cpp
- All `state.loadValue(id::dragAnchorRow/Col/Active)` calls in Mouse.cpp

### Step 3: Delete selection identifiers from terminal::id and State

**Scope:** `Source/terminal/Identifier.h` + `Source/terminal/Input.cpp` + `Source/terminal/Mouse.cpp` + `Source/terminal/State.h` + `Source/terminal/State.cpp` + `Source/terminal/component/ScreenSelection.h`

- `terminal::id::selectionAnchorRow` — from Identifier.h
- `terminal::id::selectionAnchorCol` — from Identifier.h
- `terminal::id::selectionCursorRow` — from Identifier.h
- `terminal::id::selectionCursorCol` — from Identifier.h
- `terminal::id::selectionType` (if on State) — from Identifier.h
- Selection parameter registrations on State
- All `state.storeValue(id::selectionAnchorRow/Col, ...)` calls in Input.cpp and Mouse.cpp
- All `state.loadValue(id::selectionAnchorRow/Col, ...)` calls in ScreenSelection.h and anywhere else
- Any dead code in State that only existed for selection identifiers

### Step 4: Delete old Screen rendering path

**Scope:** `Source/terminal/component/Screen.h` + `.cpp`

- Per-row `setText (Block<Cell>, Range)` loop in `valueTreePropertyChanged` — the entire row-by-row copy
- `setActiveScreen (activeScreenIndex)` call
- `setCaretPosition` direct setter call
- `repaint()` call
- `mouseWheelMove` override
- `jam::TextEditor ({}, 2)` constructor call — numScreens parameter
- `Block<Cell>` construction from `r->cells` pointer — old cell extraction path
- Dead `#include`

### Step 5: Delete stale ARCHITECTURE.md content

**Scope:** `ARCHITECTURE.md`

- All VBlankAttachment references
- CVDisplayLink references
- "onVBlank" references
- Key Design Decision "VBlankAttachment for Render Trigger" section
- Any description of Screen copying cells from Grid into TextEditor's buffer
- Any description of TextEditor owning Buffer<Cell> or dual-screen management
- Stale comments referencing selection on State

### Step 6: Delete old selection rendering coupling

**Scope:** All files touching selection rendering.

- Any selection rendering code that reads from TextEditor's old Buffer<Cell>
- Any `processCellForSnapshot` or similar path that constructs selection overlay from owned buffer data
- Any stale comments referencing old selection-on-State or onVBlank selection construction

---

## Phase 2: IMPLEMENT — Resolve Every Compiler Error

Compiler errors from Phase 1 are the checklist. When the compiler is clean, the plan is done.

### Step 7: Grid — getBlock() for viewport and history

**Scope:** `Source/terminal/Grid.h` + `Grid.cpp`

- Add `mutable juce::HeapBlock<const jam::Row*> blockPointers` member — viewport-sized logical-order pointer array. Grid owns it. Resized in `setSize()`.
- Add `jam::Block<jam::Row> getBlock (int screen, int scrollOffset, int viewportRows) const noexcept`:
  - `scrollOffset == 0` (live): fills `blockPointers` from `buffer.getReadPointer (screen, physicalRow (screen, r))` for each viewport row.
  - `scrollOffset > 0` (history): fills `blockPointers` from history rows via same ring arithmetic as existing `getRow()`.
  - Returns `jam::Block<jam::Row> (blockPointers.getData(), viewportRows, getCols())` using Block's raw-pointer constructor (jam_block.h L82 — designed for ring-buffer logical mapping).
- Grid owns the pointer array. Block borrows from Grid. Lifetime: Grid > Screen > TextEditor. All MESSAGE thread.
- Internal ring arithmetic (`head`, `physicalRow`, `ringMask`) unchanged — reader thread hot path unaffected.
- O(viewportRows) per call. Called once per frame.

### Step 8: glyph::Arrangement — Block<Row> shape overload

**Scope:** `~/Documents/Poems/dev/jam/jam_fonts/jam_font/glyph/jam_glyph_arrangement.h` + `.cpp`

- Add overload: `void shape (const jam::Block<jam::Row>* blocks, int numBlocks, const jam::Font& font, int wrapColumns, int lineOffset = 0) noexcept`
- Add convenience: `void shape (const jam::Block<jam::Row>& block, const jam::Font& font, int wrapColumns, int lineOffset = 0) noexcept`
- Row overload iterates rows, reads `row->usedCols` to stop shaping early. Accesses cell data via `row->cells`.
- Convenience delegates to array overload (same pattern as existing Cell overloads).
- ContentView calls the Row overload directly — no adapter, no pointer extraction.

### Step 9: jam::Scrollbar — interactive, LookAndFeel-driven

**Scope:** New file `~/Documents/Poems/dev/jam/jam_gui/text_editor/jam_scrollbar.h` + `.cpp`

- New component: `jam::Scrollbar` — interactive scroll indicator.
- API: `void setPosition (int offset, int capacity) noexcept` — thumb at offset/capacity ratio.
- Interactive: drag thumb to scroll, click track to jump. Fires callback with new offset.
- Renders via `juce::LookAndFeel::drawScrollbar()` — same LookAndFeel as `juce::Viewport`'s scrollbar. SSOT: proportional or absolute mode, scrollbar looks and behaves identically.
- Metrics (thickness, colours) driven by user config via LookAndFeel — not hardcoded.
- TextEditor creates lazily when ViewportMode::proportional. Right edge in `resized()`. Hidden in absolute mode.

### Step 10: TextEditor — ValueTree node + ViewportMode + setText(Block<Row>)

**Scope:** `~/Documents/Poems/dev/jam/jam_gui/text_editor/jam_text_editor.h` + `.cpp` + `_content_view.cpp`

**ValueTree node (RFC §1).** TextEditor owns a `juce::ValueTree` node. Properties seeded at construction:

```
TEXT_EDITOR (node type)
  viewportMode         (int — 0=proportional, 1=absolute)
  scrollOffset         (int — proportional mode: caller-supplied)
  scrollCapacity       (int — proportional mode: caller-supplied)
  selectionAnchorRow   (int — block-local)
  selectionAnchorCol   (int — block-local)
  selectionCursorRow   (int — block-local)
  selectionCursorCol   (int — block-local)
  selectionType        (int — none/visual/visualLine/visualBlock)
  caretRow             (int — block-local)
  caretCol             (int — block-local)
  contentDirty         (int — monotonic counter, same pattern as screenDirty)
```

TextEditor is `juce::ValueTree::Listener` on its own node. Property changes -> `calc()` -> geometry recomputation -> `repaint()`. Caller grafts node into State tree via `getNode()`. Plugs into established APVTS flush cycle.

**Selection is TextEditor's responsibility.** Selection identifiers live on TextEditor's node. Input/Mouse write directly via `storeValue()`. No translation layer.

**New API:**
```cpp
enum class ViewportMode { proportional, absolute };

void setText (jam::Block<jam::Row> block) noexcept;                              // full dirty
void setText (jam::Block<jam::Row> block, juce::Range<int> dirtyRows) noexcept;  // partial dirty
void setViewportMode (ViewportMode mode) noexcept;
void setScrollPosition (int offset, int capacity) noexcept;
juce::ValueTree& getNode() noexcept;
```

- First `setText` delegates to second with `{0, block.getNumRows()}`
- `Block<Row> content` stored as member (trivially copyable, non-owning)
- `setText()` stores block, calls `calc()`. `calc()` invalidates glyph cache for dirty rows.
- `setScrollPosition()` forwards to jam::Scrollbar (proportional), no-op in absolute
- `getNode()` returns owned ValueTree node — caller grafts into State
- `resized()` — computes `cols`/`visibleRows`, clears stored content (empty Block), calls `calc()`. No buffer allocation.
- All setters follow TETRIS: check if changed -> store -> `calc()`.
- Constructor seeds all properties with defaults before any `appendChild()`.

**`calc()`:**
- Content height from `content.getNumRows() * cellHeight`
- **proportional:** ContentView sized to component bounds. jam::Scrollbar active.
- **absolute:** ContentView height = total rows * cellHeight. juce::Viewport wraps ContentView.

**`ContentView::shapeVisibleContent()`:**
- Read from `owner.content` (Block<Row>)
- Sub-region via Block's startRow arithmetic for clip rect
- Call `arrangement.shape (Block<Row>)` directly

### Step 11: Screen — graft TextEditor node + setText(Block<Row>)

**Scope:** `Source/terminal/component/Screen.h` + `.cpp`

**Constructor (RFC §8):**
```cpp
Screen::Screen (terminal::State& state, terminal::Grid& grid) noexcept
    : state (state), grid (grid)
{
    state.get().appendChild (getNode(), nullptr);  // graft -> registerNodeAtomics
    setViewportMode (ViewportMode::proportional);
}
```

Grafts TextEditor's node into State's SESSION root. `registerNodeAtomics()` fires via `valueTreeChildAdded`.

**`valueTreePropertyChanged` (RFC §6):**
- Read active screen, scroll offset, viewport dimensions from State
- `grid.getBlock (activeScreenIndex, scrollOffset, viewportRows.value)` — Block<Row> from Grid
- `setText (block)` — one Block, no per-row loop
- `setScrollPosition (scrollOffset, numRows)` — proportional scrollbar
- Caret via ValueTree properties on TextEditor's node

### Step 12: Mouse — drag as private members

**Scope:** `Source/terminal/Mouse.h` + `Source/terminal/Mouse.cpp`

- Add private members: `juce::Point<int> dragAnchor`, `bool dragActive { false }`
- handleDown/handleDrag/handleUp use private members for drag mechanics
- Write selection result to TextEditor's grafted node via `storeValue()` (replacing deleted State path)

### Step 13: Selection migration — Input/Mouse/ScreenSelection to TextEditor node

**Scope:** `Source/terminal/Input.cpp` + `Source/terminal/Mouse.cpp` + `Source/terminal/component/ScreenSelection.h`

- Input::handleSelectionKey writes selection to TextEditor's grafted node via `storeValue()`
- Mouse::handleDown/Drag/Up writes selection result to TextEditor's grafted node
- ScreenSelection reads from TextEditor's grafted node properties
- Same atomic read/write pattern, different ValueTree location

### Step 14: Selection anchor adjustment on scroll (RFC §7)

**Scope:** `Source/terminal/Processor.cpp` + `Source/terminal/State.h` + `State.cpp`

Processor's `id::scrollUp` handler gains selection anchor adjustment:

```cpp
events.add<int, int> (id::scrollUp, [this] (int screen, int count)
{
    numRows.at (screen) = juce::jmin (numRows.at (screen) + count, scrollbackLines);
    grid.setNumRows (screen, numRows.at (screen));
    state.setNumRows (screen, numRows.at (screen));

    state.adjustSelectionAnchors (screen, -count);
});
```

`state.adjustSelectionAnchors()` reads anchors from TextEditor's grafted node via `loadValue()`, subtracts count, writes back via `storeValue()`. Negative anchors -> clear selection. All reader thread, lock-free atomics. Event wiring via `jam::Function::Map`.

### Step 15: ARCHITECTURE.md — reflect new reality

**Scope:** `ARCHITECTURE.md`

- Architecture Philosophy: timer-driven flush (60/120 Hz) is the render trigger.
- Threading Model: timer flush -> ValueTree -> Screen::valueTreePropertyChanged -> Grid::getBlock -> setText(Block<Row>).
- Communication Contracts: Data -> Component via timer flush, not VBlank.
- TextEditor section: stateless renderer over borrowed Block<Row>, owns ValueTree node grafted into State, selection on TextEditor's node.
- Glossary: delete VBlank entry.

**Cross-Thread Contract (state explicitly):**
- READER thread always reads/writes to atomics (raw value).
- MESSAGE thread always reads/writes to `juce::ValueTree` property/value.
- UNIDIRECTIONAL data flow. No hacks. No workaround. No manual state tracking. No shadow state.
- Objects are stateless — hold only transient values for calculation, never mutate state machine.
- Top to bottom, always tell never ask. Virtually no getters. EXCEPT SSOT reads from State machine.

---

## Completion

**Compiler clean = plan done.** Zero old code surviving. Zero dead code. Zero stale comments. Zero fallback paths.

## BLESSED Alignment

- **Bounds** — Grid owns the Block pointer array. Block borrows from Grid. Lifetime chain: Grid > Screen > TextEditor (base). TextEditor's ValueTree node grafted into State — lifetime managed by tree ownership.
- **Lean** — Removes Buffer<Cell> copy path, dual-screen management, filledRows. Scrollbar uses existing LookAndFeel — no custom rendering. Net line reduction in TextEditor.
- **Explicit** — ViewportMode enum. setText() contract. getBlock() parameters explicit. ValueTree properties seeded with defaults. getNode() exposes graftable node. Selection identifiers explicit on TextEditor's node.
- **SSOT** — Grid owns content. State owns scalar parameters. TextEditor's ValueTree node owns view properties (selection, caret, scroll, viewport mode). Scrollbar rendering SSOT via LookAndFeel — identical in both modes. No shadow state, no duplicate selection identifiers.
- **Stateless** — TextEditor stateless on content. Block<Row> set per frame, never owned. View properties live on ValueTree (the SSOT), not as shadow members.
- **Encapsulation** — TextEditor defines its own ValueTree node shape. Knows nothing about Grid, State, terminals, or screens. Screen grafts; Input/Mouse write selection via storeValue; Processor adjusts anchors via storeValue; TextEditor listens on its own node. All event wiring uses `jam::Function::Map` — no new patterns invented.
- **Deterministic** — Same Block<Row> + same ValueTree properties = same render output.

## Risks

1. **Block<Row> lifetime across resize** — Between resized() and next flush, stored Block<Row> could be stale. Mitigated: TextEditor::resized() clears stored content (empty Block). ContentView skips rendering when content.getNumRows() == 0. Next flush repopulates via getBlock.

2. **Selection migration scope** — Moving selection from State to TextEditor's node touches Input, Mouse, ScreenSelection, and Processor. All use the same storeValue/loadValue pattern — the change is retargeting the ValueTree path, not changing the mechanism.

3. **Whelmed future path** — whelmed::Screen will migrate to jam::TextEditor with ViewportMode::absolute. This sprint establishes that path — the two-mode API, ValueTree node, and LookAndFeel-driven scrollbar exist for this reason. Whelmed's content source supplies Block<Row>, grafts TextEditor's node into whelmed::State. Same rendering pipeline, same glyph shaping, same ValueTree-driven view properties. Scrollbar renders identically via shared LookAndFeel.
