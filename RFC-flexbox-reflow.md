# RFC — Flexbox Reflow Algorithm

Date: 2026-05-23
Status: Ready for COUNSELOR handoff

## Problem Statement

Sprint 30 delivered lossless reflow infrastructure (Row FAM, Buffer\<Row\> FlexType stride, DST owned by Screen, Display wiring). The reflow ALGORITHM is broken. The cell-streaming approach (tmux model — join wrapped rows, split at new width) treats cells as a flat byte stream with no structural awareness:

- Items split mid-word at column boundaries (ls output destroyed)
- OMP prompt with left-pinned + right-pinned content gets mangled (elastic gap not understood)
- Empty viewport rows below cursor accumulate as phantom history
- Upsize scrambles content instead of unwrapping + expanding gaps

Root cause: `jam::Row` is modeled after tmux `grid_line`, which inevitably causes implementations to follow tmux reflow logic. END's text renderer is NOT a conventional terminal character grid — it is `jam::TextEditor`, a `juce::Component` with a `juce::Viewport` whose content height grows. The reflow must think in terms of text layout (items and gaps), not character grid manipulation.

No other published terminal emulator has solved lossless live buffer reflow. Training priors are unreliable. The solution is an adaptation of HTML flexbox layout (reactive resizable UI, battle-tested) applied to the terminal cell domain, combined with a DSP oversampling mental model where items are signal (never altered) and gaps are interpolation (elastic).

## Research Summary

### CSS Flexbox §9 Algorithm (W3C Spec)

Three spec sections map to terminal reflow:

**§9.3 — Line Collection.** Single greedy sequential pass. Items pack into lines in source order. No backtracking. A new line breaks when the next item would exceed container width. The item that triggers overflow is the first item on the new line.

**§9.7 — Resolving Flexible Lengths.** Per-line freeze/distribute loop:
1. Items with flex-factor 0 freeze immediately (their size is fixed)
2. Compute free space = container width − sum of frozen sizes
3. Distribute free space proportionally to flex factors
4. CSS scaled shrink formula: `flexShrink × flexBasis / Σ(flexShrink × flexBasis)`. With uniform shrink=1, reduces to `basis / Σ(basis)` — proportional to current width. Wider gaps absorb more change.
5. Freeze items hitting min/max, redistribute remaining. Loop until all frozen.

**§9.7 simplification for terminal cells:** Items have flex-shrink=0, flex-grow=0 — they freeze in step 1. Only gaps participate. The loop body runs once (gaps have no min/max constraints beyond 1-cell minimum). No iteration needed.

**Integer remainder distribution:** Bresenham running-total approach:
```cpp
int distributed { 0 };
int gapSoFar { 0 };

for (int i { 0 }; i < gapCount; ++i)
{
    gapSoFar += gap[i].currentWidth;
    const int target { (freeSpace * gapSoFar) / totalGapWidth };
    gap[i].delta = target - distributed;
    distributed = target;
}
```
Zero remainder loss. Every cell accounted for. No floating point.

