# PLAN: Shader Pipeline — Background render() + Post-Process Chain

**RFC:** none — objective from ARCHITECT discussion (this session); supersedes PLAN-vulkan-redesign.md Steps B3/B4
**Date:** 2026-07-02
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM — LANGUAGE.md: C++/JUCE reference implementation, no overrides

---

## Context

END's Shadertoy-compatible shader pipeline (SPEC.md Phase 14, `SPEC.md:856-874`) lost its execution engine when the dead OpenGL pipeline was deleted (B2). The config surface survived: `config::Shader` (`Config.h:34-62`, two instances background/postProcessing), `file::Shaders` pass bimap (`Bimap.h:496-546` — bufferA-D, common, image), and the display.lua graphics block (`display.lua:88-104`). This plan rebuilds execution on the Vulkan engine per the revised design locked with ARCHITECT this session — replacing the old PLAN's B3/B4 with a component-centric architecture.

## Locked Decisions (ARCHITECT, this session)

1. **Vocabulary:** `paint` = geometry/image/text; `render` = shader. API: `jam::vulkan::render (juce::Graphics&, Shader&)` free function; compiled-shader type: `jam::vulkan::Shader`.
2. **One active shader per slot** (background, post-process), each **multi-pass** (Image + optional BufferA-D feedback passes, Common prelude) per the existing `file::Shaders` bimap.
3. **Background is a self-managed dumb component** — owns its `jam::vulkan::Shader` and its repaint timer (`frame_rate` config). View routes config events to it; View never pokes `repaint()` downstream.
4. **GPU off → Shader never created.** No runtime fallback branch; on the CPU LLGC no shader object exists at all.
5. **Post-process is a per-window frame-graph stage**, not a component: END hands the compiled chain to `Registry` (app-global, matches config granularity); each `Graphics` rebuilds lazily via generation counter. PLAN B3's name-keyed `PostProcessRegistry` **dropped** (YAGNI).
6. **Config drives everything; config owns file watching.** View/Background listen to config events only (`ID::background`, `ID::postProcessing`). Hot reload = same code path as init.
7. **Component/scene painting always full resolution.** Only shader intermediate passes render scaled: `background_resolution` / `post_processing_resolution` (0.0–1.0, hot-reloadable, documented in display.lua) — replaces both the single `resolution_scale` key and old PLAN decision 4's compile-time `postProcessBufferScale` constant.
8. **shaderc = static `libshaderc_combined` from the Vulkan SDK, END target only.** Verified locally: `/usr/local/lib/libshaderc_combined.a` (universal x86_64+arm64), header `/usr/local/include/shaderc/shaderc.hpp`, shaderc 2026.2.1; linked footprint bound ≈ 18 MB (dylib equivalence). SDK already a build prerequisite (glslc autocompile, AppBuilder.cmake:564-606). JAM never links shaderc — receives finished SPIR-V.
9. **Engine stamps Shadertoy uniforms at record time** (`iTime` from clock, `iFrame`, `iResolution`, `iTimeDelta`, `iMouse` per SPEC.md:860) — no component-managed frame state, no `advanceFrame()`.

## Grounding Facts (file:line)

- `juce::Graphics::getInternalContext()` → `LowLevelGraphicsContext&` (`juce_GraphicsContext.h:747`) — the `render()` entry seam.
- `jam::vulkan::LowLevelGraphicsContext` (`jam_VulkanLowLevelGraphicsContext.h:27`) — target of the downcast.
- Scene-pass suspend/resume machinery exists: `Graphics::endRenderPass()` (idempotent, `jam_VulkanGraphics.h:268`) + `beginRenderPassLoad()` (`:280-288`) — the TransparencyStack pattern, reused for mid-paint offscreen buffer passes.
- `Pipelines::createShaderModule (VkDevice, const char*, int)` is already public static (`jam_VulkanPipelines.h:116`) — runtime SPIR-V → `VkShaderModule` exists today.
- Swapchain composite lives in `Graphics::endFrame()` — post-process chain hooks between scene resolve and that composite.
- `Registry` public surface (`jam_VulkanRegistry.h:50-93`): ctor params threaded END→JAM precedent (`targetFrameBudgetMs`/`pipelineCacheFile`/`gpuEnabled`); `setGpuEnabled()` is the mutable-setter precedent for `setPostProcess()`.
- Event handler precedent: `EventRegistration.cpp:7-66` (`jam::Function::Map`, e.g. `ID::gpu` → `Registry::getInstance()->setGpuEnabled()`).
- Timer precedent: `MessageOverlay.h:61` (`private juce::Timer`).
- Validator precedent: `Config.cpp:198-204` (Filter/FontRasterizerBackend bimap validators).
- `config::Shader::loadFromPath` fires `IDtype::graphics` property change (`Config.h:44-55`) — the existing downstream signal, previously consumed by graphics::Processor.

