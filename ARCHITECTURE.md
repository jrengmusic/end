# END - Architecture

**Purpose:** Single source of truth for architectural contracts, patterns, and invariants.

**Status:** ACTIVE — MVP pattern formalized. Vulkan dual-engine rendering complete (never-null context factory, GPU + CPU fallback). Terminal pipeline pre-implementation.

**Last Updated:** 2026-07-02

---

## Identity

END is a **JUCE GUI application** that renders terminal output. See SPEC.md Section 0 for priority order and rationale.

---

## Layer Separation

```
 Application (ENDApplication, end::View, Tabs, Panes)
    — orchestrates; owns all top-level lifetimes (config, LookAndFeel,
      jam::vulkan::Registry, window)
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
    — DAW host per terminal instance. Owns CodeModel (document buffer)
      and Processor (engine). Phase 4: also owns terminal::Model and Resizer.
    |
    v
 Terminal / Processor (terminal::Processor)
    — AudioProcessor analog. Reader thread pipeline (Video → Buffer<Row> → CellFifo).
      Reader thread writes atomics on terminal::Model.
    |
    v
 Terminal / Model (terminal::Model)
    — APVTS bridge; atomics (reader), ValueTree (message); timer flush
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
| **Timer** (JUCE) | default | ValueTree properties (flush dirty atomics) | `needsFlush` atomic | Buffer\<Row\>, CodeModel |
| **Message** (main) | user-interactive | ValueTree, CodeModel mutations (drain) | ValueTree (listener), CellFifo drain | Atomics (except flush) |
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

Sparse, low-volume, consumed by UI listeners.

```
READER → atomic slots on terminal::Model → timer flush → ValueTree → MESSAGE reads via listener
```

terminal::Model is the SSOT for all scalar state. `flush()` copies dirty atomics to ValueTree properties. MESSAGE thread reads exclusively from ValueTree (via `ValueTree::Listener`). `ParameterText` pattern for cross-thread strings (double-buffered, seqlock generation).

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
         → JUCE paint dispatch → jam::vulkan::Registry::createContext()
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

- Handled per paint inside `Registry::createContext()`: when the peer's physical
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

## Two Independent State Trees

```
config::Model (independent tree)           end::Model (independent tree)
  CONFIG                                    END
    GRAPHICS                                  VIEW
      SHADER (ParameterText per pass)           ID::size (packed jam::Size<int16_t>, Parameter<int>)
      gpu, fontRasterizer,                      ID::focusedPane
      fontGamma, fontContrast                 TABS
    THEME                                       TAB[N]
      FLEX                                         PANE (uuid)
    KEYS                                      OVERLAY
    POPUP                                       ID::message (ParameterText)
    WHELMED
```

- **config::Model** — config constants. Changes on reload only. Lua files on disk are the SSOT. Config tree is derived state, rebuilt from disk on every reload (same code path as init). Shader source stored as ParameterText under GRAPHICS→SHADER (one per existing pass file). Font rasterization values (`graphics.font_rasterizer` / `font_gamma` / `font_contrast`) are validated config (string-enum via `end::FontRasterizerBackend` bimap) and hot-reload live.
- **end::Model** — runtime state. Changes during app lifetime. Components graft their state nodes via `jam::Model::Attachment` (RAII).

No config values on end::Model. No runtime state on config::Model. Consumers that need both register as listener on both trees.

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

## Rendering Engine — jam::vulkan::Registry (Dual Engine, Never Null)

`jam::vulkan::Registry` replaces JUCE's default renderers (`CoreGraphicsContext` /
`Direct2DGraphicsContext` / software) for ALL painting — terminal text, panes, chrome.
JUCE's own renderers are structurally unreachable: `Registry::createContext()`, installed
as `juce::ComponentPeer::externalContextFactory`, **never returns nullptr**.

### Ownership / Lifecycle

- `end::Application` owns `vulkanEngine` (`std::unique_ptr<jam::vulkan::Registry>`),
  declared after `lookAndFeel` (Registry constructs after LookAndFeel, destructs before it).
- Constructed **unconditionally, exactly once**, in `Application::initialiseVulkan()`:
  - `targetFrameBudgetMs` — provisional 60Hz-safe 11.1 ms literal at the END call site
    (per-monitor refresh-rate detection replaces it later); JAM has no hidden default.
  - `pipelineCacheFile` — `~/.config/end/cache/vulkan_pipeline.cache`, END-resolved.
  - `gpuEnabled` — `config gpu flag AND jam::GpuProbe::probe().isAvailable`.
- Immediately after construction: `lookAndFeel.registerTypeface (vulkanEngine->getAtlas())`
  registers the six embedded fonts with the shared glyph atlas (one pass, one parse per
  font — creates the JUCE name-resolution map entry AND the FreeType face together), then
  applies font rasterization config.
- **Never reset/reconstructed.** The `ID::gpu` config handler only calls
  `Registry::getInstance()->setGpuEnabled(...)` — GPU preference selects which engine
  `createContext()` dispatches to per paint, never whether the Registry, its Device, or
  the shared atlas exist. The atlas and every registered typeface survive GPU toggles.
- Registry owns: the shared `jam::vulkan::Device` (one VkInstance/VkDevice/VkQueue/VMA
  allocator per application), the shared `jam::GlyphAtlas`, and per-window
  `jam::vulkan::Graphics` instances keyed by native window handle.

### Engine Dispatch (per paint)

```
Registry::createContext (peer)
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
  — the atlas's VkImages are shared, their array slots are not.
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
  growth retires (never destroys) the old buffer until the frame fence; reset after
  `vkWaitForFences`. Atlas uploads write into the CALLING window's arena — no cross-window
  staging races, no same-frame overwrite.
