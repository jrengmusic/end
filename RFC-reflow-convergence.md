# RFC — Grid Reflow Convergence: Faithful tmux Translation

Date: 2026-05-20
Status: Ready for COUNSELOR handoff

## Problem Statement

END's `Grid::reflow()` diverged from the tmux model it was based on. The divergence introduced two bugs:
1. **Downsize never wraps correctly** — interaction between `reflowSplit`'s inline continuation loop and the outer loop's `consumed`/`cellOffset` tracking causes double-processing of partially consumed rows.
2. **Upsize doubles content** — `reflowJoin` over-consumes wrapped successors, and the active prompt gets processed twice through both the join's inner loop and the outer loop's next iteration.

Root cause: END replaced tmux's dead-row tombstone pattern with `consumed` counter + `cellOffset` bookkeeping, and inlined join logic into split. The outer loop's advancement arithmetic cannot correctly account for what the inner loops consumed.

GridResize (the timing/architecture machinery — coalesce timer, call sequence) is correct and stays. Only the algorithm inside `Grid::reflow()` and its static helpers is broken.

## Research Summary

### tmux's Canonical Model (grid.c, screen.c)

**Three primitives:**

| Primitive | Purpose |
|---|---|
| `grid_reflow_move` | Shallow transfer of source row to target. Mark source dead. |
| `grid_reflow_split` | Chunk wide row into newCols-width rows. Call `grid_reflow_join` on last chunk if it has room and source was wrapped. |
| `grid_reflow_join` | Pull cells from wrapped successor rows into target row. Mark fully consumed successors dead. Partially consumed rows: shift remaining cells to index 0, reduce cellused, leave alive. |

**Dead-row tombstones:**
- `grid_reflow_dead()` — `memset(gl, 0, sizeof *gl); gl->flags = GRID_LINE_DEAD`
- Outer loop: `if (gl->flags & GRID_LINE_DEAD) continue`
- Eliminates all consumed-counter bookkeeping. No `cellOffset`. No `i += 1 + consumed`.

**Outer loop dispatch (grid_reflow, grid.c:1431):**

| Condition | Path |
|---|---|
| `width == sx` | `grid_reflow_move` |
| `width > sx` | `grid_reflow_split` (may call `grid_reflow_join` on tail) |
| `width < sx` AND `WRAPPED` | `grid_reflow_join` (standalone) |
| `width < sx` AND NOT `WRAPPED` | `grid_reflow_move` |

**Split calls join:** At the end of `grid_reflow_split`, if last chunk has room (`width < sx`) AND original was wrapped: `grid_reflow_join(target, gd, sx, yy, width, already=1)`. The `already=1` parameter tells join to reuse the last existing target row instead of adding a new one.

**Join is the only consumer of successor rows.** One code path for pulling wrapped successors — used by both standalone join (outer loop) and split tail join. No inline duplication.

**Cursor tracking:**
- Before reflow: `grid_wrap_position()` converts physical `(cx, cy)` to logical `(wx, wy)` — paragraph index + character offset within paragraph.
- After reflow: `grid_unwrap_position()` converts back.
- `wx = UINT_MAX` sentinel when cursor is at/past end of used content on its row.

**Height resize before reflow (screen_resize_cursor, screen.c:315):**
1. `gd->sx = sx` + `screen_reset_tabs()` — width applied first
2. `screen_resize_y(sy)` — height change: prune empty rows below cursor (shrink), pull scrolled history back (grow), adjust hsize/hscrolled, reset scroll region
3. `screen_reflow(sx)` — reflow at already-updated dimensions
4. Restore cursor from absolute coordinates

**hscrolled adjustment during reflow:**
- Per-split: `if (yy <= gd->hscrolled) gd->hscrolled += lines - 1`
- Per-join: `if (gd->hscrolled > to + lines) gd->hscrolled -= lines; else if (gd->hscrolled > to) gd->hscrolled = to`
- Post-loop: `gd->hsize = target->sy - gd->sy; clamp hscrolled to hsize`

**Alternate screen:** NOT reflowed. `screen_resize` passes `reflow = (saved_grid == NULL)` — alternate screen gets height resize only.

