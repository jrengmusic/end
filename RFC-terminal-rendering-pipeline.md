# RFC — Terminal Rendering Pipeline
Date: 2026-05-07
Status: Ready for COUNSELOR handoff
Supersedes: RFC-logical-line-grid.md, PLAN-logical-line-grid.md (Layer 2 and Layer 3 replaced by TextEditor)

## Problem Statement

Three disconnected pieces need to become one pipeline:

1. **Glyph::Graphics clears and recomposites the entire renderTarget every frame** — even when nothing changed. WHELMED proves JUCE can render 2000+ lines with near-zero scroll cost by caching rendered output. TextEditor's Glyph::Graphics should do the same.

2. **Grid stores cells in a flat 2D buffer** — reflow on resize mutates cells, causing visual corruption while source data (LinkIds) remains intact. RFC-logical-line-grid designed the fix: logical lines as SSOT, visual rows as computed mapping, zero cell mutation on resize.

3. **Screen::render() is a stub** — Grid → Screen → TextEditor content pipeline is not wired. Screen::rebuildContent() exists but rebuilds ALL content as a flat string. No incremental updates.

4. **TextEditor's rendering pipeline is broken for non-ASCII** — two independent font systems that don't talk to each other:
   - JUCE's ShapedText shapes text using JUCE's system font lookup + JUCE's fallback chain, producing glyph IDs from whatever font JUCE chose.
   - `drawContent → drawGlyphs` receives those glyph IDs but renders via `jam::Typeface` which has a DIFFERENT fallback chain. Glyph IDs from JUCE's font are meaningless to jam's typeface.
   - Result: any codepoint not in the primary JUCE system font renders as "?" — even when jam::Typeface's fallback chain has the glyph.
   - Additionally, `juce::String`'s `const char*` construction path silently corrupts bytes > 127 in release builds. Text entering TextEditor through this path is destroyed before it ever reaches rendering.

These are one problem: a rendering pipeline from Grid content to cached pixels, driven by existing dirty signals, using jam's proven codepoint → Typeface → Atlas rendering path.

## Root Cause Analysis

### The proven pipeline (f385f70)

The old `Terminal::Renderer::Glyph` at commit f385f70 had a working rendering pipeline:

```
Grid::Cell (uint32_t codepoint)
  → buildCodepointSequence (uint32_t[] from Cell + Grapheme)
  → Typeface::shapeText (style, codepoints[], count)
  → GlyphRun { glyphs[], count, fontHandle }    ← correct fontHandle from fallback chain
  → emitShapedGlyphsToCache (uses shaped.fontHandle)
  → Packer::getOrRasterize (Key{glyphIndex, fontHandle, ...})
```

This pipeline worked for ALL Unicode: box drawing, emoji, CJK, NF icons, fullwidth chars. It was abandoned not because rendering was broken but because the architecture around it was wrong: manual layout, fighting JUCE Component hierarchy, text wrapping/reflow micro-management — BLESSED violations.

### What TextEditor broke

TextEditor was adopted for its architecture (wrapping, reflow, Component integration). But it severed the rendering pipeline by inserting `juce::String` + JUCE's ShapedText:

```
juce::String (UTF-8, asserts for > 127 via const char*)
  → JUCE ShapedText (shapes with JUCE's fonts, produces JUCE glyph IDs)
  → drawContent receives JUCE glyph IDs
  → drawGlyphs builds Key{glyphId, mainFont, ...}   ← ALWAYS mainFont, ignores fallback
  → Atlas rasterizes glyph ID from wrong font → .notdef → "?"
```

Two failures: storage corruption (`const char*` > 127) and glyph ID mismatch (JUCE's font vs jam's font).

### The fix

Replace JUCE's ShapedText with jam's own shaping/layout engine, reconnecting the proven codepoint → `Typeface::shapeText` → Atlas pipeline. TextEditor keeps its architectural role (wrapping, reflow, Component integration). jam owns the entire text pipeline: storage, shaping, layout, rendering.

## Architecture

