# RFC — jam::TextEditor + Terminal::Screen + Whelmed::Screen

Date: 2026-05-09
Status: Ready for COUNSELOR handoff

## Problem Statement

END has two parallel text rendering implementations:
- `jam::TextEditor` (2060-line fork of juce::TextEditor) with dual rendering paths (JUCE ShapedText + Glyph Cells)
- `Terminal::Screen` (316-line WIP) duplicating the Glyph Cells path

Both render through the proven Glyph atlas pipeline (`jam::Glyph::ShapedText`, `jam::Glyph::Graphics`, `jam::Typeface`). Everything else — data model, viewport, scrollback, layout — has gaps: no visible-area culling (O(N) per frame), full reshape every frame, memmove growing with scrollback, resize drops scrollback, no reflow, 64-row dirty bitmask limit.

The design question: what is the relationship between a terminal viewport and a text editor? Answer: they are the same widget. Monospace vs proportional is arithmetic, not architecture — `col * fixedAdvance` vs `sum(advances[0..col])`. The font determines the mode. END's renderer IS a TextEditor. That's how a true terminal can have WHELMED built in.

## Research Summary

### Six terminal emulators surveyed (Alacritty, Ghostty, Kitty, WezTerm, xterm, tmux)

**Scrollback storage — two schools:**
- Unified: WezTerm (`VecDeque<Line>`), tmux (`linedata[hsize+sy]`), Ghostty (`PageList`)
- Split: Alacritty (ring buffer), Kitty (`LineBuf` + `HistoryBuf`), xterm (`saveBuf` + `editBuf`)

**Viewport position:**
- All terminals: single integer or tag. WezTerm puts it in the GUI layer entirely (`PaneState::viewport: Option<StableRowIndex>`) — terminal model doesn't track scroll position.

**Wrap tracking for reflow:**
- Per-cell flag on last cell: Alacritty (`Flags::WRAPLINE`), WezTerm (`CellAttributes::wrapped` bit 11), Kitty (`CPUCell::next_char_was_wrapped`)
- Per-row metadata: Ghostty (`Row.wrap` + `Row.wrap_continuation`), tmux (`GRID_LINE_WRAPPED`), xterm (`LINEWRAPPED`)
- xterm does NOT reflow. All others do.

**Reflow consensus algorithm:** drain all rows → read wrap flags to identify logical lines → re-split at new column width → write back. O(N), runs once per resize.

### JUCE TextEditor (juce_TextEditor.cpp, juce_TextEditorModel.cpp)

**Architecture:** `TextEditorViewport` + `TextHolderComponent` (content component inside viewport). Three-layer text model: `TextEditorStorage` → `ParagraphsModel` → `ParagraphStorage`. Each paragraph owns lazily-computed `detail::ShapedText`. `drawContent()` does paragraph-level clip culling via `g.getClipBounds()`. `checkLayout()` sets content component size to drive scrollbar range. `visibleAreaChanged()` uses `ScopedValueSetter<bool>` reentrancy guard to break scrollbar↔layout cycle.

**Anti-pattern:** `ParagraphStorage::getTop()` is O(N) — iterates all preceding paragraphs. Terminal with 100K+ scrollback needs O(1) row positioning. Monospace cell grid provides this for free: `row * lineHeight`.

### Zed editor (crates/gpui, crates/editor, crates/terminal_view)

**Shared text shaping system** between editor and terminal. Terminal adds one parameter: `force_width: Some(cell_width)` in `shape_line()`, which post-processes glyph positions to snap to cell-width multiples via `apply_force_width_to_layout()`. Editor never uses `force_width` — proportional advances from HarfBuzz pass through unchanged.

**But Zed stops at the shaping layer.** Editor and terminal have completely separate layout/viewport/rendering code: editor uses `display_map` + `wrap_map` + `prepaint_lines()`; terminal uses `TerminalElement::layout_grid()` + `BatchedTextRun` + cell batching. Two customers of one shaper, two rendering architectures.

**Relevant patterns:**
- `LineLayoutCache` — two-frame rolling cache keyed by content hash, avoids reshaping unchanged lines
- `BatchedTextRun` — run-length encoding: adjacent same-style cells → one shaped run
- Background region merging — adjacent same-bg cells → one quad
- `CursorLayout` shared type used by both editor and terminal

