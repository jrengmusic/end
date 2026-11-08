# SPRINT-LOG

---

## Sprint 98: Build Time — Selected-Format Lanes, Host-Arch Debug, END PCH and Job Pool, ccache ✅

**Date:** 2026-11-09
**Duration:** one session, over a context compaction. The start and end times are not recorded.
**Scope (ARCHITECT):** `PLAN-build-time.md` (9 steps). *"[x] Selected-format SSOT … [x] Install gate + 1-arch Debug … [x] END PCH + job pool … [x] ccache … write RFC to MACHINIST to ~/.config/ to execute"*; archetype answer "Also archetype"; *"EVE manifest by default (Release) must always build all, pack all, etc as now"*; *"the main `driver` flag, is at END"*; *"still the default `cast spell` is full chain, release, all format, universal, signed and notarized, etc"*; *"the implementation must be written now, so it was written UNIVERSALLY. no hardcoding of any platform"*; architecture answer "Universal in template". After the audit: *"B. write RFC to cast repo"*.

### Agents Participated
- COUNSELOR — PLAN, every validation read, the DCF triage of the audit, `~/.config/RFC-ccache.md`, `cast/RFC-toolchain-working-directory.md`, the small edits named below, this log.
- Pathfinder (haiku) — verbatim reads of the EVE, END and archetype templates and tables; the cast SPEC output and toolchain sections; the cast toolchain working-directory path (doxygen and LSP); the inventory of every `## toolchain` table; the bootstrap scripts; the CMake 4.4.5 help, `man ccache` and `clang-cl -help`.
- Engineer (sonnet) — every template and table edit below.
- Auditor (opus) — one pass, 17 findings.
- MACHINIST (separate session) — executed `~/.config/RFC-ccache.md` (ARCHITECT: *"machinist done its job"*).

### Files Modified (15 total)
**end:**
- `cast/cmake.cast:13-23` — compile job pool, pasted from EVE. `:29` — `CMAKE_OSX_ARCHITECTURES` default `"x86_64;arm64"` in the template ("Universal in template"). `:33` — `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT` `Embedded` (`/Z7`), because ccache does not support `/Zi`. `:47-52` — `find_program(CAST_CCACHE ccache NO_CACHE REQUIRED)` and one `ENABLED_LANGUAGES` loop that sets each `CMAKE_<LANG>_COMPILER_LAUNCHER`. `:141-154` — precompiled header, `-Xclang -fno-pch-timestamp` on APPLE, module sources skip the PCH and glob through `${CAST_USER_MODULE_PATH}`.
- `project-info.md:94-97` — `compileJobMemory` and `precompiledHeader` rows. `:114`, `:124`, `:132` — explicit architecture flags (`-UCMAKE_OSX_ARCHITECTURES` universal, `-DCMAKE_OSX_ARCHITECTURES=` host). `:120`, `:128` — END's debug and no-sign chains call EVE's `--vst-debug` and `--vst-no-sign` (the format word lives in END: "END toolchain, literal"). `## architecture` deleted. `:272` — `debugSymbols` win cell blank (CMake owns the MSVC format).
- `cast/spell.md` — the `@project-info:architecture` source line deleted from the `@CMakeLists` row (it fed the deleted `:::[list]:::`). `:131-137`, `:145-151` — second `@Files` and `@FilesSource` rows read `@files:platform files`.
- `cast/files.md:40-46` — `## platform files` (`mac` `EVE.vst`, `win` `EVE.dll`); the `eve plugin` row left `## files`. `files::evePlugin` keeps its name (one consumer, `Nexus.cpp:199`).
- `cast/source.cast:8-14` — fence `platform-identifier-definition` (`#if JUCE_MAC` / `#else`).
- `carol/SPRINT-LOG.md` — this entry and the handoff below.

**eve:**
- `cast/cmake.cast:35` — architecture default in the template. `:38` — MSVC `Embedded`. `:53-58` — the ccache block. `:60-69` — `CAST_PLUGIN_FORMAT` cache value: a fatal check against `CAST_FORMATS`, then `CAST_FORMATS` narrows to one format and `CAST_INSTALLER_FORMATS` intersects (`list(FILTER … INCLUDE REGEX "^${CAST_PLUGIN_FORMAT}$")`, `:68`). `:182-186` — PCH timestamp option. `:188-189` — module glob through `${CAST_USER_MODULE_PATH}`. Both install gates — `CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CAST_PLUGIN_FORMAT`. Pack stays ungated in Debug (*"ensure both release and debug build always run post build copy to end repo"*).
- `project-info.md` — `architecture` row deleted. `## toolchain` `:119-201`: every `cmake` row states the architecture and the format; six new lanes `vst-debug`, `vst3-debug`, `au-debug` (Debug, host) and `vst-no-sign`, `vst3-no-sign`, `au-no-sign` (Release, `-DCAST_SIGN=OFF`, host). `:260` — `debugSymbols` win cell blank.

**archetype:**
- `app/cast/cmake.cast` — the END changes in generic form: job pool `:12-24`, architecture `:29`, MSVC `:33`, ccache `:47-52`, PCH and timestamp `:133-146`.
- `app/init.cast` — `compileJobMemory`, `precompiledHeader` (`:63-64`); toolchain flags (`:77`, `:85`, `:91`); `## architecture` and its manifest source line deleted; `debugSymbols` win cell blank; the `## release` and `## debug` tables get the missing border between `fullPathInPdb` and `explicitTemplateInstantiation` (`:157`, `:215`); `## juce module` and `## define` re-padded (cast SPEC §3.1).
- `plugin-only/cast/cmake.cast` — the EVE changes without the installer lane: architecture `:35`, MSVC `:38`, ccache `:53-58`, `CAST_PLUGIN_FORMAT` narrows `CAST_FORMATS` only, PCH timestamp, module glob, both install gates.
- `plugin-only/init.cast` — `architecture` row deleted; toolchain `:122-184` with the arch and format flags and the lanes `vst3-debug`, `au-debug`, `vst3-no-sign`, `au-no-sign`; `debugSymbols` (`:229`) win cell blank.
- `ARCHETYPE.md` — Build flows (`:172-187`), the `## architecture` row (`:205`), section 6 plugin-only and app bullets (`:358-372`, `:402-414`), section 7 (`:455-458`). The §8 sentence that named projects needing a fresh cast run is deleted (ARCHITECT: *"ARCHETYPE is for ANY PROJECT. why hardcode eve into its docs?"*).

**~/.config:**
- `RFC-ccache.md` (new) — ccache in both bootstrap scripts, one monorepo `ccache.conf` with `sloppiness = pch_defines,time_macros,include_file_mtime,include_file_ctime` through `CCACHE_CONFIGPATH`; 4 open questions.

**cast:**
- `RFC-toolchain-working-directory.md` (new) — each toolchain row runs in the folder of the file that declares its table (option a) or in the manifest's folder (option b); scaffold for `Toolchain::runProcess`, `Processor::runToolchainRow`, `Pack.cpp:146` and SPEC §6.9; inventory of all 24 toolchain tables; 5 open questions.

### Alignment Check
- [x] BLESSED principles followed — one format word in END drives EVE (S); every lane states its architecture and format, so a shared build folder holds no stale value (D); the template fails fast without ccache (E).
- [x] NAMES.md adhered — new names ratified by plan approval (`CAST_PLUGIN_FORMAT`, the lane words, `CAST_CCACHE`, `platform-identifier-definition`, `## platform files`); `CAST_LANGUAGES` / `CAST_LANGUAGE` from the `CAST_SIGNING_PROFILES` / `CAST_FORMAT` families (Rule 5).
- [x] MANIFESTO.md principles applied
- [ ] Destructive-Edit Discipline: the Engineer that edited `archetype/plugin-only/init.cast` rebuilt the toolchain table by script with no backup (CAROL.md Destructive-Edit Discipline, step 2). The read-back showed no damage.

