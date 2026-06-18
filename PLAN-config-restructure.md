# PLAN: config restructure — config::Directory generalization

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-06-18
**BLESSED Compliance:** verified (C++/JUCE = reference impl, no LANGUAGE.md overrides)
**Language Constraints:** C++17 / JUCE / JAM. All BLESSED principles enforced as written.

---

## Context

`config::Theme` and `config::Shader` do the same job — pull files from a user-selectable
subdirectory into a live `ValueTree` and pipe them to a consumer (Theme→LookAndFeel SVG,
Shader→`shader::Controller` GLSL). But the lifecycle is **split**: seeding (`saveToPath`),
watching (`startWatcher`), and `.svg` dispatch (`fileChanged`/`graphicsCallbacks`) live in
`config::Model`, while `Theme::load`/`Shader::load` only do the load half. Neither sub-model
is self-contained, and the two diverge in shape (Theme = validated lua children; Shader =
raw string properties). The `config::File` bimap is over-nested (`File::Theme`,
`File::Shaders::Buffer`) and carries a dead `config` enum entry gated out at every use
(`if (key != File::config)`), plus a dead `Buffer`/`sourcePass` pair orphaned by the
deleted shader Compiler. The config-section file/type name `end` collides semantically with
the app namespace and the runtime `end::Model`.

**Outcome:** one self-contained base `config::Directory : jam::Model` owning the four-phase
lifecycle; `Theme`/`Shader` derive and override only policy; flat de-nested registries;
`end`→`init` rename; dead code removed. `shader::Controller` and the renderable-build in
LookAndFeel are untouched — config pipes source only.

## Locked Decisions (ARCHITECT-approved)

1. **Base** `config::Directory : jam::Model`; `Theme`/`Shader` derive, override load/seed policy only.
2. **Lifecycle = existing four-phase contract**, same method names, run via `load(name)`:
   `initialise()` → `saveToPath()` → `loadFromPath()` → `startWatcher()` → notify.
3. **Each Directory owns its own watcher**. `config::Model` watches only the root lua files.
4. **config pipes source only** — LAF keeps `loadGraphics` (SVG→`Flex::Segments`), Controller
   keeps GLSL compile. config fires `sendPropertyChangeMessage`; consumers build renderables.
5. **Registries flat plural**: `config::File {init,popups,keys}`, `config::Themes {theme,whelmed}`,
   `config::Shaders {common,bufferA..D,image}`. Models stay singular.
6. **Drop** the `config` File entry + both `if (key != File::config)` gates.
7. **Rename** `end`→`init`: `end.lua`→`init.lua`, tree type `END`→`INIT`, retarget config-section
   `IDtype::end` reads. `end::Model`'s own `IDtype::end` (Model.cpp:8) stays.
8. **Remove dead** `Shaders::Buffer` + `sourcePass` + doc refs. **Keep** `iChannel0..3` identifiers.

## OOTB Mandate (NO MANUAL HANDROLL where jam provides — blocking finding)

| Operation | Use | Replaces |
|---|---|---|
| Property iteration | `jam::Model::forEachProperty` (jam_model.h:183) | `for(i; getNumProperties)` (Config.cpp:72–80) |
| Recursive walk | `jam::Model::applyFunctionRecursively` (jam_model.h:139/153) | manual recursion |
| Create dir if absent | `jam::File::getOrCreateDirectory` (jam_file.h:16) | `if(!exists()) createDirectory()` (Config.cpp:90,120,286) |
| Lua→tree + errors | `jam::Model::fromLua` (jam_model.h:200) | manual State/parse |
| State overlay | `jam::Model::setValuesFrom` (jam_model.h:459) | manual merge |
| var decode | `toInt/toColour/toStringArray/getInt16/getInt` | manual casts / CSV split |
| File watch | `jam::File::Watcher` (jam_file_watcher.h) | any poll/timer |
| Registry | `jam::Bimap` + `jam::Instance` | hand-rolled int↔string |

