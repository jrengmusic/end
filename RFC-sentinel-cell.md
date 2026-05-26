# RFC — Eliminate Row FAM: Sentinel Cell Metadata Architecture

Date: 2026-05-26
Status: Ready for COUNSELOR handoff

## Problem Statement

Row is a C99 FAM struct (`uint16_t usedCols` + `uint8_t flags` + `Cell cells[]`).
It prevents trivially-copyable buffer operations, forces runtime stride computation
from FAM, blocks memcpy across widths for reflow, and propagates a dual-path
rendering pipeline (`Block<Row>` vs `Block<Cell>`) through every layer from Video
to TextEditor.

ARCHITECT proposed: eliminate Row entirely. Flat `Buffer<Cell>` with a sentinel
cell at position `cells[cols]` per row, carrying row metadata in Cell's 23 unused
padding bits (41-63).

## Research Summary

### Cell bit layout (jam_cell.h:102-108)

```
| codepoint (21) | contentTag (2) | wide (2) | styleId (16) | padding (23) |
  bits 0-20        bits 21-22       bits 23-24  bits 25-40     bits 41-63
```

`sizeof(Cell) == 8`, `is_trivially_copyable_v<Cell> == true`.
Bits 41-63 are always 0 in the current codebase. 23 bits available.

### Row metadata inventory

From `jam_row.h:27-39`:

| Field | Current type | Bits needed | Source |
|-------|-------------|-------------|--------|
| flexWrap | `flags & (1<<0)` | 1 | Video::resolveWrapPending (Video.cpp:470) |
| justify | `flags & (1<<2)` | 1 | Video::print (Video.cpp:634) |
| collapsed | `flags & (1<<1)` | 1 | Reflow tombstone (not yet used) |
| usedCols | `uint16_t` | 12 (max 4096) | Video::print (Video.cpp:621) |
| **Total** | | **15** | **8 spare** |

From `jam_cell.h:237-259` — `Cell::RowState` is a standalone `uint8_t` struct
with a single `doubleWidth` flag. **Dead code** — not referenced by any
implementation file in END or jam. Deleted as part of this RFC.

### Proposed sentinel bit layout (bits 41-63)

```
| flexWrap (1) | justify (1) | collapsed (1) | usedCols (12) | spare (8) |
  bit 41         bit 42        bit 43           bits 44-55      bits 56-63
```

### Row FAM stride formula (jam_buffer.h:95-100)

```cpp
// Current FAM path (hasFlexType<Row> == true):
constexpr int alignment { jmax (1, 64 / sizeof (Row::FlexType)) };  // = 8
newAlignedCols = (newNumCols + alignment - 1) & ~(alignment - 1);
newRowStride = sizeof (Row) + alignedCols * sizeof (Cell);           // 3 + n*8
```

### Impact surface — complete file:line map

**jam (shared library):**

| File | What changes |
|------|-------------|
| `jam_row.h` | **Deleted.** Row struct, FlexType alias, flag constants. |
| `jam_cell.h:237-259` | **Delete** RowState struct. **Add** sentinel bit constants + static accessor functions. |
| `jam_buffer.h:21-29` | **Delete** `has_flex_type` trait, `hasFlexType` variable template. |
| `jam_buffer.h:95-100` | **Delete** FAM stride branch. Flat branch adds +1 before alignment. |
| `jam_block.h` | No Row-specific code. Template works with Cell unchanged. |
| `jam_glyph_arrangement.h:171-185` | **Delete** two `shape(Block<Row>)` overloads. |
| `jam_glyph_arrangement.cpp:145-170` | **Delete** `shape(Block<Row>)` implementations. **Modify** `shape(Block<Cell>)` lambda (line 122-126) to read `usedCols` from sentinel. |
| `jam_ParagraphStorage.h:53-71` | **Modify** `build()`: takes `Block<Cell>`, reads sentinel `isFlexWrap()` instead of `Row::flexWrap`. |
| `jam_text_editor.h:78-102` | **Delete** `setText(Block<Row>)`, `rowContent` member, `hasRowContent` flag. Single `setText(Block<Cell>)` path. |
| `jam_text_editor.cpp:88-91` | **Delete** `calc()` dual-path dispatch. Single `arrangement.shape(content, ...)`. |
| `jam_text_editor.cpp:128-134` | **Delete** `setText(Block<Row>)` implementation. |
| `jam_discrete_state_transition.h` | No change — fully generic template. |

