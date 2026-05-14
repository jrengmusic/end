# PLAN: jam::TextEditor — Cell Rendering Substrate

**RFC:** none — objective from ARCHITECT prompt + session decisions
**Date:** 2026-05-15
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation — no overrides)

## Overview

Write jam::TextEditor as our own class built on jam::Buffer<Cell> and jam::Block<Cell> — the AudioBuffer/AudioBlock pattern applied to terminal cells. One rendering pipeline for Terminal and Whelmed. Grid uses AbstractFIFO + Buffer for lock-free parser-to-display transfer. Screen adds a Live buffer. Cells type deleted.

## Architecture

### jam::Buffer<T> — AudioBuffer Pattern

Flat contiguous storage. Owns memory. No ring, no FIFO, no opinions.

```
AudioBuffer<float>  →  jam::Buffer<Cell>
AudioBuffer<float>  →  jam::Buffer<Grapheme>
```

API mirrors AudioBuffer:
- `setSize (int numRows, int numCols, bool keepExisting = false, bool clearExtra = false, bool avoidReallocating = false)`
- `getReadPointer (int row)` → `const T*`
- `getWritePointer (int row)` → `T*` (sets isClear = false)
- `getNumRows()`, `getNumCols()`
- `clear()`, `clear (int row)`, `clear (int row, int startCol, int numCols)`
- `hasBeenCleared()`

Rows × Cols. HeapBlock<char, true> (SIMD-aligned). T must be trivially copyable.

### jam::Block<T> — AudioBlock Pattern

Non-owning view into Buffer<T>. Zero cost. No allocation. Valid for paint call duration.

```
AudioBlock<float>  →  jam::Block<Cell>
AudioBlock<float>  →  jam::Block<Grapheme>
```

Members:
- `const T* const* rows` — pointer to row pointer array
- `int startRow`
- `int numRows`
- `int numCols`

API:
- Construct from `Buffer<T>&` (full), or `Buffer<T>&` + startRow + numRows (sub-region)
- `getRowPointer (int row)` → `const T*` (accounts for startRow offset)
- `getNumRows()`, `getNumCols()`
- `getSubBlock (int offset, int length)` — pure pointer arithmetic, zero copy

Block<Cell> + Block<Grapheme> travel together for rendering. Arrangement takes both.

### Grid — Composite

Grid owns AbstractFIFO (index manager) + Buffer<Cell> + lazy Buffer<Grapheme>.

AbstractFIFO manages scroll-off transfer indices (lock-free SPSC: parser thread writes, message thread reads). Buffer stores the cell data. Dual-screen (normal + alternate) stays.

Public API unchanged: getReadPointer, getWritePointer, setSize, clear, scrollUp/Down, consumeDirtyRows, getScrolledReadPointer, consumeScrolledRows.

