# END - Architecture

**Purpose:** Single source of truth for architectural contracts, patterns, and invariants.

**Status:** ACTIVE — MVP pattern formalized. Vulkan dual-engine rendering complete (never-null context factory, GPU + CPU fallback). END is now a CLAP plugin host: `Nexus` owns `end::Session` instances, each hosting `juce::AudioPluginInstance`s created through `jam::ClapPluginFormat`; `EditorView` panes host each plugin's own `AudioProcessorEditor` (see "Session Layer — Landed Contract"). The terminal-emulator pipeline (CellFifo, `terminal::Processor`, `jam::CodeView`) has moved out of this repository — END builds no `jam_terminal` module.

**Last Updated:** 2026-09-12

---

## Identity

END is a **JUCE GUI application** and CLAP plugin host, GPU-rendered through `jam::VulkanEngine`. See SPEC.md Section 0 for priority order and rationale.

---

## Shared Resources — Globally-Owned Instances

**Phase 4 (current):** `jam::VulkanEngine` (Application-owned, `end::Application::vulkanEngine`) is the unified resource-ownership tree for four shared-resource globally-owned instances via `jam::Instance<T>`, plus the shared Device and shared GlyphAtlas. Each self-registers on construction. Member-declaration order inside `jam::VulkanEngine` is the teardown contract (reverse-order destruction):

| Instance | Type | Owner | Registered as |
|----------|------|-------|----------------|
| **Typeface** | `jam::Typeface` | `jam::VulkanEngine` member | `jam::Typeface::getInstance()` |
| **Stamp** | `jam::Stamp` | `jam::VulkanEngine` member | `jam::Stamp::getInstance()` |
| **Grapheme** | `jam::Grapheme` | `jam::VulkanEngine` member | `jam::Grapheme::getInstance()` |
| **Link** | `jam::Link` | `jam::VulkanEngine` member | `jam::Link::getInstance()` |
| **Device** | `jam::VulkanDevice` | `jam::VulkanEngine` member | `jam::VulkanDevice::getInstance()` |
| **GlyphAtlas** | `jam::GlyphAtlas` | `jam::VulkanEngine` member | `jam::GlyphAtlas::getInstance()` |

`end::Application` declares `vulkanEngine` (`std::unique_ptr<jam::VulkanEngine>`) **after** `lookAndFeel`, **before** `window` (Main.h). Destruction order is reverse: window → vulkanEngine (which itself tears down, in reverse member-declaration order: contexts → glyphAtlas → link/grapheme/stamp/typeface → device) → lookAndFeel, ensuring every consumer is torn down before its dependencies.

**Dependency graph (jam::VulkanEngine member order, inside Application::vulkanEngine):**
```
device (destructs LAST — every GPU resource depends on it)
    ↓
typeface / stamp / grapheme / link
    ↓
glyphAtlas (constructed from device; keys carry typeface pointers)
    ↓
contexts (per-window Graphics, destructs FIRST)
```

**Application member order:**
```
lookAndFeel (uses/registers typefaces)
    ↓
vulkanEngine (jam::VulkanEngine; depends on lookAndFeel to exist first)
    ↓
window (end::View, depends on all above)
```

---

## Layer Separation