```
READER THREAD                         MESSAGE THREAD
─────────────                         ──────────────
Parser → Grid::Writer                 
  writes cells to Line                
  markRowDirty(row) → atomic OR       
  state.setSnapshotDirty() → release  
                                      Display::onVBlank()
                                        consumeSnapshotDirty() ← acquire
                                        ScopedTryLock(resizeLock)
                                        screen.feedContent(grid, dirtyRows)
                                          ↓
                                        Grid Cell → resolve Color → Glyph::Pen
                                        build Glyph::Pens per dirty paragraph
                                        TextEditor::setText(pens) / setParagraph(i, pens)
                                          ↓
                                        Glyph::ShapedText consumes Pens
                                          shapeText(codepoints) per run
                                          positions: mono=fixed advance, proportional=natural
                                          line breaking: mono=column count, proportional=word-aware
                                          ↓
                                        JUCE schedules repaint(dirty clip)
                                          ↓
                                        TextHolderComponent::paint(g)
                                          clip = dirty region from JUCE
                                          ↓
                                        Glyph::Graphics::push(clip)
                                          persistent renderTarget
                                          clear only dirty strip
                                          ↓
                                        drawContent → composite only clip paragraphs
                                          from ShapedText glyph runs
                                          correct fontHandle per glyph (from Typeface fallback)
                                          ↓
                                        Glyph::Graphics::pop(g)
                                          blit renderTarget (mostly cached pixels)
```

Unidirectional. No backflow. UI never mutates Grid. Dirty tracking is Grid's existing 256-bit atomic bitmask. State signals via existing acquire/release. JUCE provides dirty clips. Glyph::Graphics trusts the clip as sole invalidation signal.

## New jam Types

### Glyph::Pen

The rich text unit. Carries codepoint + resolved visual attributes. Terminal-specific — WHELMED introduces its own type for proportional text later.

```cpp
struct Pen
{
    uint32_t     codepoint;    // Unicode scalar, 0 = empty
    juce::Colour fg;           // resolved foreground (4 bytes)
    juce::Colour bg;           // resolved background (4 bytes)
    uint8_t      style;        // SGR: BOLD, ITALIC, UNDERLINE, STRIKE, BLINK, INVERSE, DIM
    uint8_t      width;        // display columns (1 or 2)
    uint8_t      layout;       // WIDE_CONT, EMOJI, HAS_GRAPHEME, HYPERLINK
    uint8_t      reserved;     // pad to 16 bytes
};

static_assert (sizeof (Pen) == 16);
static_assert (std::is_trivially_copyable_v<Pen>);
```

Colors are resolved `juce::Colour` — final form for rendering. Resolution from `Terminal::Color` + Theme happens at the Grid → Screen boundary via a static helper in Glyph.

### Glyph::Grapheme

Combining marks sidecar. Indexed by position when Pen's layout flag `HAS_GRAPHEME` is set. Same pattern as current `Terminal::Grapheme`.

```cpp
struct Grapheme
{
    std::array<uint32_t, 7> extraCodepoints;
    uint8_t count;
};
```

### Glyph::Pens

Rich string type. Sequence of Pen objects + internal Grapheme sidecar. Replaces `juce::String` as TextEditor's text type.

```cpp
class Pens
{
public:
    // Construction
    // ... builders from Pen data, from CharPointer_UTF8 (convenience)

    int size() const noexcept;
    const Pen& operator[] (int index) const noexcept;

    const Grapheme* getGrapheme (int index) const noexcept;  // nullptr if no grapheme at index

private:
    juce::HeapBlock<Pen> pens;
    juce::HeapBlock<Grapheme> graphemes;   // sidecar, indexed by position
    int count { 0 };
};
```

Storage: `juce::HeapBlock` — established codebase pattern for trivially-copyable data. Same as Grid's cell storage and Glyph cache storage.

### Glyph::ShapedText

Layout + shaping engine. Replaces JUCE's ShapedText entirely. Consumes Pens, produces positioned glyph runs via `Typeface::shapeText`.

- **Input:** Pens + layout mode + wrap constraint
- **Shaping:** `Typeface::shapeText(style, codepoints, count)` → `GlyphRun{glyphs, fontHandle}` — correct fontHandle per glyph from jam's fallback chain
- **Positioning:**
  - Monospace: fixed advance = cell width. Character-level wrap at column count.
  - Proportional: natural advance from Typeface metrics. Word-aware wrap at pixel boundary.
- **Output:** Positioned glyph runs per line (glyphIndex, fontHandle, position, color, isEmoji). Consumed by `Glyph::Graphics` for compositing into renderTarget.

### TextEditor API

```cpp
void setText (const Glyph::Pens& pens);
void setParagraph (int index, const Glyph::Pens& pens);
void setText (juce::CharPointer_UTF8 text);   // convenience — creates Pens with default fg/bg
```

`setText(const Glyph::Pens&)` is the primary API. `CharPointer_UTF8` overload is convenience for test fixtures and non-terminal use — explicit encoding, no ambiguous `const char*`.