### Position: END supersedes both poles

END is not "text editor with terminal inside" (Zed) nor "terminal with text editor inside" (classic). END's renderer IS a TextEditor — one widget, parameterized. The font determines monospace vs proportional arithmetic. That's how a terminal can natively contain WHELMED.

## Principles and Rationale

### Audio plugin analogy

| Audio (JFS) | Terminal (END) |
|---|---|
| DSP core (Atan, RBJ, Butterworth) | Glyph pipeline (ShapedText, Graphics, Typeface, Atlas) |
| ProcessorChain (orchestrator) | jam::TextEditor (widget orchestrator) |
| AudioBuffer | jam::Cells |
| APVTS | Terminal::State |
| Visualizer (spectrum, meter) | Terminal::Screen, Whelmed::Screen (derived classes) |

The Glyph pipeline is the proven DSP. jam::TextEditor is the ProcessorChain — it orchestrates layout, viewport, selection, and rendering. Derived classes are visualizers with domain-specific input methods.

### BLESSED mapping

- **B (Bound):** TextEditor owns Viewport, ContentView, cells, ShapedText, Graphics. Derived classes populate cells — clear ownership chain.
- **L (Lean):** One widget replaces two parallel implementations. No code duplication. 300/30/3 enforced per file.
- **E (Explicit):** Font determines layout mode — no boolean flag, no enum. Advance width IS the arithmetic. No magic.
- **S (SSOT):** One layout engine. One rendering path. One selection model. One viewport implementation.
- **S (Stateless):** TextEditor renders whatever cells contain — it does not track history, interpret VT sequences, or parse markdown. Dumb worker.
- **E (Encapsulation):** TextEditor knows nothing about Grid, Parser, Video, State, markdown, or blocks. Derived classes feed cells. TextEditor renders.
- **D (Deterministic):** Same cells + same font + same viewport position → same pixels. Always.

## Scaffold

### Ownership chain

```
jam::TextEditor : juce::Component
  ├── viewport          (std::unique_ptr<juce::Viewport>)
  ├── contentView       (ContentView* — viewed component, owned by viewport)
  ├── caret             (juce::CaretComponent via LookAndFeel)
  ├── cells             (jam::Owner<jam::Cells> — protected)
  ├── shapedText        (jam::Glyph::ShapedText)
  ├── glyphGraphics     (jam::Glyph::Graphics)
  ├── selection         (juce::Range<int>)
  └── layout metrics    (cellWidth, lineHeight, baseline, fontSize)
```

```
Terminal::Screen : jam::TextEditor
  ├── cells[0]          normal screen (scrollback + visible)
  ├── cells[1]          alternate screen (visible only)
  ├── scrollbackRows    (int — boundary between scrollback and visible in cells[0])
  ├── visibleRows       (int)
  ├── cols              (int)
  └── dirtyRows         (bitmask — which visible rows need reshaping)

Whelmed::Screen : jam::TextEditor
  ├── cells[0]          document content
  └── blocks            (domain-specific — TextBlock, MermaidBlock, etc.)
```

### jam::TextEditor — class scaffold

