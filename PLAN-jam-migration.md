# PLAN: END Migration to JAM (JRENG! Architectural Modules)

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-04-19
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE (LANGUAGE.md: reference implementation, no overrides)

---

## Status (2026-04-20)

| Phase | Status |
|-------|--------|
| Phase 0 — JAM Build Helper | Complete |
| Phase 1 — JAM Consolidation | Complete |
| Phase 2 — Repoint TIT | Complete |
| Phase 3 — Repoint END toolchain + source (Phases 3.0–3.5) | Complete |
| Phase 3.6 — Functional parity testing | Ongoing — parity observed during daily usage |
| Phase 4 — Delete `end/modules/jreng_*` | Complete |

---

## Overview

Migrate END from `jreng_*` modules in `end/modules/` to `jam_*` modules in `~/Documents/Poems/dev/jam/` (JAM = JRENG! Architectural Modules — Single Source of Truth for foundation modules across all C++/JUCE projects). JAM consolidates the most-advanced design from each fork (kuassa-base for architectural patterns, jam/jreng deltas absorbed). End state: END at 100% functional parity on JAM, with `jreng_*` directory deleted.

---

## Language / Framework Constraints

C++17/JUCE per LANGUAGE.md is reference implementation. No overrides. All BLESSED principles enforced as written. JRENG-CODING-STANDARD.md applies to all new module code (alternative tokens, `.at()` access, no early returns, no anonymous namespaces, brace init, `noexcept` where applicable).

JUCE module declaration dependencies are not auto-resolved at consumer level — every consumer must `juce_add_module()` each declared dep.

---

## Methodology (locked by ARCHITECT)

1. **Stage 1 (Phases 0–1):** Build/merge/fork everything END needs into `jam/` — `jreng_*` untouched on disk
2. **Stage 2 (Phases 2–3):** Repoint TIT (one line), repoint END toolchain + source — `jreng_*` still present but unlinked
3. **Stage 3 (Phase 3 validation):** Build and verify 100% functional parity vs HEAD
4. **Stage 4 (Phase 4):** Delete `end/modules/jreng_*` only after parity confirmed

Non-destructive throughout — no `jreng_*` deletion until ARCHITECT signs off on parity.

---

## Validation Gate

Each step MUST be validated before proceeding. Validation = @Auditor confirms compliance with:
- MANIFESTO.md (BLESSED)
- NAMES.md (no improvised names; `jam_` prefix uniformly; `jam::` namespace; no `jm` alias)
- ~/.carol/JRENG-CODING-STANDARD.md
- The locked PLAN decisions below (no scope drift, no unsanctioned design change)

**Locked PLAN decisions (immutable without ARCHITECT approval):**
- Universal `jam_` prefix; namespace `jam::` only; no `jm` alias
- `jam_core` is GPU-aware foundation (kuassa-base architectural patterns); declares `juce_opengl` as hard module dependency (TIT will add `juce_opengl` link in Phase 2)
- `kuassa_data_structures` plugin-domain classes (AquaticPrime, ParameterManager, Registry, ViewModel, ViewManager) STAY in kuassa — only `StyleManager` ports to `jam_data_structures` (Pathfinder-confirmed clean port: zero plugin-domain deps; only depends on `juce_gui_basics` + `kuassa_core` foundation utilities)
- `Map::Instance` adopts kuassa CRTP design `Instance<Derived>` — no debug log restoration in `contains()`
- `jam_markdown` absorbs mermaid; declares `jam_javascript` dependency
- `jam_animation` — foundation slice only: `Animator` + `Base` + `ScrollingText` (skip `Logo`, `Sequencer`)
- `BackgroundBlur` lives in `jam_graphics/blur/` (kuassa pattern, BLESSED Single-Responsibility — blur is a graphics concern, factored out of window machinery)
- `jam_gui` — kuassa-base architecture (Style::Window factored out of blur, GLRoot+setRenderables Apple Silicon fix, BackgroundBlur isEnabled/setEnabled atomics, ModalWindow caller-ctor) + jreng compat features preserved (BackgroundBlur::Type enum, GlassWindowDelegate setCloseCallback, setNativeSharedContext setter, isUserResizing/resizeStart/resizeEnd, WindowsTitleBarLookAndFeel, applyBackgroundBlur opacity coupling)
- `jam_fonts` — port from `kuassa_fonts` verbatim (Pathfinder-confirmed foundation-grade: pure binary asset wrapper, OpenSans + Segment, zero AudioProcessor coupling)
- `jam_look_and_feel` — kuassa-base, depends on `jam_data_structures::StyleManager` + `jam_fonts` + `jam_graphics`. END's `Terminal::LookAndFeel` UNTOUCHED this sprint (END migration to jam_look_and_feel is future work)
- `Terminal::ModalWindow` — Option 2 (shrink wrapper). `jam::ModalWindow`'s caller-ctor absorbs setupWindow + sequencing; wrapper retains items 2/4/5/6/7 (Display::onRepaintNeeded wiring, paint border, keyPressed, corner constants, Config injection)

