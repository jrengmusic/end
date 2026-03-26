# PLAN-WHELMED.md — Whelmed Markdown Viewer: Architecture, Threading, Rendering

**Project:** END
**Date:** 2026-03-27
**Author:** COUNSELOR
**Status:** Active — Phase 1 in progress

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Whelmed is END's built-in markdown viewer. Opens `.md` files in a pane alongside the terminal. Three-phase rendering strategy from simple to optimized, each phase self-contained.

| Phase | Objective | Status |
|-------|-----------|--------|
| 1 | CPU rendering — JUCE components, async parsing, ship this | In progress |
| 2 | GPU rendering — Whelmed::Grid, atlas pipeline | Future |
| 3 | Hybrid — forked JUCE components with our GPU/CPU backend | Far future |

---

## Current State (Sprint 126)

### What works
- Whelmed pane opens `.md` files via open-file hint mode or `open_markdown` action
- Terminal stays alive when whelmed opens (hide/show, no destroy/create)
- `Whelmed::Block` renders markdown text via `juce::TextLayout`
- `Whelmed::CodeBlock` renders code fences via `juce::CodeEditorComponent` with tokenisation
- All fonts, colors, heading sizes, padding configurable via `whelmed.lua`
- Token colors for code syntax highlighting configurable
- File handler map in `end.lua` (`.md` -> `"whelmed"`, user-overridable)
- Paginated hint labels with spacebar cycling

### What doesn't work yet
- Parsing blocks message thread (synchronous `rebuildBlocks`)
- Mermaid rendering (TODO stub)
- Table component (TODO stub)
- No text selection/copy in markdown blocks (TextLayout has no interaction)
- No `setBufferedToImage` caching

### What needs to change for Phase 1 completion
1. Async parsing pipeline (`Whelmed::Parser` background thread)
2. `juce::TextEditor` replaces `juce::TextLayout` for text blocks (selection, copy)
3. `setBufferedToImage(true)` on `Whelmed::Component`
4. Braille spinner during parse
5. Mermaid component (wired to existing `jreng::Mermaid::Parser`)
6. Table component

---

## Threading Model

Identical pattern to Terminal's reader thread.

```
Terminal:                           Whelmed:
Reader Thread (PTY)                 Parser Thread (file)
  |                                   |
  v                                   v
VT Parser -> Grid cells             Markdown Parser -> ParsedDocument
  |                                   |
  atomic fence                        atomic fence
  |                                   |
  v                                   v
Message Thread                      Message Thread
  Screen::buildSnapshot()              Component::buildBlocks()
  Component::paint()                   Component::paint()
```

### Whelmed::Parser (background thread)

Analogous to Terminal's reader thread. Owns parsing + inline span detection + rasterization. `FontConfig` set before rasterization begins.

**Input:** Raw file bytes (read on background thread).
**Output:** `ParsedDocument` — three flat `juce::HeapBlock` arrays.

```cpp
struct InlineSpan
{
    int startOffset;      // relative to block content
    int endOffset;
    uint8_t style;        // bitmask: Bold | Italic | Code | Link
    int uriOffset;        // into ParsedDocument::text (0 if not a link)
    int uriLength;
};

struct Block
{
    BlockType type;       // Markdown, CodeFence, Mermaid, Table
    int contentOffset;    // into ParsedDocument::text
    int contentLength;
    int languageOffset;   // into ParsedDocument::text (code fence language tag)
    int languageLength;
    int spanOffset;       // index into ParsedDocument::spans
    int spanCount;
    int level;            // heading level 1-6, 0 for non-headings
};

struct ParsedDocument
{
    juce::HeapBlock<char> text;           // all content concatenated
    int textSize { 0 };
    int textCapacity { 0 };

    juce::HeapBlock<Block> blocks;        // block metadata
    int blockCount { 0 };
    int blockCapacity { 0 };

    juce::HeapBlock<InlineSpan> spans;    // inline style spans
    int spanCount { 0 };
    int spanCapacity { 0 };
};
```

All three structs are trivially copyable. `static_assert` enforced.

### Pre-allocation

File size drives initial allocation. No realloc during parse.

```
textCapacity   = fileSize                    // worst case: all content
blockCapacity  = fileSize / 50              // ~1 block per 50 bytes average
spanCapacity   = fileSize / 20              // ~1 span per 20 bytes average
```

Parser writes sequentially. If capacity is exceeded (malformed input with extreme nesting), the parse fails gracefully — truncate at capacity, log warning.

### Synchronization

`std::atomic<int> completedBlockCount` with `memory_order_release` on writer, `memory_order_acquire` on reader. Same pattern as `State::StringSlot` generation counter.

No `AbstractFIFO`. No mutex. No lock.

The message thread polls `completedBlockCount` on VBlank or timer. When new blocks are available, it creates components for them. Single-shot: parse completes, spinner hides, all components created, `setBufferedToImage(true)` caches the result.

### Progress Overlay

Braille spinner in `MessageOverlay` during parse. Shows while parsing active. Hides on completion.

---

## Phase 1: CPU Rendering — JUCE Components