Dirty tracking stays on Grid (consumer-specific, not Buffer's concern).

### 3-Screen Model

TextEditor base: 2 Buffers — Normal (0) + Alternate (1).
Screen (derived): adds Live Buffer — terminal-specific.

Map 0-based:
```cpp
enum { normal = 0, alternate = 1 };
```

| Buffer | Owner | Purpose |
|---|---|---|
| Normal | TextEditor | Content / scrollback history. Ring-managed (head + count). FIFO trim at scrollbackLines. |
| Alternate | TextEditor | Full-screen apps (vim, less). Overwrite by row index. |
| Live | Screen | Current visible frame from Grid. Overwrite by row index. |

Normal screen view = Normal + Live (history above, live below).
Alternate screen view = Alternate only.

### Content Flow

**Terminal:**
1. Parser writes to Grid via getWritePointer (reader thread)
2. Rows scroll off Grid → AbstractFIFO captures indices
3. Display drains FIFO → memcpy rows from Grid Buffer to Screen Normal Buffer (message thread)
4. Display copies dirty rows from Grid Buffer to Screen Live Buffer
5. ContentView::paint: Screen constructs Block<Cell> array → Arrangement shapes → glyph::Graphics draws

**Whelmed (future):**
1. AttributedString → static decoder → writes Cell records directly into TextEditor Normal Buffer
2. ContentView::paint: TextEditor constructs Block<Cell> → Arrangement shapes → glyph::Graphics draws

### Rendering Pipeline

Shape in paint. NOT in calc.

**calc()** — sizes ContentView from row count, auto-scrolls if at bottom, repaints.

**ContentView::paint** — shape + draw (one shape per visual frame via JUCE repaint coalescing):
1. `g.fillAll(bg)` → `atlas.advanceFrame()`
2. Owner constructs Block<Cell> array for active mode:
   - Normal: `{ Block over Normal, Block over Live }`
   - Alternate: `{ Block over Alternate }`
   - Startup: `{ Block over Live }`
3. `arrangement.shape (blocks, numBlocks, font, 0)`
4. `glyphGraphics.push` → iterate draw runs → drawGlyphs per run → `glyphGraphics.pop`

No setBufferedToImage. Render directly each paint. glyph::Graphics clears full render target each push — no scroll-delta memmove.

### Font

Set once on config change. Not on resize. SSOT for cell metrics. Display stores `lastFont`.

### Emoji Resolution

Text font first. If text font returns .notdef (glyphIndex == 0), fall back to emoji. OS delegation always last.

```cpp
jam::Typeface::GlyphRun run { typeface.shapeText (style, &pen.codepoint, 1) };
const bool textFontHasGlyph { run.count > 0 and run.glyphs[0].glyphIndex != 0 };
const bool isEmojiCodepoint { not textFontHasGlyph and pen.codepoint > 0x7eu
                              and typeface.hasEmojiGlyph (pen.codepoint) };
```

### CaretComponent

Forked from juce::CaretComponent. Inherits juce::Component + private juce::Timer. Blink via shouldDraw flag + repaint on self only — no visibility toggling, no parent invalidation. Owns caretColourId = 0x4100001.

### Performance

Buffer<Cell> is contiguous SIMD-aligned memory. Block<Cell> is a zero-cost view. No per-row allocation. No Cells wrapper. memcpy for row transfer. Shape only visible rows via Block sub-region.

`seq 1 10000000` — parser at 99% CPU. Message thread drains at vblank. Lock-free. No blocking. No allocation on hot path.

### Memory Budget (10K scrollback, 200 cols)

| Instance | Rows | Cell Data | Grapheme (lazy) |
|---|---|---|---|
| Grid normal | visible + margin | ~870KB | on demand |
| Grid alternate | visible | ~75KB | on demand |
| Screen Normal | 10K | 32MB | on demand |
| Screen Live | visible | ~75KB | on demand |
| Screen Alternate | visible | ~75KB | on demand |
| **Total** | | **~33MB** | sparse |

## Contract

- **Buffer is flat** — AudioBuffer pattern. No ring, no FIFO. Consumers add behavior.
- **Grid: composite** — owns AbstractFIFO + Buffer<Cell> + lazy Buffer<Grapheme>. Does not inherit Buffer.
- **Block is non-owning** — valid for paint call duration only. Dangling if source Buffer destroyed/resized.
- **screen is PRIVATE** — derived uses API (appendRow/setVisibleRow/setActiveScreen), never touches buffers directly.
- **No Cells type** — Buffer<Cell> replaces it everywhere. Cells deleted.
- **No defensive programming** — jasserts only for debug. Trust the pipeline.
- **No dead API** — every public method has a proven caller NOW.
- **No shadow state** — no manual offset counters, no re-deriving what Grid/Buffer already guarantee.
- **Move not copy** — Grid row scrolls off → memcpy to Screen Normal Buffer (trivially copyable Cell, single transfer).
- **Map 0-based** — Normal = 0, Alternate = 1. Video uses Map constants.
- **Text font priority** — font → NF icons → emoji (OS delegation last).
- **No setBufferedToImage** — render directly.
- **glyph::Graphics clears full render target each push** — no scroll-delta pixel caching.
- **ARCHITECT builds.** Agents never run build commands.
- **ARCHITECT runs git.** Agents never run git commands.

## Steps

### Step 1: jam::Buffer<T>
**Scope:** `jam_core/buffer/jam_buffer.h`
**Action:** Write Buffer<T> — AudioBuffer pattern. Flat HeapBlock<char, true> (SIMD-aligned). setSize, getReadPointer, getWritePointer, clear, hasBeenCleared, getNumRows, getNumCols. T must be trivially copyable (static_assert). Add to jam_core.h includes.
**Validation:** Compiles. API matches AudioBuffer shape. No ring logic. BLESSED L (under 300 lines). NAMES.md compliant.

### Step 2: jam::Block<T>
**Scope:** `jam_core/buffer/jam_block.h`
**Action:** Write Block<T> — AudioBlock pattern. Non-owning view. Construct from Buffer<T>. getRowPointer, getSubBlock, getNumRows, getNumCols. Trivially copyable. Add to jam_core.h includes.
**Validation:** Compiles. Zero allocation. No ownership. BLESSED L. NAMES.md compliant.

### Step 3: Grid → Buffer<Cell> + AbstractFIFO
**Scope:** `Source/terminal/logic/Grid.h`, `Grid.cpp`
**Action:** Rewrite Grid internals. Replace manual HeapBlock + Buffer struct with jam::Buffer<Cell> + lazy jam::Buffer<Grapheme>. Replace manual scroll-off ring with juce::AbstractFIFO. Dual-screen (2 Buffer<Cell> instances). All allocation sizes wired from `config.nexus.terminal.scrollbackLines` (default 10K) — no hardcoded margins. Public API unchanged. Dirty tracking stays on Grid. Video per-screen arrays size 2 (Map 0-based).
**Validation:** Compiles. Public API unchanged. AbstractFIFO for scroll-off. Buffer<Cell> for storage. Allocation sizes from config — no magic numbers. No manual ring arithmetic for scroll-off. BLESSED S (SSOT — one buffer type).

### Step 4: Arrangement Block<Cell> Overload
**Scope:** `jam_fonts/jam_font/glyph/jam_glyph_arrangement.h`, `.cpp`
**Action:** Add shape overload taking Block<Cell> array: `shape (const Block<Cell>* blocks, int numBlocks, const Font& font, int wrapColumns)`. Single-buffer Block overload delegates to array overload. Text font priority for emoji resolution (already done this session — verify).
**Validation:** Compiles. Shapes from Block<Cell> without Cells. Emoji resolution: text font first.

### Step 5: TextEditor → Buffer<Cell>
**Scope:** `jam_gui/text_editor/jam_text_editor.h`, `.cpp`
**Action:** Rewrite TextEditor storage. Replace Owner<Owner<Cells>> with 2 Buffer<Cell> (Normal + Alternate) + lazy Buffer<Grapheme>. Normal Buffer capacity wired from setScrollbackLines (config-driven, no hardcoded size). Ring management on Normal (head + count for FIFO). ContentView::paint constructs Block<Cell> array, calls arrangement.shape with Blocks. calc() sizes + auto-scrolls + repaints. CaretComponent forked (already done this session). setCaretPosition offsets by history depth internally. insertTextAtCaret writes Cell records to Buffer. Remove all Cells usage.
**Validation:** Compiles. No Cells. Buffer<Cell> storage. Block<Cell> rendering. Allocation from config. Shape in paint. TETRIS contract (setters call calc). BLESSED E (screen private). No dead API.

### Step 6: Screen → Live Buffer + Map 0-Based
**Scope:** `Source/terminal/rendering/Screen.h`, `Screen.cpp`
**Action:** Screen adds Live Buffer<Cell>. Map 0-based (normal=0, alternate=1). updateVisibleRow writes to Live. append transfers to Normal. Constructs Block array for TextEditor rendering: Normal mode = {Normal Block, Live Block}, Alternate = {Alternate Block}. Update all Map consumers (Video, State, Display).
**Validation:** Compiles. Map 0-based. Live separate from Normal. No arithmetic in Screen — TextEditor handles history offset. BLESSED E (encapsulation).

### Step 7: Display Integration
**Scope:** `Source/component/TerminalDisplay.h`, `.cpp`
**Action:** Update onVBlank: drain AbstractFIFO scroll-off → memcpy rows from Grid Buffer to Screen Normal. Copy dirty rows from Grid to Screen Live. setCaretPosition. resized() uses lastFont, getCellArea. No magic numbers. Remove stale array include.
**Validation:** Compiles. Lock-free drain via AbstractFIFO. No Cells::fromArray. No batch cap. BLESSED S (stateless Display — mediator only).

### Step 8: Delete Cells
**Scope:** `jam_tui/cell/jam_cells.h`, `jam_cells.cpp`, `jam_tui.h`
**Action:** Delete Cells class. Remove from jam_tui module includes. Verify zero remaining references.
**Validation:** Compiles. No Cells references anywhere. Buffer<Cell> is the sole cell storage type.

### Step 9: Dirty Tracking — Eliminate 64-Row Ceiling
**Scope:** Grid, TerminalDisplay
**Action:** Replace `atomic<uint64_t>` bitmask with multi-word dirty tracking. Remove `r < 64` ceiling.
**Validation:** Compiles. No row limit on dirty tracking.

### Step 10: Newline Signal — Video → Screen
**Scope:** Video, Grid, TerminalDisplay, Screen
**Action:** Persist newline/wrap distinction from Video through Grid to Screen. Additive parameter on Cell or Buffer metadata.
**Validation:** Compiles. Newline vs wrap distinguishable in Screen for reflow.

### Step 11: Docs + Cleanup
**Scope:** ARCHITECTURE.md, SPEC.md, inline doxygen
**Action:** Update docs to reflect Buffer/Block architecture. Delete RFC-text-editor.md. Delete PLAN-text-editor.md.
**Validation:** Docs match code. No stale references to Cells, Owner, old screen model.

## BLESSED Alignment

- **B (Bound):** Buffer owns memory. Block is non-owning view with explicit lifetime. Grid owns AbstractFIFO + Buffers. Clear RAII.
- **L (Lean):** Buffer<T> and Block<T> are small templates. Grid composes, doesn't god-object. 300/30/3 enforced.
- **E (Explicit):** AudioBuffer/AudioBlock API — proven names, proven semantics. No magic. Map 0-based. Text font priority explicit.
- **S (SSOT):** One Buffer type for all cell storage. One Block type for all views. Font for metrics. Map for screen indices.
- **S (Stateless):** TextEditor renders content. Buffer stores. Block views. No opinions.
- **E (Encapsulation):** screen private. Buffer flat — consumers add behavior. Grid composes, doesn't expose internals.
- **D (Deterministic):** Same content + font + viewport = same pixels. Buffer is contiguous. Block is deterministic view.

## Failure Prevention

1. **Buffer is AudioBuffer.** Battle-tested pattern. No improvisation.
2. **Block is AudioBlock.** Zero-cost view. No allocation on render path.
3. **Grid: composite.** AbstractFIFO for lock-free. Buffer for storage. Not inherited.
4. **3 screens, Live separate.** No arithmetic in consumer code.
5. **Map 0-based.** Video arrays size 2 naturally. No padding.
6. **Cells deleted.** One cell storage type (Buffer<Cell>). No dual representation.
7. **glyph::Graphics clears each push.** No scroll-delta memmove. No ghost artifacts.
8. **Text font first.** No emoji hijacking NF icons.
9. **No magic numbers.** No 512 batch cap. No hardcoded margins without rationale.
10. **ARCHITECT builds and runs git.**