```cpp
namespace jam
{

class TextEditor
    : public juce::TextInputTarget
    , public juce::Component
{
public:
    explicit TextEditor (const juce::String& name = {}) noexcept;
    ~TextEditor() override;

    //==========================================================================
    // Configuration
    //==========================================================================

    /** Font determines layout arithmetic.
     *  Monospace font → equal advance → cell grid.
     *  Proportional font → natural advance → flowing text. */
    void setFont (const juce::Font& font) noexcept;
    const juce::Font& getFont() const noexcept;

    void setReadOnly (bool shouldBeReadOnly) noexcept;
    bool isReadOnly() const noexcept;

    void setMultiLine (bool shouldBeMultiLine) noexcept;
    bool isMultiLine() const noexcept;

    void setScrollbarsShown (bool shown) noexcept;
    void setScrollBarThickness (int thickness) noexcept;

    void setCaretVisible (bool visible) noexcept;
    bool isCaretVisible() const noexcept;

    //==========================================================================
    // Content
    //==========================================================================

    /** Sets which Cells object is active for rendering.
     *  Derived classes populate cells, then call setActiveCells. */
    void setActiveCells (int index) noexcept;

    /** Triggers reshape of active cells content. Call after modifying cells. */
    void reshapeContent() noexcept;

    //==========================================================================
    // Selection
    //==========================================================================

    void setHighlightedRegion (const juce::Range<int>& region) noexcept;
    juce::Range<int> getHighlightedRegion() const noexcept;

    void copy() noexcept;
    void paste() noexcept;
    void cut() noexcept;

    //==========================================================================
    // Caret
    //==========================================================================

    int getCaretPosition() const noexcept;
    void setCaretPosition (int index) noexcept;

    //==========================================================================
    // Layout queries
    //==========================================================================

    /** Cell width in logical pixels. Derived from font metrics.
     *  Monospace: max advance. Proportional: average advance (layout reference). */
    int getCellWidth() const noexcept;

    /** Line height in logical pixels. Derived from font metrics. */
    int getLineHeight() const noexcept;

    /** Total content height in logical pixels. */
    int getContentHeight() const noexcept;

    //==========================================================================
    // Scroll
    //==========================================================================

    /** Returns true when the viewport is scrolled to the bottom. */
    bool isAtBottom() const noexcept;

    /** Scrolls to the bottom of the content. */
    void scrollToBottom() noexcept;

    //==========================================================================
    // Component overrides
    //==========================================================================

    void resized() override;
    void paint (juce::Graphics&) override;

    //==========================================================================
    // LookAndFeel
    //==========================================================================

    enum ColourIds
    {
        backgroundColourId       = 0x4000001,
        textColourId             = 0x4000002,
        highlightColourId        = 0x4000003,
        highlightedTextColourId  = 0x4000004,
        caretColourId            = 0x4000005,
        outlineColourId          = 0x4000006,
    };

protected:
    //==========================================================================
    // Protected — derived classes populate cells
    //==========================================================================

    jam::Owner<jam::Cells> cells;

private:
    //==========================================================================
    // Internal types
    //==========================================================================

    struct ContentView;

    //==========================================================================
    // Viewport
    //==========================================================================

    std::unique_ptr<juce::Viewport> viewport;
    ContentView* contentView { nullptr };

    //==========================================================================
    // Rendering
    //==========================================================================

    jam::Glyph::ShapedText shapedText;
    jam::Glyph::Graphics   glyphGraphics;

    //==========================================================================
    // Layout
    //==========================================================================

    juce::Font currentFont;
    int   cellWidth  { 0 };
    int   lineHeight { 0 };
    int   baseline   { 0 };
    float fontSize   { 0.0f };
    int   activeCellsIndex { -1 };

    void computeMetrics() noexcept;      ///< Derive cellWidth/lineHeight/baseline from font.
    void checkLayout() noexcept;         ///< Size contentView, update scrollbars.
    void drawContent() noexcept;         ///< Visible-area culling + draw runs → glyphGraphics.

    //==========================================================================
    // Selection + caret
    //==========================================================================

    std::unique_ptr<juce::CaretComponent> caret;
    juce::Range<int> selection;
    bool readOnly    { false };
    bool multiline   { true };
    bool caretShown  { true };
    bool scrollbarsVisible { true };

    void updateCaretPosition() noexcept;

    //==========================================================================
    // Mouse
    //==========================================================================

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int indexAtPosition (float x, float y) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextEditor)
};

} // namespace jam
```

### Terminal::Screen — class scaffold

