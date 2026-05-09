# RFC — Grid Redesign as AudioBuffer Analog
Date: 2026-05-09
Status: Ready for COUNSELOR handoff

## Problem Statement

Screen (the terminal renderer) has been through multiple failed attempts. Scrollback and reflow keep breaking. The root cause is architectural — Grid conflates transport (FIFO) with storage, ownership is wrong, and the data flow is not truly unidirectional. ARCHITECT wants to rebuild from the ground up, starting with Grid as a properly designed live buffer modeled 1:1 after juce::AudioBuffer.

## Research Summary

### Comparative Parser Analysis (END vs Ghostty vs Alacritty)

All three implement the Paul Williams VT100 DFA state machine (vt100.net/emu/dec_ansi_parser). Parser fidelity is not the problem.

| Aspect | END | Ghostty | Alacritty |
|--------|-----|---------|-----------|
| State machine | Paul Williams DFA, 15 states, DispatchTable 7680 bytes | Paul Williams DFA, comptime table | Paul Williams DFA, vte crate |
| PTY read buffer | 64 KB (Linux PTY kernel buffer) | 1 KB | 1 MB |
| Ground fast path | ASCII 0x20–0x7E only | SIMD UTF-8 decode | memchr(0x1B) + str::from_utf8 |
| UTF-8 across chunks | Manual accumulator (char[5]), correct | Bjoern Hoehrmann DFA | partial_utf8[4] |
| Parser → Grid | Command FIFO (AbstractFifo), silent drop | Direct writes (mutex) | Direct writes (FairMutex) |
| Threading | Lock-free (APVTS pattern) | Mutex on renderer_state | FairMutex on Term |

Key finding: END's 64 KB read buffer is well-reasoned (matches kernel buffer, no mutex to amortize). Parser DFA is faithful. The divergence is in how parsed output reaches the renderer.

### AudioBuffer Pattern (juce_AudioSampleBuffer.h)

Single flat `HeapBlock<char, true>` with two-region layout:
```
[ Type** channel_ptrs | padding | sample data ch0 | sample data ch1 | ... ]
```
- One allocation, SIMD-aligned (samples rounded to multiple of 4)
- `getReadPointer(ch)` / `getWritePointer(ch)` — direct pointer access
- `isClear` optimization flag — set by `clear()`, cleared by `getWritePointer()`
- `setSize()` with keep-content and avoid-realloc options
- Not thread-safe by design. noexcept on all non-allocating paths.
- Owned by the host (DAW), passed by reference to AudioProcessor

### Existing Types

- `jam::Cell` — 16 bytes, trivially copyable: `codepoint(4) + style(1) + layout(1) + width(1) + reserved(1) + fg(4) + bg(4)`. Already SIMD-sized (128-bit = one SSE register).
- `jam::Cells` — 1D sequence of Cell records with optional Grapheme sidecar. Used for text shaping and rendering. Not SIMD-aligned. Analogous to FFT bins — the processed, ready-to-render representation.
- Current `Terminal::Grid` — SPSC FIFO (`AbstractFifo` + `HeapBlock<Command>`), not a cell buffer. No `HeapBlock<Cell>` in `Source/terminal/`.

## Principles and Rationale

### Architecture: 1:1 Audio Plugin Analogy

The terminal architecture maps exactly to the JUCE audio plugin pattern:

| Audio Plugin | Terminal | Role |
|---|---|---|
| AudioProcessor | Processor (Parser) | Transforms input → output buffer |
| AudioBuffer | Grid | The live buffer — read/write in-place |
| APVTS | State | Parameters via atomics → ValueTree |
| PluginEditor | Display | Orchestrator — reads buffer + state, tells children |
| Spectrum visualizer | Screen | Renders from processed data (jam::Cells) |
| float samples | jam::Cell (16 bytes) | The unit of data |
| FFT bins | jam::Cells | Processed, ready-to-render representation |

### MVC is Fractal (Meta, Not Three God Objects)

