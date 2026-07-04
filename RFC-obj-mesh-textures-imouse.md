# RFC — External Textures/LUT · Universal OBJ Mesh Loader · Shadertoy iMouse
Date: 2026-07-04
Status: Ready for COUNSELOR handoff
Source request: `oracle-slangp.md` (three items)
Primary agent: ORACLE

---

## Problem Statement

`oracle-slangp.md` requested research + RFC on three items that surfaced during the ShaderCompiler/gather-combine sprint (PLAN `starry-crafting-bear.md`), all deferred as out-of-contract at the time:

1. **LUT / external static-image textures** — RetroArch `.slangp` `textures=` directive (external image files, e.g. phosphor-mask LUTs) and Shadertoy external-image `iChannel` inputs (a static texture asset as a channel, not another buffer pass). ARCHITECT's initial assessment: reuses existing infra (`jam::vulkan::Image` RAII, bindless texture array + `writeBindlessTextureDescriptor()`), needs one new thing — an image-file→`vk::Image` loader registered into the bindless array, exposed as a named texture. **Confirm/challenge + concrete design.**

2. **Runtime OBJ-as-vertices asset rendering** — load an OBJ mesh and render it as real 3D geometry. Original premise: `jam_vulkan` is entirely zero-vertex-input, `gl_VertexIndex`-driven fullscreen triangles; no depth buffer; no camera/view/projection. **Concrete design + honest effort estimate — do not assume small.**

3. **Shadertoy mouse interactivity** — `jam::vulkan::ShaderUniforms::iMouse` exists in the push-constant block but is stamped to zeros ("mouse plumbing not implemented"). **How should real mouse state flow from JUCE into the per-frame stamp, matching Shadertoy's sign-encoded `iMouse` convention? Cleanest injection point?**

During the session the scope of item 2 expanded materially: ARCHITECT reframed the OBJ loader as a **universal SSOT mesh asset** serving both END's native Vulkan renderer *and* a future runtime-JS path (three.js / p5.js hosted in the existing `jam::JavaScriptEngine` / `juce::WebBrowserComponent` seam already proven by the shipped mermaid.js pipeline). This RFC captures that expansion in full.

---

## Research Summary

All findings below are grounded in file:line evidence (codebase) or cited library/spec sources (web), gathered via @Pathfinder, @Librarian, @Researcher. No claim rests on training priors.

### A. Item 1 — texture/bindless infrastructure (CONFIRMED reusable)

- **`jam::vulkan::Image`** — RAII `vk::Image` + `vk::ImageView` + `VmaAllocation`, move-only. `Image::create2D(...)` convenience factory (mipLevels=1, arrayLayers=1, OPTIMAL). **No built-in pixel upload** — staging delegated to caller. (`jam_vulkan/resource/jam_VulkanImage.h:14-168`).
- **Bindless slot management is per-window (per `Graphics`)**: `Graphics::setBindlessIndex(capacity)` (`jam_VulkanGraphics.h:1109`), `resetBindlessIndex(index)` (`:1123`), `writeBindlessTextureDescriptor(index, view)` writing `eSampledImage` into set 1 binding 0 (`jam_VulkanGraphics.cpp:906-916`). Slot store: `std::vector<int> bindlessIndex` in `RenderResources` (`jam_VulkanRenderResources.h:66`).
- **The glyph-atlas GPU mirror already IS the upload primitive** we need: `GlyphAtlas::uploadGlyphAtlasImage(cmd, dst, staging, mapped, offset, BitmapData, w, h)` — `memcpy` pixels→staging, `recordUploadBarrier(UNDEFINED→TRANSFER_DST)`, `copyBufferToImage`, `recordUploadBarrier(TRANSFER_DST→SHADER_READ_ONLY)` (`jam_GlyphAtlasGpuMirror.cpp:47-97`). Hot-reload **reuses** the same `vk::Image`/slot — pixels re-copied, not reallocated (`GlyphAtlas` owns `juce::Image` CPU atlases + `jam::HashMap<Type, jam::vulkan::Image> gpuImages`; dirty tracked via `isDirty`/`clearDirty`).
- **ShaderCompiler name resolver is per-compile, pass-ordinal only today** — `channelMacros(bufferPassNames, format, filter)` generates one sampler macro per ordinal + format aliases (`iChannel0-3` / `PassOutputN`) (`jam_VulkanShaderCompiler.cpp:123-150`). No named external-texture slot yet.
- **Channel stamping**: `stampChannels(ShaderUniforms&, sceneFallback)` writes bindless indices into `int32_t channels[21]` (`jam_VulkanShaderInstance.cpp:555-562`).
- **`config::Shader`** enumerates the shader project dir into a state tree; `.slangp` parse stops at pass enumeration — **`textures=` not yet parsed** (`Source/config/Config.h:33-76`).
- **No existing image-file→GPU path** — `juce::ImageFileFormat` is unused in `jam_vulkan`. Precedent for `juce::Image`→GPU is the atlas path only.
- **VMA (3.3.0)**: no API to update image contents in place; re-upload = re-copy via staging (`vk_mem_alloc.h`). Static texture recommendation: DEVICE_LOCAL + `DEDICATED_MEMORY_BIT`. Atlas (frequent): host-access + mapped staging.
- **JUCE OOTB image load** (JUCE 8.0.14, `juce_graphics`): `ImageFileFormat::loadFrom(File | InputStream | rawData,size)` → `juce::Image` → `Image::BitmapData(image, readOnly)` → `uint8* data` + `lineStride`/`pixelStride`. `ImageCache::getFromFile/getFromMemory` also available.

