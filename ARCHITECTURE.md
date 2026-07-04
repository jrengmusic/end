# END - Architecture

**Purpose:** Single source of truth for architectural contracts, patterns, and invariants.

**Status:** ACTIVE — MVP pattern formalized. Vulkan dual-engine rendering complete (never-null context factory, GPU + CPU fallback). Terminal pipeline Phase 3 (Session/Model/Processor/View).

**Last Updated:** 2026-07-04

---

## Identity

END is a **JUCE GUI application** that renders terminal output. See SPEC.md Section 0 for priority order and rationale.

---

## Shared Resources — Globally-Owned Instances

**Phase 4 (current):** `jam::VulkanEngine` (Application-owned, `end::Application::vulkanEngine`) is the unified resource-ownership tree for four shared-resource globally-owned instances via `jam::Instance<T>`, plus the shared Device and shared GlyphAtlas. Each self-registers on construction. Member-declaration order inside `jam::VulkanEngine` is the teardown contract (reverse-order destruction):

| Instance | Type | Owner | Registered as |
|----------|------|-------|----------------|
| **Typeface** | `jam::Typeface` | `jam::VulkanEngine` member | `jam::Typeface::getInstance()` |
| **Stamp** | `jam::Stamp` | `jam::VulkanEngine` member | `jam::Stamp::getInstance()` |
| **Grapheme** | `jam::Grapheme` | `jam::VulkanEngine` member | `jam::Grapheme::getInstance()` |
| **Link** | `jam::Link` | `jam::VulkanEngine` member | `jam::Link::getInstance()` |
| **Device** | `jam::vulkan::Device` | `jam::VulkanEngine` member | `jam::vulkan::Device::getInstance()` |
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
 Application (ENDApplication, end::View, Tabs, Panes)
    — orchestrates; owns all top-level lifetimes (config::Model, LookAndFeel,
      jam::VulkanEngine — itself owning the Stamp/Grapheme/Link/Typeface
      globally-owned instances, Device, and GlyphAtlas — window)
    |
    v
 Config (config::Model, config::Display, config::Nexus, ...)
    — lua files on disk are SSOT; config::Model is derived state
    |
    v
 Session Host (Nexus)
    — owns terminal::Session instances; manages lifecycle
    |
    v
 Terminal / Session (terminal::Session)
    — DAW host per terminal instance. Owns CodeModel (document buffer),
      terminal::Model (VT state SSOT), and Processor (engine).
    |
    v
 Terminal / Processor (terminal::Processor)
    — AudioProcessor analog. Reader thread pipeline (Video → Buffer<Row> → CellFifo).
      Reader thread writes atomics on terminal::Model.
    |
    v
 Terminal / Model (terminal::Model)
    — Per-session APVTS bridge; atomics (reader), ValueTree (message); timer flush.
      NOT a globally-owned instance — one per terminal::Session.
    |
    v
 Terminal / View (terminal::View)
    — message thread; listens on terminal::Model + config::Model;
      parents CodeView; calls Session::drain() on screenDirty
    |
    v
 Terminal / TTY (jam_terminal, platform)
    — reader thread feeds raw bytes to Processor