### Problems Solved
- **Debug chain builds every format, both architectures, and installs.** END's debug and no-sign rows now call one EVE lane each. That lane builds one format for the host architecture, packs it into `end/Source/plugins/`, and installs nothing. The default chain stays universal, all formats, signed, installed.
- **Architecture needed a table.** ARCHITECT: *"i actually bit bothered by architecture need dedicated table"*. The default lives once in each template. A lane overrides it by flag; an empty value gives the host architecture (CMake `CMAKE_OSX_ARCHITECTURES.rst:10-12`).
- **END compiled without PCH and job pool.** END and the app archetype now have both, byte-identical to EVE.
- **ccache.** The templates launch through ccache in every enabled language. PCH caching follows `man ccache` (PRECOMPILED HEADERS, lines 1994-2021): sloppiness in the machine config, `-fno-pch-timestamp` for clang. MSVC uses `/Z7`.
- **Audit (17 findings).** Fixed: #2 (one `ENABLED_LANGUAGES` loop), #6 (`NO_CACHE`, the `CAST_SIGNTOOL` pattern), #7 (glob reads `${CAST_USER_MODULE_PATH}`), #8 (installer formats intersect), #10 (app tables), #12 and #13 (RFC errors). Partly fixed: #14 (uncited RFC claims removed; the quote is ARCHITECT's message), #15 (§8 sentence; citation part rejected). Rejected with citation: #1 (SPEC §6.4 arity and §6.7 merge), #3 (no data lane carries the values; PLAN Step 5), #4 (`man ccache:1994`, `:1119` — no clang-cl/MSVC PCH caching), #5 (ARCHITECT's RFC ruling), #9 (PLAN Step 2), #16 (Step 7 validation), #17 (MANIFESTO L, one artifact per template). #11 (macOS absolute toolchain paths) has no fix inside these repos: each row runs in the caller's folder (`cast/Source/Toolchain.cpp:21-22`, `cast/SPEC.md:939-941`). It went to the cast RFC.

### State for Continuation
- No build was run. Before the next build, ARCHITECT runs a fresh `cast` in EVE, then in END. Check:
  - `Source/generated/Files.h` and `Files.cpp` still hold one `namespace files` block (SPEC §6.7 merge of the two `@Files` rows). A second block still compiles.
  - `cast cast/spell.md --debug` in END: EVE builds `EVE_VST` only, `lipo -info` on it shows the host architecture, and `unzip -l "Source/plugins/EVE v0.0.1 macOS.zip"` lists `EVE.vst/` only.
  - A second Debug build after one jam edit: `ccache -s` shows hits; compare `.ninja_log` with the 43.2 s baseline.
  - The default `cast cast/spell.md` in END: universal (`lipo -info`: x86_64 arm64), all EVE formats, signed and notarized.