---

## Steps

### Phase 0 — JAM Build Helper

#### Step 0.1: Create `jam/Jam.cmake` thin helper

**Scope:** new file `~/Documents/Poems/dev/jam/Jam.cmake`

**Action (@Engineer):**
- Create `Jam.cmake` providing two functions/macros:
  - `jam_set_root()` — resolves `JAM_ROOT` from `$ENV{JAM_ROOT}` if defined, else falls back to `$ENV{HOME}/Documents/Poems/dev/jam`. Caches result in `JAM_ROOT` cache variable.
  - `jam_add_modules(target <module> [<module> ...])` — for each module name, calls `juce_add_module("${JAM_ROOT}/${module}")` and appends the module to the target's `target_link_libraries(... PRIVATE ...)` list.
- File header doc-comment: usage example, single-source-of-truth contract.

**Validation (@Auditor):**
- File at `~/Documents/Poems/dev/jam/Jam.cmake`
- BLESSED: SSOT (single point of JAM_ROOT resolution), Explicit (env var name and fallback path documented in header)
- NAMES.md: `jam_set_root`, `jam_add_modules` (verb-prefixed snake_case CMake convention)
- No magic paths — fallback derived from `$ENV{HOME}` only

---

### Phase 1 — JAM Consolidation

**Methodology for every module port:** kuassa-base where most-advanced; absorb jam/jreng deltas; rename files and namespace strictly to `jam_*` / `jam::` only; preserve internal include relative paths; preserve existing module `.h` declaration block format; declare deps explicitly in module header.

#### Step 1.1: Consolidate `jam_core`

**Scope:** `~/Documents/Poems/dev/jam/jam_core/` (existing — full rewrite/merge in place)

**Action (@Engineer):**
1. **Architectural base = `kuassa_core`** — copy structure verbatim, rename `kuassa_*` → `jam_*` filenames, namespace `kuassa::` → `jam::`, remove `namespace ku = kuassa` alias (no `jm` alias).
2. **Preserve from `jam_core` (existing):**
   - `file/jam_file_watcher.{cpp,h}` — keep verbatim
   - `jam_core.mm` — keep
   - CoreServices OSX framework declaration in module header
3. **Absorb from `jreng_core`:**
   - `concurrency/jreng_mailbox.h` → `concurrency/jam_mailbox.h` (rename + namespace)
   - `concurrency/jreng_snapshot_buffer.h` → `concurrency/jam_snapshot_buffer.h`
   - `utilities/jreng_taper.h` → `utilities/jam_taper.h` — change `struct Map : public jreng::Map::Instance` to `struct Map : public jam::Map::Instance<Map>`; update constructor `: jreng::Map::Instance(*this)` → `: jam::Map::Instance<Map>(*this)`; preserve `getDefault()` override
4. **Adopt kuassa `map.h` CRTP design verbatim** — `Instance<Derived>` inherits `Context<Derived>`. NO debug log restoration in `contains()` (ARCHITECT-confirmed: original log violated fast-fail).
5. Module header `jam_core.h`:
   - Declared deps: `juce_core, juce_data_structures, juce_graphics, juce_opengl`
   - OSX framework: `CoreServices`
6. **Rename all `KUASSA_*` / `JRENG_*` macros** (e.g., `KUASSA_DECLARE_*`) to `JAM_*` analogs.

**Validation (@Auditor):**
- All files prefixed `jam_`, all classes in `namespace jam`, no `kuassa::`/`jreng::`/`ku::`/`jm::` references
- `Map::Instance<Derived>` CRTP design present; non-template `Instance` deleted
- `Taper::Map` builds against new CRTP base (compile check via TIT or scaffold)
- BLESSED: SSOT (one core), Encapsulation (no leaked types), Explicit (no anonymous namespaces, alternative tokens)
- JRENG-CODING-STANDARD.md: brace init, `.at()` for containers, `noexcept` where applicable, no early returns, `not`/`and`/`or` tokens
- NAMES.md: no improvised names; all renamed names trace 1:1 from source

#### Step 1.2: Port `StyleManager` into `jam_data_structures`

**Scope:** `~/Documents/Poems/dev/jam/jam_data_structures/style_manager/`

**Pathfinder-confirmed:** clean port, zero plugin-domain deps. StyleManager sits OUTSIDE the `JUCE_MODULE_AVAILABLE_juce_audio_processors` guard in `kuassa_data_structures.h`. Only depends on JUCE primitives (juce::String, juce::XmlElement, juce::Typeface::Ptr, juce::FontOptions, juce::Colour, juce::Image, juce::LookAndFeel) + kuassa_core utilities (Context, XML, Map, String, Image, debug, identifier) — all of which are in jam_core after Step 1.1.

