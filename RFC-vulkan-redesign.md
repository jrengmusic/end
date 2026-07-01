# RFC — Vulkan Rendering Engine Redesign (jam::vulkan LLGC + Graphics)

Date: 2026-07-01
Status: Ready for COUNSELOR handoff

> Scope note: the core rewrite lands in **JAM** (`~/Documents/Poems/dev/jam/jam_vulkan/`). The custom-shader / post-process workstream (e) and shaderc integration land in **END**. See Handoff Notes for the file-level split. This RFC is written to END's root because the session is END-rooted; COUNSELOR should treat it as spanning both trees.

---

## Problem Statement

Redesign JAM's `jam::vulkan::LowLevelGraphicsContext` + `jam::vulkan::Graphics` — END's `juce::Graphics` GPU backend — **downstream only**, without changing a single JUCE surface API call. JUCE is the battle-tested baseline; jam::Vulkan is an accelerator bolted under it, with JUCE's native (CoreGraphics / Direct2D) and software renderers preserved as graceful fallback (`Registry::createContext` returns `nullptr` → JUCE renders — ARCHITECTURE.md GPU section).

Objectives (ARCHITECT, verbatim intent):
1. Seamless integration with `juce::Component::paint(juce::Graphics&)` — no surface API change; implement only downstream in the Vulkan LLGC + Graphics.
2. Extend past JUCE's design ceiling: (i) multi-pass custom shader rendering + post-processing over the whole composited frame; (ii) atlas text via `jam::GlyphAtlas`/`jam::GlyphArrangement` (`juce::GlyphArrangement` re-derives layout per call — slower at terminal throughput).
3. Adhere to MANIFESTO.md (BLESSED), JRENG-CODING-STANDARD.md, NAMES.md, and JUCE architecture.
4. Maximize use of Vulkan / JUCE / JAM APIs (JAM extensible); minimize hand-rolling.

Motivating defects (grounded in code, this session):
- **Bug #2 — clip-state desync.** `isStencilClipActive` (`jam_VulkanLowLevelGraphicsContext.h:750`) and `currentStencilRef` (`:753`) are out-of-band mutable members, absent from the saved `State` struct (`:708-727`). `saveState()`/`restoreState()` don't touch them, so a `clipToPath`/`clipToImageAlpha` inside a save/restore block leaves them dangling → subsequent draws test stencil `EQUAL` against a stale ref → wrong clipping.
- **Path fill winding bug.** `triangulatePath` earcuts each sub-path **independently** (`jam_VulkanLowLevelGraphicsContextPath.cpp:53` `singlePoly { ring }`, loop `:82-83`) → a holed/self-intersecting path fills the **hole solid**; JUCE's winding rule is ignored. (Glyphs unaffected — they route through the atlas, not `fillPath`.)
- **Nested clip intersection wrong.** `stencilWriteState` uses `passOp=REPLACE`, `compareOp=ALWAYS` (`jam_VulkanPipelinesState.cpp:232-243`); the inner clip's stencil write is not masked by the outer, so multi-level arbitrary-path clips do not compute a true intersection (only `deviceSpaceClipList` rect-bounds approximates it, `Path.cpp:263`).
- **Stencil shared across transparency layers.** The stencil attachment is shared across offscreen transparency layers and never cleared between them (`jam_VulkanTransparencyStack.cpp:87`) → cross-layer contamination.
- **No anti-aliasing.** Every sample count is `VK_SAMPLE_COUNT_1_BIT` (7 sites). Path/curve edges + rotated quads are aliased. (Text is already AA via atlas coverage.)
- **Image descriptor churn + hard cap.** Set-1 sampler descriptor allocated **per drawImage** (`jam_VulkanGraphics.cpp:317-347`) from a pool capped at `maxImageSamplerDescriptors{64}` (`jam_VulkanGraphics.h:51-54`).

---

## Research Summary

### Current architecture (ground truth)

