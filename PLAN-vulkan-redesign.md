# PLAN: Vulkan Rendering Engine Redesign — jam::vulkan LLGC + Graphics

**RFC:** `RFC-vulkan-redesign.md`
**Date:** 2026-07-01
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM — LANGUAGE.md: C++/JUCE is the reference implementation, no overrides.

**Supersedes:** the previous plan (`clipToImageAlpha` — Sprint 54) is fully executed and logged. This plan is a new, much larger objective spanning both `jam` and `end` repos, locked as a **single continuous sprint** (ARCHITECT decision) — no re-gate between phases, execution runs to completion or genuine discrepancy per CAROL.md Step Gate.

---

## Context

END's Vulkan backend (`jam::vulkan::LowLevelGraphicsContext`/`Graphics`) works (Sprints 50-54) but has 6 grounded defects and a per-draw-immediate architecture that caps it at JUCE's design ceiling:

1. **Clip-state desync** — `isStencilClipActive`/`currentStencilRef` (`jam_VulkanLowLevelGraphicsContext.h:750,753`) sit outside the saved `State` struct (`:708-727`); `saveState()`/`restoreState()` never touch them, so a clip inside a save/restore block leaks stencil-gating onto every subsequent draw for the rest of the frame. **Confirmed live** — ARCHITECT's own report this session: "clipToImageAlpha breaks path rendering."
2. **Path fill winding bug** — `triangulatePath` earcuts each sub-path independently (`jam_VulkanLowLevelGraphicsContextPath.cpp:53,82-83`); a holed/self-intersecting path fills the hole solid, ignoring JUCE's winding rule.
3. **Nested clip intersection wrong** — `stencilWriteState` (`jam_VulkanPipelinesState.cpp:232-243`, `passOp` at `:236`) uses `passOp=REPLACE, compareOp=ALWAYS` — an inner clip's write isn't gated by the outer, so multi-level path/image-alpha clips don't truly intersect.
4. **Stencil shared across transparency layers** — `TransparencyTarget` (`jam_VulkanTransparencyStack.h:26-31`) owns only a color attachment; the stencil view is caller-supplied and shared across all nesting levels, never cleared per-layer.
5. **No anti-aliasing** — every `multisampleState()` call is hardcoded `VK_SAMPLE_COUNT_1_BIT` (`jam_VulkanPipelinesState.cpp:203-210`).
6. **Image descriptor churn + hard cap** — `allocateImageDescriptor` (`jam_VulkanGraphics.cpp:317-347`) allocates a fresh `VkDescriptorSet` per `drawImage` call from a pool capped at `maxImageSamplerDescriptors{64}`.

This plan redesigns the engine downstream of JUCE's `Graphics` API (zero surface change) to fix all 6 defects and add: unified vertex-pulling SSBO for all quad primitives, bindless image descriptors, clip-as-data, 4x MSAA with capability+performance calibration, refresh-rate-aware frame budgeting, and a full Shadertoy-parity multi-pass post-process pipeline (END-owned, shaderc-driven). Outcome: correct, deterministic clipping/winding/layering, AA'd output, and a shader/post-process ceiling JUCE cannot reach natively — while JUCE's native/software fallback path stays fully intact.

---

## Locked Decisions (this session, ARCHITECT)

1. Single `PLAN.md`, one continuous sprint — no phase-boundary re-gate.
2. `PLAN.md`/`RFC-vulkan-llgc.md` (Sprint 50/51, `jam` root) deleted — superseded, facts already carried forward here.
3. Target resolution for calibration/validation: **SPEC.md's existing 5K figure stands** (`SPEC.md:957`); GPU budget stays `SPEC.md:938` (<5.8ms/120fps) with `:939` fallback (<11.1ms/60fps). "4K physical" was DPI-class shorthand, not a literal override — no SPEC.md edit.
4. BufferA-D (post-process feedback buffers) render at a **named constant scale factor** of the scene-target extent (scene target itself stays full physical res) — not full-res, not runtime-tuned.
5. MSAA sample count: **runtime-calibrated once at init** via GPU timestamp query (`VkQueryPool`/`vkCmdWriteTimestamp`), decision **locked for session lifetime** (no per-frame adaptive flapping — preserves BLESSED Deterministic).
6. Monitor refresh-rate detection (60 vs 120 target) is a **prerequisite input** to the MSAA calibration budget, not a separate mechanism — detected once via JUCE (`VBlankAttachment`/`Displays`), feeds the calibration's target budget.
7. (e) post-process shader tick rate stays independently `juce::Timer`-driven (existing pattern, `Processor.cpp:114-120`) — decoupled from the core engine's fixed-Hz capability requirement.
8. `(c)` draw-indirect excluded — `vkCmdDrawIndirectCount` is a MoltenVK no-op; plain indirect is CPU-looped. Zero GPU offload on target platform.
9. Draw order stays JUCE-immediate (no whole-frame batching/regroup) — SSBO records are appended in paint order, one draw-range per paint op.

