# PLAN: Flexbox Reflow Algorithm

**RFC:** RFC-cell-flex.md + RFC-flexbox-reflow.md
**Date:** 2026-05-23
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (Reference Implementation)

---

## Overview

Two RFCs, one sprint. RFC-cell-flex.md adds flex layout bits to `Cell.packed` (bits 41–60,
no sizeof change). RFC-flexbox-reflow.md replaces the broken cell-streaming `Screen::reflow()`
with a 4-phase flexbox algorithm (parse → collect → resolve → write).

Already done — no action needed:
- `Cell::FLEX_GAP { 2 }` — `jam_cell.h:80` ✓
- `Row::flexWrap`, `Row::collapsed` — `jam_row.h:34-35` ✓
- `Video::print()` FLEX_GAP stamping — `Video.cpp:577–589` ✓
- No `// DIAG` markers remaining ✓
- `Row::wrapped` / `Row::dead` renames — already correct names ✓

---

## Language / Framework Constraints

C++ / JUCE — all BLESSED principles enforced as written. JRENG-CODING-STANDARD additions:
- **NO anonymous namespaces.** File-scope internal types (Segment, FlexLine, SourceCursor)
  are plain structs in Screen.cpp — translation-unit local by virtue of being in a .cpp file.
  Helper functions are `static` (explicit translation-unit-local linkage).
- No early returns. Positive checks, `jassert` at preconditions.
- `.at()` for all indexed container access.
- `not`, `and`, `or` alternative tokens.
- Brace initialization throughout.

---

## Validation Gate

Each step validated by @Auditor before proceeding. Validation = compliance with:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- `~/.carol/JRENG-CODING-STANDARD.md`
- Locked PLAN decisions below (no deviation, no scope drift)

---

## Steps

### Step 1: Cell flex bit fields
**Scope:** `jam/jam_fonts/cell/jam_cell.h` only

**Action:**
1. Add `CONTENT_FLEX { 3 }` constant after `FLEX_GAP { 2 }` (line 80) with doc comment.
2. Add 9 flex value constants in the `public:` section (before the accessors) with doc
   comments for the three enumerations: `flexWrap` values, `flexAlign` values,
   `flexBasis` values — exact values from RFC-cell-flex.md §flexWrap Values,
   §flexAlign Values, §flexBasis Values:
   ```
   FLEX_WRAP_WRAP { 0 }, FLEX_WRAP_NOWRAP { 1 }, FLEX_WRAP_REVERSE { 2 }
   FLEX_ALIGN_START { 0 }, FLEX_ALIGN_CENTER { 1 }, FLEX_ALIGN_END { 2 }, FLEX_ALIGN_STRETCH { 3 }
   FLEX_BASIS_CONTENT { 0 }, FLEX_BASIS_FIXED { 1 }, FLEX_BASIS_FILL { 2 }
   ```
3. In the `private:` bit-layout constants block (after `styleIdMask`): add 5 mask/shift
   constant pairs for bits 41–60, exactly as in RFC §New Mask/Shift Constants:
   ```
   flexWrapShift  { 41 },  flexWrapMask  { uint64_t(0x3)  << 41 }
   flexAlignShift { 43 },  flexAlignMask { uint64_t(0x3)  << 43 }
   flexBasisShift { 45 },  flexBasisMask { uint64_t(0x3)  << 45 }
   flexMinShift   { 47 },  flexMinMask   { uint64_t(0x7F) << 47 }
   flexMaxShift   { 54 },  flexMaxMask   { uint64_t(0x7F) << 54 }
   ```
4. In the `public:` accessors block: add 5 accessors (`flexWrap()`, `flexAlign()`,
   `flexBasis()`, `flexMin()`, `flexMax()`) — exact bodies from RFC §New Accessors.
