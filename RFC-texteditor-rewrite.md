# RFC — Terminal Rendering Architecture: Buffer<Cell> + DiscreteStateTransition + Universal TextEditor
Date: 2026-05-22 (amended 2026-05-22)
Status: Ready for COUNSELOR handoff

## Problem Statement

The terminal renderer has three active regressions rooted in a single architectural cause: tmux-faithful row reflow.

**Active regressions:**
1. Downsizing reflow always goes upward — viewport shrinks and creates scrollbar instead of pushing rows into scrollback correctly.
2. When scrollback is full, resizing only affects visible rows — history rows are skipped by the reflow walk.
3. Upsizing does not reflow to fill available space — wrapped rows fail to join back.

**Root cause:** `Grid::reflow()` and `Grid::reflowFrom()` implement the tmux `screen_resize` algorithm verbatim. This algorithm operates on physical rows with `wrapped` flags to reconstruct and re-split logical paragraphs. It is correct for tmux's model but fragile at edge cases: cursor at wrap boundary, history overflow interaction with resizeHeight, partial-history reflow when the ring is full. Every interaction between these cases compounds into new bugs.

**Architectural debt beyond the regressions:**
- The proportional/absolute viewport split in `jam::TextEditor` exists only because content height was unknowable without a completed reflow pass.
- The fake `jam::Scrollbar` and its micromanagement (visibility toggling affects col/row dimensions via `resized()`) are direct consequences.
- Two diverging viewport modes is complexity that exists solely to work around the reflow dependency.
- `GridSizeTransition` is hardwired to `Grid&` and `Video&` — the SST pattern is generic but the implementation is not reusable.
- `jam::Row` FAM struct exists only to carry `usedCols` and `wrapped` flags that the reflow algorithm consumes.

**Scope of this RFC:** Four architectural pillars:
1. **Buffer<Cell> replaces Buffer<Row>** — Row struct deleted, Grid slimmed (storage-mutating reflow deleted, render-time wrapping via Arrangement replaces it), Buffer stays dumb storage.
2. **jam::DiscreteStateTransition** — generalized from GridSizeTransition into jam. Registered triggers via Function::Map, no hardwired references. Same SST contract.
3. **jam::TextEditor as universal dumb renderer** — one viewport mode, juce::Viewport subclass with reentrant guard, content-agnostic cell-grid renderer.
4. **Ownership and responsibility restructuring** — Display sole dimension author, Video sole cursor author, Screen owns DST and resize coordination.

---

## What Is Removed

| Component | Removed |
|---|---|
| `jam::Row` struct | FAM struct, `usedCols`, `flags`, `wrapped`, `dead` constants. File `jam_fonts/cell/jam_row.h` deleted. |
| `Grid::reflow()` | Full tmux join+split algorithm |
| `Grid::reflowFrom()` | Reflow from snapshot source, scratch buffer allocation, paragraph cursor wrapping/unwrapping |
| All static reflow helpers in Grid.cpp | `wrapCursorPosition`, `unwrapCursorPosition`, `reflowDead`, `reflowMove`, `reflowJoin`, `reflowSplit`, `reflowScreen` (~470 lines) |
| `Grid::getRow()` | Absolute-index history access — only consumed by reflow |
| `Grid::getBuffer()` | Const ref exposure — only consumed by GridSizeTransition snapshot |
| `GridSizeTransition` class (END) | Replaced by `jam::DiscreteStateTransition` + registered terminal triggers |
| `jam::TextEditor::ViewportMode` enum | `proportional` and `absolute` — single mode remains |
| `jam::TextEditor::setViewportMode()` | No modes to switch |
| `jam::TextEditor::setScrollRange()` | Terminal scrollback vocabulary — juce::Viewport handles range |
| `jam::TextEditor::attach()` | jam::Scrollbar binding — no jam::Scrollbar usage |
| `jam::Scrollbar` usage in TextEditor | Class stays in module (may be used elsewhere), not used by TextEditor |
| `scrollOffsetId`, `scrollCapacityId`, `scrollbarVisibleId` | Terminal-specific properties removed from TextEditor |
| `ViewportMode viewportMode` member | No modes |
| Proportional branch in `calc()` | Dead code |
| Proportional branch in `shapeVisibleContent()` | Dead code |
| Scrollbar bounds reservation in `resized()` | No jam::Scrollbar |
| `Block<Row>` as content type | Replaced by `Block<Cell>` throughout |
| `shape(Block<Row>, ...)` Arrangement overloads | Removed with Row type |
| `Video::resolveWrapPending()` `flags |= wrapped` write | No `Row::wrapped` flag exists |
| `Video` `usedCols` writes (3 sites) | No `Row::usedCols` field exists |

The tmux storage-mutating reflow model is discarded entirely. No partial preservation. No compatibility shim. Cross-row joining on upsize is abandoned by design — the three regressions and the code that caused them are deleted together. Render-time wrapping via `Arrangement::shape()` with `wrapColumns` replaces storage reflow — same stored cells, different visual line breaks at current viewport width.

---

## Pillar 1: Buffer<Cell> Replaces Buffer<Row>

### jam::Buffer<Cell> — Dumb Storage

`jam::Buffer` stays exactly what it is: multi-channel, 2-D, SIMD-aligned flat storage. AudioBuffer analogue. Lock-free. No ring logic, no terminal logic. Already in jam.

With `Buffer<Row>` (FAM mode): row stride = `sizeof(Row) + alignedCols * sizeof(Cell)`. Each "row" has a `Row` header (`usedCols` + `flags`) followed by cells.

With `Buffer<Cell>` (non-FAM mode): row stride = `alignedCols * sizeof(Cell)`. No header. Pure cell storage. `getWritePointer(channel, row)` returns `Cell*`. `getReadPointer(channel, row)` returns `const Cell*`.

No API changes to Buffer. The type parameter change from `Row` to `Cell` switches from FAM to non-FAM mode automatically via the existing `HasFlexType<T>` trait at `jam_buffer.h:48`.

### Grid — Slimmed Ring Buffer Manager

Grid stays in END. Terminal-specific ring buffer with scrollback bookkeeping. Not absorbed into jam.

**Buffer type:** `jam::Buffer<jam::Row>` → `jam::Buffer<jam::Cell>`. Two channels (normal=0, alternate=1).

