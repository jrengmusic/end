# PLAN: Viewport — Terminal Scrollback with tmux-Conformant Grid

**RFC:** none — objective from ARCHITECT prompt + tmux architecture research
**Date:** 2026-05-19
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation — no overrides)

## Overview

Terminal viewport with lossless scrollback, text wrapping, and reflow on resize. Adopts tmux's grid architecture into the END/JUCE framework. Grid owns a `jam::Buffer<jam::Row>` with pointer reordering for O(1) scroll. TextEditor is a universal dumb renderer — caller (Screen) feeds content via Block. Fully event-driven: no VBlank, no stray atomics, no shadow state. State is the only state machine (APVTS analog).

## Architecture Analog — Audio Plugin 1:1

| Audio Plugin | Terminal | Role |
|---|---|---|
| Host (DAW) | Session | Owns the pipeline. Calls prepareToPlay / processBlock. |
| AudioProcessor | Processor | Orchestrator. Owns State, references Grid. prepare() + process(). |
| AudioBuffer | Grid (jam::Buffer\<Row\>) | Dumb storage. Told everything. No state. |
| APVTS | State | SSOT. All state machine. Parameters. ValueTree. |
| AudioProcessorEditor | Display | UI host. ValueTree::Listener on State. No VBlank. |
| Component (renderer) | Screen : TextEditor | Rendering orchestrator. Reads State + Grid, feeds TextEditor. |

## tmux Model (reference, not copied verbatim)

```c
struct grid {
    u_int sx, sy;       // width, viewport height
    u_int hsize;        // history row count — boundary
    u_int hlimit;       // max history (config)
    u_int hscrolled;    // user scroll offset
    struct grid_line *linedata;  // flat array: [history...][viewport...]
};
// grid_view_y(gd, y) = hsize + y
// grid_scroll_history: hsize++, extend array, clear bottom
// grid_collect_history: batch evict oldest 10% when hsize >= hlimit
```

### Key divergence from tmux

tmux uses `xreallocarray` to grow the flat array on every scroll — O(1) data movement but O(N) allocation amortized via batch eviction.

We use `jam::Buffer<jam::Row>` with fixed allocation (power-of-two ring >= `scrollbackLines + viewportRows`) and **ring head index** per screen. `physicalRow = (head + offset) & ringMask`. scrollUp full-screen = `head++`, clear new bottom. O(1) — no allocation, no data movement, no memmove, no pointer reordering. Standard ring buffer math using jam::Buffer's `getWritePointer(channel, physicalRow)`.

When `numRows >= scrollbackLines`, eviction = oldest row overwritten naturally by ring wrap. No batch trim needed.

## Components

### Terminal::Grid — rewritten from scratch

Owns `jam::Buffer<jam::Row>` with 2 channels (normal=0, alternate=1). No State dependency. No atomic refs. Pure TETRIS — Processor tells Grid everything, Grid uses it as calculation inputs.

**Members (all calculation inputs, set by Processor):**

```
jam::Buffer<jam::Row>  buffer          — ring-sized (power of two >= scrollbackLines + viewportRows)
std::array<int, 2>     head            — ring head per screen (structural, like array index)
std::array<int, 2>     numRows         — history row count per screen. Calculation input, NOT state.
                                         Set by Processor. Used by getWritePointer to compute offset.
                                         Same pattern as DSP core storing sampleRate from prepareToPlay.
int                    ringMask        — power-of-two bitmask for ring indexing
int                    viewportRows    — viewport height (set by setSize)
int                    scrollbackLines — max history (set by setSize, from config)
```

`head[screen]` is structural — ring buffer index. `numRows[screen]` is a calculation input — Processor sets it, Grid uses it for physicalRow mapping. Grid never reads or writes State. Processor orchestrates all State interaction.

`physicalRow(screen, row)` = `(head[screen] + row) & ringMask` — maps any logical row (history or viewport) to physical ring position.

`getWritePointer(screen, viewportRow)`: reads `numRows` from State, computes logical row = `numRows + viewportRow`, maps to physical via ring head.

**API — Told (Processor/Video tell Grid what to do):**

```cpp
void setSize (int viewportRows, int numCols, int scrollbackLines) noexcept;
```
Called by Processor::prepare(). Computes ring size (power of two >= scrollbackLines + viewportRows). Allocates buffer. Resets head and numRows. Sets viewportRows, scrollbackLines.

```cpp
void setNumRows (int screen, int value) noexcept;
```
Processor tells Grid the current history count. Calculation input for physicalRow mapping. Called by Processor after scrollUp events. Same pattern as DSP `setSampleRate()`.