**END:**

| File | What changes |
|------|-------------|
| `Video.h:340` | `std::atomic<Block<Row>*>&` → `std::atomic<Block<Cell>*>&` |
| `Video.h:349` | `Block<Row>* blocks` → `Block<Cell>* blocks` |
| `Video.cpp:470` | `completedRow->flags \|= Row::flexWrap` → `Cell::setFlexWrap(row[numCols], true)` |
| `Video.cpp:634` | `writeRowPtr->flags \|= Row::justify` → `Cell::setJustify(row[numCols], true)` |
| `Video.cpp:621` | `writeRowPtr->usedCols = ...` → `Cell::setUsedCols(row[numCols], ...)` |
| `Video.cpp:378-379, 430-431` | `row->usedCols = 0; row->flags = 0;` → sentinel at `row[numCols]` zeroed (memset covers it, or explicit `row[numCols] = Cell{}`) |
| `VideoEdit.cpp` | ~11 sites: same pattern — metadata access via sentinel at `row[numCols]` |
| `VideoCSI.cpp:565-568` | Same pattern as Video.cpp scroll fills |
| `Screen.h:119` | `Buffer<Row> buffers[2]` → `Buffer<Cell> buffers[2]` |
| `Screen.h:122` | `std::array<Block<Row>, 2> blockSets[2]` → `std::array<Block<Cell>, 2> blockSets[2]` |
| `Screen.h:126` | `std::atomic<Block<Row>*>` → `std::atomic<Block<Cell>*>` |
| `Screen.cpp` | `resizeBuffers()` — mechanical type change. Block construction unchanged. |
| `Session.h:258` | `DST<Row>` → `DST<Cell>` |
| `Processor.h:122` | `std::atomic<Block<Row>*>&` → `std::atomic<Block<Cell>*>&` |

### memset safety analysis

4 memset sites in VideoEdit.cpp (lines 93, 167, 294, 312) zero `cells[cCol..nCols-1]`
or `cells[0..cCol-1]` — partial row erases. The sentinel at `cells[numCols]` is
**outside** these ranges. Safe.

`Block::clear()` (jam_block.h:248-273) memsets full stride to zero — including the
sentinel. This is correct: clearing a row resets both content and metadata. Video
re-stamps the sentinel on next write.

No `operator==`, no hash, no memcmp on Cell data found anywhere in the codebase.
`Cell::getKey()` (jam_cell.h:276) encodes a `(row, col)` grid coordinate — does
not touch Cell's u64 data. No padding-bit pollution risk.

### Whelmed compatibility

Whelmed uses `Block<Cell>` today (TextEditor single-path after this RFC). Whelmed
never references Row, Block<Row>, usedCols, or flexWrap. The only change Whelmed
sees is `Buffer<Cell>` allocating one extra sentinel cell per row — 8 bytes/row,
negligible. **Whelmed path is unaffected.**

### DST template

`DiscreteStateTransition` (jam_discrete_state_transition.h) is fully generic —
parameterized on `ElementType`, holds `Buffer<ElementType> scratch`. No
Row-specific API. `DST<Row>` → `DST<Cell>` is a pure template parameter swap.

## Principles and Rationale

**Why sentinel cell over parallel sidecar array:**