Sources: W3C CSS Flexible Box Layout Module Level 1 (https://www.w3.org/TR/css-flexbox-1/), §9.3 and §9.7. Cross-verified against JUCE `juce_FlexBox.cpp` implementation and WeasyPrint's spec-annotated Python implementation.

### JUCE FlexBox Implementation

JUCE's `FlexBox::performLayout()` is an 11-step algorithm. Steps relevant to terminal reflow:

- `initialiseItems()` — line collection with greedy bin-packing
- `resolveFlexibleLengths()` — freeze/distribute loop with lock-at-min/max convergence

Key divergence: JUCE's shrink formula uses `flexShrink` directly (not `flexShrink × flexBasis`). The CSS spec's weighted formula is more correct for our case — wider gaps should absorb more contraction.

### Terminal ↔ Flexbox Mapping

| Terminal Row | HTML Flexbox | Behavior |
|---|---|---|
| Logical line (rows until flexWrap=false) | Flex container (display:flex; flex-wrap:wrap) | Groups content that reflows together |
| Row (flexWrap=true) | Continuation of wrapped flex items | Not a boundary — content continues |
| Row (flexWrap=false) | Last row of container — hard break | \n equivalent, never merged across |
| Non-whitespace cell run | Flex item (flex-shrink:0) | Atomic, never split mid-item |
| FLEX_GAP cell run (interior) | Gap (flex-grow:1) | Elastic — contracts on downsize, expands on upsize |
| Single space between words | Flex item (rigid) | Not elastic — preserved as-is |
| Leading FLEX_GAP cells | Part of first item (fixed indent) | Not elastic — indentation preserved |
| cols (target width) | Container width | Determines where items wrap |
| Downsize | Container shrinks | Gaps contract first → items flex-wrap to next line |
| Upsize | Container grows | Wrapped items unwrap → gaps expand proportionally |

### DSP Oversampling Analogy

- **Items = signal** — never altered, never split. The content.
- **Gaps = interpolation** — elastic zero-padding/decimation. The spacing.
- **Upsize = upsample** — insert blank cells (zero-pad) between items, distribute proportionally (anti-imaging filter)
- **Downsize = downsample** — contract gaps (anti-aliasing filter), if still too wide, items wrap (decimate)

The process is lossless for signal content. Only interpolation changes.

### Gap Identity Problem (Key Discovery)

When gaps contract to 1 cell during reflow, they become indistinguishable from single-space word separators. A single downsize→upsize cycle destroys layout structure:

```
Original:  file1      file2      file3      file4
Downsize:  file1 file2 file3  file4           ← gaps contracted to 1,1,1,2
Upsize:    file1 file2 file3                                           file4
           ↑ items merged (single spaces = word separators)   ↑ only remaining gap
```

**Solution:** Cell-level `FLEX_GAP` tag. Video stamps elastic whitespace at write time. Gap identity survives across reflow cycles regardless of width — even a 1-cell gap retains its FLEX_GAP tag.

### Video Stamping: Ground Truth at Source

Two approaches compared:

| Approach | Where | Pro | Con |
|---|---|---|---|
| A. Video stamps at write time | Reader thread, `print()` | SSOT — only actor that knows "application wrote a space" vs "erase cleared a cell" | Adds look-back check to hot path |
| B. Reflow stamps at first use | Message thread, `reflow()` | No reader-thread change | Heuristic — can't distinguish application spaces from erased cells |

**Decision: A.** The distinction between `print(0x20)` and erase is lost the moment the cell hits the buffer. Reflow can't recover it. Video has ground truth.

**Look-back mechanism:** When Video writes a blank cell (codepoint 0x20) via `print()`, check if previous cell is also blank → stamp both as FLEX_GAP. Third+ consecutive blank: previous is already FLEX_GAP → stamp current. Cost: one comparison per blank cell.

Erase operations (ECH, EL, ED) write cells with codepoint 0 (empty), not 0x20 (space). No FLEX_GAP tag. The codepoint distinction is the discriminator.

**Tab handling:** Video stamps all consecutive blanks unconditionally (tab-expanded spaces included). Whether tab whitespace is elastic depends on the PARSER, not Video. Leading FLEX_GAP cells (before first non-blank) → merged into first item (fixed indentation). Interior FLEX_GAP cells → elastic gap. Same cell tag, different parse treatment based on position.

## Principles and Rationale

### Why Flexbox over Cell-Streaming

Cell-streaming (tmux model) treats terminal content as a flat byte array. It has no concept of items or gaps. This is fundamentally incompatible with structured content like columnar output, status bar prompts, or any layout with elastic whitespace.

HTML flexbox is a proven layout algorithm for reactive resizable containers. The mapping to terminal cells is direct: items are non-whitespace runs, gaps are elastic whitespace, container width is column count. The algorithm handles downsize (gap contraction → item wrapping) and upsize (item unwrapping → gap expansion) correctly by construction.

The DSP oversampling analogy reinforces the mental model: items are signal (never destroyed), gaps are interpolation (elastic). This prevents implementations from falling into cell-streaming patterns.

### Why Cell-Level Metadata

Row-level metadata cannot express per-cell classification. The gap identity problem requires each cell to carry its classification through all buffer operations. Cell's existing `contentTag` field (2 bits, values 0-3) has an unused value (2) — `FLEX_GAP` fits with zero size increase.

### Why Video Stamping

SSOT principle. The gap classification must happen at the source — the only point where application-emitted spaces are distinguishable from erase-cleared cells. Deferred classification (at reflow time) requires heuristics that can misfire.

### BLESSED Pillar Mapping

- **B (Bound):** FLEX_GAP tag is bound to Cell — travels with it through all buffer operations. No separate sidecar state.
- **L (Lean):** Algorithm decomposes into 4 phases (parse, collect, resolve, write), each under 30 lines.
- **E (Explicit):** Cell tag makes gap classification explicit. No heuristic inference at reflow time. Flexbox terminology in flag names prevents tmux mental model.
- **S (SSOT):** Video is the single stamping authority. Cell tag is the single source of gap identity.
- **S (Stateless):** Reflow remains a pure transform. No side effects, no State access.
- **E (Encapsulation):** Video stamps (its concern — it writes cells). Parser classifies (its concern — it reads cells). Reflow layouts (its concern — it transforms buffers).
- **D (Deterministic):** Same input buffer → same output buffer. Integer arithmetic, no floating point, no randomness.

## Scaffold

### Cell Change (jam_fonts/cell/jam_cell.h)

```cpp
// Existing values:
static constexpr uint8_t CONTENT_CODEPOINT { 0 };
static constexpr uint8_t CONTENT_GRAPHEME  { 1 };

// New value — uses existing unused bit pattern in 2-bit contentTag field:
static constexpr uint8_t FLEX_GAP          { 2 };  ///< Elastic whitespace — flex-grow cell.
```

Cell stays 8 bytes. No layout change.

### Row Change (jam_fonts/cell/jam_row.h)

```cpp
struct Row
{
    using FlexType = Cell;

    uint16_t usedCols { 0 };
    uint8_t flags { 0 };

    static constexpr uint8_t flexWrap  { 1 << 0 };  ///< Content continues on next row (flex-wrap point).
    static constexpr uint8_t collapsed { 1 << 1 };   ///< Reflow tombstone — row consumed by unwrap/join.

    Cell cells[];
};
```

Same size, same layout, same FAM protocol. Rename only.

### Video Stamping (terminal/Video.h or VideoOps.cpp)

In `Video::print()` (or the cell-write call site), after writing a blank cell:

```cpp
// Look-back: stamp consecutive blanks as FLEX_GAP
if (cell.codepoint() == 0x20)
{
    if (col > 0)
    {
        auto& prev { row->cells[col - 1] };

        if (prev.codepoint() == 0x20)
        {
            if (prev.contentTag() != Cell::FLEX_GAP)
                prev = Cell::make (0x20, Cell::FLEX_GAP, prev.wide(), prev.styleId());

            cell = Cell::make (0x20, Cell::FLEX_GAP, cell.wide(), cell.styleId());
        }
    }
}
```

One comparison per blank cell on the reader thread. Only `print()` path — erases write codepoint 0, not 0x20.

### Reflow Algorithm (terminal/component/Screen.cpp)

Function signature unchanged:
```cpp
static int reflow (jam::Buffer<jam::Row>& dest,
                   const jam::Buffer<jam::Row>& source,
                   int scrollbackLines,
                   int oldVisibleRows,
                   int newVisibleRows,
                   int numHistoryNormal,
                   int numHistoryAlternate,
                   int cursorRow) noexcept;
```

#### Internal Types (local to Screen.cpp, anonymous namespace or file-scope)

```cpp
struct Segment
{
    int offset;     ///< Starting cell index in the logical line.
    int width;      ///< Number of cells.
    bool isGap;     ///< true = elastic FLEX_GAP run, false = content item.
};

struct FlexLine
{
    int firstSegment;   ///< Index into segments array.
    int segmentCount;   ///< Number of segments on this line.
    int itemWidthTotal; ///< Sum of item widths on this line.
    int gapCount;       ///< Number of gaps on this line.
};
```

Stack-allocated arrays. Max segments per logical line: ~341 (1024 cols / 3). Max lines per logical line: ~512 (one item per line worst case).

#### Phase 1 — Parse

Walk logical line cells (across joined flexWrap rows). Classify by FLEX_GAP tag. Leading FLEX_GAP cells merge into first item.

```cpp
// Segment classification rules:
// 1. FLEX_GAP cell runs between content → Gap segment
// 2. Non-FLEX_GAP cell runs (codepoint > 0) → Item segment
// 3. Single non-FLEX_GAP space (0x20) between content → part of enclosing Item
// 4. Leading FLEX_GAP cells (before first content) → part of first Item (fixed indent)
// 5. Trailing cells beyond usedCols → excluded
```

Source cells read via cursor pattern (row, col) advancing through flexWrap source rows. No flat copy needed.

#### Phase 2 — Line Collection (§9.3)

Greedy single-pass. Items pack into lines at `newCols`. Gap hypothetical size = 1 (minimum) for the packing test.

```cpp
for each item segment (in order):
    needed = item.width + (line.hasItems ? 1 : 0)   // 1 for minimum inter-item gap

    if (needed > line.remaining and line.hasItems)
    {
        // Finalize current line, start new
        finalizeLine();
        startNewLine();
    }

    if (item.width > newCols)
    {
        // Single item wider than container — character-level split
        // (only case where an item is broken)
        splitItemAcrossLines (item, newCols);
    }
    else
    {
        line.addItem (item);
        line.remaining -= needed;
    }
```

#### Phase 3 — Flex Resolution (§9.7)

Per line: items are frozen. Distribute free space across gaps.

```cpp
for each FlexLine:
    const int freeSpace { newCols - line.itemWidthTotal };

    if (line.gapCount == 0)
        continue;   // single item, no gaps to distribute

    // Bresenham proportional distribution
    int distributed { 0 };
    int gapSoFar { 0 };
    const int totalGapWidth { /* sum of current gap widths on this line */ };

    for each gap on this line:
        gapSoFar += gap.currentWidth;

        // freeSpace is total available for all gaps on this line
        const int target { (freeSpace * gapSoFar + totalGapWidth / 2) / totalGapWidth };
        gap.newWidth = juce::jmax (1, target - distributed);
        distributed = target;
```

If `totalGapWidth == 0` (gaps have zero current width — from line break join), distribute equally: `gap.newWidth = freeSpace / gapCount` with Bresenham remainder.

#### Phase 4 — Write

Copy item cells from source buffer (via source cursor). Write FLEX_GAP blank cells at computed widths. Set `usedCols` and `flexWrap` flag.

```cpp
for each FlexLine:
    auto* destRow { dest.getWritePointer (0, dstRow) };
    int destCol { 0 };

    for each segment on this line:
        if (segment.isGap)
        {
            const auto blank { Cell::make (0x20, Cell::FLEX_GAP, Cell::NARROW, 0) };

            for (int i { 0 }; i < segment.newWidth and destCol < newCols; ++i)
                destRow->cells[destCol++] = blank;
        }
        else
        {
            for (int i { 0 }; i < segment.width and destCol < newCols; ++i)
            {
                destRow->cells[destCol] = sourceCursor.read();
                sourceCursor.advance();
                ++destCol;

                // Wide char: copy SPACER_TAIL
                // Wide char at last column: insert SPACER_HEAD
            }
        }

    destRow->usedCols = static_cast<uint16_t> (destCol);
    destRow->flags = (moreLinesFollow) ? Row::flexWrap : 0;
    ++dstRow;
```

#### Alternate Screen (Channel 1)

Unchanged from current: verbatim viewport copy. Apps redraw after SIGWINCH.

#### Return Value

```cpp
const int newHistoryNormal { juce::jmax (0, dstRow - newVisibleRows) };
return newHistoryNormal;
```

### Call Site Renames

All `Row::wrapped` → `Row::flexWrap`, all `Row::dead` → `Row::collapsed` across:
- `Video::resolveWrapPending()` — sets `flexWrap`
- `Screen::reflow()` — reads `flexWrap`, sets `collapsed`
- Any other reader of `row->flags & Row::wrapped`

Grep `Row::wrapped` and `Row::dead` across END and jam codebases for complete list.

### DIAG Removal

All `// DIAG` markers from Sprint 30 removed. Grep `// DIAG` across END codebase.

## BLESSED Compliance Checklist

- [x] Bounds — FLEX_GAP tag bound to Cell, travels with it. No separate sidecar.
- [x] Lean — Four phases, each decomposed into helper functions under 30 lines. Stack arrays, no heap.
- [x] Explicit — Cell tag makes classification explicit. Flexbox terminology prevents tmux mental model.
- [x] SSOT — Video is sole stamping authority. Cell tag is sole source of gap identity.
- [x] Stateless — Reflow is a pure transform. No side effects, no State, no Video access.
- [x] Encapsulation — Video stamps (writer concern). Parser classifies (reader concern). Reflow layouts (transform concern).
- [x] Deterministic — Same input → same output. Integer arithmetic, Bresenham distribution, no floating point.

## Open Questions

None. All design decisions resolved in discussion.

## Handoff Notes

1. **Row flag rename is mechanical but wide.** `Row::wrapped` and `Row::dead` are referenced across both END and jam codebases. Grep both trees. The rename is safe — same semantics, different name.

2. **Video stamping location.** The look-back goes in the cell-write path of `print()` — wherever Video writes a character cell to the buffer. Not in erase paths (ECH, EL, ED write codepoint 0). Study `Video::print()` or the equivalent cell-write call site to find the exact insertion point.

3. **Cell `FLEX_GAP` constant.** Must be added alongside existing `CONTENT_CODEPOINT` and `CONTENT_GRAPHEME` in `jam::Cell`. The value is 2 (fits in existing 2-bit contentTag field). The `make()` factory and accessor methods already handle arbitrary contentTag values — no accessor changes needed.

4. **Parser leading-whitespace rule.** Leading FLEX_GAP cells (before first non-blank cell in the logical line) are merged into the first Item segment. This preserves indentation as fixed-width content. Interior FLEX_GAP cells are Gap segments. This is a parse-phase concern, not a Video concern.

5. **Character-level item split.** The only case where an item is broken is when a single item is wider than `newCols`. This uses the same wide-char boundary handling as the current implementation (SPACER_HEAD insertion when WIDE char lands on last column).

6. **Gap loss at line breaks.** When items wrap to a new line, the gap between the last item of line N and the first item of line N+1 vanishes. On subsequent upsize and unwrap, those items appear adjacent. This is an accepted tradeoff — the shell redraws after SIGWINCH. The gap loss is a brief visual artifact during the DST crossfade transition.

7. **No new files.** All internal types (Segment, FlexLine) are local to Screen.cpp. No public API changes. No new headers.

8. **The reflow function is a PURE TRANSFORM.** It reads source Buffer\<Row\>, writes dest Buffer\<Row\>. No State access. No Video. No side effects. The function signature, DST lifecycle, Display wiring, and alternate screen handling are all unchanged.

9. **DIAG removal.** Sprint 30 left `// DIAG` diagnostic logging throughout. Grep `// DIAG` in END codebase for complete list. All must be removed in this sprint.
