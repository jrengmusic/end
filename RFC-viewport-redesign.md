# RFC — Viewport Redesign: CellBuffer + Screen as TextEditor
Date: 2026-05-08
Status: Ready for ARCHITECT resolution of open questions, then COUNSELOR

---

## Problem Statement

END's current viewport is built on the old terminal framework model:

- Parser writes directly into Grid's HeapBlocks via raw `memcpy` on the reader thread
- Screen reads Grid directly at VBlank, builds a GL snapshot, publishes to GLSnapshotBuffer
- Grid is a persistent addressable cell store (ring of `Lines<TerminalLine>`)
- Parser maintains its own `rowMapping` — a viewport-relative coordinate map rebuilt on every resize
- GUI resize mutates Grid via `resizeLock`, destroying scrollback content when the terminal grows narrower then wider

The symptom: only the active prompt line renders, replaced on each update rather than accumulated. Root cause: `onVBlank()` is empty — nothing connects Grid to the new `jam::TextEditor`-based Screen. The new Screen was forked but never wired.

The deeper problem: the entire topology couples Parser to viewport dimensions. Parser needs `cols` and `visibleRows` to write cells, manage scroll regions, and maintain `rowMapping`. GUI resize flows top-down through Parser, Grid, and Screen — violating the unidirectional data flow contract.

---

## Research Summary

### Codebase facts (grounded, no assumptions)

**Parser (terminal/logic/Parser.h, ParserEdit.cpp, ParserVT.cpp, Parser.cpp):**
- Holds `Grid::Writer writer` and `HeapBlock<Grid::Row> rowMapping`
- `rowMapping[visibleRow] = {lineIndex, cellOffset}` — maps viewport row to Grid Line segment
- `rowCells(visibleRow)` resolves through rowMapping to `writer.directLinePtr()` — raw Cell pointer into Grid's HeapBlock
- Edit ops (shiftLines, shiftCellsRight, removeCells, eraseCells, eraseInLine, eraseInDisplay) use `std::memcpy` directly on cell arrays
- `resize()` on Parser rebuilds `rowMapping`, reinitialises tab stops, clamps cursors
- Parser reads `cols` and `visibleRows` from State atomics for every spatial computation

**Grid (terminal/logic/Grid.h):**
- `Lines<TerminalLine>` ring — persistent addressable cell store
- `TerminalLine` owns `HeapBlock<jam::Cell>`, `HeapBlock<jam::Grapheme>`, `HeapBlock<uint16_t> linkIds`
- `Grid::Writer` exposes `directLinePtr()` — gives Parser raw pointer into the HeapBlock
- `scrollbackVisualRows` on `Lines` tracks history visual row count
- Dirty tracking: `std::atomic<uint64_t> dirtyRows[4]` (256-bit bitmask), seqno, scrollDelta

**State (terminal/data/State.h):**
- APVTS pattern: reader thread writes atomics, timer flushes to ValueTree, message thread reads ValueTree
- Holds cursor (row, col, shape, visibility), modes, scroll region, title, cwd, scrollbackUsed
- `getScrollPosition()` — pixel offset into scrollback
- `consumeSnapshotDirty()` — atomic exchange, render trigger
- All scalar/sparse terminal state. SSOT for cursor and modes.

**Screen — current (terminal/rendering/Screen.h):**
- Existing: reads Grid directly at VBlank, builds GL snapshot, publishes to GLSnapshotBuffer
- ARCHITECTURE.md: "reads Grid directly every frame, no cell cache"

**Screen — new (terminal/rendering/Screen.h):**
- `class Screen : public jam::TextEditor`
- Constructor calls `setText(jam::Cells::fromArray(pens, penCount))` — static test data
- `onVBlank()` body is empty — no Grid → Screen wiring exists
- `cellsContent` (jam::Cells) + `cellsShapedText` (jam::Glyph::ShapedText) — the Cells render path
- `reshapeCellsContent()` — recomputes cell metrics from font, shapes via `cellsShapedText.shape()`
- `resized()` calls `reshapeCellsContent()` — reflow on every resize from accumulated `cellsContent`