**Ring state unchanged:** `head[2]`, `numRows[2]`, `ringMask`, `viewportRows`, `scrollbackLines`, `blockPointers`. Same power-of-two ring sizing: `nextPow2((scrollbackLines + viewportRows) * 2)`.

**Physical addressing unchanged:** `physicalRow(screen, row)` = `(head[screen] + row.value) & ringMask`.

**Methods that stay (signature changes only):**

| Method | Change |
|---|---|
| `getWritePointer(screen, row)` | Returns `Cell*` (was `Row*`) |
| `getBlock(screen, scrollOffset, viewportRows)` | Returns `Block<Cell>` (was `Block<Row>`). Same `blockPointers` fill logic. |
| `scrollUp(screen, top, bottom, count)` | Full-screen: O(1) head advance + `numRows` increment capped at `scrollbackLines` + `buffer.clear` on new row. Partial: row-by-row `buffer.copyFrom` + clear. Unchanged logic. |
| `scrollDown(screen, top, bottom, count)` | Full-screen: O(1) head retreat + `buffer.clear`. Partial: row-by-row. Unchanged. |
| `clear(screen)` / `clear(screen, row)` / `clear(screen, row, startCol, numCols)` | Unchanged — delegates to `buffer.clear` at `physicalRow`. |
| `setSize(viewportRows, numCols, scrollbackLines)` | Unchanged — `buffer.setSize(2, ringSize, numCols, false, true, false)`. Resets heads and numRows. |
| `resizeHeight(newRows, cursorRow)` | Stays — viewport height adjustment with scrollback pull/push. Cursor clamping stays here (Grid adjusts `cursorRow` in-place, caller writes to State). |
| `isAllocated()` | Unchanged — `ringMask > 0`. |
| `getNumRows(screen)` | Unchanged. |
| `getViewportRows()` | Unchanged. |
| `getRingMask()` | Unchanged. |
| `getHeadPosition(screen)` | Unchanged. |

**Methods added:**

| Method | Purpose |
|---|---|
| `resizeCols(newCols, scrollbackLines)` | Lossless column resize. Allocates new buffer at new stride. Copies `min(oldCols, newCols)` cells per row for all rows (history + viewport, both screens). Pads with `Cell::erase()` on grow. Updates `ringMask`, reallocates `blockPointers`. No join, no split, no paragraph awareness. |

**Methods deleted:**

| Method | Why |
|---|---|
| `reflow()` | tmux reflow — deleted |
| `reflowFrom()` | tmux reflow from snapshot — deleted |
| `getRow(screen, absoluteIndex)` | Only consumed by reflow — deleted |
| `getBuffer()` | Only consumed by GridSizeTransition snapshot — snapshot logic moves to DST triggers |

**Static helpers deleted (Grid.cpp):**

`wrapCursorPosition` (lines 19-60), `unwrapCursorPosition` (lines 62-172), `reflowDead` (lines 174-179), `reflowMove` (lines 181-186), `reflowJoin` (lines 188-319), `reflowSplit` (lines 321-420), `reflowScreen` (lines 422-489). ~470 lines removed.

### Video — Cell* Write Path

Video's `Grid&` interface narrows. Every callsite that accessed `Row*` members changes mechanically:

**Pattern change:**
```
// Before (Row*)
jam::Row* const row { grid.getWritePointer(scr, cell(writeRow)) };
row->cells[writeCol] = jam::Cell::make(...);
row->usedCols = juce::jmax(row->usedCols, uint16_t(writeCol + charWidth));

// After (Cell*)
jam::Cell* const cells { grid.getWritePointer(scr, cell(writeRow)) };
cells[writeCol] = jam::Cell::make(...);
// usedCols write deleted — no usedCols field exists
```

**Callsites (exhaustive):**

| File | Lines | Current access | After |
|---|---|---|---|
| `Video.cpp:379` | `resolveWrapPending()` | `currentRow->flags \|= jam::Row::wrapped` | Line deleted — no `wrapped` flag. `resolveWrapPending()` still advances cursor and scrolls; only the flag write is removed. |
| `Video.cpp:443` | `print()` grapheme cluster | `&row->cells[lastWriteCol]` | `&cells[lastWriteCol]` |
| `Video.cpp:528` | `print()` main write | `writtenRow->cells[writeCol] = glyph` + `writtenRow->usedCols = ...` | `cells[writeCol] = glyph`. `usedCols` write deleted. |
| `Video.cpp:306-314` | `scrollUpAndFill()` | `row->cells[col] = fill` | `cells[col] = fill` |
| `Video.cpp:337` | `scrollDownAndFill()` | `row->cells[col] = fill` | `cells[col] = fill` |
| `VideoEdit.cpp:83-275` | `eraseInDisplay/eraseInLine` (12 sites) | `row->cells[c] = fill` | `cells[c] = fill` |
| `VideoEdit.cpp:380,389` | `shiftLines` fill | `row->cells[c] = fill` | `cells[c] = fill` |
| `VideoEdit.cpp:430-442` | `shiftCellsRight` | `row->cells` for memmove + `row->usedCols` update | `cells` for memmove. `usedCols` write deleted. |
| `VideoEdit.cpp:469` | `removeCells` | `row->cells` for memmove | `cells` for memmove |
| `VideoEdit.cpp:506` | `eraseCells` | `row->cells[c] = fill` | `cells[c] = fill` |
| `VideoESC.cpp:249-254` | `escDispatchDEC` DECALN | `rowPtr->cells[col] = E` + `rowPtr->usedCols = nCols` | `cells[col] = E`. `usedCols` write deleted. |
| `VideoCSI.cpp:527` | `scrollDown` fill | `row->cells[c] = fill` | `cells[c] = fill` |

**No other Grid methods change signature for Video.** `grid.scrollUp()`, `grid.scrollDown()`, `grid.clear()` signatures are unchanged — they take `(screen, top, bottom, count)` and operate on ring state internally.

### jam::Block<Cell> — Non-Owning View

`Block<Cell>` already exists in jam. Same pointer-per-row model as `Block<Row>`:
- `getRowPointer(row)` returns `const Cell*` (was `const Row*`)
- `getNumRows()`, `getNumCols()`, `getSubBlock()` unchanged
- `isEmpty()` unchanged