**Goal:** Ship a working markdown viewer with text selection, code highlighting, and acceptable performance. Pure JUCE components. No custom rendering.

### Component Stack

```
Whelmed::Component (PaneComponent, setBufferedToImage=true)
  |
  +-- juce::Viewport
       |
       +-- content (juce::Component)
            |
            +-- juce::TextEditor (read-only, multi-line) -- markdown text
            +-- Whelmed::CodeBlock (juce::CodeEditorComponent) -- code fence
            +-- juce::TextEditor -- more markdown text
            +-- Whelmed::MermaidBlock -- mermaid diagram
            +-- juce::TextEditor -- more markdown text
            +-- Whelmed::TableBlock -- table
```

### Block Types

| BlockType | Component | Rendering |
|-----------|-----------|-----------|
| Markdown | `juce::TextEditor` (read-only) | Styled text, selection, copy |
| CodeFence | `Whelmed::CodeBlock` | `juce::CodeEditorComponent`, tokenised |
| Mermaid | `Whelmed::MermaidBlock` | `jreng::Mermaid::Parser` -> SVG -> rendered |
| Table | `Whelmed::TableBlock` | Custom component, grid layout |

### TextEditor for Markdown Blocks

Replace current `Block` (TextLayout) with `juce::TextEditor`:
- `setMultiLine(true)`
- `setReadOnly(true)`
- `setScrollbarsShown(false)` — viewport handles scroll
- `setCaretVisible(false)`
- Styled text via `setFont()` + `setColour()` per range, or `insertTextAtCaret` with attribute runs
- Text selection + copy for free
- Height: `getTextHeight()` after content insertion

### Consecutive Block Merging

Consecutive `Markdown` blocks merge into one `TextEditor`. Reduces component count.

A 3000-line doc with ~10 code fences = ~11 `TextEditor` + ~10 `CodeBlock` = ~21 components. Not 50+.

### setBufferedToImage

`Whelmed::Component::setBufferedToImage(true)` after all blocks are created. JUCE caches the entire component tree as one image. Scroll moves the cached image. No per-block repainting. Repaint only on:
- Initial load
- Resize (relayout blocks)
- Config reload (font/color change)

### Config

All visual properties driven by `whelmed.lua`. Font families, sizes, colors, heading sizes, padding, token colors, code fence background — all configurable. Already implemented in Sprint 126.

---

## Phase 1 Implementation Steps

### Step 1.0 — ParsedDocument data structures

Define `ParsedDocument`, `Block`, `InlineSpan` as trivially copyable flat structs in `jreng_markdown` module. `static_assert` trivial copyability. URI storage as offset+length into text buffer.

**Files:**
- `modules/jreng_markdown/markdown/jreng_markdown_types.h` — new structs

**Validate:** Module builds. `static_assert` passes.

### Step 1.1 — Parser produces ParsedDocument

Rewrite `Parser::getBlocks()` to fill a `ParsedDocument`. Pre-allocate from input size. Inline span detection moves into the parser (currently in `toAttributedString`). Link URI extraction stores offset+length into text buffer.

**Files:**
- `modules/jreng_markdown/markdown/jreng_markdown_parser.h` — updated API
- `modules/jreng_markdown/markdown/jreng_markdown_parser.cpp` — rewrite

**Validate:** Parser produces identical block structure. Content round-trips.

### Step 1.2 — Whelmed::Parser background thread

New class. Owns a `juce::Thread`. Reads file, calls parser, fills `ParsedDocument`. Signals completion via `std::atomic<int>`.

**Files:**
- New: `Source/whelmed/Parser.h`
- New: `Source/whelmed/Parser.cpp`

**Validate:** Background parsing completes. Main thread not blocked.

### Step 1.3 — Whelmed::State owns ParsedDocument

`State` gains `ParsedDocument` member. Parser writes to it. Component reads after atomic fence. `consumeDirty()` pattern.

**Files:**
- `Source/whelmed/State.h` — `ParsedDocument` member, atomic fence
- `Source/whelmed/State.cpp` — updated

**Validate:** State owns document data. Component reads after fence.

### Step 1.4 — Braille spinner

`MessageOverlay` shows braille spinner during parse. Component shows spinner on `openFile`, hides on parse completion.

**Files:**
- `Source/whelmed/Component.cpp` — spinner lifecycle

**Validate:** Spinner visible during parse, hidden after.

### Step 1.5 — TextEditor replaces TextLayout blocks

Replace `Whelmed::Block` (TextLayout) with `juce::TextEditor` (read-only). Consecutive markdown blocks merge into one TextEditor. Styled text via attribute ranges from `ParsedDocument::spans`. Selection and copy work.

**Files:**
- `Source/whelmed/Block.h` — rewrite or replace
- `Source/whelmed/Block.cpp` — rewrite or replace
- `Source/whelmed/Component.cpp` — block creation from ParsedDocument

**Validate:** Text renders identically. Selection and copy work.

### Step 1.6 — setBufferedToImage

Add `setBufferedToImage(true)` after block creation. Verify scroll performance with cached image.

**Files:**
- `Source/whelmed/Component.cpp`

**Validate:** Smooth scroll. No per-block repainting on scroll.

