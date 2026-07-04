# PLAN: External Textures/LUT · Universal OBJ Mesh Loader · Shadertoy iMouse

**RFC:** RFC-obj-mesh-textures-imouse.md
**Date:** 2026-07-04
**BLESSED Compliance:** verified
**Language Constraints:** C++17/JUCE — LANGUAGE.md C++/JUCE override applies (single-header preferred ~300 LOC; split only for real circular-include/heavy-recompile/decomposition reasons, never to shrink a line count)

## Overview

Three RFC-locked, decoupled deliverables, landing smallest-first: (A) Shadertoy `iMouse` plumbing, (B) `BindlessTexture` extraction + external LUT/image texture support (closes the `sirenian-dawn` blank-render class of gap — `iChannel0` bound to nothing), (C) universal `jam::WavefrontObj` mesh loader + native Vulkan 3D render path, phased per RFC's own effort signal (C is larger than A+B combined).

## Language / Framework Constraints

- `jam::WavefrontObj` lands in `jam_graphics` (deps `juce_core`/`juce_graphics` only) — **never** `juce_opengl`, per RFC §B and the existing `jam_graphics.h:104-111` module-arrow doxygen ("jam_graphics cannot depend on jam_vulkan"). `glm` (already vendored in `jam_vulkan.h:55-56`) is the only 3D-math dependency for the render path — no `juce::Vector3D`/`Matrix3D`.
- `juce::Array` (not `std::vector`) for all contiguous numeric mesh buffers — no `.at()`, use `getReference`/`getUnchecked`. `jam::Owner<Shape>` for shape ownership only.
- `jam::Function::Map` replaces every `if(matchToken) continue;`-style keyword chain (OBJ line dispatch). No hand-rolled dispatch tables.
- No bail-out guards anywhere in new code — positive nesting / result returns only (MANIFESTO §E).

## Validation Gate

Each step validated by `@Auditor` against `MANIFESTO.md` (BLESSED), `NAMES.md`, `~/.carol/JRENG-CODING-STANDARD.md`, and this plan's locked decisions before the next step proceeds. Steps B1/B2 additionally require a **byte-equivalence check**: GlyphAtlas GPU-mirror output must be bit-identical pre/post extraction (same barrier sequence, same slot reuse) — this is a refactor of shipped, audited behavior, not new behavior.

---

## Phase A — Shadertoy `iMouse` (item 3, smallest, ships first)

### Step A1: Extend the uniform stamp to carry mouse state
**Scope:** `jam_vulkan/resource/jam_VulkanShaderInstance.h/.cpp` (`stampUniforms`, currently `h:350`, zeroed `iMouse` per doc comment `h:133-134`), `jam_vulkan/shader/jam_VulkanShaderUniforms.h` (`iMouse[4]` already at offset 0, `h:135` — no struct change).
**Action:** `stampUniforms(vk::Extent2D targetExtent, const std::array<float, 4>& iMouse) noexcept` — thread the parameter through the existing call chain unchanged in shape: `jam::vulkan::render()` → `renderShader()` → `recordShaderBufferPasses()` → `stampUniforms()`. Post-process call site passes `{0,0,0,0}` (documented limitation — app-global, no owning component).
**Validation:** Auditor confirms no new state introduced on `ShaderInstance` (Stateless) — `iMouse` is a per-call parameter, not tracked; call-chain signature change is the only diff at each hop.

