# PLAN: cast-migration — END host on CAST, config on markdown

**RFC:** RFC-config-migration.md (consumed; §1-§5, §8 mapped below; §6/§7 descoped — see Risks)
**Date:** 2026-09-11
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM (LANGUAGE.md C++ section — header-first, 30/3 unchanged)
**Project-root copy:** on approval this file is written verbatim as `PLAN-cast-migration.md` at `~/Documents/Poems/dev/end/`.

## Context

END cannot configure or compile: `CMakeLists.txt:14` includes `jam/cmake/BuildSetup`, which no longer exists (jam Sprint 98); `jam_lua` is deleted (jam Sprint 85, commit `b5a9ae620`); `jam_lexicon` and its generator are gone (`Lexicon.h:17`); `jam_mermaid` is now `jam_mermaid_diagram`. jam moved to CAST (`archetype/`, `cast/SPEC.md`); EVE converged on it. END is the host: its terminal counterparts move to `plugins/eve/`, whelmed counterparts to `plugins/whelmed/`. Every remaining lua file becomes markdown consumed through one ctor shared by END, EVE and WHELMED (write-if-missing → read → validate → AST → ValueTree → watch). Runtime behaviour of the last cmake build is preserved.

## Overview

Part A makes END a CAST project (`project-info.md`, `cast/`, generated `CMakeLists.txt` + `Source/generated/*`), sweeps END's symbols onto current jam, and relocates terminal/whelmed counterparts. Part B replaces `jam::lua::fromLua` with `jam::ConfigDocument` (jam_markdown) behind the untouched `ConfigDirectory::loadFromPath` seam and rewrites END's config corpus as typed markdown tables.

## Language / Framework Constraints

- C++17/JUCE: header-first (LANGUAGE.md L); non-template bodies in `.cpp` (CODING.md TU placement) — `jam_ConfigDocument.cpp` / `jam_ConfigValidator.cpp` like `jam_MarkdownDocument.cpp` / `jam_MarkdownValidator.cpp`.
- CAST SPEC §1.1: data is correct; the engine is never changed. Every build/gen value is a table row or a fence — no engine work.
- Code Hygiene: no comments/doxygen until the post-audit pass.

## Dependency & API Inventory

**CAST (data-driven codegen + toolchain)** — `archetype/app/init.cast` (project-info + manifest fences), `archetype/app/cast/cmake.cast` (410-line template, 14 `:::[list]:::` slots + 12 command fences), `jam/cast/code.cast` fences: `namespace`, `identifier`, `bimap`, `name-entry`, `enum-entry`, `struct`, `shared-instance`, `include`, `line`, `constant`, `linebreak`. `cast`'s `## output index` (`cast/cast/CAST.md:274-287`) renders the global `struct Generated` with `jam::SharedInstance<map::Generated> generated { std::in_place }` plus every row declaring `- type:`/`- instance:` (SPEC §6.5 binding-name selector) — END's aggregate uses this exactly; no new fence, no new name.
**jam** — `jam::MarkdownDocument::parse (text, origin)` stamps `Id::path`/`Id::line` (`jam_MarkdownDocument.cpp:32-51`); table query API `getTables/getTableRows/getTableValue/getTableHeaders` (`jam_MarkdownDocument.h:59-116`); `Document::Validator::Rules` = `jam::Function::Map<juce::String, juce::Result>` (`jam_Document.h:430`); `MarkdownValidator` structural rules + `getLocation` (`jam_MarkdownValidator.cpp:188-299`); `jam::Model (juce::ValueTree)` adopt ctor, `setValuesFrom` diff-overlay, `getValue (type, id)`, `getInt16` (`jam_Model.h:421`, `jam_Model.cpp:298,348,361`); `jam::Bimap<int>` API `get()/get(key)/get(value)/contains/getDefault` (`jam_Bimap.h:134-174`); `map::Position/WindowFX/ButtonState/ImageResample/FontRasterizerBackend/MouseButton` (`generated/jam_Bimaps.h`), `map::Generated` (`jam_Generated.h:43`); `jam::StyleCustom` virtual set (`jam_StyleCustom.h:23-158`); `BinaryData::Raw/getString/fetcher` (`jam_Raw.h`); `Extensions::md` (`jam_Files.h:46`); `jam::ColourScheme::fromValueTree/toColour` (`jam_ColourScheme.h:7,74`); `jam::File::Watcher` (`jam_Listener.h:91-94`).
**Historical parity source** — `AppBuilder.cmake` @ `26dae357f~1` (scratchpad copy): shaderc `-Wl,-load_hidden` :597-631, spirv-cross :633-653, jam svg embed :436-444, shader templates via BinaryData, `-weak_framework UserNotifications` (`end/CMakeLists.txt:157`).
**JUCE** — `juce_add_gui_app`, `juce_add_binary_data`, `juce_add_module`; `var(String)→int64` is `getLargeIntValue` (no hex) (`juce_Variant.cpp:275`) → typed conversion is mandatory.
**Established patterns confirmed** — markdown `type`-cell dispatch: `kuassa_ParameterLayout::get` (`kuassa_ParameterLayout.h:258-323`); `choices` column tokenised on commas (`:49-55`, `parameters.md:8`); domain rules on `Document::Validator`: `PluginEditorLayout::getRules` + separate structural `MarkdownValidator` pass (`jam_PluginEditorLayout.h:66-231, 303-343`); Document→ValueTree at one creation point: `PluginEditorLayout::addParameters/populateTree` (`:446-494`); events maps `jam::Function::Map<juce::Identifier, void>` (ENDView, ENDLookAndFeel).