## Names Gate (new names — ARCHITECT approved in discussion where marked)

| Name | Kind | Status |
|---|---|---|
| `jam::vulkan::Shader` | class — compiled multi-pass shader (SPIR-V per pass + params) | **locked** |
| `jam::vulkan::render (g, shader)` | free function — emits shader draw through juce::Graphics | **locked** |
| `Registry::setPostProcess` | method — installs/clears the active post-process Shader | proposed (mirrors `setGpuEnabled`) |
| `graphics::Background` | juce::Component subclass, `Source/graphics/Background.h` | **locked** |
| `graphics::Compiler` | END-side GLSL→SPIR-V compile unit (shaderc wrapper + Shadertoy prelude), `Source/graphics/Compiler.h` | proposed |
| `Graphics::resumeRenderPass` | rename of `beginRenderPassLoad` (both overloads) — Rule 3: role (continue on existing pixels), not Vulkan loadOp mechanism | **locked** |
| `background_resolution` / `post_processing_resolution` | display.lua keys (0.0–1.0) | **locked** |

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and this locked PLAN.

---

## Steps

### Step 0: Rename `beginRenderPassLoad` → `resumeRenderPass`

**Scope:** `jam_vulkan/context/jam_VulkanGraphics.{h,cpp}` (+ split TUs), all call sites (`TransparencyStack`, LLGC TUs), doc comments
**Action:** Rename both overloads (`(VkFramebuffer)` and `()`) and every call site; doc comments updated to state the semantic (begin = fresh/clear, resume = continue on existing pixels) with loadOp as the mechanism detail.
**Validation:** Auditor confirms zero remaining `beginRenderPassLoad` references; doc counts/wording consistent.

### Step 1: `jam::vulkan::Shader` — compiled multi-pass shader data type

**Scope:** new `jam_vulkan/shader/jam_VulkanShader.h` (+.cpp only if >~300 LOC logic), `jam_vulkan.h` include list
**Action:** Data-only type: ordered passes (BufferA-D subset + Image, each = SPIR-V fragment blob; one shared vertex stage = existing fullscreen-triangle/quad technique), per-pass feedback flag (samples own previous frame), params (`opacity`, `resolutionScale`, `filter` linear/nearest), and a monotonically increasing `generation` id (SSOT for "chain changed" detection). No Vulkan handles — pipelines/images belong to `Graphics` (Bound: device-lifetime objects owned where the device lives). Constructed by END (Compiler, Step 6) with all fields explicit.
**Validation:** Auditor confirms zero VkPipeline/VkImage ownership inside `Shader`; all fields explicit ctor params.

### Step 2: `Graphics` runtime shader-pass machinery

**Scope:** `jam_vulkan/context/jam_VulkanGraphics.{h,cpp}` (+ new split TU per Lean if needed, e.g. `jam_VulkanGraphicsShaderPass.cpp`), `jam_VulkanPipelines.h` (reuse `createShaderModule`)
**Action:**
- Per-`Graphics` cache keyed by `Shader::generation`: VkPipelines built from the Shader's SPIR-V (reuse `Pipelines::createShaderModule`), offscreen buffer images (ping-pong pair per feedback pass) at `extent × resolutionScale`, sampler per `filter`. Old generation's resources retired via the established deferred-destroy pattern (never mid-frame).
- Buffer-pass recording: suspend scene pass (`endRenderPass()`), record each buffer pass into its offscreen image (single-sample render pass), resume (`resumeRenderPass()`, Step 0 rename) — exact TransparencyStack precedent.
- Shadertoy uniform block (push constants): `iResolution`, `iTime`, `iTimeDelta`, `iFrame`, `iMouse`, channel bindless indices, `opacity` — stamped from clock/frame counter at record time (SSOT: one stamp site).
- Buffer images registered in the bindless array (BindlessAllocator) — Image pass samples them as `iChannel0-3`.
- Resize: buffer images recreated with scene target (existing resize path).
**Validation:** Auditor confirms deferred retire (no destroy-while-referenced), one uniform-stamp site, buffer extent computed in exactly one place, no per-frame pipeline rebuild when generation unchanged.