- Windows is not tested (ARCHITECT: *"the actual testing/hardening will eventually get on actual windows machine"*).
- The EVE and END toolchain rows still hold `/Users/jreng/…` absolute paths. They wait for the cast working-directory change (handoff below).
- `PLAN-build-time.md` is done and still at the project root.

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261103T195500` — fit of plugin editors to the pane (deferred by ARCHITECT).
- `DEBT-20261025T135707` — END hosts EVE as CLAP; ENAX's CLAP format is future work (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (ARCHITECT: *"windows build still miles ahead. defer."*).

---

## Sprint 97: One Load Path, Async Instance, Drop Target in Front, EVE Known, Editor Once, OMP Race, Model-Driven Tabs ✅

**Date:** 2026-11-04
**Duration:** several sessions (compactions). The start and end times are not recorded.
**Scope (ARCHITECT):** `PLAN-load-plugin.md` (parent `PLAN-instant-plugin-loading.md` rows 5 and 6, Step 12). *"loading plugin, ANY PLUGIN including EVE is ONE PATH. one SSOT unique commandId"*; *"files::evePlugin is default args"*; *"drop file is override"*; *"blank component to front. always in front of everything, this is the FileDragAndDropTarget"*; *"split with direction should be command. load plugin is command. drag and mouse up call the chain"*; *"EVE is a plugin binary carried by END itself. it is KNOWN"*; *"editor must be created only ONCE"*; *"fix the OMP bug (init race)"*; *"remove all ode instrumentation"*; *"isn't tab could be fully event driven from Model?"*; *"proceed until /log no gate."* Fit of plugin editors deferred to DEBT.

### Agents Participated
- COUNSELOR — PLAN and its amendments, every validation read, the DCF triage of the audit, small edits named below, scout manifests, `ARCHITECTURE.md`, `DEBT.md`, this log.
- Pathfinder (haiku) — discovery reads (JUCE drag path, hosted-editor scale path, VST2 editor build, jam TTY, ENAX size path, layout listener order), whatdbg trace, two scout runs, the doxygen pass.
- Librarian (haiku) — AppKit drag destination routing.
- Engineer (sonnet) — every code change below.
- Auditor (opus) — one pass, 28 findings.

### Files Modified
**end:**
- `Source/plugins/Nexus.h` / `.cpp` — `knownPluginList`; `registerExtensions`, `addFormats`, `registerEVE` (EVE description listed at construction: `pluginFormatName`, `fileOrIdentifier`, `lastFileModTime`); `removeSession` deleted (no caller); `createSession` passes the sessions row.
- `Source/plugins/Session.h` / `.cpp` — load from the pane row event `Id::plugin` (`loadPlugin`, async `createPluginInstanceAsync`, the Model decides a late instance); `addPlugin` / `addConnections`; static `getPluginFormat (formatManager, file)` plus the member; `removePlugins (row)`; listens on the parent sessions row, `valueTreeChildRemoved` removes a closed tab's plugins after its editors; `jassert` on missing format, empty scan, null instance.
- `Source/ui/ENDView.h` / `.cpp` — `newPane (edge)` splits in half or adds; `loadPlugin (plugin)` writes `Id::plugin` of the focused pane from `getPath (Id::plugins).getChildFile (plugin)`; `newTab` and the split keys run `newPane` then `loadPlugin (files::evePlugin)`; `closeTab` removes the focused tab row only; actions split by responsibility (`registerApplicationActions`, `registerFocusActions`, `registerJoinActions`, `registerSwapActions`, `registerResizeActions`, `registerProfilerActions`); `getActiveTabView`.
- `Source/ui/EditorView.h` / `.cpp` — editor created on `Id::name` after layout; `DropTarget` (nested `juce::Component` + `juce::FileDragAndDropTarget`) in front of every child, focuses its pane on drag enter, drop runs `newPane (getEdge)` then `loadPlugin (path)`; `setEditorBounds` two branches (transform removed); `getParameter` const, ENAX lane only (pane row `Id::plugin` is the EVE file), `compareIgnoreCase`; master formatting and `getWidthName` deleted.
- `Source/ui/TabView.h` / `.cpp` — adopt constructor; `add` void; no longer a drop target; corner split runs `loadPlugin (files::evePlugin)`; `getJoinDirections`, `getJoinAction` (one join table).
- `Source/ui/SessionView.h` / `.cpp` — tabs follow the Model: `add` appends a tab row, `remove` removes it, `valueTreeChildAdded` / `valueTreeChildRemoved` mount and unmount `TabView`, last tab runs `quit`.
- `Source/Main.h` / `.cpp` — `settings` (`END.settings`): window state restored before show, saved at shutdown.
- `cast/identifiers.md` (`loadPlugin`, `plugin`; `bottom` moved to jam), `cast/files.md` (`settings file name`); `benchmark/pane.md`, `split.md` (cast rows for EVE and END, baselines); `ARCHITECTURE.md` (Hosting Contract); `DEBT.md`; `PLAN-load-plugin.md`.

**jam:**
- `jam_gui/layout/jam_PaneComponent.h` / `.cpp` — `getEdge (point)` through `juce::Line::isPointAbove`.
- `jam_gui/widgets/jam_TextEditor.cpp` — `bindToModel` no longer writes the winsize (OMP race).
- `jam_core/text/jam_Format.cpp` — an empty group yields the name alone. `jam_plugin_bootstrap/parameter/jam_ParameterLayout.cpp` — groups only for non-empty cells.
- `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.cpp` — instrumentation removed (memo of `PLAN-settings-once.md` in place).
- `patch/juce-plugin-editor-once-hook.patch` (new: `handleOpenEditor` keeps the editor that `effEditGetRect` built); `patch/juce-plugin-view-scale-hook.patch` deleted; `user-modules-info.md:1371`; `cast/data_structures.md` (`bottom`).
- Doxygen on every changed header.

**eve:**
- `Source/layout/parameters.md` — empty group cells (no master). `EVEProcessor.cpp`, `EVETerminalProcessor.cpp`, `EVEView.cpp` — instrumentation removed.

### Alignment Check
- [x] BLESSED principles followed — one load path, one split path, the Model decides late instances, tabs follow the Model, JUCE gives the close order.
- [x] NAMES.md adhered — new names from established families, recorded in the PLAN.
- [x] MANIFESTO.md principles applied
- [ ] Doxygen not regenerated; ARCHITECT regenerates and reads the warning count.

### Problems Solved
- **Cold start.** The first EVE load scanned EVE (339 ms). EVE is now listed at startup: `new pane` 308.9 ms → 0.030 ms, `session plugin` 244.6 ms → 0.020 ms (`benchmark/pane-load-report.md`).
- **Editor built twice** by the JUCE VST2 client (`effEditGetRect` then `effEditOpen`): one-line jam patch.
- **OMP help and `%`.** The bind wrote a 110×37 winsize before the pane size; the second restart sent `SIGWINCH` into the user's `TRAPWINCH` before OMP's first `precmd`. Fix ruled by ARCHITECT: no write at bind.
- **Drop over a plugin.** A drop target in front of each pane, from ARCHITECT's design.
- **Close crash** (`HashMap::at`) gone after the command chain; ARCHITECT confirmed.
- **Audit.** 28 findings: resolved 2, 3, 4, 7, 8, 9, 10, 11, 16, 18, 19, 20, 21, 22, 27, 28 (tabs); rejected 1, 12 (ARCHITECT's `toFront`), 5, 6, 13, 14, 15, 17, 23, 24, 25, 26, each with its clause or ruling in this session. Doxygen pass finding C1 rejected (MANIFESTO D).
- **Mistakes owned.** Claims from training data about AppKit drag routing and overlays; a z-order chosen against *"always in front of everything"*; `setTransform` on a hosted editor against JUCE's contract; a scout run from the wrong folder; a position argument raised as a blocker; a Model-driven pane design that the row moves of `MatrixComponent` forbid, corrected to tabs.

### State for Continuation
- Run `cast` in jam, eve and end (patch list changed: `juce-plugin-editor-once-hook.patch` in, scale patch out), then build.
- Runtime check: no OMP help and no `%` above the first prompt (Debug too); one EVE view build per pane; drop on a pane centre; tab close order (editor before plugin); last tab quits.
- Regenerate doxygen (`ninja doxygen`; jam `<leader>bd`).
- Panes stay imperative: `MatrixComponent` moves live pane rows by detach and re-add, so `closePane` keeps `getChildCount() > 1` → `closeTab`.
- Known gap (ARCHITECT chose it): when a terminal's final bounds equal the default layout, no winsize is written.

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261103T195500` — fit of plugin editors to the pane (ARCHITECT, 2026-11-04).
- `DEBT-20261025T135707` — END hosts EVE as CLAP (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (ARCHITECT).

---

## Sprint 96: END and EVE Config — Named Light/Dark Palette, One Load Path, Pane Padding, Canon Settings Lane, Keyword Key Form ✅

**Date:** 2026-11-03
**Duration:** several sessions (compactions). The start and end times are not recorded.
**Scope (ARCHITECT):** PLAN/RFC hygiene; END and EVE config cleaned (END carries no markdown, mermaid or terminal config; EVE organised like END; colours as names with `light | dark`; every key lower-case words formatted by the reader); one SSOT load path with version front matter, `Document::Validator`, errors in an overlay, the binary default as fallback, no assert on user input; pane padding as config. Then the amendment: *"KANJUT PluginEditorLayout is the canon"*, *"no bloating Style concern into PluginEditorLayout"*, *"hardcoding case into generated definition is the problem. format case is C++ responsibility always"*. `/audit` with every validated finding resolved; the doxygen pass; this log.

### Agents Participated
- COUNSELOR — PLAN and its amendment, every validation read, the DCF triage of both audits, small edits named below, this log.
- Pathfinder (haiku) — discovery maps (KANJUT canon diff, hard-coded keyword forms, END/EVE config-tree readers, map value readers), the doxygen pass.
- Engineer (sonnet) — implementation of PLAN steps, amendment K1–K8, both audit fix rounds.
- Auditor (opus) — two passes (before and after the amendment).

### Files Modified
**jam:**
- `jam_core/utils/jam_Id.h`, `jam_Id.cpp` (new), `jam_core.cpp` — `Id::toTag` gives one tag from any form of a keyword through `Format::toWords`; bodies moved to the `.cpp` (CODING one object one translation unit).
- `jam_core/text/jam_Format.h` / `.cpp` — `toWords (std::string_view) -> std::string`: any case to lower-case words, no juce::String (ARCHITECT: *"NOT with juce::String"*); `isNumber` strict.
- `jam_markdown/document/jam_ConfigDocument.h` / `.cpp` — `light`/`dark` columns; colour by hex, `colours` table, then `map::ColourNames`; `getRowKey` returns the tag (private); `getConfigRow` compares tags and never builds an empty Identifier; writer writes words; `getOrCreateDefaultFile` (version gate); `addTables` validates, checks the header set, then `getTableFailures` (duplicate, unknown key, numbers count, unresolved reference) against the default tree built once.
- `jam_markdown/document/jam_ConfigValidator.cpp` — rules as `get*Failures` file-statics; strict number, integer through `std::from_chars`, colour, key (empty or no words), choices; well-formed set texts.
- `jam_data_structures/model/jam_Model.h` / `.cpp` — static `setValuesFrom (target, source)`.
- `jam_web/css/jam_Css.h` / `.cpp` — every custom-property name, `var()` value, metric, window and fonts attribute id stored as `Id::toType (name)`; `stripCustomPropertyPrefix` deleted.
- `jam_style/style_manager/jam_StyleManager.h` / `.cpp` — every lookup formats its name at the owner; `setAppearance` matches `map::ColourId` in O(1) through `Format::toWords`; font aliases keyed by tag; `setAppearance (int64)`.
- `jam_style/jam_StyleCustom.*` — `getPanePadding`; `jam_StyleVariDisplay.cpp`, `jam_StylePopupTextBox.cpp` — alias compare through `Id::toType`.
- `jam_gui/style/jam_StyleTheme.h` / `.cpp` — `setFontRasterization`, `setEmbolden` (the canon place; text compare as the canon).
- `jam_gui/commands/jam_CommandManager.*` — command and row match through `Id::toType`; key rows and layers stored as tags; `mode` assert removed (a settings rule checks it).
- `jam_gui/layout/jam_Viewport.cpp` — scroll rows by tag; ordered ranges, no `jmap` on an empty range. `jam_gui/layout/jam_PaneComponent.h` — `edgePadding` deleted.
- `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h` / `.cpp`, `jam_SettingsDocument.h` / `.cpp` — back on the KANJUT canon text; markdown lane: version gate, `addTables` overlay on the binary default, errors in `Id::message`, rules through `getConfigRow`, new rules `appearance`, `rasterizer`, `src`, `mode`; XML lane verbatim canon; `addParameters` adds tags; `setSettingsValue` writes a new key as words and never dereferences a missing table.
- `jam_plugin_bootstrap/view/jam_PluginEditor.cpp`, `jam_ViewManager.cpp` — zoom step, scaling, size by tag; zoom applies only for a positive step; `lookAndFeelChanged` calls the theme; one key form for component parameters.
- `cast/style.md` (`ColourId`, `FontId` values are lower-case words), `cast/terminal.md` (`TerminalIndexed` values; `AnsiColour` added then removed), `cast/spell.md`, `cast/core.md` (fail texts `failNotNumber`, `failNotColour`, `failEmptyKey`, `failTableStructure`, `failNoWords`, `failCountMismatch`, `failUnknownKey`, `failDuplicateKey`, `failUnknownReference`, `failMissingDirectory`).
- `resources/markdown.css` — one `mono` (ARCHITECT: *"there's only ONE `mono`"*).

**end:**
- `Source/config/display.md`, `keys.md`, `theme/gfx/theme.md` — front matter `version: 0.0.1`; keys in words; `## colours` palette; `light | dark`; `## code` deleted; embolden in `## font`; editor colours in `## tab`; `## pane padding`; `appearance` row.
- `Source/config/ConfigModel.h` / `.cpp` — one load path through `ConfigDocument::addTables`; `getRowValue` reads by tag and picks `dark`; no copy of the config tree; missing theme folder reports an error.
- `Source/lookAndFeel/ENDLookAndFeel.*`, `Source/Main.cpp`, `Source/ui/ENDView.cpp` — event keys in tag form; one dispatch through `getEventKey`; `getPanePadding`.
- `Source/ui/EditorView.*` — hosted editor inside the outline; `TabView.cpp` — previews show pixel size.
- `cast/identifiers.md`, `SPEC.md`, `ARCHITECTURE.md`, `README.md`, `CLAUDE.md`, `PLAN-end-eve-config.md`; deleted `PLAN-enax-plugin-host-type.md`, `PLAN-enax-command-id.md`, `INVENTORY-end-plugin-host.md`, `carol/HANDOFF.md`.

**eve:**
- `Source/layout/ViewLayout.md` — keys in words; `Source/layout/style.css`, `Source/textEditor.css` — colour names, one `mono`.
- `Source/EVEView.*` — message overlay fed by the `message` component property; empty FIFO names build nothing. `Source/EVETerminalProcessor.*` — the command event captures its tag once (audio thread); `addCommand` removed.
- `SPEC.md`, `README.md`, `PLAN-Grand-Scheme.md` amendment; deleted `HANDOFF-profiling.md`, `PLAN-eve-terminal-protocol.md`.

**kuassa, cast, ggwp, scout:** `markdown.css` — the `--mono` duplicate removed.

### Alignment Check
- [x] BLESSED principles followed — one definition per keyword; C++ formats; the binary default owns every row.
- [x] NAMES.md adhered — names added in resolution recorded in the PLAN (`getMergedValue`, `aliasType`, `getTableFailures`).
- [x] MANIFESTO.md principles applied
- [ ] Doxygen not regenerated; ARCHITECT regenerates and reads the warning count.

### Problems Solved
- **Style translation layer.** COUNSELOR designed a css↔config translation in `PluginEditorLayout` without reading the KANJUT canon; removed, the four bootstrap files restored to the canon.
- **Keyword forms.** Cast tables hard-coded the css var form; the md reader invented camelCase. Now one tag form from any input (`Id::toTag`), definitions are lower-case words, every reader formats.
- **Crashes surfaced by ARCHITECT.** `findColour` and `getFontsStyleValue` asserts (key round trip), `events.get` by a raw id, `getOrCreateTypeface` (font-face attributes are header columns, raw), prefix-layer `perform` false (layers stored raw).
- **Audit 1.** 34 findings triaged. **Audit 2.** 44 findings: accepted 1–9, 11–17, 19, 22, 25, 28, 29, 31, 32 (partly), 34, 44; rejected 10, 18, 20, 21, 23, 24, 26, 27, 32 (EVE text source), 33, 35–43, each with its read in `PLAN-end-eve-config.md` "Audit 2 triage".
- **Mistakes owned.** The translation layer; the camelCase key; a wrong claim of the text of `Id::UIScale`; line-count-driven split requests; stale background agents; font-face attributes formatted.

### State for Continuation
- Run `cast cast/spell.md` in jam (cast tables `ColourId`, `FontId`, `TerminalIndexed`, `core.md` texts, `AnsiColour` removed), end, eve. Then build.
- Test from an empty `~/.config/end/` (shaders kept): EVE and END launch; an invalid eve.md shows the error in the overlay and keeps the defaults (`jam_PluginEditorLayout.cpp:49-77`).
- Regenerate doxygen (`ninja doxygen`, jam `<leader>bd`; jam `docs/xml/index.xml` was 0 bytes during the audit).
- Pathfinder note, not fixed (file outside the sprint set): `jam_MarkdownDocument.h` doc of `getTableRows (const Element&)` says an empty array for a table with no header row; `jam_MarkdownDocument.cpp:124-135` keeps every non-border row.

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261025T135707` — END hosts EVE as CLAP (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (ARCHITECT).

---

## Sprint 95: Prefix Layer in jam CommandManager, One-Way Focus Told by the Owner, Audit Sweep, Lane → TextBlock ✅

**Date:** 2026-11-02
**Duration:** two sessions (one compaction). The start and end times are not recorded.
**Scope (ARCHITECT):** the prefix-layer key method in jam `CommandManager`; focus told one way by the owner (*"UNIDIRECTIONAL"*); `/audit` over the unlogged work, every validated finding resolved; the corner-button bug ARCHITECT surfaced; the doxygen pass; this log.

### Agents Participated
- COUNSELOR — every brief and validation read, the DCF triage of each Auditor and Pathfinder finding, the small edits named below, SPEC/ARCHITECTURE/README/DEBT, END `CLAUDE.md` Version Rule, this log.
- Pathfinder (haiku) — discovery reads (TextEditor jobs, cast key tables), the whatdbg corner trace, the doxygen pass and its correction pass.
- Engineer (sonnet) — CommandManager layers, focus, MatrixComponent `setRow`/orientation, TabView gesture, renames (`node` → `plugin`, `applyZoom` → `setZoom`), the `TextEditorLane` extraction and the Lane → Block rename.
- Auditor (opus) — one pass over the unlogged work.

### Files Modified
**jam:**
- `jam_gui/commands/jam_CommandManager.h` / `.cpp` — private `juce::Timer`; `sequenceLength`, `defaultSequenceTimeout` (1000); `keyPresses` built once in `setKeyPresses` (`Id::sequenceTimeout`, `Id::layer` per row); `invokeKey (key, componentState)` enters a layer, matches a sequence or a key press per layer and mode, clears on a state change; `isInLayer`, `isLayer`, `hasSequence`, `getKey`, `setSequence`, `timerCallback`. ARCHITECT: *"prefix is only tell to "enter a layer", while "gg" is on "default layer""*.
- `cast/gui.md` — Ids `layer`, `sequence`, `sequenceTimeout`, `editorBlocks` (was `editorLanes`); `TextEditorCommandId::prefix` 0x4800016; `normal` key `]`, lone backtick row; `## textEditorLayers` (23 rows) with its brief; sequence wording. `cast/spell.md` — `@textEditorLayer`, counts 23.
- `jam_gui/widgets/jam_TextEditor.h` / `.cpp` — `getDefaultKeys` adds `Id::layer`; `pendingChord` gone; lane layout moved out; every `lane` identifier renamed to the block family (`addBlocks`, `setBlocks`, `setLiveBlock`, `removeHeadBlocks`, `getBlockNumber`, `getBlockNumberAt`, `getBlockStart`, `getBlockTop`); unused `editorMode` holder removed (`map::EditorMode` is read only by its enum constants).
- `jam_gui/widgets/jam_TextBlock.h` / `.cpp` (new) — the old nested `Lane` and its static layout functions (`setBlock`, `setRows`, `setLineRows`, `setTableRows`, `setGridRows`, `setImageRows`, `addRowHeights`, `addRuns`, `getContentWidth`, `isGridLine`, `getGutterWidth`) moved verbatim; Auditor #17 (LANGUAGE.md:35: *"If the object has more than one responsibility, decompose it into objects"*). Name ruled by ARCHITECT: *"TextBlock?"*, *"in the codebase Block is used everywhere"*.
- `jam_terminal/document/jam_TerminalSwap.h` / `.cpp` — `linesPerLane` → `linesPerBlock`.
- `user-modules-info.md:1030`, `:1063` — submodule rows for `jam_TextBlock.h` / `.cpp` (cast writes the include region from them).
- `jam_gui/layout/jam_OwnerComponent.h` / `.cpp` — `addMouseListener (this, true)`; `mouseDown` sets the focused child; `add`/`remove` write no focus and grab nothing; `valueTreePropertyChanged` writes `Id::focus` to each child from the focused-child parameter and own `Id::focus`.
- `jam_gui/layout/jam_OwnedComponent.*` — focus handlers removed (the owner tells).
- `jam_gui/layout/jam_MatrixComponent.h` / `.cpp` — `childAdded` focuses and lays out (ruling (b)); `childRemoved` focuses the survivor; private `setRow` serves `split`, `rotate`, `childRemoved`; orientation stored as an int key.
- `jam_gui/layout/jam_PaneEdge.h` — `int newOrientation`, `Parameter<int>`; `jam_TabbedComponent.cpp` — `changeCallback` without grab.
- `jam_gui/layout/jam_PaneComponent.cpp:133-135` — corner menus take the mouse (`setInterceptsMouseClicks (false, false)` removed); `juce::Button` hovers and clicks by itself (`juce_Button.cpp:467`, `:484-497`).
- `jam_gui/windows/jam_Window.h` / `.cpp` — `visibilityChanged` sets the macOS buttons; `const juce::String& name`.
- `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp` — engine rule installs context-factory, peer-deleted, key and mouse hooks; keys rule removes `normal` and `prefix`.
- `jam_plugin_bootstrap/view/jam_PluginEditor.h` / `.cpp` — focus callback grabs when the focus parameter is true; key listener gone; `applyZoom` → `setZoom` (ARCHITECT: *"there is no fucking apply apply bullshit, those are parameter contract. set** not fucking apply"*).
- Doxygen pass and correction pass over every header above.

