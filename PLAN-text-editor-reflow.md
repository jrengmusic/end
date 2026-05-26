# PLAN: TextEditor Reflow

**RFC:** RFC-reflow.md, RFC-sentinel-cell.md
**Date:** 2026-05-26 (revised)
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

---

## Overview

Reflow moves from Screen into jam::TextEditor's rendering pipeline. Word wrap +
justification handled by `jam::JustifiedText` (forked from `juce::JustifiedText`,
adapted to cell-based layout). Logical lines managed by `ParagraphStorage`. Screen
is sole author of terminal dimensions in cell units AND owns Buffer storage.
Video writes through a shared `jam::Block` view. All state flows through
`terminal::State` as SSOT -- reader thread stores via atomics, message thread
reads from ValueTree after flush. Hot path lock-free on both threads. Resize is
cold path under `suspendProcessing`.

`jam::DiscreteStateTransition<Cell>` (DST) is the resize coordinator -- owned by
Session, not Display. DST manages the scratch buffer, coalescing timer, and the
start/stop trigger lifecycle. No smoothResizer, no onStop callback.

Row FAM is eliminated. `Buffer<Cell>` with sentinel cell at `cells[numCols]` per
row replaces `Buffer<Row>`. Row metadata (usedCols, flexWrap, justify, collapsed)
packed into Cell's 23 padding bits on the sentinel. Content cells (0..numCols-1)
stay pure -- padding bits always 0. Flat storage is trivially copyable.

Reflow is a buffer-to-buffer transform inside the DST lifecycle. Destructive but
lossless -- all content preserved. FLEX_GAP cells stripped and restamped at new
width. `wrapColumns` stays 0 in the renderer -- buffer rows are always correctly
stamped at the current terminal width.

---

## Ownership Model

```
Session (owns Screen, Processor, resizer)
  |-- Screen (jam::TextEditor) -- sole author of dims, owns Buffer, always exists
  |     |-- jam::Buffer<Cell>  -- cell storage (2 channels: NORMAL=0, ALTERNATE=1)
  |     |                         stride = (cols + 1) * sizeof(Cell), sentinel at cells[numCols]
  |     +-- Block<Cell>        -- shared view into Buffer (atomic activeBlocks pointer)
  |-- Processor                   -- owns Video, Parser, State, TTY
  |     +-- Video                 -- writes cells through Block, dumb worker
  +-- jam::DiscreteStateTransition<Cell> resizer  -- scratch buffer, timer, start/stop lifecycle

Display (UI shell -- parents Screen for rendering via addAndMakeVisible)
  -- owns no resize infrastructure. Display::resized() writes viewport to State only.
```

**Screen always exists.** Screen is owned by Session, not Display. Screen owns
Buffer regardless of whether it is visible. Display parents Screen for rendering
via `addAndMakeVisible` when the UI appears. Display can attach/detach Screen
without affecting Buffer or Video -- juce::Component separates hierarchy from
ownership.

**Screen owns Buffer.** Screen is sole author of dimensions -- it computes
cols/rows from `getVisibleWidth()`/`getVisibleHeight()`, which account for
scrollbar visibility. Even a single col change from scrollbar makes Screen
and Video disagree if Buffer is owned elsewhere. Screen is the FFT visualizer
analog -- it owns the rendering buffer because it dictates the resolution.
Screen always has dimensions -- Display writes bounds to State at creation
and on resize. For daemon mode, dimensions come from persisted State.

**Session owns the resizer.** `jam::DiscreteStateTransition<Cell>` lives on Session.
It manages the scratch buffer internally, the 16ms coalescing timer, and fires
"start" and "stop" triggers. Session registers both triggers in its constructor.
Display does NOT own or wire the resizer. DST does NOT access Screen internals.

**DST scratch vs NORMAL/ALTERNATE:** Screen's `buffers[2]` (2 channels) is for
DECSC/DECRC terminal screen switching -- NOT resize ping-pong. DST scratch is a
separate Buffer owned by DST, used only during resize. They are unrelated.