---

## Pathfinder Facts Grounding This Plan (file:line)

- **Device:** `jam_VulkanDevice.cpp:110-122` — `VkDeviceCreateInfo` has no `pNext`, no `enabledFeatures`. First extension point for `VkPhysicalDeviceVulkan12Features`. API target already `VK_API_VERSION_1_2` (`:31`) — descriptor indexing is core, no new extension string needed. No `vkGetPhysicalDeviceProperties`/`Features2` query exists yet (needed for `timestampPeriod`, descriptor-indexing limits, feature-support gating).
- **Pipelines:** `jam_VulkanPipelines.h` — `VertexInput{quad=0,glyph=1}` (`:139`), `StagePair` 8 entries (`:118-127`), `PipelineSpec{id,stagePair,blend,depthStencil,topology,vertexInput}` (`:144-152`), `pipelineSpecs[19]` (`:156-177`), `Pipelines::ID` 19 entries (`:19-40`). Push constants: single shared 44-byte range, `VK_SHADER_STAGE_VERTEX_BIT|FRAGMENT_BIT`, not per-pipeline (`jam_VulkanGraphicsSetup.cpp:62-85`).
- **Graphics lifecycle:** `jam_VulkanGraphics.h:102-140` (begin/endFrame, begin/endRenderPass, beginRenderPassLoad). `resize()` (`jam_VulkanGraphicsSetup.cpp:237-268`) destroys/recreates swapchain+stencil+framebuffers only — does NOT touch descriptor pool/pipelines/command pool/sync objects. Descriptor pool (`:296-375`): `maxImageSamplerDescriptors=64`, `maxDescriptorSets=64`, reset per frame. `allocateImageDescriptor` (`jam_VulkanGraphics.cpp:317-347`): fresh set per draw.
- **TransparencyStack:** `jam_VulkanTransparencyStack.h:26-31` — `TransparencyTarget{Image, VkFramebuffer, VkExtent2D}`, stencil view caller-supplied/shared (`:23,60-64`), not owned per-layer.
- **TextureCache:** `jam_VulkanTextureCache.h:148` — `jam::HashMap<const juce::Image*, CachedTexture>`, pointer-identity keyed.
- **JAM containers in jam_vulkan today:** `jam::HashMap` only (5 sites) — no `AnyMap`/`Function::Map`/`Bimap` in `jam_vulkan`.
- **Naming precedent:** `Pipelines::ID` = `{content}{variant}{stencilOp}` (e.g. `stencilWriteImageAlpha`); `StagePair` = lowercase purpose-driven; shader files = `{stagepair}.{stage}.spv`.
- **MSAA today:** hardcoded `VK_SAMPLE_COUNT_1_BIT` (`jam_VulkanPipelinesState.cpp:203-210`).
- **END `Source/graphics/Program.h` (`:22-329`) is ALIVE code (compiles, owned by `graphics::Compositor.h:189`), but its instantiation point `graphics::Processor processor;` in `end::View.h` is **commented out** — orphaned at runtime, not deleted from the tree. `config::Shader` (`Config.h:34-62`, `background`/`postProcessing` instances at `:275-276`) is a **live, in-use config surface** (ParameterText, `glslBufferSize=65536`, `file::Shaders::get()` bimap for Image/BufferA-D pass-name keys) — this is REUSED by the new Vulkan post-process consumer, not revived-from-dead.
- **shaderc:** confirmed absent from both repos' CMake (`grep -ri shaderc` — zero hits).
- **`jam::AnyMap` in END:** zero hits today — new for this plan.
- **Refresh-rate detection:** no `VBlankAttachment`/`Displays::getDisplays()` usage anywhere in END today. Current repaint is `juce::Timer::startTimerHz(fps)` driven purely by config (`Processor.cpp:114-120`), no display polling.
- **`file::Shaders::get()` Bimap already exists** (pass-name↔slot for Image/BufferA-D) — answers RFC's own open question ("whether `jam::Bimap` is needed in (e)"): yes, already present, reused as-is — no new Bimap.
- **Vulkan consumer entry point:** `end::View.h:185` owns `std::unique_ptr<jam::vulkan::Registry> vulkanEngine` — not `Nexus.h` (confirmed no Vulkan reference there).

---

## Names Gate (new names — ARCHITECT approval required, Rule -1)