Cell is 8 bytes, trivially copyable, naturally atomic on x86-64/ARM64.
Sentinel cell lives in the same allocation as content cells — no second
allocation, no lifetime mismatch, no cache miss on metadata access.
Metadata travels with the row through memcpy, ring rotation, and buffer swap
without any coordination.

BLESSED mapping:

- **Bounds** — sentinel is within the row's stride allocation. One owner (Buffer),
  one allocation, no aliasing.
- **Lean** — no wrapper type, no accessor object, no intermediate state. Named
  constants + static functions on Cell.
- **Explicit** — sentinel position is `cells[numCols]`, stated once, deterministic.
  Content cells are `cells[0..numCols-1]`. Boundary is the logical terminal width.
- **SSOT** — sentinel cell IS the row metadata. Not a copy, not a cache, not
  synced from elsewhere.
- **Stateless** — static functions on Cell operate on the sentinel cell passed by
  reference. No retained state.
- **Encapsulation** — Buffer allocates the sentinel internally (+1). Callers see
  logical width via `getNumCols()`. Sentinel access is explicit by indexing
  `row[numCols]`.
- **Deterministic** — same bit layout, same constants, same functions. No runtime
  dispatch, no polymorphism.

**Why eliminate Row entirely (not deprecate):**

Row has exactly one consumer: END's terminal pipeline. Whelmed uses Block<Cell>.
No other jam consumer references Row. Keeping a deprecated Row adds dead code,
maintains the FAM stride path in Buffer, and preserves Block<Row> shape overloads
— all violations of Lean. Clean deletion.

## Scaffold

### 1. Sentinel bit constants on Cell (jam_cell.h)

```cpp
// Sentinel metadata — bits 41-63 of the packed u64.
// Only meaningful on the sentinel cell at position cells[numCols] per row.
// Content cells (0..numCols-1) have bits 41-63 == 0 always.

static constexpr uint64_t flexWrapBit   { uint64_t (1) << 41 };
static constexpr uint64_t justifyBit    { uint64_t (1) << 42 };
static constexpr uint64_t collapsedBit  { uint64_t (1) << 43 };
static constexpr int      usedColsShift { 44 };
static constexpr uint64_t usedColsMask  { uint64_t (0xFFF) << 44 };
```

### 2. Static accessor functions on Cell (jam_cell.h)

```cpp
// Sentinel read accessors — pass the sentinel cell at row[numCols].

static bool isFlexWrap (const Cell& sentinel) noexcept
{
    return (sentinel.data & flexWrapBit) != 0;
}

static bool isJustify (const Cell& sentinel) noexcept
{
    return (sentinel.data & justifyBit) != 0;
}

static bool isCollapsed (const Cell& sentinel) noexcept
{
    return (sentinel.data & collapsedBit) != 0;
}

static int getUsedCols (const Cell& sentinel) noexcept
{
    return static_cast<int> ((sentinel.data & usedColsMask) >> usedColsShift);
}

// Sentinel write accessors.

static void setFlexWrap (Cell& sentinel, bool value) noexcept
{
    sentinel.data = (sentinel.data & ~flexWrapBit)
                  | (static_cast<uint64_t> (value) << 41);
}

static void setJustify (Cell& sentinel, bool value) noexcept
{
    sentinel.data = (sentinel.data & ~justifyBit)
                  | (static_cast<uint64_t> (value) << 42);
}

static void setCollapsed (Cell& sentinel, bool value) noexcept
{
    sentinel.data = (sentinel.data & ~collapsedBit)
                  | (static_cast<uint64_t> (value) << 43);
}

static void setUsedCols (Cell& sentinel, int cols) noexcept
{
    jassert (cols >= 0 and cols <= 0xFFF);
    sentinel.data = (sentinel.data & ~usedColsMask)
                  | (static_cast<uint64_t> (cols) << usedColsShift);
}
```

### 3. Buffer stride change (jam_buffer.h)

Delete `has_flex_type` trait (lines 21-29). Delete FAM branch (lines 95-100).
Single stride path with +1 sentinel:

