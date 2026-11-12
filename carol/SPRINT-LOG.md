# SPRINT-LOG

---

## Sprint 100: Selection Above the Cell Background, One Author of the Plugin `focus` Row, NORMAL Entry at the Terminal Cursor ✅

**Date:** 2026-11-11
**Duration:** 02:02 recorded before one compaction (2026-11-10T17:01Z to 19:03Z). The time after the compaction is not measured.
**Scope (ARCHITECT):** three runtime defects: the selection highlight is hidden on cells that have a background; keyboard focus is lost after a tab close, a new tab, and a hide and show of the plugin window; NORMAL mode starts at the top when the default mode is TERMINAL. Contract: *"no hack, no patch, no manual flags, no manual state management"*; *"SSOT parameter with Model, listener react. be careful because there's also event coming from juce native component interaction"*; *"SSOT callback with CommandManager"*; *"direct lookup SSOT command"*; *"we dont fight juce nor reinvent new pattern, new semantics, we integrate seamlessly to established UNIDIRECTIONAL data flow pattern"*; *"FULLY EVENT DRIVEN. listener react"*. Rulings: the selection paints *"After the runs"*; *"make only ONE sole author. Orchestrator always TELL"*; *"Session could set the parameter, then SessionView could forward unidirectional chain, per juce native contract"*; the parameter is *"Plugin's focus row"*; *"Pane wants no focus"*; finding 5: *"5. FIX IT"*, then *"why we must patch juce?"*; *"you update auditor role"*.

### Agents Participated
- COUNSELOR — the cause reads, the plan, every validation read, the audit triage, the `ARCHITECTURE.md` edits, the `auditor.md` edit, this log.
- Pathfinder (haiku) — discovery reads (paint order, focus chain, VST2 open and close, plugin formats, the JUCE hierarchy and grab paths), the doxygen passes.
- Engineer (sonnet) — the jam, END and EVE code changes. One run (Windows hunks for the JUCE focus patch) was stopped before any write, after ARCHITECT's question.
- Auditor (opus) ×1 — one pass, 20 findings.

### Files Modified (13 total)
**jam:**
- `jam_gui/widgets/jam_TextEditor.cpp:2090-2128` — `Content::paint` draws the background, the graphics and the fills. `Content::paintOverChildren` draws the runs, then the selection, the active list, the visual-block fill, the caret and the link labels. The opaque cell background of `GlyphArrangement::Run::paint` (`jam_GlyphArrangement.cpp:647-666`) no longer covers the selection.
- `jam_gui/widgets/jam_TextEditor.cpp:2183-2200` — `paintActiveList` tests `getMode() == map::EditorMode::open` itself.
- `jam_gui/widgets/jam_TextEditor.cpp:471-474`, `:893-910` — the `Id::normal` action only sets the mode. The `Id::mode` reaction calls `setCursorToTerminal()` when the mode is `normal` and `Id::defaultMode` is `terminal`. A host that writes the mode row gets the same cursor placement as the key.
- `jam_gui/layout/jam_PaneComponent.cpp:12-25`, `:119-123` — both constructors lost `setWantsKeyboardFocus (true)`. `CornerButton` calls `setWantsKeyboardFocus (false)`.
- `jam_gui/widgets/jam_TextEditor.h`, `jam_gui/layout/jam_PaneComponent.h` — doxygen.

**end:**
- `Source/plugins/Session.h`, `Session.cpp:43-49`, `:68-93`, `:142` — public `setRow (uuid, row, value)`; private `registerEvents()` with the `events` map (`Id::plugin`, `Id::focus`); `valueTreePropertyChanged` dispatches through it; `addPlugin` writes the `focus` row from the pane row. `Session` is the one writer of the plugin's `focus` row.
- `Source/ui/EditorView.h`, `EditorView.cpp:207-210` — both focus writes deleted; `setRow` forwards to `Session::setRow`.
- `Source/ui/SessionView.h`, `SessionView.cpp:56-71`, `:90-103`, `:141-148` — `currentTabChanged` and the `Id::name` / `Id::focusedPane` changes call `focusPane()`, which grabs the keyboard focus for the default component of the active tab. The `applyTabName` guard is removed.
- `ARCHITECTURE.md` — Singular focus, the new paragraph "Keyboard focus follows the Model on one chain", the Host automation row, the Focus bullet (`:831`).

**eve:**
- `Source/EVEView.h:82-91`, `EVEView.cpp:76-80` — `parentHierarchyChanged()` override. A new editor grabs the keyboard focus when it becomes showing and its `focus` row is on.

