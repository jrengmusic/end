# RFC — Logical-Line Grid Architecture
Date: 2026-05-03
Status: Ready for COUNSELOR handoff
Supersedes: RFC-proportional-layout.md

## Problem Statement

Grid is both Parser write target and renderer read source. Reflow on resize mutates the cell buffer — copying cells to new positions at the new column width. This is architecturally wrong.

Evidence: after resize, rendering shows misplaced rows/columns or erased rows. LinkIds survive (phantom links — clickable at correct positions despite visual corruption), proving the source data is intact but the rendering diverged from it.

The terminal grid is structurally identical to a text document with block-level wrapping:
- Parser output = document content (sequence of cells with hard line breaks)
- Visual rows = wrapping view (determined by viewport width)
- Resize = recompute wrapping (zero content mutation)

The current Grid stores cells in a 2D fixed-width array indexed by (physicalRow, col). This conflates document content with presentation. Reflow tears apart the content to reshape the presentation — the fundamental SSOT violation.

## Research Summary

### JUCE TextEditor (validates this model)
- Document: `ParagraphStorage` holds content between hard breaks. Immutable once set.
- Visual: `ShapedText` — lazy computed cache, invalidated on resize. Content never mutated.
- Wrap: greedy accumulation of `glyph.advance.getX()` until exceeding `wordWrapWidth`, break.
- On resize: `clearShapedTexts()` → lazy reshape. Zero content mutation.

### Industry terminal emulators (Alacritty, WezTerm, kitty, foot)
- ALL store physical rows with wrap-continuation bit. Logical lines reconstructed on demand.
- ALL mutate grid on reflow (flatten → re-split → write back).
- None uses logical-line as SSOT. This design would be the first correct implementation.

### Text editors (Zed, xi-editor)
- Both separate document content from visual presentation.
- Zed: `WrapMap` — `SumTree<Transform>` mapping input ranges to wrapped visual rows.
- xi-editor: wrap = pure function `(string, widths) → break_points`. Incremental.

### Universal wrapping algorithm
- Greedy first-fit. O(N) single pass. Used by every terminal and most text editors.
- Input: block widths + viewport width. Output: break indices.
- UAX #14 handles break opportunity discovery (where CAN you break). Width accumulation handles break SELECTION (where DO you break). Two orthogonal concerns.
- DirectWrite's `DWRITE_CLUSTER_METRICS` is the cleanest reference: `{width, canWrapLineAfter}` per cluster, greedy walker sums widths.

### Key insight
Wrapping logic does not care about cell width. It is pure math. Whether cells render at uniform width (monospace) or variable width (proportional), the wrapping algorithm is identical — same function, different width inputs. The function lives at JAM level, generic across all consumers.

## Principles and Rationale

### Three-layer architecture

```
Layer 1: CONTENT  — Grid (document)
Layer 2: WRAPPING — jam::Wrap (pure function: widths + lineWidth → break points)
Layer 3: RENDERING — Layout (pure function: cells + packer → per-cell X pixel positions)
```

Each layer is unidirectional. Each layer's input is the layer above's output. No reverse dependencies. No layer knows about the layers below it.

### Why this direction

The current Grid conflates all three layers in one 2D array. Parser writes content into the presentation surface. Reflow mutates the content to reshape the presentation. LinkIds prove the violation — they're placed by Parser at content positions, and reflow moves them.

Separating layers means:
- Parser writes to Layer 1 (content). Immutable once written.
- Resize recomputes Layer 2 (wrapping). Zero content mutation.
- Renderer reads Layer 1 through Layer 2 (content via view). Proportional rendering is purely Layer 3.
- LinkIds never move. SSOT.

### BLESSED pillar mapping

- **B** — Line owns its cells. Lines owns Lines. Row references by index. Clear lifecycle. Ring recycles oldest lines — deterministic.
- **L** — Three small structs. Reflow reduces from 450 lines to ~15. No new classes beyond the three named types.
- **E** — Row explicitly maps visual → logical. `directRowPtr` explicitly returns Line's cell array at offset. All parameters visible. Wrapping function signature: all inputs visible, pure.
- **S (SSOT)** — Line IS the truth. Row is computed. No shadow state. No copy masquerading as source. LinkIds stay where Parser placed them.
- **S (Stateless)** — Wrapping is pure function of Lines + width. No state between resizes. No memory of previous layout.
- **E (Encap)** — Parser writes to Lines via Writer. Renderer reads Lines via Row. Wrapping function has no knowledge of Grid, Parser, or rendering.
- **D** — Same Lines + same widths = same Row mapping. Same cells + same font = same advances. Always.