```
 Application (ENDApplication, ENDView, SessionView, TabView)
    — orchestrates; owns all top-level lifetimes (ConfigModel, ENDLookAndFeel,
      jam::VulkanEngine — itself owning the Stamp/Grapheme/Link/Typeface
      globally-owned instances, Device, and GlyphAtlas — window)
    |
    v
 Config (ConfigModel, ConfigTheme, ConfigShader)
    — markdown files on disk are SSOT, read through jam::ConfigDocument /
      jam::ConfigValidator; ConfigModel is derived state
    |
    v
 Session Host (Nexus)
    — gui-less. Owns ENDModel (the app SSOT), every end::Session keyed by
      uuid, the CLAP jam::ClapPluginFormat + juce::AudioPluginFormatManager,
      and the per-plugin VirtualClock threads that demand-clock a hosted
      plugin outside any live audio device callback.
    |
    v
 Session (end::Session)
    — gui-less, a jam::Model::Listener only. Owns the uuid-keyed
      juce::AudioPluginInstance objects minted into one Session's own PANE
      rows; newPlugin()/removePlugin() are try_emplace/erase only. No
      lifecycle verb of its own — instances persist (see "Session Layer"
      below).
    |
    v
 Pane / EditorView (jam::PaneComponent)
    — message thread. Listens on its own PANE row's Id::pluginId; on change,
      resolves the owning Session via Nexus::getSession() and calls
      juce::AudioPluginInstance::createEditorAndMakeActive(), parenting the
      returned juce::AudioProcessorEditor as its sole child.
```

**Header inclusion rules:**
- Lower layers never know about higher layers. No reverse dependencies.
- Communication flows through explicit APIs only.

---

## Resize Path

### Swapchain / Scene-Target Resize (jam::vulkan)

- Handled per paint inside `VulkanEngine::createContext()`: when the peer's physical
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
  display (theme, size, zoom_step, ...)       WINDOW (Id::size — packed jam::Size<int16_t>,
  graphics (gpu, fontRasterizer,                      Parameter<int>)
    fontGamma, fontContrast, ...)             SESSIONS — the topology
    BACKGROUND (ConfigShader tree —             (jam::ID::focusedPane app-singular,
      shader source properties)                  jam::ID::focusedSession)
    POST_PROCESSING (ConfigShader tree)         SESSION (jam::ID::id — SessionView adopts)
  mouse                                           TAB[N] (TabView row, jam::ID::name,
  keys (prefix, prefixTimeout, bindings)                 jam::ID::focus, jam::ID::focusedPane)
  actions (macro tables)                            PANE (jam::ID::id, Id::pluginId, Id::name,
  THEMES (ConfigTheme tree, appended as a                 Id::zoom, jam::ID::focus — EditorView row)
    direct CONFIG child — window, style,            EDGE (jam::ID::id, jam::ID::head,
    code, scrollbar, tab, button, overlay,                jam::ID::tail, jam::ID::orientation,
    pane, status_bar, hint, menu,                         jam::ID::proportions — jam::PaneEdge row)
    action_list, flex — flat siblings)          OVERLAY
                                                   ID::message (ParameterText)
```
\* PANE and EDGE rows are flat TAB siblings — nesting is pure metadata:
each EDGE's head/tail names a SPACE (a PANE row or another EDGE row) by
UUID; the root space is derived — the one row no EDGE references.

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
  <OwnedComponent>` CRTP mixin) — state-bound child, self-reports keyboard
  focus onto its own row's `jam::ID::focus`.
  `jam::OwnerComponent : OwnedComponent` (abstract) — owns UUID-keyed
  children (`jam::HashMap<jam::UUID, std::unique_ptr<OwnedComponent>>` +
  one `jam::Model::Attachment` per child), aggregates child focus
  self-reports into one focused-child parameter on its own row (identifier
  supplied by the derived strategy), pure virtual
  `childAdded`/`childRemoved`/`layout`. An owner IS ownable — composite
  recursion, no diamond.
  Strategies: `jam::TabbedComponent : OwnerComponent` (machinery
  `jam::button::Bar`, one-visible swap, authors `jam::ID::focusedTab`) and
  `jam::MatrixComponent : OwnerComponent` (machinery EDGE rows +
  `jam::PaneEdge` — binary space graph: head/tail name a SPACE (PANE or
  EDGE uuid), layout = recursive descent from the derived root, split
  rewires the referencing slot to the new edge, removal collapses the
  parent EDGE into the sibling space; authors `jam::ID::focusedPane`).
  **Matrix verbs (jam_MatrixComponent.cpp):** `getNeighbor(pane, direction)` — ascend to the bounding EDGE, accumulate normalized coordinate through perpendicular ancestors, descend the far side to the leaf whose span contains it (centre-from-proportions, zero pixel reads); `swap(a, b)` — exchange two PANE uuids in their referencing EDGE slots, `layout()`; `join(pane, direction)` — three shapes: sibling collapse (shared parent dies), initiator-absorbs (multi-level: accumulated extent folds into proportions, intermediate edges rescaled), mirrored-perpendicular rotate; ineligible returns `UUID::none()`, zero mutation; `rotate(parent, child)` — AVL-style rotation of an EDGE and its child EDGE. `split(edge, position)` — creates new PANE + EDGE, rewires referencing slot.
  **Gesture state:** corner drag-to-split/join gesture state held as registered model parameters on the TAB row: `jam::ID::edge` (ParameterText — direction or empty) + `jam::ID::position` (Parameter<float> — normalized split position; -1 sentinel = join mode). TabView carries zero gesture members.
  Leaf: `jam::PaneComponent : OwnedComponent`.
