# END - Architecture

**Purpose:** Single source of truth for architectural contracts, patterns, and invariants.

**Status:** ACTIVE — MVP pattern formalized, Phase 3 terminal stubs (Processor, View, Nexus). Shader pipeline complete. Terminal pipeline pre-implementation.

**Last Updated:** 2026-06-27

---

## Identity

END is a **JUCE GUI application** that renders terminal output. See SPEC.md Section 0 for priority order and rationale.

---

## Layer Separation

```
 Application (ENDApplication, end::View, Tabs, Panes)
    — orchestrates; owns all top-level lifetimes
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

### HARD INVARIANTS

- The **reader thread NEVER touches CodeModel.** The boundary is CellFifo.
- The **message thread NEVER writes atomics** (except during timer flush).
- The **GL thread NEVER writes** ValueTree or CodeModel.
- `Session::drain()` runs on the **message thread ONLY.**
- Violation of any of these is a **B violation** (BLESSED Bound — thread binding).

### Parameter Access by Thread

- **ValueTree read/write** is EXCLUSIVE to the **message thread**. VT listeners (`valueTreePropertyChanged`) fire on the message thread.
- **Non-message threads** (GL, reader, timer) read parameter values through **atomics only**:
  - `Parameter<T>`: `getRawParameterValue<T>(tag, id)->load()`
  - `ParameterText`: `getParameter<jam::ParameterText>(tag, id)->getValue()`
- **Model::Listener::parameterChanged** delivers the new value directly — use `newValue` parameter, do not re-read from VT or atomic.
- Any VT access from the GL thread is a **B violation** (thread binding).

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
         → JUCE composites component paint through GL context when GPU renderer active
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

Two independent resize paths — shader FBO and terminal grid:

### Shader FBO Resize (graphics::Processor)

- `end::View::resized()` → packs `end::Size` → writes ID::size as single int to view state
- `Parameter<int>` adapter fires `parameterChanged(ID::size)` → Processor event dispatches
- Event unpacks `end::Size`, calls `resizer.set()` → `jam::Resizer` coalesces (16ms timer)
- Resizer stop trigger → `executeOnGLThread` → re-initialise FBO pair for each buffer pass at scaled dims

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
      SHADER (ParameterText per pass)           ID::size (packed end::Size, Parameter<int>)
    THEME                                       ID::focusedPane
      FLEX                                    TABS
    KEYS                                        TAB[N]
    POPUP                                         PANE (uuid)
    WHELMED                                   OVERLAY
                                                ID::message (ParameterText)
```

- **config::Model** — config constants. Changes on reload only. Lua files on disk are the SSOT. Config tree is derived state, rebuilt from disk on every reload (same code path as init). Shader source stored as ParameterText under GRAPHICS→SHADER (one per existing pass file).
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

### Packed Value Transport — end::Size

Window dimensions are packed as `end::Size` (inherits `jam::Union<int16_t, int16_t>`), stored on VT as a single int property. One property write = one VTPC = one parameterChanged = atomic resize.

- **Write:** `end::Size (width, height).toInt()` → `state.setProperty (ID::size, ...)`
- **Read:** `end::Size { appModel.getValue (IDtype::view, ID::size) }` → structured binding `auto [w, h] = size`
- **Parameter:** `Parameter<int>` with adapter fires `parameterChanged (ID::size)`

This pattern avoids separate width/height parameters that would fire two events per resize.

---

## GPU Rendering Pipeline — jam::vulkan::Registry

`jam::vulkan::Registry` replaces JUCE's default software `LowLevelGraphicsContext` with a
Vulkan-backed implementation, accelerating all UI painting (terminal text, panes, chrome)
when a Vulkan-capable GPU is available. Supersedes the former OpenGL `graphics::Processor`
Shadertoy-style background-shader pipeline (removed — `View.h` keeps only a commented-out
member as historical marker). Gated by `jam::GpuProbe::probe().isAvailable` and the `ID::gpu`
config flag.

### Construction / Lifecycle

```
View::registerEvents() ID::gpu handler (message thread, VT listener)
  → canUseGpu = config flag and GpuProbe::probe().isAvailable
  → if canUseGpu and no vulkanEngine: vulkanEngine = make_unique<jam::vulkan::Registry>()
  → if not canUseGpu and vulkanEngine exists: vulkanEngine.reset()
```

`Registry` construction creates the shared `jam::vulkan::Device` (one VkInstance /
VkPhysicalDevice / VkDevice / VkQueue / VmaAllocator per application — not per window),
creates the shared `jam::GlyphAtlas` (CPU-side glyph rasterization cache, shared across all
windows), and installs itself as `juce::ComponentPeer::externalContextFactory`. Destruction
clears the factory pointer; owned `Device` / `Graphics` / `GlyphAtlas` self-destruct via RAII.

### Per-Window Graphics

`Registry::createContext()` — the static factory JUCE calls per peer needing to paint — is
gated on GPU availability + device validity. Lazily creates one `jam::vulkan::Graphics` per
native window handle (`jam::HashMap<void*, unique_ptr<Graphics>>`), keyed by
`juce::ComponentPeer::getNativeHandle()`. Handles swapchain resize when the peer's physical
size changes. On success, calls `graphics->beginFrame()` + `beginRenderPass()` +
`incrementContextCount()`, then constructs a `jam::vulkan::LowLevelGraphicsContext` for JUCE
to record paint commands into. Returns `nullptr` to fall back to JUCE's default software
renderer if Vulkan is unavailable, the device is invalid, context creation failed, or
`beginFrame()` could not acquire a swapchain image.