**Verdict:** ARCHITECT's assessment confirmed. The one new element (file decode) is `juce::ImageFileFormat::loadFrom`. Everything else exists. Directive: **do not copy the atlas path — extract it into one isomorphic abstraction so atlas + external LUT are one SSOT upload path.**

### B. Item 2 — 3D rendering feasibility (factual corrections + confirmed contract fit)

- **Correction to the original premise:** `jam_vulkan` is *not* entirely zero-vertex-input. A `VertexInput::position2D` state exists (`jam_VulkanPipelinesState.cpp:62-71`, real `vk::VertexInputBindingDescription`) driving 5 of 21 pipelines (`jam::Earcut` path fills), plus a 24-byte slang-quad vertex input. `jam::vulkan::Buffer` (`jam_VulkanBuffer.h`) already backs vertex/index data. Vertex-input plumbing is precedented.
- **Programmable vertex pulling for 3D is proven** — geometry in an SSBO pulled via `gl_VertexIndex` (zeux.io "Writing an efficient Vulkan renderer": `positionData[gl_VertexIndex + offset]`; vkguide GPU-driven rendering; origin: Rákos, *OpenGL Insights* ch.21). Fits ARCHITECT's hard contract exactly: **init all geometry once into a single buffer, upload, update only transform state per frame.**
- **Hard blockers confirmed (all zero grep):** no depth attachment anywhere — all 3 render passes stencil-only `eS8Uint` (`jam_VulkanGraphicsSetupRenderPass.cpp`); no depth pipeline state; no camera/view/projection — the sole `MVP` is a 2D orthographic screen transform (`z=0.0` always, `instanced.vert:46`, `mvp.glsl`).
- **`glm` 1.0.3 is vendored AND active** (`jam_vulkan.h:55-56`; `glm::ortho` builds both projections — `jam_VulkanLowLevelGraphicsContext.h:37`, `jam_VulkanGraphics.cpp:609`; `glm::mat4` rides the MVP UBO and slang reflection). `perspective`/`lookAt` present, unused. Vulkan-correct projection needs `GLM_FORCE_DEPTH_ZERO_TO_ONE` + `GLM_FORCE_LEFT_HANDED` + manual Y-flip (`result[1][1]` negate; glm does not flip Vulkan clip-space Y).
- **PrimitiveRecord SSBO** is the "single buffer, update as we go" reference: `PrimitiveRecordBuffer` is `VMA_MEMORY_USAGE_CPU_ONLY` + `MAPPED_BIT`, written in place, grown by doubling (old buffer parked in `previousBuffers` until fence) (`jam_VulkanPrimitiveRecordBuffer.{h,cpp}`). For a static mesh the analogous choice is device-local, uploaded once via staging.
- **Depth options (Researcher, cited):** (a) add depth to main pass — "moderately invasive, touches every pipeline/subpass/framebuffer" (vulkan-tutorial Depth_buffering); (b) **offscreen target with its own depth, composited as a texture** — Sascha Willems `offscreen` pattern, leaves main passes untouched, reuses existing offscreen-as-bindless model; (c) CPU painter's-sort — rejected on cyclic-occlusion correctness.
- **Effort signal:** "OBJ mesh in a 2D-ortho UI renderer" is a subsystem, not a small feature (LearnOpenGL model-loading is a 4-chapter sequence; MVP/camera/normals/depth are load-bearing). All pieces de-risked, but larger than items 1+3 combined.
- **PBR render-path facts (Researcher):** direct-light Cook-Torrance/GGX = zero new deps (self-authored GLSL on existing infra). Tangents (normal mapping) → MikkTSpace *recommended* by glTF spec, zlib, 2 vendor-in-tree files; naive per-triangle averaging is documented-divergent. IBL (irradiance + prefilter + BRDF LUT) = 4 new passes + `stb_image` HDR; **not** a correctness gate (Sascha Willems `pbrbasic` ships without it). KTX-Software = the only heavy lib, and it is optional/avoidable. **OBJ-PBR is a non-spec** (exocortex `.mtl` extension): no AO-map convention, no color-space convention, no emissive-strength convention — glTF is the canonical PBR format. → ARCHITECT scoped render to "enough," not full PBR/IBL/HDR.

### C. Item 3 — iMouse plumbing (cleanest seam CONFIRMED)

- `ShaderUniforms` 128-byte push-constant layout: `iMouse[4]` at offset 0, then `iResolution[2]`, `iTime`, `iTimeDelta`, `iFrame`, `channels[21]`, `iScene`, `opacity` (`jam_VulkanShaderUniforms.h:124`, doc comment: "sign-encoded per Shadertoy convention. Stamped to all zeros — mouse plumbing not implemented").
- `stampUniforms(vk::Extent2D)` builds the block, leaves `iMouse` at `{0,0,0,0}` (`jam_VulkanShaderInstance.cpp:564-580`).
- **Precedent seam:** opacity/resolution flow `render(g,shader,opacity,resolution)` → `renderShader` → `recordShaderBufferPasses` → `stampUniforms` synchronously per paint (`jam_VulkanRender.h:33`, `...ContextRender.cpp:23`, `...GraphicsShaderPass.cpp:125`). iMouse rides the same chain.
- `ShaderComponent` explicitly `setInterceptsMouseClicks(false)` + doc "Never intercepts mouse input" (`jam_VulkanShaderComponent.h`). END has **zero** mouse handlers in its tree today. Enabling intercept is trivial.
- **Post-process is app-global** (`Registry::setPostProcess`) with no owning component → no component-relative coordinates.
- **Shadertoy iMouse encoding (Researcher, IQ reference shader `Mss3zH` + cross-checked):** `.xy` = current position while button held; `abs(.zw)` = click-start position; `sign(.z) > 0` while down; `.w > 0` only on the click frame; negatives otherwise. `fragCoord` origin is bottom-left → Y-flip required: `iMouse.y = viewportHeight - mouseY`, applied to `.xy` and `abs(.zw)`. Sign bits are event state, tracked in the component's mouse handlers, not derived from coordinates.

