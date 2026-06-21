# END - Architecture

**Purpose:** Single source of truth for architectural contracts, patterns, and invariants.

**Status:** ACTIVE — parameter system, shader pipeline, and FBO resize implemented. Terminal pipeline pre-implementation.

**Last Updated:** 2026-06-21

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
    — owns terminal::Controller instances; manages lifecycle
    |
    v
 Terminal / Controller (terminal::Controller)
    — owns Model, Processor, CodeModel, CodeView; the AudioProcessor analog
    |
    v
 Terminal / Logic (terminal::Processor → Video → Buffer<Row>)
    — reader thread; writes atomics on terminal::Model, pushes CellFifo
    |
    v
 Terminal / Model (terminal::Model)
    — APVTS bridge; atomics (reader), ValueTree (message); timer flush
    |
    v
 Terminal / View (terminal::View)
    — message thread; listens on terminal::Model + config::Model;
      parents CodeView; calls Controller::drain() on screenDirty
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
| **GL** (OpenGL) | user-interactive | Shader uniforms, FBO initialise/release, program compile/link | Shader source (via config ParameterText) | Buffer\<Row\>, ValueTree, CodeModel |

### HARD INVARIANTS

- The **reader thread NEVER touches CodeModel.** The boundary is CellFifo.
- The **message thread NEVER writes atomics** (except during timer flush).
- The **GL thread NEVER writes** ValueTree or CodeModel.
- `Controller::drain()` runs on the **message thread ONLY.**
- Violation of any of these is a **B violation** (BLESSED Bound — thread binding).

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
MESSAGE → Controller::drain() → CellFifo drain → CodeModel mutations → CodeView::calc() → repaint
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
            → calls Controller::drain()
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

### Shader FBO Resize (shader::Controller)

- `end::View::resized()` → packs `end::Size` → writes ID::size as single int to view state
- `Parameter<int>` adapter fires `parameterChanged(ID::size)` → Controller event dispatches
- Event unpacks `end::Size`, calls `resizer.set()` → `jam::Resizer` coalesces (16ms timer)
- Resizer stop trigger → `executeOnGLThread` → re-initialise FBO pair for each buffer pass at scaled dims

### Terminal Grid Resize (terminal::Controller)

- `terminal::View::resized()` → writes new pixel dimensions
- `Controller` owns `jam::Resizer` — coalesces rapid changes via 16ms timer
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
2. View calls `Controller::drain()`.
3. Controller drains **history ring** — permanently appends departed scrollback lines to CodeModel.
4. Controller drains **active ring** — removes previous live tail (`liveTailExtent[screen]` rows from end), lays down new active rows as the live tail, stores drained count as new `liveTailExtent`.
5. Controller calls `CodeView::calc()`.
6. CodeView repaints.

**Invariant:** history ring rows and active ring rows must NOT overlap by row index. A row enters history OR active, never both in the same tick. Violation produces content doubling.

---

## Two Independent State Trees

```
config::Model (independent tree)          end::Model (independent tree)
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
- **Model::Listener::parameterChanged** — fires on the calling thread when parameter value changes. Controller, View, and other listeners receive parameter events through this.
- **Flush timer** — 10 Hz. Iterates adapters, writes dirty atomics to VT properties.

### Packed Value Transport — end::Size

Window dimensions are packed as `end::Size` (inherits `jam::Union<int16_t, int16_t>`), stored on VT as a single int property. One property write = one VTPC = one parameterChanged = atomic resize.

- **Write:** `end::Size (width, height).toInt()` → `state.setProperty (ID::size, ...)`
- **Read:** `end::Size { appModel.getValue (IDtype::view, ID::size) }` → structured binding `auto [w, h] = size`
- **Parameter:** `Parameter<int>` with adapter fires `parameterChanged (ID::size)`

This pattern avoids separate width/height parameters that would fire two events per resize.

---

## Shader Pipeline — shader::Controller

shader::Controller owns the OpenGL lifecycle. Inherits `juce::OpenGLRenderer` + `jam::Model::Listener`.

### Shader Loading

```
config::Model::loadFromPath()
  → pre-creates ParameterText per existing shader file (bimap × file.existsAsFile())
  → shader.load() reads files, calls param->setValue(source)
  → ParameterAdapter fires parameterChanged(IDtype::shader)
  → Controller event dispatches loadShaders
  → executeOnGLThread: compile vertex+fragment, link, store in programs HashMap
  → buffer passes (not Image) get FBO pair emplaced + initialised at current dims