Grid's `getBlock()` fills `blockPointers` with `const Cell*` entries (was `const Row*`) via ring-mapped physical rows, constructs `Block<Cell>` from the raw pointer array. Ring-to-logical unwrapping happens in Grid, same as today.

---

## Pillar 2: jam::DiscreteStateTransition

### Design — SST Pattern for Discrete State Changes

`jam::DiscreteStateTransition` is the terminal-domain analogue of `kuassa::dsp::SmoothStateTransition`. Same TETRIS contract:

| SST (audio) | DST (terminal) |
|---|---|
| `ObjectClass current, previous, target` | Live grid (external), `previous` snapshot (owned), target dims |
| `previous = current` (trivial copy, memcpy) | `captureSnapshot()` — cell-level memcpy via `Buffer::copyFrom` per row |
| `triggers.get(name, current, value)` — mutate current | Registered trigger — resize grid, clamp cursor, sync State |
| `crossfadePosition`, `crossfadeIncrement` | Same — timer-driven position advance |
| `isTransitioning`, `isReady` | Same — cold start suppresses first transition |
| `hasPending` — replace, not queue | Same — coalescing, latest value wins |
| `set(name, value)` — entry point | `set(cols, rows)` — entry point |
| `process(sample)` — per-sample crossfade | `process()` — per-tick crossfade advance (timer-driven) |
| `flush()` — immediate apply, no animation | `flush()` — immediate apply, no animation |
| `prepare(sampleRate, blockSize)` | `prepare(scrollbackLines)` — cold start config |
| `addTrigger<ValueType>(name, setter)` | `addTrigger<Args...>(name, callback)` — registered via `jam::Function::Map` |
| `getCurrent()` — access wrapped object | Not needed — grid is external, accessed by reference |

### Class Definition (jam library)

```cpp
namespace jam
{

class DiscreteStateTransition : private juce::Timer
{
public:
    DiscreteStateTransition() noexcept;
    ~DiscreteStateTransition() override = default;

    //==========================================================================
    // SST contract
    //==========================================================================

    /** Register a named trigger. Same pattern as SmoothStateTransition::addTrigger.
     *  Callback receives the values passed to set(). */
    template<typename... Args, typename FunctionType>
    void addTrigger (const juce::Identifier& name, FunctionType&& callback)
    {
        triggers.add<Args...> (name, std::forward<FunctionType> (callback));
    }

    /** Trigger a discrete state change. If transitioning, replaces pending.
     *  Same semantics as SmoothStateTransition::set(). */
    template<typename... Args>
    void set (const juce::Identifier& name, Args... args)
    {
        if (isTransitioning)
        {
            pendingChanges.clear();
            pendingChanges.add<> ([this, name, args...]()
            {
                applyChange (name, args...);
            });
        }
        else
        {
            applyChange (name, args...);
        }
    }

    /** Advance crossfade. Called from timerCallback(). */
    void process() noexcept;

    /** Apply all pending immediately, no animation.
     *  Same as SmoothStateTransition::flush(). */
    void flush() noexcept;

    /** Cold start configuration. Sets isReady = false.
     *  First applyChange after prepare() fires immediately without animation. */
    void prepare() noexcept;

    /** Configure crossfade timing. */
    void setCrossfadeTimeMs (double timeMs) noexcept;

    /** Query transition state. */
    bool isInTransition() const noexcept { return isTransitioning; }
    double getCrossfadePosition() const noexcept { return crossfadePosition; }

private:
    template<typename... Args>
    void applyChange (const juce::Identifier& name, Args... args)
    {
        triggers.get (name, args...);

        if (isReady)
        {
            crossfadePosition = 0.0;
            isTransitioning = true;
            startTimer (tickIntervalMs);
        }
        else
        {
            isReady = true;
        }
    }

    void advanceCrossfade() noexcept;
    void updateCrossfadeIncrement() noexcept;
    void timerCallback() override { process(); }

    jam::Function::Map<juce::Identifier, void> triggers;
    jam::Function::Vector<void> pendingChanges;

    bool isReady { false };
    bool isTransitioning { false };
    double crossfadePosition { 1.0 };
    double crossfadeIncrement { 0.0 };
    double transitionTimeMs { 200.0 };
    static constexpr int tickIntervalMs { 16 };
};

} // namespace jam
```

### Terminal Registration — Screen Owns DST

Screen constructs `jam::DiscreteStateTransition` and registers terminal-specific triggers. No hardwired `Grid&`, `Video&`, or `events&` in the DST class itself.

**Trigger registration (Screen constructor or init):**

```cpp
// "resize" trigger — called by set() when dimensions change
transitioner.addTrigger<cell, cell> (id::resize,
    [this] (cell cols, cell rows)
    {
        // 1. Capture snapshot BEFORE mutation (SST contract: previous = current)
        captureSnapshot();

        // 2. Resize grid (mutate current)
        if (rows != grid.getViewportRows())
        {
            cell cursorRow { state.loadValue(activeScreenId, id::cursorRow) };
            grid.resizeHeight (rows, cursorRow);
            state.storeValue (activeScreenId, id::cursorRow, cursorRow.value);
        }

        if (cols != grid.getBuffer().getNumCols())
        {
            grid.resizeCols (cols.value, scrollbackLines);
            cell cursorCol { state.loadValue(activeScreenId, id::cursorCol) };
            cursorCol = cell { juce::jmin(cursorCol.value, cols.value - 1) };
            state.storeValue (activeScreenId, id::cursorCol, cursorCol.value);
        }

        // 3. Sync State (message thread — direct ValueTree write)
        state.setValue (normalScreenId, id::numRows, grid.getNumRows(Map::Screen::normal));
        state.setValue (alternateScreenId, id::numRows, grid.getNumRows(Map::Screen::alternate));
        state.setValue (activeScreenId, id::scrollOffset, 0);
    });

// "settled" trigger — called when crossfade completes
transitioner.addTrigger<> (id::resizeEnd,
    [this] ()
    {
        // SIGWINCH delivery via events map — Processor has registered the handler
        events.get (id::resizeEnd);
    });

// "cellSize" trigger — coalesced cell pixel size change
transitioner.addTrigger<int, int> (id::cellSize,
    [this] (int cellWidth, int cellHeight)
    {
        // Apply to font metrics before resize triggers
        // ...
    });
```

