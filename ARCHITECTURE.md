# END - Architecture

**Purpose:** Single source of truth for architectural contracts, patterns, and invariants.

**Status:** ACTIVE — MVP pattern formalized. Vulkan dual-engine rendering complete (never-null context factory, GPU + CPU fallback). END is an audio plugin host: `Nexus` owns `Session` instances, each hosting `juce::AudioPluginInstance`s created through the registered format that accepts the plugin file (`Session.cpp:36-51`); `EditorView` panes host each plugin's own `AudioProcessorEditor` (see "Session Layer — Landed Contract"). The terminal-emulator pipeline (CellFifo, `terminal::Processor`, `jam::CodeView`) has moved out of this repository, into the EVE plugin. END still links the `jam_terminal` module (`project-info.md:312`).

**Last Updated:** 2026-11-02

---

## Identity

END is a **JUCE GUI application** and an audio plugin host, end to end (VST2, VST3, AU and CLAP, `Nexus.cpp:66-76`), GPU-rendered through `jam::VulkanGraphicsEngine`. Its default plugin is EVE, under the ENAX contract (see "ENAX" below). See SPEC.md Section 0 for priority order and rationale.

---

## Shared Resources — Globally-Owned Instances

Two JAM engines own the shared rendering resources. Each is a `jam::Instance<T>` and a `juce::DeletedAtShutdown`, so it lives until JUCE's shutdown deletion (`jam_CpuGraphicsEngine.h:30-31`, `jam_VulkanGraphicsEngine.h:72-74`). No `ENDApplication` member holds an engine.

| Engine | Members (declaration order) | Created by |
|--------|-----------------------------|------------|
| `jam::CpuGraphicsEngine` | `typeface`, `glyphAtlas`, `glyphArrangement`, `imageLoader`, `imageOperations` (`jam_CpuGraphicsEngine.h:155-171`) | `CpuGraphicsEngine::getOrCreate()`, called first by `VulkanGraphicsEngine::getOrCreate` (`jam_VulkanGraphicsEngine.h:167-170`) |
| `jam::VulkanGraphicsEngine` | `device`, `textureCache`, `geometryCache`, `pipelineCache`, `imageRenderer`, `blurRenderer`, `contexts` (one `VulkanGraphics` per peer), the post-process shader (`jam_VulkanGraphicsEngine.h:528-600`) | `ENDApplication::initialiseVulkan` → `VulkanGraphicsEngine::getOrCreate (maxImageExtent, …)` (`Main.cpp:250-267`, `:262`) |

**Application member order** (`Main.h:43-63`):
```
generated, shaderFormat, fontRasterizerBackend
    ↓
nexus → config → actions → lookAndFeel
    ↓
window (jam::Window holding ENDView; created in initialise(), Main.cpp:237-240)
```
`initialise` creates the engines (`initialiseVulkan`) before the window. `lookAndFeel.registerTypeface()` runs after the engines exist (`Main.cpp:250-267`).

---

## Layer Separation

```
 Application (ENDApplication, ENDView, SessionView, TabView)
    — orchestrates; owns all top-level lifetimes (Nexus, ConfigModel,
      ENDActions, ENDLookAndFeel, window); creates the two JAM engines
      (jam::CpuGraphicsEngine, jam::VulkanGraphicsEngine) at init
    |
    v
 Config (ConfigModel, ConfigTheme, ConfigShader)
    — markdown files on disk are SSOT, read through jam::ConfigDocument /
      jam::ConfigValidator; ConfigModel is derived state
    |
    v
 Session Host (Nexus)
    — gui-less. Owns ENDModel (the app SSOT), every end::Session keyed by
      uuid, the juce::AudioPluginFormatManager (JUCE default formats +
      jam::ClapPluginFormat), the one juce::AudioProcessorGraph and its
      player and device, and the ENAX extensions (Nexus.h:121-122). Each
      hosted plugin owns its own clock.
    |
    v
 Session (end::Session)
    — gui-less, a juce::ValueTree::Listener on its own state. A change of
      a PANE row's Id::plugin runs loadPlugin (paneRow), which creates the
      plugin asynchronously; the creation callback runs addPlugin (paneRow,
      instance), which adds the node to Nexus's graph, keyed by uuid
      (Session.cpp:53-63, :71-120); removePlugin (uuid) removes the node
      (Session.cpp:141-148).
    |
    v
 Pane / EditorView (jam::PaneComponent)
    — message thread. When its row receives Id::name, it resolves the owning
      Session via Nexus::getSession() and calls createEditorAndMakeActive(),
      parenting the returned juce::AudioProcessorEditor as its sole child
      (EditorView::valueTreePropertyChanged, createProcessorEditor,
      EditorView.cpp:172-214). The pane exists before its plugin loads; the
      editor follows.
```

**Header inclusion rules:**
- Lower layers never know about higher layers. No reverse dependencies.
- Communication flows through explicit APIs only.

---

## Resize Path

### Swapchain / Scene-Target Resize (jam::vulkan)

- Handled per paint inside `VulkanGraphicsEngine::createContext()` (macOS
  `jam_VulkanGraphicsEngine.cpp:374-446`): when the peer's physical
  extent no longer matches the swapchain extent, `Graphics::resize()` recreates
  the swapchain, stencil image, MSAA scene target (color + stencil + resolve),
  and framebuffers at the new extent.
- The session-locked MSAA sample count is NOT re-calibrated on resize.
- No coalescing timer needed — resize happens lazily on the next paint request.

---

## State Trees — Global and Session-Scoped

### Global Owned Instances (Application Lifetime)

Two independent global state trees (both `jam::Instance<T>` globally-owned instances):

```
ConfigModel (Id::config)                    ENDModel (Id::end)
  display (theme, size, zoomStep, ...)        WINDOW (Id::size — packed jam::Size<int16_t>,
  graphics (gpu, fontRasterizer,                      Parameter<int>)
    fontGamma, fontContrast, ...)             SESSIONS — the topology
    BACKGROUND (ConfigShader tree —             (jam::ID::focusedSession; the focused
      shader source properties)                  pane is the TAB row's focusedPane)
    POST_PROCESSING (ConfigShader tree)         SESSION (jam::ID::id — SessionView adopts)
  mouse                                           TAB[N] (TabView row, jam::ID::name,
  keys (one row per command: key, description)          jam::ID::focus, jam::ID::focusedPane)
                                                    PANE (jam::ID::id, Id::name, Id::plugin,
  THEMES (ConfigTheme tree, appended as a                 pluginState (Id::title, Id::foregroundProcess,
                                                          Id::cwd), jam::ID::focus — EditorView row)
    direct CONFIG child — window, style,            EDGE (jam::ID::id, jam::ID::head,
    code, scrollbar, tab, button, overlay,                jam::ID::tail, jam::ID::orientation,
    pane, status_bar, hint, menu,                         jam::ID::proportions — jam::PaneEdge row)
    action_list, flex — flat siblings)          OVERLAY
                                                   ID::message (ParameterText)
```
\* PANE and EDGE rows live under their TAB row. A split re-parents the split
pane's row under its new EDGE row (`MatrixComponent::split`, `setRow`), so a PANE
row can sit under EDGE rows. An EDGE row holds its head and tail SPACE (a PANE row
or another EDGE row) as child rows 0 and 1; the root space is own state's child 0
(`jam_MatrixComponent.h:10-17`).

- **ConfigModel** (globally-owned instance via `jam::Instance<T>`) — config constants. Changes on reload only. Markdown files on disk (`~/.config/end/*.md`) are the SSOT, read through `jam::ConfigDocument`/`jam::ConfigValidator` — see "Config Chain" below. Config tree is derived state, rebuilt from disk on every reload (same code path as init). `BACKGROUND`/`POST_PROCESSING` are first-class `GRAPHICS` children, each a `ConfigShader` (`ConfigDirectory` subclass) holding one shader project's parsed source tree. Font rasterization values (`graphics.font_rasterizer` / `font_gamma` / `font_contrast`) are validated config and hot-reload live.
- **ENDModel** (globally-owned instance via `jam::Instance<T>`) — app-lifetime runtime state, owned by `Nexus`. Changes during app lifetime. State placement follows the Attachment Contract below — placement tokens exist only at the engine tier, never on views.

**Invariant:** No config values on ENDModel. No runtime state on ConfigModel. Consumers that need both register as listener on both trees.

### Session Layer — Landed Contract (2026-07-10)

Supersedes the 2026-07-09 block (which described SessionView/TabView with
per-consumer attachment maps and the dormant `jam::PaneManager` spec — both
dead). Describes the landed code:

- **Owner/Owned composite (jam_gui/layout).** One abstraction owns the
  container contract at every level:
  `jam::OwnedComponent` (`juce::Component` + `jam::Model::Component
  <OwnedComponent>` CRTP mixin) — state-bound child; holds the
  `jam::ID::focus` parameter of its own row and publishes its bounds. It never
  reads or writes keyboard focus.
  `jam::OwnerComponent : OwnedComponent` (abstract) — owns UUID-keyed
  children (`jam::HashMap<jam::UUID, std::unique_ptr<OwnedComponent>>` +
  one `jam::Model::Attachment` per child), owns one focused-child parameter on
  its own row (identifier supplied by the derived strategy) and tells each child
  row its focus, pure virtual `childAdded`/`childRemoved`/`layout`. An owner IS
  ownable — composite recursion, no diamond.
  Strategies: `jam::TabbedComponent : OwnerComponent` (machinery
  `jam::ButtonBar`, one-visible swap, authors `jam::ID::focusedTab`) and
  `jam::MatrixComponent : OwnerComponent` (machinery EDGE rows +
  `jam::PaneEdge` — binary space graph: an EDGE row holds its head and tail
  spaces as child rows 0 and 1, layout = recursive descent from own state's
  child 0, split puts the new EDGE row in the split pane's slot, removal
  collapses the parent EDGE into the sibling space; authors
  `jam::ID::focusedPane`).
  **Matrix verbs (jam_MatrixComponent.cpp):** `getNeighbour(pane, direction)` — ascend to the bounding EDGE, accumulate normalized coordinate through perpendicular ancestors, descend the far side to the leaf whose span contains it (centre-from-proportions, zero pixel reads); `swap(a, b)` — move the two PANE rows into each other's slots, `layout()`; `join(pane, direction)` — three shapes: sibling collapse (shared parent dies), initiator-absorbs (multi-level: accumulated extent folds into proportions, intermediate edges rescaled), mirrored-perpendicular rotate; ineligible returns `UUID::none()`, zero mutation; `rotate(parent, child)` — AVL-style rotation of an EDGE and its child EDGE. `split(edge, position)` — creates new PANE + EDGE and puts the EDGE in the split pane's slot. `setRow(row, newRow)` puts a row into another row's slot under its parent, for `split`, `rotate` and `childRemoved`.
  **Gesture state:** corner drag-to-split/join gesture state held as registered model parameters on the TAB row: `jam::ID::edge` (ParameterText — direction or empty) + `jam::ID::position` (Parameter<float> — normalized split position; -1 sentinel = join mode). TabView carries zero gesture members.
  Leaf: `jam::PaneComponent : OwnedComponent`.