5. Replace `make()` (line 157) with the 9-param version from RFC §Updated make()
   (5 new optional params with defaults: `fWrap=FLEX_WRAP_WRAP`, `fAlign=FLEX_ALIGN_START`,
   `fBasis=FLEX_BASIS_CONTENT`, `fMin=0`, `fMax=0x7F`).
6. Add `makeFlexGap()` factory after `make()` — exact body from RFC §New makeFlexGap().
7. Update the bit-layout table in the file header doc comment (lines 21–28) to the new
   bit map from RFC §Bit Map — After.

**Validation:**
- `sizeof(Cell) == 8` and `is_trivially_copyable_v<Cell>` are enforced by existing
  `static_assert` lines — both must still pass.
- All 5 mask/shift values match RFC §New Mask/Shift Constants bit positions exactly.
- Existing 4-param `make()` callers compile unchanged (new params all have defaults).
- `erase()` still compiles (calls `make()` with 4 args).
- No new files created.

---

### Step 2: Internal types in Screen.cpp
**Scope:** `Source/terminal/component/Screen.cpp` — file-scope, replacing lines 22–35
(the `mapPosition()` helper and its doc comment)

**Action:** Delete `mapPosition()` (lines 22–35) and replace with the three file-scope
type definitions below. No namespace qualifier. Static linkage for helper functions.

```cpp
struct Segment
{
    int  offset { 0 };    ///< Starting cell index in the flattened logical line.
    int  width  { 0 };    ///< Number of cells.
    bool isGap  { false };///< true = FLEX_GAP run; false = content item.
};

struct FlexLine
{
    int firstSegment   { 0 };///< Index into Segment array.
    int segmentCount   { 0 };///< Number of segments on this line.
    int itemWidthTotal { 0 };///< Sum of all item widths on this line.
    int gapCount       { 0 };///< Number of gap segments on this line.
};

struct SourceCursor
{
    const jam::Buffer<jam::Row>& source;
    int channel { 0 };
    int row     { 0 };
    int col     { 0 };
    int lineEnd { 0 };///< Exclusive row boundary (logical line).

    jam::Cell read  () const noexcept;
    void      advance ()   noexcept;
};
```

Add `SourceCursor::read()` and `SourceCursor::advance()` as static-linked inline
definitions immediately after the struct (each ≤15 lines). `advance()` increments col;
when col reaches the current row's `usedCols`, spills to next row. Skips rows past
`lineEnd` silently.

**Validation:**
- No anonymous namespace. File-scope structs visible to tooling.
- All struct fields brace-initialized.
- `SourceCursor::advance()` does not cross `lineEnd`.
- `mapPosition` — grep confirms zero remaining references after deletion.

---

### Step 3: Delete old reflow body
**Scope:** `Source/terminal/component/Screen.cpp`

**Action:**
1. Delete the existing `reflow()` doc comment block (lines 37–70) and replace with a
   fresh, accurate doc comment describing the 4-phase flexbox algorithm.
2. Delete the entire function body (lines 79–384) and replace with `{ return 0; }`.

**Validation:** File compiles with the stub. No references to the old EXPAND/CONTRACT
path comment blocks remain. `mapPosition` already removed in Step 2.

---

### Step 4: Phase 1 — static parseSegments()
**Scope:** `Source/terminal/component/Screen.cpp` — static, before `reflow()` signature

**Signature:**
```cpp
static void parseSegments (const jam::Buffer<jam::Row>& source, int ch,
                            int lineStart, int lineEnd,
                            Segment* segs, int& segCount) noexcept;
```

**Action:** Implement per RFC Phase 1 rules:
1. Walk cells across flexWrap-joined rows (lineStart..lineEnd-1), col 0..usedCols-1.
2. Classify: `contentTag() == jam::Cell::FLEX_GAP` → gap; otherwise → item.
3. Coalesce consecutive same-type cells into one Segment (extend `width`).
4. Leading FLEX_GAP cells (before first non-gap, non-zero cell) → merge into first
   item segment (`isGap = false`). Preserves fixed indentation.
