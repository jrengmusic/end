# SPRINT-LOG

---

## Sprint 106: Windows END Hosts EVE with Shared Vulkan — No Null Factory Call, No Release Hang, One Shell per Pane at Its Real Size ✅

**Date:** 2026-11-21
**Duration:** Not measured. Several sessions, joined by context compactions.
**Scope (ARCHITECT):**
- `/pay`, `vulkan` (`DEBT-20260912T150000`). The debt text was stale: *"the issue is EVE loaded at END with shared Vulkan"*; *"i have no issue with mac"*; *"one by one. fix the crash first"*.
- The Release hang: *"release build, hangs at the time terminal created"*. The `/GL` warning: *"fix it and probe the crash"*. PDB flags: *"just add it"*; *"if you need to read eve pdb just add flags"*.
- The leaked `%` before the prompt. Ruling: *"ViewEditor contract is with plugin_bootstrap. this is kernel modules. it will be synced to KANJUT. JFS will break"*, then *"ViewEditor ctor overload with explicit (width, height)"*.
- The double shell: *"double shell itself is wrong. why double?"*; *"no fake cwd"*, *"no fake size"*, *"must be deterministic"*; *"TextEditor write to Model isnt it?"*; *"so where is the absolute seam? that works as SSOT?"*, then `/go`.
- Binding rulings: *"MUST CHECK THOROUGHLY, NEVER GUESS AND SUGGEST MACRO GUARD AT ARBITRARY PLACE"*; *"NEVER FIX THE SYMPTOMS. fix the cause, fix the design, design by contract"*; *"be careful with those changes, i had no issues with mac, ensure it was windows only"*.
- `/audit doxygen pass then /log no-gate`. *"all DEBT paid"*.

### Agents Participated
- COUNSELOR: the probe specs and every cause read (VST2 host and client, JUCE graph and player, VirtualDevice, Model restore), every validation read, the audit triage, the SPEC, ARCHITECTURE, CLAUDE.md and `parameters.md` edits, this log.
- Pathfinder (haiku): whatdbg traces and samples of Release END and EVE, discovery reads, two doxygen passes.
- Engineer (haiku): the Vulkan, Nexus, ViewEditor, EVEView and EVE shell-seam edits, and the audit fixes.
- Auditor (opus) ×1: 3 code findings and 9 document findings.

### Files Modified (19 total)
**jam:**
- `jam_vulkan/engine/jam_VulkanGraphicsEngine.cpp:133`: `getOrCreateGraphics (juce::ComponentPeer&)` creates Graphics only for a component with non-empty local bounds, on every platform. The Windows `createContext` (`:430-497`) has no check of its own. `.h`: the `getOrCreateGraphics` and `createContext` blocks.
- `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp:23-31`: the context factory forwards to the host factory while it is set. When the host factory is null, it sets the process factory to null, repaints the peer's component and returns nullptr. The patch then gives the peer its default engine. `.h`: the constructor block.
- `jam_plugin_bootstrap/view/jam_ViewEditor.h`: a `create` overload and a private constructor with explicit `width` and `height`. The existing `create` and constructor are unchanged (kernel, synced to KANJUT).

**END:**
- `Source/plugins/Nexus.h/.cpp`: Nexus also derives privately from `juce::AsyncUpdater`. `run()` opens the audio devices on the Nexus thread, then triggers the update. `handleAsyncUpdate()` adds the player on the message thread.
- `project-info.md` `## release`: the Windows `/GL` cell is empty (clang-cl ignored it; LTO comes from `-flto=thin`). The probe rows `debugSymbols` (`/Zi`) and `linkDebugSymbols` (`/debug`) were added for the PDB traces, then removed. ARCHITECT: *"remove /debug and /zi for release as they were"*.
- `SPEC.md` (EVE's shell start, `cwd` fallback), `ARCHITECTURE.md` (Nexus line refs, the factory set and clear, EVE's factory forward), `CLAUDE.md` (Current State).