```

### Resize Flow

```
View::resized()
  → setViewState() packs end::Size, writes ID::size to VT
  → Parameter<int> adapter fires parameterChanged (ID::size)
  → Controller events map dispatches ID::size event
  → event unpacks end::Size, calls resizer.set (IDtype::view, w, h)
  → jam::Resizer coalesces (16ms timer)
  → stop trigger: executeOnGLThread → loop programs, emplace + initialise FBOs at scaled dims
```

### FBO Lifecycle

- **Program struct:** `{ OpenGLShaderProgram, optional<array<OpenGLFrameBuffer, 2>> }` — ping-pong pair per buffer pass.
- **Creation:** emplaced in loadShaders for buffer passes (not Image). Image renders to default framebuffer.
- **Resize:** Resizer stop trigger re-initialises both FBO slots at new scaled dimensions.
- **Release:** shutdown() (called from openGLContextClosing) releases all FBOs, clears programs, resets quad.

### Thread Contract

| Method | Thread |
|--------|--------|
| attach / detach / isAttached | MESSAGE |
| newOpenGLContextCreated / renderOpenGL / openGLContextClosing | GL |
| parameterChanged | MESSAGE (AsyncUpdater delivery) |
| Resizer stop trigger | MESSAGE (juce::Timer) |
| FBO initialise / release | GL (via executeOnGLThread) |

### Event → Setter → Trigger Pattern

Controller follows the KANJUT event/trigger pattern:

- **Events map** (`jam::Function::Map`) — dispatched from `parameterChanged`. One event per concern (IDtype::shader → loadShaders, ID::size → resize).
- **Resizer** — trigger mechanism. `set()` fires optional start trigger, coalesces via timer, fires "stop" trigger with final dimensions.
- **parameterChanged** is the single dispatch point. No dual-listener mechanisms (no VT::Listener on Controller).

---

## Message System — MessageOverlay

MessageOverlay inherits `jam::Model::Component` (IDtype::overlay) + `juce::Timer`.

- **registerParameters()** creates ParameterText for ID::message + ParameterAttachment with showMessage callback.
- **Writers** call `end::Model::setMessage(text)` → retrieves ParameterText → setValue (any thread, lock-free).
- **Delivery:** ParameterAttachment delivers to showMessage on MESSAGE THREAD via AsyncUpdater.
- Called from config::Model (load success/error) and shader::Controller (compile errors).

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
| **Document** | `(lineIndex, col)` over CodeModel lines | CodeModel (owned by Controller) |
| **Screen/pixel** | wrapped projection via `getWrappedLines(viewWidth)` | CodeView |

### Conversion Authority — HARD RULE

**`jam::Cell::Point::fromPixel` / `toPixel` is the ONLY sanctioned pixel-cell conversion.** Hand-rolled arithmetic is **forbidden**.

| Translation | Owner | Mechanism |
|---|---|---|
| pixel <-> cell | `jam::Cell` | `Cell::Point::fromPixel` / `toPixel` |
| Video-grid -> document cell-row | Controller | cell-space row arithmetic via `liveTailExtent` |
| document -> screen/pixel | CodeView | the wrapped projection (`getWrappedLines`) |

CodeView never sees a Video-grid coordinate. Controller translates at the boundary.

---

## Plugin Architecture Mapping

| JUCE Audio Plugin | END |
|---|---|
| Host (DAW) | `Nexus` |
| AudioProcessor | `terminal::Controller` |
| ProcessorChain | `terminal::Processor` |
| APVTS | `jam::Model` (terminal::Model, end::Model, config::Model) |
| APVTS::Listener | `jam::Model::Listener` |
| ParameterAttachment | `jam::Model::ParameterAttachment` |
| parameterChanged → parameters map → setter → trigger | event → setter → resizer/transition |
| PluginEditor | `terminal::View` / `end::View` |
| SpectrumFIFO | `CellFifo` |
| SpectrumProcessor::outputDB | `CodeModel` |

The View is detachable. Controller survives View destruction (daemon mode).

---

## META-MVC

Model-View-Controller is not three god objects — it is a recursive pattern where each layer is itself an MVC triad. See SPEC.md Section 2 for the full hierarchy.

Every View at one level is a Controller at the level below. Each layer carries its own Model node. All runtime Model nodes attach to end::Model. All config is on config::Model.

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
