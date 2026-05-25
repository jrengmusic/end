# PLAN: TextEditor Reflow

**RFC:** RFC-reflow.md
**Date:** 2026-05-24 (revised)
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

### Ownership Model

```
Session (owns Screen, Processor)
  |-- Screen (jam::TextEditor) -- sole author of dims, owns Buffer, always exists
  |     |-- jam::Buffer<Row>   -- cell storage, allocated by Screen
  |     +-- Block<Row>         -- shared view into Buffer
  +-- Processor                -- owns Video, Parser, State, TTY
        +-- Video              -- writes cells through Block, dumb worker

Display (UI shell -- parents Screen for rendering via addAndMakeVisible)
  +-- smoothResizer (DST)      -- coalesces resize, drives visual transition
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

**Display owns smoothResizer.** Resize coalescing and visual transitions are UI
lifecycle concerns. Display orchestrates: tells Processor to suspend, Screen
resizes its Buffer, Processor resumes, SIGWINCH fires.

**Session owns Screen and Processor.** Session is the complete terminal instance
-- it always has a Screen (with Buffer) and a Processor (with Video). Display
is optional UI attachment. This model is uniform: standalone, daemon, and client
sessions all have the same structure. Display is the only variable.

### State as SSOT

All terminal state flows through `terminal::State`:

- **Reader thread (Video):** writes via `state.storeValue()` -- lock-free atomic store
- **Message thread (Screen, Display):** reads from ValueTree after `flush()` timer tick
- **No shadow state.** Objects do not cache, track, or duplicate State values.
  Block does not manage head. Block is a dumb view -- it receives head at
  construction time from State and maps rows. Video does not track head locally --
  it writes `WriteHead` to State after every scroll.

`WriteHead` (position + historyRows packed into single int) is the canonical
example: written atomically to State by Video, read from ValueTree by Screen.
Both values arrive on the same flush tick -- always consistent.

### Thread Model (TETRIS / AudioProcessor contract)

```
Audio analogy          Terminal equivalent
---------------------  -----------------------------------------
Host                   Nexus -- owns Sessions, UI owns Displays
FFT bin buffer         jam::Buffer<Row> -- Screen-owned storage
spectrum data view     shared jam::Block -- view into Screen's Buffer
audio thread           reader thread (TTY -> Parser -> Video)
message thread         message thread (State flush -> Screen -> paint)
prepareToPlay          suspendProcessing -> Screen resize -> resume
callbackLock           callbackLock (same CriticalSection)
AudioProcessor         Session/Processor -- processing engine
FFT visualizer         Screen -- owns buffer, dictates resolution
```

**Invariant:** Video accesses storage through Block indirection. Neither thread
blocks on the hot path. Resize (cold path) briefly suspends Video -- bounded by
one process() call duration (~50us).

### Construction Order

```
1. Nexus creates Session
     -> Session creates Screen (always, not just when visible)
     -> Screen allocates Buffer (2 channels, initial size from config)
     -> Screen constructs Block (shared view into Buffer)
     -> Session creates Processor (receives Block pointer from Screen)
     -> Processor creates Video (receives Block pointer)
     -> Processor creates Parser

2. UI creates Display (when terminal pane appears)
     -> Display calls addAndMakeVisible (session.getScreen())
     -> Display owns smoothResizer
     -> Display wires smoothResizer to Processor suspend/resume + Screen resize

3. Session::start() (called after Display is attached, or immediately for daemon)
     -> TTY opens, reader thread begins
     -> Video writes through Block