**carol (ARCHITECT's instruction):**
- `~/.carol/agents/auditor.md:67-77`, `:100-104` — Lean ladder step 5 is a finding only with a stated split and a quoted contract check; the Auditor reads `## Lean rulings` before the audit.

### Alignment Check
- [x] BLESSED principles followed — S: one writer of the plugin's `focus` row. E: focus flows Model → `Session` → plugin row → editor; views grab, they write nothing. Stateless: no flag added.
- [x] NAMES.md adhered — `setRow` joins `setRows`; `registerEvents` / `events` from the `EditorView` family; `focusPane` chosen by ARCHITECT.
- [x] MANIFESTO.md principles applied
- [ ] No build of the EVE change. The jam and END changes ran on macOS (ARCHITECT: *"runtime so far no issue. doxygen clean."*). Windows is not tested.

### Problems Solved
- **Selection hidden.** `paintSelection` ran in `Content::paint`, under the runs. The runs paint an opaque cell background (`jam_Colours.cpp:15`). The selection now paints after the runs.
- **Focus lost.** Three facts: an equal value sends no event (`jam_Model.cpp:266-267`); the macOS VST2 host window closes and opens the plugin editor on hide and show (`juce_VSTPluginFormat.cpp:154-161`; `juce_audio_plugin_client_VST2.cpp:1545`, `:1524`); the focus write ran before `layout()` (`jam_TabbedComponent.cpp:139-140`). The exact cause of the close-then-new-tab case was not proven by a static read.
- **NORMAL at the top.** Under END, `normal` wrote only the mode row (`ENDView.cpp:313-329`). The cursor placement lived only in jam's `normal` action. It now lives in the `Id::mode` reaction.
- **Finding 5, Windows.** `juce-plugin-window-focus-hook.patch` acts under `JUCE_MAC` only, so the host window takes no keyboard focus on Windows. No JUCE patch was added. JUCE calls `parentHierarchyChanged()` on the child editor when the VST2 wrapper is shown (`juce_Component.cpp:583-584`, `:1637`, `:1647-1649`); `visibilityChanged()` does not reach the child (`:577`). The override tests `isShowing()` because the callback also runs earlier (`:738`) and `grabKeyboardFocus()` asserts `isShowing() || isOnDesktop()` (`:3033`). The grab reaches `SetFocus (hwnd)` (`juce_Windowing_windows.cpp:1643-1649`, `:2355-2359`).
- **Audit, 20 findings.** Resolved: 1, 3, 5, 8, 11, 13, 16. Closed by ruling: 4, 10. Rejected with a refuting read: 2, 6, 7, 9, 12, 14, 17, 18, 19, 20. Finding 15 (`jam_TextEditor.cpp` size): ARCHITECT: *"NEVER RAISE AS ISSUE AGAIN. NEVER."*
- **`Session.h:167-168`** (a `plugin` row with no value finds no format): confirmed for VST, VST3, AU and CLAP (`juce_VSTPluginFormatHeadless.cpp:110-121`, `juce_VST3PluginFormatHeadless.cpp:116-121`, `juce_AudioUnitPluginFormatHeadless.mm:153-165`, `jam_ClapPluginFormat.cpp:212-216`). No change.
- **Mistake owned.** COUNSELOR read code files for context with its own hands; context reads go to Pathfinder.

### State for Continuation
- Build EVE, then END (END embeds the EVE archive). Check the EVE change on macOS: hide and show the plugin window, close a tab, open a tab; the focused pane takes the keys.
- Windows: test the focus chain (`EVEView.cpp:76-80`, `SessionView.cpp:61-71`). It was never run there.
- Regenerate doxygen in EVE (`ninja doxygen`) and read the warning count for `EVEView.h:82-91`.
- `auditor.md:100-104` has no effect yet: no `## Lean rulings` section exists, and jam has no `ARCHITECTURE.md`. The `jam::TextEditor` entry waits for ARCHITECT's word.
- Stale documents, not corrected: `eve/PLAN-Grand-Scheme.md:1149-1151`; `ARCHITECTURE.md` lines outside the focus regions (found near `:189-191`, `:216-218`, `:740`, `:759-778` before this sprint's edits; read them again before use).
- END log hygiene still open: the consumed handoffs at the end of this file.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 99: ENAX Plugin Format Is One Word — `pluginFormat`, Generated `pluginBundle.cmake`, Bundle From the Zip ✅

**Date:** 2026-11-09
**Duration:** not measured (one session, run from the cast COUNSELOR session)
**Scope (ARCHITECT):** *"add to end's CLAUDE.md, what AGENTS need to change if in the future we want to switch EVE (or any END's enax plugin) to other format"*; *"ONLY one place, one word, if it need to be replaced in couple of places, then the design is garbage"*; *"the selection should live at END's project-info.md"*; *"isnt this could be done in DETERMINISTICALLY in cmake way? but universal abstraction as `template`, then cast only drives the parameter?"*; rulings *"Generated -P script"*, *"From the zip"*, *"Default also narrows"*, *"Redefine vst/vst3/au"*, *"pluginFormat, plugin.cmake"*, then *"pluginBundle.cmake? more accurate?"*.

### Agents Participated
- COUNSELOR — the reads of cast cell resolution (`cast/Source/Model.cpp:72`, `:129-132`, `:987-988`), five ARCHITECT questions, every validation read, the audit triage, the SPEC / ARCHITECTURE / CLAUDE.md edits, this log.
- Pathfinder (haiku) ×4 — the format-site inventory; the gh.cmake chain, Nexus unpack path and JUCE `ZipFile` API; the Sprint 98 checks (`lipo`, `unzip -l`, `ccache -s`); the doxygen pass.
- Engineer (sonnet) ×4 — data and template; Nexus load from the zip; the audit fixes (CMake archive gate, Nexus split); the deletion of `PLAN-build-time.md`.
- Auditor (opus) ×1 — one pass, 14 findings.

### Files Modified (12 total)
**end:**
- `project-info.md:104`, `:106` — `## cmake` rows `pluginFormat | vst` (the one word) and `pluginManifest | ../eve/cast/spell.md`; the table re-gridded. `## toolchain` `:114`, `:124`, `:132` — `cmake -P pluginBundle.cmake`, `-DCAST_LANE=debug …`, `-DCAST_LANE=no-sign …` replace the three literal EVE lane calls.
- `cast/cmake.cast:540-544` — fence `plugin-bundle`: `string(JOIN "-" CAST_PLUGIN_LANE :::pluginFormat::: ${CAST_LANE})`, then `execute_process(COMMAND cast "${CMAKE_CURRENT_LIST_DIR}/:::pluginManifest:::" "--${CAST_PLUGIN_LANE}" COMMAND_ERROR_IS_FATAL ANY)`. `:356-391` — the plugin-archive block moved after the pack values; it keeps the archive of `${CAST_PACK_platform}` and fails the configure unless exactly one is left.
- `cast/spell.md:58`, `:249` — `@PluginBundle | ../pluginBundle.cmake` and its wiring row; both `@files:platform files` rows deleted.
- `cast/files.md` — `## platform files` (`EVE.vst` / `EVE.dll`) deleted; brief reworded.
- `cast/source.cast` — fence `platform-identifier-definition` deleted (its one user was the deleted row).
- `Source/plugins/Nexus.cpp:170-211`, `Nexus.h:77-115` — `offloadPlugins` deleted. `getOrCreatePlugin()` returns the first entry of the embedded archive and writes the stamp only after a good unpack. The private static overload `getOrCreatePlugin (resourceName, pluginsDirectory, isOffloaded)` handles one archive with `uncompressTo`.
- `Source/ui/TabView.h:61-68`, `Nexus.h:20-27` — doxygen.
- `CLAUDE.md` — `## ENAX Plugin Format (one word)`; stale `Nexus.cpp` line cites corrected.
- `SPEC.md:67-71`, `:236-244`; `ARCHITECTURE.md:13`, `:726`, `:817-822` — `offloadPlugins` / `files::evePlugin` text replaced, line cites corrected.
- `PLAN-build-time.md` — deleted (ARCHITECT: *"delete PLAN-build-time.md"*).

**eve:**
- `project-info.md:150-164` — the `vst`, `vst3`, `au` lanes pass `-DCAST_PLUGIN_FORMAT=VST|VST3|AU` and build `-C Builds/Release`: Release, signed, universal, one format, packed.

### Alignment Check
- [x] BLESSED principles followed — S: the format is one cell; the bundle name comes from EVE's output, not restated. D: one embedded archive, enforced at the owner (CMake). E: the unpack Result and the stamp write are asserted.
- [x] NAMES.md adhered — `pluginFormat`, `pluginBundle.cmake`, `CAST_LANE` ratified by ARCHITECT; `pluginManifest` from the same `## cmake` key family and cast's "manifest"; `@PluginBundle` / `plugin-bundle` from the `@Gh` / `gh` siblings; `isStamped` from `jam_Subprocess.cpp:70`.
- [x] MANIFESTO.md principles applied

### Problems Solved
- **The format word was fused into three places** (`project-info.md` `--vst-debug`, `--vst-no-sign`; `files.md` `EVE.vst` / `EVE.dll`). cast resolves only a whole-cell `@alias` (`Model.cpp:987-988`), so the composition moved to CMake: one word, one generated script, one lane word per END lane.
- **Audit, 14 findings.** Fixed: 1 + 7 (CMake one-archive gate), 2 (unpack Result, stamp), 4 (split under 30 lines), 5 (`pluginsHash` scope), 6 (re-check assert removed), 9 (`au` macOS only in CLAUDE.md), 10-13 (stale docs, line cites, CLAUDE.md wording, files.md brief). Rejected with a read: 3 (`getOrCreate` is creation, NAMES verb contract; creation may allocate, MANIFESTO E), 8 (ARCHITECT Sprint 98: *"EVE manifest by default (Release) must always build all, pack all, etc as now"*), 14 (`CAST_LANE` was in the option ARCHITECT chose). Doxygen-pass claims: C1 rejected (the same invariant as finding 6), C2 rejected (`isOffloaded` is a computed argument, not a tracked flag), C3 is covered by the CMake gate.
- **CMake facts used:** `execute_process` shares the parent pipes when no `OUTPUT_*` is given (`execute_process.rst:143-144`); `COMMAND_ERROR_IS_FATAL ANY` (`:213-241`); `string(JOIN)` (`string.rst:234-239`).

### State for Continuation
- Not built. ARCHITECT runs `cast cast/spell.md` in END: it must write `pluginBundle.cmake` at the END root and regenerate `Source/generated/Files.h` / `Files.cpp` without `evePlugin` (`Files.h:48`, `Files.cpp:50` still hold it).
- Check per lane: `cast cast/spell.md --debug` calls EVE `--vst-debug`; `--no-sign` calls `--vst-no-sign`; the default calls `--vst` (signed, universal, VST only, no install: `eve/CMakeLists.txt:723`).
- END's configure now fails unless `Source/plugins/` holds exactly one `* <platform>[ scout].zip` (`cast/cmake.cast:366-371`).
- Doxygen not regenerated (`ninja doxygen`).
- END log hygiene still open: the consumed handoffs at the end of this file (*END Host Contract*, *Relative Toolchain Paths in EVE and END*).
- Windows not tested.

### Debts Paid
- None

### Debts Deferred
- None

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

---

## Handoff to COUNSELOR: Style-Driven Theme — END style.css as SSOT

**From:** COUNSELOR
**Date:** 2026-11-09
**Status:** In Progress — code landed, one runtime crash fixed, awaiting ARCHITECT's cast + build

### Read First
1. `PLAN-style-driven-theme.md` (END root) — the locked plan, all rulings and the canon shape.
2. `jam_PluginEditorLayout.cpp:83-116`, `:683-690` and `kuassa_PluginEditorLayout.cpp:59-77`, `:599-617` — the two theme lanes; the KANJUT one is the canon shape (BinaryData default + newline + css-built markdown; `ConfigDocument::addTables` merge).
3. `kuassa_ConfigDocument.cpp:370-392` — the emit contract jam's twin must mirror.
4. `eve/Source/EVEView.cpp:217-219` and `kuassa/jreng-filter-strip/Source/FilterStripView.cpp:107-114` — the StyleManager registers the css tree directly; JFS runs the XML lane, no markdown round trip for style.

### Context
The sprint converts END's gfx theme to css-driven styling per the JFS/KANJUT canon: `gfx.css` is the SSOT for the style lane (palette, appearance ids, metrics, fonts); the StyleManager registers the css tree directly (EVE's shape); `theme.md` stays the authored settings default (window/style/font/scrollbar/tab/overlay/pane/menu), disk wins. Three earlier approaches were rejected by ARCHITECT as hacks (runtime css→markdown round trip, jam emit contract changes, duplicate font intern paths) — the working tree now holds the corrected canon shape; jam's emit is reverted to the KANJUT twin.

### Completed
- **EVE css** — obsidian scaffold removed; all 50 uses re-pointed to EVE's own named palette; zero obsidian anywhere (`eve/Source/layout/style.css`).
- **END gfx.css** — `:root` palette, `.appearance`/`.appearance.DARK` (scrollbar/TextButton/Label/PopupMenu/PaneComponent/Ansi/TextEditor/Markdown/MermaidDiagram ids + the two PopupMenu rows), `.metrics` (`--ScrollBar--thickness`), 84 `@font-face` rules with the four attributes on the first face (`--rasterizer: freetype; --gamma: 2.2; --contrast: 0.0; --embolden: false;`).
- **END theme.md** — recreated as the authored settings default: `version: 0.0.1`, `## window`, `## style`, `## font`, `## scrollbar`, `## tab`, `## overlay` (font rows), `## pane`, `## menu`, original values word-for-word.
- **jam** — `cast/graphics.md` obsidian rows deleted; `addTable` reverted to the KANJUT twin's two-branch shape with the boundary `jassert` (`jam_ConfigDocument.cpp:555-577` vs `kuassa_ConfigDocument.cpp:370-392`); `getTypeName` keeps no assert (ConfigDocument trusts the Validator); `jam::Model::toStrings` (was `toStringArray`) returns `jam::Strings`; `jam_Css` gained the universal numbers-list attribute (`getStyleAttributes`/`getStyleAttributeType` private statics, bare-number tokens only).
- **END wiring** — `ConfigDirectory`/`ConfigTheme` reverted to BinaryData defaults; `files::themeStyleSheet` (`cast/files.md`); the three dead font ids dropped (`cast/identifiers.md`); glob rows ship both `*.md` and `*.css` (`project-info.md:421-423`); Main's direct style reads deleted; `ENLookAndFeel`: `initialiseStyle()` creates the StyleManager over the css tree (`ENDLookAndFeel.cpp:492-502`), `applyColours` = the one `setAppearance` flow (:504-510), `applyFontStyles` = the StyleTheme shape (:512-523), `getTypefaceForFont` resolves through the manager's interned faces (:178-204), `registerTypeface`/`typefaces` map deleted (one font path), settings read via the original `getRowValue`; the default-LAF install moved into `initialiseStyle`'s tail so the engine's ctor font resolution falls to juce's default (the last crash fix, EVE's canon order).
- **display.md** — `font rasterizer`/`font gamma`/`font contrast` rows deleted (the css owns them).