### D. The runtime-JS seam (proven; capabilities bounded)

- **`jam::JavaScriptEngine`** exists and is integrated (`jam_javascript/engine/jam_JavaScriptEngine.h`) — a wrapper over `juce::WebBrowserComponent` (the OS WebView: WKWebView on macOS, WebView2/Chromium on Windows), NOT QuickJS/V8. MESSAGE-thread only.
- **Mermaid runs the real `mermaid.min.js`** (3.3 MB embedded) — `jam::Mermaid::Parser` loads it, `evaluateJavascript("validateAndRender(...)")`, reads back `window.lastResult` as a `juce::var` array of SVG strings (`jam_markdown/mermaid/jam_Parser.cpp`; original proven pattern in `~/Documents/Poems/dev/whelmed/Source/mermaid/mermaid_parser.h`, `mermaid::Parser : public juce::WebBrowserComponent`). **Correction logged:** SPEC.md lines 784-786 ("native AST→SVG→juce::Path") are stale; the code is a JS-engine path. Mermaid SVG cannot be produced without mermaid.js.
- **WebView graphics capability (JUCE 8.0.14, Librarian, source-cited):**
  - three.js (WebGL1/2) and p5 (canvas2D/WebGL) **do run** — WKWebView + WebView2/Chromium ship WebGL2 by default, no flag (Windows needs `JUCE_USE_WIN_WEBVIEW2=1` + `NEEDS_WEBVIEW2`).
  - **Only as a VISIBLE, attached component.** Headless GPU rendering is NOT supported (WKWebView paints only when in the view hierarchy; JUCE gates the Windows control on a real peer). Mermaid's headless string-extraction does NOT transfer to WebGL.
  - The webview is a **native heavyweight overlay** (`NSView`/`HWND` subview composited by the OS), "obliterates any JUCE components that overlap" (`NSViewComponent.h:48`). Standard JUCE native-component behavior — **renderer-independent**.
  - **Pixel readback into Vulkan is blocked** — `createComponentSnapshot`/`paint()` capture nothing for a webview; only a JS-side `readPixels`/`toDataURL` string round-trip exists.
  - **C++→JS binary channel:** `evaluateJavascript`/native-function/events all serialize through JSON text (`WebBrowserComponent.cpp:423`). The ONE binary channel is `WebBrowserComponent::Options::withResourceProvider` — `struct Resource { std::vector<std::byte> data; String mimeType; }`, `using ResourceProvider = std::function<std::optional<Resource>(const String&)>` (`juce_WebBrowserComponent.h:103-115`). Served over a custom URL scheme; JS does `fetch(root+"mesh.bin").then(r=>r.arrayBuffer())` → `Float32Array` → three.js `BufferGeometry`. `ResourceProvider`/`Resource` live in `juce_gui_extra`. No OOTB `MemoryBlock`↔`vector<std::byte>` adapter (JUCE hand-copies at `WebBrowserComponent_linux.cpp:1007`).

### E. The Vulkan engine's rendering contract (verified in code)

ARCHITECT correction (verified): the engine sits at the **lowest level** and respects the `juce::Component` native rendering contract.
- `jam::vulkan::LowLevelGraphicsContext : public juce::LowLevelGraphicsContext` (`jam_VulkanLowLevelGraphicsContext.h:25`), overriding the full low-level drawing contract (`fillRect`/`fillPath`/`drawImage`/`drawGlyphs`/`clipToPath`/`saveState`/`setFill`/…).
- Installed at JUCE's own seam: `juce::ComponentPeer::externalContextFactory = &createContext` (`jam_VulkanRegistry.h:59`), factory returns `std::unique_ptr<juce::LowLevelGraphicsContext>` (`:186`). Invoked per paint through standard `ComponentPeer::handlePaint`; `Component::paint(juce::Graphics&)` unchanged; LLGC dtor calls `endFrame()`. Per-window `Graphics` keyed by native handle. END overrides no `paint()` semantics and hosts zero native heavyweight components today.
- **Consequence:** the engine is the low-level *pen* JUCE hands every Component — natively-rendered content composites/scrolls/themes like any painted component; a live webview is an ordinary JUCE native-heavyweight overlay, renderer-independent. There is no Vulkan-specific "scene" a webview is "outside" of.

### F. JAM/JUCE primitives + module layering (for placement + OOTB reuse)

