# PLAN: Logical-Line Grid Architecture

**RFC:** RFC-logical-line-grid.md
**Date:** 2026-05-03
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides per LANGUAGE.md)

## Overview

Replace Grid's flat 2D cell buffer (Buffer struct) with logical-line storage (Line/Lines/Row). Parser output becomes immutable content. Visual rows become a computed mapping recomputed on resize — zero cell mutation. Eliminates GridReflowHelpers.cpp (~450 lines) and the destructive reflow algorithm. Proportional rendering (Layer 3) deferred to separate sprint — this plan covers Layer 1 (Content) and Layer 2 (Wrapping) only.

## Language / Framework Constraints

C++ / JUCE — MANIFESTO.md reference implementation. All BLESSED principles enforced as written. No overrides.

- `juce::HeapBlock` for per-Line cell/grapheme/linkId arrays (JRENG-CODING-STANDARD: prefer HeapBlock for arrays)
- `juce::CriticalSection` + `juce::ScopedLock` for resizeLock (existing pattern)
- Brace initialization, `not`/`and`/`or` operators, `.at()` for container access, no early returns, no anonymous namespaces (JRENG-CODING-STANDARD)
- All new names (Line, Lines, Row, visualRowSpan, recomputeVisibleRows) approved by ARCHITECT in RFC session

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy — no improvised names)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)
- RFC-logical-line-grid.md (architectural decisions)

## Steps

### Step 1: Data Structures — Line, Lines, Row

**Scope:** `Source/terminal/logic/Grid.h`

**Action:** Add three structs to Grid class (alongside existing Buffer — no functional change, no existing code modified):

```cpp
struct Line
{
    juce::HeapBlock<Cell> cells;
    juce::HeapBlock<Grapheme> graphemes;
    juce::HeapBlock<uint16_t> linkIds;
    int length { 0 };
    int capacity { 0 };

    void ensureCapacity (int needed) noexcept;
    void reset() noexcept;
};

struct Lines
{
    juce::HeapBlock<Line> lines;
    int capacity { 0 };
    int mask { 0 };
    int head { 0 };
    int count { 0 };
    int scrollbackVisualRows { 0 };

    Line& at (int index) noexcept;
    const Line& at (int index) const noexcept;
    Line& advance() noexcept;
};

struct Row
{
    int lineIndex { 0 };
    int cellOffset { 0 };
};
```

Implement `Line::ensureCapacity` (grow HeapBlocks, zero-init new range, update capacity), `Line::reset` (set length to 0, keep allocation), `Lines::at` (ring indexing: `lines[(head - count + 1 + index) & mask]`), `Lines::advance` (increment head, recycle oldest if ring full, return new slot).

**Validation:**
- Structs compile alongside existing Buffer with zero functional change
- NAMES.md: Line (noun, singular), Lines (noun, plural container), Row (noun, mapping entry) — all ARCHITECT-approved
- BLESSED-B: Line owns its HeapBlocks. Lines ring owns Line objects. Clear lifecycle.
- BLESSED-L: Three small structs, each under 30 lines
- JRENG-CODING-STANDARD: brace init, HeapBlock (not raw new), no anonymous namespaces

---

### Step 2: Wrapping Helper

**Scope:** `Source/terminal/logic/Grid.h` (declaration), `Source/terminal/logic/Grid.cpp` or inline

**Action:** Add static helper for monospace visual row span computation:

```cpp
static int visualRowSpan (int lineLength, int numCols) noexcept
{
    jassert (numCols > 0);
    return (lineLength > 0) ? ((lineLength + numCols - 1) / numCols) : 1;
}
```

This is the mono fast path from RFC Layer 2. Empty lines still occupy 1 visual row (blank prompt line). Full `jam::Wrap` module with float widths deferred to proportional phase (YAGNI — mono needs only integer ceil division).

**Note for ARCHITECT:** RFC places wrapping in JAM as `jam::Wrap::computeBreaksMono`. This plan defers the JAM module to the proportional sprint and uses a static helper for now. The function is trivially extractable later. If ARCHITECT prefers JAM placement now, this step changes to a JAM module addition.

