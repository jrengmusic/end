# RFC — vulkan-hpp Adoption into jam_vulkan
Date: 2026-07-02
Status: Ready for COUNSELOR handoff

## Problem Statement

Three days after completing the OpenGL→Vulkan migration (1:1 parity achieved, Sprint 55), ARCHITECT found `~/Documents/Poems/dev/vulkan-hpp/`. Question: should jam_vulkan's hand-rolled raw C Vulkan API usage be replaced with vulkan-hpp C++ bindings — evaluated on maintenance burden, debugging, scalability, binary/runtime cost — and if so, what is the integration point?

**Decision reached this session: ADOPT.** Vendor the SDK 1.4.350 matched header pair into `jam_vulkan/vulkan/`, full-sweep conversion in one sprint, plain `vk::` types only, under the four load-bearing configuration conditions in Principles below.

## Research Summary

All findings cited from Pathfinder (jam_vulkan survey + boilerplate quantification) and Librarian (vulkan-hpp checkout + installed SDK inspection, with direct compile/disassembly measurements). No training priors.

### Current handroll (Pathfinder)

- `jam_vulkan`: ~18.7K LOC non-vendored across 61 files. Of that, **11.4K is `font/jam_GlyphConstraintTable.cpp`** (pure lookup data, zero Vulkan) + ~700 box-drawing encoding. **Actual Vulkan-touching code ≈ 7K LOC.**
- Raw C API exclusively (`jam_vulkan.h:39` includes `vulkan/vulkan.h`). **C headers already vendored** at `jam_vulkan/vulkan/` (`vk_platform.h`, `vulkan.h`, `vulkan_core.h`, `vk_icd.h`, `vk_layer.h`, platform headers). VMA also vendored (`jam_vulkan/vma/vk_mem_alloc.h`). Vendoring precedent established twice over.
- Ownership: Application → Registry (`std::unique_ptr`) → Device + GlyphAtlas + per-window Graphics (HashMap keyed by native window handle). RAII move-only wrappers (Buffer, Image) with destructor guards. Manual `vkDestroy*` in destructors. No exceptions anywhere.
- Error handling: chained-bool setup steps, `!= VK_SUCCESS → return false`, factories return nullptr → Registry falls back to CPU renderer (`jam::LowLevelGraphicsGlyphRenderer`). **This propagation is functional in release builds** — the never-null factory contract depends on it.
- END touches zero raw Vulkan — ARCHITECTURE.md boundary invariant.
- No validation-layer enablement found in Device::createInstance. No dynamic loader — link-time symbol resolution.

### Boilerplate quantification (Pathfinder, measured)

- **Deletable/compressible: ~1,500–2,000 LOC = 20–27% of the real 7K Vulkan surface.**
- Concentration: `context/jam_VulkanPipelinesState.cpp` — 200 of 247 LOC are `Vk*StateCreateInfo` field assignments (~80% boilerplate; file nearly vanishes under vk:: constructors). Five `GraphicsSetup*`/`Pipelines` files at 36–53% boilerplate.
- Counts: 128 `.sType =` lines, 161 `VkXxxInfo` declarations, 103 vkCreate/Destroy/Allocate/Free sites (~200 lines with checks), ~47 VkResult-check lines, ~100+ lines `VkWriteDescriptorSet` array plumbing, 1,013 `VK_*` enum tokens (compress to scoped enums — readability/type-safety, not line deletion).
- Survives unchanged: 133 `vkCmd*` recording sites (the rendering algorithm, 1:1 method calls), LLGC path/transparency/clipping logic, glyph atlas, VMA-bound Buffer/Image.
- `.pNext` chaining nearly absent today (5 instances) — `StructureChain` value is forward-looking (extension growth), not existing-debt payment.

### vulkan-hpp (Librarian, verified against source + measured)

