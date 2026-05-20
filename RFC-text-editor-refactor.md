# RFC — jam::TextEditor Refactor: Content Ownership to Borrowed Block<Row>

Date: 2026-05-20
Status: Ready for COUNSELOR handoff

## Problem Statement

terminal::Screen copies cells from Grid into TextEditor's internal `jam::Buffer<jam::Cell>` via `setText()` on every flush. Grid already owns the live buffer (`jam::Buffer<jam::Row>`). The copy is redundant. TextEditor should become a stateless renderer over non-owning `Block<Row>`, following TETRIS principles: one-way setters → `calc()`, no reaching out.

Additionally, TextEditor's dual-screen buffer management (`Owner<Buffer<Cell>> screen`, `setActiveScreen()`) is terminal-specific knowledge that doesn't belong in a generic text rendering component. Screen switching is upstream in Grid/State.

## Research Summary

### Current Architecture (Codebase as Ground of Truth)

**jam::TextEditor** (`~/Documents/Poems/dev/jam/jam_gui/text_editor/jam_text_editor.h`):
- Owns `jam::Owner<jam::Buffer<jam::Cell>> screen` — two Buffer<Cell> instances (primary + alternate)
- `setText(Block<Cell>, Range<int>)` copies cell data into owned buffers
- `setActiveScreen(int)` switches between screen[0] and screen[1]
- `resized()` allocates: `screen.at(0)->setSize(1, visibleRows, cols, true, true, true)`
- ContentView constructs `Block<Cell>` from owned buffers, passes to `arrangement.shape()`
- TETRIS contract already in place: every setter calls `calc()`

**jam::Block<T>** (`~/Documents/Poems/dev/jam/jam_core/buffer/jam_block.h`):
- Non-owning, trivially copyable view: `const T* const* rows`, `startRow`, `numRows`, `numCols`
- Lifetime tied to source Buffer — caller responsibility

**jam::Row** (`~/Documents/Poems/dev/jam/jam_fonts/cell/jam_row.h`):
- FAM struct: `uint16_t usedCols`, `uint8_t flags` (bit 0 = `wrapped`), `Cell cells[]`
- `using FlexType = Cell` signals Buffer's FAM stride computation
- Row is the Grid scrollback element type; Cell is the display atom

**terminal::Screen** (`Source/terminal/component/Screen.cpp`):
- Inherits `jam::TextEditor` + `juce::ValueTree::Listener`
- On `valueTreePropertyChanged`: reads Grid rows via `grid.getWritePointer()`/`grid.getRow()`, wraps `r->cells` as `Block<Cell>`, calls `setText(block, {row, row+1})` per row, then `repaint()`
- No scroll state held — reads `state.getScrollOffset()` each flush

**Render trigger** (NO VBlank — timer-driven):
- `jam::ValueTree` base is a `juce::Timer` (60/120 Hz)
- `timerCallback()` → `flush()` → dirty atomics written to ValueTree properties
- `Screen::valueTreePropertyChanged()` fires → reads Grid → calls TextEditor setters → `repaint()`

**whelmed::Screen** (`Source/whelmed/Screen.h`):
- Does NOT use jam::TextEditor — plain `juce::Component` with own Block polymorphism
- Future: parser could output `Buffer<Row>` (or `Buffer<RichTextRow>`) to use TextEditor

### Established Patterns (MUST USE — No New Infrastructure)

**Event dispatch** — `jam::Function::Map<juce::Identifier, void> events` owned by Processor:
- Video fires events: `events.get(id::scrollUp, screen, count)`
- Processor handles: wired in `registerEvents()`
- File: `Processor.cpp` lines 289–301

**State parameter registration** — `State::registerNodeAtomics(juce::ValueTree& node)`:
- Called automatically via `valueTreeChildAdded` when node appended to SESSION root
- Iterates pre-seeded properties → creates atomic mirrors
- Existing pattern: Display grafts `id::DISPLAY` node (`Display.cpp` lines 11–18)

**screenDirty** — monotonic counter, not boolean:
- `State::setScreenDirty(screen)` increments via `storeValue(screenId, id::screenDirty, current + 1)`
- Screen compares to last-seen value
- Fired by Video::flush() via `id::screenDirty` event

