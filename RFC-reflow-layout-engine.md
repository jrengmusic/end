# RFC: Layout Engine — Reflow
**Status:** DRAFT  
**Component:** `terminal::Screen::reflow()`  
**Depends on:** RFC-cell-flex (Cell flex property additions)  
**Author:** ARCHITECT  
**Scope:** Replace existing two-path reflow with a unified logical→physical projection engine

---

## Problem Statement

The current `reflow()` implementation is destructive and asymmetric:

- **EXPAND path** and **CONTRACT path** are separate code branches with different
  semantics — resize behavior is inconsistent by direction
- `dest` buffer is written while `source` is being read; no structural guarantee
  prevents aliasing or partial overwrites
- Downsize loses logical line structure — upsize cannot recover it because the
  source has already been mutated
- `FLEX_GAP` contraction uses a hardcoded floor of 1 cell with no per-cell
  constraint — `Cell.flexMin`/`Cell.flexMax` are not consulted
- `Cell.flexWrap`, `Cell.flexBasis`, `Cell.flexAlign` are entirely absent from
  the reflow pass

---

## Guiding Principle

```
Buffer<Row>   ← parser writes (canonical logical store, never mutated by reflow)
Block<Row>    ← reflow reads (non-owning view, read-only)
Buffer<Row>   ← reflow writes (dest, pre-allocated, cleared before call)
Block<Row>    ← renderer reads from dest
```

Reflow is a **pure projection**: `Block<Row>` in → `Buffer<Row>` out.  
No ownership transfer. No mutation of source. Idempotent at fixed W.

---

## Definitions

### Logical Line
One or more contiguous `Buffer<Row>` rows joined by `Row::flexWrap` flag chains.
The terminal unit of content — what a user typed before pressing Enter, or what
the parser emitted as a single wrapped sequence.

```
Row[n].flags & Row::flexWrap != 0  →  Row[n+1] is a continuation of the same logical line
Row[n].flags & Row::flexWrap == 0  →  Row[n] is the terminating row of its logical line
```

**Authority:** `Row::flexWrap` is the sole logical line boundary marker.
`Cell.flexWrap` is a layout intent flag — it controls where within a logical
line physical wrapping is permitted, not where logical lines end.

### Physical Line
One rendered row at width `W`. Derived from a logical line. Ephemeral — valid
only for the current `W`. Carries a back-reference to its logical origin for
cursor stability across resize.

### Flex Line
The unit of free-space distribution. One physical line = one flex line.
`FLEX_GAP` cells within a physical line absorb free space after basis resolution.

---

## Data Types

```cpp
// Maps one physical line back to its logical origin.
struct PhysicalLine
{
    int logicalRow;  // index into source Block (first source row of logical line)
    int cellStart;   // flat cell offset within the logical line
    int cellCount;   // number of logical cells on this physical line
};

// One cell with its resolved display width after flex distribution.
struct ResolvedCell
{
    jam::Cell cell;
    int       width; // display columns after flexMin/flexMax/grow
};
```

Both are stack types — not stored between reflow calls.

---

## Algorithm

### Stage 1 — Logical Line Reconstruction

Walk source rows, consuming `Row::flexWrap` chains into `LogicalLine` spans.
No cell copying — produces only index ranges.

```
srcRow = 0
while srcRow < source.numRows:
    lineStart = srcRow
    totalCells = 0
    loop:
        totalCells += source[srcRow].usedCols
        wraps = source[srcRow].flags & Row::flexWrap
        srcRow++
        if not wraps: break
    yield LogicalLine { lineStart, srcRow, totalCells }
```

Empty logical lines (totalCells == 0) emit one empty physical line and continue.

---

### Stage 2 — Physical Line Projection

For each logical line, determine how many cells fit per physical line at width `W`.

**Break decision per cell:**

| Condition | Action |
|---|---|
| `cell.flexWrap == FLEX_WRAP_NOWRAP` | never break here — continue regardless of W |
| `usedWidth + cellWidth > W && count > 0` | break before this cell |
| `cell.flexMin > 0 && usedWidth + flexMin > W && count > 0` | break before this cell |
| end of logical line | terminate current physical line |

Wide characters (`wide == WIDE`) contribute 2 display columns.  
`FLEX_GAP` cells contribute 1 column as basis (expanded in Stage 3).

A `FLEX_WRAP_NOWRAP` group that exceeds `W` produces one overlong physical line.
The renderer is responsible for horizontal overflow — reflow never truncates content.

---

### Stage 3 — Flex Distribution (resolveFlexLine)

For each physical line, distribute free space `(W - baseWidth)` across `FLEX_GAP`
cells. Applied after break decision, before writing to dest.