**end:**
- `Source/config/keys.md` — `layer` column, 11 `prefix` rows, `prefix` and `sequenceTimeout` rows. `cast/bimaps.md` — `prefix` 0x4700020. `cast/identifiers.md` — `joinPreview`, `swapPick` rows removed.
- `Source/ui/ENDView.h` / `.cpp` — `newSession` tells the session row `Id::focus`; zoom actions through `setZoom`; `normal` action → `setMode`; `getBackgroundRenderParams` removed; unused `position` holder removed.
- `Source/ui/EditorView.h` / `.cpp` — `setZoom`, `setMode`; private `getParameter`/`setParameter`; the row's `Id::focus` sets the plugin's `focus` parameter; the hosted editor goes to the back (`addAndMakeVisible (*editor, 0)`, `:141`) so the pane's corner menus are on top.
- `Source/ui/TabView.h` / `.cpp` — `getEdge`, `isInward`; gesture from the event payload; directional swap options; the pane found with `findParentComponentOfClass` (`:96`, `:105`, `:147-149`); redundant `overlayAxisLine` and `position` holders removed (`jam_MessageOverlay.cpp:67`, `jam_MatrixComponent.h:206` hold them).
- `Source/ui/SessionView.*` — `setZoom`. `Source/plugins/Session.*`, `Nexus.*` — `node` → `plugin`, `audioInputId`, `audioOutputId`, `midiInputId`; `applyToPlugins` deleted. `Source/Main.cpp` — `getPostProcessRenderParams` removed.
- `SPEC.md`, `ARCHITECTURE.md`, `README.md`, `DEBT.md`, `CLAUDE.md` (Version Rule: always 0.0.1).