**No jam OOTB** (juce:: canonical): "write BinaryData file if absent"
(`existsAsFile`+`replaceWithData`+`BinaryData::Raw`), read/write string. The repeated seed
pattern collapses to **one** registry-driven `config::Directory` method (DRY/SSOT) — never per-site.

## Validation Gate

Each step validated by `@Auditor` before the next: compliance with MANIFESTO.md (BLESSED),
NAMES.md, `~/.carol/JRENG-CODING-STANDARD.md`, and the Locked Decisions. Refactor-rewrite:
delete old before writing new; inter-step compiler breakage expected and correct.

## Steps

### Step 1: Bimap.h + Identifier.h — flat registries, rename, dead-code removal
**Scope:** `Source/Bimap.h`, `Source/Identifier.h`
Split `File::Theme`/`File::Shaders` → top-level `config::Themes`/`config::Shaders`; `File` enum
`{init,popups,keys}` (drop `config`); delete `Shaders::Buffer`+`sourcePass`; update `end::Map`;
Identifier.h remove stale Buffer doc ref (keep `iChannel0..3`).

### Step 2: config::Directory base — new files
**Scope:** new `Source/config/Directory.h/.cpp`
`Directory : jam::Model`; `load(name)` runs four-phase + notify. Common seed (one method,
`getOrCreateDirectory` + BinaryData) + watcher. Virtual `initialise`/file→tree/`fileChanged`.

### Step 3: Theme/Shader as Directory subclasses
**Scope:** `Source/config/Config.h/.cpp`
Theme: Themes registry, lua-parse, BinaryData seeds, svg machinery absorbed, fires `ID::theme`.
Shader: Shaders registry, raw load, no seeds, fires `IDtype::shaders`. Preserve public `state`.

### Step 4: Shrink config::Model to the root
**Scope:** `Source/config/Config.h/.cpp`
Remove theme/shader/svg machinery; keep root four-phase + validators; drop both gates;
internal `IDtype::end`→`IDtype::init`; keep `getTheme/getShader` + drives `load(name)`.

### Step 5: Lua rename + consumer retarget
**Scope:** `Source/config/lua/end.lua`→`init.lua`; `View.cpp`, LAF/`end` EventRegistration.cpp
Retarget config-section `IDtype::end`→`IDtype::init` (View.cpp:21,44,99; LAF ER:87; end ER:53).
Do NOT touch `end::Model` (Model.cpp:8) or `IDtype::shaders`.

## BLESSED Alignment

- **B** each Directory owns `watcher` (RAII); ownership traceable from `end::Application`.
- **L** new base ≤300; one seed method kills 4× dup; dead `Buffer`/`config` removed; lookup over loops.
- **E (Explicit)** semantic names; assert + positive nesting; no bail-outs; jam APIs named explicitly.
- **S (SSOT)** registries single-source paths; seed logic once; no shadow state.
- **S (Stateless)** Directory is told `load(name)`; never asks config.
- **E (Encap)** config pipes via property-change; LAF/Controller build renderables; bimap reused.
- **D** binary defaults guarantee valid tree before disk I/O (§1.5); overlay deterministic.

## Verification (ARCHITECT runs builds — agents never build)

Launch END; config loads, tab SVGs render, shader background loads when set. Edit `init.lua`
(theme) → hot swap; edit `.svg` → element repaints; edit shader source → Controller reloads.
Fresh `~/.config/end/` re-seeds `init.lua`. `ninja doxygen` zero warnings.

## Risks

- **SPEC.md doc-lag (non-blocking):** SPEC §2 references `display.lua`/sol2; code uses
  `end.lua`/jam::lua. Plan follows code ground truth + SPEC §1.5. SPEC sync out of scope unless directed.
- **BinaryData rename:** `init.lua` needs a rebuild to regenerate the resource (ARCHITECT owns builds).
