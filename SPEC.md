# SPEC.md — END: Ephemeral Nexus Display

**Version:** 0.0.1
**Date:** 2026-06-04 (original), updated 2026-09-12
**Identity revision:** END is a CLAP plugin host. The terminal emulator moved out of
this repository into a hosted plugin.

---

## 0. Identity

END is a **JUCE GUI application** that hosts CLAP plugins in a tabbed, splittable
pane workspace, rendered by the JAM Vulkan engine.

It is not a terminal emulator. The VT engine, the document model, and the terminal
widgets that earlier versions of this SPEC described now live outside this
repository. They return as hosted plugins:

- **eve** — the terminal plugin.
- **whelmed** — the markdown editor/viewer plugin.

END supplies the window, the workspace (tabs, panes, splits, focus), the config
system, the rendering engine, and the plugin lifecycle. Plugins supply content.

### Priority Order

1. **JUCE GUI application.** Component hierarchy, ValueTree state, native
   windowing. Every architectural decision starts here.
2. **CLAP plugin host.** jam_clap is the in-house wrapper and host format. One
   plugin instance per pane. The host stays content-ignorant.
3. **GPU-first rendering.** The JAM Vulkan engine renders every window. Shader
   pipeline (Shadertoy background, Slang post-process, OBJ mesh) is host-level
   decoration.

### Why This Order Is Non-Negotiable

The previous iteration fought JUCE because its mental model was "terminal
emulator", not "JUCE application". That lesson stands for the host: END is a JUCE
application first. The host never absorbs plugin responsibilities. When a feature
belongs to content, it belongs to a plugin.

---

## 1. Architecture

### 1.0 Layer Order (top → bottom)

```
Application → Config → Nexus → Session → hosted CLAP plugins (EditorView panes)
```

- **ENDApplication** (Main.cpp) — orchestrator. Constructs `jam::Window` directly
  (there is no ENDWindow class). Owns config and the Nexus lane.
- **Config** (`Source/config/`) — ConfigDirectory, ConfigModel and its per-domain
  subclasses (theme, shader). Four-phase lifecycle. Data source is markdown.
- **Nexus** (`Source/Nexus.h`) — host singleton. Owns the sessions Model state
  (`Id::sessions` tree: `focused_session`, `focused_pane` parameters), the
  per-session plugin instances, and the per-plugin `Nexus::VirtualClock` demand
  clocks. Creates plugins via jam_clap from `~/.config/end/plugins/`.
- **Session** — uuid-keyed container of hosted plugin instances for one window's
  session. `newPlugin (uuid, pluginId, instance)` / `removePlugin (uuid)`.
- **EditorView panes** — each pane hosts one plugin editor
  (`juce::AudioProcessorEditor` from the CLAP instance). A pane with no loaded
  plugin renders bare (outline only).

### 1.1 View Composition

```
ENDView                                  ← app surface; key dispatch; owns actions
  ├── background (VulkanShaderComponent) ← config-driven background shader
  ├── SessionView : jam::TabbedComponent ← one-visible tab owner, adopts SESSION row
  │     └── TabView[N] : jam::MatrixComponent  ← binary-space pane graph per tab
  │           ├── EditorView[N] : jam::PaneComponent  ← one hosted plugin per pane
  │           └── jam::PaneEdge[N]       ← EDGE rows, draggable seams
  └── MessageOverlay                     ← status/preview overlay painting
```

The Owner/Owned composite is jam's layout law:

- `jam::OwnedComponent` self-reports keyboard focus onto its own row's
  `Id::focus`; it publishes its own bounds. The child stays dumb.
- `jam::OwnerComponent` aggregates: a direct child row's `Id::focus` becoming 1
  writes that child's identity into the owner's focused-child parameter. Focus is
  event-driven; there is no manual focus bookkeeping anywhere above the owner.
- `jam::MatrixComponent` (TabView base) mutates the binary-space graph: `split`
  targets the focused child, EDGE rows name head/tail spaces by uuid.

### 1.2 State — jam::Model Contract

`jam::Model` is the APVTS-analog state owner (owned ValueTree by value, atomic
`Parameter<T>` store, adaptive-rate flush timer, `createAndAddParameter<T>`).
Contract points that bind END:

- Parameter-backed properties accept raw `setProperty` writes on the message
  thread; the adapter reverse-syncs VT→atomic, and the flush timer is CAS-gated —
  it never reverts a raw ValueTree write.
- Two independent trees: the runtime Model (Nexus-owned sessions state) and the
  config Model. No config values on the runtime tree, no runtime state on the
  config tree.
- Focus lane (verified end to end): click → pane takes keyboard focus →
  `Id::focus = 1` on the pane row → owner aggregates into `focused_pane` →
  actions and splits read `focused_pane`. Creation (`OwnerComponent::add`) stamps
  the new child as focused — a state update at creation, not a manual override of
  the event lane.

### 1.3 Config — Markdown Tables, Cast Toolchain

Config data lives in markdown grid tables, parsed by jam_markdown
(`jam::ConfigDocument::parse` → `getValueTree`, validated by
`jam::ConfigValidator`).

- **Sources (deployed, live):** `~/.config/end/display.md`, `keys.md`,
  `themes/<name>/theme.md`. Shipped defaults live at `Source/config/`.
- **Value typing** is per-row (`integer`, `float`, `bool`, `string`, `colour`,
  `numbers`) via `ConfigDocument::getValueTypes`.