### Frame Lifecycle (Per-Peer)

Multiple `LowLevelGraphicsContext` instances can share one Vulkan frame (e.g. nested paint
calls) — `Graphics::activeContextCount` tracks how many are live. `beginFrame()` is
idempotent (no-op if a frame is already active); on the first call of a new frame it resets
the descriptor pool and re-allocates the projection descriptor set. `LowLevelGraphicsContext`'s
destructor calls `context.endFrame()`, which decrements the count and only submits + presents
when it reaches zero (last LLGC for the frame).

### Render Path (Per Draw Call)

`LowLevelGraphicsContext` overrides JUCE's `LowLevelGraphicsContext` virtual draw API
(`fillRect`, `drawImage`, `fillPath`, `clipToPath`, `drawGlyphs`,
`beginTransparencyLayer`/`endTransparencyLayer`, etc.). Vertices are pre-transformed on the
CPU (`TransformState` accumulator — fast-path integer-translation tracking, falls back to a
full `juce::AffineTransform` for rotation/scale); the GPU-side projection is a fixed
orthographic matrix (`updateProjection()`), not a full MVP — there is no model or view
matrix, only a 2D pixel-to-NDC projection.

Glyph rendering: `drawGlyphs()` resolves each glyph through the shared `jam::GlyphAtlas`
(`getOrRasterize()`), bucketing quads by atlas slot type (`jam::GlyphAtlas::Type::mono` /
`emoji`) into a `jam::HashMap`-backed structure, then issues one `vkCmdDrawIndexed` per
non-empty slot type in a loop. Dirty atlas slots are uploaded to their GPU `Image` via a
dedicated staging buffer (`uploadDirtyAtlasSlots()`), bracketed by
`endRenderPass()`/`beginRenderPassLoad()` since transfer commands are illegal inside an
active render pass.

Stencil-based clipping (`clipToPath()`) writes a stencil mask; all subsequent draw types
(rect fill, image, path fill, glyphs) check `isStencilClipActive` and select a stencil-test
pipeline variant + set dynamic stencil state, so clip state is respected deterministically
regardless of draw order.

### Thread Contract

| Method | Thread |
|--------|--------|
| `View::registerEvents()` ID::gpu handler (Registry construction/teardown) | MESSAGE (VT listener) |
| `Registry::createContext()` (per-peer Graphics creation/resize/beginFrame) | MESSAGE (JUCE paint dispatch) |
| `LowLevelGraphicsContext` draw overrides (fillRect/drawImage/fillPath/drawGlyphs/etc.) | MESSAGE (JUCE paint dispatch) |
| `jam::GlyphAtlas` (getOrRasterize / rasterize / pack) | MESSAGE (explicit doxygen contract — MESSAGE THREAD only, all methods) |
| `Graphics::endFrame()` (submit + present) | MESSAGE (LLGC destructor, called after `handlePaint()` returns) |

Unlike the former OpenGL pipeline (dedicated `juce::OpenGLContext` GL thread), the Vulkan
pipeline runs entirely on the message thread — there is no separate GPU thread. Command
buffer recording, swapchain/pipeline creation, and glyph atlas rasterization all happen
synchronously within JUCE's paint dispatch.

---

## Message System — MessageOverlay

MessageOverlay inherits `jam::Model::Component` (IDtype::overlay) + `juce::Timer`.

- **registerParameters()** creates ParameterText for ID::message + ParameterAttachment with showMessage callback.
- **Writers** call `end::Model::setMessage(text)` → retrieves ParameterText → setValue (any thread, lock-free).
- **Delivery:** ParameterAttachment delivers to showMessage on MESSAGE THREAD via AsyncUpdater.
- Called from config::Model (load success/error) and graphics::Processor (compile errors).

---

## Config Chain

```
lua files on disk (SSOT)
  → ENDApplication (jam::File::Watcher::Listener) detects change
  → tells config::Model to re-read files (SAME code path as init)
  → config::Model ValueTree properties updated
  → valueTreePropertyChanged fires on all listeners
  → each listener reacts: LookAndFeel re-styles, terminal::View re-applies font/colours, etc.
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
Application:  end::Model      — end::View      — graphics::Processor (GL thread)
Terminal:     terminal::Model  — terminal::View  — terminal::Processor (reader thread)
```

- **graphics::Processor** — GL thread orchestrator: shaders, FBOs, render loop
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
- GL thread must NEVER write to Buffer\<Row\>, ValueTree, or CodeModel
- Reader thread must NEVER touch CodeModel
- `terminal/` headers must NEVER include `nexus/` headers
- `jam_terminal` headers must NEVER include any END application header
- Lower layers must NEVER include headers from higher layers
- Manual pixel arithmetic (`pixelX / cellWidth`) is FORBIDDEN — use `jam::Cell` converters

---

## Open Seam — Font / Atlas GL-Thread Binding (UNRESOLVED)

See SPEC.md Section 1.8. Must be resolved before Phase 4 rendering work.

---

*This document reflects the architectural contract. Code is ground truth — if code diverges from this document, ARCHITECTURE.md is wrong and must be updated.*
