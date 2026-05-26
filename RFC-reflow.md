# Request For Comment — Text Reflow
Date: 2026-05-26
Status: Ready for COUNSELOR handoff

## Problem Statement

When the TextEditor viewport resizes, the existing `Buffer<Row>` was stamped
by the parser at the original column width. `flexWrap` flags are correct for
the old dimensions only. Rendering from the old buffer at a new viewport width
produces incorrect line breaks. The buffer must be reflowed into a new
allocation before the renderer sees it.

## Research Summary

Confirmed from codebase inspection (`jam_row.h`, `Video.cpp`,
`jam_glyph_arrangement.cpp`, `jam_JustifiedText.h`, `jam_buffer.h`,
`jam_block.h`, `jam_cell.h`, `jam_ParagraphStorage.h`):

- `Row::usedCols` — rightmost written display-column + 1. Wide glyphs count
  as 2. Confirmed via `print()` and `resolveWrapPending()` in `Video.cpp`.
- `Row::flexWrap` — stamped by `Video::resolveWrapPending()` when DECAWM
  wraps a line. Records where the parser wrapped, not where the display
  should wrap. Not authoritative for reflow input — authoritative only after
  reflow writes new stamps into the scratch buffer.
- `Row::justify` — set when row contains `FLEX_GAP` cells.
- `Cell::FLEX_GAP` — elastic whitespace. A run of ≥ 2 consecutive FLEX_GAP
  cells is an atomic unit; must not be split at a wrap boundary.
- `Cell::WIDE` + `Cell::SPACER_TAIL` — two-cell atom; must not be split at a
  wrap boundary. Already enforced in `Video::print()`.
- `Block<Row>` — non-owning view. All rendering and reflow reads through
  Block only. Buffer is never accessed directly by rendering pipeline.
- `ParagraphsModel` — scans `flexWrap` flags to group buffer rows into
  logical paragraphs. Paragraph = N contiguous rows terminated by a row with
  `flexWrap == 0`.
- `glyph::Arrangement::shapeImpl` — currently dispatches `buildArrangements`
  per buffer row. Must be changed to dispatch per paragraph for correct
  reflow-aware rendering.
- `JustifiedText` — consumes shaped `Arrangement`; distributes free space
  across `FLEX_GAP` entries per line. Operates on visual lines only;
  unaffected by reflow design.
- Resizer — owns Buffer swap machinery. Uses a trigger/registration map.
  Reflow must register as a trigger that fires after scratch allocation,
  before buffer swap.

Path A (pure display transform, no writeback) was considered and rejected:
Buffer and visual line breaks would disagree. History content written at 80
cols reinterpreted at 120 cols without writeback corrupts logical paragraphs
irreversibly.

Path B (upstream rewrites old content into new buffer dimensions without
reflow) was considered and rejected: old content packed into new column width
without paragraph-aware re-packing loses original paragraph boundaries.
Lossless is false.

## Principles & Rationale

**Reflow is a Buffer→Buffer transform registered with the Resizer.**

It runs after scratch allocation, before buffer swap. It is the SSOT for
`flexWrap` stamps in the new buffer. The renderer always sees a correctly
stamped buffer and shapes with `wrapColumns == 0`.

BLESSED mapping:
- **Bounds** — reflow reads old `Block<Row>`, writes into scratch `Buffer<Row>`.
  Ownership is explicit. No aliasing.
- **Lean** — no speculative abstraction. Two passes: count rows, write rows.
- **Explicit** — wrap boundary rule is stated precisely (see below). No
  implicit cell splitting.
- **SSOT** — `flexWrap` in the new buffer is the single authority for line
  breaks. Parser stamps are correct for original dimensions only.
- **Stateless** — `Reflow` is a pure function of old Block + new col count.
  No retained state between calls.
- **Encapsulation** — Reflow does not touch Buffer internals. Writes through
  `Block<Row>` write pointers into scratch only.
- **Deterministic** — same old Block + same new col count always produces the
  same output buffer.

## Scaffold

### Logical line cap

User config supplies `maxLogicalLines` (default 10 000). This is a paragraph
count cap, not a physical row cap. If reflow would produce more paragraphs
than the cap, oldest paragraphs are dropped (same policy as normal append
overflow).

### Pass 1 — compute new physical row count

```cpp
// O(n) over Block rows. No paragraph awareness needed.
// Returns physical row count needed in scratch buffer.
static int computePhysicalRows (const jam::Block<jam::Row>& oldBlock,
                                 int newCols) noexcept
{
    int totalGlyphs { 0 };

    for (int r { 0 }; r < oldBlock.getNumRows(); ++r)
        totalGlyphs += oldBlock.getRowPointer (r)->usedCols;

    // Integer ceiling division.
    return newCols > 0 ? (totalGlyphs + newCols - 1) / newCols : 0;
}
```

Resizer calls this, allocates `scratch` at `(newCols, physicalRowCount)`,
then calls Pass 2.

### Pass 2 — reflow write

**Wrap boundary rule (universal):**
Any multi-cell atom that would be split by the wrap boundary is bumped
entirely to the next output row. Vacated columns on the previous output row
are filled with `Cell::erase(styleId)` blank cells. Atoms:
- `FLEX_GAP` run: all consecutive FLEX_GAP cells move together.
- `WIDE` + `SPACER_TAIL`: pair moves together (matches `Video::print()` rule).