**Trigger points (Screen::valueTreePropertyChanged):**

```cpp
void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    // Dimension change → trigger DST
    if (property == id::cols || property == id::visibleRows)
    {
        auto cols = state.getCols();
        auto rows = state.getVisibleRows();
        transitioner.set (id::resize, cols, rows);
        return;
    }

    // Cell size change → trigger DST
    if (property == id::cellWidth || property == id::cellHeight)
    {
        // ...coalesce cell size via DST...
        return;
    }

    // Normal flush path — render
    // ...read grid, setText, repaint...
}
```

### Snapshot Discipline

The `previous` snapshot buffer is NOT inside `jam::DiscreteStateTransition` — it is managed by the registered trigger callback. The trigger owns the snapshot because:

1. The snapshot type (`jam::Buffer<Cell>`) is domain-specific — DST is generic.
2. The snapshot capture logic (ring-mapped row-by-row `copyFrom`) is terminal-specific.
3. Screen reads from `previous` during the transition for visual continuity — Screen must own the data.

Screen members for snapshot:

```cpp
jam::Buffer<jam::Cell> previous;
std::array<int, 2> previousHead { 0, 0 };
std::array<int, 2> previousNumRows { 0, 0 };
int previousRingMask { 0 };
int previousViewportRows { 0 };
```

`captureSnapshot()` copies only live rows (history + viewport) per screen from the live grid into `previous`, using the same absolute-index formula as current `GridSizeTransition::captureSnapshot()` (GridSizeTransition.cpp:213-233).

During `isInTransition()`: Screen reads from `previous` at intermediate pixel width. `Arrangement::shape()` wraps correctly at any width. After transition: Screen reads from live grid.

### advanceCrossfade — Completion Fires Settled Trigger

```cpp
void DiscreteStateTransition::advanceCrossfade() noexcept
{
    crossfadePosition += crossfadeIncrement;

    if (crossfadePosition >= 1.0)
    {
        crossfadePosition = 1.0;
        isTransitioning = false;
        stopTimer();

        // Fire settled trigger if registered
        if (triggers.contains (settledId))
            triggers.get (settledId);

        // Drain pending
        if (pendingChanges.size() > 0)
        {
            pendingChanges.get (0);
            pendingChanges.clear();
        }
    }
}
```

The settled trigger name is configurable via `setSettledTrigger(Identifier)` or a fixed convention (e.g., the trigger name with `"End"` suffix). COUNSELOR decides the exact naming convention during PLAN.

---

## Pillar 3: jam::TextEditor — Universal Dumb Renderer

### Design Principle

TextEditor renders `Block<Cell>` through our glyph pipeline. Content-agnostic — it does not know what the cells represent (terminal output, markdown, anything). Input: `Block<Cell>`. Pipeline: `Arrangement::shape()` → `glyph::Graphics` → `paint()`. Viewport integration via juce::Viewport subclass.

Future: whelmed parser outputs `Block<Cell>`, whelmed::Screen inherits jam::TextEditor.

### What Stays (Generic)

| Member | File:Line | Purpose |
|---|---|---|
| `ContentView` inner component | content_view.cpp:14 | Universal paint delegate |
| `glyph::Arrangement arrangement` | h:137 | Shaping engine |
| `glyph::Graphics glyphGraphics` | h:138 | Draw run renderer |
| `jam::Font font` + `setFont()` | h:125, cpp:62 | Font management |
| `CaretComponent` + `setCaretPosition/Char/BlinkRate` | h:136, h:66-79 | Cursor display |
| `Block<Cell> content` (was `Block<Row>`) | h:123 | Non-owning content view |
| `setText(Block<Cell>)` | h:85 | Content entry point |
| `calc()` | h:128, cpp:87 | Content sizing + bottom-pin + repaint |
| `shapeVisibleContent(clip)` — absolute branch | content_view:51-64 | Clip-based sub-blocking for visible rows only |
| `drawGlyphRuns(g, clip)` | content_view:73-107 | Atlas draw pipeline |
| ValueTree `state` node | h:124 | Selection, caret, dimensions |
| Selection properties (anchor/cursor row/col, type) | h:28-32 | Text selection coordinates |
| `contentDirtyId` | h:35 | Repaint gating |
| `visibleWidthId`, `visibleHeightId` | h:37-38 | Viewport geometry → State |
| `resized()` | cpp:74 | Viewport bounds + dimension writes |
| `valueTreePropertyChanged` → `calc()` | cpp:186 | Reactive repaint |
| `textEditorId` | h:24 | Node identity |
| `caretRowId`, `caretColId` | h:33-34 | Caret position properties |
| Colour IDs | h:52-56 | Background, text, caret, outline colours |
| `static const std::array<Identifier, propertyCount> properties` | h:53 | SSOT property accessor |

### What Is Removed

| Member | File:Line | Why |
|---|---|---|
| `ViewportMode` enum | h:96 | One mode — absolute with juce::Viewport |
| `setViewportMode()` | cpp:133-161 | No modes to switch |
| `jam::Scrollbar` member | h:140 | juce::Viewport native scrollbar |
| `setScrollRange(capacity)` | cpp:163-178 | Terminal scrollback vocabulary |
| `attach(juce::Value)` | cpp:180-184 | Scrollbar binding |
| `scrollOffsetId` | h:26 | Terminal-specific |
| `scrollCapacityId` | h:27 | Terminal-specific |
| `scrollbarVisibleId` | h:48 | juce::Viewport manages visibility |
| `viewportMode` member | h:142 | No modes |
| Proportional branch in `calc()` | cpp:98-100 | Dead code |
| Proportional branch in `shapeVisibleContent()` | content_view:47-49 | Dead code |
| Scrollbar bounds in `resized()` | cpp:78-79 | No jam::Scrollbar |
| `setCaretShape(int decscusr)` | h:69 | Terminal VT vocabulary (DECSCUSR) — moves to terminal::Screen |

### What Is Added — Adopted from juce::TextEditor

#### 1. TextEditorViewport — Viewport Subclass with Reentrant Guard

Replaces plain `std::unique_ptr<juce::Viewport>`. Adopted from juce_TextEditor.cpp:164-199.