## Scaffold

### Layer 1: Content — Grid

#### Grid::Line

The storage primitive. A logical line — variable-length sequence of cells between hard line breaks (LF). Everything the Parser wrote on one line.

```cpp
struct Line
{
    juce::HeapBlock<Cell> cells;
    juce::HeapBlock<Grapheme> graphemes;
    juce::HeapBlock<uint16_t> linkIds;
    int length { 0 };      // cells written (content length)
    int capacity { 0 };    // allocated (for reuse without realloc)

    /// Grow capacity if needed. Reuses existing allocation when possible.
    void ensureCapacity (int needed) noexcept;

    /// Reset length to zero. Keep allocation for reuse.
    void reset() noexcept;
};
```

#### Grid::Lines

Ring buffer of Line objects. The document. Manages scrollback lifecycle.

```cpp
struct Lines
{
    juce::HeapBlock<Line> lines;
    int capacity { 0 };             // power of two (for mask arithmetic)
    int mask { 0 };                 // capacity - 1
    int head { 0 };                 // index of newest line
    int count { 0 };                // active lines in ring
    int scrollbackVisualRows { 0 }; // running total for capacity check

    Line& at (int index) noexcept;
    const Line& at (int index) const noexcept;

    /// Advance ring: recycle oldest line, return slot for new line.
    Line& advance() noexcept;
};
```

Scrollback capacity measured in visual rows (same config numbers as current). `scrollbackVisualRows` tracks total visual row coverage. When a new line is added and total exceeds configured capacity, oldest lines are recycled.

#### Grid::Row

Per-visible-row mapping entry. The computed view — where a visual row's content lives in the document.

```cpp
struct Row
{
    int lineIndex { 0 };    // index into Lines ring
    int cellOffset { 0 };   // starting cell within the Line for this visual row
};
```

The visible area: `Row visibleRows[numVisibleRows]`.

#### Grid structure

```cpp
class Grid
{
    // ... (resizeLock, state, dirty tracking — unchanged)

    std::array<Lines, 2> buffers;   // normal + alternate (same dual-screen pattern)
    Row* visibleRows { nullptr };   // computed view, size = numVisibleRows
    int numVisibleRows { 0 };

    // Writer facade (interface preserved for Parser)
    class Writer { /* ... */ };
};
```

### Layer 2: Wrapping — jam::Wrap

Pure function. Lives in JAM (shared library). No knowledge of Grid, Terminal, or rendering.

```cpp
namespace jam::Wrap
{

/// Greedy first-fit line breaking. Pure function.
///
/// Returns: number of visual lines the content spans.
///
/// breaks[i] = cell index where visual line i+1 starts.
/// breaks array must hold at least (blockCount - 1) entries (worst case: every cell wraps).
///
/// For terminal mono:         widths = {1,1,1,2,1,...}, lineWidth = numCols
/// For terminal proportional: widths = {0.5,1.2,0.8,...}, lineWidth = viewportPixels
/// For any other consumer:    whatever widths and lineWidth make sense
///
int computeBreaks (const float* widths, int blockCount,
                   float lineWidth,
                   int* breaks, int maxBreaks) noexcept;

} // namespace jam::Wrap
```

Implementation:

```cpp
int jam::Wrap::computeBreaks (const float* widths, int blockCount,
                              float lineWidth,
                              int* breaks, int maxBreaks) noexcept
{
    float accumulated { 0.0f };
    int numBreaks { 0 };

    for (int i { 0 }; i < blockCount; ++i)
    {
        if (accumulated + widths[i] > lineWidth)
        {
            if (numBreaks < maxBreaks)
                breaks[numBreaks] = i;

            ++numBreaks;
            accumulated = widths[i];
        }
        else
        {
            accumulated += widths[i];
        }
    }

    return numBreaks + 1;
}
```

Terminal mono fast path (no float array needed):

```cpp
/// Integer fast path. When all cells have uniform width (1 or 2 columns max),
/// visual row count = ceil(lineLength / numCols). Breaks at numCols intervals.
/// Wide chars at boundary: handled by Parser at write time (fills last col, wraps).
int computeBreaksMono (int lineLength, int numCols) noexcept;
```

### Layer 3: Rendering — Layout

Pure function. Computes per-cell X pixel positions within a visual row. This is where proportional rendering lives.