## Validation Gate

Each step is validated by COUNSELOR before the next — against MANIFESTO.md (BLESSED), NAMES.md, CODING.md, and the locked decisions below; file content vs step text is the only completion check. @Auditor runs ONCE after Step 9. ARCHITECT runs `cast cast/CAST.md` / builds; agents never run cast, cmake, ninja, git.

## Locked decisions

D1. Row schema for every config table (END, eve.md, whelmed.md): `| key | type | value | choices | comment |`. `key` = the on-tree property string exactly as today's lua key (`success_message`, `font-family`, `zoom_step`…) — vocabulary strings stay byte-identical (Sprint 81 contract). `type` ∈ `int`, `float`, `bool`, `string`, `colour`, `numbers` (jam `Id::integer/floatingPoint/boolean/string/colour/numbers` strings). `choices` = comma-separated allowed values, blank = unconstrained. `comment` carries the lua doc line.
D2. Table heading = tree type (`## display`, `## graphics`, `## mouse`, `## keys`, `## window`, `## style`, `## code`, `## scrollbar`, `## tab`, `## button`, `## overlay`, `## pane`, `## status_bar`, `## hint`, `## menu`, `## action_list`). Tables are flat siblings; every consumer reads through recursive `getValueFromChildWithProperty` (`jam_Model.cpp:348-351`), so lua nesting is not reproduced.
D3. Typed conversion lives in `jam::ConfigDocument::getValueTree` via one dispatch table keyed by the `type` cell (ParameterLayout shape): `int`→`var(int)`, `float`→`var(double)`, `bool`→`var(bool)`, `string`→`var(String)`, `colour`→`var(int64)` from `0xAARRGGBB` hex, `numbers`→`var(Array<var>)` of ints from the comma list. Output: `juce::ValueTree { rootType }` with one child `Id::toType (tableId)` per table, one property per row.
D4. Validation is `jam::ConfigValidator : jam::MarkdownValidator` with two rules keyed `type` (cell ∈ dispatch keys) and `choices` (non-blank choices ⇒ value ∈ choices); the four structural rules run through a `static const MarkdownValidator` first, exactly as `PluginEditorLayout::getDocumentValidators` does. Diagnostics are `getLocation`'s `path:line (column)`. `ConfigModel::getValidators`, `getValidator<>`, `registerParameters` and `jam::lua::*` are deleted — no consumer reads a CONFIG parameter through `getParameter/getRawParameterValue/ParameterAttachment` (grep: only ENDModel/Nexus/ENDActions on `end::Model`).
D5. ValueTree stays the Model; `ConfigDocument` is the creation. `ConfigModel`/`ConfigTheme`/`ConfigShader` keep `saveToPath` → `loadFromPath` → `startWatcher` and the `setValuesFrom` overlay; only the builder call changes. Watcher matches `Extensions::md`.
D6. `jam::ConfigDocument`/`ConfigValidator` live in `jam_markdown/document/` beside `MarkdownDocument`/`MarkdownValidator`. They produce `juce::ValueTree` (juce_data_structures, already a jam_core dependency) — jam_markdown gains no module dependency.
D7. Generated headers: `Source/generated/{ProjectInfo,Identifiers,Bimaps,Files,Generated}.h` (cast/EVE family). `Generated` (global struct) owns `jam::SharedInstance<map::Generated>` + END's four bimaps; `ENDApplication` owns `Generated generated;` replacing `Id::Lexicon lexicon`. No "lexicon" token survives anywhere.
D8. END keeps: `display` {theme, size, zoom_step, pane_step, always_on_top, title_bar_buttons, save_window_state, success_message, confirmation_on_exit, force_dwm, gpu, daemon, auto_reload}, `graphics`, `mouse`, `keys` (host bindings), `theme.md` {window, style, code, scrollbar, tab, button, overlay, pane, status_bar, hint, menu, action_list}, `actions` macro tables (RFC §6 schema, data only). Moves to `plugins/eve/Source/config/eve.md`: `shell`, `terminal`, `hyperlinks`, `image`, `cursor`, `ansi`, keys rows {copy, paste, newline, enter_selection, enter_open_file, open_file_next_page, selection_*}, all of `popup.lua`, bimaps `DropMode`/`CursorShape`. Moves to `plugins/whelmed/Source/config/whelmed.md`: all of `whelmed.lua`, keys rows {scroll_*}. `code` stays (consumed by `TabView.cpp:185,206` + `setEmbolden`).
D9. Build data (parity with `CMakeLists.txt:81-131,157,166`): defines add `JUCE_PLUGINHOST_VST3=1`, `$<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>`, `JAM_VULKAN_RUNTIME_SHADER_COMPILER=1`, `HAVE_FREETYPE` (consumed by `juce_graphics_Harfbuzz.cpp`), `$<$<NOT:$<PLATFORM_ID:Windows>>:HAVE_UNISTD_H>`, `$<$<NOT:$<PLATFORM_ID:Windows>>:HAVE_FCNTL_H>`; `JUCE_WEB_BROWSER=0` (no consumer in END Source; mermaid leaves); link `-weak_framework UserNotifications` (mac); shaderc + spirv-cross block; BinaryData rows for `Source/config/**.md`, `Source/config/svg/*.svg`, `${CAST_USER_MODULE_PATH}/resources/svg/*.svg`, `${CAST_USER_MODULE_PATH}/jam_vulkan/shader/*.frag`, `…/*.vert`; user modules `jam_core jam_debug jam_data_structures jam_gui jam_graphics jam_animation jam_freetype jam_vulkan jam_style jam_markdown jam_web jam_clap jam_audio_devices`; JUCE rows `juce_gui_extra juce_audio_basics juce_audio_devices juce_audio_formats juce_audio_processors_headless juce_audio_processors juce_audio_utils`; include row `${CAST_USER_MODULE_PATH}/jam_clap` (vendored `<clap/…>`, AppBuilder :687-690); bundle `com.JRENG.END`; target/product `END`.