- **`jam::HashMap`** (`jam_core/map/jam_Wyhash.h`) — open-addressed robin-hood; custom struct key via `jam::Hash<Key>` specialization + `operator==`. `insert`/`emplace`/`find`/`contains`/`at`/`begin`/`end`/`erase`.
- **`jam::Owner<T>`** (`jam_core/utilities/jam_IsHashable.h`) — owning container, `: public std::vector<std::unique_ptr<T>>`; `add(unique_ptr&&)`, `contains`, `indexOf`, iteration. Precedent: `jam::Owner<ShaderPass>` in `jam_VulkanShader.h`.
- **`jam::Function::Map<KeyType, ReturnType>`** (`jam_core/function_map/jam_Function.h:172`) — `: jam::HashMap<KeyType, unique_ptr<Function::Common>>`; `add<Arg>(key, lambda)`, `get(key, arg)`, `contains`. String→function dispatch (used for terminal OSC/CSI). Correct primitive for the OBJ keyword dispatch (`v`/`vn`/`vt`/`f`/`usemtl`/`mtllib`/`g`/`o`/`s`), replacing the JUCE parser's 8-branch `if(matchToken) continue;` chain (Lean 3-branch fix).
- **`jam::Bimap`**, **`jam::LookupTable`** — int/enum registries; not callable dispatch.
- **`jam::Earcut`** — pure geometry (clean-room single-ring ear clipping), currently `jam_vulkan/earcut/jam_Earcut.{h,cpp}`, no `vk::` dependency.
- **Module dependency direction (from module declarations):** `jam_vulkan` → `jam_graphics` (vulkan depends on graphics; graphics deps `juce_core` only). Enforced with precedent — `GlyphAtlas` was relocated OUT of `jam_graphics` because "jam_graphics cannot depend on jam_vulkan" (`jam_graphics.h:104-111`).
- **JUCE OOTB (JUCE 8.0.14, verified):**
  - `juce::Array<T>` (`juce_core`) — contiguous, documented memcpy-relocatable, `getRawDataPointer()`/`size()`/`add()`/`getReference()`. **No `.at()`** (use `getReference`/`getUnchecked`). JUCE's own OBJ demo feeds `Array<Vertex>::getRawDataPointer()` to `glBufferData` (`OpenGLAppDemo.h:359-379`).
  - `juce::MemoryBlock`, `juce::HeapBlock<T>` (`juce_core`) — byte/raw-array primitives. `HeapBlock` has no `size()`/growth (raw block); unsuited to incremental parse.
  - **3D math (`Vector3D`/`Matrix3D`/`Quaternion`/`Draggable3DOrientation`) lives in `juce_opengl`** (`@tags{OpenGL}`), which transitively pulls the entire GUI/WebKit/Cocoa/Metal/CoreText stack. `juce::Point3D` does not exist. JUCE's own `WavefrontObjParser.h` deliberately hand-rolls `struct Vertex { float x,y,z; }` to avoid pulling `juce_opengl` into the parser. → **do not use JUCE 3D math; `glm` (incumbent in `jam_vulkan`) is the render math.**
  - JUCE ships **no reusable mesh/OBJ class** (parser is example source only) and **no interleaved-vertex abstraction** — its own demo hand-rolls both. So the mesh PODs are legitimately new, matching JUCE precedent 1:1.
- **`juce::WavefrontObjParser.h`** (`examples/Assets/`, ISC) — the structural base to clean-room rewrite: `class WavefrontObjFile`, `struct Vertex{float x,y,z;}`, `TextureCoord{float x,y;}`, `Mesh{Array<Vertex> vertices,normals; Array<TextureCoord> textureCoords; Array<Index> indices;}`, `Material`, `Shape`, `OwnedArray<Shape>`. Absolute-index only, `std::map` dedup, always-fan triangulation, no warnings, no normal generation.
- **tinyobjloader capability delta (Librarian) — enrichment candidates the JUCE parser lacks:** relative/negative indices (`fixIndex`, `tiny_obj_loader.h:6397`); shortest-diagonal quad + earcut n-gon triangulation; smoothing groups (`s`); structured warn/err channel; out-of-bounds + degenerate-face detection; per-face material_ids; full PBR `.mtl` (`Pr`/`Pm`/`Ps`/`norm`/maps + `texture_option_t`); lines/points; vertex colors; `vt.w`; BOM handling; multi-file `mtllib`. tinyobj itself violates JRENG wholesale (`!`/`&&`, raw `[]`, `namespace detail`, 13,865-line macro-guarded header) → mine behavior, not code.

---

## Principles and Rationale

### Locked design (BLESSED-mapped)

**Item 1 — isomorphic texture abstraction `jam::vulkan::BindlessTexture`:**
- **SSOT + Lean + DRY:** one type owns `jam::vulkan::Image` + per-window bindless slot + upload/re-upload. The glyph-atlas GPU-mirror primitive (`jam_GlyphAtlasGpuMirror.cpp:47-97`) is **extracted** into `BindlessTexture`; `GlyphAtlas` then *consumes* it (byte-equivalence check, same discipline as the gather/combine refactor). External LUTs are the identical path with a different CPU producer (`juce::ImageFileFormat::loadFrom` → `BitmapData`). Not a copy — one upload primitive, two producers (rasterizer / file decoder). Evidence: zeux.io material-index array, jorenjoestar handle + deferred descriptor write, Chunk Stories "overwrite descriptor N".
- **Bound:** CPU pixels shareable; bindless slot is per-`Graphics` (per-window) — exactly the atlas's existing shared-`vk::Image`/per-window-slot split. Ownership: glyph atlas → Registry (as today); external LUT → `ShaderInstance` per the buffer-pass precedent (config declares path, engine owns GPU resource). Static LUT = DEVICE_LOCAL + DEDICATED.
- Named-texture resolver: `.slangp textures=` parsed in `config::Shader`; a named entry added to the per-compile resolver (`jam_VulkanShaderCompiler.cpp:123-150`, pass-ordinal-only today).