5. Cells beyond `usedCols` — skip.
6. Clamp at `segCount == 341`.

**Validation:** BLESSED L ≤30 lines. No early returns. Same input → same output.
Leading-gap merge correctly classifies indentation as item. segCount bounded at 341.

---

### Step 5: Phase 2 — static collectFlexLines()
**Scope:** `Source/terminal/component/Screen.cpp` — static, before `reflow()` signature

**Signature:**
```cpp
static void collectFlexLines (const Segment* segs, int segCount,
                               int newCols,
                               FlexLine* lines, int& lineCount) noexcept;
```

**Action:** Implement per RFC Phase 2 (CSS §9.3 greedy packing):
1. Single forward pass over item segments only (gaps are not line-break triggers).
2. For each item: `needed = item.width + (line.hasItems ? 1 : 0)`.
3. If `needed > line.remaining` and line already has items → finalize current line,
   start new. The gap segment immediately before this item belongs to the new line
   (vanishes from old line — RFC §6 accepted tradeoff).
4. Wide-item split: if `item.width > newCols`, split into `ceil(item.width / newCols)`
   sub-items of width `newCols` each (last sub-item carries remainder).
5. Track `itemWidthTotal` and `gapCount` per line during collection.
6. Finalize last line.

**Validation:** BLESSED L ≤30 lines. Single-pass. No floating point. Items wider than
`newCols` produce valid sub-lines. `lineCount` bounded by caller-allocated array size.

---

### Step 6: Phase 3 — static resolveFlexLengths()
**Scope:** `Source/terminal/component/Screen.cpp` — static, before `reflow()` signature

**Signature:**
```cpp
static void resolveFlexLengths (const Segment* segs, int segCount,
                                 const FlexLine* lines, int lineCount,
                                 int newCols,
                                 int* gapWidths) noexcept;
```
`gapWidths` — caller-allocated array indexed by segment index; only gap entries written.

**Action:** Implement per RFC Phase 3 (CSS §9.7 simplified):
1. For each FlexLine: `freeSpace = newCols - line.itemWidthTotal`.
2. If `line.gapCount == 0`: skip.
3. Compute `totalGapWidth` = sum of `segs[i].width` for gap segments on this line.
4. If `totalGapWidth == 0`: distribute equally — `freeSpace / gapCount` per gap,
   Bresenham remainder on last gap.
5. Otherwise: Bresenham proportional distribution per RFC formula:
   `target = (freeSpace * gapSoFar) / totalGapWidth; gap.newWidth = target - distributed`.
6. Floor: `juce::jmax (1, computed)` for each gap entry in `gapWidths`.

**Validation:** BLESSED L ≤30 lines. Integer arithmetic only. No floating point.
`gapWidths[i] >= 1` for all gap segments. Zero remainder loss (Bresenham).

---

### Step 7: Phase 4 — static writeFlexLines()
**Scope:** `Source/terminal/component/Screen.cpp` — static, before `reflow()` signature

**Signature:**
```cpp
static void writeFlexLines (jam::Buffer<jam::Row>& dest, int dstCh, int& dstRow,
                             SourceCursor& cursor,
                             const Segment* segs,
                             const FlexLine* lines, int lineCount,
                             const int* gapWidths,
                             int newCols) noexcept;
```

**Action:** Implement per RFC Phase 4:
1. For each FlexLine:
   a. Write dest row: `dest.getWritePointer (dstCh, dstRow)`, `destCol = 0`.
   b. Walk segments on this line (firstSegment..firstSegment+segmentCount-1):
      - Gap: write `gapWidths[segIdx]` cells of
        `Cell::make (0x20, Cell::FLEX_GAP, Cell::NARROW, 0)`.
      - Item: copy `seg.width` cells via `cursor.read()` / `cursor.advance()`.
        Wide-char boundary: WIDE char landing on `newCols - 1` →
        insert SPACER_HEAD, do NOT advance cursor (cell writes on next row).
   c. `destRow->usedCols = static_cast<uint16_t> (destCol)`.
   d. `destRow->flags = (isLastLineOfLogicalLine) ? 0 : jam::Row::flexWrap`.
   e. `++dstRow`.
