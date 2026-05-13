# PLAN: jam::TextEditor — Clean Implementation

**Source:** ARCHITECT discussion (supersedes RFC-text-editor.md)
**Date:** 2026-05-14
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation — no overrides)

## Overview

Write jam::TextEditor from scratch using Terminal::Screen as the baseline. Screen is the proof that Cell-based rendering with our glyph pipeline works. TextEditor is the universal base — the generic Cell renderer with viewport, selection, cursor, coordinate queries. Screen inherits TextEditor, adds terminal-specific behavior. Terminal must work exactly as it does now.

## Architecture

### TETRIS-Analogous Pattern

| TETRIS (DSP) | TextEditor |
|---|---|
| setter → calc() | setFont / setActiveCells / setBounds → shape (build image) |
| process() | paint() — blit the pre-built image |
| calc() ≠ reset() | shape ≠ paint — separate concerns |
| getCoefficients() | getCellArea() — read-only report for Display to write State |
| ProcessorChain orchestrates | Display orchestrates — calls setters, reads getCellArea |
| DSP core is dumb worker | TextEditor is dumb worker — render what you're told |
| parameterChanged → calc | setFont/setActiveCells → shape (rebuild image) |
| Cold state (ProcessorChain owns) | Scrollback (Screen owns) |
| Hot state (DSP core, trivially copyable) | Grid cells (live buffer, reader writes, display drains) |

### APVTS Topology

| Audio Plugin | Terminal | Thread |
|---|---|---|
| AudioBuffer | Grid — live buffer, no history | READER |
| DSP Processor | Parser + Video | READER |
| PluginProcessor | Processor — Controller | READER |
| Host | Session — owns Grid + Processor | READER |
| APVTS | State — Model, SSOT | MESSAGE |
| PluginEditor | Display — View, owns Screen | MESSAGE |
| Visualizer widget | Screen : jam::TextEditor | MESSAGE |

### PROJECTION

- Parser = ABSOLUTE. Screen = LOGICAL. Decoupled via State.
- Creation: ABSOLUTE == LOGICAL
- Downsize normal: ABSOLUTE stays, LOGICAL reflows, no SIGWINCH, lossless
- Upsize past ABSOLUTE: SIGWINCH, ABSOLUTE grows to match
- Alternate: always SIGWINCH (CLI responsibility)

### Grid = AudioBuffer

Live buffer only. No history, no scrollback. Display drains scroll-off like a visualizer copies samples.

### OSC 133 A = Scrollback Boundary

Everything above A = scrollback (Screen territory). Everything from A down = live (Grid territory).

### Two Rendering Regions (same cell data, different lifecycle)

- Region 1: Scrollback + output above cursor — cached in image, blit on scroll
- Region 2: Active area around cursor — rebuilt on dirty. CaretComponent overlay.

## Critical Constraint

**Terminal must work exactly as it does now after Screen inherits TextEditor.** If it doesn't, TextEditor's base implementation is wrong — not Screen.

## Contract

- **No manual arithmetic** in consumer code
- **No manual boolean flags**
- **No shadow state** — Font is SSOT for metrics, Arrangement is SSOT for layout
- **Fully event-driven**
- **Unidirectional** — Display calls setters, reads getCellArea. Reverse: only PROJECTION.
- **shape ≠ paint** — shape builds the image (calc), paint blits (process). Never conflated.
- **ARCHITECT builds.** Agents never run build commands.
- **ARCHITECT runs git.** Agents never run git commands.

## Steps

### Step 1: jam::TextEditor from Scratch

**Scope:** `jam_gui/text_editor/jam_text_editor.h`, `jam_gui/text_editor/jam_text_editor.cpp`

**Action:** Delete the 2000-line juce fork. Delete `jam_text_editor_model.cpp`. Write jam::TextEditor from scratch — Terminal::Screen is the baseline for what works.

**What goes in TextEditor (universal — any Cell renderer needs this):**

Ownership:
- Private inner `ContentView : juce::Component` — `setBufferedToImage(true)`, `paint()` blits the pre-built image
- `std::unique_ptr<juce::Viewport> viewport`
- `ContentView* contentView` — non-owning, Viewport owns it
- `jam::glyph::Arrangement arrangement`
- `jam::glyph::Graphics glyphGraphics`
- `jam::Typeface::Atlas&` — context reference stored once at construction (like `lua::Engine::getContext()`)
- Caret via `getLookAndFeel().createCaretComponent(this)` — juce::LookAndFeel already inherits the method
- `jam::Font font` — SSOT for cell metrics
- `jam::Owner<jam::Cells> cells` (protected) — cell buffer storage
- `int activeCellsIndex { -1 }`