**Session doesn't listen to State for resize.** Pure callback chain via DST.
No Session vTPC listener for resize. No resizeStart/resizeEnd State params needed
for resize coordination -- DST owns the lifecycle internally.

---

## Sentinel Cell Architecture

Row FAM (`jam::Row`) is eliminated. `Buffer<Cell>` flat storage with sentinel
cell at position `cells[numCols]` per row.

**Bit layout** (Cell padding bits 41-63, sentinel cell only):

```
bit 41      flexWrap    -- content continues on next row
bit 42      justify     -- row contains FLEX_GAP cells
bit 43      collapsed   -- reflow tombstone
bits 44-55  usedCols    -- rightmost non-blank column + 1 (12 bits, max 4096)
bits 56-63  spare       -- reserved, 0
```

**Content cells** (`cells[0..numCols-1]`): padding bits always 0. Pure display atoms.

**Sentinel cell** (`cells[numCols]`): carries row metadata. Not rendered.
Within the stride allocation -- `Buffer<Cell>` allocates `(numCols + 1)` cells
per row (aligned). `getNumCols()` returns logical terminal width (numCols).

**Accessor API** -- static functions on Cell:

```cpp
Cell::isFlexWrap(sentinel)      Cell::setFlexWrap(sentinel, bool)
Cell::isJustify(sentinel)       Cell::setJustify(sentinel, bool)
Cell::isCollapsed(sentinel)     Cell::setCollapsed(sentinel, bool)
Cell::getUsedCols(sentinel)     Cell::setUsedCols(sentinel, int)
```

**Video write pattern** (representative):

```cpp
jam::Cell* row { blocks[scr].getWritePointer (writeRow, writePosition[scr]) };
row[writeCol] = cell;
jam::Cell::setUsedCols (row[numCols], jmax (jam::Cell::getUsedCols (row[numCols]), writeCol + charWidth));
```

**Row clear pattern**: `row[numCols] = jam::Cell {};` -- zeroes all 64 bits,
resetting usedCols=0 and all flags=0.

---

## State as SSOT

All terminal state flows through `terminal::State`:

- **Reader thread (Video):** writes via `state.storeValue()` -- lock-free atomic store
- **Message thread (Screen, Display):** reads from ValueTree after `flush()` timer tick
- **No shadow state.** Objects do not cache, track, or duplicate State values.
  Block does not manage head. Block is a dumb view -- it receives head at
  construction time from State and maps rows. Video writes `WriteHead` to State
  on every `flush()`, not just on scroll.

`WriteHead` (position + historyRows packed into single int) is written by Video
on every flush. Read by Screen when constructing the Block view. Both values
arrive on the same flush tick -- always consistent.

---

## Resize Lifecycle (DST Pattern -- Revised)

```
Display::resized()
  -> state.setValue(id::viewport, packedBounds)          // logical pixel

Screen vTPC (viewport changed):
  -> compute new cols/rows from pixel dims
  -> if (cols/rows changed):
       resizer.set(jam::ID::start, newCols, newRows)    // stores pending dims, starts timer

DST coalescing (16ms window):
  -> additional resize events update pending dims, restart timer
  -> Video keeps running freely during coalescing

DST timer fires (no new events for 16ms):
  -> fire "stop" trigger:

DST "stop" trigger (all work here, synchronous):
  -> processor.suspendProcessing(true)                  // callbackLock held, reader blocked
  -> read WriteHead from State (head.position)

  -> REFLOW:
     1. First walk (exact row count):
        - Per paragraph, walk cells respecting atom boundaries (WIDE pair, FLEX_GAP run)
        - Strip FLEX_GAP cells from count (not content)
        - Compute exact output rows needed at new width
     2. Allocate scratch at (newCols, exactRows)
     3. Second walk (reflow + restamp):
        - Write content cells into scratch rows at new width
        - Restamp FLEX_GAP at new width
        - Stamp sentinel: flexWrap, justify, usedCols per output row
     4. Write to scratch buffer

  -> swap: activeBlocks.store(scratch blocks)
  -> screen.setText(block)                              // TextEditor sees new content
  -> processor.suspendProcessing(false)                  // reader resumes
  -> processor.setWinsize(newCols, newRows)            // Video fires SIGWINCH
```