### Remaining
1. ARCHITECT: `cast cast/spell.md` in jam (graphics.md) and end (files.md, identifiers.md, glob rows); build jam and END.
2. ARCHITECT: reseed — delete `~/.config/end/themes/gfx/theme.md` and `~/.config/end/display.md` (old content fails validation; defaults win; same version never overwrites).
3. Runtime checks: laf colours from the GFX sheet; fonts and the four attributes through the manager (no double intern); settings from the disk theme.md; no startup assertions.
4. COUNSELOR: the post-audit doxygen pass (Pathfinder) over the touched headers — `ENDLookAndFeel.h` (ctor/getTypefaceForFont/applyColours/registerEvents/applyFontStyles/initialiseStyle docs stale), `Main.h`, `ConfigDirectory.h`, `ConfigTheme.h`; then @Auditor once, then the sprint log.
5. EVE build + its runtime check (markdown preview renders with the new named palette).

### Key Decisions
- *"JFS with KANJUT is the canon contract shape. although JAM is now ahead, JAM shape must conformed to JFS, thus later sync should not break JFS with KANJUT"*
- *"no such thing as obsidian, no such thing as obsidian colour, everything should be style.css driven as SSOT, exactly like JFS"*
- *"END gains style.css"*; scope *"END + EVE + jam"*
- *"project like END, only need to define what it actually need, NOT EVERYTHING, unless END uses EVERYTING for its theme. gfx/theme.md is the shipped default theme"*
- *"Component always carry its colourId (enum int), StyleManager, StyleCustom build the map, map keyword-int-colour, materialise for usage when setAppearance event driven by init, file watcher from config changes, etc, one SSOT, one flow, one path, unidirectional"*
- *"no handrolling anything, no hand-authored anything"*
- theme.md keys: no `colour id`/`font id` words; no bare hex outside `## colours`; no SCREAMING_SNAKE anywhere; exception: XML-conformant parameter values (`AUTO`, `MEDIUM`)
- *"why round trip? runtime always from theme.md"*; *"there are no guard with ConfigDocument, garbage defensive programming is not allowed. means all check must through Validator. ConfigDocument must trust Validator"*
- *"NO. this is fucking hack. your implementation is garbage, and you change the whole contract. no duplication allowed. you will break JFS-KANJUT if you changed StyleManager. must be design by contract"*
- *"what's the established pattern? you only need to extend, why this need to be asked?"*