```

**Header inclusion rules:**
- `terminal/` headers MUST NOT include `nexus/` headers.
- `jam_terminal` headers MUST NOT include any END application header.
- Lower layers never know about higher layers. No reverse dependencies.
- Communication flows through explicit APIs only.

---

## Cross-Thread Data Contract (MANDATORY)

Lock-free architecture, unidirectional data flow. No mutex on any hot path. No wait, no stall, no yield.

### Thread Ownership

| Thread | QoS | Writes | Reads | NEVER |
|--------|-----|--------|-------|-------|
| **Reader** (TTY) | high | terminal::Model atomics, Buffer\<Row\> cells, CellFifo push | Raw PTY bytes | ValueTree, CodeModel, mutex, allocation, block |
| **Timer** (JUCE) | default | terminal::Model's ValueTree properties (flush dirty atomics) | `needsFlush` atomic on terminal::Model | Buffer\<Row\>, CodeModel |
| **Message** (main) | user-interactive | terminal::Model ValueTree, CodeModel mutations (drain) | terminal::Model ValueTree (listener), CellFifo drain | Atomics (except flush) |
| **Message** (Vulkan paint) | user-interactive | Vulkan command buffer recording, swapchain/pipeline/atlas-image creation (`jam::vulkan::Graphics`), glyph rasterization + dirty-flag clears (`jam::GlyphAtlas`, MESSAGE THREAD only) | Component paint bounds, glyph/font state | Buffer\<Row\>, atomics (except flush) |

There is **no dedicated GPU thread** — the entire Vulkan pipeline (and the CPU-fallback renderer) runs synchronously inside JUCE's paint dispatch on the message thread.

### HARD INVARIANTS

- The **reader thread NEVER touches CodeModel.** The boundary is CellFifo.
- The **message thread NEVER writes atomics** (except during timer flush).
- The **paint path NEVER writes** ValueTree or CodeModel.
- `Session::drain()` runs on the **message thread ONLY.**
- Violation of any of these is a **B violation** (BLESSED Bound — thread binding).

### Parameter Access by Thread

- **ValueTree read/write** is EXCLUSIVE to the **message thread**. VT listeners (`valueTreePropertyChanged`) fire on the message thread.
- **Non-message threads** (reader, timer) read parameter values through **atomics only**:
  - `Parameter<T>`: `getRawParameterValue<T>(tag, id)->load()`
  - `ParameterText`: `getParameter<jam::ParameterText>(tag, id)->getValue()`
- **Model::Listener::parameterChanged** delivers the new value directly — use `newValue` parameter, do not re-read from VT or atomic.

### Scalar Data — Parameters, Mode Flags, Strings, Metadata

Sparse, low-volume, consumed by UI listeners. Per-session (one per terminal::Session).

```
READER → atomic slots on terminal::Model → timer flush → terminal::Model ValueTree → MESSAGE reads via listener
```

**terminal::Model is the per-session SSOT for all scalar state** (not a globally-owned instance). Each Session owns one terminal::Model. `flush()` copies dirty atomics to ValueTree properties. MESSAGE thread reads exclusively from ValueTree (via `ValueTree::Listener`). `ParameterText` pattern for cross-thread strings (double-buffered, seqlock generation).

### Bulk Data — Cell Content

High-volume (25,000+ cells at 5K fullscreen), consumed by render path.

```
READER → Video writes Buffer<Row> → CellFifo push (drop-oldest SPSC)
MESSAGE → terminal::View calls Session::drain() → CellFifo drain → CodeModel mutations → CodeView::calc() → repaint
```

**Buffer\<Row\>** is Video's scratch surface — dual channel (normal + alternate), destroyed on resize. It is NOT the document. NOT the SSOT. NOT persistent.

**CellFifo** is transport — two independent `jam::BufferSPSC` rings (history + active) with producer-side drop-oldest. Data passes through and is consumed. Under flood, oldest entries are dropped — reader never stalls. CellFifo is NOT storage. NOT scrollback.

**CodeModel** is the document SSOT — `ParagraphsModel` bounded deque of `jam::String` lines. History appended permanently. Active rows laid down as live tail (tracked by `liveTailExtent`). FIFO eviction drops oldest when over `scrollbackLines` capacity.

### Classification Rule

If the data is one-per-cell (O(rows x cols)), it is **bulk** → `Buffer<Row>` (Video scratch) → CellFifo → CodeModel.

If the data is sparse/scalar (O(1) or O(small N)), it is **scalar** → terminal::Model atomics → ValueTree.

---

## Data Flow: Keystroke to Pixel

```
Keystroke → Message Thread → TTY::write()
         → Reader Thread reads response → Processor::process() → Parser → Video
         → Buffer<Row> written, terminal::Model atomics set
         → Timer flush (60/120 Hz) on Message Thread
         → terminal::Model flushes dirty atomics to ValueTree
         → terminal::View::valueTreePropertyChanged()
            → terminal::View calls drain on Session
            → CellFifo drained into CodeModel
            → CodeView::calc() → repaint
         → JUCE paint dispatch → jam::VulkanEngine::createContext()
            → Vulkan LLGC (GPU) or jam::LowLevelGraphicsGlyphRenderer (CPU)
            — never JUCE's own default renderers