**Key change from previous PLAN:** "start" trigger is lightweight (stores dims,
starts timer). All heavy work (suspend, reflow, swap, resume, SIGWINCH) happens
in "stop" trigger after coalescing settles. Video is never suspended during the
coalescing window -- only for the synchronous reflow duration (microseconds).

**Coalescing:** If `resizer.set(jam::ID::start, ...)` is called again during the
timer window, latest cols/rows replace pending. Timer restarts. On timer fire,
stop trigger executes with final values. Exactly one resize per cycle.

**SIGWINCH fires once per resize cycle** -- after buffer is valid and reader
has resumed. Buffer is ready before shell is notified.

**Content always preserved.** No paragraph drop, no silent truncation.
Scrollback config is logical maximum -- reflow may temporarily exceed physical
row count (downsizing adds rows). Buffer allocation is deterministic from the
first walk's exact count.

---

## Thread Model (TETRIS / AudioProcessor contract)

```
Audio analogy          Terminal equivalent
---------------------  -----------------------------------------
Host                   Nexus -- owns Sessions, UI owns Displays
FFT bin buffer         jam::Buffer<Cell> -- Screen-owned storage
spectrum data view     shared jam::Block<Cell> -- view into Screen's Buffer
audio thread           reader thread (TTY -> Parser -> Video)
message thread         message thread (State flush -> Screen -> paint)
prepareToPlay          suspendProcessing -> buffer swap -> resume
callbackLock           callbackLock (same CriticalSection)
AudioProcessor         Session/Processor -- processing engine
FFT visualizer         Screen -- owns buffer, dictates resolution
SmoothChain/DST        jam::DiscreteStateTransition -- resize coordinator
trivially copyable T   Cell (8 bytes, static_assert enforced)
```

**Invariant:** Video accesses storage through Block indirection. Neither thread
blocks on the hot path. Resize (cold path) briefly suspends Video -- bounded by
reflow duration (~microseconds for 10k rows). Reader thread is blocked only
during the reflow+swap window inside the "stop" trigger.

---

## Construction Order

```
1. Nexus creates Session
     -> Session creates Screen (always, not just when visible)
     -> Screen allocates buffers[0] (2 channels, initial size from config)
     -> Screen constructs blockSets[0]
     -> Screen sets activeBlocks

2. Session creates Processor (receives screen.getActiveBlocksRef())
     -> Processor creates Video (receives blocks pointer)
     -> Processor creates Parser

3. Session creates resizer (jam::DiscreteStateTransition<Cell>(buffers[0]))
     -> resizer.addTrigger(jam::ID::start, [this] (int c, int r) { ... store pending dims ... })
     -> resizer.addTrigger(jam::ID::stop,  [this] (int c, int r) { ... suspend + reflow + swap + resume + SIGWINCH ... })

4. UI creates Display (when terminal pane appears)
     -> Display calls addAndMakeVisible(session.getScreen())
     -> Display wires AppState listener for font changes
     -> Display::resized() writes viewport to State

5. Session::start() (called after Display is attached, or immediately for daemon)
     -> TTY opens, reader thread begins
     -> Video writes through Block
     -> Video flushes writeHead to State on every flush() tick
```

No deferred init. No nullptr Block. No "set later" setters. Video's dependency
(storage to write to) exists before Video exists -- Screen creates Buffer before
Processor/Video are constructed. Resizer is created after Screen and Processor
are fully constructed.

---

## Object Responsibilities (BLESSED: Stateless, Encapsulation)

All objects are dumb workers. They are told, not asked. State is SSOT.

