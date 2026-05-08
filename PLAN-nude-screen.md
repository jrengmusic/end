# PLAN: Nude Screen

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-05-09
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

Strip jam::TextEditor from Screen. Screen becomes a purpose-built cell grid renderer: juce::Component + juce::Viewport + ShapedText + Glyph::Graphics. No paragraphs, no text storage, no word wrap, no selection machinery. ~200 lines replaces ~2000.

## Language / Framework Constraints

C++17 / JUCE. No overrides from LANGUAGE.md — reference implementation.

## Architecture

Screen stops inheriting `jam::TextEditor`. Becomes `juce::Component` with embedded `juce::Viewport`.

```
Screen : juce::Component
    Viewport viewport
        ContentComponent content   ← sized to totalDocRows * cellH, paints cells
    Owner<Cells> cells             ← normal (0) + alternate (1)
    ShapedText shapedText          ← shapes Cells → GlyphDrawRun[]
    Glyph::Graphics glyphGraphics  ← atlas compositor → renderTarget → blit
    CaretComponent caret           ← jam::CaretComponent
```

**Rendering pipeline (unchanged):**
1. `content.paint()` → `glyphGraphics.push()` (set viewport/clip)
2. `shapedText.shape(activeCells, typeface, cols)` → groups cells into DrawRuns
3. For each DrawRun: convert (col, row) → pixel, call `glyphGraphics.drawGlyphs()` (Overload C)
4. `glyphGraphics.pop()` → blit render target to Graphics

**Cell metrics** from font (same computation as current `reshapeCellsContent`):
- `cellW` = max ASCII advance × display scale
- `cellH` = (ascent + descent + leading) × display scale
- `baseline` = ascent × display scale

**Content component size** = `totalDocRows * cellH` height. Viewport scrolls within it. Auto-scroll to bottom on new content.

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions.

## Steps

### Step 1: Screen base class change

**Scope:** Screen.h, Screen.cpp, TerminalDisplay.h, TerminalDisplay.cpp

**Action:**
- Screen stops inheriting `jam::TextEditor`. Inherits `juce::Component` directly.
- Screen owns a `juce::Viewport` (member, not base class).
- Screen owns a content component (private inner struct, like TextHolderComponent) — child of Viewport, sized to full document height.
- Screen owns `jam::Owner<Cells> cells` directly (was on TextEditor).
- Screen owns `jam::Glyph::ShapedText shapedText` directly.
- Screen owns `jam::Glyph::Graphics glyphGraphics` directly.
- Screen has cell metric members: `cellW`, `cellH`, `baseline`, `fontSize`.
- Remove `setActiveCells()`, `activeCellsIndex`, `reshapeCellsContent()` — Screen manages its own cells and shaping.
- `setActive(int)` stays — switches active screen index.
- `setDimensions(int, int)` stays — sets cols/rows.
- `makeLayout(Command*, int)` stays — processes commands.
- Constructor: creates Viewport + content, adds normal + alternate Cells.
- Display: update any calls that used TextEditor API (setCaretPosition, setFont, setReadOnly, etc.).

**Validation:** Compiles. Screen is a Component with Viewport. No TextEditor dependency.

### Step 2: Cell metrics + font

**Scope:** Screen.h, Screen.cpp

**Action:**
- `setFont(juce::FontOptions)` — computes cell metrics from Typeface:
  - Find typeface via `jam::Typeface::findTypeface(family)`
  - Compute maxAdvance from ASCII range (32-127)
  - `cellW = toInt(maxAdvance, true)`
  - `cellH = toInt((ascent + descent + leading) * fontSize, true)`
  - `baseline = toInt(ascent * fontSize, true)`
- Called from constructor with config font. Called on font change.
- `setDimensions` uses cell metrics to size content component: `width = cols * cellW`, `height = totalDocRows * cellH`.

**Validation:** Cell metrics computed correctly. Content component sized.

### Step 3: Rendering

**Scope:** Screen.cpp (content component paint)