- **Pipeline cache** — persistent `VkPipelineCache` blob at the END-resolved cache file;
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
- Embedded fonts: registered with bytes at Registry construction;
  `LookAndFeel::getTypefaceForFont()` override guarantees the same Typeface object
  identity flows from name-based Font construction into atlas keys.
- System fonts (user config): resolved lazily on first miss — font file located via
  CoreText/DirectWrite, loaded, cached as an FT face. Failures negative-cache.
- Color/emoji fonts: never enter the grayscale backends — JUCE ImageLayer path renders
  them into the BGRA emoji atlas.
- Backend/gamma/contrast changes (config hot-reload) flush the atlas and rebuild the LUT —
  handled by `LookAndFeel`'s own config event handlers (font events live with the font
  owner), reaching the atlas via `Registry::getInstance()`. Font family/size changes need
  no handler: they produce new atlas keys and re-rasterize naturally.

### Shaders

GLSL sources under `jam_vulkan/shaders/` are the SSOT — shared declarations live in
include files (`mvp.glsl`, `push_constants_rect.glsl`, `push_constants_glyph.glsl`,
`bindless_texture.glsl`) consumed via glslc's include directive. CMake compiles each
source to its committed `.spv` via per-file custom commands with depfile tracking
(include edits recompile dependents); the `.spv` files are embedded as binary data and
are platform-neutral — compiled once, valid everywhere.

### Frame Lifecycle (Per-Peer)

Multiple `LowLevelGraphicsContext` instances can share one Vulkan frame (nested paint
calls) — `Graphics::activeContextCount` tracks how many are live. `beginFrame()` is
idempotent; the first call of a new frame waits the in-flight fence, resets the descriptor
pool, per-frame buffers, and staging arena. The LLGC destructor calls `endFrame()`, which
submits + presents only when the count reaches zero.

### Thread Contract

| Method | Thread |
|--------|--------|
| `Application::initialiseVulkan()` (Registry construction, typeface registration) | MESSAGE (app init) |
| `ID::gpu` handler → `setGpuEnabled()` | MESSAGE (VT listener) |
| `Registry::createContext()` (engine dispatch, per-peer Graphics, resize, beginFrame) | MESSAGE (JUCE paint dispatch) |
| Both engines' draw overrides | MESSAGE (JUCE paint dispatch) |
| `jam::GlyphAtlas` (all methods, explicit doxygen contract) | MESSAGE only |
| `Graphics::endFrame()` (submit + present) / CPU fallback native presentation | MESSAGE (LLGC destructor) |
| `LookAndFeel` font-config handlers (atlas rebuild) | MESSAGE (VT listener) |

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
Application:  end::Model      — end::View      — jam::vulkan::Registry (message thread)
Terminal:     terminal::Model  — terminal::View  — terminal::Processor (reader thread)
```

- **jam::vulkan::Registry** — rendering authority: engine dispatch, per-window Graphics, shared Device + GlyphAtlas. Message thread.
- **terminal::Processor** — reader thread orchestrator: Parser, Video, CellFifo, TTY.
- **terminal::Session** — DAW host per terminal instance. Owns CodeModel (document buffer) and Processor (engine).
- **View** — message thread: parents CodeView, calls drain(), owns Font. Detachable — Session persists without View (daemon mode).
- **Nexus** — Session manager. Owns all terminal::Session instances. Instance<Nexus> singleton.

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
