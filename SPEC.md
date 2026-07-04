# SPEC.md — END: Ephemeral Nexus Display

**Version:** 0.0.1 — Implementation in Progress
**Date:** 2026-06-04 (original), updated 2026-07-04
**Current Phase:** 14 (Final Polish — Advanced Shaders) | Phases 3-13 COMPLETE

---

## 0. Identity

END is a **JUCE GUI application** that renders terminal output.

It is not a terminal emulator wrapped in JUCE. It is not a custom OpenGL renderer with a JUCE shell. The JUCE component lifecycle, threading model, and event system are the foundation — not obstacles to work around. JAM extends where JUCE cannot reach; it never replaces what JUCE already provides.

### Priority Order

1. **JUCE GUI application.** Component hierarchy, ValueTree state, timer-driven rendering, native windowing. Every architectural decision starts here.
2. **Faithful full-spec VT terminal emulator.** VT100/VT520 parser, xterm-compatible input encoding, modern protocol extensions (CSI u, OSC 133, OSC 8). The VT engine is a subsystem within the application, not the application itself.
3. **Modern development environment.** Built-in markdown renderer with mermaid graphics (Whelmed), Lua scripting with hot reload, configurable shader pipeline (Shadertoy-compatible background and post-process passes), SVG-driven tab styling, native fuzzy finder, inline image preview.

### Why This Order Is Non-Negotiable

The previous iteration started as priority 2 with priority 1 as an afterthought. Every agent, every sprint, every refactor fought JUCE because the mental model was "terminal emulator" — not "JUCE application." The rendering pipeline, the state management, the component ownership — all were designed terminal-first and bent to fit JUCE after the fact. Each feature added revealed another seam where the terminal model and the JUCE model collided. A month of refactoring could not converge because the foundation encoded the wrong priority.

---

## 1. Architectural Foundation — APVTS-Analogous Lock-Free Architecture

### 1.0 What This Is NOT — Anti-Mental-Model (MANDATORY READ)

**END is NOT a terminal emulator with a scanline grid model.** Every agent's training data is dominated by terminal emulators that use the scanline model. That model is fundamentally wrong for END. Reading the SPEC through the scanline lens will produce wrong implementations. This section exists to destroy that lens.

**The scanline model (WRONG for END):**
- The screen is a volatile grid of cells sized to the viewport, replaced on every frame.
- Scrollback is an afterthought bolted onto the same grid.
- The grid IS the document — resize mutates it, corrupts or loses content.
- SIGWINCH reflows the grid, destroying content that doesn't fit.
- The reader thread writes directly to the display surface.

**END's model (CORRECT):**
- The screen is a JUCE `Component` that renders from a persistent document (`CodeModel`).
- Scrollback is first-class — the same document, same storage, same projection.
- `Buffer<Row>` (owned by Video) is a **scratch surface** — destroyed and rebuilt on resize. It is NOT the document. It is NOT the SSOT for content. It is the VT engine's working memory, analogous to an audio processor's sample buffer.
- `CodeModel` (`ParagraphsModel`) is the document SSOT — a dimensionless bounded deque of `jam::String` lines. Width enters exactly once, at projection time, when `CodeView` calls `getWrappedLines(viewWidth)`. Storage knows nothing about width, pixel dimensions, or viewport geometry.
- SIGWINCH changes the projection width only — `CodeModel` is untouched. History lines survive resize unchanged. `Processor::prepare()` resizes `Buffer<Row>` (the scratch); `CodeModel` content is unaffected.
- The reader thread NEVER touches `CodeModel`. The boundary is `CellFifo` — a lock-free SPSC ring that data passes through. `Video` writes to `Buffer<Row>` and pushes rows into `CellFifo` on the reader thread. The message thread drains `CellFifo` into `CodeModel`. Two threads, one bridge, unidirectional.

**This is the same design used by both Neovim and JUCE:**
- External sources (VT parser, markdown renderer, user keyboard) commit text INTO the editor.
- The editor IS the SSOT for all content it renders.
- Width enters exactly once, at projection time.
- Storage knows nothing about width, pixel dimensions, or viewport geometry.

### 1.1 Data Ownership — What Lives Where

| Object | Owner | What it is | What it is NOT |
|--------|-------|-----------|----------------|
| `Buffer<Row>` | `Video` (reader thread) | VT engine scratch surface. Dual channel (normal/alternate). Destroyed on resize. | NOT the document. NOT the SSOT. NOT persistent. |
| `CellFifo` | `Processor` (reader thread) | Lock-free SPSC transport. Two rings (history + active). Drop-oldest under flood. Data passes through and is consumed. | NOT storage. NOT scrollback. NOT persistent. Under flood, oldest entries are dropped — this is correct. |
| `CodeModel` | `Session` (message thread) | The document SSOT. `ParagraphsModel` — bounded deque of `jam::String` lines. History lines survive resize. FIFO eviction drops oldest when over `scrollbackLines` capacity. | NOT owned by Processor. NOT written by the reader thread. NOT touched by resize. |
| `CodeView` | `Session` (message thread, parented by View) | Dumb rendering widget. Reads `const CodeModel&`. Wraps at paint time via `getWrappedLines(viewWidth)`. | NOT the document. NOT a state owner. NOT a ValueTree::Component. |
| `terminal::Model` | `Session` | APVTS bridge. Atomics (reader writes), ValueTree (message reads). Timer flushes dirty atomics to ValueTree. | NOT a config store. NOT a document store. Only scalar state (cursor, modes, flags, text params). |

### 1.2 APVTS Analogy

END's cross-thread architecture is modeled after JUCE's `AudioProcessorValueTreeState` (APVTS). The audio plugin world solved the same problem: a real-time thread producing data consumed by a UI thread, with zero locks on the hot path.

The closest analogy is a **spectrum analyzer plugin**, not a synthesizer. In a synthesizer, the audio thread is the hot path. In an analyzer, the audio thread just pushes raw data — the **display thread is the hot path**, reading processed results and painting at 60/120fps. END is the same: the reader thread pushes raw cells, the message thread paints.

### Plugin Architecture Mapping

| JUCE Audio Plugin | END |
|-------------------|-----|
| Host (DAW) | `Nexus` — owns terminal instances, manages lifecycle, routes IPC |
| `AudioProcessor` | `terminal::Session` — owns Model, Processor, CodeModel, CodeView |
| `ProcessorChain` | `terminal::Processor` — reader thread pipeline, references Model |
| `APVTS` | `terminal::Model` — state bridge, owned by Session |
| `PluginEditor` | `terminal::View` — GUI, listens on Model, created/destroyed independently |
| `SpectrumFIFO` | `CellFifo` — lock-free SPSC transport, reader pushes, message drains |
| `SpectrumProcessor::outputDB` | `CodeModel` — processed output ready for display |

Key property: the Editor (View) is detachable. In daemon mode, the Session keeps running without a View — identical to a DAW running a plugin headless. The View is created when a GUI client connects, destroyed when it disconnects. The Session and its state survive.

### 1.3 Thread Contract

| Thread | Writes | Reads | Never |
|--------|--------|-------|-------|
| **Reader** (TTY) | Atomics on terminal::Model, Buffer\<Row\> cells, CellFifo push | Raw PTY bytes | ValueTree, mutex, allocation, block, **CodeModel** |
| **Message** (main) | ValueTree, CodeModel mutations (drain) | ValueTree (via listener/referTo), CellFifo drain | Atomics (except flush) |
| **Timer** (JUCE) | ValueTree properties (flush dirty atomics) | `needsFlush` atomic | Buffer\<Row\>, CodeModel |
| **GL** (OpenGL) | Shader uniforms, FBO | Shader source | Buffer\<Row\>, ValueTree, CodeModel |

