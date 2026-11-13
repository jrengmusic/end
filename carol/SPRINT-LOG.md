# SPRINT-LOG

---

## Sprint 103: Each Config File Parsed Once, Validated at the Parse ✅

**Date:** 2026-11-14
**Duration:** one session, from 2026-11-13 00:18 UTC. Not measured. `jam/carol/SPRINT-LOG.md` Sprint 187 is the primary record.
**Scope:** `jam/PLAN-document-contract.md` Step 13 (RFC `jam/RFC-config-document-parse-once.md` Q1–Q4). ARCHITECT: *"Full END one-parse"*; *"Caller memoizes"*.

### Agents Participated
- COUNSELOR: delegation, the read of each return, this log.
- Engineer (sonnet): the config changes, END with EVE build.
- Pathfinder (haiku): reads, the doxygen pass.

### Files Modified
- `Source/config/ConfigDirectory.h`, `.cpp` — `addFileTables` takes `getFile` and `getDefaultText`; each file goes through `ConfigDocument::getOrCreateFileDocument`; nullptr adds the `ConfigDocument::isValid` message. The second `ConfigValidator` run is deleted.
- `Source/config/ConfigModel.h`, `.cpp` — `saveToPath` keeps only the directory creation. `ConfigModel.h:16-17` doxygen corrected.
- `Source/config/ConfigTheme.h`, `.cpp` — `saveToPath` keeps directories and flex files. `getOrCreateDefaultText (fileName)` replaces `getDefaultText`; `:53-58` passes a lambda (G2). `:30` reads `CssDocument::parse (…)->getValueTree`. `addFileFlex` gives `XmlDocument::isValid` on nullptr.
- `Source/ui/ENDView.cpp` — callers of the pointer-returning `getOrCreate` and `parse`.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Each config file was parsed in `loadFromPath` and again in `saveToPath`, and validated twice. Now one parse per file per load, validated at the parse.

### State for Continuation
- The default theme style tree goes through `CssDocument::getOrCreate (name, text)`, once per process. No read shows that the CssWriter output of the default document equals `themeStyleSheet`, so the round trip is memoized, not removed (PLAN Step 13, Q3).
- Behaviour change: an invalid config file is not overwritten when its version is old; it is reported.

### Debts Paid
- None

### Debts Deferred
- None

## Sprint 102: EVE Launch Time — Config Documents Built Once and Read in Place, Key Index, No Theme Round Trips ✅

**Date:** 2026-11-13
**Duration:** not recorded. The session ran over two compactions.
**Scope (ARCHITECT):** *"let's fix eve loading time, then end loading eve time. must be as FAST as we can. profile with scout"*; *"Standalone EVE launch"*. Contract rulings: *"Markdown parser is not only cast, IT IS EVERYTHING JAM PROJECT USES as manifest. this is kernel module, synced to KANJUT"*; *"fuck the doc. code speaks to compiler. runtime undoubtedly FUCKING SLOW. meaning CODE IS WRONG ... FIX THE CAUSE, NOT THE FUCKING SYMPTOMS. FIX THE DESIGN, DESIGN BY CONTRACT"*; *"DOCUMENT CONTRACT ALWAYS NON-NEGOTIABLE: - built complete AST once - memoized, cached - state update - 0 rules"*; *"consumer/reader/parser/writer/etc always only consume TRUSTING VALID data"*. Choices: theme round trips *"Both (Recommended)"*; validator *"Validator reads document"*; key lookup *"Key index at parse"*. Log: *"no need audit, we'll audit wholesale when all optimized"*.

### Agents Participated
- COUNSELOR — cause reads, every validation read, the briefs, the scout result reads, small fixes named below, this log.
- Pathfinder (haiku) — scout runs (Release, 11 epochs), the theme-tree hash checks, one whatdbg crash trace, git status reads.
- Engineer (sonnet) — key stamping, the direct style tree, the shared CssWriter helpers, parse-once settings files, the validator document reads, the key index, profiler zones.