| Name | Kind | Rationale |
|---|---|---|
| `PrimitiveRecord` | struct (JAM) | Rule 1 (noun), matches RFC scaffold naming exactly — one instance record per rect/image/glyph/clip-mask quad, pulled by `gl_InstanceIndex` |
| `VertexInput::position2D` | enum value (renamed from `quad`) | Rule 3: after this plan, this vertex-attribute shape is used ONLY by triangulated path geometry (2-float position), never by quads anymore — "quad" would misdescribe its remaining sole use |
| `VertexInput::instanced` | enum value (new, replaces retired `glyph`) | Rule 1/3: describes the SSBO-pulled, zero-vertex-attribute shape shared by all unified quad primitives |
| `Pipelines::ID::opaqueRectInstanced` / `opaqueRectInstancedStencil` | enum values (replace `opaqueRect`/`opaqueRectStencil`) | Rule 5 consistency — same family suffix pattern, "Instanced" marks the SSBO-pulled variant |
| `Pipelines::ID::alphaBlendRectInstanced` / `alphaBlendRectInstancedStencil` | enum values (replace `alphaBlendRect`/`alphaBlendRectStencil`) | Same pattern |
| `Pipelines::ID::imageInstanced` / `imageInstancedStencil` | enum values (replace `transformedImage`/`transformedImageStencil`) | Same pattern — "transformed" dropped since transform is now baked into the record at emit time, not a per-pipeline concern |
| `Pipelines::ID::glyphMonoInstanced` / `glyphMonoInstancedStencil` | enum values (replace `glyphQuadMono`/`glyphQuadMonoStencil`) | Same pattern |
| `Pipelines::ID::glyphEmojiInstanced` / `glyphEmojiInstancedStencil` | enum values (replace `glyphQuadEmoji`/`glyphQuadEmojiStencil`) | Same pattern |
| `Pipelines::ID::clipMaskInstanced` | enum value (replaces `stencilWriteImageAlpha`) | Now SSBO-pulled like every other quad primitive — folded into the unified record path per RFC decision (a) |
| `Pipelines::ID::windingStencilAccumulate` | enum value (new) | Stencil-only pass (`colorWriteMask=0`), `INCR_WRAP`/`DECR_WRAP` per-subpath fan, implements nonzero/even-odd winding without per-subpath earcut |
| `Pipelines::ID::windingCoverOpaque` / `windingCoverAlpha` | enum values (new) | Cover pass — draws path bbox, stencil-tested against the accumulated winding value, writes color |
| `StagePair::instancedRectOpaque` / `instancedRectAlpha` / `instancedImage` / `instancedGlyphMono` / `instancedGlyphEmoji` / `instancedClipMask` | enum values (replace `fillRect`/`alphaRect`/`image`/`glyphMono`/`glyphEmoji`/`imageAlphaMask`) | Mirrors the ID-family rename 1:1, same axis-independence precedent as existing `opaqueTriList`↔`StagePair::fillRect` |
| `StagePair::windingStencil` / `windingCover` | enum values (new) | New shader stage pairs for the winding stencil-then-cover technique |
| `currentState.stencilClipDepth` | `State` struct member (new, replaces `isStencilClipActive`/`currentStencilRef`) | Rule 3: names what it actually is (nesting depth of active path/image-alpha clips), lives INSIDE `State` so `saveState`/`restoreState` cover it — structurally fixes defect #1 |
| `maxBindlessTextures` | named constant (JAM) | Rule 4: bindless descriptor array bound — sized with headroom over current usage (2 atlases + typical multi-tab image count), clamped at init against the runtime-queried `VkPhysicalDeviceDescriptorIndexingPropertiesEXT` limit |
| `postProcessBufferScale` | named constant (END) | Rule 4: BufferA-D render scale relative to scene-target extent — per locked decision 4 |
| `targetFrameBudgetMs` | new parameter, `Registry::createContext`/`Graphics` init | Rule 3: the calibration budget (5.8ms or 11.1ms per SPEC.md, chosen by refresh-rate detection), passed down from END (owns display/refresh-rate query) into JAM (owns the calibration mechanism) |
| `activeSampleCount` | `Graphics` member (new) | Rule 3: session-locked MSAA sample count, set once by calibration, read by every `multisampleState()` call and the scene-target attachment description |
| `calibrateSampleCount` | method (new, `Graphics`) | Rule 1 (verb) — runs the timestamp-queried calibration burst at init, returns the locked sample count |
| `jam::vulkan::PostProcessRegistry` (END-facing type, `jam::AnyMap`-backed) | class (END) | Rule 1 — heterogeneous, rarely-mutated user-shader registry keyed by user shader name, per RFC decision (f) |

---

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, `~/.carol/JRENG-CODING-STANDARD.md`, and this locked PLAN — no deviation, no scope drift.

---

## Phase A — JAM Core (`jam_vulkan/`)

### Step A1: Device feature query + enablement (bindless prerequisite)

