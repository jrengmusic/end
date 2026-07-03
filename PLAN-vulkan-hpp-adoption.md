# PLAN: vulkan-hpp Adoption — Full vk:: Sweep + Opacity Defect Fix

**RFC:** RFC-vulkan-hpp-adoption.md
**Date:** 2026-07-02
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM — LANGUAGE.md C++/JUCE reference implementation; single-header-preferred Lean override applies (no new TUs created by this plan)

## Overview

Convert jam_vulkan's entire raw C Vulkan surface (~9.5K LOC across 37 files) to vulkan-hpp plain `vk::` types in one sprint, under the RFC's four locked conditions; fix the shader-opacity runtime defect (background + post-process) in the same sprint. END code untouched (boundary invariant).

## Locked Decisions (RFC, ARCHITECT-ratified)

1. **Plain `vk::` types only** — no `vk::raii`, no `UniqueHandle`, no `SharedHandle`, no `vk::su::*`. JAM's wrappers (Buffer/Image/Owner/unique_ptr) remain the RAII layer.
2. **Config in `jam_vulkan.h` before include:** `VULKAN_HPP_NO_EXCEPTIONS`, `VULKAN_HPP_ASSERT = jassert`, `VULKAN_HPP_ASSERT_ON_RESULT = jassert`.
3. **Vendored matched SDK 1.4.350 header generation, wholesale** replacing `jam_vulkan/vulkan/` contents. Never mix generations (`vulkan.hpp:39` static_assert enforces).
4. **`-fno-strict-aliasing`** on the TU compiling jam_vulkan (docs/Usage.md:1039–1041 requirement).
5. **Error policy:** Result-returning overloads on every setup/creation path — bool/nullptr propagation to `Registry::createContext`'s never-null CPU-fallback contract (`jam_VulkanRegistry.h:186–261`) preserved **in release**. Enhanced value-returning calls only where failure is programmer error.
6. **Migration shape: full sweep, one sprint.** Delete-first / whole module speaks `vk::` at once; no dual-vocabulary period. Compiler errors (ARCHITECT builds) are ground truth for completion.
7. **Dispatch:** `DispatchLoaderStatic` default (static linking). Never `vk::detail::DynamicLoader`.
8. **In-sprint defect:** background/post-process shader opacity has zero runtime effect despite being wired end-to-end.
9. **Device conformance fixes in sweep scope** (RFC Open Q4 RESOLVED, ARCHITECT-directed; vk-bootstrap rejected as dependency — patterns as reference only): (a) `VK_KHR_portability_enumeration` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` at instance creation; (b) `VK_KHR_portability_subset` presence-checked before enable (today pushed unconditionally under `JUCE_MAC`, `jam_VulkanDevice.cpp:152–158`); (c) validation-layer + debug-messenger wiring, debug builds only, routed through jassert/`debug::Log`.
10. **Present-mode negotiation** (RFC Handoff 9, ARCHITECT-directed): replace hardcoded FIFO with enumerate-prefer-MAILBOX-fallback-FIFO per the RFC Scaffold's BLESSED-compliant pattern (Result-checked, positive selection, FIFO fallback total by spec guarantee). Runtime pacing validation under MAILBOX rides ARCHITECT's post-sweep multi-pass testing (RFC Open Q5).

## Grounding Facts (Pathfinder, this session)

- Unity build: `jam_vulkan.cpp` includes all 37 .cpp files → **one TU**; vulkan.hpp's ~1s/TU compile cost paid once — PCH unnecessary (RFC residual 1 resolved by measurement + build shape).
- `jam_vulkan.h:39` includes `vulkan/vulkan.h`; zero existing `VULKAN_HPP_*` config; MoltenVK include Apple-only at `:36`; VMA declarative include at `:64`.
- Current vendored set: `vk_platform.h`, `vulkan_core.h`, `vulkan.h`, `vk_icd.h`, `vk_layer.h`, `vulkan_metal.h`, `vulkan_win32.h`, `vulkan_xlib.h`, `vulkan_wayland.h` — C-only, no .hpp.
- VMA seam: `vmaCreateAllocator` (`jam_VulkanDevice.cpp:184`), `vmaCreateBuffer/Image` in `resource/jam_VulkanBuffer.h:27–96`, `jam_VulkanImage.h:27–41`, `jam_VulkanUploadHelpers` — VMA keeps C types; `vk::` ↔ `Vk*` implicit 64-bit conversion covers pass-in; out-params need C temporaries wrapped after.
- Boilerplate concentration: `jam_VulkanPipelines.cpp` (313 sType/CreateInfo instances), `jam_VulkanGraphics.cpp` (~120), `jam_VulkanPipelinesState.cpp` (~80% of file), `GraphicsSetup*` (36–53%).
- Two-call enumerate loops: `jam_VulkanDevice.cpp:56,63` (physical devices) — delete via enhanced enumerations.
- Never-null chain: `Registry::createContext` → `device->isValid()` gate → GPU branch nullptr → CPU `jam::LowLevelGraphicsGlyphRenderer` fallback.
- Opacity trace (verified write sites, no diagnosis yet): background — `jam_VulkanLowLevelGraphicsContextRender.cpp:96–101` stamps `imageUniforms.opacity = opacity` then `vkCmdPushConstants`; post-process — `jam_VulkanGraphicsShaderPass.cpp:244–263` same shape; buffer passes fixed 1.0 at `:179`; Registry origin `jam_VulkanRegistry.h:131–150`.
- Non-Vulkan files (no conversion): shader/ descriptors, font/ (14.3K), earcut, Registry internals, TransformState/Colour.
- Build flags: no `-fno-strict-aliasing` anywhere today; JAM modules compiled inside consumer target's TU set (END `CMakeLists.txt:101`).

## Names Gate

Vocabulary conversion introduces no new names (`VkDevice` → `vk::Device`, `vkCmdX(cmd,…)` → `cmd.x(…)`). One new name from the RFC Scaffold:

| Name | Kind | Status |
|---|---|---|
| `selectPresentMode` | swapchain-setup function (enumerate-prefer-MAILBOX-fallback-FIFO) | proposed (RFC Scaffold, ARCHITECT-authored pattern) |

Any other genuinely new name that surfaces mid-sweep stops for ARCHITECT per Decision Gate.

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and this locked PLAN. ARCHITECT builds gate compile-completion (agents never build).

---

## Steps

### Step 1: Opacity defect — root cause + fix (pre-sweep, on current code)

**Scope:** `jam_VulkanLowLevelGraphicsContextRender.cpp`, `jam_VulkanGraphicsShaderPass.cpp`, `jam_VulkanShaderInstance.{h,cpp}`, END `Source/graphics/Compiler.cpp` (generated GLSL), read-only elsewhere
**Action:** Root-cause by reading the full stamp→push→consume chain: stamp order vs `stampChannels()` overwrite, push-constant offset/stage flags vs pipeline layout, generated GLSL opacity formulas (background `userColor.a * opacity`, post-process `mix(iScene, userColor, opacity)`), pipeline blend/write state (blendEnable VK_FALSE + full RGBA write is the documented contract). Fix the found defect on the current C code — isolated from the sweep so the fix is bisectable.
**Validation:** Auditor confirms fix preserves the ShaderUniforms SSOT stamp sites and both documented opacity formulas; ARCHITECT visual gate.

### Step 2: Vendor SDK 1.4.350 headers + config site + build flag

**Scope:** `jam_vulkan/vulkan/` (wholesale replace), `jam_vulkan.h`, JAM `cmake/BuildSetup.cmake` / END `CMakeLists.txt` (flag only)
**Action:**
- Replace `jam_vulkan/vulkan/` contents wholesale with the matched set from `~/Documents/Poems/dev/jam/___sdk___/vulkan/macOS/include/vulkan/` (SDK 1.4.350, `VK_HEADER_VERSION == 350` verified at its `vulkan.hpp:40`; in-tree SDK copy, ARCHITECT-designated source — gitignored at jam `.gitignore:36`, so it is the copy source only, never the vendored location): `vk_platform.h`, `vulkan_core.h`, `vulkan.h`, platform headers (metal/win32), `vulkan_hpp_macros.hpp`, `vulkan.hpp`, `vulkan_enums.hpp`, `vulkan_handles.hpp`, `vulkan_structs.hpp`, `vulkan_funcs.hpp`, `vulkan_to_string.hpp`; opt-in `vulkan_format_traits.hpp`, `vulkan_hash.hpp` copied but included only when first consumed. **Exclude** `vulkan_raii.hpp`, `vulkan_shared.hpp`. `vk_icd.h`/`vk_layer.h` replaced from the same 350 generation (wholesale means wholesale).
- `jam_vulkan.h`: `#define VULKAN_HPP_NO_EXCEPTIONS` / `VULKAN_HPP_ASSERT jassert` / `VULKAN_HPP_ASSERT_ON_RESULT jassert` before `#include "vulkan/vulkan.hpp"` (macro guards at `vulkan_hpp_macros.hpp:74–79` confirmed overridable). MoltenVK/platform-macro block order preserved.
- Add `-fno-strict-aliasing` scoped to the jam_vulkan compile unit (per-source property on the module TU, or the consumer-target flag — Engineer picks from actual build wiring; global only if scoping impossible).
**Validation:** Auditor confirms one config site, matched-pair completeness (no mixed generations), raii/shared absent, flag present and scoped.