### Files Modified
- `eve/Source/layout/style.css` — obsidian removed, named palette.
- `end/Source/config/theme/gfx/gfx.css` — new; the style lane.
- `end/Source/config/theme/gfx/theme.md` — recreated; settings default only.
- `end/Source/config/ConfigDirectory.h/.cpp`, `ConfigTheme.h/.cpp` — reverted to BinaryData flow.
- `end/Source/lookAndFeel/ENDLookAndFeel.h/.cpp` — css-tree StyleManager, one setAppearance flow, manager font path, original settings reads.
- `end/Source/Main.cpp` — registerTypeface call removed; initialiseStyle/applyFontStyles kept.
- `end/cast/files.md`, `cast/identifiers.md`, `project-info.md`, `Source/config/display.md` — data edits above.
- `jam/cast/graphics.md`, `jam_markdown/document/jam_ConfigDocument.cpp`, `jam_web/css/jam_Css.h/.cpp`, `jam_data_structures/model/jam_Model.h` — lexicon rows out, emit reverted to the twin, numbers-list support, `toStrings`.

### Open Questions
None. The design is settled by the canon reads and ARCHITECT's rulings.

### Next Steps
- Trigger: ARCHITECT runs the cast + build (Remaining 1) and reports the result. On a clean run, COUNSELOR continues with the doxygen pass, the audit, and the sprint log.