**Scope:** `jam_vulkan/device/jam_VulkanDevice.{h,cpp}`
**Action:** Before `createLogicalDevice()` (`:102-122`), add a capability query: `vkGetPhysicalDeviceFeatures2` + `vkGetPhysicalDeviceProperties2` with `VkPhysicalDeviceVulkan12Features`/`Properties` chained via `pNext`. Read back `descriptorIndexing`, `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`, `descriptorBindingSampledImageUpdateAfterBind`, `shaderSampledImageArrayNonUniformIndexing` (needed for `texture[nonuniformEXT(index)]`), and `maxDescriptorSetUpdateAfterBindSampledImages` (bindless array ceiling). Enable only the features actually reported supported, chained onto `VkDeviceCreateInfo.pNext`. Also query `VkPhysicalDeviceProperties.limits.timestampPeriod` and `timestampComputeAndGraphics` (needed for Step A7 calibration) and the queue family's `timestampValidBits`. Store results as `Device` members for downstream consumption (`Pipelines`, `Graphics`).
**Validation:** Auditor confirms every enabled feature bit was gated by its corresponding queried support bit (no blind-enable). No behavior change to existing extensions/queue/allocator setup.

### Step A2: Unified `PrimitiveRecord` + SSBO vertex-pulling

**Scope:** New `jam_vulkan/context/jam_VulkanPrimitiveRecord.h`; `jam_VulkanPipelines.{h,cpp}`; `jam_VulkanLowLevelGraphicsContext*.cpp` (rect/image/glyph/clip-mask emit sites)
**Action:**
- Define `PrimitiveRecord` per RFC scaffold (`RFC-vulkan-redesign.md:119-130`): `position[2]`, `size[2]`, `uvRect[4]`, `color[4]`, `clip[4]` (int32, replaces the dormant `ivec4 clip` push-constant channel already declared-but-unused in every shader), `textureIndex` (bindless), `stencilRef` (baked clip-nesting depth), `flags` (primitive kind + winding + mono/emoji), `_pad`.
- New `VertexInput::instanced` (retires `VertexInput::glyph`; `VertexInput::quad` renamed `position2D`, kept for path-only triangulated draws — Names Gate above).
- New per-frame growable `VkBuffer` SSBO of `PrimitiveRecord[]`, HOST_VISIBLE + persistently mapped, reusing `jam::vulkan::FrameBuffer`'s existing growth/`retiredBuffers` pattern (`jam_FrameBuffer.h:136-137`) rather than inventing a second buffer-growth mechanism (SSOT).
- New descriptor set (set 2 — set 0 stays MVP UBO, set 1 becomes the bindless image array per Step A3) binds this SSBO as `STORAGE_BUFFER`, `readonly`.
- Vertex shader for all instanced pipelines: reads `records[gl_InstanceIndex]`, expands quad corner from `gl_VertexIndex % 4`, applies ortho projection (unchanged UBO). Retires the fixed-function `quad`/`glyph` vertex-attribute bindings for rect/image/glyph/clip-mask (`jam_VulkanPipelinesState.cpp` vertex-input state, exact lines TBD by Engineer against current `quadVertexInputState`/`glyphVertexInputState`).
- Rect/image/glyph/`clipToImageAlpha` emit sites (`jam_VulkanLowLevelGraphicsContext{Path,Image,Glyph}.cpp`) rewritten to APPEND a `PrimitiveRecord` (not raw vertex floats) per primitive, reading `clip[4]`/`stencilRef` from `currentState` (Step A5) at emit time.
**Validation:** Auditor confirms `PrimitiveRecord` is exactly 16-byte aligned (required for SSBO struct layout — `std140`/`std430` rules), confirms no vertex-attribute binding remains wired to any instanced pipeline, confirms FrameBuffer growth reuse (no new buffer-growth code duplicated).

### Step A3: Bindless image descriptor path