```cpp
void setSize (int newNumChannels, int newNumRows, int newNumCols) noexcept
{
    jassert (newNumChannels >= 0 and newNumRows >= 0 and newNumCols >= 0);
    jassert (newNumChannels <= maxPreallocatedChannels);

    constexpr int alignment { juce::jmax (1, 64 / static_cast<int> (sizeof (ElementType))) };
    const int newAlignedCols { ((newNumCols + 1) + alignment - 1) & ~(alignment - 1) };
    const size_t newRowStride { static_cast<size_t> (newAlignedCols) * sizeof (ElementType) };

    // ... rest unchanged — totalBytes, realloc, channel pointers, head clamping.
    // numCols stores newNumCols (logical width, NOT +1).
}
```

`getNumCols()` returns the logical terminal width. Sentinel is at
`getRowPointer(r)[getNumCols()]` — within the allocation but beyond the
reported column count.

### 4. shape(Block<Cell>) sentinel-aware extractor (jam_glyph_arrangement.cpp)

```cpp
void glyph::Arrangement::shape (const jam::Block<jam::Cell>* blocks,
                                 int numBlocks,
                                 const jam::Font& font,
                                 int wrapColumns,
                                 int lineOffset) noexcept
{
    shapeImpl (blocks, numBlocks, font, wrapColumns, lineOffset,
               [] (const jam::Block<jam::Cell>& block, int r, int numCols)
                  -> std::pair<const jam::Cell*, int>
               {
                   const jam::Cell* row { block.getRowPointer (r) };
                   return { row, jam::Cell::getUsedCols (row[numCols]) };
               });
}
```

The lambda reads `usedCols` from the sentinel at `row[numCols]`. Content cells
shaped are `row[0..usedCols-1]` — identical semantics to the old Row extractor.

### 5. ParagraphsModel sentinel-aware scan (jam_ParagraphStorage.h)

```cpp
void build (const jam::Block<jam::Cell>& block) noexcept
{
    clear();

    const int totalRows { block.getNumRows() };
    const int numCols   { block.getNumCols() };
    int rowIdx { 0 };

    while (rowIdx < totalRows)
    {
        const int lineStart { rowIdx };

        while (rowIdx < totalRows
               and jam::Cell::isFlexWrap (block.getRowPointer (rowIdx)[numCols]))
            ++rowIdx;

        ++rowIdx;
        paragraphs.add ({ lineStart, rowIdx - lineStart });
    }
}
```

### 6. TextEditor single path (jam_text_editor.h/cpp)

Header — delete `rowContent`, `hasRowContent`, `setText(Block<Row>)`:

```cpp
void setText (jam::Block<jam::Cell> block) noexcept;

// Members:
jam::Block<jam::Cell>  content;
jam::ParagraphsModel   paragraphsModel;
jam::glyph::Arrangement arrangement;
```

Implementation:

```cpp
void TextEditor::setText (jam::Block<jam::Cell> block) noexcept
{
    content = block;
    paragraphsModel.build (content);
    calc();
}

void TextEditor::calc() noexcept
{
    glyphGraphics.clear();
    jassert (font.bounds.isValid());

    const int numRows { content.getNumRows() };
    const int numCols { content.getNumCols() };

    if (numRows > 0)
    {
        shapedTextOptions = jam::ShapedTextOptions {}.withWrapColumns (0);
        arrangement.shape (content, font, shapedTextOptions.wrapColumns, 0);

        const int numLines { numRows };
        const auto contentPixels { jam::Cell::Point::totalPixels<int> (
            cell { numCols }, cell { numLines }, font.bounds) };

        const int contentH { contentPixels.y };
        const int oldContentH { contentView->getHeight() };
        const int viewportMaxH { viewport->getMaximumVisibleHeight() };
        const int viewY { viewport->getViewPositionY() };
        const bool wasAtBottom {
            viewY >= juce::jmax (0, oldContentH - viewportMaxH - font.bounds.height) };

        contentView->setSize (contentPixels.x, contentH);

        if (wasAtBottom)
        {
            const int maxY { juce::jmax (0, contentH - viewportMaxH) };
            viewport->setViewPosition (0, maxY);
        }
    }

    contentView->repaint();
}
```