```cpp
namespace Terminal
{

class Screen : public jam::TextEditor
{
public:
    explicit Screen() noexcept;

    //==========================================================================
    // Terminal-specific API — called by Display
    //==========================================================================

    /** Sets terminal dimensions. Triggers visual re-wrap of scrollback. */
    void setDimensions (int newCols, int newRows) noexcept;

    /** Updates a visible row from Grid data.
     *  @param newlineTerminated  true if Video processed LF/NEL at end of this row. */
    void updateVisibleRow (int row, const jam::Cell* src, int numCols,
                           bool newlineTerminated) noexcept;

    /** Receives scroll-off rows from Grid. Builds logical lines in scrollback.
     *  @param newlineTerminated  per-row flag — true if row ends a logical line. */
    void appendScrollbackRows (const jam::Cell* const* rows,
                               const bool* newlineTerminated,
                               int rowCount, int numCols) noexcept;

    /** Switches active screen buffer (normal/alternate). */
    void setActiveScreen (int index) noexcept;

    /** Positions the caret from VT absolute cursor coordinates. */
    void setCaretPosition (int col, int row) noexcept;

    /** Triggers reshape + repaint. */
    void repaintContent() noexcept;

private:
    int cols        { 0 };
    int visibleRows { 0 };

    //==========================================================================
    // Scrollback — logical lines (normal screen only)
    //==========================================================================

    /** Logical lines in scrollback. Each Cells entry is one logical line
     *  (variable length). Alternate screen has no scrollback. */
    std::vector<std::unique_ptr<jam::Cells>> scrollback;

    /** Pending logical line being built from scroll-off rows.
     *  Extended when rows arrive without newline termination.
     *  Committed to scrollback when a newline-terminated row arrives. */
    std::unique_ptr<jam::Cells> pendingLine;

    //==========================================================================
    // Coordinate projection (absolute ↔ logical) — internal
    //==========================================================================

    /** Wraps logical lines + active grid content at current cols for display.
     *  Computes total visual row count for content height.
     *  Pure arithmetic — no stored wrap flags. */
    void wrapContent() noexcept;

    /** Maps absolute grid (col, row) to visual row index for caret positioning.
     *  Accounts for scrollback visual rows above the active grid. */
    int absoluteToVisualRow (int col, int row) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

} // namespace Terminal
```

### Visible-area culling in drawContent()

```cpp
void TextEditor::drawContent() noexcept
{
    // Uniform line height (monospace or proportional with fixed line spacing)
    // → O(1) row range computation.
    const int viewY { viewport->getViewPositionY() };
    const int viewH { viewport->getMaximumVisibleHeight() };

    const int firstRow { (lineHeight > 0) ? (viewY / lineHeight) : 0 };
    const int lastRow  { (lineHeight > 0) ? ((viewY + viewH) / lineHeight + 1) : 0 };

    // Shape only rows in [firstRow, lastRow] that are dirty.
    // Draw only draw runs whose row falls in visible range.
    // Glyph pipeline does the rest.
}
```

### Auto-scroll behavior

```cpp
void TextEditor::onContentGrew() noexcept
{
    // WezTerm pattern: auto-scroll when at bottom, hold when scrolled up.
    if (isAtBottom())
        scrollToBottom();

    // isAtBottom(): viewport.getViewPositionY() >= contentHeight - viewportHeight - lineHeight
}
```

### Logical lines and visual wrapping in Terminal::Screen

```cpp
void Screen::appendScrollbackRows (const jam::Cell* const* rows,
                                   const bool* newlineTerminated,
                                   int rowCount, int numCols) noexcept
{
    // For each scroll-off row:
    //   1. Append row cells to pendingLine (extend logical line)
    //   2. If newlineTerminated[i]: commit pendingLine to scrollback,
    //      start new pendingLine
    //
    // Logical lines in scrollback are variable-length.
    // No wrap flags stored. No reconstruction needed on resize.
}

void Screen::wrapContent() noexcept
{
    // Visual wrapping — Screen's sole responsibility.
    //
    // For each logical line in scrollback:
    //   visualRows += ceil (line.size() / cols)
    //
    // For active grid visible rows:
    //   visualRows += visibleRows  (fixed grid, always cols-wide)
    //
    // Total content height = visualRows * lineHeight
    // Set contentView size → scrollbar range updates naturally.
    //
    // On resize (cols changed): just re-run this. Logical lines unchanged.
    // Scrollback is immune to resize — never broken.
}
```

### Data flow

```
TERMINAL:
  PTY → Parser → Video → Grid (reader thread)
    → dirty rows + newline-termination bits + scroll-off rows
      → Display::onVBlank() (message thread)
        → Screen.updateVisibleRow()     — copy dirty row + newline bit
        → Screen.appendScrollbackRows() — build logical lines from scroll-off
        → Screen.repaintContent()       — wrapContent() + reshapeContent() + repaint
          → TextEditor.drawContent()    — visible-area culling
            → ShapedText.shape()        — only visible rows
              → Graphics.drawGlyphs()   — atlas compositing
                → paint

  Coordinate spaces:
    Video    → absolute (col, row) in fixed cols×rows grid
    Screen   → logical lines (variable length) + visual wrapping at current cols
    Projection: Screen-internal. absoluteToVisualRow() for caret.
                Scrollback = logical lines. Active grid = tail of visual content.

WHELMED:
  Markdown parser → blocks → cells populated as logical lines
    → reshapeContent() → drawContent() → shape → drawGlyphs → paint
```