## Part 1: Glyph::Graphics Optimization

Scope: jam::TextEditor internals only. No changes to Screen, Grid, or END.

### 1.1 Persistent renderTarget

`push()` stops clearing the full renderTarget every frame. Content accumulates across frames. Clear only on:
- First frame (renderTarget invalid)
- Resize (dimensions changed)
- Full invalidation (font change, etc.)

### 1.2 Clip-aware push/pop

`push()` receives clip bounds from `g.getClipBounds()` (already passed today). Clears only the clip region in the renderTarget. `drawContent()` already culls paragraphs outside clip. Compositing writes only into the clip region.

```cpp
void Graphics::push (int viewportWidth, int viewportHeight,
                     int clipX, int clipY, int clipW, int clipH) noexcept
{
    // Resize renderTarget if dimensions changed (full clear)
    if (needsResize(viewportWidth, viewportHeight))
    {
        renderTarget = juce::Image (juce::Image::ARGB,
                                    viewportWidth, viewportHeight,
                                    true, juce::SoftwareImageType());
        // ... update frame dimensions
    }

    // Clear ONLY the dirty clip region
    renderTarget.clear (juce::Rectangle<int> (clipX, clipY, clipW, clipH));

    // Store clip for compositing bounds
    clipRegion = { clipX, clipY, clipW, clipH };
}
```

### 1.3 Scroll-aware memmove

Detect that clip offset changed from previous frame (scroll delta). Shift renderTarget pixels by delta. Clear only the newly exposed strip. Composite only the strip's content.

```cpp
// Inside push(), after resize check:
const int scrollDelta { clipY - lastClipY };

if (scrollDelta != 0 and renderTarget.isValid())
{
    juce::Image::BitmapData data (renderTarget, juce::Image::BitmapData::readWrite);

    if (scrollDelta > 0)  // scrolled down — content moves up
    {
        // shift pixels up by scrollDelta rows
        const int rowsToShift { frameHeight - scrollDelta };
        std::memmove (data.getLinePointer (0),
                      data.getLinePointer (scrollDelta),
                      static_cast<size_t> (rowsToShift * data.lineStride));
        // clear exposed strip at bottom
        std::memset (data.getLinePointer (rowsToShift), 0,
                     static_cast<size_t> (scrollDelta * data.lineStride));
    }
    else  // scrolled up — content moves down
    {
        const int absDelta { -scrollDelta };
        const int rowsToShift { frameHeight - absDelta };
        std::memmove (data.getLinePointer (absDelta),
                      data.getLinePointer (0),
                      static_cast<size_t> (rowsToShift * data.lineStride));
        // clear exposed strip at top
        std::memset (data.getLinePointer (0), 0,
                     static_cast<size_t> (absDelta * data.lineStride));
    }
}

lastClipY = clipY;
```

### 1.4 Rendering cost after optimization

| Scenario | Before | After |
|---|---|---|
| Idle (no content change) | Clear 14.7MB + composite 4000 glyphs + upload | paint() not called (if setBufferedToImage) OR no-op push/pop |
| New line at bottom | Clear 14.7MB + composite 4000 glyphs | memmove + clear 1 line strip + composite ~80 glyphs |
| Scroll (read history) | Clear 14.7MB + composite 4000 glyphs | memmove + clear 1 strip + composite ~80 glyphs |
| Resize | Full recomposite | Full recomposite (correct — dimensions changed) |
| Font change | Full recomposite | Full recomposite (correct — all pixels stale) |

## Part 2: Grid Logical-Line Architecture

From RFC-logical-line-grid.md Layer 1. Layers 2 and 3 are replaced by TextEditor.

### 2.1 Grid::Line

Variable-length sequence of cells between hard line breaks. The content primitive.

```cpp
struct Line
{
    juce::HeapBlock<Cell> cells;
    juce::HeapBlock<Grapheme> graphemes;
    juce::HeapBlock<uint16_t> linkIds;
    int length { 0 };
    int capacity { 0 };

    void ensureCapacity (int needed) noexcept;
    void reset() noexcept;
};
```

### 2.2 Grid::Lines

Ring buffer of Line objects. The document. Manages scrollback lifecycle.

```cpp
struct Lines
{
    juce::HeapBlock<Line> lines;
    int capacity { 0 };
    int mask { 0 };
    int head { 0 };
    int count { 0 };
    int scrollbackVisualRows { 0 };

    Line& at (int index) noexcept;
    const Line& at (int index) const noexcept;
    Line& advance() noexcept;
};
```