```cpp
struct TextEditorViewport final : juce::Viewport
{
    explicit TextEditorViewport (TextEditor& ed) : owner (ed) {}

    void visibleAreaChanged (const juce::Rectangle<int>&) override
    {
        if (not reentrant)
        {
            auto wrapWidth = owner.getWrapWidth();

            if (wrapWidth != lastWrapWidth)
            {
                lastWrapWidth = wrapWidth;
                juce::ScopedValueSetter<bool> svs (reentrant, true);
                owner.checkLayout();
            }
        }
    }

private:
    std::unique_ptr<AccessibilityHandler> createAccessibilityHandler() override
    {
        return createIgnoredAccessibilityHandler (*this);
    }

    TextEditor& owner;
    int lastWrapWidth { 0 };
    bool reentrant { false };
};
```

**Why this guard is mandatory:** juce::Viewport fires `visibleAreaChanged` when the visible area changes — including when scrollbar visibility toggles. Scrollbar appears → `getMaximumVisibleWidth()` shrinks → wrap width changes → layout recalculates → content height changes → scrollbar may disappear → width changes back → infinite loop. JUCE's own TextEditor has this exact guard with this exact comment: "it's rare, but possible to get into a feedback loop as the viewport's scrollbars appear and disappear, causing the wrap width to change" (juce_TextEditor.cpp:173-174).

**Viewport scroll position flow:**

`visibleAreaChanged()` is the reactive hook for scroll position. When the user scrolls (mouse wheel, scrollbar drag), juce::Viewport calls `visibleAreaChanged()`. The override computes `scrollOffset` from the viewport position and writes to State:

```
User scrolls → juce::Viewport::setViewPosition
  → Viewport::updateVisibleArea() (juce_Viewport.cpp:458: lastVisibleArea != visibleArea guard)
  → visibleAreaChanged()
  → compute scrollOffset from viewY
  → state.storeValue(screenId, id::scrollOffset, offset)  [atomic]
```

Bottom-pin: `scrollOffset == 0` means "show live content." `setViewPosition(0, contentHeight - viewportHeight)` pins to bottom. `visibleAreaChanged` fires → computes offset = 0 → no State change → stable.

History scroll: `scrollOffset > 0` means "show history." `viewY = (numRows - scrollOffset) * cellHeight`. State is SSOT — juce::Viewport position is derived.

#### 2. getWrapWidth() — Derived from Viewport Visible Width

```cpp
int getWrapWidth() const noexcept
{
    return viewport->getMaximumVisibleWidth();
}
```

`juce::Viewport::getMaximumVisibleWidth()` (juce_Viewport.cpp:240) returns `contentHolder.getWidth()` — the content area width AFTER scrollbar subtraction. This is the wrap width for Arrangement::shape(). No manual scrollbar-width arithmetic.

#### 3. checkLayout() — Content Sizing with Scrollbar Auto-Management

Absorbs the content-sizing logic from `calc()` and adds scrollbar visibility management adopted from juce::TextEditor::checkLayout() (juce_TextEditor.cpp:964-997).

```cpp
void TextEditor::checkLayout()
{
    auto contentHeight = computeContentHeight();  // subclass provides
    contentView->setSize (getWrapWidth(), contentHeight);
    // juce::Viewport auto-shows vertical scrollbar when content > viewport
    // Reentrant guard in TextEditorViewport prevents oscillation
}
```

### Arrangement::shape() — Wrapping at Column Count

The existing `Block<Cell>` shape overload (jam_glyph_arrangement.cpp:122-126) already works for terminal wrapping. The `wrapColumns` parameter in `buildArrangements` (jam_glyph_arrangement_shape.cpp:345-349) wraps when `currentCol >= wrapColumns`. Each terminal row is shaped independently per `shapeImpl`'s per-row loop.

**No new overload needed.** Pass `wrapColumns = getWrapWidth() / cellWidth` (viewport pixel width ÷ cell pixel width = effective columns). The existing `wrapColumns > 0 && currentCol >= wrapColumns` logic handles it.

The `Block<Row>` shape overloads are removed with the Row type. The `Block<Cell>` overloads stay unchanged.

**`lineOffset` for clip-based sub-blocking:** `shapeVisibleContent()` computes `firstVisibleRow` from clip rect, extracts `subBlock = content.getSubBlock(firstVisibleRow, visRowCount)`, calls `arrangement.shape(subBlock, font, wrapColumns, firstVisibleRow)`. The `lineOffset` parameter offsets all entry `.row` values so pixel positions land at correct absolute ContentView coordinates. Unchanged from today's absolute-mode path.

### ContentView height and Scroll Position

**Content height formula:**
```
contentHeight = (numRows + viewportRows) * cellHeight
```

- `numRows` = history row count from Grid (per-screen, from State ValueTree)
- `viewportRows` = visible viewport row count (from State)
- `cellHeight` = cell pixel height (from Font::bounds)

**Alternate screen:** `numRows[alternate]` is always 0. `contentHeight = viewportRows * cellHeight`. Content fits viewport. Scrollbar auto-hides. No scroll.

**Scroll position contract:**
- Live rendering (`scrollOffset == 0`): `viewport->setViewPosition(0, contentHeight - viewportHeight)` — bottom-pinned.
- History mode (`scrollOffset > 0`): `viewY = (numRows - scrollOffset) * cellHeight`.
- `visibleAreaChanged()` → `scrollOffset = (contentHeight - viewportHeight - viewY) / cellHeight` → write to State. State is SSOT.
- On resize: DST trigger resets `scrollOffset` to 0 (bottom-pinned).

---

## Pillar 4: Ownership and Responsibility Restructuring

### Authorship Rules

| Data | Sole Author | Thread | Consumers |
|---|---|---|---|
| Dimensions (cols, rows, cellWidth, cellHeight) | `Display::resized()` | Message | Video (atomics), Screen (ValueTree), Grid (via resize triggers) |
| Cursor position (cursorRow, cursorCol) | Video | Reader | Screen (ValueTree), DST trigger (clamp on resize, message thread — no race: Video has no new data until after SIGWINCH) |
| Cell content | Video | Reader | Screen via `Block<Cell>` (message thread) |
| Scroll offset | Screen (via `visibleAreaChanged`) + Display (mouse wheel) | Message | Screen (ValueTree), Video (not consumed) |
| History row count (numRows) | Grid (internal, via scrollUp/clear) | Reader | Processor syncs to State (reader), DST trigger syncs to State (message) |

