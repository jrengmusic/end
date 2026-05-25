# PLAN: TextEditor Reflow

**RFC:** RFC-reflow.md
**Date:** 2026-05-25 (revised)
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

`jam::DiscreteStateTransition<Row>` (DST) is the resize coordinator -- owned by
Session, not Display. DST manages the scratch buffer, coalescing timer, and the
start/stop trigger lifecycle. No smoothResizer, no onStop callback.

---

## Ownership Model

```
Session (owns Screen, Processor, resizer)
  |-- Screen (jam::TextEditor) -- sole author of dims, owns Buffer, always exists
  |     |-- jam::Buffer<Row>   -- cell storage (2 channels: NORMAL=0, ALTERNATE=1)
  |     +-- Block<Row>         -- shared view into Buffer (atomic activeBlocks pointer)
  |-- Processor                   -- owns Video, Parser, State, TTY
  |     +-- Video                 -- writes cells through Block, dumb worker
  +-- jam::DiscreteStateTransition<Row> resizer  -- scratch buffer, timer, start/stop lifecycle

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

**Session owns the resizer.** `jam::DiscreteStateTransition<Row>` lives on Session.
It manages the scratch buffer internally, the 16ms coalescing timer, and fires
"start" and "stop" triggers. Session registers both triggers in its constructor.
Display does NOT own or wire the resizer. DST does NOT access Screen internals.
DST creates Block views from its scratch buffer and calls `screen.swapActiveBlocks(blockData)`.

**DST scratch vs NORMAL/ALTERNATE:** Screen's `buffers[2]` (2 channels) is for
DECSC/DECRC terminal screen switching -- NOT resize ping-pong. DST scratch is a
separate Buffer owned by DST, used only during resize. They are unrelated.

**Session doesn't listen to State for resize.** Pure callback chain via DST.
No Session vTPC listener for resize. No resizeStart/resizeEnd State params needed
for resize coordination -- DST owns the lifecycle internally.

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

## Resize Lifecycle (DST Pattern)

```
Display::resized()
  → state.setValue(id::viewport, packedBounds)          // logical pixel

Screen vTPC (viewport changed):
  → compute new cols/rows from pixel dims
  → if (cols/rows changed):
       resizer.set(jam::ID::start, newCols, newRows)    // fires "start" trigger + starts timer

DST "start" trigger (fires synchronously):
  → processor.suspendProcessing(true)                  // callbackLock held, reader blocked
  → read WriteHead from State (head.position)
  → copy rows from buffers[activeIndex] to DST scratch at head

DST timer fires (16ms coalescing window closed):
  → isTransitioning = false, timer stops
  → fire "stop" trigger (directly via triggers.get, no new timer)

DST "stop" trigger:
  → swap: activeBlocks.store(scratch.blocks[activeIndex].data())
  → screen.setText(block)                              // TextEditor sees new content
  → processor.suspendProcessing(false)                  // reader resumes
  → processor.setWinsize(newCols, newRows)            // Video fires SIGWINCH