## Handoff to COUNSELOR: Windows Runtime Defects of END Hosting EVE — Pane Size, Keyboard Routes, Build Warning, OMP

**From:** COUNSELOR
**Date:** 2026-11-13
**Status:** In Progress — pane-size fix, PCH fix and OMP fix landed, not yet built; keyboard design not yet planned

### Read First
1. `PLAN-file-uri-one-flow.md` (END root) — the locked and executed plan of this session's first part (one `file://` URI form, one reader `juce::URL::getLocalFile()`).
2. `Source/ui/EditorView.cpp:41-70`, `:97-125` and `EditorView.h:157`, `:166`, `:226` — the pixel-grid placement that makes END the one author of the hosted editor's size.
3. `jam/jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp:5-39` and `Source/plugins/Nexus.cpp:6-15`, `:54-64` — the ENAX key route, the one route that the keyboard fix keeps.
4. Build JUCE copy `C:\Users\jreng\AppData\Local\cast\juce\b1501cf634ada382e5d91018efaae42c68178593a6a4f839fcb8ea0c1dcb554b\modules`: `juce_gui_basics/native/juce_WindowsHooks_windows.cpp:54-66`, `juce_gui_basics/native/juce_Windowing_windows.cpp:1856-1918`, `:2983-3125`, `:3877-3898`, `juce_audio_processors/format_types/juce_VSTPluginFormat.cpp:412-431`, `:578-608` — the four Windows key routes.

