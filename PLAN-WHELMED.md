# PLAN-WHELMED.md — Whelmed Markdown Viewer: Architecture, Threading, Rendering

**Project:** END
**Date:** 2026-03-28
**Author:** COUNSELOR
**Status:** Active — Phase 1 consolidation

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Whelmed is END's built-in markdown viewer. Opens `.md` files in a pane alongside the terminal. Three-phase rendering strategy from simple to optimized, each phase self-contained.

| Phase | Objective | Status |
|-------|-----------|--------|
| 1 | CPU rendering — unified Screen, single paint(), ship this | In progress |
| 2 | GPU rendering — jreng::TextLayout, atlas pipeline | Future |
| 3 | Hybrid — forked JUCE components with our GPU/CPU backend | Far future |

---

## Current Architecture (Sprint 130)

### Rendering Model

Blocks are NOT juce::Components. They are data objects that own renderable primitives (juce::TextLayout, juce::Path, etc.) and know how to paint themselves into a Graphics context.

`Whelmed::Screen` is a single juce::Component inside a juce::Viewport. It owns all blocks and renders them in one `paint()` call with viewport culling.

```
Whelmed::Component (PaneComponent)
  |
  +-- juce::Viewport
  |    |
  |    +-- Whelmed::Screen (single Component, single paint())
  |         |
  |         +-- BlockEntry[] (data: Block*, type, y, height)
  |              |
  |              +-- TextBlock (juce::TextLayout + optional background)
  |              +-- MermaidBlock (juce::Path + text primitives)
  |              +-- TableBlock (TextLayout cache + grid layout)
  |
  +-- LoaderOverlay (progress bar, terminal font/colours)
```

### Block Types

| BlockType | Render class | Rendering |
|-----------|-------------|-----------|
| Markdown | TextBlock | juce::TextLayout, styled spans from config |
| CodeFence | TextBlock | Monospace juce::TextLayout + background rect. Tokenization via free function (planned) |
| Mermaid | MermaidBlock | juce::Path primitives + text from SVG parse. JS engine async |
| Table | TableBlock | Custom paint, TextLayout cache per cell, column distribution |
| ThematicBreak | TextBlock | Empty line break (transparent newline, body font height) |

### Threading Model

Identical pattern to Terminal's reader thread.

```
Terminal:                           Whelmed:
Reader Thread (PTY)                 Parser Thread (style resolution)
  |                                   |
  v                                   v
VT Parser -> Grid cells             appendBlock() per block
  |                                   |
  atomic fence                        atomic fence
  |                                   |
  v                                   v
Message Thread                      Message Thread
  State timer flush (60/120Hz)        State timer flush (60/120Hz)
  Screen::buildSnapshot()              valueTreePropertyChanged -> screen.build()
  Component::paint()                   Screen::paint()
```

### Loading Flow

1. `openFile()`: synchronous markdown parse, move document to State
2. `screen.load(doc, viewportHeight)`: create all block entries, build blocks that fill viewport immediately, `updateLayout()`
3. Return initial batch count to Component
4. `State::setInitialBlockCount(batch)` — timer starts flushing from batch offset
5. `Parser` thread resolves styles for remaining blocks, calls `appendBlock()` per block
6. `State` timer flushes one block per tick via ValueTree `blockCount` property
7. `Component::valueTreePropertyChanged` calls `screen.build(index, doc)` for each new block
8. `screen.build()` is idempotent — skips already-built blocks
9. When all blocks built: LoaderOverlay hides, `setBufferedToImage(true)`

First screen of content is instant. Remaining blocks build progressively.

### State (SSOT)

`Whelmed::State` owns:
- `ParsedDocument` (moved from synchronous parse)
- ValueTree properties: filePath, displayName, scrollOffset, blockCount, parseComplete, pendingPrefix, totalBlocks
- Atomic counters for Parser thread communication (completedBlockCount, parseComplete)
- Timer for flushing (60Hz idle, 120Hz active) — identical to Terminal::State

Component reads/writes state exclusively through ValueTree properties. No stray member variables.

### Config

- `Whelmed::Config` at `Source/config/WhelmedConfig.h/.cpp` — Lua-backed, `jreng::Context<Whelmed::Config>` pattern
- Screen reads config directly via `Whelmed::Config::getContext()` — no DocConfig intermediary
- Parser copies needed values to private members at construction (thread safety)
- LoaderOverlay uses terminal `Config` for font and status bar colours

### Block Spacing

- Block gap between all blocks: `bodyFontSize * 1.2f`
- Thematic breaks (`---`, `***`, `___`): rendered as empty TextBlock with one newline height

---

## What Works