```cpp
namespace Terminal::Layout
{

/// Populate accumulated X advances for a visual row of cells.
/// advances[col] = X origin of cell at column col, in physical pixels.
/// advances[numCells] = total row width (sentinel for last cell's right edge).
///
/// Monospace: proportional=false → every cell gets physCellWidth * cell.width. Uniform.
/// Proportional: proportional=true → ASCII 0x20-0x7E use Region::xAdvance. Everything else uniform.
void computeRowAdvances (float* advances,
                         const Cell* cells,
                         int numCells,
                         float physCellWidth,
                         const jam::Glyph::Packer& packer,
                         bool proportional) noexcept;

} // namespace Terminal::Layout
```

Logic (from RFC-proportional-layout, unchanged):

```cpp
float x { 0.0f };

for (int col { 0 }; col < numCells; ++col)
{
    advances[col] = x;

    const auto& cell { cells[col] };
    const auto cp { cell.codepoint };

    if (proportional && cp >= 0x20 && cp <= 0x7E)
    {
        const auto* region { packer.findCached (/* glyph key for cp */) };

        if (region != nullptr)
            x += region->xAdvance;
        else
            x += physCellWidth;
    }
    else
    {
        x += physCellWidth * cell.width;
    }
}

advances[numCells] = x;
```

### Write Path (Parser → Content)

Parser writes at cursor (row, col). The visual row map translates to document position.

```
1. visibleRows[cursorRow] → { lineIndex, cellOffset }
2. Cell position in Line = cellOffset + cursorCol
3. Write: lines.at(lineIndex).cells[cellOffset + cursorCol] = cell
```

Hot path (`processGroundChunk`):

```cpp
// directRowPtr(row) returns pointer into Line's cell array at visual row offset.
// Inner loop: linePtr[col] = cellTemplate — identical to current.
// Row change: one map lookup to update linePtr.
Cell* Grid::directRowPtr (int visibleRow) noexcept
{
    const auto& row { visibleRows[visibleRow] };
    auto& line { bufferForScreen().at (row.lineIndex) };
    line.ensureCapacity (row.cellOffset + cols);
    return line.cells.get() + row.cellOffset;
}
```

The inner write loop is structurally identical to today. Zero overhead on the hot path (one indirection per row change, not per character).

LF handling:
1. Current Line finalized (length set to cursor position or existing length, whichever is greater)
2. New Line created via `lines.advance()` (recycles oldest if ring full)
3. If at bottom of scroll region: viewport advances, top line enters scrollback
4. Recompute visibleRows for affected entries

Auto-wrap:
- Cursor reaches numCols → Line keeps growing (cellOffset advances past numCols)
- visibleRows naturally maps the next visual row to the same Line at higher cellOffset
- No explicit wrap flag needed — wrapping is implicit from Line.length > numCols

### Read Path (Renderer ← View)

Renderer iterates visible rows:

```cpp
for (int r { 0 }; r < numVisibleRows; ++r)
{
    const auto& row { grid.visibleRows[r] };
    const auto& line { grid.lines().at (row.lineIndex) };

    const Cell* cells { line.cells.get() + row.cellOffset };
    const Grapheme* graphemes { line.graphemes.get() + row.cellOffset };
    const uint16_t* linkIds { line.linkIds.get() + row.cellOffset };

    const int cellsInRow { juce::jmin (numCols, line.length - row.cellOffset) };

    // Layer 3: compute pixel positions
    Layout::computeRowAdvances (advances, cells, cellsInRow, physCellWidth, packer, proportional);

    // Render using advances table...
}
```

Scrollback reading: walk backwards through Lines ring from viewport anchor, computing visual row span per Line (`jam::Wrap::computeBreaksMono(line.length, numCols)` for mono).

### Resize Path

The entire point. On column count change:

```cpp
void Grid::resize (int newCols, int newVisibleRows)
{
    const juce::ScopedLock lock (resizeLock);

    // 1. Compute cursor's logical position (stable across resize)
    const auto& oldRow { visibleRows[cursorRow] };
    const int cursorLineIndex { oldRow.lineIndex };
    const int cursorCellPos { oldRow.cellOffset + cursorCol };

    // 2. Reallocate visibleRows array if height changed
    if (newVisibleRows != numVisibleRows)
    {
        /* realloc visibleRows to newVisibleRows entries */
    }

    // 3. Recompute visibleRows from Lines + new numCols
    recomputeVisibleRows (newCols, newVisibleRows, cursorLineIndex, cursorCellPos);

    // 4. Derive new cursor visual position from logical position
    /* find cursorLineIndex in new visibleRows, compute new row/col */

    // 5. Update state
    cols = newCols;
    numVisibleRows = newVisibleRows;
    state.setCursorRow (/* ... */);
    state.setCursorCol (/* ... */);
    markAllDirty();

    // ZERO cell mutation. Lines untouched. Content unchanged.
}
```