### Context
ARCHITECT reported runtime defects of END hosting EVE (VST2) on Windows: black lines at the pane edges, a markdown-link crash, a Ctrl+W assert, a `TRAPWINCH: command not found: omp_repaint_prompt` line in panes, a `CAST_PLUGINS_HASH` PCH warning, keyboard commands that fail (prefix "`" then "-" or "\" does not split; Ctrl+T opens fzf in zsh, not a new tab), and slow pane creation and close. ARCHITECT states that focus and keyboard have no issue on macOS. The markdown crash was fixed earlier in the session (the URI plan). ARCHITECT reported after a build: *"split only works with dragging from corner. keyboard is still messy. previous pane still showing OMP error. new instance pane and closing is SUPER FUCKING UNBEARABLY SLOW"*.

### Completed
- **URI one flow (earlier in this session, per `PLAN-file-uri-one-flow.md`).** Shell OSC 7 lines write `file:///<path>` with no host. The encoders escape `% ; # ?` and control bytes. `jam_TerminalProcessor.cpp` stores the raw URI, and `getUriPath` is deleted. The readers use `juce::URL::getLocalFile()`: `jam_TextEditor.cpp:1040`, `EVEView.cpp:579-582`, `EVETerminalProcessor.cpp:440-441`. `SessionView.cpp:133` uses `juce::URL::removeEscapeChars (juce::URL {...}.getFileName())`. ARCHITECT confirmed that markdown links open.
- **`cwd` is a URI from session start.** `EVETerminalProcessor.cpp:442-444` writes `juce::URL { startDirectory }.toString (false)` into `Id::cwd`.
- **The rounding patch is withdrawn.** `jam/patch/juce-plugin-window-size-hook.patch` is deleted. Its row in `jam/user-modules-info.md` is removed (the row was the last of the patch table). ARCHITECT: *"no fucking patch and hack"*.
- **Pane size, one author (END).**
  - Cause, cited: END wrote the `width`/`height` rows (`EditorView.cpp:62-68`, `ARCHITECTURE.md:743`), but sized only the editor position (`setTopLeftPosition`, old `EditorView.cpp:97-103`).
  - The host editor took its size from EVE's `audioMasterSizeWindow` reply. The physical size passed through three roundings: `toNearestIntEdges` (`juce_PluginScaleFactorUtilities.h:127`), `getSmallestIntegerContainer` (`juce_VSTPluginFormat.cpp:127-130`, `juce_Rectangle.h:1025`), and floor/ceil with position (`juce_HWNDComponent_windows.cpp:60`).
  - The chain is synchronous (`EditorView.cpp:64-68` → `PluginState.cpp:158-166` → `juce_VSTPluginFormatImpl.h:1953-1960` → `EVEProcessor.cpp:168-187` → `EVEView.cpp:179-182` → `VST2.cpp:1015-1061` → `juce_VSTPluginFormat.cpp:140-151`).
  - Fix: `EditorView::getEditorBounds (int inset)` (`EditorView.cpp:97-117`) snaps the editor rectangle to whole physical pixels. It works in peer coordinates (`peer->getAreaCoveredBy`) with `gridStep = 96 / gcd (roundToInt (scale × 96), 96)`, so every JUCE rounding gets whole numbers.
  - `setEditorBounds (juce::Rectangle<int>)` (`:119-125`) and the rows take the same rectangle (`:59-68`).
  - `juce::NativeScaleFactorNotifier scaleNotifier` (`EditorView.h:226`) calls `resized()` on a scale change or a peer change.
  - EVE code is unchanged, so EVE in other hosts and standalone keeps its behaviour. COUNSELOR read every line.
- **PCH warning.** `cast/cmake.cast:397` sets `SKIP_PRECOMPILE_HEADERS ON` on `Nexus.cpp`, next to its per-source `CAST_PLUGINS_HASH` define. Cause: the define is on `Nexus.cpp` only, while the target PCH is built without it (`CMakeLists.txt:285`, `build.ninja:52-55`, `:188-190`).
- **OMP `TRAPWINCH` error.**
  - Cause: zsh resolved `oh-my-posh` to `C:\Users\jreng\.local\bin\oh-my-posh.exe` 29.9.0, which defines no `omp_repaint_prompt`.
  - The winget copy 32.0.0 (`WindowsApps`) defines it at init lines 744-747. `zshrc:181` was written for it.
  - The stale copy came from `bootstrap-windows.sh` (curl download, skipped when the file exists).
  - Fix: the stale exe is deleted. The bootstrap download block and the unused `OMP_ARCH` values are removed. Step 12 lists `winget install JanDeDobbeleer.OhMyPosh` (`bootstrap-windows.sh:677`). `WINDOWS-SETUP.md:61` no longer lists oh-my-posh among the script installs.
- **Keyboard — evidence collected, no code change.** A whatdbg trace with ARCHITECT's real typing (Debug END, 2026-11-13 12:08) shows one keystroke ("t") reaching `ENDView::keyPressed` → `CommandManager::invokeKey` four times, by four routes:
  1. EVE's `WindowsHooks` hook → `offerKeyMessageToJUCEWindow` → `doKeyDown` → ENAX callback → END.
  2. `vstHookWndProc` `SendMessage` to END's window → END `doKeyChar`.
  3. `vstHookWndProc` `CallWindowProc` → EVE `doKeyChar` → ENAX → END.
  4. EVE `forwardMessageToParent` `PostMessage` → END's queue → END `doKeyChar`.
- **Key route rules, by code.**
  - Routes 2-4 run only when route 1 does not consume the message (`juce_WindowsHooks_windows.cpp:58-63`).
  - Route 2 exists because EVE answers `effKeysRequired` with 1 (`eve/project-info.md:108` → `juce_audio_plugin_client_VST2.cpp:1856` → `juce_VSTPluginFormat.cpp:412`, `:427-431`). The macOS branch installs no hook (`:357-359`).
  - Each `invokeKey` call writes the layer/sequence state (`jam_CommandManager.cpp:57-58`, `:111-119`).
- **Ruled out by reads.** `keys.md` cells reach END as `` ` ``, `\`, `-` (`jam_MarkdownInlineParser.cpp:378-407`, `:1293-1306`; `jam_ConfigDocument.cpp:59-60`). The keyboard layout is US (KBDUS.DLL), so backtick is not a dead key.

### Remaining
1. ARCHITECT: run `cast cast/spell.md` in END and in EVE. Both `CMakeLists.txt` still list the deleted `juce-plugin-window-size-hook.patch` (END `CMakeLists.txt:136`, EVE `CMakeLists.txt:158`), so configure fails until cast runs. Then build END (it builds EVE).
2. ARCHITECT runtime check: pane edges show no black line at a fractional Windows scale; the `CAST_PLUGINS_HASH` warning is gone; new shells show no `TRAPWINCH` error.
3. COUNSELOR, keyboard (Windows only):
   - Pathfinder reads EVE's key-to-PTY path: `EVEView::keyPressed` (`EVEView.cpp:82-87`), the focused `jam::TextEditor` `keyPressed`/`TextInputTarget`, the PTY write, and the return value for a printable key. The dispatch was stopped by ARCHITECT and must run again.
   - Then write a PLAN that keeps the ENAX callback as the one route and closes routes 2-4 at their JUCE seams, with no patch, and no change to EVE in other hosts. Gate the PLAN with ARCHITECT.
   - Do not run more click/key tests. ARCHITECT's observation is the evidence.
4. COUNSELOR, slowness: profile pane creation and close on Windows with scout (profile skill). No Windows manifest exists: every `benchmark/*.md` command is the macOS `END.app` path. Log evidence: in the Debug build under the debugger, END's log starts at 12:27:13 and EVE's at 12:27:56.
5. COUNSELOR: fix the validated audit items of the URI sprint:
   - `SessionView.cpp:133` turns `+` into a space (`removeEscapeChars`, `juce_URL.cpp:963`).
   - `EVEView.cpp:542`/`:546` parse `Id::url` twice, with a File↔string round trip into `openFile`.
6. COUNSELOR: the post-audit doxygen pass (Pathfinder). Stale blocks: `EditorView.h` docs of `resized` and `setEditorBounds` (still describe `inset`), `EVEView.h:289-291`, `EVETerminalProcessor.h:298`, `jam_TextEditor.h:1022-1023`, `eve/SPEC.md:227` (OSC 7 row says "decoded path"). Then @Auditor once, after ARCHITECT's runtime test, then the sprint log on ARCHITECT's `/log`.
7. Pending from earlier: scout launch-time optimisation (item 3 of the session's first request).

### Key Decisions
- *"use juce API to its fullest extent, fix any terminal command that use it crossplatform, SSOT"*
- *"UNIDIRECTIONAL, ONE FLOW"*
- *"ok add one to fix black lines. resizing must be SSOT, should not disagree between host and client. must be crossplatform."*
- *"no fucking patch and hack. i told you no patch, you insist on patch."*
- *"i thought it was explicitly RULED OUT, when END is the host, END always TOLD EVE THE SIZE. there's no disagreement, this is ENAX contract, not the EVERYTHING PLUGIN lane"*
- *"our DESIGN with working with juce ALWAYS SEAMLESS INTEGRATION. we never want to fight juce. patch is the only last inevitably option when there is no other seam, juce reached its design limit"*
- *"be careful with the design. this only END-EVE host client contract? what will you break when EVE loaded on other host? standalone?"*
- *"fix it, why do i always need to repeat"* (the PCH warning)
- *"fix this so no more confusion"* (the oh-my-posh version)
- *"FACTS: - no issue with focus on mac - no issue with keyboard on mac"*
- *"DESIGN BY CONTRACT NON NEGOTIABLE"*
- *"dont waste my tokens for audit. i havent even tested runtime"*

### Files Modified
- `end/Source/ui/EditorView.h`, `EditorView.cpp` — `getEditorBounds`, `setEditorBounds (juce::Rectangle<int>)`, `scaleNotifier`; rows from the snapped rectangle.
- `end/cast/cmake.cast` — `SKIP_PRECOMPILE_HEADERS ON` on `Nexus.cpp`.
- `end/Source/ui/SessionView.cpp` — directory name from `juce::URL` (URI plan).
- `end/PLAN-file-uri-one-flow.md` — new; the URI plan.
- `eve/Source/shell/zsh_integration.zsh`, `bash_integration.bash`, `powershell_integration.ps1`, `eve` — OSC 7 `file:///` form; encoders escape `% ; # ?`.
- `eve/Source/EVEView.cpp` — `getUrlFile` through `juce::URL::getLocalFile()`.
- `eve/Source/EVETerminalProcessor.cpp` — start directory from the `cwd` URI; `cwd` written as a URI at `openTTY`.
- `jam/jam_terminal/processor/jam_TerminalProcessor.h`, `.cpp` — OSC 7 stores the raw URI; `getUriPath` deleted.
- `jam/jam_gui/widgets/jam_TextEditor.cpp` — `cwd` read through `juce::URL::getLocalFile()`.
- `jam/patch/juce-plugin-window-size-hook.patch` — created, then deleted (no net change); its `jam/user-modules-info.md` row is also gone.
- `~/.config/bootstrap-windows.sh` — oh-my-posh curl block and `OMP_ARCH` removed; winget line added; comments updated.
- `~/.config/WINDOWS-SETUP.md` — oh-my-posh removed from the script-installed list.
- `C:\Users\jreng\.local\bin\oh-my-posh.exe` — deleted (stale 29.9.0).

### Open Questions
- The seam for each of key routes 2-4 (EVE's `effKeysRequired` reply and the VST2 host hook, the `CallWindowProc` re-entry, EVE's `forwardMessageToParent`). Decider: ARCHITECT, through the keyboard PLAN.
- Saved sessions from the old build hold per-block `cwd` rows in path form. There is no migration (CAROL forbids legacy compatibility). Decider: ARCHITECT, if the Ctrl+W assert still shows.

### Next Steps
- Trigger: ARCHITECT runs cast and the build (Remaining 1) and reports the runtime result (Remaining 2).
- In parallel, COUNSELOR runs again the EVE key-to-PTY read (Remaining 3) and presents the keyboard PLAN for approval.