## Steps

### Part A — CAST toolchain, generated vocabulary, moves

### Step 1: project-info.md + cast/ from the app archetype
**Scope:** `project-info.md`, `cast/CAST.md`, `cast/cmake.cast`, `cast/identifiers.md`, `cast/bimaps.md`, `cast/files.md`, `entitlements.plist` (new, EVE's content).
**Action:** Author from `archetype/app/init.cast` (`[no-banner]project-info` / `[no-banner]manifest` fences) with END values and D9 rows; `cmake.cast` = archetype template + two fences rendered as template text after the LINK block: (a) shaderc/spirv-cross from `AppBuilder.cmake.hist:597-653` on `${CAST_VULKAN_PATH}` (mac: `-Wl,-load_hidden,${CAST_VULKAN_PATH}/macOS/lib/libshaderc_combined.a`, `libspirv-cross-core.a` IMPORTED; win: `${CAST_USER_MODULE_PATH}/jam_vulkan/lib/*.lib` with `_DEBUG` locations), (b) `-weak_framework UserNotifications`. `identifiers.md`: every `Source/lexicon.md` word END still consumes (D8) that jam's `cast/identifiers.md` does not declare (`| @id | word | string |`); `bimaps.md`: `OverlayAxisLine`, `FileConfig` {display, keys}, `FileThemes` {theme}, `FileFlex` (jam row shape `| name | key |`); `files.md`: the four corner-menu icons (`split_vertical_normal.svg`, `split_horizontal_normal.svg`, `join_cells_vertical_normal.svg`, `join_cells_horizontal_normal.svg`) as `files::` rows. `CAST.md` outputs: ProjectInfo (archetype), Identifiers (`@code:identifier` in `Id`), Bimaps (four `@code:bimap` rows in `map`, each with `- type:`/`- instance:`), Files (`files` namespace), Generated (`cast`'s `## output index` shape), CMakeLists (archetype wiring + the two new named-token wrappers).
**Validation:** every value traces to one row (SPEC §2); arity matches slot count; D9 rows present; no word duplicated against jam's identifiers.md; no `lexicon` token.