### Object Ownership After Redesign

```
Session owns:
  Grid grid                          [ring buffer + terminal scrollback]
  TextBuffer textBuffer              [cross-thread string slots]
  unique_ptr<Processor> processor    [pipeline orchestrator]

Processor owns:
  State state                        [APVTS bridge — atomics + ValueTree]
  Video video                        [VT command processor — Grid& ref]
  Skit skit                          [image protocol decoder]
  unique_ptr<Parser> parser          [VT byte stream decoder]
  unique_ptr<TTY> tty                [platform PTY]
  Function::Map events               [event dispatch]

  Processor does NOT own:
    Grid (Session owns, Processor receives Grid& — passes to Video)
    DiscreteStateTransition (Screen owns)

Display owns:
  Screen screen                      [renderer + DST coordinator]
  Input input                        [key dispatch]
  Mouse mouse                        [mouse events]
  ComponentAttachment attachment     [DISPLAY node — cellWidth/cellHeight/baseline]
  ValueTree normalScreen, alternateScreen  [screen state nodes]

Screen owns:
  jam::DiscreteStateTransition transitioner  [resize lifecycle]
  jam::Buffer<Cell> previous                 [snapshot — SST contract]
  Snapshot metadata (previousHead, previousNumRows, previousRingMask, previousViewportRows)

  Screen inherits jam::TextEditor:
    TextEditorViewport viewport       [reentrant guard]
    ContentView contentView           [paint delegate]
    CaretComponent caret              [cursor display]
    glyph::Arrangement arrangement    [shaping engine]
    glyph::Graphics glyphGraphics     [draw runs]

Screen holds references to:
  Grid& grid                          [read: getBlock; resize: resizeHeight, resizeCols]
  State& state                         [read/write: scrollOffset, cursor, numRows, dimensions]
  Function::Map& events               [fire: resizeEnd — obtained via Display → Processor& → processor.getEvents()]
```

### Data Flow After Redesign

```
VT parser (reader thread)
  → Video::print() → grid.getWritePointer(screen, row) → Cell* write
  → State atomics set (cursorRow, cursorCol, modes, etc.)

Timer flush (60Hz, message thread)
  → State::flush() → ValueTree properties

Display::resized() (message thread)
  → Cell::Rectangle(Font::bounds, componentBounds) → cols, rows
  → state.setDimensions(cols, rows)                  [sole dimension author]
  → State flush → ValueTree

Screen::valueTreePropertyChanged() (message thread)
  → dimension change detected?
      YES → transitioner.set(id::resize, cols, rows)
          → registered trigger fires:
              1. captureSnapshot()                    [previous = current]
              2. grid.resizeHeight(newRows, cursorRow) [mutate current]
              3. grid.resizeCols(newCols)              [mutate current]
              4. cursor clamp → state.storeValue()     [atomic]
              5. numRows sync → state.setValue()        [message thread VT write]
              6. scrollOffset reset → state.setValue()
          → crossfadePosition = 0, isTransitioning = true
          → timer starts (16ms ticks)
          → on settle: triggers.get(id::resizeEnd)
              → Processor handler → tty->platformResize() → SIGWINCH
      NO → normal render path:
          → grid.getBlock(activeScreen, scrollOffset, viewportRows)
          → setText(Block<Cell>)
          → Arrangement::shape(subBlock, font, wrapColumns, lineOffset)
          → ContentView::setSize(wrapWidth, contentHeight)
          → viewport->setViewPosition(0, bottom)     [if scrollOffset == 0]
          → repaint()

During transition (isInTransition()):
  → Screen reads from `previous` snapshot at current pixel width
  → Arrangement::shape() wraps at current wrapColumns
  → Visual continuity — content never garbled

After transition:
  → Screen reads from live grid
  → Shell has received SIGWINCH, redraws at new dimensions
```

### Thread Safety — APVTS Pattern

Identical to audio plugin with APVTS. Cell is 8-byte `uint64_t` — trivially copyable, naturally atomic on x86-64 and ARM64. Buffer<Cell> is dumb storage. State uses `std::atomic` for all cross-thread values.

| Thread | Reads | Writes |
|---|---|---|
| Reader (Video) | State atomics (dimensions) | Grid cells, State atomics (cursor, modes) |
| Message (Screen) | Grid cells via Block<Cell>, State ValueTree | State ValueTree (scroll, cursor clamp on resize) |
| Message (Display) | State ValueTree | State (dimensions — sole author) |
| Timer (State) | Dirty atomics | ValueTree properties |

Unidirectional: Reader → atomics → timer flush → ValueTree → Message reads. No locks. No hacks. Same contract as Cross-Thread Data Contract in ARCHITECTURE.md.

---

## Affected Files — Comprehensive

### END Source (terminal-specific)

| File | Change |
|---|---|
| `Source/terminal/Grid.h` | Remove `reflow()`, `reflowFrom()`, `getRow()`, `getBuffer()`. Change buffer type to `jam::Buffer<jam::Cell>`. Update `getBlock()`, `getWritePointer()` return types. Add `resizeCols()`. |
| `Source/terminal/Grid.cpp` | Delete ~470 lines of static reflow helpers. Delete `reflow()`, `reflowFrom()` implementations. Add `resizeCols()` implementation. Update all method signatures for `Cell*` returns. |
| `Source/terminal/GridSizeTransition.h/cpp` | Delete entirely. Replaced by `jam::DiscreteStateTransition` + registered triggers in Screen. |
| `Source/terminal/Video.cpp` | `getWritePointer()` returns `Cell*`. All `row->cells[col]` → `cells[col]`. Delete `row->usedCols` writes (3 sites). Delete `row->flags \|= wrapped` (1 site). |
| `Source/terminal/VideoEdit.cpp` | Same `Row*` → `Cell*` mechanical change across 12+ sites. Delete `row->usedCols` write in `shiftCellsRight`. |
| `Source/terminal/VideoESC.cpp` | Same change in `escDispatchDEC` (DECALN). Delete `rowPtr->usedCols` write. |
| `Source/terminal/VideoCSI.cpp` | Same change in `scrollDown` fill path. |
| `Source/terminal/component/Screen.h` | Add: `jam::DiscreteStateTransition transitioner`, `jam::Buffer<Cell> previous`, snapshot metadata. Add: `captureSnapshot()`. Add: `setCaretShape(int)` (moved from TextEditor). Remove: `attach()` call. |
| `Source/terminal/component/Screen.cpp` | Rewrite `valueTreePropertyChanged`: dimension change → DST trigger, normal flush → `getBlock` + `setText(Block<Cell>)`. Register DST triggers in constructor. Remove `setViewportMode(proportional)`. Remove `setScrollRange()`, `attach()`. |
| `Source/terminal/component/Display.h/cpp` | `resized()`: read `viewport->getMaximumVisibleWidth()` for grid dimensions (already the case — `visibleWidth` in State node comes from TextEditor::resized which reads viewport bounds). Remove scrollbar-width subtraction if any. |
| `Source/terminal/Processor.h/cpp` | Remove `GridSizeTransition gridResize` member. Remove dimension-change handling from `valueTreePropertyChanged` (moves to Screen). Keep `resizeEnd` event handler registration for SIGWINCH delivery. Remove `Video&` from GridSizeTransition construction. |
| `Source/terminal/Session.h/cpp` | No change — Session owns Grid (passed to Processor as ref). Processor no longer owns GridSizeTransition. |
| `Source/terminal/State.h/cpp` | No structural change. `scrollOffset` atomic stays. |
| `Source/terminal/Identifier.h` | Add `id::resize`, `id::cellSize` if not already present (trigger names). |