**Selection identifiers already exist** in `terminal::id` (`Identifier.h` lines 217–237):
- `selectionCursorRow`, `selectionCursorCol`, `selectionAnchorRow`, `selectionAnchorCol`
- `dragAnchorRow`, `dragAnchorCol`, `dragActive`
- Currently used by `terminal::Input`, `terminal::Mouse`, `ScreenSelection`

**Scroll identifiers** in `terminal::id` (`Identifier.h` lines 268–270):
- `numRows`, `scrollOffset`

## Principles and Rationale

### TETRIS Compliance (Analogous to AudioPlugin-APVTS)

| Audio Plugin | END Terminal |
|---|---|
| APVTS (Model) | terminal::State (ValueTree + atomics) |
| ProcessorChain (Controller) | Processor → Video → Grid |
| Editor (View) | Screen → TextEditor |
| `parameterChanged()` callback | `valueTreePropertyChanged()` listener |
| Private setters → `calc()` | Private setters → `calc()` |
| Pull model, never push | Pull model, timer-driven flush |

TextEditor is the View. It never reaches outside itself. Screen is the translator between State/Grid and TextEditor's setter API. All communication is pull-based through ValueTree listeners.

### BLESSED Pillar Mapping

- **Bounds** — Block<Row> is bounded by Grid's ring buffer. No unbounded allocation.
- **Lean** — Remove owned Buffer<Cell>, remove copy path. One Block<Row> reference replaces per-row setText loop.
- **Explicit** — ViewportMode enum, explicit setContent() contract. No implicit state.
- **SSOT** — Grid owns content. State owns parameters. TextEditor owns view properties on its grafted node. No duplication.
- **Stateless** — TextEditor is stateless on content. Block<Row> set per frame, never owned. View properties live on ValueTree (the State SSOT), not as shadow state.
- **Encapsulation** — TextEditor defines its own ValueTree node shape. Knows nothing about terminals, Grid, State, or Processor. Screen grafts; Processor writes; TextEditor listens.
- **Deterministic** — Same Block<Row> + same ValueTree properties = same render output. No hidden state.

## Scaffold

### 1. TextEditor ValueTree Node

TextEditor owns a `juce::ValueTree` node. Properties are seeded at construction. Caller grafts it into whatever State manages the flush cycle.

```
TEXT_EDITOR (node type)
  PARAM: viewportMode        (int — 0=proportional, 1=absolute)
  PARAM: scrollOffset         (int — proportional mode: caller-supplied)
  PARAM: scrollCapacity       (int — proportional mode: caller-supplied)
  PARAM: selectionAnchorRow   (int — block-local)
  PARAM: selectionAnchorCol   (int — block-local)
  PARAM: selectionCursorRow   (int — block-local)
  PARAM: selectionCursorCol   (int — block-local)
  PARAM: selectionType        (int — none/visual/visualLine/visualBlock)
  PARAM: caretRow             (int — block-local)
  PARAM: caretCol             (int — block-local)
  PARAM: contentDirty         (int — monotonic counter, same pattern as screenDirty)
```

TextEditor is a `juce::ValueTree::Listener` on its own node. Property changes → `calc()` → geometry recomputation → `repaint()`.

### 2. TextEditor API Changes

**Removed:**
- `jam::Owner<jam::Buffer<jam::Cell>> screen` — owned content buffers
- `setText(Block<Cell>&, Range<int>)` — copy path
- `setActiveScreen(int)` — dual-screen management
- `int activeScreen` member
- Buffer allocation in `resized()`

**Added:**
```cpp
// Content — non-owning, set per frame
void setContent (Block<Row>) noexcept;                      // full dirty
void setContent (Block<Row>, juce::Range<int> dirtyRows) noexcept; // partial dirty

// Viewport mode — lazy resource creation
enum class ViewportMode { proportional, absolute };
void setViewportMode (ViewportMode) noexcept;

// Scroll — proportional mode only, called by Screen on flush
void setScrollPosition (int offset, int capacity) noexcept;

// Access to owned node — caller grafts into State
juce::ValueTree& getNode() noexcept;
```