**Scope:** `jam_vulkan/context/jam_VulkanGraphics.{h,cpp}`, `jam_VulkanGraphicsSetup.cpp`, `jam_vulkan/resource/jam_VulkanTextureCache.{h,cpp}`
**Action:** Replace set-1's per-draw combined-image-sampler (`allocateImageDescriptor`, `jam_VulkanGraphics.cpp:317-347`) with: separate `texture2D[maxBindlessTextures]` (binding 0, `SAMPLED_IMAGE`) + single non-array `sampler` (binding 1 — TextureCache already uses one `linearSampler` for all images, no evidence of multiple filter configs needed, YAGNI against a full sampler array). Layout flags: `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | PARTIALLY_BOUND_BIT` via `VkDescriptorSetLayoutBindingFlagsCreateInfo`. `maxBindlessTextures` clamped at init against Step A1's queried `maxDescriptorSetUpdateAfterBindSampledImages`. `TextureCache` gains a stable index assignment per cached texture (extends `CachedTexture` with an `int bindlessIndex`); descriptor array slot written once on first upload (`UPDATE_AFTER_BIND`), never per-draw. Over-capacity: graceful degradation mirroring the existing `allocateImageDescriptor` null-handle precedent (skip binding, log) — no LRU eviction added this sprint (YAGNI: SPEC Phase 7 image-preview LRU eviction is future scope, not this plan's).
**Validation:** Auditor confirms descriptor writes happen only on texture upload, never in any per-draw path. Confirms pool/layout changes don't regress the projection UBO (set 0, unaffected).

### Step A4: Clip-as-data — retire out-of-band stencil-clip state (fixes defect #1)

**Scope:** `jam_VulkanLowLevelGraphicsContext.h` (`State` struct `:708-727`, members `:750,753`), all LLGC split `.cpp` files
**Action:** Delete `isStencilClipActive`/`currentStencilRef` as class members. Add `int stencilClipDepth` to the `State` struct itself (Names Gate: `currentState.stencilClipDepth`) — now covered by `saveState()`/`restoreState()`'s existing push/pop. Every `clipToPath`/`clipToImageAlpha` call increments `currentState.stencilClipDepth` and bakes the new depth into the emitted `PrimitiveRecord::stencilRef` (Step A2) at record time. Every content-draw record's `stencilRef` field is populated from `currentState.stencilClipDepth` at THAT record's emit time (not read from a mutable class member later) — this is the structural fix: a leaked clip can no longer desync from JUCE's logical state because there is no out-of-band copy left to desync.
**Validation:** Auditor greps for zero remaining references to `isStencilClipActive`/`currentStencilRef` outside git history. Confirms every draw call site's stencil reference is record-sourced, not member-sourced. This is the direct regression test for ARCHITECT's reported "clipToImageAlpha breaks path rendering."

### Step A5: Nested clip intersection fix (defect #3)

**Scope:** `jam_VulkanPipelinesState.cpp:232-243` (`stencilWriteState`)
**Action:** Change `passOp` `REPLACE`→`INCREMENT_AND_CLAMP`, `compareOp` `ALWAYS`→`EQUAL`. Each nested clip write is gated by `vkCmdSetStencilReference(currentDepth - 1)` (the parent depth) before the write draw; a passing compare increments the existing stencil value by exactly 1, naturally serializing nested writes (depth 0→1→2→...). Content draws continue to test `EQUAL` against their record's baked `stencilRef` (exact depth) — Auditor to confirm this compare mode is what content draws already use, or correct it to match if not.
**Validation:** Auditor constructs/traces a 2-level nested `clipToPath` scenario and confirms the inner region only renders where BOTH clips' stencil bits are satisfied (true intersection), not just the inner clip's own bbox.

### Step A6: Non-zero/even-odd winding stencil-then-cover (defect #2)

**Scope:** `jam_VulkanLowLevelGraphicsContextPath.cpp` (`triangulatePath`, `fillPath`), `jam_VulkanPipelines.{h,cpp}` (new pipelines from Names Gate)
**Action:** `fillPath` branches on path complexity (read via `juce::Path::Iterator`, count subpaths + detect self-intersection eligibility): a single simple convex subpath keeps the existing fast fan/earcut path unchanged (cheap case preserved). A complex/holed path uses two passes: (1) `windingStencilAccumulate` — per-subpath triangle fan (NOT full earcut — correctness comes from the stencil parity, not the triangulation), `INCR_WRAP` (front-facing)/`DECR_WRAP` (back-facing), `colorWriteMask=0`; (2) `windingCoverOpaque`/`windingCoverAlpha` — draws the path's bbox quad, stencil test against the accumulated value: `compareOp NOT_EQUAL, reference=0` for `juce::Path::isUsingNonZeroWinding()==true`, or a bit-0 test (`compareMask=0x01, reference=0, compareOp EQUAL` inverted via prior `INVERT` op — Engineer to confirm exact even-odd encoding against Vulkan's stencil op set) for even-odd.
**Risk (flagged, not silently resolved):** this reuses the SAME stencil buffer as clip-nesting (Step A5). The winding-accumulate pass must not corrupt active clip bits. Engineer partitions the 8-bit stencil buffer by write/compare MASK (e.g. low nibble = clip depth per Step A5, high nibble = winding accumulation scratch, cleared before each `fillPath` complex-path invocation via a scoped clear limited to that mask range) — Auditor validates the bit-range partition does not leak across concerns before sign-off.
**Validation:** Auditor constructs a holed-path test case (e.g. a ring shape) and confirms the hole renders unfilled (not solid) — direct regression test for defect #2. Confirms an active outer `clipToPath` combined with a complex inner `fillPath` still intersects correctly (bit-range partition holds).