```

No deferred init. No nullptr Block. No "set later" setters. Video's dependency
(storage to write to) exists before Video exists -- Screen creates Buffer before
Processor/Video are constructed. Clean ownership, clean construction order.

### Object Responsibilities (BLESSED: Stateless, Encapsulation)

All objects are dumb workers. They are told, not asked. State is SSOT.

| Object | Responsibility | Owns | Reads from State | Writes to State |
|--------|----------------|------|------------------|-----------------|
| Session | Terminal instance lifecycle | Screen, Processor | -- | -- |
| Screen | Dimension computation (sole author), Buffer allocation, rendering | Buffer, Block | writeHead, scrollOffset, cursor*, viewport | viewport (via onCellChanged) |
| Display | UI shell, resize orchestration, smoothResizer | smoothResizer | viewport, cellWidth, cellHeight | -- |
| Video | Cell writing, VT command execution | -- (writes through Block pointer) | -- | writeHead, cursor*, modes, screen dirty |
| Processor | Event routing, TTY ownership, suspendProcessing | Video, Parser, State, TTY | viewport (for SIGWINCH) | -- (routes Video's events) |

### What is already done

- `setWinsize` rename across TTY/Processor/Session/Display
- `jam_fonts` merged into `jam_graphics/fonts/`
- `jam::ParagraphStorage` + `ParagraphsModel` (jam_graphics/detail/)
- `jam::ShapedTextOptions` with builder pattern (jam_graphics/detail/)
- `Row::justify` flag (bit 2) -- defined + stamped by Video on FLEX_GAP
- `glyph::Arrangement` FLEX_GAP-aware word wrap (jam_glyph_arrangement_shape.cpp)
- `jam::JustifiedText` with `Value::map` distribution (jam_graphics/detail/)
- `TextEditor` holds `paragraphsModel`, `shapedTextOptions` (jam_text_editor.h)
- Screen sole author of cell dims via `onCellChanged` lambda + packed viewport param
- TETRIS lifecycle: `suspendProcessing` / `callbackLock` on Processor
- `Bounds::pack()`/`unpack()` for single-write viewport parameter (16+16 bits)
- Full-content Block (history + viewport) passed to TextEditor -- native Viewport scroll
- `WriteHead` struct with `pack()`/`unpack()` (jam_core/jam_write_head.h)
- `<SCREEN>` section in Parameters.xml -- per-screen params declared in schema
- `State::buildLayout` creates NORMAL/ALTERNATE nodes from `<SCREEN>` XML
- `id::writeHead` replaces `id::numRows` -- packed position+historyRows
- `jam::AtomicOps` cross-platform lock-free builtins (jam_core/jam_atomic_ops.h)
- `Block` mutable access: `getWritePointer()`, `clear()`, `copyRow()`

---

## Steps

### Step 1: WriteHead -- packed State Parameter  done

**Problem:** Buffer head lives in `Buffer::headPositions[]` (immediate, racy),
`numRows` lives in State Parameter (flush-delayed). Screen reads both --
inconsistent pair causes wrong `liveStartRow` -> orphaned prompt, wrong viewport.

**Solution:** `WriteHead` struct -- packs `position` (16 bits) and `historyRows`
(16 bits) into a single `int`, same pattern as `Bounds::pack()`/`unpack()`.
Written atomically by Video in the same scroll operation. Both arrive at Screen
on the same flush tick -- always consistent.

```cpp
struct WriteHead
{
    int position;     ///< Ring write position (0..ringSize-1). Audio playhead analog.
    int historyRows;  ///< Scrollback depth -- rows above viewport.

    int pack() const noexcept;
    static WriteHead unpack (int v) noexcept;
};
```

---

### Step 2: Block mutable access  done

**Problem:** Block is currently a read-only snapshot (`const getRowPointer`).
For the new ownership model, Video needs mutable write access through Block.

**Solution:** Extend Block with dumb mutation operations:
- `getWritePointer(row)` -- mutable row access (same ring mapping as `getRowPointer`)
- `clear(row)` -- zero-fill a row
- `copyRow(destRow, srcRow)` -- row-to-row copy within the Block
- `static_assert(std::is_trivially_copyable_v<Block<T>>)` preserved

Block does NOT manage head. Block is a dumb view -- it receives head at
construction time and maps logical rows to physical rows. Head is managed by
State (WriteHead Parameter).

---

### Step 3: Ownership restructure

**Problem:** Buffer is Session-owned, shared between reader thread (Video writes)
and message thread (Screen reads) with no lock on cell access. Resize changes
stride/dimensions under Video's feet. Screen is created by Display, not Session --
it doesn't exist for daemon sessions. smoothResizer is on Processor but resize
coalescing is a UI concern.

**Solution:** Session owns Screen (always exists). Screen owns Buffer (sole author
of dimensions). Display parents Screen for rendering (attach/detach via
`addAndMakeVisible`/`removeChildComponent`). Display owns smoothResizer.
Processor receives Block pointer from Screen. Video writes through Block.

**Construction order:**
```
1. Session creates Screen
     -> Screen allocates Buffer (jam::Buffer<Row>)
     -> Screen constructs Block (shared view into Buffer)

2. Session creates Processor (receives Block* from Screen)
     -> Processor creates Video (receives Block*)
     -> Processor creates Parser

3. UI creates Display
     -> Display calls addAndMakeVisible (session.getScreen())
     -> Display owns smoothResizer
     -> Display wires resize lifecycle

4. Session::start()
     -> TTY opens, reader thread starts
     -> Video writes through Block