### Files Modified (24 total)
**jam:**
- `jam_markdown/document/jam_ConfigDocument.cpp:33-55` — `parse` stamps `Id::key` (the key tag) on every table and every row, in the creation pass. `getRowKey` reads the stamp (`get`, no check). `addProperty` takes the headers once per table.
- `jam_markdown/document/jam_ConfigDocument.h`, `.cpp:216-270` — the key index: private `configRows` (key tag → data rows, document order), `addRowKey` (stamp, then index; header rows and empty keys are not indexed), `getConfigRows (keyTag)`, the overload `getConfigRow (table, keyTag)` through `Element::parent`, and `getConfigRow (tableId, key)` over the index.
- `jam_markdown/document/jam_ConfigDocument.cpp:327-554` — `getValueTree (rootType, sheet)` builds the style tree from the config tables: `addSheetTree`, `addColourProperties`, `addMetricProperties`, `addFontsTree`, `addFontsProperties`, `addFaceTree`, `addWindowTree`, `addWidgetTrees`, `addAliasProperties`, `addRecordTrees`. Keys come from the stamps.
- `jam_markdown/document/jam_CssWriter.cpp` — `getCamelName`, `getRecordHeaders`, `getAttributeHeaders` moved to `jam_ConfigDocument.cpp:327-355`; both files use the one copy (unity order `jam_markdown.cpp:9`, `:12`).
- `jam_markdown/document/jam_ConfigValidator.cpp` — the default-value, reference, source and unknown-key checks read the default `ConfigDocument` (`getConfigRow`, `getConfigRows`); no `juce::ValueTree`. `RowCheck` takes the default table. `hasReference` looks up the key index; the `DARK` case reads `map::Appearance`. `getDefaultTable` compares the stamped keys.
- `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h`, `.cpp` — `getThemeSettings` returns the `toDocument` result as a `SettingsDocument` (no markdown write and reparse). `getStyleTree` calls `ConfigDocument::getValueTree (rootType, sheet)` (no CSS text and parse). File-static `getOrCreateFileDocument` parses each settings and theme file once and writes the default text only when the file version is old. `getSettingsTree` takes the parsed user document.
- `jam_markdown/document/jam_MarkdownBlockParser.cpp` — a key-tag stamp was added, then removed (kernel parser contract). Net: no change.
- `cast/plugin_bootstrap.md`, `generated/jam_PluginBootstrap.*` — profiler ids of the layout stages and validation.
- `cast/vulkan.md`, `generated/jam_Vulkan.*`, `jam_vulkan/context/jam_VulkanMsaaCalibration.cpp`, `jam_vulkan/engine/jam_VulkanGraphicsEngine.cpp` — Vulkan start-up profiler zones.

**eve:**
- `Source/EVEProcessor.h`, `.cpp`, `Source/EVETerminalProcessor.cpp`, `Source/EVEView.cpp`, `cast/identifiers.md`, `Source/generated/Identifiers.*` — launch profiler zones and their ids (`launchPrompt`, `processorLayout`, `processorEditor`, `ttyOpen` and others).
- `benchmark/launch-windows.md`, `benchmark/launch-windows.sh` (new) — the Windows launch manifest (Release profile build through `## toolchain`) and the profiler shell script.

**end:**
- `benchmark/pane-windows.md` (new) — the Windows manifest for END loading EVE. Not run.

### Alignment Check
- [x] BLESSED principles followed — E (Complete at Creation): keys and the key index are built in the parse; readers read them in place and check nothing. S: one copy of the CssWriter helpers; `map::Appearance` is the one source of `DARK`.
- [x] NAMES.md adhered — new names join existing families (`getConfigRow`/`getConfigRows`, `add*Properties`/`add*Tree`, `getOrCreate*`).
- [x] MANIFESTO.md principles applied
- [ ] Not audited. ARCHITECT: *"no need audit, we'll audit wholesale when all optimized"*.
- [ ] No doxygen on the new and changed members; the doxygen pass waits for the audit.