```
READER THREAD
  C: Processor
    Parser reads/writes Grid in-place (processBlock)
    Parser writes State atomics

MESSAGE THREAD
  M: State (ValueTree, flushed from atomics)
  V+C: Display (orchestrator)
    Reads Grid, listens to State
    Tells Screen what to store, render, reflow
  V: Screen (renderer, visualizer)
    Stores jam::Cells, renders, reflows when told
    Dumb — does what Display says
```

Display is V from the top level. C from Screen's perspective. Each layer has its own V and C — nested, not flat. Unidirectional at every layer. No object reaches up.

### Grid is AudioBuffer, Not Storage

Grid is a live frame buffer — the current visible terminal content. Parser reads/writes it in-place on the reader thread (like processBlock writes AudioBuffer). Display reads it on the message thread.

Grid does NOT store scrollback. Scrollback is a VISUAL concern — Screen's responsibility. Screen uses JUCE's real Viewport for scrolling, not a fake text array. This is JUCE's advantage — real components, real GPU-accelerated viewport, real smooth scrolling.

Scrollback is not in the VT spec. VT100/VT220/ECMA-48 defines only the visible screen buffer. Grid as live-only buffer is 100% VT-faithful.

### Scroll-Off Capture

When Parser scrolls Grid (LineFeed at bottom), rows shift up. Row 0 is overwritten. Screen needs that row for visual scrollback.

Grid uses ring buffer indexing internally — no memmove. Head pointer advances on scroll. Grid capacity = `visibleRows + scrollMargin`. The margin holds scroll-off rows until Display consumes them. Display calls `consumeScrolledRows()` to drain scroll-off rows into Screen as `jam::Cells`.

This is functional headroom — same as AudioBuffer's `+32` byte alignment headroom in `setSize()`. Grid manages the ring internally. Parser and Display don't know about the ring — they see row indices. Encapsulated.

### BLESSED Mapping

- **B (Bound):** Grid owns its memory (HeapBlock), RAII. Session owns Grid. Parser receives Grid& — does not own it. Clear ownership chain: Session → Grid, Session → Processor → Parser(Grid&).
- **L (Lean):** One class, one allocation, flat memory. No god objects.
- **E (Explicit):** `getWritePointer(row)` / `getReadPointer(row)` — no magic. SIMD alignment explicit.
- **S (SSOT):** Grid is the single source of live cell data. Dirty tracking lives where mutation happens.
- **S (Stateless):** Grid is a dumb buffer. No opinions, no history. Parser tells it what to write. Display reads it.
- **E (Encapsulation):** Parser doesn't know about dirty tracking. Display doesn't know about internal dual-buffer or ring indexing. Grid manages itself. `setScreen()` swaps internally.
- **D (Deterministic):** Same Parser input → same Grid content. Always.

## Scaffold

### Grid Class Design — AudioBuffer-Equivalent API