- Header-only, generated from `vk.xml` (same SSOT as C headers — no drift risk). Ships in LunarG SDK since 1.0.24; installed at `/usr/local/include/vulkan/vulkan.hpp` (SDK 1.4.350). ~10 years maintained, 2,495 commits, **4 documented breaking changes total** (README.md:83–107). MoltenVK/macOS in CI matrix.
- **C++17 fully sufficient** for everything in scope. Designated initializers need C++20 (not needed — positional constructors + setters at C++17). `vk::raii` works under C++17 + `VULKAN_HPP_NO_EXCEPTIONS` (verified against `vulkan_raii.hpp:3411` gating — docs' C++23 claim describes only the std::expected path) — but raii is excluded from scope anyway.
- **Runtime cost: measured zero for plain `vk::` types** — `-O2` disassembly identical to raw C (2 instructions), object files byte-identical, handles binary-identical (`operator VkDevice()` / implicit ctor, `vulkan_handles.hpp:19985/11951`). **NOT zero-cost:** `vk::UniqueHandle` (stores allocator + parent, docs/Handles.md:30–35) and `vk::raii` (heap-allocated per-object dispatcher, `vulkan_raii.hpp:3393–3397`) — both excluded.
- **Compile cost: measured.** ~15MB header text/TU; 0.92–1.03s per TU vs 0.04s raw C (Apple clang 17, arm64, -std=c++17). PCH reduces >90% (5 TUs: 4.12s → 0.35s). JAM's JUCE-module unity build means few TUs. PCH requires identical `VULKAN_HPP_*` macros across sharing TUs.
- **Error handling under `VULKAN_HPP_NO_EXCEPTIONS`:** enhanced-mode returns become `vk::ResultValue<T>{result, value}` (C++17, no std::expected needed), `[[nodiscard]]`. Internal check: `detail::resultCheck` calls `VULKAN_HPP_ASSERT_ON_RESULT(result == Result::eSuccess)` (`vulkan.hpp:9104–9116`). **Default is `assert()` — stripped under NDEBUG: failed calls silently return garbage `.value` in release unless the macro is redefined.** Both `VULKAN_HPP_ASSERT` and `VULKAN_HPP_ASSERT_ON_RESULT` independently redefinable (`vulkan_hpp_macros.hpp:74–79`, docs/Usage.md:734–737) — jassert injection confirmed supported.
- **Debugging:** validation layers / RenderDoc / `VK_EXT_debug_utils` see identical native handle values (no indirection). Templated `setDebugUtilsObjectNameEXT<Handle>` (`vulkan_handles.hpp:15393`). `VulkanHpp.natvis` is **VS-only — no lldb formatters ship**; `vk::Flags<>`/scoped enums print opaquely in codelldb without custom type summaries.
- **`-fno-strict-aliasing` documented requirement** (docs/Usage.md:1039–1041) — headers reinterpret_cast between bit-identical C/C++ structs.
- **Interop:** 64-bit implicit bidirectional conversion `Vk*` ↔ `vk::*` — free, incremental-capable, reversible.
- **Dispatch:** `DispatchLoaderStatic` calls link-time `vk*` symbols directly, zero indirection — matches JAM's static linking. `vk::detail::DynamicLoader` has two macOS path-order fixes in its history (`32bd2b7a`, `43f7a18b`) — do not use.

### OOTB API surface (Librarian, cited)

Replaces hand-rolled code:
1. **Enhanced enumerations** — `enumeratePhysicalDevices()` / `getSwapchainImagesKHR()` / `enumerateDeviceExtensionProperties()` return `std::vector` directly (`vulkan_funcs.hpp:114/9344/637`); C two-call count/fill loops delete.
2. **`vk::ArrayProxy`** — single parameter accepts value/init-list/array/span/vector/C-array (docs/Usage.md:188–245); every (count, pointer) pair compresses. `ArrayProxyNoTemporaries` in struct setters (refuses temporaries — struct retains pointer).
3. **`vk::StructureChain`** — compile-time-validated pNext wiring, `unlink/relink<T>()` (docs/Usage.md:276–345). Available even with enhanced mode disabled.
4. **`vk::to_string`** — every enum/flags type (`vulkan_to_string.hpp`); pairs with `jam::debug::Log`, no hand switch tables. Opt-out `VULKAN_HPP_NO_TO_STRING`.
5. **Format traits** (`vulkan_format_traits.hpp`, opt-in include) — constexpr `blockSize/blockExtent/texelsPerBlock/componentCount/componentBits/isCompressed/planeCount...` (docs/Usage.md:754–784) — replaces hand-written bytes-per-pixel/upload-sizing logic in atlas/texture paths.
6. **`vulkan_hash.hpp`** (opt-in include) — `std::hash` for all handles (by native value) and structs (member-combining via redefinable `VULKAN_HPP_HASH_COMBINE`). Note: struct hashes are shallow (pointer members by pointer value). Not a substitute for `vk::PipelineCache` (binary blob cache, exposed 1:1 only).

Explicitly NOT provided (confirmed absent): VMA integration, swapchain recreation/resize management, frame-in-flight pacing/fence rings, shader compilation. **That list is exactly `jam::vulkan::Graphics`, `Registry`, Buffer/Image+VMA, `graphics::Compiler` — JAM's layer does not overlap with the binding's layer.**

**Trap:** `vk::su::*` (samples/utils — oneTimeSubmit, setImageLayout) is sample scaffolding — not shipped, not versioned, absent from SDK. Copy-paste reference only, never a dependency.

### Version pairing (Librarian, verified — load-bearing)

`vulkan.hpp` hard-gates: `VULKAN_HPP_STATIC_ASSERT(VK_HEADER_VERSION == <N>)` (`vulkan.hpp:39`). Measured on this machine:
- Checkout submodule pair: 355/355 (self-consistent, but unreleased dev tree)
- Installed SDK pair: 350/350 (self-consistent, released, LunarG-validated)
- Mixing = compile failure (loud, not silent)

Headers must come from **one matched generation, wholesale**. `vk_platform.h` alone is version-stable (SPDX-only diff).

### vk-bootstrap survey (Librarian, post-RFC session addendum — evaluated and rejected as dependency)

`charles-lunarg/vk-bootstrap` (LunarG-affiliated maintainer, tags version-synced to SDK — `v1.4.350` exists). Four builders (Instance, PhysicalDeviceSelector, Device, Swapchain), init-only. No exceptions (`Result<T>`, all `noexcept`). **Rejected as dependency:** ~3,300 LOC vendored to replace ~240 LOC of working code; outputs raw C types (zero vulkan-hpp awareness — second vocabulary at init exactly when the sweep unifies on `vk::`); surface creation and VMA not covered. Lean opposes; no pillar advocates. ARCHITECT-directed disposition: adopt its *patterns* as reference, not the library (MIT-licensed; patterns are standard spec idioms anyway).

Factual findings surfaced by the survey against current `jam_VulkanDevice.cpp` (cited, independent of any adoption):
1. `createInstance()` (lines 22-51) never requests `VK_KHR_portability_enumeration` nor sets `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` — MoltenVK-conformant instance path absent.
2. `createLogicalDevice()` (lines 152-158) pushes `VK_KHR_portability_subset` unconditionally under `JUCE_MAC` without querying device support.
3. No validation-layer/debug-messenger wiring exists anywhere in Device setup.
4. Present mode hardcoded FIFO; swapchain recreation does full teardown (`oldSwapchain` = `VK_NULL_HANDLE`).

## Principles and Rationale

### Decision: adopt, under four conditions (each ARCHITECT-locked this session)

1. **Plain `vk::` types only.** No `vk::raii`, no `UniqueHandle`, no `SharedHandle` — the zero-cost measurement is void otherwise, and JAM's ownership wrappers already are the RAII layer. vulkan-hpp is vocabulary inside existing JAM wrappers, not a lifetime model.
2. **`VULKAN_HPP_NO_EXCEPTIONS` + assertion macros redefined** in `jam_vulkan.h` before the include: `VULKAN_HPP_ASSERT = jassert`; `VULKAN_HPP_ASSERT_ON_RESULT` = jassert (see error policy below). Default config in release is worse than the current inline checks — unconfigured adoption is a regression.
3. **Matched header generation, wholesale** — SDK 1.4.350 pair (ARCHITECT-selected), replacing the current vendored C-only set in `jam_vulkan/vulkan/`. Never mix generations.
4. **`-fno-strict-aliasing`** verified/added in JAM build flags.

### Error-handling policy (ARCHITECT-locked)

**Result-returning overloads on setup/creation paths** (Device chain, Graphics::create, swapchain, pipeline creation): explicit `vk::Result` checks preserved → bool/nullptr propagation to the never-null factory → CPU fallback intact **in release builds**, identical to today's contract. Enhanced value-returning calls only where failure is programmer error. `ASSERT_ON_RESULT = jassert` covers the enhanced-mode paths in debug.

### Migration shape (ARCHITECT-locked)

**Full sweep, one sprint.** Refactor-Rewrite Discipline: delete-first, whole module speaks `vk::` at once, compiler errors drive completion. No dual-vocabulary period. (Interop being free made incremental viable; ARCHITECT chose the sweep.)

### Integration point (BLESSED-selected)

Vendor into `jam_vulkan/vulkan/` — extends the existing vendored-Vulkan-headers pattern (and VMA precedent). Rejected alternatives:
- **JUCE user module** — wrong shape: header-only, zero .cpp, module system adds nothing, complicates PCH.
- **Framework peer / third-party framework level** — overweight: it is the binding vocabulary of exactly one JAM module; END and other modules never see it (Encapsulation invariant: END touches zero Vulkan).
- **Consume from installed SDK** — version floats per-machine across ARCHITECT's 4 synced dev machines; breaks Deterministic. Vendored = pinned, identical bytes everywhere; SDK remains link-time only.

### Why adopt at all — the evidence-settled case

- **Less code is less liability, precisely bounded:** ~2K lines deleted at 20–27% of the real Vulkan surface, concentrated in the highest-defect-density class (hand-zeroed structs, hand-typed sType, two-call loops) where the compiler currently checks nothing and a typo is a silent driver-level failure. Liability transfers to Khronos-generated, spec-locked code.
- **Zero runtime/binary cost** under condition 1 — measured, not claimed.
- **Architecture untouched:** what vulkan-hpp lacks is exactly what JAM provides. Three-domain split: vulkan-hpp = GPU vocabulary, JUCE = platform/paint dispatch, JAM = orchestration.
- **What remains subjective is sequencing only** — ARCHITECT resolved it: now, one sprint.
- Status quo's only advantage: it already exists.

### BLESSED pillar mapping

- **Explicit:** compile-checked struct construction and pNext chains over hand-wired; type-safe scoped enums/Flags block cross-type bitmask errors.
- **Lean:** ~2K fewer hand-owned lines; no new runtime, no new .cpp, no new lifetime model.
- **SSOT:** one vendored matched header set; one config-macro site (`jam_vulkan.h`); generated from the same vk.xml as the C API.
- **Deterministic:** pinned 350/350 pair, identical across 4 machines; static_assert enforces pairing at compile time.
- **Encapsulation:** dependency lives inside the one module that speaks Vulkan; END boundary invariant untouched.

## Scaffold

No code scaffold produced — this was a research/audit session; the deliverable is the locked decision set. Concrete anchors for COUNSELOR/Engineer:

**Config site — `jam_vulkan.h`, before any Vulkan include:**
```cpp
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_ASSERT            jassert
#define VULKAN_HPP_ASSERT_ON_RESULT  jassert   // enhanced-mode debug net; setup paths use Result overloads (policy above)
#include "vulkan/vulkan.hpp"
// opt-in as adopted:
// #include "vulkan/vulkan_format_traits.hpp"   // atlas upload sizing
// #include "vulkan/vulkan_hash.hpp"            // cache keys
```
(Exact macro ordering/placement per `vulkan_hpp_macros.hpp` guards — both `#if !defined(...)`, independently overridable.)

**Vendoring:** copy the complete matched `.h`+`.hpp` set from `/usr/local/include/vulkan/` (SDK 1.4.350) into `jam_vulkan/vulkan/`, replacing current contents wholesale. Minimal set: `vk_platform.h`, `vulkan_core.h`, `vulkan.h`, platform headers (metal/win32), `vulkan_hpp_macros.hpp`, `vulkan.hpp`, `vulkan_enums.hpp`, `vulkan_handles.hpp`, `vulkan_structs.hpp`, `vulkan_funcs.hpp`, `vulkan_to_string.hpp`; opt-in: `vulkan_format_traits.hpp`, `vulkan_hash.hpp`. Exclude: `vulkan_raii.hpp`, `vulkan_shared.hpp`.

**Dispatch:** static (`DispatchLoaderStatic` default with static linking) — no loader init, zero indirection. Do not use `vk::detail::DynamicLoader`.

**Prime conversion targets (highest boilerplate first, per Pathfinder):** `context/jam_VulkanPipelinesState.cpp` (~80%), `context/jam_VulkanPipelines.h` (~53%), `context/jam_VulkanGraphicsSetupDrawState.cpp` (~47%), `context/jam_VulkanGraphicsSetupCalibrationPipeline.cpp` (~42%), `context/jam_VulkanGraphicsSetupSceneTarget.cpp` (~36%).

**Present-mode negotiation (ARCHITECT-directed addition — pattern adopted from vk-bootstrap, rewritten BLESSED-compliant):**
```cpp
// Setup path — Result-checked per locked error policy. FIFO is spec-guaranteed
// (VK_KHR_surface: "VK_PRESENT_MODE_FIFO_KHR ... required to be supported"),
// so the fallback is total: positive selection, no bail-out guard.
vk::PresentModeKHR selectPresentMode (vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
{
    const auto modes { physicalDevice.getSurfacePresentModesKHR (surface) };
    jassert (modes.result == vk::Result::eSuccess);

    const bool hasMailbox { std::find (modes.value.begin(), modes.value.end(),
                                       vk::PresentModeKHR::eMailbox) != modes.value.end() };

    return hasMailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}
```
BLESSED trace: Explicit (enumerate-then-select, no hidden default), Deterministic (spec-guaranteed fallback, same input → same mode), Bounds (selection domain is the enumerated set), no bail-out guard (fallback is total by spec citation). Function name subject to Names Gate — ARCHITECT approval before introduction. Consumed by `Graphics` swapchain creation in place of hardcoded `VK_PRESENT_MODE_FIFO_KHR`.

## BLESSED Compliance Checklist

- [x] Bounds — scope bounded to jam_vulkan internals; END untouched; explicit exclusion list (raii/UniqueHandle/su)
- [x] Lean — net LOC reduction; zero new runtime constructs
- [x] Explicit — compile-checked construction; Result overloads on propagation paths; config in one visible site
- [x] SSOT — one vendored generation; one config site; vk.xml upstream
- [x] Stateless — no new state; binding layer only
- [x] Encapsulation — vulkan-hpp opaque outside jam_vulkan; END boundary invariant preserved
- [x] Deterministic — pinned matched pair; static_assert-enforced pairing; identical across machines

## Open Questions

None blocking — all load-bearing decisions resolved this session (header pair, migration shape, integration point, error policy, opacity-fix scope). Implementation-level residuals for COUNSELOR/Engineer discretion:
1. PCH setup for `vulkan.hpp` in JAM's build — measured >90% compile-time win; JUCE unity build may make it unnecessary. Engineer decides from actual build timings.
2. lldb type summaries for `vk::Flags<>`/scoped enums in `.lldbinit` — quality-of-life, not blocking (natvis is VS-only).
3. `-fno-strict-aliasing` presence in current JAM flags — verify, add if absent.

4. ~~vk-bootstrap survey findings 1-3~~ RESOLVED (ARCHITECT-directed): **all three Device fixes are in sweep scope** — VK_KHR_portability_enumeration + ENUMERATE_PORTABILITY_BIT at instance creation, portability_subset presence-checked before enable, validation-layer/debug-messenger wiring (debug builds, routed through jassert/debug::Log). ~20 lines each; Device setup is rewritten by the sweep regardless.
5. MAILBOX runtime behavior on END's paint model: MAILBOX unblocks presentation vs FIFO's vblank pacing — `targetFrameBudgetMs` calibration and the synchronous message-thread paint path were measured under FIFO. Post-sweep validation should confirm pacing characteristics under MAILBOX on both MoltenVK and Windows; preference order is trivially swappable in `selectPresentMode` if measurement says FIFO.

## Handoff Notes

1. **Sprint scope is locked: full vk:: sweep + opacity defect fix, one sprint** (ARCHITECT-confirmed in-sprint). Defect: background/post-process shader opacity has no runtime effect, both render passes. Wired through config → `graphics::Compiler` → `jam::vulkan::render(g, shader, opacity, resolution)` seam but produces zero visual difference — value lost between injection seam and draw (suspect: uniform upload or blend state in pipeline creation — exactly the code the sweep rewrites). Debt ODE already written for `carol debt add`; if captured, drains at log time.
2. **Post-migration validation requirement (ARCHITECT-stated):** multi-pass post-processing (BufferA–D feedback, scene composite, glass-window alpha) needs rigorous testing — only single-pass is verified today. Migration touches every `Vk*CreateInfo`; this is the regression hotspot.
3. **PLAN-vulkan-redesign.md is fully superseded — verified against code this session:** B1 implemented (`Main.cpp:97–114`, refresh-rate → `targetFrameBudgetMs` → Registry; monitor-change re-query deliberately dropped, "detected once, never polled"); B2 executed (Processor/Compositor/Program + GL shader files deleted Sprint 55); B3/B4 marked SUPERSEDED-executed in the plan itself (PLAN-shader-pipeline.md; PostProcessRegistry dropped YAGNI); B5 done (ARCHITECTURE.md synced `aa717ef`, DEBT.md at sole entry). Plan file is dead — deletion at ARCHITECT's next commit.
4. **CLAUDE.md "Current State" block is stale** ("Open PLAN: B1, B3, B4, B5 remaining") — correct when next touched.
5. **Live debt:** `DEBT-20260629T100000` (drawLine native-line-pipeline gap) — sole ledger entry, unrelated to this scope unless the sweep touches the line pipeline.
6. **Never-null factory contract is the constraint the error policy protects:** `Registry::createContext` must keep returning CPU fallback on GPU-path failure in release builds. Any conversion of a setup-path call to an enhanced-mode (value-returning) overload silently breaks this under NDEBUG — Auditor should specifically check setup paths use Result overloads.
7. **Doxygen protocol applies to the sweep:** JAM index `~/Documents/Poems/dev/jam/docs/xml/index.xml` before grep; regen after API-shape changes; doxygen comments move with signatures.
8. Discrepancy note resolved during research: earlier Pathfinder claim of SDK-include was wrong — JAM vendors its C headers (`jam_vulkan/vulkan/`, dated 28 Jun). Dispatch config is therefore zero work (static linking + vendored headers already the pattern).
9. **Present-mode negotiation is ARCHITECT-directed sweep scope** (this RFC's Scaffold): replace hardcoded FIFO with enumerate-prefer-MAILBOX-fallback-FIFO per the BLESSED-compliant pattern above. Pattern provenance vk-bootstrap (reference only — the library itself is rejected as dependency, see Research Summary). Open Question 5's runtime validation rides the multi-pass testing requirement (note 2).
10. **Vendoring source clarification (post-RFC session):** COUNSELOR sourced the 1.4.350 pair from `~/Documents/Poems/dev/jam/___sdk___/vulkan/macOS/include/vulkan/` — verified self-consistent 350/350 (vulkan_core.h defines 350, vulkan.hpp asserts 350). Same generation the RFC pinned; compliant. Confirm the complete `.hpp` companion set is copied from that single directory (macros/enums/handles/structs/funcs/to_string + opt-in format_traits/hash) — the static_assert catches generation mismatch but not a partial `.hpp` set.