**jam::TextEditor (jam_gui/text_editor/):**
- Two content paths, mutually exclusive in `drawContent()`:
  - **Cells path**: `setText(Cells&&)` → `cellsShapedText` → GPU draw via `glyphGraphics`
  - **String path**: `insert()`/`remove()`/`replaceText()` → `textStorage` → JUCE paragraph model
- `cellsContent` is `jam::Cells` (move-only, private HeapBlock)
- `reshapeCellsContent()` recomputes `wrapColumns = getMaximumTextWidth() / cellsCellSize.width`
- Rendering: `jam::Glyph::Graphics` + `jam::Typeface::getAtlas()` — hooks into JUCE's native GL pipeline
- No `appendCells()` API exists today — only full-replace `setText(Cells&&)`

**jam::Cells (jam_tui/cell/):**
- `HeapBlock<Cell> cells`, `HeapBlock<Grapheme> graphemes`, `int count`
- Move-only, non-copyable
- Factory: `fromUTF8()`, `fromArray(const Cell*, int)`
- `getGrapheme(index)` — sparse grapheme sidecar
- No `append()` method exists today

**ARCHITECTURE.md — key decisions:**
- Previous SPSC FIFO rejection: "The FIFO added a drain step on the message thread that was unnecessary — the parser is fast enough to run on the reader thread without blocking." This decision was for the OLD architecture (hand-rolled, micro-managed, no jam). It does not apply to the new JUCE-native topology.
- "JUCE native OpenGL pipeline — we only intercept where necessary: Glyph, SIMD/NEON rasterization." Everything else trusts the framework.
- "Classification rule: if the data is one-per-cell (O(rows × cols)), it is bulk → Grid HeapBlock. If sparse/scalar → State ValueTree."

---

## Principles & Rationale

### Core contract

**GUI interaction (resize) never mutates Grid. Data flow is always unidirectional.**

Parser does not know what Screen renders. Screen does not know what Parser emits. Grid (CellBuffer) is the crossing point — transient, directional, self-managed.

```
Reader thread:   Parser → CellBuffer (SPSC FIFO)
Message thread:  Display drains CellBuffer on VBlank → Screen.applyOps()
                 Screen (TextEditor) owns all content, history, layout
                 JUCE renders via native GL pipeline
                 Interceptions: jam::Glyph::ShapedText (atlas), SIMD rasterization
State:           cursor, modes, title — existing APVTS pattern, unchanged
Resize:          Screen reflows from cellsContent → reshapeCellsContent()
                 Display sends SIGWINCH to shell (shell notification only)
```

### Why CellBuffer is not shadow state

Grid-as-persistent-store AND TextEditor's `cellsContent` would be shadow state — same truth in two places. Grid-as-transient-FIFO is not shadow state — it is a queue that empties into the authoritative store. After drain, CellBuffer is empty. TextEditor's `cellsContent` is the sole SSOT for cell data on the GUI side.

### Why this design is BLESSED-compliant

- **B**: CellBuffer owns its lifecycle. TextEditor owns cellsContent. Parser never touches TextEditor. No cross-thread direct calls.
- **L**: No speculative abstraction. CellBuffer is AbstractFifo + HeapBlock. Nothing else.
- **E**: CellOp carries all information needed to apply it. No implicit context.
- **S1**: `cellsContent` is SSOT for cell data. State is SSOT for cursor/modes. No duplication.
- **S2**: CellBuffer is stateless between drains. TextEditor is stateless about what came from Parser.
- **E2**: Parser emits ops. Screen applies them. No ask-and-act, only tell.
- **D**: Same Parser output → same TextEditor content. Deterministic.

### Why the FIFO was rejected before and why it applies now

Previous rejection: hand-rolled micro-managed pipeline with no jam, no JUCE framework trust. The drain step was costly because Screen had to rebuild a GL snapshot from scratch on every drain cycle.