### jam Library (generic, reusable)

| File | Change |
|---|---|
| `jam_core/buffer/jam_buffer.h` | No change — `Buffer<Cell>` non-FAM mode already works. |
| `jam_core/buffer/jam_block.h` | No change — `Block<Cell>` already works. |
| `jam_fonts/cell/jam_row.h` | Delete file. `jam::Row` removed entirely. |
| `jam_fonts/cell/jam_cell.h` | No change. |
| `jam_fonts/jam_font/glyph/jam_glyph_arrangement.h` | Remove `shape(Block<Row>, ...)` overloads (2). Keep `shape(Block<Cell>, ...)` overloads (2). |
| `jam_fonts/jam_font/glyph/jam_glyph_arrangement.cpp` | Remove `Block<Row>` extractor lambda and `shapeImpl` instantiation. Keep `Block<Cell>` path unchanged. |
| `jam_gui/text_editor/jam_text_editor.h` | Remove: `ViewportMode` enum, `setViewportMode()`, `setScrollRange()`, `attach()`, `scrollOffsetId`, `scrollCapacityId`, `scrollbarVisibleId`, `viewportMode` member, `jam::Scrollbar` member, `setCaretShape()`. Add: `TextEditorViewport` inner struct, `getWrapWidth()`, `checkLayout()`. Change: `setText(Block<Cell>)`, `content` type to `Block<Cell>`. |
| `jam_gui/text_editor/jam_text_editor.cpp` | Remove: proportional branches in `calc()`, `setViewportMode()`, `setScrollRange()`, `attach()`, scrollbar bounds in `resized()`. Add: `TextEditorViewport` construction, `getWrapWidth()`, `checkLayout()`. Update: `calc()` to single absolute path with bottom-pin. |
| `jam_gui/text_editor/jam_text_editor_content_view.cpp` | Remove: proportional branch in `shapeVisibleContent()`. Update: `shape(Block<Cell>, ...)` call with `wrapColumns` parameter. |
| `jam_gui/text_editor/jam_scrollbar.h/cpp` | No change — class stays (may be used elsewhere). Not used by TextEditor. |
| **NEW: `jam_gui/transition/jam_discrete_state_transition.h/cpp`** | New class: `jam::DiscreteStateTransition`. Generic SST for discrete state changes. Timer-driven crossfade, coalescing, registered triggers via Function::Map. |

---

## ARCHITECTURE.md Delta

### Sections to Update

- **Module Map → `Source/terminal/`**: Remove `GridResize.h/cpp`. Note `GridSizeTransition.h/cpp` deleted (replaced by `jam::DiscreteStateTransition`).
- **Module Map → `jam_gui/`**: Add `transition/jam_discrete_state_transition.h/cpp`.
- **Module Inventory**: Update Terminal module description. Add `jam::DiscreteStateTransition` to jam_gui module.
- **Key Data Types → Grid Ring Buffer**: Replace `jam::Buffer<jam::Row>` with `jam::Buffer<jam::Cell>`. Flat rectangular ring, stride = `numCols`. No FAM. No `Row` struct. `getWritePointer()` returns `Cell*`. `getBlock()` returns `Block<Cell>`.
- **Key Data Types → TextEditor**: Remove proportional mode description. Remove `jam::Scrollbar` reference. Single absolute mode with juce::Viewport subclass (`TextEditorViewport` with reentrant guard). `setText(Block<Cell>)`. `getWrapWidth()` from `viewport->getMaximumVisibleWidth()`.
- **Data Flow → Bulk data**: Replace `jam::Buffer<jam::Row>` with `jam::Buffer<jam::Cell>`. `Grid::getBlock()` returns `Block<Cell>`. Screen calls `setText(Block<Cell>)`.
- **Communication Contracts → Data → Component**: Screen reads Grid via `getBlock()` → calls `setText(Block<Cell>)` on itself. Screen owns `DiscreteStateTransition` for resize lifecycle.
- **Glossary → Grid**: Update — slimmed ring buffer manager, no reflow.
- **Glossary → GridResize**: Remove. Add **DiscreteStateTransition** — generic SST for discrete state changes, owned by Screen.
- **Glossary → Snapshot**: Update — `Screen::previous` snapshot buffer, owned by Screen, captured before resize mutation.
- **Design Patterns**: Add **DiscreteStateTransition pattern** — TETRIS SST applied to terminal resize. Same contract as `kuassa::dsp::SmoothStateTransition`: previous = current, apply, crossfade, coalescing. Registered triggers via `jam::Function::Map`.
- Remove `jam::Row` from all sections.

---

## BLESSED Compliance

