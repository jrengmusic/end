# PLAN-WHELMED.md — WHELMED Integration into END

**Project:** END
**Date:** 2026-03-26
**Author:** COUNSELOR
**Status:** Draft — awaiting ARCHITECT approval + Plan 4 completion

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

**Dependency:** Plan 4 (GPU/CPU runtime switching) must be complete before Step 5.5.

---

## Overview

Integrate WHELMED markdown/mermaid rendering into END as a pane-compatible component. `Whelmed::Component` is a peer of `Terminal::Component` — identical lifecycle, identical renderer switching, identical config hot-reload path.

**Surface API:** `juce::AttributedString` → `jreng::TextLayout` → duck-typed renderer. The component never knows which renderer is active.

**What ports from WHELMED scaffold (`~/Documents/Poems/dev/whelmed/`):**
- `markdown::Parse` — 3-stage parser (blocks → units → inline spans → `AttributedString`)
- `markdown::parseTables()` — GFM table parser (data extraction)
- `mermaid::extractBlocks()` — fence extraction
- `MermaidSVGParser` — SVG string → `juce::Path`/`juce::Graphics` primitives (scaffolded by BRAINSTORMER against real mermaid.js output)
- `mermaid::Parser` — singleton `WebBrowserComponent` + mermaid.js → SVG string

**What is replaced by END's pipeline:**
- WHELMED's `GLString`, `GLGraphics::drawText`, `GLAtlas`, `GLComponent` block stacking → `jreng::TextLayout::draw<Renderer>`

---

## Architecture

```
Config (end.lua: markdown = { ... })
        |
        v
MainComponent::applyConfig()
        |
        v
    resolveRenderer()  — same SSOT path as terminals
        |
        v
    for each PaneComponent:
        pane->switchRenderer (type)
        pane->applyConfig()
        |
        v
Whelmed::Component                  Terminal::Component
    |                                   |
    v                                   v
Document (file, blocks, dirty)      Session (Grid, State, TTY)
    |                                   |
    v                                   v
TextLayout::createLayout()          Screen<Renderer>::buildSnapshot()
    |                                   |
    v                                   v
TextLayout::draw<Renderer>()        Renderer::drawQuads / drawBackgrounds
```

### Component Hierarchy

```
jreng::GLComponent (modules/jreng_opengl)
    ^
    |
PaneComponent (Source/component/)  — pure virtual base
    ^                    ^
    |                    |
Terminal::Component   Whelmed::Component
```

### Module Dependency

```
jreng_core
    ^
    |
jreng_graphics   (Typeface, Atlas, TextLayout, GraphicsTextRenderer)
    ^
    |
jreng_opengl     (GLComponent, GLRenderer, GLTextRenderer)
    ^
    |
END app
    ├── Source/component/    (PaneComponent, Terminal::Component, Whelmed::Component, Panes, Tabs)
    ├── Source/whelmed/      (markdown parser, mermaid parser, SVG parser, Document)
    └── Source/config/       (Config: markdown table)
```

---

## Design Decisions (ARCHITECT approved)

1. **`PaneComponent` pure virtual base.** Both `Terminal::Component` and `Whelmed::Component` inherit from it. `Panes` owns `Owner<PaneComponent>`. No `dynamic_cast`. MANIFESTO-compliant (Explicit Encapsulation — communicate via API, not type inspection).

2. **Whelmed owns its own Typeface.** Separate font pipeline — proportional families, no ASCII fast path. Not shared with terminal's monospace Typeface. Each Whelmed::Component instance owns its own `jreng::Typeface`.

3. **Typeface proportional mode via constructor flag.** Not a separate class. One boolean gates the ASCII fast path bypass. Lean — minimal change, zero impact on terminal path.

4. **Atlas already supports multi-face multi-size.** `Glyph::Key` has `{glyphIndex, fontFace*, fontSize, span}`. Different Typeface instances produce different `fontFace*` pointers → distinct cache entries. No structural change needed.

5. **Config under `markdown = { ... }` in `end.lua`.** Follows existing Config pattern exactly — `Config::Key` members, `initKeys()` calls, `default_end.lua` placeholders.

6. **Creation triggers via `LinkManager::dispatch()`.** Single hook point — both mouse click and keyboard hint-label converge here. Branch on `.md` extension to create Whelmed pane instead of writing to PTY.

7. **Mermaid via `juce_gui_extra` + `WebBrowserComponent`.** Adds dependency. Deferred to last step — can ship markdown rendering without mermaid.