```cpp
class Grid
{
public:
    Grid() = default;

    //==========================================================================
    /** @name Size — mirrors AudioBuffer::setSize / getNumChannels / getNumSamples */
    ///@{

    /** Allocates or resizes the grid.
     *  Single HeapBlock<char, true> allocation with row-pointer table + cell data,
     *  SIMD-aligned (AudioBuffer allocation pattern).
     *
     *  @param numRows           Visible row count (analogous to numChannels).
     *  @param numCols           Column count per row (analogous to numSamples).
     *  @param keepExistingContent  Preserve existing cell data up to the smaller of old/new dims.
     *  @param clearExtraSpace      Zero-init any newly exposed cells.
     *  @param avoidReallocating    Reuse existing allocation if large enough.
     */
    void setSize (int numRows, int numCols,
                  bool keepExistingContent = false,
                  bool clearExtraSpace = false,
                  bool avoidReallocating = false) noexcept;

    int getNumRows() const noexcept;
    int getNumCols() const noexcept;

    ///@}

    //==========================================================================
    /** @name Pointer access — mirrors AudioBuffer::getReadPointer / getWritePointer */
    ///@{

    /** Returns a read-only pointer to the first Cell in the given row. */
    const Cell* getReadPointer (int row) const noexcept;

    /** Returns a read-only pointer to a specific Cell position. */
    const Cell* getReadPointer (int row, int col) const noexcept;

    /** Returns a writable pointer to the first Cell in the given row.
     *  Marks the row dirty (clears isClear). */
    Cell* getWritePointer (int row) noexcept;

    /** Returns a writable pointer to a specific Cell position.
     *  Marks the row dirty (clears isClear). */
    Cell* getWritePointer (int row, int col) noexcept;

    ///@}

    //==========================================================================
    /** @name Element access — mirrors AudioBuffer::getSample / setSample */
    ///@{

    /** Returns the Cell at the given position. */
    const Cell& getCell (int row, int col) const noexcept;

    /** Writes a Cell at the given position. Marks the row dirty. */
    void setCell (int row, int col, const Cell& cell) noexcept;

    ///@}

    //==========================================================================
    /** @name Clear — mirrors AudioBuffer::clear / hasBeenCleared */
    ///@{

    /** Clears all rows. Sets isClear flag. */
    void clear() noexcept;

    /** Clears an entire row. */
    void clear (int row) noexcept;

    /** Clears a range of cells within a row. */
    void clear (int row, int startCol, int numCols) noexcept;

    /** Returns true if the entire grid has been cleared and no writes have occurred since. */
    bool hasBeenCleared() const noexcept;

    ///@}

    //==========================================================================
    /** @name Dual screen (normal / alternate) — terminal-specific */
    ///@{

    /** Swaps the active buffer. Grid manages two internal buffers. */
    void setScreen (bool alternate) noexcept;

    /** Returns true if the alternate screen buffer is active. */
    bool isAlternateScreen() const noexcept;

    ///@}

    //==========================================================================
    /** @name Dirty tracking (reader → message sync) — terminal-specific */
    ///@{

    /** Atomically exchanges the dirty-row bitmask, returning the previous value.
     *  Called by Display on the message thread to discover which rows changed. */
    uint64_t consumeDirtyRows() noexcept;

    ///@}

    //==========================================================================
    /** @name Scroll-off capture — terminal-specific */
    ///@{

    /** Returns the number of unconsumed scroll-off rows available for reading. */
    int getNumScrolledRows() const noexcept;

    /** Returns a read-only pointer to a scroll-off row (0 = oldest unconsumed).
     *  Display calls this to drain scroll-off rows into Screen as jam::Cells. */
    const Cell* getScrolledReadPointer (int index) const noexcept;

    /** Marks N scroll-off rows as consumed. Called by Display after draining. */
    void consumeScrolledRows (int count) noexcept;

    ///@}

private:
    // Two internal buffers (normal + alternate)
    // Each: HeapBlock<char, true> with row-pointer table + cell data
    // SIMD-aligned (Cell count per row rounded to multiple of 4 for 128-bit ops)
    // Single allocation per buffer (AudioBuffer pattern)
    //
    // Ring buffer indexing — head advances on scroll, no memmove
    // Capacity: visibleRows + scrollMargin
    //
    // Active pointer swapped by setScreen()
    //
    // isClear: bool — optimization gate (AudioBuffer pattern)
    // dirtyRows: std::atomic<uint64_t> — per-buffer bitmask
};
```

### Data Flow

```
READER THREAD:
  PTY → 64 KB read → Parser::process(bytes, length)
    Parser decodes UTF-8 → codepoints (ground fast path, future SIMD)
    Parser constructs Cell (codepoint + pen)
    Parser calls grid.getWritePointer(row) → writes Cell in-place
    Parser calls grid.clear(row, col, n) for erases
    Parser advances ring head on scroll (row 0 becomes scroll-off)
    Parser writes State atomics for scalar params
    Grid marks dirty rows internally

MESSAGE THREAD (VBlank):
  Display:
    Reads grid.consumeDirtyRows() → knows what changed
    Reads grid.getNumScrolledRows() → drains scroll-off via getScrolledReadPointer()
    Feeds scroll-off rows to Screen as jam::Cells (scrollback storage)
    Reads grid.getReadPointer(row) for changed visible rows
    Reads State (ValueTree) for cursor, modes, title, etc.
    Tells Screen to store dirty rows as jam::Cells
    Tells Screen to render
  Screen:
    Stores jam::Cells (the FFT bins — processed, ready-to-render)
    Maintains visual scrollback via JUCE Viewport
    Renders via Glyph pipeline
```