- **B (Bound)** — `previous` snapshot owned by Screen for transition duration. Grid owns live ring. Screen borrows via `Block<Cell>` (non-owning). DST owns no domain-specific state — all in registered triggers. Lifetimes explicit.
- **L (Lean)** — Deletes ~500+ lines (reflow helpers, Row struct, proportional mode, fake scrollbar, GridSizeTransition hardwiring). DST is ~60 lines of generic code. Net deletion.
- **E (Explicit)** — `Arrangement::shape(block, font, wrapColumns, lineOffset)` — all parameters visible. Wrap width derived from `viewport->getMaximumVisibleWidth() / cellWidth`. No hidden state. DST triggers registered with named identifiers and typed arguments.
- **S (SSOT)** — `scrollOffset` in State is sole scroll authority. Cursor in State is sole cursor authority. Dimensions in State are sole dimension authority. `juce::Viewport` position is derived. No shadow state.
- **S (Stateless)** — Arrangement produces glyph positions fresh every frame at current wrap width. No reflow state accumulated. Grid is ring storage, no paragraph awareness. DST is a state machine with no domain knowledge.
- **E (Encapsulation)** — Grid: ring storage. Arrangement: glyph shaping. Screen: render + resize coordination. DST: transition lifecycle. Video: VT semantics. Display: dimensions. Each has one job. No hardwired cross-references in DST.
- **D (Deterministic)** — Same `Block<Cell>` + same `wrapColumns` → same glyph positions every call. No tmux history interaction. No ring-full edge cases. DST behavior determined solely by trigger registration and timing parameters.

---

## Open Questions

None. All decisions resolved in pre-RFC discussion:

| Decision | Resolution |
|---|---|
| Scroll direction on resize | Reset to 0 (bottom-pinned) — DST trigger clears `scrollOffset` |
| Alternate screen scroll | No scroll — `numRows[alternate]` always 0, viewport = screen bounds |
| Whelmed integration | Deferred — whelmed parser outputs `Block<Cell>` when ready |
| Viewport scroll position | `visibleAreaChanged()` → State write. State is SSOT. juce::Viewport position is derived. |
| GridSizeTransition Video& dependency | Eliminated — DST is generic, terminal logic in registered triggers |
| Cursor authorship on resize | Video is sole author. Screen clamps on message thread during resize — no race (no VT data until after SIGWINCH). |
| resizeHeight cursor logic | Stays in Grid. Grid adjusts `cursorRow` in-place, caller writes to State. |
| Column resize mechanism | `Grid::resizeCols()` — lossless row-copy, `min(oldCols, newCols)` per row, pad with `Cell::erase()` on grow. No join, no split. |
| Cross-row joining on upsize | Abandoned by design — tmux model discarded entirely |
| usedCols performance | Existing `Block<Cell>` extractor shapes full `numCols` per row. Codepoint-0 cells skip `shapeCodepoint` quickly (no glyph mapped → count=0 → no Entry). Existing behavior — not a regression. |
| Buffer thread safety | APVTS pattern — Cell is 8-byte trivially copyable, atomic on x86-64/ARM64. Same model as audio plugin. |
| DST snapshot ownership | Screen owns `previous` buffer and snapshot metadata. DST is generic — no domain-specific members. |
| Reentrant guard for Viewport | TextEditorViewport subclass with `bool reentrant` + `ScopedValueSetter`. Adopted from juce::TextEditor (juce_TextEditor.cpp:164-199). |
| Wrapping mechanism | Existing `wrapColumns` parameter in `buildArrangements`. Pass `getWrapWidth() / cellWidth`. No new overload needed. |
| DST settled trigger | Fired from `advanceCrossfade()` when `crossfadePosition >= 1.0`. Terminal registers `id::resizeEnd` handler for SIGWINCH delivery. |
| Grid stays or dissolved | Grid stays — slimmed ring buffer manager. Terminal-specific. Not absorbed into jam. |
| Buffer stays dumb | Yes — AudioBuffer analogue. No ring logic, no terminal logic. Already in jam. |
| Display sole dimension author | Already true — `Display::resized()` → `state.setDimensions()`. No change needed. |

---

## Handoff Notes

### For COUNSELOR — Reading Order

1. **TETRIS.md** (`~/Documents/Poems/kuassa/___lib___/codebase-for-dummies/docs/TETRIS.md`) — understand SST contract: previous = current, trivial copy, crossfade, coalescing, registered triggers.
2. **SmoothStateTransition** (`~/Documents/Poems/kuassa/___lib___/kuassa_dsp/utilities/kuassa_dsp_smooth_state_transition.h`) — production SST implementation. DST mirrors this structure exactly.
3. **ProcessorChain** (`~/Documents/Poems/kuassa/jreng-filter-strip/Source/ProcessorChain.cpp`, `ProcessorChainRegistration.cpp`) — how SST triggers are registered and fired in production code. `addTrigger` + `set` pattern.
4. **Grid.h/cpp** — current ring arithmetic (`physicalRow`, `head`, `ringMask`). Unchanged. Only stored type and stride change.
5. **GridSizeTransition.h/cpp** — current snapshot + crossfade mechanics. Moving to DST + Screen triggers.
6. **juce::TextEditor** (`JUCE/modules/juce_gui_basics/widgets/juce_TextEditor.cpp:164-199`) — `TextEditorViewport` reentrant guard pattern to adopt.
7. **jam_text_editor.h/cpp** + **jam_text_editor_content_view.cpp** — current TextEditor internals. Remove proportional mode, adopt viewport subclass.
8. **jam_glyph_arrangement.cpp** — existing `Block<Cell>` shape path. `wrapColumns` parameter. No new overload needed.
9. **Video.cpp, VideoEdit.cpp, VideoESC.cpp, VideoCSI.cpp** — all `getWritePointer()` callsites. Mechanical `Row*` → `Cell*` change.

### Implementation Sequencing Guidance

The rewrite touches four independent axes that can be sequenced to minimize breakage:

1. **Buffer<Cell> + Grid slim** (foundation) — delete Row, update Grid, update Video callsites. Compile-driven: Row deletion produces compiler errors at every site that needs updating.
2. **TextEditor cleanup** (independent of Grid) — remove proportional mode, add viewport subclass, update setText signature.
3. **DST extraction** (depends on #1 and #2) — create jam::DiscreteStateTransition, delete GridSizeTransition, wire Screen triggers.
4. **Ownership restructuring** (depends on #3) — move DST from Processor to Screen, update Processor::valueTreePropertyChanged, wire event handlers.

Each step should compile and run before proceeding to the next. Step 1 is the largest (most callsites). Steps 2-4 are structural but touch fewer files.