8. **`jreng::TextLayout` is the rendering surface.** `createLayout (AttributedString, maxWidth, font)` → `draw<Renderer>()`. Already proportional-capable — uses per-glyph `xAdvance` from HarfBuzz. Untested with proportional fonts — Step 5.2 validates.

---

## PLAN 5: WHELMED Integration

| Step | Objective | Depends on |
|------|-----------|------------|
| 5.0 | Config keys — `markdown` table | — |
| 5.1 | Port WHELMED semantic layer | — |
| 5.2 | Typeface proportional mode | — |
| 5.3 | `PaneComponent` pure virtual base | Plan 4 complete |
| 5.4 | Document model | 5.1 |
| 5.5 | `Whelmed::Component` | 5.0, 5.2, 5.3, 5.4 |
| 5.6 | Panes generalization | 5.3, 5.5 |
| 5.7 | Creation triggers | 5.5, 5.6 |
| 5.8 | Mermaid integration | 5.5 |

Steps 5.0, 5.1, 5.2 have no dependencies and can proceed before Plan 4 completes.

---

### Step 5.0 — Config keys for `markdown` table

Add `markdown` config table to `end.lua`. Follows existing pattern exactly.

**Config keys:**

```lua
markdown = {
    font_family   = "Georgia",       -- proportional body font
    font_size     = 14.0,            -- base body size in points
    code_family   = "JetBrains Mono",-- code block font (monospace)
    code_size     = 12.0,            -- code block size
    h1_size       = 28.0,            -- header sizes
    h2_size       = 24.0,
    h3_size       = 20.0,
    h4_size       = 18.0,
    h5_size       = 16.0,
    h6_size       = 14.0,
    line_height   = 1.4,             -- line height multiplier
},
```

**Files:**
- `Source/config/Config.h` — add `Key::markdownFontFamily`, `Key::markdownFontSize`, `Key::markdownCodeFamily`, `Key::markdownCodeSize`, `Key::markdownH1Size` through `Key::markdownH6Size`, `Key::markdownLineHeight`
- `Source/config/Config.cpp` — add `addKey()` calls in `initKeys()` with defaults and ranges
- `Source/config/default_end.lua` — add `markdown = { ... }` block with placeholders

**Pattern:** Identical to existing `font`, `gpu`, `colours` tables. The main `load()` loop handles scalar fields automatically — no changes to `load()` needed.

**Validate:** END builds. Config loads `markdown.*` keys. `Config::getContext()->getString (Config::Key::markdownFontFamily)` returns `"Georgia"`.

---

### Step 5.1 — Port WHELMED semantic layer

Copy parsing code from WHELMED scaffold into `Source/whelmed/`. Pure data transforms — zero rendering dependency.

**Port:**
- `markdown::Parse` — block detection, line classification, inline span tokenization, `toAttributedString()`
- `markdown::parseTables()` — GFM table data extraction
- `mermaid::extractBlocks()` — fence extraction
- `MermaidSVGParser` — SVG → `SVGPrimitive` / `SVGTextPrimitive` (flat lists for `juce::Graphics` rendering)

**Files:**
- `Source/whelmed/markdown/Parse.h` — block/unit/span types + parse functions
- `Source/whelmed/markdown/Parse.cpp` — implementation
- `Source/whelmed/markdown/Table.h` — table parser
- `Source/whelmed/markdown/Table.cpp` — implementation
- `Source/whelmed/mermaid/Extract.h` — fence extraction
- `Source/whelmed/mermaid/Extract.cpp` — implementation
- `Source/whelmed/mermaid/SVGParser.h` — `MermaidSVGParser::parse()`
- `Source/whelmed/mermaid/SVGParser.cpp` — implementation

**Adaptation during port:**
- Namespace: `Whelmed::Markdown`, `Whelmed::Mermaid` (follows END namespace patterns)
- Naming: rename to match NAMING-CONVENTION (nouns for types, verbs for functions)
- `toAttributedString()` — reads font families and sizes from `Config::Key::markdown*` instead of hardcoded values
- Code style: JRENG-CODING-STANDARD (braces on new line, `not`/`and`/`or`, brace initialization, no early returns, `.at()` for containers)
- Remove any WHELMED-specific dependencies (`jreng_opengl` types, `GLString`, etc.)

**Validate:** END builds. `Whelmed::Markdown::Parse::getBlocks (testString)` returns correct block types. `MermaidSVGParser::parse (testSVG)` returns populated `MermaidParseResult`. No rendering — pure data.

---

### Step 5.2 — Typeface proportional mode