### Step A7: 4x MSAA scene target + init-time GPU-timestamp calibration

**Scope:** `jam_vulkan/context/jam_VulkanGraphics.{h,cpp}`, `jam_VulkanGraphicsSetup.cpp`, `jam_VulkanPipelinesState.cpp:203-210`, `jam_vulkan/registry/jam_VulkanRegistry.h`
**Action:**
- `Graphics` gains `activeSampleCount` (Names Gate) and a `VkQueryPool timestampPool` (2 slots: begin/end).
- New `calibrateSampleCount()` (Names Gate), run once during `Registry::createContext`/`Graphics` init, BEFORE the first real frame is presented: for each candidate in descending order `{VK_SAMPLE_COUNT_4_BIT, _2_BIT, _1_BIT}`, render a synthetic full-scene-extent quad + resolve through a throwaway transient MSAA attachment at that sample count, bracketed by `vkCmdWriteTimestamp`, average over a small fixed warm-up-discarded frame count. Convert ticks to ms via `Device`'s queried `timestampPeriod` (Step A1). First candidate whose measured time fits `targetFrameBudgetMs` (Names Gate — passed in from END, Step B1) wins; locks `activeSampleCount` for the session (no re-check). If `timestampComputeAndGraphics`/`timestampValidBits` (Step A1) report unsupported, skip measurement and default deterministically (documented fallback, not silent): `VK_SAMPLE_COUNT_4_BIT` on Apple platforms (memoryless transient resolve, near-free per RFC's own MoltenVK research), `VK_SAMPLE_COUNT_2_BIT` elsewhere.
- Real scene render pass: transient (`TRANSIENT_ATTACHMENT`+`LAZILY_ALLOCATED`) MSAA color + matching transient MSAA stencil at `activeSampleCount`, `STORE_OP_DONT_CARE`, resolve to a single-sample 8-bit UNORM scene texture. Swapchain itself stays single-sample.
- `multisampleState()` (`jam_VulkanPipelinesState.cpp:203-210`) reads `activeSampleCount` instead of the hardcoded `VK_SAMPLE_COUNT_1_BIT`.
- `Graphics::resize()` (`jam_VulkanGraphicsSetup.cpp:237-268`) extended to also recreate the scene target (color+stencil+resolve) at the new extent — it already recreates the stencil image and framebuffers via the same established resize path (Pathfinder-confirmed pattern), this just adds the scene-target attachments to that existing recreation list. No re-calibration on resize (locked-for-session per decision 5).
**Validation:** Auditor confirms `activeSampleCount` is set exactly once per `Graphics` instance lifetime and never mutated outside `calibrateSampleCount()`. Confirms every pipeline's `multisampleState()` call reads the same member (SSOT, no duplicated sample-count literals). Confirms MoltenVK timestamp support is queried, not assumed (flags for manual macOS+Windows hardware verification if the query path itself is unverified against real driver behavior).

### Step A8: Per-transparency-layer stencil isolation (defect #4)