**Action (@Engineer):**
1. Copy `~/Documents/Poems/kuassa/___lib___/kuassa_data_structures/style_manager/kuassa_style_manager.{h,cpp}` to `~/Documents/Poems/dev/jam/jam_data_structures/style_manager/jam_style_manager.{h,cpp}`.
2. Rename namespace `kuassa::` → `jam::`. Replace `ku::` qualifier (if used internally) with `jam::`.
3. Update `jam_data_structures.h` to include the new style_manager headers (mirroring the kuassa unity-build pattern — style_manager files have no explicit `#include` directives; they rely on module-header chain).
4. Confirm declared module deps in `jam_data_structures.h` include `juce_gui_basics` (StyleManager touches `juce::LookAndFeel`); add if missing.

**Validation (@Auditor):**
- `jam::Style::Manager` accessible from `jam_data_structures`
- Zero references to plugin-domain kuassa types in `jam_data_structures`
- `jam_data_structures.h` declares `juce_gui_basics` + `jam_core` deps
- BLESSED + JRENG standards as Step 1.1

#### Step 1.3: Port `jam_freetype`

**Scope:** new `~/Documents/Poems/dev/jam/jam_freetype/`

**Action (@Engineer):**
1. Copy `~/Documents/Poems/dev/end/modules/jreng_freetype/` to `~/Documents/Poems/dev/jam/jam_freetype/`.
2. Rename module header `jreng_freetype.h` → `jam_freetype.h` (and `.cpp`).
3. Rename JUCE module declaration block: `ID:` → `jam_freetype`, `name:` updated.
4. **Vendored FreeType C source files** — DO NOT rename. They are upstream BSD code; identifier rewrites would break upstream compatibility. Only rename the JUCE module wrapper layer.
5. Rename any wrapper-layer C++ classes/namespaces from `jreng::` to `jam::` (FreeType C code untouched).

**Validation (@Auditor):**
- Module declaration block has `ID: jam_freetype`
- Wrapper compiles against jam namespace
- FreeType vendored C files unchanged (file count + headers identical to jreng_freetype source)

#### Step 1.4: Port `jam_javascript`

**Scope:** new `~/Documents/Poems/dev/jam/jam_javascript/`

**Action (@Engineer):**
1. Copy `~/Documents/Poems/dev/end/modules/jreng_javascript/` to `~/Documents/Poems/dev/jam/jam_javascript/`.
2. Rename file headers, class names, namespace `jreng::` → `jam::`.
3. **Resource assets** (`mermaid.min.js`, `mermaid.html`) — preserve verbatim, including filenames. They are external libraries, not JAM identifiers.
4. JUCE module declaration: `ID: jam_javascript`, deps `juce_core` (and any others originally declared in jreng_javascript).

**Validation (@Auditor):**
- Module declaration `ID: jam_javascript`
- `mermaid.min.js` and `mermaid.html` byte-identical to jreng_javascript source
- All wrapper classes in `namespace jam`

#### Step 1.5: Consolidate `jam_markdown` (absorb mermaid)

**Scope:** `~/Documents/Poems/dev/jam/jam_markdown/` (existing — additive)

**Action (@Engineer):**
1. Verify existing `jam_markdown` content matches jreng_markdown's CommonMark/GFM core (per Pathfinder survey, identical except prefix).
2. **Absorb `mermaid/` subtree from `jreng_markdown`:**
   - Copy `mermaid/jreng_mermaid_extract.{h,cpp}`, `mermaid/jreng_mermaid_parser.{h,cpp}`, `mermaid/jreng_mermaid_svg_parser.{h,cpp}` into `jam_markdown/mermaid/`.
   - Rename files `jreng_mermaid_*` → `jam_mermaid_*`.
   - Namespace `jreng::Mermaid::` → `jam::Mermaid::`.
   - Update internal includes between mermaid files.
3. Update `jam_markdown.h` module header: declare new dependency `jam_javascript`.
4. Update `BlockType::Mermaid` enum if present in jam_markdown (verify it exists; if not, port from jreng).

**Validation (@Auditor):**
- `jam::Mermaid::Parser` exported from `jam_markdown`
- `jam_markdown.h` declares `jam_javascript` dependency
- `BlockType::Mermaid` present in jam_markdown's parser block enum
- BLESSED: jam_javascript dep is explicit (no transitive surprise)

#### Step 1.6: Port `jam_graphics`

**Scope:** new `~/Documents/Poems/dev/jam/jam_graphics/`

