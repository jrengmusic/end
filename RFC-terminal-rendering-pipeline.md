# RFC — Terminal Rendering Pipeline
Date: 2026-05-07
Status: Ready for COUNSELOR handoff
Supersedes: RFC-logical-line-grid.md, PLAN-logical-line-grid.md (Layer 2 and Layer 3 replaced by TextEditor)

## Problem Statement

Three disconnected pieces need to become one pipeline:

1. **Glyph::Graphics clears and recomposites the entire renderTarget every frame** — even when nothing changed. WHELMED proves JUCE can render 2000+ lines with near-zero scroll cost by caching rendered output. TextEditor's Glyph::Graphics should do the same.

2. **Grid stores cells in a flat 2D buffer** — reflow on resize mutates cells, causing visual corruption while source data (LinkIds) remains intact. RFC-logical-line-grid designed the fix: logical lines as SSOT, visual rows as computed mapping, zero cell mutation on resize.

3. **Screen::render() is a stub** — Grid → Screen → TextEditor content pipeline is not wired. Screen::rebuildContent() exists but rebuilds ALL content as a flat string. No incremental updates.

These are one problem: a rendering pipeline from Grid content to cached pixels, driven by existing dirty signals.

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
                                        Grid Lines → text → TextEditor::setText()
                                        (incremental: only dirty paragraphs)
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
                                          ↓
                                        Glyph::Graphics::pop(g)
                                          blit renderTarget (mostly cached pixels)
```

Unidirectional. No backflow. UI never mutates Grid. Dirty tracking is Grid's existing 256-bit atomic bitmask. State signals via existing acquire/release. JUCE provides dirty clips. Glyph::Graphics trusts the clip as sole invalidation signal.

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

- **Layer 2 (jam::Wrap)** — TextEditor with `withAllowBreakingInsideWord()` handles character-level wrapping. No custom wrap function for monospace.
- **Layer 3 (Terminal::Layout::computeRowAdvances)** — TextEditor handles glyph positioning via JUCE ShapedText. No custom layout function.
- **proportional rendering** — deferred. TextEditor + ShapedText handles it when needed.
- **RowState::isWrapped** — eliminated. Soft-wrap implicit from `row.cellOffset + numCols < line.length`.

## Part 3: Screen Content Feed

Screen is a TextEditor subclass. It feeds content. It does not render.

### 3.1 Rename render() → feedContent()

Screen translates Grid cell storage to TextEditor text. That's its only job.

### 3.2 Incremental content feed

Current `rebuildContent()` rebuilds ALL content as a flat string. New approach:

```cpp
void Screen::feedContent (const Grid& grid, const DirtyRows& dirty)
{
    const int lineCount { grid.getLineCount() };
    const int cols { grid.getCols() };

    for (int row { 0 }; row < lineCount; ++row)
    {
        if (dirty.isSet (row))
        {
            // Translate Grid Line cells → text for this paragraph
            // Update only this paragraph in TextEditor
        }
    }
}
```

Only dirty rows are translated and fed. TextEditor receives targeted updates. JUCE schedules repaint for only the affected paragraphs.

### 3.3 Cell → text translation

Per-line: walk cells, skip wide continuations, emit codepoints as UTF-8. Resolve colors via `resolveColour()` → apply as TextEditor attributed text (colour per run). This is what `rebuildContent()` already does, scoped to dirty lines only.

### 3.4 What Screen does NOT do

- Does not know about Glyph::Graphics, renderTarget, compositing, clips
- Does not manage scroll (TextEditor Viewport handles it)
- Does not track dirty state (Grid's atomic bitmask, consumed by Display)
- Does not resize Grid (unidirectional — Grid resize comes through Parser/State channel)

## BLESSED Compliance

- **B (Bound)** — Line owns cells. Lines owns Lines. renderTarget owned by Graphics. Clear lifecycle at each layer.
- **L (Lean)** — Grid reflow: ~450 lines → ~30. Glyph::Graphics: three additions to push/pop, no new classes. Screen: feedContent replaces rebuildContent, same logic scoped to dirty rows.
- **E (Explicit)** — Row explicitly maps visual → logical. push() explicitly receives clip. No magic dirty detection — atomic bitmask consumed explicitly.
- **S (SSOT)** — Grid Lines = content truth. renderTarget = pixel cache (downstream, never authoritative). No shadow state.
- **S (Stateless)** — Row mapping is pure function of Lines + numCols. push/pop is pure function of clip rect. No state between frames except the persistent renderTarget (cache, not truth).
- **E (Encap)** — Grid doesn't know about pixels. Screen doesn't know about compositing. TextEditor doesn't know about Grid. Glyph::Graphics doesn't know about terminal content. Each layer communicates through its API only.
- **D (Deterministic)** — Same Lines + same numCols = same Row mapping. Same clip + same content = same renderTarget pixels.

## Open Questions

None. All decisions made by ARCHITECT:
- renderTarget is persistent pixel cache, sole invalidation from JUCE clip (ARCHITECT decision)
- Grid logical lines from RFC-logical-line-grid Layer 1 (ARCHITECT decision)
- TextEditor replaces Layers 2 and 3 (ARCHITECT decision)
- Screen feeds content, does not render (ARCHITECT decision)
- Dirty tracking stays in Grid's existing 256-bit atomic bitmask (ARCHITECT decision)
- UI never mutates Grid, resize is rendering domain, SIGWINCH is unidirectional CLI command (ARCHITECT decision)
- Scrollback = rendered image on Screen, not Grid data structure (ARCHITECT decision)

## Handoff Notes

- **Execution order:** Part 1 (Glyph::Graphics optimization) first — it's self-contained inside jam::TextEditor, no END changes. Part 2 (Grid logical lines) second — structural Grid migration. Part 3 (Screen content feed) third — wires the pipeline.
- **Part 1 is a jam change.** Part 2 and 3 are END changes.
- **RFC-logical-line-grid.md and PLAN-logical-line-grid.md are superseded.** Layer 1 content absorbed here. Layers 2-3 replaced by TextEditor.
- **Grid::Writer API preserved.** Parser hot path unchanged — directRowPtr returns pointer into Line's cell array. Zero regression risk for Parser.
- **Thread model unchanged.** resizeLock serializes resize against writes. snapshotDirty acquire/release pairs. dirtyRows atomic bitmask. All existing patterns.
- **Incremental feed (Part 3) requires TextEditor paragraph-level update API.** Current setText() replaces all content. May need TextEditor enhancement to update individual paragraphs without full relayout. If not feasible, full setText() with dirty-scoped content rebuild is the fallback — still better than current because Glyph::Graphics only recomposites the dirty clip.
- **setBufferedToImage(true) on TextHolderComponent** is complementary but optional. The persistent renderTarget achieves the same cache effect at our layer. If JUCE's buffered image provides additional benefit (skip paint() entirely), it can be added later.