```

---

## Document Model — NOT Scanline

END does NOT use the terminal scanline model. See SPEC.md Section 1.0 for the full anti-mental-model.

**The correct model:**
- External sources (VT parser) commit text INTO the editor (CodeModel).
- The editor IS the SSOT for all content it renders.
- **Width enters exactly once, at projection time** — `CodeView` calls `getWrappedLines(viewWidth)` at paint.
- Storage knows nothing about width, pixel dimensions, or viewport geometry.
- **SIGWINCH changes the projection width only — storage is untouched.**
- History lines survive resize unchanged. `Processor::prepare()` resizes Video's Buffer\<Row\> (the scratch). CodeModel content is unaffected.

---

## Resize Path

Two independent resize paths — swapchain and terminal grid:

### Swapchain / Scene-Target Resize (jam::vulkan)

- Handled per paint inside `VulkanEngine::createContext()`: when the peer's physical
  extent no longer matches the swapchain extent, `Graphics::resize()` recreates
  the swapchain, stencil image, MSAA scene target (color + stencil + resolve),
  and framebuffers at the new extent.
- The session-locked MSAA sample count is NOT re-calibrated on resize.
- No coalescing timer needed — resize happens lazily on the next paint request.

### Terminal Grid Resize (terminal::Processor)

- `terminal::View::resized()` → writes new pixel dimensions
- `Processor` owns `jam::Resizer` — coalesces rapid changes via 16ms timer
- Resizer start trigger → `Processor::suspendProcessing(true)` → `Processor::prepare()` (resizes Video grid only) → `Processor::suspendProcessing(false)`
- `prepare()` resizes Buffer\<Row\> only. CellFifo untouched. CodeModel untouched. CodeView untouched.
- CodeView re-wraps at new width on next paint via `getWrappedLines(viewWidth)`
- History is never touched during resize
- SIGWINCH delivered to PTY after Resizer stop trigger
- `suspendProcessing()` / `callbackLock` gate the reader thread during resize

---

## Drain Sequence (Message Thread)

When `screenDirty` fires on terminal::Model's ValueTree:

1. terminal::View's `valueTreePropertyChanged` fires.
2. View calls `Session::drain()`.
3. Session drains **history ring** — permanently appends departed scrollback lines to CodeModel.
4. Session drains **active ring** — removes previous live tail (`liveTailExtent[screen]` rows from end), lays down new active rows as the live tail, stores drained count as new `liveTailExtent`.
5. View calls `CodeView::calc()`.
6. CodeView repaints.

**Invariant:** history ring rows and active ring rows must NOT overlap by row index. A row enters history OR active, never both in the same tick. Violation produces content doubling.

---

## State Trees — Global and Session-Scoped

### Global Owned Instances (Application Lifetime)

Two independent global state trees (both `jam::Instance<T>` globally-owned instances):

```
config::Model (IDtype::config)              end::Model (IDtype::end)
  CONFIG                                      END
    GRAPHICS                                    VIEW
      SHADER (ParameterText per pass)           ID::size (packed jam::Size<int16_t>, Parameter<int>)
      gpu, fontRasterizer,                      ID::focusedPane
      fontGamma, fontContrast                 TABS
    THEME                                       TAB[N]
      FLEX                                         PANE (uuid)
    KEYS                                      OVERLAY
    POPUP                                       ID::message (ParameterText)
    WHELMED