Setters (each triggers shape — builds the image):
- `setFont (const jam::Font&)` — stores font, reshapes
- `setActiveCells (int index)` — switches buffer, reshapes
- `resized()` — viewport bounds, computes cols/rows from own bounds + Font, reshapes

Read-only (report back, no mutation):
- `const jam::Bounds getCellArea() const noexcept` — cols/rows for Display to read after resized
- `const jam::Font& getFont() const noexcept`
- `int getNumLines() const noexcept` — from arrangement
- `bool isValid() const noexcept` — arrangement has content

Shape (calc — builds the image):
- Private `shape() noexcept` — called by setters when layout inputs change
  1. `arrangement.shape(activeCells, font, wrapCols)`
  2. `glyphGraphics.push(bounds, fullClip)`
  3. Iterate draw runs → `glyphGraphics.drawGlyphs(...)` per run, resolve colour via `findColour(textColourId)`
  4. Draw selection rects via `arrangement.boundsForRange(selection)`
  5. `glyphGraphics.pop()` → image ready
  6. Size ContentView from arrangement (content height)
  7. Auto-scroll if at bottom

Paint (process — blit only):
- `ContentView::paint(g)` — `g.drawImageAt(glyphGraphics.getImage(), 0, 0)` or equivalent blit. No shaping, no compositing, no atlas work.

Coordinate queries (using Arrangement API from Step 1 of previous work — already implemented):
- `indexAtPosition`, `positionForIndex`, `boundsForRange` — delegate to `arrangement`
- juce::TextInputTarget — all 11 methods delegate to arrangement + cells

Selection:
- `juce::Range<int> selection`
- Mouse handlers: mouseDown/Drag/Up → `arrangement.indexAtPosition()`. DoubleClick → `arrangement.getWordAtIndex()`
- `copy()` → `getTextInRange(selection)` → `SystemClipboard`
- `insertTextAtCaret(String)` — virtual, Screen overrides for PTY

Viewport:
- `visibleAreaChanged` for event-driven scroll notification
- `mouseWheelMove` → viewport scroll

ColourIds:
- `backgroundColourId`, `textColourId`, `highlightColourId`, `highlightedTextColourId`, `caretColourId`, `outlineColourId` — preserved for LookAndFeel compatibility

**What does NOT go in TextEditor:**
- State ref — TextEditor has no knowledge of terminal State
- Config ref — TextEditor has no knowledge of lua::Engine
- Terminal ColourIds (ansi0-15, cursor, selection, hintLabel)
- scrollbackRows — derived from data, not stored
- setDimensions — terminal-specific (Display calls setBounds, Screen computes internally)
- updateVisibleRow, appendScrollbackRows — terminal data flow
- Map nested struct — terminal buffer naming
- Per-frame metric lookups from State — Font is SSOT
- `glyphGraphics.clear()` per frame — shape builds image incrementally, not full rebuild
- leftIndent, topIndent, yOffset, borderSize — juce baggage

**Validation:** New TextEditor compiles. Same ContentView paint flow as current Screen. Same Arrangement shape. Same glyphGraphics push/drawContent/pop. No leftIndent/topIndent/yOffset. Atlas as context ref. Font as SSOT. Shape builds image, paint blits.

### Step 2: Screen Inherits TextEditor

**Scope:** `Source/terminal/rendering/Screen.h`, `Source/terminal/rendering/Screen.cpp`, `Source/component/TerminalDisplay.cpp`

**Action:** Screen inherits jam::TextEditor. Safe because TextEditor was built from Screen's pattern.

Screen removes (inherited from TextEditor):
- `ContentView` inner class
- `viewport`
- `arrangement`
- `glyphGraphics`
- `caret`
- `shapeAndRepaint` — replaced by base `shape()` triggered by setters
- `drawContent` — image built by base `shape()`
- `Owner<Cells> cells` — inherited (protected)

Screen keeps:
- `Map` nested struct
- Terminal-specific ColourIds (ansi0-15, cursor, selection, hintLabel)

Screen adds/adapts:
- Constructor: add two Cells to inherited `cells`, call `setActiveCells(0)`
- `updateVisibleRow(int row, const Cell* src, int numCols)` — writes into inherited cells
- `appendScrollbackRows(...)` — appends to inherited cells (flat model for now)
- `repaintContent()` — triggers base reshape via setter pattern
- `setActive(int index)` → base `setActiveCells(index)`
- `setCaretPosition(int index)` — calls base caret positioning
- `insertTextAtCaret(String)` override — no-op (terminal input via PTY)
- No State ref. No config ref. Display tells Screen everything via setters.