**Algorithm per paragraph:**

1. Walk constituent old buffer rows in order (via `ParagraphsModel`).
2. For each old row, walk cells 0..`usedCols-1`.
3. Identify atom type at current cell:
   - `WIDE`: atom size = 2 (cell + SPACER_TAIL).
   - `FLEX_GAP`: atom size = run length of consecutive FLEX_GAP cells.
   - All others: atom size = 1.
4. If `outputCol + atomSize > newCols`: fill remainder of current output row
   with blank cells, advance to next output row, reset `outputCol = 0`.
5. Write atom cells to output row starting at `outputCol`. Advance
   `outputCol += atomSize`. Update `outputRow->usedCols`.
6. After exhausting all cells of the paragraph: clear `flexWrap` on the
   final output row (paragraph terminator). Set `flexWrap` on all preceding
   output rows of this paragraph.
7. Set `justify` flag on any output row that received a FLEX_GAP cell.

```cpp
struct Reflow
{
    // Pass 1 — called by Resizer before scratch allocation.
    static int computePhysicalRows (const jam::Block<jam::Row>& oldBlock,
                                    int newCols) noexcept;

    // Pass 2 — called by Resizer after scratch allocation, before swap.
    // paragraphs must be built from oldBlock before calling.
    static void write (const jam::Block<jam::Row>& oldBlock,
                       const jam::ParagraphsModel& paragraphs,
                       jam::Buffer<jam::Row>& scratch,
                       int newCols) noexcept;
};
```

`write()` is stateless. It takes no mutable state beyond its parameters.
It does not call into Buffer directly — it obtains a `Block<jam::Row>` write
view from scratch and writes through that.

### Resizer registration

```cpp
resizer.on (ResizeEvent::scratchReady, [&]() {
    const auto oldBlock   { screen.getActiveBlock() };
    const auto paragraphs { jam::ParagraphsModel::buildFrom (oldBlock) };
    jam::Reflow::write (oldBlock, paragraphs, resizer.getScratch(), newCols);
});
```

### Arrangement shape change

`shapeImpl` currently dispatches `buildArrangements` per buffer row.
After reflow lands in the buffer, this is correct — each buffer row is one
physical line. `wrapColumns` is always `0` post-reflow.

For the Whelmed (rich text editor) use case where no upstream reflow exists,
`shapeImpl` must dispatch per paragraph. This requires `ParagraphsModel` to
be passed into `shape()` or built internally from the Block's `flexWrap`
flags. This is a separate RFC — out of scope here.

## BLESSED Compliance Checklist

- [x] Bounds — explicit ownership: old Block read-only, scratch write-only,
      no aliasing
- [x] Lean — two passes, no speculative state, no intermediate allocation
      beyond scratch
- [x] Explicit — wrap boundary rule fully stated, no implicit splitting
- [x] SSOT — new buffer `flexWrap` is the single authority post-reflow
- [x] Stateless — `Reflow` is a pure function, no retained state
- [x] Encapsulation — reflow does not expose Buffer internals; writes through
      Block write view
- [x] Deterministic — same inputs always produce same output

## Open Questions

1. **`ParagraphsModel::buildFrom` static factory** — does this exist or does
   COUNSELOR need to add it? Current API is `build(const Block<Row>&)` as a
   mutating method on an instance.

2. **Scratch Block write view** — `Buffer<Row>` does not currently expose a
   `Block<Row>` write view directly. Does `resizer.getScratch()` return
   `Buffer<Row>&` and reflow constructs `Block<Row>(scratch, 0)` itself, or
   does Resizer hand a pre-constructed write Block?

3. **Alternate screen** — does reflow apply to both normal and alternate
   screen buffers, or alternate screen only on explicit user resize? Alternate
   screen is typically not a scrollback buffer.

4. **Whelmed / rich text path** — `shapeImpl` paragraph-level dispatch is
   deferred. Confirm sequencing: does this RFC land before or after the
   Whelmed editor work begins?

5. **SIGWINCH timing** — reflow must complete before SIGWINCH fires to the
   pty. Is the Resizer's trigger map guaranteed ordered (reflow before
   SIGWINCH), or does COUNSELOR need to enforce ordering explicitly?

## Handoff Notes

- `flexWrap` is a parser artifact in the old buffer and a reflow output in
  the new buffer. These are the same flag with different provenance.
  COUNSELOR must not conflate them.
- `usedCols` confirmed as display-column width (wide = 2). Area calculation
  `totalGlyphs = sum of usedCols` is correct.
- Wrap boundary rule is universal: WIDE pairs and FLEX_GAP runs are atomic.
  No exceptions. Matches existing `Video::print()` wide-char behavior.
- `maxLogicalLines` cap applies to paragraph count, not physical row count.
  Oldest paragraphs dropped on overflow.
- `Arrangement::shapeImpl` paragraph-level dispatch is a follow-on RFC.
  This RFC covers only the Buffer→Buffer reflow transform and Resizer
  integration.
- All rendering pipeline components (`glyph::Arrangement`, `JustifiedText`,
  `ContentView`) are unaffected by this RFC. They see a correctly stamped
  buffer and operate as today.