**Per-draw immediate model.** Each paint op = exactly one `vkCmdDraw*`. No batching. Vertices pre-transformed CPU-side into a persistently-mapped per-frame `FrameBuffer` (VMA `CPU_ONLY`, power-of-2 growth, retired buffers held to fence) as **fixed-function vertex attributes**:
- fillRect → `issueRectFill` (`jam_VulkanLowLevelGraphicsContext.cpp:223-239`), 4 verts (vec2) TRIANGLE_STRIP, one `vkCmdDraw`.
- drawImage → `recordImageDrawCommands` (`...ContextImage.cpp:61-90`), 4 verts + set-1 sampler descriptor allocated per draw, one `vkCmdDraw`.
- fillPath → earcut triangulate + `vkCmdDrawIndexed` (`...ContextPath.cpp:171-194`).
- drawGlyphs → bucket by atlas type, **≤2 `vkCmdDrawIndexed`** (mono/emoji), atlas descriptors allocated once/frame (`...ContextGlyph.cpp:255-284`). Glyph vertex is **12 floats × 4 verts/quad** — 4× redundant (`jam_VulkanPipelinesState.cpp:108-151` glyph vertex input).

**Descriptors.** Pool reset once at `beginFrame` (`jam_VulkanGraphics.cpp:106`), cap 64 sets / 64 image-samplers. Set 0 = projection UBO (once/frame). Set 1 = combined image sampler (per-draw for images).

**Pipelines.** Already data-driven and BLESSED-clean: `pipelineSpecs[19]` declarative table (`jam_VulkanPipelines.h:156`), flat `VkPipeline[19]` (`:79`) enum-indexed, one shared `VkPipelineLayout`, `VertexInput` enum `{quad, glyph}` (`:139`). Manual destruction order (cache → pipelines → layout → set-layouts → shader modules, `:6-14`, `shutdown`). Dynamic state includes viewport + scissor + stencil-ref.

**Clip (three mechanisms today).**
1. Rect clip → `deviceSpaceClipList` (`RectangleList<int>`, in `State`, saved/restored, `:714`) → `computeScissor()` collapses the whole list to its **bounding-box** `VkRect2D` (`...Context.cpp:118-131`) → `vkCmdSetScissor` per draw (`:143-144`). `excludeClipRectangle`/disjoint rects lose everything but the bbox.
2. Path / image-alpha clip → stencil write (`stencilWriteTriList` / `stencilWriteImageAlpha`) + stencil-test pipeline variant + `vkCmdSetStencilReference(currentStencilRef)`. **Out-of-band** state → bug #2.
3. `ivec4 clip` push-constant — declared in **every** shader (`fill_rect.vert:5`, `glyph.vert:8`, `image.frag:5`), **used by none**. Dormant per-primitive clip channel.

**Transparency layers.** `beginTransparencyLayer`/`endTransparencyLayer` (`...ContextTransparency.cpp:41-50,162-181`) render to an offscreen target (`jam_VulkanTransparencyStack`), composite back with `LOAD_OP_LOAD`. Offscreen target reuses the **shared stencil attachment** (`TransparencyStack.cpp:87`).

**Render-pass interleaving (ordering constraint).** Transfer commands are illegal inside a render pass, so uploads bracket `endRenderPass()`/`beginRenderPassLoad()`: image upload (`...ContextImage.cpp:13-21`), atlas upload (`...ContextGlyph.cpp:15-41`), and transparency-layer boundaries. These are ordered events in the same stream as draws — any batching must preserve them.

**Frame lifecycle.** `beginFrame` (waitFence, acquire, resetCmd, **resetDescriptorPool** `:106`, alloc projection set, resetUsage) → `beginRenderPass` (clear color+stencil) → draws → `endRenderPass` → `endFrame` (submit+present when `activeContextCount==0`, `:139-174`). Runs entirely on the **message thread** (no GPU thread).

**Glyph atlas — dual-path, cached.** `jam::GlyphAtlas` (`jam_graphics/fonts/`, FreeType via `juce::Typeface::getLayersForGlyph`). Rasterize once → cache (`GlyphAtlas.cpp:308`, hit returns cached region `:136-141`). Rasterization is **scalar** (JUCE `EdgeTable` → `MonoCoverageWriter` memset/byte writes `:164-186`; emoji `std::memcpy` `:251`). **Software composite is SIMD/NEON** — `jam_SimdBlend.h` NEON (`:5,65-92`) + SSE2 (`:27-62`), kernels `blendMonoTinted4`/`blendSrcOver4`, driven 4-wide by `compositeRows` (`GlyphAtlas.cpp:49,93,375-389`). Atlas feeds both GPU upload (Vulkan path) and SIMD CPU composite (fallback path). Per-frame text cost = SIMD blend of cached glyphs, not rasterization.