### Step 1.7 — MermaidBlock component

Wraps `jreng::Mermaid::Parser` (already scaffolded). Converts mermaid code to SVG, renders as image or via `Mermaid::Graphic::extractFromSVG`.

**Files:**
- New: `Source/whelmed/MermaidBlock.h`
- New: `Source/whelmed/MermaidBlock.cpp`
- `Source/whelmed/Component.cpp` — wire mermaid blocks

**Validate:** Mermaid diagrams render.

### Step 1.8 — TableBlock component

Custom component for markdown tables. Grid layout, styled cells, column alignment.

**Files:**
- New: `Source/whelmed/TableBlock.h`
- New: `Source/whelmed/TableBlock.cpp`
- `Source/whelmed/Component.cpp` — wire table blocks

**Validate:** Tables render with correct alignment.

---

## Phase 2: GPU Rendering — Whelmed::Grid (Future)

**Goal:** Same visual output as Phase 1, rendered through the terminal's GL pipeline. Proportional text in an atlas-backed instanced quad renderer.

### Architecture

```
Whelmed::Grid (proportional Cell array)
      |
      v
Whelmed::Screen::buildSnapshot() (MESSAGE THREAD)
      |
      v
Render::Snapshot (Quad[] + Background[])
      |
      v
SnapshotBuffer::write() -> read()
      |
      v
GLContext::drawQuads() (GL THREAD)
```

### Key Differences from Terminal Grid

| Aspect | Terminal::Grid | Whelmed::Grid |
|--------|---------------|---------------|
| Cell width | Fixed (monospace) | Variable (proportional) |
| Line breaking | Fixed columns | Word-wrap at viewport width |
| Content | Shell output (streaming) | Static document (parse once) |
| Styling | ANSI SGR attributes | Markdown inline styles |
| Scrollback | Ring buffer | Full document |

### What can be reused as-is
- `jreng::Typeface` — already handles proportional fonts via HarfBuzz
- `jreng::Glyph::Atlas` — caches proportional glyphs (keyed by glyph index)
- `jreng::Glyph::GLContext` — `drawGlyphs()` accepts positioned glyph arrays
- `jreng::TextLayout` — HarfBuzz shaping, libunibreak word-wrap
- `SnapshotBuffer` — lock-free double-buffered snapshot exchange
- `Render::Quad`, `Render::Background` — geometry PODs

### What must be built
- `Whelmed::Grid` — stores pre-laid-out glyph positions, styled runs, line breaks
- `Whelmed::Screen` — builds snapshots from Grid, handles selection hit-testing
- Proportional text selection — hit-test variable-width glyphs
- Cursor rendering for document navigation

---

## Phase 3: Hybrid — Forked JUCE + Our Backend (Far Future)

**Goal:** JUCE component interaction model (selection, copy, caret) with our atlas-backed rendering underneath. Best of both worlds.

### Approach

Fork `juce::TextEditor` and `juce::CodeEditorComponent`:
- Keep: input handling, selection state, caret management, clipboard, scrolling
- Replace: `drawContent()` internals — route through `Glyph::GraphicsContext` (CPU) or `Glyph::GLContext` (GPU)
- Same surface API as Phase 1 — `TextEditor` with `setReadOnly(true)`, `CodeEditorComponent`

### Why fork is necessary
- `TextEditor::drawContent()` is `private`, not `virtual` (JUCE 8)
- Internal text model (`TextEditorStorage`, `ShapedText`) is entirely `private`
- Selection rendering is tightly coupled to the text drawing loop
- No surgical injection point — composition or subclassing cannot replace rendering

### What the fork preserves
- All keyboard/mouse interaction
- Text selection + copy
- Caret positioning
- Scroll management

### What the fork replaces
- `ShapedText` rendering -> `jreng::TextLayout` + `GraphicsContext`/`GLContext`
- `CoreGraphics` per-glyph compositing -> atlas blit (CPU) or instanced quads (GPU)

---

## Module Dependencies

```
jreng_core       (Mailbox, SnapshotBuffer)
     ^
     |
jreng_graphics   (Font, Atlas, Typeface, TextLayout, GraphicsContext)
     ^
     |
jreng_opengl     (GLContext, GLRenderer, shaders)
     ^
     |
jreng_markdown   (Parser, ParsedDocument, Block, InlineSpan, Mermaid, Table)
     ^
     |
END app          (Whelmed::Component, Whelmed::Parser, Whelmed::State,
                  Terminal::Component, Terminal::Screen, MainComponent)
```

---

## Rules for Execution

1. **Always invoke @Pathfinder first** — before any code change, discover existing patterns
2. **Validate each step before proceeding** — ARCHITECT builds and tests
3. **Never assume, never decide** — discrepancy between plan and code, discuss with ARCHITECT
4. **No new types without ARCHITECT approval** — check if existing types can be reused
5. **Follow all contracts** — JRENG-CODING-STANDARD, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO
6. **Incremental execution** — one step at a time, ARCHITECT confirms before next step
7. **ARCHITECT runs all git commands** — agents prepare changes only
8. **When uncertain — STOP and ASK**