```

- **config::Model** (globally-owned instance via `jam::Instance<T>`) — config constants. Changes on reload only. Lua files on disk are the SSOT. Config tree is derived state, rebuilt from disk on every reload (same code path as init). Shader source stored as ParameterText under GRAPHICS→SHADER (one per existing pass file). Font rasterization values (`graphics.font_rasterizer` / `font_gamma` / `font_contrast`) are validated config (string-enum via `end::FontRasterizerBackend` bimap) and hot-reload live.
- **end::Model** (globally-owned instance via `jam::Instance<T>`) — app-lifetime runtime state. Changes during app lifetime. Components graft their state nodes via `jam::Model::Attachment` (RAII).

**Invariant:** No config values on end::Model. No runtime state on config::Model. Consumers that need both register as listener on both trees.

### Session-Scoped Ephemeral (Per-Session Lifetime)

One per terminal::Session (NOT a globally-owned instance):

```
terminal::Model (IDtype::terminal, per-session)
  SESSION
    MODES
    NORMAL (screen 0)
      TEXT (cell state)
    ALTERNATE (screen 1)
      TEXT (cell state)
```

- **terminal::Model** — Per-session, owned by `terminal::Session`. NOT a globally-owned instance (not `jam::Instance<T>`) — multiple concurrent sessions each own independent terminal::Model instances. Lifecycle coupled with Session. VT state SSOT for the terminal (RFC-terminal-editor.md P12). Atomics (reader) and ValueTree (message) follow the scalar-data pattern. Direction A/B as described in Cross-Thread Data Contract.

**Phase 3 (current):** One terminal per Session.  
**Phase ∞ (WIP):** Session may host multiple terminals; terminal ownership TBD.

---

**Future (WIP):** terminal::Model will attach to end::Model. Currently independent.

**No cross-tree references:** config → end → terminal dependency is one-way data flow. No upward references.

---

## Parameter System — jam::Model APVTS Analog

jam::Model is a 1:1 APVTS analog for multi-type parameters. Key contracts:

- **createAndAddParameter\<T\>** — creates typed Parameter (int, float, int64_t, ParameterText) in AnyMap, seeds VT property, registers ParameterAdapter. Parameters live on Model forever.
- **ParameterAdapter** (cpp-internal) — bridges Parameter↔VT. Bidirectional: atomic→VT (flush timer), VT→atomic (valueTreePropertyChanged reverse sync). Loopback-guarded, equality-gated.
- **ParameterAttachment** (public) — per-parameter listener bridge. Takes ParameterBase& + callback. Delivers changes on MESSAGE THREAD via AsyncUpdater. Does NOT create/destroy parameters.
- **Model::Listener::parameterChanged** — fires on the calling thread when parameter value changes. Processor, View, and other listeners receive parameter events through this.
- **Flush timer** — 10 Hz. Iterates adapters, writes dirty atomics to VT properties.

### Packed Value Transport — jam::Size

Window dimensions are packed as `jam::Size<int16_t>`, stored on VT as a single int property. One property write = one VTPC = one parameterChanged = atomic resize.

- **Write:** `jam::Size<int16_t> (width, height).toInt()` → `state.setProperty (ID::size, ...)`
- **Read:** `jam::Size<int16_t> { appModel.getValue (IDtype::view, ID::size) }` → structured binding `auto [w, h] = size`
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
- VulkanEngine owns: the shared `jam::vulkan::Device` (one `vk::Instance`/`vk::Device`/
  `vk::Queue`/VMA allocator per application), the shared `jam::Typeface`/`jam::Stamp`/
  `jam::Grapheme`/`jam::Link` interning tables, the shared `jam::GlyphAtlas`, and per-window
  `jam::vulkan::Graphics` instances keyed by native window handle.

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
    → beginFrame() + beginRenderPass() → jam::vulkan::LowLevelGraphicsContext
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
  — the atlas's `vk::Image`s are shared, their array slots are not.
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
| `edge_table` | JUCE EdgeTable coverage | unhinted, platform-identical |
| `freetype` | vendored FreeType (autofit + stem darkening) | hinted, platform-identical |
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
(`display.lua` graphics block; `config::Shader` enumerates the project directory into
its state tree; hot reload = init path):

```
file change (lua/GLSL) → CONFIG state → ID::background / ID::postProcessing event
  → View funnel — full recompile (applyBackground / applyPostProcess) or cheap
    param-only path (applyBackgroundParams / applyPostProcessParams)
  → jam::vulkan::ShaderCompiler (shaderc, vendored + isolated in jam_vulkan) → jam::vulkan::Shader → consumer