`recomputeVisibleRows` walks Lines from the viewport anchor:

```cpp
void Grid::recomputeVisibleRows (int numCols, int numVisibleRows,
                                 int anchorLineIndex, int anchorCellPos)
{
    // Walk backwards from anchor line, filling visibleRows bottom-up.
    // For each Line: visualSpan = max(1, (line.length + numCols - 1) / numCols)
    // Fill Row entries: { lineIndex, span * numCols } for each visual row of that Line.
}
```

This is the entire reflow. ~30 lines. Replaces 450 lines of GridReflowHelpers.cpp.

### Scrollback Model

- Lines ring capacity sized to cover configured scrollback (visual rows).
- `Lines::scrollbackVisualRows` tracks total visual row coverage of non-visible Lines.
- When a new Line enters scrollback and total exceeds configured capacity: recycle oldest Lines until within budget.
- Recycled Lines keep their HeapBlock allocation (`line.reset()`) — zero new malloc in steady state.
- Scrollback is NOT a separate buffer. It's older Lines in the same ring that are above the viewport.

### Hyperlinks

LinkIds stored in Line's `linkIds` sidecar — parallel array, same index as cells. Part of CONTENT.

Reflow recomputes visibleRows (the view). Never touches cells or linkIds. After resize, renderer reads linkIds at the same offsets as cells — through the Row mapping. Parser's placement preserved exactly. No phantom links. SSOT.

### Proportional Integration

**Region::xAdvance** (JAM change — same as RFC-proportional-layout §1):
- Add `float xAdvance` to `jam::Glyph::Region`
- Populate from HarfBuzz advance during rasterization
- This is the SSOT for glyph advance width

**Layer 2 (wrapping) — identical for mono and proportional:**
- Terminal wraps at numCols for BOTH modes
- numCols = viewport / physCellWidth (protocol concern, unchanged)
- VT100 cursor addressing, TIOCSWINSZ, SIGWINCH all use numCols
- Mono: widths = cell.width (1 or 2), lineWidth = numCols
- Proportional: SAME — wrapping still at numCols. Proportional affects rendering only.

**Layer 3 (rendering) — where proportional lives:**
- `Layout::computeRowAdvances` produces per-cell X pixel positions
- ASCII 0x20-0x7E: use Region::xAdvance (proportional advance)
- Everything else: physCellWidth × cell.width (uniform)
- Proportional may cause visual overflow/underflow at row edges — acceptable (mlterm does the same)

**Touch points (six sites in renderer that read X positions):**

| Touch point | Current | New (both modes) |
|---|---|---|
| BG quad X + width | `col * physCellWidth` | `advances[col]`, width = `advances[col+1] - advances[col]` |
| Glyph X origin | `col * physCellWidth` | `advances[col]` |
| Ligature advance clamp | `physCellWidth` | `advances[col+1] - advances[col]` |
| Box drawing X | `col * physCellWidth` | `advances[col]` |
| Cursor X | `col * physCellWidth` | `advances[cursorCol]` |
| Mouse hit-test | `pixelX / physCellWidth` | binary search on advances table |

**Data budget:** Per visible row: `float[numCols + 1]`. At 200 cols × 50 rows: ~40 KB. Negligible.

### RowState::isWrapped — eliminated

In the current model, `RowState::isWrapped()` marks soft-wrap boundaries. In the logical-line model, wrapping is implicit:
- A visual row is "soft-wrapped" if it's NOT the last visual row of its Line (more cells follow)
- A visual row is "hard-break" if it IS the last visual row of its Line
- No explicit flag stored. Derivable from: `row.cellOffset + numCols < line.length`

Text selection and extractText use this to decide whether to insert `\n` between visual rows. The derivation is O(1).

### Sub-region scroll (DECSTBM)

When a scroll region (not full viewport) scrolls:
- If the scroll boundary falls at a Line boundary: clean — Line enters/leaves the visible set
- If the scroll boundary falls mid-Line: split the Line at that visual row boundary

Splitting is a Parser-initiated mutation (Parser sent the scroll command), not a reflow mutation. Acceptable. The resulting fragments are separate Lines — each with their own cell/grapheme/linkId arrays. `memcpy` of the split portion into a new Line.