| Object | Responsibility | Owns | Reads from State | Writes to State |
|--------|----------------|------|------------------|-----------------|
| Session | Terminal instance lifecycle, resize orchestration | Screen, Processor, resizer | -- | -- |
| Screen | Dimension computation (sole author), Buffer allocation, rendering | Buffer<Cell>, Block<Cell>, activeBlocks | writeHead, scrollOffset, cursor*, viewport | viewport (via onCellChanged) |
| Display | UI shell | -- | viewport, cellWidth, cellHeight, fontSize | -- |
| Video | Cell writing, VT command execution, sentinel stamping | -- (writes through Block pointer) | -- | writeHead, cursor*, modes, screen dirty (every flush) |
| Processor | Event routing, TTY ownership, suspendProcessing, setWinsize | Video, Parser, State, TTY | -- | -- |
| DST (resizer) | Scratch buffer, coalescing timer, start/stop trigger lifecycle | scratch buffer | -- (reads head from State via Session) | -- |
| Reflow | Pure function: old Block + newCols -> scratch content | -- | -- | -- |

---

## What is already done

**jam HEAD (commit 1644941):**
- `WriteHead` struct with `pack()`/`unpack()` (jam_write_head.h)
- `jam::AtomicOps` cross-platform lock-free builtins (jam_atomic_ops.h)
- `Block` mutable access: `getWritePointer()`, `clear()`, `copyRow()` with head override overloads
- `Buffer::copyFrom()` cross-buffer row copy
- `Bounds::pack()`/`unpack()` for single-write viewport parameter (16+16 bits)
- `<SCREEN>` section in Parameters.xml -- per-screen params declared in schema
- `State::buildLayout` creates NORMAL/ALTERNATE nodes from `<SCREEN>` XML
- `Row::justify` flag (bit 2) -- defined + stamped by Video on FLEX_GAP
- `glyph::Arrangement` FLEX_GAP-aware word wrap
- `jam::JustifiedText` with `Value::map` distribution
- `TextEditor` holds `paragraphsModel`, `shapedTextOptions`

**END HEAD:**
- `setWinsize` rename across TTY/Processor/Session/Display
- `id::writeHead` declared in Identifier.h (packed position+historyRows)
- `id::writeHead` stored internally in Processor for scrollUp (lines 242, 328, 333)
- TETRIS lifecycle: `suspendProcessing` / `callbackLock` on Processor
- Session owns Screen (value member), Processor receives `screen.getActiveBlocksRef()`
- Display takes `Session&`, no smoothResizer, no Screen member
- Screen owns `buffers[2]` (NORMAL+ALTERNATE channels) + `activeBlocks` atomic pointer
- Screen sole author of cell dims via `onCellChanged` lambda + packed viewport param
- Full-content Block (history + viewport) passed to TextEditor -- native Viewport scroll
- Video flushes cursor values every tick
- Video flushes writeHead every tick
- DST resizer wired on Session, coalescing 16ms, lossless ring copy via resizeBuffers
- Callback elimination complete (15 std::function callbacks replaced with APVTS patterns)
- TTY owned by Processor via startTTY()
- DA2 parsing fix (CSI > c intermediate byte)

---

## Steps

### Step 1: WriteHead -- packed State Parameter  DONE

**jam:** Done. `WriteHead` struct + `pack()`/`unpack()` at jam HEAD.

**END:** Done. `id::writeHead` flush in Video::flush() every tick.
`id::writeHead` event registered in Processor::registerEvents().
`id::screenSwitch` removed from Identifier.h.

---

### Step 2: Block mutable access  DONE

Done. Block extended with `getWritePointer()`, `clear()`, `copyRow()` with
head-override overloads. `static_assert(std::is_trivially_copyable_v<Block<T>>)`
preserved.

---

### Step 3: DST Resizer + Ownership Restructure  DONE

Done. Session owns DST, coalescing 16ms timer, lossless content copy via
ring-order per-channel resizeBuffers. Callback elimination complete.

---

### Step 4: Sentinel Cell Architecture (jam)

**RFC:** RFC-sentinel-cell.md

