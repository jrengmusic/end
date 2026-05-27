# PLAN: END Text Rendering Foundation

**RFC:** RFC-text-editor.md
**Date:** 2026-05-27
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides — reference implementation)

## Overview

Implement the Neovim-modeled text rendering foundation. Ownership restructure first (Bounds), then SSOT storage, then wiring, then rendering. Full implementation validated by Auditor — single ARCHITECT review after completion.

## Naming Decisions (Locked)

| RFC name | Final name |
|---|---|
| `commit()` | `pushHistory()` |
| `overwriteLive()` | `flushLine()` |
| `committedCount()` | `historyCount()` |
| `plines()` free function | `TextLine::getWrappedLines(int viewWidth)` member |
| `calc()` | `calc()` (TETRIS convention) |
| `TextLineArray` | `TextLineArray` (juce::StringArray convention) |
| `TextLine` | `TextLine` |

## Validation Gate

All steps execute continuously — no ARCHITECT gate per step. @Auditor validates each step against ALL contracts before proceeding to next:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions (no deviation, no scope drift)
- RFC-text-editor.md (source verification)

Single ARCHITECT review after full implementation.

## Steps

### Step 1: Ownership Restructure (Bounds — CRITICAL FOUNDATION)

The entire plan depends on correct ownership. Current ownership is wrong for the RFC model. This step fixes it before anything else.

**Current ownership (wrong):**
```
Screen owns Buffer<Row>[2] (double-buffered rings + scrollback)
Screen owns atomic<Block<Row>*> activeBlocks
Video holds reference to Screen's atomic → loads lock-free on reader thread
Session owns Screen, Processor (which owns Video)
```

**Target ownership (RFC §4.1):**
```
Video owns Buffer<Row> (single, cols × visibleRows only, no scrollback, disposable scratch)
Session owns TextLineArray (SSOT — all content, scrollback + live)
Screen holds non-owning ref to TextLineArray (read-only for rendering)
```

**Scope:** All files below, atomic — all changes in this step together.

#### Construction Order Contract (IMMUTABLE — RFC §10)

C++ member declaration order = construction order. This is load-bearing and NON-NEGOTIABLE.

```
Session.h member declaration order:
    1. TextBuffer textBuffer           — no deps
    2. State state                     — before Screen
    3. terminal::Screen screen         — IS jam::TextEditor, SOLE AUTHOR of viewport dims
       → Screen::updateWinsize() fires in constructor → writes packed (cols, rows) to State
       → ALL operations depending on viewport dims MUST come AFTER this point
    4. jam::TextLineArray textLineArray — after Screen (needs visibleRows from Screen's dims)
    5. std::unique_ptr<Processor> processor — last (owns Video)
       → Video constructed inside Processor with (cols, rows) from State
       → Video allocates its own Buffer<Row> using these dims
    6. std::unique_ptr<jam::Resizer> resizer — wired in constructor body after Processor
    7. juce::Value winsize              — bound to TextEditor's viewport property
```

**TextEditor is SOLE AUTHOR of terminal viewport dimensions.** Viewport dims are derived from TextEditor's logical pixel dimensions. This flows ONE DIRECTION: Screen → State → consumers. Never reversed. Never bypassed. Any operation that needs (cols, rows) reads from State where Screen wrote them.

**No init sequence changes.** If the compiler requires reordering to satisfy a dependency, the dependency is wrong — fix the dependency, not the order.

#### Assert Discipline (HARD RULE)

- `jassert` ONLY for legitimate runtime invariants with a named threat
- Every assert must answer: "what specific scenario does this defend against?"
- No defensive guards on values guaranteed valid by construction order or framework contracts
- JUCE already asserts on invalid component bounds — do not duplicate
- Excessive guarding is a symptom of wrong design, not a safety measure
- If you need guards everywhere, the implementation is wrong — fix the design

#### 1a: New types in jam_graphics

**Files:** `jam/jam_graphics/detail/jam_text_line.h` (new), `jam/jam_graphics/detail/jam_text_line_array.h` (new), `jam/jam_graphics/jam_graphics.h`

`jam::TextLine` struct:
- `std::vector<Cell> cells` — content at commit time, `cells.size()` IS usedCols
- `bool isContinued { false }` — maps from Row::flexWrap
- `bool isJustified { false }` — maps from Row::justify
- `int getWrappedLines (int viewWidth) const noexcept` — ceiling division, returns 1 for empty/zero

