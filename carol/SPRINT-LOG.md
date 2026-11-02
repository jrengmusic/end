# SPRINT-LOG

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

## Sprint 93: ENAX Plan 1 Steps 1–8d — END Hosts EVE in Every Pane, One Instance per Pane ✅

**Date:** 2026-11-02
**Duration:** one session, over a context compaction. The start and end times are not recorded.
**Plan:** `PLAN-enax-plugin-host-type.md` Steps 1–8d (locked). Steps 9 and 10 are folded into `PLAN-enax-command-id.md` by ARCHITECT's order.

### Agents Participated
- COUNSELOR — discussion, trace specs, every validation read, the small edits named below, this log.
- Pathfinder (haiku) — reads, the whatdbg `trace` runs (interactive with ARCHITECT), git status and diff, binary hash checks.
- Engineer (sonnet) — Steps 8c, "pane complete at creation", the plugin offload stamp, the TextEditor terminal lanes, the ViewManager bind-before-bounds order.
- Auditor — not run. ARCHITECT: *"/audit wholesale until /log"*; it runs once over Plan 2's sprint.

### Files Modified
**end (24 files):**
- `Source/Main.cpp:29-40` — one export `enax_get_extension (const char* id)` returns `Nexus::getExtension (id)`. The three `jam_host_*` exports are deleted.
- `Source/plugins/Nexus.h` / `.cpp` — `extensions` table: `enaxContextFactory`, `enaxPeerDeleted`, `enaxKeyPressed`, `enaxMouseEvent`, `enaxDisplayBounds` (`Nexus.cpp:30-34`); `keyPressedCallback` and `mouseEventCallback` forward to the active window's peer (`:6-24`); `getExtension` (`:112-121`); `registerAudioUnit` (`:188-209`). `offloadPlugins` extracts only when the stamp differs from `CAST_PLUGINS_HASH` (`:123-150`), because the old size test kept a stale EVE of equal size.
- `Source/plugins/Session.cpp:39-82` — one synchronous load per pane: `PluginDescription` with `fileOrIdentifier` and `pluginFormatName`, then `formatManager.createPluginInstance`. The `findAllTypesForFile` scan (it built a throw-away instance, `juce_VSTPluginFormatHeadless.cpp:53`) and the `Id::pluginId` write are deleted.
- `Source/ui/EditorView.cpp:4-12`, `:72-92` — the editor is created in the constructor (`createProcessorEditor()`), JUCE-native. `resized` sets the hosted `Width`/`Height` parameters through `convertTo0to1 (snapToLegalValue (…))` (`:22-55`). `visibilityChanged` and the `pluginId` branch are deleted.
- `Source/ui/SessionView.cpp:56` — `property == Id::name or property == Id::focusedPane`.
- `Source/lookAndFeel/ENDLookAndFeel.h` / `.cpp:509-519` — the pane outline reads the pane row's `Id::focus`; `createFocusOutlineForComponent` is deleted.
- `cast/cmake.cast:295-306` — `CAST_PLUGINS_HASH`: SHA256 over the `.zip` files of the layout glob, a configure dependency, a compile definition.
- `cast/files.md:33` — `plugins stamp` = `.plugins-stamp`. `cast/identifiers.md` — `pluginId` row deleted.
- `project-info.md` — the EVE spell runs before jam in each cast row. `Source/plugins/EVE v0.1.0 macOS.zip` — the current EVE build.
- Generated: `CMakeLists.txt`, `Source/generated/Files.*`, `Identifiers.*`, `.clangd`, `.project`.
- `SPEC.md`, `ARCHITECTURE.md` — END as an audio plugin host with ENAX (partial; see State for Continuation). `PLAN-enax-plugin-host-type.md` — Steps 8a–8d recorded. `PLAN-enax-command-id.md:101` — "arm" wording replaced.