**Problem:** Row is FAM -- not trivially copyable, forces runtime stride
computation, blocks memcpy for reflow, propagates dual-path rendering pipeline
(`Block<Row>` vs `Block<Cell>`) through every layer.

**Solution:** Eliminate Row. Flat `Buffer<Cell>` with sentinel cell at
`cells[numCols]` carrying row metadata in Cell's 23 padding bits.

**jam scope:**

1. **jam_cell.h** -- Add sentinel bit constants + static accessor functions
   (isFlexWrap, setFlexWrap, isJustify, setJustify, isCollapsed, setCollapsed,
   getUsedCols, setUsedCols). Delete `Cell::RowState` struct (dead code).

2. **jam_buffer.h** -- Delete `has_flex_type` trait and `hasFlexType` variable
   template (lines 21-29). Delete FAM stride branch (lines 95-100). Single stride
   path: `(numCols + 1)` cells per row, aligned. `getNumCols()` returns logical
   width (numCols), not numCols+1.

3. **jam_row.h** -- Delete file entirely.

4. **jam_glyph_arrangement.h/.cpp** -- Delete `shape(Block<Row>)` overloads
   (2 header declarations + 2 implementations). Modify `shape(Block<Cell>)` lambda
   to read `usedCols` from sentinel at `row[numCols]`.

5. **jam_ParagraphStorage.h** -- `build()` takes `Block<Cell>`, reads
   `Cell::isFlexWrap(row[numCols])` instead of `Row::flexWrap`.

6. **jam_text_editor.h/.cpp** -- Delete `setText(Block<Row>)`, `rowContent`
   member, `hasRowContent` flag. Single `setText(Block<Cell>)` path. `calc()`
   single-path dispatch.

**Validation:** @Auditor checks: Cell static_asserts preserved (sizeof==8,
trivially_copyable), no Row references remain in jam, Buffer stride allocates
sentinel, shape() reads usedCols from sentinel, BLESSED compliance (Lean: no
dead code, Explicit: named constants, SSOT: sentinel IS metadata).

---

### Step 5: Sentinel Cell Architecture (END)

**Depends on:** Step 4

**Problem:** END references `Buffer<Row>`, `Block<Row>`, `Row::` across 13 files
(76+ access sites in Video subsystem alone).

**Solution:** Mechanical type change `Row` -> `Cell` + sentinel access pattern.

**END scope:**

1. **Video.h** -- `Block<Row>*` -> `Block<Cell>*`,
   `std::atomic<Block<Row>*>&` -> `std::atomic<Block<Cell>*>&`

2. **Video.cpp** -- All `row->cells[col]` -> `row[col]`. All `row->usedCols` ->
   `Cell::getUsedCols(row[numCols])` / `Cell::setUsedCols(row[numCols], ...)`.
   All `row->flags |= Row::flexWrap` -> `Cell::setFlexWrap(row[numCols], true)`.
   All `row->flags |= Row::justify` -> `Cell::setJustify(row[numCols], true)`.
   All `row->usedCols = 0; row->flags = 0;` -> `row[numCols] = jam::Cell {};`.

3. **VideoEdit.cpp** -- Same pattern as Video.cpp (~11 sites).

4. **VideoCSI.cpp** -- Same pattern (scroll fills).

5. **Screen.h/.cpp** -- `Buffer<Row>` -> `Buffer<Cell>`, `Block<Row>` -> `Block<Cell>`,
   `std::atomic<Block<Row>*>` -> `std::atomic<Block<Cell>*>`.
   `resizeBuffers()` mechanical type change.

6. **Session.h/.cpp** -- `DST<Row>` -> `DST<Cell>`.

7. **Processor.h** -- `std::atomic<Block<Row>*>&` -> `std::atomic<Block<Cell>*>&`.

8. **Input.cpp** -- Any Row references in cursor/selection logic.