```cpp
int scrollUp (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;
```
Full-screen (scrollTop == 0, scrollBottom == viewportRows - 1): advance `head[screen]` by count. Clear new bottom viewport row(s) via `buffer.clear(screen, physicalRow(...))`. O(1). No data movement. No memcpy. No memmove. Standard ring buffer head advance. Grid does NOT update numRows — Processor does that after receiving the scrollUp event.

Partial (scroll region): `buffer.copyFrom()` row-by-row within region using physicalRow mapping. No numRows change. Same as tmux partial path.

Returns actual count scrolled.

```cpp
void scrollDown (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;
```
Full-screen: retreat `head[screen]`. Partial: copyFrom within region. No numRows change on scrollDown.

```cpp
void clear (int screen) noexcept;
void clear (int screen, int row) noexcept;
void clear (int screen, int row, int startCol, int numCols) noexcept;
```
Viewport-relative. Reads numRows atomic, computes logical row, maps to physical via ring head. Delegates to `buffer.clear()`.

**API — Asked (storage access only):**

```cpp
jam::Row* getWritePointer (int screen, int row) noexcept;
```
Viewport-relative. Physical = `(head[screen] + row) & ringMask` — head IS viewport top, no numRows offset needed. Returns `buffer.getWritePointer(screen, physicalRow)`. Video writes `row->cells[col]`.

```cpp
const jam::Row* getRow (int screen, int absoluteIndex) const noexcept;
```
Absolute index in logical space. Row 0 = oldest history. Physical = `(head[screen] + absoluteIndex) & ringMask`. For Screen scroll mode reads.

No getters. Processor manages numRows externally and tells Grid via `setNumRows()`. Screen reads State's ValueTree (post-flush).

**Ring scroll model:**