- Screen: unified single-paint rendering with viewport culling
- TextBlock: juce::TextLayout, styled spans, optional background for code fences
- CodeBlock: merged into TextBlock (monospace font + background)
- MermaidBlock: SVG path/text rendering from JS engine parse result
- TableBlock: custom paint, TextLayout cache, column distribution
- LoaderOverlay: progress bar with terminal font/status bar colours
- First-screen batch loading: blocks filling viewport built instantly
- Incremental building: remaining blocks via State timer
- Vim navigation: j/k/gg/G with configurable keybindings
- setBufferedToImage caching after all blocks built
- Config: all visual properties from whelmed.lua

---

## What Needs Work

### P0 — Code block tokenization

CodeFence blocks render as plain monospace text. No syntax highlighting.

Plan: free function `tokenizeCode(code, language)` -> `juce::AttributedString` with coloured spans. Uses JUCE's `CodeTokeniser` + `CodeDocument::Iterator` standalone (no CodeEditorComponent). Token type int maps to Whelmed::Config token colour keys.

File: `Source/whelmed/Tokenizer.h/.cpp` (next to TextBlock)

Languages: cpp/c/h/cc/cxx (CPlusPlusCodeTokeniser), lua (LuaTokeniser), xml/html/svg (XmlTokeniser). Unknown languages fall back to plain code colour.

### P0 — Mermaid rendering improvements

MermaidBlock works but needs significant work:
- SVG parse result quality varies — some diagrams render incorrectly
- Text positioning/sizing needs tuning
- Colour scheme not wired to config (hardcoded in SVG parse)
- Complex diagram types (sequence, state, class) may not parse correctly
- Placeholder height (200px) used before async JS engine returns result
- No error state rendering when parse fails

### P1 — Text selection and copy

Currently no text interaction. Plan (from ARCHITECT's scaffold):
- `Pos { block, index }` for caret/selection
- Hit testing via `TextLayout::Line` glyph positions
- Caret positioning from glyph x coordinates
- Selection rendering: fill rects per affected line
- Keyboard navigation: arrow keys, shift-extend
- Copy: `SystemClipboard::copyTextToClipboard`
- ~300-600 LOC for solid basic behavior

### P1 — Table selection and copy (restore)

TableBlock had mouse drag selection + Cmd/Ctrl+C copy as TSV. Removed during refactor (no longer juce::Component). Needs reimplementation through Screen's mouse/key handling.

### P2 — Config hot reload for Whelmed

`applyConfig()` rebuilds all blocks via `screen.load()`. Works but rebuilds everything. Could be optimized to only re-style without re-parsing.

### P2 — Scroll position preservation

Scroll offset stored in ValueTree but not restored on config reload or window resize.

---

## Phase 2: GPU Rendering — jreng::TextLayout (Future)

**Goal:** Same visual output as Phase 1, rendered through the terminal's GL pipeline. Proportional text in an atlas-backed instanced quad renderer.

The current Block architecture already aligns with Phase 2:
- Blocks are data + render methods, not Components
- Screen owns all blocks, single paint() call
- TextBlock stores juce::AttributedString source — can be replaced with jreng::TextLayout
- Block::paint(g, area) interface unchanged — just swap Graphics backend

### What can be reused as-is
- `jreng::Typeface` — proportional fonts via HarfBuzz
- `jreng::Glyph::Atlas` — caches proportional glyphs
- `jreng::Glyph::GLContext` — drawGlyphs() accepts positioned glyph arrays
- `jreng::TextLayout` — HarfBuzz shaping, libunibreak word-wrap (identical surface API to juce::TextLayout)
- `SnapshotBuffer` — lock-free double-buffered snapshot exchange

### What must be built
- Replace juce::TextLayout with jreng::TextLayout in TextBlock
- Whelmed::Screen GPU render path (paintGL)
- Proportional text selection hit-testing

---

## Phase 3: Hybrid — Forked JUCE + Our Backend (Far Future)

Fork `juce::TextEditor` and `juce::CodeEditorComponent`:
- Keep: input handling, selection state, caret management, clipboard
- Replace: drawContent() internals — route through our GPU/CPU backend
- Same surface API as Phase 1

---

## Module Dependencies

```
jreng_core       (Owner, Context, Mailbox)
     ^
     |
jreng_graphics   (Font, Atlas, Typeface, TextLayout, GraphicsContext)
     ^
     |
jreng_opengl     (GLContext, GLRenderer, shaders)
     ^
     |
jreng_markdown   (Parser, ParsedDocument, Block, InlineSpan, Mermaid)
     ^
     |
END app          (Whelmed::Component, Whelmed::Screen, Whelmed::State,
                  Whelmed::Parser, Terminal::Component, MainComponent)
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
9. **State is SSOT** — all mutable state goes through State/ValueTree, no stray variables
10. **No layer violations** — Component writes to State, Rendering reads from State, no direct references