**Validation:** @Auditor checks: no `Row`, `Block<Row>`, `Buffer<Row>` references
remain in END. Video sentinel access pattern correct (row[numCols] for metadata,
row[col] for content). memset safety (partial erases stay within 0..numCols-1,
sentinel untouched). BLESSED compliance.

---

### Step 6: DST Lifecycle Revision

**Depends on:** Step 5

**Problem:** Current DST wiring has "start" trigger doing suspend+copy and "stop"
doing swap+resume+SIGWINCH with timer between them. Video is suspended for the
entire coalescing window. New design: "start" is lightweight (coalesce only),
"stop" does all heavy work.

**Solution:** Revise Session's DST trigger wiring:

- **"start" trigger**: no suspend, no copy. Just stores pending dims. Timer starts/restarts.
- **"stop" trigger** (timer fires after 16ms settled): suspend Video, reflow
  (placeholder: row copy for now, full reflow in Step 7), swap activeBlocks,
  resume Video, SIGWINCH.

**Scope:**
- `Session.cpp wireResizer()` -- revise trigger lambdas per new lifecycle
- `Screen.cpp resizeBuffers()` -- may simplify (no longer called from "start")

**Validation:** @Auditor checks: Video suspended only in "stop" trigger, not
during coalescing. SIGWINCH fires after swap+resume. Content preserved across
resize. Timer coalescing verified (rapid resize events produce single cycle).

---

### Step 7: Buffer Reflow

**RFC:** RFC-reflow.md (with sentinel cell corrections)

**Depends on:** Steps 5 + 6

**Problem:** After resize, buffer content needs to be reflowed at the new column
width. FLEX_GAP cells are width-derived artifacts that must be stripped and
restamped. Paragraph boundaries must be preserved. Content must be 100% intact.

**Solution:** `jam::Reflow` -- pure function, two-pass buffer-to-buffer transform.

**Pass 1 -- exact row count (per paragraph, atom-aware):**
- Walk cells per paragraph via `ParagraphsModel`
- Content cells (codepoint, grapheme, WIDE) contribute to area
- FLEX_GAP cells stripped (not content -- regenerated at new width)
- Atom boundary rule: WIDE pair (2 cells) and FLEX_GAP run are atomic units
- If atom straddles wrap boundary, bump to next row (vacated cells = erase)
- Return exact physical row count needed at new dims

**Pass 2 -- reflow write:**
- Write content cells into scratch rows at new width
- Restamp FLEX_GAP at new width (regenerate elastic whitespace)
- Stamp sentinel per output row: flexWrap, justify, usedCols
- Paragraph boundaries preserved: last row of each paragraph gets flexWrap=0

**API:**
```cpp
struct Reflow
{
    static int computePhysicalRows (const jam::Block<jam::Cell>& oldBlock,
                                    const jam::ParagraphsModel& paragraphs,
                                    int newCols) noexcept;

    static void write (const jam::Block<jam::Cell>& oldBlock,
                       const jam::ParagraphsModel& paragraphs,
                       jam::Buffer<jam::Cell>& scratch,
                       int newCols) noexcept;
};
```

**Integration with DST "stop" trigger:**
```
suspend Video
-> paragraphs.build(oldBlock)
-> exactRows = Reflow::computePhysicalRows(oldBlock, paragraphs, newCols)
-> scratch.setSize(2, exactRows, newCols)
-> Reflow::write(oldBlock, paragraphs, scratch, newCols)
-> swap activeBlocks to scratch
-> resume Video
-> SIGWINCH
```

**Scope:**
- New file: `jam_reflow.h` (in jam_graphics or jam_core)
- `Session.cpp wireResizer()` -- replace row copy with Reflow calls in "stop" trigger

**Validation:** @Auditor checks: content 100% preserved (no paragraph drop, no
cell loss). FLEX_GAP stripped from old, restamped in new. Atom boundary rule
enforced (WIDE pairs and FLEX_GAP runs never split). Sentinel correctly stamped.
Pass 1 count == actual rows written in Pass 2 (deterministic). BLESSED: Stateless
(pure function), Deterministic (same input = same output), Bound (old Block
read-only, scratch write-only).