**Validation:**
- Pure function, no state, deterministic (BLESSED-S, BLESSED-D)
- `jassert` precondition on numCols > 0 (BLESSED-E: fail fast)
- BLESSED-L: one-liner

---

### Step 3: Grid Storage Migration

**Scope:** `Source/terminal/logic/Grid.h`, all `Source/terminal/logic/Grid*.cpp`, `Source/terminal/rendering/ScreenRender.cpp`

**This is the core migration step.** Buffer struct replaced by Lines. All Grid access methods change simultaneously. The step is architecturally atomic — write path, read path, and resize must change together because they share the same storage.

**Sub-deliverables (order of implementation within the step):**

#### 3a: Replace Buffer with Lines in Grid

Remove `Buffer` struct. Replace `std::array<Buffer, 2> buffers` with `std::array<Lines, 2> buffers`. Add `Row* visibleRows` array (heap-allocated, size = numVisibleRows). Add `int numVisibleRows`, `int cols` members.

Implement `initLines (Lines&, int lineCapacity)` replacing `initBuffer`. Lines ring capacity sized to cover configured scrollback in visual rows. Power-of-two for mask arithmetic (existing pattern).

#### 3b: Row Mapping — recomputeVisibleRows

```cpp
void recomputeVisibleRows (int numCols, int numVisibleRows,
                           int anchorLineIndex, int anchorCellPos);
```

Walk Lines from anchor backward, compute `visualRowSpan(line.length, numCols)` per Line, fill Row entries bottom-up. Each Row: `{lineIndex, span * numCols}` for each visual row of that Line. ~30 lines. Replaces 450 lines of GridReflowHelpers.cpp.

#### 3c: Write Path — directRowPtr through Row Mapping

```cpp
Cell* directRowPtr (int visibleRow) noexcept
{
    const auto& row { visibleRows[visibleRow] };
    auto& line { bufferForScreen().at (row.lineIndex) };
    line.ensureCapacity (row.cellOffset + cols);
    line.length = juce::jmax (line.length, row.cellOffset + cols);
    return line.cells.get() + row.cellOffset;
}
```

Same for `directGraphemeRowPtr`, `directLinkIdRowPtr`. Signature unchanged — Parser hot path (processGroundChunk) sees no API change. One indirection per row change, not per character.