**jam (36 tracked + 6 new):**
- `jam_core/utilities/jam_PluginHostType.h` / `.cpp` (new) — one detection object over `juce::PluginHostType`; `jam_PluginHost.h` deleted; callers moved (`jam_AudioModel.cpp:126`, `:446`, `jam_PopupMenu.h:1966`, `jam_ScaledContent.cpp:7`, `jam_ViewEditor.cpp:76`, `:93`, `jam_ViewPanel.cpp:18`).
- `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.h` / `.cpp` (new) — mixin, rule table, one `isEND()` probe; `set (rule, callback, args…)` forwards its arguments (`.h:12-19`); the size rule fills `ranges[width/height]` from `minimumPaneExtent` and END's display bounds (`.cpp:21-27`).
- `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h:57-128` — `create` takes the extensions; `engine` and `keys` rules replace the `getHostFunction` branch.
- `jam_plugin_bootstrap/parameter/jam_ParameterLayout.h` / `.cpp:77-166` — `get (parameters, ranges)` with an integer entry.
- `jam_plugin_bootstrap/view/jam_PluginEditor.cpp` — `resized` writes width/height through a gesture (`:58-70`); `getZoomSize() const` (`:168-177`); `width`/`height` registry callbacks call `setScaling()` (`:236-246`).
- `jam_plugin_bootstrap/view/jam_ViewManager.h:75-78` / `.cpp:60-70`, `:95-146` — build, bind, then lay out: the build pass sets no bounds; `applyComponentBounds<DocumentType>` runs after `addBindings`; the XML lane lays out from each row's `rect`. MANIFESTO E, Complete at Creation.
- `jam_gui/widgets/jam_TextEditor.h` / `.cpp` — the `onKeyPress`, `onMouse`, `onImages` slots and the `liveCursor`, `liveCursorVisible`, `liveIsRecording`, `winsize` Values are deleted. `bindToModel` binds the instance's `Id::terminal` row before it redirects `state` (`.cpp:148-163`); `cursor`, `cursorVisible`, `isRecording` dispatch through the `properties` map (`:782-789`); `setWinsize()` writes the row (`:1098`). Terminal mode encodes keys and mouse with `TerminalKeyboard::map` / `TerminalMouse::map` against the row and sends prvalues through `bindToEvents` → `chainEvents[Id::terminalInput]` (`:137-145`, `:542-557`, `:1457-1471`). `setImageBounds`, `addImageBounds`, `imageBounds` and `Value::Listener` are deleted. Local `node` renamed `componentState` (NAMES Rule 6).
- `jam_gui/layout/jam_OwnedComponent.h` (`focusOfChildComponentChanged`), `jam_OwnerComponent.h:80` (`addAndMakeVisible`), `jam_MatrixComponent.cpp:353-359` (no `setVisible`), `jam_PaneComponent.h` (`setHasFocusOutline` removed).
- `jam_plugin_bootstrap/standalone/jam_AudioStandaloneApp.*` — `MainWindow::focusGained` gives the editor focus.
- `patch/juce-plugin-host-type-hook.patch`, `juce-plugin-window-focus-hook.patch`, `juce-mouse-event-hook.patch` (new); `juce-plugin-transparency-hook.patch` (opaque only when the host is not END).
- `cast/core.md:74-78` (five ENAX ids), `cast/plugin_bootstrap.md` (`engine`), `cast/terminal.md:63` (`winsize`, moved from EVE because jam now names it), generated, `user-modules-info.md`, `jam_core.*`, `jam_plugin_bootstrap.*`.

**eve (12 files):**
- `Source/EVEProcessor.h` — `jam::ENAXClientExtensions` mixin; `create (*this, …)`; size ranges into `ParameterLayout::get`.
- `Source/EVEViewComponents.cpp:26-37` — `registration.events` is stateless (`[]`): `bindToEvents (chainEvents->at (eventName))`, then terminal mode. The old `[this]` capture bound every later editor to the first instance's terminal row through the first-wins shared registry (`jam_SharedInstance.h:1-3`).
- `Source/EVEView.h` / `.cpp` — `setOverlays`, `applyImage`, `overlays` deleted.
- `Source/EVETerminalProcessor.cpp:37` — the destructor calls `releaseResources()`.
- `Source/layout/parameters.md` — `width` (80–7680, 800) and `height` (80–4320, 600).
- `cast/identifiers.md` — `winsize` moved to jam. `CMakeLists.txt` (patch list), `project-info.md` (absolute toolchain paths), generated, `PLAN-Grand-Scheme.md` ("arm" wording).

### Alignment Check
- [x] BLESSED principles followed — one instance per pane; per-instance state through the lanes each bind receives; bind before layout.
- [x] NAMES.md adhered — corrected in-sprint: the conditional `set` in `setWinsize` was reverted (Rule 1), `node` renamed (Rule 6).
- [x] MANIFESTO.md principles applied
- [ ] No @Auditor and no doxygen pass. ARCHITECT ordered both to run once, wholesale, at the end of Plan 2.