## BLESSED Compliance Checklist

- [x] Bounds — TextEditor owns all rendering resources. Derived classes own domain data. Clear RAII chain.
- [x] Lean — One widget replaces two. No duplication. File splits by concern (TextEditor.h, TextEditorLayout.cpp, TextEditorMouse.cpp, TextEditorDraw.cpp).
- [x] Explicit — Font determines layout arithmetic. No mode flags. No magic. Advance width IS the mode.
- [x] SSOT — One layout engine, one rendering path, one viewport, one selection model.
- [x] Stateless — TextEditor renders cells. Does not track VT state, markdown blocks, or editing history. Dumb renderer.
- [x] Encapsulation — TextEditor knows nothing about Grid, Parser, Video, State, or markdown. Cells in, pixels out.
- [x] Deterministic — Same cells + same font + same viewport = same pixels.

## Resolved Decisions

1. **ShapedText output coordinates** — cell coordinates. Renderer does pixel conversion: `col * cellWidth` (monospace) or accumulated advance lookup (proportional). GlyphDrawRun API unchanged.

2. **Incremental shaping cache** — ShapedText owns it internally. Same pattern as JUCE's ParagraphStorage owning `std::optional<detail::ShapedText>` — lazily computed, invalidated when content changes. TextEditor just calls `shape()`.

3. **Wrapping is Screen's responsibility.** No `LAYOUT_WRAPPED` bit. No wrap flags on cells. Video outputs content in absolute coordinates. Grid carries a one-bit-per-row newline-termination flag (via RowState). Screen consumes it at scroll-off to build logical lines. Scrollback stores logical lines — variable length, self-describing, immune to resize. Visual wrapping is `ceil(line.length / cols)`, computed by Screen on demand.

4. **Line height** — mixed. Each line's height comes from its font metrics (ascent + descent + leading). Different font sizes produce different heights naturally. Accumulate line heights for positioning — no constant multiplication.

5. **juce::TextInputTarget** — keep. Required for CJK IME input composition. OS needs a TextInputTarget to position candidate windows and route composition events.

6. **No `wrapPending` on Video.** Deferred wrap (VT100 LCF) is a hardware workaround for when the buffer IS the display. With separated content and presentation, Video writes at `(col, row)` and advances `col`. Column overflow is a coordinate fact, not a deferred visual decision.

7. **Coordinate projection is Screen-internal.** Two coordinate spaces: absolute `(col, row)` for Video, logical lines for Screen. Projection is arithmetic on data Screen already owns. No dedicated projection object — YAGNI (L violation).

## Handoff Notes

- **Glyph pipeline stays unchanged.** `jam::Glyph::ShapedText`, `jam::Glyph::Graphics`, `jam::Typeface`, atlas infrastructure — proven, tested with full UTF-8 terminal spec. This RFC does not touch the pipeline internals, only how the widget orchestrates calls to it.
- **Current `jam::TextEditor` (fork) and `Terminal::Screen` (WIP) will be replaced.** The new `jam::TextEditor` is built from scratch using juce::TextEditor patterns but rendered through the Glyph pipeline. No JUCE fork, no dual rendering path.
- **Terminal::Screen inherits jam::TextEditor.** Domain-specific behavior (scrollback, dirty rows, dual buffer, VT cursor, reflow) layered on top. Same for Whelmed::Screen (proportional mode, document blocks).
- **Video Terminal work (RFC-video-terminal.md / PLAN-video-terminal.md) is independent but aligned.** Parser→Video split and command dispatch do not affect this RFC. Key alignment point: Video no longer owns wrapping. Video writes absolute coordinates. Grid carries a newline-termination bit per row (RowState). Screen builds logical lines from scroll-off and wraps visually. This eliminates `wrapPending` from Video and `LAYOUT_WRAPPED` from Cell.
- **File location:** `jam::TextEditor` lives in `~/Documents/Poems/dev/jam/jam_gui/text_editor/`. `Terminal::Screen` lives in `Source/terminal/rendering/Screen.h`. Both will be rewritten in place.
