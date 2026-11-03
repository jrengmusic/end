# PLAN: Load Plugin — one load path, async instance, deterministic pane editor, fit to pane

**RFC:** RFC-instant-plugin-loading.md (parent `PLAN-instant-plugin-loading.md`, Amendment row 5 "The pane chain is synchronous on the message thread", row 6 "The terminal restarts several times while the pane grows", Step 12 `%` and OMP help)
**Date:** 2026-11-04
**Status:** locked 2026-11-04 (ARCHITECT approval).
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM. One object, one translation unit (CODING.md). No comment and no doxygen before the audit (CAROL.md Code Hygiene). Agents run no build; scout manifests hold the profile builds; runtime builds are ARCHITECT's.

## Overview

END loads every plugin, EVE included, through one registered action. The pane row holds the plugin file, and Session creates the instance asynchronously from that Model event. The pane builds its editor after the instance exists and after the pane is laid out, so EVE's first winsize is the final one. The window state persists in `~/.config/end/END.settings`. A fixed-size editor is fitted into its pane with `juce::RectanglePlacement`, and a minimal JUCE patch makes the native plugin view follow that fit on macOS. A runtime gate decides the final patch shape.

## Step 0 — Hosting contract (ARCHITECT, verbatim; no code)

- *"END will host ANY 3rd party audio plugin VST, VST3, AU"* (CLAP later: `DEBT-20261025T135707`).
- *"audio plugin instance could be detach/attach on its own window, IIUC this alrady the contract with audio plugin host"*.
- *"by default (including EVE), resizable audio plugin resized into the target pane."*
- *"any arbitrary plugin which usually have fixed size (or scaled), should be fitted into available pane bounds by keeping their aspect ratio, only scaled."*
- *"future work will can make the "docked" pane, detachable and redraw its EditorView into its dedicated window floating (like how other daw create plugin instance). Editor only, so not recreating new instance from scratch"*.
- *"undocking pane, will remove the split. and vice versa, docking back to pane create new split."*
- *"the most BLESSED approach without fighting what juce have OOTB. so future work is extension and scaling. not rewriting and redesigning anything"*.
- *"this is END as the HOST. we control of how they created their Editor … they are dumb, only acknowledge their own size. when docked, we scaled whatever their width/height to available pan with their fixed size. when they are resizable, we resize to fit. otherwise undocked, dedicated window, their own actual size."*
- *"size parameters is exclusive to EVE ENAX … size parameters is ENAX exclusive lane"*.
- *"no handroll anything, no manual arithmetic. juce::Rectangle probably have everything you need"*.
- *"i wanted to ensure the patch is as MINIMAL as as possible"*.

How this plan keeps the dock/undock future an extension: the instance lives in Session (graph), the editor lives in the pane (`EditorView::editor`). The editor is created from the Model event and destroyed with its owner. A floating window later owns an editor of the same instance through the same `createEditorAndMakeActive` path that AudioPluginHost uses (`PluginGraph.cpp:165-198`). No step here binds the instance to the pane component.

## ARCHITECT rulings (this session)

- Load contract: *"loading plugin, ANY PLUGIN including EVE is ONE PATH. one SSOT unique commandId"*; *"use juce::FileDragAndDropTarget"*; *"files::evePlugin is default args"*; *"drop file is override"*; *"LOADING any plugin should be SSOT method, a registered command, a callback, hence any component, any events, anything could acces the function to load plugin"*; *"so either loading EVE, loading any drag and drop plugin. ONE COMMAND"*.
- Load shape: *"Pane row event (Recommended)"*, with *"NO NODE. @NAMES.md"*.
- Pending creation: *"Model decides (Recommended)"*.
- Async creation: *"Async instance (Recommended)"*.

## Amendment 2026-11-04 — drop target (ARCHITECT runtime ruling)