### Problems Solved
- **Second pane empty.** The cause was the shared registry. `jam::PluginEditor` holds `SharedInstance<Registry>` (`jam_PluginEditor.h:217`), first-wins. EVE's `[this]` bind lambda therefore bound instance 2's text editor to instance 1's terminal row. The trace showed instance 2's `winsize` never firing and instance 1 restarting at 110×37. Fixed by stateless registration, with per-instance data through the bind lanes (JFS pattern, `FilterStripViewRegistration.cpp:365-401`).
- **Stale EVE.** The size-only offload test kept an older EVE of equal size (SHA256 differed). Fixed by the build-time hash stamp.
- **Two instances per pane.** The type scan built a throw-away instance. Replaced by one direct load.
- **The `winsize` write on an unbound row** (`juce_ValueTree.cpp:777`). Fixed at the causes: `bindToModel` binds the row before it redirects `state`, and `ViewManager` binds before it lays out.
- **Crash on first key.** `Function::Array::get` deduced `<const char*&, int&>`; fixed by passing prvalues (`jam_Function.h:145-149`).
- ARCHITECT's runtime test: 2 tabs, 2 panes, every terminal created, focus correct, each pane closed until quit, no issue.

### State for Continuation
- Plan 2 (`PLAN-enax-command-id.md`) is next. It absorbs, by ARCHITECT's order:
  1. Per-instance key `mode`. `jam::CommandManager::mode` is one `juce::Value` per binary (`jam_CommandManager.h:23`). Each TextEditor `referTo`s it (`jam_TextEditor.cpp:105`, `:159`), so the editor bound last owns it for every pane (`jam_CommandManager.cpp:54`). Plan 2 `:38`, `:95`, `:109` keep this design and need amendment.
  2. `Session::newPlugin` takes a `juce::PluginDescription`; EVE's description is fixed data from `Nexus` (`Session.cpp:48` hardcodes the VST format name). The name `defaultPlugin` is not ratified.
  3. The `MVKBlockObserver` duplicate class (END and EVE) and the `FSEventStream` assertion, logged once per instance.
  4. Plan 1 Step 9 (AU, one format at a time) and Step 10 (SPEC/ARCHITECTURE).
  5. Plan 2's own citations are stale (for example `:39`, `:51`); refresh them first.
