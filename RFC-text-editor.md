# RFC: END Text Rendering Foundation

**Author:** ARCHITECT  
**Status:** Approved — Ready for COUNSELOR  
**Date:** 2026-05-27

---

## 1. Preamble

This RFC replaces all prior RFC and PLAN documents relating to text rendering,
reflow, scrollback, and buffer architecture in END. Those documents are deleted.
They encoded the wrong mental model and are poison to COUNSELOR context.

There is no legacy. There is no fallback. There is no backward compatibility.
There is no status quo to preserve.

This RFC defines the FUNDAMENTAL FOUNDATION for ALL text rendering in END —
terminal and WHELMED — from the ground up.

---

## 2. The Correct Mental Model

### 2.1 What it is NOT

END's text rendering is NOT a terminal scanline model.

The scanline model treats the screen as a volatile grid of cells, sized to the
viewport, replaced on every frame. Scrollback is an afterthought bolted onto the
same grid. SIGWINCH resizes the grid, which corrupts or loses content. This is
the model every existing terminal emulator uses. It is fundamentally wrong for
lossless content preservation.

END does not use this model.

### 2.2 What it IS

END's text rendering IS the Neovim text editor model, translated faithfully into
the JUCE/JAM domain.

In Neovim, the terminal buffer (`buf_T` / `memline_T`) is the SSOT for all
content — history and current screen. The libvterm scratch grid is a mutable
surface for VT sequence processing only. The window (`win_T`) projects a view
into the buffer via `w_topline`. Width (`w_view_width`) enters exactly once, at
projection time, in `plines_win_nofold`. The buffer knows nothing about width.
SIGWINCH changes `w_view_width` only. Buffer is untouched.

END's architecture is identical in every structural respect, translated into
JUCE primitives:

| Neovim | END |
|---|---|
| `libvterm` internal screen grid | `Video::Buffer<Row>` — mutable scratch |
| `sb_pushline` callback | commit path: `scrollUpAndFill(top=0)` → `TextLineArray` |
| `refresh_screen()` / `ml_replace_buf` | flush path: timer → overwrite live slots in `TextLineArray` |
| `buf_T` / `memline_T` | `jam::TextLineArray` — SSOT storage |
| `win_T` + `w_topline` | `jam::TextEditor` + Viewport scroll position |
| `w_view_width` | `view_width` in cells — known only to `plines` at render time |
| `plines_win_nofold` | `jam::plines(const TextLine&, int viewWidth)` free function |
| `update_screen()` / `drawline.c` | `Arrangement::shape(...)` + glyph pipeline |
| SIGWINCH | `Resizer` stop trigger → `Video::setWinsize` → shell SIGWINCH |

The mapping is 1:1. Every structural decision in this RFC traces directly to a
verifiable decision in Neovim's `terminal.c`, `memline.c`, `plines.c`, or
`buffer_defs.h`.

### 2.3 The JUCE-specific difference

Neovim's scroll is logical: `w_topline` is a line index. No pixel reality.

`jam::TextEditor` is a real JUCE component with a real `juce::Viewport`.
ContentView has real pixel height. JUCE handles scrollbar, scroll physics, and
overscroll natively — **if and only if** ContentView height correctly reflects
the true total content height projected via `plines`.

This is not a liability. It is the asset. JUCE's Viewport IS the scroll engine.
END does not build one. END feeds it correct pixel dimensions derived from
logical line projection.

---

## 3. New Types

### 3.1 `jam::TextLine`

The atom. One committed terminal line. Equivalent of one `ml_get()` result in
Neovim. Lives in `jam_graphics`.

```cpp
struct TextLine {
    std::vector<Cell> cells;      // exactly usedCols cells at commit time
    bool isContinued { false };   // DECAWM soft-wrap — content continues on next line
    bool isJustified { false };   // contains Cell::FLEX_GAP — distribute free space
};
```

**`cells.size()` IS `usedCols`.** No separate field. Vector length is
authoritative content width. No geometry beyond the cells themselves.

**`isContinued`** maps directly from `Row::flexWrap`. Distinguishes soft-wrap
(DECAWM auto-wrap, content continues) from hard newline (line terminates).
Required for `ParagraphsModel` grouping and justification.

**`isJustified`** maps from `Row::justify`. Required for `JustifiedText` to
distribute free space to elastic cells.