```

- **`jam::vulkan::Shader`** — POD descriptor, pure data: `jam::Owner<ShaderPass>`
  (`ShaderPass{name, spirv}`, name-hashed for O(1) lookup) + `contentHash` (wyhash over all pass
  names+bytes — content-derived identity, no counter; identical recompile = cache hit).
  Public const fields, zero getters. Presentation params (opacity, resolution, frame rate)
  are NOT in the descriptor — they travel explicitly (`render (g, shader, opacity,
  resolution)`, `setShader`/`setParams`, `setPostProcess`/`setPostProcessParams`), so
  param-only config changes never recompile.
- **`jam::vulkan::ShaderCompiler`** (`jam_vulkan/shader/jam_VulkanShaderCompiler.{h,cpp}`) wraps
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
- **`jam::vulkan::ShaderComponent`** — generic JUCE component in jam_vulkan (knows nothing
  of config/gpu), bottom-most View child: owns its `Shader` + repaint timer (`frame_rate`
  Hz, running only while a shader is installed). `paint()` = `render (g, *shader, opacity,
  resolution)`. View tells (`setShader`/`setParams`), never pokes `repaint()`/timer.
  GPU off or empty project → View installs nullptr; the shader never exists.
- **`jam::vulkan::render (g, shader, opacity, resolution)`** — the injection seam
  (`paint` = geometry/image/text, `render` = shader): downcasts `g.getInternalContext()`
  to the Vulkan LLGC; buffer passes record offscreen mid-paint (scene pass suspended via
  `endRenderPass()`/`resumeRenderPass()`, the TransparencyStack technique), then the Image
  pass draws at the current clip bounds in paint order, full resolution, stencil untouched.
- **`jam::vulkan::ShaderInstance`** — per-window GPU realization of a Shader, cached by
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
| View shader funnels (`applyBackground`/`applyPostProcess`, shaderc compile) | MESSAGE (VT listener) |
| `jam::vulkan::ShaderComponent` timer → `repaint()` | MESSAGE (juce::Timer) |

---

## Message System — MessageOverlay

MessageOverlay inherits `jam::Model::Component` (IDtype::overlay) + `juce::Timer`.

- **registerParameters()** creates ParameterText for ID::message + ParameterAttachment with showMessage callback.
- **Writers** call `end::Model::setMessage(text)` → retrieves ParameterText → setValue (any thread, lock-free).
- **Delivery:** ParameterAttachment delivers to showMessage on MESSAGE THREAD via AsyncUpdater.
- Called from config::Model (load success/error).

---

## Config Chain

```
lua files on disk (SSOT)
  → ENDApplication (jam::File::Watcher::Listener) detects change
  → tells config::Model to re-read files (SAME code path as init)
  → config::Model ValueTree properties updated
  → valueTreePropertyChanged fires on all listeners
  → each listener reacts: LookAndFeel re-styles + rebuilds glyph atlas
    (font rasterization values), terminal::View re-applies font/colours, etc.