### Step A2: `ShaderComponent` mouse interception + sign-encoded state
**Scope:** `jam_vulkan/shader/jam_VulkanShaderComponent.h` (constructor currently `setInterceptsMouseClicks(false, false)` at `h:34`, doc comment `h:20-22` "Never intercepts mouse input" — both updated to reflect new contract).
**Action:** `setInterceptsMouseClicks(true, false)`; override `mouseDown`/`mouseDrag`/`mouseUp` to maintain the Shadertoy-encoded `float iMouse[4]` per RFC §C (`.xy` current pos while down, `abs(.zw)` click-start, `sign(.z)>0` while down, `.w>0` only the click frame). Y-flip at emit: `iMouse.y = getHeight() - e.position.y`, applied to `.xy` and `abs(.zw)`. Hand the transient array to `render(...)` at paint (existing `h:88-95` call site, extended per A1).
**Validation:** Auditor confirms `iMouse[4]` is transient component state (calculation input, not machinery state — Stateless exception per MANIFESTO's DSP-parameter carve-out), sign bits driven by event type only, no polling/getter added.

---

## Phase B — `BindlessTexture` extraction + external textures/LUT (item 1)

### Step B1: Extract `jam::vulkan::BindlessTexture` — inverted slot ownership (ARCHITECT-decided)
**Scope:** NEW `jam_vulkan/resource/jam_VulkanBindlessTexture.h/.cpp`. Source: `jam_vulkan/font/jam_GlyphAtlasGpuMirror.cpp:47-97` (`uploadGlyphAtlasImage` — memcpy→staging, `recordUploadBarrier(UNDEFINED→TRANSFER_DST)`, `copyBufferToImage`, `recordUploadBarrier(TRANSFER_DST→SHADER_READ_ONLY)`), `Graphics::setBindlessIndex`/`resetBindlessIndex` (`jam_VulkanGraphics.h:1245,1259`), `writeBindlessTextureDescriptor` (`jam_VulkanGraphics.cpp:1019-1041`).

**Discovered discrepancy (resolved by ARCHITECT before this step began):** the original RFC scaffold gave `BindlessTexture` ONE owned bindless slot (int member). That fits a genuinely per-window resource (external LUT/OBJ textures, Step B3+) but does NOT fit retrofitting it onto `GlyphAtlas`, whose `gpuImages` is **one Registry-wide-shared `Image` per `Type`, with N independently-tracked per-window slots** (today: `Graphics::atlasBindlessIndex`, a `HashMap<Type,int>` per window — `jam_VulkanGraphics.h:1803`, doc comment `jam_GlyphAtlas.h:391-397`). A single owned slot would collapse that N-window shape and regress multi-window correctness. `jam::SharedResource<T>` was investigated as a possible fit and ruled out — confirmed (`jam_core/.../jam_SharedResource.h:110-200`) to be strictly a value-interning dedup table (VALUE→stable index, e.g. `Stamp`/`Grapheme`), with no facility for "one owned resource + per-consumer keyed auxiliary data."

**ARCHITECT-decided resolution — invert the ownership:** `BindlessTexture` owns ONE `jam::vulkan::Image` (the shared GPU resource) PLUS an internal `jam::HashMap<Graphics*, int> perWindowSlot` (per-consumer-window slot, keyed by whichever `Graphics&` binds it) — the inverse of today's split (today: consumer owns a map keyed by resource-type; new: resource owns a map keyed by consumer-identity).

**Action:**
- `create(int width, int height, vk::Format)` — builds the device-local image only; no slot claimed yet (slots are claimed lazily, per consumer window, on first bind).
- `int getSlot(Graphics& graphics)` — returns the existing slot for `graphics` from `perWindowSlot` if present; otherwise claims one via `graphics.setBindlessIndex(capacity)`, stores it, and returns it. First-bind-per-window semantics.
- `void bind(Graphics& graphics)` — calls `getSlot(graphics)`, then `graphics.writeBindlessTextureDescriptor(slot, image.getView())` — the actual descriptor write, callable idempotently per window.
- `void releaseWindow(Graphics& graphics)` — calls `graphics.resetBindlessIndex(slot)` and erases the `graphics` entry from `perWindowSlot`. This is the cleanup call Step B1b (below) wires into window-close.
- `upload(vk::CommandBuffer, const juce::Image::BitmapData&)` — re-runs the exact extracted barrier/copy sequence (no longer takes a `Graphics&` — upload writes pixels into the ONE shared image, independent of which window's slot is being bound). CPU pixel source is caller-supplied, so the glyph rasterizer and (Step B3) `juce::ImageFileFormat` decoder are two producers of the one upload path.
- Move-only, RAII (`JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`). Doc comment must document the `Graphics*`-as-map-key lifetime contract explicitly: safe only because Step B1b guarantees `releaseWindow()` is called before a `Graphics` is destroyed (no stale-key/address-reuse hazard).
**Validation:** Auditor byte-equivalence check (upload barrier sequence identical to pre-extraction `GlyphAtlasGpuMirror`) — MANDATORY per Validation Gate above. Auditor additionally confirms `perWindowSlot` can never hold a key for a destroyed `Graphics` once B1b lands (no dangling-key read path).

### Step B1b: Wire `Registry::removePeer()` into the real window-close path (discovered pre-existing gap, fixed as part of this plan)
**Scope:** `jam_vulkan/registry/jam_VulkanRegistry.h:68` (`removePeer(juce::ComponentPeer*)` — declared, confirmed via whole-codebase grep to be **never called**, i.e. dead code — `Registry::contexts`, a `HashMap<void*, std::unique_ptr<Graphics>>` at `jam_VulkanRegistry.h:279`, currently never erases entries; windows/`Graphics` instances leak until process exit today).
**Action:** Find the actual window-close/peer-destruction signal (JUCE `ComponentPeer` lifecycle — likely a `Desktop`/`ComponentPeer` removal hook, or END's own `end::View`/`Window` teardown path) and call `Registry::removePeer(peer)` from it. Inside `removePeer`, before erasing the `Graphics` entry from `contexts`, call `releaseWindow(graphics)` on both of `GlyphAtlas`'s `BindlessTexture` entries (mono + emoji) — the only Registry-wide-shared `BindlessTexture` instances that outlive individual windows and therefore the only ones requiring explicit per-window cleanup (external-LUT `BindlessTexture` instances from Step B3+ are owned per-window by their `ShaderInstance` and are destroyed along with their owning window already — no explicit release needed there).
**Validation:** Auditor confirms `removePeer` is now reachable from a real call site (no longer dead code), confirms `releaseWindow` is called for both atlas types before the `Graphics` entry is erased from `contexts` (ordering: release slot bookkeeping before the `Graphics` object it references is destroyed), and confirms no other Registry-owned resource was silently left with the same latent gap.

### Step B2: `GlyphAtlas` consumes `BindlessTexture`
**Scope:** `jam_vulkan/font/jam_GlyphAtlas.h:82-243` (`jam::HashMap<Type, jam::vulkan::Image> gpuImages` member).
**Action:** Replace the raw `jam::vulkan::Image` map value with `jam::vulkan::BindlessTexture` (Step B1's inverted-ownership type) — delete the now-redundant upload body in `GlyphAtlasGpuMirror.cpp`, call `BindlessTexture::upload(...)` instead. Every current call site of `Graphics::getAtlasBindlessIndex`/`setAtlasBindlessIndex` (`jam_VulkanGraphics.h:527-541`, and consumers at `jam_VulkanGraphics.cpp:1056-1059`, `jam_VulkanLowLevelGraphicsContextGlyph.cpp:177`, `jam_VulkanPrimitiveRecord.h:65`) becomes a call to `BindlessTexture::getSlot(graphics)`/`bind(graphics)` instead — `Graphics::atlasBindlessIndex` (`jam_VulkanGraphics.h:1803`) and its getter/setter are DELETED (delete-first per Refactor-Rewrite Discipline — no coexistence of the old per-window HashMap and the new inverted ownership).
**Validation:** Auditor byte-equivalence check (same requirement as B1) — glyph atlas hot-reload path must re-copy into the same image/slot, never reallocate; confirms `Graphics::atlasBindlessIndex` and its accessors are fully removed (no dead duplicate bookkeeping left behind — SSOT).

### Step B3: External image-file loader → `BindlessTexture`
**Scope:** Producer function alongside `BindlessTexture` (same file or an adjacent `jam_vulkan/resource/` sibling if it grows past single-responsibility — decide at implementation per LANGUAGE.md's real-reason-only split rule).
**Action:** `juce::ImageFileFormat::loadFrom(File)` → `juce::Image::BitmapData` → `BindlessTexture::upload(...)`. Static LUT texture: DEVICE_LOCAL + `DEDICATED_MEMORY_BIT` (per RFC §A VMA guidance), uploaded once, no re-upload cadence (unlike the atlas's frequent path).
**Validation:** Auditor confirms no new image-decode path duplicated elsewhere (SSOT — this is the only `juce::ImageFileFormat` consumer in `jam_vulkan`).

### Step B4: `config::Shader` parses `textures=` (slangp) + per-project lua manifest (Shadertoy) — ARCHITECT-decided
**Scope:** `Source/config/Config.cpp:16-30` (format detection + `ShaderFormat::load` call), `jam_vulkan/shader/jam_VulkanShaderPreset.h/.cpp` (currently discards the key — `jam_VulkanShaderPreset.cpp:146` marks `textures` as `consumedAsFixedKey` with no storage field).
**Action:**
- **slangp:** add a storage field to `ShaderPreset` (`jam::HashMap<juce::String, juce::String> textures` — name→relative-path, mirroring the existing `parameterOverrides` field's shape) and populate it during `ShaderPreset::parse()` instead of discarding.
- **Shadertoy:** ARCHITECT-decided manifest convention — a per-project lua file **named after its own containing directory** (e.g. project `sirenian-dawn` at `~/.config/end/shaders/sirenian-dawn/` looks for `sirenian-dawn/sirenian-dawn.lua`), sol2-parsed identically to `display.lua`/`keys.lua`. `config::Shader` derives the expected filename from `dir.getFileName() + ".lua"`, checks existence (absence = today's directory-listing-only behavior, unchanged), parses via sol2 when present. ARCHITECT-scoped to carry **both** iChannelN→external-texture-asset bindings **and** top-level parameter overrides — unifying Shadertoy's manifest capability with slangp's existing top-level `id = value` parameter-override convention in one pass, not two.
- Both formats feed the same `ShaderPreset::textures` storage shape and the same parameter-override storage (`parameterOverrides`) — one consumer-facing shape, two producers (slangp key-value lines / lua table), matching the `BindlessTexture` two-producers precedent from Step B1.
**Validation:** Auditor confirms one storage shape, no duplicate parsing path between slangp `textures=`/`parameterOverrides` and the Shadertoy lua manifest's equivalent tables (SSOT); confirms the lua manifest parse reuses the existing sol2 integration (no second lua VM stood up — same pattern as `config::Display`/`config::Keys` parsers, not a new one).

### Step B5: Named-texture resolver in `ShaderCompiler`
**Scope:** `jam_vulkan/shader/jam_VulkanShaderCompiler.cpp:152-170` (`channelMacros` — currently pass-ordinal-only).
**Action:** Per RFC §7 — a `jam::Bimap`-derived **local/transient** resolver, freshly constructed once per `ShaderCompiler::compile()` call (verified conformant against `jam::Instance<T>`'s actual sequential-construction contract, `jam_Instance.h:58-66`). Base mapping: every buffer-pass name → its ordinal channel slot (unchanged behavior). Layer `textures=` names into the **same** map via a declarative per-format alias-rule table — not a second code path. `get(name)` → ordinal for macro generation; `get(ordinal)` → name for diagnostics.
**Validation:** Auditor confirms the resolver is genuinely transient (constructed/destroyed per compile call, no static/singleton state — Stateless), and that base ordinal-mapping behavior for existing (non-textured) presets is byte-identical to today's `channelMacros` output.

### Step B6: Wire named textures into `ShaderInstance` descriptor writes
**Scope:** `jam_vulkan/context/jam_VulkanGraphicsShaderPass.cpp` (per-pass descriptor/push-constant sites established in the prior sprint), `jam_vulkan/resource/jam_VulkanShaderInstance.cpp` (channel-slot population).
**Action:** For each `ShaderPreset::textures` entry (populated by either producer from Step B4 — slangp `textures=` or the Shadertoy per-project lua manifest), load via Step B3's path, call `BindlessTexture::bind(graphics)` (Step B1's inverted-ownership API — claims/reuses this window's slot and writes the descriptor in one call) at the resolved channel ordinal from Step B5 — one-time load (no per-frame re-upload; static LUT). `sirenian-dawn`-class shaders (Shadertoy `iChannel0` bound to an external noise/asset texture) become renderable once this lands, driven by that project's own `sirenian-dawn.lua` declaring the `iChannel0` binding.
**Validation:** Auditor confirms bindless array capacity accounting (existing ÷3 budget from the combined-sampler sprint) still holds with static LUT slots added; no orphan-sweep interaction since static LUTs have no `ShaderInstance` per-frame lifecycle tie.

---

## Phase C — Universal `jam::WavefrontObj` (item 2, phased — largest scope)

### Step C1: Relocate `jam::Earcut` — delete first, implement after
**Scope:** `jam_vulkan/earcut/jam_Earcut.h/.cpp` (confirmed zero `vk::` dependency, pure geometry, `namespace jam`) → `jam_graphics/earcut/jam_Earcut.h/.cpp`.
**Action:** Move files, update `jam_graphics.h`/`jam_vulkan.h` include wiring and CMake source globs. `jam_vulkan` continues consuming it via its existing `jam_graphics` dependency — zero call-site changes beyond the include path.
**Validation:** Auditor confirms `jam_vulkan` still builds after the move (Refactor-Rewrite: delete-then-implement, compiler is ground truth for completeness) and no second copy left behind.

### Step C2: `jam::WavefrontObj` parser scaffold
**Scope:** NEW `jam_graphics/mesh/jam_WavefrontObj.h/.cpp` per RFC scaffold (§ "1. jam_graphics/mesh/jam_WavefrontObj.h") — `Vertex`, `TextureCoord`, `Mesh` (SoA `juce::Array`), `Material` (full standard MTL incl. PBR fields), `Shape`, `jam::Owner<Shape> shapes`, `TripleIndex` dedup key, `jam::Function::Map<juce::String, void> lineHandlers`.
**Action:** Declare the class exactly per the RFC scaffold signatures (`load(objText, sourceFile, warnings)`, `load(objFile, warnings)`, `getShapes()`). `registerHandlers()` populates `lineHandlers` for `v`/`vn`/`vt`/`f`/`usemtl`/`mtllib`/`g`/`o`/`s` — replaces the JUCE example parser's `if(matchToken) continue;` chain.
**Validation:** Auditor confirms zero `vk::`/`juce_opengl` includes anywhere in this file, `jam::Function::Map` dispatch used (not a hand-rolled if-chain), `jam::Owner` used only for `Shape` (not for numeric buffers).

### Step C3: OBJ parse behavior
**Scope:** `jam_graphics/mesh/jam_WavefrontObj.cpp` — line handler bodies.
**Action:** Relative/negative index resolution (clean-room, not tinyobj-derived — mine behavior only per RFC's rejection of vendoring tinyobjloader), `jam::HashMap<TripleIndex, uint32_t>` dedup (replacing JUCE example's `std::map`), degenerate-face skip, `jam::Earcut::triangulate` for n-gon faces, smoothing-group-aware `generateNormals()` when `vn` is absent. Diagnostics accumulate into `juce::StringArray& warnings` (warn-and-continue, never fatal except unreadable `mtllib`).
**Validation:** Auditor confirms positive-nesting/result-return control flow throughout (no bail-out guards), `jam::Hash<TripleIndex>` specialization added correctly (wyhash the 3 ints, per RFC comment at scaffold line 246-248).

### Step C4: MTL parser
**Scope:** Same file — `mtllib`/`usemtl` handler bodies.
**Action:** Full standard MTL parse (Phong fields + exocortex PBR extension fields `Pr`/`Pm`/`Ps`/`norm`/texture maps) into the typed `Material` struct already scaffolded in C2. Renderer (Step C6) consumes only diffuse+normal subset now — accepted YAGNI-bounded cost per RFC §"Locked design," parse is free, render bill lands only when a consumer is written.
**Validation:** Auditor confirms no renderer code in this step reads unused PBR fields yet (scope discipline — parse now, consume later per RFC).

**— Checkpoint: C1-C4 is the pure-parser wave, zero Vulkan/render risk. Auditor pass required before C5 begins.**

### Step C5: GPU mesh upload — `jam::vulkan::Vertex` + `jam::vulkan::Mesh`
**Scope:** NEW `jam_vulkan/resource/jam_VulkanMesh.h/.cpp` (ratified names, RFC "Open Questions — RATIFIED").
**Action:** `jam::vulkan::Vertex` interleaved GPU POD (`float position[3]; float normal[3]; float uv[2];`), distinct from CPU `jam::WavefrontObj::Vertex`. `jam::vulkan::Mesh` interleaves the SoA `WavefrontObj::Mesh` into a `juce::Array<Vertex>`, uploads once via `jam::vulkan::Buffer` (device-local, `eVertexBuffer`, staged — mirrors the `PrimitiveRecordBuffer` staging precedent but device-local since this is static, not per-frame-mutable) + an index `Buffer` (`eIndexBuffer`). Per-window ownership. No per-frame re-upload.
**Validation:** Auditor confirms one upload, no re-copy per frame; `juce::Array` (not `std::vector`) for the interleave step, `getReference`/`getUnchecked` only.

### Step C6: Mesh-backed shader pass
**Scope:** Extends the existing `ShaderInstance`/buffer-pass machinery (`jam_vulkan/resource/jam_VulkanShaderInstance.{h,cpp}`, `jam_vulkan/context/jam_VulkanGraphicsShaderPass.cpp`); NEW `mesh_default.vert`/`mesh_default.frag` (default turnkey lit shader, ratified name).
**Action:** A pass variant pulling `{pos,normal,uv}` from the mesh SSBO via `gl_VertexIndex` instead of synthesizing a fullscreen triangle. Default = Lambert/headlight sampling MTL diffuse+normal (Preview-style turnkey). User can override vertex+fragment via the existing slang `#pragma stage` model — same shader-pass substitution mechanism already proven for background/post-process, extended to mesh passes.
**Validation:** Auditor confirms no duplication of the existing fullscreen-triangle pass path (mesh pass is a genuinely distinct pipeline variant, not a branch bolted onto the existing one — Lean 3-branch discipline).

### Step C7: `jam::vulkan::OrbitCamera`
**Scope:** NEW `jam_vulkan/resource/jam_VulkanOrbitCamera.h/.cpp` (ratified name); extends the MVP UBO population site (currently `glm::ortho`-only, `jam_VulkanLowLevelGraphicsContext.h:37`, `jam_VulkanGraphics.cpp:609`).
**Action:** `glm::perspective` + `glm::lookAt` + normal matrix (inverse-transpose) for mesh-backed draws specifically — the existing 2D orthographic MVP path for text/UI rendering is untouched (this is an additive draw-path variant, not a replacement). `GLM_FORCE_DEPTH_ZERO_TO_ONE` + `GLM_FORCE_LEFT_HANDED` + manual Y-flip (`result[1][1]` negate — glm does not flip Vulkan clip-space Y). AABB-derived auto-fit default; glm arcball orbit (not `juce::Draggable3DOrientation` — avoids pulling `juce_opengl`).
**Validation:** Auditor confirms the existing `glm::ortho` 2D text/UI path is byte-unaffected (regression check — this is the highest-risk step for accidentally touching shared MVP code).

### Step C8: Offscreen depth-composited render target
**Scope:** NEW offscreen render pass (mirrors the existing `Graphics`-owned offscreen-render-pass-factory pattern from the format-keyed slangp offscreen passes, prior sprint) with its own depth attachment + depth-test/write pipeline variant; composited back as a `BindlessTexture` (Step B1's type).
**Action:** Per RFC's rejected-alternatives rationale — do NOT add depth to the 3 existing stencil-only (`eS8Uint`) render passes (`jam_VulkanGraphicsSetupRenderPass.cpp` — confirmed all 3 passes stencil-only, no depth anywhere). Isolate the change entirely to a new offscreen target.
**Validation:** Auditor confirms zero modification to the 3 existing render-pass attachment lists (regression-critical — this is the RFC's explicitly-rejected-alternative boundary).

**— Checkpoint: C5-C8 is the render-integration wave. Auditor pass required before C9.**

### Step C9: CMake wiring
**Scope:** `end/CMakeLists.txt` / `AppBuilder.cmake` module source globs for `jam_graphics` (add `mesh/`, relocate `earcut/` glob per C1) and `jam_vulkan` (add `jam_VulkanMesh.*`, `jam_VulkanOrbitCamera.*`, `mesh_default.vert/frag` swept into `BinaryData` via the existing recursive glob — no new wiring needed for shader resource inclusion, confirmed same pattern as the shadertoy/slang wrapper shaders).
**Validation:** Auditor confirms no machine-environment-dependent path introduced (no `find_path`/`find_library` against env vars — vendored/glob-based only, matching the shaderc-vendoring precedent).

### Step C10: Documentation sync
**Scope:** `ARCHITECTURE.md` (shader-pipeline section — new mesh-backed pass, `BindlessTexture`, offscreen depth target), `SPEC.md` (Phase 14 "Resource loader (WIP)" bullet — OBJ loader / image sampler binding / per-pass resource lifecycle move from WIP to complete per what actually lands).
**Action:** Codebase-is-SSOT sync, per audit protocol — update stale references, add missing sections for the three new subsystems.
**Validation:** Auditor confirms ARCHITECTURE.md/SPEC.md reflect only what was actually built in Phases A/B/C, no aspirational content (D — Deterministic: docs must match code, not intent).

---

## BLESSED Alignment

- **Bound** — `BindlessTexture`/`Image`/`Buffer`/`Mesh` RAII, one owner each; per-window bindless slots via `Graphics`; mesh SSBO uploaded once.
- **Lean** — `jam::Function::Map` replaces the OBJ keyword if-chain; mesh-backed pass is a distinct variant, not a branch; MTL parses full but renders "enough" (YAGNI-bounded, no PBR/IBL subsystem this plan).
- **Explicit** — positive nesting/result-returns throughout; `warnings` out-param; sign-encoded `iMouse` documented; `stampUniforms` parameter explicit, not implicit context.
- **SSOT** — one `BindlessTexture` upload path for atlas + LUT + OBJ textures; one `jam::Earcut` triangulator (relocated, not duplicated); one named-texture resolver (Step B5) for both ordinal and named channels.
- **Stateless** — parser (`WavefrontObj`) produces data only; `BindlessTexture` is a dumb resource; per-compile resolver (B5) is transient, not cached; `iMouse` is a per-call parameter, not tracked state on `ShaderInstance`.
- **Encapsulation** — `jam::WavefrontObj` in `jam_graphics`, zero `vk::`/3D-math; `jam_vulkan → jam_graphics` layering preserved (never inverted); `BindlessTexture` knows nothing of shaders/config.
- **Deterministic** — same OBJ bytes → same SoA mesh → same SSBO; byte-equivalence required for the glyph-atlas extraction (B1/B2); depth-composite change isolated so existing 2D pipeline stays bit-identical (C7/C8 regression checks).

## Risks / Open Questions

- **Shadertoy manifest naming — RATIFIED by ARCHITECT (no longer open):** per-project lua file named after its own containing directory (`<project>/<project>.lua`, e.g. `sirenian-dawn/sirenian-dawn.lua`), sol2-parsed, scoped to carry both texture-channel bindings AND parameter overrides (unifying with slangp's top-level `id=value` convention).
- **Runtime-JS (three.js/p5) seam** — explicitly out of this plan (RFC: "seam-only... native Vulkan mesh path ships first"). SoA buffers kept `ResourceProvider`-ready by construction (Step C5's SoA-then-interleave shape), no seam code written this plan.
- **Phase C is the RFC's own largest-effort item** — sequenced last and checkpointed (C1-C4 / C5-C8 / C9-C10) so Auditor passes gate each wave independently rather than one monolithic review at the end.