New topology: TextEditor IS the render substrate. It manages its own redraw via `repaint()`. Draining CellBuffer calls `appendCells()` or `applyOp()` on TextEditor — this is a cell accumulation, not a snapshot rebuild. JUCE handles the GL path. The drain step cost is negligible.

---

## Scaffold

### 1. CellOp — semantic op type

Based on actual Parser operations (ParserEdit.cpp, ParserVT.cpp, ParserOps.cpp):

```cpp
// terminal/data/CellOp.h
namespace Terminal
{

struct CellOp
{
    enum class Type : uint8_t
    {
        Print,           // jam::Cell at cursor position (from State)
        LineFeed,        // LF/VT/FF — advance cursor, scroll if at region bottom
        CarriageReturn,  // CR — col → 0
        Tab,             // HT — advance to next tab stop
        Backspace,       // BS — col - 1
        EraseInLine,     // EL — mode 0 (to right) / 1 (to left) / 2 (entire)
        EraseInDisplay,  // ED — mode 0/1/2/3
        InsertLines,     // IL CSI L — count lines
        DeleteLines,     // DL CSI M — count lines
        InsertChars,     // ICH CSI @ — count cells
        DeleteChars,     // DCH CSI P — count cells
        EraseChars,      // ECH CSI X — count cells
        SetScreen,       // switch normal ↔ alternate
        ClearScrollback, // ED mode 3
    };

    Type         type    { Type::Print };
    jam::Cell    cell    {};       // valid for Print
    juce::Colour fillBg  {};      // erase/shift ops — stamp.bg at emission time
    int          param   { 0 };   // mode (erase ops) or count (insert/delete)
};

} // namespace Terminal
```

### 2. Terminal::CellBuffer — SPSC FIFO

Replaces `Grid` in the live render path. Owned by `Processor`, drained by `Display`.

```cpp
// terminal/logic/CellBuffer.h
namespace Terminal
{

class CellBuffer
{
public:
    explicit CellBuffer (int capacity = 4096) noexcept
        : fifo (capacity)
        , ops (capacity)
    {
        ops.allocate (capacity, false);
    }

    // READER THREAD — Parser pushes one op at a time
    void push (const CellOp& op) noexcept
    {
        int start, count;
        fifo.prepareToWrite (1, start, count);

        if (count > 0)
        {
            ops[start] = op;
            fifo.finishedWrite (1);
        }
    }

    // MESSAGE THREAD — Display drains all pending ops
    // Returns number of ops drained into outOps
    int drain (CellOp* outOps, int maxOps) noexcept
    {
        int start1, count1, start2, count2;
        fifo.prepareToRead (maxOps, start1, count1, start2, count2);

        const int total { count1 + count2 };

        if (count1 > 0)
            std::memcpy (outOps, ops.getData() + start1, sizeof (CellOp) * (size_t) count1);

        if (count2 > 0)
            std::memcpy (outOps + count1, ops.getData() + start2, sizeof (CellOp) * (size_t) count2);

        fifo.finishedRead (total);
        return total;
    }

    bool isEmpty() const noexcept { return fifo.getNumReady() == 0; }

private:
    juce::AbstractFifo          fifo;
    juce::HeapBlock<CellOp>     ops;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CellBuffer)
};

} // namespace Terminal
```

### 3. jam::Cells::append() — new API (jam_tui)

Required addition to `jam_tui/cell/jam_cells.h` and `jam_cells.cpp`:

```cpp
// jam_cells.h — add to Cells class
void append (Cells&& other) noexcept;

// jam_cells.cpp
void Cells::append (Cells&& other) noexcept
{
    if (other.count == 0)
        return;

    const int newCount { count + other.count };

    juce::HeapBlock<Cell> newCells;
    newCells.malloc (newCount);

    if (count > 0)
        std::memcpy (newCells.getData(), cells.getData(), sizeof (Cell) * (size_t) count);

    std::memcpy (newCells.getData() + count,
                 other.cells.getData(),
                 sizeof (Cell) * (size_t) other.count);

    cells = std::move (newCells);

    // graphemes sidecar — parallel HeapBlock, offset indices by old count
    if (other.graphemes.getData() != nullptr)
    {
        juce::HeapBlock<Grapheme> newGraphemes;
        newGraphemes.calloc (newCount);

        if (graphemes.getData() != nullptr)
            std::memcpy (newGraphemes.getData(), graphemes.getData(), sizeof (Grapheme) * (size_t) count);

        std::memcpy (newGraphemes.getData() + count,
                     other.graphemes.getData(),
                     sizeof (Grapheme) * (size_t) other.count);

        graphemes = std::move (newGraphemes);
    }

    count = newCount;
}
```

### 4. jam::TextEditor — new APIs (jam_gui)

Two additions to `jam_text_editor.h` and `jam_text_editor.cpp`:

```cpp
// Accumulates cells into cellsContent without full reshape.
// Caller must call reshapeCellsContent() + repaint() when batch is complete.
void appendCells (jam::Cells&& cells) noexcept;

// Exposes mutable raw cell pointer for in-place ops (erase, shift).
// Caller must call cellsModified() after mutations.
jam::Cell* getMutableCells() noexcept;
int getCellsCount() const noexcept;

// Signals that cellsContent was mutated externally.
// Calls reshapeCellsContent() + repaint().
void cellsModified() noexcept;
```

Implementation:

```cpp
void TextEditor::appendCells (jam::Cells&& newCells) noexcept
{
    cellsContent.append (std::move (newCells));
}

jam::Cell* TextEditor::getMutableCells() noexcept
{
    return cellsContent.getMutableData(); // requires HeapBlock accessor on Cells
}

int TextEditor::getCellsCount() const noexcept { return cellsContent.size(); }

void TextEditor::cellsModified() noexcept
{
    reshapeCellsContent();
    repaint();
}
```

**Note:** `jam::Cells` also needs `Cell* getMutableData() noexcept` added — returns raw HeapBlock pointer for Screen's in-place mutation ops.

### 5. Terminal::Screen — op application

Screen subclasses `jam::TextEditor`. On `applyOps()`, it drains the batch and applies each op. Append-only ops (Print, LineFeed, CarriageReturn, Tab, Backspace) accumulate into a `jam::Cells` batch then call `appendCells()` once per drain cycle. Mutation ops (erase, shift) call `getMutableCells()` + `cellsModified()`.

Screen reads cursor position from State when placing Print cells. Screen owns the visual-row-to-cell-index mapping (what `rowMapping` was in Parser). Screen knows its own `cols` and `rows` — it IS the viewport authority.

Alternate screen: Screen owns two `jam::Cells` instances (`cellsNormal`, `cellsAlternate`). `SetScreen` op swaps which is active. `setText()` on TextEditor receives the active one.

### 6. Display — drain loop

```cpp
void Terminal::Display::onVBlank()
{
    const bool dirty { state.consumeSnapshotDirty() };

    if (dirty)
    {
        state.refresh();

        // Drain all pending ops from CellBuffer into Screen
        CellOp opBatch[256];
        int count { 0 };

        while ((count = processor.getCellBuffer().drain (opBatch, 256)) > 0)
            screen.applyOps (opBatch, count);
    }
}
```

### 7. Parser changes

Parser stops writing to Grid's HeapBlocks. Instead, `Grid::Writer` is replaced with `CellBuffer&`. Every cell write becomes a `CellOp::Print` push. Every erase becomes a `CellOp::EraseInLine` or `CellOp::EraseInDisplay` push. Every lineFeed becomes a `CellOp::LineFeed` push.

`rowMapping` is deleted — Parser no longer maps viewport rows to Grid Lines. Parser tracks cursor via State (already does this). Parser still needs `cols` for tab stop table and cursor clamping in CUP. `visibleRows` is no longer needed by Parser for cell placement — Screen owns that.