`std::vector<Cell>` is correct for committed storage. Variable-width per line.
No stride constraint. `Cell` is trivially copyable; the vector itself is not —
this is intentional. `TextLine` is never stored in `jam::Buffer<T>` (which
requires trivially copyable elements). It lives in `jam::TextLineArray` only.

### 3.2 `jam::TextLineArray`

The SSOT storage. Equivalent of `buf_T` / `memline_T` in Neovim. Lives in
`jam_graphics` alongside `TextLine`.

```cpp
class TextLineArray {
public:
    void commit (TextLine&& line);                        // append committed line, enforce capacity
    void overwriteLive (int row, TextLine&& line);        // replace live slot (flush path)
    void setCapacity (int scrollbackLines, int visibleRows); // called on init and SIGWINCH
    void clear();                                         // ED 3 — clear all content

    const TextLine& operator[] (int index) const noexcept;
    int committedCount() const noexcept;                  // lines.size() - visibleRows
    int totalRows() const noexcept;                       // lines.size()
    int visibleRows() const noexcept;

private:
    std::deque<TextLine> lines;
    int scrollbackLines { 0 };
    int liveRows { 0 };
};
```

**Backing store is `std::deque<TextLine>`.** O(1) `push_back`, O(1) `pop_front`,
stable iteration, no reallocation of existing elements on growth. Correct for
variable-size elements. `jam::Buffer<T>` is wrong here — it uses `memcpy` and
`memset` on elements, which is UB for `std::vector<Cell>`.

**Structure of `lines`:**

```
index 0 .. committedCount()-1     — immutable committed history
                                    append-only from commit()
                                    never mutated after commit
index committedCount() .. end     — live rows (visibleRows entries)
                                    overwritten on every flush
                                    mirror of Video's current Buffer<Row>
```

**`commit()`:** appends one line to the committed region. If
`committedCount() > scrollbackLines`, calls `lines.pop_front()` to drop the
oldest line. Identical to Neovim's `adjust_scrollback` calling `ml_delete_buf`
from the front.

**`overwriteLive(row, line)`:** replaces `lines[committedCount() + row]`.
Called by the flush path on every State timer tick. Equivalent to
`ml_replace_buf` in `refresh_screen()`.

**`TextLineArray` knows nothing about `view_width`, pixel dimensions, or
projection.** It is pure content storage. Width enters exactly once — in
`plines`, outside this type.

### 3.3 `jam::plines` — free function

Faithful translation of `plines_win_nofold` from Neovim's `plines.c`.

```cpp
// Lives in jam_graphics, declared alongside TextLine.
int plines (const TextLine& line, int viewWidth) noexcept;
```

Implementation:

```cpp
int plines (const TextLine& line, int viewWidth) noexcept {
    if (viewWidth <= 0 || line.cells.empty()) return 1;
    return (static_cast<int>(line.cells.size()) + viewWidth - 1) / viewWidth;
}
```

`cells.size()` is the cell-unit content width. `viewWidth` is the viewport
width in cells. This is `plines_win_nofold` for terminal content — no
`win_col_off`, no `breakindent`, no fold columns. Terminal cells are already
display-width. Wide chars are encoded in `Cell::wide`; `usedCols` already
accounts for them at commit time.

**`plines` is a free function. It is not a method of `TextLine` or
`TextLineArray`.** Faithful to Neovim: `memline_T` has zero projection
knowledge. `plines_win_nofold` is a separate function in `plines.c`.

---

## 4. Storage Ownership and Boundaries

### 4.1 Ownership map

```
Session
    owns: Video                     — scratch surface, Buffer<Row>
    owns: TextLineArray             — SSOT content storage
    owns: State                     — APVTS parameter bridge
    owns: Screen                    — IS jam::TextEditor, renderer
    owns: Processor                 — PTY pipeline
    owns: Resizer                   — resize debouncer

Video
    owns: Buffer<Row>               — mutable scratch, cols × visibleRows only
    fires: commit event             — on scrollUpAndFill(top=0)
    fires: (implicit via State)     — flush timer triggers Screen repaint

Screen (IS jam::TextEditor)
    holds: non-owning ref to TextLineArray — read-only, for rendering
    owns nothing about content
    IS the SOLE AUTHOR of viewport dimensions = winsize
```

### 4.2 The two write paths into TextLineArray

**Commit path (sparse — O(scroll events) per second):**

`Video::scrollUpAndFill(top=0)` is about to rotate the ring head. Before doing
so, it fires the commit event. Session's handler:

1. Reads the departing row from `Video::Buffer<Row>` at current head position
2. Constructs `TextLine { cells[0..usedCols-1], flags }`
3. Calls `textLineArray.commit(std::move(line))`

This is `term_sb_push` / `sb_pushline` in Neovim. The row has left Video's
surface and will never be written again. It is now permanently committed.

**Flush path (60/120Hz — State timer):**

On every State parameter flush, Session serializes Video's current `Buffer<Row>`
live rows into the TextLineArray's live slots:

1. For each visible row `r` in `Video::Buffer<Row>`:
   - Read `Row* ptr = blocks[activeScreen].getWritePointer(r, writePosition)`
   - Construct `TextLine { cells[0..usedCols-1], flags }`
   - Call `textLineArray.overwriteLive(r, std::move(line))`
2. Trigger Screen repaint via State property change

This is `refresh_screen()` / `ml_replace_buf` in Neovim. Live rows are
overwritten with current Video state. Committed rows are untouched.

### 4.3 Video isolation

Video writes into its own `Buffer<Row>` only. It never touches `TextLineArray`
directly. It fires events; Session handles them. Video has zero knowledge of
storage, projection, or rendering.

Video's `Buffer<Row>` is always exactly `(cols, visibleRows)`. No scrollback
rows. No ring larger than the visible surface. SIGWINCH resizes it to new
`(cols, visibleRows)`. This resize is safe because there is no content to
preserve — the deque holds it all.

### 4.4 Screen isolation

Screen (`jam::TextEditor` subclass) holds a non-owning reference to
`TextLineArray`. It reads during paint. It never writes. It owns nothing about
content lifecycle.

Screen IS the SOLE AUTHOR of viewport dimensions → winsize. This contract is
unchanged. `jam::TextEditor::updateWinsize()` computes `(cols, visibleRows)`
from the visible pixel dimensions and writes to State. Session hears the
property change and triggers the Resizer. This is the only path that produces a
winsize change.

---

## 5. SIGWINCH Safety

SIGWINCH arrives when `jam::TextEditor::updateWinsize()` writes a new packed
`(cols, rows)` to State. This happens when:

- The window is resized by the user
- The scrollbar appears or disappears (scrollbar width changes `visibleWidth`,
  which changes `cols`)

Both cases are safe.

**`TextLineArray` committed lines are untouched on SIGWINCH.** They have no
geometry. `cells.size()` is their content width from commit time. `plines`
recomputes their screen row count at the new `view_width` on the next render
pass. Zero mutation.

**`Video::Buffer<Row>` is resized** to new `(cols, visibleRows)`. New blank
canvas. Shell redraws via SIGWINCH response. Flush path overwrites live slots
with new content at new dimensions on next timer tick.

**Scrollbar appearing during extreme throughput** (e.g., `seq 1000000`):
`updateWinsize()` already accounts for scrollbar visibility:

```cpp
const bool isVerticalScrollBarShown { viewport->isVerticalScrollBarShown() };
const int visWidth { isVerticalScrollBarShown
                     ? viewport->getMaximumVisibleWidth()
                     : viewport->getWidth() };
```

When scrollbar appears, `cols` shrinks by scrollbar width. Resizer debounces
rapid changes with 16ms coalescing. One SIGWINCH fires. Content in
`TextLineArray` is untouched throughout.

---

## 6. Resizer (formerly DiscreteStateTransition)

`jam::DiscreteStateTransition<jam::Row>` is renamed `Resizer` and the template
is removed. The `scratch` member (`jam::Buffer<ElementType>`) is removed — it
existed to copy ring content during resize. With `TextLineArray` as SSOT and
`Video::Buffer<Row>` holding only live rows, there is no content to copy.

`Resizer` retains:
- 16ms coalescing timer — collapses rapid SIGWINCH events during window drag
- `jam::Function::Map` trigger mechanism — start/stop callbacks registered by Session
- `set(cols, rows)` — fires start trigger synchronously, starts/restarts timer
- `timerCallback()` — fires stop trigger

Session wires the Resizer stop trigger as:

```
stop trigger:
    Video::setWinsize(newCols, newRows)    — resize Buffer<Row>, blank canvas
    textLineArray.setCapacity(scrollbackLines, newRows)  — update live slot count
    processor->suspendProcessing(false)
    processor->setWinsize(newCols, newRows) — SIGWINCH to shell
    screen.setText(textLineArray)          — TextEditor repaints at new dimensions
```

