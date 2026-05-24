# PLAN: TextEditor Reflow

**RFC:** RFC-reflow.md
**Date:** 2026-05-23
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE

---

## Overview

Reflow moves from Screen into jam::TextEditor's rendering pipeline. Word wrap +
justification handled by a new `jam::JustifiedText` (forked from `juce::JustifiedText`,
adapted to cell-based layout). Logical lines managed by `ParagraphStorage` — a
boundary index over `Buffer<Row>` rows, shared abstraction for terminal (append-only)
and future Whelmed (editable). Screen becomes sole author of terminal dimensions in
cell units. DST removed. Buffer writeback gates SIGWINCH.

**Already done this session:**
- `setWinsize` rename across TTY/Processor/Session/Display
- `jam_fonts` merged into `jam_graphics/fonts/`
- Screen::reflow() rewritten (to be replaced by Arrangement + JustifiedText)
- Display::resized() bypasses DST (writes cols/visibleRows to State directly)

---

## Steps

### Step 1: Mirror JUCE text model structure in jam

JUCE's pipeline (surface we mirror):
```
TextEditorStorage -> ParagraphsModel -> ParagraphStorage (content)
ShapedTextOptions (config: wordWrapWidth, justification)
SimpleShapedText (shaping + line breaking)
JustifiedText (positioning + stretch)
draw via accessTogetherWith
```

jam's pipeline (same surface, cell-based implementation):
```
TextEditorStorage -> ParagraphsModel -> ParagraphStorage (content)
ShapedTextOptions (config: wrapColumns, justification)
glyph::Arrangement (shaping + line breaking via atlas)
JustifiedText (positioning + stretch via Value::map)
draw via drawGlyphs
```

Divergence is ONLY downstream: atlas rendering vs CoreText/DirectWrite.

---

### Step 2: ParagraphStorage + ParagraphsModel
**Scope:** `jam_graphics/detail/jam_ParagraphStorage.h`

Mirror `juce::detail::ParagraphStorage` and `juce::detail::ParagraphsModel`.

**ParagraphStorage** — one logical line (paragraph). JUCE owns a `String`.
jam owns a row span into `Buffer<Row>`:
```cpp
struct ParagraphStorage
{
    int startRow;    ///< First row of this paragraph in the buffer.
    int rowCount;    ///< Number of rows in this paragraph.
};
```

Lazily creates shaped `Arrangement` for its content (mirrors JUCE's lazy
`getShapedText()`).

**ParagraphsModel** — collection of paragraphs. Tracks boundaries.
```cpp
int getNumParagraphs() const;
const ParagraphStorage& getParagraph (int index) const;
```

Terminal: Video appends. `flexWrap` set → extend current paragraph. Newline → new
paragraph. Append-only, O(1) per write.

Whelmed (future): same interface, backed by owned editable content.

---

### Step 3: ShapedTextOptions
**Scope:** `jam_graphics/detail/jam_ShapedTextOptions.h`

Mirror `juce::ShapedTextOptions`. Carries layout configuration:
```cpp
struct ShapedTextOptions
{
    int wrapColumns { 0 };                          ///< 0 = no wrap. Mirrors wordWrapWidth.
    juce::Justification justification { juce::Justification::topLeft };
    bool allowBreakingInsideWord { false };          ///< Fallback to char wrap when no FLEX_GAP.
};
```

Builder pattern mirrors JUCE:
```
withWrapColumns(int), withJustification(Justification), withAllowBreakingInsideWord(bool)
```

Passed to `Arrangement::shape()` and consumed by `JustifiedText`.

---

### Step 4: Row justify flag
**Scope:** `jam_graphics/fonts/cell/jam_row.h`

Add bit 2 to `Row::flags`:
```
static constexpr uint8_t justify { 1 << 2 };
```

**Scope:** `Source/terminal/Video.cpp` — FLEX_GAP stamping block (line ~577)

Video sets `Row::justify` on the current row when it stamps the first FLEX_GAP cell.

---

### Step 5: Arrangement FLEX_GAP-aware word wrap
**Scope:** `jam_graphics/fonts/jam_font/glyph/jam_glyph_arrangement_shape.cpp`

Current wrap (line ~345):
```cpp
if (wrapColumns > 0 and currentCol >= wrapColumns)
{
    ++currentLine;
    currentCol = 0;
}
```

Replace with FLEX_GAP-aware word wrap:
- Track last FLEX_GAP position as potential break point
- When `currentCol >= wrapColumns`: backtrack to last FLEX_GAP, break there
- If no FLEX_GAP on current line and `allowBreakingInsideWord`: character-level wrap
- Arrangement receives `ShapedTextOptions` (replaces raw `wrapColumns` param)

Arrangement receives pre-joined logical lines (paragraphs) from the caller. It does
not read `Row::flexWrap` — paragraph joining is ParagraphsModel's concern.

---

### Step 6: jam::JustifiedText
**Scope:** `jam_graphics/detail/jam_JustifiedText.h`, `jam_graphics/detail/jam_JustifiedText.cpp`

Fork `juce::detail::JustifiedText` (~300 lines). Same API surface:
- Constructor: `JustifiedText (const glyph::Arrangement&, const ShapedTextOptions&)`
- `getHeight()` — total layout height in cell rows
- `getLineMetricsForGlyphRange()` — per-line metrics
- `accessTogetherWith(callback)` — glyph traversal with applied stretch