`Grid::Writer` is deleted. `Grid.h` / `Grid.cpp` / `GridScroll.cpp` / `GridErase.cpp` / `GridReflow.cpp` are removed or archived.

`Processor` replaces `Grid grid` with `CellBuffer cellBuffer`.

---

## BLESSED Compliance Checklist

- [x] **Bounds** — CellBuffer owned by Processor. TextEditor (Screen) owned by Display. Parser never outlives CellBuffer. No cross-thread direct calls.
- [x] **Lean** — CellBuffer is AbstractFifo + HeapBlock. No speculative abstraction. CellOp is a flat 24-byte struct.
- [x] **Explicit** — CellOp carries all context (fillBg, param) at emission time. No implicit state carried across op boundaries.
- [x] **SSOT** — `cellsContent` is SSOT for cell data. State is SSOT for cursor/modes. CellBuffer is transient — empties after drain. No duplication.
- [x] **Stateless** — CellBuffer has no memory of what it held. Screen applies ops and moves on. Parser holds no render state.
- [x] **Encapsulation** — Parser emits ops, knows nothing of Screen. Screen applies ops, knows nothing of Parser. Display orchestrates, tells both sides.
- [x] **Deterministic** — Same byte stream → same CellOp sequence → same cellsContent. Resize does not change the cell sequence, only the visual layout.

---

## Open Questions

ARCHITECT must resolve before COUNSELOR begins Sprint A.

**Q1 — Alternate screen ownership:**
Two options:
- (A) Screen owns `cellsNormal` + `cellsAlternate` as separate `jam::Cells` members. `SetScreen` op swaps which is active. TextEditor receives whichever is active via `setText()`.
- (B) Screen owns two full `jam::TextEditor` instances. `SetScreen` swaps which is the active child component.

Option A is lighter but requires `setText()` on every screen switch (full reshape). Option B is heavier but each TextEditor manages its own state independently. **ARCHITECT to decide.**

**Q2 — Mutation API on jam::Cells:**
EraseInLine, InsertLines, DeleteLines, InsertChars, DeleteChars, EraseChars all require in-place mutation of `cellsContent`. Two options:
- (A) `Cells::getMutableData()` — raw HeapBlock pointer. Screen applies mutations directly. Screen calls `cellsModified()` when done. Lean and explicit.
- (B) `Cells::applyOp(CellOp, int cursorRow, int cursorCol, int cols)` — pure function, returns new Cells. No mutation, immutable data path. Heavier allocation cost per op batch.

**ARCHITECT to decide.**

**Q3 — Tab stop authority:**
Parser currently owns tab stops (`std::vector<char> tabStops`, `initializeTabStops(cols)`). Tab stop position depends on `cols`. Options:
- (A) Parser retains tab stops — needs `cols` from State for tab advance computation. Emits `CellOp::Tab` with the resolved target column as `param`.
- (B) Screen owns tab stops — Parser emits `CellOp::Tab` with no param. Screen resolves tab position using its own `cols`.

Option A keeps tab stop logic in Parser (where the VT spec puts it). Option B is purer but splits tab semantics. **ARCHITECT to decide.**

**Q4 — Scroll region with LineFeed:**
`CellOp::LineFeed` needs scroll region context (scrollTop, scrollBottom) to know whether to scroll. Options:
- (A) Parser embeds scroll region bounds into `CellOp::LineFeed.param` at emission time. Screen uses them directly.
- (B) Screen reads scroll region from State at drain time.

Option A is explicit — state at emission time is captured. Option B risks a race if State flushes between emission and drain (unlikely but possible). **ARCHITECT to decide — lean toward A.**

**Q5 — CellBuffer capacity:**
Default capacity of 4096 ops covers typical parse bursts. Under paste or fast TUI output this may overflow (ops silently dropped). Options:
- (A) Fixed capacity, overflow drops ops (current scaffold). Simple, bounded.
- (B) Dynamic growth when overflow detected. More complex, unbounded.