Add proportional shaping mode to `jreng::Typeface`. One boolean flag — bypass ASCII fast path when set. Zero impact on terminal rendering path.

**Changes to `jreng::Typeface`:**

1. **Constructor** — add `bool proportional = false` parameter. Store as member.
2. **`shapeText()`** — change fast path guard:
   ```
   Before: if (count == 1 and codepoints[0] < 128) return shapeASCII (...)
   After:  if (not proportional and count == 1 and codepoints[0] < 128) return shapeASCII (...)
   ```
   One line change. When proportional, single ASCII codepoints route through HarfBuzz — get natural per-glyph advance instead of `max_advance`.

**No other changes needed:**
- `calcMetrics()` — unchanged. `logicalCellW` will be the max advance, which is fine. TextLayout uses per-glyph `xAdvance` from shaping, not cell width.
- Font loading — unchanged. Family name is already a constructor parameter. Pass `"Georgia"` instead of `"JetBrains Mono"` and the same resolution chain works.
- Atlas keying — unchanged. `Glyph::Key` already discriminates by `fontFace*` + `fontSize`.

**Files:**
- `modules/jreng_graphics/fonts/jreng_typeface.h` — add `bool proportional` member + constructor parameter
- `modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp` — one-line fast path guard change

**Validate:** Construct `Typeface (registry, "Georgia", 14.0f, AtlasSize::standard, true)`. Call `shapeText (Style::regular, codepoints, 1)` with ASCII 'i'. Verify `xAdvance` differs from 'M' advance (proportional). Construct terminal Typeface — verify ASCII fast path still active, `xAdvance` identical for all ASCII (monospace).

---

### Step 5.3 — `PaneComponent` pure virtual base

Extract shared interface from `Terminal::Component` into `PaneComponent`. Both `Terminal::Component` and `Whelmed::Component` inherit from it.

**`PaneComponent` class:**

```cpp
// Source/component/PaneComponent.h
namespace Terminal
{
    class PaneComponent : public jreng::GLComponent
    {
    public:
        virtual ~PaneComponent() = default;

        virtual void switchRenderer (RendererType type) = 0;
        virtual void applyConfig() noexcept = 0;

        std::function<void()> onRepaintNeeded;
    };
}
```

**Design rationale:**
- Inherits `jreng::GLComponent` — both pane types need GL lifecycle hooks (`glContextCreated`, `glContextClosing`, `renderGL`) and `paint()` for CPU path. `GLComponent` already provides these as virtual no-ops.
- `switchRenderer` — called by `MainComponent::applyConfig()` via Tabs/Panes propagation. Both pane types implement identically-structured variant emplacement.
- `applyConfig` — called after config hot-reload. Each pane type reads its own config keys.
- `onRepaintNeeded` — callback wired by Panes/Tabs/MainComponent. Both pane types fire it after rendering dirty content.

**Changes to `Terminal::Component`:**
- Inherit from `PaneComponent` instead of `jreng::GLComponent` directly
- Move `onRepaintNeeded` member to `PaneComponent` (remove from `Terminal::Component`)
- Add `override` to `switchRenderer` and `applyConfig`
- No behavioral changes

**Validate:** END builds. Terminal rendering identical. `Terminal::Component` is-a `PaneComponent` is-a `GLComponent`. All existing call sites compile.

---

### Step 5.4 — Document model

File-backed equivalent of `Terminal::Session`. Owns parsed blocks, file state, dirty tracking. One job: manage document state.

**`Whelmed::Document` class:**

```cpp
// Source/whelmed/Document.h
namespace Whelmed
{
    class Document
    {
    public:
        explicit Document (const juce::File& file);

        void reload();
        bool consumeDirty() noexcept;

        const Markdown::Blocks& getBlocks() const noexcept;
        const juce::File& getFile() const noexcept;

    private:
        juce::File file;
        Markdown::Blocks blocks;
        std::atomic<bool> dirty { true };
    };
}
```

**Responsibilities:**
- Load file → parse via `Markdown::Parse::getBlocks()` → store blocks
- Track dirty flag (set on `reload()`, consumed by render loop)
- Mermaid blocks: store raw fence content. SVG conversion is deferred to rendering step (async via `WebBrowserComponent`)

**What Document does NOT do:**
- No rendering (Lean, Explicit Encapsulation)
- No config awareness (receives what it needs via parameters)
- No PTY, no grid, no terminal state

**Files:**
- `Source/whelmed/Document.h`
- `Source/whelmed/Document.cpp`

**Validate:** Construct `Document (testFile)`. `getBlocks()` returns parsed blocks. `consumeDirty()` returns `true` once, then `false`.