- **Mirror law: the component tree is a pure function of the state tree.**
  `SESSIONS` ↔ `ENDView` (`sessions` map + per-child Attachment), `SESSION`
  ↔ `SessionView : jam::TabbedComponent` (adopts the SESSION row), `TAB` ↔
  `TabView : jam::MatrixComponent` (build-or-adopts its TAB row; sits
  directly in SessionView's children — owner-as-owned), `PANE` ↔
  `EditorView : jam::PaneComponent` (build-or-adopts its PANE row; when the
  row receives `Id::name`, it resolves the owning Session and parents that
  plugin's own `juce::AudioProcessorEditor`, `EditorView.cpp:172-214`).
- **`jam::Model::Component<Derived>` (jam_Model.h) — CRTP build-or-adopt.**
  The 4-param ctor `(model, parentState, type, uuid)` searches
  `parentState`'s direct children for `type` + matching `jam::ID::id`:
  adopts when found, else creates the row, stamps `jam::ID::id` (before any
  `createAndAddParameter` — the grouping-order requirement is internal, no
  caller hand-stamping), and appends under `parentState`. ComponentID is
  stamped via `static_cast<Derived*> (this)` — compile-time enforced
  (`static_assert` on `juce::Component` base), no self param. Fresh and
  restored state ride the same path. The 2-param adopt-existing ctor
  remains for root-level adoption (`ENDView` ↔ WINDOW, `MessageOverlay` ↔
  OVERLAY, `SessionView` ↔ SESSION).
- **`jam::Model::Attachment` — pure connector (JUCE attachment contract).**
  Ctor binds an already-placed row's component (asserts the row has a
  parent) and calls `sendInitialUpdate()` (resized + repaint); dtor
  disconnects only — NEVER `removeChild`. State survives every view death
  (the stateInformation contract). Each parent owns the Attachments of its
  children (`ENDView.attachments`; `OwnerComponent::attachments` for every
  owner below), exactly as a PluginEditor owns SliderAttachments.
- **Row removal is an explicit verb only** — `ID::closeTab` →
  `SessionView::remove (uuid)` → `state.removeChild`. No destructor removes
  state.
- **`end::Session` = engine (P).** SESSION row carries its `jam::ID::id`
  parameter only — zero UI vocabulary, no TABS authoring. Owns the
  uuid-keyed graph node ids of the plugins created for its own PANE rows
  (`Session.cpp:103-120`). `addPlugin` also stamps the row's `Id::name`
  (`Session.cpp:119`). The pane's `EditorView` is the only consumer of a plugin
  instance (`Session::getPlugin`, `EditorView::createProcessorEditor`).
- **Nexus = host.** Owns `ENDModel`, every `Session`
  (`createSession`/`getSession`/`removeSession`/`getActiveSession`), the
  `juce::AudioPluginFormatManager` with JUCE's default formats and
  `jam::ClapPluginFormat` (`Nexus.cpp:77-87`), the one
  `juce::AudioProcessorGraph`, and the ENAX extensions. SESSION row
  placement under SESSIONS happens in `createSession` — engine-side
  authorship, the host verb.
- **Plugin lifecycle is one flow:** a write of `Id::plugin` to a PANE row →
  `Session::valueTreePropertyChanged` removes the plugin of the row, clears the
  row's `Id::name` and runs `Session::loadPlugin (paneRow)` (`Session.cpp:53-63`).
  `loadPlugin` takes the format that accepts the file, gets the plugin
  description from `knownPluginList.scanAndAddFile`, adds the pane id to
  `loadingPanes` and runs `createPluginInstanceAsync` (`Session.cpp:71-101`). The
  callback runs `addPlugin (paneRow, instance)`: the node in the graph, audio and
  MIDI connections, and the row's `Id::name` (`Session.cpp:103-139`). The pane's
  `EditorView` hosts the plugin's editor when the row receives `Id::name`.
  `Session::removePlugin (uuid)` removes the node (`Session.cpp:141-148`). A
  removed row makes `Session` remove the plugins of every pane row below it
  (`Session.cpp:65-69`, `:150-157`). A closed tab removes its views first, then
  the plugins of its pane rows, so no editor outlives its processor
  (`ENDView.cpp:188-193`, `Session.cpp:65-69`).

**Singular focus:** `focusedPane`/`focusedTab`/`focusedSession` are
SINGULAR uuid-valued parameters — the value IS the identity,
`parameterChanged (id, value)` is fully self-describing downstream. Each
`OwnerComponent` is the sole author of its own row's focused-child parameter
(TabView authors `TAB.focusedPane`, SessionView authors `SESSION.focusedTab`). It
writes it in the derived `childAdded` hook, with the survivor on remove, on a
click inside a child (`OwnerComponent::mouseDown`), and on the focus-move action
(`MatrixComponent::focusPane`, run by the `map::MatrixCommandId` pane commands,
`jam_MatrixComponent.cpp:385-406`). Focus is told downward only: on a change of its
focused-child parameter or of its own row's `jam::ID::focus`, the owner sets each
child row's `jam::ID::focus` to 1 when the child is focused and its own row is
focused, else 0 (`OwnerComponent::valueTreePropertyChanged`). ENDView tells the
active session row (`ENDView.cpp:148`). Keyboard focus never writes the Model.
There is no app-level copy of the focused pane: a consumer reads the active tab's
focused child (`SessionView::getActiveTabView()->getFocusedChild()`), so a tab
switch can never leave a stale pane id. No other writer of a `focused*` parameter
may exist.

**No cross-tree references:** ConfigModel → ENDModel dependency is one-way data flow. No upward references.

---

## Parameter System — jam::Model APVTS Analog

jam::Model is a 1:1 APVTS analog for multi-type parameters. Key contracts:

- **createAndAddParameter\<T\>** — creates typed Parameter (int, float, int64_t, ParameterText) in AnyMap, seeds VT property, registers ParameterAdapter. Parameters live on Model forever.
- **ParameterAdapter** (cpp-internal) — bridges Parameter↔VT. Bidirectional: atomic→VT (flush timer), VT→atomic (valueTreePropertyChanged reverse sync). Loopback-guarded, equality-gated.
- **ParameterAttachment** (public) — per-parameter listener bridge. Takes ParameterBase& + callback. Delivers changes on MESSAGE THREAD via AsyncUpdater. Does NOT create/destroy parameters.
- **Model::Listener::parameterChanged** — fires on the calling thread when parameter value changes. Processor, View, and other listeners receive parameter events through this.
- **Flush timer** — adaptive 60/120 Hz. Iterates adapters, writes dirty atomics to VT properties.

### Packed Value Transport — jam::Size

Window dimensions are packed as `jam::Size<int16_t>`, stored on VT as a single int property. One property write = one VTPC = one parameterChanged = atomic resize.

- **Write:** `jam::Size<int16_t> (width, height).toInt()` → `state.setProperty (ID::size, ...)`
- **Read:** `jam::Size<int16_t> { appModel.getValue (Id::toType (Id::window), Id::size) }` → structured binding `auto [w, h] = size`
- **Parameter:** `Parameter<int>` with adapter fires `parameterChanged (ID::size)`

This pattern avoids separate width/height parameters that would fire two events per resize.

---

## Rendering Engine — jam::CpuGraphicsEngine and jam::VulkanGraphicsEngine

Two JAM engines replace JUCE's default renderers for all painting — panes, chrome, and
the hosted plugins' editors under ENAX. `jam::VulkanGraphicsEngine::createContext`,
installed as `juce::ComponentPeer::externalContextFactory`
(`jam_VulkanGraphicsEngine.cpp:18-20`), makes the context of each peer. On Windows the
factory is set only while the GPU is available (`:275-281`).

### Ownership / Lifecycle

- Each engine is a `jam::Instance` and a `juce::DeletedAtShutdown`
  (`jam_CpuGraphicsEngine.h:30-31`, `jam_VulkanGraphicsEngine.h:72-74`). See "Shared
  Resources" above for their members.
- Created **exactly once**, in `ENDApplication::initialiseVulkan()` (`Main.cpp:250-267`):
  `jam::VulkanGraphicsEngine::getOrCreate (maxImageExtent, ProjectInfo::projectName,
  jam::VulkanGraphicsEngine::getFrameBudget(), cacheFile, canUseGpu)` (`:262`). Its
  `getOrCreate` creates the CPU engine first (`jam_VulkanGraphicsEngine.h:167-170`).
  - `maxImageExtent` — `VulkanGraphicsEngine::getPrimaryDisplayExtent()`.
  - `cacheFile` — the pipeline cache under END's own `cache` directory
    (`ConfigDirectory::Config::path`), named by `jam::Format::toFileName`. JAM decides no
    path.
  - `canUseGpu` — the `useGpu` row of the display config.
- `lookAndFeel.registerTypeface()` runs after the engines exist, then the font
  rasterization and the emboldening go to `jam::CpuGraphicsEngine`
  (`Main.cpp:250-267`, `setFontRasterization`, `setEmbolden`).
- **Never reset or reconstructed.** The `useGpu` config event re-applies the background
  and the post-process (`Main.cpp`, `registerEvents`); `isGpuAvailable()` selects them.

### Vulkan Vocabulary — vulkan-hpp (plain vk::)

jam_vulkan speaks vulkan-hpp exclusively — no raw C Vulkan API anywhere in module code.
- **Vendored matched pair** — SDK 1.4.350 header generation (C + `.hpp` set) at
  `jam_vulkan/vulkan/`, pinned identical across machines; `vulkan.hpp`'s
  `VK_HEADER_VERSION` static_assert enforces the pairing at compile time.
- **Config site** — one, `jam_vulkan.h`: `VULKAN_HPP_NO_EXCEPTIONS`,
  `VULKAN_HPP_ASSERT`/`VULKAN_HPP_ASSERT_ON_RESULT` = `jassert`, before the include.
  `-fno-strict-aliasing` on the module TU (AppBuilder.cmake). `vulkan_hash.hpp` opted in
  (std::hash for vk:: handles — ShaderInstance's pipeline cache key).
- **Plain `vk::` types only** — no `vk::raii`/`UniqueHandle`; JAM's own wrappers
  (Buffer/Image, unique_ptr ownership) remain the RAII layer. Dispatch is static
  (link-time symbols, zero indirection).
- **Error policy** — setup/creation paths use `vk::Result`-returning overloads checked
  explicitly, feeding the never-null factory's bool/nullptr propagation (CPU fallback
  intact in release). Enhanced value-returning calls only for enumerations/queries, with
  `.result` checked before `.value`.
- **VMA seam** — VMA keeps C types; raw `VkBuffer`/`VkImage` appear only as
  `vmaCreate*` out-param temporaries, wrapped immediately.
- **Device conformance** — instance requests `VK_KHR_portability_enumeration` (+ flag)
  when present; `VK_KHR_portability_subset` presence-checked before enable; validation
  layer + debug-utils messenger wired in debug builds only (messages → `jam::debug::Log`,
  errors → `jassertfalse`), compiled out of release entirely.
- **Present mode** — `Graphics::selectPresentMode()`: enumerate, prefer MAILBOX, fall
  back FIFO (spec-guaranteed) — replaces the previous hardcoded FIFO.

### Engine Dispatch (per paint)

```
VulkanGraphicsEngine::createContext (peer)        (macOS jam_VulkanGraphicsEngine.cpp:374-446)
  GPU available
    → getOrCreateGraphics (peer): one VulkanGraphics per peer (.cpp:113-144)
    → jam::VulkanLowLevelGraphicsContext
  no Vulkan context made
    → CpuGraphicsEngine::createContext (peer)       (.cpp:442-443)
```

**CPU lane** — `jam::CpuLowLevelGraphicsContext<BaseContext>` decorates a JUCE native
context: the base context draws everything except glyphs, which route through the CPU
engine's `jam::GlyphAtlas` (`jam_CpuLowLevelGraphicsContext.h:30-32`, `:50-51`). The
glyph typeface is the current font's, interned in the binary's own `jam::Typeface`, so a
hosted plugin and END each resolve their own typeface index.

### GPU Draw Architecture

- **Unified vertex pulling** — every quad primitive (rect, image, glyph, clip mask) is one
  80-byte `PrimitiveRecord` appended to a per-frame growable SSBO (set 2), expanded in the
  shared instanced vertex shader from `gl_InstanceIndex` + `gl_VertexIndex`. Triangulated
  path geometry is the only vertex-attribute input (2-float position), triangulated by
  `jam::Earcut` (clean-room single-ring ear clipping) for simple paths.
- **Bindless images** — set 1 is a `texture2D[]` sampled-image array (UPDATE_AFTER_BIND,
  partially bound) + one shared linear sampler. Each texture gets a stable array slot at
  upload; no per-draw descriptor allocation. Slot bookkeeping is per-window (per Graphics)
  — the atlas's `vk::Image`s are shared, their array slots are not. `jam::VulkanBindlessTexture`
  (below) formalizes this shared-image/per-window-slot split as a reusable type — the glyph
  atlas and every external-texture (LUT) producer are both its consumers.
- **Push constants** — one shared 40-byte block: `colour` (opacity pre-multiplied into
  alpha CPU-side; brush alpha × context opacity collapse at emit time), `clip`, `textureScale`.
- **Clip as data** — `State::stencilClipDepth` lives inside the saved/restored state
  struct; every record bakes its stencil reference at emit time. Nested clips intersect
  via INCREMENT_AND_CLAMP/EQUAL. Complex/holed path fills use winding stencil-then-cover
  on an isolated scratch target. Transparency layers own per-layer stencil.
- **MSAA** — sample count calibrated once at init (GPU timestamp measurement against
  `targetFrameBudgetMs`), locked for the session. Transient MSAA color+stencil resolve
  into a single-sample scene target composited to the swapchain. The image-alpha clip-mask
  pipeline enables alpha-to-coverage so mask edges keep sub-pixel coverage (per-fragment
  discard alone would defeat MSAA at the mask boundary).
- **Staging arena** — per-window, offset-allocated from one persistently-mapped buffer;
  growth moves the old buffer to `previousStagingBuffers` (never destroys it) until the
  frame fence; reset after the beginFrame() fence wait. Atlas uploads write into the
  CALLING window's arena — no cross-window staging races, no same-frame overwrite.
- **Pipeline cache** — persistent `vk::PipelineCache` blob at the END-resolved cache file;
  loaded at Graphics init, serialized at shutdown. Driver/OS updates invalidate it
  silently (one cold launch); shader changes simply miss and merge.

### Glyph Pipeline — jam::GlyphAtlas (shared, message-thread only)

One canonical atlas serves both engines. Mono rasterization backend is **user config**,
dispatched through a branchless member-function-pointer table. The backend applies to
embedded fonts (typefaces registered with font data). System fonts and colour fonts do not
use it (see below):

| Backend | Rasterizer | Character |
|---|---|---|
| `edgeTable` | JUCE EdgeTable coverage | unhinted, platform-identical |
| `freetype` | vendored FreeType (unhinted, no stem darkening) | unhinted, platform-identical |
| `native` | CoreText (mac) / DirectWrite (Windows) | OS-native smoothing, per-OS look |

- Coverage conditioning: a gamma+contrast LUT (config: `fontGamma` — sRGB-derived 2.2
  default — and `fontContrast`) applied at the single atlas-write site, backend-blind.
- **Embedded fonts:** `jam::Typeface::getInstance()` is the `typeface` member of
  `jam::CpuGraphicsEngine` (`jam_CpuGraphicsEngine.h:155`). After the engines exist, LookAndFeel registers
  each embedded typeface with the atlas (reached directly via `jam::GlyphAtlas::getInstance()`)
  via `LookAndFeel::getTypefaceForFont()` override, which guarantees the same Typeface object
  identity flows from name-based Font construction into atlas keys.
- System fonts (user config): `jam::Typeface::registerSystemTypeface()` interns an installed
  system font. The atlas draws its glyphs from the JUCE outline
  (`juce::Typeface::getLayersForGlyph` / `getOutlineForGlyph`) through the EdgeTable path,
  whatever the backend setting.
- Color/emoji fonts: never enter the grayscale backends — JUCE ImageLayer path renders
  them into the BGRA emoji atlas.
- Backend/gamma/contrast changes (config hot-reload) go to
  `jam::CpuGraphicsEngine::setRasterization`, then a LookAndFeel change cascades to every
  desktop component (`ENDApplication::setFontRasterization`, `Main.cpp`; one event per
  `fontRasterizer`, `fontGamma`, `fontContrast` row, `registerEvents`). Font family/size
  changes need no handler: they produce new atlas keys and re-rasterize naturally.

### Shaders

GLSL sources under `jam_vulkan/shaders/` are the SSOT — shared declarations live in
include files (`mvp.glsl`, `push_constants_rect.glsl`, `push_constants_glyph.glsl`,
`bindless_texture.glsl`) consumed via glslc's include directive. CMake compiles each
source to its committed `.spv` via per-file custom commands with depfile tracking
(include edits recompile dependents); the `.spv` files are embedded as binary data and
are platform-neutral — compiled once, valid everywhere.

### User Shader Pipeline — Background + Post-Process (Shadertoy-compatible, generalized)

Two user-shader slots, both multi-pass: N named buffer passes (every regular, non-hidden
file in the shader project directory, keyed by its extensionless stem, lexicographic
order; `Common`/`Image` special) + mandatory Image pass. Driven entirely by config
(`display.md` graphics table; `ConfigShader` enumerates the project directory into
its state tree; hot reload = init path):

```
file change (markdown/GLSL) → CONFIG state → ID::background / ID::postProcessing event
  → View funnel — full recompile (setBackground / setPostProcess) or cheap
    param-only path (setBackgroundParams / setPostProcessParams)
  → jam::VulkanShaderCompiler (shaderc, vendored + isolated in jam_vulkan) → jam::VulkanShader → consumer
```

- **`jam::VulkanShader`** — POD descriptor, pure data: `jam::Owner<ShaderPass>`
  (`ShaderPass{name, spirv}`, name-hashed for O(1) lookup) + `contentHash` (wyhash over all pass
  names+bytes — content-derived identity, no counter; identical recompile = cache hit).
  Public const fields, zero getters. Presentation params (opacity, resolution, frame rate)
  are NOT in the descriptor — they travel explicitly (`render (g, shader, opacity,
  resolution)`, `setShader`/`setParams`, `setPostProcess`/`setPostProcessParams`), so
  param-only config changes never recompile.
- **`jam::VulkanShaderCompiler`** (`jam_vulkan/shader/jam_VulkanShaderCompiler.{h,cpp}`) wraps
  each pass with the engine-owned prelude (set-0 bindless declarations, `ShaderUniforms`
  push-constant block — byte-exact mirror of `jam_VulkanShaderUniforms.h`, `channels[21]`
  array —, one named sampler macro per buffer pass (`texture (BufferA, uv)`) + `iChannel0-3`
  aliases to the first four passes for Shadertoy paste-compat, through linear/nearest per
  `filter` — compile-time, so `filter` changes recompile). Prelude/macro/`main()` text is
  BinaryData-backed template files (`jam_vulkan/shader/shaderToy*.frag`) substituted via
  `jam::Format::replaceholder`, never inline string literals. `jam_vulkan` owns no
  consumer-specific property-name identifiers — every caller supplies its own common/image
  property names and background/post-processing mode explicitly (`compile()`'s parameters);
  this keeps the compiler decoupled from any one consumer's `Identifier.h` (JAM is a shared
  framework, consumed by more than one project). Compiles via `shaderc_combined`, vendored
  and isolated under `jam_vulkan/shaderc/<platform>/` (`AppBuilder.cmake`'s `IMPORTED` static
  target, gated on `_ca_mod STREQUAL "jam_vulkan"` — no machine SDK lookup, no
  `$ENV{VULKAN_SDK}`/`/usr/local` discovery). Compile failure → `debug::Log` + nullptr;
  callers keep the last-good shader. Opacity semantics differ per slot in the generated
  `main()`: background = `userColor.a * opacity` (transparency); post-process = an
  rgb-only effect-intensity mix against the STRAIGHT-alpha scene, re-premultiplied by
  the scene's own immutable alpha afterward — `vec4 scene = texture (iScene, uv);
  fragColor = vec4 (mix (scene.rgb, userColor.rgb, opacity) * scene.a, scene.a)`. The
  background prelude omits the `iScene` macro — using it there is a compile error.
  `iScene` on the post-process path resolves to the engine's `straightAlphaImage`
  (see below), never the premultiplied scene directly.
- **`jam::VulkanShaderComponent`** — generic JUCE component in jam_vulkan (knows nothing
  of config/gpu), bottom-most View child: owns its `Shader` + repaint timer (`frame_rate`
  Hz, running only while a shader is installed). `paint()` = `render (g, *shader, opacity,
  resolution)`. View tells (`setShader`/`setParams`), never pokes `repaint()`/timer.
  GPU off or empty project → View installs nullptr; the shader never exists.
- **`jam::render (g, shader, opacity, resolution)`** — the injection seam
  (`paint` = geometry/image/text, `render` = shader): downcasts `g.getInternalContext()`
  to the Vulkan LLGC; buffer passes record offscreen mid-paint (scene pass suspended via
  `endRenderPass()`/`resumeRenderPass()`, the TransparencyStack technique), then the Image
  pass draws at the current clip bounds in paint order, full resolution, stencil untouched.
- **`jam::VulkanShaderInstance`** — per-window GPU realization of a Shader, cached by
  `contentHash` + scaled extent (pipelines from runtime SPIR-V, one ping-pong image pair
  per buffer pass — every buffer pass ping-pongs, Image-pass pipeline lazily built per
  target render pass + sample count). Stale entries move to `previousShaderInstances`
  (deferred destroy); entries unreferenced past the swapchain-image bound are swept at
  `beginFrame()` (hot-reload never leaks).
  Uniforms (`iTime`/`iTimeDelta`/`iFrame`/`iResolution`/`iMouse`/`iScene`/`channels[21]`/
  `opacity`, exactly 128 bytes — the guaranteed push-constant floor) stamped engine-side
  at record time; `stampChannels` is the single channel rule: buffer-pass bindless index
  by ordinal, scene fallback on the post-process path, `noScene` elsewhere.
- **Post-process** — app-global frame-graph stage, not a component:
  `VulkanGraphicsEngine::setPostProcess (unique_ptr<VulkanShader>, opacity, resolutionScale)` installs/clears (`jam_VulkanGraphicsEngine.h:240`);
  each window's `Graphics::endFrame()` pulls it (contentHash compare) between scene
  resolve and swapchain composite — the chain's Image pass IS the composite. Before any
  buffer/Image pass records, `Graphics::recordStraightAlphaPass()` (engine-owned
  `straight_alpha.frag`, `jam_vulkan/shaders/`) un-premultiplies the resolved
  (premultiplied) scene into `straightAlphaImage`; every channel/`iScene` fallback on
  this composite then resolves to `straightAlphaImage`'s bindless index, never the
  premultiplied scene directly — glass alpha is immutable (a user shader's own alpha is
  never read), and this composite's own generated formula re-premultiplies by the
  scene's alpha before its no-blend full-RGBA write (same technique the identity
  composite uses). No chain → identity composite untouched (still samples the
  premultiplied scene directly — no un-premultiply step, since no user shader ever
  reads it).
- **Resolution/filter** — `background_resolution` / `post_processing_resolution` (0.0-1.0)
  scale ONLY the intermediate buffer passes; component/scene painting is always full
  resolution. `filter` (linear/nearest, `jam::map::ImageResample` = string↔enum SSOT)
  selects the sampler baked into the prelude — bindless set binding 2 carries the nearest
  sampler alongside binding 1's linear.

### External Textures — `jam::VulkanBindlessTexture` + Resource Manifest

**`jam::VulkanBindlessTexture`** (`jam_vulkan/resource/`) — the shared GPU-resident-texture
type: owns one `jam::VulkanImage` + a per-window bindless-slot registry
(`jam::HashMap<void*, int>`, keyed by native window handle — the atlas's own shared-image/
per-window-slot split, formalized) + the caller-staged upload/re-upload (`upload()`:
memcpy into the caller's already-reserved staging-arena region, barrier → buffer-to-image
copy → barrier; re-upload re-copies into the existing image, never reallocates — VMA has
no in-place image update). It assigns no slot and writes no descriptor itself —
`jam::VulkanGraphics` remains the orchestrator (`assignBindlessIndex()` +
`writeBindlessTextureDescriptor()`), recording the result back via
`registerBindlessIndex()`. One SSOT upload path, two producers:
- **The glyph atlas** — `jam::GlyphAtlas::gpuImages` is a `jam::HashMap<Type,
  jam::VulkanBindlessTexture>` (one per atlas GPU slot type); the atlas no longer owns its
  own upload/slot-bookkeeping copy, it is a `BindlessTexture` consumer.
- **The file-decoder path** — `juce::ImageFileFormat::loadFrom` → `BitmapData` →
  `BindlessTexture::upload()`, DEVICE_LOCAL + DEDICATED for static LUTs — the second producer
  sharing the exact same upload path (no atlas-path copy).

**Resource manifest** — a single `.slangp` file per shader project (any filename,
extension-only discovery — first match) is THE resource manifest for both formats. For a
`slang` project it's the format's own full RetroArch preset, unchanged (`shaders=`/per-pass
directives, `textures=`, `#pragma parameter` overrides). For a `shadertoy` project it's a
resource-manifest-only `.slangp` — `textures=` (`a=path`, `a_linear`, `a_wrap_mode`,
`a_mipmap` per RetroArch's own convention, `gfx/video_shader_parse.c` vocabulary) plus `mesh=`
(the OBJ mesh connection, below) — no `shaders=`/passes at all.

Format itself is content-derived (`ConfigShader::loadFromPath()`, `Source/config/
ConfigModel.cpp`): the directory's own `.slangp` (when one exists) is parsed via
`jam::VulkanShaderPreset::parse()` — the ONE lex every reader shares — and a parsed result
with one or more passes is `slang`; an absent `.slangp`, or a zero-pass one, is `shadertoy`.

Channel binding is name-based: an entry literally named `iChannel2` binds that reference
directly (`ShaderCompiler::channelMacros()` already emits one `#define` per texture name — the
name IS the macro) — no explicit `channel` key, no `iChannelN` alias machinery for textures.
Each `ShaderPreset::Texture`'s own push-constant `channels[]` slot is therefore pure
declaration order — `numBufferPasses + index` into the `textures=` list, computed where
consumed (`ShaderCompiler::channelMacros()`, `ShaderInstance::build()`) rather than carried as
a field.

`mesh=` is an END-extension key (not RetroArch vocabulary) carried in the same manifest for
both formats — its value (an OBJ path, relative to the project directory, the same
absolutization as every `textures=` path) becomes `jam::VulkanShaderPreset::meshPath`, read
by `ShaderCompiler::compile()` into `jam::VulkanShader::meshPath`.

**External-texture (LUT) pipeline** — name resolution differs per format, same manifest data:
- **Shadertoy** — `ShaderCompiler::channelMacros()` emits one `#define` per named texture,
  binding its own author-assigned name directly to its resolved channel's `sampler2D`
  expression — no `iChannelN` alias; the manifest name IS the sole identifier.
- **Slang** — `Graphics::resolveSlangTextureBindings()` resolves a reflected texture name
  against `execution.getExternalTextures()` **first**, before the engine's own
  `PassOutputN`/`Original`/`Source` vocabulary — a manifest LUT name is first-class, the same
  standing as an author's own pass alias.
- **`jam::VulkanShaderInstance`** owns each execution's LUT `BindlessTexture`s
  (`externalTextures`, keyed by name) and their resolved channel slots
  (`externalTextureChannels`) — but builds neither itself; `Graphics::ensureShaderInstance()`'s
  post-build orchestration calls `Graphics::buildExternalTexture()` (needs the staging
  arena/command buffer/bindless-slot allocator `ShaderInstance::build()` doesn't have).
  `stampChannels()` is the single channel-stamping rule: buffer-pass bindless index by
  ordinal, scene fallback on the post-process path, then every named external texture
  overwrites its own slot with its registered bindless index (a texture whose decode/upload
  failed is simply skipped — graceful degradation).

### Mesh Subsystem — `jam::WavefrontObj` + Vertex Pulling

**`jam::WavefrontObj`** (`jam_graphics/mesh/`) — clean-room OBJ/MTL loader, deps stay
`juce_core`-family only (no `vk::`, no 3D-math) so both the native Vulkan renderer and a
future runtime-JS asset seam can consume the same parsed mesh data. SoA `Shape`/`Mesh`
(`juce::Array` PODs) + full-standard `Material` (Phong + PBR fields; the render path consumes
diffuse + normal). Beyond the JUCE example parser: relative/negative face-vertex index
resolution, out-of-bounds detection and degenerate-face skip (warn-and-continue, never
fatal), n-gon triangulation via `jam::Earcut` (dominant-plane projection + Newell's method for
the face normal), smoothing-group-aware normal generation when a shape carries no `vn` data,
and per-shape `(v, vt, vn)` triple dedup via `jam::HashMap<TripleIndex, Index,
TripleIndex::Hash>`. Keyword dispatch (`v`/`vn`/`vt`/`f`/`g`/`o`/`s`/`usemtl`/`mtllib` and the
MTL keyword set) is table-driven via `jam::Function::Map`, never an if/else chain.
Diagnostics accumulate into the caller's `juce::StringArray` (warn-and-continue); the only
fatal condition is an unreadable `mtllib` file, surfaced as a `juce::Result` failure.

**`jam::Earcut`** (single-ring ear-clipping triangulation) relocated from the former
`jam_vulkan/earcut/` to `jam_graphics/mesh/` — now shared by both path-fill triangulation
(`LLGCPath.cpp`'s `appendEarcutTriangulatedRing`, simple paths) and `WavefrontObj`'s own n-gon
triangulation, since `jam_vulkan` already depends on `jam_graphics` (one dependency direction
only — the reverse is forbidden).

**GPU upload** — `jam::VulkanVertex` POD (`position[3]` + `normal[3]` + `uv[2]`) +
`jam::VulkanMesh` (`jam_vulkan/resource/`) interleave every `WavefrontObj::Shape`'s SoA
geometry into ONE device-local vertex-pulling SSBO (`eStorageBuffer | eTransferDst` — **not**
`eVertexBuffer`; the mesh-backed pass reads `{position, normal, uv}` out of this SSBO manually,
indexed by `gl_VertexIndex`, no `vkCmdBindVertexBuffers` call anywhere in the consumption path)
+ one device-local index buffer, staged-uploaded **once** (never per-frame) via the
buffer-to-buffer analog of the existing image-staging path (`copyBuffer()` standing in for
`copyBufferToImage()`, a `vk::MemoryBarrier` standing in for the image path's layout-transition
barriers). `Mesh` is window-agnostic — unlike `BindlessTexture`'s per-window slot registry, a
built `Mesh` is valid and shareable across every window. The interleave pass also accumulates
the object's AABB (the auto-fit camera's consumer, below) and one `MaterialRange`
(`firstIndex`/`numIndices` into the shared index buffer + that shape's copied `Material`) per
shape — the mesh-backed draw loop iterates these to bind/stamp each shape's material for its
own index sub-range.

**`jam::VulkanOrbitCamera`** (`jam_vulkan/resource/`) — pure glm math + interaction, no
`vk::` handles:
- `computeAutoFitModelMatrix()` — static, called exactly once by `ShaderInstance::build()`
  from the built `Mesh`'s own AABB: recenters the box onto the origin and uniformly scales it
  so its largest half-extent hits a fixed target radius. Baked once into the MODEL matrix, so
  every mesh — regardless of its real-world scale — already renders normalized into a
  comfortable unit-ish cube by the time any camera sees it; the interactive camera below needs
  no AABB backchannel of its own.
- The interactive orbit (`addOrbitDelta()`) is `OrbitCamera`'s own method, driven by
  `jam::VulkanShaderComponent`'s own gesture state machine (`mouseDrag()` — the SAME
  `juce::Component` override this component's own direct-hit path already used, invoked
  as a `juce::MouseListener` callback once `ENDView` registers it via
  `addMouseListener (&background, true)` (`ENDView.cpp:16`), mirroring the SAME event stream its iMouse
  tracking already intercepts — separate state, separate purpose, not iMouse itself).
  `ENDView` holds zero orbit state of its own.
- `getViewMatrix()`/`getProjectionMatrix()` (`GLM_FORCE_DEPTH_ZERO_TO_ONE`, right-handed —
  glm's own default, `GLM_FORCE_LEFT_HANDED` deliberately NOT defined — combined with the
  manual Vulkan-NDC Y-flip; left-handed + that same Y-flip would compound into an odd number
  of axis inversions, mirroring the render) / `getNormalMatrix()` (view-only
  inverse-transpose — `computeAutoFitModelMatrix()`'s own matrix is uniform-scale-only, so
  folding it into the normal-matrix correction would be mathematically inert).

**Mesh-backed material-range draw** — every gather/offscreen shader render pass
(`Graphics::getOrCreateShaderOffscreenRenderPass (vk::Format)`) carries an unconditional second
(depth) attachment now — `eD32Sfloat` (this codebase's first depth format; the 3 stencil-only
main passes, `jam_VulkanGraphicsSetupRenderPass.cpp`, stay untouched), `loadOp=eClear`,
`storeOp=eDontCare`, never sampled outside its own render pass's own depth test
(`RenderResources::depthImage`, one shared depth image per target, per buffer pass and the
mandatory Image pass's own gather target alike — inert scratch space for a meshless target,
since every fullscreen-triangle pipeline built against this shape leaves depth test/write
disabled, `Pipelines::noStencilState()`). There is no separate mesh render target/render pass
anymore: when `shader.meshPath` is non-empty AND the OBJ parse/upload succeeded (`hasMesh()`),
`Graphics::recordMeshGatherDrawCommands()` records its own material-range/feature-edge draws
directly INTO `imagePassGatherTarget`'s own already-active render-pass instance, immediately
AFTER the ordinary, unmodified Image-pass fullscreen draw (which serves as the mesh's own
backdrop unchanged, no clone) — draw order: Image-pass fullscreen draw → per-`MaterialRange`
fill draw (depth test+write **enabled**, `eLess`; material diffuse pushed as a per-range push
constant; a default-material range draws instead with a low-alpha transparent-fill variant) →
one whole-mesh feature-edge line-art overlay draw. A parse/upload failure gracefully leaves the
ordinary, unmodified Image pass alone (zero behavior change for meshless shaders — this call is
simply never made). The result is then sampled by the SAME `background_combine.frag` combine
draw that already consumes `imagePassGatherTarget`, composited back as a bindless texture
through the existing combine pipeline; no new combine path, no separate bindless slot. The mesh
draw substitution exists on the **background path only** — the app-global post-process path
renders meshless (the same documented limitation as its all-zeros iMouse: no owning component,
no camera interaction). `mesh_default.vert`/`.frag` (`jam_vulkan/shaders/`) are engine-owned
static SPIR-V — the same offline `glslc` → `.spv` → BinaryData route as
`background_combine.frag`/`straight_alpha.frag` — a Lambert/headlight lit shader sampling MTL
diffuse, fed an MVP + normal-matrix UBO (binding 0) and the vertex-pulling SSBO (binding 1),
with per-`MaterialRange` diffuse as a fragment push constant.

### Frame Lifecycle (Per-Peer)

Multiple `LowLevelGraphicsContext` instances can share one Vulkan frame (nested paint
calls) — `Graphics::numActiveContexts` tracks how many are live. `beginFrame()` is
idempotent; the first call of a new frame waits the in-flight fence, resets the descriptor
pool, per-frame buffers, and staging arena. The LLGC destructor calls `endFrame()`, which
submits + presents only when the count reaches zero.

### Thread Contract

| Method | Thread |
|--------|--------|
| `ENDApplication::initialiseVulkan()` (engine creation, typeface registration) | MESSAGE (app init) |
| `useGpu` event → `setBackground()` / `setPostProcess()` | MESSAGE (VT listener) |
| `VulkanGraphicsEngine::createContext()` (engine dispatch, per-peer Graphics) | MESSAGE (JUCE paint dispatch) |
| Both lanes' draw overrides | MESSAGE (JUCE paint dispatch) |
| `jam::GlyphAtlas` (all methods, explicit doxygen contract) | MESSAGE only |
| `Graphics::endFrame()` (submit + present) | MESSAGE (LLGC destructor) |
| `ENDApplication` font-config events (`setFontRasterization`, `setEmbolden`) | MESSAGE (VT listener) |
| View shader funnels (`setBackground`/`setPostProcess`, shaderc compile) | MESSAGE (VT listener) |
| `jam::VulkanShaderComponent` timer → `repaint()` | MESSAGE (juce::Timer) |

---

## Message System — MessageOverlay

MessageOverlay inherits `jam::Model::Component` (IDtype::overlay) + `juce::Timer`.

- **registerParameters()** creates ParameterText for ID::message + ParameterAttachment with showMessage callback.
- **Writers** call `ENDModel::setMessage(text)` → retrieves ParameterText → setValue (any thread, lock-free).
- **Delivery:** ParameterAttachment delivers to showMessage on MESSAGE THREAD via AsyncUpdater.
- Called from `ConfigModel` (load success/error).

---

## Config Chain

```
binary defaults (BinaryData, built once into ConfigModel::state at construction)
markdown files on disk (overlay — ~/.config/end/*.md, themes/<name>/*.md)
  → ConfigModel::saveToPath: jam::ConfigDocument::getOrCreateDefaultFile writes the
    binary text when a file is missing or its front-matter version is older
  → ConfigModel::watcher (jam::File::Watcher, Extensions::md-filtered) detects change
  → ConfigModel::fileChanged tells ConfigModel::loadFromPath() to re-read: per file,
    jam::ConfigDocument::addTables (disk, binary document, disk document,
    jam::ConfigValidator rules) validates the disk file and appends each table whose
    columns equal the binary table's (the same jam path EVE's PluginEditorLayout uses)
  → ConfigModel::state properties updated via setValuesFrom (one overlay pass); a
    failed file or table adds nothing, so its binary rows stay
  → ConfigModel::getRowValue returns a row's dark value while display.md appearance
    (AUTO follows the OS, via jam::StyleManager::isDark) is dark
  → valueTreePropertyChanged fires on all listeners
  → each listener reacts: ENDLookAndFeel re-styles; ENDApplication applies the font
    rasterization, the window flags (alwaysOnTop, titleBarButtons), the theme
    cascade and the shaders; ENDView applies the background and mouse config.
```

Config delivery: consumers listen on ConfigModel's tree. No referTo. No manual distribution cascade. Standard JUCE ValueTree listener chain. A load or validation failure's accumulated error text (or the success message on a clean load) is written to `MessageOverlay` via `ENDModel::setMessage`.

---

## Plugin Architecture Mapping

END hosts audio plugins through JUCE's default formats and
`jam::ClapPluginFormat` — the JUCE-audio-plugin analogy below names the
literal hosting relationship:

| JUCE Audio Plugin | END |
|---|---|
| Host (DAW) | `Nexus` (owns `ENDModel`, all Sessions, the `juce::AudioPluginFormatManager` and the one `juce::AudioProcessorGraph`) |
| Project / session state | `end::Session` (gui-less: uuid-keyed graph node ids under one SESSION row) |
| PluginProcessor | The hosted plugin's own `juce::AudioPluginInstance`. `Session::loadPlugin` creates it asynchronously when the pane row's `Id::plugin` changes, and `Session::addPlugin` adds it to the graph, which owns it |
| Host-side APVTS analog | `jam::Model` (`ENDModel`, `ConfigModel`) — the hosted plugin owns its own parameter state internally |
| APVTS::Listener | `jam::Model::Listener` |
| ParameterAttachment | `jam::Model::ParameterAttachment` |
| Plugin's audio clock | The plugin's own clock |
| PluginEditor | The hosted plugin's own `juce::AudioProcessorEditor`, created via `createEditorAndMakeActive()` when the pane row receives `Id::name`, after layout, and parented by `EditorView` (`EditorView::createProcessorEditor`) |
| Host automation | ENAX lane only: the pane tells EVE through its host parameters, found by host-visible name only when the pane row's `Id::plugin` is the EVE file (private `EditorView::getParameter`) and written as one gesture (private `EditorView::setParameter`): `width` / `height` on resize (`EditorView::resized`), `focus` from its row (`EditorView::valueTreePropertyChanged`), `zoom` (`EditorView::setZoom`, told by `ENDView::registerZoomActions` through `SessionView::setZoom` and `TabView::setZoom`), `mode` (`EditorView::setMode`, told by the `normal` action) |

### Hosting Contract

- END hosts any VST2, VST3 or AU plugin. CLAP follows later.
- One load path: the `loadPlugin (plugin)` action writes the plugin file into `Id::plugin` of the focused pane row. The default argument is the bundle that `Nexus::getOrCreatePlugin` returns, the first entry of the plugin archive. A dropped plugin file is the override. `Session` reacts to that Model event.
- One split path: `newPane (edge)` splits the focused pane in half, or adds a pane. The split keys, `newTab` and a file drop each run `newPane`, then `loadPlugin` (`ENDView.cpp:172-186`, `:358-371`, `EditorView.cpp:74-85`). The corner drag calls `TabView::split (edge, proportion)`, then runs `loadPlugin` (`TabView.cpp:133-150`).
- EVE is known: `Nexus::registerEVE` lists EVE's description at construction, so the first EVE load runs no scan.
- `EditorView::DropTarget` sits in front of every child of a pane. A file drag that enters it focuses the pane. A plugin file dropped on it runs `newPane (pane.getEdge (point))`, then `loadPlugin (path)`.
- A resizable editor takes the inner pane area. Any other editor sits at the inset position. Fitting a fixed-size editor into its pane is open debt (`DEBT.md`).
- Tabs follow the Model: `SessionView` creates and destroys a `TabView` from the add and remove events of its tab rows. The last tab removed runs `quit`. `Session` listens on the parent `sessions` row, so JUCE destroys a closed tab's editors before `Session` removes its plugins.
- An instance can later move to its own window (undocked editor) without a new instance, because `Session` owns the instance and the pane owns only the editor.

### ENAX — END Native API eXtension

ENAX is the contract between END and its in-house plugins. The plugin is not
hardcoded to END. The contract is formalised and enforced at run time.

- **Host identity.** `jam/patch/juce-plugin-host-type-hook.patch` adds
  `juce::PluginHostType::END`, `isEND()`, the description `"END"` and a
  host-file-name row per OS (`hostFilename.startsWith ("END")`).
  `jam::PluginHostType` (`jam_core/utilities/jam_PluginHostType.h:7-13`)
  derives from it and is jam's one detection object: `isAUSandboxHost`,
  `isProToolsHost`, `getHostScale`, each computed once
  (`jam_PluginHostType.cpp:6-39`).
- **Channel.** END exports one C function, `enax_get_extension (const char* id)`
  (`Main.cpp:29-32`). It returns `Nexus::getExtension (id)`: the
  `Nexus::extensions` entry, or nullptr for an absent id (`Nexus.cpp:160-169`).
  `Nexus::extensions` is also passed to `jam::ClapPluginFormat` (`Nexus.cpp:80-86`).
- **Ids** (`jam/cast/core.md:74-83`), their values (`Nexus.cpp:7-35`) and their
  registration (`Nexus::registerExtensions`, `Nexus.cpp:61-75`), each
  the address of a host variable of the matching type:
  - `Id::enaxContextFactory` → `&juce::ComponentPeer::externalContextFactory`
  - `Id::enaxPeerDeleted` → `&juce::ComponentPeer::externalPeerDeletedCallback`
  - `Id::enaxKeyPressed` → `&keyPressedCallback`, which forwards the key to the
    active top-level window's peer (`Nexus.cpp:7-16`).
  - `Id::enaxMouseEvent` → `&mouseEventCallback`, which forwards the mouse event to
    the active top-level window's peer (`Nexus.cpp:18-25`).
  - `Id::enaxDisplayBounds` → `&displayBoundsGetter`, the total bounds of the
    displays (`Nexus.cpp:27`).
  - `Id::enaxPaneUuid` → `&paneUuidGetter`, the id of the pane whose plugin is
    loading (`Nexus.cpp:28`, `Nexus::getLoadingPane`, `:149-153`). The pane UUID is
    the plugin UUID.
  - `Id::enaxStartDirectory` → `&startDirectoryGetter`. END copies the `cwd` of the
    pane row into the caller's buffer (`Nexus.cpp:30`, `ENDView.cpp:404-408`).
  - `Id::enaxTerminalReport` → `&terminalReportCallback`. END writes the report
    (`title`, `foregroundProcess` or `cwd`) to the pane row
    (`Nexus.cpp:29`, `ENDView::registerTerminalActions`). The name `Id::enax` marks the
    pane row as an ENAX plugin. `EditorView::isENAX` reads that mark.
  - Profiler builds only: `Id::enaxPromptPainted` → `&promptPaintedCallback` and
    `Id::enaxManifestFile` → `&manifestFileGetter` (`Nexus.cpp:32-35`, `:71-74`).
- **Rules.** `jam::ENAXClientExtensions` is a mixin of the plugin's processor
  (`EVEProcessor.h:9`). Its constructor calls `PluginHostType().isEND()` once and,
  only under END, registers the END callback of each rule in
  `jam::Function::Map<juce::Identifier, void> rules`
  (`jam_ENAXClientExtensions.cpp:5-107`, `.h:58`). `set (rule, callback, args…)` runs the
  END callback, or else the caller's `callback` (`jam_ENAXClientExtensions.h:36-43`).
  `getExtension` finds `enax_get_extension` in the host executable
  (`jam_ENAXClientExtensions.cpp:110-124`). Each hook reads END's variable through
  its address at each call, so the plugin keeps no copy of END's value
  (`jam_ENAXClientExtensions.cpp:15-37`).
  - `Id::engine`: under END, the plugin reads END's context factory, peer-deleted,
    key and mouse callbacks; its peer hooks forward to END. Elsewhere, it creates its
    own `VulkanGraphicsEngine`. The processor gives this rule to
    `PluginEditorLayout::create` as its `engineRule` (`EVEProcessor.h:94`), so
    `jam_plugin_bootstrap` names no host type. The CPU engine is created in every host
    (`jam_PluginEditorLayout.h:141-147`). The key hook gives END every key before
    the plugin's components (`jam/patch/juce-key-pressed-hook.patch:21-23`).
  - `Id::keys`: under END, the text editor's `normal` and `prefix` rows leave the
    default key sheet, so the hosted text editor holds no layer binding; elsewhere
    nothing (`jam_ENAXClientExtensions.cpp:39-45`). The plugin's concrete editor applies
    the rule to the text editor's defaults (`EVEView::getViewDefaultKeys`).
  - `Id::size`: under END, the plugin's `width` and `height` parameter ranges run from
    `jam::PaneComponent::minimumPaneExtent` to END's display bounds
    (`jam_ENAXClientExtensions.cpp:47-57`).
  - The constructor reads END's pane UUID once (`Id::enaxPaneUuid`, `.cpp:9-13`).
  - `Id::terminalReport`: under END, the plugin tells END the pane UUID, the report
    name and the report text (`.cpp:59-66`).
  - `Id::startDirectory`: under END, the plugin takes the `cwd` of its pane row as the
    start directory of the shell, when that path is a directory (`.cpp:68-82`).
  - Profiler builds only: `Id::promptPainted` and `Id::manifestFile` (`.cpp:84-106`).
- **Transparency.** The VST2 and VST3 plugin editor wrappers are opaque, as JUCE
  ships them, except under END
  (`jam/patch/juce-plugin-transparency-hook.patch`:
  `setOpaque (! detail::PluginUtilities::getHostType().isEND())`). END's VST2, VST3
  and AU host windows and `NSViewComponentWithParent` are not opaque (same patch,
  `juce_VSTPluginFormat.cpp`, `juce_VST3PluginFormat.cpp`,
  `juce_AudioUnitPluginFormat.mm` hunks).
- **Focus.** END owns focus. A pane sets its plugin's `focus` host parameter from
  its row's `Id::focus` (`EditorView::valueTreePropertyChanged`). jam `PluginEditor`
  grabs keyboard focus only when the parameter turns on (`PluginEditor::addCallbacks`),
  and EVE passes it to its default component (`EVEView::focusGained`). JUCE's
  `grabFocus` on the embedded editor's peer makes END's window key and the plugin
  view the first responder (`juce_NSViewComponentPeer_mac.mm:237-239`, `:1590-1600`).
  The plugin never decides focus itself. In standalone, `MainWindow::focusGained`
  gives the editor focus (`jam_AudioStandaloneApp.cpp:74-78`).
- **AU registration.** On macOS, when the plugin bundle is a `.component`
  (`files::audioUnitExtension`), the `Nexus` constructor registers that one bundle in
  the process after `Nexus::getOrCreatePlugin` unpacks it: `CFBundleCreate`, the plist
  `AudioComponents` entries, `CFBundleGetFunctionPointerForName` with `factoryFunction`,
  then `AudioComponentRegister` (`Nexus::registerEVE`, `Nexus.cpp:78-95`;
  `Nexus::registerAudioUnit`, `:259-267`). No other bundle
  loads. Nexus owns each bundle for the process (`Nexus.h:111`).

The View is detachable. `EditorView` creates the hosted editor once, when its
row receives `Id::name` (`EditorView.cpp:172-198`); `Session` (and its plugin instances)
survives `EditorView` destruction — its subtree in `ENDModel` is complete with
zero Views. (See "Session Layer" above.)

### Commands — Ids Owned by the State Owner

A command id is a callback id, owned by the owner of the state that its callback
changes. Each owner's table carries its default keys. Config only overrides keys by
command name, and a widget never reads config.

- **END.** `map::CommandId` (`cast/bimaps.md:56-93`). `ENDView` is the one binder
  (`ENDView.cpp:13`, `:31`, `:460`), `invokeKey` is its one key entry (`:57-60`), and
  `perform` runs the `ENDActions` entry of the command name (`ENDView.cpp:85-88`).
- **jam `MatrixComponent`.** `map::MatrixCommandId` (`paneLeft`, `paneDown`, `paneUp`,
  `paneRight`; `jam/cast/gui.md:226-242`) holds the pane focus commands.
  `MatrixComponent::perform` runs `focusPane` (`jam_MatrixComponent.cpp:385-406`).
  `ENDView::getNextCommandTarget` gives the commands to the active `TabView`
  (`ENDView.cpp:70-73`).
- **jam `TextEditor`.** `map::TextEditorCommandId` with the default keys
  `map::textEditorKeys` (per platform), the modes `map::textEditorModes` and the
  layers `map::textEditorLayers`, one row per command in table order
  (`jam/cast/terminal.md`). `TextEditor::getDefaultKeys()` builds the default key sheet.
- **jam `PluginEditor`.** `map::PluginEditorCommandId` and `map::pluginEditorKeys`
  (`jam/cast/plugin_bootstrap.md`), `PluginEditor::getDefaultKeys()`.
- **Plugin binder.** `EVEView::getViewDefaultKeys` applies the host `keys` rule to
  `TextEditor::getDefaultKeys()`. `PluginEditor::addCommands` joins that sheet with the
  PluginEditor defaults, lets each config row replace the default rows of its name,
  registers every command target in the view, then sets the key presses.
- **Mode is per instance.** `jam::CommandManager::setKeyPresses` builds the sheet once:
  one row per key and mode, with the command id, the mode key and the layer; an
  unknown mode token asserts. `invokeKey` is the one key entry: it compares a row's
  mode and layer with the caller's own state. No row enters JUCE's
  `KeyPressMappingSet`. The TextEditor mode is the host parameter `mode`, attached
  to the editor's node (`TextEditor::bindToModel`).
- **Layers and sequences.** A row's `layer` names its layer; an empty layer is the
  default layer. A row whose name is a layer name enters that layer (`prefix`). In a
  layer, the prefix key clears the layer and passes through; any other unmatched key
  is dropped. In the default layer an unmatched key passes through. The caller's
  state holds `Id::layer` and `Id::sequence`; a key from another component ends the
  previous component's sequence. One `juce::Timer` in `CommandManager` ends a
  sequence or a layer after `sequenceTimeout` (jam default
  `defaultSequenceTimeout`, 1000 ms; a config row overrides it).

---

## META-MVP (Model - View - Processor)

Model-View-Processor is not three god objects — it is a recursive pattern where each layer is itself an MVP triad. See SPEC.md Section 2 for the full hierarchy.

Every View at one level is the presentation surface at the level below. Each layer carries its own Model node. All runtime Model nodes attach to ENDModel. All config is on ConfigModel.

### The Orchestrator Law — TELL, NEVER ASK (HARD RULE)

MVP is layered. **A Processor and a View are each an ORCHESTRATOR** of their own
members. The law:

- **TELL is the orchestrator's responsibility**, and it flows in exactly one
  direction: DOWNSTREAM, to the orchestrator's OWN members — objects that have
  no visibility of the higher structure of the state machine. That is not
  poking; it is the orchestrator's job.
- **There is ALWAYS an orchestrator with access to both axes** (engine axis and
  view axis) at its layer. Cross-axis wiring is performed BY that orchestrator
  as downstream tells — never by members reaching sideways, upward, or across.
- **Every other event propagation rides the state machine**: one sole AUTHOR
  writes the change; every listener reacts automatically on its own lane —
  - **VTPC** (`valueTreePropertyChanged`) — the MESSAGE-thread lane.
  - **`jam::Model::Listener::parameterChanged`** — the any-thread lane (fires
    synchronously on the writing thread).
  The lane split IS the threading contract; the lane order IS the sequencing
  (parameterChanged reactions land before the flushed VTPC reactions — engine
  side reacts before view side by construction, never by choreography).
- Each reactive object owns an **events map** (`jam::Function::Map` keyed by
  `juce::Identifier`) — direct lookup dispatch, never branch chains.

**Violation symptoms — each one is ALWAYS a contract failure:**

| Symptom | Violation |
|---|---|
| A plain value member that must be tracked manually | Shadow state — **S** (SSOT) |
| A model reference held without listening and reacting | Dead reference — **E** (Encapsulation) |
| Poking another object's members across an axis | Only the layer's orchestrator crosses, downstream — **E** |
| A getter on a non-Model object | The state machine is the only query surface; machinery exposes none — **S** (Stateless) + **E** |

**END's orchestrators, per layer:** `ENDView` is the application layer's
cross-axis orchestrator — it alone stands on both axes (the projected
`Nexus`/`Session` state and the component hierarchy it owns) and wires them
by downstream tells. `Nexus` orchestrates the host axis (formats, graph,
ENAX extensions). `end::Session` orchestrates the engine axis (its hosted
plugin instances). `TabView`/`EditorView` orchestrate their own presentation
(pane graph, hosted editor). None of their members ever looks up.

### The Pattern

| JUCE Plugin | END | Role |
|---|---|---|
| APVTS | Model (`jam::Model`) | State bridge — atomics ↔ ValueTree |
| Host | `Nexus` | Authority — owns Sessions, plugin formats, the graph, ENAX extensions. Persists. |
| PluginEditor | `EditorView` / `ENDView` | Display — message thread, detachable |

### Hierarchy

```
Application:  ENDModel — ENDView — jam::CpuGraphicsEngine — jam::VulkanGraphicsEngine (message thread)
Hosting:      Nexus — end::Session — hosted juce::AudioPluginInstance (graph node)
```

- **jam::CpuGraphicsEngine** — CPU fast lane. Owns Typeface, GlyphAtlas, GlyphArrangement and ImageLoader. Owns the CPU form of each image operation (blur, choke, feather, matte). Installs the CPU-lane LowLevelGraphicsContext. Created first, deleted last. Message thread.
- **jam::VulkanGraphicsEngine** — GPU lane. Owns the shared Device and the per-window Graphics. Tells jam::CpuGraphicsEngine the GPU form of each image operation. When no GPU frame begins, it returns the CPU-lane context. Message thread.
- **ENDApplication** — the engine owner. Reads the font, background and post-process config and tells the engines and ENDView. No Component or LookAndFeel names an engine.
- **Nexus** — gui-less Host. Owns `ENDModel`, every `Session`, the plugin formats, the one graph, and the ENAX extensions. Globally-owned instance via `jam::Instance<Nexus>`.
- **end::Session** — owns the graph node ids of the hosted plugins of one SESSION row (`addPlugin`/`removePlugin`), a `juce::ValueTree::Listener` on its own state (`Session.cpp:24`).
- **EditorView** — message thread: owns the hosted plugin's `juce::AudioProcessorEditor`. Detachable — the owning `end::Session` (and its plugin instances) persists without an EditorView.

---

## Layer Violations (FORBIDDEN)

- The paint path must NEVER write to `ValueTree`
- Lower layers must NEVER include headers from higher layers

---

*This document reflects the architectural contract. Code is ground truth — if code diverges from this document, ARCHITECTURE.md is wrong and must be updated.*