```

**Coalescing:** If `resizer.set(jam::ID::start, ...)` is called again during the
timer window, latest cols/rows replace pending. Timer restarts. On timer fire,
drain pending and fire "stop" with final values. Exactly one resize per cycle.

**SIGWINCH fires once per resize cycle** -- after buffer is valid and reader
has resumed. Buffer is ready before shell is notified.

---

## Thread Model (TETRIS / AudioProcessor contract)

```
Audio analogy          Terminal equivalent
---------------------  -----------------------------------------
Host                   Nexus -- owns Sessions, UI owns Displays
FFT bin buffer         jam::Buffer<Row> -- Screen-owned storage
spectrum data view     shared jam::Block -- view into Screen's Buffer
audio thread           reader thread (TTY -> Parser -> Video)
message thread         message thread (State flush -> Screen -> paint)
prepareToPlay          suspendProcessing -> buffer swap -> resume
callbackLock           callbackLock (same CriticalSection)
AudioProcessor         Session/Processor -- processing engine
FFT visualizer         Screen -- owns buffer, dictates resolution
SmoothChain/DST        jam::DiscreteStateTransition -- resize coordinator
```

**Invariant:** Video accesses storage through Block indirection. Neither thread
blocks on the hot path. Resize (cold path) briefly suspends Video -- bounded by
one process() call duration (~50us). Reader thread is blocked only during the
copy/swap window inside the "start" trigger.

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

3. Session creates resizer (jam::DiscreteStateTransition<Row>(buffers[0]))
     -> resizer.addTrigger(jam::ID::start, [this] (int c, int r) { ... suspend + copy ... })
     -> resizer.addTrigger(jam::ID::stop,  [this] (int c, int r) { ... swap + resume + SIGWINCH ... })

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
| Screen | Dimension computation (sole author), Buffer allocation, rendering | Buffer, Block, activeBlocks | writeHead, scrollOffset, cursor*, viewport | viewport (via onCellChanged) |
| Display | UI shell | -- | viewport, cellWidth, cellHeight, fontSize | -- |
| Video | Cell writing, VT command execution | -- (writes through Block pointer) | -- | writeHead, cursor*, modes, screen dirty (every flush) |
| Processor | Event routing, TTY ownership, suspendProcessing, setWinsize | Video, Parser, State, TTY | -- | -- |
| DST (resizer) | Scratch buffer, coalescing timer, start/stop trigger lifecycle | scratch buffer | -- (reads head from State via Session) | -- (Session writes resizeStart/resizeEnd if needed for external observers) |

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
- `id::screenSwitch` declared in Identifier.h -- has NO handler (cleanup needed)
- TETRIS lifecycle: `suspendProcessing` / `callbackLock` on Processor
- Session owns Screen (value member), Processor receives `screen.getActiveBlocksRef()`
- Display takes `Session&`, no smoothResizer, no Screen member
- Screen owns `buffers[2]` (NORMAL+ALTERNATE channels) + `activeBlocks` atomic pointer
- Screen sole author of cell dims via `onCellChanged` lambda + packed viewport param
- Full-content Block (history + viewport) passed to TextEditor -- native Viewport scroll
- Video flushes cursor values every tick

**END HEAD missing (Step 1 remaining):**
- `id::screenSwitch` declared but unused -- remove from Identifier.h
- Video::flush() does NOT write `id::writeHead` every tick -- add it
- `id::writeHead` event NOT registered in Processor::registerEvents() -- add handler

---

## Steps

### Step 1: WriteHead -- packed State Parameter  IN PROGRESS

**jam:** Done. `WriteHead` struct + `pack()`/`unpack()` at jam HEAD.

**END:** Remaining:
1. Remove `id::screenSwitch` from `Identifier.h` -- declared but no handler, dead code
2. Add `id::writeHead` flush in `Video::flush()` -- fires for both screens every tick:
   ```cpp
   for (int s = 0; s < 2; ++s)
       events.get (id::writeHead, s, writePosition[s].pack());
   ```
3. Register `id::writeHead` event handler in `Processor::registerEvents()`:
   ```cpp
   events.add<int, int> (id::writeHead,
       [this] (int screen, int packedWH) {
           const juce::Identifier screenId { Map::Screen::getContext()->get (screen) };
           state.storeValue (screenId, id::writeHead, packedWH);
       });
   ```
   This stores `writeHead` into State. Video fires it every tick. Processor stores it.
   State is SSOT for cross-thread values.

---

### Step 2: Block mutable access  done

**Problem:** Block is currently a read-only snapshot (`const getRowPointer`).
For the new ownership model, Video needs mutable write access through Block.

**Solution:** Extend Block with dumb mutation operations:
- `getWritePointer(row)` -- mutable row access (same ring mapping as `getRowPointer`)
- `getWritePointer(row, headPosition)` -- caller-supplied head for Video's writePosition
- `clear(row)` -- zero-fill a row
- `clear(row, headPosition)` -- caller-supplied head
- `copyRow(destRow, srcRow)` -- row-to-row copy within the Block
- `copyRow(destRow, srcRow, headPosition)` -- caller-supplied head
- `getHead()` -- return stored head position
- `static_assert(std::is_trivially_copyable_v<Block<T>>)` preserved

Block does NOT manage head. Block receives head at construction time and uses
it for ring mapping. Video tracks its own `writePosition` and passes it via the
head-override overloads. Head state lives in State (`WriteHead` Parameter).

---

### Step 3: DST Resizer + Ownership Restructure  IN PROGRESS

**jam remaining:**
- Add `jam::ID::start` / `jam::ID::stop` to `jam_identifier_misc.h` (IDENTIFIER_MISCELLANEOUS macro)
- Adapt `jam::DiscreteStateTransition` for terminal resize (see below)

**Problem:** Buffer resize happens inline in `onCellChanged` with no content copy,
no suspend, no SIGWINCH coordination. History lost when viewport grows. smoothResizer
was on Display which is the wrong layer. Session needs to own the resize coordinator.

**Solution:** `jam::DiscreteStateTransition<Row>` is the resize coordinator, owned
by Session. It holds the scratch buffer internally. "start" trigger fires suspend +
copy. Timer coalesces. "stop" trigger fires swap + resume + SIGWINCH.

**DST adaptation (jam):**
- Holds `scratch` buffer internally (not `previous` snapshot)
- `set(jam::ID::start, cols, rows)`: sizes scratch based on `cols` and `rows`, fires "start" trigger synchronously, starts 16ms coalescing timer
- Timer callback: `isTransitioning = false`, `stopTimer()`, fires "stop" trigger via `triggers.get(jam::ID::stop, ...)`
- `getTargetValue()` returns pending bounds from last `set()` call
- `captureSnapshot()` removed (not used)
- Crossfade mechanics removed (not used)
- Coalescing: `set()` during transition replaces pending bounds, restarts timer
- Fixed 16ms timer. No `setCrossfadeTimeMs()`.

**jam_identifier changes:**
- `jam::ID::start` / `jam::ID::stop` in `jam_identifier_misc.h` -- pre-defined identifiers
  for the resize lifecycle

**Session changes:**
```cpp
// Session.h
jam::DiscreteStateTransition<jam::Row> resizer;