### 7. Video write pattern (representative transformation)

Before (Video.cpp:621, 470, 634):
```cpp
jam::Row* writeRowPtr { blocks[scr].getWritePointer (writeRow, writePosition[scr]) };
writeRowPtr->cells[writeCol] = cell;
writeRowPtr->usedCols = static_cast<uint16_t> (jmax (static_cast<int> (writeRowPtr->usedCols), writeCol + charWidth));
completedRow->flags |= jam::Row::flexWrap;
writeRowPtr->flags |= jam::Row::justify;
```

After:
```cpp
jam::Cell* row { blocks[scr].getWritePointer (writeRow, writePosition[scr]) };
row[writeCol] = cell;
jam::Cell::setUsedCols (row[numCols], jmax (jam::Cell::getUsedCols (row[numCols]), writeCol + charWidth));
jam::Cell::setFlexWrap (row[numCols], true);
jam::Cell::setJustify (row[numCols], true);
```

`numCols` read from State atomics — Video already reads this value every flush.

### 8. Row metadata clear pattern (Video.cpp:378-379, 430-431, VideoEdit.cpp, VideoCSI.cpp)

Before:
```cpp
row->usedCols = 0;
row->flags    = 0;
```

After — sentinel is just another cell, zero it:
```cpp
row[numCols] = jam::Cell {};
```

Equivalent: all 64 bits zeroed — usedCols=0, all flags=0, content fields=0.

### 9. Reflow integration

RFC-reflow.md signatures change mechanically:

```cpp
struct Reflow
{
    static int computePhysicalRows (const jam::Block<jam::Cell>& oldBlock,
                                    int newCols) noexcept;

    static void write (const jam::Block<jam::Cell>& oldBlock,
                       const jam::ParagraphsModel& paragraphs,
                       jam::Buffer<jam::Cell>& scratch,
                       int newCols) noexcept;
};
```

Key reflow improvements from flat Cell storage:

- **Same-width resize:** stride is identical across old and new buffers. Row
  memcpy (including sentinel) is viable — no FAM header to skip.
- **Cross-width reflow:** walk old rows as `row[0..usedCols-1]` (reading
  `usedCols` from sentinel), write to new rows as `row[0..newCols-1]`, stamp
  new sentinel. Sequential Cell access — no FAM pointer arithmetic.
- **Pass 1 (computePhysicalRows):** reads `Cell::getUsedCols(row[numCols])`
  per row instead of `row->usedCols`. Same O(n).
- **Pass 2 (write):** stamps `Cell::setFlexWrap(row[newCols], ...)` and
  `Cell::setUsedCols(row[newCols], ...)` on scratch sentinel. Same algorithm,
  simpler addressing.

### 10. Dead code removal summary

| Deleted | Location |
|---------|----------|
| `jam::Row` struct | jam_row.h (file deleted) |
| `Cell::RowState` struct | jam_cell.h:237-259 |
| `has_flex_type` trait + `hasFlexType` variable template | jam_buffer.h:21-29 |
| FAM stride branch in `Buffer::setSize()` | jam_buffer.h:95-100 |
| `shape(Block<Row>*)` array overload | jam_glyph_arrangement.h:171-172, .cpp:145-158 |
| `shape(Block<Row>&)` single overload | jam_glyph_arrangement.h:184-185, .cpp:164-170 |
| `TextEditor::setText(Block<Row>)` | jam_text_editor.h:81, .cpp:128-134 |
| `TextEditor::rowContent` member | jam_text_editor.h:100 |
| `TextEditor::hasRowContent` flag | jam_text_editor.h:101 |
| `TextEditor::calc()` dual-path dispatch | jam_text_editor.cpp:81-91 |
| `Row::FlexType` alias | jam_row.h:29 |
| `Row::flexWrap`, `Row::collapsed`, `Row::justify` constants | jam_row.h:34-36 |