**Action:**
- Content component `paint()`:
  1. `atlas.advanceFrame()`
  2. `glyphGraphics.push(physW, physH, clipX, clipY, clipW, clipH)`
  3. Shape active cells: `shapedText.shape(activeCells, typeface, cols)`
  4. For each DrawRun in shapedText:
     - Convert `(col, row)` → pixel position: `(col * cellW + leftPad, row * cellH + baseline + topPad)`
     - Call `glyphGraphics.drawGlyphs(atlas, fontHandle, glyphCodes, codepoints, spans, styles, bgColours, positions, count, fontSize, colour, isEmoji, physCellW, physCellH, physBaseline)`
  5. `glyphGraphics.pop(g, clipX, clipY)`
- Only shape + render the visible rows (clip optimization):
  - `firstVisibleRow = viewport.getViewPositionY() / cellH`
  - `lastVisibleRow = min(firstVisibleRow + visibleRows + 1, totalDocRows)`
  - Shape only visible range (or shape all, let clip cull)

**Validation:** Terminal text renders correctly. Glyphs at correct positions. Colors correct.

### Step 4: Caret

**Scope:** Screen.h, Screen.cpp

**Action:**
- Screen owns `std::unique_ptr<jam::CaretComponent> caret` — child of content component.
- `setCaretPosition(int index)` — computes pixel rect from index: `col = index % cols`, `row = index / cols`, rect = `(col * cellW, row * cellH, cellW, cellH)`. Sets caret bounds.
- Display calls `screen.setCaretPosition(caretIndex)` as before.
- Caret color from LookAndFeel or config.

**Validation:** Cursor visible at correct position. Blink works.

### Step 5: Viewport auto-scroll

**Scope:** Screen.cpp

**Action:**
- After `makeLayout` modifies cells and reshapes: if content grew, scroll viewport to bottom.
- `content.setSize(cols * cellW, totalDocRows * cellH)` — resize content to match document.
- `viewport.setViewPosition(0, maxScrollY)` — auto-scroll to bottom.
- Scrollbar visible when content exceeds viewport.

**Validation:** New content auto-scrolls into view. Scrollbar works for scrollback. Resize doesn't destroy content.

### Step 6: Cleanup

**Scope:** Screen.h, Screen.cpp, jam_text_editor.h

**Action:**
- Remove from `jam::TextEditor`: `cells` (Owner), `activeCellsIndex`, `setActiveCells()`, `reshapeCellsContent()`, Cells-path branches in `drawContent`, `getCursorEdge`, `getTotalNumChars`, `indexAtPosition`, `updateEdge`, `resized`, `getCaretRectangleForCharIndex`. These were added for terminal and are no longer needed.
- Remove `setText(Cells&&)` — was for WHELMED Cells path. WHELMED can re-add if needed later.
- TextEditor returns to pure text editor — no Cells dual-path.

**Validation:** TextEditor compiles without Cells path. Screen compiles without TextEditor. WHELMED still compiles (verify separately — may need stub).

## BLESSED Alignment

- **B:** Screen owns Cells via Owner. Glyph::Graphics owns renderTarget. Lifecycle clear. No cross-thread.
- **L:** Screen is ~200 lines. No paragraph model. No text storage. drawContent is one loop.
- **E:** Cell → pixel mapping is `(col * cellW, row * cellH)`. No hidden context. No ShapedText wrapping surprises.
- **S(SSOT):** Cells = SSOT for content. ShapedText = derived (reshaped on change). renderTarget = visual output.
- **S(Stateless):** ShapedText rebuilt per frame. Glyph::Graphics has no memory after pop. Screen holds only cells + metrics.
- **E(Encap):** Parser → Grid → Display → Screen. Screen owns rendering. Display orchestrates. No layer violations.
- **D:** Same Cells → same ShapedText → same draw runs → same pixels.

## Risks / Open Questions

1. **ShapedText per frame** — currently shapes all cells every makeLayout call. For large scrollback, this may need visible-range-only shaping. Optimization for later.
2. **WHELMED** — removing Cells path from TextEditor may break WHELMED. Verify separately. May need `setText(Cells&&)` retained or moved.
3. **Selection** — not in scope. Future: Screen-native selection on cell grid (not TextEditor selection).
4. **Mouse events** — not in scope. Future: click-to-position, mouse reporting to PTY.
5. **Content component naming** — private inner struct. Propose `ContentView` or ARCHITECT decides.