// Session.cpp constructor (after screen + processor created):
resizer = jam::DiscreteStateTransition<jam::Row>(screen.getActiveBuffer());

resizer.addTrigger<int, int>(jam::ID::start,
    [this] (int newCols, int newRows) {
        processor->suspendProcessing(true);
        const jam::WriteHead wh { jam::WriteHead::unpack (
            state.loadValue (Map::Screen::getContext()->get(screen.getActiveScreen()), id::writeHead)) };
        const int head { wh.position };
        const int rowsToCopy { jmin(buffers[activeIndex].getNumRows(), scratch.getNumRows()) };
        for (int r = 0; r < rowsToCopy; ++r) {
            scratch.copyRow(r, (head + r) % buffers[activeIndex].getNumRows());
        }
    });

resizer.addTrigger<int, int>(jam::ID::stop,
    [this] (int /*newCols*/, int /*newRows*/) {
        const jam::Bounds target { resizer.getTargetValue() };
        const int ch { screen.getActiveScreen() };
        jam::Block<jam::Row> scratchBlock { resizer.scratch, ch };
        screen.setText (scratchBlock);  // TextEditor renders from scratch
        processor->suspendProcessing (false);
        if (target.width > 0 and target.height > 0)
            processor->setWinsize (cell (target.width), cell (target.height));
    });