**HARD INVARIANT:** The reader thread NEVER touches `CodeModel`. The message thread NEVER writes atomics (except during flush). The GL thread NEVER writes ValueTree or CodeModel. Violation of any of these is a **B violation** (thread binding) — not a bug to fix, an architecture to reject.

**`Session::drain()` runs on the message thread ONLY.** It is called by `terminal::View` in response to `screenDirty` ValueTree listener (which fires after timer flush). It pulls from `CellFifo` (message-thread read side of the SPSC ring) and mutates `CodeModel`. An agent must never call `drain()` from the reader thread.

### 1.4 Data Flow — Unidirectional, Always

```
Scalar state:
  READER → atomics on terminal::Model → Timer flush → ValueTree → Listeners → View repaint

Bulk cell data:
  READER → Video writes Buffer<Row> → CellFifo push (drop-oldest SPSC)
  MESSAGE → Session::drain() → CellFifo drain → CodeModel mutations → CodeView::calc() → repaint
```

No thread pushes to another. All communication is pull-based. No mutex on any hot path. No wait, no stall, no yield.

### 1.5 CONFIG Contract — Binary Defaults Are Foundation, Disk Files Are Override

Default lua files are compiled into the binary as BinaryData. The `config::Model` ValueTree is **built from these compiled defaults first** — the tree is guaranteed valid before any disk I/O. Disk lua files are user overrides applied onto the already-valid tree. No XML. No serialized ValueTree. No `referTo`.

**Init chain (app start):**
1. Build `config::Model` ValueTree from compiled binary default lua files (BinaryData). Parse via sol2, walk tables, `setProperty` per value. **Tree is valid and complete at this point.**
2. Check `~/.config/end/` for each lua module file (display.lua, keys.lua, nexus.lua, popups.lua, actions.lua, whelmed.lua).
3. **Virgin machine?** Any missing file → write the compiled default to disk immediately.
4. Read lua files from disk → parse via sol2 → overlay onto the already-built tree: `setProperty` only for values that parse successfully.
5. Any invalid/unreadable value → the compiled default stays (tree was built with it in step 1). **No value is ever missing or corrupt.**
6. Parse failures are tracked with file path and line number. Errors are reported to the user (MessageOverlay) — the application never fails to start, but the user sees exactly what went wrong and where.

**Hot-reload chain (file change):**
7. ENDApplication (the `jam::File::Watcher::Listener`) fires on any `.lua` change.
8. Reset tree to compiled binary defaults (step 1), then overlay from disk (step 4). Init and reload are the same code path — one function, two call sites.
9. config::Model `valueTreePropertyChanged` fires on all its listeners.
10. Each listener reacts accordingly — LookAndFeel re-styles, terminal::View re-applies font/colours, etc.

**Config delivery — config::Model listeners react:**
- **No `referTo`.** No manual distribution cascade from end::View.
- config::Model is an **independent tree** (`jam::Model` + `Instance<config::Model>`), NOT grafted to end::Model. Two separate trees, two separate listener registrations.
- Each consumer that needs config values registers as `ValueTree::Listener` on config::Model directly.
- Main's only job is: own the watcher, trigger the reload on `config::Model`, let the listener chain propagate naturally through config::Model.
- This is the standard JUCE ValueTree listener pattern. No magic. No manual push.

**Design constraint:** The build-and-overlay function must be clean and reentrant. Each invocation resets to compiled defaults and re-applies disk overrides. It never caches intermediate lua state between calls. Disk corruption, parse failure, or partial write cannot produce an invalid tree — the compiled defaults are the floor.

### 1.6 jam::Model Contract

`jam::Model` is the APVTS-analog state owner used by `end::Model`, `terminal::Model`, and `config::Model`. It is not a plain ValueTree wrapper. Its contract:

- **Owns a `juce::ValueTree` by value** (not `unique_ptr` — mirrors `juce::AudioProcessorValueTreeState` which holds `ValueTree state;` by value).
- **`get()` returns the ValueTree by value** — cheap ref-counted handle. Callers bind via `auto x = model.get()`, never `juce::ValueTree& x = model.get()`.
- **Atomic parameter map** (`jam::AnyMap`) — `Parameter<int>`, `ParameterText` adapters for cross-thread scalar transport. Reader thread writes via `storeValue` (atomic store). Timer flushes dirty atomics to ValueTree properties.
- **Timer flush** — inherits `juce::Timer`. `timerCallback()` copies dirty atomics to ValueTree. ValueTree fires `valueTreePropertyChanged` on listeners.
- **`addParameter<T>(id, ...)`** — registers a parameter in the AnyMap, creates the PARAM child node.

### 1.7 action::Registry — Parameterized Dispatch

`action::Registry` dispatches actions by string name. In the previous iteration, all actions had `void()` signatures. The rewrite requires **parameterized actions** because:

- `splitWithRatio` needs `(direction, isVertical, ratio)`
- `launchPopup` needs `(name, command, args, cwd, cols, rows)`

The old pattern used `DisplayCallbacks` — a struct of `std::function` closures stored in `lua::Engine`, capturing Tabs/Panes pointers. This is eliminated (§2 — config::Model holds no UI closures). All actions dispatch through `Registry::perform()`.

**The Registry must support a payload mechanism** — either typed action variants (handler receives `juce::var` or a typed payload struct), or per-entry registered closures that capture their parameters at registration time (each popup entry registers its own `void()` that already knows its command/args/cwd). The exact design is Phase 3 execution work, but the requirement is captured here: `void()` is insufficient.

### 1.8 Rendering Engine — First-Class Vulkan Architecture (RESOLVED in Phase 4)

`jam::VulkanEngine` (Application-owned, via `end::Application::vulkanEngine`) is the application's rendering authority — not a bolt-on, but first-class architecture, and the unified resource-ownership tree for the Vulkan rendering backend. It owns six singular/interning members — the shared `jam::vulkan::Device` (one per application), the `jam::Typeface`/`jam::Stamp`/`jam::Grapheme`/`jam::Link` `SharedResources<T>` interning tables, and the shared `jam::GlyphAtlas` (one per application) — plus the per-window `jam::vulkan::Graphics` collection. Member-declaration order inside `jam::VulkanEngine` governs teardown (reverse-order destruction): Device first (destructs last, since every GPU resource depends on it), then Typeface/Stamp/Grapheme/Link, then GlyphAtlas, then the per-window Graphics collection.

**Dual-engine dispatch (never-null factory pattern):**
- GPU path: `jam::vulkan::LowLevelGraphicsContext` (Vulkan LLGC) on available hardware
- CPU fallback: `jam::LowLevelGraphicsGlyphRenderer` (software rasterizer with shared atlas) when GPU unavailable/disabled

`VulkanEngine::createContext()` (installed as `juce::ComponentPeer::externalContextFactory`) **never returns nullptr** — JUCE's default renderers are structurally unreachable.

**Font/Atlas lifecycle (GL-thread binding resolved):**
- Typeface singleton (`jam::Typeface::getInstance()`) is a `jam::VulkanEngine` member, self-registers on construction
- GlyphAtlas is a `jam::VulkanEngine` member, self-registers on construction (`jam::GlyphAtlas::getInstance()`), reachable directly without going through VulkanEngine
- Font rasterization config (gamma, contrast, backend) applied at VulkanEngine init; hot-reload updates via config listeners, no mid-frame mutation
- Thread contract: message thread owns config/font changes, Vulkan paint dispatch is message-thread-synchronous (no dedicated GPU thread), atomicity by design — no guards needed

---

## 2. Architectural Foundation — META-MVC

BLESSED is the technical northstar. Its architectural expression is **META-MVC**: Model-View-Controller is not three god objects — it is a recursive pattern where each layer is itself an MVC triad.

### The Pattern

Every component in END occupies one or more roles:

- **Model** — a `jam::Model` instance or a `juce::ValueTree` node. Owns state. Accessed by its Controller.
- **View** — a `juce::Component`. Renders state. Receives input only to forward or translate — never to author Model state directly.
- **Controller** — orchestrates its own Model and child Views. A Controller IS a View to its parent.

A View at one level is a Controller at the level below. Each layer carries its own Model node. All Model nodes attach to the single application state tree — the SSOT.

### The Hierarchy — Ownership

```
ENDApplication                                        ← THE ORCHESTRATOR (Main.cpp, not namespaced)
  ├── config::Model                                    ← CONFIG tree + sol2 VM
  │     composed from: config::Display, config::Nexus, config::Keys, config::Popups, config::Actions, config::Whelmed
  │     (consumers read config::Model only, never individual parsers)
  │
  ├── end::Model (jam::Model + Instance<end::Model>)    ← runtime state SSOT
  │
  ├── end::View                                        ← Controller of the application surface
  │     ├── Tabs (View to end::View, Controller of Panes)
  │     │     └── Pane[N]
  │     │           └── terminal::View (PaneView subclass)
  │     │                 ├── parents CodeView (addAndMakeVisible, does NOT own)
  │     │                 ├── listens on terminal::Model (runtime) + config::Model (config)
  │     │                 └── calls Controller::drain() on screenDirty
  │     │
  │     ├── action::Registry (Instance<Registry>)
  │     ├── LookAndFeel (listener on config::Model)
  │     └── GL Pipeline
  │           ├── Background shader slot (user GLSL)
  │           └── Post-process shader slot (user GLSL)
  │
  └── Nexus (Instance<Nexus>)                           ← HOST — owns all terminal instances
        └── terminal::Controller[N] (= AudioProcessor)
              ├── terminal::Model (= APVTS) — state bridge, owned by Controller
              ├── terminal::Processor (= ProcessorChain) — references terminal::Model
              │     ├── Parser
              │     ├── Video (jam::terminal::Video subclass)
              │     └── CellFifo
              ├── CodeModel — document SSOT (processed output)
              └── CodeView — dumb jam_gui widget, cell-space setter/getter API
```

**terminal::View is the Controller of CodeView.** CodeView is a generic, dumb widget (see §2.1). It owns no authoritative state, listens to no tree. The View listens on `terminal::Model` (`valueTreePropertyChanged`) and translates each property change into a CodeView setter call. The View also owns the input handlers (key/mouse) that author selection.

**ENDApplication is the orchestrator.** It owns `config::Model` and `end::Model` as two independent trees — config and runtime state never mixed. It IS the `jam::File::Watcher::Listener` — on lua file change, it tells `config::Model` to re-read files from disk (same code path as init). config::Model updates its CONFIG tree, ValueTree listeners fire, consumers react.

**config::Model is the config Model — a dedicated independent tree, NOT grafted to end::Model.** `jam::Model` + `Instance<config::Model>`. It holds the sol2 VM privately (parser plus live custom-action Functions) and owns its own `juce::ValueTree`. Config is message-thread only — the reader never reads config::Model directly. Config-derived values the reader needs cross through `terminal::Model` atomics (see §2.0 Config → reader thread bridge). Main owns it as a value member.

**config::Model is composed from adjacent parsers** — `config::Display`, `config::Nexus`, `config::Keys`, `config::Popups`, `config::Actions`, `config::Whelmed`. Each parser handles one lua file/table and writes into config::Model's tree. **Consumers ONLY read config::Model** — they never access individual parsers. The parsers are internal to the config namespace.

**Two Instance models, clean separation:**
- `config::Model` — config constants. Changes on reload only. Listeners fire only on config changes.
- `end::Model` (`jam::Model` + `Instance<end::Model>`) — runtime state. Changes during app lifetime. Listeners fire only on runtime changes.

No config values on end::Model. No runtime state on config::Model. Consumers that need both (e.g. terminal::View) register as listener on both trees — two explicit registrations, two distinct concerns.

**Config → reader thread bridge:** Config is message-thread only — the reader never reads config::Model. Config-derived values the reader needs (cellWidth, cellHeight, scrollbackLines) are computed by `terminal::View` (listener on config::Model), then written as atomics to `terminal::Model` (the per-session APVTS bridge) where the reader can read them. View is the bridge between config and the reader thread.