Implementation differences:
- Input: `glyph::Arrangement` entries (not `SimpleShapedText`)
- Whitespace signal: `contentTag() == Cell::FLEX_GAP` (not `isWhitespace()`)
- Stretch: integer cell distribution via `jam::Value::map` (not float division)
- Per-line: read `Row::justify` flag to decide whether to distribute stretch
- Trailing empty (beyond usedCols) absorbs width delta first — gaps only contract
  when trailing is exhausted

Update `jam_graphics/jam_graphics.h` and `jam_graphics/jam_graphics.cpp` to include
the new files.

---

### Step 7: Integrate into TextEditor pipeline
**Scope:** `jam_gui/text_editor/jam_text_editor.h`, `jam_text_editor.cpp`,
`jam_text_editor_content_view.cpp`

Pipeline becomes:
```
Buffer<Row> -> ParagraphsModel -> Arrangement::shape(options) -> JustifiedText(arrangement, options) -> drawGlyphs
```

Changes:
- `TextEditor` holds `ParagraphsModel` (built from Block<Row> in setText)
- `ContentView::shapeVisibleContent`: iterate visible paragraphs, pass each to
  `arrangement.shape()` with `ShapedTextOptions { .wrapColumns = Cell::Rectangle(font.bounds, viewportBounds).getWidth() }`
- After shape, construct `JustifiedText` from arrangement + options. Draw via
  `accessTogetherWith` → `drawGlyphs`.
- `calc()`: compute content height from post-wrap line count, not raw `numRows`.

---

### Step 8: Screen sole author of cell dimensions
**Scope:** `Source/terminal/component/Screen.h`, `Screen.cpp`,
`Source/terminal/component/Display.h`, `Display.cpp`

Move cell dimension computation from Display::resized() into Screen::resized():
- Screen::resized(): `Cell::Rectangle(font.bounds, getLocalBounds())` -> cols/rows
- Screen::resized(): write cols/rows to State
- Display::resized(): only `screen.setBounds(contentBounds)` + pixel dims to State
- Remove cell dimension code from Display::resized()

---

### Step 9: Rewire DST in Screen
**Scope:** `Source/terminal/component/Screen.h`, `Screen.cpp`, `Display.cpp`

DST stays in Screen — terminal-specific concern. TextEditor is generic, no DST.

Remove:
- `Screen::reflow()` static method and declaration
- `Screen::reflowedContent` member — `transitioner.previous` IS the scratch
- `Screen::reflowedHistoryNormal` member
- `Screen::reflowLock` member

Keep:
- `Screen::transitioner` — owns DST lifecycle and scratch buffer (`previous`)

DST lifecycle on resize:
1. `transitioner.set(resizeStart, newCols, newRows)` — captures live buffer snapshot
   into `transitioner.previous`
2. Trigger: write new dims to State. Pass `Block(transitioner.previous)` to
   TextEditor via `setText()`. TextEditor wraps/justifies at new display width.
3. Crossfade: TextEditor renders from `previous` via Arrangement + JustifiedText.
4. `onStop`: materialize justified layout from TextEditor into live buffer at new
   width. Resize live buffer. `processor.setWinsize(cols, rows)` → SIGWINCH.

`valueTreePropertyChanged`: during transition render from `transitioner.previous`
(already the pattern). Outside transition render from live buffer. TextEditor
handles wrapping in both cases — same pipeline, different Block source.

---

### Step 10: Buffer writeback + SIGWINCH
**Scope:** `Source/terminal/component/Screen.cpp`, `Display.cpp`

In DST `onStop` handler:
1. Read justified layout from TextEditor's JustifiedText (resolved gap widths per line)
2. Resize live buffer to new width: `buffer.setSize(channels, ringSize, newCols)`
3. Write justified content into live buffer
4. Write new history count to State
5. `processor.setWinsize(cols, rows)` → SIGWINCH

DST guarantees: snapshot captured before trigger, onStop fires after transition,
coalescing handles rapid resize (latest wins). Buffer and PTY agree on dimensions
before app acts.

---

## Constraints

- `Cell::FLEX_GAP` is the sole gap identity mechanism
- `Row::justify` flag is the sole justification signal per row
- `ParagraphStorage` is the sole logical line boundary tracker
- `Value::map` for all proportional distribution
- `Cell::Rectangle` / `Cell::Point` for all pixel-cell conversions
- `<JuceHeader.h>` is the only include in project source files
- No anonymous namespaces. Static linkage for file-scope helpers.
- No early returns. Positive checks, jassert at preconditions.
- Reflow is a rendering concern — lives in TextEditor/Arrangement/JustifiedText
- DST is a terminal concern — lives in Screen, not TextEditor
- `transitioner.previous` is the sole scratch buffer — no `reflowedContent`
- Buffer writeback + SIGWINCH gated by DST onStop
- Screen is sole author of terminal winsize (cols/rows in cell units)

---

## Risks

- **calc() chicken-and-egg**: ContentView height depends on post-wrap line count,
  but shape runs in paint with a clip rect derived from ContentView height. May need
  to shape twice (once for height, once for visible clip) or shape in calc with full
  content.
- **Performance**: shaping full content in calc() may be expensive for large scrollback.
  Current clip-aware shaping in paint() only shapes visible rows. May need a
  pre-pass that counts wrapped lines without full shaping.
- **Buffer writeback thread safety**: Video writes to buffer on reader thread.
  Buffer writeback from reflow happens on message thread (DST onStop). DST
  coalescing prevents concurrent resize. SIGWINCH pauses app output until redraw.