```

Config delivery: consumers listen on config::Model's tree. No referTo. No manual distribution cascade. Standard JUCE ValueTree listener chain.

Config-derived values for the reader thread (cellWidth, cellHeight, scrollbackLines): computed by terminal::View (listener on config::Model), written as atomics to terminal::Model. Reader reads atomics. View is the bridge.

---

## Coordinate Spaces — Three, Never Conflated

| Space | Coordinates | Owner |
|---|---|---|
| **Video-grid** | `(gridRow, gridCol)`, viewport-bounded | Video (reader thread), packed into terminal::Model |
| **Document** | `(lineIndex, col)` over CodeModel lines | CodeModel (owned by Session) |
| **Screen/pixel** | wrapped projection via `getWrappedLines(viewWidth)` | CodeView |

### Conversion Authority — HARD RULE

**`jam::Cell::Point::fromPixel` / `toPixel` is the ONLY sanctioned pixel-cell conversion.** Hand-rolled arithmetic is **forbidden**.

| Translation | Owner | Mechanism |
|---|---|---|
| pixel <-> cell | `jam::Cell` | `Cell::Point::fromPixel` / `toPixel` |
| Video-grid -> document cell-row | Session | cell-space row arithmetic via `liveTailExtent` |
| document -> screen/pixel | CodeView | the wrapped projection (`getWrappedLines`) |

CodeView never sees a Video-grid coordinate. Session translates at the boundary.

---

## Plugin Architecture Mapping

| JUCE Audio Plugin | END |
|---|---|
| Host (DAW) | `terminal::Session` (per-instance host) |
| Session Manager | `Nexus` (owns all Sessions) |
| AudioProcessor | `terminal::Processor` |
| APVTS | `jam::Model` (terminal::Model, end::Model, config::Model) |
| APVTS::Listener | `jam::Model::Listener` |
| ParameterAttachment | `jam::Model::ParameterAttachment` |
| parameterChanged → parameters map → setter → trigger | event → setter → resizer/transition |
| PluginEditor | `terminal::View` / `end::View` |
| SpectrumFIFO | `CellFifo` |
| SpectrumProcessor::outputDB | `CodeModel` |

The View is detachable. Session (and its Processor) survives View destruction (daemon mode).

---

## META-MVP (Model - View - Processor)

Model-View-Processor is not three god objects — it is a recursive pattern where each layer is itself an MVP triad. See SPEC.md Section 2 for the full hierarchy.

Every View at one level is the presentation surface at the level below. Each layer carries its own Model node. All runtime Model nodes attach to end::Model. All config is on config::Model.

### The Pattern

| JUCE Plugin | END | Role |
|---|---|---|
| APVTS | Model (jam::Model) | State bridge — atomics ↔ ValueTree |
| PluginProcessor | Processor | Authority — owns pipeline, output, state. Persists. |
| PluginEditor | View | Display — message thread, detachable |

### Hierarchy

```
Application:  end::Model      — end::View      — jam::VulkanEngine (message thread)
Terminal:     terminal::Model  — terminal::View  — terminal::Processor (reader thread)
```

- **jam::VulkanEngine** — rendering authority: engine dispatch, per-window Graphics, shared Device + GlyphAtlas + Typeface/Stamp/Grapheme/Link. Message thread.
- **terminal::Processor** — reader thread orchestrator: Parser, Video, CellFifo, TTY.
- **terminal::Session** — DAW host per terminal instance. Owns CodeModel (document buffer) and Processor (engine).
- **View** — message thread: parents CodeView, calls drain(), owns Font. Detachable — Session persists without View (daemon mode).
- **Nexus** — Session manager. Owns all terminal::Session instances. Globally-owned instance via `jam::Instance<Nexus>`.

---

## CodeView Contract (TETRIS E-Contract)

`jam::CodeView` is a dumb jam_gui widget. Same contract as a TETRIS DSP core: private state, validated setters, every setter calls `calc()`, no reaching out.

- NOT a `jam::ValueTree::Component`. No tree. No listener.
- Cell-space API only: `setCaret`, `setSelection`/`getSelection`, `setViewportWidth`.
- No pixel methods. `jam::Cell::Point::fromPixel`/`toPixel` is the only converter.
- Selection TYPE (visual/line/block) → end::Model TABS (app-level, cross-component).
- Selection COORDINATES → transient in CodeView (`CodeView::Selection` value struct, not in any tree).

See SPEC.md Sections 2.1 and 2.2.

---

## Layer Violations (FORBIDDEN)

- Rendering must NEVER call Video or Buffer\<Row\> mutators
- TTY must NEVER call UI/Component code
- Video must NEVER allocate on reader thread
- The paint path must NEVER write to Buffer\<Row\>, ValueTree, or CodeModel
- Reader thread must NEVER touch CodeModel
- `terminal/` headers must NEVER include `nexus/` headers
- `jam_terminal` headers must NEVER include any END application header
- Lower layers must NEVER include headers from higher layers
- Manual pixel arithmetic (`pixelX / cellWidth`) is FORBIDDEN — use `jam::Cell` converters

---

*This document reflects the architectural contract. Code is ground truth — if code diverges from this document, ARCHITECTURE.md is wrong and must be updated.*
