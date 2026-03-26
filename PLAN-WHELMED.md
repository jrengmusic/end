# PLAN-WHELMED.md — WHELMED Integration into END

**Project:** END
**Date:** 2026-03-26
**Author:** COUNSELOR
**Status:** Draft — awaiting ARCHITECT approval. Plan 4 complete (Sprint 121/122).

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Integrate WHELMED markdown/mermaid rendering into END as a pane-compatible component. `Whelmed::Component` is a peer of `Terminal::Component` — identical lifecycle, identical renderer switching, identical config hot-reload path.

**Surface API:** `juce::AttributedString` → `jreng::TextLayout` → duck-typed renderer. The component never knows which renderer is active.

**What ports from WHELMED scaffold (`~/Documents/Poems/dev/whelmed/`):**
- `markdown::Parser` — 3-stage parser (blocks → units → inline spans → `AttributedString`)
- `markdown::parseTables()` — GFM table parser (data extraction)
- `mermaid::extractBlocks()` — fence extraction
- `MermaidSVGParser` — SVG string → `juce::Path`/`juce::Graphics` primitives (scaffolded by BRAINSTORMER against real mermaid.js output)
- `mermaid::Parser` — singleton `WebBrowserComponent` + mermaid.js → SVG string

**What is replaced by END's pipeline:**
- WHELMED's `GLString`, `GLGraphics::drawText`, `GLAtlas`, `GLComponent` block stacking → `jreng::TextLayout::draw<Renderer>`

---

## Architecture

```
Whelmed::Config (whelmed.lua)
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
PaneComponent (Source/component/)  — pure virtual base, app-level (no namespace)
    ^                    ^
    |                    |
Terminal::Component   Whelmed::Component
```

### Module / Project Split

**Module level** (reusable — WHELMED standalone can consume):
```
modules/jreng_markdown/          NEW: reusable parsing module
    ├── markdown/
    │   ├── Parser.h/cpp         block detection, line classification, inline spans, toAttributedString
    │   ├── Table.h/cpp          GFM table parser (data extraction)
    │   └── Types.h              Block, Unit, InlineSpan, Blocks
    └── mermaid/
        ├── Extract.h/cpp        fence extraction
        └── SVGParser.h/cpp      SVG string → SVGPrimitive / SVGTextPrimitive
```

**Project level** (END-specific integration):
```
Source/whelmed/                   NEW: END integration layer
    ├── config/
    │   ├── Config.h/cpp         Whelmed::Config (Context<Config> singleton)
    │   └── default_whelmed.lua  BinaryData template
    ├── Component.h/cpp          Whelmed::Component (PaneComponent subclass)
    ├── Document.h/cpp           file state, parsed blocks, dirty tracking
    ├── TableComponent.h/cpp     dedicated table renderer component
    └── mermaid/
        └── Parser.h/cpp         WebBrowserComponent singleton wrapper
```

### Module Dependency

```
jreng_core
    ^
    |
jreng_graphics   (Typeface, Atlas, TextLayout, GraphicsTextRenderer)
    ^
    |
jreng_markdown   (Parse, Table, SVG parser — pure data, depends on jreng_core + juce_graphics)
    ^
    |
jreng_opengl     (GLComponent, GLRenderer, GLTextRenderer)
    ^
    |
END app
    ├── Source/component/    (PaneComponent, Terminal::Component, Panes, Tabs)
    ├── Source/whelmed/      (Whelmed::Config, Whelmed::Component, Document, TableComponent, mermaid::Parser)
    └── Source/config/       (Config — terminal domain only, untouched)
```

---

## Design Decisions (ARCHITECT approved)

1. **`PaneComponent` pure virtual base.** Both `Terminal::Component` and `Whelmed::Component` inherit from it. `Panes` owns `Owner<PaneComponent>`. No `dynamic_cast`. MANIFESTO-compliant (Explicit Encapsulation — communicate via API, not type inspection).