All setters follow TETRIS: check if changed → store → `calc()`.

`setContent()` stores `Block<Row>` as member. First overload delegates to second with `{0, block.getNumRows()}`. `calc()` invalidates glyph cache entries for dirty rows.

### 3. ViewportMode

```cpp
enum class ViewportMode { proportional, absolute };
```

**proportional** — content height == visible bounds always. `jam::Scrollbar` (new component) created lazily as auxiliary indicator. Position = offset / capacity. Caller supplies offset + capacity via `setScrollPosition()`. Used by terminal mode.

**absolute** — `juce::Viewport` created lazily, wraps ContentView. ContentView height == total rows × cellHeight. JUCE computes scroll position from content height vs viewport height. Used by whelmed mode.

Both modes: UI/UX identical. Scrollbar visible, content scrolls. Difference is implementation and data input.

### 4. jam::Scrollbar (New Component)

New component in `jam_gui`. Proportional indicator — takes offset + capacity, draws thumb at offset/capacity ratio. Purely visual, no scroll logic. Thin component, similar in spirit to JUCE's ScrollBar but without the interactive scroll machinery (scroll input is handled upstream by the caller).

### 5. glyph::Arrangement Overload

```cpp
// Existing — Cell-based
void shape (Block<Cell>*, int numBlocks, Font&, int colOffset, int firstRow);

// New — Row-aware, stops at usedCols per row
void shape (Block<Row>*, int numBlocks, Font&, int colOffset, int firstRow);
```

Row overload accesses `row->cells` for Cell data, `row->usedCols` to stop shaping early. Free performance win — current path shapes full column width blindly.

ContentView calls the Row overload directly. No adapter, no pointer extraction layer.

### 6. Data Flow (Refactored)

```
READER thread:
  Video → Grid cells + State atomics (screenDirty counter++)
  Video::scrollUpAndFill() → events.get(id::scrollUp, screen, count)
    → Processor handler: adjusts numRows, adjusts selection anchors
      via storeValue() into TextEditor's grafted node

Timer flush (60/120 Hz):
  jam::ValueTree::flush() → dirty atomics → ValueTree properties
    → TextEditor's node properties updated

MESSAGE thread:
  Screen::valueTreePropertyChanged()
    → reads Grid → calls textEditor.setContent(Block<Row>)
  TextEditor::valueTreePropertyChanged() (on own node)
    → calc() → selection geometry, scroll indicator → repaint()
  ContentView::paint()
    → reads stored Block<Row> → arrangement.shape(Block<Row>) → drawGlyphRuns()
```

### 7. Selection Anchor Adjustment on Scroll

Processor's existing `id::scrollUp` handler gains selection anchor adjustment:

```cpp
// Processor::registerEvents() — id::scrollUp handler (Processor.cpp ~line 289)
events.add<int, int> (id::scrollUp, [this] (int screen, int count)
{
    numRows.at (screen) = juce::jmin (numRows.at (screen) + count, scrollbackLines);
    grid.setNumRows (screen, numRows.at (screen));
    state.setNumRows (screen, numRows.at (screen));

    // Shift selection anchors — content scrolled up by count rows
    // Uses storeValue into TextEditor's grafted node
    // Anchors that scroll past block top become invalid → selection cleared
    state.adjustSelectionAnchors (screen, -count);
});
```

`adjustSelectionAnchors` is a State method that reads current anchor values via `loadValue()`, subtracts count, and writes back via `storeValue()`. If anchors go negative, clears selection (sets type to none). All on reader thread, all lock-free atomics.

### 8. terminal::Screen Migration

**Before:**
```cpp
void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    // ... reads activeScreen, scrollOffset, viewportRows, numCols
    for (int row = 0; row < viewportRows; ++row)
    {
        auto* r = grid.getWritePointer (activeScreenIndex, row);
        const auto* cells = r->cells;
        jam::Block<jam::Cell> block { &cells, 1, numCols.value };
        setText (block, { row, row + 1 });
    }
    repaint();
}
```