- **Authoring rules:** table key cells are property names verbatim. A property
  consumed through a jam lexicon identifier uses jam's spelling (single owner —
  e.g. `lineHeight`). A value cell whose text collides with markdown block syntax
  is backslash-escaped (`\-`, `` \` ``, `\\`) — the parser resolves the escape to
  the literal.
- **Constants pipeline:** `cast/*.md` (project-info, identifiers, bimaps, files)
  → `cast cast/spell.md` → `Source/generated/{ProjectInfo,Identifiers,Bimaps,
  Files,Generated}.h` (global `Id::`, `map::`, `files::`). The generated lexicon
  defines only what jam's lexicon does not — shared identifiers have exactly one
  owner: jam.
- **Key bindings** (`keys.md`): direct bindings (cmd/ctrl) fire immediately;
  modal bindings require the prefix key first. `ENDActions::buildKeyMap` reads
  the KEYS tree via `juce::KeyPress::createFromDescription`; rebuilt on any
  config change.

### 1.4 Rendering — First-Class Vulkan Architecture

`jam::VulkanEngine` (Application-owned) is the rendering authority and the
unified resource-ownership tree: shared `jam::VulkanDevice`, the
Typeface/Stamp/Grapheme/Link interning tables, shared `jam::GlyphAtlas`, plus the
per-window `jam::VulkanGraphics` collection. Member-declaration order governs
teardown.

- **Dual-engine dispatch (never-null factory):** Vulkan LLGC on GPU;
  `jam::LowLevelGraphicsGlyphRenderer` CPU fallback. JUCE's default renderers are
  structurally unreachable.
- **macOS:** MoltenVK (vendored SDK under `Poems/Vulkan/macOS/`), CAMetalLayer
  surface attached once at swapchain creation.
- **Shader pipeline:** background and post-process shader projects under
  `~/.config/end/shaders/<name>/` (Shadertoy convention; optional `.slangp`
  resource manifest for OBJ mesh + textures). Runtime compilation via shaderc;
  engine templates splice with `@token@` placeholders through
  `jam::Format::replaceholder` with an explicit at-sign delimiter.
- Paint dispatch is message-thread-synchronous; no dedicated GPU thread.

### 1.5 Plugin Hosting — jam_clap

- Format and wrapper are in-house (`jam_clap`, JAM repository).
- Plugins live in `~/.config/end/plugins/`. `Nexus::createPlugin (pluginId,
  callback)` resolves and instantiates asynchronously; a missing plugin yields a
  bare pane (`newPlugin (uuid, {}, nullptr)`).
- Each plugin gets a `Nexus::VirtualClock` — a per-plugin demand clock, created
  and removed with the pane.
- `EditorView::createProcessorEditor` hosts the instance's
  `AudioProcessorEditor`; resizable editors track pane bounds.

### 1.6 Main Goal — Shared Vulkan Engine Across Host and Plugins

The central challenge of END-as-host: hosted plugin instances must share the
host's Vulkan engine — one `jam::VulkanDevice`, one atlas/interning family, one
resource-ownership tree serving host chrome and plugin content alike. This is the
next architecture phase; it is unimplemented and undesigned as of this revision.
Design work happens plugin-side and host-side together (see §3 Roadmap).

---

## 2. Actions and Input

- `ENDActions` — key-to-action dispatch. Direct map (cmd/ctrl bindings) and modal
  map (prefix + key), timer-gated modal window. Actions registry
  (`jam::Function::Map`) keyed by generated identifiers.
- Workspace verbs: `new_tab`, `new_pane` (split by edge or append), `close_pane`
  (pane → tab → window cascade), `split_vertical`/`split_horizontal`,
  pane navigation/join/swap, pane resize by `pane_step`, zoom per-pane
  (`zoom` parameter on the pane group), tab rename/next/prev.
- Splits and pane verbs act on the Model's `focused_pane` — never on positional
  or last-created assumptions.

---

## 3. Roadmap

Iteration proceeds both ways between host and plugins:

1. **Freeze current host state** — workspace, config, rendering, focus contract
   verified (this revision).
2. **eve** — terminal as a CLAP plugin. Carries the VT engine, document model,
   CodeView rendering, PTY transport formerly specified here. The terminal-era
   SPEC is preserved verbatim as eve's baseline: `dev/plugins/eve/SPEC.md`.
3. **whelmed** — markdown editor/viewer as a CLAP plugin.
4. **Host iteration** — plugin lifecycle hardening, focus/keyboard routing with
   live editors (DEBT-20260713T230500 lane), session persistence.
5. **Shared Vulkan engine** (§1.6) — the main goal: plugin instances render
   through the host's engine.

Grand-scheme step tracking: `PLAN-END-plugin-host.md` (plugin-host phases),
`PLAN-terminal-editor.md` (eve, gated), `PLAN-cast-migration.md` (landed; Step 9
doxygen regen open).

---

## 4. Platform

| Platform | Compiler | Graphics |
|----------|----------|----------|
| macOS (Intel + ARM) | Xcode clang | Vulkan via MoltenVK (vendored) |
| Windows 10/11 | MSVC + clang-cl (MSYS2) | Vulkan (D3D11 composition lane for the swapchain-less window path) |
| Linux | GCC/clang | Vulkan (Xlib surface) |

---

*This SPEC is the ground of truth for END the host. Content specifications
(terminal semantics, markdown rendering) belong to the plugins that implement
them.*