### Problems Solved
- **EVE launch, scout medians (Release, 11 epochs, Windows).** `processor layout` 1.182 s → 152 ms. `layout theme tree` 1.140 s → 113 ms. `layout settings tree` 1.037 s → 76 ms. `validator is valid` 628 ms → 12.5 ms, plus `validator default rules` 10.1 ms. `layout style tree` 394 ms → 52 ms.
- **Keys computed again by every reader.** The key tag is now stamped once at parse (`jam_ConfigDocument.cpp:33-55`), not in the kernel parser.
- **Theme text round trips.** The theme default went CSS → markdown text → parse, and the style tree went config → CSS text → parse. Both are now direct. The new theme tree is byte-identical to the old one (SHA-256 of the written XML, 82,275 bytes).
- **Files parsed three times.** `getOrCreateDefaultFile` parsed the file and the default text for the version, then `getSettingsTree` parsed the file again. Now one parse per file; the theme markdown text (51 ms) is built only to write an old file.
- **Validator rebuilt the default tree in five rules.** It now reads the default document. The first rewrite was slower (88 ms: `getTableRows`/`getTableHeaders` arrays built in nested loops); the key index made it O(1).
- **Launch crash (access violation).** A file-scope `static const auto darkTag { Format::toTag (Id::dark) }` ran before `Id::dark` (another translation unit, `generated/jam_Style.cpp:27`) was built. Replaced by `map::Appearance::getInstance()->get (map::Appearance::dark)` with a local owner.
- **Mistakes owned.** The kernel Markdown parser was changed (reverted on ARCHITECT's ruling); readers with `find`/`isNotEmpty` checks (moved to creation); a brief that put a static at file scope; a `getDarkTag()` function where a local static was enough; `darkTag` built again what `map::Appearance` holds; an Engineer name outside the brief (`defaultRows`) rewritten.

### State for Continuation
- Not audited, no doxygen pass. Stale or missing docs: `ConfigDocument::getRowKey`, `getConfigRow` (both), `getConfigRows`, `getValueTree (rootType, sheet)`, `addProperty`; `PluginEditorLayout::getThemeSettings`, `getStyleTree`, `getSettingsTree` (`@param userDocument`).
- Temporary profiler zones stay for the next optimisation: `layoutDefaultFile`, `validatorDefaultRules` (`jam_PluginEditorLayout.cpp`), and the other launch zones in jam and EVE. `Id::settingsDocumentParse` has no user left (`cast/plugin_bootstrap.md:79`).
- `getUnknownKeyFailures` still builds `Format::toTag (Id::any)` as a local static (`jam_ConfigValidator.cpp`).
- `ConfigDocument::getValueTree (rootType, sheet)` equals the CSS path for valid cells only. Not reproduced: multi-token window and font-attribute cells, quoted cells, non-`0x` colour text, duplicate widget tables, a table named `appearance`.
- KANJUT is not on this machine (`D:\Poems\kuassa\user_modules` absent). The `ConfigDocument` key stamps and key index need the KANJUT sync check.
- END's own theme keeps its CSS round trip (`end/Source/config/ConfigTheme.cpp:29-32`).
- Next hot zones (ms): `layout style tree` 52, `tty open` 42, `vulkan flush recording` 30, `layout default tree` 25, `layout user tree` 27, `layout theme settings` 18, `layout documents` 17, `layout default file` 16.
- `launchPrompt` (OSC 133 A) is not detected, so `epoch` still holds the 5 s `read` of `launch-windows.sh`.
- Not done: `end/benchmark/pane-windows.md` (END loading EVE); the Ctrl+W markdown preview crash with EVE in END.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 101: END to the Front at Launch, Keys and Mouse Without the Active Window, Audit of Two Sprints, VST2 Host Window at the Plugin's Size ✅

**Date:** 2026-11-13
**Duration:** not recorded. The session ran over one compaction.
**Scope (ARCHITECT):** *"let's fix window focus and prefix keyboard not working"* (Windows, END hosts EVE as VST2). Then *"/audit 2"*: *"2 sprint: this one and last logged (havent been audited)"*. Then *"no risks allowed"* and *"clean up until /log no gate"*. Then the black lines at custom scales: *"HOST know its size always at any arbitrary scale"*; *"so can we solve this by patch? to enforce enax VST2 to scale with the exact SSOT method?"*; *"patch contract: must be as MINIMAL as possible"*. Contract rulings: *"END is the host, the owner, the orchestrator, END must be at DESKTOP IN FRONT FIRST"*; *"FULLY EVENT DRIVEN, nothing need to be handrolled"*; *"PATCHING juce IS FUCKING TRIVIAL. but it's not the best solution, it's always last resort."*

### Agents Participated
- COUNSELOR — cause reads, every validation read, the triage of three audits, `ARCHITECTURE.md` edits, trivial fixes, this log.
- Pathfinder (haiku) — discovery reads (launch chain, focus, key and mouse callbacks, close-pane chain, VST2, VST3 and AU size chains), two doxygen passes.
- Engineer (sonnet) — the Nexus callbacks, the jam foreground call, the risk fixes, the shell scripts, the VST2 size patch and the grid deletion.
- Auditor (opus) ×3 — this sprint (END, jam) and the URI and pane-size sprint of the handoff below.

### Files Modified (27 total)
**jam:**
- `jam_gui/windows/jam_StyleWindow.h:54-64`, `.cpp:39-42` — `setForeground (juce::ComponentPeer&) noexcept`: `SetForegroundWindow` on the peer's window. JUCE shows a window with `SW_SHOWNA` (`juce_Windowing_windows.cpp:1270`), which does not activate it.
- `jam_gui/windows/jam_Window.h`, `.cpp` — a `parentHierarchyChanged` foreground call was added, then deleted. It also fired on peer re-creation and on `setAlwaysOnTop` (`juce_Component.cpp:738`, `:1041`), and for every `jam::Window` (`DebugConsole`, `DebugModelMonitor`, `ModalWindow`). Net: no change.
- `jam_gui/commands/jam_CommandManager.h:44-65`, `.cpp:43`, `:61` — `invokeKey (key, componentState, asynchronously)`; the matched command runs through `invokeDirectly (id, asynchronously)`.
- `jam_gui/widgets/jam_TextEditor.cpp:615`, `:636`, `:658` — pass `false`. `jam_TextEditor.h:463-466`, `:1017-1024` — doxygen corrected to the code.
- `patch/juce-plugin-window-size-hook.patch` (new) — in `VSTPluginWindow::componentMovedOrResized` (Windows), the embedded host window takes the physical size of the plugin's `effEditGetRect` with `SetWindowPos`. `user-modules-info.md:1369` — its patch row.

**end:**
- `Source/Main.cpp:212-215` — after `window->setVisible (true)`, Windows: `jam::StyleWindow::setForeground (*peer)`. END, the owner of its window, tells it once.
- `Source/plugins/Nexus.cpp:6-11` — the ENAX key callback passes the `window` row of `ENDModel` to `invokeKey (key, state, true)`. It no longer reads JUCE's active top-level window.
- `Source/plugins/Nexus.cpp:13-23` — the ENAX mouse callback sends the event to the peer of `Desktop::findComponentAt`, else to the peer of the component under END's main mouse source.
- `Source/ui/ENDView.cpp:57` — `invokeKey (key, state, false)`.
- `Source/ui/EditorView.h`, `.cpp` — the pixel grid is deleted (`getEditorBounds`, `scaleNotifier`). The editor bounds are the local bounds reduced by the inset (`EditorView.cpp:59`). `setEditorBounds` asserts the editor (`:99`). Doxygen corrected.
- `Source/ui/SessionView.cpp:133-134` — the tab name reads `cwd` through `juce::URL::getLocalFile()`, after `isLocalFile()`. `removeEscapeChars` turned `+` into a space.
- `ARCHITECTURE.md` — the ENAX section: the key and mouse bullets; four extensions and four client rules that the code does not have were deleted; every `Nexus.cpp`, `jam_ENAXClientExtensions.cpp` and `EditorView.cpp` citation corrected; the Host automation row (`:743`) records the VST2 size patch.

**eve:**
- `Source/EVEView.h`, `.cpp` — `getUrl()` is the one reader of the `url` row. `openFile()` tests `isLocalFile()` once. `openFile (const juce::File&, terminal)` builds no second `juce::File`. The `markdown` command tests `isLocalFile()` before `getLocalFile()` (`juce_URL.cpp:415-421` asserts otherwise). `keyPressed` passes `false` to `invokeKey`. Doxygen corrected.
- `Source/EVETerminalProcessor.cpp:416-430` — the profile branch writes the `cwd` URI of the manifest folder. `EVETerminalProcessor.h:295`, `:298-299` — doxygen.
- `Source/shell/eve:17-30` — `_eve_cwd` lives here, next to `_eve_path`. A UNC folder gives `file://server/share`. `zsh_integration.zsh:4-6` sources `eve` and has no copy of `_eve_encode` or `_eve_cwd`. `bash_integration.bash:31-37` has no copy of `_eve_cwd`. `powershell_integration.ps1:15-17` uses `$PWD.ProviderPath` and gives the UNC form.
- `SPEC.md:227` — the OSC 7 row: the raw `file://` URI, without decoding.

### Alignment Check
- [x] BLESSED principles followed — E: END tells its window once (orchestrator tells); the key and mouse callbacks read no JUCE focus state. S: one reader of `url` and `cwd`, one copy of the shell helpers, one size for the VST2 host window and the plugin window.
- [x] NAMES.md adhered — `setForeground` joins `setButtons` / `setMenu`; `asynchronously` is JUCE's parameter name; `getUrl` joins the `get` family.
- [x] MANIFESTO.md principles applied
- [ ] Nothing of the risk fixes, the doxygen passes or the VST2 size patch is built. ARCHITECT built and ran only the foreground call (then in `jam::Window`) and the Nexus key callback: *"window and keyboard fixed."*

### Problems Solved
- **END never at front.** JUCE shows with `SW_SHOWNA` and has no `SetForegroundWindow` (`juce_Windowing_windows.cpp:1270`; `Process::makeForegroundProcess` is empty, `:5603-5604`). `toFront (true)` gives only `BringWindowToTop`; ARCHITECT's run: *"NOT IN THE FUCKING FRONT"*. END now calls `SetForegroundWindow` once, after the show.
- **Prefix keys and Ctrl+T dead.** The ENAX callbacks used JUCE's active top-level window, which is empty while EVE's child window holds focus (`juce_TopLevelWindowManager.h:62-77`, `:128-143`). They now read the Model and the desktop.
- **Ctrl+W inside the plugin's key call.** The close chain was synchronous to `editor.reset()` while the plugin's `ComponentPeer::handleKeyPress` was on the stack. A key from the plugin now posts its command.
- **Mouse release outside END.** `MouseInputSourceImpl::handleEvent` needs an END peer for the button-up (`juce_MouseInputSourceImpl.h:326-357`); the fallback gives it one.
- **Black lines at the pane edges, at any scale.** Under VST2 the plugin owns its size (`effEditGetRect`, `audioMasterSizeWindow`); the host follows. EVE's window is `round (w × s)` and its report is the same (`juce_PluginScaleFactorUtilities.h:128`, zero origin; `juce_Windowing_windows.cpp:1348`). The host converted the report to logical with a ceiling and back with `getSmallestIntegerContainer` at its position (`juce_VSTPluginFormat.cpp:130`; `juce_HWNDComponent_windows.cpp:60`), so the black `ViewComponent` (`juce_VSTPluginFormat.cpp:674-687`) showed one or two pixels. The grid hid this only at standard scales: at 110 % (106 DPI) its step was 48 logical pixels. The patch gives the host window the reported physical size; the grid is deleted. VST3 (host owns the size, `IPlugView::onSize`) and AU were read; no format change.
- **Audit, this sprint, jam (6 findings and 1 outside the set).** Resolved: 5 (`noexcept`). Rejected: 1 (CODING "Narrow pointer scope"; ARCHITECT: *"previously already adhere to coding standard instead you churn delete rewrite garbage"*), 2 (`toFront (true)` disproved by run), 3, 4, 6, the `jam_Platform.h` include (the Windows header wrapper, `jam_Platform.h:14-24`).
- **Audit, this sprint, END (6).** Resolved: F3, F4, F6 (`ARCHITECTURE.md`). Rejected: F1 (narrow pointer scope), F2 (two copies). F5 (no finding). Risks resolved: foreground on every hierarchy change and for every `jam::Window`, B1 (mouse release), B2 (Ctrl+W). Rejected: B3 (no side sets a global scale).
- **Audit, last sprint (16).** Resolved: F1 (grid replaced by the patch), F3, F6, F7, F8, F9, F12, F13, F15. Rejected: F2 (no move without a resize), F4, F5, F10, F11 (the PLAN locks the escape set; the one reader gives one file), F14 (the PCH skip costs time only when `Nexus.cpp` recompiles), F16 (no clause).
- **Doxygen.** Two passes, each block read against the code. Pathfinder finding "no writer of the root `outputBlock`" rejected: `jam_TextEditor.cpp:1035` writes it.
- **Mistakes owned.** A question a read answered; manual callbacks proposed; `toFront (true)` claimed to work; the macOS Hammerspoon listener called the owner of "to front"; an unneeded git read; an Auditor guard finding accepted against CODING; "two rulings cannot both hold" for the black lines (false: the cause is the VST2 contract and JUCE's roundings); a VST2-only, operating-system read proposed; a question that CONTRACT answered (MANIFESTO S, E).

### State for Continuation
- Run `cast cast/spell.md` in END and in EVE so the JUCE copy takes `juce-plugin-window-size-hook.patch`. Then build EVE, then END.
- Windows runtime checks: END at front at launch; Ctrl+W from EVE closes the pane with no assert; a drag that ends outside END ends; no black line at the pane edges at 125 %, 150 % and a custom scale; markdown links open; a UNC folder gives the right tab name; zsh in EVE has `_eve_encode` and `_eve_cwd` from `eve`.
- The size patch has no ENAX gate: it applies to every VST2 plugin that END hosts on Windows. Plugins that do not answer `supportsViewDpiScaling` still run JUCE's `resizeToFit` polling, which calls `updateHWNDBounds` (`juce_VSTPluginFormat.cpp:556-576`).
- Regenerate doxygen (END `ninja doxygen`; jam and EVE `<leader>bd`) and read the warning count.
- Open from the handoff below: profile pane creation and close on Windows with scout (no Windows manifest); scout launch-time optimisation; the Windows key routes 2-4 (not traced again after ARCHITECT's *"window and keyboard fixed"*).

### Debts Paid
- None

### Debts Deferred
- None

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

---

## Handoff to COUNSELOR: END Loading EVE Under One Second — Profiling Paused for the Document Contract

**From:** COUNSELOR
**Date:** 2026-11-13
**Status:** Blocked — on ARCHITECT's document contract enforcement (macOS session)

### Read First
1. `jam/RFC-config-document-parse-once.md` — the ConfigDocument findings handed to the document contract work. Its scout table is the baseline for the config zones.
2. This entry, `### Remaining`.
3. `~/.carol/skills/profile/SKILL.md` `## Builds` — the cast-only profile build, rewritten this session.
4. `end/benchmark/pane-windows-report.md` and `eve/benchmark/launch-windows-report.md` — the latest medians.

### Context
ARCHITECT asked for EVE launch and END-loading-EVE under one second, prompt ready to type, with no black pane. Sprint 102 cut EVE's layout parse. This session moved END's audio device open to a thread, built the Vulkan device in parallel on Windows, made the VST2 host panes transparent in END, and profiled the rest. ARCHITECT then took the config document path to the macOS session: *"config document optimization delegated. out of your scope."* and *"i will fix Document contract enforcement first, this also profiled and measured so will affect our work here. we will continue to optimize EVE and END after that."*

### Completed
- **Vulkan device in parallel on Windows.** `jam_VulkanGraphicsEngine.cpp:18` sets `externalContextFactory` on every OS. The Windows wait in `getOrCreate` is gone (`.h`). `createContext` marks the GPU unavailable and repaints when no device exists yet (`.cpp:489-493`).
- **No black host pane in END.** `jam/patch/juce-plugin-transparency-hook.patch` hunk on `juce_VSTPluginFormat.cpp`: `ViewComponent` and `Inner` are not opaque under `PluginHostType().isEND()` and paint black only when opaque. A PrintWindow capture showed the theme background (`scratchpad/end-profile-window.png`). ARCHITECT has not checked it at runtime.
- **END audio device on its own thread.** `Nexus` is a `juce::Thread`. `Nexus::run` opens `jam::ComApartment`, then `initialiseWithDefaultDevices (2, 2)` and `addAudioCallback` (`end/Source/plugins/Nexus.cpp:69-81`). `~Nexus` waits for it (`:142`). Device types stay on the message thread: the WASAPI type owns a `DeviceChangeDetector` (`juce_WASAPI_windows.cpp:1886`).
- **`jam::ComApartment`** moved to `jam_core/native/jam_ComApartment_windows.h/.cpp`, rows in `jam/user-modules-info.md`.
- **Window added to the desktop after its setup.** `jam_gui/windows/jam_Window.cpp`: the `DocumentWindow` base gets `addToDesktop = false`, and the constructor calls `addToDesktop()` at its end.
- **Profile builds go only through cast.** New `end/profile-info.md` (lane `profile`: `pluginBundle.cmake` with `CAST_LANE=profile`, jam spell, `Builds/Profile`). New `eve/profile-info.md` (lanes `vst-profile`, `standalone-profile`). `@profile-info` index rows in `end/cast/spell.md` and `eve/cast/spell.md`. Each scout manifest has one toolchain row: `cast | ../cast/spell.md --profile` (`end/benchmark/pane-windows.md`, `pane-windows-build.md`) or `--standalone-profile` (`eve/benchmark/launch-windows.md`). Cast reads every `## toolchain` table (`cast/Source/Processor.cpp:66`). Both runs exited 0; END's run packed `EVE v0.0.1 Windows scout.zip` through EVE's `--vst-profile` lane. `Source/generated/*` now comes from cast in both projects.
- **Measurements (scout, Release, 11 epochs, Windows, medians).**
  - END: epoch 2.77 s; application members 684 ms; config save 3.5 ms; config tables 2.8 ms; theme save 127 ms; theme load 289 ms; config resources 412 ms; nexus device types 49 ms; application window 234 ms; pane prompt (view construction → prompt painted) 240 ms; graph node 128 ms; pane editor 120 ms; nexus audio device 1.62 s (own thread).
  - The prompt shows at about 1.0 s after the members zone starts. The rest of the epoch is the quit: `~Nexus` `waitForThreadToExit (-1)` waits for the 1.6 s device open.
  - EVE standalone: epoch 1.27 s; launch prompt 0.82 s; processor layout 170 ms; processor prepare 175 ms = buffer prepare 125 ms (rows prepare 116 ms, swap prepare 10 ms, processor reset 0.01 ms) + tty open 48 ms; device open ~0; queues prepare 0.13 ms; processor delete 81 ms; standalone shutdown 186 ms.
- **Cause of `rows prepare` (read, not measured per part).** `virtualBlockSize` is 65536 on every OS (`eve/Source/EVETerminalProcessor.cpp:5`). The ring holds `blocksPerVblank × 65536 + rows` transit rows (`:528`), and `prepareRows` reserves every slot's arrays (`jam_TerminalBuffer.cpp:46-48`, `jam_AttributedString.cpp:157-164`). 65536 is the TTY read chunk on every OS: the processor asks `65536 - pendingBytes` (`jam_TerminalProcessor.cpp:72-73`); Windows `ReadFile` asks `readBuffer.getSize()` = 65536 (`jam_TerminalTTY_windows.cpp:850`); POSIX `::read` takes `maxBytes` (`jam_TerminalTTY_posix.cpp:320`). Rationale: `eve/SPEC.md:125-135` and commit `d1e2679`.

### Remaining
1. ARCHITECT: document contract enforcement (macOS). It covers `jam/RFC-config-document-parse-once.md`.
2. COUNSELOR, after the pull: re-measure both. Commands: `scout D:/Poems/dev/end/benchmark/pane-windows.md` from `end/benchmark`, and `scout D:/Poems/dev/eve/benchmark/launch-windows.md` from `eve/benchmark`. Compare `theme save`, `theme load`, `config resources`, `application members`, `processor layout`.
3. ARCHITECT: the decision on EVE's 116 ms `rows prepare` (Open Questions 1).
4. COUNSELOR: the remaining END zones — `application window` 234 ms, `graph node` 128 ms, `pane editor` 120 ms. EVE's zones are not visible in END's run: `Profiler::writeSamples` replaces the samples file (`jam_Profiler.cpp:174`), and only the standalone app writes EVE's.
5. COUNSELOR: the zsh marks in END. zsh in END emits no OSC 7 and no 133;A that reaches EVE (`EVE VST.ode`). With END's environment, zsh registers `_eve_precmd`, which prints them. Next read: a whatdbg trace of a Debug END-hosted EVE at the OSC dispatch (`jam_TerminalProcessor.cpp:430-436`).
6. Engineer, before the sprint log: remove every temporary profiler zone and its id.
   - END: `applicationBackground`, `applicationInitialise`, `applicationMembers`, `applicationShutdown`, `applicationWindow`, `graphicsEngine`, `nexusAudioDevice`, `nexusDeviceTypes`, `nexusFormats`, `nexusRegisterEVE`, `configResources`, `configSave`, `configTables`, `themeLoad`, `themeSave`, and the `panePrompt.emplace` in `ENDView::registerProfilerActions`.
   - EVE: `deviceOpen`, `deviceStart`, `queuesPrepare`, `bufferPrepare`, `rowsPrepare`, `swapPrepare`, `processorReset`.
   - jam: `ttyClose`, `ttyClosePseudoConsole`, `ttyExitPseudoConsole`, `ttyTerminate`.
   - The earlier launch zones of Sprint 102.
7. Pathfinder, post-audit doxygen pass. Stale blocks:
   - `jam_VulkanGraphicsEngine.h:47-50`, `:125-129`.
   - `end/Source/plugins/Nexus.h` (class, constructor, destructor).
   - `jam_TerminalTTY.h:76-77` and `eve/Source/EVETerminalProcessor.h:304`: both say `bufferSize` bounds one read; POSIX reads `maxBytes` (`jam_TerminalTTY_posix.cpp:320`).
   - `jam_TerminalSwap.h:40` vs `:45`.
   - `jam_TerminalBuffer.h:186-187` (`write` ready "when the handle goes out of scope"; `jam_TerminalBuffer.cpp:33` drops the handle at once).
8. ARCHITECT: runtime check of the transparent pane in END (Completed 2).
9. @Auditor once, then the sprint log on ARCHITECT's `/log`. Earlier threads still open: the intermittent quit hang with real zsh; the Ctrl+W markdown preview crash with EVE in END.

### Key Decisions
- *"let's fix eve loading time, then end loading eve time. must be as FAST as we can. profile with scout"*
- *"HOW DO WE MAKE THIS LOAD ENTIRELY LESS THAN 1s? Why we still have black panel edges?"*; *"THE REAL USAGE IS END HOST EVE"*; *"there are no black pane should ever exists"*
- *"ensure the transparency patch is small surface and END only"*
- END opens its sound card *"On a background thread"* (AskUserQuestion answer).
- *"DOCUMENT CONTRACT ALWAYS NON-NEGOTIABLE: built complete AST once, memoized, cached, state update, 0 rules"*
- *"config document optimization delegated. out of your scope."*
- *"you build project with cast, jam always automatically built"*; *"NEVER FUCKING USE MANUAL NINJA. ALWAYS WITH CAST"*
- *"NO. no touching project-info.md"*; *"you only need to write profile-info.md whatever as the manifest to build the profiling build with cast"*
- *"i will fix Document contract enforcement first, this also profiled and measured so will affect our work here. we will continue to optimize EVE and END after that."*

### Files Modified
- `jam/jam_vulkan/engine/jam_VulkanGraphicsEngine.h`, `.cpp` — parallel device build on Windows.
- `jam/patch/juce-plugin-transparency-hook.patch` — END-only transparent `ViewComponent` and `Inner`.
- `jam/jam_core/native/jam_ComApartment_windows.h`, `.cpp` (new); `jam/jam_core/jam_core.h`, `.cpp`; `jam/user-modules-info.md` — `ComApartment` in jam_core.
- `jam/jam_terminal/image/native/jam_TerminalImageSequence_windows.cpp` — local `ComApartment` deleted.
- `jam/jam_gui/windows/jam_Window.cpp` — `addToDesktop` after setup.
- `jam/jam_terminal/tty/native/jam_TerminalTTY_windows.cpp`, `jam/cast/terminal.md`, `jam/generated/jam_Terminal.*` — temporary TTY close zones.
- `jam/RFC-config-document-parse-once.md` (new).
- `end/Source/plugins/Nexus.h`, `.cpp` — audio device thread; temporary zones.
- `end/Source/Main.h`, `.cpp`, `end/Source/config/ConfigModel.cpp`, `ConfigTheme.cpp`, `end/Source/ui/ENDView.cpp`, `end/cast/identifiers.md`, `end/Source/generated/Identifiers.*` — temporary zones.
- `end/profile-info.md` (new), `end/cast/spell.md` — the `profile` lane.
- `end/benchmark/pane-windows.md`, `pane-windows-build.md` (new), `pane-windows.sh` (new) — the END benchmark, cast toolchain row, and `133;A` shell.
- `eve/profile-info.md` (new), `eve/cast/spell.md` — the `vst-profile` and `standalone-profile` lanes.
- `eve/benchmark/launch-windows.md` — cast toolchain row.
- `eve/Source/EVETerminalProcessor.cpp`, `eve/cast/identifiers.md`, `eve/Source/generated/Identifiers.*` — temporary prepare zones.
- `~/.carol/skills/profile/SKILL.md` — cast-only toolchain example and `## Builds` text.

### Open Questions
1. EVE's 116 ms `rows prepare`. Options: a smaller `virtualBlockSize` (`EVETerminalProcessor.cpp:5`); a cap on the rows one `process` writes into the ring, so the ring holds rows plus a fixed transit count; or keep the sizing and move `prepare` off the path to the prompt. Decider: ARCHITECT.
2. The quit waits up to 1.6 s for the device thread (`Nexus.cpp:142`). Whether that matters is ARCHITECT's call.

### Next Steps
- Trigger: ARCHITECT says the document contract work is pulled to this machine.
- Then COUNSELOR runs Remaining 2, and continues with Remaining 3 and 4.