On full-screen scrollUp:
1. `head[screen] = (head[screen] + count) & ringMask` — advance ring head
2. Clear new bottom viewport row: `buffer.clear(screen, physicalRow(screen, numRows + viewportRows - 1))`
3. Grid does NOT update numRows. Returns count.
4. Video fires scrollUp event with count.
5. Processor handler: increments numRows (capped at scrollbackLines), writes to State, tells Grid via `setNumRows(screen, newValue)`.
6. The old viewport-top row now lives in history (ring didn't move data — only head advanced).

No getArrayOfReadPointers. No pointer reordering. No State refs. Clean jam::Buffer API: `getWritePointer(channel, physicalRow)`, `clear(channel, physicalRow)`, `copyFrom(...)`. Standard ring buffer math. Pure TETRIS: Processor sets, Grid uses.

### Processor — orchestrator (PluginProcessor analog)

**prepare()** — analogous to `prepareToPlay()`:
```cpp
void prepare (int viewportRows, int numCols, int scrollbackLines) noexcept;
```
Called by Session on construction and on resize (from Display dimension change event). Sets Grid size. Sets Video dimensions. NOT called from process(). No resize in process().

**process()** — analogous to `processBlock()`:
```cpp
void process (const char* data, int length) noexcept;
```
Guards: Grid must be prepared (viewportRows > 0, cols > 0). No resize detection. No dimension reads from State. Processor::prepare() handles dimensions separately.

**Event handlers (registered in constructor):**

- Video fires `ID::screenDirty` → Processor sets `screenDirty` Parameter on State.
- Video fires `ID::scrollUp` with `(screen, count)` → Processor increments its numRows tracking, writes to State, and tells Grid via `grid.setNumRows(screen, newValue)`. Unidirectional: Processor → Grid, Processor → State.
- Video fires `ID::activeScreen` → Processor sets State's `activeScreen`.
- Video fires cursor/mode events → Processor sets State Parameters.
- Display fires `ID::scrollOffset` → Processor sets State's `scrollOffset` Parameter.

**Session calls prepare():**

Session is the host. Session detects dimension changes (from Display's `terminalResize` event) and calls `processor->prepare(rows, cols, scrollbackLines)`. Then signals PTY resize (SIGWINCH). Processor never reads dimensions from State in process().

### State — APVTS analog, SSOT

All state lives here. No stray atomics. No shadow state. No manual booleans.

**Parameters (XML-driven, lock-free):**

SESSION group:
- `activeScreen` — int, 0=normal 1=alternate
- `cols`, `visibleRows` — dimensions
- existing: selection, hints, OSC 133, paste echo, sync output, shell exit, etc.

SCREEN group (per-screen: NORMAL, ALTERNATE):
- `cursorRow`, `cursorCol`, `cursorVisible`, `cursorShape`, `cursorColor`, `keyboardFlags` — existing
- `numRows` — history row count. Mirrors Grid's numRows. Processor writes after scroll events.
- `scrollOffset` — user scroll position. 0 = live, >0 = scrolled into history.
- `screenDirty` — monotonic int counter. Processor increments. Screen repaints on change.

MODES group: existing mode flags.

**Cross-thread contract:**
- READER thread: Video fires events → Processor handlers write Parameters (lock-free atomics).
- Timer (60Hz): State::flush() copies dirty Parameters to ValueTree properties.
- MESSAGE thread: Display/Screen read ValueTree via valueTreePropertyChanged. Never read atomics directly.

### Display — PluginEditor analog, no VBlank

`juce::ValueTree::Listener` on State's ValueTree. No VBlankAttachment. No grid reads. No onVBlank lambda.

**Constructor:** Takes `Processor&`. Gets `State&` from processor. Registers as VT listener. Creates Screen. Applies config.

**valueTreePropertyChanged:** Delegates to Screen for rendering. Syncs activeScreen, cursor position.

**resized():** Computes cell dimensions from font. Fires `terminalResize` event → Session calls `processor->prepare()`.

**applyConfig():** Sets font, cursor, scrollbar. No `setScrollbackLines` on TextEditor — Grid owns scroll capacity.

### Screen — rendering orchestrator, derived from TextEditor

Screen inherits `jam::TextEditor`. Takes `State&` and `Grid&` references. Registers as `juce::ValueTree::Listener` on State's ValueTree.

**valueTreePropertyChanged:**
1. Read `activeScreen`, cursor position from State.
2. Read `screenDirty` — if changed, repaint.
3. Call `shouldClear()` — clear if signaled.
4. Read `scrollOffset` from State.

**Live mode (scrollOffset == 0):**
- Read viewport rows from Grid: `grid.getWritePointer(screen, row)` → `Row*`
- Access `row->cells` → create `jam::Block<jam::Cell>` view
- Feed to TextEditor via `setText(block, range)` (existing API)
- Full viewport repaint — no per-row dirty tracking (tmux model: whole-pane redraw)

**Scroll mode (scrollOffset > 0):**
- Read history from Grid: `grid.getRow(screen, numRows - scrollOffset + row)` → `const Row*`
- Feed to TextEditor same way. Live viewport frozen while scrolled.

**TextEditor as dumb renderer:**

TextEditor's rendering pipeline already works via Block → `arrangement.shape()` → `drawGlyphRuns()`. ContentView reads from `screen.at(activeScreen)` at one seam (line 74 in jam_text_editor_content_view.cpp).

For this sprint: Screen feeds TextEditor via existing `setText(Block, Range)` API — copies block rows into TextEditor's internal buffer. This works and preserves TextEditor's universality for both Terminal and Whelmed.

Future sprint (TextEditor dumb renderer): eliminate the internal buffer copy. TextEditor receives Block reference directly. Single seam change in ContentView. Not in scope now — the copy is fast (compiler auto-vectorizes memcpy at -O3, benchmarked: 10M seq < 6s at 4K retina).

### Video — writes Grid, fires events

Video takes `Grid&` (not Buffer&). Writes viewport-relative via `grid.getWritePointer(screen, row)` → `Row*` → `row->cells[col]`.

**Changes from current HEAD:**

Current: `grid.getWritePointer(screen, row)` returns `jam::Cell*`, cell access via `ptr[col]`.
New: returns `jam::Row*`, cell access via `row->cells[col]`.

Current: `grid.getWritePointer(screen, row, col)` returns `jam::Cell*` at specific position.
New: removed. Always get Row*, then access `row->cells[col]`. Consistent with FAM model.

**Wrap flag:** `row->flags |= jam::Row::wrapped` at auto-wrap point in `resolveWrapPending()` and `print()`.

**Events fired (through Processor's events map):**

- `ID::screenDirty` — fired in `flush()` (renamed from `flushState()`). Signals cell data was written.
- `ID::scrollUp` with `(screen, count)` — fired in `scrollUpAndFill()` when scrollTop == 0 (full-screen scroll).
- Existing: `ID::activeScreen`, `ID::cursorRow`, `ID::cursorCol`, `ID::cursorVisible`, mode flags, etc.

### Scrollbar — State-driven (future sprint)

`juce::Component` + `juce::ValueTree::Listener` on State. Reads `numRows`, `visibleRows`, `scrollOffset` from State. Fires `ID::scrollOffset` event on user interaction → Processor → State. No Screen dependency. Not in scope for this sprint.

### Reflow — width change (future sprint)

Processor drives reflow on width change. Reads all rows from Grid (history + viewport). Joins wrapped rows (Row::wrapped flag). Splits at new width. Writes back. Updates numRows. Not in scope for this sprint.

## Data Flow

```
[READER thread]
Video writes cells to Grid (viewport-relative via numRows + ring head)
  Grid::scrollUp: head advance, clear bottom row. O(1). No numRows change — Grid doesn't manage it.
  Video::flush(): fires screenDirty + scrollUp events
  Processor handlers: increment numRows, write to State, tell Grid via setNumRows(). Set screenDirty.

[Timer — MESSAGE thread]
State flushes Parameters to ValueTree (60Hz)
  Screen::valueTreePropertyChanged fires
  Display::valueTreePropertyChanged fires

[MESSAGE thread — Screen]
Screen reads State: scrollOffset, numRows, visibleRows, cursor
Screen reads Grid: getWritePointer (live) or getRow (scroll) → Row* → cells
Screen creates Block from cells, feeds to TextEditor via setText
TextEditor renders from internal buffer. Standard pipeline.

[MESSAGE thread — Display]
Display reads State: activeScreen, cursor
Display tells Screen: setActiveScreen, setCaretPosition
No grid access. No VBlank. Pure event-driven.
```

## Data Ownership

| Component | Owns | Role |
|---|---|---|
| Session | Grid, TextBuffer, TTY, History, Processor | Host. Calls prepare() on resize. |
| Grid | jam::Buffer\<Row\> + head[2] + ringMask | Dumb storage with ring head. numRows is a calculation input set by Processor. No State dependency. Pure TETRIS. |
| Processor | State, Video, Parser, events map | Orchestrator. prepare(), process(), event handlers. Syncs Grid → State. |
| State | Parameters (VT-backed) | SSOT. All state. APVTS analog. |
| Video | Nothing | Writes Grid viewport-relative. Fires events. Calculation inputs only. |
| Display | Screen (value member) | UI host. VT listener. No VBlank. Delegates rendering to Screen. |
| Screen | Nothing | Reads State + Grid. Feeds TextEditor. Orchestrates rendering. |
| TextEditor | Internal Buffer\<Cell\> (screen[]) | Universal dumb renderer. Receives data via setText. |

## Steps

### Step 1: Rewrite Grid — jam::Buffer\<Row\>, pointer reorder scroll, numRows boundary

**Scope:** `Grid.h`, `Grid.cpp`
**Action:** Rewrite from scratch. `jam::Buffer<jam::Row>` storage. 2 channels. Ring-sized (power of two). `setSize(viewportRows, numCols, scrollbackLines)`. `setNumRows(screen, value)` — Processor tells Grid the history count. Ring head scroll — `head[screen]` advance, `physicalRow = (head + offset) & ringMask`. `getWritePointer` maps viewport-relative via numRows + ring head. `getRow` for absolute access. `clear` overloads. No manual memcpy/memmove — use jam::Buffer API exclusively. No State dependency. No atomic refs. Pure TETRIS.
**Validation:** Compiles. Grid API matches this PLAN. Pointer reorder scroll is O(1). No BLESSED violations.

### Step 2: Video — Row* access, wrap flag, events

**Scope:** `Video.h`, `Video.cpp`, `VideoEdit.cpp`, `VideoCSI.cpp`, `VideoESC.cpp`, `VideoOps.cpp`
**Action:** All `grid.getWritePointer(screen, row)` returns `jam::Row*`. Cell access via `row->cells[col]`. Remove `getWritePointer(screen, row, col)` calls — always get Row* first. Wrap flag: `row->flags |= jam::Row::wrapped`. Rename `flushState()` → `flush()`. Fire `ID::screenDirty` in flush(). Fire `ID::scrollUp` in `scrollUpAndFill()` when scrollTop == 0.
**Validation:** All cell writes correct. Events fire. No `jam::Cell*` from Grid — always `jam::Row*`.

### Step 3: Processor — prepare(), event handlers, Grid→State sync

**Scope:** `Processor.h`, `Processor.cpp`
**Action:** Add `prepare(viewportRows, numCols, scrollbackLines)`. Remove resize detection from process(). Add guard in process() for unprepared grid. Register event handlers: `ID::screenDirty` → State, `ID::scrollUp` → read grid.getNumRows, set State numRows. Constructor calls prepare() with initial dimensions.
**Validation:** prepare() sets Grid size. process() never resizes. Events sync Grid → State.

### Step 4: State — numRows, scrollOffset, screenDirty Parameters

**Scope:** `State.h`, `State.cpp`, `Parameters.xml`, `Identifier.h`
**Action:** Add to SCREEN group in Parameters.xml: `numRows` (int, default 0), `scrollOffset` (int, default 0), `screenDirty` (int, default 0). Add Identifiers. Add setters/getters on State following existing pattern (atomic setter, VT getter).
**Validation:** Parameters declared. Setters/getters work. Flush propagates to ValueTree.

### Step 5: Session — host, calls prepare()

**Scope:** `Session.h`, `Session.cpp`
**Action:** Session calls `processor->prepare(rows, cols, scrollbackLines)` at construction and on resize (from `terminalResize` event handler). `scrollbackLines` from `lua::Engine::getContext()->nexus.terminal.scrollbackLines`. Remove any `grid.setSize()` calls in Session — Processor::prepare() handles it.
**Validation:** Grid sized by Processor::prepare(). Session never sizes Grid directly.

### Step 6: Screen — VT listener, reads State + Grid, feeds TextEditor

**Scope:** `Screen.h`, `Screen.cpp`
**Action:** Screen takes `State&` and `Grid&` in constructor. Registers as VT listener on State. On valueTreePropertyChanged: reads State for scrollOffset/numRows/visibleRows/cursor. Live mode: reads Grid viewport rows → creates Block → setText. Scroll mode: reads Grid history rows → same. No internal state beyond refs.
**Validation:** Live rendering correct. Scroll mode reads correct history. Screen is stateless.

### Step 7: Display — no VBlank, event-driven, simplified

**Scope:** `TerminalDisplay.h`, `TerminalDisplay.cpp`
**Action:** Remove VBlankAttachment, onVBlank lambda, previousHistoryRows. Remove direct grid reads. Screen takes State& and Grid& (not just TextEditor). Display creates Screen with refs. valueTreePropertyChanged: sync activeScreen and cursor to Screen. resized(): compute dimensions, fire terminalResize event. applyConfig(): no setScrollbackLines on TextEditor.
**Validation:** No VBlank. No grid access in Display. Pure event-driven.

### Step 8: Cleanup

**Scope:** All files
**Action:** Remove all stale artifacts. Remove `getWritePointer(screen, row, col)` from Grid (always Row* now). Verify no getters on Grid except getNumRows. Verify State is SSOT. Verify zero manual memcpy/memmove for buffer operations. Update doxygen.
**Validation:** Clean codebase. BLESSED compliant. All documentation current.

## BLESSED Alignment

- **B (Bound):** Grid owns buffer + boundary. Clear lifecycle. Session owns Grid. Processor references Grid.
- **L (Lean):** No VBlank polling. No DirtyRows infrastructure. No internal TextEditor buffer for terminal (future). Minimal code.
- **E (Explicit):** numRows is visible boundary. scrollOffset on State. screenDirty counter. No hidden state. No magic.
- **S (SSOT):** State is the only state machine. numRows on Grid is a calculation input set by Processor (like sampleRate on a DSP core) — not shadow state, because Processor is the sole writer and Grid never reads State. head[screen] is structural (ring index). No shadow copies.
- **S (Stateless):** Grid is dumb storage. Video holds calculation inputs only. Screen is stateless. Display is stateless. TextEditor is dumb renderer.
- **E (Encapsulation):** Video writes Grid. Screen reads Grid + State. TextEditor renders Block. Tell, don't ask. Unidirectional. No layer violations.
- **D (Deterministic):** Same Grid + same State = same render. Pointer reorder scroll is deterministic. Compiler auto-vectorizes at -O3.

## Risks / Open Questions

1. **Timing of setNumRows:** Between scrollUp (head advance) and Processor's setNumRows call, Grid's numRows is stale by `count`. This is safe because the scrollUp event fires synchronously on the reader thread — Processor handler runs immediately and calls setNumRows before Video's next getWritePointer. Verify this timing in the jam::Function::Map event dispatch.
2. **TextEditor dumb renderer:** Deferred to future sprint. Current approach (setText copy) is proven fast. No risk.
3. **Scrollbar:** Deferred to future sprint. No risk.
4. **Reflow:** Deferred to future sprint. jam::Row::wrapped flag laid in Step 2.