**EVE:**
- `project-info.md`: the probe rows `programDatabase` (Release, Windows, `/Zi` and `/debug`) were added for the PDB traces, then removed. Net: no change.
- `Source/EVEProcessor.h/.cpp`: the constructor writes no `cwd`. Only `Id::winsize` calls `terminalProcessor.prepare()`.
- `Source/EVETerminalProcessor.h/.cpp`: the `prepare()` gate is `winsize.isValid()` only. `releaseResources()` stops and closes the clock and keeps the TTY and `ttyResize`. RAII closes them, because they are declared after `device`.
- `Source/EVEView.h/.cpp`: `initialiseView` builds the view at `jam::ViewEditor::getViewBounds` of the terminal `width`, `height` and `zoom` rows, which is the formula of `resized()`.
- `Source/layout/parameters.md`: the `width` and `height` descriptions name their three writers. `SPEC.md`: the shell start.

### Alignment Check
- [x] BLESSED principles followed. D: the readiness of a peer is checked once, at the owner of the create. S: the first valid `winsize` in the Model is the one seam of the shell. The view size has one formula. E: the restore and the host tell; EVE does not ask a host lifecycle for its shell. B: the shell's TTY lives and dies with `EVETerminalProcessor`.
- [x] NAMES.md adhered. No new names except `viewBounds` (the `getViewBounds` family).
- [x] MANIFESTO.md principles applied.
- [ ] Not built after the shell seam, the audit fixes and the owner move. ARCHITECT ran the Release lane after the crash, hang and `%` fixes.