- **Mirror law: the component tree is a pure function of the state tree.**
  `SESSIONS` ↔ `ENDView` (`sessions` map + per-child Attachment), `SESSION`
  ↔ `SessionView : jam::TabbedComponent` (adopts the SESSION row), `TAB` ↔
  `TabView : jam::MatrixComponent` (build-or-adopts its TAB row; sits
  directly in SessionView's children — owner-as-owned), `PANE` ↔
  `EditorView : jam::PaneComponent` (build-or-adopts its PANE row; on
  Id::pluginId change, resolves the owning Session and parents that
  plugin's own `juce::AudioProcessorEditor`).
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
  uuid-keyed `juce::AudioPluginInstance`s minted into its own PANE rows
  (`newPlugin`/`removePlugin` — try_emplace/erase only, `newPlugin` also
  stamps the row's `Id::name`/`Id::pluginId`).
- **Nexus = host.** Owns `ENDModel`, every `Session`
  (`createSession`/`getSession`/`removeSession`/`getActiveSession`), the
  `jam::ClapPluginFormat` + `juce::AudioPluginFormatManager`
  (`createPlugin`), and one `VirtualClock` (`juce::Thread` +
  `jam::VirtualDevice`) per hosted plugin (`createVirtualClock`/
  `removeVirtualClock`) that clocks the plugin outside any live audio
  device callback. SESSION row placement under SESSIONS happens in
  `createSession` — engine-side authorship, the host verb.
- **Plugin lifecycle is verb-bound, one call stack per transition:**
  `ID::newPane` → `ID::newPlugin` → `Nexus::createPlugin` (async instance
  creation) → `Nexus::createVirtualClock` + `Session::newPlugin (uuid, ...)`
  (birth, `EditorView` reacts to the row's `Id::pluginId` write by hosting
  the plugin's own editor); `ID::closePane` → `Session::removePlugin
  (uuid)` + `TabView::remove (uuid)` (death) — the same `removePlugin`
  call also fires on a join's absorbed pane. No view pokes the engine —
  removal touches only its own pool.

**Singular focus:** `focused_pane`/`focused_tab`/`focused_session` are
SINGULAR uuid-valued parameters — the value IS the identity,
`parameterChanged (id, value)` is fully self-describing downstream. The
chain: each `OwnedComponent` self-reports on its OWN row (`jam::ID::focus`
via focusGained/focusLost — the report channel); each `OwnerComponent` is
the SOLE AUTHOR of its own row's focused-child parameter (its
`valueTreePropertyChanged` sees a direct-child row's focus become 1 →
writes that child's uuid — TabView authors `TAB.focused_pane`, SessionView
authors `SESSION.focused_tab`); ENDView's `jam::ID::focusedPane` events-map
reaction re-points a `juce::Value` (`referTo`) at the reporting TAB row's
`focused_pane` property (type-filtered — the SESSIONS write re-fires the
same key); its `Value::Listener::valueChanged` writes the app-level
`SESSIONS.focused_pane` parameter — last change wins, the singular stays a
registered parameter.
Every downstream consumer reacts to the singular parameter by id+value
alone. No other writer of a `focused_*` parameter may exist.

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

## Rendering Engine — jam::VulkanEngine (Dual Engine, Never Null)

`jam::VulkanEngine` replaces JUCE's default renderers (`CoreGraphicsContext` /
`Direct2DGraphicsContext` / software) for ALL painting — terminal text, panes, chrome.
JUCE's own renderers are structurally unreachable: `VulkanEngine::createContext()`, installed
as `juce::ComponentPeer::externalContextFactory`, **never returns nullptr**.

### Ownership / Lifecycle

- `end::Application` owns `vulkanEngine` (`std::unique_ptr<jam::VulkanEngine>`), declared
  **after** `lookAndFeel`, **before** `window` (Main.h).
  - `jam::VulkanEngine` is itself the unified resource-ownership tree for the four
    shared-resource globally-owned instances (Typeface, Stamp, Grapheme, Link), the shared
    Device, and the shared GlyphAtlas — see "Shared Resources" above for its internal
    member-declaration/teardown order.
  - Critical order: lookAndFeel → vulkanEngine → window.
- Constructed **unconditionally, exactly once**, in `Application::initialiseVulkan()`:
  - `targetFrameBudgetMs` — derived from the primary display's refresh rate at init
    (`Main.cpp`: CoreVideo nominal rate on macOS, `verticalFrequencyHz` elsewhere;
    ≥120 Hz → 5.8 ms, otherwise 11.1 ms; detected once, never polled); JAM has no
    hidden default.
  - `pipelineCacheFile` — `~/.config/end/cache/vulkan_pipeline.cache`, END-resolved.
  - `gpuEnabled` — `config gpu flag AND jam::GpuProbe::probe().isAvailable`.
- **jam::Typeface** (a `jam::VulkanEngine` member, self-registers via `jam::Typeface::getInstance()`)
  exists as soon as VulkanEngine is constructed. LookAndFeel registers these typefaces with the
  shared glyph atlas during `lookAndFeel.registerTypeface(*jam::GlyphAtlas::getInstance())`, which
  creates the JUCE name-resolution map entry AND FreeType face together for each font.
  Font rasterization config applied to the atlas after VulkanEngine construction.
- **Never reset/reconstructed.** The `ID::gpu` config handler only calls
  `jam::VulkanEngine::getInstance()->setGpuEnabled(...)` — GPU preference selects which engine
  `createContext()` dispatches to per paint, never whether the VulkanEngine, its Device, or
  the shared atlas exist. The atlas and every registered typeface survive GPU toggles.
- VulkanEngine owns: the shared `jam::VulkanDevice` (one `vk::Instance`/`vk::Device`/
  `vk::Queue`/VMA allocator per application), the shared `jam::Typeface`/`jam::Stamp`/
  `jam::Grapheme`/`jam::Link` interning tables, the shared `jam::GlyphAtlas`, and per-window
  `jam::VulkanGraphics` instances keyed by native window handle.

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
VulkanEngine::createContext (peer)
  gpuEnabled and GpuProbe available and device valid
    → lazily create per-window Graphics; resize swapchain on extent mismatch
    → beginFrame() + beginRenderPass() → jam::VulkanLowLevelGraphicsContext
  otherwise (categorically unavailable, disabled, or transient begin failure)
    → jam::LowLevelGraphicsGlyphRenderer (CPU fallback)
```

**CPU fallback** — `jam::LowLevelGraphicsGlyphRenderer` subclasses
`juce::LowLevelGraphicsSoftwareRenderer` over an owned image: JUCE's software rasterizer
handles everything except `drawGlyphs`, which routes through the SAME shared `GlyphAtlas`
(SIMD blend compositing). It presents its image itself at frame end via native OS calls
(CoreGraphics on macOS, GDI on Windows) — returning non-null from the factory means JUCE
performs zero presentation of its own. Same atlas, same rasterization: visual parity with
the GPU path by construction.

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
dispatched through a branchless member-function-pointer table:

| Backend | Rasterizer | Character |
|---|---|---|
| `edgeTable` | JUCE EdgeTable coverage | unhinted, platform-identical |
| `freetype` | vendored FreeType (unhinted, no stem darkening) | unhinted, platform-identical |
| `native` | CoreText (mac) / DirectWrite (Windows) | OS-native smoothing, per-OS look |

- Coverage conditioning: a gamma+contrast LUT (config: `font_gamma` — sRGB-derived 2.2
  default — and `font_contrast`) applied at the single atlas-write site, backend-blind.
- **Embedded fonts:** `jam::Typeface::getInstance()` is self-registered by VulkanEngine's
  `typeface` member at VulkanEngine construction. Immediately after, LookAndFeel registers
  each embedded typeface with the atlas (reached directly via `jam::GlyphAtlas::getInstance()`)
  via `LookAndFeel::getTypefaceForFont()` override, which guarantees the same Typeface object
  identity flows from name-based Font construction into atlas keys.
- System fonts (user config): resolved lazily on first miss — font file located via
  CoreText/DirectWrite, loaded, cached as an FT face. Failures negative-cache.
- Color/emoji fonts: never enter the grayscale backends — JUCE ImageLayer path renders
  them into the BGRA emoji atlas.
- Backend/gamma/contrast changes (config hot-reload) flush the atlas and rebuild the LUT —
  handled by `LookAndFeel`'s own config event handlers (font events live with the font
  owner), reaching the atlas directly via `jam::GlyphAtlas::getInstance()`. Font family/size
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
  `VulkanEngine::setPostProcess (unique_ptr<Shader>, opacity, resolution)` installs/clears;
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
declaration order — `bufferPassCount + index` into the `textures=` list, computed where
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
  as a `juce::MouseListener` callback once `end::View` registers it via
  `addMouseListener (&background, true)`, mirroring the SAME event stream its iMouse
  tracking already intercepts — separate state, separate purpose, not iMouse itself).
  `end::View` holds zero orbit state of its own.
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
calls) — `Graphics::activeContextCount` tracks how many are live. `beginFrame()` is
idempotent; the first call of a new frame waits the in-flight fence, resets the descriptor
pool, per-frame buffers, and staging arena. The LLGC destructor calls `endFrame()`, which
submits + presents only when the count reaches zero.

### Thread Contract

| Method | Thread |
|--------|--------|
| `Application::initialiseVulkan()` (VulkanEngine construction, typeface registration) | MESSAGE (app init) |
| `ID::gpu` handler → `setGpuEnabled()` | MESSAGE (VT listener) |
| `VulkanEngine::createContext()` (engine dispatch, per-peer Graphics, resize, beginFrame) | MESSAGE (JUCE paint dispatch) |
| Both engines' draw overrides | MESSAGE (JUCE paint dispatch) |
| `jam::GlyphAtlas` (all methods, explicit doxygen contract) | MESSAGE only |
| `Graphics::endFrame()` (submit + present) / CPU fallback native presentation | MESSAGE (LLGC destructor) |
| `LookAndFeel` font-config handlers (atlas rebuild) | MESSAGE (VT listener) |
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
markdown files on disk (SSOT — ~/.config/end/*.md, themes/<name>/*.md)
  → ConfigModel::watcher (jam::File::Watcher, Extensions::md-filtered) detects change
  → ConfigModel::fileChanged tells ConfigModel::loadFromPath() to re-read (SAME code
    path as init: jam::ConfigDocument::parse → jam::ConfigValidator::isValid →
    getValueTree(rootTag), merged via addTables())
  → ConfigModel::state properties updated via setValuesFrom (one overlay pass)
  → valueTreePropertyChanged fires on all listeners
  → each listener reacts: ENDLookAndFeel re-styles + rebuilds the glyph atlas
    (font rasterization values), ENDView applies size/gpu/mouse config, etc.
```

Config delivery: consumers listen on ConfigModel's tree. No referTo. No manual distribution cascade. Standard JUCE ValueTree listener chain. A load or validation failure's accumulated error text (or the success message on a clean load) is written to `MessageOverlay` via `ENDModel::setMessage`.

---

## Plugin Architecture Mapping

END hosts CLAP plugins directly through `jam::ClapPluginFormat` — the
JUCE-audio-plugin analogy below is no longer a metaphor for an in-tree
subsystem, it names the literal hosting relationship:

| JUCE Audio Plugin | END |
|---|---|
| Host (DAW) | `Nexus` (owns `ENDModel` + all Sessions + the `jam::ClapPluginFormat`/`juce::AudioPluginFormatManager`) |
| Project / session state | `end::Session` (gui-less: uuid-keyed `juce::AudioPluginInstance` HashMap under one SESSION row) |
| PluginProcessor | The hosted plugin's own `juce::AudioPluginInstance`, created via `Nexus::createPlugin` and owned by `end::Session` |
| Host-side APVTS analog | `jam::Model` (`ENDModel`, `ConfigModel`) — the hosted plugin owns its own parameter state internally |
| APVTS::Listener | `jam::Model::Listener` |
| ParameterAttachment | `jam::Model::ParameterAttachment` |
| Plugin's audio clock outside a live device callback | `Nexus::VirtualClock` (`juce::Thread` + `jam::VirtualDevice`), one per hosted plugin instance |
| PluginEditor | The hosted plugin's own `juce::AudioProcessorEditor`, created via `createEditorAndMakeActive()` and parented by `EditorView` |

The View is detachable. `EditorView::createProcessorEditor()` re-creates the
hosted editor whenever its own PANE row's `Id::pluginId` changes; `Session`
(and its plugin instances) survives `EditorView` destruction — its subtree
in `ENDModel` is complete with zero Views. (See "Session Layer" above.)

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
by downstream tells. `Nexus` orchestrates the host axis (plugin creation,
virtual clocking). `end::Session` orchestrates the engine axis (its hosted
plugin instances). `TabView`/`EditorView` orchestrate their own presentation
(pane graph, hosted editor). None of their members ever looks up.

### The Pattern

| JUCE Plugin | END | Role |
|---|---|---|
| APVTS | Model (`jam::Model`) | State bridge — atomics ↔ ValueTree |
| Host | `Nexus` | Authority — owns Sessions, plugin format, virtual clocks. Persists. |
| PluginEditor | `EditorView` / `ENDView` | Display — message thread, detachable |

### Hierarchy

```
Application:  ENDModel — ENDView — jam::VulkanEngine (message thread)
Hosting:      Nexus — end::Session — hosted juce::AudioPluginInstance (VirtualClock thread)
```

- **jam::VulkanEngine** — rendering authority: engine dispatch, per-window Graphics, shared Device + GlyphAtlas + Typeface/Stamp/Grapheme/Link. Message thread.
- **Nexus** — gui-less Host. Owns `ENDModel`, every `Session`, the CLAP plugin format, and one `VirtualClock` thread per hosted plugin. Globally-owned instance via `jam::Instance<Nexus>`.
- **end::Session** — owns the hosted `juce::AudioPluginInstance`s of one SESSION row (`newPlugin`/`removePlugin`), a `jam::Model::Listener` only.
- **EditorView** — message thread: owns the hosted plugin's `juce::AudioProcessorEditor`. Detachable — the owning `end::Session` (and its plugin instances) persists without an EditorView.

---

## Layer Violations (FORBIDDEN)

- The paint path must NEVER write to `ValueTree`
- Lower layers must NEVER include headers from higher layers

---

*This document reflects the architectural contract. Code is ground truth — if code diverges from this document, ARCHITECTURE.md is wrong and must be updated.*