### Step 2: Generate and converge
**Scope:** `CMakeLists.txt` (generated, replaces hand-written), `Source/generated/*`.
**Action:** ARCHITECT runs `cast cast/CAST.md`; Engineer compares generated `CMakeLists.txt` line-by-line to `CMakeLists.txt` (old) + `AppBuilder.cmake.hist` for D9 parity and reports diffs; second run must be an empty diff.
**Validation:** fixpoint; generated `Generated.h` = global `struct Generated` with jam's `map::Generated` + four END bimaps; `ProjectInfo::projectName/companyName/versionString` present.

### Step 3: Symbol sweep onto current jam
**Scope:** `Source/**/*.{h,cpp}` (14 files with bimap sites; `ENDLookAndFeel.h/.cpp`, `Main.h`, `LexiconFiles.h` → `config/ConfigDirectory.h`).
**Action:** `Id::X::get (v)` → `map::X::getInstance()->get (v)`, `Id::X::value` → `map::X::value`, `Id::X::get()` → `map::X::getInstance()->get()` for Position, WindowFX, ButtonState, ImageResample, FontRasterizerBackend, MouseButton, OverlayAxisLine, FileConfig, FileThemes, FileFlex; `jam::StyleMethods<ENDLookAndFeel>` → `jam::StyleCustom` (overrides already match `jam_StyleCustom.h:23-90`); `Resource::splitVerticalNormal` etc. → `files::…`; `jam::ColourMap` → `jam::ColourScheme`; `#include "generated/Lexicon.h"` → `"generated/Generated.h"`; `Id::Lexicon lexicon` → `Generated generated`; `Id::Files::*` path composition moves verbatim into `config/ConfigDirectory.h` (statics on `ConfigDirectory`), `Id::lua` → `Extensions::md`. Delete `Source/lexicon.md`, `Source/generated/Lexicon.h/.cpp`, `Source/LexiconFiles.h`, old `CMakeLists.txt` content is already replaced by Step 2.
**Validation:** grep `Id::(Position|…|Lexicon)|StyleMethods|Resource::|ColourMap|jam::lua|Lexicon` → zero; CODING.md (`not/and/or`, `.at()`, no bail-outs) on touched lines.