The lua subsystem is its own MVC triad: `config::Model` (state + VM), Main (Controller — watcher, triggers re-read), consumers (Views that listen on config::Model's tree).

**Nexus is the Host.** It owns all `terminal::Session` instances in a map. Sessions survive View destruction (daemon mode). In standalone mode, Nexus exists with `Mode::standalone` — same ownership model, no IPC.

**terminal::Session is the AudioProcessor.** It owns the terminal instance: Model (APVTS), Processor (ProcessorChain), CodeModel (output), CodeView (renderer). Processor references Model but does not own it.

**terminal::View is the PluginEditor.** Created/destroyed independently of the Session. Parents CodeView for rendering (addAndMakeVisible) but does not own it — Session owns CodeView's lifetime. View listens on Model and calls `Session::drain()` when `screenDirty` fires.

**Keyboard dispatch is centralized.** end::View is a `juce::KeyListener` — catches all key events first, dispatches to the active PaneView. Global actions (prefix key, command palette, tab/pane navigation) handled at end::View. Terminal-specific input (CSI u encoding, selection, scroll) handled by the active PaneView.

**Mouse dispatch is per-component.** JUCE delivers mouse events to the component under the cursor. Each PaneView handles its own mouse events. No centralized interception.

### Model Attachment

Two independent trees. Components that carry state attach to `end::Model` via `jam::ValueTree::Attachment` (RAII graft). Config lives in `config::Model`'s own tree — never grafted to end::Model.

**end::Model tree** (runtime state — nodes only, properties are phase-gated, see §4):

```
END (end::Model root)
  WINDOW                   ← runtime app-level state
  TABS                     ← grafted from Tabs component
    TAB[N]                 ← grafted per tab
      PANES                ← grafted from Panes component (IS the PaneManager tree)
        PANE (uuid)        ← PaneManager leaf
          SESSION           ← grafted from terminal::Model
            VIEW            ← grafted from terminal::View
```

**config::Model tree** (config state — separate tree, NOT grafted to end::Model):

```
CONFIG (config::Model root)
  DISPLAY                  ← colours, font, tab SVG, cursor, shader paths
  KEYS                     ← prefix, prefixTimeout, bindings
  NEXUS                    ← shell, terminal, hyperlinks, daemon, gpu, autoReload
  POPUPS                   ← popup entries
  ACTIONS                  ← custom action entries
  WHELMED                  ← typography, colours, navigation
```

`jam::CodeView` is NOT a `jam::ValueTree::Component` (§2.1) — it has no node in either tree.

Consumers that need runtime state listen on end::Model. Consumers that need config listen on config::Model. Consumers that need both (e.g. terminal::View) register on both — two explicit listener registrations, two distinct concerns. No shadow state. No side channels. No `referTo`.

**Properties are phase-gated.** No property enters either tree before the feature that authors/consumes it is implemented. See each phase in §4 for exact Model additions.

### Resize Path

Same proven pattern from the previous iteration:
- `terminal::View::resized()` → writes new pixel dimensions
- `Controller` owns `jam::Resizer` — coalesces rapid changes via 16ms timer
- Resizer start trigger → `Processor::suspendProcessing(true)` → `Processor::prepare()` (resizes Video grid only) → `Processor::suspendProcessing(false)`
- CodeView re-wraps at new width on next paint via `getWrappedLines(viewWidth)` — no content mutation on resize
- History is untouched. CellFifo is untouched. Only Video grid resizes.

### Naming

| Role | Name | Rationale |
|------|------|-----------|
| App orchestrator | `ENDApplication` (Main.cpp) | Owns config::Model, end::Model, end::View, Nexus. Two independent trees. File watcher. Not namespaced. |
| Config Model | `config::Model` | Independent ValueTree (NOT jam::Model — no atomics, no flush). CONFIG tree + sol2 VM (private). Composed from `config::Display`, `config::Nexus`, `config::Keys`, etc. — consumers read config::Model only. |
| App state SSOT | `end::Model` | `jam::Model` + `Instance<end::Model>`. Runtime state tree. Paired with `end::View`. |
| App surface | `end::View` | `juce::KeyListener` + `juce::Desktop::FocusChangeListener`. Owns Tabs, LookAndFeel. Centralizes keyboard dispatch. |
| Session host | `Nexus` | Owns all Sessions. Manages lifecycle. Routes IPC in daemon mode. |
| Terminal instance | `terminal::Session` | = AudioProcessor. Owns Model, Processor, CodeModel, CodeView. |
| Per-session state | `terminal::Model` | = APVTS. Atomics (reader), ValueTree (message). Owned by Session. |
| Reader pipeline | `terminal::Processor` | = ProcessorChain. Owns Parser, Video, CellFifo. References Model. |
| Terminal GUI | `terminal::View` | = PluginEditor. Controller of CodeView. Listens on terminal::Model + config::Model. Detachable. |
| Document renderer | `jam::CodeView` | Dumb jam_gui widget. Cell-space API. No state, no tree, no listener. |
| Pane base | `PaneView` | Base for terminal::View, whelmed::View. |

---

## 2.1 CodeView — Dumb Widget Contract (TETRIS E-Contract)

`jam::CodeView` lives in **jam_gui**. It is a generic library widget, reused by terminal and WHELMED and every future editor. It must therefore be a dumb widget — the same contract a DSP core obeys (TETRIS): private state, validated setters, every setter calls `calc()`, no reaching out. It is **not** a `jam::ValueTree::Component`. It grafts no node. It listens to no tree.

### Why CodeView authors nothing

Every value CodeView renders is born elsewhere and pushed in via a setter:

| What CodeView renders | Genuine author | Reaches CodeView via |
|---|---|---|
| Document content | `Controller::drain()` from CellFifo (reader→message) | CodeModel mutation |
| Cursor / caret | **Video** (VT engine, reader thread) | terminal::Model atomic → flush → View `vtpc` → `setCaret()` |
| Font metrics | config (`config::Model`) | end::Model → terminal::Model → View → `setFont()` |
| Viewport width | `terminal::View::resized()` | `setViewportWidth()` |
| Selection coordinates | input handlers (in the View) | `setSelection()` |

No row has CodeView as author. The cursor's true author is Video on the reader thread; CodeView authoring it would be a lie. This is why CodeView holds only transient, derived render state (the wrapped-projection cache) — analogous to a DSP core's runtime delay lines, never authoritative parameters.

### Selection split

- **Selection TYPE** (none/visual/visualLine/visualBlock) → genuine app-level state. Lives on `TABS` (end::Model). Observed by StatusBarOverlay, gates input dispatch. Cross-component → stays in the SSOT tree.
- **Selection COORDINATES** (anchor/end cells) → transient, message-only, consumed only by CodeView rendering and Controller copy. Never crosses threads, never persists. Owned by CodeView as a pure cell-space value (`CodeView::Selection`). Not in the SSOT tree. `pack()`/`unpack()` provided as the uniform value-type escape hatch if a coordinate ever must enter Model.

### CodeView public API (cell-space only)

```
setCaret (jam::Cell::Point)            — Controller translates from Video-grid first
setSelection (Selection)               — cell-space anchor/end
getSelection () -> Selection           — Controller pulls for copy
setViewportWidth (cell)                — drives the wrapped projection
// every mutator calls calc()
```

CodeView owns the wrapped projection (document line → screen rows via `getWrappedLines`) internally. It exposes **no pixel methods**.

## 2.2 Coordinate Spaces — Three, Never Conflated

This is where the previous iteration bled (the D15 caret bug). Three spaces; conversions are owned, never improvised.

| Space | Coordinates | Owner |
|---|---|---|
| **Video-grid** | `(gridRow, gridCol)`, viewport-bounded, no wrapping (autowrap already split rows) | Video (reader thread), packed into terminal::Model |
| **Document** | `(lineIndex, col)` over CodeModel logical lines (history + live tail), `col` may exceed viewport width | CodeModel (owned by Controller) |
| **Screen/pixel** | wrapped projection; one document line → N screen rows via `getWrappedLines(viewWidth)` | CodeView (owns the projection) |

### Conversion authority — HARD RULE

**`jam::Cell::Point::fromPixel(pixel, cellWidth, cellHeight)` / `toPixel(...)` is the ONLY sanctioned pixel↔cell conversion.** Hand-rolled arithmetic (`pixelX / cellWidth`) is **forbidden**. `cellWidth`/`cellHeight` come from the DISPLAY node.

| Translation | Owner | Mechanism |
|---|---|---|
| pixel ↔ cell | `jam::Cell` | `Cell::Point::fromPixel` / `toPixel` — the only converter |
| Video-grid → content cell-row | **Controller** | cell-space row arithmetic via `liveTailExtent` (logical addressing, not pixel conversion) |
| document → screen/pixel | **CodeView** | the wrapped projection (`getWrappedLines`) |

The mouse handler converts a pixel point to a cell coordinate with `jam::Cell::Point::fromPixel` and nothing else. CodeView never sees a Video-grid coordinate; the Controller translates at the boundary. WHELMED reuses CodeView with the identical cell-space API and never touches Video-grid space at all.

---

## 3. Module Foundation — JAM

END depends on JAM framework modules. These exist independently and are consumed by all JAM-based projects (END, TIT, CAKE, WHATDBG).

### jam_core
Shared utilities: `Instance<T>`, `Owner<T>`, `BufferSPSC`, `Resizer`, `AnyMap`, `Function::Map`.

**Identifier system:** X-macro convention (`IDENTIFIER_*(X)`) expanded via `MAKE_VIEW` into `jam::ID` (Identifier), `jam::IDref` (StringRef), `jam::IDtag` (uppercase), `jam::IDtype` (uppercase Identifier). All identifiers across all jam modules share the same `jam::ID` struct — no per-module namespaces. Domain-specific identifier blocks are defined in their owning module and expanded alongside the rest. See §3.1.

### jam_graphics
Foundation types and rendering:
- `jam::Char` — 8-byte packed attributed character atom. Absorbs CharProps/Charset as static methods (single codepoint→cell authority).
- `jam::Row` — FAM struct: `Char chars[]` + metadata.
- `jam::Stamp` / `jam::Grapheme` — `SharedResource<T>` interning tables (style, cluster).
- `jam::Cell` — coordinate unit (Point, Rectangle). Not character content.
- `jam::String` — attributed string line (`HeapBlock<Char>` + cellCount + wrap metadata).
- `jam::CodeLine` — legacy alias, equivalent to String.
- Font pipeline: `Typeface`, `Font`, `Atlas`, `FontCollection`, `GlyphConstraint`, `BoxDrawing`.
- Glyph rendering: `glyph::Arrangement` (shaping), `glyph::Graphics` (compositing).

### jam_gui
- `jam::Model` — APVTS-analog state owner (ValueTree by value, atomic parameter map, timer flush).
- `jam::ValueTree` — integration bag: `Component` (owns node), `ComponentWithID<T>` (CRTP, TYPE + optional UUID), `Attachment` (RAII graft).
- `jam::CodeView` — dumb monospace document widget. Cell-space setter/getter API (`setCaret`, `setSelection`/`getSelection`, `setViewportWidth`), every mutator calls `calc()`. Owns the wrapped projection internally. NOT a `jam::ValueTree::Component`; no tree, no listener, no pixel methods. Reused by terminal and WHELMED. See §2.1/§2.2.
- `jam::CodeModel` — multi-screen dimensionless document model. `ParagraphsModel` per screen.
- `jam::ParagraphsModel` — bounded deque of `ParagraphStorage`. Neovim-style line-indexed API: `insert`, `remove`, `set`.
- `PaneManager` — binary tree ValueTree layout engine.
- `PaneResizerBar` — draggable divider.
- `button::Group` — radio button strip with sliding indicator animation.
- `button::TabButton` — tab button with drag-reorder and inline rename.
- `TabbedComponent` — content panel host backed by `button::Group`.
- `Window` — glass overlay window.

### jam_terminal (NEW — per RFC-jam-terminal-extraction)
The VT engine extracted as a reusable module. Foundation for END and all future JAM TUI applications (TIT, CAKE).

- `parser/` — `Parser` (DFA byte decoder), `DispatchTable` (O(1) state machine), `CSI` (parameter accumulator)
- `video/` — `Video` (base VT command processor, no app coupling, protected virtual hooks), `CursorState`, `Winsize`
- `transport/` — `CellFifo` (two-ring SPSC: history + active)
- `cell/` — `Palette` (256-slot mutable colour table)
- `tty/` — `TTY` (abstract base + reader thread), `UnixTTY` (forkpty), `WindowsTTY` (ConPTY, sideloaded, NtCreateNamedPipeFile). Constructor takes config path for conpty extraction dir — no app coupling. Event identifiers (`data`, `drainComplete`, `shellExited`) in `jam::terminal::`.
- `ui/core/` — `Writer`, `Graphics`, `Screen`, `Component`, `escapes`, `Metrics`, `Input`, `KeyPress` (absorbed from jam_tui)
- `ui/widgets/` — `Label`, `Menu`, `ListPane`, `SplitPane`, `Dialog`, `Console`, `TextPane`, `Spinner`, `ThemeResolver`, `Braille`, `MarkdownRenderer`, `LookAndFeel` (absorbed from jam_tui)

Video base class exposes protected virtual hooks — END's `terminal::Video` subclass overrides to fire application events. The base carries zero knowledge of ValueTree or END's application namespace.

### 3.1 Identifier Architecture — Single Namespace, Module-Owned Definitions

All jam identifiers live in `jam::ID`. There is no `terminal::id::` namespace, no `app::id::` namespace in the rewrite. The X-macro system defines identifiers in domain-specific blocks, each owned by its declaring module, all expanded into the same `jam::ID` struct.

**Convention:**
```
// jam_terminal/identifier/jam_identifier_terminal.h
#define IDENTIFIER_TERMINAL(X) \
    X(activeScreen,     "activeScreen") \
    X(cursor,           "cursor") \
    X(cursorShape,      "cursorShape") \
    X(keyboardFlags,    "keyboardFlags") \
    X(originMode,       "originMode") \
    ...
```

The module header (`jam_terminal.h`) includes `jam_identifier_terminal.h`. The `MAKE_VIEW` expansion in `jam_core/identifier/jam_identifier.h` includes `IDENTIFIER_TERMINAL(EXPANDER)` alongside all other domain blocks. Result: `jam::ID::activeScreen`, `jam::ID::cursor`, etc. — uniform access, no namespace prefix at call sites.

**Identifier ownership by module:**

| Module | Block | Covers |
|--------|-------|--------|
| `jam_core` | `IDENTIFIER_CORE`, `IDENTIFIER_DATA`, `IDENTIFIER_PARAMETERS`, ... | Framework-wide: `id`, `value`, `type`, `name`, `data`, ... |
| `jam_terminal` | `IDENTIFIER_TERMINAL` | VT state machine: screen state, DEC modes, cursor, OSC params, transport events (`data`, `drainComplete`, `shellExited`), transient atomics |
| `jam_gui` | `IDENTIFIER_LAYOUT`, `IDENTIFIER_UI_COMPONENTS`, ... | Layout, components, editor |

**END application identifiers** (VIEW node type, hint/preview state, IPC events) live in END's own `AppIdentifier.h` as plain `static const juce::Identifier` — they are not part of the jam X-macro system because they are application-specific, not framework-reusable.

**Why this matters for Model building:** Every `jam::Model` (end::Model, terminal::Model, config::Model) stores its ValueTree properties keyed by `juce::Identifier`. The identifier set IS the state schema. By placing terminal state identifiers in `jam::ID` via `IDENTIFIER_TERMINAL`, the schema is:
- Defined once (X-macro, SSOT)
- Reusable across all jam_terminal consumers (END, TIT, CAKE)
- Deduplicated with jam_core identifiers (no shadow `id`, `value`, `type` in terminal)
- Accessible at every call site without namespace qualification beyond `jam::ID::`

**What moved vs what was eliminated:**

In the old END, `terminal::id::` held ~100 identifiers mixing three concerns:
1. **VT state properties** (activeScreen, cursor, DEC modes, OSC params) → moved to `IDENTIFIER_TERMINAL` in jam_terminal
2. **Event-map keys** (pushLine, screenDirty, bell, writeToHost, ...) → **eliminated**. Video's event-firing sites become protected virtual hooks on `jam::terminal::Video`. No identifiers needed — method names replace string-keyed dispatch.
3. **END-specific state** (DISPLAY node type, hintPage, preview, splitCol, bytesReceived) → stays in END's `AppIdentifier.h`

---

## 4. Build Sequence

The rewrite is skeleton-first. Phases 1–3 produce a compiling, running JUCE application with every class declared, every ownership edge real, every lifecycle proven by the compiler. Subsequent phases fill implementations into known shapes.

| Phase | Name | Layer |
|-------|------|-------|
| 1 | jam_terminal Extraction | JAM |
| 2 | jam_gui Tab System Rewrite | JAM |
| 3 | END Skeleton | END |
| 4 | VT Pipeline + Rendering + Resize + Scrollback | END |
| 5 | Input + Selection | END |
| 6 | Hyperlinks and URI | END |
| 7 | SKiT Image Preview | END |
| 8 | Whelmed (Edit) | JAM + END |
| 9 | Whelmed (Read) | JAM + END |
| 10 | Whelmed (Mermaid) | END |
| 11 | Popup Terminals | END |
| 12 | Command Palette | END |
| 13 | Native Finder | JAM + END |
| 14 | Shadertoy Shaders | END |
| 15 | Daemon and Session Persistence | END |

### Phase 1 — jam_terminal Extraction
Extract END's VT engine into `jam_terminal` module. Foundation for END and all future JAM TUI applications (TIT, CAKE).

**jam_graphics foundation (prerequisite, compiles first):**
- `jam::Char` absorption — `CharProps`, `Charset`, `CharPropsData` free functions become `jam::Char` static methods. Lookup tables become TU-static data in `jam_char.cpp`. `jam::Char::fromCodepoint(codepoint, styleId, lineDrawing)` is the single cell producer. Width lookup, charset translation, all absorbed. `jam::Char` stays 8 bytes, trivially copyable — `static_assert` unchanged. Charset translation state (lineDrawing flag) passed by caller (Video), never stored in Char.
- `jam::StampEntry` widening — `uint8_t flags` → `uint16_t flags`. New fields: `juce::Colour underline` (SGR 58/59, alpha==0 → follow fg), 3-bit underline style field (none/single/double/curly/dotted/dashed), `OVERLINE`, `SUPERSCRIPT`, `SUBSCRIPT` flag bits. Old `UNDERLINE` single bit removed — replaced by style field. `Hash` updated. `jam::Char` unaffected (8-byte invariant, `styleId` indirection).
- `jam::Row::chars[]` rename (from `cells[]`). `FlexType = Char`.
- `IDENTIFIER_TERMINAL` X-macro block — all VT state machine identifiers defined here, expanded into `jam::ID` (§3.1).

**jam_terminal module creation (depends on jam_graphics):**
- `parser/` — `Parser` (DFA byte decoder), `DispatchTable` (O(1) `(ParserState, byte) → (nextState, ParserAction)` table, immutable after construction), `CSI` (trivially copyable parameter accumulator, `static_assert`). Namespace → `jam::terminal`.
- `video/` — `Video` base class: VT command processor, owns `jam::Buffer<jam::Row>` (dual channel normal/alternate), pen state, cursor, modes, scroll region, tab stops, grapheme segmentation state. **No app coupling.** Where the old Video fired events via `jam::Function::Map<juce::Identifier, void>`, the base class calls **protected virtual hooks** (e.g. `onLineDeparted`, `onScreenDirty`, `onBell`, `onTitle`, `onCwd`, `onClipboard`, `onRegisterLink`, `onCursorFlush`, `onModeFlush`, etc.). END's `terminal::Video` subclass overrides these to fire into `terminal::Model`. The base carries zero knowledge of ValueTree, `juce::Identifier`, or END's application namespace. Also: `CursorState` (packed int32), `Winsize` (packed int64). RFC-missing-video-dispatch implemented here: full SGR sub-parameter disambiguation via `CSI::isSubSeparator()`, OSC 4/10/11 set/query, DECRQSS (`reportStatusString` via existing `sendResponse` path).
- `transport/` — `CellFifo` (two independent `jam::BufferSPSC` rings: history + active. Producer-side drop-oldest. Seqlock per slot. `pushHistory`/`pushActive` on reader, `drainHistory`/`drainActive` on message).
- `cell/` — `Palette` (256-slot `std::array<juce::Colour, 256>`, seeded from ANSI_16 + cube + gray formulas, mutable via `setPaletteColour` for OSC 4).
- `tty/` — `TTY` abstract base (owns reader thread, `jam::Function::Map` event dispatch: `jam::ID::data`, `jam::ID::drainComplete`, `jam::ID::shellExited`). `UnixTTY` (forkpty, macOS/Linux). `WindowsTTY` (ConPTY via sideloaded `conpty.dll` + `OpenConsole.exe` from BinaryData, `NtCreateNamedPipeFile` full-duplex pipe, overlapped I/O, `PSEUDOCONSOLE_WIN32_INPUT_MODE`). Constructor takes config path for conpty extraction dir — no app coupling.
- `ui/` — jam_tui fully absorbed into `jam_terminal/ui/`. `Writer`, `Graphics` (grid backed by `jam::Buffer<jam::Row>`, style sidecar deleted, intern via `jam::Stamp`), `Screen`, `Component`, `escapes` (`namespace ANSI` → `jam::terminal`, `CURSOR_MARKER` `caroline` literal removed), `Metrics`, `Input`, `KeyPress`, widgets (Label, Menu, ListPane, SplitPane, Dialog, Console, TextPane, Spinner, ThemeResolver, Braille, MarkdownRenderer, LookAndFeel). `jam_tui` module header deleted.

**Exit state:** `jam_terminal` compiles. `jam_graphics` foundation changes compile. END does not yet consume either.

### Phase 2 — jam_gui Tab System + PaneManager Fix
Rewrite TabbedComponent. Fix PaneManager resizer bar lifecycle. Required for Phase 3 animated tabs and correct split panes.

**button::Group (sync from kuassa + extend):**
- `addButton(unique_ptr<Button>, bool isFreeButton = false)` — isFreeButton buttons are not part of the radio group (used for overflow Options button).
- `ID::groupButton` property guards in `valueChanged`, `snapIndicator`, `animateIndicator` — only group buttons (not free buttons) participate in selection.
- `removeButton(int index)`, `moveButton(int from, int to)` — new, needed for dynamic tabs.
- `onButtonMoved`, `onButtonRightClicked` callbacks.
- Index-based selection — `juce::Value` stores int index (not string). `setCurrentIndex` / `getCurrentIndex`.
- `SlidingIndicator` — animates to active button bounds, z-ordered behind buttons (`indicator.toBack()`), 120ms default.

**button::Options (fork from kuassa):**
- Popup-menu-on-click component. `std::map<int, String>` menu model. Owned trigger button. `wireButton()` → `onClick` shows `PopupMenu` async. `juce::Value selectedItem`.
- Added to Group as free button (`isFreeButton = true`). When total tab-button width exceeds Group bounds, Options appears with hidden tabs as popup menu items.

**button::TabButton:**
- `juce::Button` subclass. Drag-reorder (mouseDown/Drag/Up, `dragThreshold = 5`, calls `owner.moveButton`). Inline rename (`showRenameEditor`, `juce::Label`, `onRenameCommit` callback). `paintButton` delegates to LookAndFeel.

**TabbedComponent rewrite:**
- Backed by `button::Group` (replaces TabbedButtonBar entirely). Content area geometry via `getContentArea()`. `createTabButton` virtual override point. `addTab`, `removeTab`, `moveTab`, `setTabName`, `setCurrentTabIndex`. `currentTabChanged` virtual callback.

**LookAndFeel surface:**
- `drawButtonGroupSlidingIndicator` — renders indicator 3-slice SVG (active highlight, moves).
- `drawButtonGroupTrack` — renders bar background.
- `drawTabButton` — renders static tab shape (button 3-slice SVG). Hover: filled. Not hover: stroked.
- SVG element layout: single SVG file, 6 elements (`button-left/center/right` + `indicator-left/center/right`). 3-slice stretch algorithm: `scaleFactor = drawArea.height / svgRowHeight`, left cap + right cap fixed, center stretches.
- Default: embedded binary SVG from `BinaryData::default_tab_button_svg`. User override via `display.tab.button_svg` config path.

**Deletion:** `jam::TabbedButtonBar`, `jam::TabBarButton` — replaced entirely.

**PaneManager resizer bar lifecycle fix:**
- Root cause: `PaneManager::remove()` restructures the tree (promotes sibling, collapses nodes). Resizer bars hold a reference to the old split node which becomes detached. The orphan-scan cleanup (checking `splitNode.getParent().isValid()`) misses the case where the node was *replaced* by sibling promotion.
- Fix: resizer bar lifetime is **RAII-bound to the split node**. `PaneManager::remove()` creates/rebinds resizer bars as part of the restructure. No post-hoc orphan scan. Bar creation paired with split node creation. Bar destruction paired with split node removal. (DEBT-20260602T204214 resolved.)
- Split asymmetry (DEBT-20260501T193217) investigated and resolved alongside — same `remove()` path.

**Exit state:** `jam::TabbedComponent` works with animated `button::Group` sliding indicator, SVG 3-slice tab rendering, drag-reorder, overflow menu. PaneManager `remove()` correctly maintains resizer bars. END does not yet consume either.

### Phase 3 — END Skeleton
Working JUCE GUI application. Tabs split, panes navigate, keyboard and mouse input works, config drives visual appearance. Every terminal class exists as a compiled stub with correct ownership.

**Functional (working, testable by ARCHITECT):**
- `ENDApplication` (Main.cpp) — orchestrator. Owns `config::Model`, `end::Model`, `end::View`, `Nexus`. Owns `jam::Stamp`, `jam::Grapheme` contexts. IS the `jam::File::Watcher::Listener`. Two independent trees: config::Model (config) and end::Model (runtime).
- `config::Model` — `jam::Model`. Parses **display.lua** (colours, font family/size, tab SVG, pane bar) and **keys.lua** (prefix key, bindings for pane/tab actions). Other modules (nexus, popups, actions, whelmed) not yet parsed.
- `end::Model` — `jam::Model` + `Instance<end::Model>`. App state SSOT. CONFIG subtree grafted by Main.
- `end::View` — `juce::KeyListener` + `juce::Desktop::FocusChangeListener`. Owns `Tabs`, `LookAndFeel`, GL context. Centralizes keyboard dispatch to active PaneView. Writes `activePaneID` on focus change.
- `Tabs` — `jam::TabbedComponent` (button::Group, SVG 3-slice, sliding indicator). `jam::ValueTree::Component` — TABS node grafted into end::Model via Attachment.
- `Panes` — per-tab container. `jam::ValueTree::Component` — PANES node grafted into TAB via Attachment. `PaneManager` binary tree (resizer bar lifecycle fix from Phase 0b), `PaneResizerBar`, `Owner<PaneView>`. Split horizontal/vertical, pane navigation (h/j/k/l).
- `PaneView` base — `jam::ValueTree::ComponentWithID<PaneView>`
- `Nexus` — `Instance<Nexus>`. Minimal working: `create`/`remove`/`get` map. `Mode::standalone`. Stub Controllers created on tab open, destroyed on tab close. Lifecycle proven.
- `action::Registry` — functional. Built-in pane/tab actions registered (split_horizontal, split_vertical, close_pane, close_tab, new_tab, pane_left/right/up/down, next_tab, prev_tab). Prefix key state machine working. Key map built from keys.lua config. Lua custom actions dispatch through Registry (no DisplayCallbacks).
- `LookAndFeel` — style-driven from CONFIG/DISPLAY (colours, tab SVG, pane bar). Hot-reload: lua file change → Main re-parses → CONFIG in place → LookAndFeel reacts.
- GL pipeline — `renderOpenGL()` with background shader slot and post-process FBO capture slot (empty, compiling). JUCE GL compositing via `setComponentPaintingEnabled(true)`.

**Stubs (compiled, owned, empty — filled in Phase 4+):**
- `terminal::Session` — owns Model, Processor, CodeModel, CodeView. Empty internals.
- `terminal::Model` — `jam::Model`, empty (no parameters yet).
- `terminal::Processor` — references Model, empty.
- `terminal::Video` — `jam::terminal::Video` subclass, empty overrides.
- `terminal::View` — `PaneView` subclass. `jam::ValueTree::Component` — VIEW node grafted into SESSION via Attachment. Handles mouse events (per JUCE delivery). Empty terminal logic.
- `LinkManager` — empty.
- `CodeModel` — wired to CodeView, empty content.
- `jam::CodeView` — dumb widget, cell-space API (§2.1), no tree, parented by View.

**Model additions (Phase 3):**
```
config::Model tree:
  CONFIG
    DISPLAY       ← colours, font family/size, tab SVG, pane bar (display.lua)
    KEYS          ← prefix, prefixTimeout, bindings (keys.lua)

end::Model tree:
  WINDOW
    width, height ← authored by end::View::resized()
    zoom          ← authored by zoom actions
    renderer      ← authored by config (gpu/cpu, read from config::Model at init)
  TABS
    activeTab     ← authored by Tabs (tab switch)
    activePaneID  ← authored by end::View (FocusChangeListener)
```

**Trees at Phase 3 exit:**
```
end::Model (root)                         config::Model (root)
  WINDOW   ← width, height, zoom, renderer    CONFIG
  TABS     ← activeTab, activePaneID            DISPLAY ← colours, font, tab SVG, pane bar
    TAB[N]                                       KEYS    ← prefix, prefixTimeout, bindings
      PANES ← PaneManager tree
        PANE(uuid)
          SESSION ← terminal::Model (empty)
            VIEW  ← terminal::View (empty)
```

**Exit state:** Application launches. Window with animated tabs (add, close, reorder, SVG-styled). Panes split and navigate via keyboard (prefix key + h/j/k/l). Config hot-reloads visual appearance. Mouse input reaches the active PaneView. Focus tracking writes activePaneID. Every terminal stub compiled and correctly owned. No terminal content, no VT processing.

### Phase 4 — VT Pipeline + Rendering + Resize + Scrollback ✅ COMPLETE

The full terminal rendering pipeline end-to-end, Vulkan rendering as first-class architecture, and content preservation across resize.

**Completed scope:**
- `terminal::Video` — override virtual hooks, fire events into `terminal::Model`. Full SGR (underline styles/color, overline, super/subscript), OSC 4/10/11, DECRQSS.
- `terminal::Processor` — owns Parser, Video, CellFifo, TTY (from jam_terminal). References Model. Reader thread pipeline. `prepare()` resize. `suspendProcessing()`.
- `terminal::Session` — owns Processor (unique_ptr), Model, CodeModel, CodeView, Resizer. Factory methods. `start()` deferred init. `drain()` facade (pulls CellFifo into CodeModel).
- `terminal::View` — parents CodeView, listens on Model, calls `Session::drain()` on screenDirty, resize path.
- `terminal::Model` — full APVTS atomic parameter set, timer flush, ParameterText.
- **Vulkan Rendering (first-class architecture)** — `jam::VulkanEngine` owns Device, Typeface, Stamp, Grapheme, Link, GlyphAtlas, per-window Graphics. Dual-engine dispatch: GPU (Vulkan LLGC) or CPU fallback (software + shared atlas). Never-null factory. Font/atlas GL-thread binding resolved via message-thread-synchronous paint dispatch (no dedicated GPU thread).
- Typeface system wired — full glyph pipeline: Atlas, HarfBuzz shaping, FontCollection, BoxDrawing, GlyphConstraint, embolden, platform font dispatch (CoreText/FreeType).
- CodeModel/CodeView — ParagraphsModel, wrap-aware projection, SIGWINCH-safe resize.
- CellFifo drain — history ring (committed lines) + active ring (live viewport), liveTailExtent tracking.
- Resize preservation — Resizer suspends Processor, prepare() resizes Video grid only, CodeView re-wraps lazily. History untouched.
- Scrollback — history lines survive in CodeModel, scroll via CodeView's juce::Viewport.
- Shell integration — OSC 133 auto-inject (zsh/bash/fish/pwsh), working directory tracking.
- OSC suite — 0/2, 7, 8, 9/777, 12, 52, 133.
- Notifications — native desktop (macOS UNUserNotificationCenter, Windows/Linux fallback).
- **Conformance suite:** All VT correctness tests passing (relocated to tests/conformance/).

**Phase 4 architecture artifact:** Conformance suite + proven APVTS-analog threading model + Vulkan rendering foundation.

### Phases 5-13 ✅ COMPLETE

**Phase 5 — Input + Selection** ✅
- Keyboard: CSI u encoding (full flag stack per screen), selection keys, scroll navigation
- Mouse: SGR forwarding, drag selection, click dispatch, wheel scroll
- Selection: visual/line/block modes, keyboard selection in scrollback, copy to clipboard

**Phase 6 — Hyperlinks and URI** ✅
- Link scanning: heuristic + OSC 8 (cell-native)
- LinkDetector, LinkManager, hint mode, directory navigation
- File dispatch: OS default, editor, Whelmed, image preview

**Phase 7 — SKiT Image Preview** ✅
- Sixel, Kitty, iTerm2 decoder/renderer
- Inline rendering, LRU texture eviction

**Phase 8 — Whelmed (Edit)** ✅
- Markdown editing in monospace CodeView
- Style-resolution pass (ParsedDocument → styled CodeModel)
- Config-driven theme

**Phase 9 — Whelmed (Read)** ✅
- `jam::TextView` widget for proportional rendering
- Styled markdown display (headings, lists, tables, inline code)
- Edit↔read toggle

**Phase 10 — Whelmed (Mermaid)** ✅
- Mermaid parser and renderer
- Inline diagram composition (text + code + diagrams, single scroll)

**Phase 11 — Popup Terminals** ✅
- ModalWindow + PTY, Lua-configurable
- Accessible via prefix key or command palette

**Phase 12 — Command Palette** ✅
- Fuzzy-searchable action launcher
- Lists: built-in + popup + custom Lua
- Inline shortcut remapping

**Phase 13 — Native Finder** ✅
- `jam::Fuzzy` integration
- Sources: files, scrollback, history, actions
- Match highlighting, preview pane, status bar

### Phase 14 — Advanced Shader Pipeline 🔄 IN FINAL TESTING

User-configurable multi-format shader pipeline (background + post-process) with advanced resource loading.

**Completed scope:**
- **Shadertoy compatibility** ✅ — fullscreen quad rendering with standard uniforms (`iTime`, `iResolution`, `iMouse`, `iFrame`, `iTimeDelta`)
- **RetroArch Slang-P format** ✅ — multi-pass shader chains with buffer feedback, scaling modes (integer/viewport/custom), filter modes
- **Background + post-process pipeline** — background renders before JUCE composition; post-process captures composited scene via FBO, applies shader, composites result
- **Hot-reload** — shader source files watched, recompilation on change with error reporting
- **Vulkan SPIR-V compilation** — `shaderc` (vendored) compiles GLSL/SPIRV sources; pipeline cache persisted, driver/OS updates silently invalidate
- **Resource loader (WIP)** 🔄 — extra resources (OBJ geometry, PNG/JPG images, custom data files) loadable within shaders
  - OBJ loader — triangulated meshes with vertex pulling
  - Image sampler binding — external textures in bindless set
  - Per-pass resource lifecycle

**Shader framework features:**
- Multi-pass chains with history buffers
- Feedback loops (buffer-to-buffer)
- Custom resolution scaling per pass
- Linear vs nearest filter selection
- GLSL `#include` support via prelude injection
- Vendor-specific extensions (macOS extensions, Windows DX/VK feature sets)

**Model additions (Phase 14):**
```
config::Model tree:
  CONFIG
    DISPLAY
      background        ← shader project path (display.lua)
      postProcessing    ← shader project path (display.lua)
      backgroundParams  ← opacity, resolution, frameRate (runtime)
      postProcessParams ← opacity, resolution, frameRate (runtime)
```

**Exit state:** User can drop in Shadertoy-compatible GLSL, RetroArch Slang-P multi-pass chains, or custom shader projects with resource assets. Advanced passes with geometry and texture loading. Hot-reloadable. Final refactor and testing in progress.

### Phase 15 — Daemon and Session Persistence ⏳ PENDING

Detachable GUI, session persistence, multi-window.

**Planned scope:**
- `Nexus` — full implementation: `Mode::daemon`, `Mode::client`, events ValueTree lifecycle listeners
- `nexus::Daemon` — TCP server, per-client Channel, session callback wiring, broadcast
- `nexus::Link` — client connector, retry timer, PDU dispatch
- Wire protocol — binary PDU (hello, create, output, input, resize, detach, kill, stateUpdate, sessions)
- State serialization — `getStateInformation` / `setStateInformation` (Grid+State snapshot)
- Session restore on launch
- Platform — macOS dock icon hiding, Windows Job Object cascade-kill, daemon port file
- Multi-window — `Cmd+N` spawns new instance

**Model additions (Phase 15):**
```
config::Model tree:
  CONFIG
    NEXUS
      daemon        ← daemon mode flag (nexus.lua — full parse, extends Phase 3 partial)
    shell         ← program, args, integration
    terminal      ← scrollbackLines, scrollStep, padding*
    gpu, autoReload

end::Model tree:
  WINDOW
    daemonMode    ← authored by Main (resolved from config)
    port          ← authored by Daemon (TCP port written to nexus file)
```

**CONFIG additions (Phase 15):** nexus.lua fully parsed — daemon, gpu, autoReload sections added (Phase 4 parsed SHELL + TERMINAL, Phase 6 added HYPERLINKS). All six lua modules fully parsed: display, keys, nexus, popups, actions, whelmed.

**Exit state:** END is feature-complete.

---

## 5. Performance Targets

### Core Guarantee — Lock-Free Under Full Throttle

The architecture guarantees that **no thread ever locks, waits, stalls, or yields** — even under extreme byte flood. This is not a target to optimize toward; it is a structural invariant enforced by the APVTS-analog design (§1).

**Extreme stream test:** `seq 1000000`, `seq 10000000`, `seq 100000000` — side-by-side with Kitty, Ghostty, and WezTerm. Success criteria:

- **Zero blocking on either thread.** Reader thread pushes CellFifo (drop-oldest SPSC). Message thread drains into CodeModel (bounded by scrollbackLines). Neither thread waits on the other. CellFifo overflow drops oldest — reader never stalls. CodeModel FIFO eviction drops oldest — message thread never grows unbounded.
- **No torn, no orphaned lines.** The viewport is derived from counts: `liveTailExtent` (drained active rows) and `totalRows` (CodeModel document size). No manual index tracking. Last `scrollbackLines` entries always survive — eviction is FIFO from the front.
- **Comparably on par or faster** than Kitty/Ghostty/WezTerm wall-clock time on the extreme seq tests. The architectural advantage: zero lock contention + drop-oldest under flood means the display thread never starves waiting for the reader.

### Throughput Budget

| Path | Constraint | Mechanism |
|------|-----------|-----------|
| Reader → CellFifo | Never blocks | `jam::BufferSPSC` drop-oldest CAS on full. Zero allocation. Zero lock. |
| CellFifo → CodeModel | Bounded by scrollbackLines | FIFO eviction (`ParagraphsModel::insert` pops front when over capacity). |
| CodeModel → CodeView paint | Bounded by visible lines | Wrap-aware projection walks only the visible sub-range. Off-screen paragraphs have shaped caches freed. |
| Font/atlas mutation | Suspend-coordinated | GL-thread binding resolution (§1 Open Seam) — mutation never races the render thread. |

### Latency Targets

| Metric | Target |
|--------|--------|
| Launch to shell prompt | < 100ms |
| Input to PTY | < 5ms (keystroke → TTY write, no intermediate queue) |
| Frame time (GPU, 120fps) | < 5.8ms (70% of 8.33ms budget) |
| Frame time (CPU, 60fps) | < 11.1ms (67% of 16.6ms budget) |

### Memory Budget

| Resource | Budget |
|----------|--------|
| Base (no terminal) | < 30MB |
| Per terminal (10k scrollback) | < 50MB |
| VRAM mono atlas | < 5MB (4096×4096 SingleChannel) |
| VRAM emoji atlas | < 5MB (4096×4096 ARGB) |
| Glyph cache hit rate | > 99% for ASCII |

### Render Budget

| Metric | Target |
|--------|--------|
| Draw calls per frame | 3–4 (background + mono + emoji + shader) |
| Render loop allocations | 0 (pre-allocated buffers) |
| 5K fullscreen (25k cells) | 120fps locked |
| Glyph shaping | Visible lines only (lazy per-paragraph, cached between frames) |

---

## 6. Platform

| Platform | Compiler | PTY | Font Backend |
|----------|----------|-----|-------------|
| macOS (Intel + ARM) | Xcode clang | forkpty | CoreText + HarfBuzz |
| Windows 10/11 | MSVC + clang-cl (MSYS2) | ConPTY (sideloaded) | FreeType + HarfBuzz |
| Linux | GCC/clang | forkpty | FreeType + HarfBuzz |

---

*SPEC v0.0.1 is the complete specification. Each phase section is the RFC for that phase's PLAN. No further elaboration is deferred — what is written here is the ground of truth for execution.*