### Ownership Chain

```
Session
  ├── Grid              (value member — the AudioBuffer)
  ├── Processor          (unique_ptr)
  │     └── Parser       (unique_ptr, receives Grid& and State&)
  └── State              (value member — the APVTS)
```

### TODO Sequence

1. Extend Parser `processGroundChunk()` for full UTF-8 → codepoints (not just ASCII 0x20–0x7E)
2. Design and implement Grid class (AudioBuffer pattern, HeapBlock<char>, SIMD-aligned, dual screen, dirty tracking, scroll-off ring)
3. Move Grid ownership from Processor to Session
4. Rewire Parser to receive Grid& — read/write in-place (eliminate Command FIFO)
5. Rewire Display to read Grid directly + consumeDirtyRows() + consumeScrolledRows()
6. Design Screen as JUCE Viewport with jam::Cells storage (the visualizer)
7. Wire Display → Screen data flow (dirty rows + scroll-off → jam::Cells → render)
8. SIMD UTF-8 optimization in Parser ground fast path (debt)

## BLESSED Compliance Checklist

- [x] Bounds — Grid owns memory (RAII), Session owns Grid, Parser borrows via reference
- [x] Lean — one class, one allocation, AudioBuffer pattern
- [x] Explicit — getWritePointer/getReadPointer API, SIMD alignment explicit, no magic
- [x] SSOT — Grid is single source of live cells, dirty tracking at mutation site
- [x] Stateless — Grid is dumb buffer, no history, no opinions
- [x] Encapsulation — internal dual-buffer swap, ring indexing, dirty tracking all invisible to callers
- [x] Deterministic — same input → same content, always

## Open Questions

None. All design decisions resolved during ORACLE session.

## Handoff Notes

- Parser is correct and faithful (Paul Williams DFA). No parser rewrite needed — only extend the ground fast path for UTF-8.
- Grid is the AudioBuffer, not storage. Screen owns visual storage (jam::Cells). This distinction is load-bearing — do not conflate them.
- Grid API mirrors AudioBuffer 1:1 — same naming pattern (getReadPointer, getWritePointer, getCell, setCell, clear, setSize, hasBeenCleared). Any JUCE developer reads Grid and already knows how it works. NAMES.md compliant: verbs for actions, nouns for things, semantic over literal, consistent with JUCE ecosystem.
- Scroll-off capture uses ring buffer indexing internally (no memmove). Grid capacity = visibleRows + scrollMargin. Display drains scroll-off rows via consumeScrolledRows() → feeds to Screen as jam::Cells. Encapsulated — Parser and Display don't know about the ring.
- Scrollback is NOT in VT spec. It is a visual concern, implemented via real JUCE Viewport — not a fake text array. This is END's genuine architectural advantage over other terminals.
- The MVC is fractal: Display is V+C, Screen is V. Not three god objects. Each layer has its own V and C.
- 64 KB PTY read buffer is correct — matches Linux kernel buffer, no mutex to amortize (lock-free architecture).
- `jam::Cells` is analogous to FFT bins — the processed, ready-to-render representation. Grid is the live audio. Display transforms one into the other.
- Dirty-row tracking is internal to Grid (like `isClear` in AudioBuffer). Parser doesn't know about it. Display consumes it via atomic exchange.
- Normal/alternate screen is internal to Grid — one `setScreen()` call, Grid swaps internally. Caller doesn't manage two buffers.
- jam::Cell is 16 bytes = 128-bit = one SSE register. Grid uses HeapBlock<char> with explicit SIMD alignment (AudioBuffer pattern) to enable future SIMD operations on Cell data.