### Step 3: Sweep — device + resource wrappers (VMA seam)

**Scope:** `device/jam_VulkanDevice.{h,cpp}`, `allocator/jam_VulkanAllocator.{h,cpp}`, `resource/jam_VulkanBuffer.h`, `jam_VulkanImage.h`, `jam_VulkanUploadHelpers.{h,cpp}`, `jam_FrameBuffer.{h,cpp}`, `jam_VulkanPrimitiveRecordBuffer.*`, `jam_VulkanTransparencyStack.*`, `jam_VulkanWindingScratch.*`
**Action:** Full `vk::` conversion: handles, structs (positional ctors/setters — C++17, no designated init), scoped enums/Flags, `vk::ArrayProxy` where (count,ptr) pairs exist. Device chain keeps Result-overload checks → bool (never-null contract). Two-call physical-device loop → `instance.enumeratePhysicalDevices()` (`vulkan_funcs.hpp:114`). VMA call sites: `vk::` types pass in via implicit conversion; VMA out-handles land in `Vk*` temporaries wrapped to `vk::` immediately — seam confined to the vma* call line, never leaking C types into signatures.
Device conformance fixes (Locked Decision 9) land here, inside the Device-chain rewrite: portability-enumeration instance bit + extension, presence-checked `portability_subset`, debug-only validation-layer/debug-messenger wiring (messages → `debug::Log`, severity → jassert policy).
**Validation:** Auditor confirms zero raw `Vk*` types in public signatures (except the vma* call lines), Result overloads on all creation paths, no UniqueHandle/raii, doxygen comments moved with changed signatures; conformance fixes present, validation layers compiled out of release.

