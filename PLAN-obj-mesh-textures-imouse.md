# PLAN: External Textures/LUT + Universal OBJ Mesh Subsystem

**RFC:** RFC-obj-mesh-textures-imouse.md
**Date:** 2026-07-04
**BLESSED Compliance:** verified (RFC checklist + LANGUAGE.md C++ Lean adaptation)
**Language Constraints:** C++17 / JUCE / JAM — header-preferred ~300 LOC, 30-line functions, 3-branch dispatch (tables over if/else); JRENG control flow throughout

## Overview
Deliver RFC items 1 (BindlessTexture + external textures) and 2 (OBJ mesh subsystem). Item 3 (iMouse) shipped in Sprint 61; the jam::VulkanEngine restructure (E0) landed first — every RFC "Registry" reference now maps to jam::VulkanEngine.

## Locked decisions (supplementing the RFC)
1. **Resource manifest:** `~/.config/end/shaders/<shader-name>/<shader-name>.lua` — per-project lua table, same convention as the shader project itself. Declares external textures (both formats) and the OBJ mesh connection. This IS the END-side consumption surface for C6.
2. **slangp `textures=` precedence:** both honored — `textures=` parsed for drop-in author intent (slang-shaders corpus LUTs); lua manifest overrides/extends by name (mirrors the existing parameter-override precedence).
3. **BindlessTexture is per-window-slot-map shaped:** one shared `jam::vulkan::Image`, per-`Graphics` slot map — the atlas's existing shared-image/per-window-slot split, formalized.
4. Sequencing: B-phase (textures) before C-phase (mesh). C8's depth composite consumes B1's BindlessTexture.

## Ground facts (Pathfinder, post-Sprint-61)
- Upload primitive: `uploadGlyphAtlasImage()` `jam_GlyphAtlasGpuMirror.cpp:47-96` — caller-owned staging arena (`Graphics::allocateStaging()`), barrier→copy→barrier.
- Slot claim: `Graphics::ensureGlyphAtlas()` `jam_VulkanGraphics.cpp:1049-1068`; `setBindlessIndex`/`resetBindlessIndex`/`writeBindlessTextureDescriptor` (triple-write bindings 0/3/4, capacity ÷3) `jam_VulkanGraphics.cpp:766/786/1025-1046`.
- `textures=` recognized-and-discarded today (`jam_VulkanShaderFormat.h:121`, ShaderPreset doc :96).
- Name resolvers: `channelMacros`/`sceneMacro` `jam_VulkanShaderCompiler.h:257-276`; `resolveSlangTextureBindings` `jam_VulkanGraphics.h:875` / `jam_VulkanGraphicsShaderPass.cpp:1137` (fresh per frame).
- `jam::Earcut` in `jam_vulkan/earcut/`, single consumer `appendEarcutTriangulatedRing` (LLGCPath.cpp).
- `VulkanEngine::removePeer` (`jam_VulkanEngine.h:74`) — zero callers.
- MVP UBO: single mat4, set 0 binding 0 (`mvp.glsl:11`).
- Shader glob already sweeps `${JAM_ROOT}/**/*.frag|vert|glsl` (`end/CMakeLists.txt:52-62`) — new mesh shaders need no CMake wiring.

## Validation Gate
Auditor validates at each phase boundary (marked below) against MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md, and this plan's locked decisions. B2 additionally carries the byte-equivalence gate.

## Steps

### Phase B — BindlessTexture + external textures

**B1 — Extract `jam::vulkan::BindlessTexture`**
Scope: `jam_vulkan/resource/jam_VulkanBindlessTexture.h` (new), jam_vulkan.h wiring.
Action: RFC scaffold §2, adapted per locked decision 3: owns `Image` + per-window slot map + upload/re-upload via caller's staging arena (extracted verbatim from `jam_GlyphAtlasGpuMirror.cpp:47-96` + the `ensureGlyphAtlas` slot-claim/descriptor-write sequence). Re-upload re-copies, never reallocates.
Validation: upload sequence byte-identical to the atlas primitive; RAII; no non-owning raw members.

**B2 — GlyphAtlas consumes BindlessTexture**
Scope: `jam_GlyphAtlasGpuMirror.cpp`, `jam_GlyphAtlas.h`, `Graphics::ensureGlyphAtlas`.
Action: delete the atlas's own copy of the upload/slot logic (Refactor-Rewrite: delete first); atlas becomes a consumer.
Validation: **byte-equivalence gate** — atlas output bit-identical (same barrier sequence, same slot reuse). → **Auditor phase gate 1**

**B1b — Wire `VulkanEngine::removePeer` into the real window-close path**
Scope: END window teardown seam.
Action: call removePeer where the peer actually dies (zero callers today).
Validation: per-window Graphics + its bindless slots released on close.

**B3 — File-decoder producer**
Scope: new loader (home gated with ARCHITECT at execution), `juce::ImageFileFormat::loadFrom` → `BitmapData` → BindlessTexture.
Action: second producer of the same upload; DEVICE_LOCAL + DEDICATED for static LUTs.
Validation: one upload path (SSOT), no atlas-path copy.