Sub-region scrolls are common (tmux, vim status lines). Split cost: O(cellCount) of the split Line. Visible-area Lines are typically short. Negligible.

### Dirty Tracking

256-bit atomic bitmask over visible rows — unchanged. Operates on visual row indices, orthogonal to storage model. `markRowDirty(r)` still sets bit `r`. Renderer still calls `consumeDirtyRows()`.

### Thread Model

Unchanged:
- READER THREAD: writes to Lines via Writer (Parser)
- MESSAGE THREAD: reads Lines via visibleRows (renderer), recomputes visibleRows (resize)
- `resizeLock`: serializes resize against writes. READER holds during parse batch. MESSAGE holds during resize. visibleRows consistent under lock.

### Alternate Screen

Same model: `buffers[alternate]` is a Lines ring with zero scrollback capacity. On resize: recompute visibleRows. App redraws anyway (SIGWINCH). No special handling needed.

## BLESSED Compliance Checklist

- [x] Bounds — Line owns cells/graphemes/linkIds. Lines ring owns Lines. Row references by index. Ring recycles deterministically.
- [x] Lean — Three structs (Line, Lines, Row). Wrapping function: 15 lines. Reflow: ~30 lines. Down from 450.
- [x] Explicit — Row explicitly maps visual → logical. directRowPtr returns Line's cell array at offset. jam::Wrap signature: all inputs visible, pure. No magic.
- [x] SSOT — Line IS the truth. Row is computed view. No copy masquerading as source. LinkIds never move.
- [x] Stateless — Wrapping is pure function. No state between resizes. No memory of previous layout.
- [x] Encapsulation — Parser writes to Lines via Writer. Renderer reads Lines via Row. jam::Wrap has no knowledge of Grid. Layout has no knowledge of wrapping. Unidirectional.
- [x] Deterministic — Same Lines + same numCols = same Row mapping. Same cells + same font = same advances.

## Open Questions

None. All decisions made by ARCHITECT during session:
- Storage: logical lines, rewrite from scratch (ARCHITECT decision)
- Naming: Line, Lines, Row (ARCHITECT decision)
- Wrapping: pure function at JAM, generic, takes float widths (ARCHITECT decision)
- Scrollback: measured in visual rows, same config numbers (ARCHITECT decision)
- Proportional strategy: wrap at numCols, proportional affects rendering only (ARCHITECT decision, per RFC-proportional-layout §4)
- numCols: stays physCellWidth-derived (ARCHITECT decision)
- Scope: ASCII 0x20-0x7E for proportional advance (ARCHITECT decision)

## Handoff Notes

- **RFC-proportional-layout.md is superseded.** All its content is absorbed into Layer 3 of this RFC. The file can be deleted or archived.
- **GridReflowHelpers.cpp (450 lines) and reflow logic in GridReflow.cpp are deleted entirely.** Not refactored — replaced by ~30 lines of visibleRows recomputation.
- **Grid::Buffer struct is replaced by Grid::Lines.** The five parallel HeapBlocks become per-Line arrays.
- **Grid::Writer interface is preserved.** `directRowPtr(row)` returns pointer into Line's cell array. Zero API change for Parser hot path. Writer methods translate visual coords via Row mapping internally.
- **Dirty tracking (256-bit bitmask) stays unchanged.** Operates on visual rows, orthogonal to storage.
- **Thread model unchanged.** resizeLock serializes resize against writes. Same lock, same pattern.
- **RowState::isWrapped() eliminated.** Soft-wrap is implicit from Line.length vs cellOffset. extractText and selection derive it O(1).
- **JAM change required first.** `Region::xAdvance` must be added and populated before Layer 3 can consume it. JAM lives at `~/Documents/Poems/dev/jam/`. One field + one assignment.
- **jam::Wrap module placement.** The wrapping function needs a home in JAM. Likely `jam_tui` (where terminal-adjacent utilities live) or a new lightweight module. COUNSELOR should evaluate.
- **The `proportional` flag** — how it's configured (Lua config field, runtime toggle, per-pane) is a COUNSELOR/ARCHITECT decision for PLAN. Layer 3 accepts it as a parameter.
- **Hit-test (mouse → cell):** Row mapping gives visual row → Line + offset. Within a row, binary search on advances table gives pixel → col. Simpler than current model.
- **Alternate screen:** same architecture, no scrollback. App redraws after SIGWINCH — visibleRows recomputation is effectively free.
- **State serialization:** `getStateInformation` / `setStateInformation` serialize Lines ring (line lengths + cell data). Structurally simpler than current Buffer serialization.