### Step 4: Sweep — Pipelines + PipelinesState

**Scope:** `context/jam_VulkanPipelines.{h,cpp}`, `context/jam_VulkanPipelinesState.cpp`
**Action:** Highest-boilerplate conversion (313 instances; PipelinesState ~80% deletes under `vk::` struct constructors). Pipeline/descriptor/cache creation on Result overloads → bool chain preserved. `noStencilState()` and shared state definitions stay SSOT — conversion must not duplicate state blocks currently shared.
**Validation:** Auditor confirms LOC reduction realized (no 1:1 field-assignment transliteration), shared-state SSOT intact, pipeline-cache binary-blob load/save behavior unchanged.

### Step 5: Sweep — GraphicsSetup* + Graphics + Registry seam

**Scope:** `context/jam_VulkanGraphics.{h,cpp}`, `jam_VulkanGraphicsSetup{Surface,RenderPass,DrawState,SceneTarget,Calibration,CalibrationPipeline,SampleMeasurement}.cpp`, `registry/jam_VulkanRegistry.h` (types only)
**Action:** Surface/swapchain/renderpass/framebuffer/sync-object creation → `vk::` with Result overloads; `getSwapchainImagesKHR()` enhanced enumeration; swapchain-recreation and frame-pacing logic unchanged (algorithm survives 1:1). Registry touched only where Vk types appear in members/signatures.
Present-mode negotiation (Locked Decision 10): `selectPresentMode` per the RFC Scaffold pattern replaces the hardcoded FIFO at swapchain creation — enumerate, prefer MAILBOX, fall back FIFO (spec-guaranteed, positive selection, no bail-out).
**Validation:** Auditor re-walks `Registry::createContext` → CPU fallback under every failure branch (RFC handoff 6 — release-build contract); confirms no enhanced-mode value-returning call on any setup path; confirms present-mode selection matches the RFC Scaffold's BLESSED trace.

### Step 6: Sweep — LLGC family + shader pass recording