---

### Step 5.5 — `Whelmed::Component`

Mirrors `Terminal::Component` lifecycle. `ScreenVariant` for GPU/CPU. VBlank render loop. Config-driven.

**`Whelmed::Component` class:**

```cpp
// Source/whelmed/Component.h
namespace Whelmed
{
    class Component : public Terminal::PaneComponent
    {
    public:
        explicit Component (jreng::Typeface& bodyFont, jreng::Typeface& codeFont);

        // PaneComponent interface
        void switchRenderer (Terminal::RendererType type) override;
        void applyConfig() noexcept override;

        // GLComponent interface
        void glContextCreated() noexcept override;
        void glContextClosing() noexcept override;
        void renderGL() noexcept override;

        // juce::Component
        void paint (juce::Graphics& g) override;
        void resized() override;

        void openFile (const juce::File& file);

    private:
        Document document;

        // Typefaces owned externally (by MainComponent or Panes)
        jreng::Typeface& bodyFont;
        jreng::Typeface& codeFont;

        // Renderer variant — same pattern as Terminal::Component
        using RendererVariant = std::variant<
            jreng::Glyph::GraphicsTextRenderer,
            jreng::Glyph::GLTextRenderer>;
        RendererVariant renderer;

        juce::VBlankAttachment vblank;
    };
}
```

**Render loop (VBlank-driven, mirrors Terminal::Component):**
1. `onVBlank()` — check `document.consumeDirty()`. If dirty: rebuild `TextLayout` per block, fire `onRepaintNeeded`.
2. GPU path: `renderGL()` — iterate layouts, call `layout.draw<GLTextRenderer>()`.
3. CPU path: `paint(g)` — iterate layouts, call `layout.draw<GraphicsTextRenderer>()`.

**`switchRenderer(RendererType)`:**
- Same pattern: emplace variant, `setOpaque`/`setBufferedToImage`, reapply config.

**`applyConfig()`:**
- Read `Config::Key::markdown*` values.
- Update font sizes on body/code typefaces.
- Mark document dirty for re-layout.

**Scroll:** `juce::Viewport` wrapping the rendered block content. Block stacking is vertical — each block's `TextLayout` reports height via `getHeight()`. Mermaid blocks report height from `MermaidParseResult::viewBox`.

**Files:**
- `Source/whelmed/Component.h`
- `Source/whelmed/Component.cpp`

**Validate:** Construct `Whelmed::Component`. Call `openFile (testMarkdown)`. CPU path renders styled text via `paint()`. GPU path renders via `renderGL()`. `switchRenderer()` toggles between paths. `applyConfig()` picks up new config values.

---

### Step 5.6 — Panes generalization

Change `Panes` to own `Owner<PaneComponent>` instead of `Owner<Terminal::Component>`.

**Changes to `Source/component/Panes.h`:**
- `jreng::Owner<Terminal::Component> terminals` → `jreng::Owner<Terminal::PaneComponent> panes`
- Rename throughout: `terminals` → `panes` (semantic — container holds panes, not just terminals)

**Changes to `Source/component/Panes.cpp`:**
- `createTerminal()` — creates `Terminal::Component`, adds to `panes`. Terminal-specific callback wiring accesses `Terminal::Component` API via the returned pointer (before upcasting to `Owner<PaneComponent>`).
- New: `createWhelmed (const juce::File& file)` — creates `Whelmed::Component`, calls `openFile()`, adds to `panes`. Whelmed-specific callback wiring.
- `PaneManager::layOut()` — already a template, already works with any `ComponentType` that has `getComponentID()` and `setBounds()`. `PaneComponent` inherits both from `juce::Component` via `GLComponent`. No change needed.
- `switchRenderer()` — iterates `panes`, calls `pane->switchRenderer()` on each. No type inspection — `PaneComponent` interface.
- `applyConfig()` — iterates `panes`, calls `pane->applyConfig()` on each. No type inspection.
- `getTerminals()` — GL renderer needs to iterate only terminals for the component iterator. Returns filtered view or separate accessor. **DISCUSS WITH ARCHITECT: cleanest approach for GL iterator filtering.**

**Changes to `Source/component/Tabs.h/cpp`:**
- Forward `switchRenderer` and `applyConfig` through Panes to all pane components.

**Validate:** END builds. Terminal panes work identically. `Panes::createWhelmed()` creates a Whelmed pane. Split pane can hold one terminal and one Whelmed component side by side. Renderer switching propagates to both.

---

### Step 5.7 — Creation triggers