### END's Current Implementation (Divergences)

| Aspect | tmux | END (current) |
|---|---|---|
| Dead rows | `GRID_LINE_DEAD` tombstone, outer loop skips | `consumed` counter + `cellOffset` — broken |
| Split tail | Calls `grid_reflow_join(already=1)` | Inline continuation loop (lines 221–257) — duplicates join logic |
| Dispatch | 3 cases + move fallback | 4 cases including `used <= 0 and isWrapped` skip |
| Height resize | Separate step before reflow | None — height and width handled simultaneously in `grid.reflow()` |
| scrollOffset | `hscrolled` adjusted per-split and per-join | Not adjusted during reflow |
| Alternate screen | Not reflowed | Both screens reflowed |
| Empty row pruning | `eat_empty` deletes below cursor before pushing to history | None |

### Other Terminals (Confirming tmux's Model)

All major terminals follow tmux's fundamental approach:

| Terminal | Split/Join | Dead Rows | Cursor Tracking |
|---|---|---|---|
| tmux | 3 primitives, split-calls-join | `GRID_LINE_DEAD` tombstone | `wrap_position`/`unwrap_position` (logical coords) |
| Alacritty | Separate grow/shrink passes | Dropped empty rows from output Vec | `cursor_buffer_line` + `cursor_line_delta` accumulation |
| Ghostty | `ReflowCursor` write-head, `reflowRow` per row | Old pages freed after last row processed | Tracked Pin — updated inline per-cell |
| Kitty | `fast_copy`/`multiline_copy`, fresh buffers | Source not mutated (read-only iteration) | `TrackCursor[]` list, updated per-copy-chunk |
| Wezterm | Join phase then split phase (two-phase) | `drain()` consumes source deque | Inline during join phase |

Key insight: no terminal uses consumed-counter + cellOffset bookkeeping. All use either tombstones, fresh-buffer streaming, or drain/rebuild.

### GridResize Timing — Confirmed Aligned

`GridResize::apply()` call sequence is architecturally sound:

```
1. video.setCellSize()               ← pixel dims
2. cursor snapshot                   ← video.getCursorRow/Col()
3. [NEW: height resize step]         ← prune/pull rows, adjust numRows
4. grid.reflow()                     ← the algorithm (width change only now)
5. grid.setNumRows() + state.setNumRows()
6. video.setDimensions()
7. video.loadScreenState()
8. video.resize()                    ← clamp cursor, reset scroll region, tab stops
9. tty->platformResize()             ← SIGWINCH
```

The coalesce timer (50ms), discrete state switch, and SmoothStateTransition analogy are correct. Only the algorithm at step 4 (and addition of step 3) changes.

## Principles and Rationale

### Why Verbatim Translation

tmux's reflow is battle-tested across decades. The algorithm is correct. END's storage model (ring buffer, Row with wrapped flag, usedCols) was already modeled after tmux's `grid_line`. The algorithm should match.

No reimagination. No new algorithm. tmux's three primitives, translated into END's infrastructure with BLESSED compliance.

### BLESSED Pillar Mapping

- **Bound** — Dead-row flag is a value on Row::flags, not a separate tracking structure. Lifecycle is clear: set once during reflow, cleared when reflow completes (scratch buffer is discarded).
- **Lean** — Three primitives (move, split, join). No inline duplication. Split calls join — one code path for successor consumption.
- **Explicit** — Dead flag is explicit (`Row::dead`). No implicit consumed-counter arithmetic. Each source row is either alive or dead — binary, visible.
- **SSOT** — Row::flags is the single source of truth for row state during reflow. No parallel `consumed` counter that must agree with flag state.
- **Stateless** — Reflow helpers are static functions. No persistent state between calls. Scratch buffer is transient.
- **Encapsulation** — Grid::reflow() is self-contained. GridResize calls it. The algorithm doesn't reach into Video, State, or Processor. scrollOffset adjustment uses State's existing `storeValue()` API.
- **Deterministic** — Same input grid + same dimensions = same output grid. Dead-row tombstones eliminate the consumed/cellOffset state machine that was the source of non-determinism.

## Scaffold