**Scope:** `context/jam_VulkanLowLevelGraphicsContext.{h,cpp}` + `{Image,Path,Transparency,Glyph,Render}.cpp`, `jam_VulkanGraphicsShaderPass.cpp`, `resource/jam_VulkanShaderInstance.{h,cpp}`
**Action:** 133 `vkCmd*` sites → `vk::CommandBuffer` member calls (1:1, algorithm untouched); ShaderInstance public signatures (`VkPipeline`/`VkRenderPass`/`VkExtent2D`/…) → `vk::` equivalents; push-constant sites → `cmd.pushConstants` preserving exact offset/size/stage semantics (Step 1's fix survives conversion byte-identically). `vk::to_string` replaces any hand-formatted enum diagnostics in `debug::Log` lines encountered in scope.
**Validation:** Auditor confirms zero remaining `vk[A-Z]`-prefixed function calls and zero raw `Vk*` struct declarations module-wide except vma* seam lines (grep-clean = sweep complete), path/transparency/clip logic semantically untouched, uniform-stamp SSOT sites unchanged.

### Step 7: Docs closure

**Scope:** END `ARCHITECTURE.md` (graphics section), `CLAUDE.md` Current State block, `PLAN-vulkan-redesign.md` (delete), JAM doxygen comments in swept files
**Action:** ARCHITECTURE.md: jam_vulkan now speaks vulkan-hpp plain `vk::` (config conditions, error policy, vendored 350 pair) — descriptive, code is SSOT. CLAUDE.md stale "Open PLAN: B1, B3, B4, B5 remaining" corrected. Delete dead `PLAN-vulkan-redesign.md` (RFC handoff 3 — verified fully superseded). Doxygen regen is ARCHITECT's.
**Validation:** Auditor confirms docs match shipped code; no stale raw-C references in module docs.

---

## BLESSED Alignment

- **B:** ownership model untouched — vulkan-hpp is vocabulary inside existing JAM RAII wrappers; no new lifetime constructs (raii/UniqueHandle excluded by lock).
- **L:** ~1.5–2K LOC deleted (measured bound, 20–27% of real Vulkan surface); zero new runtime, zero new TUs; opt-in headers included only on first consumption (YAGNI).
- **E:** compile-checked struct construction replaces hand-typed sType/zeroing; scoped enums block cross-type bitmask errors; config macros in one visible site; Result checks explicit on every propagation path.
- **S (SSOT):** one vendored matched generation; one config site (`jam_vulkan.h`); generated from the same vk.xml as the C API; shared pipeline-state definitions preserved.
- **S (Stateless):** no new state anywhere — binding layer only.
- **E (Encapsulation):** dependency confined to jam_vulkan; END touches zero Vulkan (invariant re-verified at Step 6); VMA seam confined to call lines.
- **D:** pinned 350/350 pair identical across 4 machines; static_assert-enforced pairing; measured byte-identical codegen at -O2.

## Risks / Open Questions

- **Regression hotspot (RFC handoff 2):** every `Vk*CreateInfo` is rewritten; multi-pass post-process (BufferA–D feedback, scene composite, glass alpha) is verified single-pass only today — ARCHITECT's post-migration validation pass covers it.
- **Windows:** MSVC/clang-cl compile of vulkan.hpp + strict-aliasing flag equivalence — same Windows-unverified status as the existing set.
- **Opacity root cause unknown until Step 1 reading** — if it lands outside the traced chain (e.g. END Compiler GLSL), fix follows the defect wherever it lives; still in-sprint (locked).
- **MAILBOX pacing (RFC Open Q5):** `targetFrameBudgetMs` calibration and the synchronous paint path were measured under FIFO — post-sweep validation on MoltenVK + Windows confirms; preference order trivially swappable in `selectPresentMode` if measurement says FIFO.

## Progress

- Step 1 ✅ — opacity root cause: Image-pass pipeline hardcoded no-blend for every target; in-scene background draw now `Pipelines::alphaBlendAttachment()`, buffer passes + post-process composite `opaqueBlendAttachment()` (caller-chosen param through `createFullscreenPipeline`/`getOrCreateImagePassPipeline`). Audit PASS, findings resolved. ARCHITECT visual gate pending.
- Step 2 ✅ — 18-file matched 350 set vendored from `___sdk___/vulkan/macOS/include/vulkan/` (RFC Handoff 10 satisfied: full .hpp companion set confirmed); config macros live at `jam_vulkan.h:43–47`; `-fno-strict-aliasing` merged into jam_vulkan TU flags (`AppBuilder.cmake:546`, NOT MSVC). Audit PASS. Module still compiles as raw C until the sweep.