### Step 4: Relocate terminal and whelmed counterparts
**Scope:** `Source/openConsole/`, `Source/fonts/SymbolsNerdFont-Regular.ttf`, `Source/mermaid/`, `tests/`, `Source/button/`, `Source/graphics/` (empty dirs).
**Action:** Move (never delete) `Source/openConsole/*`, `Source/fonts/SymbolsNerdFont-Regular.ttf`, `tests/**` → `~/Documents/Poems/dev/plugins/eve/` (same relative paths); `Source/mermaid/*` → `~/Documents/Poems/dev/plugins/whelmed/Source/mermaid/`; remove the two empty dirs. Stop and report if `plugins/eve/` does not exist yet (ARCHITECT moves the eve repo).
**Validation:** END tree has zero terminal/whelmed files; destinations hold byte-identical copies.

### Part B — config on jam::ConfigDocument

### Step 5: jam::ConfigDocument + jam::ConfigValidator (jam repo)
**Scope:** `jam/jam_markdown/document/jam_ConfigDocument.{h,cpp}`, `jam_ConfigValidator.{h,cpp}`, `jam_markdown/jam_markdown.h` (+2 includes), `jam_markdown/jam_markdown.cpp` (+2 TUs), `jam/cast/identifiers.md` (add `key` if absent).
**Action:** `struct ConfigDocument : MarkdownDocument` — `static ConfigDocument parse (const juce::String& text, const juce::String& origin)` (same body shape as `MarkdownDocument::parse (text, origin)`, `jam_MarkdownDocument.cpp:32-51`); `juce::ValueTree getValueTree (const juce::Identifier& rootType) const` per D3, dispatch table `static const jam::Function::Map<juce::Identifier, juce::var>` named `valueTypes` keyed `Id::integer/floatingPoint/boolean/string/colour/numbers`. `struct ConfigValidator : MarkdownValidator` — `getRules()` per D4 (`Id::type`, `Id::choices`), plus `static juce::Result isValid (const ConfigDocument&)` running the structural `MarkdownValidator` then its own rules (the `getDocumentValidators` shape). Zero includes in the submodule files.
**Validation:** NAMES (verbs `get`/`is`/`parse`, table noun `valueTypes` Rule 8); one typing point (S); no `==` on strings (dispatch by Identifier); 30/3.

### Step 6: Markdown config corpus
**Scope:** `Source/config/display.md`, `Source/config/keys.md`, `Source/config/theme/gfx/theme.md` (dir renamed from `lua/theme/gfx`), `plugins/eve/Source/config/eve.md`, `plugins/whelmed/Source/config/whelmed.md`; delete the five `.lua` files.
**Action:** Transcribe every lua key per D1/D2/D8 — values literal (`withAlpha` colours pre-resolved: `0xbf090d12`, `0x2000ddee`, `0x802c4144`; arrays as `numbers` "640, 480"; `style.mac/win` rows under `## style`; enum-constrained rows carry `choices` from the bimap they validate against); lua doc lines → `comment`; file-level prose → table documentation paragraphs (SPEC §5.4). `actions` → RFC §6 tables. `~/.config/end/*.lua` on this machine are user files — untouched.
**Validation:** every consumed key of Pathfinder's read table present with the type its consumer casts to (colour rows for every `setColourId` site, `numbers` for `padding`/`size`); no lua key lost except the D8 moves; seed `BinaryData::Raw` names = `FileConfig/FileThemes` stems + `.md`.