- `ARCHITECTURE.md` still describes the removed `Id::pluginId` and the type scan (the lifecycle bullets and "The View is detachable"). Read the code before you rewrite it.
- Stale doc lines for the doxygen pass: `jam_TextEditor.h:67`, `:141`, `:160`, `:471`, the constructor and `setWinsize` docs; `jam_ViewManager.h:278-279`, `:302-303`, `:323-329`, `:685`; `EVEView.h:7-10` and the `registerViewComponents` doc; `EditorView.h:14-16`, `:44-47`; `jam_OwnerComponent.h:64-73`; the `jam_PluginEditor.h` `getZoomSize` / `addCallbacks` docs; `ENDLookAndFeel.h:258`; "arm" prose at `jam_ParameterLayout.h:18`, `:199`, `jam_ModelLayout.h:17`.
- Under the debugger only, `split` asserted at `jam_MatrixComponent.cpp:70` when the window was not focused at the pane's focus grab (`juce_Component.cpp:2960-2966`). It did not occur in ARCHITECT's runtime test.
- DEBT-20261025T214955 and DEBT-20261009T112447 are recorded as paid in `CLAUDE.md`. Their receipts belong to Plan 2 Step 10 (`PLAN-enax-command-id.md:146`).

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261025T135707` — END hosts EVE as CLAP; ENAX's CLAP format is future work (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (ARCHITECT: *"windows build still miles ahead. defer."*).

---

## Sprint 92: ENAX Host–Client Contract Planned — Grand-Scheme Amendment, Two Executable Plans, Plan Hygiene ✅

**Date:** 2026-11-01
**Duration:** one session. The start and end times are not recorded.

### Agents Participated
- COUNSELOR — discussion with ARCHITECT, every ruling recorded verbatim, the Grand-Scheme amendment, both plans, the debt rewrite, the CLAUDE.md refresh, this log.
- Pathfinder (haiku) — reads: plan and log inventory over end, jam and eve; the key and command path; the Grand-Scheme; the JFS and KANJUT attachment pattern; the glyph raster path under a transform; the JUCE hosted-parameter, focus and transparency code; the AU registration facts.
- Engineer (sonnet) — `rm` of 11 plan files.
- Auditor — not run. This sprint wrote documents only; each plan runs @Auditor at its own end.

### Files Modified (17 total)
- `PLAN-enax-plugin-host-type.md` (new) — Plan 1, 10 steps: `jam::PluginHostType` (END registered in JUCE's shape), ENAX ids, one `enax_get_extension` export over `Nexus::extensions`, `jam::ENAXClientExtensions` (mixin, rule registry, one `isEND()` probe), engine and key-hook rules, transparency through JUCE's host query, focus forwarded by END, AU in-process registration, docs. Its gate runs once per format.
- `PLAN-enax-command-id.md` (new) — Plan 2, 10 steps: trace first, EditorMode as a key, owner-scoped command tables (`map::TextEditorCommandId`, `map::PluginEditorCommandId`) with default bindings, one binder per binary, the hosted key rule, `mode` as an EVE host parameter with `jam::Model::ValueAttachment`, END driving the focused plugin by parameter name, END zoom through the plugin's `zoom` parameter with `exclusiveZoom`, copy and paste, docs.
- `DEBT.md:20-25` — DEBT-20261025T135707 rewritten to its CLAP-format remainder and marked deferred. The `VirtualClock` part is done (`:125`, `:36` of this log).
- `CLAUDE.md:3`, `:9-21` — END named an audio plugin host end to end. Last sprint, the four debts, the two open plans and the Layer Order follow the working tree.
- `carol/SPRINT-LOG.md` — this entry.
- `~/Documents/Poems/dev/eve/PLAN-Grand-Scheme.md` (§4.3, after `:986`) — `[AMENDED 2026-11-01 — ENAX, the END host–client contract, ARCHITECT verbatim]`.
- Deleted (done or superseded): `PLAN-eve-hosted-at-end.md`, `PLAN-END-plugin-host.md`, `PLAN-terminal-editor.md`, `PLAN-eve-shared-engine.md`, `PLAN-end-host-contract.md`; eve `PLAN-column-reflow.md`, `PLAN-eve-beat-endless.md`, `PLAN-terminal-clock-rhythm.md`, `PLAN-terminal-clock.md`, `PLAN-seq-profiling.md`; jam `PLAN-cpu-graphics-engine.md`.

### Alignment Check
- [x] BLESSED principles followed — each design point was checked against SSOT, Encapsulation and Explicit, and each scattered site is cited in the plans.
- [x] NAMES.md adhered — every new name was ratified by ARCHITECT or derived from a cited family. The full list is in each plan's rulings.
- [x] MANIFESTO.md principles applied
- [ ] No build and no runtime check: this sprint changed no code.

### Problems Solved
- **Plan sprawl.** 13 plans were spread over three repos. 11 are deleted. The kept plans are `eve/PLAN-Grand-Scheme.md` (founding RFC), `eve/PLAN-eve-terminal-protocol.md` (Step 14 waits for ARCHITECT's runtime test) and the two new END plans.
- **A vague key and command contract.** The old Step 7.3 pushed the plugin's key rows into a coalesced END table. The session replaced it:
  - A command id is a callback id, owned by the owner of the state it changes. This is the `ColourId` isomorph (`jam/cast/style.md:66`).
  - Widgets carry their default bindings. Config overrides by name, and the widget never reads config.
  - One binder and one entry per binary.
  - The TextEditor keeps resolving its own keys by its own mode (`jam_TextEditor.cpp:726-729`).
- **END driving plugin state.** The state goes through JUCE's host parameter contract, which every format carries.
  - Hosted parameter ids are format-specific (`juce_VSTPluginFormatImpl.h:979-981`, `juce_VST3PluginFormatImpl.h:2106-2108`, `juce_AudioUnitPluginFormatImpl.h:758-760`). So END finds a parameter by name. The name comes from one jam function (`jam_ParameterLayout.cpp:26-31`).
  - END works only in normalised values, with JUCE's own API (`juce_AudioProcessorParameter.h:105-159`).
- **Zoom.** The JUCE view transform reflows, because the view bounds are divided by the zoom (`jam_ViewEditor.cpp:96`). Glyphs rasterise at the scaled size (`jam_GlyphRasterizer.cpp:55-61`, `:463-465`). So zoom stays the transform over the `zoom` parameter, and the font-size multiplier is dropped.
- **Contract name.** ARCHITECT ratified ENAX (*"END Native API eXtension"*) and `jam::ENAXClientExtensions`. The latter is the isomorph of JUCE's per-format client extensions (`juce_AudioProcessor.h:1254-1284`).

### State for Continuation
- Next: `/go` on `PLAN-enax-plugin-host-type.md`, then `PLAN-enax-command-id.md`. Both state "Risks / Open Questions: None".
- Before Plan 1 Step 6, read every `PluginEditorLayout::create` caller (LSP). The known caller is `eve/Source/EVEProcessor.h:96-97`.
- Before Plan 1 Step 7, read the AU client editor wrapper (`juce_audio_plugin_client_AU_1.mm`). It was not read this session.
- Before Plan 2 Step 3, read the generated form of a `static-value-lookup-table` section (`jam/cast/spell.md:2711-2721`).
- The two 2026-10-31 handoffs below record the previous sprint's completed work. That work is committed but not logged as a sprint.
- `eve/PLAN-eve-terminal-protocol.md` waits for ARCHITECT's Step 14 runtime test. Its sprint is not logged (`eve/carol/SPRINT-LOG.md:144`).

### Debts Paid
- None

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