**Scope:** `jam_vulkan/resource/jam_VulkanTransparencyStack.{h,cpp}`
**Action:** `TransparencyTarget` (`:26-31`) gains its own owned `Image stencil` member (RAII, same lifecycle as its existing `Image` color member — created/destroyed together). `ensureTransparencyTarget` (`:60-64`) no longer accepts a caller-supplied shared `stencilView` parameter — creates its own, `LOAD_OP_CLEAR` at layer creation (matching the color attachment's existing per-layer semantics). Render pass used for offscreen composite gains its own stencil attachment description per layer instead of referencing the caller's.
**Validation:** Auditor confirms no remaining call site passes a shared `stencilView` into `TransparencyStack`. Constructs a 2-layer nested `beginTransparencyLayer` scenario with different clip state per layer and confirms no cross-layer stencil bleed.

### Step A9: Rename `VertexInput`/`Pipelines::ID`/`StagePair` per Names Gate; retire dead enum values

**Scope:** `jam_vulkan/context/jam_VulkanPipelines.h`
**Action:** Apply every rename/addition/retirement from the Names Gate table above in one pass — this is bookkeeping over Steps A2-A6's functional changes, not a separate design decision. Update `pipelineCount`/`stagePairCount`/`shaderCount` and all doc-comment counts (established Sprint 54 precedent: every count site gets updated together, verified by direct Read, not just grep).
**Validation:** Auditor confirms zero remaining references to retired names (`opaqueRect`, `transformedImage`, `glyphQuadMono`, etc. without their `Instanced` suffix) anywhere in `jam_vulkan/`. Confirms all count constants match actual array sizes.

---

## Phase B — END (`Source/`)

### Step B1: Refresh-rate detection → `targetFrameBudgetMs`

**Scope:** `Source/end/View.h` (or `Source/end/EventRegistration.cpp` per existing event-wiring pattern), new call into `jam::vulkan::Registry`/`Graphics` init
**Action:** At window/peer creation, query the current monitor's native refresh rate via `juce::VBlankAttachment` (covers macOS via its `CVDisplayLinkGetNominalOutputVideoRefreshPeriod`-backed implementation, confirmed the only cross-platform-complete mechanism — `Displays::Display::verticalFrequencyHz` is Windows/Linux-only, macOS's `findDisplays` doesn't populate it). Compute `targetFrameBudgetMs = (reported rate >= 120 ? 5.8 : 11.1)` (SPEC.md:938-939's own ratios, not new numbers). Pass this value down into the new JAM API surface (`Registry::createContext`/`Graphics` init gains a `targetFrameBudgetMs` parameter, consumed by Step A7's `calibrateSampleCount`). Re-query on monitor change (window dragged to a different-Hz display) — does NOT re-trigger MSAA re-calibration (locked-for-session, decision 5); only affects future `Graphics` instances created after the change (e.g. new windows/tabs), consistent with "set once at init."
**Validation:** Auditor confirms the value crosses the JAM/END boundary as an explicit function parameter (Explicit — no hidden global), confirms no continuous polling loop was introduced (single query at creation, not per-frame).

### Step B2: Dead OpenGL pipeline removal

**Scope:** `Source/graphics/Processor.{h,cpp}`, `Source/graphics/Compositor.{h,cpp}`, `Source/graphics/Program.h`
**Action:** Engineer first greps for ANY remaining live instantiation of `graphics::Processor`/`graphics::Compositor` beyond the already-commented-out `end::View.h` member — if genuinely zero, delete these 3 files (`rm`, not blanked). `config::Shader` (`Config.h:34-62`), its `background`/`postProcessing` instances, `ParameterText`/`glslBufferSize`, and `file::Shaders::get()` Bimap are **preserved** — they are the live config surface reused by Step B3's Vulkan consumer, not part of this deletion.
**Validation:** Build breakage (if any) surfaces immediately as compiler errors per Refactor-Rewrite Discipline (delete first) — Auditor confirms zero dangling references remain in `end::View.h`/`Nexus.h`/CMake source lists.

### Step B3: shaderc integration + `PostProcessRegistry`

**Scope:** New END module (e.g. `Source/graphics/shader/`), CMakeLists.txt (link `libshaderc`), new `jam::AnyMap`-backed `PostProcessRegistry` (Names Gate)
**Action:** Link `libshaderc` (confirmed absent today) into the END target only (JAM stays baked-`.spv`, per locked decision — RFC Handoff Notes item 1). On `config::Shader::loadFromPath` (existing, reused), compile the user's GLSL to SPIR-V at runtime via shaderc, register the resulting `VkShaderModule`+pipeline in `PostProcessRegistry` keyed by user shader name (`juce::Identifier`). Hot-reload path reuses the existing lua-file-watcher pattern (`ENDApplication`'s `jam::File::Watcher::Listener`, per SPEC.md §1.5) — GLSL file change triggers re-compile + registry re-register, same init/reload code-path symmetry SPEC.md already mandates for lua config.
**Validation:** Auditor confirms `PostProcessRegistry`'s `get<T>()` is never called on a per-frame hot path (RFC's own AnyMap cost citation — 3 hashmap probes — is fine for rare shader-swap lookups, forbidden if found in the per-frame render loop).

### Step B4: Multi-pass Shadertoy-parity post-process pipeline