**ARCHITECT to decide.**

---

## Handoff Notes

### What COUNSELOR must implement

**Sprint A — jam layer (jam_tui + jam_gui):**
1. `jam::Cells::append(Cells&&)` — as scaffolded above
2. `jam::Cells::getMutableData()` — raw pointer accessor (if Q2 resolved as option A)
3. `jam::TextEditor::appendCells(Cells&&)`
4. `jam::TextEditor::getMutableCells()` + `getCellsCount()` + `cellsModified()`

**Sprint B — CellBuffer:**
1. `terminal/data/CellOp.h` — struct as scaffolded
2. `terminal/logic/CellBuffer.h` — AbstractFifo wrapper as scaffolded
3. `Processor` replaces `Grid` with `CellBuffer`

**Sprint C — Parser refactor:**
1. Replace `Grid::Writer writer` with `CellBuffer& cellBuffer`
2. Delete `rowMapping`
3. Replace all `rowCells()` / `memcpy` ops with `cellBuffer.push(CellOp{...})`
4. `print()` → push `CellOp::Print`
5. `executeLineFeed()` → push `CellOp::LineFeed`
6. `execute(CR)` → push `CellOp::CarriageReturn`
7. `eraseInLine()` → push `CellOp::EraseInLine`
8. `eraseInDisplay()` → push `CellOp::EraseInDisplay`
9. `shiftLines()` → push `CellOp::InsertLines` / `CellOp::DeleteLines`
10. `shiftCellsRight()` → push `CellOp::InsertChars`
11. `removeCells()` → push `CellOp::DeleteChars`
12. `eraseCells()` → push `CellOp::EraseChars`
13. `setScreen()` → push `CellOp::SetScreen`
14. Delete `Grid.h`, `Grid.cpp`, `GridScroll.cpp`, `GridErase.cpp`, `GridReflow.cpp`
15. Remove `Processor::resized()` call to `parser.resize()` for `visibleRows` — Screen owns that now

**Sprint D — Screen (Terminal::Screen) op application:**
1. Screen owns cell buffer state for normal + alternate (per Q1 decision)
2. `applyOps(const CellOp*, int count)` — applies batch, calls `appendCells()` or `getMutableCells()` per op type
3. Screen reads cursor from State at drain time for Print cell placement
4. Screen owns `cols` and `rows` — set by Display on resize
5. Screen's `resized()` calls `reshapeCellsContent()` — reflow without content loss

**Sprint E — Display wiring:**
1. `onVBlank()` drain loop as scaffolded
2. `Display::resized()` tells Screen new cols/rows, then sends SIGWINCH via `processor.onResize`

### Constraints for COUNSELOR

- One file write per turn when producing multiple files
- Read MANIFESTO.md and this RFC before writing any code
- BLESSED: no guard without a named threat, no getter without a proven caller, no parallel channel where State already exists
- `jam_tui` and `jam_gui` are separate repos — coordinate file location carefully
- Do not touch `terminal/rendering/ScreenRender.cpp` or `ScreenSnapshot.cpp` until Sprint D is complete — the old GL path must remain functional during transition
- `State` is unchanged — cursor, modes, scroll region stay exactly as-is
- Parser still calls `state.setCursorRow()`, `state.setCursorCol()` etc. — that path is not touched

### Prior decisions that stand

- JUCE native GL pipeline — TextEditor renders through `jam::Glyph::Graphics` + JUCE. No hand-rolled shaders for text.
- State APVTS pattern — scalar/sparse data flows through State. Cell data flows through CellBuffer. Classification rule is unchanged.
- VBlankAttachment render trigger — unchanged. Display polls `consumeSnapshotDirty()` every vsync.
- `resizeLock` on Grid — eliminated along with Grid. No lock needed between Parser and TextEditor because CellBuffer mediates the boundary.