**eve:**
- `Source/layout/parameters.md` — `focus` bool row. `Source/layout/ViewLayout.md` — `sequenceTimeout` row, keys prose, version `0.0.1` (ARCHITECT: *"CHANGE MOTHERFUCKER"*).
- `SPEC.md` (focus, blocks), `PLAN-Grand-Scheme.md` §4.3.

### Alignment Check
- [x] BLESSED principles followed — focus flows one way from the owner; the model holds layer and sequence on the component row.
- [x] NAMES.md adhered — `node`, `apply` on parameters, and `Lane` removed by ARCHITECT's rulings.
- [x] MANIFESTO.md principles applied
- [ ] No build. Cast not re-run in jam, end, eve. ARCHITECT runs cast, builds and tests.

### Problems Solved
- **Prefix keys.** The backtick enters the `prefix` layer; sequences live in the default layer; a key with no match in a layer is dropped.
- **Focus.** Owners tell `Id::focus`; children never grab; END tells the plugin's `focus` parameter.
- **Auditor findings.** Resolved: #1, #2, #3, #4, #5, #7, #14, #15, #16 (setRow, orientation), #17 (TextBlock), #18, #21, D1–D18. Rejected by read with clause: #6, #8 (each table one fact, keyed once by `TextEditorCommandId`, `gui.md:266`), #10, #11, #12, #13, #16 join length (cause fixed, a count remains), #19 (transient colour, MANIFESTO S), #20.
- **Pathfinder findings.** F2 unused holders: three removed (`TabView.h` ×2, `ENDView.h` position, `TextEditor.h` editorMode), the others back a `getInstance()` read. F1 rejected (state is the Model row, `jam_CommandManager.cpp:113-114`). F3 rejected (MANIFESTO D). F4 = the cast include region (`user-modules-info.md`).
- **Corner buttons hidden.** The pane's corner menus were added before the hosted editor, which went in front (`jam_PaneComponent.cpp:136`, `EditorView.cpp:141`), and they refused the mouse (`jam_PaneComponent.cpp:134`).
- **Mistakes owned.** Focus patches to working code (reverted), a ViewLayout version bump (reverted), `swapPick`/`joinPreview` and the `"swap:"` string (removed), and a wrong claim that nothing JUCE can draw over a hosted editor (withdrawn).

### State for Continuation
- Run `cast cast/spell.md` in jam (include region from `user-modules-info.md:1030`, `:1063`; `Id::editorBlocks`), end, eve. Then build.
- Test on a hosted pane: corner hover icon, left/right click menu, corner drag split/join (`EditorView.cpp:141`, `jam_PaneComponent.cpp:133-135`, `TabView.cpp:94-167`).
- Regenerate doxygen (`ninja doxygen`, jam `<leader>bd`) and read the warning count.

### Debts Paid
- `DEBT-20261025T214955` (eve) — OSC command names reach the commands: the command FIFO drains into `EVEView::invokeCommand` (`eve/Source/EVEView.cpp:249-288`).