## BLESSED Compliance Checklist

- [x] Bounds — sentinel within row stride allocation, single owner (Buffer)
- [x] Lean — no wrapper type, no accessor object, no FAM machinery
- [x] Explicit — sentinel position = `cells[numCols]`, constants named, functions named per NAMES.md
- [x] SSOT — sentinel cell IS the metadata, not synced from elsewhere
- [x] Stateless — static functions, no retained state
- [x] Encapsulation — Buffer hides +1 allocation, getNumCols() returns logical width
- [x] Deterministic — same bit layout, same constants, no runtime dispatch

## Open Questions

1. **Same-width resize fast path** — with identical stride, bulk memcpy of rows
   (content + sentinel) is viable for same-width resize (only row count changes).
   Should COUNSELOR implement this optimization, or rely on the generic
   row-by-row copy?

2. **Alternate screen reflow** — carried from RFC-reflow.md: does reflow apply
   to the alternate screen buffer (typically used by full-screen apps with no
   scrollback)?

3. **ParagraphsModel::build call site** — `TextEditor::setText(Block<Cell>)`
   now calls `paragraphsModel.build(content)` on every setText. Previously only
   the Block<Row> path built paragraphs. Whelmed's Block<Cell> path skipped it.
   With single path, Whelmed will also build paragraphs. Verify this is
   harmless (Whelmed content has no flexWrap flags — all rows are standalone
   paragraphs, build is O(n) scan with no allocation beyond Array growth).

4. **SIGWINCH timing** — carried from RFC-reflow.md: reflow must complete before
   SIGWINCH fires. DST stop trigger sequence (reflow → swap → resume → setWinsize)
   is synchronous within the trigger callback. Confirm ordering is guaranteed.

## Handoff Notes

- **PLAN Steps 1-3 are locked.** WriteHead, Block mutable access, DST ownership —
  all preserved. This RFC changes only the element type flowing through those
  structures (Row → Cell).

- **Video reads `numCols` from State atomics** — already does this. No new data
  path needed for sentinel access. `numCols` is the sentinel offset.

- **Buffer +1 is universal.** Every `Buffer<T>` allocates one extra element per
  row. Cost is `sizeof(T)` per row — 8 bytes for Cell. For a 10,000-row
  scrollback that is 80KB. Negligible.

- **Content cells stay pure.** Bits 41-63 are always 0 on content cells
  (`cells[0..numCols-1]`). Only the sentinel at `cells[numCols]` carries
  metadata. No display-atom pollution.

- **memset on content cells is safe.** The 4 partial-row memset sites in
  VideoEdit.cpp erase within `cells[0..numCols-1]`. Sentinel is untouched.
  `Block::clear()` zeros the full stride including sentinel — correct behavior
  for a full row reset.

- **Whelmed is unaffected.** Uses `Block<Cell>`, never references Row. Gets a
  harmless +1 sentinel allocation. `ParagraphsModel::build` on Whelmed content
  produces one paragraph per row (no flexWrap flags) — functionally identical
  to the previous no-build path, just with an explicit scan.

- **Arrangement `shapeImpl` is a template** — parameterized on the extractor
  lambda. Deleting Block<Row> overloads does not affect the template. The
  Block<Cell> extractor lambda changes from `{ row, numCols }` to
  `{ row, Cell::getUsedCols(row[numCols]) }` — one line.

- **Cell static_asserts preserved.** `sizeof(Cell) == 8` and
  `is_trivially_copyable_v<Cell>` remain true. Sentinel metadata is stored in
  existing padding bits — no structural change to Cell.