### Problems Solved
- **Null call at `0x0` (the debt's stack, `jam_ENAXClientExtensions.cpp:23`).** A 0×0 EVE peer reached the Windows `createContext`. The composition create failed, `setGpuAvailable (false)` cleared END's factory (`jam_VulkanGraphicsEngine.cpp:301-303`), and EVE's forward called it. The owner now skips an empty component. EVE's forward also honours a cleared host factory, so any other close of END's gate no longer crashes EVE.
- **Release hang at terminal creation.** `addAudioCallback` ran on the Nexus thread, and a whatdbg sample pinned the deadlock. It now runs on the message thread through `handleAsyncUpdate`, after the device opens on the Nexus thread. The ruling *"On a background thread"* (sound card) still holds.
- **The `%` before the prompt.** The first zsh started at EVE's config size, 97×37, which the kernel `ViewEditor` constructor gave the view, while the grid was 74 columns. PROMPT_SP padded 96 spaces and wrapped. EVE now builds the view through the explicit-size overload, at the Model's size. Verified in a Release run: first open at 74×26, 73 spaces, no wrap.
- **Two shells per pane.** The shell lived between the host's `prepareToPlay` and `releaseResources`. The player attached after EVE's first shell opened. Its `audioDeviceAboutToStart` released and prepared the graph (`juce_AudioProcessorPlayer.cpp:364-371`, `juce_AudioProcessorGraph.cpp:459-483`), and `tty.reset()` killed shell 1. The seam is now the first valid `winsize`, and a host release does not close the shell.
- **Fake `cwd` and its loop.** The constructor always wrote the config `cwd`, and `openTTY`'s `cwd` write re-triggered `prepare`. Both are gone. `openTTY` falls back to the config start directory when the Model has no local `cwd` (`EVETerminalProcessor.cpp:475-476`).
- **Audit.** Resolved: F1 (null host factory), F2 (empty bounds at the owner), F3 (one view-size formula), documents F4 to F8, F10 to F12. F9 (stale `DEBT-20260912T150000` text) resolved by the drain. F2 was first rejected against *"ensure it was windows only"*, then corrected after ARCHITECT's *"isnt single check whether peer size is ready crossplatform?"*.
- **Doxygen.** Two passes, each block read against the code. Pathfinder's "own factory" correction was accepted: `externalContextFactory` is one static pointer for the process (patch `:325`).
- **Mistakes owned.** A Pathfinder sent with the LSP; scope widened to the Editor debt; a race guessed and a Windows macro guard proposed; `project-info.md` edited by COUNSELOR; two wrong explanations of the `%`; removal of the kernel `setSize` proposed; F2 rejected on a wrong reading of a ruling.

### State for Continuation
- Build: `cast cast/spell.md --no-sign` (Release) or `cast cast/spell.md --debug` in END. The default lane signs and runs `gh.cmake`.
- Runtime checks on Windows: one `openTTY` per pane at load; a split keeps one shell per pane; a GPU gate close in END paints EVE with its default engine, with no crash.
- Runtime check on macOS: a 0×0 peer now gets the CPU context for that frame (`jam_VulkanGraphicsEngine.cpp:133`, `:425-426`).
- Not addressed (ARCHITECT's report, not ruled): a split resize erases a pane's active prompt until the next input. When a prompt is active and the grid size changes, `TerminalProcessor::reset` erases the screen (`jam_TerminalProcessor.cpp:37-40`). Each resize goes through `device.stop`, `device.open` and `prepare (winsize, history)` (`EVETerminalProcessor.cpp:370`, `:392`, `:536-572`).
- Restore order: `Model::setValuesFrom` sets one property at a time and notifies at once (`jam_Model.cpp:261-267`). `parameters.md` creates `winsize` (`:57`) before `cwd` (`:59`). So a restored pane can open its shell before its saved `cwd` arrives. Check it against END `SPEC.md` *"Own saved cwd"*.
- KANJUT `kuassa_ViewEditor.h:35` has no explicit-size `create`. The kernel sync is ARCHITECT's.
- Regenerate doxygen in jam, END and EVE, and read the warning counts.

### Debts Paid
- `DEBT-20260912T150000`: Windows END hosting EVE with shared Vulkan. The null factory call, the Release hang, the `%` and the double shell are fixed.
- `DEBT-20261103T195500`: ARCHITECT: *"all DEBT paid"*.
- `DEBT-20261025T135707`: ARCHITECT: *"all DEBT paid"*.

### Debts Deferred
- None

---

## Sprint 105: One Lane for All Host State, Listener-Only Attach, Native Plugin State and Session Restore ✅

**Date:** 2026-11-20
**Duration:** Not measured. Several sessions, joined by context compactions.
**Scope (ARCHITECT):**
- The IPC lane and its server lifecycle (`PLAN-end-ipc-server.md`, executed, then replaced). Rulings: *"ONE SHAPE, ONE PATH, ONE LANE, ONE SERIALISATION, ONE SSOT. the difference only the IPC server lifecycle either live and die with the app, or running on bg"*; *"END is always IPC server / Daemon is IPC server on background"*; *"cmd+q close immediately, the app, the whole app. for Daemon mode, it destroy the GUI, keeping process running bg"*.
- The runtime defect: *"winsize still always WRONG. resizing END's window never set winsize. winsize init already WRONG. DONT HACK, DONT PATCH. DESIGN BY CONTRACT"*.
- `PLAN-one-lane-native-state.md`, Part A and Part B (AskUserQuestion: *"Part A and Part B"*). Rulings: *"enax extension or IPC, CHOOSE ONE FOR ALL"* (IPC for all terminal state); *"AudioParameter lane IS NOT THE WAY"*; *"why use change broadcaster? NO"*; *"why dont we trust ValueTreeSynchroniser? ValuTree::Listener? APVTS::Listener?"*; *"why we cant simply call setStateInfo() and getStateInfo() as is?"*; restored pane start directory *"Own saved cwd"*; *"PANE SIZE is the sole author of EVEView"*.
- `/audit doxygen pass until /log no gate`.

### Agents Participated
- COUNSELOR: discovery synthesis, both PLANs, every validation read, the audit and doxygen triage, the SPEC, ARCHITECTURE, CLAUDE.md, DEBT and EVE SPEC edits, this log.
- Pathfinder (haiku): the inventory reads of the project and of JUCE (host creation order, IPC callbacks, synchroniser, VST2 state and window paths), and the doxygen pass with three corrections.
- Engineer (haiku): each code step, the split fix, the audit fixes, the `applyRule` rename, and the deletion of four completed or replaced PLAN files.
- Auditor (opus) ×1: one pass, 14 code findings and 8 document findings.

### Files Modified
**jam:**
- `patch/juce-value-tree-synchroniser-hook.patch`, `user-modules-info.md`: the synchroniser applies a delta and skips itself, for property and child events.
- `jam_data_structures/connection/jam_ValueTreeConnection.h/.cpp`: the client end. One `juce::InterprocessConnection` and one `juce::ValueTreeSynchroniser` over one tree. It is a `ValueTree::Listener`, so it follows a root that `replaceState` installs (`valueTreeRedirected`). No `std::function`.
- `jam_data_structures/connection/jam_ValueTreeServer.h/.cpp`: the server end. The nested `Connection` creates its synchroniser, then attaches the synced tree under the root: it replaces the held row with the same `Id::id`, or it appends a tree with no id. A connection ends only when its peer disconnects: `connectionLost` removes its tree, then calls `owner.removeConnection (*this)` (the JUCE pattern of `juce_ConnectedChildProcess.cpp:122`, `:135`).
- `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.h/.cpp`: the `cwd` rule and `serverConnection` are deleted. The `server` rule connects the plugin's own connection. The new `size` rule is empty under END. `set` is renamed `applyRule` (NAMES Rule 1: a callable parameter → `apply`).
- `cast/core.md`, `generated/jam_Core.h/.cpp`: `enaxServerPort` added, `enaxStartDirectory` deleted. `cast/plugin_bootstrap.md`, `generated/jam_PluginBootstrap.h/.cpp`: the `server` rule id.
- `jam_gui/layout/jam_OwnerComponent.h/.cpp`, `jam_PaneEdge.h` and the new `jam_PaneEdge.cpp`, `jam_MatrixComponent.h/.cpp`, `jam_gui.cpp`: adopt constructors and `addChildrenFromState`, so a window builds from existing rows. The `PaneEdge` bodies moved into their own `.cpp` (audit F12).

**END:**
- `Source/plugins/Nexus.h/.cpp`: the server serves the tree of `ENDModel`. `attachState`, `attachPluginState`, `onWindowRequested`, `getStartDirectory` and the start-directory extension are deleted. New: `getSessionsState`, `setSessionsState`.
- `Source/plugins/Session.h/.cpp`: one adopt constructor (`createSession` builds the row). `addPlugin` restores `state` through `setStateInformation` before the graph adds the plugin, and appends a held row with only the type and `Id::id` of the raw VST2 chunk. New: `loadPlugins`, `setPluginStates`. The `Id::focus` and `Id::cwd` events keep the last focused `cwd` on the sessions row.
- `Source/Main.h/.cpp`: in server mode the lock is taken before Nexus exists (audit F3). The launch request is a tree event (`valueTreeChildAdded` with an `end` child). Shutdown saves the sessions to `END.settings`, and `host` restores them before the window.
- `Source/ui/EditorView.h/.cpp`: the new-row constructor writes the sessions `cwd` on every new pane row (audit F2). `valueTreeChildAdded` writes on the attached terminal row: `cwd` (then removes it from the pane row), `focus`, then `width` and `height`. `resized` writes the size rows.
- `Source/ui/TabView.h/.cpp`, `SessionView.h/.cpp`, `ENDView.h/.cpp`: build from existing rows.
- `Source/config/display.md`, `cast/identifiers.md`, `Source/generated/Identifiers.h/.cpp` (`port`, `server`, `state`), `project-info.md` (`jam_subprocess`).
- `SPEC.md` §0, §1.0, §1.5, §1.7, §1.8, §3; `ARCHITECTURE.md`; `CLAUDE.md` (Open PLANs, line refs, the format-bound lane note); `DEBT.md` (`DEBT-20261103T195500` text).
- Deleted: `PLAN-end-ipc-server.md`, `PLAN-eve-state-in-end.md`, `PLAN-file-uri-one-flow.md`.

**EVE:**
- `Source/EVEProcessor.h/.cpp`: the member `connection`, declared after `model`, listens to `model.state`. The `server` rule connects it under END. The layout-size write is the fallback of the `size` rule, so under END EVE writes no size. The start-directory read is deleted, and EVE writes its own config `cwd`.
- `Source/EVETerminalProcessor.cpp`, `Source/EVEView.h/.cpp`: the `applyRule` call sites.
- `Source/layout/parameters.md`: the `cwd` row. `SPEC.md`: host-to-EVE, shell start, `markdownFile` in the host rows, the close row, size item 6.

### Alignment Check
- [x] BLESSED principles followed. S: EVE's tree is the one state of its pane, and END is the one author of its size. E: every reaction is a `ValueTree::Listener` event on its payload. B: the server owns each connection, and EVE's connection ends before its model.
- [x] NAMES.md adhered. `applyRule` joins the `apply…` family. `removeConnection` follows the JUCE owner-told connection.
- [x] MANIFESTO.md principles applied.
- [ ] Not built by COUNSELOR. ARCHITECT ran the lane before the split fix (*"no crash. terminal run and interactive"*), and the split defect led to the server correction. The split fix, the audit fixes and session restore are not run yet.

### Problems Solved
- **Wrong init `winsize`.** The server connection called `onAttach` before it created its synchroniser (`jam_ValueTreeConnection.cpp:56-58`, before this sprint), so no attach write reached EVE. EVE also wrote its own layout size (`EVEProcessor.cpp:28-33`, before this sprint). The synchroniser now exists before the attach, and under END EVE writes no size, so its first valid `winsize` comes from END's size.
- **Two lanes.** `cwd` used ENAX and the other rows used IPC. All terminal state now uses IPC. ENAX keeps the peer hooks and the server port.
- **Manual callbacks.** `onAttach`, `onLost` and `onWindowRequested` are replaced by listener events and the JUCE owner-told connection.
- **A split lost the first pane's lane.** `split` moves the pane row (`jam_MatrixComponent.cpp:72`, `:341-342`). The first server design dropped a connection whose tree left the root. A connection now ends only when its peer disconnects.
- **Native state.** END keeps each plugin's `getStateInformation` blob only in `END.settings`, and it gives the blob back through `setStateInformation` before the graph adds the plugin. `replaceState` swaps EVE's root, and the connection follows it through `valueTreeRedirected`.
- **Corrections owned.** The claims that EVE's state pair could be one line each, and that `AudioParameter` or `ChangeBroadcaster` belonged in the lane, were wrong and were withdrawn.
- **Audit.** Resolved: F1, F2, F3, F5, F6, F8, F9, F10, F11, F12; documents F4, F14, D1 to D8 and the PLAN drifts. Rejected with a read: F7 (a jam parameter reaches its tree only at the flush, `EVETerminalProcessor.cpp:24`, `:54-59`) and F13 (no read proves the listener unused).
- **Doxygen pass.** Pathfinder's F1 was rejected: `jam::AudioModel` derives from APVTS (`jam_AudioModel.h:25`), whose `replaceState` assigns a new root (`juce_AudioProcessorValueTreeState.cpp:400`). Its F4 led to the `applyRule` rename. Its F2, F3 and F5 were rejected (`MANIFESTO.md:113-126`; named threat in `split`; in-process plugin).

### State for Continuation
- Run `cast` on jam (`core.md`, `user-modules-info.md` for `jam_PaneEdge.cpp`) and `cast cast/spell.md` in END (`identifiers.md`: `state`). Then build the EVE pack and END. The generated files were also edited by hand to match.
- Runtime checks: a new pane opens at the pane size in the last focused `cwd`; a window resize and a split resize every EVE pane (`jam_ValueTreeServer.cpp:9-14`); quit and relaunch restore the sessions, each EVE pane in its own saved `cwd` (`Nexus::setSessionsState`, `Session::addPlugin`).
- Server mode: a second launch attaches to the running server (`Main.cpp` `host`, `launch`).
- The lane identity is VST2-only (`Session::addPlugin`, `CLAUDE.md` ENAX Plugin Format). VST3 or AU needs an identity read.
- Regenerate doxygen (`ninja doxygen`) in jam, END and EVE, and read the warning counts.
- `PLAN-one-lane-native-state.md` stays until ARCHITECT confirms the runtime.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 104: EVE Loads in END Again — One Host Lane, No Hand-Rolled State Seam, No AppleDouble in cast Zips ✅

**Date:** 2026-11-20
**Duration:** Not measured. Two sessions, joined by one context compaction.
**Scope:**
- cast: ARCHITECT: *"`__MACOSX/<parent>/._<name>`, << this should not be written at all."*
- END Step 6 of `eve/PLAN-eve-plugin-contract.md`: `PluginState` deleted. ARCHITECT: *"no manual handroll setState/getState"*.
- `PLAN-eve-host-lane.md` Steps 1-8. ARCHITECT: *"ONE LANE, ONE PATH, ONE SERIALISATION, SSOT"*.

### Agents Participated
- COUNSELOR: discovery synthesis, the PLAN, the read of each return, two plan corrections, SPEC and ARCHITECTURE edits, this log.
- Pathfinder (haiku): JUCE IPC, ValueTreeSynchroniser, message queue, per-format host lanes, the endless nexus precedent, the Framework Inventory, the doxygen pass.
- Engineer (haiku): each code step and each audit fix.
- Auditor (opus): one sprint audit, 10 findings, all validated and fixed.

### Files Modified
- cast `Source/ZipWriter.h`, `.cpp`: the AppleDouble entries are removed. The macOS zip is written from the items. The CRC constants are named through `std::numeric_limits`.
- cast `Source/Pack.h`, `.cpp`: `toZip` has one path. The single-use `toStage` template is folded into `toDmg`.
- cast `Source/IconWriter.h`, `.cpp`: `applyAttribute` is renamed `setAttribute` (Verb Contract). `nameListOffset` derives from `mapLength`.
- cast `Source/AppleDoubleWriter.h`, `.cpp`: deleted. cast `SPEC.md`, `Source/HELP.md`: the zip and icon sections.
- END `Source/plugins/PluginState.h`, `.cpp`: deleted.
- jam `jam_data_structures/connection/jam_ValueTreeConnection.h`, `.cpp` (new): a `juce::InterprocessConnection` and a `juce::ValueTreeSynchroniser` over one named pipe per pane. The pipe name is the pane UUID. `setProperty` sends one message per tell. `paneIndex` is the `effVendorSpecific` index.
- jam `user-modules-info.md:308`, `:314`: the submodule rows. cast writes the include lines from them.
- jam `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp`: the `Id::cwd` rule. Under END it has no effect.
- EVE `Source/EVEProcessor.h`, `.cpp`:
  - It is a `juce::VST2ClientExtensions`. `handleVstManufacturerSpecific` connects, then sends the full state.
  - The ENAX `cwd` fallback writes the default start directory outside END.
  - `Id::cwd` calls `prepare()`.
- EVE `Source/EVETerminalProcessor.cpp:372`: the shell opens when the winsize is valid and `cwd` is not empty.
- END `Source/plugins/Session.h`, `.cpp`:
  - There is one connection per pane. The VST2 pairing gives the pane UUID.
  - `cwd` (home when empty) and `focus` are told at pairing.
  - The `Id::focus` event sends tells. The `Id::plugin` event removes the old replica holder.
  - `setProperty (pane, property, value)` sends one tell.
- END `Source/ui/EditorView.h`, `.cpp`: the handlers act on the pane row only. `setMode` tells `mode`.
- END `Source/ui/ENDView.h`, `.cpp`: `getFocusedDirectory` reads the replica. `addPane (edge, directory)`. `registerModeActions`.
- END `Source/ui/TabView.h`, `.cpp`: `addPane (edge, directory)` writes the pane `cwd`.
- END `Source/ui/SessionView.cpp`: the name branch acts on tab and pane rows only.
- END `cast/bimaps.md`, `Source/config/keys.md`: `normal` and `open` restored. The zoom rows are removed.
- END `cast/identifiers.md`, `Source/config/display.md`: `pluginState`, `exclusiveZoom` and `zoomStep` are removed.
- END `SPEC.md` (§1.5 host lane, focus, §1.7, host tells), `ARCHITECTURE.md` (PANE row, host automation), EVE `SPEC.md`, EVE `Source/layout/parameters.md` (`cwd`).
- Doxygen pass: the new jam class, ENAX, `EVEProcessor.h`, `EVETerminalProcessor.h`, `Session.h`, `Nexus.h`, `EditorView.h`, `ENDView.h`, `TabView.h`, `SessionView.h`, cast `Pack.h`.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **EVE failed to load in END.** The root cause: END wrote a partial `pluginState` tree through `setChunkData`. EVE took it as its full state, so `historyCapacity` read 0 and `jassert (newCapacity > 0)` fired at `jam_TerminalSwap.cpp:29`. The hand-rolled seam is deleted.
- **One lane replaces it.** There is one pipe per pane and one serialisation, `ValueTreeSynchroniser`.
  - EVE publishes its whole `model.state`. END holds the replica and never writes it.
  - END tells EVE's terminal row `cwd`, `focus` and `mode`.
- **Start-directory order.** The editor opens synchronously, and a told `cwd` arrives as a posted IPC delivery. So EVE joins on its Model: it opens the shell when the winsize is valid and `cwd` is not empty.
- **Plan corrections:**
  - The home default moved to the one tell site, because the drop path writes no `cwd`.
  - New-tab inheritance was restored, per SPEC §1.7.
- **Audit findings fixed:**
  - the module rows;
  - focus from the pane row's `Id::focus`, because a split pane sits under an edge row;
  - the stale replica on a plugin reload;
  - the pairing reply as a bool;
  - four cast CONTRACT findings;
  - the EVE SPEC callers of `prepare()`.

### State for Continuation
- Not built. Run in order:
  1. `cast` on jam, to write `connection/jam_ValueTreeConnection.h`/`.cpp` into the `jam_data_structures` aggregators from `user-modules-info.md:308`, `:314`.
  2. `cast cast/spell.md` in END, for `normal` and `open` in `Source/generated/*`.
  3. The EVE pack, then END.
- Check `Session.cpp:2` (global `<pluginterfaces/vst2.x/aeffectx.h>`) on macOS and on Windows clang-cl.
- Check `jam_ValueTreeConnection.h:75` (`makeInt` constexpr).
- A user `~/.config/end/keys.md` or `display.md` that holds a removed row (zoom, `exclusiveZoom`, `zoomStep`) fails validation, and `ConfigDirectory` drops the whole file.
- Each `cwd` report from the shell runs `prepare()`, which restarts the clock (`EVETerminalProcessor.cpp:370-398`). No compliant change was found.
- Decided after the audit:
  - The close-key requirement is dropped from `end/SPEC.md` Keys. A close key closes the pane.
  - Under END, a pane with no inherited `cwd` starts in the home directory (`Session.cpp:140-141`). EVE's `startDirectory` setting applies outside END.
- `~/.config/dev/plugins/__MACOSX` from earlier packs needs a manual delete.

### Debts Paid
- None

### Debts Deferred
- None

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