### Step 3: `jam::vulkan::render()` — background injection seam

**Scope:** new `jam_vulkan/context/jam_VulkanRender.h` (free function; small), `jam_VulkanLowLevelGraphicsContext.h` (one public entry method)
**Action:** `render (juce::Graphics& g, Shader& shader)`: `dynamic_cast<LowLevelGraphicsContext*> (&g.getInternalContext())`; on match, LLGC entry method runs the Shader's buffer passes (Step 2 machinery, mid-paint suspend/resume), then emits the Image pass as a quad at the current clip bounds/transform, full resolution, into the scene pass in paint order. Non-Vulkan context → no-op by design (locked decision 4 makes this unreachable in practice; positive check, no fallback machinery).
**Validation:** Auditor confirms the Image-pass quad honors current clip/transform state (record-sourced, per clip-as-data), confirms no scene-pass state leaks across suspend/resume.

### Step 4: `Registry::setPostProcess` + per-window chain execution

**Scope:** `jam_vulkan/registry/jam_VulkanRegistry.h`, `jam_VulkanGraphics.{h,cpp}` (endFrame)
**Action:** `Registry::setPostProcess (std::unique_ptr<Shader>)` — installs (or clears with nullptr) the app-global post-process chain; Registry owns it. In `Graphics::endFrame()`, between scene resolve and swapchain composite: if a chain is installed, run its buffer passes (offscreen, `post_processing_resolution` extent), then the Image pass renders to the swapchain sampling the resolved scene — this pass IS the composite, mixing by `opacity` (`post_processing_opacity`: 0 = original scene, 1 = fully processed) and preserving glass-window alpha. No chain → existing composite untouched.
**Validation:** Auditor confirms glass alpha survives the full pass graph; confirms Graphics reads the chain via generation compare (no callback into windows, windows pull); confirms nullptr-clear path retires resources deferred.

### Step 5: CMake — link shaderc

**Scope:** END `CMakeLists.txt` (target only; JAM untouched)
**Action:** `find_library` `shaderc_combined` + include dir from the SDK/`/usr/local` (same provenance as glslc), `target_link_libraries` END only. Windows: SDK's `shaderc_combined.lib` via the same find path (`$ENV{VULKAN_SDK}`).
**Validation:** Auditor confirms JAM modules gain zero shaderc dependency; no vendored source.

### Step 6: `graphics::Compiler` — GLSL → `jam::vulkan::Shader`

**Scope:** new `Source/graphics/Compiler.{h,cpp}` (or header-only per Lean)
**Action:** Input: a `config::Shader` state tree (pass properties per `file::Shaders` bimap — Common, Image, BufferA-D). Wraps each pass with the Shadertoy prelude (declarations for `iResolution/iTime/iTimeDelta/iFrame/iMouse/iChannel0-3`, `main()` calling user `mainImage()`, Common prepended — SSOT: one prelude definition), compiles via `shaderc::Compiler` to SPIR-V, assembles a `jam::vulkan::Shader` with params from config (`*_opacity`, `*_resolution`, `filter`). Compile error → `debug::Log` the shaderc diagnostic, return nothing (caller keeps last good shader). One Compiler, both consumers (Background + post-process handler).
**Validation:** Auditor confirms single prelude definition; confirms error path retains last-good (no blank screen on typo mid-hot-reload); confirms no shaderc symbol leaks into any header included by JAM.

### Step 7: `graphics::Background` component