`jam::TextLineArray` class with `std::deque<TextLine>`:
- `void pushHistory (TextLine&& line)` — append, enforce capacity via pop_front
- `void flushLine (int row, TextLine&& line)` — replace live slot at `lines[historyCount() + row]`
- `void setCapacity (int scrollbackLines, int visibleRows)` — init and SIGWINCH
- `void clear()` — ED 3: reset deque, preserve empty live slots
- `const TextLine& operator[] (int index) const noexcept` — uses .at() internally
- `int historyCount() const noexcept`, `int totalRows() const noexcept`, `int visibleRows() const noexcept`

Module header includes both new files (TextLine before TextLineArray). Zero includes in detail files.

#### 1b: Resizer refactor in jam_core

**File:** `jam/jam_core/buffer/jam_discrete_state_transition.h` → `jam/jam_core/buffer/jam_resizer.h`

`DiscreteStateTransition<T>` → `jam::Resizer`:
- Remove template parameter
- Remove `scratch` member (`Buffer<ElementType>`)
- Remove `buffer` reference member
- Keep: 16ms coalescing timer, `jam::Function::Map` triggers, `set(cols, rows)`, `timerCallback()`

Update jam_core module header.

#### 1c: Video owns its own Buffer<Row>

**Files:** `end/Source/terminal/Video.h`, `end/Source/terminal/Video.cpp` (and VideoEdit.cpp, VideoCSI.cpp etc.)

- Video constructor: replace `std::atomic<jam::Block<jam::Row>*>& activeBlocksRef` with initial `(cols, rows)`. Video constructs its own `jam::Buffer<jam::Row>` internally.
- Video owns `jam::Buffer<jam::Row> buffer` — single, not double-buffered. 2 channels (normal + alternate).
- Video owns `std::array<jam::Block<jam::Row>, 2> blocks` — constructed from its own buffer.
- Remove `activeBlocksRef` member, remove `refreshBlocks()`.
- All block access via `blocks[activeScreen]` directly (already reader-thread-only).
- `setWinsize(cols, rows)` resizes its own buffer (safe: called while processing suspended OR at reader-thread cold start).
- Expose read accessor for flush path: `const jam::Buffer<jam::Row>& getBuffer() const noexcept` (message-thread reads with same tearing tolerance as current atomic-pointer pattern — processing may be concurrent).

#### 1d: Screen loses buffer ownership

**Files:** `end/Source/terminal/component/Screen.h`, `end/Source/terminal/component/Screen.cpp`

- Remove `buffers[2]`, `blockSets[2]`, `activeBlocks` atomic, `activeIndex`
- Remove `resizeBuffers()` — no content to copy, TextLineArray is SSOT
- Remove `getBlocks()`, `getActiveBlocksRef()`, `getActiveBuffer()`
- Screen holds `const jam::TextLineArray*` non-owning ref (set by Session)
- `valueTreePropertyChanged` now calls `setText(textLineArray)` instead of constructing Block<Row>

#### 1e: Session wiring update

**Files:** `end/Source/terminal/Session.h`, `end/Source/terminal/Session.cpp`

- Add `jam::TextLineArray textLineArray` member (constructed after Screen)
- Replace `DiscreteStateTransition<Row>` with `jam::Resizer`
- Pass initial (cols, rows) to Processor/Video constructor instead of activeBlocksRef
- `textLineArray.setCapacity(scrollbackLines, visibleRows)` in constructor
- Set Screen's TextLineArray ref
- `wireResizer()` stop trigger: `Video::setWinsize` + `textLineArray.setCapacity` + resume + SIGWINCH + `screen.setText(textLineArray)` repaint

#### 1f: Processor wiring update

**Files:** `end/Source/terminal/Processor.h`, `end/Source/terminal/Processor.cpp`

- Video construction: pass (cols, rows) instead of `activeBlocksRef`
- Remove any activeBlocks-related plumbing

**Validation:** B (Bound) — every object has exactly one owner, lifecycle traceable. Video owns scratch buffer, Session owns SSOT, Screen owns nothing about content. No double-buffer. No atomic pointer indirection. No cross-owner mutations.

### Step 2: Wire Commit Path

**Scope:** `end/Source/terminal/ProcessorEvents.cpp`

In the `id::scrollUp` handler:
- Read the departing row(s) from Video's Buffer<Row> at the position about to be overwritten
- Construct `TextLine { cells[0..usedCols-1], isContinued=flexWrap, isJustified=justify }`
- Call `textLineArray.pushHistory(std::move(line))` for each departed row
- Retain WriteHead update temporarily (eliminated in Step 5)