### Debts Deferred
- `DEBT-20261025T135707` — END hosts EVE as CLAP (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86 (ARCHITECT).

---

## Sprint 94: ENAX Plan 2 — Owner-Scoped Command Ids, Per-Instance Mode, END Drives the Plugin by Host Parameter; Wholesale Audit ✅

**Date:** 2026-11-02
**Duration:** one session. The start and end times are not recorded.
**Plan:** `PLAN-enax-command-id.md` (locked), with the open items of Sprint 93 folded in by ARCHITECT's order: *"fold outstanding to PLAN 2, execution"*, *"/audit wholesale until /log"*. Plan 1 Steps 9 and 10 are Plan 2 Steps 11 and 10.

### Agents Participated
- COUNSELOR — the fold (`PLAN-enax-command-id.md`), every brief and validation read, the trace spec, the small edits named below, SPEC/ARCHITECTURE/eve SPEC, the Auditor triage, this log.
- Pathfinder (haiku) — discovery reads (jam key path, cast tables, END host, file watcher, AU format, MoltenVK, command tables), the Step 1 trace run, the doxygen pass.
- Engineer (sonnet) — Steps 2–9, 11, the Auditor fixes, the one-TU moves, `rm` of four files.
- Librarian (haiku) — FSEvents dispatch-queue ownership (SDK headers, LLVM, Git, fswatch, Chromium).
- Auditor (opus) — one wholesale pass over Plan 1 and Plan 2: 38 findings.

### Files Modified
**end:**
- `PLAN-enax-command-id.md` — fold: per-instance mode (Step 2), paste moved to the TextEditor (Step 9), Step 11 (format from the file), Step 12 (watcher teardown), Step 10 docs; every citation re-read after `6c017a1`.
- `Source/plugins/Session.cpp:44-54` — `pluginFormatName` is the `getName()` of the first format whose `fileMightContainThisPluginType` accepts the file (`juce_AudioPluginFormatManager.cpp:139-152`); the hardcoded VST name is gone. `Session.h:46-51` — `applyToPlugins`.
- `Source/ui/ENDView.cpp` — one TU (CODING): the bodies of `ENDActions.cpp` and `EventRegistration.cpp` moved in (635 lines). `normal` action sets the focused plugin's `Mode` parameter (`:442-460`). Zoom actions set the plugin's `Zoom` parameter by a normalised `zoomStep`, every pane or the focused pane when `exclusiveZoom` (`:297-354`). `closeTab` removes the plugins of the closed tab (`:208-226`).
- `Source/ui/EventRegistration.cpp` — deleted. `Source/action/ENDActions.cpp` / `.h` — only `ENDActions::run` (moved out of the header).
- `Source/ui/EditorView.h` / `.cpp` — pane `Id::zoom` and `defaultZoom`/`zoomMin`/`zoomMax` deleted; `getParameter` by host-visible name with `compare` (`:98-108`); width/height through `jam::Format::toParameterName`; listens to its own row.
- `Source/ui/TabView.cpp:203`, `:227`, `Source/lookAndFeel/ENDLookAndFeel.h` / `.cpp` — `getCodeMetrics()` takes no zoom; `drawPaneOutline` asserts the downcast.
- `Source/plugins/Nexus.cpp:123-124` — named archive and AU extensions.
- `cast/bimaps.md` (`normal 0x470001f`), `cast/identifiers.md` (`exclusiveZoom`), `cast/cmake.cast:306` (`CAST_PLUGINS_HASH` on `Nexus.cpp` only).
- `Source/config/keys.md` (`normal` = `` `] ``, zoom descriptions), `Source/config/display.md` (`zoomStep` normalised, `exclusiveZoom`).
- `SPEC.md`, `ARCHITECTURE.md` — host format, keys, commands, host parameters, ENAX ids and hooks, focus chain, pane rows under EDGE rows, every stale `pluginId`/scan/CLAP line; each line cites code.
- Headers: doc pass (`EditorView.h`, `Session.h`, `Nexus.h`, `ENDView.h`, `ENDLookAndFeel.h`, `ENDActions.h`).

**jam:**
- `cast/gui.md` — `EditorMode` 0-based (`:232-234`); `copy`, `paste` ids; `## TextEditorCommandId` (`0x48…`), `## textEditorKeys mac` / `windows`, `## textEditorModes` (`:253-435`). `cast/plugin_bootstrap.md` — `## PluginEditorCommandId` (`0x49…`), `## pluginEditorKeys`; `master` moved to `cast/core.md:108`. `cast/spell.md:134-136`, `:937-964`, `:1411-1431`; `cast/code.cast` — `platform-value-lookup-table`.
- `jam_gui/commands/jam_CommandManager.h` / `.cpp` — no shared `mode`; `setKeyPresses` builds the sheet once (one row per key and mode, `Id::id`, `Id::mode` key); `invokeKey` matches chords and key descriptions by the caller's own mode; only modeless rows enter the shared key mappings; `getCommandForName` public.
- `jam_gui/widgets/jam_TextEditor.h` / `.cpp` — mode key; `jam::Model::Object` with its own `mode` Value and a `ValueAttachment` on `MODE`; no own key binding; TextEditorCommandId commands; `getDefaultKeys`; `copy`; `paste` (bracketed when DEC 2004). `jam_TextEditorStatusBar.cpp` — mode key.
- `jam_gui/layout/jam_OwnedComponent`, `jam_OwnerComponent`, `jam_PaneComponent` — bodies in new `.cpp` files (`jam_gui.cpp:6-8`); the owner writes focus 0 to the previous child only; `cornerJustifications` defined in the `.cpp`.
- `jam_terminal/keyboard/jam_TerminalKeyboard.h` — `bracketedPasteStart`, `bracketedPasteEnd`.
- `jam_core/text/jam_Format.h` / `.cpp` — `toParameterName`. `jam_core/file/jam_Listener.cpp` — no unschedule before invalidate; the queue and the paths are released. `jam_core/utilities/jam_PluginHostType.cpp` — format.
- `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.h` / `.cpp` — hooks read END's variables through their addresses at each call; the keys rule takes the key sheet and drops `normal`; `addOrReplace` ranges. `layout/jam_PluginEditorLayout.h` — `keys` (TextEditor defaults, host rule applied). `parameter/jam_ParameterLayout.h` / `.cpp` — `toParameterName`. `view/jam_PluginEditor.h` / `.cpp` — PluginEditorCommandId commands, `getDefaultKeys`, the one binder `addCommands` after `initialiseView`, reload through it. `PluginHostType` per-event sites made static (`jam_PluginEditor.cpp`, `jam_AudioModel.cpp`, `jam_PopupMenu.h`, `jam_ViewEditor.cpp`, `jam_ViewPanel.cpp`). Doc pass over the touched headers, `jam_ViewManager.h`, `jam_ModelLayout.h`.

**eve:**
- `Source/EVEView.cpp` — one TU (the bodies of `EVEViewComponents.cpp`, `EVEViewPanelCallbacks.cpp`, `EVEViewRegistration.cpp` moved in, files deleted, `project-info.md` rows removed); commands = PluginEditor's + EVE's; shell commands by any registered name (`:150-151` before the move). `EVEViewComponents` `setMode` call removed.
- `cast/identifiers.md` — `## CommandId` keeps `clear`, `preview`. `Source/layout/ViewLayout.md` — version 0.0.2, keys keep `clear`, `preview`. `Source/layout/parameters.md` — `mode` choice, default 2.
- `Source/EVETerminalProcessor.cpp` — quits only in the standalone app; no manual `cancelPendingUpdate`.
- `SPEC.md` — keys summary, command protocol, mode parameter, terminal consumers. `Source/EVEView.h` — doc pass.

### Alignment Check
- [x] BLESSED principles followed — the mode lives in each instance's node; one key entry per caller; END tells by host parameter.
- [x] NAMES.md adhered — `candidate`, shadowing locals and "resolve" prose corrected in-sprint.
- [x] MANIFESTO.md principles applied
- [ ] No build. Cast was not re-run. ARCHITECT runs `cast` (jam, eve, end), builds and tests.

### Problems Solved
- **Mode per binary.** `CommandManager::mode` was one Value per binary; the last bound editor owned every pane's mode. The mode now lives only in each editor's node and the host parameter.
- **Command id collision.** JUCE asserts on two commands with one id (`juce_ApplicationCommandManager.cpp:61`, `:71`). TextEditor ids are `0x48…`, PluginEditor ids `0x49…`; the tables are read by row through `Bimap::indexOf`.
- **SharedInstance lifetime.** The new maps had no live holder at first use (layout constructor); members and function-local static holders added.
- **FSEventStream assertion.** The destructor unscheduled the stream before `FSEventStreamInvalidate`; the SDK forbids it (`FSEvents.h:1209-1213`). The queue leak is closed (`queue.h:1057-1060`).
- **Pane format.** `EVE.vst3` and `EVE.component` load through the format that accepts the file.
- **Auditor (38 findings).** Fixed: 1, 3, 4, 5, 6, 7, 8, 9, 11, 15, 19, 21, 22, 25 (own row), 28, 34, 35, 36, 37, 38. Rejected by read: 13 (END is the source of both ranges), 15 out-parameter (ruling *"Registry returns value … through jam::Function::Map argument forwarding"*, `PLAN-enax-plugin-host-type.md:180`), 17 (sibling `spell.md:133`), 18 (`#else` keeps Linux compiling), 20 (ruled in Sprint 93), 23 (creation), 26 (MANIFESTO E example asserts and nests), 27 (a third-party plugin can have no editor), 29 (numbers only), 30 (END tells), 31, 32 (*"it STILL HANDLE its own hjlk per its state"*), 33 (no read confirms).

### State for Continuation
- ARCHITECT: run `cast cast/spell.md` in jam, eve and end; build; test the Plan 2 ARCHITECT Gate (`PLAN-enax-command-id.md`) for `EVE.vst`, `EVE.vst3`, `EVE.component`.
- Step 1 trace: the 2026-11-02 run had no key input (`END outcome=timeout stops=0`, spec `scratchpad/trace-keypath.json`). It runs again with ARCHITECT at the keyboard. Auditor finding 2 (a lone `` ` `` under END is lost: END keeps it as its chord prefix, `ENDView.cpp:71`, and never forwards it) is a key-path fix, and the plan requires a trace stop for each key fix (`PLAN-enax-command-id.md:67`).
- Rulings that conflict with the locked plan or CONTRACT (stop set, both citations):
  1. Finding 14 — *"if it was jam wiget parameter then it must be at jam"* (`PLAN-enax-command-id.md:23`) against Plan 2 Step 6.1 (`mode` in `eve/Source/layout/parameters.md`, like `zoom` at `:11`).
  2. Finding 16 — MANIFESTO S (a value appearing more than once) against Plan 2 Step 3.2 (full `textEditorKeys mac` / `windows` tables, `jam/cast/gui.md:290-399`).
  3. Finding 10 — *"must be crossplatfrom"* (`PLAN-enax-plugin-host-type.md:33`) against the macOS-only focus patch (`jam/patch/juce-plugin-window-focus-hook.patch`) and DEBT-20260912T150000.
  4. Finding 12 — Plan 2 constraint *"`jam_gui` never names `jam_plugin_bootstrap`"* (`:35`) against the guarded `TextEditor::bindToModel (…, AudioModel&)` (`jam_TextEditor.h:142-154`).
  5. Finding 24 — END is the size author (`PLAN-enax-plugin-host-type.md:180`) against `PluginEditor::resized` writing `Width`/`Height` too (`jam_PluginEditor.cpp` `resized`).
  6. Finding 25 — `EditorView` reaches `Nexus::getInstance()` (`EditorView.cpp:31`, `:79`) against ARCHITECTURE.md "never by members reaching sideways, upward, or across".
- L crossings after the one-TU moves: `ENDView.cpp` 635 lines, `jam_TextEditor.cpp` ~1720 lines (MANIFESTO L: rethink the decomposition).
- MoltenVK: `Class MVKBlockObserver is implemented in both …/END and …/EVE.vst/Contents/MacOS/EVE` on each EVE load (END run log 2026-11-02 06:02:34). END and EVE each link `libMoltenVK.a` (`end/CMakeLists.txt:333-341`, `eve/CMakeLists.txt:401-409`). Recorded as the Plan 1 gate asks (`PLAN-enax-plugin-host-type.md:229`).
- `ViewLayout.md` version 0.0.2 replaces each user's seeded `~/.config/end/eve.md` on first load.
- Doxygen not regenerated (`ninja doxygen`).

### Debts Paid
- `DEBT-20261025T214955` — Action List replaced by CommandManager: END dispatches through `jam::CommandManager` and the action map behind `perform` (`ENDView.h:69`, `ENDView.cpp:116-119`).
- `DEBT-20261009T112447` — END keys through `jam::CommandManager` (`ENDView.cpp:17-18`, `:69-72`).

### Debts Deferred
- `DEBT-20261025T135707` — END hosts EVE as CLAP; ENAX's CLAP format is future work (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (ARCHITECT: *"windows build still miles ahead. defer."*).

---

## Handoff to COUNSELOR: END Host Contract — EVE Hosted at END as VST2

**From:** COUNSELOR
**Date:** 2026-10-31
**Status:** Ready for Implementation (plan written, not locked, not executed)

### Read First
1. `PLAN-end-host-contract.md` — the next plan, 8 steps, with every ruling and fact cited. ARCHITECT approves it before execution.
2. `PLAN-eve-shared-engine.md` — the running sprint. Steps 1-7 and 10 are done. Its Step 8 (gate C), Steps 9-11 (CLAP), Steps 12-13 (daemon), Step 14 (docs) run after the new plan.
3. The previous handoff below (two consecutive PLANs), for the sprint-N context.

### Context
- Objective: END hosts EVE as an in-house plugin and paints it through END's `VulkanGraphicsEngine` (one device), with END owning focus and keys.
- First pane renders EVE's terminal with a transparent background (ARCHITECT: "plugin loaded, on first pane works. terminal working, transparent background", END Release + EVE Release, VST3 at that time).
- END now loads only `EVE.vst` (VST2), one flow per pane. That build has not been run by ARCHITECT yet.
- The rest of the in-house rules (engine, keys, focus, transparency) are scattered over five sites. ARCHITECT ruled one SSOT: `jam::PluginHostType` derived from `juce::PluginHostType`, END registered as a known JUCE host, one extension export.

### Completed
- **PLAN-eve-shared-engine Step 1:** no split crash. A whatdbg run exited with code 0 and no exception stop.
- **Step 2:** `MenuTarget`, `JAMSheetDelegate`, `JAMSheetContentView` are `juce::ObjCClass` subclasses (`jam_Menu.mm:7-73`, `jam_ModalSheet_mac.mm:15-69`). `jam_gui.mm:1-2` defines `JUCE_CORE_INCLUDE_NATIVE_HEADERS` / `JUCE_CORE_INCLUDE_OBJC_HELPERS` (sibling `jam_clap.mm:1-2`).
- **Step 3:** END exports `jam_host_context_factory`, `jam_host_peer_deleted_callback`, `jam_host_key_pressed_callback` (`end/Source/Main.cpp:29-63`, inside `/** @cond */`). `jam::ClapServices` is deleted (`jam_clap.h`, `Nexus`).
- **Step 4:**
  - EVE's host branch (`jam_PluginEditorLayout.h:117-134`).
  - Typeface identity: both contexts intern the typeface of the current font (`jam_CpuLowLevelGraphicsContext.h:267-291`, `jam_VulkanLowLevelGraphicsContext.cpp:1695-1706`).
  - CPU context temporary images are software (`jam_CpuLowLevelGraphicsContext.h:293-296`).
- **Step 5 (partial):**
  - `jam/patch/juce-key-pressed-hook.patch` (`ComponentPeer::externalKeyPressedCallback`).
  - `ENDView::keyPressed` is the single END resolver (`ENDView.cpp:69-72`); `addKeyListener` removed from `ENDView`.
  - EVE `editorWantsKeyboardFocus TRUE` (`eve/project-info.md:110`).
- **Step 6:** `jam/patch/juce-plugin-transparency-hook.patch`: `ComponentPeer::getHostFunction`, VST3 ContentWrapperComponent transparent under END, host VST3/AU windows and `InnerNSView` non-opaque. VST2 client wrapper NOT covered.
- **Step 7:** `Nexus` split into `Nexus.h` + `Nexus.cpp`. `registerAudioUnit` was added and then deleted by ARCHITECT's VST-only ruling.
- **Step 10:** `ClapPluginInstance::hostRequestProcess` activates an inactive plugin on the message thread (`jam_ClapPluginInstance.cpp:443-459`).
- **One flow, VST only:**
  - `Session` is a `ValueTree::Listener` on its own state. Each pane row added creates the pane's plugin synchronously (`Session.cpp:31-90`).
  - `files::evePlugin` = `EVE.vst` from `cast/files.md:31`.
  - Deleted: `Nexus::evePluginId`, `Nexus::createPlugin`, `Id::newPlugin` (action and `cast/identifiers.md` row), `registerAudioUnit`.
- **EVE pack:** Release and Debug both pack into `end/Source/plugins/` (`eve/cast/cmake.cast:490-522`, Release gate removed). Both `<leader>br` (`--no-sign`) and `<leader>bb` (`--debug`) build the `post-build ALL` target (`~/.config/nvim/lua/core/cast-build.lua:22-32`, `eve/project-info.md:132-142`).
- **Doxygen warning fix:** `Main.cpp:29`, `:37` `/** @cond */ … /** @endcond */` (JUCE `juce_ObjCHelpers_mac.h:395`, `:437`).

### Remaining
1. ARCHITECT approves `PLAN-end-host-contract.md` (`/go`). COUNSELOR executes Steps 1-8 with @Engineer, validating each step; @Auditor once at the end.
2. ARCHITECT runs `cast cast/spell.md` in END to generate `files::evePlugin` and drop `Id::newPlugin`, then builds END. The one-flow change (`Session.cpp:31-90`) is untested at runtime.
3. Known defects the new plan addresses:
   - `jassert` at `juce_Component.cpp:3033` from `EVEView.cpp:12` (Plan Step 6).
   - END chords fail with EVE focused; visual `y` and ESC fail (Plan Step 7, trace first).
   - VST2 client wrapper is opaque/black (`juce_audio_plugin_client_VST2.cpp:931`, `:942`, `:956-959`) (Plan Step 5).
4. Not yet attributed, need runtime evidence:
   - **OMP help at shell start, intermittent.** Shell spawn is identical in standalone and hosted (`jam_TerminalTTY_posix.cpp:46-62`, `:155-169`); only the inherited process environment differs. Capture the hosted zsh environment.
   - **Foreground looks 0.5 opacity.** No code path dims glyphs: scene composite `1.0f` (`jam_VulkanGraphics.cpp:1419`); no `postProcessing` row in `~/.config/end/display.md`; `backgroundOpacity` 0.5 only scales END's background shader (`background_combine.frag:43`). Needs a pixel measurement or screenshot with a known colour.
5. Gate C, CLAP, daemon and docs remain in `PLAN-eve-shared-engine.md` Steps 8-14 (the CLAP target premise is wrong: whelmed has no CLAP target, `grep -i clap` over the whelmed tree is empty).
6. Scratch backups left by Engineers are outside the project tree (previous handoff).
7. DEBT: pay DEBT-20261025T135707 (CLAP clock) and DEBT-20261025T214955 / DEBT-20261009T112447 at `/log`; DEBT-20260912T150000 deferred by ARCHITECT.

### Key Decisions
- *"consolidate all host-aware detection into our own jam::PluginHostType"*
- *"patch and derive? so downstream deterministically no scattered handroll method, but through a single SSOT"*
- *"then we could set rules for END-EVE contract, including so far vulkan, focus management, keyboard management (command manager) etc"*
- *"just follow juce pattern, VERBATIM. this is not our handroll. this is juce::PluginHostType, juce native contract for host detection. we only "register" END as one of the known host"*
- *""magic strings" is an exception here because conforming to framework extension is topmost priority. at callsite, the check still could use static declared variable from cast manifest, just like other string/id type check"*
- Hook channel: *"One export, id lookup"*.
- Chord collision: *"END's table governs"*.
- *"END as HOST should control the FOCUS. END already always control new pane created to be always the FOCUSED one. this already tracked as END state machine. hence eve's contract as in-house default terminal plugin is when hosted at END, it should never manage its own focus, never steal focus."*
- *"these rules should not be scattered, BUT governs by ONE RULE. one method? one check? ONE SSOT? so later if we need to enforce new rule, we only need to register, not manually branching."*
- *"so END pane focus management actually only need to forward, focused PANE give the keyboard focus to its loaded plugin"*
- *"ONE flow, ONE path, ONE action, ONE SSOT, new pane = load eve."*
- *"VST ONLY FIRST. no VST3, no AU. FOCUS. and no harcode, no magic strings, DESIGN BY CONTRACT."*
- *"loading EVE.vst == new pane. is SPEC. but the string name, the path, etc table driven, never inline on the code. follow established pattern VERBATIM"*
- *"NO HACK, NO WORKAROUND, NO PATCHING BULLSHIT without comprehensive understanding. must be DESIGN BY CONTRACT"*
- *"what shared juce means in that context is, compiling juce as dynamic library, and both host and client use that. that's BAD DESIGN"*
- *"is EVE debug build also run post build to copy to END plugins? check its cast manifest, ensure both release and debug build always run post build copy to end repo"*

### Files Modified
- **jam:**
  - `jam_gui/jam_gui.mm`, `jam_gui/menu/jam_Menu.mm`, `jam_gui/windows/native/jam_ModalSheet_mac.mm`
  - `jam_clap/jam_clap.h`; `jam_clap/services/` deleted; `jam_clap/format/jam_ClapPluginInstance.cpp`
  - `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h/.cpp`
  - `jam_graphics/context/jam_CpuLowLevelGraphicsContext.h`, `jam_vulkan/context/jam_VulkanLowLevelGraphicsContext.cpp`
  - `patch/juce-plugin-transparency-hook.patch` (new), `patch/juce-key-pressed-hook.patch` (new), `user-modules-info.md` (two patch rows)
- **EVE:** `project-info.md` (`editorWantsKeyboardFocus TRUE`), `cast/cmake.cast` (post-build ungated)
- **END:**
  - `Source/Main.cpp` (exports)
  - `Source/plugins/Nexus.h`, `Source/plugins/Nexus.cpp` (new), `Source/plugins/Session.h/.cpp`
  - `Source/ui/ENDView.cpp`, `Source/action/ENDActions.cpp`
  - `cast/files.md`, `cast/identifiers.md`
  - `PLAN-end-host-contract.md` (new)

### Open Questions
None. Chord handling is resolved by ARCHITECT: *"CommandManager should have "merge" or "coalesce" design. this definitely could be designed with context-aware focus."* The design is in `PLAN-end-host-contract.md` Step 7.3: one resolver (END's `jam::CommandManager`) over END rows coalesced with the focused plugin's rows, registered on focus through extension `jam.command-coalesce`; END rows win collisions.

### Next Steps
- When ARCHITECT says `/go` on `PLAN-end-host-contract.md`, start Step 1 (the `juce-plugin-host-type-hook.patch`).
- Before Step 7, ARCHITECT builds END Debug and EVE Debug (same configuration on both sides).

---

## Handoff to COUNSELOR: Relative Toolchain Paths in EVE and END

**From:** COUNSELOR
**Date:** 2026-11-09
**Status:** Blocked — waits for the cast working-directory change

### Read First
1. `~/Documents/Poems/dev/cast/RFC-toolchain-working-directory.md` — the rule cast will implement, the data rows after the change ("The data after the change"), and the 5 open questions.
2. Sprint 98 above — the lanes and flags that the rows carry now.
3. `~/Documents/Poems/dev/eve/project-info.md` `## toolchain` (`:119-201`) and `~/Documents/Poems/dev/end/project-info.md` `## toolchain` (`:105-135`) — the rows to rewrite.

### Context
- ARCHITECT's rule: *"the implementation must be written now, so it was written UNIVERSALLY. no hardcoding of any platform"*.
- The EVE rows and three END rows hold macOS absolute paths, for example `eve/project-info.md:126` `-S /Users/jreng/Documents/Poems/dev/eve -B /Users/jreng/Documents/Poems/dev/eve/Builds/Release …` and `end/project-info.md:120` `cast | /Users/jreng/Documents/Poems/dev/eve/spell --vst-debug`.
- They are absolute because each toolchain row runs in the caller's folder (`cast/Source/Toolchain.cpp:21-22`, `cast/SPEC.md:939-941`), so EVE's rows run in END's folder when END calls EVE.
- ARCHITECT selected the fix in cast: *"B. write RFC to cast repo"*. A cast COUNSELOR session works on it (ARCHITECT: *"counselor at cast just started"*).

### Completed
- `cast/RFC-toolchain-working-directory.md` written, with the inventory of all 24 `## toolchain` tables and their relative-path base.
- Sprint 98: every EVE `cmake` row states its architecture and format; END's debug and no-sign rows call `--vst-debug` and `--vst-no-sign` (`end/project-info.md:120`, `:128`).

### Remaining
1. Cast COUNSELOR — implement the working-directory rule that ARCHITECT selects (RFC Open Question 1).
2. COUNSELOR (END/EVE) — rewrite the EVE toolchain table (`eve/project-info.md:119-201`): every `/Users/jreng/Documents/Poems/dev/jam/cast/spell.md` → `../jam/cast/spell.md`; `-S /Users/jreng/Documents/Poems/dev/eve -B /Users/jreng/Documents/Poems/dev/eve/Builds/<X>` → `-S . -B Builds/<X>`; `-C /Users/jreng/Documents/Poems/dev/eve/Builds/<X>` → `-C Builds/<X>`. All other cells stay. This holds under RFC option (a). Under option (b) the paths are one level up (`../../jam/cast/spell.md`, `-S .. -B ../Builds/<X>`, `-C ../Builds/<X>`).
3. COUNSELOR (END/EVE) — rewrite `end/project-info.md:110`, `:120`, `:128` to `../eve/cast/spell.md`, `../eve/cast/spell.md --vst-debug`, `../eve/cast/spell.md --vst-no-sign` (RFC Open Question 5: the direct path, not the `eve/spell` link). Under option (b), END's relative rows (`:112`, `:114`, `:116`, `:118`, `:122`-`:134`) also move one level up.
4. ARCHITECT — fresh `cast cast/spell.md` in EVE, then in END, from any folder; then the Sprint 98 checks (`State for Continuation`).

### Key Decisions
- *"the implementation must be written now, so it was written UNIVERSALLY. no hardcoding of any platform. correct foundation, the actual testing/hardening will eventually get on actual windows machine"*
- *"my DX is identical on any machine, any platform"*
- *"B. write RFC to cast repo"*
- *"ARCHETYPE is for ANY PROJECT. why hardcode eve into its docs?"*
- *"machinist done its job. counselor at cast just started. are you done with yours?"*

### Files Modified
- `~/Documents/Poems/dev/cast/RFC-toolchain-working-directory.md` — new.

### Open Questions
- Which working directory cast uses: the folder of the file that declares the table (a) or the manifest's folder (b). Decider: ARCHITECT (cast RFC Open Question 1). The row text of Remaining 2 and 3 depends on it.
- jreng-filter-strip's `cast/signing.md:48-52` changes under (a). Decider: ARCHITECT (cast RFC Open Question 2). It is not in the END/EVE scope.

### Next Steps
- Start when ARCHITECT says the cast working-directory change is in. Read the cast SPEC §6.9 text that lands, then do Remaining 2 and 3 with @Engineer, and validate each row by a read.