2. **Whelmed Typefaces shared, config-driven.** `MainComponent` owns body + code `Typeface` instances (like terminal's shared typeface). Config-driven via `markdown.font_family`, `markdown.code_family`. Shared across all Whelmed panes. SSOT — one config, one typeface set, all panes reflect.

3. **Typeface proportional mode via constructor flag.** Not a separate class. One boolean gates the ASCII fast path bypass. Lean — minimal change, zero impact on terminal path.

4. **Atlas already supports multi-face multi-size.** `Glyph::Key` has `{glyphIndex, fontFace*, fontSize, span}`. Different Typeface instances produce different `fontFace*` pointers → distinct cache entries. No structural change needed.

5. **Separate `Whelmed::Config` with `Context<Config>`.** Own lua file (`~/.config/end/whelmed.lua`), own defaults, own reload. END's Config stays clean. Explicit Encapsulation — one object, one job.

6. **Creation triggers via `LinkManager::dispatch()`.** Single hook point — both mouse click and keyboard hint-label converge here. Branch on `.md` extension to create Whelmed pane instead of writing to PTY.

7. **Mermaid via `juce_gui_extra` + `WebBrowserComponent`.** Add to module deps. Deferred to last step — can ship markdown rendering without mermaid.

8. **`jreng::TextLayout` is the rendering surface.** `createLayout (AttributedString, maxWidth, font)` → `draw<Renderer>()`. Already proportional-capable — uses per-glyph `xAdvance` from HarfBuzz. Untested with proportional fonts — Step 5.2 validates.

---

## PLAN 5: WHELMED Integration

| Step | Objective | Depends on |
|------|-----------|------------|
| 5.0 | Config keys — `markdown` table | — |
| 5.1 | `jreng_markdown` module — port semantic layer | — |
| 5.2 | Typeface proportional mode | — |
| 5.3 | `PaneComponent` pure virtual base | — |
| 5.4 | Document model | 5.1 |
| 5.5 | `Whelmed::Component` | 5.0, 5.2, 5.3, 5.4 |
| 5.6 | Panes generalization | 5.3, 5.5 |
| 5.7 | Creation triggers | 5.5, 5.6 |
| 5.8 | Table component | 5.1, 5.5 |
| 5.9 | Mermaid integration | 5.1, 5.5 |

Steps 5.0–5.3 have no cross-dependencies and can proceed independently.

---

### Step 5.0 — `Whelmed::Config` — separate config object

Whelmed gets its own `Context<Config>` singleton. Separate lua file, separate defaults, separate reload. END's `Config` stays untouched — no markdown keys there. Clean domain separation (Explicit Encapsulation — one object, one job).

**Pattern:** Identical to `Config` — same CRTP base, same `initKeys()`/`load()`/`writeDefaults()`/`getConfigFile()` machinery. Different file, different keys.

**Config file:** `~/.config/end/whelmed.lua` (same config dir as END, separate file)

**Default template:** `Source/whelmed/config/default_whelmed.lua` (embedded as BinaryData)

```lua
WHELMED = {
    font_family = "%%font_family%%",     -- proportional body font family
    font_size   = %%font_size%%,          -- base body size in points (8-72)
    code_family = "%%code_family%%",     -- code block font (monospace)
    code_size   = %%code_size%%,          -- code block size in points (8-72)
    h1_size     = %%h1_size%%,            -- header 1 size (8-72)
    h2_size     = %%h2_size%%,            -- header 2 size (8-72)
    h3_size     = %%h3_size%%,            -- header 3 size (8-72)
    h4_size     = %%h4_size%%,            -- header 4 size (8-72)
    h5_size     = %%h5_size%%,            -- header 5 size (8-72)
    h6_size     = %%h6_size%%,            -- header 6 size (8-72)
    line_height = %%line_height%%,        -- line height multiplier (0.8-3.0)
}
```

**`Whelmed::Config` class:**

```cpp
// Source/whelmed/config/Config.h
namespace Whelmed
{
    struct Config : jreng::Context<Config>
    {
        Config();

        struct Key
        {
            inline static const juce::String fontFamily { "font_family" };
            inline static const juce::String fontSize { "font_size" };
            inline static const juce::String codeFamily { "code_family" };
            inline static const juce::String codeSize { "code_size" };
            inline static const juce::String h1Size { "h1_size" };
            inline static const juce::String h2Size { "h2_size" };
            inline static const juce::String h3Size { "h3_size" };
            inline static const juce::String h4Size { "h4_size" };
            inline static const juce::String h5Size { "h5_size" };
            inline static const juce::String h6Size { "h6_size" };
            inline static const juce::String lineHeight { "line_height" };
        };

        // Same API as Config: getString, getFloat, getBool, etc.
        // Same internals: values map, schema map, addKey, load, writeDefaults

        bool reload();
        std::function<void()> onReload;

    private:
        void initKeys();
        bool load (const juce::File& file);
        void writeDefaults (const juce::File& file) const;
        juce::File getConfigFile() const;
    };
}
```

**Ownership:** Value member of `ENDApplication` in Main.cpp, alongside `Config config`. Declared after `config` (same lifetime):
```cpp
Config config;
Whelmed::Config whelmedConfig;
```

**Reload wiring:** `config.onReload` already calls `content->applyConfig()`. `whelmedConfig.onReload` wired the same way — calls through to `Whelmed::Component::applyConfig()` via MainComponent.

**Files:**
- `Source/whelmed/config/Config.h` — new
- `Source/whelmed/config/Config.cpp` — new (initKeys, load, writeDefaults, getConfigFile, constructor)
- `Source/whelmed/config/default_whelmed.lua` — new (BinaryData template)
- `Source/Main.cpp` — add `Whelmed::Config whelmedConfig` member, wire `onReload`

**No changes to END's Config.h, Config.cpp, or default_end.lua.**

**Validate:** END builds. `Whelmed::Config::getContext()->getString (Whelmed::Config::Key::fontFamily)` returns `"Georgia"`. `~/.config/end/whelmed.lua` created on first launch.

---

### Step 5.1 — `jreng_markdown` module — port semantic layer

Create new JUCE module `modules/jreng_markdown/`. Port parsing code from WHELMED scaffold. Pure data transforms — zero rendering dependency. Reusable by WHELMED standalone.

**Port:**
- `markdown::Parser` — block detection, line classification, inline span tokenization, `toAttributedString()`
- `markdown::parseTables()` — GFM table data extraction
- `mermaid::extractBlocks()` — fence extraction
- `MermaidSVGParser` — SVG → `SVGPrimitive` / `SVGTextPrimitive` (flat lists for `juce::Graphics` rendering)

**Module structure:**
```
modules/jreng_markdown/
    jreng_markdown.h                  — module header
    jreng_markdown.cpp                — module source
    markdown/
        jreng_markdown_types.h        — Block, Unit, InlineSpan, Blocks
        jreng_markdown_parser.h       — parse functions (getBlocks, getUnits, inlineSpans, toAttributedString)
        jreng_markdown_parser.cpp
        jreng_markdown_table.h        — GFM table parser
        jreng_markdown_table.cpp
    mermaid/
        jreng_mermaid_extract.h       — fence extraction
        jreng_mermaid_extract.cpp
        jreng_mermaid_svg_parser.h    — SVG string → primitives
        jreng_mermaid_svg_parser.cpp
```

**Module dependencies:** `juce_core`, `juce_graphics` (for `juce::Path`, `juce::Colour`, `juce::AttributedString`)

**Adaptation during port:**
- Namespace: `jreng::Markdown`, `jreng::Mermaid` (module-level, not app-level)
- Naming: NAMING-CONVENTION (nouns for types, verbs for functions)
- `toAttributedString()` — takes font config parameters (family, sizes, colours), not hardcoded. Does NOT read Config directly (module has no Config dependency).
- Code style: JRENG-CODING-STANDARD (braces on new line, `not`/`and`/`or`, brace initialization, no early returns, `.at()` for containers)
- Remove WHELMED-specific dependencies (`jreng_opengl` types, `GLString`, etc.)

**CMakeLists.txt:** Add `jreng_markdown` to END's module list.

**Validate:** END builds. `jreng::Markdown::Parserr::getBlocks (testString)` returns correct block types. `jreng::Mermaid::SVGParser::parse (testSVG)` returns populated result. No rendering — pure data.

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

Extract shared interface from `Terminal::Component` into `PaneComponent`. Both `Terminal::Component` and `Whelmed::Component` inherit from it. Panes communicates via this API — no type inspection.

**`PaneComponent` class:**

```cpp
// Source/component/PaneComponent.h
#pragma once
#include <modules/jreng_opengl/jreng_opengl.h>

class PaneComponent : public jreng::GLComponent
{
public:
    enum class RendererType { gpu, cpu };

    virtual ~PaneComponent() = default;

    virtual void switchRenderer (RendererType type) = 0;
    virtual void applyConfig() noexcept = 0;

    std::function<void()> onRepaintNeeded;
};
```

**`RendererType` is a public member enum of `PaneComponent`.** Not in `Terminal::` namespace — it is app-wide. Both `Terminal::Component` and `Whelmed::Component` reference it as `PaneComponent::RendererType`. The existing `PaneComponent::RendererType` and its free functions (`resolveRenderer()`, `resolveRendererFromConfig()`) move to `PaneComponent` scope or become free functions referencing `PaneComponent::RendererType`.

**Namespace:** `PaneComponent` is NOT in `Terminal::` — shared between domains. Lives at app level (same as `AppState`, `MainComponent`).

**Why this interface:**
- `jreng::GLComponent` base — both pane types need GL lifecycle hooks and `paint()`. Already provides virtual no-ops.
- `switchRenderer` — `MainComponent::applyConfig()` propagates via Tabs/Panes. Both types implement variant emplacement.
- `applyConfig` — hot-reload propagation. Each type reads its own config keys.
- `onRepaintNeeded` — callback wired by Panes/Tabs/MainComponent. Both types fire after dirty content rendered.
- No `getValueTree()` — not needed for layout. Can be added when persistence is needed.

**Changes to `Terminal::Component`:**
- Inherit from `PaneComponent` instead of `jreng::GLComponent` directly
- Move `onRepaintNeeded` to `PaneComponent` (remove from `Terminal::Component`)
- Add `override` to `switchRenderer` and `applyConfig`
- Zero behavioral changes — pure extraction

**Changes to `RendererType`:**
- Move `RendererType` enum from `Terminal::` namespace (Source/component/RendererType.h) into `PaneComponent` as public member enum
- `resolveRenderer()`, `resolveRendererFromConfig()` become free functions returning `PaneComponent::RendererType`
- All call sites update: `Terminal::RendererType` → `PaneComponent::RendererType`

**Files:**
- `Source/component/PaneComponent.h` — new file (owns RendererType enum)
- `Source/component/RendererType.h` — remove enum, keep free functions updated to return `PaneComponent::RendererType`
- `Source/component/TerminalComponent.h` — change base class, add `override`
- All files referencing `Terminal::RendererType` — update to `PaneComponent::RendererType`

**Validate:** END builds. Terminal rendering identical. `Terminal::Component` is-a `PaneComponent` is-a `GLComponent`. All existing `RendererType` references compile with new path.

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
- Load file → parse via `Markdown::Parser::getBlocks()` → store blocks
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
    class Component : public PaneComponent
    {
    public:
        explicit Component (jreng::Typeface& bodyFont, jreng::Typeface& codeFont);

        // PaneComponent interface
        void switchRenderer (PaneComponent::RendererType type) override;
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
        std::optional<Document> document;  // created on openFile()

        // Typefaces shared — owned by MainComponent, config-driven
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
- Read `Whelmed::Config::getContext()` values (font sizes, families, line height).
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
- `jreng::Owner<Terminal::Component> terminals` → `jreng::Owner<PaneComponent> panes`
- Rename throughout: `terminals` → `panes` (semantic — container holds panes, not just terminals)

**Changes to `Terminal::Component::create()` factory:**
- Current signature takes `Owner<Terminal::Component>&` — incompatible with `Owner<PaneComponent>`.
- Change: return `std::unique_ptr<Terminal::Component>` instead of adding to owner. Caller handles ownership. `unique_ptr<Terminal::Component>` implicitly upcasts to `unique_ptr<PaneComponent>` via converting constructor. `Owner` stays as-is — no changes to `jreng::Owner`.

**Changes to `Source/component/Panes.cpp`:**
- `createTerminal()` — calls modified `Terminal::Component::create()`, wires terminal-specific callbacks on the raw pointer, then moves `unique_ptr` into `panes` (implicit upcast to `PaneComponent`).
- New: `createWhelmed (const juce::File& file)` — creates `Whelmed::Component`, calls `openFile()`, wires callbacks, adds to `panes`.
- `PaneManager::layOut()` — already a template, already works with any `ComponentType` that has `getComponentID()` and `setBounds()`. `PaneComponent` inherits both from `juce::Component` via `GLComponent`. No change needed.
- `switchRenderer()` — iterates `panes`, calls `pane->switchRenderer()` on each. No type inspection — `PaneComponent` interface.
- `applyConfig()` — iterates `panes`, calls `pane->applyConfig()` on each. No type inspection.
- `getTerminals()` → rename to `getPanes()` — returns `Owner<PaneComponent>&`. GL renderer iterates all panes (all are `GLComponent`). Single container, no filtering needed.

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

### Step 5.8 — Table component

Dedicated `Whelmed::TableComponent` for GFM table rendering. Self-contained — manages its own rendering like mermaid diagrams. Parent viewport stacks it as a block.

**`Whelmed::TableComponent` class:**
- Receives `jreng::Markdown::TableData` (parsed rows, columns, alignment, headers)
- Renders grid lines, cell backgrounds, text per cell via `juce::Graphics` (CPU) or `jreng::TextLayout` per cell (GPU)
- Reports `getPreferredHeight()` based on row count and font metrics
- Config-driven: reads font sizes, colours from `Whelmed::Config::getContext()`
- Participates in renderer switching (inherits from `juce::Component`, not `PaneComponent` — it is a child of `Whelmed::Component`, not a pane itself)

**Files:**
- `Source/whelmed/TableComponent.h`
- `Source/whelmed/TableComponent.cpp`

**Validate:** Table block in `.md` file renders as formatted grid with headers, alignment, cell text. Resizing reflows column widths.

---

### Step 5.9 — Mermaid integration

Wire mermaid parser (WebBrowserComponent singleton) into `Whelmed::Component` for live mermaid diagram rendering.

**Dependency:** `juce_gui_extra` module (for `WebBrowserComponent`). Add to END's CMakeLists.txt module dependencies.

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

## Resolved Decisions

1. **Typeface ownership** — shared in MainComponent, config-driven, user-configurable. (ARCHITECT, Step 5.5)
2. **GL iterator** — single container, iterate all panes. All just Components that render both GL and Graphics. (ARCHITECT, Step 5.6)
3. **`juce_gui_extra`** — add to module deps. (ARCHITECT, Step 5.9)
4. **Table rendering** — dedicated `Whelmed::TableComponent`, self-contained like mermaid diagrams. (ARCHITECT, Step 5.8)
5. **Module/project split** — reusable parsing at module level (`jreng_markdown`), app integration at project level (`Source/whelmed/`). (ARCHITECT)
6. **`create()` factory** — returns `unique_ptr`, caller handles ownership. `Owner` unchanged. (ARCHITECT)
7. **`RendererType`** — public member enum of `PaneComponent`. Not in `Terminal::` namespace. (ARCHITECT)

---

## Contract Compliance Audit

**NAMING-CONVENTION:**
- `Parse` renamed to `Parser` (Rule 1: nouns for things)
- `Document`, `Component`, `TableComponent` — all nouns
- `getBlocks()`, `openFile()`, `switchRenderer()`, `applyConfig()` — all verbs
- `consumeDirty()` — verb, reveals side effect (Rule 3: semantic over literal)
- Consistent patterns throughout (Rule 5)

**JRENG-CODING-STANDARD:**
- All code examples use brace initialization, `not`/`and`/`or`, no early returns
- `.at()` for container access enforced in execution rules
- `noexcept` on `applyConfig()` — follows Terminal::Component pattern
- `override` on all virtual method implementations

**ARCHITECTURAL-MANIFESTO (LIFESTAR):**
- **Lean:** Typeface proportional mode = one boolean, one line change. No new classes.
- **Explicit Encapsulation:** Document does NOT read Config (receives parameters). PaneComponent communicates via API (switchRenderer, applyConfig), no type inspection. Objects are dumb — tell, don't ask.
- **SSOT:** Config → AppState → Component. No duplicated state. Shared Typeface instances.
- **Findable:** Module split mirrors existing pattern (jreng_graphics / Source/terminal/).
- **Reviewable:** Each step is self-contained, validates independently.

**ARCHITECTURE.md compliance:**
- Layer separation: Module (data) → Source (component) → Config (coordination)
- Communication: callbacks (onRepaintNeeded, onOpenMarkdown), not direct cross-layer calls
- No poking: PaneComponent interface, not dynamic_cast
- Factory pattern: `create()` returns `unique_ptr`, caller handles ownership. `Owner` unchanged.

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