The scrollUp event fires on the reader thread. Video's buffer is safe to read (same thread). TextLineArray.pushHistory must be thread-safe or called via callAsync — RFC says TextLineArray is message-thread-only storage. If scrollUp handler runs on reader thread, the pushHistory call must be dispatched to message thread.

**Validation:** Commit path matches RFC §4.2. Row → TextLine preserves usedCols, flexWrap, justify. S (SSOT): committed content is in TextLineArray.

### Step 3: Wire Flush Path

**Scope:** `end/Source/terminal/component/Screen.cpp` or `Session.cpp`

On State parameter flush (valueTreePropertyChanged, message thread):
- For each visible row r: read Row from Video's Buffer<Row> via accessor, construct TextLine
- Call `textLineArray.flushLine(r, std::move(line))`
- Call `setText(textLineArray)` on Screen

Message-thread reads of Video's Buffer<Row> have same tearing tolerance as the current atomic-pointer pattern (Video may be writing concurrently on reader thread).

**Validation:** Flush path matches RFC §4.2. Live slots overwritten every tick. Committed rows untouched. S (Stateless): idempotent.

### Step 4: TextEditor Rendering

**Scope:** `jam/jam_gui/text_editor/jam_text_editor.h`, `jam/jam_gui/text_editor/jam_text_editor.cpp`, `jam/jam_graphics/detail/jam_ParagraphStorage.h`

TextEditor:
- New overload: `void setText (const jam::TextLineArray& lines) noexcept`
- Stores non-owning ref. Calls `calc()`.
- `calc()` updated: when TextLineArray ref is set, computes viewWidth internally from viewport + font.bounds (same derivation as `updateWinsize()`), sums `getWrappedLines(viewWidth)` over all rows → `contentH = totalScreenRows * font.bounds.height` → `contentView->setSize(...)`
- Arrangement::shape with wrapColumns = internally-computed viewWidth
- Scrollbar: `setRange(totalProjectedScreenRows, visibleScreenRows)` with projected counts

ParagraphsModel:
- New overload: `void build (const jam::TextLineArray& lines) noexcept`
- Each TextLine is one row. `isContinued` maps to flexWrap scanning logic.

**Validation:** calc() matches RFC §7.2. Projection happens here only — no geometry in storage (S: SSOT). Scrollbar receives projected screen row counts (RFC §7.5).

### Step 5: Eliminate WriteHead

**Scope:** All END call sites referencing `jam::WriteHead`

- Remove WriteHead pack/unpack from ProcessorEvents.cpp scrollUp handler
- Remove WriteHead from State properties
- History depth = `textLineArray.historyCount()` — computed directly
- Remove WriteHead usage from Screen (already gone from Step 1d)
- Remove WriteHead from Video flush events where replaced

**Validation:** No WriteHead references remain in END. historyCount() is sole source for scrollback depth (S: SSOT).

### Step 6: Clear Behavior

**Scope:** `end/Source/terminal/ProcessorEvents.cpp` (id::clearBuffer handler)

Add `textLineArray.clear()` in the existing clearBuffer handler. TextLineArray::clear() resets deque, preserves empty live slots. Existing Video clear path unchanged.

**Validation:** ED 2/ED 3 clears both Video and TextLineArray (RFC §9). Idempotent.

## BLESSED Alignment

- **B (Bound):** Step 1 — ownership restructure is the foundation. Video owns scratch. Session owns SSOT. Screen owns nothing about content. Resizer is timer-scoped RAII. WriteHead eliminated.
- **L (Lean):** Resizer slimmed. Screen simplified (no double-buffered rings, no atomic pointer, no resizeBuffers). WriteHead eliminated.
- **E (Explicit):** getWrappedLines names its computation. pushHistory/flushLine name their write paths. No implicit geometry in storage.
- **S (SSOT):** TextLineArray is the single source for all content. viewWidth enters once at projection time. No shadow state.
- **S (Stateless):** TextEditor is pure renderer. calc() and getWrappedLines are pure projections. Flush path is idempotent.
- **E (Encapsulation):** TextLineArray knows nothing about width/pixels/projection. TextEditor knows nothing about terminal semantics. Video knows nothing about storage. Layer boundaries strict.
- **D (Deterministic):** Same TextLineArray + viewWidth always produces same ContentView height and glyph positions.

## Risks / Open Questions

None. All ownership boundaries resolved in Step 1. Thread safety for commit path (Step 2) uses callAsync dispatch from reader thread to message thread — same pattern as existing event handlers.