### Platform feasibility — MoltenVK (Vulkan 1.2 target, `jam_VulkanDevice.h:31`; portability subset, `jam_VulkanDevice.cpp:106`; zero optional features currently enabled)

| Technique | MoltenVK | Evidence |
|---|---|---|
| Descriptor indexing / bindless | **Supported**, all macOS GPUs | `MVKDevice.mm:187-202` (feature bits), separate `texture[]`+`sampler[]`, fixed-size array (unbounded `[]` fails MSL — SPIRV-Cross #2112), `UPDATE_AFTER_BIND` pool; Tier-2 limits runtime-queried |
| Vertex pulling (SSBO + `gl_VertexIndex`) | **Supported**, cleanest path | native base-vertex/instance, no caveat |
| `vkCmdDrawIndirect` (CPU count) | works but **CPU-looped** | `MVKCmdDraw.mm:765` — O(drawCount), no GPU offload |
| `vkCmdDrawIndirectCount` (GPU count) | **NO-OP** | `MVKDevice.mm:2808` `drawIndirectCount=false`; `vulkan.mm:2390-2414` empty bodies → **excluded** |
| 4× MSAA color+stencil + resolve | **Supported** both platforms | color=depth=stencil sample limits identical (`MVKDevice.mm:2640-2648,2841-2848`); Apple Silicon transient → `MTLStorageModeMemoryless` on-chip resolve (`:3374-3400`, `MVKRenderPass.mm:908-911`); M1–M4 cap 4×, M5 8× (Apple Metal Feature Set Tables 2026-05-21); Intel ceiling runtime-query; stencil resolve `SAMPLE_ZERO`-only (`MVKDevice.mm:878`) — irrelevant for a transient clip mask (no resolve); use 8-bit UNORM color for native (non-compute) resolve |

### Prior-art grounding (primary source, local repos)

Reader↔renderer synchronization — every shipped GPU terminal is **lock-based**; END is the only **lock-free** one (SPSC drop-oldest + atomics, "No mutex on any hot path", ARCHITECTURE.md):

| Terminal | Sync (cited) |
|---|---|
| Alacritty | `Arc<FairMutex<Term>>` — `sync.rs:11`, `window_context.rs:390`; PTY `event_loop.rs:117,140` |
| WezTerm | `Mutex<Terminal>` — `localpane.rs:126,390` |
| Zed | `alacritty_terminal` `Arc<FairMutex<Term>>` — `terminal.rs:16,981` |
| Ghostty | `std.Thread.Mutex` in `renderer.State` — `State.zig:14`; PTY `Termio.zig:643` |
| kitty | grid single-threaded (main-thread only) + pthread mutexes `screen.h:135`, `child-monitor.c:86` — not lock-free, not concurrent parse↔render |

Render technique — all use glyph/sprite atlas + instanced records, few draws; **none** use bindless, indirect, or SSBO vertex-pulling. Most-advanced references corroborate the chosen design:
- **GPUI (Zed):** per-primitive-type instanced draws, vertex shader reads per-instance records by `[[instance_id]]` from a `constant Quad*` buffer (`shaders.metal:66`, `draw_primitives_instanced(...,6,instance_count)`); **no bindless** (explicit texture per batch); multi-pass offscreen — paths rasterize to a **4× MSAA intermediate** then composite (`metal_renderer.rs:41,780-811`).
- **Ghostty:** 32-byte `CellText` per-instance, one instanced draw per cell type (`RenderPass.zig:213`).
- Alacritty `glDrawElementsInstanced` (`glsl3.rs:243`); kitty `glDrawArraysInstanced` (`gl.c:136`).

Validates (a) instanced records; contextualizes (b) bindless as a step beyond the field (no one here uses it); confirms (c) indirect used by none; (e) multi-pass offscreen is proven ground.

---

## Principles and Rationale

**Framing.** jam::Vulkan **empowers** JUCE — it does not replace JUCE's model, and it is not benchmarked against or modeled on any external engine. The only transferable idea from prior art is the instanced-record shape, which JAM implements in its own code. External renderers (Skia/GPUI/ImGui) were used solely as existence-proofs during discussion and carry zero code weight.

**Decisions and BLESSED mapping:**

- **(a) Unified vertex-pulling SSBO — all quad primitives.** rect+image+glyph become instance records in one shared SSBO (corner via `gl_VertexIndex`, per-primitive data pulled by `instanceID`); paths stay triangulated. Collapses the `{quad, glyph}` vertex-input duality (retires fixed-function vertex input; pipelines read the SSBO). **Draw order stays JUCE-immediate** (SSBO ranges issued in paint order — whole-frame regroup rejected, see below). Rationale: SSOT (one record layout), Lean (one vertex path), removes the 4× glyph vertex redundancy. Corroborated by GPUI/Ghostty.
- **(b) Bindless image path.** Separate fixed-size `texture[]` + `sampler[]`, `UPDATE_AFTER_BIND`; texture index rides in the instance record. Removes per-draw descriptor allocation + the 64-cap (`jam_VulkanGraphics.h:51-54`). Rationale: Lean (kills per-draw descriptor management), Bound (one descriptor array, indexed). Narrow (image path); feasible per MoltenVK with the fixed-bound + separate-texture/sampler constraints.
- **(d) Clip-as-data + correctness.** `ivec4` rect-clip travels in the record (lights up the dormant shader channel), captured at record time from the then-correct logical state → **structurally removes** the out-of-band `isStencilClipActive`/`currentStencilRef` desync (bug #2). Stencil ref for path/image-alpha clips also baked into the record (no mutable member). **Non-zero winding parametrized** on `juce::Path::isUsingNonZeroWinding()` — stencil-then-cover (`INCR_WRAP`/`DECR_WRAP` + cover test `!=0`; even-odd = `INVERT` + bit 0) for complex/holed paths, fast fan for simple — replacing the per-subpath earcut (fixes the hole bug). **Fix both deeper defects:** correct nested-path intersection (inner stencil write masked by outer) + per-transparency-layer stencil isolation. Rationale: SSOT/Stateless (no shadow clip state → Determinism), Explicit.
- **(e) Full Shadertoy-parity multi-pass shader + post-process [PRIMARY].** The one true JUCE-ceiling capability. Whole UI renders to an offscreen **scene target**; pipeline: background multi-buffer with **BufferA-D feedback ping-pong** + `iChannel0-3` + component capture + whole-frame **post** (`iScene`/`iPostOpacity`) + full uniform set (`iResolution`/`iTime`/`iTimeDelta`/`iFrame`) → composite to swapchain; **glass-window alpha preserved**. **END** owns runtime GLSL→SPV via **shaderc** with hot-reload from user config; **JAM** stays baked-`.spv` + gains a runtime-SPV→pipeline API (extends JAM, keeps it compiler-free). Registry via `jam::AnyMap` (heterogeneous, user-name-keyed). Reuses the offscreen machinery (transparency stack) + the dead OpenGL `Program.h` config surface (`config::Shader` background+postProcessing, `display.lua`, ParameterText) as the in-tree reference. Rationale: Encapsulation (END owns app-level shader content, JAM owns pipeline mechanics), Lean (reuse existing offscreen + config plumbing).
- **(f) Containers.** `jam::HashMap<Enum,T>`/flat arrays for homogeneous hot-path dispatch (already: `pipelineSpecs[19]`, `atlasDescriptors`); `jam::Function::Map` for event/callback dispatch (already: `View.h:179`); `jam::AnyMap` **only** for the heterogeneous (e) shader registry (per-frame it's 3 probes/`get<T>`, `jam_AnyMap.h:82-88` — hot-path use forbidden); named RAII for destruction-ordered Vk objects (never a generic map — `Pipelines::shutdown`). Rationale: SSOT, Bound, Deterministic.
- **AA — 4× MSAA on the whole scene target.** Transient (`TRANSIENT_ATTACHMENT` + `LAZILY_ALLOCATED`) 4× MSAA color + matching transient 4× stencil, `STORE_OP_DONT_CARE`, color resolve to the single-sample scene texture; on Apple Silicon → memoryless on-chip resolve (near-free), Intel → VRAM-backed (correct, pays bandwidth). One MSAA pipeline set (uniform AA for paths + rotated quads; glyphs already AA but harmlessly supersampled). 8-bit UNORM color; sample count runtime-queried, 1× graceful fallback. Rationale: Lean (one mechanism, one pipeline set, leverages the (e) scene target), Explicit.

**Rejected (with cause):**
- **(c) Draw-indirect** — `vkCmdDrawIndirectCount` is a MoltenVK no-op (`MVKDevice.mm:2808`, `vulkan.mm:2390-2414`); plain indirect is CPU-looped (`MVKCmdDraw.mm:765`). Delivers zero GPU offload on the target platform. Also structurally at odds with the immediate, order-dependent LLGC contract.
- **Whole-frame draw regroup** — batching all primitives then drawing per-type breaks JUCE's painter order for overlapping primitives; reconciling it requires overlap/depth order-safety machinery that fights obj-1 (downstream-only) and the immediate contract. END's draw counts (tens–low hundreds) are already in the comfortable zone; not justified. Draw order stays JUCE-immediate.
- **External renderer dependency (Skia/GPUI/etc.)** — off the table: would replace JAM's LLGC and break obj-1/obj-4. Zero code, zero vendoring, zero upstream sync.

---

## Scaffold

> All new type/method/pipeline-ID names below are **proposals pending NAMES Rule -1 approval** — flagged `[proposed]`. Data structures are given concretely (data dictates design, per JRENG standard); shader/pipeline transforms are given structurally, not as fabricated logic.

### 1. Unified instance record (a) + clip-as-data (d) + bindless index (b)

One SSBO of records, one draw range per paint op (in order). `[proposed]` names:

```cpp
// [proposed] jam::vulkan::PrimitiveRecord — one per rect/image/glyph quad,
// pulled by gl_InstanceIndex; corner via gl_VertexIndex % 4.
struct PrimitiveRecord
{
    float    position[2];   // device-space top-left
    float    size[2];       // quad extent
    float    uvRect[4];     // atlas/texture UV (unused for solid rect)
    float    color[4];      // premultiplied RGBA
    int32_t  clip[4];       // per-primitive clip rect (replaces dormant ivec4 clip channel)
    uint32_t textureIndex;  // bindless index into texture[]/sampler[] (0 = none/solid)
    uint32_t stencilRef;    // baked clip-nesting ref (replaces out-of-band currentStencilRef)
    uint32_t flags;         // primitive kind + winding rule + emoji/mono, etc.
    uint32_t _pad;          // 16-byte alignment
};
```

- Vertex shader: read `records[gl_InstanceIndex]`, expand corner from `gl_VertexIndex`, project by the ortho UBO (unchanged projection model). Retires `quadVertexInputState`/`glyphVertexInputState` (`jam_VulkanPipelinesState.cpp:79-151`).
- Fragment shader: sample `sampler2D` via bindless `texture[records[i].textureIndex]`; apply in-shader rect clip via `clip[4]` (discard outside) — retires the bounding-box scissor limitation for the data-carried case.
- Paths remain the earcut/winding path (separate triangulated buffer), not instanced.

### 2. Clip correctness (d)

- **Rect clip:** in-record `ivec4` + in-shader discard (handles disjoint/rotated the current bbox-scissor drops).
- **Path/image-alpha clip:** stencil, ref baked into the record at emit time (no mutable member) → bug #2 gone by construction.
- **Nested intersection:** replace `stencilWriteState` `compareOp=ALWAYS` (`PipelinesState.cpp:236`) so the inner write is gated by the outer level (depth-nested stencil: write at level N only where stencil==N-1; test ==depth). `[decision-locked: fix]`.
- **Winding:** `[proposed]` `Pipelines::ID::stencilWindingCover` pair — `INCR_WRAP`/`DECR_WRAP` accumulate, cover test `!=0` (non-zero) or bit-0 (even-odd from `Path::isUsingNonZeroWinding()`). Fast fan retained for single convex subpath.
- **Per-layer stencil isolation:** transparency layers get isolated stencil (dedicated per-level clear or separate attachment) instead of the shared uncleared one (`TransparencyStack.cpp:87`). `[decision-locked: fix]`.

### 3. Bindless setup (b)

- Device: enable `descriptorIndexing` + `runtimeDescriptorArray` + `descriptorBindingPartiallyBound` + `descriptorBindingSampledImageUpdateAfterBind` via `VkPhysicalDeviceVulkan12Features` pNext at device creation (`jam_VulkanDevice.cpp` — currently no features enabled).
- Set-1 layout: separate `texture2D[N]` + `sampler[M]` (not combined), fixed `N` `[proposed constant]`, `UPDATE_AFTER_BIND` pool. Updated only when the texture SET changes, not per draw. Retires `allocateImageDescriptor` per-draw (`jam_VulkanGraphics.cpp:317-347`) and the 64-cap.
- MoltenVK constraints honored: fixed bound (no unbounded `[]`), separate texture/sampler, runtime Tier query for the ceiling.

### 4. MSAA scene target (AA)

- Render pass: 4× MSAA color (`TRANSIENT`+`LAZILY_ALLOCATED`, `STORE_OP_DONT_CARE`) + resolve → single-sample scene texture; 4× transient stencil (no resolve). Every pipeline `multisampleState` 1×→4× (`PipelinesState.cpp:207`); attachments 1×→4× (`GraphicsSetup.cpp:136,146,187,197,394`). Swapchain stays single-sample.
- Frame lifecycle gains: render UI → MSAA scene target → resolve → (e) post passes → swapchain. Extends `Graphics::beginFrame`/`endFrame` (`jam_VulkanGraphics.cpp:81-174`).
- 8-bit UNORM color; runtime `framebufferColorSampleCounts` query, 1× fallback.

### 5. Post-process pipeline (e) [PRIMARY]

Pass graph (full Shadertoy parity, reference = dead `Source/graphics/Program.h`):
```
[UI paint] -> MSAA scene target -> resolve -> sceneTex
BufferA..D (feedback ping-pong, iChannel0-3, iResolution/iTime/iTimeDelta/iFrame)
   -> Image/post pass (samples sceneTex as iScene, iPostOpacity) -> swapchain
   (glass-window alpha preserved through post)
```
- **END:** shaderc (libshaderc) compiles user GLSL→SPV on config load / hot-reload; parses uniform set; owns the `jam::AnyMap` registry keyed by user shader name; drives config via the existing `config::Shader` (background + postProcessing) / `display.lua` / ParameterText surface.
- **JAM:** `[proposed]` API to build a `VkPipeline` + `VkShaderModule` from runtime-supplied SPIR-V (JAM stays baked-`.spv` for its own pipelines; gains this consumer entry point). Reuses `jam_VulkanTransparencyStack` offscreen-target machinery for BufferA-D + scene target.
- `jam::Bimap` only if a user-name ↔ registry-slot roundtrip is needed.

### 6. Container placement (f)

| Use | Container | Status |
|---|---|---|
| pipeline-by-ID, descriptor-by-type (hot) | `jam::HashMap<Enum,T>` / flat array | exists (`pipelineSpecs[19]`, `atlasDescriptors`) |
| event/callback dispatch | `jam::Function::Map` | exists (`View.h:179`) |
| (e) user-shader registry (heterogeneous, rare-mutation) | `jam::AnyMap` | new |
| user-name ↔ slot roundtrip (if needed) | `jam::Bimap` | conditional |
| destruction-ordered Vk objects | named RAII members | exists (`Pipelines`) |

### 7. File-level change map

**JAM (`jam_vulkan/`):** `context/jam_VulkanLowLevelGraphicsContext.{h,cpp}` (record model, clip-as-data, retire out-of-band members), `context/jam_VulkanLowLevelGraphicsContextPath.cpp` (winding stencil-then-cover), `context/jam_VulkanPipelines.{h,cpp}` (SSBO vertex input, MSAA, bindless set-1, winding pipelines, nested-stencil ops), `context/jam_VulkanPipelinesState.cpp` (multisample 4×, stencil ops), `context/jam_VulkanGraphics.{h,cpp}` + `jam_VulkanGraphicsSetup.cpp` (MSAA render pass, scene target, bindless pool, frame lifecycle), `device/jam_VulkanDevice.cpp` (enable 1.2 features), `resource/jam_VulkanTransparencyStack.*` (per-layer stencil isolation, scene/BufferA-D targets), `shaders/*` (vertex-pull + bindless + clip-in-shader), new runtime-SPV pipeline API.
**END:** shaderc integration, custom-shader registry (`AnyMap`), config surface wiring (revive `config::Shader` postProcessing on Vulkan), scene-target/post-process consumer, removal of dead `Source/graphics/` OpenGL pipeline.

---

## BLESSED Compliance Checklist

- [x] **Bounds** — bindless array + SSBO are owned, fixed-bound RAII; MSAA attachments transient RAII; destruction-ordered Vk objects stay named RAII (never a generic map). Fallback ownership preserved (JUCE renders when Vulkan absent).
- [x] **Lean** — one vertex path (SSBO) replaces quad/glyph duality; one AA mechanism; reuse of offscreen + config plumbing; draw-indirect and whole-frame regroup rejected as YAGNI/infeasible. Watch `LowLevelGraphicsContext.h` (already 769 lines) — split if the record/clip logic pushes it further.
- [x] **Explicit** — clip carried as visible per-primitive data; no hidden out-of-band state; winding rule read from the path, not inferred.
- [x] **SSOT** — one `PrimitiveRecord` layout for all quads; clip/stencil-ref sourced once at emit; container-per-role matched to workload.
- [x] **Stateless** — removes `isStencilClipActive`/`currentStencilRef` shadow state; records are transient per-frame.
- [x] **Encapsulation** — END owns app-level shader content + registry; JAM owns pipeline mechanics; JUCE surface untouched; layer flow preserved.
- [x] **Deterministic** — clip desync eliminated by construction; winding/nesting made correct; same paint stream → same output.

---

## Open Questions

**No architectural open questions — scope is fully locked.** Remaining items are RFC-scaffold detail for COUNSELOR/Engineer, not gates:
- Bindless array bound `N` — propose a named constant sized to END's live-texture count (2 atlases + images), verified against the runtime Tier limit.
- All `[proposed]` names (`PrimitiveRecord`, `stencilWindingCover`, the runtime-SPV pipeline API, registry types) — require NAMES Rule -1 approval before introduction.
- Intel/AMD MSAA sample-count ceiling — runtime-query `framebufferColorSampleCounts` (not documented by Apple), 1× graceful fallback.
- Whether `jam::Bimap` is needed in (e) — only if a user-name ↔ slot roundtrip surfaces.

---

## Handoff Notes

**Locked decisions (ARCHITECT, this session):**
1. shaderc split — JAM baked `.spv`; END shaderc for hot-reload user config.
2. Batching — keep JUCE immediate paint order; no whole-frame regroup.
3. (e) — full Shadertoy parity (BufferA-D feedback + iChannel0-3 + component capture + iScene/iPostOpacity + full uniform set).
4. (a) — all quad primitives (rect+image+glyph) unified into one vertex-pulled SSBO; paths triangulated.
5. (d) — clip-as-data; fix both deeper defects (nested-path intersection + per-layer stencil isolation); non-zero winding parametrized on the JUCE path rule.
6. AA — 4× MSAA on the whole scene target (transient/memoryless on Apple Silicon).
7. Scope = Core + (a) + (b); (c) draw-indirect excluded (MoltenVK no-op).

**Hard constraints COUNSELOR must preserve:**
- Zero change to the JUCE `LowLevelGraphicsContext` surface API (obj 1).
- Software/native JUCE fallback intact (`Registry::createContext` → `nullptr`). The (e) shader layer is GPU-only — fallback renders UI+text without effects; degrade cleanly.
- Glass-window alpha preserved through the post pass.
- Render-pass interleaving order (upload / transparency boundaries) preserved under the new batching.
- Vulkan objects with destruction-order dependencies stay named RAII — never collapsed into any generic map.
- Doxygen-first on all C++ work; header-only docs; new names gated (NAMES Rule -1).
- Agents never run builds/git; ARCHITECT builds.

**Implementation split:** core rewrite is JAM `jam_vulkan/` (see §7); (e) + shaderc + registry + config revival + dead-OpenGL removal is END.

**Predecessor:** `~/Documents/Poems/dev/jam/RFC-vulkan-llgc.md` (Sprint 50/51 coordinate model — the isomorphic CTM/scissor/state-stack foundation this redesign builds on).