- Runtime: *"drag and drop VST3, nothing happen"*. Every JUCE window view registers for drags (`juce_NSViewComponentPeer_mac.mm:198`), so a drop over EVE goes to EVE's own JUCE.
- ARCHITECT: *"with current design there is no "empty pane", each pane is a terminal, an EVE instance. i think drag and drop should be END as target to load into new split, new pane, WITHOUT terminal. but each pane STILL must receive drag drop precisely to support, forward mouse events, like dropping file samples to plugin that support it"*; ruling *"END surfaces only"*.
- Edge: ARCHITECT: *"NO HARDCODE, no HANDROLL ANYTHING. fully event driven. IIUC newPane already a command, so dropping plugin file only chain of existing command"*; *"why not edge detection? we already capture the relative coordinate no?"*; ruling *"jam PaneComponent::getEdge"*: new `juce::Identifier jam::PaneComponent::getEdge (juce::Point<int> position) const noexcept`, beside `getCorner` (`jam_PaneComponent.h:74-79`), returns the side nearest the point. The drop runs `newPane (pane.getEdge (point), file)`; `Id::right` is no longer used for a drop.
- Crash fix: `widthName` at file scope ran `jam::Format::toParameterName` during image load (`_GLOBAL__sub_I_EditorView.cpp`). It is now `getWidthName()`, a function-local static (sibling `getDisplaySheet`, `jam_PluginEditor.cpp:5-10`).
- `getEdge` uses `juce::Line::isPointAbove` against the two diagonals of the pane bounds (ARCHITECT: *"you are not reading juce API thoroughly"*), not a distance table. `Id::bottom` moved from `end/cast/identifiers.md` to `jam/cast/data_structures.md` beside `top`.
- No master (ARCHITECT: *"EVE DOES NOT USE MASTER. the table definition IS WRONG"*): EVE `parameters.md` group cells are empty. A cell of spaces yields no tokens and `getTableValue` returns `""` (`jam_MarkdownBlockParser.cpp:1638-1645`, `:1539-1556`, `:1574-1627`). jam `Format::toParameterName` returns the name alone for an empty group; `ParameterLayout::Group` creates groups only for non-empty cells. END `EditorView::getParameter` compares with `compareIgnoreCase` against `Id::width` and its siblings; `getWidthName` and every `toParameterName (Id::master, …)` are deleted.
- Design: `TabView` is the `juce::FileDragAndDropTarget`; `EditorView` is not. `newPane` takes `(const juce::Identifier& edge, const juce::File& file)`; every existing caller passes the EVE file (*"files::evePlugin is default args"*). A dropped plugin binary focuses the pane under the cursor (`setFocusedChild`, sibling `TabView::focusPane`) and runs `newPane (Id::right, file)` — `split` acts on the focused pane (`jam_MatrixComponent.cpp:64`), and `Id::right` is the edge of the `splitVertical` command. The format check is `getActiveSession().getPluginFormat`.

## Amendment 2026-11-04 — drop target in front, command chain, EVE is known (ARCHITECT ruling)

This amendment supersedes line 38 (its sentence "so a drop over EVE goes to EVE's own JUCE" had no runtime proof) and the Design line above (`TabView` as the drop target, `newPane (edge, file)`).

