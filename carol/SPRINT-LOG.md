# SPRINT-LOG

---

## Sprint 30: Parameter/Adapter APVTS-Verbatim Split + Controller Model::Listener ✅

**Date:** 2026-06-20
**Duration:** ~04:00

### Agents Participated
- COUNSELOR: design, planning, CONTRACT enforcement, audit orchestration
- Engineer: Parameter refactoring (6 passes), ParameterAdapter creation, Model rewire, Controller restore
- Auditor: two full CONTRACT audits (initial + final)
- Librarian: JUCE APVTS full API surface map (2 passes)
- Pathfinder: codebase discovery (GL thread model, View state, end::Model ownership, caller migration)

### Files Modified (12 total)

**JAM Framework (4 files):**
- `jam_data_structures/model/jam_parameter.h` — ParameterBase: added Listener nested struct, addListener/removeListener, sendValueChangedMessageToListeners, setValueFromVar/getValueAsVar pure virtuals. Deleted flush/restoreValues/onValueChanged. Fixed propertyId_ underscore. Parameter<int/float/int64_t>: stripped flush/restoreValues/setRawValue/ignoreCallbacks/needsUpdate/tree. Renamed store→setValue, load→getValue, raw→getRawValue. setValue calls sendValueChangedMessageToListeners.
- `jam_data_structures/model/jam_parameter_text.h` — ParameterText: stripped flush/restoreValues/ignoreCallbacks/needsUpdate/tree. Renamed store→setValue, load→getValue. Constructor removed node param. Added setValueFromVar/getValueAsVar overrides.
- `jam_data_structures/model/jam_model.h` — Added Model::Listener (parameterChanged), LockedListeners, ParameterAdapter forward decl, OwnedArray<ParameterAdapter> adapters, addParameterAdapter. Deleted storeValue/loadValue/loadText/addTextParameter/restoreValues. Added getParameter<T>. Rewired addProperties to create adapters. Updated getRawParameterValue to use getRawValue.
- `jam_data_structures/model/jam_model.cpp` — Created ParameterAdapter (cpp-internal): setRawValue (positive nesting), flushToTree (CAS + equality-against-tree + loopback guard), parameterValueChanged (equality gate + listenersNeedCalling, fans out to Model::Listener). Rewired flush to iterate adapters. vtpc: no type branching, adapter scan by tree+propertyId. replaceState: iterates adapters restoreFromTree. Deleted restoreValues/addTextParameter bodies.

**END Project (8 files):**
- `Source/shader/Controller.h` — Drop VT::Listener, add Model::Listener. attach takes Model&. parameterChanged override. Removed buildQuad, Program::fbo, getFrameCounter (dead code).
- `Source/shader/Controller.cpp` — parameterChanged calls loadShaders(). Original loadShaders body preserved. Restored setComponentPaintingEnabled/setContinuousRepainting. openGL4_1. Removed buildQuad definition.
- `Source/shader/wrapper.frag` — #version 410 core, out vec4 fragColor instead of gl_FragColor.
- `Source/shader/screen.vert` — #version 410 core, in/out instead of attribute/varying.
- `Source/config/Config.h` — Added glslBufferSize constant (65536).
- `Source/config/Config.cpp` — SHADER node ParameterText registration after shader.load. Shader::loadFromPath writes via ParameterText::setValue when registered.
- `Source/end/EventRegistration.cpp` — shader.attach(*this, model).
- `Source/Bimap.h`, `Source/Identifier.h`, `Source/config/lua/popup.lua`, `Source/config/lua/config.lua` — Sprint 29 carryover (validator cleanup, popup flattening, success_message).

### Alignment Check
- [x] BLESSED principles followed — Parameter/Adapter split is 1:1 JUCE APVTS topology
- [x] NAMES.md adhered — propertyId_ underscore fixed, all new names follow conventions
- [x] MANIFESTO.md principles applied — no bail-out guards (positive nesting), no type branching in vtpc, SSOT (adapter owns bridge state, parameter owns atomic only)

### Problems Solved
- Parameter+Adapter merger caused 10+ divergences from JUCE APVTS. Split restored verbatim topology.
- Controller did GL work from vtpc on message thread (threading bug). Rewired via Model::Listener.
- wrapper.frag/screen.vert used legacy GLSL (gl_FragColor, attribute, varying) with GL 3.2 core — fixed to 410 core.
- ParameterText reverse sync would break SPSC (two producers on double buffer). Correctly excluded from vtpc.
- vtpc if/else/if type chain eliminated — adapter scan, no branching.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 29: Validator Registration Cleanup ✅

**Date:** 2026-06-20
**Duration:** ~02:00

### Agents Participated
- COUNSELOR: design, planning
- Engineer: implementation

### Files Modified (7 total)
- `Source/config/Config.h` — IIFE validator initialization, getValidators() static, Theme errors member
- `Source/config/Config.cpp` — Deleted enumCheck/getBimapValidator/registerValidator. Theme validation with error accumulation. success_message field.
- `Source/Bimap.h` — Added getValidator() to Position and DropMode
- `Source/Identifier.h` — Added successMessage identifier
- `Source/config/lua/popup.lua` — Flattened defaults, packed size, root position
- `Source/config/lua/config.lua` — Added success_message field

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Runtime Bimap probe chain replaced with upfront HashMap registration via IIFE
- Structured bindings for try_emplace results (JRENG-CODING-STANDARD)
- popup.lua defaults table eliminated, packed size
- success_message configurable from config.lua

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 28: init.lua → config.lua Rename — Single Triple ✅

**Date:** 2026-06-18
**Duration:** ~00:30

### Agents Participated
- COUNSELOR: orchestration, fact-checking
- Engineer: rename across bimap, identifiers, lua files, doxygen

### Files Modified (6 total)
- `Source/config/lua/init.lua` → `Source/config/lua/config.lua` (renamed, self-ref comment updated)
- `Source/Bimap.h` — enum `init`→`config`, bimap entry `{ File::config, IDref::config }`, `getDefault()`
- `Source/Identifier.h` — comment updates to `config.lua` in IDENTIFIER_CONFIG/THEME context lines
- `Source/config/Config.cpp` — `File::init`→`File::config` at lines 49/51/128; ternary collapsed to `toValidID(value, true)`; loadFromPath config branch parses as CONFIG type, no more removeChild in flatten loop
- `Source/config/Config.h` — doxygen references updated
- `Source/end/View.h` — doxygen references updated
- `Source/config/lua/keys.lua` — comment updated

### Alignment Check
- [x] BLESSED principles followed (SSOT: one name = one role)
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied (Lean: tautology removed)

### Problems Solved
- Single bimap/tree/filename triple alignment: File::config / IDtype::config / config.lua
- Tautology ternary collapsed in Model::initialise
- removeChild anti-pattern eliminated from Model::loadFromPath flatten loop

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 27: Config Tree Flattening — Flat CONFIG, FLEX/SHADER Children ✅

**Date:** 2026-06-18
**Duration:** ~04:00

### Agents Participated
- COUNSELOR: planning, orchestration, fact-checking
- Engineer: all code implementation (12 dispatches)
- Pathfinder: codebase discovery (bimap/consumer mapping)

### Files Modified (16 total)
- `Source/config/Directory.h` — fileChanged moved to base, initialise/saveToPath non-pure
- `Source/config/Config.h` — getTheme/getShader removed, Theme/Shader/Model doxygen rewritten for flat tree
- `Source/config/Config.cpp` — Model::initialise flattens init.lua as CONFIG type; loadFromPath overlay pattern; Shader::loadFromPath creates SHADER child under GRAPHICS; Theme::loadFromPath creates FLEX child under THEME; no removeChild anywhere
- `Source/Identifier.h` — popups→popup, added flex, shader, tabButtonNormalOn identifiers; removed duplicate popup from IDENTIFIER_THEME
- `Source/Bimap.h` — File::popups→popup; config::Flex bimap (4 SVG entries, was Graphics); end::Map updated
- `Source/config/lua/init.lua` — shaders section renamed to graphics
- `Source/config/lua/popup.lua` — renamed from popups.lua
- `Source/config/lua/theme/gfx/theme.lua` — graphics={} CSV sections removed
- `Source/lookAndFeel/LookAndFeel.h` — theme member removed, doxygen updated
- `Source/lookAndFeel/LookAndFeel.cpp` — theme.addListener/removeListener removed, all theme.getValue→config.getValue
- `Source/lookAndFeel/EventRegistration.cpp` — loadGraphics reads FLEX child via forEachProperty; IDtype::graphics/tabButton events removed; ID::theme fires initialiseColours+loadGraphics; all setColours(theme.state)→config.state
- `Source/end/View.h` — theme member removed
- `Source/end/View.cpp` — theme listener removed; IDtype::init→root property access
- `Source/end/EventRegistration.cpp` — IDtype::graphics/tabButton events removed; IDtype::init→root property
- `Source/end/MessageOverlay.h` — getTheme()→getInstance() direct
- `Source/end/Tabs.cpp` — getTheme().getValue→config.getValue
- `Source/end/Panes.cpp` — getTheme().getValue→getInstance()->getValue
- `Source/shader/Controller.h` — getChildWithName recursive search for SHADER
- `~/Documents/Poems/dev/jam/jam_look_and_feel/jam_look_and_feel_custom.h` — tree→node parameter rename (contains, setColours)

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (singular POPUP, SHADER, FLEX)
- [x] MANIFESTO.md principles applied (SSOT single tree, no shadow state, Lean removal of graphicsCallbacks/CSV manifest)

### Problems Solved
- Three separate ValueTree roots unified into single flat CONFIG tree
- SVG CSV manifest replaced with bimap-driven disk scan (consistent with Shader pattern)
- SHADERS name collision resolved (init.lua section→GRAPHICS, GLSL→SHADER child)
- INIT wrapper removed (init.lua parsed as CONFIG type)
- graphicsCallbacks/buildGraphicsCallbacks deleted (unnecessary indirection)
- fileChanged unified in Directory base (Theme/Shader had identical implementations)
- JAM parameter name collision fixed (tree→node in setColours/contains)
- removeChild anti-pattern eliminated throughout

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 26: Config Restructure — config::Directory Generalization ✅

**Date:** 2026-06-18
**Duration:** ~05:00

### Agents Participated
- COUNSELOR: Sprint lead — design discussion (split-lifecycle diagnosis, Theme/Shader divergence, abstraction-boundary decisions, registry de-nesting, end→init rename), PLAN authoring, OOTB mandate, delegation + verification across all steps, runtime crash diagnosis (registerTypeface jassert → Theme::saveToPath existence-vs-selection guard), de-abstraction pass (stripped getDirectory/seedFile/notify/notifyId on ARCHITECT directive)
- Pathfinder: config/Theme/Shader flow survey, jam::Model base contract, Bimap consumer map, shader::Controller + LookAndFeel boundary trace
- Librarian: jam OOTB API catalogue (forEachProperty, applyFunctionRecursively, fromLua, setValuesFrom, getOrCreateDirectory, File::Watcher, Bimap/Instance) — the no-handroll reference set
- Engineer: Bimap/Identifier restructure, Directory base, Theme/Shader subclasses, Model shrink, lua rename + consumer retarget, crash fix, de-abstraction, header inlining
- Auditor: per-step compliance audits (BLESSED/NAMES/JRENG/locked PLAN) — caught undeclared startWatcher, public-virtual encapsulation leak, missing ~Model() override, EventRegistration File::Theme compile-blocker, residual handrolls

### Files Modified (13 total)