```
Pass 1 — resolve base widths:
    for each cell in physical line:
        basis = FLEX_BASIS_CONTENT → wcwidth / wide hint
                FLEX_BASIS_FIXED   → codepoint field value
                FLEX_BASIS_FILL    → W (fills before grow; only meaningful on sole cell)
        resolved = clamp(basis, flexMin || basis, flexMax || ∞)
        if contentTag == FLEX_GAP: gapCount++
        baseWidth += resolved

Pass 2 — distribute free space:
    freeSpace = max(0, W - baseWidth)
    if gapCount > 0 and freeSpace > 0:
        share = freeSpace / gapCount
        rem   = freeSpace % gapCount
        for each FLEX_GAP cell:
            extra = 1 if rem > 0 else 0
            cell.width = min(cell.width + share + extra, flexMax || ∞)
            rem = max(0, rem - 1)
```

`flexAlign` is not applied during reflow — it is a cross-axis (vertical) property
consumed by the renderer, not the line-break engine.

---

### Stage 4 — Write to Dest

For each resolved physical line, write cells into `dest` row sequentially.

```
for each ResolvedCell in physical line:
    if contentTag == FLEX_GAP:
        write `width` FLEX_GAP space cells to dest
    else:
        write cell verbatim
        if wide == WIDE:
            write SPACER_TAIL immediately after
    dstCol += width

dstRow.usedCols = dstCol
dstRow.flags    = Row::flexWrap  if logical line continues
                = 0              if logical line ends here
dstRow++
```

Wide char at last column of dest row: insert `SPACER_HEAD` placeholder, defer
wide cell to next physical line (existing behavior, preserved).

---

## Function Signature

```cpp
// Rewrites dest from source at column width W.
//
// source       — read-only Block view of canonical logical content.
//                Ring mapping is handled by Block::getRowPointer().
// dest         — pre-allocated Buffer<Row> at new dimensions.
//                Caller clears dest before calling.
// W            — new column width (dest.getNumCols()).
// maxRows      — maximum dest rows to write (dest.getNumRows()).
// contentRows  — number of logical rows to process from source.
//                Caller computes: numHistoryRows + cursorRow + 1.
//
// Returns number of physical rows written to dest.

int reflow (jam::Buffer<jam::Row>& dest,
            const jam::Block<jam::Row>& source,
            int W,
            int maxRows,
            int contentRows) noexcept;
```

The alternate screen (channel 1) is a verbatim viewport copy — apps redraw on
`SIGWINCH`. This is unchanged from the current implementation and is not part
of this engine.

---

## Cursor Stability Across Resize

Physical position is ephemeral. Logical address is stable.

Before resize, caller saves:
```cpp
// Flat cell index within its logical line — survives any W change
int savedLogicalRow = cursorLogicalRow;
int savedCellOffset = cursorCellOffset; // flat index within logical line
```

After reflow, caller re-derives physical row:
```cpp
// Walk PhysicalLine[] to find which physical line contains savedCellOffset
// within savedLogicalRow
```

`PhysicalLine.logicalRow` + `PhysicalLine.cellStart` + `PhysicalLine.cellCount`
provide the full inverse mapping. Flat cell index is the invariant.

---

## Properties of the New Engine

| Property | Old | New |
|---|---|---|
| Source mutated | yes — dest overwrites source in place | no — source Block is read-only |
| Downsize | CONTRACT path, destructive | projection: more physical lines, logical intact |
| Upsize | EXPAND path, segment mapping | projection: fewer physical lines, gaps absorb space |
| FLEX_GAP contraction | `jmax(1, gapLen * destCols / srcCols)` | `resolveFlexLine()` with per-cell `flexMin`/`flexMax` |
| `Cell.flexWrap` | not consulted | controls break permission per cell |
| `Cell.flexMin/Max` | not consulted | clamps basis and growth per cell |
| `Cell.flexBasis` | not consulted | selects width mode per cell |
| `Cell.flexAlign` | not applicable | deferred to renderer (cross-axis) |
| Wide char wrap | SPACER_HEAD at last col | preserved, same behavior |
| Empty logical lines | preserved as empty dest row | preserved |
| Idempotency | no — direction-dependent | yes — same source + W always yields same dest |

---

## What Is Not Addressed

- **Renderer** — `flexAlign` cross-axis positioning is a painter concern, not reflow.
- **`Row.rowWrap`** — row-level wrap mode gate (WRAP / NOWRAP / WRAP_REVERSE)
  is deferred. Bits 2–7 of `Row.flags` are free when needed.
- **Alternate screen reflow** — verbatim copy unchanged, out of scope.
- **`PhysicalLine[]` persistence** — whether the caller caches the derived
  physical line map or discards it after each paint is a caller decision.
  The engine always produces it from scratch; caching is an optimization.
- **Stack buffer sizing** — `ResolvedCell resolved[N]` stack allocation upper
  bound is an implementation detail. 1024 covers any real terminal width.
  A heap fallback for pathological cases is left to the implementor.