**Action (@Engineer):**
1. Copy `~/Documents/Poems/dev/end/modules/jreng_graphics/` to `~/Documents/Poems/dev/jam/jam_graphics/`.
2. Rename all files `jreng_*` → `jam_*`, namespace `jreng::` → `jam::`.
3. **BackgroundBlur lands in `jam_graphics/blur/`** (ARCHITECT-locked, per kuassa pattern):
   - Port `kuassa_graphics/blur/background_blur/kuassa_background_blur.{h,mm,cpp}` → `jam_graphics/blur/background_blur/jam_background_blur.{h,mm,cpp}` (rename + namespace).
   - Port `kuassa_graphics/blur/implementations/` (vImage.h, vImage_macOS14.h, float_vector_stack_blur.h, gin.h, ipp_vector.h) → `jam_graphics/blur/implementations/` (preserve filenames where they reference upstream libraries; rename only kuassa-authored headers).
   - Absorb jreng compat features: `BackgroundBlur::Type` enum (caller blur-strategy choice), `GlassWindowDelegate` ObjC `NSWindowDelegate`, `setCloseCallback(std::function<void()>)`. Source: `jreng_gui/window/jreng_background_blur.{h,cpp,mm}`.
   - kuassa's `isEnabled()` / `setEnabled()` atomics retained.
   - kuassa's `applyAccentPolicy()` Windows path retained.
   - kuassa's `isWindows10()` branch + DWM 10/11 split retained.
4. Update module header `ID: jam_graphics`, preserve declared deps + add `juce_opengl` if missing.
5. Resource assets (font files, shaders) preserved verbatim.

**Validation (@Auditor):**
- Module `ID: jam_graphics`
- All non-asset code in `namespace jam`
- BackgroundBlur location decision deferred to ARCHITECT (open question raised, Step 1.8 blocked until resolved)

#### Step 1.7: Create `jam_animation` (foundation slice)

**Scope:** new `~/Documents/Poems/dev/jam/jam_animation/`

**Pathfinder-confirmed:** `jreng::Animator` lives in `end/modules/jreng_gui/animation/jreng_animator.h` (header-only struct, two `toggleFade` static overloads). When `jreng_gui` deletes in Phase 4, all 5 END call sites (MessageOverlay.h ×4, ActionList.cpp ×1) reference `jam::Animator` from `jam_animation`.

**Action (@Engineer):**
1. Create new module `jam_animation` from `~/Documents/Poems/kuassa/___lib___/kuassa_animation/` — selective port:
   - Port: `kuassa_toggle_fade.{h,cpp}` (Animator), `Animation::Base`, `kuassa_scrolling_text.{h,cpp}` (ScrollingText)
   - Port: any common utility headers required by the above (e.g., `common/`, `utilities/perlin_noise.h` if depended on by ScrollingText — verify before porting; do NOT port if unused)
   - SKIP: `Animation::Logo` (Kuassa-brand specific), `Animation::Sequencer` (empty stub), `fade/`, `strip/`, `waves/`, `scrambled_text/`
2. Rename files + namespace `kuassa::` → `jam::`.
3. Module deps in `jam_animation.h`: `juce_gui_basics`, `juce_opengl`, `jam_graphics`.
4. **Verify `jam::Animator::toggleFade` signature compatibility** with the 5 END call sites — kuassa Animator and jreng Animator are both single-file structs with `toggleFade` overloads; if signatures diverge, document the diff for Phase 3.5 to adapt.

**Validation (@Auditor):**
- Module `ID: jam_animation` declares correct deps
- Only foundation classes ported (no Logo/Sequencer/brand types)
- BLESSED Lean: every ported file justified by Animator/Base/ScrollingText API
- `jam::Animator::toggleFade` signature compat note recorded if jreng/kuassa diverge

#### Step 1.8: Consolidate `jam_gui`

**Scope:** new `~/Documents/Poems/dev/jam/jam_gui/`

**Action (@Engineer):**
1. **Architectural base = `kuassa_gui`** — copy structure, rename + namespace `kuassa::` → `jam::`. Remove `namespace ku = kuassa` alias.
2. **Apple Silicon CALayer fix — preserve verbatim:**
   - `GLRoot` zero-sized sentinel `GLComponent` in `Window` (`kuassa_window.h:216` and constructor add at `kuassa_window.cpp:57-59`)
   - `setRenderables` tree-walker in `attachRendererToContent()` (`kuassa_window.cpp:104-136`)
   - Strict order: `setComponentPaintingEnabled(true)` → `setRenderables(...)` → `attachTo(*content)`
   - Documentation comment naming the bug family (Electron/Tauri/Ghostty/Alacritty) preserved (`kuassa_window.h:180-218`)