Display adapts:
- `resized()`:
  1. `screen.setBounds(contentBounds)`
  2. `screen.setFont(font)` — once on config change, or when font changes
  3. Read `screen.getCellArea()` → cols/rows
  4. PROJECTION: compare with State, conditionally write + SIGWINCH
- `onVBlank()`:
  1. Drain scroll-off → `screen.appendScrollbackRows(...)`
  2. Drain dirty rows → `screen.updateVisibleRow(...)`
  3. `screen.repaintContent()`
  4. Caret position from State cursor

**Terminal renders exactly as it does now.** TextEditor was built from Screen's pattern. Inheritance removes duplicate code. Same image, same pipeline, same result.

### Step 3: Dirty Tracking — Eliminate 64-Row Ceiling

**Scope:** END (Grid, TerminalDisplay)

**Action:** Replace `uint64_t` bitmask with `std::atomic<uint64_t>[8]` (512-row ceiling). Remove `r < 64` ceiling in Display onVBlank.

**Terminal renders exactly as before** for ≤ 64 rows. Fixes > 64 rows.

### Step 4: Newline Signal — Video → Screen

**Scope:** END (Video, Grid, TerminalDisplay, Screen)

**Action:** Persist newline/wrap distinction. `scrollUpAndFill()` gains `bool isNewlineTerminated`. Grid stores in parallel ring. Display passes to Screen. Screen stores for future logical-line building.

**Terminal renders exactly as before.** Flag stored, not yet consumed.

### Step 5: PROJECTION — Conditional SIGWINCH

**Scope:** END (TerminalDisplay only)

**Action:** Display.resized() conditionally writes to State.

1. `screen.setBounds(contentBounds)` — triggers Screen::resized() → shape
2. Read `screen.getCellArea()` → LOGICAL dims
3. Read State → ABSOLUTE dims
4. Alternate screen: always write + SIGWINCH
5. Normal screen AND LOGICAL > ABSOLUTE: write + SIGWINCH
6. Otherwise: skip. Grid stays. Screen wraps. Lossless.

**Terminal renders exactly as before** for upsize/creation. On downsize, content preserved.

### Step 6: Logical-Line Scrollback

**Scope:** END (Screen only)

**Action:** Evolve scrollback from flat memmove to logical-line model using newline flags from Step 4.

- `std::vector<std::unique_ptr<jam::Cells>> scrollback`
- `std::unique_ptr<jam::Cells> pendingLine`
- Build unified Cells from scrollback + visible → triggers base shape → image built
- Scrollback immune to resize

**Terminal renders exactly as before.** Storage changes, rendering identical.

### Step 7: Docs + Cleanup

**Scope:** ARCHITECTURE.md, SPEC.md, inline doxygen

**Action:** Update docs. Remove dead State metric getters. Delete RFC-text-editor.md. Delete PLAN-text-editor.md.

## BLESSED Alignment

- **B:** TextEditor owns rendering resources. Screen owns scrollback. Grid owns live buffer. Clear RAII chain.
- **L:** Clean implementation from scratch, not 2000-line fork. 300/30/3 enforced.
- **E:** Shape ≠ paint. Setters trigger calc. Coordinate queries delegate to Arrangement. No manual arithmetic. LookAndFeel — no casts.
- **S (SSOT):** Font for metrics. Arrangement for layout. Viewport for scroll. Image for rendered content.
- **S (Stateless):** TextEditor renders cells. Dumb worker.
- **E (Encapsulation):** TextEditor knows nothing about Grid/Video/State/config. Screen adds terminal API. Display mediates.
- **D:** Same cells + font + viewport = same pixels. Shape builds deterministic image. Paint blits.

## Failure Prevention

1. **Screen is baseline.** TextEditor built from Screen's proven pattern.
2. **shape ≠ paint.** Shape builds image (calc). Paint blits (process). Never conflated. No glyphGraphics.clear() per frame.
3. **No State/config ref in TextEditor or Screen.** Display mediates. Setters receive parameters.
4. **No build commands by agents.** ARCHITECT builds.
5. **No git commands by agents.** ARCHITECT runs git.
6. **No sycophancy.** Questions answered with facts. Not treated as commands.
7. **Terminal must work after Step 2.** If it doesn't, Step 1 is wrong.