```

**Resize flow (cold path -- Display orchestrates, Screen executes):**
1. User drags window -> Display::resized() fires (message thread)
2. Screen::resized() -> onCellChanged computes new dims (sole author)
3. smoothResizer coalesces rapid resize events (owned by Display)
4. smoothResizer trigger fires:
   a. `processor.suspendProcessing(true)` -- acquires callbackLock, Video quiesced
   b. Screen resizes its Buffer at new dimensions
   c. Screen copies content from old layout with reflow/wrap -- single-threaded
   d. Screen rebuilds Block pointing to resized Buffer
   e. `processor.suspendProcessing(false)` -- Video resumes through updated Block
5. smoothResizer.onStop fires:
   a. Display tells Processor to send SIGWINCH
   b. `processor.setWinsize()` -- shell redraws at new dimensions

**Daemon mode (no Display at runtime):**
- Display determines Screen size at creation and writes bounds to State
- When Main is destroyed, State persists as XML on disk
- Daemon reads State from disk -- Screen has dimensions from the persisted State
- No smoothResizer at runtime (no resize events without UI)
- Single-threaded (message thread) -- no concurrency concern

**Hot path (unchanged):**
- Video: writes cells through Block, writes WriteHead to State -- reader thread
- Screen: reads WriteHead from State, constructs Block with flushed head, paints
- No locks. 8-byte Cell writes naturally atomic on x86-64/ARM64.

**Scope:**
- `Session.h/cpp` -- owns `Screen` (value or unique_ptr member). Creates Screen
  before Processor. Removes `jam::Buffer<jam::Row>` member. Provides
  `getScreen()` for Display attachment.
- `Screen.h/cpp` -- owns `jam::Buffer<jam::Row>`. Allocates in constructor
  (reads scrollbackLines from config). Constructs Block. Exposes `getBlock()`
  for Processor. No longer receives Buffer reference from outside.
- `Display.h/cpp` -- no longer owns Screen. Parents Screen via
  `addAndMakeVisible(session.getScreen())`. Owns smoothResizer. Constructor
  takes `Session&` instead of `Processor&` (accesses both Screen and Processor).
- `Processor.h/cpp` -- receives `jam::Block<jam::Row>*` instead of `Buffer&`.
  `prepare()` removed -- Screen handles allocation. Loses smoothResizer.
  Keeps `suspendProcessing()` / `callbackLock` / `setWinsize()` -- dumb executor.
  Passes Block pointer to Video.
- `Video.h/cpp` -- all `buffer.getWritePointer()` -> `block->getWritePointer()`.
  All `buffer.copyFrom()` -> `block->copyRow()`.
  All `buffer.clear()` -> `block->clear()`.
  `buffer.advanceHead()` removed -- Video writes WriteHead to State instead.
  Reference to Buffer replaced with reference to Block.
- `Panes.cpp` -- `createTerminal` updated: Session creation includes Screen.
  Display takes Session reference. `addAndMakeVisible(session.getScreen())`.
- `Nexus.h/cpp` -- `create()` methods no longer pass Buffer. Session creates
  its own Screen internally.

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
fire `onResized` to trigger `onCellChanged` -> viewport param update -> resize flow.

**Scope:**
- `jam_text_editor.cpp` `calc()` -- compare current `getVisibleWidth()` against
  last-known width. If changed, call `onResized` (deferred via
  `MessageManager::callAsync` to avoid re-entrancy during `calc()`).
- Track `lastVisibleWidth` member in TextEditor.

**Constraint:** Must not cause oscillation. Guard: if scrollbar appearance changes
width by less than one cell width, do not fire (sub-cell change is cosmetic only).

---

### Step 6: Buffer writeback + SIGWINCH coordination

**Problem:** After reflow/wrap produces a new layout at new dimensions, the live
buffer content must match what the shell expects. Shell gets SIGWINCH and redraws
at new dimensions -- buffer must be ready.

**Solution:** In the resize flow (Step 3, cold path), Screen copies content to
resized Buffer with reflow applied. The resized Buffer IS the live storage at new
dimensions. Video resumes writing at new dimensions. SIGWINCH fires after resume
(smoothResizer.onStop). Shell redraws into the correctly-sized buffer.

**Scope:**
- `Screen.cpp` resize handler -- copy content from old Block to resized Buffer
  using Arrangement + JustifiedText to reflow at new width
- `Display.cpp` `smoothResizer.onStop` -- tells Processor to `setWinsize()`,
  sends SIGWINCH. Processor is dumb executor.
- Cursor stability: physical cursor position re-derived from logical address after
  reflow. Logical address = flat cell offset within logical line (paragraph).
  ParagraphsModel provides the mapping.

**Constraint:** Reflow during copy must produce identical visual output to what
TextEditor's wrap pipeline renders. Same ShapedTextOptions, same Arrangement,
same JustifiedText -- SSOT pipeline used for both.

---

## Sequencing

```
Step 1 -> WriteHead packed Parameter                    done
Step 2 -> Block mutable access (cleanup done)           done
Step 3 -> ownership restructure (Screen owns Buffer, Session owns Screen)
Step 4 -> enable wrapping pipeline
Step 5 -> scrollbar width accounting
Step 6 -> buffer writeback + SIGWINCH
```

Steps 1-2 complete.
Step 3 depends on Step 2 (Block must be a dumb view before ownership moves).
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
- Display owns smoothResizer -- coalesces resize, drives visual transition
- Block is a dumb view -- receives head at construction, maps rows, no state mgmt
- State is SSOT -- all cross-thread values flow through State Parameters
- WriteHead (position + historyRows) is the sole head/history transport
- Reader thread writes to State via atomics. Message thread reads from ValueTree.
- No shadow state. No object caches or duplicates State values.
- Hot path lock-free on both threads. Cold path (resize) under suspendProcessing
- 8-byte Cell writes naturally atomic on x86-64/ARM64
- C++17. Cross-platform atomic builtins in `jam_atomic_ops.h`
- Session owns Screen + Processor. Display is optional UI attachment.
- No deferred init. No nullptr Block. No "set later" setters.
- Processor is a dumb executor -- suspends/resumes on command, sends SIGWINCH on command
- Video is a dumb worker -- writes where told, reports state to State, no storage mgmt
- Daemon mode: same structure, Display writes dims to State before destruction, daemon reads persisted State

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