**Item 2 — universal `jam::WavefrontObj`:**
- **Encapsulation + layering:** parser lives in **`jam_graphics`** (deps `juce_core` only, no `juce_opengl`), so both the native Vulkan path (`jam_vulkan` already depends on `jam_graphics`) and the JS/asset path consume it with zero coupling. Matches the enforced `jam_vulkan → jam_graphics` direction and the `GlyphAtlas`-relocation precedent. Parser touches **no** `vk::`, no 3D math.
- **DRY/OOTB:** reuse `juce::Array` (contiguous, memcpy-safe) for numeric buffers; `jam::Owner<Shape>` for shape ownership (precedent `jam::Owner<ShaderPass>`); plain-float PODs per JUCE's own precedent. `jam::Function::Map` for keyword dispatch (Lean 3-branch fix). `jam::HashMap<TripleIndex,uint32>` dedup (replaces `std::map`). `jam::Earcut` relocated `jam_vulkan`→`jam_graphics` for n-gon triangulation.
- **Explicit control flow:** positive nesting + result-returns, no bail-out guards; parse status via `juce::Result` + a `juce::StringArray& warnings` out-param (warn-and-continue).
- **Output = SoA** (per-shape separate arrays), matching JUCE and mapping 1:1 to three.js `BufferGeometry` attributes; interleaving into the Vulkan SSBO happens at upload (JUCE demo `createVertexListFromMesh` pattern).
- **Native render (Preview-style turnkey + user override):** default = AABB-derived auto-fit camera + minimal built-in lit shader (Lambert/headlight sampling MTL diffuse+normal) + glm arcball orbit — behaves like macOS Quick Look 3D. User can override shader/camera/lighting via the existing shader-pass model. Bounded ("enough," not a PBR/IBL subsystem).
- **Contract-preserving 3D:** geometry uploaded once into a single SSBO (`jam::vulkan::Buffer`), transforms updated per frame via the MVP UBO (glm `perspective`/`lookAt` + normal matrix, `GLM_FORCE_DEPTH_ZERO_TO_ONE`/`LEFT_HANDED` + Y-flip). Depth via an **offscreen target with its own depth attachment, composited back as a `BindlessTexture`** — leaves the 3 stencil-only passes untouched, reuses the ShaderInstance offscreen-as-texture model.
- **MTL scope:** full standard MTL (incl. PBR fields) parsed into the typed `Material`; renderer consumes diffuse+normal subset now. Accepted YAGNI cost on unused fields, chosen for future-proof data (parse is ~free; render bill lands only when a consumer is written).

**Item 3 — iMouse:**
- Extend `stampUniforms` to carry `iMouse[4]` through the existing opacity/resolution seam. `ShaderComponent` flips `setInterceptsMouseClicks(true)`, overrides `mouseDown/Drag/Up`, holds transient `iMouse[4]`, hands it to `render()` at paint. Sign bits tracked as event state; Y-flip `iMouse.y = height - mouseY`.
- Component-owned background shaders get live iMouse; **post-process stays `{0,0,0,0}`** (documented limitation — app-global, no component coordinates).

### Considered and REJECTED (with reason)

- **Vendor JUCE's `WavefrontObjParser.h` as-is** — rejected: absolute-index-only (negative index underflows to garbage), `std::map`, always-fan, no warnings/normal-gen, `OwnedArray`+raw `new`. Clean-room rewrite keeps its *structure/style* (JRENG-idiomatic), fixes all four, enriches with tinyobj robustness.
- **Vendor tinyobjloader** — rejected: 13,865-line macro-guarded header violating JRENG wholesale (`!`/`&&`, raw `[]`, `namespace detail`, `std::map`). Mine its *behavior* (rel indices, earcut, smoothing groups, warnings), not its code.
- **Full PBR render + IBL + HDR** — rejected by ARCHITECT: "not full parity, HDR overkill." OBJ-PBR is a non-spec (no AO/color-space/emissive conventions); IBL is a 4-pass + `stb_image` subsystem and not a correctness gate. Render stays "enough"; parser still captures full MTL data for the future.
- **JUCE 3D math (`Vector3D`/`Matrix3D`)** — rejected: lives in `juce_opengl`, dragging the full GUI/WebKit/Cocoa/Metal chain into a `juce_core`-only parser. glm is already vendored+wired in `jam_vulkan`. JUCE's own parser avoids `juce_opengl` identically.
- **`std::vector` / custom vertex containers** — rejected in favor of `juce::Array` (OOTB, memcpy-safe, JUCE-demo-proven).
- **`jam::Owner` for vertex/index buffers** — rejected: `vector<unique_ptr<Vertex>>` scatters vertices across the heap, breaking the contiguous SSBO memcpy AND the ResourceProvider byte-view (Bound violation). `jam::Owner` only for `Shape` (object ownership); contiguous `juce::Array` for numeric buffers.
- **`juce::HeapBlock` for parse buffers** — rejected: no `size()`/growth; incremental parse needs `juce::Array`'s append+count. `HeapBlock` fits only fixed-size raw blocks.
- **`MemoryBlock` in the mesh path** — rejected as SSOT container (untyped); typed `juce::Array<float-POD>` is the SSOT, `std::vector<std::byte>` produced only at the `ResourceProvider` boundary (JUCE's own hand-copy pattern; no OOTB adapter exists).
- **Parser in `jam::vulkan` namespace** — rejected: couples the JS/asset path to Vulkan. Neutral `jam_graphics` home required for the universal-SSOT goal.
- **Standalone `jam::Mesh`** — rejected (YAGNI): only OBJ exists; nested `jam::WavefrontObj::Mesh` (OBJ-scoped, JUCE-mirrored). Revisit if a second loader (glTF) lands.
- **Add depth to the main render pass** — rejected: "moderately invasive, touches every pipeline/subpass/framebuffer." Offscreen-with-own-depth isolates the change.
- **CPU painter's-sort depth** — rejected: cyclic-occlusion correctness ceiling for arbitrary meshes.
- **three.js output readback into Vulkan** — rejected: blocked at JUCE API level (webview overlay bypasses `Graphics`; only a slow JS string round-trip exists). The JS path is a *dedicated visible webview pane*, not composited into the shader pipeline.
- **Native mermaid SVG render replacing mermaid.js** — rejected as infeasible: mermaid's grammar is far too large to reimplement; SVG cannot be produced without mermaid.js. (Live-webview vs native-SVG-render of mermaid *output* remains an open WHELMED-side tradeoff, out of this RFC's scope.)