**After:**
```cpp
// Constructor
Screen::Screen (terminal::State& state, terminal::Grid& grid) noexcept
    : state (state), grid (grid)
{
    state.get().appendChild (getNode(), nullptr);  // grafts TextEditor's node → registerNodeAtomics
    // ...
}

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    const auto activeScreenIndex = state.getActiveScreen();
    const auto scrollOffset = state.getScrollOffset (activeScreenIndex);
    const auto area = getCellArea();

    // One Block<Row> from Grid — replaces per-row setText loop
    auto block = grid.getBlock (activeScreenIndex, scrollOffset, area.visibleRows);
    setContent (block);
}
```

`grid.getBlock()` — new Grid method returning `Block<Row>` for a viewport-sized region. Replaces per-row `getWritePointer`/`getRow` access pattern.

### 9. Whelmed Future Path

Whelmed parser currently outputs `juce::AttributedString` blocks. Migration path:
- Parser outputs `Buffer<Row>` (or `Buffer<RichTextRow>` with extra metadata) instead
- whelmed::Component wraps a `jam::TextEditor` with `ViewportMode::absolute`
- Grafts TextEditor's node into `whelmed::State`
- Same Block<Row> read contract, same ValueTree-driven view properties
- Same glyph rendering pipeline — shaper doesn't care about content source

## BLESSED Compliance Checklist

- [x] Bounds — Block<Row> bounded by Grid ring buffer; no unbounded allocation
- [x] Lean — Removes Buffer<Cell> copy, removes dual-screen management from TextEditor
- [x] Explicit — ViewportMode enum, explicit setContent() contract, no implicit state
- [x] SSOT — Grid owns content, State owns parameters, TextEditor owns view props on grafted node
- [x] Stateless — TextEditor stateless on content; view props on ValueTree not shadow state
- [x] Encapsulation — TextEditor self-contained; Screen grafts; Processor writes; no cross-knowledge
- [x] Deterministic — Same inputs = same render output

## Open Questions

1. **Existing selection identifiers** — `terminal::id` already defines `selectionAnchorRow/Col`, `selectionCursorRow/Col` etc. (Identifier.h lines 217–237). These are terminal-specific and used by `terminal::Input`, `terminal::Mouse`, `ScreenSelection`. TextEditor's node needs its own generic selection properties. Decision: do the terminal-specific identifiers migrate to TextEditor's node, or do they coexist (terminal::id for input handling, TextEditor node for rendering)?

2. **grid.getBlock()** — New method on Grid returning `Block<Row>` for a viewport region. Grid currently exposes per-row access (`getWritePointer`, `getRow`). The Block<Row> construction needs the row pointer array from `Buffer<Row>`. Confirm this can be constructed via `Block<Row>(buffer, channel, rowOffset, rowCount)` using Block's existing sub-region constructor.

3. **jam::Scrollbar** scope — Minimal proportional indicator, or interactive (drag thumb to scroll)? Terminal mode: scroll input is already handled by `terminal::Mouse` wheel events → State. Scrollbar could be purely visual. Whelmed mode uses juce::Viewport's own scrollbar. Decision affects component complexity.

## Handoff Notes

- ARCHITECTURE.md describes VBlank as a decision but it was never implemented. Render trigger is timer-driven flush (60/120 Hz). ARCHITECTURE.md needs update — stale doc.
- `jam::Scrollbar` does not exist yet — must be created as part of this work.
- The `shape(Block<Row>)` overload on `glyph::Arrangement` is additive — existing `shape(Block<Cell>)` stays for any consumer that doesn't have Row context.
- TextEditor's existing TETRIS contract (`calc()` on every setter) is the foundation — refactor extends it, doesn't change it.
- `registerNodeAtomics()` requires properties pre-seeded on the node before `appendChild()`. TextEditor constructor must seed all properties with defaults.
- All event wiring uses `jam::Function::Map<juce::Identifier, void>` — no manual lambdas, no manual callbacks outside the established pattern.
- Selection anchor adjustment co-locates in existing `id::scrollUp` handler — same place as `numRows` update. Atomic, same transaction.