---

### Step 8: JustifiedText in Render Pipeline

**Depends on:** Step 5 (sentinel cell, single Block<Cell> path)

**Problem:** `JustifiedText` is built but never enters the render chain. `calc()`
shapes arrangement but doesn't apply gap distribution. Content height uses raw
row count, not post-justify metrics.

**Solution:** Wire JustifiedText into calc() after shape():

```cpp
void TextEditor::calc() noexcept
{
    // ... existing shape call with wrapColumns = 0 ...
    arrangement.shape (content, font, 0, 0);

    // Apply gap distribution for rows with justify flag
    JustifiedText jt { arrangement, shapedTextOptions };
    jt.applyTo (arrangement);

    // Content height from arrangement (reflects any future wrap changes)
    const int numLines { arrangement.getNumLines() };
    // ... rest of calc uses numLines for content height ...
}
```

**Note:** `wrapColumns` stays 0 for the terminal path. Reflow stamps correct
rows in the buffer. JustifiedText distributes FLEX_GAP space on justified rows.
No display-time wrapping.

**Scope:**
- `jam_text_editor.cpp` calc() -- construct JustifiedText, call applyTo,
  use arrangement.getNumLines() for content height

**Validation:** @Auditor checks: wrapColumns == 0 for terminal path. JustifiedText
constructed after shape(). applyTo called before content height computation.
Content height uses post-justify line count.

---

### Step 9: Scrollbar Viewport Width Accounting

**Problem:** When vertical scrollbar appears (content > viewport height),
`getMaximumVisibleWidth()` shrinks. But `onResized` only fires from
`TextEditor::resized()`, not from scrollbar appearance. Terminal cols stay stale --
content renders wider than visible area, rightmost cols hidden behind scrollbar.

**Solution:** Detect scrollbar-induced width change in `calc()`. When
`getVisibleWidth()` differs from the width used to compute current viewport cols,
fire `onResized` to trigger DST resize flow.

**Scope:**
- `jam_text_editor.cpp` `calc()` -- compare current `getVisibleWidth()` against
  last-known width. If changed by >= one cell width, call `onResized` (deferred
  via `MessageManager::callAsync` to avoid re-entrancy during `calc()`).
- Track `lastVisibleWidth` member in TextEditor.

**Constraint:** Must not cause oscillation. Guard: if scrollbar appearance changes
width by less than one cell width, do not fire (sub-cell change is cosmetic only).

**Validation:** @Auditor checks: oscillation guard present. Deferred callAsync
(not synchronous re-entry). lastVisibleWidth tracked and compared. BLESSED:
Explicit (guard condition named), Deterministic (sub-cell threshold).

---

## Sequencing

```
Step 1  -> WriteHead packed Parameter                    DONE
Step 2  -> Block mutable access                          DONE
Step 3  -> DST resizer + ownership restructure           DONE
Step 4  -> Sentinel cell architecture (jam)              <- NEXT
Step 5  -> Sentinel cell architecture (END)              depends on 4
Step 6  -> DST lifecycle revision                        depends on 5
Step 7  -> Buffer reflow                                 depends on 5, 6
Step 8  -> JustifiedText in render pipeline              depends on 5
Step 9  -> Scrollbar width accounting                    after 8
```

Steps 4-5 are the foundation (Row elimination).
Step 6 revises DST timing (coalesce-only start, heavy stop).
Step 7 is the reflow transform.
Step 8 wires justify distribution (independent of reflow).
Step 9 handles scrollbar edge case.

---

## Constraints