Wire `.md` file opening to spawn `Whelmed::Component` in a new pane.

**Trigger 1: Hyperlink / open-file dispatch**

Hook into `LinkManager::dispatch()` at the extension branch. When `ext == ".md"`: instead of writing to PTY, fire a callback that creates a Whelmed pane.

**Changes to `LinkManager`:**
- Add callback: `std::function<void (const juce::File&)> onOpenMarkdown`
- In `dispatch()`, before the PTY write branch: if `ext == ".md"` and `onOpenMarkdown` is set, call it instead.

**Wiring:** `Terminal::Component` wires `linkManager.onOpenMarkdown` → calls up to `Panes::createWhelmed()` via existing callback chain (same pattern as `onShellExited`).

**Trigger 2: Drag and drop**

`Terminal::Component` already implements `juce::FileDragAndDropTarget`. Add `.md` detection in `filesDropped()`: if dropped file is `.md`, create Whelmed pane instead of default action.

**Trigger 3: Action registry**

Register `open_markdown` action in `Terminal::Action`. Triggered from command palette or keybinding. Opens file chooser filtered to `*.md`, creates Whelmed pane with selected file.

**Files:**
- `Source/terminal/selection/LinkManager.h` — add `onOpenMarkdown` callback
- `Source/terminal/selection/LinkManager.cpp` — branch in `dispatch()`
- `Source/component/TerminalComponent.cpp` — wire callback, handle drag-and-drop `.md`
- `Source/component/Panes.cpp` — `createWhelmed()` call from callback
- `Source/terminal/action/Action.cpp` — register `open_markdown` action

**Validate:** Click a `.md` hyperlink in terminal output → Whelmed pane opens in split. Drag `.md` file into END → Whelmed pane opens. Command palette → "open_markdown" → file chooser → Whelmed pane.

---

### Step 5.8 — Mermaid integration

Wire `mermaid::Parser` (WebBrowserComponent singleton) into `Whelmed::Component` for live mermaid diagram rendering.

**Dependency:** `juce_gui_extra` module (for `WebBrowserComponent`). Must be added to END's CMakeLists.txt / JUCE module dependencies.

**Changes:**
- `Whelmed::Component` — when `Document::getBlocks()` contains `Type::Mermaid` blocks: extract fence content, pass to `mermaid::Parser::convertToSVG()`, store SVG result, parse via `MermaidSVGParser::parse()`, cache `MermaidParseResult` per block.
- Render mermaid blocks in `paint()` / `renderGL()`: iterate `result.primitives` (fill/stroke paths) and `result.texts` (text labels). Scale from viewBox to allocated block bounds.
- Mermaid parser singleton lifecycle: created lazily on first mermaid block encounter. Not created if no mermaid blocks exist (Lean — no cost when unused).
- Mermaid conversion is async — block shows placeholder until SVG arrives, then marks dirty for re-render.

**Files:**
- `Source/whelmed/mermaid/Parser.h` — singleton wrapper (ported from WHELMED scaffold)
- `Source/whelmed/mermaid/Parser.cpp` — implementation
- `Source/whelmed/Component.cpp` — mermaid block rendering in paint/renderGL paths
- CMakeLists.txt — add `juce_gui_extra` dependency

**Validate:** Open `.md` file containing ```` ```mermaid ```` block. Diagram renders inline after async conversion. Scrolling, resize, renderer switching all work with mermaid blocks present.

---

## Open Questions (for ARCHITECT)

1. **Step 5.5 — Typeface ownership.** Body and code `Typeface` instances for Whelmed — owned by `MainComponent` (shared across all Whelmed panes, like terminal's `Typeface`) or per-component? Recommend shared (SSOT, memory efficiency).

2. **Step 5.6 — GL iterator filtering.** `GLRenderer::setComponentIterator()` currently iterates `Owner<Terminal::Component>`. With `Owner<PaneComponent>`, both terminals and Whelmed components are in the same container. The GL iterator needs all visible `GLComponent` instances — which `PaneComponent` already is. Should the iterator just iterate all `panes`? Or maintain separate collections?

3. **Step 5.8 — `juce_gui_extra` dependency.** Acceptable for END? This module brings `WebBrowserComponent` but also other things. If not, mermaid rendering can be deferred entirely.

4. **Table rendering (from Step 5.1).** WHELMED has table parsing but no table rendering. Should table rendering use `juce::Graphics` directly (draw lines, cells, text) or compose `TextLayout` per cell? Recommend direct `Graphics` — tables are grid-based, not flowing text.

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