2. SourceCursor advances monotonically through item cells only.

**Validation:** BLESSED L ≤30 lines. `destCol` never exceeds `newCols`. `usedCols`
correct. `flexWrap` flag set on all rows except last of each logical line.

---

### Step 8: Rewrite Screen::reflow() body
**Scope:** `Source/terminal/component/Screen.cpp` — `Screen::reflow()` body only

**Action:** Replace `{ return 0; }` placeholder with orchestrator:

Normal screen (channel 0) loop:
```
totalSourceRows = numHistoryNormal + cursorRow + 1
srcRow = 0, dstRow = 0

while srcRow < totalSourceRows and dstRow < scrollbackLines:
    lineStart = srcRow
    advance srcRow over flexWrap rows → lineEnd (exclusive)

    if logical line is empty (all usedCols == 0):
        dest.clear (0, dstRow)
        ++dstRow
    else:
        Segment segs[341];   int segCount { 0 };
        FlexLine lines[512]; int lineCount { 0 };
        int gapWidths[341]   (zero-init);

        parseSegments       (source, 0, lineStart, lineEnd, segs, segCount)
        collectFlexLines    (segs, segCount, destCols, lines, lineCount)
        resolveFlexLengths  (segs, segCount, lines, lineCount, destCols, gapWidths)
        SourceCursor cursor { source, 0, lineStart, 0, lineEnd }
        writeFlexLines      (dest, 0, dstRow, cursor, segs, lines, lineCount, gapWidths, destCols)
```

Alternate screen (channel 1): verbatim viewport copy — identical to current
implementation (no change to this path).

Return: `juce::jmax (0, dstRow - newVisibleRows)`

Stack arrays declared inside the `else` block — no heap allocation.

**Validation:** BLESSED L ≤30 lines for orchestrator body. No State access. No Video
access. Pure transform. Correct history count returned. Alternate screen path unchanged.

---

## BLESSED Alignment

| Principle | How satisfied |
|-----------|---------------|
| **B** | Segment/FlexLine/gapWidths stack-allocated, lifetime scoped to the function call. SourceCursor holds a const-ref, never outlives source. |
| **L** | Each static helper ≤30 lines. Orchestrator ≤30 lines. Cell additions stay within existing file structure. |
| **E** | `FLEX_GAP`, `CONTENT_FLEX`, all flex constants named. Flexbox naming (`FlexLine`, `Segment`, `resolveFlexLengths`) enforces mental model. No magic values. All params explicit. |
| **S (SSOT)** | Video is sole FLEX_GAP stamping authority (unchanged). Cell tag is sole gap identity source. |
| **S (Stateless)** | `reflow()` is a pure transform — no State, no Video, no side effects. |
| **E (Encapsulation)** | Internal types in Screen.cpp only. Helpers are `static`. `jam_cell.h` exposes only public API (constants + accessors + factories). |
| **D** | Integer Bresenham distribution. Same input → same output. No floating point. |

---

## Risks / Open Questions

- **SourceCursor wide-char handling.** When a WIDE cell is read, `advance()` must also
  consume the paired SPACER_TAIL. Implementation detail within Step 2 — no decision
  needed, but Engineer must handle it.
- **collectFlexLines gap-ownership at wrap boundary.** The gap between item A and item B
  when item B wraps to a new line vanishes (RFC §Handoff Note 6 — accepted tradeoff).
  Phase 2 must not double-count these edge-spanning gaps.
- **Screen.cpp line count post-sprint.** Current file: ~490 lines. Removing ~303 lines
  (old body) and adding ~150 lines (helpers + orchestrator) keeps the file within
  BLESSED L bounds.