---

## Scaffold

Representative real code (JRENG standard: braces on new line; `not`/`and`/`or`; brace-init; `noexcept`; `juce::Array` with `getReference`; `jam::Owner`; positive nesting; header-only doxygen; includes belong at the topmost module header, omitted here). Method bodies marked `// ENGINEER:` are implementation fill points; signatures and structure are final.

### 1. `jam_graphics/mesh/jam_WavefrontObj.h`

```cpp
/** @file jam_WavefrontObj.h
 *  @brief Clean-room Wavefront OBJ + MTL loader. Format-agnostic mesh output
 *         (plain-float SoA) consumable by the Vulkan renderer and the runtime-JS
 *         seam alike. No vk::, no 3D-math, no juce_opengl dependency.
 */

namespace jam
{
/*____________________________________________________________________________*/
/** @brief Wavefront OBJ/MTL loader (clean-room; JUCE-precedent PODs, JRENG idiom).
 *
 *  Robustness beyond the JUCE example: relative/negative indices, degenerate-face
 *  skip, out-of-bounds detection, smoothing-group-aware normal generation when
 *  vn is absent, full-standard-MTL parse (incl. PBR fields), n-gon triangulation
 *  via jam::Earcut. Diagnostics are accumulated (warn-and-continue), never fatal
 *  except on file-open failure.
 */
class WavefrontObj
{
public:
    using Index = juce::uint32;

    struct Vertex        { float x, y, z; };
    struct TextureCoord  { float x, y;    };

    /** @brief One shape's geometry. SoA: contiguous, memcpy-safe (juce::Array). */
    struct Mesh
    {
        juce::Array<Vertex>       vertices;
        juce::Array<Vertex>       normals;
        juce::Array<TextureCoord> textureCoords;
        juce::Array<Index>        indices;
    };

    /** @brief Full standard MTL (Phong + exocortex PBR). Renderer consumes a subset. */
    struct Material
    {
        juce::String name;
        Vertex ambient { 0.0f, 0.0f, 0.0f };
        Vertex diffuse { 0.0f, 0.0f, 0.0f };
        Vertex specular { 0.0f, 0.0f, 0.0f };
        Vertex emission { 0.0f, 0.0f, 0.0f };
        float shininess { 1.0f };
        float refractiveIndex { 0.0f };
        float roughness { 0.0f };   // Pr
        float metallic { 0.0f };    // Pm
        float sheen { 0.0f };       // Ps
        juce::String diffuseTextureName;    // map_Kd
        juce::String normalTextureName;     // norm / map_bump
        juce::String roughnessTextureName;  // map_Pr
        juce::String metallicTextureName;   // map_Pm
        juce::String emissiveTextureName;   // map_Ke
    };

    struct Shape
    {
        juce::String name;
        Mesh mesh;
        Material material;
    };

    WavefrontObj() = default;

    /** @brief Parse OBJ text. Warnings accumulated; fatal only on unreadable mtllib.
     *  @param objText     the .obj file contents.
     *  @param sourceFile  used to resolve sibling .mtl / texture paths.
     *  @param warnings    non-fatal diagnostics appended here.
     *  @return ok, or fail on a hard error.
     */
    juce::Result load (const juce::String& objText,
                       const juce::File& sourceFile,
                       juce::StringArray& warnings);

    juce::Result load (const juce::File& objFile, juce::StringArray& warnings);

    const jam::Owner<Shape>& getShapes() const noexcept   { return shapes; }

private:
    /** @brief Dedup key into the source OBJ attribute pools. */
    struct TripleIndex
    {
        int vertexIndex  { -1 };
        int textureIndex { -1 };
        int normalIndex  { -1 };
        bool operator== (const TripleIndex& other) const noexcept;
    };

    jam::Owner<Shape> shapes;

    // Keyword dispatch (v/vn/vt/f/usemtl/mtllib/g/o/s) — replaces the JUCE
    // 8-branch if/continue chain. Built once, keyed by first token.
    jam::Function::Map<juce::String, void> lineHandlers;

    void registerHandlers();                    // ENGINEER: populate lineHandlers
    void generateNormals (Mesh&) const;         // ENGINEER: smoothing-group aware
    // ENGINEER: relative-index resolve, degenerate skip, jam::Earcut n-gon fan.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavefrontObj)
};
}

// jam::Hash<WavefrontObj::TripleIndex> specialization (in jam_core hash header)
// provides the HashMap<TripleIndex, uint32> dedup key. ENGINEER: wyhash the 3 ints.
```

### 2. `jam_vulkan/resource/jam_VulkanBindlessTexture.h`