**B4 — Resource manifest + `textures=` carry**
Scope: `jam_VulkanShaderPreset.h/.cpp`, `jam_VulkanShaderFormat` registry, END `config::Shader`.
Action: ShaderPreset carries `textures=` entries (RetroArch vocabulary: `textures="a;b"`, `a=path`, `a_linear`, `a_wrap_mode`, `a_mipmap` — verbatim from video_shader_parse.c, keys through the IDref/registry canon); config::Shader reads `<shader-name>.lua` manifest; lua wins by name. Lua schema (table keys) gated with ARCHITECT before Engineer writes it (NAMES Rule -1).
Validation: parse vocabulary registry-sourced, zero loose literals; precedence matches parameter-override pattern.

**B5 — Named-texture resolver in ShaderCompiler**
Scope: `channelMacros`/`sceneMacro` + slang alias vocabulary.
Action: named external-texture slots enter the per-compile resolver — shadertoy: manifest name → channel macro; slang: LUT name is a first-class sampler alias.
Validation: existing pass-ordinal behavior unchanged (regression: current presets compile identically).

**B6 — Wire named textures into execution**
Scope: `jam_VulkanShaderInstance.h/.cpp`, `resolveSlangTextureBindings`, `stampChannels`.
Action: ShaderInstance owns the LUT BindlessTextures (config declares path, engine owns GPU resource — buffer-pass precedent); named-LUT branch in resolveSlangTextureBindings; shadertoy channels stamped from manifest names.
Validation: slot lifecycle (claim/release on instance destroy); ping-pong/history behavior untouched. → **Auditor phase gate 2** (test: a slang-shaders LUT preset + a shadertoy external channel render)

### Phase C — OBJ mesh subsystem

**C1 — Relocate `jam::Earcut` → `jam_graphics`**
Delete-first from `jam_vulkan/earcut/`; jam_vulkan keeps consuming via its existing jam_graphics dep. Include wiring both module headers.

**C2 — `jam::WavefrontObj` scaffold** — `jam_graphics/mesh/jam_WavefrontObj.h` per RFC scaffold §1 (signatures final): SoA Mesh (juce::Array PODs), full Material, `jam::Owner<Shape>`, `jam::Function::Map` line dispatch, `jam::Hash<TripleIndex>` specialization.

**C3 — OBJ parse behavior** — v/vn/vt/f handlers; relative/negative indices; degenerate skip; OOB detection; Earcut n-gon triangulation; smoothing-group normal generation when vn absent; TripleIndex dedup. Warn-and-continue via `juce::Result` + `StringArray& warnings`.

**C4 — MTL parser** — mtllib/usemtl/newmtl handlers; full standard MTL incl. PBR fields (parsed, render consumes diffuse+normal subset).
→ **Auditor phase gate 3** (C1-C4; JRENG control-flow scrutiny on the parser)

**C5 — GPU mesh upload** — `jam::vulkan::Vertex` POD (position[3]+normal[3]+uv[2]) + `jam::vulkan::Mesh` (interleave SoA → device-local SSBO via `jam::vulkan::Buffer`, staged once; index buffer; per-window). No per-frame re-upload.

**C6 — Mesh-backed shader pass** — vertex pulling via `gl_VertexIndex` from the mesh SSBO; `mesh_default.vert`/`mesh_default.frag` (Lambert/headlight, MTL diffuse+normal) or user-supplied slang `#pragma stage` pair; connected via the lua manifest (locked decision 1).

**C7 — `jam::vulkan::OrbitCamera`** — glm perspective + lookAt + normal matrix (`GLM_FORCE_DEPTH_ZERO_TO_ONE`/`LEFT_HANDED` + Y-flip); AABB auto-fit turnkey default; glm arcball orbit fed by the existing ShaderComponent mouse interception (Sprint 61).

**C8 — Offscreen depth target** — new offscreen render target with its own depth attachment + depth-test/write pipeline variant; composited back as a BindlessTexture; the 3 stencil-only main passes untouched.

**C9 — CMake** — jam_graphics compile surface for mesh/earcut (AppBuilder.cmake currently marks jam_graphics sources; verify the relocation compiles); mesh shaders already covered by the existing glob.

**C10 — Documentation sync** — ARCHITECTURE.md (BindlessTexture, resource manifest, mesh pass, depth target), SPEC.md.
→ **Auditor phase gate 4** (C5-C10 + full-plan sweep)

## BLESSED Alignment
Per the RFC's checklist (§BLESSED Compliance) — unchanged by this plan. LANGUAGE.md C++ Lean: new types header-preferred; parser dispatch via Function::Map (3-branch rule).

## Risks / Open Questions
- Lua manifest schema keys — gated with ARCHITECT at B4 execution (Rule -1).
- B3 loader home (jam_vulkan vs END config layer) — gated at B3 execution.
- MikkTSpace/stb_image explicitly NOT in scope (RFC: later phases, none needed for P1).

## Verification
- B2: atlas byte-equivalence (glyph rendering visually identical, same descriptor writes).
- B6: `slang-shaders` LUT preset (e.g. phosphor mask) + a shadertoy project with an external image channel — ARCHITECT rebuild/test.
- C-phase: an OBJ in a shader project dir + lua manifest renders turnkey (auto-fit camera, headlight); orbit responds to mouse; existing conformance suite stays green.