`line.length` set conservatively to `cellOffset + cols` on access (high-water mark pattern from tmux's `cellused`). Avoids need for explicit finalize step.

#### 3d: Read Path — activeVisibleRow through Row Mapping

```cpp
const Cell* activeVisibleRow (int visibleRow) const noexcept
{
    const auto& row { visibleRows[visibleRow] };
    const auto& line { bufferForScreen().at (row.lineIndex) };
    return line.cells.get() + row.cellOffset;
}
```

Same for `activeVisibleGraphemeRow`, `activeVisibleLinkIdRow`. Scrollback reads (`scrollbackRow`) walk Lines ring backward from viewport anchor, computing visual row span per Line to find the target.

**Thread safety for renderer:** `buildSnapshot` copies relevant Row entries under `resizeLock` at start, then reads cells through the copied mapping without lock. Lock hold time: copy of ~50 Row entries (~400 bytes). Parser holds `resizeLock` during `processWithLock` — no concurrent access to visibleRows under lock.

`extractText`: already acquires `resizeLock`. Reads through Row mapping. No change to locking.

#### 3e: LF and Scroll Operations with Lines

**LF at bottom of scroll region (Writer::scrollUp):**
1. `lines.advance()` — new Line allocated (oldest recycled if ring full)
2. Update `scrollbackVisualRows` for recycled/new Lines
3. `memmove` visibleRows up by one entry, bottom entry → new Line at cellOffset 0
4. Increment `scrollDelta` (atomic, for renderer smooth scroll — unchanged)

**LF not at bottom:** cursor moves down. visibleRows[cursorRow + 1] already maps to an existing Line segment. No Line creation.

**Auto-wrap (cursor reaches numCols):** Line grows. Next visual row maps to same Line at higher cellOffset. When cursor wraps, visibleRows entry for the next row is updated: `{sameLineIndex, cellOffset + numCols}`.

**scrollDown, insertLine, deleteLine:** analogous Line management. Sub-region scroll (DECSTBM) where boundary falls mid-Line: split the Line at the visual row boundary (memcpy split portion to new Line). This is a Parser-initiated content mutation, not a reflow mutation. Cost: O(cellCount) of the split Line. Acceptable.

**clearRow:** zero cells in Line from `cellOffset` to `cellOffset + numCols`. Line.length unchanged (clearing is not trimming).

**eraseInLine, eraseInDisplay, insertChars, deleteChars:** same operations on Line's cell array at appropriate offsets. Addressing changes from `buffer.cells[physRow * cols + col]` to `line.cells[cellOffset + col]`.

#### 3f: Grid::resize — recomputeVisibleRows

```cpp
void Grid::resize (int newCols, int newVisibleRows)
{
    const juce::ScopedLock lock (resizeLock);

    // 1. Save cursor logical position (stable across resize)
    const auto& oldRow { visibleRows[cursorRow] };
    const int cursorLineIndex { oldRow.lineIndex };
    const int cursorCellPos { oldRow.cellOffset + cursorCol };

    // 2. Reallocate visibleRows if height changed

    // 3. Recompute visibleRows from Lines + newCols
    recomputeVisibleRows (newCols, newVisibleRows, cursorLineIndex, cursorCellPos);

    // 4. Derive cursor visual position from logical position

    // 5. Update cols, numVisibleRows, state, markAllDirty

    // ZERO cell mutation. Lines untouched.
}
```

Height-only fast path preserved: if cols unchanged and new height fits, only adjust visibleRows metadata (shift entries, update scrollbackUsed). No Line recomputation.

Alternate screen: same Lines model, zero scrollback capacity. On resize: recomputeVisibleRows. App redraws after SIGWINCH.

#### 3g: State Serialization

`getStateInformation`: serialize Lines ring (per-Line: length, cell data, grapheme data, linkId data). Structurally simpler than current Buffer serialization — variable-length lines instead of fixed-width 2D array.

`setStateInformation`: deserialize Lines, call `recomputeVisibleRows` to rebuild Row mapping.

#### 3h: Dirty Tracking and rowSeqnos

256-bit atomic bitmask over visible rows — unchanged. `markRowDirty(r)` still sets bit `r`. `consumeDirtyRows()` unchanged.

`rowSeqnos`: move from per-physical-row (Buffer) to per-visible-row array. Size = numVisibleRows. Reallocated alongside visibleRows on height change.

**Validation:**
- Compile and run. Terminal renders correctly. Content survives resize (narrow → wide → narrow).
- No cell mutation during resize (BLESSED-S: Line IS the truth, Row is computed view)
- LinkIds stay where Parser placed them (BLESSED-S: SSOT for hyperlinks)
- processGroundChunk hot path: one indirection per row change, zero per character (BLESSED-L: lean)
- directRowPtr signature unchanged — Parser code untouched (BLESSED-E: encapsulation preserved)
- resizeLock usage correct — no data races between Parser and renderer
- All container access via `.at()` (JRENG-CODING-STANDARD)
- No early returns, no anonymous namespaces, brace initialization throughout
- NAMES.md: no improvised names — all names from RFC (ARCHITECT-approved)

---

### Step 4: Delete Old Code

**Scope:** `Source/terminal/logic/GridReflowHelpers.cpp`, `Source/terminal/logic/GridReflow.cpp`, `CMakeLists.txt`, `Source/terminal/data/Cell.h`

**Action:**
- Delete `GridReflowHelpers.cpp` entirely (~450 lines: flattenLogicalLine, writeNewRow, reflowPass, nextLogicalLine, WalkParams, Grid::reflow)
- Delete old reflow code from `GridReflow.cpp` (resize() is rewritten in Step 3f, file may be merged into Grid.cpp or kept as Grid::resize() home)
- Remove `Buffer` struct from `Grid.h` (replaced by Lines in Step 3a)
- Remove `RowState::isWrapped()` / `setWrapped()` from `Cell.h` — soft-wrap is now implicit: `row.cellOffset + numCols < line.length` (O(1) derivation, per RFC)
- Update `CMakeLists.txt`: remove GridReflowHelpers.cpp from source list
- Merge `GridReflow.cpp` into main Grid .cpp if ARCHITECT prefers (RFC notes: "no reason for two files")

**Validation:**
- Project compiles without deleted files
- No references to removed symbols (grep for flattenLogicalLine, writeNewRow, reflowPass, isWrapped, Buffer)
- BLESSED-L: ~450 lines removed, ~30 lines added. Net reduction ~420 lines.
- BLESSED-S (SSOT): no shadow state. Buffer gone. Lines is the single storage.

---

### Step 5: Full Audit

**Scope:** All files modified in Steps 1-4

**Action:** @Auditor validates entire migration against:
- MANIFESTO.md (every BLESSED principle)
- NAMES.md (no improvised names, Rule -1 compliance)
- JRENG-CODING-STANDARD.md (formatting, control flow, container access, memory management)
- RFC-logical-line-grid.md (architectural decisions preserved)
- This PLAN (no scope drift)

Specific audit targets:
- Thread model: resizeLock correctly serializes resize against writes; buildSnapshot Row mapping access safe
- Hot path: processGroundChunk inner loop is zero-overhead (one indirection per row change only)
- SSOT: LinkIds never moved by resize — verify by resize test with OSC 8 hyperlinks
- Determinism: same Lines + same numCols = same Row mapping (BLESSED-D)
- Sub-region scroll: Line splitting is correct for DECSTBM cases
- Scrollback: Lines ring recycling respects configured capacity

## BLESSED Alignment

| Principle | How satisfied |
|---|---|
| **B (Bound)** | Line owns cells/graphemes/linkIds. Lines ring owns Lines. Row references by index. Ring recycles oldest — deterministic lifecycle. |
| **L (Lean)** | Three small structs. Reflow: ~450 lines deleted, ~30 lines added. visualRowSpan: one-liner. No new classes beyond RFC types. |
| **E (Explicit)** | Row explicitly maps visual -> logical. directRowPtr returns Line's cell array at offset. visualRowSpan: all inputs visible, pure. No magic. |
| **S (SSOT)** | Line IS the truth. Row is computed view. No copy masquerading as source. LinkIds stay where Parser placed them. No shadow state. |
| **S (Stateless)** | Row mapping is pure function of Lines + numCols. No state between resizes. No memory of previous layout. |
| **E (Encap)** | Parser writes to Lines via Writer. Renderer reads Lines via Row. visualRowSpan has no knowledge of Grid. Unidirectional layer flow. |
| **D (Deterministic)** | Same Lines + same numCols = same Row mapping. Emergent from BLESSE compliance. |

## Risks / Open Questions

1. **Sub-region scroll (DECSTBM) Line splitting** — when scroll boundary falls mid-Line, we split. Implementation needs care: memcpy cell range to new Line, update visibleRows for affected entries. Test with tmux, vim status lines, scroll regions.

2. **Scrollback capacity accounting** — `Lines::scrollbackVisualRows` is a running total. When recycling a Line, subtract its visual row span. When adding to scrollback, add the span. Must stay accurate across all operations (LF, scroll, clear, resize).

3. **State serialization backward compatibility** — new format (variable-length Lines) is incompatible with old format (fixed-width Buffer). Existing saved states will not restore. ARCHITECT decision: is a clean break acceptable, or do we need a migration path?

4. **processGroundChunk batch dirty tracking** — currently accumulates dirty bits in local array, flushes once via `batchMarkDirty`. Same pattern works with Lines — dirty bits are visual-row-indexed, orthogonal to storage. Verify no regression.

5. **Wide character at wrap boundary** — current model handled by Parser at write time (fills last col + wrap). New model: Line grows past numCols, wide char's two cells stay contiguous in Line. visualRowSpan correctly accounts for this. Verify with CJK content across wrap boundaries.