**END — Source/config/**
- `Directory.h` — NEW, header-only abstract base `config::Directory` (jam::Model + jam::File::Watcher::Listener). `load(const juce::File&)` runs initialise → saveToPath → loadFromPath → startWatcher inline; three protected pure virtuals; private startWatcher; `coalesceMs` constant. No getDirectory/seedFile/notify/notifyId.
- `Directory.cpp` — created then DELETED (bodies inlined into the header per ARCHITECT).
- `Config.h` — `Shader`/`Theme` redeclared as `Directory` subclasses (ctor passes treeType only); `config::Model` shrunk to root, `buildGraphicsCallbacks`/`graphicsCallbacks` moved to `Theme`; `~Model() override`; doxygen reworked (root-only, no seed wording).
- `Config.cpp` — `Theme`/`Shader` four-phase overrides (inline BinaryData write via local `writeWhenNeeded`, inline `sendPropertyChangeMessage`); `Model::initialise` validator walk uses `forEachProperty`; `Model::saveToPath` inline root write; `Model::loadFromPath` resolves theme/shader dir via `config::Themes::getPath`/`config::Shaders::getPath` directly; root-only startWatcher/fileChanged. `Theme::saveToPath` guard = selection (non-empty path), not existence (crash fix).
- `lua/end.lua` → `lua/init.lua` — renamed (tree type END → INIT).
- `lua/keys.lua` — reload-key comment end.lua → init.lua.

**END — Source/**
- `Bimap.h` — `File::Theme`/`File::Shaders` de-nested to top-level `config::Themes`/`config::Shaders`; `File` enum reduced to `{init, popups, keys}` (dropped `config`); `Shaders::Buffer` + `sourcePass` deleted; `end::Map` updated; `DropMode` comment init.terminal.
- `Identifier.h` — IDENTIFIER_SHADER comment: dropped deleted `File::Shaders::Buffer` ref, fixed stale `Source/shaders/shader.frag` → `Source/shader/wrapper.frag`; config-section `end.lua` → `init.lua` comments.

**END — Source/end/**
- `View.cpp` — config-section `IDtype::end` → `IDtype::init` (size, getChildWithName, tabOrientation).
- `View.h` — `resized()` doxygen corrected (removed stale `shader.resizeViewport`/FBO language); `end.lua` → `init.lua` doc refs.
- `EventRegistration.cpp` — `getValue(IDtype::end, ID::gpu)` → `IDtype::init`.

**END — Source/lookAndFeel/**
- `EventRegistration.cpp` — `getValue(IDtype::end, ID::theme)` → `IDtype::init`; `config::File::Theme::getPath` → `config::Themes::getPath` (compile-blocker).

**END — Source/action/**
- `Registry.cpp` — `buildKeyMap` manual `getNumProperties` loop → `jam::Model::forEachProperty` (explicit `[this]`).

**Root**
- `PLAN-config-restructure.md` — NEW plan document.

### Alignment Check
- [x] BLESSED principles followed
  - B: each Directory owns its `jam::File::Watcher` (RAII); config::Model owns Theme/Shader by value; lifetimes traceable from end::Application. `~X() override` across the hierarchy.
  - L: Directory header-only ≤112 lines; one inline write per saveToPath; dead `Buffer`/`config`/`sourcePass` removed (YAGNI); lookup/`forEachProperty` over hand loops.
  - E (Explicit): no bail-out guards (positive nesting); `not/and/or`; brace-init; explicit lambda captures (`[this]`/`[graphicsDir]`, no `[&]`).
  - S (SSOT): registries single-source filenames/paths; coalesceMs named constant; no shadow state (active name lives only in the config tree).
  - S (Stateless): Directory is told `load(dir)`; never asks config for the name.
  - E (Encapsulation): config pipes source via property-change; LookAndFeel builds Segments, Controller compiles GLSL — config does not build renderables; bimap pattern reused, not reinvented.
  - D: BinaryData defaults guarantee a valid tree before disk I/O; overlay deterministic.
- [x] NAMES.md adhered (Theme/Shader singular models, Themes/Shaders plural registries; `init` disambiguates the config section from runtime `end::Model`; four-phase contract verbs reused, no invented `seed`/`notify`/`getDirectory`)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md (brace-init, not/and/or, override, explicit captures, header-only doxygen, positive nesting, structured binding on bimap)

### Problems Solved
- **Split lifecycle unified:** seeding/watching/svg-dispatch were scattered in config::Model while Theme/Shader only did the load half. Now each Directory subclass owns its full initialise→write→read→watch→notify cycle; config::Model is root-only.
- **Over-nested + dead registries:** `File::Theme`/`File::Shaders`/`Shaders::Buffer` 3-level nesting + always-skipped `config` enum entry + orphaned `sourcePass` (deleted Compiler). Flattened to `config::File`/`Themes`/`Shaders`; gates and dead code removed.
- **Semantic collision:** config section `end` (tree type END) collided with namespace `end` and runtime `end::Model` (also END). Renamed to `init`/INIT; runtime model untouched.
- **OOTB enforcement:** ARCHITECT directive "no manual handroll." Replaced `getNumProperties` loops with `jam::Model::forEachProperty` (config + action::Registry); `jam::File::getOrCreateDirectory` for dir creation.
- **First-launch crash:** `Theme::saveToPath` guarded on `dir.isDirectory()` (false before creation) → theme dir never written → empty `theme.state` → `LookAndFeel::registerTypeface` jassert on `getValue(IDtype::code)`. Fixed: guard on selection (non-empty path); `getOrCreateDirectory` creates the dir.
- **Over-abstraction reverted:** `getDirectory`/`seedFile`/`notify`/`notifyId` (invented semantics against ARCHITECT's "no seed, no notify, no new patterns" directive) stripped. Owner calls the bimap directly; writes + notify inline; Directory.cpp inlined into the header.

### Debts Paid
None

### Debts Deferred
None

---

## Sprint 25: Shader Controller Redesign — Assets-to-Renderer, Ping-Pong, OOTB Uniform ✅

**Date:** 2026-06-16
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: Sprint lead, deep design discussion (graphics-LAF vs shader-Controller asymmetry analysis, OpenGL ping-pong semantics, JUCE Uniform API discovery, SwapChain design, bimap ordering, data structure corrections — 8+ design iterations with ARCHITECT), plan authoring, delegation, verification, direct edits (Map.h bimap reorder, Controller.cpp VTPC/loadShaders/render rewrites, jam_map.h framework change)
- Pathfinder: Graphics-LAF and shader-Controller data flow survey (full asset pipeline trace), jam Function::Map/HashMap API surface, JUCE OpenGL API surface (Uniform, OpenGLFrameBuffer, OpenGLContext, OpenGLAppComponent, VBlankAttachment)
- Researcher: OpenGL ping-pong FBO semantics (GL spec feedback loop, glMemoryBarrier, glTextureBarrier, Shadertoy convention, Three.js GPUComputationRenderer, first-frame initialization, cross-pass binding model)
- Librarian: JUCE OpenGL API exhaustive survey (Uniform ctor signature, FBO initialise/release lifecycle, translateFragmentShaderToV3, executeOnGLThread, VBlankAttachment platform analysis)
- Engineer: Identifier.h iChannel entries, Map.h Buffer bimap, Compiler.h/cpp rewrite (Pass + SwapChain + buildPass factory), Controller.h/cpp rewrite (pipeline + render + loadShaders + resizeViewport), View.h/cpp resized() wiring, doxygen updates
- Auditor: Full compliance audit (BLESSED/NAMES/JRENG/locked decisions — 11 PLAN checks, L line counts, E bail-out analysis, S SSOT verification)

### Files Modified (9 total)

**JAM — jam_core/map/**
- `jam_map.h:148` — `Map::Instance::contains()` uses `Map::containsValue` (avoids C++20 `std::map::contains`).
- `jam_map.h:153` — `Map::Instance::map` storage changed from `jam::HashMap<int, juce::String>` to `std::map<int, juce::String>`. Key-ordered iteration enables bimap-driven render order without manual arrays.

**END — Source/shader/**
- `Compiler.h` — rewritten: `Pass` struct (program + `optional<SwapChain>` + `unique_ptr<Uniform>` × 9). `SwapChain` struct (2 FBOs + ping-pong index + read/write/flip). `buildPass` factory declaration. `buildUniformSetter` + `UniformSetter` DELETED.
- `Compiler.cpp` — rewritten: `buildPass` factory (compiles program, constructs Uniforms OOTB from Buffer bimap iteration). `buildUniformSetter` DELETED (37 lines).
- `Controller.h` — rewritten: `std::vector<Pass> pipeline` (replaces `HashMap<Identifier, Pass>`). `resizeViewport(w,h)` public. `loadShaders(ValueTree)` (replaces HashMap param). `lastFrameTime`/`currentWidth`/`currentHeight` members. `bindIChannels`/`drawPass` private helpers. Cross-thread contract doxygen.
- `Controller.cpp` — rewritten: `renderOpenGL` uses `juce::Time::getMillisecondCounterHiRes()`. `render` flat per-pass loop with `bindIChannels`/`drawPass`. `loadShaders` reads ValueTree directly via `getChildWithName` (no intermediate HashMap). `resizeViewport` re-inits all buffer FBOs + `makeCurrentAndClear()`. `valueTreePropertyChanged` passes tree by value to GL thread. `shutdown` releases FBOs before clear. Buffer bimap structured binding throughout.

**END — Source/end/**
- `Map.h` — `Shaders` enum reordered: `common, bufferA, bufferB, bufferC, bufferD, image` (render order = key order). Nested `Buffer` bimap: `channel0→"BufferA"` (source pass stem, not uniform name). `sourcePass(slot)` = direct `map.at(slot)` lookup.
- `View.h:57-66` — `resized()` doxygen updated to document `shader.resizeViewport` call.
- `View.cpp:65` — `shader.resizeViewport (getWidth(), getHeight())` added to `resized()`.

**END — Source/**
- `Identifier.h:262-272` — `IDENTIFIER_SHADER` block: added `iChannel0..3` uniform name constants.

### Alignment Check
- [x] BLESSED principles followed
  - B: Pass owns program + SwapChain (2 FBOs) + Uniforms via unique_ptr. Release-before-destroy in shutdown/loadShaders. Cross-thread: GL thread never touches ValueTree; message thread never touches GL context. executeOnGLThread(block=false) is the only bridge.
  - L: Compiler.cpp 57 lines. Controller.cpp 248 lines. buildPass ≤20 lines. render ≤30 lines. bindIChannels ≤20 lines. drawPass ≤10 lines.
  - E: No magic strings — iChannel keywords from `"iChannel" + String(slot)` via Buffer bimap iteration. ID::image is the screen check. No bail-out guards — positive nesting throughout. Doxygen header-only.
  - S: Buffer bimap is SSOT for channel→buffer mapping. pipeline is the only pass collection. lastFrameTime is the only time state. No intermediate HashMap in VTPC or loadShaders.
  - S: Pass is dumb data — no isCompiled/isReady flags. SwapChain index is resource state co-located with FBOs.
  - E: Compiler is static utility. Pass is dumb data. Controller orchestrates. View tells Controller to resize.
  - D: std::map key-order iteration = render order. Same sources → same pipeline. juce::Time monotonic. First-frame FBO clear via makeCurrentAndClear().
- [x] NAMES.md adhered (resizeViewport semantic over literal, SwapChain/Pass nouns, read/write/flip/buildPass/loadShaders verbs, Buffer for source-identity bimap)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md (brace init, not/and/or, .at() on std::array, no anonymous namespace, doxygen header-only, positive nesting, structured binding on bimap)

### Problems Solved
- **Hand-rolled uniform setter replaced:** Compiler::buildUniformSetter (37-line lambda with captured GLint locations) deleted. JUCE OpenGLShaderProgram::Uniform OOTB — caches GLint at construction, .set() at render. Zero per-frame dispatch.
- **Multipass ping-pong:** SwapChain (2 FBOs + index) enables self-feedback. read() = previous frame's output; write() = this frame's target; flip() at end of draw. Same code path for self-feedback and upstream reads.
- **Render order deterministic:** Map::Instance::map changed from HashMap (hash-order) to std::map (key-order). Shaders enum reordered: buffers first, image last. Bimap iteration = render order. No manual std::array workaround.
- **Buffer bimap redesigned:** Was iChannel uniform name lookup (channel0→"iChannel0") + sourcePass() arithmetic (bufferA + slot). Now source-identity bimap (channel0→"BufferA") + direct map.at(slot) lookup. Uniform names constructed from "iChannel" + String(slot) during bimap iteration.
- **No intermediate HashMap:** VTPC passed tree by value to GL thread. loadShaders reads ValueTree directly via getChildWithName. No sources HashMap, no fragment assembly on message thread.
- **FBO resize event-driven:** View::resized() → Controller::resizeViewport(w,h). No per-frame GL_VIEWPORT check (fallback retained for first frame before resized fires).
- **Wall-clock time:** iTime/iTimeDelta from juce::Time::getMillisecondCounterHiRes(). Correct on 120Hz+ displays. Replaces frameCounter/60.0f.
- **First-frame FBO determinism:** makeCurrentAndClear() on both ping and pong at pipeline init. GL spec does not guarantee cleared texture contents after glTexStorage2D.
- **Pipeline as value vector:** std::vector<Pass> replaces jam::Owner<Pass>. Direct value iteration per frame — no pointer deref alias.

### Debts Paid
- None (sprint 24's multipass debt is structurally resolved by this redesign but requires ARCHITECT build verification)

### Debts Deferred
- None

---

## Sprint 24: Shader Config Pipeline Rework + SSOT fromLua ✅

**Date:** 2026-06-16
**Duration:** ~08:00

### Agents Participated
- COUNSELOR: Sprint lead, architecture discussion (config::Shader mirroring Theme, shader.frag template, multipass FBO design, Controller rewrite against LAF pattern, uniform baked lambda design, VTPC tree-children-direct-read, SSOT fromLua extraction), plan authoring, delegation, verification, iterative design with ARCHITECT (6+ design discussions)
- Pathfinder: Shader config discovery, disk shader files, config loading patterns, jam::lua::State API, LookAndFeel event flow
- Librarian: JUCE OpenGLFrameBuffer API, OpenGLContext::executeOnGLThread, translateFragmentShaderToV3, OpenGLContext::setOpenGLVersionRequired, GL core profile VAO requirements
- Engineer: All file edits — Compiler rewrite, Controller rewrite (3 iterations), Config.cpp SSOT refactor, Identifier/Map changes, end.lua table, shader.frag template, Quad VAO, GL version fix
- Auditor: Two audit passes — BLESSED/NAMES/JRENG compliance, dead code, cross-thread contract, pattern fidelity

### Files Modified (12 total)

**JAM — jam_data_structures/model/**
- `jam_model.h:181-218` — NEW: `fromLua(source, tag, fileName, validators, errors)` SSOT overload. Encapsulates lua::State + getType + from. Replaces 4 duplicated parse sequences.

**END — Source/shader/**
- `Compiler.h` — rewritten: `build()` takes single `juce::String` fragment (not StringArray). `buildUniformSetter()` replaces `registerUniforms()` — returns baked `UniformSetter` lambda. `UniformSetter` type alias. Static setter table deleted.
- `Compiler.cpp` — rewritten: `build()` single fragment, translateVertexShaderToV3 + translateFragmentShaderToV3, glBindAttribLocation. `buildUniformSetter()` discovers locations via glGetUniformLocation, captures in lambda — zero per-frame dispatch.
- `Controller.h` — rewritten: Pass = program + UniformSetter + FBO. One `passes` HashMap. `loadShaders()` mirrors `loadGraphics()`. No events map, no registerEvents, no Function::Map for uniforms. VTPC = one if-check.
- `Controller.cpp` — rewritten (3 iterations): VTPC reads tree children directly, extracts sources on message thread, posts to GL via executeOnGLThread. loadShaders compiles each pass, builds baked setter, creates FBO for buffers. render iterates bimap, one setUniforms call per pass. GL 3.2 core profile. Positive nesting throughout.
- `Quad.h` — added VAO member for GL 3.2 core profile
- `Quad.cpp` — create() generates VAO + captures attrib pointer; draw() binds VAO; destroy() deletes VAO

**END — Source/shaders/**
- `shader.frag` — template with `%%source%%` placeholder + iChannel0-3 sampler declarations

**END — Source/config/**
- `Config.h` — added config::Shader class (mirrors config::Theme), getShader() accessor, Shader member. Dead findLineNumber declaration removed. Stale doxygen fixed.
- `Config.cpp` — Shader::load() reads extensionless files from shader project directory, stores source as jam::ID::value property per child. loadFromPath() calls shader.load(). initialise() + loadFromPath() + Theme::load() refactored to use SSOT fromLua overload. loadFromPath wrapper fix (root tree wrapping prevents property leakage to CONFIG root). File::config guard added to loadFromPath (matches initialise).

**END — Source/**
- `Identifier.h` — IDENTIFIER_SHADER: pass names (common, image, bufferA-D) + config leaf keys (background, backgroundOpacity, postProcessing). Dead keys removed (shader, channels, postProcess, buffer, quad, wrapper). Shadow comment.
- `end/Map.h` — config::File::Shaders bimap reworked: deterministic pass names { common, image, bufferA-D }, extensionless filenames, no getName/extension.
- `config/lua/end.lua` — flat shaders key → nested `shaders = { background, background_opacity = 0.5, post_processing }` table.

### Alignment Check
- [x] BLESSED principles followed
  - B: Controller owns passes. Each Pass owns program + setter + FBO. GL compile via executeOnGLThread. RAII shutdown.
  - L: Controller rewritten 3× to reach clean design. No events map (one signal). No Function::Map for uniforms (baked lambda). Pass is one struct, passes is one map. ≤30 lines per method.
  - E: No magic strings — bimap drives iteration. %%source%% template via jam::Format::replaceholder. No bail-out guards — positive nesting. Baked uniform setter — no per-frame dispatch.
  - S: passes is SSOT for compiled shader state. config::Shader state is SSOT for source strings. fromLua is SSOT for lua→ValueTree. No parallel maps, no shadow state.
  - S: Pass is fully resolved artifact — no machinery state. Controller holds only frameCounter + passes + quad.
  - E: loadShaders mirrors loadGraphics. Controller mirrors LAF + OpenGLAppComponent. Bimap drives. VTPC reads tree children directly.
  - D: Same config → same sources → same compiled passes → same render.
- [x] NAMES.md adhered (loadShaders/loadGraphics, passes/graphics, Pass artifact, buildUniformSetter, fromLua SSOT)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md (positive nesting, not/and/or, brace init, contains+at not find/iterator, doxygen header-only, no anonymous namespace)

### Problems Solved
- **Config pipeline rework:** Dead shaders.lua manifest eliminated. Filesystem-is-manifest pattern. config::Shader mirrors config::Theme. Dedicated shaders table in end.lua.
- **Controller architecture:** 3 iterations. First: 3 parallel maps + compileFromConfig + events map + Function::Map uniforms. Second: 1 passes map + loadShaders + events map. Final: 1 passes map + loadShaders + direct VTPC + baked UniformSetter. Each iteration removed a layer of garbage.
- **SSOT fromLua:** 4 duplicated lua parse sequences (initialise config, initialise theme, loadFromPath, Theme::load) collapsed to one jam::Model::fromLua overload.
- **loadFromPath property leakage:** setValuesFrom(child) wrote END properties to CONFIG root (type mismatch). Fixed: wrapper root tree aligns the recursive walk.
- **GL 3.2 core profile:** Default GL 2.1 compatibility profile blocked translateFragmentShaderToV3 (version check < 3.2). setOpenGLVersionRequired(openGL3_2) enables core profile. VAO added to Quad (required by core profile).
- **shader.frag template:** Separate addFragmentShader calls created independent compilation units — user source couldn't see wrapper uniforms. Single compilation unit via %%source%% placeholder + jam::Format::replaceholder.

### Debts Paid
- None

### Debts Deferred
- Multipass FBO rendering (buffer passes produce black output — single-pass shaders work). Channel binding convention established (iChannel0=BufferA, etc.) but untested with working multipass content. Root cause undiagnosed.

---

## Sprint 23: Model Bidirectional Sync + Shader Architecture Rewrite ✅

**Date:** 2026-06-15 — 2026-06-16
**Duration:** ~04:00

### Agents Participated
- COUNSELOR: Sprint lead, architecture discussion (APVTS bidirectional contract, OpenGLAppComponent pattern, Compiler static builder, Controller ownership, YAGNI enforcement, Function::Map dispatch, static setter table, Shaders multi-extension map), plan authoring, delegation, verification, audit processing, iterative design with ARCHITECT
- Pathfinder: jam::Model API survey, endless GL wiring research, JUCE VBlank/OpenGLAppComponent discovery, codebase HashMap/Function::Map pattern survey
- Researcher: JUCE APVTS bidirectional sync mechanism (valueTreePropertyChanged chain, per-parameter loopback guard)
- Librarian: JUCE OpenGLAppComponent API, OpenGLShaderProgram internals (addShader, glShaderSource count=1), VBlankAttachment
- Engineer: All file edits across JAM + END (15+ delegations)
- Auditor: Full sprint audit — H1 (type dispatch), H2 (dead code), M1/M2/L1/L2

### Files Modified (22 total)

**JAM — jam_data_structures/model/**
- `jam_model.h:28-29` — Model inherits juce::ValueTree::Listener; valueTreePropertyChanged override (dispatches on registered AnyMap type via isType<T>)
- `jam_model.cpp:5-21,266-288` — constructors register state listener, destructor removes; valueTreePropertyChanged with per-parameter isIgnoringCallbacks guard; flush/replaceState/setValuesFrom global guards removed
- `jam_parameter.h` — moved from value_tree/; added per-parameter ignoreCallbacks + isIgnoringCallbacks() + ScopedValueSetter in flush() on all three specializations (VERBATIM APVTS pattern)
- `jam_parameter_text.h` — moved from value_tree/, removed submodule includes
- `jam_value_tree_utils.cpp` — moved from value_tree/

**JAM — jam_data_structures/**
- `jam_data_structures.h:30-31` — include paths value_tree/ → model/
- `jam_data_structures.cpp:3` — include path value_tree/ → model/

**END — Source/shader/ (architecture rewrite)**
- `Compiler.h` — NEW: static builder struct (jam::view::Manager pattern). build() + registerUniforms()
- `Compiler.cpp` — NEW: static setter table (braced init HashMap<GLenum, Setter>), uniform discovery via glGetActiveUniform, Function::Map dispatch lambdas
- `Controller.h` — rewritten: mirrors OpenGLAppComponent VERBATIM. Owns Owner<OpenGLShaderProgram> programs + Function::Map uniforms + Quad
- `Controller.cpp` — rewritten: initialise/shutdown/render delegates. quad.create() no params. shutdown clears programs + uniforms
- `Quad.h` — stripped to VBO only. Removed getVertexShader(), isCreated(), vertexShader member, context param from create()
- `Quad.cpp` — stripped: create() allocates VBO only, no vertex shader loading
- `Pass.h` — DELETED (replaced by Compiler + Controller ownership)
- `Pass.cpp` — DELETED
- `Buffer.h` — DELETED (YAGNI)
- `Buffer.cpp` — DELETED

**END — Source/shaders/**
- `shader.frag` — NEW: single wrapper (uniforms + mainImage prototype + main). Replaces shadertoy_uniforms.frag + shadertoy_main.frag
- `quad.vert` — renamed from passthrough.vert
- `shadertoy_uniforms.frag` — DELETED
- `shadertoy_main.frag` — DELETED

**END — Source/**
- `Identifier.h:259-267` — IDENTIFIER_SHADER: added quad, iResolution, iTime, iTimeDelta, iFrame
- `end/Map.h:235-279` — config::File::Shaders: multi-extension map (lua/vert/frag), enum adds quad + shader, getName() per-key extension lookup
- `end/View.cpp:21-23,85-89` — seed width/height before Attachment, setViewState via model.setValue()

### Alignment Check
- [x] BLESSED principles followed
  - B: GL thread reads atomics, message thread writes VT. Per-parameter loopback guard (VERBATIM APVTS). Single owner for GL programs (Owner<>). RAII.
  - L: Compiler is stateless utility (zero members). Buffer deleted. Pass deleted. Controller stripped. Static setter table replaces switch. YAGNI enforced.
  - E: No magic strings — Shaders::getName() for filenames, ID:: constants for uniforms, braced init setter table. No shadow state.
  - S: VT is message-thread truth, atomic is GL-thread truth. Bidirectional sync. shader.frag is sole SSOT for uniform declarations. Function::Map for uniform dispatch — .frag defines, Compiler discovers, Controller dispatches.
  - S: Compiler is stateless. Controller holds only GL resources + dispatch map. No machinery state.
  - D: Same VT write → same atomic → same GL output. Same fragments → same linked program.
- [x] NAMES.md adhered (shader::Compiler, build(), registerUniforms(), quad.vert, shader.frag — nouns for things, verbs for actions, literal names for Identifier constants)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md (brace init, not/and/or, namespace consistency, no submodule includes, getReference() not [], static for file-local, braced init for static HashMap)

### Problems Solved
- **jam::Model missing VT→atomic sync:** Added ValueTree::Listener + valueTreePropertyChanged. Per-parameter loopback guard (VERBATIM APVTS — ignoreCallbacks on Parameter, ScopedValueSetter in flush, isIgnoringCallbacks in listener). Global guard initially implemented, corrected to per-parameter after ARCHITECT review.
- **Type dispatch divergence:** valueTreePropertyChanged dispatches on registered AnyMap type (isType<T>), not live var type. Safe across int/double coercion.
- **Assertion crash on config reload:** params.get<AnyMap>(groupId) asserted on absent key. Fixed: guard with params.isType<AnyMap>(groupId).
- **Pass was stateful + owned program + did compilation + rendering + uniform dispatch:** Decomposed: Compiler (static builder), Controller (ownership + rendering), Function::Map (dispatch). Single responsibility each.
- **String concatenation for shader sources:** Eliminated. Two addFragmentShader() calls (wrapper + user source). JUCE compiles each as separate shader object, links together. No raw GL boilerplate.
- **Switch-case for uniform types:** Replaced with static HashMap<GLenum, Setter> braced initializer. Dispatch resolved at discovery time via Function::Map lambdas. Zero per-frame conditionals.
- **Magic strings everywhere:** Eliminated. Shaders::getName() for filenames. ID:: Identifier constants for uniform names. Static setter table for GL type dispatch. Zero literal strings in shader:: code.
- **Shadow state on Controller:** All removed. OpenGLAppComponent pattern: frameCounter only.

### Debts Paid
- None

### Debts Deferred
- None

---

---

## Sprint 22: shader:: GL Rendering Infrastructure + Event-Driven Init ✅

**Date:** 2026-06-15
**Duration:** ~04:00

### Agents Participated
- COUNSELOR: Sprint lead, architecture discussion (Shadertoy pipeline, JUCE GL integration, config flow, VTPC event-driven init), plan authoring, delegation, verification, cleanup iterations
- Pathfinder: Endless (old END) GL wiring survey, JAM module survey (Mailbox, Buffer, Resizer, Atlas)
- Researcher: Shadertoy multipass spec (Buffer A-D, iChannel routing, ping-pong, uniforms)
- Librarian: JUCE OpenGL API research (OpenGLContext render sequence, OpenGLFrameBuffer, OpenGLShaderProgram, OpenGLGraphicsContextCustomShader, translateFragmentShaderToV3)
- Engineer: All file creation and edits across 10+ delegations

### Files Modified (17 total)

**New — Source/shader/**
- `shader/Controller.h` — GL pipeline orchestrator, juce::OpenGLRenderer, owns context + quad + Pass + Buffer[4]
- `shader/Controller.cpp` — attach/detach (setComponentPaintingEnabled true, JUCE native), renderOpenGL (clear + buffers + background), GL resource lifecycle
- `shader/Pass.h` — Shadertoy shader pass: compile with uniform wrapper, render with cached glUniform locations
- `shader/Pass.cpp` — Shadertoy wrapper prepend, translateFragmentShaderToV3, iChannel texture binding
- `shader/Buffer.h` — ping-pong double-buffered FBO for multipass feedback
- `shader/Buffer.cpp` — two OpenGLFrameBuffer + index swap, resize, render delegate to Pass
- `shader/Quad.h` — fullscreen quad VBO, passthrough vertex shader from BinaryData
- `shader/Quad.cpp` — VBO create/destroy/draw, triangle strip 4 vertices

**New — Source/shaders/**
- `shaders/passthrough.vert` — GLSL 1.10 passthrough vertex shader (BinaryData-embedded)

**Modified — Source/end/**
- `View.h` — added shader::Controller member, removed setRenderer/initRenderer/paint override, SSOT doxygen
- `View.cpp:36-45` — callAsync fires gpu/alwaysOnTop/titleBarButtons events for init (DRY/SSOT), removed initRenderer/setRenderer/buildAndPostSnapshot
- `EventRegistration.cpp:49-58` — added ID::gpu event handler (GpuProbe + config → canUseGpu → BackgroundBlur + shader attach/detach)
- `Window.h:42-46` — constructor simplified to (Component*, String)
- `Window.cpp:7-12` — constructor defaults alwaysOnTop=false, showWindowButtons=true

**Modified — Source/config/**
- `config/lua/end.lua:82-86` — added `shaders = ""` config key
- `Config.cpp` — unchanged from prior sprint (watcher additions removed during cleanup)

**Modified — Source/**
- `Identifier.h:191,274-278,299` — IDENTIFIER_SHADER block (shader, channels, postProcess, buffer), shaders in IDENTIFIER_APP, wired into END_MAKE_VIEW
- `end/Map.h:228-283,297` — config::File::Shaders struct (parallel to Theme), added to end::Map
- `Main.cpp:15-36` — stripped to pure lifecycle: LookAndFeel default, View+Window construct, setVisible. Zero config reads.

### Alignment Check
- [x] BLESSED principles followed
  - B: GL thread owns GL resources, message thread owns config/state, Mailbox removed (was wrong transport)
  - L: Controller stripped to minimum (context + quad + background + buffers), no sceneFBO/postProcess/MM lock
  - E: Event-driven init via SSOT handlers, config gpu bool explicit
  - S: Single event handler for gpu init AND hot-reload, no duplication
  - D: Fixed render order (clear → buffers → background → JUCE components)
- [x] NAMES.md adhered (shader::Controller, shader::Pass, shader::Buffer, shader::Quad — all nouns, semantic)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md

### Problems Solved
- **Architecture violation (Snapshot/Mailbox):** Initial design bypassed ValueTree config chain with a lock-free Mailbox + Snapshot struct. Violated ARCHITECTURE.md:196-205 (config chain). Deleted Snapshot.h, removed Mailbox, config data flows through ValueTree listeners.
- **Architecture violation (View file I/O):** buildAndPostSnapshot() on View did lua parsing + disk reads — config::Model's job. Deleted entirely.
- **UB crash (GL attach during initialise):** Attaching GL context synchronously in Main::initialise() before event loop runs caused UB. Moved to deferred callAsync in View constructor.
- **UB crash (manual component paint from GL thread):** setComponentPaintingEnabled(false) + manual paintEntireComponent with MessageManagerLock from GL thread caused deadlocks and thread starvation. Reverted to setComponentPaintingEnabled(true) — JUCE native pipeline.
- **Main bloat:** Main read config, resolved state, called setRenderer — all View's responsibility. Stripped Main to pure lifecycle owner.
- **Init/hot-reload duplication:** initRenderer() duplicated event handler logic. Eliminated by firing event handlers via callAsync for initial state — same code path as VTPC.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 21: Tab Orientation from Theme to Config ✅

**Date:** 2026-06-15
**Duration:** ~00:15

### Agents Participated
- COUNSELOR: Sprint lead, decision framing (lua key naming), delegation, verification
- Pathfinder: Theme/config structure discovery, tab orientation flow trace
- Engineer: All file edits — identifier, lua configs, View, EventRegistration, doxygen

### Files Modified (5 total)

**END — Source/**
- `Identifier.h:203` — added `X (tabOrientation, "tab_orientation")` to IDENTIFIER_APP
- `end/View.cpp:84` — `theme.getValue (IDtype::tab, ID::orientation)` → `config.getValue (IDtype::end, ID::tabOrientation)`
- `end/View.h:112-113,120` — doxygen: registerEvents handler list updated (orientation → tabOrientation, theme handler description), setTabOrientation source updated
- `end/EventRegistration.cpp:15` — event key `ID::orientation` → `ID::tabOrientation`; `ID::theme` handler: removed `setTabOrientation()` call

**Config — Source/config/lua/**
- `end.lua:51-52` — added `tab_orientation = "top"` with comment in APP section
- `theme/gfx/theme.lua:243-244` — removed `orientation = "top"` and comment from tab table

### Alignment Check
- [x] BLESSED principles followed (S/SSOT: orientation owned by one config surface, not two)
- [x] NAMES.md adhered (tabOrientation follows camelCase convention, lua key snake_case)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md

### Problems Solved
- View VTPC listened to theme tree solely for tab orientation changes — moved orientation to end.lua config tree, eliminating unnecessary theme→View coupling for this property

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 20: View + LookAndFeel VTPC Event Dispatch ✅

**Date:** 2026-06-15
**Duration:** ~01:30

### Agents Participated
- COUNSELOR: Sprint lead, dispatch pattern design discussion, delegation, verification
- Engineer: EventRegistration.cpp creation (View + LookAndFeel), VTPC refactor, doxygen updates
- Pathfinder: jam::Function::Map API discovery, Identifier macro system verification (AS_TYPE uppercase vs AS_IDENTIFIER lowercase)

### Files Modified (6 total)

**END — Source/end/**
- `View.h` — doxygen: comprehensive documentation for all public/private members, class doc updated with action/event dispatch system description, single-key dispatch rule
- `View.cpp:15-16` — added `registerEvents()` call in constructor after `registerActions()`
- `View.cpp:60-66` — `valueTreePropertyChanged` refactored: single-key dispatch (`property` priority, `tree.getType()` fallback), replaces 34-line inline if-chain
- `EventRegistration.cpp` — **new file**: `View::registerEvents()` populating events map with 8 handlers (loadMessage, orientation, focus, theme, graphics, tabButton, alwaysOnTop, titleBarButtons)

**END — Source/lookAndFeel/**
- `LookAndFeel.h:124,128-130` — added `events` member (`jam::Function::Map<Identifier, void>`), `registerEvents()` declaration; doxygen: comprehensive documentation for all public/private members including class doc, constructor, destructor, draw/get methods, registration methods, event map key listing
- `LookAndFeel.cpp` — `registerTypeface()`, `initialiseColours()`, `loadGraphics()` moved out to EventRegistration.cpp; `registerEvents()` call added to constructor; `valueTreePropertyChanged` refactored to single-key dispatch (identical to View); all `jam::debug::Log::write` diagnostics removed from loadGraphics
- `EventRegistration.cpp` — **new file**: `registerTypeface()`, `initialiseColours()`, `loadGraphics()` (moved from LookAndFeel.cpp, debug logs stripped), `registerEvents()` registering 11 handlers (theme, graphics, tabButton, + 8 colour tree types: code, scrollbar, tab, button, overlay, pane, statusBar, hint)

### Alignment Check
- [x] BLESSED principles followed (L: 3-branch rule — VTPC if-chains replaced by direct lookup; S/SSOT: event handlers declared once in registration, dispatched uniformly; E: explicit key selection logic)
- [x] NAMES.md adhered (no new names introduced)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md (doxygen header-only, @param matching signatures, zero-warning target)

### Problems Solved
- View VTPC: 34-line inline if-chain with mixed property/type checks → single-key Function::Map dispatch (parameterChanged pattern from kuassa/jreng-filter-strip)
- LookAndFeel VTPC: inline `contains(tree, property)` colourMap check eliminated → colour tree types registered as events, same single-key dispatch
- LookAndFeel.cpp god-object tendency: registration methods extracted to EventRegistration.cpp, .cpp now exclusively LAF rendering methods + VTPC dispatch
- Stale debug diagnostics: 7 `jam::debug::Log::write` calls removed from loadGraphics (infrastructure preserved)

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 19: Named Colour Palette + lua::State Full Standard Library ✅

**Date:** 2026-06-15
**Duration:** ~02:00

### Agents Participated
- COUNSELOR: Sprint lead, design discussion, plan, delegation, verification
- Engineer: theme.lua palette rewrite, kuassa colour DB nearest-name computation, jam::lua::State luaL_openlibs
- Pathfinder: theme.lua colour flow trace (lua → ValueTree → ColourMap → LAF)
- Auditor: value-identity validation of theme.lua rewrite (PASS)
- Librarian: Lua 5.4 library loading mechanisms research
- Researcher: embedded Lua sandbox library patterns research

### Files Modified (3 total)

**JAM framework:**
- `jam_lua/jam_lua_state.h` — State constructor adds `luaL_openlibs(state.get())`; `openLibraries()` template + `openLibrary()` private method deleted; doxygen updated
- `jam_lua/jam_lua_types.h` — `enum class Lib` deleted (dead code, zero callers)

**END project:**
- `Source/config/lua/theme/gfx/theme.lua` — `local colours = { ... }` palette block (27 entries: 26 DB-named via kuassa::colours::getNearestNameFormatted + `transparent`); `withAlpha(c, a)` as colours table method; all 46 colour properties reference `colours.<name>` or `colours.withAlpha(...)`, zero bare hex literals

### Alignment Check
- [x] BLESSED principles followed (S/SSOT: single palette block eliminates duplicate colour literals; E: named over magic hex)
- [x] NAMES.md adhered (palette names from kuassa DB algorithm, collision 519299/4e8c93 resolved: lagoon/paradiso)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md

### Problems Solved
- theme.lua bare hex colour literals scattered and duplicated (S/SSOT violation) → single named palette, referenced everywhere
- math.floor assertion at startup — jam::lua::State opened zero standard libraries; theme.lua withAlpha uses math.floor → tree empty → downstream assert. Root cause: State constructor never called luaL_openlibs. Fix: luaL_openlibs in constructor, all standard libs OOTB
- Collision in kuassa DB nearest-name: 519299 and 4e8c93 both → "lagoon"; resolved by ARCHITECT: 519299=lagoon (1st nearest), 4e8c93=paradiso (2nd nearest)

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 18: button::Tab, SVG Flex Data-Driven, LAF + Config Restructure ✅

**Date:** 2026-06-15
**Duration:** ~08:00

### Agents Participated
- COUNSELOR: Sprint lead, design discussion, plan, delegation, verification
- Engineer: Code implementation across JAM + END
- Pathfinder: Codebase discovery (tab button patterns, LAF structure, PaneManager)
- Auditor: N/A (findings resolved inline)

### Files Modified (35 total)

**JAM framework:**
- `jam_graphics/svg/jam_svg_flex.h` — SVG::Flex::paint + StyledGraphics::paint: Component& removed, takes LookAndFeel& + bounds
- `jam_graphics/svg/jam_svg_flex.cpp` — implementations updated
- `jam_gui/button/jam_button_tab.h` — NEW: button::Tab with TabLabel, showEditor, setLabelLayout
- `jam_gui/button/jam_button_bar.h` — Events<SVG> → Events<Tab>, TabInfo type, createTabButton/getTabButton types
- `jam_gui/button/jam_button_bar.cpp` — createTabButton creates Events<Tab>, onRightClick → showEditor, highlight uses target bounds, label layout propagation
- `jam_gui/layout/jam_tabbed_component.h` — createTabButton type
- `jam_gui/layout/jam_tabbed_component.cpp` — createTabButton type + impl
- `jam_gui/layout/jam_pane_resizer_bar.h` — delegates to Custom::drawResizerBar, LookAndFeelMethods removed, isVerticalBar reads live, isVertical member removed, constructor simplified
- `jam_gui/layout/jam_pane_resizer_bar.cpp` — constructor + paint updated
- `jam_gui/layout/jam_pane_manager.h` — resizerBarSize configurable, NeededBar::isVertical removed, onMouseDrag reads live
- `jam_gui/jam_gui.h` — jam_button_tab.h include added
- `jam_look_and_feel/jam_look_and_feel_custom.h` — drawTabLabel, drawResizerBar virtuals
- `jam_data_structures/model/jam_model.h` — toStringArray static utility

**END project:**
- `Source/Identifier.h` — resizeBar/resizeBarHighlight/resizeBarThickness identifiers, dead Graphics identifiers removed, "node" → "tree" comments
- `Source/end/Map.h` — File::Theme + File::Graphics nested inside File, File::Graphics struct deleted, end::Map updated
- `Source/config/Config.h` — config::Theme class inlined (was config::LookAndFeel), getLookAndFeel → getTheme, LookAndFeel.h include removed
- `Source/config/Config.cpp` — Theme::load moved here, initialise builds theme tree, saveToPath tree-driven SVG seeding, buildGraphicsCallbacks tree walk, fileChanged uses jam::IDref::svg
- `Source/config/LookAndFeel.h` — DELETED
- `Source/config/LookAndFeel.cpp` — DELETED (merged into Config.cpp)
- `Source/lookAndFeel/LookAndFeel.h` — config::Theme& theme, drawTabLabel, drawResizerBar, drawStretchableLayoutResizerBar removed
- `Source/lookAndFeel/LookAndFeel.cpp` — drawTabButton text removed, drawTabLabel, drawResizerBar with SVG Flex, drawBarBackground/drawBarHighlight/drawResizerBar contains() guards, loadGraphics single tree walk with toStringArray
- `Source/end/View.cpp` — model.addListener for focus events, setTabOrientation on theme reload, getTheme() refs
- `Source/end/Tabs.h` — "node" → "tree" doxygen
- `Source/end/Tabs.cpp` — getTheme() ref, Label value wired after Attachment
- `Source/end/Panes.h` — "node" → "tree" doxygen
- `Source/end/Panes.cpp` — seeds jam::ID::name, reads resize_bar_thickness
- `Source/end/PaneView.h` — "node" → "tree" doxygen
- `Source/end/MessageOverlay.h` — getTheme() ref
- `Source/config/lua/theme/gfx/theme.lua` — graphics arrays on tab/pane tables, resize_bar/resize_bar_highlight/resize_bar_thickness, flat graphics section deleted
- `Source/config/svg/resizer_bar.svg` — NEW: 3-slice dummy SVG

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (node → tree enforced)
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md (no bail-out guards, positive nesting, brace init)

### Problems Solved
- Tab orientation not updating on theme reload — View vtpc now calls setTabOrientation on ID::theme
- Vertical tab rendering — SVG Flex paint decoupled from Component (bounds param), LAF rotation for vertical bars
- Highlight position stale after drag-reorder — uses computed target bounds, not pre-animation getBounds
- PaneResizerBar isVertical stale after tree restructure — reads live from splitNode, no cached bool
- focusedPane never set — View now listens to end::Model for PaneView focus events
- Data-driven SVG loading — single tree walk replaces manual per-file blocks, toStringArray for CSV properties

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 17: Theme Engine — Config/Theme Restructure, Per-Component Colour Map ✅

**Date:** 2026-06-14
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: architecture, planning, orchestration, direct fixes
- Engineer: Map.h rewrite, Identifier.h restructure, lua files, Config.cpp, LookAndFeel.cpp, View.cpp, Window.h/cpp
- Pathfinder: full config consumer audit (2x), jam identifier survey
- Auditor: 28-finding audit — all resolved
- Researcher: ANSI colour naming convention

### Files Modified (32 total)

**END — Config lua (eliminated):**
- `Source/config/lua/display.lua` — DELETED (all content → theme.lua)
- `Source/config/lua/graphics.lua` — DELETED (filename mappings → theme.lua graphics section)
- `Source/config/lua/nexus.lua` — DELETED (content → end.lua)
- `Source/config/lua/actions.lua` — DELETED (content → end.lua actions section)
- `Source/config/lua/whelmed.lua` — DELETED (moved to theme dir)

**END — Config lua (modified):**
- `Source/config/lua/end.lua` — rewritten: app config + runtime (absorbs nexus + actions), native booleans, gpu collapsed to bool
- `Source/config/lua/keys.lua` — added whelmed navigation keys (scroll_down/up/top/bottom)

**END — Theme lua (new/modified):**
- `Source/config/lua/theme/gfx/theme.lua` — complete visual config: window, ansi, code, cursor, scrollbar, tab, button, overlay, pane, status_bar, hint, menu, action_list, graphics
- `Source/config/lua/theme/gfx/whelmed.lua` — NEW (moved from config root, nav keys removed)

**END — C++ headers:**
- `Source/Identifier.h` — DISPLAY→THEME, NEXUS→APP, removed nexus/display/actions from CONFIG, whelmed nav keys→KEYS, blurStyle dead ID removed, titleBarButtons/saveWindowState/ansi/scrollbar/hint/thumb/track/labelBackground/labelText/spinner added, backgroundColour removed
- `Source/end/Map.h` — File enum 8→3, Theme Map::Instance with nested Graphics, standalone Graphics deleted, Boolean/GpuMode deleted, end::Map updated
- `Source/end/Window.h` — dumb: no VTL, no config, no styleParameters
- `Source/end/View.h` — unchanged
- `Source/end/Tabs.h` — doxygen: display→theme
- `Source/end/MessageOverlay.h` — reads from theme tree, doxygen updated
- `Source/config/Config.h` — doxygen: 3 files, four-phase init, removed stale references
- `Source/config/LookAndFeel.h` — doxygen: themes/gfx/theme.lua, reads theme.lua+whelmed.lua
- `Source/lookAndFeel/LookAndFeel.h` — Glass typedef Union<Colour, int16, int16>, getWindowGlass

**END — C++ source:**
- `Source/Main.cpp` — reads alwaysOnTop/titleBarButtons from config, stale comment fixed
- `Source/end/Window.cpp` — dumb: ctor calls lookAndFeelChanged() only, no VTL
- `Source/end/View.cpp` — dispatches window ops via getTopLevelComponent, setTabOrientation from theme, sendLookAndFeelChange on ID::theme signal, size from IDtype::end
- `Source/end/Tabs.cpp` — depth from theme via config.getLookAndFeel()
- `Source/config/Config.cpp` — File 3 entries, theme seeding via Theme::get/Graphics, buildGraphicsCallbacks from theme tree, watcher watches theme dir, Boolean/GpuMode validators removed, Windows desktop scale block removed
- `Source/config/LookAndFeel.cpp` — iterates Theme::get() for all theme lua files, sendPropertyChangeMessage(ID::theme) after complete tree, removeAllChildren on reload
- `Source/lookAndFeel/LookAndFeel.cpp` — ColourMap from theme.state, per-component setColourId (code/scrollbar/tab/button/overlay/pane/statusBar/hint), all getValue/getInt16 from theme, vtpc handles ID::theme signal, loadGraphics from theme tree, getWindowGlass uses jam::ID::background

**JAM framework:**
- `jam_lua/jam_lua_types.h` — LineMapBuilder: std::map→HashMap, std::set→HashSet, Error class deleted

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Theme engine complete: all visual properties in theme directory, config root shrunk to 3 operational files
- Per-component colour nodes with 1:1 colourId mapping (no flat dump)
- Window dumb — View orchestrates operational properties
- Native lua booleans everywhere — Boolean/GpuMode bimaps eliminated
- Proper ValueTree::Listener for theme reload: sendPropertyChangeMessage(ID::theme) after copyPropertiesAndChildrenFrom, no bail-out guards
- config::Theme Map::Instance with nested Theme::Graphics — follows existing Map::Instance pattern
- Startup bug: initial always_on_top/title_bar_buttons read from config at construction

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 16: Theme Engine Foundation, Window Glass Migration, map::WindowFX

**Date:** 2026-06-14
**Duration:** 03:30

### Agents Participated
- COUNSELOR: architecture discussion, incremental migration planning, chain tracing, BLESSED violation identification
- Pathfinder: config/theme architecture survey, construction order tracing, JUCE Colour/ValueTree source verification
- Librarian: JUCE ValueTree::copyPropertiesAndChildrenFrom event analysis, jam::debug::Log API
- Engineer: all code implementation (theme files, config::LookAndFeel, Window migration, map::WindowFX, rename)

### Files Modified (25 total)

**JAM framework (11 files):**
- `jam_data_structures/map_instance/jam_map_window_fx.h` (new) — map::WindowFX bidirectional int-string bimap for BackgroundBlur window FX styles, platform-conditional
- `jam_data_structures/jam_data_structures.h:50` — added jam_map_window_fx.h include
- `jam_style/background_blur/jam_background_blur.h:15,31,105,120-146` — enum class Backend renamed to WindowFX, fromString() deleted
- `jam_style/background_blur/jam_background_blur.cpp:128-139` — enable() signature + switch cases renamed Backend to WindowFX
- `jam_style/background_blur/jam_background_blur.mm:179-190` — same rename in macOS implementation
- `jam_gui/window/jam_window.h:20,90,137` — setGlass signature, windowFX member, doxygen
- `jam_gui/window/jam_window.cpp:84-173` — setGlass impl + all deferred paths renamed
- `jam_gui/window/jam_modal_window.h:69,85` — signature rename
- `jam_gui/window/jam_modal_window.cpp:26,46` — definition rename
- `jam_gui/window/jam_glass_component.cpp:56,58` — enum qualifier rename
- `jam_gui/button/jam_button_dialog.h:137,139` — enum qualifier rename
- `jam_look_and_feel/theme/jam_look_and_feel_theme.cpp:123,125` — enum qualifier rename

**END project (14 files):**
- `Source/config/LookAndFeel.h` (new) — config::LookAndFeel class, jam::Model for theme state
- `Source/config/LookAndFeel.cpp` (new) — load() reads theme.lua from themes dir, seeds from BinaryData
- `Source/config/Config.h:4,112,233` — includes LookAndFeel.h, getLookAndFeel() getter, lookAndFeel member
- `Source/config/Config.cpp:47,121,168` — File::config skip removed in initialise/loadFromPath, lookAndFeel.load() call wired
- `Source/config/lua/end.lua:35-37` — rewritten from require-hub to return { theme = "gfx" }
- `Source/config/lua/theme/gfx/theme.lua` (new) — default theme window section
- `Source/end/Map.h:197,334-361` — File::config mapped to IDref::end, config::Theme struct added, end::Map members updated with WindowFX
- `Source/end/View.cpp:24-39,57-83` — theme tree listener registration, graphics propagation via sendLookAndFeelChange, theme child detection in valueTreeChildAdded
- `Source/end/Window.h:1-114` — rewritten: lookAndFeelChanged override added, cached glass members removed, doxygen updated for LAF-driven glass
- `Source/end/Window.cpp:1-74` — lookAndFeelChanged reads packed glass from LAF, visual styleParameters removed, blur_style platform-guarded, setStyle simplified
- `Source/lookAndFeel/LookAndFeel.h:70-79` — getWindowGlass() packed Union getter
- `Source/lookAndFeel/LookAndFeel.cpp:13-14,95,97-104,159-181` — ThemeListener removed, Desktop walk deleted, getWindowGlass implementation
- `Source/Identifier.h:35-36,73-74` — theme, themes, windowFx, backgroundColour identifiers added

### Alignment Check
- [x] BLESSED principles followed — Desktop walk (Encapsulation violation) removed from LAF, View owns propagation
- [x] NAMES.md adhered — WindowFX (platform-neutral), Theme (path struct), getWindowGlass (packed Union)
- [x] MANIFESTO.md principles applied — SSOT for window glass values (theme.lua), Lean (packed Union replaces 3 cached members + 4 lambdas)

### Problems Solved
- BackgroundBlur::Backend/fromString duplicate HashMap eliminated — map::WindowFX is SSOT
- Window glass cached-member pattern (3 members + 4 per-property lambdas) collapsed to one packed Union getter
- Desktop walk BLESSED violation in LAF::valueTreePropertyChanged removed — View owns propagation hierarchy
- Platform crash: map::WindowFX::get("blurBehind") on macOS — blur_style registration platform-guarded
- ValueTree::copyPropertiesAndChildrenFrom does not fire valueTreePropertyChanged for child properties — theme detection moved to valueTreeChildAdded
- findColour(backgroundColourId) returning 00000000 — window tint sourced from theme tree directly via getWindowGlass

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 15: LookAndFeel Rendering Context, Model Typed-Array API, Tab Bar Padding ✅

**Date:** 2026-06-13
**Duration:** ~02:30

### Agents Participated
- COUNSELOR: plan, orchestration, verification, root-cause analysis (CSV storage bug)
- Engineer: all code changes across JAM + END
- Pathfinder: typeface registration chain, JUCE font resolution
- Librarian: JUCE Typeface/Font/LookAndFeel API research

### Files Modified (18 total)

**JAM framework:**
- `jam_data_structures/lua/jam_lua_value_tree.h` — isArray(), pack() helpers; isArray branch in both from() overloads; getLua() Array<var> export
- `jam_data_structures/model/jam_model.h` — getInt16/getInt declarations, getValue/setValue doxygen, getValueFromChildWithName→getValueFromChildWithProperty rename
- `jam_data_structures/model/jam_model.cpp` — getInt16/getInt implementations, getValueFromChildWithProperty rename
- `jam_data_structures/value_tree/jam_value_tree_utils.cpp` — getValueFromChildWithProperty rename
- `jam_look_and_feel/jam_look_and_feel_custom.h` — getTabBarPadding() virtual
- `jam_gui/button/jam_button_bar.h` — Orientation enum CSS reorder
- `jam_gui/button/jam_button_bar.cpp` — updateTabPositions contentArea from barPadding, background sized to contentArea

**END project:**
- `Source/lookAndFeel/LookAndFeel.h` — rendering context members (TypefaceResources, Stamp, Grapheme, typefaces HashMap), registerTypeface/getTabBarPadding declarations, doxygen updates
- `Source/lookAndFeel/LookAndFeel.cpp` — registerTypeface() dual jam+juce, getTabBarPadding() via getInt16, getTabPadding reads ID::textPadding, all getDisplay calls→getValue
- `Source/Main.h` — three singleton members + registerTypefaces() + JamFontsBinaryData include removed
- `Source/Main.cpp` — dead registerTypefaces() deleted
- `Source/config/Config.h` — getDisplay/getGraphics declarations deleted
- `Source/config/Config.cpp` — getDisplay/getGraphics implementations deleted, getGraphics() call→getValue, getInitWindowSize via getInt
- `Source/end/MessageOverlay.h` — getDisplay→getValue
- `Source/end/Map.h` — Position enum + bimap CSS reorder, doxygen updated
- `Source/Identifier.h` — X(textPadding, "text_padding") added
- `Source/config/lua/display.lua` — padding→text_padding rename, padding={4,4,4,4} table added

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Typeface portability: embedded fonts now registered for both jam glyph pipeline (FreeType/HarfBuzz) and JUCE LAF name lookup (CoreText Registered set) — FontOptions().withName() resolves without system-installed fonts
- CSV tokenization eliminated: lua flat integer arrays stored as Array<var>, consumed via getInt16/getInt → Union structured binding — no StringArray::fromTokens at call sites
- Tab bar padding not rendering: root cause was lua parser storing {4,4,4,4} as CSV string "4,4,4,4" — var.getArray() returned nullptr; fixed by Array<var> storage
- Responsibility delegation: Application→Maps+Config, LookAndFeel→all rendering context

### Debts Paid
None

### Debts Deferred
None

---

## Sprint 14: Tab Button + Highlight Wired, Deterministic Tab Metrics ✅

**Date:** 2026-06-13
**Duration:** ~05:00

### Agents Participated
- COUNSELOR: orchestration; bank-home design (LAF graphics map as single store, Bar/buttons stateless); sparse 8-slot contract; Graphics-map/ButtonState decoupling; root-cause chase on tab padding inflation (uppercase divergence → overflow stretch → depth-derived measurement font); failure-protocol stop + evidence demand; JUCE API verification (var::operator float, FontOptions::withKerningFactor, TextLayout::getStringWidth, File::hasFileExtension not on String)
- Engineer: JAM state-enum/bank deletion, END loadGraphics/draw wiring, indicator→highlight rename, deploy exists() guard + 10-stem catalog, tab.depth/padding/kerning_factor config keys, getTabText/getTabPadding virtuals, scale-machinery deletion, TextLayout measurement, getTabFont heuristic deletion, Flex proportional getLayout
- ARCHITECT (handcode/design): jam::map::ButtonState registry, tab_highlight.svg + tab_button SVGs, drawTabButton uppercase draft, getTabFont(int) override removal that exposed the jam default heuristic, layout contract definitions (3 iterations), graphics.lua/runtime config

### Files Modified (~16 total)

**JAM:**
- `jam_graphics/svg/jam_svg_button.h` / `.cpp` — SVG::Button::State enum deleted; getState/paint indices via jam::map::ButtonState (project declares instance); `or`/brace-init on touched lines
- `jam_gui/button/jam_button_svg.h` — graphics bank/setGraphics/getSegments deleted; button::SVG = dumb paint delegate (LAF owns graphics)
- `jam_graphics/svg/jam_svg.h` / `.cpp` — getStyledGraphics: unnamed/unmatched elements self-styled; Identifier constructed only for named elements (empty-id juce assert fixed)
- `jam_graphics/svg/jam_svg_flex.h` / `.cpp` — getLayout: uniform scale = targetH/sourceH computed once pre-carve; corners keep aspect ratio, edges stretch adjacent axis at scaled thickness, centre both; area-mutation asymmetry dead
- `jam_gui/button/jam_button_bar.h` / `.cpp` — indicator→highlight rename (SlidingHighlight, highlight member, snapHighlight, animateHighlight); scale machinery + minimumScale + setMinimumTabScaleFactor deleted (natural widths always); getBestTabLength: TextLayout::getStringWidth of getTabText-transformed name + getTabPadding×2, jmax depth clamp + depth param deleted
- `jam_look_and_feel/jam_look_and_feel_custom.h` — drawBarIndicator→drawBarHighlight; getTabFont(int barDepth) depth heuristic + tabFontDepthRatio DELETED → getTabFont() no-arg; new virtuals getTabPadding(font), getTabText(name)

**END:**
- `Source/lookAndFeel/LookAndFeel.h` / `.cpp` — loadGraphics: tab_highlight + sparse 8-slot tab_button bank into graphics map (state-id keys, ButtonState-driven loop); drawBarHighlight + drawTabButton (getState count=8, paint only authored slots, label via getTabText/getTabFont); vtpc reload on IDtype::tabButton; getTabPadding/getTabText overrides; getTabFont + kerning_factor; tabButtonStateCount born and died (sparse contract)
- `Source/Identifier.h` — tabHighlight; 8 tabButton state stems; depth, kerningFactor
- `Source/end/Map.h` — Graphics = 10-stem file catalog (bar, highlight, 8 button states), no privileged state, alignment fixed (COUNSELOR)
- `Source/end/Tabs.h` / `.cpp` — tabFontRatio deleted; bar depth = fontHeight × tab.depth; ValueTree::Listener on config (tab-node edits relayout live)
- `Source/config/Config.cpp` / `.h` — saveToPath raw.exists() guard (no 0-byte deploys; binary-driven rework reverted on ARCHITECT command); stale State-enum comment
- `Source/config/lua/display.lua` — tab.depth, tab.padding, tab.kerning_factor
- `Source/config/lua/graphics.lua` — tab_highlight key; sparse tab_button contract comment (any subset, unset not painted)
- `Source/config/svg/*` — tab_highlight.svg, tab_button SVGs (ARCHITECT)

### Alignment Check
- [x] BLESSED principles followed (graphics SSOT in LAF map; ButtonState/Graphics registries decoupled by purpose; measure==render via shared getTabFont/getTabText; no privileged button state; absence = design choice end-to-end)
- [x] NAMES.md adhered (highlight rename ARCHITECT-directed; depth/padding/kerning_factor keys ARCHITECT-chosen)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Parser asserted on unnamed SVG elements (empty id → juce::Identifier) → self-styled contract covers unnamed/unmatched
- First-N button state contract treated absence as invalid → sparse 8-slot: any subset authored, unauthored skipped, never enforced by logic
- Deploy wrote 0-byte files for non-embedded stems and privileged tab_button_normal → exists() guard + full 10-stem catalog
- Tab padding inflation (longer text more padding) — three real defects peeled: uppercase paint-side only (measure≠render string), overflow branch stretched visible tabs >1×, and the killer: measurement font fell back to jam depth-derived default after ARCHITECT removed END's getTabFont(int) override → heuristic deleted at the root, single getTabFont() SSOT
- 9-slice corners distorted: stretch-to-fit → as-is (wrong) → uniform proportional scale keeping corner aspect
- Bar depth hardcoded ratio → tab.depth config; text pad hardcoded font-height → tab.padding config; kerning → tab.kerning_factor

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 13: ColourMap Distributor From Tree, Flex Segment 9-Slice Complete ✅

**Date:** 2026-06-12
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: orchestration, design discussion (registry shape iterations: vector fan-out → map-to-map → recursive mirror → AnyMap-derived ColourMap built from tree; Segment unit encapsulation; getLayout/StyledGraphics::paint split), fact-checking (caret colourId readers, jam::map::Segment registry, getPath family children-iteration semantics, mapToTarget area-mutation asymmetry), paint draft defect analysis (colourId ignored, source-pixel corners, transformed stroke)
- Engineer: ColourMap + Methods rework (3 iterations), END registry migration, Shape purge + getElementPath restore, Flex Segment/getSegments/getLayout/paint, consumers migration
- ARCHITECT (handcode): SVG parse walk + flatten foundation, paint draft, jam::map::Segment registry, Layout alias, tab_bar.svg authoring

### Files Modified (~14 total)

**JAM:**
- `jam_core/utilities/jam_any_map.h` — storage section private → protected (derived containers)
- `jam_core/identifier/jam_identifier_svg.h` — round/bevel/square IDref entries added
- `jam_graphics/colour_map/jam_colour_map.h` — NEW: jam::ColourMap : AnyMap; fromValueTree (1:1 tree mirror skeleton), recursive getChildWithName (const + non-const)
- `jam_graphics/jam_graphics.h` — colour_map include before SVG section
- `jam_look_and_feel/jam_look_and_feel_custom.h` — Methods: default ctor, protected colourMap, setColourId (recursive find + fill), tree-driven setColours lockstep walk, one-line contains; hand-rolled ColourMap struct + path-reconstruction contains deleted
- `jam_graphics/svg/jam_svg.h` / `.cpp` — Shape purified (bounds/segment dropped; operator== = colourId+colour+stroke); getElementPath single-element geometry SSOT; getRectPath/getEllipsePath/getCirclePath/getAllFoundPath delegate; getStyledGraphics walk simplified (bounds skip, no stamping loops, magic strings → IDref)
- `jam_graphics/svg/jam_svg_flex.h` / `.cpp` — Flex::Segment { id, bounds, graphics }; Segments = array<Segment, 9> (jam::map::Segment indexed); Layout alias; getSegments (registry slotting, bounds via getElementPath); getLayout (pure carving, Value::map depth scaling); StyledGraphics::paint (paint-time findColour, geometry-then-stroke); Flex::paint orchestration ~13 lines
- `jam_gui/button/jam_button_svg.h` — bank → array<Flex::Segments, 8>; getStyledGraphics(int) → getSegments(int)
- `jam_data_structures/map_instance/jam_map_segment.h` — 9-slot bidirectional registry (ARCHITECT)

**END:**
- `Source/lookAndFeel/LookAndFeel.h` — static colourIds registry deleted; graphics member → HashMap<Identifier, SVG::Flex::Segments>
- `Source/lookAndFeel/LookAndFeel.cpp` — ctor: colourMap = fromValueTree(config.state) + 28 flat setColourId lines; caret → jam::CaretComponent::caretColourId (single target, dead pair removed); vtpc contains/setColours + graphics branch (loadGraphics + Desktop repaint); loadGraphics → Flex::getSegments with tab node; drawBarBackground → Flex::paint
- `Source/config/svg/tab_bar.svg` — 9 segment groups, bounds metadata rects, LAF-coloured outline/background groups (ARCHITECT)
- `Source/end/Map.h` — jam::map::Segment instantiation (ARCHITECT)

### Alignment Check
- [x] BLESSED principles followed (tree = hierarchy SSOT, map derived not hand-rolled; Shape = pure draw call; Segment owns layout; getLayout pure/deterministic; paint split per single responsibility)
- [x] NAMES.md adhered (all names ARCHITECT-gated: ColourMap, fromValueTree, setColourId, getChildWithName, Segment, Segments, getSegments, getLayout, Layout)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Nested registry literal was hand-rolled garbage → hierarchy derived from config.state (fromValueTree), values filled by flat setColourId lines
- contains() path-reconstruction walk was garbage → recursive getChildWithName (jam_value_tree_utils semantics), one-line contains
- vector<int> fan-out leaf was insurance for a broken case (both caret targets had zero readers; actual painter jam::CaretComponent reads 0x4100001) → single-int leaves, real target wired
- Flex unit encapsulation wrong: Shape carried bounds (N duplicated copies) + segment (in equality only to block cross-cell merge) → Segment owns id/bounds/flattened graphics; Shape = geometry + draw attributes
- Geometry extraction duplicated verbatim across walk and getPath helpers → getElementPath SSOT restored
- ARCHITECT paint draft defects: LAF shapes painted transparent (colourId ignored), corners carved in source pixels (asset ate the bar), stroke outline transformed (smeared width) → findColour at paint time, Value::map depth scaling, geometry-then-stroke
- Tab bar renders end-to-end: lua → svg → Segments → LAF paint, hot reload via graphics vtpc branch

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 12: button::SVG + StyledGraphics Foundation, Scoped Colour Distributor ✅

**Date:** 2026-06-12
**Duration:** ~08:00

### Agents Participated
- COUNSELOR: orchestration, design discussion (StyledGraphics/Shape semantics, static Flex, kuassa button model, JUCE-native colourId rule, nested registry distributor), fact-checking (juce::Drawable capability matrix, TextButton/TabbedButtonBar ColourIds, jam::ID coverage), trivial fixes (jam::IDtype shadowing, Window background key, duplicate identifier)
- Engineer: font-key renames, SVG module move, Flex rewrite→stubs, jam::button::SVG, Bar rework, colour registry restructure
- Pathfinder: jam button/LAF/colour-consumer surveys
- Librarian: juce::Drawable vs custom StyledGraphics capability research (juce_SVGParser.cpp evidence)

### Files Modified (~30 total)

**JAM:**
- `jam_core/identifier/jam_identifier_svg.h` — font/font_size/font_family entries removed
- `jam_core/identifier/jam_identifier_appearance.h` — fontFamily ("font-family"), fontSize ("font-size") added
- `jam_core/jam_core.h` / `jam_core.cpp` — SVG includes removed (moved to jam_graphics)
- `jam_graphics/svg/jam_svg.h` / `.cpp` — moved from jam_core/xml (byte-verbatim)
- `jam_graphics/svg/jam_svg_button.h` / `.cpp` — moved from jam_core/xml
- `jam_graphics/svg/jam_svg_flex.h` / `.cpp` — REWRITE: instance class deleted; static-only struct — getStyledGraphics (parser STUB), paint (9-slice STUB), getName (implemented)
- `jam_graphics/styled_graphics/jam_styled_graphics.h` — new: StyledGraphics { Shape { path, stroke, colour, colourId, bounds }, shapes, width, height }; attributed_path/ deleted
- `jam_graphics/jam_graphics.h` / `.cpp` — SVG section before graphics utilities (declaration order), styled_graphics include
- `jam_gui/button/jam_button_svg.h` — new: jam::button::SVG : juce::Button — array<StyledGraphics, 8>, setGraphics, getStyledGraphics, getStateCount, paintButton → Custom::drawTabButton
- `jam_gui/button/jam_button_bar.h` / `.cpp` — Tab class DELETED; Orientation + ColourIds (backgroundColourId/outlineColourId/highlightColourId) on Bar; TabInfo holds mouse::Events<SVG>; lambda-wired click/drag-reorder/right-click; getBestTabLength(name, depth) on Bar
- `jam_gui/layout/jam_tabbed_component.h` / `.cpp` — createTabButton type + Orientation references updated
- `jam_gui/jam_gui.h` — jam_button_svg.h include
- `jam_gui/view/jam_view_content.h`, `jam_data_structures/view/jam_view_manager.cpp`, `jam_view_manager_panel.cpp`, `jam_markdown/mermaid/jam_mermaid_svg_parser.cpp` — IDref::font_size/font_family → fontSize/fontFamily
- `jam_markdown/jam_markdown.h` — jam_graphics dependency + include added

**END:**
- `Source/Identifier.h` — IDtype::code added; codeFamily/codeStyle/codeSize, family, indicator, inactiveText, inactiveBackground, selection, foreground, outline, background duplicates removed; caret, highlight, textOn, textOff, buttonOn added
- `Source/Main.cpp` — registerTypefaces reads ID::fontFamily/fontSize from IDtype::code node
- `Source/config/Config.cpp` — getFont→getDisplay(IDtype::code); ID::fontSize (Windows scale)
- `Source/config/lua/display.lua` — key renames: colours.{caret,text,highlight}, tab.{button,button_on,text_off,text_on,highlight}, window.background, overlay.{background,text}
- `Source/config/lua/whelmed.lua` — code_family/code_style/code_size block deleted
- `Source/lookAndFeel/LookAndFeel.h` / `.cpp` — colourIds → nested HashMap<node, HashMap<property, vector<int>>> distributor; scoped setColours; vtpc nested lookup; tab paint stubbed (loadGraphics/drawBarBackground/drawBarIndicator TODO); drawTabButton test fill via TextButton button/buttonOn ids; selectionColourId removed
- `Source/end/MessageOverlay.h` — Label::backgroundColourId/textColourId; font from overlay node (jam::IDtype::overlay)
- `Source/end/Window.cpp` — style dispatcher keyed/reads jam::ID::background (was jam::ID::colour)
- `Source/end/Tabs.cpp` — variable-length tab names retained; no Tab:: references

### Alignment Check
- [x] BLESSED principles followed (registry = SSOT distributor; Flex stateless; button owns its bank)
- [x] NAMES.md adhered (all new names ARCHITECT-gated: StyledGraphics, Shape, setGraphics, highlightColourId)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Flex instance class was unreliable garbage (clone-filter XML at paint, Bucket dedup, bail-outs) → deleted; static parser/painter stubs await ARCHITECT handcode
- Tab button was a LAF-painted lookalike with 3× duplicated 8-state plumbing → jam::button::SVG IS a button (kuassa model); state plumbing collapsed to getState + getStateCount
- Colour collision: flat registry applied bare property names from every node (whelmed.background overwrote TabbedComponent background → transparent tab fill) → nested node-scoped distributor, collision impossible by construction
- juce::Drawable evaluated and rejected on evidence: no paint-time findColour, no collapse, no 9-slice, serif:id ignored
- Invisible window: Window style dispatcher read renamed lua key → jam::ID::background fix
- jam::IDtype shadowing (END IDtype hides jam's) → explicit jam::IDtype:: qualification

### Debts Paid
- None

### Debts Deferred
- None (side finding flagged in-session: registerTypefaces() defined but never called — awaiting ARCHITECT direction)

---

## Sprint 11: SVG Flex Layout System + LAF 1:1 Colour Chain ✅

**Date:** 2026-06-10
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: orchestration, design discussion (Segment/Shape/Flex vocabulary, 1:1 chain, open vs fixed roles, String→Identifier keys, HashMap consolidation), fact-checking (jam SVG API, JUCE ColourIds, XmlElement iteration API)
- Engineer: code implementation (many delegations — jam SVG module, Flex, END LookAndFeel)
- Pathfinder: Phase 3 completion survey, jam SVG module API discovery

### Files Modified (14 total)

**JAM:**
- `jam_core/identifier/jam_identifier_svg.h` — stroke_width, stroke, topLeft, topRight, bottomLeft, bottomRight, outline, foreground entries added to IDENTIFIER_SVG X-macro.
- `jam_core/xml/jam_svg.h` — getElementPath (per-element path reader), parseStyle (generic CSS key-value from style attr), parseColour (hex decoder), Flex forward declaration. getEllipsePath/getCirclePath/getRectPath/getAllFoundPath doxygen updated to reflect delegation to getElementPath.
- `jam_core/xml/jam_svg.cpp` — parseStyle + parseColour implementations. getElementPath (path/rect/ellipse/circle dispatch). getRectPath/getEllipsePath/getCirclePath refactored to delegate to getElementPath. getStrokeWidth refactored onto parseStyle.
- `jam_core/xml/jam_svg_flex.h` — new file: SVG::Flex struct — Shape (path, PathStyle, colourId, colour, strokeWidth), Segment (vector\<Shape\>, bounds), Segments = HashMap\<Identifier, Segment\>, getName, getSegments, paint. Comprehensive doxygen.
- `jam_core/xml/jam_svg_flex.cpp` — new file: getName (serif:id/digit-strip), getSegments (collectShapes lambda walker — LAF groups via colourIds, self-styled via parseStyle+parseColour, bounds rect exclusion via getChildByName), paint (cornerTransform lambda, HashMap\<Identifier, AffineTransform\>, range-for over segments, findColour at paint time).
- `jam_core/jam_core.h` — jam_svg_flex.h include wired.
- `jam_core/jam_core.cpp` — jam_svg_flex.cpp unity include wired.
- `jam_gui/button/jam_button_bar.h` — Tab::ColourIds { backgroundColourId = 0x4200001 }, Bar::ColourIds { indicatorColourId = 0x4200100 }.

**END:**
- `Source/lookAndFeel/LookAndFeel.h` — barBackgroundColourId, frontBackgroundColourId, inactiveBackgroundColourId, frontTextColourId, inactiveTextColourId, tabOutlineColourId, indicatorColourId removed from ColourIds enum. Three Segments members → one flexGraphics HashMap\<Identifier, Segments\>. getTabFont() public accessor. Class + method doxygen rewritten (1:1 chain SSOT).
- `Source/lookAndFeel/LookAndFeel.cpp` — colourIds registry retargeted: background→TabbedComponent::backgroundColourId, outline→tabOutlineColourId, indicator→Bar::indicatorColourId, inactiveBackground→Tab::backgroundColourId, foreground→frontTextColourId, inactiveText→tabTextColourId. frontBackground entry deleted. loadGraphics populates flexGraphics via insert_or_assign. Draw virtuals lookup flexGraphics.at(key). drawTabButton text via TabbedButtonBar ids. vtpc narrowed (Array\<var\> relic deleted). getTabFont() SSOT extracted.
- `Source/end/Tabs.cpp` — updateTabBarVisibility: inline font construction replaced with LAF getTabFont() call.
- `Source/Identifier.h` — ID::frontBackground X-macro entry deleted.
- `Source/config/lua/display.lua` — front_background key + comment deleted from tab section. Outline comment rewritten. 1:1 chain note added.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- SVG string members + cout diagnostic → parsed Segment/Shape cache (then → module-level jam::SVG::Flex)
- Array\<var\> event encoding relic in vtpc → plain property message dispatch
- Full recursive setColours on every vtpc → narrowed: colour key → setColours, SVG key → loadGraphics
- Hand-rolled SVG parsing (resolveGroupName, parseStrokeWidth, buildRolePath) → jam::SVG module APIs (getElementPath, parseStyle, parseColour)
- Inline Graphic struct + pointer-to-member dispatch → HashMap\<Identifier, Segment\> keyed by SVG group name
- Fixed outline/foreground/background role paths → dynamic Shape ops in document order, LAF-aware via colourIds registry
- Six duplicated END ColourIds → retargeted to JUCE TabbedComponent/TabbedButtonBar + jam::button component ids
- Font construction duplicated (Tabs.cpp + LookAndFeel.cpp) → getTabFont() SSOT
- Three separate Segments members → one flexGraphics HashMap
- String-keyed Segments → Identifier-keyed (hashed, interned)
- getRectPath bounds pollution (unions all child rects) → getChildByName single direct-child rect
- SegmentTransform struct + 5-entry array → cornerTransform lambda + HashMap\<Identifier, AffineTransform\>

### Debts Paid
- None

### Debts Deferred
- None

**Date:** 2026-06-10
**Duration:** ~08:00

### Agents Participated
- COUNSELOR: orchestration, design discussion, fact-finding, delegation
- Engineer: code implementation (many delegations)
- Pathfinder: codebase discovery (endless validation/message chain, jam::Map, Function::Map, config tree structure)
- Librarian: embedded Lua API research (lua_sethook, OP_SETFIELD decoding, chunk names, nil semantics, parseSVGPath, jam::SVG/XML)
- Auditor: mid-sprint validation (Map::Instance refactor)

### Files Modified (18 total)

**JAM:**
- `jam_core/map/jam_map.h` — Instance<T>::contains(value) added — O(1) key lookup via Map::getKey inversion.
- `jam_lua/jam_lua_types.h` — LineMap nested type (tag → property → line), LineMapBuilder (result + tableRegisters + pendingRegisters + flushRoot).
- `jam_lua/jam_lua_state.h` — getType(code, chunkName) overload (luaL_loadbufferx "@name" → real filenames in Lua errors). lineHook: LUA_MASKLINE hook decoding OP_NEWTABLE/OP_SETFIELD from savedpc[-1] via vendored internals, register-tracked key→line capture, LineMap* via lua_getextraspace. getLineMap/getLineMapBuilder.
- `jam_data_structures/lua/jam_lua_value_tree.h` — jam::lua::Validators (nested HashMap, tag → property → predicate). from() optional Validators* — registers type validators (int/double/string) during the same build walk. from() validation overload (validators + errors + lineMap) — rejects invalid values during parse, reports missing (nil/undefined) properties, line-numbered error lines via appendError tag+key lookup.
- `jam_data_structures/model/jam_model.h` — fromLua forwarding overloads for both validator modes.

**END:**
- `Source/end/Map.h` — config::File + config::Graphics relocated here as Map::Instance<T> CRTP (canonical location next to Boolean). New maps: GpuMode (auto/true/false), DropMode (space/newline), Position expanded to 5 values (top/bottom/left/right/center). TabOrientation removed — Position enum mirrors jam::button::Tab::Orientation 0–3.
- `Source/Main.h` — Context owners: config::File, config::Graphics, GpuMode, Position, DropMode. tabOrientationMap removed.
- `Source/Identifier.h` — loadMessage added to IDENTIFIER_CONFIG.
- `Source/config/Config.h` — Model API: load → loadFromPath, validators member, registerValidator (try_emplace/insert_or_assign — no bracket), graphicsCallbacks (Function::Map), loadMessage member + getLoadMessage(). Comprehensive doxygen rewrite.
- `Source/config/Config.cpp` — ctor CONTRACT: initialise → saveToPath → loadFromPath → startWatching. enumCheck<MapType> + getEnumValidator static resolvers (result returns, no if/else chain, no second map). loadFromPath: per-file getType with chunk name, flushRoot, validated fromLua, error accumulation, single atomic loadMessage + unconditional sendPropertyChangeMessage. buildGraphicsCallbacks: filename → sendPropertyChangeMessage dispatch built from config values per reload. fileChanged: lua → loadFromPath, svg → graphicsCallbacks direct lookup. All diagnostics removed.
- `Source/end/View.cpp` — messageOverlay enabled, ID::loadMessage → showMessage(config.getLoadMessage()) — message is event not state, no clearing.
- `Source/lookAndFeel/LookAndFeel.h` — drawBarIndicator + drawTabButton overrides, indicatorSegments + buttonSegments, parseTabBarSvg → parseSvg (generic).
- `Source/lookAndFeel/LookAndFeel.cpp` — parseSvg full implementation (role groups via serif:id/suffix-strip, bounds rects, parseSVGPath + addRectangle, style colour extraction). loadGraphics: all three SVGs → segment vectors. Three draw methods: scale-to-fit fill/stroke per segment.
- `Source/config/lua/graphics.lua` — tab_inactive/tab_active filenames set.
- `Source/config/svg/tab_inactive.svg`, `tab_active.svg` — wired through pipeline (BinaryData GLOB → seed → watch → parse → paint).

### Alignment Check
- [x] BLESSED principles followed (B: validators/lineMap/watcher owned by Model, hook lifetime scoped to getType. L: one walk builds tree + validators, one resolver. E: explicit CONTRACT ctor sequence, named registerValidator. S: BinaryData defaults are the single schema — validators derived from it. S: loadMessage is an event, no shadow state cycle. E: JAM knows var types only, END enriches with its Maps. D: same lua → same tree, same errors.)
- [x] NAMES.md adhered (loadFromPath/saveToPath verbs, getEnumValidator result-return resolver, Position/GpuMode/DropMode semantic nouns, no "node" anywhere)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Lua values invalid but silently accepted — validators now built from BinaryData walk (type + enum), invalid values rejected before setValuesFrom, defaults survive.
- Nil/undefined lua values vanish silently (Lua spec: nil removes table key) — missing-property detection against registered validators.
- Same-value setProperty suppressed loadMessage notification — message moved to member + unconditional sendPropertyChangeMessage; message is an event, fires once per loadFromPath, always.
- Validation errors had no source location — LUA_MASKLINE hook decodes OP_SETFIELD from vendored Lua internals, nested LineMap (tag → key → line) eliminates same-key collisions across sections.
- Lua syntax errors showed `[string "..."]` — chunk name overload gives `display.lua:42:` format.
- `position = "center"` rejected by 2-value Position map — Position expanded to 5 values, single map for all positional values, TabOrientation eliminated.
- SVG hot-reload if/else chain — graphicsCallbacks Function::Map direct lookup, rebuilt per reload from live config values.
- idMap parallel-map garbage, flat validator collisions, isLuaValid post-build walk, enrichStringValidators second walk, registerStringEnumValidator template + 4-branch chain — all eliminated through ARCHITECT-led redesign: one walk, one validators map, one resolver.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 9: Tab Bar LookAndFeel Refactor + SVG Pipeline + Config Watcher ✅

**Date:** 2026-06-08
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: orchestration, design discussion, plan, delegation
- Engineer: code implementation (multiple delegations)
- Pathfinder: codebase discovery (button::Bar, LookAndFeel, kuassa patterns, JUCE tab internals)
- Researcher: web research on 9-slice/3-slice rendering models (CSS border-image, Android NinePatch, macOS NSDrawThreePartImage/NinePartImage, Qt QSS, GTK CSS, JUCE capabilities)
- Auditor: mid-sprint validation

### Files Modified (24 total)

**JAM:**
- `jam_look_and_feel/jam_look_and_feel_custom.h` — renamed virtuals: drawButtonGroupTrack → drawBarBackground, drawButtonGroupSlidingIndicator → drawBarIndicator, drawTabButton signature unchanged. Added @brief doxygen per virtual.
- `jam_gui/button/jam_button_bar.h` — added Background nested class (LAF-aware, delegates to drawBarBackground), SlidingIndicator nested class (delegates to drawBarIndicator), background + animator members, snapIndicator/animateIndicator methods. Removed BehindFrontTabComp forward decl + unique_ptr member.
- `jam_gui/button/jam_button_bar.cpp` — removed BehindFrontTabComp class definition. Bar ctor: addAndMakeVisible(background + indicator), mouse-pass-through. addTab: indicator.toBehind(&background). setCurrentTabIndex: animateIndicator() after currentTabChanged. updateTabPositions: background.setBounds + snapIndicator at end. Renamed local `animator` → `desktopAnimator` (shadow fix). Removed behindFrontTab usage. Bar::paint now empty (background component paints itself).

**END:**
- `Source/Identifier.h` — IDENTIFIER_CONFIG: added graphics. IDENTIFIER_COMMON: added inactiveText, frontBackground, inactiveBackground, background (removed foreground — moved to DISPLAY). IDENTIFIER_DISPLAY: added tabBar, tabInactive, tabActive, tabBarSvg removed, outline + foreground added.
- `Source/config/Config.h` — File enum: added graphics. Model: added Watcher::Listener inheritance, fileChanged override, watcher member. Updated doxygen (8 sections, watcher ownership).
- `Source/config/Config.cpp` — File::map: added graphics entry. loadPath: creates graphics/ dir (replaced button/), starts watcher at end. Added fileChanged: .lua → load(), .svg → sendPropertyChangeMessage on matching graphics property.
- `Source/Main.h` — removed Watcher::Listener inheritance, fileChanged declaration, watcher member.
- `Source/Main.cpp` — removed watcher setup (addFolder/coalesceEvents/addListener), removed fileChanged method entirely. Application now config-only.
- `Source/lookAndFeel/LookAndFeel.h` — ColourIds: semantic names (barBackgroundColourId, frontBackgroundColourId, inactiveBackgroundColourId, frontTextColourId, inactiveTextColourId, tabOutlineColourId, indicatorColourId). Removed old IDs (tabBarBackgroundColourId, tabLineColourId, tabActiveColourId, tabIndicatorColourId). Added Segment struct, barSegments, parseTabBarSvg private. Removed loadTabBarSvg, draw3Slice, SlicePaths, buttonSlice/indicatorSlice. drawBarBackground + drawStretchableLayoutResizerBar overrides.
- `Source/lookAndFeel/LookAndFeel.cpp` — colourIds map: all semantic keys (ID::background, frontBackground, inactiveBackground, foreground, inactiveText, outline, indicator). drawBarBackground: empty skeleton (ARCHITECT fills). drawStretchableLayoutResizerBar: unchanged. setColours: unchanged. parseTabBarSvg: empty skeleton. Removed loadTabBarSvg, draw3Slice, SVG path cache, all 3-slice rendering code.
- `Source/config/lua/display.lua` — tab section: semantic keys (background, front_background, inactive_background, inactive_text, outline, indicator). Removed line, active, inactive (replaced), tab_bar_svg (moved to graphics.lua).
- `Source/config/lua/graphics.lua` — NEW: path = "gfx", tab_bar = "tab_bar.svg", tab_inactive = "", tab_active = "".
- `Source/config/lua/end.lua` — added graphics = require("graphics") after display.
- `Source/config/svg/tab_bar.svg` — tab bar SVG with 4 corner groups (top-left, top-right, bottom-left, bottom-right) + 1 flex group, each with outline/foreground role groups + bounds rect. Replaces old 3-slice tab.svg.

### Alignment Check
- [x] BLESSED principles followed (B: RAII ownership of Background/SlidingIndicator/Watcher. L: single-responsibility virtuals. E: semantic colour IDs, jam identifiers for all SVG parsing. S: config::Model SSOT for file watching. S: no shadow state. E: LAF decides how, Component decides what. D: same config → same render.)
- [x] NAMES.md adhered (Segment, Background, SlidingIndicator — semantic nouns. drawBarBackground/drawBarIndicator — verbs. No type suffixes.)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Tab bar background was using tabLineColourId (a "line" colour) for full background fill — wrong semantics. Replaced with dedicated barBackgroundColourId from tab.background config.
- drawButtonGroupTrack/drawButtonGroupSlidingIndicator names were opaque — renamed to drawBarBackground/drawBarIndicator (role-based).
- BehindFrontTabComp (old JUCE V2 pattern) removed — replaced by Background component that delegates to LAF.
- Application::fileChanged coupled Application to LookAndFeel — moved watcher to config::Model, SVG changes routed through sendPropertyChangeMessage on ValueTree.
- button/ subfolder replaced by graphics/ (config contract: config::Model::loadPath creates it).
- 3-slice SVG approach abandoned (wrong model for tab bar that needs both horizontal + vertical orientation). Replaced with 9-slice corner+flex model.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 8: Phase 3 Pane Splits + Navigation + PaneManager Cleanup ✅

**Date:** 2026-06-07
**Duration:** 04:00

### Agents Participated
- COUNSELOR: plan, orchestration, bug diagnosis (focusedPane sync, resizer bar bounds, orphan scan)
- Engineer: identifier additions, SplitDirection map, PaneManager refactor, Panes/View/Tabs implementation, LookAndFeel override
- Pathfinder: codebase discovery (pane/split patterns, PaneManager API, endless tab bar height pattern)
- Auditor: (inline validation during Engineer delegation)

### Files Modified (16 total)

**JAM Framework:**
- `jam_core/identifier/jam_identifier_layout.h:92-98` — added panes, pane, direction, ratio, vertical, horizontal to IDENTIFIER_LAYOUT
- `jam_data_structures/map_instance/jam_map_split_direction.h` — NEW: SplitDirection Map::Instance (vertical/horizontal enum)
- `jam_data_structures/jam_data_structures.h:46` — registered jam_map_split_direction include
- `jam_gui/layout/jam_pane_manager.h:167-172` — NeededBar struct (node + isVertical + bounds), neededBars always tracks all required bars (fixes orphan scan), new bars get setBounds on creation
- `jam_gui/layout/jam_pane_manager.cpp:6-10,23-68,121-165` — removed 5 file-local statics, API takes jam::UUID + Identifier direction, direction comparison via ID::vertical.toString()

**END Project:**
- `Source/Identifier.h:126` — added closePane to IDENTIFIER_KEYS
- `Source/end/Panes.h:39-50` — split/removePane take jam::UUID, added focusPane
- `Source/end/Panes.cpp:17,28-39,57-117` — addLeaf(uuid) direct, split Attachment after layout + toFront, removePane UUID, focusPane bounds-based navigation
- `Source/end/Tabs.h:7,43-44` — config/Config.h include, tabFontRatio constant, updateTabBarVisibility declaration
- `Source/end/Tabs.cpp:10,22-24,37,55-69` — tab bar depth 0 at init, updateTabBarVisibility from config font family+size
- `Source/end/View.cpp:124-183` — 7 action handlers: splitHorizontal, splitVertical, closePane, paneLeft/Right/Up/Down
- `Source/end/PaneView.h` — unchanged (focus mechanism preserved)
- `Source/lookAndFeel/LookAndFeel.h:59-62` — drawStretchableLayoutResizerBar declaration
- `Source/lookAndFeel/LookAndFeel.cpp:87-95` — drawStretchableLayoutResizerBar implementation (paneBarColourId/paneBarHighlightColourId)
- `Source/config/lua/keys.lua:48,82,85` — cmd+w -> close_pane, split shortcuts corrected

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- PaneManager file-local statics promoted to jam identifier system (SSOT)
- PaneManager String-based UUID/direction replaced with typed jam::UUID + Identifier
- Attachment ordering bug: Attachment created before layout caused premature valueTreeChildAdded, overwrote focusedPane to void. Fix: Attachment after layout, toFront after Attachment
- PaneManager orphan scan bug: neededBars only tracked NEW bars, causing orphan scan to remove valid existing bars on each layout pass (odd/even bar visibility). Fix: neededBars tracks all required bars
- PaneManager new bar bounds: bars created during layout had zero bounds until next layout pass. Fix: NeededBar carries bounds, setBounds on creation
- Close hierarchy: cmd+w went straight to close_tab. Now close_pane (pane>tab>quit)
- Split shortcuts inverted: \ was horizontal, - was vertical. Corrected
- Tab bar visible with 1 tab. Now hidden when <=1, shown from config font metrics (endless pattern)

### Debts Paid
None

### Debts Deferred
None

---

## Sprint 7: Hierarchical Attachment + Focus Sync + Model Lean ✅

**Date:** 2026-06-07
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead — hierarchical attachment design, Component/Attachment API, focus chain architecture, UUID design, AudioModel static attach migration
- Engineer: AnyMap::remove, Attachment refactors (3-arg→2-arg→template 1-arg), Component Model& storage, ComponentWithID removal, Tabs/Panes/PaneView rework, jam::UUID creation, Registry diagnostics removal, static attach migration to AudioModel, Identifier cleanup
- Pathfinder: Static attach consumer mapping, Tabs tree management survey
- Librarian: UUID cross-thread usage analysis in endless

### Files Modified — JAM

- `jam_core/utilities/jam_any_map.h` — added `remove(key)` + String overload for atomics cleanup on detach
- `jam_core/misc/jam_uuid.h` — NEW: 64-bit trivially copyable UUID, std::abs positive, atomic-compatible
- `jam_core/jam_core.h` — registered jam_uuid.h include
- `jam_data_structures/model/jam_model.h` — Component: Model& member (public), Property::defaultValue int→juce::var, removed onAttachment, removed virtual on getValueTree. Attachment: template ctor (1-arg, parent auto-discovery), stores Component& only, no Model&/VT members, no getState/registerAtomics/attachRecursively/detachRecursively. Removed ComponentWithID template entirely.
- `jam_data_structures/value_tree/jam_value_tree_utils.cpp` — Component ctor definitions, Attachment destructor (uses component.getValueTree()), removed old Attachment ctors/methods
- `jam_data_structures/model/jam_audio_model.h` — received static attach methods (getRoot, getParent, attach×3, attachChild)
- `jam_data_structures/model/jam_audio_model.cpp` — received static attach implementations
- `jam_gui/view/jam_view_panel.h:43` — Model::attach → AudioModel::attach

### Files Modified — END

- `Source/end/View.h` — removed private Model& (inherited), added valueTreeChildAdded override, model/config as VT members, Tabs init via ctor list
- `Source/end/View.cpp` — ARCHITECT's focus implementation: listens model tree, valueTreePropertyChanged syncs focused_pane on PANE focus change, valueTreeChildAdded syncs on new tab creation
- `Source/end/Tabs.h` — Model::Component type=IDtype::tabs, jam::Owner<Attachment>, removed model member
- `Source/end/Tabs.cpp` — addNewTab creates jam::UUID, Attachment 1-arg, Owner::add/remove, empty currentTabChanged
- `Source/end/Panes.h` — Model::Component (was ComponentWithID), jam::Owner<Attachment>, removed model member
- `Source/end/Panes.cpp` — jam::UUID, Model::Component base (no seeds), setName/setComponentID in body, Attachment 1-arg
- `Source/end/PaneView.h` — Model::Component (was ComponentWithID), jam::UUID param, focusGained/focusLost set PANE focus property, visibilityChanged toFront
- `Source/Identifier.h` — added IDtype::tabs, ID::focusedPane, ID::focus; removed ID::activeTab, ID::activePaneID (dead)
- `Source/action/Registry.cpp` — removed all jam::debug::Log::write diagnostics

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Hierarchical parent→child attachment: each parent owns Attachments for its children (View→Tabs→Panes→PaneView), RAII lifecycle
- Parent auto-discovery: Attachment walks getParentComponent() chain, finds nearest Model::Component, no explicit parent parameter
- Focus chain: PaneView::focusGained sets PANE.focus=1, View listens model tree and syncs VIEW.focused_pane; valueTreeChildAdded handles new tab creation
- Model& plumbing eliminated: Component stores Model& directly, derived classes inherit it
- ComponentWithID CRTP removed: derived classes call setName/setComponentID directly
- Static attach methods moved to AudioModel (audio plugin path only, not END)
- jam::UUID replaces juce::Uuid: 64-bit int64_t, trivially copyable, atomic-compatible, no string allocation for identity

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 6: Tab UX + Model architecture ✅

**Date:** 2026-06-07
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead — tab system rename design, Model/Attachment architecture, PARAM removal, ValueTree absorption, orientation wiring
- Engineer: button::Bar/Tab rename, Owner<TabInfo> migration, drag-reorder, orientation support, Model cleanup (getRootTree/UniqueNodeMap/attach deletion), Attachment recursive graft, PARAM scrapping, accessor rework, CodeView cleanup, dangling declaration removal
- Pathfinder: Tab system survey (END, JAM, kuassa, JUCE), consumer analysis
- Librarian: JUCE ComponentAnimator API, TabBarButton drag patterns, PopupMenu right-click

### Files Modified — JAM (4 new, 10 modified, 4 deleted)

**New (4):**
- `jam_gui/button/jam_button_bar.h` — button::Bar + button::Tab (renamed from TabbedButtonBar/TabBarButton), Owner<TabInfo>, drag-reorder, orientation
- `jam_gui/button/jam_button_bar.cpp` — implementations: drag threshold 5px, moveTab with onTabMoved, axis-aware layout

**Deleted (4):**
- `jam_gui/layout/jam_tabbed_button_bar.h` — replaced by button/jam_button_bar.h
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — replaced by button/jam_button_bar.cpp
- `jam_gui/button/jam_button_group.h` — dead code, replaced by button::Bar
- `jam_gui/button/jam_button_tab.h` — dead code, replaced by button::Tab

**Modified (10):**
- `jam_gui/layout/jam_tabbed_component.h` — uses button::Bar/Tab, setOrientation(int), getOrientation()
- `jam_gui/layout/jam_tabbed_component.cpp` — orientation switch in paint/resized, button::Tab references
- `jam_gui/jam_gui.h` — updated includes: button_bar.h replaces tabbed_button_bar.h, group.h, tab.h
- `jam_gui/jam_gui.cpp` — updated TU includes
- `jam_gui/code_editor/jam_code_view.h` — removed ValueTree::Component/Listener inheritance, stateAttachment, state, properties array, VTPC per SPEC §2.1
- `jam_gui/code_editor/jam_code_view.cpp` — simplified ctor (no VT), deleted VTPC impl, deleted properties array
- `jam_data_structures/model/jam_model.h` — absorbed ValueTree (Component, ComponentWithID, Attachment, all statics); deleted PARAM/addParameter/getValue/setValue/storeValue/loadValue/getRawParameterValue/attach/getRootTree/UniqueNodeMap; added addProperties (isInt64), recursive Attachment, nested AnyMap accessors
- `jam_data_structures/model/jam_model.cpp` — deleted PARAM definition, all old accessors, attach implementations; simplified addTextParameter; dual-tree applyFunctionRecursively in setValuesFrom
- `jam_data_structures/value_tree/jam_value_tree_utils.cpp` — recursive graft/ungraft implementations, registerAtomics delegates to addProperties, dual-tree overload
- `jam_data_structures/value_tree/jam_value_tree_json.cpp` — removed stale includes

### Files Modified — END (7 modified)

- `Source/end/View.h` — takes Model&, owns unique_ptr<Attachment>, applyTabOrientation renamed setTabOrientation
- `Source/end/View.cpp` — recursive attachment in ctor body after addAndMakeVisible, TabOrientation bimap, orientation from config
- `Source/end/Map.h` — TabOrientation bimap (Tab::Orientation enum values), typed get() returning Orientation
- `Source/end/Tabs.h` — jam::Model::Component (was ValueTree::Component)
- `Source/end/PaneView.h` — jam::Model::ComponentWithID (was ValueTree::ComponentWithID)
- `Source/Main.h` — deleted viewAttachment member
- `Source/Main.cpp` — View(model), no viewAttachment
- `Source/end/Window.cpp` — jam::Model::toColour (was ValueTree::toColour)
- `Source/lookAndFeel/LookAndFeel.cpp` — jam::Model::applyFunctionRecursively, jam::Model::toColour
- `Source/config/Config.cpp` — addProperties (was addParametersFromProperties), jam::Model::fromLua
- `Source/config/lua/display.lua` — tab.position renamed to tab.orientation
- `Source/Identifier.h` — added orientation identifier

### Alignment Check
- [x] BLESSED principles followed (SSOT: PARAM shadow eliminated, Bound: recursive Attachment RAII, Encapsulation: Model stops exposing state)
- [x] NAMES.md adhered (addProperties, setTabOrientation, button::Bar/Tab)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Tab system naming: TabbedButtonBar/TabBarButton renamed to button::Bar/Tab, button::Group deleted
- PARAM shadow state: active_tab existed as both node property AND PARAM child — PARAM pattern scrapped entirely
- Model::attach exposed raw state tree — deleted, Attachment is the only graft path
- CodeView had ValueTree infrastructure violating SPEC §2.1 — removed, pure juce::Component
- registerAtomics created PARAM children — now delegates to addProperties (direct property binding)
- ValueTree struct absorbed into Model — single namespace for all state infrastructure
- Recursive Attachment: one construction grafts entire Component hierarchy + registers atomics at every level

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 5: TabbedButtonBar fork + bit_cast + Union + config pipeline ✅

**Date:** 2026-06-06
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead — TabbedButtonBar analysis, bit_cast/Union design, config pipeline redesign, Window style collapse, colour format migration
- Engineer: TabbedButtonBar+TabBarButton fork, TabbedComponent fork, bit_cast, Union, CellFifo replacement, toInt replacement, lua::ValueTree, Window rewrites, LookAndFeel rewrites, Tabs simplification, View ValueTree::Component, colour conversion
- Pathfinder: button::Group internals, TabbedComponent wiring, VTPC flow
- Librarian: JUCE TabbedButtonBar/TabBarButton/TabbedComponent deep dive, juce::var internals, juce::Colour(uint32)
- Researcher: C++17 bit_cast implementation, packed value type patterns, existing library survey

### Files Modified — JAM (8 new, 7 modified)

**New (8):**
- `jam_core/utilities/jam_bit_cast.h` — constexpr __builtin_bit_cast polyfill
- `jam_core/utilities/jam_union.h` — variadic packed transport (uint32/uint64 backing, pack/unpack, structured bindings)
- `jam_gui/layout/jam_tabbed_button_bar.h` — TabBarButton + TabbedButtonBar fork from JUCE
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — layout algorithm verbatim, Custom LAF paint
- `jam_data_structures/lua/jam_lua_xml.h` — moved from jam_lua, getBody rename, Text::numeric, quoted()
- `jam_data_structures/lua/jam_lua_value_tree.h` — moved from jam_lua, typed vars (int64/double), fromValueTree alias

**Modified (7):**
- `jam_core/utilities/jam_toInt.h` — bit_cast replaces C-style union type-pun, removes UB
- `jam_core/jam_core.h` — registered bit_cast, union includes
- `jam_gui/layout/jam_tabbed_component.h` — verbatim JUCE fork (content management, ButtonBar subclass)
- `jam_gui/layout/jam_tabbed_component.cpp` — changeCallback, clearTabs, addTab with content component
- `jam_gui/jam_gui.h` — registered jam_tabbed_button_bar.h
- `jam_gui/button/jam_button_group.h` — static_cast<Custom&> replaces dynamic_cast<Theme*>
- `jam_look_and_feel/jam_look_and_feel_custom.h` — drawButtonGroupTrack, drawButtonGroupSlidingIndicator virtuals
- `jam_terminal/transport/jam_cell_fifo.h` — packHeader/unpackHeader → Union<int32_t, uint8_t>
- `jam_data_structures/value_tree/jam_value_tree.h` — fromLua, toInt, toColour utilities
- `jam_data_structures/jam_data_structures.h` — jam_lua dependency, lua/ includes
- `jam_lua/jam_lua.h` — removed xml/value_tree includes (moved to jam_data_structures)

### Files Modified — END (4 new/rewritten, 8 modified)

**Modified (8):**
- `Source/end/Tabs.h` + `.cpp` — simplified: TabbedComponent manages content, no Owner<Panes>, removal selects next tab
- `Source/end/View.h` + `.cpp` — inherits ValueTree::Component (IDtype::view), owns tabsAttachment
- `Source/end/Window.h` + `.cpp` — removed ValueTree::Component, collapsed to setStyle(property) + registerStyleParameters, inline lambdas
- `Source/lookAndFeel/LookAndFeel.h` + `.cpp` — drawTabButton toggle visual, drawButtonGroupTrack, drawButtonGroupSlidingIndicator, fromRGBA removed, toColour
- `Source/Main.h` + `.cpp` — windowAttachment removed, viewAttachment added
- `Source/Identifier.h` — added size, view identifiers
- `Source/config/Config.cpp` — lua::ValueTree::from replaces Xml::from + fromXml
- `Source/config/lua/display.lua` — size={w,h} packed, all colours 0xAARRGGBB
- `Source/config/lua/whelmed.lua` — all colours 0xAARRGGBB
- `Source/config/lua/end.lua` — format comment updated

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (node→state, getBody, blacklisted words)
- [x] MANIFESTO.md principles applied (SSOT for pack/unpack, Encapsulation for ValueTree ownership)

### Problems Solved
- Tab layout: FlexBox equal-width replaced with JUCE per-button variable width + proportional scaling
- Tab removal: proper index tracking via JUCE's removeTab + setCurrentTabIndex post-removal
- Tab painting: button::Group dynamic_cast<Theme*> → static_cast<Custom&>
- Colour pipeline: eliminated string→hex roundtrip (lua stores 0xAARRGGBB int, var(int64), direct Colour construction)
- Window size atomicity: single CSV property, single VTPC
- toInt UB: C-style union type-pun replaced with bit_cast
- CellFifo ad-hoc memcpy replaced with Union<int32_t, uint8_t>
- Config typed vars: jam::lua::ValueTree::from stores int64/double instead of strings
- Window style dispatch: three layers collapsed to two (registerStyleParameters + setStyle)
- Attachment ownership: View owns tabsAttachment, Window no longer ValueTree::Component

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 4: Phase 3 — Tabs + Panes + action::Registry

**Date:** 2026-06-06
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead, design discussion (Registry architecture, LookAndFeel CRTP pattern, Owner API usage, tab lifecycle), direct edits on Registry/View/Tabs/LookAndFeel
- Engineer: PaneView stub, Panes container, Tabs component, Registry initial impl, View rewrite, LookAndFeel base change + drawTabButton, jam::LookAndFeel::Custom + Methods
- Pathfinder: Codebase survey (TabbedComponent, PaneManager, Owner, Function::Map APIs)
- Librarian: juce::KeyPress::createFromDescription API, juce::Component::getLookAndFeel() internals, JUCE LookAndFeelMethods pattern
- Researcher: LookAndFeelMethods patterns (CRTP, static registration, intermediate base, component-side virtual)

### Files Modified — JAM (1 new, 3 modified)

**New (1):**
- `jam_look_and_feel/jam_look_and_feel_custom.h` — Custom base (LookAndFeel_V4 + custom virtuals), Methods<Derived> CRTP with static_cast get()

**Modified (3):**
- `jam_look_and_feel/jam_look_and_feel.h` — include jam_look_and_feel_custom.h before Theme
- `jam_gui/button/jam_button_tab.h` — paintButton: static_cast<Custom&> replaces dynamic_cast<Theme*>
- `jam_data_structures/value_tree/jam_value_tree.h` — member renamed node→state

### Files Modified — END (7 new, 7 modified)

**New (7):**
- `Source/end/PaneView.h` — stub: juce::Component + ValueTree::ComponentWithID<PaneView>, UUID ctor
- `Source/end/Panes.h` — per-tab pane container: PaneManager + Owner<PaneView> + Owner<PaneResizerBar>
- `Source/end/Panes.cpp` — split, removePane, layout delegation
- `Source/end/Tabs.h` — TabbedComponent + ValueTree::Component, owns Owner<Panes>
- `Source/end/Tabs.cpp` — addNewTab (counter-named), removeCurrentTab (quit on last), currentTabChanged (state + visibility)
- `Source/action/Registry.h` — std::hash<KeyPress> injection, prefix key state machine, config listener
- `Source/action/Registry.cpp` — buildKeyMap from config KEYS (createFromDescription), run via Function::Map

**Modified (7):**
- `Source/end/View.h` — KeyListener + ValueTree::Listener, owns Tabs + Registry
- `Source/end/View.cpp` — registers actions via registry.actions.add([this]...), setSize from config, first tab
- `Source/end/Map.h` — removed Modifier/KeyName bimaps (replaced by juce::KeyPress::createFromDescription)
- `Source/Main.h` — tabsAttachment member, removed Modifier/KeyName contexts
- `Source/Main.cpp` — tabsAttachment wiring
- `Source/lookAndFeel/LookAndFeel.h` — inherits jam::LookAndFeel::Methods<LookAndFeel>, drawTabButton override
- `Source/lookAndFeel/LookAndFeel.cpp` — drawTabButton (rounded rect + text), setColours fixed: applyFunctionRecursively recursive walk

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [ ] JRENG-CODING-STANDARD.md — multiple violations corrected mid-sprint (bail-out guards, hand-rolled parsers, manual booleans, forward declarations, stored pointer shadow state)

### Problems Solved
- LookAndFeel custom virtuals without dynamic_cast: jam::LookAndFeel::Custom + Methods<Derived> CRTP pattern — static_cast, zero runtime cost
- setColours broken (searched one level deep): replaced with applyFunctionRecursively recursive walk
- fromRGBA channel rotation was no-op: ARCHITECT fixed rotation order
- Boolean config parsing: end::Boolean::get() bimap replaces broken static_cast<int> on string vars
- Key binding parsing: juce::KeyPress::createFromDescription replaces hand-rolled parseKeyBinding
- Registry prefix key state machine: Timer IS the state, no manual isPrefixActive boolean
- Tab removal crash: reordered removeTab before tabPanes.remove (currentTabChanged fires while tabPanes intact)

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 3: Phase 3 — Config Infrastructure + Listener-Driven Window

**Date:** 2026-06-05 — 2026-06-06
**Duration:** Full session (2 days, context compaction mid-session)

### Agents Participated
- COUNSELOR: Sprint lead, design discussion (config overlay, styleParameters pattern, fromString refactor), fromRGBA bug diagnosis, direct verification of all engineer output
- Engineer: BackgroundBlur Backend expansion (jam), jam::Model::setValuesFrom, jam::Window setGlass 3-arg, end::Window + styleParameters, config::Model redesign, LookAndFeel listener, Identifier.h IDENTIFIER_BACKEND, lua config renames, call site updates (ModalWindow, GlassComponent, Dialog, Theme)
- Pathfinder: Codebase survey (kuassa glass machinery, ProcessorChain::parameters pattern, jam::Function::Map contract, config tree structure)

### Files Modified — JAM (2 new, 1 deleted, 25 modified)

**New (2):**
- `jam_core/identifier/jam_identifier_window.h` — X-macro IDENTIFIER_WINDOW (11 keys: mac, win, blurStyle, 4 macOS backends, 4 Windows backends)
- `jam_core/identifier/jam_identifier_terminal.h` — relocated from jam_terminal/identifier/

**Deleted (1):**
- `jam_terminal/identifier/jam_identifier_terminal.h` — moved to jam_core

**Modified (25):**
- `jam_style/background_blur/jam_background_blur.h` — 8-value Backend enum, shouldTintComponent, fromString (table-driven unordered_map + IDref), isGlassFXAvailable/isMicaAvailable/isAcrylic11Available, new Windows DWM constants
- `jam_style/background_blur/jam_background_blur.mm` — macOS platform: 4-case switch in enable(), applyBackgroundBlur, applyVisualFX, applyGlassFX (NSGlassEffectView), disable() with NSGlassEffectView cleanup
- `jam_style/background_blur/jam_background_blur.cpp` — Windows platform: 4-case switch in enable(), applyBlurBehind, applyAcrylic10, applyAcrylic11, applyMica, getWindowsBuildNumber delegation
- `jam_core/utilities/jam_platform.h` — extracted getWindowsBuildNumber(DWORD, cached), isWindows10 uses it
- `jam_core/identifier/jam_identifier.h` — includes jam_identifier_window.h, MAKE_VIEW includes IDENTIFIER_WINDOW
- `jam_data_structures/model/jam_model.h` — new public setValuesFrom(ValueTree), private overlay(ValueTree&, const ValueTree&)
- `jam_data_structures/model/jam_model.cpp` — setValuesFrom: isEquivalentTo early-out, per-property diff overlay. Removed debug log blocks
- `jam_gui/window/jam_window.h` — setGlass(Colour, float, Backend) 3-arg, setShowWindowButtons(bool), members: tintColour, blurRadius, glassBackend
- `jam_gui/window/jam_window.cpp` — setGlass shouldTintComponent branch, parentHierarchyChanged/visibilityChanged/handleAsyncUpdate pass stored Backend
- `jam_gui/window/jam_modal_window.h` — Backend as 6th ctor param
- `jam_gui/window/jam_modal_window.cpp` — setupWindow forwards Backend to setGlass
- `jam_gui/button/jam_button_dialog.h` — Dialog::show passes platform-default Backend
- `jam_gui/window/jam_glass_component.cpp` — handleAsyncUpdate passes platform-default Backend
- `jam_look_and_feel/theme/jam_look_and_feel_theme.cpp` — preparePopupMenuWindow passes platform-default Backend
- `jam_data_structures/value_tree/jam_parameter.h` — minor
- `jam_data_structures/value_tree/jam_parameter_text.h` — minor
- `jam_data_structures/value_tree/jam_value_tree.h` — minor
- `jam_lua/jam_lua_function.h` — minor
- `jam_lua/jam_lua_object.h` — minor
- `jam_lua/jam_lua_stack.h` — minor
- `jam_lua/jam_lua_state.h` — minor
- `jam_lua/jam_lua_types.h` — minor
- `jam_lua/jam_lua_xml.h` — minor
- `jam_terminal/jam_terminal.h` — include path update for relocated identifier
- `jam_terminal/tty/jam_tty.h` — minor
- `jam_terminal/tty/jam_tty.cpp` — minor
- `jam_terminal/video/jam_video.h` — minor

### Files Modified — END (2 new, 7 deleted, 11 modified)

**New (2):**
- `Source/endWindow.h` — end::Window : jam::Window + ValueTree::Listener, styleParameters (jam::Function::Map), registerStyleParameters/setStyle/applyStyleFor
- `Source/endWindow.cpp` — 8 styleParameter registrations (colour, blurRadius, alwaysOnTop, buttons, width, height, mac, win), ProcessorChain::parameters pattern verbatim

**Deleted (7):**
- `Source/config/lua/default_actions.lua` — renamed to actions.lua
- `Source/config/lua/default_display.lua` — renamed to display.lua
- `Source/config/lua/default_end.lua` — renamed to end.lua
- `Source/config/lua/default_keys.lua` — renamed to keys.lua
- `Source/config/lua/default_nexus.lua` — renamed to nexus.lua
- `Source/config/lua/default_popups.lua` — renamed to popups.lua
- `Source/config/lua/default_whelmed.lua` — renamed to whelmed.lua

**Modified (11):**
- `Source/Identifier.h` — IDENTIFIER_BACKEND X-macro (11 keys), integrated into END_MAKE_VIEW
- `Source/config/Config.h` — load(File, String&) overlay signature, loadPath returns StringArray
- `Source/config/Config.cpp` — build() seeds from BinaryData, load() uses setValuesFrom, loadPath() accumulates errors
- `Source/Main.h` — includes endWindow.h, window member std::unique_ptr<end::Window>
- `Source/Main.cpp` — initialise: Window reads own style, fileChanged loads single file only
- `Source/lookAndFeel/LookAndFeel.h` — setColours private, fromRGBA public static, config member with addListener/removeListener
- `Source/lookAndFeel/LookAndFeel.cpp` — fromRGBA channel rotation fix (RRGGBBAA → ARGB), setColours iterates colourIds map
- `Source/EndView.h` — simplified (transparent content)
- `Source/EndView.cpp` — paint() empty
- `SPEC.md` — blur_style config documentation
- `END.ode` — version bump

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced (no bail-out guards, positive nesting, no captures in lambdas following ProcessorChain pattern)

### Problems Solved
- Config hot-reload was destructive (removeChild + appendChild) — redesigned to overlay-in-place via jam::Model::setValuesFrom with per-property diff
- Listener disconnection on tree reassignment — solved by never reassigning state, only mutating properties
- Duplicate children on reload — solved by property-only mutation (no appendChild in load path)
- "Fire-once glass" — end::Window now a ValueTree::Listener, reacts to every config change
- fromString if/else chain for BackgroundBlur::Backend — replaced with table-driven unordered_map + jam_identifier_window.h X-macro
- fromRGBA channel rotation bug — was a no-op (identity function), fixed to correctly rotate RRGGBBAA → ARGB
- Rvalue reference mismatch in Function::Map::get<ValueTree> — fixed with std::move on lvalue args
- const qualifier loss with forwarding references — fixed by dropping const on lambda params and local vars

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 2: Phase 2 — jam_gui Tab System Rewrite + PaneManager Fix

**Date:** 2026-06-05
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead, plan writing, audit processing, direct fixes
- Engineer: Steps 1-7 implementation, audit finding fixes (F1-F17)
- Auditor: Final audit (24 findings)
- Pathfinder: Codebase survey (jam_gui, kuassa button::Group)

### Files Modified (18 total)

**Deleted (6):**
- `jam_gui/layout/jam_tab_bar_button.h` — old TabBarButton removed
- `jam_gui/layout/jam_tab_bar_button.cpp` — old TabBarButton impl removed
- `jam_gui/layout/jam_tabbed_button_bar.h` — old TabbedButtonBar removed
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — old TabbedButtonBar impl removed
- `jam_gui/layout/jam_tabbed_component.h` — old TabbedComponent replaced
- `jam_gui/layout/jam_tabbed_component.cpp` — old TabbedComponent replaced

**Created (4):**
- `jam_gui/button/jam_button_options.h` — popup menu button, forked from kuassa
- `jam_gui/button/jam_button_tab.h` — TabButton with drag-reorder + inline rename
- `jam_gui/layout/jam_tabbed_component.h` — new TabbedComponent backed by button::Group
- `jam_gui/layout/jam_tabbed_component.cpp` — new TabbedComponent implementation

**Modified (8):**
- `jam_gui/jam_gui.h` — include order: button section before TabbedComponent, added Options + TabButton
- `jam_gui/jam_gui.cpp` — added TabbedComponent .cpp TU include
- `jam_gui/button/jam_button_group.h` — isFreeButton param, index API (getCurrentIndex/setCurrentIndex/removeButton/moveButton), right-click callback, buttons private with accessors (getButtonCount/getButtonAt/getButtonNames)
- `jam_gui/layout/jam_pane_resizer_bar.h` — Base class `juce::Component` → `mouse::Events<juce::Component>`. Removed forward declaration `class PaneManager;`, naked `PaneManager*` member, submodule include, mouseDown/mouseDrag overrides. `mouseDownPos` public (transient drag state for PaneManager callbacks)
- `jam_gui/layout/jam_pane_resizer_bar.cpp` — Removed submodule includes, mouseDown/mouseDrag implementations. Constructor drops `PaneManager*` parameter
- `jam_gui/layout/jam_pane_manager.h` — layout() non-static with resizer bar reconciliation (create/prune on layout, RAII-bound to split node), extracted storeBoundsProperties/findMatchingBar/layoutSplitNode helpers. Removed submodule include. Bar creation wires mouse::Events callbacks (onMouseDown/onMouseDrag), C++17 structured binding capture fix
- `jam_look_and_feel/theme/jam_look_and_feel_theme.h` — drawTabButton virtual, drawThreeSlice, setTabSVG/setTabSVGElementIds, 6 SVG slice members, tab ColourIds, extracted drawConnectedButtonBackground/drawStandaloneButtonBackground helpers
- `jam_look_and_feel/theme/jam_look_and_feel_theme.cpp` — 3-slice SVG infrastructure, drawTabButton fallback rendering, drawButtonGroupSlidingIndicator SVG path, alternative token cleanup (not/and/or), drawButtonBackground Lean decomposition

**Also modified (pre-existing fix):**
- `jam_data_structures/model/jam_model.cpp:333-349` — removed 4 redundant juce::String explicit instantiations (setValue x2, storeValue, loadValue)

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced (alternative tokens, bail-out guards, Lean decomposition, encapsulation)

### Problems Solved
- Old tab system (TabbedButtonBar/TabBarButton) replaced with button::Group-backed TabbedComponent
- PaneManager resizer bar lifecycle: bars now RAII-bound to split nodes via layout() reconciliation — orphan bars pruned, missing bars created automatically
- tabColours SSOT divergence on addTab/moveTab — colours now reorder with buttons
- getContentArea() bug — was returning tab strip instead of content area
- drawButtonBackground Lean violation (66→31 lines) — extracted connected/standalone helpers
- layoutNode Lean violation (103→~25 lines) — extracted storeBoundsProperties/findMatchingBar/layoutSplitNode
- Alternative token violations throughout jam_look_and_feel_theme.cpp — all !/ &&/ || replaced with not/and/or
- Group::buttons encapsulation — moved to private, added getButtonCount/getButtonAt/getButtonNames accessors
- Bail-out guards in setTabSVG — restructured to positive nesting
- Pre-existing jam_model.cpp warnings — removed redundant explicit instantiations after specializations
- PaneResizerBar naked `PaneManager*` pointer (BLESSED Bound) — base class changed to `mouse::Events<juce::Component>`, PaneManager wires callbacks
- PaneResizerBar circular dependency — broken by removing PaneManager dependency entirely
- Forward declaration `class PaneManager;` (JRENG forbidden) — eliminated
- Submodule include violations in resizer_bar.h/.cpp and pane_manager.h — all removed

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 1: Phase 1 — jam_terminal Extraction + SharedResource Redesign

**Date:** 2026-06-04
**Duration:** Full session

### Agents Participated
- COUNSELOR: Led extraction planning, PLAN authoring, identifier categorization, SharedResource redesign discussion, audit remediation. All decision gates with ARCHITECT.
- Pathfinder: Video event-firing site enumeration (33 sites, 28 distinct hooks across 7 files). Function::Map contract discovery.
- Engineer: Steps 1-9 implementation, SharedResource redesign, compile error fixes, pre-existing BLESSED violation fixes, doxygen remediation.
- Auditor: Step 1 audit (2 rounds), final sprint audit (38-item checklist, 12 findings).

### Files Modified — JAM (32 new, 11 modified)

**jam_core (2 modified)**
- `jam_core/utilities/jam_shared_resource.h` — Full redesign: split into `SharedResource` (non-template polymorphic entry base with virtual `operator==`/`hash()`) and `SharedResources<Derived>` (single-param CRTP container with `Owner<SharedResource>` storage). Methods use `const SharedResource&` params and `auto&` returns for CRTP deferred lookup.

**jam_graphics (11 modified, 3 new)**
- `jam_graphics/detail/jam_char.h` — Added 6 static methods (`fromCodepoint`, `width`, `isWordChar`, `isCombining`, `graphemeSegmentationStep`, `graphemeSegmentationInit`). Added private bit-layout constants (`BIT_WIDTH_SHIFTED`, `BIT_IS_COMBINING`, `BIT_IS_WORD_CHAR`, `BIT_GRAPHEME_SEG_PROPERTY`, `WIDTH_FIELD_BITS`, `GRAPHEME_SEG_PROPERTY_BITS`, `BOOL_FIELD_BITS`, `WIDTH_SHIFT`). Removed `GraphemeSegmentationResult` struct (moved to `Grapheme::SegmentationResult`).
- `jam_graphics/detail/jam_charset.cpp` — NEW. DEC line-drawing table + `Char::fromCodepoint` body.
- `jam_graphics/detail/jam_char_props.cpp` — NEW. charPropsT1/T2/T3 tables + `Char::charPropsFor`/`width`/`isWordChar`/`isCombining` bodies.
- `jam_graphics/detail/jam_grapheme_seg.cpp` — NEW. graphemeSegT1/T2/T3 tables + `Char::graphemeSegmentationStep` body.
- `jam_graphics/detail/jam_grapheme.h` — `Grapheme : SharedResources<Grapheme>`. Nested `Entry : SharedResource` (was top-level `GraphemeEntry`). Nested `SegmentationResult` (moved from `jam_char.h`). Added `@file` doxygen.
- `jam_graphics/detail/jam_stamp.h` — `Stamp : SharedResources<Stamp>`. Nested `Entry : SharedResource` (was top-level `StampEntry`). Widened `flags` `uint8_t`→`uint16_t`. Added `juce::Colour underline`, 3-bit underline style field, `OVERLINE`/`SUPERSCRIPT`/`SUBSCRIPT` bits. Removed old `UNDERLINE` single bit. Explicit constructor (virtual base breaks aggregate init).
- `jam_graphics/detail/jam_row.h` — `cells[]` → `chars[]` rename.
- `jam_graphics/jam_graphics.h` — Include order: `jam_grapheme.h` before `jam_char.h`.
- `jam_graphics/jam_graphics.cpp` — Aggregator: replaced `jam_char.cpp` with 3 split TUs.
- `jam_graphics/fonts/font/glyph/jam_glyph.cpp:31-32` — `0x01`/`0x02` → `Stamp::BOLD`/`Stamp::ITALIC`.
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement_shape.cpp:33-34` — Removed duplicate `sgrBold`/`sgrItalic` locals.
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h` — `uint8_t style` → `uint16_t style`.
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics.h` — `const uint8_t* styles` → `const uint16_t* styles`.
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics_cells.cpp` — `uint8_t` → `uint16_t` for style, `UNDERLINE_STYLE_MASK` test.
- `jam_graphics/fonts/typeface/jam_typeface.h` — `Typeface : SharedResource`. Added `hash()` override. `operator==` signature updated.
- `jam_graphics/fonts/typeface/jam_typeface_resources.h` — `SharedResources<TypefaceResources>`. Added `@file` doxygen.
- `jam_gui/code_editor/jam_caret_component.h:137` — Brace-init fix for widened `StampEntry`.

**jam_terminal (32 new)**
- `jam_terminal/jam_terminal.h` — Module header. Deps: `jam_core`, `jam_graphics`, `jam_data_structures`, `juce_core`, `juce_data_structures`, `juce_gui_basics`.
- `jam_terminal/jam_terminal.cpp` — Aggregator: 16 sub-TU includes.
- `jam_terminal/identifier/jam_identifier_terminal.h` — `IDENTIFIER_TERMINAL(X)` X-macro, 53 identifiers in `jam::terminal::ID::`.
- `jam_terminal/cell/jam_palette.h` — 256-slot mutable palette with `setPaletteColour`/`palette256At`.
- `jam_terminal/video/jam_screen.h` — `Screen : jam::Map::Instance<Screen>` CRTP.
- `jam_terminal/video/jam_winsize.h` — Terminal dimensions.
- `jam_terminal/video/jam_video.h` — Video base class (1743 lines). Ctor preserved: `Video(dims, events)`. 33+ fire sites renamed `id::` → `jam::terminal::ID::`.
- `jam_terminal/video/jam_video.cpp` — Core Video: constructor, flush, scroll, print, reset.
- `jam_terminal/video/jam_video_csi.cpp` — CSI dispatch + DECRQSS/DECRQM.
- `jam_terminal/video/jam_video_esc.cpp` — ESC dispatch.
- `jam_terminal/video/jam_video_sgr.cpp` — SGR dispatch. RFC-missing: underline styles/color, overline, super/subscript.
- `jam_terminal/video/jam_video_mode.cpp` — DEC mode handling.
- `jam_terminal/video/jam_video_edit.cpp` — Screen edit ops.
- `jam_terminal/video/jam_video_osc.cpp` — OSC dispatch. RFC-missing: OSC 4/10/11.
- `jam_terminal/video/jam_video_oscext.cpp` — OSC 8/133/1337.
- `jam_terminal/video/jam_video_dcs.cpp` — DCS/APC payload.
- `jam_terminal/video/jam_video_ops.cpp` — Cursor primitives, tab stops.
- `jam_terminal/parser/jam_csi.h` — CSI parameter accumulator.
- `jam_terminal/parser/jam_dispatch_table.h` — VT state machine dispatch table.
- `jam_terminal/parser/jam_parser.h` — Parser DFA.
- `jam_terminal/parser/jam_parser.cpp` — Process loop.
- `jam_terminal/parser/jam_parser_action.cpp` — Action dispatch.
- `jam_terminal/transport/jam_cell_fifo.h` — Lock-free cell transport. Bail-out guards refactored to positive nesting.
- `jam_terminal/keyboard/jam_keyboard.h` — Keyboard encoding.
- `jam_terminal/keyboard/jam_keyboard.cpp` — Windows keyboard encoder.
- `jam_terminal/tty/jam_tty.h` — TTY base class.
- `jam_terminal/tty/jam_tty.cpp` — TTY drain loop.
- `jam_terminal/tty/jam_unix_tty.h` — Unix PTY.
- `jam_terminal/tty/jam_unix_tty.cpp` — Unix PTY implementation.
- `jam_terminal/tty/jam_windows_tty.h` — Windows ConPTY. Decoupled from `lua::Engine` (ctor takes `const juce::File& conptyDir`).
- `jam_terminal/tty/jam_windows_tty.cpp` — Windows ConPTY implementation.
- `jam_terminal/protocol/jam_vt_vocabulary.h` — 166 named VT protocol constants.

**jam CMakeLists**
- `jam/CMakeLists.txt:25` — `jam_tui` → `jam_terminal`.

### Files Modified — END (4 total)
- `CMakeLists.txt:113` — `jam_tui` → `jam_terminal` in `JAM_MODULES`.
- `Source/Main.h` — `ENDApplication` with `public:` access specifier.
- `Source/Main.cpp` — `ENDApplication` fixes (semicolon, class name).
- `PLAN-jam-terminal-extraction.md` — Written and updated through session. Locked decisions, SharedResource redesign notes, `static_assert` drop.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Extracted jam_terminal as reusable JUCE module (32 files, ~13K lines) — VT engine decoupled from END.
- Redesigned SharedResource into 2-type architecture: `SharedResource` (polymorphic entry base) + `SharedResources<Derived>` (single-param CRTP container). Entry types nest inside their owner. No top-level `GraphemeEntry`/`StampEntry`.
- Resolved C++ CRTP chicken-and-egg: method signatures use `SharedResource&`/`auto&`, bodies use deferred `Derived::Entry` lookup. `Owner<SharedResource>` polymorphic storage with virtual dispatch for hash/equality.
- Absorbed CharProps/Charset/CharPropsData into `jam::Char` static methods with TU-static lookup tables split into 3 files by table family.
- Widened `StampEntry::flags` to `uint16_t` with underline color, 3-bit underline style, overline/super/subscript.
- Consolidated 166 scattered VT protocol magic numbers into named `constexpr` constants in `jam_vt_vocabulary.h`.
- Decoupled WindowsTTY from `lua::Engine` (ctor takes `const juce::File& conptyDir`).
- Fixed 4 pre-existing BLESSED violations (magic numbers in jam_glyph.cpp, duplicate sgrBold/sgrItalic, CellFifo bail-out guards, Font::styleFlags investigation).

### Debts Paid
- None

### Debts Deferred
- None

### Known Residual (ARCHITECT-visible, not deferred)
- F7: Forward declaration `class Video;` in `jam_parser.h:78` — JRENG standard forbids forward decls; submodule zero-include rule prevents the alternative. Structural tension, PLAN-acknowledged.
- F8: `jam_screen.h:46` uses plain anonymous `enum` (Map::Instance convention) — JRENG requires `enum class`. Pre-existing pattern tension.
- F10: Multiple files exceed L (Lean) 300-line limit — data-dense/protocol-faithful files (Video.h 1743, WindowsTTY.cpp 1718, Keyboard.h 911, etc.).

---

## Sprint 0: END Rewrite — SPEC + ARCHITECTURE

**Date:** 2026-06-04
**Duration:** Full session

### Agents Participated
- COUNSELOR: Led SPEC writing through dialogue with ARCHITECT. Read all RFCs, old ARCHITECTURE.md, old DEBT.md, PLAN-WHELMED.md, jam source (PaneManager, Typeface, glyph pipeline, Markdown parser, SpectrumProcessor, TETRIS.md, identifier system, button::Group). Verified CONTRACT alignment.
- Pathfinder: Initial codebase survey (old END — 155 files, 46K LOC, 7 subsystems, 6 debts, last 20 commits all refactoring).
- Librarian: JUCE focus system research (getCurrentlyFocusedComponent, hasKeyboardFocus, focusOfChildComponentChanged, FocusChangeListener, focus on hide/remove/overlay — all from JUCE source with line citations).

### Files Modified (2 total)
- `SPEC.md` — Complete rewrite specification v0.0.1. 15 phases, full architecture, META-MVC, APVTS-analog, anti-mental-model, coordinate spaces, performance targets, incremental Model tracking per phase.
- `ARCHITECTURE.md` — Architectural contracts and mental model. Pre-implementation — no file details, contracts only.

### Alignment Check
- [x] BLESSED principles followed (each section references specific BLESSED pillars)
- [x] NAMES.md adhered (all new names discussed and approved: end::Model/View, config::Model, terminal::Controller/Model/View/Processor, PaneView, CodeView::Selection)
- [x] MANIFESTO.md principles applied (lock-free, unidirectional, SSOT, no shadow state, TETRIS contract for CodeView)

### Decisions Locked
1. **Priority order:** JUCE GUI app first, VT emulator second, niceties third.
2. **APVTS analog:** Spectrum analyzer pattern — reader pushes, message thread paints.
3. **META-MVC:** Recursive MVC layers, not three god objects.
4. **Plugin mapping:** Nexus=Host, Controller=AudioProcessor, Model=APVTS, View=PluginEditor, CellFifo=SpectrumFIFO, CodeModel=outputDB.
5. **Two independent trees:** config::Model (config constants) + end::Model (runtime state). Never mixed.
6. **Config SSOT:** Lua files on disk. config::Model is derived state. Init and reload are the same code path. No referTo.
7. **CodeView TETRIS contract:** Dumb widget, cell-space API, NOT jam::ValueTree::Component. Selection TYPE on TABS, selection COORDS transient in CodeView.
8. **Three coordinate spaces:** Video-grid, Document, Screen/pixel. jam::Cell::Point::fromPixel/toPixel is the ONLY converter. Manual arithmetic forbidden.
9. **Keyboard centralized at end::View** (KeyListener), mouse per-PaneView (JUCE delivery).
10. **activePaneID on TABS** authored by end::View (FocusChangeListener). Async delivery guarded.
11. **DisplayCallbacks eliminated.** Lua actions dispatch through action::Registry. Parameterized dispatch required.
12. **TTY moved to jam_terminal.** Constructor takes config path, no app coupling.
13. **Identifiers:** IDENTIFIER_TERMINAL X-macro in jam_terminal, expanded into jam::ID. Video event keys eliminated (virtual hooks replace string-keyed dispatch). END-specific identifiers in AppIdentifier.h.
14. **PaneManager resizer bar fix:** RAII-bound lifetime in Phase 2 (jam_gui).
15. **Namespace structure:** end:: (app), config:: (config), terminal:: (terminal), whelmed:: (markdown). Main.cpp not namespaced.
16. **Nexus ownership:** Nexus owns Controllers. Controllers survive View destruction (daemon mode). Minimal working Nexus in Phase 3.
17. **action::Registry functional in Phase 3** with prefix key state machine and keys.lua parsing.
18. **Whelmed two-pass pipeline:** jam::Markdown::Parser (proven, unchanged) → ParsedDocument IR → style resolution → jam::String with PROPORTIONAL Char → CodeModel → CodeView (edit) / TextView (read).
19. **Font/atlas GL-thread binding:** UNRESOLVED open seam. Must be designed before Phase 4.
20. **Anti-mental-model:** Explicit negation of terminal scanline model. Buffer<Row> is scratch, NOT document. CodeModel is SSOT. Width enters once at projection. Reader NEVER touches CodeModel.

### Problems Solved
- Identified root cause of old END's architectural rot: terminal-first mental model fighting JUCE.
- Identified font/atlas GL-thread race condition (use-after-free on reload) as principal blocker of old END.
- Identified PaneManager resizer bar lifecycle bug (RAII violation in remove()).
- Identified DisplayCallbacks as layer violation (config parser holding UI closures).
- Identified CodeView as jam::ValueTree::Component as layer violation (generic widget coupled to END state schema).

### Debts Paid
- None (Sprint 0 — specification only, no code)

### Debts Deferred
- None