`WriteHead` is eliminated. It packed ring position and `historyRows` for
cross-thread atomic use. With no ring and `TextLineArray` as message-thread-only
storage, history depth = `textLineArray.committedCount()` — computed directly,
no packing needed.

---

## 7. jam::TextEditor — Pure Renderer

`jam::TextEditor` is a domain-agnostic JUCE component layer for the glyph
pipeline. It does not know it is rendering terminal content. It does not know
about markdown. It renders whatever `TextLine` sequence it is given.

It is the foundational renderer for both:
- `terminal::Screen` — terminal content from `TextLineArray`
- WHELMED — rich text content (future, same component)

### 7.1 setText new overload

```cpp
void setText (const jam::TextLineArray& lines, int viewWidth) noexcept;
```

Stores a non-owning reference to `lines` and `viewWidth`. Calls `calc()`.

### 7.2 calc() — ContentView height

`calc()` computes ContentView pixel height as:

```cpp
int totalScreenRows = 0;
for (int i = 0; i < lines.totalRows(); ++i)
    totalScreenRows += jam::plines(lines[i], viewWidth);

const int contentH = totalScreenRows * font.bounds.height;
contentView->setSize(contentPixels.x, contentH);
```

This is the direct translation of how Neovim computes window height from
`plines_win_nofold` sum over buffer lines.

`viewWidth` is derived inside `updateWinsize()` from `visWidth / font.bounds.width`.
TextEditor is SOLE AUTHOR of this value.

### 7.3 Arrangement::shape

`Arrangement::shape(blocks, numBlocks, font, wrapColumns, lineOffset)` is called
with `wrapColumns = viewWidth`. This is the within-line wrapping that Neovim
handles via `plines_win_nofold`'s ceiling division. Already implemented.

`lineOffset` is the accumulated `plines` sum up to the first visible line — so
glyph y-positions land at their correct absolute ContentView coordinates within
the clip rectangle.

### 7.4 ParagraphsModel

`ParagraphsModel::build` receives a new overload for `TextLineArray`:

```cpp
void build (const jam::TextLineArray& lines) noexcept;
```

Each `TextLine` is one row in paragraph index space. `TextLine::isContinued`
maps directly to `Row::flexWrap` — same semantics, same scanning logic.
`ParagraphStorage::startRow` and `rowCount` are in `TextLine` index space.

### 7.5 Scrollbar

`Scrollbar::setRange(capacity, visibleRows)` receives projected screen row
counts, not logical line counts. When logical lines wrap, one `TextLine`
occupies multiple screen rows. The scrollbar thumb must reflect the proportion
of screen rows visible, not logical line count.

Caller computes: `capacity = sum(plines(line, viewWidth))` over committed lines.
`visibleRows` = viewport height in cells. Caller is Screen / TextEditor flush
path.

---

## 8. No Reflow

There is no reflow. Ever.

`TextLine::cells` is immutable after commit. `cells.size()` records content
width at commit time. When `view_width` changes (SIGWINCH), `plines` recomputes
the screen row count for every line at the new width. ContentView height
recomputes. JUCE Viewport repositions. Zero buffer mutation.

This is exactly Neovim's behavior. Neovim does not reflow scrollback. Committed
lines live at their original width. `plines_win_nofold` wraps them at
`w_view_width` on every draw. Buffer untouched.

---

## 9. Clear Behavior

When the shell sends `ED 2` (erase display) or `ED 3` (erase display +
scrollback), both `Video::Buffer<Row>` and `TextLineArray` are cleared:

- `Video::Buffer<Row>`: cleared via existing `eraseInDisplay()` path, unchanged
- `TextLineArray::clear()`: resets `lines` deque, preserves `liveRows` empty
  live slots

The `id::clearBuffer` event already fires for ED 2 and ED 3 in
`VideoEdit.cpp`. Session registers a handler that calls
`textLineArray.clear()`.

---

## 10. Initialization Sequence Contract

C++ construction order is load-bearing. The existing contract is preserved
exactly:

```
Session constructor:
    1. TextBuffer (cross-thread string slots) — constructed first, no deps
    2. State (APVTS parameter store) — constructed before Screen
    3. Screen (IS jam::TextEditor) — constructed before Processor
       - Screen grafts its state node into terminalState
       - Screen nodes (NORMAL, ALTERNATE) already exist (grafted by Display)
       - Screen calls updateWinsize() → writes viewportId to State
    4. TextLineArray — constructed after Screen, initialized with
       (scrollbackLines, visibleRows) from Screen's initial dimensions
    5. Processor (owns Video, owns PTY pipeline) — constructed last
       - Video is constructed inside Processor
       - Video receives activeBlocksRef from Screen
    6. Resizer — wired after Processor is valid (wireResizer())
       - start trigger: processor->suspendProcessing(true)
       - stop trigger: Video resize + textLineArray.setCapacity + SIGWINCH + setText

Session::start():
    - processor->setWinsize(startCols, startRows)
    - processor->startTTY(...)  — reader thread starts here
    - TextLineArray live slots initialized to blank TextLines
```

Screen IS the SOLE AUTHOR of viewport dimensions. `updateWinsize()` in
`jam::TextEditor` is called from `resized()` and `setFont()`. It writes the
packed `(cols, rows)` to State. Session hears it via `Value::Listener` and
triggers Resizer. This chain is unchanged.

---

## 11. Scope of Changes

### New in JAM (`jam_graphics`)

- `jam::TextLine` — new struct
- `jam::TextLineArray` — new class
- `jam::plines(const TextLine&, int viewWidth)` — new free function
- `jam::TextEditor::setText(const TextLineArray&, int viewWidth)` — new overload
- `jam::TextEditor::calc()` — updated to sum `plines` over `TextLineArray`
- `jam::ParagraphsModel::build(const TextLineArray&)` — new overload

### Changed in JAM (`jam_core`)

- `jam::DiscreteStateTransition<T>` → `jam::Resizer` — remove template, remove
  `scratch` member, keep coalescing timer and `Function::Map` triggers

### Changed in END

- `terminal::Session` — owns `TextLineArray`, wires commit and flush paths,
  updates `wireResizer()` stop trigger
- `terminal::Screen` — `valueTreePropertyChanged` reads from `TextLineArray`
  via `setText(textLineArray, viewWidth)` instead of constructing `Block<Row>`
- `terminal::Screen::resizeBuffers` — simplified, no content copy, no
  `WriteHead` update
- `jam::WriteHead` — eliminated from all END call sites; history depth derived
  from `textLineArray.committedCount()` directly

### Unchanged

- `jam::Cell` — correct atom, no changes
- `jam::Row` — correct for `Video::Buffer<Row>` scratch surface, no changes
- `jam::Buffer<Row>` — correct for Video's live scratch, no changes
- `jam::Block<Row>` — correct for Video's internal write path, no changes
- `glyph::Arrangement::shape(Block<Row>*, ...)` — existing overloads unchanged
- `Video` write path — `print()`, `scrollUpAndFill()`, `eraseInDisplay()` etc.
  unchanged except commit event wire-up in `scrollUpAndFill(top=0)`
- `terminal::TextBuffer` — cross-thread string slots, unrelated, unchanged
- `jam::TextEditor::updateWinsize()` — SOLE AUTHOR contract unchanged

---

## 12. Source Verification

All decisions in this RFC are traceable to source-verified behavior in:

- `neovim/src/nvim/terminal.c` — `refresh_terminal()`, `refresh_scrollback()`,
  `refresh_screen()`, `term_sb_push`, `ScrollbackLine`, `REFRESH_DELAY`
- `neovim/src/nvim/memline.c` / `memline_defs.h` — zero geometry in storage
- `neovim/src/nvim/plines.c` — `plines_win_nofold` projection formula
- `neovim/src/nvim/buffer_defs.h` — `win_T`, `w_topline`, `w_view_width`
- `jam/jam_core/buffer/jam_buffer.h` — `memcpy`/`memset` on elements,
  trivially copyable requirement
- `jam/jam_core/buffer/jam_block.h` — ring-aware view, hot path contract
- `jam/jam_graphics/detail/jam_row.h` — `usedCols`, `flexWrap`, `justify`
- `jam/jam_gui/text_editor/jam_text_editor.cpp` — `updateWinsize()`,
  `calc()`, scrollbar visibility accounting
- `end/Source/terminal/VideoEdit.cpp` — `shiftLines()`, `id::scrollUp` event,
  full-screen vs. sub-region scroll distinction
- `end/Source/terminal/Session.cpp` — `wireResizer()`, `valueChanged()`
- `end/Source/terminal/component/Screen.cpp` — `resizeBuffers()`,
  `valueTreePropertyChanged()`