```cpp
/** @file jam_VulkanBindlessTexture.h
 *  @brief GPU-resident texture: owns a jam::vulkan::Image plus its per-window
 *         bindless-array slot and the staging upload/re-upload. One SSOT upload
 *         path shared by the glyph atlas (mutable) and external LUT/image assets
 *         (static). Extracted from the former glyph-atlas GPU mirror.
 */

namespace jam::vulkan
{
/*____________________________________________________________________________*/
/** @brief Image + bindless slot + staging upload. Two-tier vocabulary:
 *         Image = low-level RAII vk::Image handle; BindlessTexture = the
 *         GPU-resident, bindless-registered, uploadable resource above it.
 *
 *  CPU pixel source is caller-supplied (juce::Image::BitmapData) — the glyph
 *  rasterizer and juce::ImageFileFormat file decoder are two producers of the
 *  same upload. Re-upload re-copies into the existing vk::Image/slot (VMA has
 *  no in-place image update); it never reallocates.
 */
class BindlessTexture
{
public:
    BindlessTexture() = default;

    /** @brief Create the device-local image + claim a bindless slot on this window. */
    void create (Graphics& graphics, int width, int height, vk::Format format);

    /** @brief (Re)upload straight-alpha pixels via the caller's staging arena. */
    void upload (vk::CommandBuffer cmd,
                 Graphics& graphics,
                 const juce::Image::BitmapData& pixels);

    int          getBindlessIndex() const noexcept { return bindlessIndex; }
    vk::ImageView getView() const noexcept          { return image.getView(); }
    bool         isValid() const noexcept           { return image.isValid(); }

private:
    jam::vulkan::Image image;
    int bindlessIndex { -1 };   // per-window slot (Graphics-owned lifecycle)

    // ENGINEER: create2D + setBindlessIndex; upload = memcpy->staging,
    // recordUploadBarrier(UNDEFINED->TRANSFER_DST), copyBufferToImage,
    // recordUploadBarrier(TRANSFER_DST->SHADER_READ_ONLY), writeBindlessTextureDescriptor.
    // Byte-equivalence with the current GlyphAtlas upload is REQUIRED.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BindlessTexture)
};
}
```

### 3. iMouse stamp extension (item 3)

```cpp
// jam_VulkanShaderUniforms.h — ShaderUniforms::iMouse[4] already exists at offset 0.

// jam_VulkanShaderInstance.h — extend the stamp signature to carry mouse state.
/** @brief Per-frame uniform stamp.
 *  @param targetExtent  scaled render-target extent (iResolution).
 *  @param iMouse        Shadertoy-encoded mouse: xy = current (while down),
 *                       abs(zw) = click-start, sign(z) = down, w>0 = click frame.
 *                       {0,0,0,0} for the post-process slot (no component).
 */
ShaderUniforms stampUniforms (vk::Extent2D targetExtent,
                              const std::array<float, 4>& iMouse) noexcept;

// jam_VulkanShaderComponent — enable interception, track sign-encoded state.
void mouseDown (const juce::MouseEvent& e) override;   // ENGINEER: set xy, zw=+pos, z>0,w>0
void mouseDrag (const juce::MouseEvent& e) override;   // ENGINEER: update xy, w<0 (held)
void mouseUp   (const juce::MouseEvent& e) override;   // ENGINEER: z<0,w<0 (released)
// Y-flip at emit: iMouse.y = height - e.position.y; applied to xy and abs(zw).
// Constructor: setInterceptsMouseClicks (true, false);
```

### 4. Engine-side native mesh path (item 2 — structure)

- **Mesh upload (`jam::vulkan::Mesh`):** interleave the SoA `Mesh` into the GPU POD `jam::vulkan::Vertex` (`float position[3]; float normal[3]; float uv[2];`) in a `juce::Array`, upload once via `jam::vulkan::Buffer` (device-local, `eVertexBuffer`|staged) + an index `Buffer` (`eIndexBuffer`); `jam::vulkan::Mesh` owns both, per-window. No per-frame re-upload.
- **Mesh-backed shader pass:** a pass variant that pulls `{pos,normal,uv}` from the mesh SSBO via `gl_VertexIndex` instead of synthesizing a fullscreen triangle — reuses `ShaderInstance`/buffer-pass machinery. Default engine lit shader (`mesh_default.vert`/`.frag`) OR user-supplied vertex+fragment (slang `#pragma stage`).
- **Camera/transform (`jam::vulkan::OrbitCamera`):** extend the MVP UBO — `glm::perspective` + `glm::lookAt` + normal matrix (inverse-transpose); AABB-derived auto-fit for the turnkey default; glm arcball orbit (not `juce::Draggable3DOrientation` — avoids `juce_opengl`).
- **Depth:** a new offscreen render target with a combined/depth attachment + a depth-test/write pipeline variant; result composited back as a `BindlessTexture` — main 3 stencil-only passes untouched.
- **JS seam (deferred build):** the same SoA buffers served as bytes via `WebBrowserComponent::Options::withResourceProvider` (`std::vector<std::byte>` produced at the boundary) → `fetch().arrayBuffer()` → `Float32Array` → three.js `BufferGeometry` / p5, rendered in a visible webview pane.

---

## BLESSED Compliance Checklist