### 2.3 Grid::Row

Visual row → logical line mapping.

```cpp
struct Row
{
    int lineIndex { 0 };
    int cellOffset { 0 };
};
```

### 2.4 Resize — zero cell mutation

```cpp
void Grid::resize (int newCols, int newVisibleRows)
{
    const juce::ScopedLock lock (resizeLock);
    // Save cursor logical position
    // Recompute visibleRows from Lines + newCols
    // Derive cursor visual position
    // ZERO cell mutation. Lines untouched.
    markAllDirty();
}
```

Replaces ~450 lines of GridReflowHelpers.cpp with ~30 lines of visibleRows recomputation.

### 2.5 Write path (Parser hot path)

```cpp
Cell* Grid::directRowPtr (int visibleRow) noexcept
{
    const auto& row { visibleRows[visibleRow] };
    auto& line { bufferForScreen().at (row.lineIndex) };
    line.ensureCapacity (row.cellOffset + cols);
    return line.cells.get() + row.cellOffset;
}
```

Zero API change for Parser. One indirection per row change, zero per character.

### 2.6 Dirty tracking

256-bit atomic bitmask over visible rows — unchanged. Operates on visual row indices. `markRowDirty(r)` atomically ORs bit `r`. `consumeDirtyRows()` atomically exchanges and returns. Orthogonal to storage model.

### 2.7 What's deleted from old RFC

- **Layer 2 (jam::Wrap)** — TextEditor with ShapedText handles character-level wrapping for monospace. No custom wrap function.
- **Layer 3 (Terminal::Layout::computeRowAdvances)** — ShapedText handles glyph positioning via Typeface::shapeText. No custom layout function.
- **proportional rendering** — supported by ShapedText (word-aware wrapping, natural advance widths). WHELMED input type deferred.
- **RowState::isWrapped** — eliminated. Soft-wrap implicit from `row.cellOffset + numCols < line.length`.

## Part 3: Screen Content Feed

Screen is a TextEditor subclass. It feeds content. It does not render.

### 3.1 Rename render() → feedContent()

Screen translates Grid cell storage to Glyph::Pens. That's its only job.

### 3.2 Color resolution at the boundary

Grid cells store `Terminal::Color` (4 bytes, palette/rgb/default mode). Screen resolves to `juce::Colour` using the active Theme via a static helper in Glyph. This happens at the Grid → Screen boundary, once per cell, producing Pen with resolved colors.

```cpp
// Static helper in Glyph
static juce::Colour resolveColor (const Terminal::Color& color, const Theme& theme) noexcept;
```

### 3.3 Incremental content feed

Only dirty rows are translated and fed. TextEditor receives targeted updates. JUCE schedules repaint for only the affected paragraphs.

```cpp
void Screen::feedContent (const Grid& grid, const DirtyRows& dirty)
{
    const int lineCount { grid.getLineCount() };

    for (int row { 0 }; row < lineCount; ++row)
    {
        if (dirty.isSet (row))
        {
            // Resolve Grid Cell → Glyph::Pen (color resolution)
            // Build Glyph::Pens for this paragraph
            // setParagraph (row, pens)
        }
    }
}
```

### 3.4 What Screen does NOT do