**Scope:** new `Source/graphics/Background.h` (namespace `graphics`, juce::Component subclass), `Source/end/View.{h,cpp}` (add as rearmost child), `Source/end/EventRegistration.cpp` (route events)
**Action:** Dumb component, `private juce::Timer` (MessageOverlay precedent): owns `std::unique_ptr<jam::vulkan::Shader>`, `paint()` = `jam::vulkan::render (g, *shader)` when shader exists (positive check — absent when config empty or GPU off), `timerCallback()` = `repaint()`, timer runs at `frame_rate` Hz only while a shader is active. Receives config via one tell-API invoked by View's handlers: `ID::background` change → recompile via Compiler (empty name → discard shader, stop timer); `frame_rate`/`background_opacity`/`background_resolution`/`filter` → param update (bumps generation → Graphics rebuilds); `ID::gpu` off → discard shader, on → recompile. View adds it as the bottom-most child, full window bounds.
**Validation:** Auditor confirms View never calls `repaint()`/timer methods on Background (tell, don't ask); confirms timer stopped when no shader (no idle burn); confirms zero shader state when GPU disabled.

### Step 8: Post-process event wiring

**Scope:** `Source/end/EventRegistration.cpp` (View handlers)
**Action:** `ID::postProcessing` change (and `post_processing_opacity`/`post_processing_resolution`/`filter` params) → Compiler (Step 6) → `Registry::getInstance()->setPostProcess (std::move (shader))`; empty project name → `setPostProcess (nullptr)`. `ID::gpu` handler extended: off → `setPostProcess (nullptr)`, on → recompile from current config. Init path (first config load) runs the identical handlers — SPEC's init/reload symmetry.
**Validation:** Auditor confirms init and hot-reload share one code path; confirms no file watching added outside config layer.

### Step 9: display.lua config surface

**Scope:** `Source/config/lua/display.lua`, `Source/config/Config.{h,cpp}` (validators), `Source/Identifier.h` (new keys)
**Action:** Rename `resolution_scale` → `background_resolution`; add `post_processing_resolution` (both 0.0–1.0, scale implicit, doc comments in display.lua). Range validators per the existing validator registration pattern (`Config.cpp:198-204`). All keys hot-reloadable through the existing config event flow.
**Validation:** Auditor confirms no remaining `resolution_scale` reference anywhere; confirms doc comments state ranges and semantics; confirms validators reject out-of-range.

### Step 10: Docs + PLAN closure

**Scope:** `ARCHITECTURE.md` (END), `PLAN-vulkan-redesign.md`
**Action:** ARCHITECTURE.md gains the shader-pipeline section (Background component, render() seam, post-process frame-graph stage, config flow). Mark old PLAN B3/B4 superseded by this plan (B1/B5 remain).
**Validation:** Auditor confirms ARCHITECTURE.md matches shipped code (descriptive, codebase is SSOT).

---

## BLESSED Alignment

- **B:** Shader = data, owned by its domain (Background owns background's, Registry owns post-process); all Vk resources owned by Graphics with deferred retire; buffer ping-pong images RAII via existing Image type.
- **L:** PostProcessRegistry dropped (YAGNI); one Compiler for both slots; reuses TransparencyStack suspend/resume, createShaderModule, bindless array, resize path — no new mechanisms where ones exist.
- **E:** All params explicit (Shader ctor, setPostProcess, Background tell-API); uniforms stamped visibly at one site; Registry singleton access mirrors the established `ID::gpu` handler precedent.
- **S (SSOT):** One prelude, one uniform-stamp site, one buffer-extent computation, generation counter as the single "changed" truth; config is the single source for every knob.
- **S (Stateless):** Background holds no frame state (engine stamps time/frame); View tells, Background manages itself.
- **E (Encapsulation):** END owns GLSL/compile/config; JAM owns execution; JAM never sees shaderc; unidirectional flow preserved.
- **D:** Same config + same GLSL → same SPIR-V → same pipelines; generation compare makes rebuilds deterministic; no mid-frame resource swaps.

## Risks / Open Questions

- **Mid-paint suspend/resume for background buffer passes** interleaves with TransparencyStack's own suspend/resume — Engineer must verify nesting order when a transparency layer is active during Background::paint (Background is bottom-most, painted before overlays, so in practice no layer is active — Auditor constructs the trace to confirm).
- **Feedback ping-pong across frames** requires buffer images to survive frame boundaries (not transient) — flagged for Auditor lifetime review.
- **Windows link** (`shaderc_combined.lib` runtime-library flavor vs MSVC/clang-cl flags) — verified only at ARCHITECT's Windows build, same status as the existing Windows-unverified set.