3. **Preserve from kuassa:**
   - `Style::Window::apply` / `applyMenu` (chrome factored out of blur) — relocate from `kuassa_look_and_feel/style_window/` to `jam_gui/style_window/` per BLESSED Single-Responsibility (Window chrome is GUI concern, not L&F concern; this also breaks the `kuassa_look_and_feel` dep loop)
   - `ModalWindow::shouldDismissOnLostFocus`
   - `ModalWindow` caller-ctor `(content, caller, renderer)`
   - `parentHierarchyChanged()` chrome re-sync on peer creation (`kuassa_window.cpp:182-188`)
   - **BackgroundBlur lives in `jam_graphics/blur/`** (per Step 1.6) — `jam_gui` depends on it, does not own it. References to BackgroundBlur in jam_gui code use `jam::BackgroundBlur` from jam_graphics.
4. **Absorb from jreng_gui (compat preservation for END):**
   - `Window::setNativeSharedContext(void*)` setter (kuassa has only the getter)
   - `Window::isUserResizing()` / `resizeStart()` / `resizeEnd()` resize-tracking state
   - `WindowsTitleBarLookAndFeel` inline `LookAndFeel_V4` subclass (END's tab bar relies on title-bar suppression)
   - **NOTE:** BackgroundBlur::Type enum, GlassWindowDelegate, setCloseCallback already absorbed into jam_graphics in Step 1.6. jam_gui inherits these via dep on jam_graphics.
5. **Caller-ctor design** — add to `jam::ModalWindow` a constructor matching the shape `Terminal::ModalWindow` needs:
   - `ModalWindow(std::unique_ptr<Component> content, Component& centreAround, std::unique_ptr<GLRenderer> renderer, void* nativeSharedContext, std::function<void()> dismissCallback)`
   - Body executes `setNativeSharedContext` → `setRenderer` → centreAroundComponent → setGlass → setVisible → enterModalState
   - This absorbs `Terminal::ModalWindow::setupWindow` + items 1+3 of the original wrapper (consumed in Step 3.4)
6. Module deps in `jam_gui.h`: `juce_events`, `juce_gui_extra`, `juce_opengl`, `jam_animation`, `jam_graphics`, `jam_data_structures` (Style::Window references `jam::Style::Manager` — verify dep is needed; declare if so).
7. **DO NOT depend on `jam_look_and_feel`** — Style::Window relocation breaks that dep. Verify clean break.

**Validation (@Auditor):**
- All preserved kuassa engineering present (GLRoot, setRenderables, Style::Window, atomics, caller-ctor, parentHierarchyChanged, DWM branch)
- All preserved jreng compat features present (Type enum, GlassWindowDelegate, setNativeSharedContext setter, resize tracking, WindowsTitleBarLookAndFeel, blur opacity coupling)
- `jam_gui` declares no `jam_look_and_feel` dependency
- BLESSED: SSOT (one Window family), Single-Responsibility (Style::Window in gui not L&F), Bound (RAII for delegate, renderer ownership)
- JRENG standards as Step 1.1
- Apple Silicon fix doc-comment present, names bug family

#### Step 1.9: Port `jam_fonts`

**Scope:** new `~/Documents/Poems/dev/jam/jam_fonts/`

**Pathfinder-confirmed:** foundation-grade. Pure binary asset wrapper (OpenSans + Segment fonts), zero AudioProcessor coupling, depends on `juce_graphics` + `kuassa_core`.

**Action (@Engineer):**
1. Copy `~/Documents/Poems/kuassa/___lib___/kuassa_fonts/` to `~/Documents/Poems/dev/jam/jam_fonts/`.
2. Rename: `kuassa_fonts.{h,cpp}` → `jam_fonts.{h,cpp}`; `open_sans/kuassa_fonts_open_sans.{h,cpp}` → `open_sans/jam_fonts_open_sans.{h,cpp}`; `segment/kuassa_fonts_segment.{h,cpp}` → `segment/jam_fonts_segment.{h,cpp}`.
3. Namespace `kuassa::Fonts::` → `jam::Fonts::`. Preserve sub-namespaces `OpenSans` and `Segment`.
4. **Binary TTF data** — preserve verbatim (binary asset arrays untouched; only the C++ wrapper renames).
5. Module deps in `jam_fonts.h`: `juce_graphics`, `jam_core`.

**Validation (@Auditor):**
- Module `ID: jam_fonts`
- `jam::Fonts::OpenSans::getResourceByFilename` and `jam::Fonts::Segment::getResourceByFilename` accessible
- TTF binary arrays byte-identical to kuassa source
- BLESSED Lean: pure asset wrapper, no logic introduced

#### Step 1.10: Create `jam_look_and_feel`

**Scope:** new `~/Documents/Poems/dev/jam/jam_look_and_feel/`

**Action (@Engineer):**
1. Copy `~/Documents/Poems/kuassa/___lib___/kuassa_look_and_feel/` to `~/Documents/Poems/dev/jam/jam_look_and_feel/` MINUS `style_window/` (already relocated to jam_gui in Step 1.8).
2. Rename files + namespace `kuassa::` → `jam::`.
3. **Plugin-coupled classes** (`Knob_V1` with `findParentComponentOfClass<juce::AudioProcessorEditor>`) — leave the cast in place for this sprint; flag in Risks for ARCHITECT review. END does not consume `jam_look_and_feel` this sprint, so the coupling is dormant.
4. Module deps in `jam_look_and_feel.h`: `jam_graphics`, `jam_fonts`, `jam_data_structures` (for `jam::Style::Manager`).

**Validation (@Auditor):**
- Module `ID: jam_look_and_feel`, deps declared explicitly
- `jam::LookAndFeel::Theme` consumes `jam::Style::Manager` from `jam_data_structures`
- `style_window/` NOT present (relocated to jam_gui)
- BLESSED: dependency direction L&F → graphics + fonts + data_structures only (no upward dep, no L&F → gui)

---

### Phase 2 — TIT Compatibility

#### Step 2.1: TIT add `jam_javascript` link

**Scope:** `~/Documents/Poems/dev/tit/CMakeLists.txt`

**Action (@Engineer):**
- Locate the `juce_add_module("${JAM_ROOT}/jam_*")` block in TIT's CMakeLists.
- Add `juce_add_module("${JAM_ROOT}/jam_javascript")`.
- Add `jam_javascript` to TIT's `target_link_libraries` list.
- **No source changes required** — TIT does not use mermaid; the dep is satisfied by linkage only.

**Validation (@Auditor):**
- TIT builds clean with new link list
- TIT binary `titc` produces identical output for a known-input smoke test (TIT regression check)
- BLESSED: explicit declaration (consumer lists every dep, JUCE does not auto-resolve)

---

### Phase 3 — END Migration

#### Step 3.1: Adopt `Jam.cmake` in END's CMake

**Scope:** `~/Documents/Poems/dev/end/CMakeLists.txt`

**Action (@Engineer):**
1. Add at top (after JUCE setup): `include("$ENV{HOME}/Documents/Poems/dev/jam/Jam.cmake")` followed by `jam_set_root()`.
2. Replace the `USER_MODULE_PATH = ./modules` auto-glob block (lines 46, 145–162 per Pathfinder survey) with explicit `jam_add_modules(END_App jam_core jam_data_structures jam_freetype jam_graphics jam_gui jam_javascript jam_markdown jam_animation)`.
3. **Do NOT delete `end/modules/jreng_*`** — leave on disk (Phase 4 cleanup).
4. **Keep all JUCE_MODULES static list (lines 39–43) unchanged** — only the user-module path changes.

**Validation (@Auditor):**
- CMake reconfigures cleanly (no missing module errors)
- `JAM_ROOT` resolves to `~/Documents/Poems/dev/jam`
- Build target `END_App` lists 8 jam_ modules + standard juce_ modules
- BLESSED: SSOT (Jam.cmake is single point of JAM_ROOT resolution)
- No legacy USER_MODULE_PATH glob remaining

#### Step 3.2: Source rename `jreng::` → `jam::`

**Scope:** `~/Documents/Poems/dev/end/Source/` (~51 files, ~352 namespace sites per Pathfinder)

**Action (@Engineer):**
1. **Pre-flight scope-narrowing grep:** confirm exact site count and file list. Capture before-count.
2. **Mechanical rename** in `Source/` only (do NOT touch docs/, carol/, *.md, SPEC.md, ARCHITECTURE.md, SPRINT-LOG.md):
   - `jreng::` → `jam::` in `.h`, `.cpp`, `.mm` files only
   - `<jreng_core/...>` includes — see Step 3.3
3. **Macro rename** (verify presence first via grep): if `JRENG_*` macros (e.g., `JRENG_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`) are referenced in END Source/, rename to `JAM_*`. If macros named `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` are used (JUCE's macro), leave unchanged.
4. **Verify no false positives:** comments containing the word "jreng" outside namespace context, file paths in error messages, etc. — review the diff before commit.
5. After-count: confirm zero `jreng::` references remain in `Source/` (excluding doc files).

**Validation (@Auditor):**
- Zero `jreng::` references in `Source/` `*.{h,cpp,mm}` files
- Zero `JRENG_` macro references in `Source/` `*.{h,cpp,mm}` files
- Doc files (`*.md`, comments referring to historical context) preserved
- BLESSED: SSOT (one namespace), Explicit (no namespace aliasing introduced)
- Compiles after Phase 3.3 + 3.4 (parity check deferred to Step 3.6)

#### Step 3.3: Source `#include` rewrites

**Scope:**
- `~/Documents/Poems/dev/end/Source/interprocess/Daemon.h:25`
- `~/Documents/Poems/dev/end/Source/nexus/Nexus.h:35`

**Action (@Engineer):**
1. `Daemon.h:25` — `#include <jreng_core/jreng_core.h>` → `#include <jam_core/jam_core.h>` (verify exact original spelling first via Read).
2. `Nexus.h:35` — same rewrite.
3. **Verify no other explicit `<jreng_*/...>` includes** via grep across `Source/`. Per Pathfinder, only these two exist; confirm.

**Validation (@Auditor):**
- Both files updated
- Grep confirms zero `<jreng_` directives remain in Source/

#### Step 3.4: Shrink `Terminal::ModalWindow` wrapper

**Scope:**
- `~/Documents/Poems/dev/end/Source/component/ModalWindow.h`
- `~/Documents/Poems/dev/end/Source/component/ModalWindow.cpp`

**Action (@Engineer):**
1. Change base class `: public jreng::ModalWindow` → `: public jam::ModalWindow` (already covered by Phase 3.2 sed, verify here).
2. **Delete from ModalWindow.cpp:** `setupWindow` body, the explicit `setNativeSharedContext` + `setRenderer` + `setVisible` + `enterModalState` sequence in the constructor — these are now provided by `jam::ModalWindow`'s caller-ctor (built in Step 1.8).
3. **Constructor body shrinks to:**
   - Forward args to `jam::ModalWindow` caller-ctor (base initializer list)
   - Wire `Terminal::Display::onRepaintNeeded` if `content` is `Terminal::Display` (item 2 — preserved)
   - Inject `Config::getContext()` reference (item 7 — preserved)
4. **Preserve verbatim:**
   - `paint()` override (item 4)
   - `keyPressed()` override (item 5)
   - Per-platform `cornerSize` constants (item 6)
   - `Config& config { *Config::getContext() }` member
5. Update doc-comment in ModalWindow.h to reflect new shape (drops items 1+3, base now `jam::ModalWindow`).

**Validation (@Auditor):**
- `Terminal::ModalWindow` derives from `jam::ModalWindow`
- `setupWindow` private method removed
- Constructor body only contains: base ctor forwarding, Display wiring, Config injection
- `paint`, `keyPressed`, `cornerSize` preserved unchanged
- BLESSED: Lean (wrapper now smaller), Encapsulation (terminal-specific concerns stay in wrapper, generic concerns delegated to base)
- Single call site `Popup.cpp:46` still compiles

#### Step 3.5: Source rename `jreng::Animator` → `jam::Animator`

**Scope:** END Source files using `jreng::Animator` (per Pathfinder: `MessageOverlay.h` — 4 sites, `ActionList.cpp:477` — 1 site)

**Action (@Engineer):**
- Already covered by Phase 3.2's `jreng::` → `jam::` blanket rename. Verify these specific sites compile against `jam::Animator` from `jam_animation`.
- Verify `jam::Animator::toggleFade` signature compatibility — if the kuassa version diverged from jreng (e.g., extra parameter), adapt call sites here.

**Validation (@Auditor):**
- All 5 sites (MessageOverlay.h ×4, ActionList.cpp ×1) compile against `jam::Animator`
- No call-site signature mismatch

#### Step 3.6: Build + functional parity test (HEAD parity gate)

**Scope:** Full END build + smoke test

**Action (@Engineer):**
1. Clean build: `cmake --build Builds/Ninja --target END_App --clean-first` (or platform equivalent)
2. Resolve any compile errors per BLESSED + JRENG-CODING-STANDARD.md (no early-return shortcuts, no silent guards).
3. Launch END binary; report any runtime errors.
4. **DO NOT proceed to Phase 4 until ARCHITECT confirms 100% functional parity vs HEAD.** Functional parity covered by ARCHITECT's manual smoke test of:
   - All key terminal flows (PTY, render, scrollback, selection, copy/paste)
   - Glass blur on macOS (CALayer residue NOT present — Apple Silicon fix should be silently active via GLRoot+setRenderables)
   - DWM glass on Windows (corners, blur)
   - Modal popups (command palette, action list)
   - WHELMED markdown (mermaid rendering specifically — confirms jam_javascript wiring)
   - Hot reload / config errors
   - Multi-window
5. ARCHITECT manual sign-off required to unlock Phase 4.

**Validation (@Auditor):**
- Clean build artifact `END_App` produced
- No new compile warnings introduced
- Smoke test results documented (ARCHITECT-verified, not @Auditor-verified)

---

### Phase 4 — Cleanup (gated on ARCHITECT parity sign-off)

#### Step 4.1: Delete `end/modules/jreng_*`

**Scope:** all 7 directories under `~/Documents/Poems/dev/end/modules/`:
- `jreng_core/`, `jreng_data_structures/`, `jreng_freetype/`, `jreng_graphics/`, `jreng_gui/`, `jreng_javascript/`, `jreng_markdown/`

**Action (@Engineer):**
1. **Pre-flight check:** confirm CMake no longer references any path under `end/modules/`. If `USER_MODULE_PATH` or any `juce_add_module` still points at `end/modules/`, STOP and fix Step 3.1.
2. Delete the 7 directories.
3. Verify `end/modules/` is empty (or remove it entirely if nothing else lives there — verify via `ls`).
4. Rebuild END to confirm no orphan references.

**Validation (@Auditor):**
- `end/modules/` empty or removed
- Rebuild green
- No `jreng_*` filesystem references remain in END project tree (grep `end/` for `jreng_` — should match only doc-history files like SPRINT-LOG)

---

## BLESSED Alignment

- **B (Bound):** Module ownership clarified — JAM owns foundation, kuassa owns plugins, each project owns its consumer code. RAII preserved (no raw owning pointers introduced). Renderer + delegate ownership scoped per `jam::Window` lifecycle.
- **L (Lean):** YAGNI applied to scope — `jam_look_and_feel::Knob_V1` plugin coupling left dormant (END doesn't consume); `jam_animation` ships only the foundation slice. `Terminal::ModalWindow` shrinks via wrapper consolidation.
- **E (Explicit):** All module deps declared in module headers. All include paths absolute via `JAM_ROOT`. No anonymous namespaces. No early returns introduced. Brace init throughout new code. Alternative tokens (`not`/`and`/`or`) per JRENG-CODING-STANDARD.md.
- **S (SSOT):** JAM is single source for foundation. `Jam.cmake` is single point of `JAM_ROOT` resolution. No prefix duality (`jam_` only). `Map::Instance<Derived>` eliminates the duplication that the non-template `Instance` carried (delegated to `Context<Derived>` already present).
- **S (Stateless):** Apple Silicon GLRoot sentinel is structural, not stateful. `BackgroundBlur::isEnabled` atomic is internal state on a service object, not orchestrator-tracked.
- **E (Encapsulation):** `Style::Window` factored out of `BackgroundBlur` per kuassa's Single-Responsibility split. `Terminal::ModalWindow` shrinks to genuinely terminal-specific concerns; generic Window machinery delegated to base.
- **D (Deterministic):** No new non-determinism introduced — port preserves existing semantics. Functional parity gate (Step 3.6) is the verdict.

---

## Risks / Open Questions

**Resolved (locked in PLAN body):**
- BackgroundBlur location: `jam_graphics/blur/` (ARCHITECT-locked)
- `juce_opengl` hard dep on `jam_core`: accepted (ARCHITECT-locked, "we're going to do a lot of GL moving forward"); TIT adds one CMake line in Phase 2
- StyleManager port: clean (Pathfinder-confirmed; no plugin-domain deps)
- `kuassa_fonts` status: foundation-grade (Pathfinder-confirmed; ports as `jam_fonts` in Step 1.9)
- `jreng::Animator` source: `end/modules/jreng_gui/animation/jreng_animator.h` (Pathfinder-confirmed; relocates to `jam_animation`)
- `Taper::Map` CRTP blast: 1 file, 0 external consumers (Pathfinder-confirmed)

**Verification deferred to runtime (Step 3.6 functional parity test):**

1. **Apple Silicon GLRoot sentinel + END existing GLComponents.** END has `Whelmed::Component` and `Terminal::Component` as `GLComponent` consumers. The new GLRoot zero-sized sentinel adds another GLComponent to the content tree. The `setRenderables` walker must correctly enumerate END's existing GLComponents alongside GLRoot — no double-render, no missed render. Cannot be verified statically; @Engineer observes terminal scrollback rendering and Whelmed pane rendering during Step 3.6 smoke test on Apple Silicon hardware. If anomalies surface, escalate to ARCHITECT.

2. **`jam::Animator::toggleFade` signature compat with END call sites.** kuassa Animator and jreng Animator are conceptually identical but may differ in parameter ordering or default args. Step 1.7 records any divergence; Step 3.5 adapts call sites if needed. No ARCHITECT decision required unless a semantic difference (not just signature) surfaces.

**Deferred to future sprints (out of scope, locked):**

- END `Terminal::LookAndFeel` migration to `jam::LookAndFeel::Theme` — future work.
- Caroline + whatdbg migration to JAM — future work; both keep their `jreng_*` modules in their own `modules/` dirs after END migration.
- Kuassa-side housekeeping (fork foundation improvements made in jam back into kuassa) — long-term, ARCHITECT-managed cadence.
- `jam_look_and_feel::Knob_V1` plugin coupling (`findParentComponentOfClass<juce::AudioProcessorEditor>`) — dormant this sprint (END doesn't consume jam_look_and_feel); flagged for future cleanup when END or another non-plugin consumer needs `Knob_V1`.

---

*End of PLAN-jam-migration.md*