- `Cell::FLEX_GAP` is the sole gap identity mechanism
- Sentinel cell at `cells[numCols]` is sole carrier of per-row metadata
- Content cells (0..numCols-1) have padding bits always 0
- `Cell::isFlexWrap` / `Cell::isJustify` / `Cell::getUsedCols` are the sole metadata accessors
- `ParagraphStorage` is the sole logical line boundary tracker
- `Value::map` for all proportional distribution
- `Cell::Rectangle` / `Cell::Point` for all pixel-cell conversions
- `Bounds::pack()` / `unpack()` pattern for all packed parameters (16+16 bits)
- `<JuceHeader.h>` is the only include in project source files
- No anonymous namespaces. Static linkage for file-scope helpers
- No early returns. Positive checks, jassert at preconditions
- Reflow is a buffer-to-buffer transform -- not a rendering concern
- `wrapColumns` stays 0 for terminal path -- buffer rows are correctly stamped
- Screen is sole author of terminal winsize (cols/rows in cell units)
- Screen owns Buffer<Cell> storage -- sole allocator, sole resizer
- Screen always exists (owned by Session) -- Display parents it for rendering
- Session owns `jam::DiscreteStateTransition<Cell>` resizer
- DST "start" is lightweight (coalesce). "stop" does all heavy work.
- Video suspended only during "stop" trigger, never during coalescing
- Block is a dumb view -- receives head at construction, maps rows, no state mgmt
- State is SSOT -- all cross-thread values flow through State Parameters
- WriteHead (position + historyRows) is the sole head/history transport
- Video flushes writeHead to State on every flush(), not just on scroll
- Reader thread writes to State via atomics. Message thread reads from ValueTree.
- No shadow state. No object caches or duplicates State values.
- Hot path lock-free on both threads. Cold path (resize) under suspendProcessing
- 8-byte Cell writes naturally atomic on x86-64/ARM64
- C++17. Cross-platform atomic builtins in `jam_atomic_ops.h`
- Session owns Screen + Processor + resizer. Display is optional UI attachment.
- No deferred init. No nullptr Block. No "set later" setters.
- Processor is a dumb executor -- suspends/resumes on command, sends SIGWINCH on command
- Video is a dumb worker -- writes where told, stamps sentinel, reports state to State
- Daemon mode: same structure, Display writes dims to State before destruction
- DST uses `jam::ID::start` / `jam::ID::stop` identifiers for the resize lifecycle
- Session does not listen to State for resize -- pure callback chain via DST
- Reflow is lossless -- all content preserved, no paragraph drop, no silent truncation
- FLEX_GAP cells are width-derived artifacts -- stripped during reflow, restamped at new width
- Reflow is a pure function: same old Block + same newCols = same output (Deterministic)

---

## Risks

- **Sentinel cell discovery:** Code that iterates `cells[0..numCols]` (inclusive)
  would read the sentinel as content. All iteration must stop at `numCols-1` or
  use `usedCols` from sentinel. Pathfinder found 76+ access sites in Video --
  all use explicit col indices, none iterate to numCols. Low risk but audit needed.
- **Buffer +1 universal cost:** Every Buffer<T> allocates one extra element per
  row. 8 bytes/row for Cell. 10k rows = 80KB. Negligible.
- **Reflow atom boundary edge cases:** WIDE pair at exactly the wrap boundary,
  FLEX_GAP run spanning wrap boundary. Algorithm must handle both. Unit-testable.
- **calc() chicken-and-egg**: ContentView height depends on post-justify line count,
  but shape runs with clip rect derived from ContentView height. May need to shape
  in calc() with full content (current approach), then clip in paint.
- **Performance**: shaping full content in calc() may be expensive for large scrollback.
  Current clip-aware shaping only shapes visible rows. May need pre-pass that counts
  lines without full shaping.
- **Scrollbar oscillation**: scrollbar appearance changes cols, which changes content
  layout, which might remove the need for scrollbar. Guard: sub-cell-width changes
  are cosmetic only, do not trigger resize.
- **Atomic builtins portability**: clang and MSVC have different intrinsics. Thin
  wrapper header (`jam_atomic_ops.h`) must be tested on both platforms.
- **Reflow fidelity**: FLEX_GAP restamping must produce correct gap distribution
  at new width. JustifiedText consumes what reflow stamps -- they must agree on
  gap semantics.