- Does not know about Glyph::Graphics, renderTarget, compositing, clips
- Does not manage scroll (TextEditor Viewport handles it)
- Does not track dirty state (Grid's atomic bitmask, consumed by Display)
- Does not resize Grid (unidirectional — Grid resize comes through Parser/State channel)

## Rendering Integration

### What changes

| Layer | Before | After |
|---|---|---|
| Storage | `juce::String` (UTF-8, asserts > 127) | `Glyph::Pens` (Pen array, HeapBlock) |
| Layout | JUCE ShapedText | `Glyph::ShapedText` — jam Typeface metrics |
| Shaping | JUCE ShapedText (JUCE's fonts) | `Typeface::shapeText` (jam's fallback chain) |
| TextEditor API | `setText(const juce::String&)` | `setText(const Glyph::Pens&)` |

### What stays

| Layer | Status |
|---|---|
| Compositing | `Glyph::Graphics` → renderTarget → SIMD pixel ops — unchanged |
| Output | `g.drawImageAt()` blit via `juce::Graphics` — unchanged |
| Atlas | `Glyph::Atlas` (CPU `juce::Image`, mono R8 + emoji ARGB) — unchanged |
| Packer | `Glyph::Packer` — unchanged |
| Context integration | Works with any `juce::LowLevelGraphicsContext` — unchanged |

## BLESSED Compliance

- **B (Bound)** — Line owns cells. Lines owns Lines. renderTarget owned by Graphics. Pens owns Pen array + Grapheme sidecar. Clear lifecycle at each layer.
- **L (Lean)** — Grid reflow: ~450 lines → ~30. Glyph::Graphics: three additions to push/pop, no new classes. ShapedText replaces JUCE's ShapedText, no parallel system. One font pipeline, not two.
- **E (Explicit)** — Row explicitly maps visual → logical. push() explicitly receives clip. No ambiguous `const char*` — Pen carries uint32_t codepoints, resolved juce::Colour. Encoding is never guessed.
- **S (SSOT)** — `Typeface::shapeText` is the SINGLE authority for codepoint → glyph resolution. No parallel font system. Grid Lines = content truth. renderTarget = pixel cache (downstream, never authoritative).
- **S (Stateless)** — Row mapping is pure function of Lines + numCols. push/pop is pure function of clip rect. ShapedText is pure function of Pens + layout mode + wrap constraint.
- **E (Encap)** — Grid doesn't know about pixels. Screen doesn't know about compositing. TextEditor doesn't know about Grid. Glyph::Graphics doesn't know about terminal content. Typeface owns fallback chain — callers don't pick fonts.
- **D (Deterministic)** — Same Lines + same numCols = same Row mapping. Same Pens + same layout mode = same ShapedText output. Same clip + same content = same renderTarget pixels.

## Open Questions

None. All decisions made by ARCHITECT:
- renderTarget is persistent pixel cache, sole invalidation from JUCE clip (ARCHITECT decision)
- Grid logical lines from RFC-logical-line-grid Layer 1 (ARCHITECT decision)
- TextEditor replaces Layers 2 and 3 (ARCHITECT decision)
- Screen feeds content, does not render (ARCHITECT decision)
- Dirty tracking stays in Grid's existing 256-bit atomic bitmask (ARCHITECT decision)
- UI never mutates Grid, resize is rendering domain, SIGWINCH is unidirectional CLI command (ARCHITECT decision)
- Scrollback = rendered image on Screen, not Grid data structure (ARCHITECT decision)
- JUCE's ShapedText replaced by jam's Glyph::ShapedText (ARCHITECT decision)
- Glyph::Pen is terminal-specific, WHELMED gets own type later (ARCHITECT decision)
- Pen stores resolved juce::Colour, resolution at Grid→Screen boundary (ARCHITECT decision)
- Glyph::Pens stores HeapBlock<Pen> + HeapBlock<Grapheme> sidecar (ARCHITECT decision)
- ShapedText: monospace = fixed advance + character-level wrap; proportional = natural advance + word-aware wrap (ARCHITECT decision)
- Both layout modes in scope (ARCHITECT decision)

## Handoff Notes

- **Execution order:** Part 1 (Glyph::Graphics optimization) first — self-contained inside jam::TextEditor, no END changes. New types (Pen, Pens, ShapedText) second — jam library additions. Part 2 (Grid logical lines) third — structural Grid migration. Part 3 (Screen content feed) fourth — wires the pipeline.
- **Part 1 and new types are jam changes.** Part 2 and 3 are END changes.
- **RFC-logical-line-grid.md and PLAN-logical-line-grid.md are superseded.** Layer 1 content absorbed here. Layers 2-3 replaced by ShapedText.
- **Grid::Writer API preserved.** Parser hot path unchanged — directRowPtr returns pointer into Line's cell array. Zero regression risk for Parser.
- **Thread model unchanged.** resizeLock serializes resize against writes. snapshotDirty acquire/release pairs. dirtyRows atomic bitmask. All existing patterns.
- **drawGlyphs has exactly one caller** (TextEditor drawContent). Safe to modify — no other callers in either repo.
- **Typeface::shapeText already exists and works.** The proven pipeline from f385f70. No new shaping logic — just reconnecting it through ShapedText.
- **Typeface fallback infrastructure exists but was never wired to rendering.** userFallbackFonts populated by addFallbackFont, codepoint→glyph helpers exist (macOS: glyphForCodepoint, FreeType: FT_Get_Char_Index) — all in metrics path only. ShapedText wires them into the rendering path via shapeText.
- **setBufferedToImage(true) on TextHolderComponent** is complementary but optional. The persistent renderTarget achieves the same cache effect at our layer.