### 1. Row::flags — Add Dead Bit

```cpp
// jam_row.h
struct Row
{
    using FlexType = Cell;
    uint16_t usedCols { 0 };
    uint8_t  flags    { 0 };

    static constexpr uint8_t wrapped { 1 << 0 };  // existing
    static constexpr uint8_t dead    { 1 << 1 };  // new — reflow tombstone

    Cell cells[];
};
```

### 2. Static Helpers — Faithful tmux Translation

**reflowDead** — mark source row as consumed:
```cpp
static void reflowDead (jam::Buffer<jam::Row>& buf, int screen, int physicalRow) noexcept
{
    jam::Row* row { buf.getWritePointer (screen, physicalRow) };
    buf.clear (screen, physicalRow);
    row->flags = jam::Row::dead;
}
```

**reflowMove** — transfer source row to target, mark source dead:
```cpp
static void reflowMove (jam::Buffer<jam::Row>& oldBuf,
                        jam::Buffer<jam::Row>& scratch,
                        int screen,
                        int srcPhysical,
                        int destPhysical) noexcept
{
    scratch.copyFrom (screen, destPhysical, oldBuf, screen, srcPhysical);
    reflowDead (oldBuf, screen, srcPhysical);
}
```

**reflowJoin** — pull wrapped successors into target row. `already` parameter: if true, reuse last target row (called from split tail); if false, move source row first.

Translates `grid_reflow_join` (grid.c:1255) verbatim:
- Walk successor rows starting at `yy + 1 + lines`
- Copy cells one-by-one (respecting wide char boundaries) until target full or non-wrapped boundary hit
- Partially consumed rows: shift remaining cells to index 0, reduce usedCols, leave alive
- Fully consumed rows: mark dead
- Adjust scrollOffset per tmux's hscrolled logic

**reflowSplit** — chunk wide row into newCols-width pieces, call reflowJoin on tail.

Translates `grid_reflow_split` (grid.c:1363) verbatim:
- Compute lines needed: `1 + (usedCols - 1) / newCols` (simple path; wide-char path counts by walking)
- Copy chunks into target rows, set wrapped flag on all except possibly last
- Truncate source row to first chunk, set wrapped, move to target, mark source dead
- If last chunk has room AND source was wrapped: call `reflowJoin(already=true)` on last target row
- Adjust scrollOffset: `+= lines - 1` if source was at/above scroll position

### 3. reflowScreen — Outer Loop

Translates `grid_reflow` (grid.c:1431) outer loop:

```
for each source row (yy = 0 .. totalRows):
    if (row->flags & Row::dead) continue          ← skip consumed rows

    width = row->usedCols                          ← content width

    if (width == newCols)     → reflowMove
    if (width > newCols)      → reflowSplit (may call reflowJoin on tail)
    if (width < newCols AND wrapped) → reflowJoin (standalone, already=false)
    else                      → reflowMove         ← short unwrapped line
```

Post-loop: pad target to at least newViewportRows. Compute `outNumRows = min(writeIdx - newViewportRows, scrollbackLines)`. Clamp scrollOffset.

### 4. Height Resize Step in GridResize::apply()

New method `Grid::resizeHeight()` called before `reflow()`:

**Shrink (newRows < oldRows):**
1. Count empty rows below cursor from bottom of viewport
2. Delete them (up to `needed = oldRows - newRows`)
3. Remaining: push top viewport rows into scrollback (`numRows += remaining`)

**Grow (newRows > oldRows):**
1. Pull rows from scrolled history back into viewport (up to `needed = newRows - oldRows`, capped by scrollOffset)
2. Adjust numRows and scrollOffset
3. Fill remaining new rows with blanks

### 5. Skip Alternate Screen Reflow

```cpp
// In Grid::reflow() or GridResize::apply():
// Screen 0 (normal): full reflow
// Screen 1 (alternate): height resize only, no reflow
```

Alternate screen content is ephemeral — the application redraws on SIGWINCH. Reflowing it is wasted work and can produce artifacts.

### 6. scrollOffset Adjustment

END's `scrollOffset` on State = tmux's `hscrolled`. Adjusted inline during reflow:

- Per-split: `if (yy <= scrollOffset) scrollOffset += lines - 1`
- Per-join: `if (scrollOffset > to + lines) scrollOffset -= lines; else if (scrollOffset > to) scrollOffset = to`
- Post-loop: `scrollOffset = min(scrollOffset, outNumRows)`

After reflow, written back via `state.storeValue()` through the existing atomic path.

### 7. Cursor Wrap/Unwrap — Edge Case Verification

Current `wrapCursorPosition` / `unwrapCursorPosition` follow tmux's model. One missing edge case:

tmux uses `wx = UINT_MAX` when cursor is at/past end of used content (`px >= linedata[py].cellused`). This sentinel means "end of logical line" — `unwrapCursorPosition` walks to the last physical row of the logical line and sets `wx = cellused`.

END must add this sentinel handling. Currently `wrapCursorPosition` always computes `wx = paragraphCol + cursorCol` without checking if cursor is past usedCols. Use `-1` as sentinel (since END uses signed int, not unsigned).

### 8. Data Flow (Complete Resize Sequence)

```
GridResize::apply()
  1. video.setCellSize()                     ← pixel dims if pending
  2. cursorRow/Col = video.getCursor()        ← snapshot
  3. grid.resizeHeight (newRows, cursor)      ← NEW: prune/pull, adjust numRows
     state.setNumRows()                       ← sync State
  4. grid.reflow (newCols, scrollbackLines,   ← reflow screen 0 only
                  cursorRow, cursorCol,
                  scrollOffset)
     grid.setNumRows (0, reflowed)
     state.setNumRows (0, reflowed)
     state.storeValue (scrollOffset)          ← adjusted during reflow
  5. video.setDimensions (cols, rows)
  6. video.loadScreenState (cursor)
  7. video.resize (cols, rows)                ← clamp, scroll region, tabs
  8. tty->platformResize()                    ← SIGWINCH
```

## BLESSED Compliance Checklist

- [x] Bounds — Dead flag on Row::flags, clear lifecycle, scratch buffer is transient
- [x] Lean — Three primitives, no duplication, split delegates to join
- [x] Explicit — Dead flag visible, no implicit counter arithmetic, sentinel for cursor-past-end
- [x] SSOT — Row::flags is sole truth for row state during reflow, scrollOffset adjusted in one place
- [x] Stateless — Static helpers, no persistent state, scratch buffer transient
- [x] Encapsulation — Grid::reflow() self-contained, uses existing Buffer/State APIs
- [x] Deterministic — Same input = same output, no state machine ambiguity

## Open Questions

None. Algorithm is tmux verbatim. Infrastructure is END's existing jam::Buffer, Row::flags, State atomics.

## Handoff Notes

- The reflow algorithm is a **verbatim translation** of tmux's `grid_reflow` (grid.c:1431), `grid_reflow_move` (grid.c:1243), `grid_reflow_split` (grid.c:1363), `grid_reflow_join` (grid.c:1255), and `grid_reflow_dead` (grid.c:1221). COUNSELOR/Engineer should read these tmux functions as the specification.
- `GridResize::apply()` call sequence stays. Only additions: height-resize step before reflow, alternate screen skip.
- `Row::dead` bit is transient — only exists during reflow on the source buffer. After reflow completes, the source buffer is replaced by scratch. No persistent dead rows.
- Wide character handling at split boundaries: tmux inserts spacer cells when a wide char would be split at the column boundary. END's `Cell` already has `WIDE`/`SPACER_TAIL`/`SPACER_HEAD` — same mechanism, use it.
- The `wrapPending` edge case (Video sets `wrapPending = true` but Row::wrapped is only committed on next character) should be resolved before reflow: if `wrapPending` is true at resize time, commit the wrapped flag on the current row. `GridResize::apply()` already passes `wrapPending = false` to `loadScreenState` after reflow — the pre-reflow commit ensures the flag is on the row where the algorithm can see it.
- ARCHITECTURE.md describes VBlank as the render trigger — stale doc, actual trigger is timer-driven flush (60/120Hz). Update ARCHITECTURE.md as part of this work.