### Step 7: ConfigModel family onto ConfigDocument
**Scope:** `Source/config/ConfigModel.h/.cpp`, `ConfigDirectory.h`, `Source/end/EventRegistration.cpp` (`Id::useGpu` unchanged), `Main.h`.
**Action:** Replace each `jam::lua::fromLua (rootTag, bimap, read, validators[, errors])` with: for each bimap key → `ConfigDocument::parse (read (key), fileName)` → `ConfigValidator::isValid` (failures appended to `errors`) → `getValueTree (rootTag)` children appended into the root tree (same merge the lua path produced: one root, one child per table). Delete `getValidators`, `getValidator<>`, `registerParameters`, `glslBufferSize` (D4). `fileChanged` filter → `Extensions::md`. `ConfigShader` untouched. `appModel.setMessage` receives `errors` unchanged.
**Validation:** `loadFromPath` signatures unchanged; every consumer read (`getValue`, `getInt16`, `getChildWithName`) unchanged; events keys unchanged; positive nesting; no shadow state.

### Step 8: Hosting literals
**Scope:** `Source/action/ENDActions.cpp:3,70,76`.
**Action:** none this sprint — `whelmedPluginId` stays until WHELMED exists (Risks R3).

### Step 9: Docs sync (post-audit)
**Scope:** `ARCHITECTURE.md` (Layer Separation, Config Chain, State Trees CONFIG block, Shaders/BinaryData prose), `CLAUDE.md` (Build, Constants, Key Docs), `SHADERS.md` build lines, `.clangd` CompilationDatabase → project root (EVE convention `eve/.clangd:2`).
**Action:** Mirror landed code only; then one dedicated doxygen delegation for new jam headers (zero warnings).
**Validation:** no statement without a code citation; no lua/lexicon vocabulary.

## BLESSED Alignment
- **B** — one owner per truth: tables own build values, `ConfigDocument` owns creation, `ConfigModel` owns the Model + watcher; generated files are artifacts.
- **L** — no new engine, no new pattern: archetype template + fences, jam fences, ParameterLayout dispatch shape; dead validator machinery removed.
- **E** — every build value visible in one row; typed conversion explicit in data (`type` column); failures loud with `path:line`.
- **S (SSOT)** — vocabulary strings unchanged; one conversion point; identifiers deduplicated against jam's table (redefinition is a compile error, so the compiler gates it).
- **S (Stateless)** — `ConfigDocument` holds parse results only; no getters added to consumers.
- **E (Encapsulation)** — `loadFromPath` seam untouched; consumers unchanged; jam_markdown gains no dependency.
- **D** — same bytes → same tree; cast fixpoint proven by the empty second-run diff.

## Risks / Open Questions
- **R1 (descope request):** RFC §6 macro *consumer* and §7 END-tagged protocol are not in this sprint; only the §6 data shape lands. Needs your explicit descope.
- **R2:** `choices` cells restate bimap enumerators (`map::Position` etc.) — a second copy of that vocabulary in data. Alternative is no membership rule (consumer `Bimap::get (value)` throws `out_of_range` at first use). Your call; plan assumes `choices`.
- **R3:** `com.jreng.whelmed` literal and the sidecar search path stay literals until the plugins exist; becoming config rows needs new key names (Rule -1).
- **R4:** `getCursorStyle`/`CursorStyle`, `getCodeLigatures`, `getCodePadding`, `getGutterWidth` have no callers; `cursor` moves to EVE, so `getCursorStyle` dies with its data (Step 7 compile gate); the other three stay (their tables stay).
- **R5:** Windows shaderc/spirv-cross `.lib` archives are absent on this machine (`jam_vulkan/lib/` holds only `build.bat`) — Windows link unverifiable here.
- **R6:** Sequencing with EVE's sprint: Step 5 is jam work shared by both; whichever sprint runs first executes it once.

## Verification (ARCHITECT)
`cast cast/CAST.md --debug` twice → empty diff; launch END: window chrome/blur, tab bar SVGs, pane split/join menu icons, background/post-process shader from `~/.config/end/shaders/*`, `Cmd+R` reload after editing `~/.config/end/display.md` shows `RELOAD`; an invalid cell shows `display.md:<line> (value)` in the overlay.