- Runtime evidence (`~/.config/end/end.ode`): three drops at the centre of an EVE pane logged no `END interest`, `dragEnter`, `dragMove`, `dragExit` or `drop` line. A drag in the pane padding logged `dragMove` at x 2 to 9 and a `dragExit` when the cursor moved inward.
- Drop target. ARCHITECT: *"blank component to front. always in front of everything, this is the FileDragAndDropTarget. if mouse enter, dragged into this area, it also automatically change the target to be FOCUSED. reminder, focus tracking is already wired, the whole chain of state machine relies on this focus tracking, so nothing to be reinvent, nothing should be redesigned, no manual state management, everything is already event driven. EXTEND and INTEGRATE."* ARCHITECT's evidence: *"this is splitting from corner drag, IN FRONT OF EVE. works"*.
  - `EditorView::DropTarget` is a nested `juce::Component` and `juce::FileDragAndDropTarget` (sibling `jam::PaneComponent::CornerButton`). `EditorView` owns it as `dropTarget`, with the bounds of the pane.
  - Z-order: `dropTarget` is in front of every child. ARCHITECT: *"0 is to back? motherfucker, why not simply dropTarget.toFront(false)?"*. The constructor adds it with `addAndMakeVisible (dropTarget)`. `createProcessorEditor` inserts the editor at index 0 and then calls `dropTarget.toFront (false)` (`EditorView.cpp`, ARCHITECT's edit).
  - `fileDragEnter` focuses the pane through `jam::OwnerComponent::setFocusedChild` (`jam_OwnerComponent.cpp:69-72`).
  - `filesDropped` runs `newPane (pane.getEdge (point))`, then `loadPlugin (path)`.
  - `TabView` is no longer a `FileDragAndDropTarget`.
- Command chain. ARCHITECT: *"split with direction should be command. load plugin is command. drag and mouse up call the chain"*. ARCHITECT: *"split by what? of course always in half. why the fuck the logic is not same as splitting with keyboard shortcut? this is actually SSOT command that exactly should call that, the difference only which plugin instance should be loaded"*.
  - `newPane (const juce::Identifier& edge)` splits the focused pane at jam's default position (`jam_MatrixComponent.h:53`), or adds a pane when the edge is invalid. A new pane becomes the focused child (`jam_MatrixComponent.cpp:340-344`).
  - `loadPlugin (const juce::String& plugin)` sets `Id::plugin` of the focused pane row to `getPath (Id::plugins).getChildFile (plugin)`. `getChildFile` keeps an absolute path (`classjuce_1_1File.xml:809`). Thus `files::evePlugin` is the default argument and a dropped path is the override.
  - Callers: `newTab`, `splitHorizontal` and `splitVertical` run `newPane`, then `loadPlugin (files::evePlugin)`. The corner mouse-up keeps `split (edge, proportion)` (`TabView.cpp:118`), then runs `loadPlugin (files::evePlugin)`. The drop runs `newPane`, then `loadPlugin (path)`.
- EVE is known. ARCHITECT: *"EVE is a plugin binary carried by END itself. it is KNOWN. why do we need to scan anything?"*; *"have you ensure eve.vst always on known list at init? so EVE as default loaded plugin for each pane NEVER scan?"*.
  - `Nexus` adds EVE's `juce::PluginDescription` to `knownPluginList` at construction. It sets `pluginFormatName` from the format that accepts the file, `fileOrIdentifier`, and each field that `pluginNeedsRescanning` compares. `findFormatForDescription` reads `pluginFormatName` and `fileOrIdentifier` (`juce_AudioPluginFormatManager.cpp:139-152`). VST2 creation reads `fileOrIdentifier` and `uniqueId`/`deprecatedUid` (`juce_VSTPluginFormatImpl.h:2789-2798`). `scanAndAddFile` then returns the listed description without a scan (`juce_KnownPluginList.cpp:163-182`).
- Fit. ARCHITECT: *"by default, the convention, the pattern, each plugin loaded in a host. host create window, a floating dedicated window for PluginEditor. PluginEditor for each plugin can request and decide the window size, because the window size always content aware size. this is FACT"*; *"we will implement the exact same pattern, same convention. for future undocked PluginEditor, undocked ours EditorView"*; *"for now, EditorView always as Pane. when they are tiled structurally within MatrixComponent, its content MUST FIT. this non-negotiable"*; *"get the the orientation, use the orientation to decide which side is the constraint, then scale. facts: ALL OF THIS METHODS already available in juce even jam. what juce dont provide jam do, OOTB, no fucking handroll, no manual arithmetic"*.
  - Runtime: JFS (not resizable) took the `setTransform` branch, and its editor grew from 652×1178 to 1417×2560 (`end.ode`, 1:50 run). A Pro-Q 3 pane of 320 px rendered across the full window.
  - Read: JUCE states that a host transform on a hosted editor "will obliterate it" (`juce_AudioProcessorEditor.cpp:194-199`). The VST3 host window converts plugin sizes through `getLocalArea (nullptr, …)` (`juce_VST3PluginFormat.cpp:342-347`). A JUCE VST3 plugin returns `kResultFalse` from `setContentScaleFactor` on macOS (`juce_audio_plugin_client_VST3.cpp:2187-2211`).

## Amendment 2026-11-04 — fit deferred, editor once, OMP race, instrumentation (ARCHITECT ruling)

- Fit deferred. ARCHITECT: *"add to @DEBT.md handling plugin editor size with pane. not yet decided how."* Entry `DEBT-20261103T195500`. ARCHITECT: *"remove the transform for now."* `setEditorBounds` has two branches: a resizable editor takes `setBounds` with the inner pane area; any other editor moves to the inset position. Step 8 and Step 9 move to that debt.
- Patch. ARCHITECT: *"delete the patch"*. `jam/patch/juce-plugin-view-scale-hook.patch` is deleted.
- Editor once. ARCHITECT: *"editor must be created only ONCE."* On macOS, the JUCE VST2 host sends `effEditGetRect` before `effEditOpen` (`juce_VSTPluginFormat.cpp:95`, `:350-351`). The VST2 client builds the editor on `effEditGetRect` (`juce_audio_plugin_client_VST2.cpp:1500`). Then `effEditOpen` calls `deleteEditor (true)` and builds it again (`:1524-1525`). New `jam/patch/juce-plugin-editor-once-hook.patch` deletes `deleteEditor (true);` from `handleOpenEditor`. Its row replaces the scale patch row at `jam/user-modules-info.md:1371`, and it applies after `juce-plugin-transparency-hook.patch`.
- OMP race. ARCHITECT: *"fix the OMP bug (init race)"*; ruling *"Drop bind write"*.
  - The winsize attachment restarts the terminal at once on the message thread (`classjam_1_1Model_1_1ParameterAttachment.xml:151`, `EVETerminalProcessor.cpp:21-22`).
  - `TextEditor::bindToModel` wrote the winsize at the default layout, 110×37 (`jam_TextEditor.cpp:94`). `EVEView` then calls `setScaling`, and the pane size writes 85×104 (`EVEView.cpp:14`).
  - The second restart reuses the shell (`jam_TerminalTTY_posix.cpp:248-250`) and sends `SIGWINCH` (`:357-370`). ARCHITECT's zshrc runs `TRAPWINCH() { omp_repaint_prompt }` (`~/.config/zsh/zshrc:181`) before OMP's first `precmd`, so OMP receives an empty `--status`.
  - Fix: `bindToModel` no longer calls `setWinsize`. `TextEditor::resized` publishes the winsize at the final layout. Fact: when the final terminal bounds equal the default layout bounds, `resized` does not run, and no winsize is written.
- Load speed. ARCHITECT: *"EVE loading must be AS FAST AS POSSIBLE. no bottleneck."* The EVE description is listed at startup, so the first load does not scan. The settings memo of `PLAN-settings-once.md` is in jam (`jam_PluginEditorLayout.cpp:13-26`). The editor is built once.
- Instrumentation. ARCHITECT: *"remove all ode instrumentation. so we could focus to plan"*. Every `debug::Log` line of this plan and of the parent plan is deleted in END, EVE and jam. The `JAM_USING_PROFILER` zones stay for scout.

## Amendment 2026-11-04 — panes and tabs driven by the Model (ARCHITECT ruling)

- Audit finding 28: `closePane` and `closeTab` ask `getChildCount() > 1` and decide for the owner (MANIFESTO E *"Almost never a getter — if the orchestrator needs to ask, the design is wrong."*). ARCHITECT: *"don't just reason from loc. understand comprehensively the design against contract. isn't tab could be fully event driven from Model?"*, then `/go`.
- Read: the view drives the Model today. `OwnerComponent::add` builds the component, and its `Model::Component` base appends the row (`jam_Model.h:154-158`). `TabView::childRemoved` and `SessionView::remove` remove the rows (`TabView.cpp:34-38`, `SessionView.cpp:29-35`). No class on this path reacts to `valueTreeChildAdded` or `valueTreeChildRemoved` (`jam_OwnerComponent.cpp:102-120`, `SessionView.cpp:57-71`, `jam_TabbedComponent.cpp:49-58`).
- Design, as corrected by the read (`juce_ValueTree.cpp:97-114`, `:976-985`; `juce_ListenerList.h:393-403`; `jam_MatrixComponent.cpp:79-84`, `:219-235`, `:289-310`):
  1. Tabs are driven by the Model. `SessionView::add` appends a tab row. `SessionView::remove` removes it. `SessionView::valueTreeChildAdded` and `valueTreeChildRemoved` create and destroy the `TabView` (adopt constructor) for a tab row of its own row. Tab rows are flat and never move (`jam_TabbedComponent.cpp:66-69` moves the bar button only).
  2. Last tab quits as a Model rule. When `SessionView` loses its last tab row, it runs the `quit` action. `closeTab` removes the focused tab row only, and asks nothing.
  3. Close order from JUCE. `Session` listens on the parent `sessions` row. JUCE notifies the session row (`SessionView`) before its ancestors (`juce_ValueTree.cpp:97-102`), so the editors die before `Session::removePlugins` removes the plugins of the removed tab row (MANIFESTO B). On one node, handles are notified in registration order (`juce_ValueTree.cpp:976-985`), and `Session` registers before `SessionView`. That is why `Session` must not listen on the session row.
  4. Panes stay imperative. `MatrixComponent` moves live pane rows by detaching and re-adding them (`split`, `setRow`, `swap`, `rotate`). JUCE sends `valueTreeChildRemoved` for each detach (`juce_ValueTree.cpp:110-114`), so a removal event does not mean a pane closed. `closePane` keeps `remove`, then `removePlugin`.

## Decisions taken from CONTRACT (citation per line)

1. **Close keeps the told order.** `OwnerComponent::remove` calls `childRemoved` (row removal) before it destroys the pane and its editor (`jam_OwnerComponent.cpp:48-56`, `TabView.cpp:33-37`). Removing the instance on the row-removal event would delete the processor while its editor lives. MANIFESTO **B**: *"No ambiguous destruction order."* So `closePane` and `closeTab` keep `remove` then `removePlugin` (`ENDView.cpp` closePane, closeTab). The "Model decides" callback still discards a late instance, because a removed row is no longer a sub-tree of the session row (`ValueTree::isAChildOf`: *"Returns true if this tree is a sub-tree (at any depth) of the given parent"*, `classjuce_1_1ValueTree.xml:1079`).
2. **Replace order comes from JUCE.** A property change notifies the changed row's listeners first, then each parent's (`juce_ValueTree.cpp:98-108`), synchronously (`:139-146`). `EditorView` listens on its own pane row, `Session` on the session row. So on `Id::plugin`, the pane releases its editor before Session removes the old instance. No flag.
3. **The key-bound command path cannot carry a file.** `ENDView::perform` → `ENDActions::run (id)` passes no argument (`ENDView.cpp:89`, `ENDActions.cpp:7`), and `Function::Map::get` asserts on a signature mismatch (`jam_Function.h:270-273`). The unique id of the load command is the action id `Id::loadPlugin` in the action registry (*"Action registry that the commands and the events run"*, `ENDView.h:105-106`). No `CommandId` bimap row is added.
4. **A path-only description does not load VST3 or CLAP.** VST3 matches `desc.name` and `desc.uniqueId`/`deprecatedUid` (`juce_VST3PluginFormatImpl.h:1374-1377`). CLAP matches `description.uniqueId` (`jam_ClapPluginFormat.cpp:307`). AU reads only `fileOrIdentifier` (`juce_AudioUnitPluginFormatImpl.h:519-528`). The framework API that fills the description is `juce::KnownPluginList::scanAndAddFile`: *"Looks for all types that can be loaded from a given file, and adds them to the list"*; with `dontRescanIfAlreadyInList` true *"the file will only be loaded and re-tested if it's not already in the list, or if the file's modification time has changed"* (`classjuce_1_1KnownPluginList.xml:325-359`). Sibling: plot-linear `PluginWindow.h:137`. CODING.md: *"Use the framework API fully."*
5. **Window state uses the JUCE pair.** `ResizableWindow::getWindowStateAsString` / `restoreWindowStateFromString` (*"restores the window's size, position and full-screen status"*, `classjuce_1_1ResizableWindow.xml:703-738`). `PropertiesFile` saves on destruction (*"When deleted, the file will first call saveIfNeeded()"*, `classjuce_1_1PropertiesFile.xml:177`). Sibling owner: `EVEStandaloneApp.h:16`.
6. **No new name contains `node`** (NAMES.md Rule 6). `graph.addNode`, `removeNode`, `NodeID` are JUCE API.

## Language / Framework Constraints

- LANGUAGE.md C++/JUCE: new bodies go into each object's one `.cpp`. No new `.cpp` file.
- CAROL.md Code Hygiene: no comment, no doxygen before the audit.
- CAROL.md Build, MEMORY `feedback_no_build`: no agent build. Step 7 scout manifests build the Profile binaries. ARCHITECT builds Release and Debug.

## Dependency & API Inventory

- **JUCE async creation:** `AudioPluginFormatManager::createPluginInstanceAsync` — *"The callback function will be called on the message thread"* (`juce_AudioPluginFormatManager.h:101-125`, headless module). The format posts an `AsyncCreateMessage` and builds the instance in `handleMessage` on the message thread (`juce_AudioPluginFormat.cpp:93-105`). The pane chain finishes first; the build runs on the next message.
- **JUCE scan:** `juce::KnownPluginList::scanAndAddFile (path, true, typesFound, format)` (`juce_KnownPluginList.h:100-103`).
- **JUCE fit:** `juce::RectanglePlacement::getTransformToFit (source, destination)`; default placement is `centred` (`juce_RectanglePlacement.h:148-175`). `Component::setTransform`.
- **JUCE drop:** `juce::FileDragAndDropTarget::isInterestedInFileDrag`, `filesDropped` (sibling plot-linear `MainComponent.cpp:20-48`).
- **JUCE window:** `getWindowStateAsString`, `restoreWindowStateFromString`; `juce::PropertiesFile` (`EVEStandaloneApp.h:16`).
- **JAM:** `jam::Function::Map::add<Args...>` / `get` (`jam_Function.h:233-275`). `MatrixComponent::split` returns `UUID` (`jam_MatrixComponent.cpp:62-89`); `TabView::add` returns `jam::UUID` (`TabView.cpp:12-19`). The pane row is appended in the `Model::Component` base before the `EditorView` body runs (`jam_Model.h:154-158`).
- **END patterns pasted into each Engineer prompt:**
  - Argument action: `actions.add<const juce::Identifier&> (Id::newPane, [this] (const juce::Identifier& edge) { … })` (`ENDView.cpp:164-184`); invocation `actions.get (Id::newPane, edge)` (`:160`).
  - Model-event load today: `Session::valueTreeChildAdded` (`Session.cpp:31-37`); `addPlugin` (`Session.cpp:39-93`); format lookup `std::find_if (formats … fileMightContainThisPluginType …)` (`Session.cpp:48-52`).
  - Pane property listener: `EditorView::valueTreePropertyChanged` (`EditorView.cpp:136-145`); editor creation `createProcessorEditor` (`:147-171`).
  - Session reference parameter: `juce::AudioPluginFormatManager& newFormatManager` (`Session.h:28-35`), passed by `Nexus::createSession` (`Nexus.cpp:84-93`).
  - JUCE patch row: `jam/user-modules-info.md:1358-1370`; patch files `jam/patch/juce-*-hook.patch`.

## New names (NAMES.md Rule 5; family cited)

| Name | Kind | Family |
|---|---|---|
| `loadPlugin` | `@id` row, action id | action ids `newPane`, `closePane`, `newTab` (`cast/identifiers.md`); ARCHITECT: *"load plugin is load plugin"* |
| `plugin` | `@id` row, pane row property | `plugins` (`cast/identifiers.md:130`), row properties `name`, `focus` |
| `settings file name` | `files.md` row, value `END.settings` | EVE `standalone settings file name` (`eve/cast/files.md:35`) |
| `settings` | `ENDApplication` member `juce::PropertiesFile` | `EVEStandaloneApp::settings` (`EVEStandaloneApp.h:16`) |
| `knownPluginList` | `Nexus` member, `Session` reference | plot-linear `PluginWindow::knownPluginList` (`PluginWindow.h:177`); `formatManager` reference shape |
| `Session::loadPlugin (juce::ValueTree& paneRow)` | requests the instance | ARCHITECT's term; Session family `addPlugin`, `removePlugin`, `getPlugin` |
| `Session::addPlugin (juce::ValueTree paneRow, std::unique_ptr<juce::AudioPluginInstance> instance)` | inserts the instance into the graph | existing `addPlugin` (`Session.h` addPlugin) |
| `Session::getPluginFormat (const juce::File&) const` | returns the format that accepts a file | `getPlugin`; two callers (load, drop check) |
| `EditorView::getSession()` | private, returns the pane's `Session&` | `Nexus::getSession`; three callers |
| `juce-plugin-editor-once-hook.patch` | jam JUCE patch (replaces the deleted `juce-plugin-view-scale-hook.patch`) | `juce-plugin-transparency-hook.patch`, `juce-plugin-window-focus-hook.patch` |
| `EditorView::DropTarget`, `dropTarget` | nested drop target component, its member | `jam::PaneComponent::CornerButton` |

## Validation Gate

COUNSELOR validates each step before the next, against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, for the whole sprint.

## Steps

### Step 1: Cast rows
**Scope:** `end/cast/identifiers.md`, `end/cast/files.md`.
**Action:**
1. `identifiers.md`: add `| @id | loadPlugin | `loadPlugin` | |` and `| @id | plugin | `plugin` | |` in alphabetical order (row shape `:98`).
2. `files.md`: append `| @string | settings file name | toCamel | END.settings | toLiteral | |` to the `## files` table (row shape `:31-33`, description cell filled in the doxygen pass).
**Validation:** each row is a byte copy of its sibling shape. Alphabetical order holds. No other table changes. ARCHITECT's cast run regenerates `Source/generated/*`.

### Step 2: Nexus owns the plugin list
**Scope:** `Source/plugins/Nexus.h`, `Nexus.cpp`, `Source/plugins/Session.h`, `Session.cpp` (constructor only).
**Action:**
1. `Nexus.h`: add `juce::KnownPluginList knownPluginList;` after `formatManager`.
2. `Session` constructor: add the parameter `juce::KnownPluginList& newKnownPluginList` after `newFormatManager`, and the member `juce::KnownPluginList& knownPluginList;` after `formatManager` — the same shape as the format manager.
3. `Nexus::createSession` (`Nexus.cpp:88`): pass `knownPluginList`.
**Validation:** shape equals the `formatManager` reference. Nexus outlives every Session (`Nexus.h` `sessions` member), so the reference is bound.

### Step 3: Session loads from the pane row event (delete first)
**Scope:** `Source/plugins/Session.h`, `Session.cpp`.
**Action:**
1. Delete `valueTreeChildAdded` (`Session.cpp:31-37`, `Session.h` declaration). Delete the hardcoded file line (`Session.cpp:47`).
2. Add `juce::AudioPluginFormat* getPluginFormat (const juce::File& file) const` — the `find_if` of `Session.cpp:48-52`, returning the format pointer, or `nullptr` when `std::find_if` reaches the end.
3. Add `void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override`. When `property == Id::plugin`: it calls `removePlugin (uuid)` of that row, then `tree.removeProperty (Id::name, nullptr)`, then `loadPlugin (tree)`. It consumes the payload: the tree and the property.
4. Add `void loadPlugin (juce::ValueTree& paneRow)`:
   1. `const juce::File file { paneRow.getProperty (Id::plugin).toString() };`
   2. When `getPluginFormat (file)` is not `nullptr`: `knownPluginList.scanAndAddFile (file.getFullPathName(), true, typesFound, *format)`.
   3. When `typesFound` is not empty: `formatManager.createPluginInstanceAsync (*typesFound.getFirst(), graph.getSampleRate(), graph.getBlockSize(), [this, paneRow, file] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String&) { … })`.
   4. The callback adds the instance only when `instance != nullptr`, `paneRow.isAChildOf (state)`, and `juce::File { paneRow.getProperty (Id::plugin).toString() } == file`. It then calls `addPlugin (paneRow, std::move (instance))`. Otherwise the `unique_ptr` destroys the instance.
5. `addPlugin (juce::ValueTree paneRow, std::unique_ptr<juce::AudioPluginInstance> instance)`: the body of today's `if (instance != nullptr)` block (`Session.cpp:63-92`): `enableAllBuses`, `graph.addNode`, `plugins.try_emplace`, the connections, then `paneRow.setProperty (Id::name, …)` last. The `sessionPlugin` zone moves to the top of `loadPlugin`; the `graphNode` zone stays at the top of `addPlugin`.
Correction 2026-11-04: `addPlugin` takes the instance unconditionally. The callback is the one owner of the `instance != nullptr` invariant (MANIFESTO **D**), so the `jassert` and the `if` wrapper in `addPlugin` were removed.
**Validation:** `valueTreeChildAdded` is gone; no caller remains (LSP `findReferences`). No flag; the Model decides. Positive nesting, no bail-out. `juce::File` equality, no string `==`. Each function ≤ 30 lines.

### Step 4: The one load action
**Scope:** `Source/ui/ENDView.cpp`.
**Action:**
1. In `registerTabActions`, add `actions.add<const jam::UUID&, const juce::File&> (Id::loadPlugin, [this] (const jam::UUID& pane, const juce::File& file) { … })`. `Map::get` deduces `Args` from forwarding references and casts to `Element<void, Args...>` (`jam_Function.h:267-273`). Thus each caller passes named `const` lvalues, as `newTab` does with `const juce::Identifier edge {}` (`ENDView.cpp:159-160`). The body follows the `newPane` nesting (`ENDView.cpp:174-183`): active session view → active tab view → `tabView->get (pane).getValueTree().setProperty (Id::plugin, file.getFullPathName(), nullptr)`.
2. `newPane` (`:178-181`): `const jam::UUID pane { edge.isValid() ? tabView->split (edge) : tabView->add() };` and `const juce::File plugin { ConfigDirectory::Config::getPath (Id::plugins).getChildFile (files::evePlugin) };` — the line removed from `Session.cpp:47` — then `actions.get (Id::loadPlugin, pane, plugin)`.
3. `closePane` and `closeTab` do not change (Decision 1).
**Validation:** one load entry: every caller uses `actions.get (Id::loadPlugin, …)`. The deduced argument types at each call equal the registered `<const jam::UUID&, const juce::File&>` (`jam_Function.h:267-273`).

### Step 5: The pane builds its editor from the Model, and takes drops
**Scope:** `Source/ui/EditorView.h`, `EditorView.cpp`.
**Action:**
1. Delete the `createProcessorEditor()` call in the constructor (`EditorView.cpp:9`).
2. Base list: add `public juce::FileDragAndDropTarget` (sibling `MainComponent.h:16`).
3. Add private `Session& getSession()`: the lookup of `createProcessorEditor` (`EditorView.cpp:149-157`). `createProcessorEditor` uses it.
4. `valueTreePropertyChanged` (`:136-145`), consuming the payload:
   - `Id::plugin`: release the editor with the destructor's existing sequence (Engineer reads `EditorView.cpp:11-18` first and reuses it verbatim).
   - `Id::name`: when the row holds a name, call `resized()` (size parameters before the editor exists), then `createProcessorEditor()`, then `resized()` (editor bounds). Correction 2026-11-04: the first draft omitted the second `resized()`, so the new editor had no layout.
5. `resized()`: when `getSession().contains (uuid)`, `setSizeParameters (inner)` runs, with or without an editor; `setEditorBounds (inset)` runs when `editor != nullptr`. `setSizeParameters` takes the plugin from `getSession().getPlugin (uuid)`, not from `editor->processor` (`:66`, `:71`). Thus EVE's ENAX size parameters hold the laid-out pane size before its editor exists.
6. `isInterestedInFileDrag (const juce::StringArray& files)`: returns `getSession().getPluginFormat (juce::File { files[0] }) != nullptr` when `files.size() == 1`.
7. `filesDropped (const juce::StringArray& files, int, int)`: `const jam::UUID pane { state.getProperty (Id::id) };` and `const juce::File plugin { files[0] };`, then `ENDActions::getInstance()->get (Id::loadPlugin, pane, plugin)` (include sibling `TabView.cpp:2`; uuid read sibling `EditorView.cpp:149`).
**Validation:** the editor is created only from the `Id::name` event, after layout and after the size parameters. No pane holds a plugin pointer. Each listener branch consumes the event payload (CODING.md).

### Step 6: Window state lane
**Scope:** `Source/Main.h`, `Source/Main.cpp`.
**Action:**
1. `Main.h`: add `juce::PropertiesFile settings { …getChildFile (files::settingsFileName), juce::PropertiesFile::Options {} };` in the shape of `EVEStandaloneApp.h:16`. The directory is `ConfigDirectory::Config::path` (Engineer reads `ConfigDirectory.h` for its type and uses its existing accessor). Declare it before `window` (`Main.h:61`).
2. `initialise`, after the window is built and before `setVisible (true)` (`Main.cpp:233-239`): when `settings.containsKey (Id::window.toString())`, call `window->restoreWindowStateFromString (settings.getValue (Id::window.toString()))`. ARCHITECT: disk always wins; a cold start keeps the config default (`ENDView.cpp:92-105`).
3. `shutdown`: `settings.setValue (Id::window.toString(), window->getWindowStateAsString());`. The destructor flushes (Decision 5).
**Validation:** no second settings file. One positive branch. No restated framework default.

### Step 7: Measure and runtime check
**Action:**
1. @Pathfinder runs `scout …/benchmark/pane.md`, then `…/split.md`. Each `baseline` is the current report.
2. ARCHITECT builds END Release and opens 4 splits. COUNSELOR reads `~/Desktop/EVE VST.ode` and `~/.config/end/end.ode`.
**Validation:**
- scout: `newPane` drops (the instance build leaves the action, Inventory: async). `panePrompt` and `sessionPlugin` are reported with their change and `probability`. A regression from the scan (Decision 4) is a measured row and goes into the parent amendment table.
- Runtime: one `EVE restart` per pane (log lines as in `EVE VST.ode`). ARCHITECT sees no `%` line and no OMP help above the first prompt. A second launch shows no window growth in `end.ode`.
- When the `%` or OMP help remains, parent Step 12 (trace) runs with its listed targets.

### Step 8: Fit spike — minimal JUCE patch and the fixed-size transform
**Scope:** `jam/patch/juce-plugin-view-scale-hook.patch` (new), `jam/user-modules-info.md` (one row after `:1370`), `Source/ui/EditorView.cpp` (`setEditorBounds`).
**Action:**
1. Patch, one hunk in `juce_NSViewComponent_mac.mm` `NSViewAttachment::componentMovedOrResized` (`:71-80`): after the frame check, inside the peer block, `[view setBoundsSize: NSMakeSize ((CGFloat) owner.getWidth(), (CGFloat) owner.getHeight())];`. The frame holds the transformed area (`:75-78`); the bounds hold the untransformed size. AppKit scales subviews by the frame/bounds ratio (Apple `setBoundsSize`, View Geometry; DTS thread 723857).
2. `user-modules-info.md`: one patch row in the shape of `:1366`, applied after `juce-plugin-window-focus-hook.patch`.
3. `setEditorBounds`, fixed-size branch (`EditorView.cpp:44-45`): replace `setTopLeftPosition` with `editor->setTransform (juce::RectanglePlacement {}.getTransformToFit (editor->getLocalBounds().toFloat(), getLocalBounds().reduced (inset).toFloat()));`. The resizable branch keeps `setBounds`.
Correction 2026-11-04 (ARCHITECT runtime: *"first pane terminal squeezed"*, *"second terminal didnt respect the EditorView padding"*): EVE's editor is not resizable from the host, so item 3 put it on the scale branch. ARCHITECT: *"size parameters is ENAX exclusive lane"*. `setEditorBounds` has three branches: resizable → `setBounds`; ENAX (the plugin has the `widthName` parameter, the same lookup `setSizeParameters` uses) → `setTopLeftPosition (inset, inset)`; otherwise → the fit transform. `widthName` moved to file scope (two users).
**Validation:** the patch is one line in one function. The END change is one call, no arithmetic. VST2, VST3 and CLAP place the plugin view inside JUCE's container (`juce_NSViewComponentWithParent.h:58-60`, `jam_ClapPluginWindow.h:101-102`). AU passes its own view (`juce_AudioUnitPluginFormat.mm:89`), and its frame watcher feeds the frame back (`juce_NSViewComponent_mac.mm:121`, `:168-169`); the gate checks it.

### Step 9: Fit gate (ARCHITECT, planned stop)
**Action:** ARCHITECT builds END Release through cast. ARCHITECT drops into a pane, one at a time: FabFilter Pro-Q 3 as `.vst3`, `.vst`, `.component`, `.clap` (`/Library/Audio/Plug-Ins/…`), and one fixed-size UAD plugin. For each, ARCHITECT reports: pixels scaled with aspect kept; mouse clicks land on the right control; no growth loop (AU). COUNSELOR records each result with its format. This is the planned stop of the approved plan shape. ARCHITECT's ruling on the results fixes the final patch shape, and COUNSELOR amends this step with it.

### Step 10: Audit
**Action:** @Auditor runs once over every file of Steps 1-8. COUNSELOR validates each finding (Design by Contract Filter). Each confirmed finding is resolved in this sprint.

### Step 11: Doxygen pass and documents
**Action:** @Pathfinder runs the doxygen pass on every new or changed member, with exact targets from COUNSELOR. The pass also fills the `settings file name` description cell, updates the `Session` and `EditorView` class text (the editor is no longer created in the constructor; the plugin file is no longer fixed), and records Step 0 in `ARCHITECTURE.md`. The sprint is logged on ARCHITECT's "log sprint".

## BLESSED Alignment

- **B:** Session (graph) owns the instance; the pane owns the editor. Close destroys the editor first (Decision 1). Replace releases the editor first (Decision 2). A late instance dies in its `unique_ptr`. No guard object, no `SafePointer`.
- **L:** each new function has one job and ≤ 30 lines. The fit is one call; the patch is one line.
- **E:** one action, typed arguments. The drop and the default are visible at their call sites.
- **S (SSOT):** the pane row's `Id::plugin` is the one truth for what a pane hosts. One load path. One settings file. One format lookup (`getPluginFormat`).
- **S (Stateless):** no flag. The callback reads the Model.
- **E (Encapsulation):** the pane tells through the action; Session reacts to the Model. The layer order holds (`ENDActions` depends only on `JuceHeader.h`, parent plan Step 6).
- **D:** the editor is built once per instance, after layout; the first winsize is the final one; the runtime log verifies it.

## Risks / Open Questions

- None open. Step 9 is the planned ARCHITECT gate that fixes the final patch shape from runtime evidence, the same shape as the parent's Step 10 amendment.
- Fact: no Apple source confirms bounds scaling for a layer-backed subview, and END's peer view is layer-backed (`juce_NSViewComponentPeer_mac.mm:222-224`, `juce_graphics.h:98-100`). Step 9 measures it.