- [x] **Bounds** — one owner per resource: `jam::vulkan::Image`/`Buffer`/`BindlessTexture` RAII; per-window bindless slots via `Graphics`; mesh SSBO uploaded once; contiguous `juce::Array` buffers (no scattered `unique_ptr`). Vulkan respects the `juce::Component` contract at the LLGC level.
- [x] **Lean** — `jam::Function::Map` dispatch replaces the 8-branch keyword chain; OOTB reuse (`juce::Array`/`Image`/`ImageFileFormat`, `glm`, `jam::Earcut`) over hand-rolling; MTL parse full but render "enough" (no PBR/IBL/HDR subsystem); nested `Mesh` over speculative `jam::Mesh`.
- [x] **Explicit** — positive nesting + result-returns, no bail-out guards; `juce::StringArray& warnings` out-param; sign-encoded iMouse documented; all parameters visible in signatures.
- [x] **SSOT** — one `jam::WavefrontObj` parse feeds native + JS (same SoA bytes); one `BindlessTexture` upload path for atlas + LUT; `jam::Earcut` single triangulator (relocated, not duplicated).
- [x] **Stateless** — parser produces data, holds no machinery state; `BindlessTexture` is a dumb resource; uniforms stamped per frame, not tracked.
- [x] **Encapsulation** — parser in `jam_graphics`, zero `vk::`/3D-math; `jam_vulkan → jam_graphics` layering preserved; `BindlessTexture` knows nothing of shaders/config; Component contract unchanged.
- [x] **Deterministic** — same OBJ bytes → same SoA mesh → same SSBO; byte-equivalence required for the glyph-atlas extraction; parse warn-and-continue is deterministic per input.

---

## Open Questions

Naming — RATIFIED by ARCHITECT (no longer open):
- Mesh GPU type: `jam::vulkan::Mesh` (owns vertex SSBO + index buffer, per-window; drives the render pass).
- Default turnkey lit shader: `mesh_default.vert` / `mesh_default.frag`.
- Camera: `jam::vulkan::OrbitCamera` (perspective + view + AABB auto-fit + glm arcball orbit).
- GPU interleaved-vertex POD: `jam::vulkan::Vertex` (`position[3]+normal[3]+uv[2]`, distinct from CPU `jam::WavefrontObj::Vertex`).

Documented limitations (accepted this session, not open):
- Post-process iMouse stays `{0,0,0,0}` (app-global, no component coordinates).
- OBJ-PBR fields are parsed but unrendered until a PBR consumer is written (deliberate future-proofing).
- three.js/p5 render only as a **visible** webview pane (no headless WebGL, no readback into Vulkan) — a JUCE/OS constraint, not a design gap.

Deferred (ARCHITECT-decided sequencing, not open):
- Runtime-JS (three.js/p5) path is **seam-only** now; native Vulkan mesh path ships first. Output kept `ResourceProvider`-ready so the JS path drops in without rework.

---

## Handoff Notes (for COUNSELOR)

- **Three deliverables, decoupled.** Item 1 (`BindlessTexture` extraction) and item 3 (iMouse) are small and independent; item 2 (OBJ subsystem) is large — plan it in phases (P1 lit geometry; textured/normal-mapped later; JS seam last). Do not conflate.
- **Item 2 spans two modules + a relocation.** `jam::WavefrontObj` lands in `jam_graphics`; `jam::Earcut` must move `jam_vulkan/earcut/` → `jam_graphics` first (pure geometry, `jam_vulkan` keeps using it via its existing graphics dep). Verify `jam_vulkan` still builds after the move (Refactor-Rewrite: delete-then-implement).
- **Byte-equivalence gate (item 1).** The `GlyphAtlas` GPU mirror is being extracted into `BindlessTexture`; atlas output must be bit-identical after the refactor (same barrier sequence, same slot reuse). Auditor check required.
- **Do NOT introduce new types where OOTB exists.** `juce::Array` (not `std::vector`), `glm` (not `juce::Matrix3D`/`juce_opengl`), `jam::Function::Map`/`jam::HashMap`/`jam::Owner`, `juce::ImageFileFormat`/`Image::BitmapData`. `jam::Owner` for `Shape` only; contiguous `juce::Array` for numeric buffers.
- **JRENG control-flow:** the JUCE parser's `if(matchToken) continue;` chain and `if(!x) return;` guards MUST become `jam::Function::Map` dispatch + positive nesting. `juce::Array` has no `.at()` — use `getReference`/`getUnchecked`.
- **Vendored dependency (item 2, later phases):** MikkTSpace (zlib, 2 files, vendor-in-tree) if/when normal-mapping tangents are needed. `stb_image`/KTX only if IBL is ever scoped (it is NOT now). None needed for P1.
- **CMake:** `jam::WavefrontObj` and moved `jam::Earcut` compile under `jam_graphics`; the mesh-pass GLSL compiles+embeds via the existing shaderc→SPIR-V→BinaryData path. No new machine-SDK lookup.
- **Docs to update:** ARCHITECTURE.md — the shader-pipeline section (new mesh-backed pass, `BindlessTexture`, offscreen depth target); the stale SPEC.md Mermaid lines 784-786 (native-SVG → mermaid.js-in-WebView) should be corrected in a WHELMED sprint, out of this RFC's direct scope but flagged.
- **iMouse:** enabling `setInterceptsMouseClicks(true)` on `ShaderComponent` is the trivial part; the real work is the `stampUniforms` signature extension threading `iMouse[4]` through `render`→`renderShader`→`recordShaderBufferPasses`. Sign encoding + Y-flip per §C.
- **Prior context:** this RFC continues PLAN `starry-crafting-bear.md` (ShaderCompiler + gather/combine, landed & audited). The three items were its explicitly-deferred gaps. `jam_javascript`/`jam_markdown` are already linked into END (mermaid.js proven); WHELMED itself is 0% implemented in C++.