screen.onDimensionsChanged = [this] (cell cols, cell rows) {
    resizer.set (jam::ID::start, cols.value, rows.value);
};
```

**Screen changes:**
- `onCellChanged` fires `onDimensionsChanged(cols, rows)` callback when dimensions change
- No inline buffer resize. Screen is a dumb renderer.
- `onDimensionsChanged` callback: owner (Session) sets this to call `resizer.set(jam::ID::start, cols, rows)`
- Screen exposes `getActiveBuffer()` for DST construction
- Screen exposes `setText(Block<Row>)` -- already exists, used by Session's "stop" trigger

**DST swap mechanics:**
- DST creates Block view from scratch: `jam::Block<Row>(scratch, channel)`
- Session's "stop" trigger calls `screen.setText(scratchBlock)` -- uses existing setText API
- Screen does NOT know about DST. No new swap method needed.

**Video changes:**
- `Video::flush()` writes `id::writeHead` (WriteHead pack) every tick, not just on scroll

**Display changes:**
- `Display::resized()` writes `id::viewport` to State only. No resize orchestration.
- No smoothResizer member. No resize wiring.

**Processor changes:**
- No resize listener. `setWinsize()` is called by Session's "stop" trigger callback.

**Constraint:** `callbackLock` acquisition during resize is bounded by one
`process()` duration. PTY pipe buffer (64KB typical) absorbs bytes during
suspend -- no overflow at any practical baud rate for sub-millisecond suspend.

---

### Step 4: Integrate wrapping into TextEditor pipeline

**Problem:** TextEditor wrapping infrastructure is built but disabled (`wrapColumns`
forced to 0 in `calc()`). ParagraphsModel builds but JustifiedText never enters
the render chain.

**Solution:** Enable the pipeline:
```
Block<Row> -> ParagraphsModel -> Arrangement::shape(options) -> JustifiedText -> drawGlyphs
```

**Scope:**
- `jam_text_editor.cpp` `calc()` -- set `wrapColumns` from viewport width
  (`Cell::Rectangle(font.bounds, visibleBounds).getWidth()`) instead of 0
- `jam_text_editor.cpp` `calc()` -- after `arrangement.shape()`, construct
  `JustifiedText(arrangement, shapedTextOptions)` and call `applyTo(arrangement)`
- `jam_glyph_arrangement.h` `shape()` -- accept `ShapedTextOptions` (currently takes
  raw `wrapColumns`). Pass through to FLEX_GAP-aware wrap logic.
- Content height in `calc()` computed from post-wrap line count
  (JustifiedText::getHeight()), not raw numRows.

**Constraint:** `Arrangement::shape()` signature change is jam API -- affects both
END and future Whelmed consumers. ShapedTextOptions is the single config carrier.

---

### Step 5: Scrollbar viewport width accounting

**Problem:** When vertical scrollbar appears (content > viewport height),
`getMaximumVisibleWidth()` shrinks. But `onResized` only fires from
`TextEditor::resized()`, not from scrollbar appearance. Terminal cols stay stale --
content renders wider than visible area, rightmost cols hidden behind scrollbar.

**Solution:** Detect scrollbar-induced width change in `calc()`. When
`getVisibleWidth()` differs from the width used to compute current viewport cols,
fire `onResized` to trigger `onCellChanged` -> DST resize flow.

**Scope:**
- `jam_text_editor.cpp` `calc()` -- compare current `getVisibleWidth()` against
  last-known width. If changed, call `onResized` (deferred via
  `MessageManager::callAsync` to avoid re-entrancy during `calc()`).
- Track `lastVisibleWidth` member in TextEditor.

**Constraint:** Must not cause oscillation. Guard: if scrollbar appearance changes
width by less than one cell width, do not fire (sub-cell change is cosmetic only).

---

### Step 6: Buffer writeback + reflow (deferred)

**Problem:** After resize, buffer content needs to be preserved across the size
change. Simple row copy is Step 3 (immediate fix). Full reflow with word wrap
is Step 4.

**Current (Step 3):** Copy rows from old buffer to scratch at current head,
then swap. Excess rows beyond new scrollback limit are discarded (natural ring
overflow). Basic preservation works.

**Full reflow (Step 4 integration):** When Steps 4-5 are complete, the copy
from old buffer to scratch uses `Arrangement` + `JustifiedText` to reflow at the
new width. Same pipeline as TextEditor's render-time wrap. Cursor position
re-derived from logical address after reflow.

---

## Sequencing

```
Step 1 -> WriteHead packed Parameter (jam done, END in progress)
Step 2 -> Block mutable access                          done
Step 3 -> DST resizer + ownership restructure            ← NEXT SPRINT
Step 4 -> enable wrapping pipeline
Step 5 -> scrollbar width accounting
Step 6 -> buffer writeback + reflow (integrated with Step 4)
```

Steps 1-2 complete.
Step 3 is the next sprint.
Steps 4-5 can proceed independently of Step 3.
Step 6 depends on Steps 3 + 4.

---

## Constraints

- `Cell::FLEX_GAP` is the sole gap identity mechanism
- `Row::justify` flag is the sole justification signal per row
- `Row::flexWrap` is the sole logical line boundary marker
- `ParagraphStorage` is the sole logical line boundary tracker
- `Value::map` for all proportional distribution
- `Cell::Rectangle` / `Cell::Point` for all pixel-cell conversions
- `Bounds::pack()` / `unpack()` pattern for all packed parameters (16+16 bits)
- `<JuceHeader.h>` is the only include in project source files
- No anonymous namespaces. Static linkage for file-scope helpers
- No early returns. Positive checks, jassert at preconditions
- Reflow is a rendering concern -- lives in TextEditor/Arrangement/JustifiedText
- Screen is sole author of terminal winsize (cols/rows in cell units)
- Screen owns Buffer storage -- sole allocator, sole resizer
- Screen always exists (owned by Session) -- Display parents it for rendering
- Session owns `jam::DiscreteStateTransition<Row>` resizer -- coalescing, start/stop triggers
- DST holds scratch buffer internally. "start" fires suspend+copy. Timer. "stop" fires swap+resume+SIGWINCH.
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
- Video is a dumb worker -- writes where told, reports state to State, no storage mgmt
- Daemon mode: same structure, Display writes dims to State before destruction, daemon reads persisted State
- DST uses `jam::ID::start` / `jam::ID::stop` identifiers for the resize lifecycle
- Session does not listen to State for resize -- pure callback chain via DST

---

## Risks

- **calc() chicken-and-egg**: ContentView height depends on post-wrap line count,
  but shape runs with clip rect derived from ContentView height. May need to shape
  in calc() with full content (current approach), then clip in paint.
- **Performance**: shaping full content in calc() may be expensive for large scrollback.
  Current clip-aware shaping only shapes visible rows. May need pre-pass that counts
  wrapped lines without full shaping.
- **Scrollbar oscillation**: scrollbar appearance changes cols, which changes content
  layout, which might remove the need for scrollbar. Guard: sub-cell-width changes
  are cosmetic only, do not trigger resize.
- **Atomic builtins portability**: clang and MSVC have different intrinsics. Thin
  wrapper header (`jam_atomic_ops.h`) must be tested on both platforms.
- **Reflow fidelity**: buffer writeback reflow must produce identical layout to
  TextEditor's render-time wrap. Both must use the same pipeline
  (Arrangement + JustifiedText + ShapedTextOptions) -- divergence means visual
  inconsistency between resize and steady-state rendering.
- **DST scratch buffer lifecycle**: DST scratch buffer must be allocated before
  first resize. Session creates DST with `screen.getActiveBuffer()` as the source.
  Scratch is sized on first "start" trigger from the pending cols/rows.