**Scope:** END consumer of JAM's new runtime-SPV pipeline API (JAM side: new API on `Pipelines`/`Graphics` to build a `VkPipeline` from runtime-supplied SPIR-V, additive to the existing baked-`.spv` path — no change to JAM's own pipeline loading)
**Action:** Reuses Step A8's per-layer offscreen machinery for BufferA-D (feedback ping-pong) + the scene target (from Step A7) as `iScene`. BufferA-D render at `postProcessBufferScale` (Names Gate, locked decision 4) fraction of the scene target's extent — resized alongside the scene target in `Graphics::resize()` (Step A7's extended resize path). Full Shadertoy uniform set (`iResolution`/`iTime`/`iTimeDelta`/`iFrame`, `iChannel0-3`, `iPostOpacity`) wired per the existing dead-pipeline's `Uniform` struct shape (`Program.h:22-130`, values map + setters — reused as the reference shape for the new uniform wiring, not reused as code, since it was GL-specific). Glass-window alpha preserved through the final composite to swapchain (explicit requirement, RFC Handoff Notes).
**Validation:** Auditor confirms BufferA-D extent is computed from `postProcessBufferScale × sceneExtent` in exactly one place (SSOT), confirms glass alpha survives the full pass graph (visual diff against a pre-post-process reference frame, alpha channel specifically).

### Step B5: Doc + debt closure

**Scope:** `end/DEBT.md`, `end/ARCHITECTURE.md`
**Action:** Update `ARCHITECTURE.md`'s GPU/rendering section to reflect the new SSBO/bindless/MSAA/post-process architecture (codebase is SSOT for this doc). Close out any DEBT.md entries this plan resolves (defects #1-#6 above) at `/log` time — narrative closure only for the 3 previously-flagged-but-undecided residuals (stencil leak = defect #1, resolved by Step A4; staging-buffer lifetime race = separate, NOT in this plan's scope, stays open; jaggy mask edges = resolved by Step A7's MSAA).
**Validation:** `DEBT.md` reflects only genuinely still-open items after this sprint (staging-buffer race explicitly remains, everything else resolved here is closed).

---

## BLESSED Alignment

- **B (Bound):** Bindless array + SSBO are fixed-bound, owned RAII (reuses `FrameBuffer`'s retire pattern). MSAA attachments transient RAII. Destruction-ordered Vk objects (pipeline cache, pipelines, layouts, shader modules) stay named RAII members — never collapsed into any generic map, per `Pipelines::shutdown()`'s existing pattern (unchanged). JUCE fallback ownership preserved (`Registry::createContext` → `nullptr` still renders via JUCE native/software).
- **L (Lean):** One vertex path (SSBO) replaces the `{quad,glyph}` duality — net pipeline-ID count stays the same (11 retired, 11 replacements, +3 new winding pipelines, justified by defect #2's actual fix, not speculative). YAGNI: no LRU eviction added for bindless textures (not yet needed), no full sampler array (single `linearSampler` suffices), no per-frame adaptive MSAA (session-locked instead). `jam_VulkanLowLevelGraphicsContext.h` (already 769 lines) — Engineer splits further if the record/clip logic pushes it past 300 (per-file, doc-excluded).
- **E (Explicit):** Clip carried as visible per-primitive data (`PrimitiveRecord::clip`/`stencilRef`) — no hidden out-of-band state (directly retires the two members that violated this). Winding rule read from `juce::Path::isUsingNonZeroWinding()`, never inferred. `targetFrameBudgetMs` crosses the JAM/END boundary as an explicit parameter, not a global.
- **S (SSOT):** One `PrimitiveRecord` layout for all quad primitives. `activeSampleCount` read from one member by every pipeline's `multisampleState()`. `postProcessBufferScale` computed in one place. Bindless index computed once per texture upload, not per draw.
- **S (Stateless):** `isStencilClipActive`/`currentStencilRef` (persistent, out-of-band, machinery-tracked) removed — replaced by `currentState.stencilClipDepth`, which is Model-analogous state properly scoped to `State`'s save/restore lifecycle, not orchestrator-tracked machinery state.
- **E (Encapsulation):** END owns app-level shader content + registry + refresh-rate query; JAM owns pipeline mechanics + calibration mechanism. JUCE surface untouched throughout. Layer flow stays unidirectional (JAM never includes END headers).
- **D (Deterministic):** Clip desync eliminated by construction (A4). Winding/nesting made correct (A5/A6) — same input path always produces the same fill. MSAA locked-for-session (A7) — same paint stream on the same machine always renders the same AA level after calibration, never mid-session flapping.

---

## Risks / Open Questions

- **Winding-accumulate vs clip-nesting stencil bit-range partition (Step A6)** — flagged as requiring careful Engineer implementation + explicit Auditor test construction (2-level clip + holed-path fill combined scenario). Not fabricated as already-solved.
- **MoltenVK timestamp query support (Step A7)** — `timestampComputeAndGraphics`/`timestampValidBits` queried at runtime (Step A1), not assumed present; documented deterministic fallback exists if absent.
- **Bindless array bound `maxBindlessTextures`** — proposed value pending Engineer's exact headroom calculation against current TextureCache usage (2 atlases + typical multi-tab image count) and the runtime-queried hardware ceiling; final number confirmed at implementation time, not fabricated here.
- **`fill_rect.frag`/`fill_rect_alpha.frag` merge-ability** — NOT collapsed into one shader this sprint (preserves existing precedent exactly, avoids an unverified behavioral assumption about why they're currently split); flagged as a possible future YAGNI-driven simplification, not this plan's scope.
- **Intel/AMD Windows MSAA sample-count ceiling** — runtime-queried (`framebufferColorSampleCounts`), not documented ahead of time; Step A7's descending-candidate calibration loop handles this generically regardless of the actual ceiling.
