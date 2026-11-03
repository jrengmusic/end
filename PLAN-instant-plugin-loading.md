# PLAN: Instant Plugin Loading (EVE)

**RFC:** RFC-instant-plugin-loading.md
**Date:** 2026-11-03
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM (LANGUAGE.md C++/JUCE: one object, one translation unit; 30/3 unchanged)
**Status:** locked 2026-11-03 (ARCHITECT approval).

## Overview

A new pane takes 2-3 s from creation to a running terminal. During that time the message thread is blocked (RFC lines 7-9). This plan does four things. It instruments END and EVE for `scout`. It measures a baseline and the Where lane. It amends this PLAN with the measured bottleneck table, and ARCHITECT approves the amendment. Then it removes each bottleneck and fixes the `%` and OMP-help bugs (RFC lines 14-21). The target is less than 500 ms from pane creation to the first painted prompt (RFC lines 36-37).

## ARCHITECT rulings (this session)

- Endpoint: *"First prompt painted"*.
- EVE zones: *"END zones + one signal (Recommended)"*. END owns the one profiler. END zones bracket each synchronous stage. One span zone runs from pane creation to the signal. EVE's profile build sends the signal through one new ENAX id. The Where lane splits EVE internals and the shell children. The jam Profiler API does not change.
- Plan shape: *"Measure, then amend (Recommended)"*.
- Prompt mark: *"jam handles mark A"*.

## Language / Framework Constraints

- LANGUAGE.md C++/JUCE: each new body goes into its object's one `.cpp`. No new `.cpp` file.
- CAROL.md Code Hygiene: no comment and no doxygen before the audit. The cast `description` cells and the doc comments of new members are written in the post-audit doxygen pass (Step 14).
- CAROL.md Build and MEMORY `feedback_no_build`: agents run no build. The profile skill grants scout builds (*"profiling with scout is granted with build permission"*). Every build in Steps 8-9 is a `## toolchain` row of a scout manifest. The Debug build for the trace (Step 12) is ARCHITECT's.

## Dependency & API Inventory

**Facts that the reads established (path and blocking points).**

- Pane creation is synchronous on the message thread. `ENDView` `newPane` action (`ENDView.cpp:164-178`) → `TabView::createChild` (`TabView.cpp:22-30`) → `jam::Model::Component` appends the PANE row (`jam_Model.h:158`) → `Session::valueTreeChildAdded` → `Session::addPlugin` (`Session.cpp:31-87`). This all runs inside the `EditorView` base constructor. Then `EditorView::createProcessorEditor` runs (`EditorView.cpp:146-166`).
- The format is VST2: `files::evePlugin` is `EVE.vst` (`cast/files.md:31`). `VSTPluginFormatHeadless::fileMightContainThisPluginType` accepts it (`juce_VSTPluginFormatHeadless.cpp:115`).
- `AudioPluginFormat::createInstanceFromDescription` blocks on `finishedSignal.wait()` (`juce_AudioPluginFormat.cpp:76`).
- `graph.addNode` and each `graph.addConnection` use `UpdateKind::sync` by default (`juce_AudioProcessorGraph.h:283`, `:324`). Each one rebuilds on the message thread (`juce_AudioProcessorGraph.cpp:1958-1971`). `applySettings` locks a mutex and calls `prepareToPlay` (`:435`, `:483`). `UpdateKind::async` and `rebuild()` exist (`juce_AudioProcessorGraph.h:239-246`, `:352`).
- EVE builds its settings twice: `EVEProcessor.cpp:20` and `EVEView.cpp:10`. Each call reads `eve.md` and can rewrite it (`ConfigDocument.cpp:405-417`). Each call runs the CSS cascade (`jam_PluginEditorLayout.cpp:589-607`).
- `ShellIntegration::addShellScripts` writes four files on every `openTTY` (`EVETerminalProcessor.cpp:255-256`, `ShellIntegration.cpp:19-33`). `tty->open` returns at once when the shell exists (`jam_TerminalTTY_posix.cpp:248-249`).
- The first valid winsize comes from the editor (`EVEView.cpp:13` → `jam_TextEditor.cpp:1037-1055`). The winsize attachment calls `restart()` on the message thread (`EVETerminalProcessor.cpp:21-22`). That chain reaches `openTTY` and `forkpty` (`EVETerminalProcessor.cpp:276`, `jam_TerminalTTY_posix.cpp:260`). `device.stop()` blocks in `dispatch_sync_f` (`VirtualDevice.cpp:49-52`, `AsyncUpdater.cpp:73-79`).
- `TerminalProcessor::reset` moves the cursor to the origin on each restart (`jam_TerminalProcessor.cpp:33`). Each winsize change runs this reset (`EVETerminalProcessor.cpp:272`).
- On macOS the shell starts through `/usr/bin/login`, then `bash`, then `exec -l <shell>` (`jam_TerminalTTY_posix.cpp:113-126`).
- jam ignores OSC 133 A. `shellIntegrationFunctions` maps only C and D (`jam/cast/terminal.md:1323-1329`). A runs the no-op handler (`jam_TerminalProcessor.cpp:847`).

**API surface each step commits to use.**

- jam_profiler: `jam::Profiler` (`prepare`, `drain`, `writeSamples`) and `jam::Profiler::Zone` (`jam_Profiler.h:17-108`). Each zone sits under `#if JAM_USING_PROFILER`, as at `EVETerminalProcessor.cpp:28-30`.
- ENAX: a host static callback registered in `Nexus::extensions` (`Nexus.cpp:6-34`). A plugin rule registered in `jam::ENAXClientExtensions` (`jam_ENAXClientExtensions.cpp:41-51`), called through `set` (`jam_ENAXClientExtensions.h:36-43`).
- Model: `jam::Model::ParameterAttachment` (`EVETerminalProcessor.cpp:21-22`, `jam_Model.h:267-268`). Processor events go to the terminal row (`EVETerminalProcessor.cpp:96-97`).
- JUCE: `juce::AudioProcessor::getActiveEditor` and `juce::ComponentPeer::performAnyPendingRepaintsNow` (`juce_ComponentPeer.h:430`: *"cause the immediate redrawing of any areas of this window that need repainting"*).
- END actions: `ENDActions::run` (`ENDView.cpp:145`). `jam::OwnerComponent::getChildCount` (`jam_OwnerComponent.h:88-89`). The `quit` action (`ENDView.cpp:210-214`).
- Manifest read: `jam::ConfigDocument::parse (…).getValueTree (Id::manifest)` and `getChildWithName (Id::toType (Id::profiler))`, as at `EVETerminalProcessor.cpp:240-242`. The manifest path is argv[1] (`EVETerminalProcessor.cpp:48-53`, `juce_ApplicationBase.cpp:225-233`).
- Build: the EVE build packs `EVE v<version> macOS.zip` into `../end/Source/plugins` (`eve/project-info.md:183-192`, `eve/cast/cmake.cast:498-521`). END hashes the zips (`cast/cmake.cast:299-306`). `Nexus::offloadPlugins` unpacks them when the hash changes (`Nexus.cpp:128-156`).

**Established patterns (siblings pasted into each Engineer prompt).**

- Profile module row: `| @user-module | jam_profiler | Profiler zones — per-thread rings drained to durations per zone |` (`eve/project-info.md:292-293`).
- Profile define row: `` | jamUsingProfiler | `$<$<BOOL:${CAST_PROFILE}>:JAM_USING_PROFILER=1>` | Profiler zones, profile builds only | `` (`eve/project-info.md:395`).
- Profiler owner: `EVEProcessor.cpp:16-18` (`prepare`), `:26-38` (`drain`, `writeSamples`), `EVEProcessor.h:118-120` (member).
- ENAX id row: `` | @string | enaxPeerDeleted | `enax.peer-deleted` | ENAX id of the host peer-deleted callback. | `` (`jam/cast/core.md:78`).
- Host callback: `displayBoundsGetter` (`Nexus.cpp:26`, `:34`). Plugin rule: the `Id::size` rule (`jam_ENAXClientExtensions.cpp:41-51`).
- Processor count: the `outputBlock` field (`jam_TerminalProcessor.h:127`) and the `imageCount` field (`:134`). EVE row `commandCount` (`eve/Source/layout/parameters.md:47`).
- Handler table: `ShellIntegrationFunction` (`jam/cast/terminal.md:1058-1064`). Lookup `shellIntegrationFunctions` (`:1323-1329`). Handlers `shellIntegrationConstruction` (`jam_TerminalProcessor.cpp:845-859`). Events `applyEvents` (`:260-270`).

**New names (NAMES.md Rule -1, ratified by ARCHITECT's approval of this plan).**

| Name | Kind | Where | Family |
|---|---|---|---|
| `promptStart` | `ShellIntegrationFunction` row | `jam/cast/terminal.md` | existing `map::ShellIntegration::promptStart` (`:906`) |
| `promptCount` | `TerminalProcessor` field, jam `Id`, EVE model row | jam, EVE | `imageCount`, `commandCount` |
| `promptPainted` | ENAX rule `Id`, END action `Id` | jam `cast/core.md` | rules `engine`, `keys`, `size` |
| `enaxPromptPainted` (`enax.prompt-painted`) | ENAX id | `jam/cast/core.md` | `enaxPeerDeleted`, `enaxKeyPressed` |
| `promptAttachment` | `EVETerminalProcessor` member | EVE | `winsizeAttachment` |
| `promptPaintedCallback` | static host callback | `Nexus.cpp` | `keyPressedCallback`, `mouseEventCallback` |
| `getManifestFile` | `Nexus` static | END | `EVETerminalProcessor::getManifestFile` |
| `profiler` | `ENDApplication` member | END | `EVEProcessor::profiler` |
| `panePrompt` | zone `Id` and `ENDView` member | END | zone keys `terminalProcess`, `swapLiveLines` |
| `sessionPlugin`, `graphNode`, `paneEditor` | zone `Id` | END | same |
| `profiler`, `panes` | manifest table and row `Id` | END | EVE `profiler`, `shell`, `args` |
| `pane.md`, `split.md`, `pane-where.md` | manifests | `END/benchmark/` | `eve/benchmark/live.md`, `live-where.md` |

The zone of the `newPane` action uses the existing `Id::newPane` (`cast/identifiers.md:96`).

## Validation Gate

COUNSELOR validates each step before the next one. The check covers MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, for the whole sprint. It never runs per step.

## Steps

### Phase A — Instrument (jam, EVE, END)

### Step 1: jam handles OSC 133 A
**Scope:** `jam/cast/terminal.md`, `jam/jam_terminal/processor/jam_TerminalProcessor.h`, `jam_TerminalProcessor.cpp`.
**Action:**
1. `ShellIntegrationFunction` table (`terminal.md:1058-1064`): append the row `| promptStart | 3 |`.
2. `shellIntegrationFunctions` table (`:1323-1329`): append `| map::ShellIntegration::promptStart | map::ShellIntegrationFunction::promptStart |`.
3. jam terminal identifiers (`terminal.md`, the table that holds `shellExited`, `:56`): add `| @id | promptCount | `promptCount` | |` in alphabetical order.
4. `jam_TerminalProcessor.h`: add `int promptCount { 0 };` after `outputBlock` (`:127`).
5. `jam_TerminalProcessor.cpp:845`: change the array bound to `map::ShellIntegrationFunction::promptStart + 1`. Append the handler `[] (TerminalProcessor& processor, TerminalBuffer&) { ++processor.promptCount; }`.
6. `applyEvents` (`:260-270`): add `events.get (Id::promptCount, int { promptCount });` after the `Id::cursor` line.
7. Before the edit, run LSP `findReferences` on `TerminalProcessor::process`. Report every events map that receives `applyEvents`, and the `jam::Function::Map::get` contract for a missing key.
**Validation:** COUNSELOR reads the four edits. The handler order matches the ordinal order. `reset` (`:18-35`) does not clear `promptCount`, the same as `outputBlock`. Every events map from item 7 is inside END, EVE or jam. A map outside them is a scope change, and COUNSELOR stops on it (CAROL.md Scope).

### Step 2: ENAX `promptPainted` channel (jam)
**Scope:** `jam/cast/core.md`, `jam/jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp`, and the jam identifier table that holds `engine`, `keys`, `size`.
**Action:**
1. `core.md`: add `` | @string | enaxPromptPainted | `enax.prompt-painted` | | `` after `:78`.
2. Add the `promptPainted` `@id` row to the table that holds the `engine`, `keys` and `size` rule ids. Engineer finds that table first and reports its file:line.
3. Constructor (`jam_ENAXClientExtensions.cpp:5-52`), inside `if (PluginHostType().isEND())`, under `#if JAM_USING_PROFILER`: add a `rules.add (Id::promptPainted, [this] { … })` rule. It mirrors the `Id::size` rule (`:41-51`). It holds `static const auto* hostPromptPainted { static_cast<void (* const*)()> (getExtension (Id::enaxPromptPainted)) };`, then `jassert (hostPromptPainted != nullptr);`, then `(*hostPromptPainted)();`.
**Validation:** the rule shape equals the `Id::size` rule. The rule exists only under END and only in a profile build. Without END the caller's fallback runs (`jam_ENAXClientExtensions.h:36-43`).

### Step 3: EVE sends `promptPainted` at the first painted prompt
**Scope:** `eve/Source/layout/parameters.md`, `eve/Source/EVETerminalProcessor.h`, `EVETerminalProcessor.cpp`.
**Action:**
1. `parameters.md` `## model`: add the row `| promptCount | int | | 0 | |` after `commandCount` (`:47`).
2. `registerEvents` (`EVETerminalProcessor.cpp:96`): add `Id::promptCount` to the `int` list `{ Id::mouseTracking, Id::cursor }`.
3. `EVETerminalProcessor.h`: under `#if JAM_USING_PROFILER`, add `std::unique_ptr<jam::Model::ParameterAttachment> promptAttachment;` after `winsizeAttachment` (`:217-218`).
4. Constructor (`:21-22` pattern), under `#if JAM_USING_PROFILER`: build `promptAttachment` on the `promptCount` parameter of `state`. When the value is 1, the callback does these steps in order:
   1. It takes `processor.getActiveEditor()`.
   2. It calls `repaint()` on that editor.
   3. It calls `getPeer()->performAnyPendingRepaintsNow()`.
   4. It calls `dynamic_cast<const jam::ENAXClientExtensions*> (&processor)->set (Id::promptPainted, [] {})`. This is the cast pattern at `PluginEditor.cpp:310-314`.
**Validation:** the callback runs on the message thread (`jam_Model.h:267-268`). It fires once for each instance, at the 0 → 1 change. It holds no flag. The audio and device path does not change: the count reaches the Model through the existing lock-free `storeValue` (`:97`). Resolution: the paint shows the swap lines that the last vblank pull took (`jam_TextEditor.cpp:69`). Thus the endpoint has a resolution of one vblank period.

### Step 4: END profile build wiring
**Scope:** `end/project-info.md`, `end/cast/identifiers.md`.
**Action:**
1. `## user module` (`project-info.md:279-313`): add the `jam_profiler` row verbatim from `eve/project-info.md:292-293`.
2. `## define` (`:344-390`): add the `jamUsingProfiler` row verbatim from `eve/project-info.md:395`.
3. `cast/identifiers.md`: add `@id` rows `graphNode`, `paneEditor`, `panePrompt`, `panes`, `profiler`, `sessionPlugin` in alphabetical order (row shape `:96`).
**Validation:** each row is a byte copy of its sibling. The order is alphabetical. No other table changes.

### Step 5: END owns the profiler and writes the samples
**Scope:** `end/Source/Main.h`, `Main.cpp`, `Source/plugins/Nexus.h`, `Nexus.cpp`.
**Action:**
1. `Nexus`: add `static juce::File getManifestFile();` under `#if JAM_USING_PROFILER`. Its body is a copy of `EVETerminalProcessor.cpp:48-53`.
2. `Main.h`: add `jam::Profiler profiler;` after `Generated generated;` (`:43`), under `#if JAM_USING_PROFILER`.
3. `ENDApplication::initialise` (`Main.cpp:209`): add `profiler.prepare (jam::Profiler::defaultCapacity);` as the first statement, under `#if JAM_USING_PROFILER`.
4. `ENDApplication::shutdown` (`Main.cpp:201`): add `profiler.drain();` and `profiler.writeSamples (Nexus::getManifestFile(), Id::toType (juce::Identifier { ProjectInfo::projectName }));` under `#if JAM_USING_PROFILER`. This mirrors `EVEProcessor.cpp:33-35`.
**Validation:** the profiler is declared before `nexus`, so every zone ends before the profiler is destroyed. A non-profile build compiles to the same code as today.

### Step 6: END zones and the ENAX host callback
**Scope:** `Source/plugins/Nexus.cpp`, `Source/plugins/Session.cpp`, `Source/ui/EditorView.cpp`.
**Action:**
1. `Nexus.cpp`: add `#include "action/ENDActions.h"` after `:1`. Under `#if JAM_USING_PROFILER`, add `static void (*promptPaintedCallback)() { [] { ENDActions::getInstance()->run (Id::promptPainted); } };` after `:26`. Add `extensions.try_emplace (Id::enaxPromptPainted, &promptPaintedCallback);` after `:34`. `ENDActions` depends only on `JuceHeader.h` (`ENDActions.h:6`), so Nexus includes no higher layer.
2. `Session::addPlugin` (`Session.cpp:39`): the first statement is `const jam::Profiler::Zone zone { Id::sessionPlugin };`.
3. In the `if (instance != nullptr)` block (`Session.cpp:60`), the first statement is `const jam::Profiler::Zone zone { Id::graphNode };`.
4. `EditorView::createProcessorEditor`: in the `if (session.contains (uuid))` block (`EditorView.cpp:159`), the first statement is `const jam::Profiler::Zone zone { Id::paneEditor };`.
5. Each zone sits under `#if JAM_USING_PROFILER`, in the shape of `EVETerminalProcessor.cpp:28-30`.
**Validation:** each zone is at the top of an existing scope. No code moves. The callback shape equals `displayBoundsGetter` (`Nexus.cpp:26`). `ENDActions::run` returns `bool` (`ENDActions.h:50`). The callback ignores it, the same as `TabView.cpp:236-239`.

### Step 7: END profile workload (span and epoch end)
**Scope:** `Source/ui/ENDView.h`, `ENDView.cpp`.
**Action:**
1. `ENDView.h`: under `#if JAM_USING_PROFILER`, add `std::optional<jam::Profiler::Zone> panePrompt;`.
2. `newPane` action (`ENDView.cpp:166`): the first statements are `const jam::Profiler::Zone zone { Id::newPane };` and `panePrompt.emplace (Id::panePrompt);`, under `#if JAM_USING_PROFILER`.
3. `registerTabActions`, under `#if JAM_USING_PROFILER`: add the action `Id::promptPainted`. The capture `panes` is read once at registration from `Nexus::getManifestFile()`: the `profiler` table, row `panes` (read shape `EVETerminalProcessor.cpp:240-242`). The body does these steps in order:
   1. It calls `panePrompt.reset();`.
   2. When the active tab view's `getChildCount()` is less than `panes`, it runs `actions.run (Id::splitVertical)`.
   3. Otherwise it runs `actions.run (Id::quit)`.
**Validation:** the span starts at the action. It ends at the signal of the new pane, because each instance signals once (Step 3). The epoch ends through the existing `quit` → `shutdown` path (Step 5). The `panePrompt` zone is the RFC number. The `newPane` zone is the message-thread block.

### Step 8: Manifests
**Scope:** `end/benchmark/pane.md`, `split.md`, `pane-where.md` (new files, cast grid tables, same format as `eve/benchmark/live.md`).
**Action:** COUNSELOR writes the three manifests.
- `## run`: `epochs` 11 (`pane.md`, `split.md`) and 5 (`pane-where.md`). `command` is `../Builds/Profile/END_artefacts/Release/END.app/Contents/MacOS/END`. `samples` is `<name>-samples.md`. `output` is `<name>-report.md`. `baseline` is empty.
- `## profiler`: `panes | int | 1` (`pane.md`, `pane-where.md`) and `panes | int | 2` (`split.md`).
- `## sample` (`pane-where.md` only): `intervalMs | int | 1`, `stackDepth | int | 128`.
- `## toolchain` (`pane.md`, `split.md` only; run from the END root):
  1. `cast | ../jam/cast/spell.md`
  2. `cmake | -S ../eve -B ../eve/Builds/Profile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCAST_SIGN=OFF -DCAST_PROFILE=ON`
  3. `ninja | -C ../eve/Builds/Profile`
  4. `cmake | -S . -B Builds/Profile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCAST_SIGN=OFF -DCAST_PROFILE=ON`
  5. `ninja | -C Builds/Profile`
- `pane-where.md` has no `## toolchain`. It uses the binary that `pane.md` builds.

Row 3 packs the profile EVE into `Source/plugins/EVE v0.1.0 macOS.zip`. Row 5 embeds it. The first epoch offloads it (`Nexus.cpp:128-156`).

**Correction (2026-11-03).** The first manifests also held `cast | cast/spell.md`. Every form of that command runs END toolchain rows (cast `HELP.md:541`; with `--no-format` too, as observed in the scout stderr). Under `sudo -n` it left 63 root-owned entries in `eve/Builds/Release` and a root-owned `end/RELEASE.md`. ARCHITECT ruling: *"You regenerate first"*. ARCHITECT runs the normal END cast flow before Step 9, and the manifests hold no END cast row.
**Validation:** each table parses with the scout grammar (profile skill, Manifest). Each path in `## run` is relative to the manifest. Each toolchain path is relative to the END root.

### Phase B — Baseline

### Step 9: Baseline and Where lane
**Action:** COUNSELOR delegates to @Pathfinder, one run at a time, from the END root:
1. `scout /Users/jreng/Documents/Poems/dev/end/benchmark/pane.md`
2. `scout …/split.md`
3. `scout …/pane-where.md`

ARCHITECT ruling: *"build permission granted. you don't need sudo to profile with scout"*. No run uses `sudo`.

**Corrections found by the Step 9 runs (2026-11-03).**
1. jam toolchain row 1 regenerated `jam_core.cpp` without `#include "utils/jam_Id.cpp"`, so `Id::toTag` did not link in EVE. Cause: `jam/user-modules-info.md` had the `utils/jam_Id.h` row and no `utils/jam_Id.cpp` row. Fix: the row `utils/jam_Id.cpp | kernel | jam_core.cpp` at `user-modules-info.md:259`, at the HEAD include position.
2. The `promptPainted` `@id` row was in `jam/cast/plugin_bootstrap.md`. END builds only `jam_core` and `jam_gui` (`CMakeLists.txt:208`, `:211`), so END had no `Id::promptPainted`. The New names table puts it in `jam/cast/core.md`. Fix: the row moved to `core.md:124`, in the table that holds `size` (`:133`).
3. END segfaulted in EVE's `engine` rule. `getExtension` called a null `enax_get_extension` (`jam_ENAXClientExtensions.cpp:79`). `nm -gU`: the Debug END has `T _enax_get_extension`; the Release-flag END does not. `ld(1)` `-export_dynamic`: *"Preserves all global symbols in main executables during LTO. Without this option, Link Time Optimization is allowed to inline and remove global functions. This option is used when a main executable may load a plug-in which requires certain symbols from the main executable."* Fix: the row `exportDynamic | -Wl,-export_dynamic | | linker` in END `## release` (`project-info.md:226`). `-exported_symbol _enax_get_extension` is rejected: it states the symbol name a second time beside `Main.cpp:25` (MANIFESTO **S**). ARCHITECT ruling: *"you always build using cast"*. COUNSELOR runs `cast cast/spell.md --debug` before the next run. It regenerates END and builds the Debug END that Step 12 uses.
4. The `pane.md` epoch did not end. scout ran for 58 min, and no report was written. Direct runs with zsh and with bash both painted a prompt, and END did not quit in 15 s or 30 s. Cause: the Step 3 `promptAttachment` never fires. `storeValue` stores the value *"without notifying its listeners"* (`jam_Model.h:994`). `flush()` writes the tree under `ignoreCallbacks` (`jam_Model.cpp:45-46`), so the Parameter listeners do not fire. The flush runs once per vblank from the editor getter (`EVETerminalProcessor.cpp:163`, called from `jam_TextEditor.cpp:69`). The established sibling reacts on the tree: `EVEView::valueTreePropertyChanged` handles `Id::commandCount` (`EVEView.cpp:48-49`). Fix (MANIFESTO **E**: *"Find the established pattern. Extend it."*): remove `promptAttachment` (`EVETerminalProcessor.h:219-222`, `EVETerminalProcessor.cpp:23-50`). Under `#if JAM_USING_PROFILER`, `EVEView::valueTreePropertyChanged` handles `Id::promptCount` with the value 1 from the event payload. It does the same three operations as Step 3 item 4 on the editor itself (`repaint`, `performAnyPendingRepaintsNow`, the ENAX `set` in the cast shape of `jam_PluginEditor.cpp:310-314`). The handler runs from the vblank callback, outside paint (`jam_TextEditor.cpp:69`).
5. The same direct runs showed the OMP-help bug of Step 12 in the zsh run. Facts for Step 12: `prepare` calls `tty->setWinsize (winsize)` right after `openTTY` (`EVETerminalProcessor.cpp:304-305`). `setWinsize` sends `SIGWINCH` to the shell each time (`jam_TerminalTTY_posix.cpp:364-368`). A `SIGWINCH` before the first `_omp_precmd` runs `TRAPWINCH() { omp_repaint_prompt }` (`~/.config/zsh/zshrc:181`) with the `_omp_*` values unset (`~/.cache/oh-my-posh/init.11065323420480217391.zsh:419-424`, `:501-516`, `:743-746`). `oh-my-posh print primary --status= …` exits 70, and it writes `Error: invalid argument "" for "--status" flag` and the usage block to stderr (oh-my-posh 30.6.5).
6. After correction 4, END quit by itself after 3 s with exit 0, but scout reported `not found: …/benchmark/pane-samples.md`. A temporary `debug::Log` in `ENDApplication::shutdown` showed that `shutdown()` ran, `writeSamples` returned, and the parsed manifest is `<RUN … SAMPLES="pane-samples.md" …/>`. `writeSamples` read `run[Id::samples]` (`jam_Profiler.cpp:161`), so the target was the manifest folder, and `replaceWithText` failed silently in Release (`:178-179`). The table keys are tags: `Id::toType` returns the SCREAMING_SNAKE_CASE tag (`jam_Id.cpp:5-21`). The siblings read `profilerTable[Id::toType (Id::panes)]` (`ENDView.cpp:278`) and `profilerTable[Id::toType (Id::shell)]` (`EVETerminalProcessor.cpp`, `openTTY`). Fix: `run[Id::toType (Id::samples)]` at `jam_Profiler.cpp:161`. The diagnostic lines were removed from `Main.cpp` in the same sprint (CODING.md DIAGNOSTICS).
7. `pane.md` passed and wrote `pane-report.md`. `split.md` aborted: `EXC_CRASH SIGABRT`, `std::terminate` from `jam::Profiler::addEntry`, called from `TerminalSwap::writeFromTerminalBuffer` on EVE's terminal dispatch thread (`END-2026-11-03-150657.ips`). Cause: each `EVEProcessor` owns a `jam::Profiler` (`EVEProcessor.h:119`). `Instance<Profiler>` publishes each new object as the global head at construction (`jam_Instance.h:66-68`), before `prepare` sizes it (`EVEProcessor.cpp:17`). `prepare` assigns `fifos` before it resizes `keys` (`jam_Profiler.cpp:69-76`). Pane 1's thread passes the guard at `:25`, and `keys.at` at `:33` throws inside a `noexcept` function. Clauses: MANIFESTO **E** Complete at Creation (*"A unit is complete when it comes into existence"*) and **D** (*"A plain (non-atomic) read that races a write"*). Shared ownership of one object is `SharedInstance<T>` (`jam_Instance.h:20-24`). Fix:
   1. jam: `explicit Profiler (int capacity)` sizes the rings at construction. It replaces `Profiler() = default` and `prepare` (Refactor-Rewrite Discipline).
   2. EVE: `jam::SharedInstance<jam::Profiler> profiler { std::in_place, jam::Profiler::defaultCapacity }`. All EVE instances in one process share one profiler. The `prepare` call is removed.
   3. END: `jam::Profiler profiler { jam::Profiler::defaultCapacity }`. END is the sole owner, so a plain member stays legal (`jam_Instance.h:23-25`). The `prepare` call is removed.

   The other `jam_profiler` consumers own no `Profiler` object.
8. `pane.md` and `split.md` pass (11 epochs each, `dropped` 0). The Where lane (`pane-where.md`, scout `## sample`) needs root: *"Sampling needs elevated rights. Run scout with `sudo` on macOS"* (`scout/Source/HELP.md:60`). Without sudo: `kperf.reset: Operation not permitted`. ARCHITECT: *"you can run kernel sample without sudo, sudo always active. did you even try?"*. `sudo -n scout` runs END as root, and the pane shell is `login -fqlp root … exec -l /bin/sh` (ps). The epoch never ends, because the OSC 133 A comes from EVE's zsh or bash integration (`zsh_integration.zsh:6`, `ShellIntegration.cpp:37-63`). `sudo -n --preserve-env=SHELL` is refused (*"sorry, you are not allowed to set the following environment variables: SHELL"*). The whatdbg `sample` driver fails at load (`~/.carol/skills/trace/epoch.lua:4`: `module 'core.markdown-table' not found`). ARCHITECT ruling: *"Manifest names shell"*. In profile builds hosted by END, EVE's `openTTY` takes `shell` from the END manifest's `## profiler` table, the same as EVE's standalone branch (`eve/benchmark/live-where.md:17-25`, `EVETerminalProcessor.cpp` `openTTY`). The integrated path stays, so the zsh integration still sends OSC 133 A (`ShellIntegration.cpp:49`). EVE reads the END manifest path through ENAX, in the shape of the `size` rule:
   1. jam `cast/core.md`: `| @string | enaxManifestFile | `enax.manifest-file` | |` after `enaxKeyPressed` (`:76`), and the `@id` row `manifestFile` in the table that holds `promptPainted` (`:124`).
   2. jam `jam_ENAXClientExtensions.cpp`, in the `#if JAM_USING_PROFILER` block: the rule `Id::manifestFile` fills a `juce::File&` from the host getter `Id::enaxManifestFile` (the shape of the `Id::size` rule, `:41-51`).
   3. END `Nexus.cpp`, under `#if JAM_USING_PROFILER`: `static juce::File (*manifestFileGetter)()` returns `Nexus::getManifestFile()`, and `extensions.try_emplace (Id::enaxManifestFile, &manifestFileGetter)` (the shape of `displayBoundsGetter`, `:27`, `:39`).
   4. EVE `EVETerminalProcessor.cpp` `openTTY`, integrated branch, under `#if JAM_USING_PROFILER`: the shell is the manifest `shell` row, with `$SHELL` as the default value (`juce::ValueTree::getProperty (name, defaultReturnValue)`).
   5. `benchmark/pane-where.md` `## profiler`: the row `shell | string | /bin/zsh`.

   New names, each in an established family (NAMES.md Rule 5): `enaxManifestFile` (`enaxDisplayBounds`), `manifestFile` (rule ids `size`, `promptPainted`), `manifestFileGetter` (`displayBoundsGetter`).
9. `sudo -n scout …/pane-where.md` finished (`pane-where-report.md`, 15:59, 5 epochs, `dropped` 0). Each `## SAMPLE` key is an image name (`END` 652, `EVE` 390, `zsh` 7), so no frame resolved to a function. scout uses the image name when it has no symbol (`scout/Source/HELP.md:70-72`). Cause: `isSymbolImage` accepts only `MH_MAGIC_64` (`scout/Source/Symbolizer.cpp:157-162`), and `getCommand` reads only a thin Mach-O (`:92`). END and EVE are universal binaries (`file`: *"Mach-O universal binary with 2 architectures: [x86_64…] [arm64]"*). ARCHITECT ruling: *"scout symbolizer"*. Fix: in `scout/Source/Symbolizer.cpp`, `getSymbols` takes the slice of the host architecture from a fat image (`<mach-o/fat.h>`). The thin-image body moves to `getSliceSymbols (const char* data, size_t size)`, and `isSymbolImage` also accepts the fat magic. Build and install: `cast cast/spell.md --no-sign` in `scout/` (`scout/project-info.md:215-219`). The post-build step copies the binary to `~/.local/bin/scout` (`scout/CMakeLists.txt:386-389`), which is the `NOPASSWD` path (`sudo -n -l`). New names: `getSliceSymbols` (family `getSymbols`, `getTableSymbols`, `getKernelSymbols`) and `hostCpuType` (family `kernelProcessId`).
10. The rebuilt scout (Nov 3 16:23) stopped before any epoch: `scout: …/benchmark: cannot write output`. The old scout (Oct 31) was built against an earlier jam. In the current jam, a `key | type | value` table yields one property per row, named by the row key tag (`jam_ConfigDocument.h:91-98`, `:200-219`). scout reads its `## run` table in lower case (`scout/Source/Processor.cpp:61`, `:66-68`, `:89`): `epochs` read 0, and `output` resolved to the manifest folder. It reads its `## sample` table the same way (`scout/Source/Sampler.cpp:232`, `:546`, `:869-870`). Child-form tables keep their column headers (`value`, `calls`, `unit`, `bins`), so `Benchmark.cpp:168-178` and `ProfileReport.cpp` stay as they are. Fix: each of those reads uses `Id::toType (…)`, the same as `Processor.cpp:27` and `jam_Profiler.cpp:161` (correction 6). scout uses only static members (`scout/docs/xml/_benchmark_8cpp.xml`, `_statistics_8cpp.xml`, `_profile_report_8cpp.xml`, `_sampler_8cpp.xml`). ggwp has no `Profiler` reference (`ggwp/docs/xml`).
11. Runs under `sudo -n scout` did not end, and END stayed on the desktop as root. Facts and fixes:
    1. `pane-where.md` had no `## toolchain` table, which Step 8 requires. The sampled runs used the 15:45 binaries (`ls -lT`: END 15:45:15, zip 15:43:26). Fix: the five toolchain rows of `pane.md` are in `pane-where.md`.
    2. scout waited without a limit (`Benchmark.cpp:101`), and on macOS the child was not bound to scout (only the Windows job object binds it, `Benchmark.cpp:10-21`). After a SIGTERM, END ran on with ppid 1 (`ps`). MANIFESTO **B**: *"No object outlives its owner"*. Fix: on macOS and Linux, `Benchmark` calls `setpgid (0, 0)` and sends SIGKILL to its process group on SIGTERM, SIGINT and SIGHUP (`killProcessGroup`, `Benchmark.cpp:6-38`; `<csignal>` in `Benchmark.h`). A stop now ends scout, END, login and zsh (`ps` after a stop: none left).
    3. Under root, EVE stored `promptCount 1`, the getter flushed about 57 times a second, and EVEView received no flushed value (`debug::Log`, 18:10). The same build as the user delivered `promptCount 1`, `promptPainted` and `quit` (18:35). The kernel sampler runs in the scout process (`Benchmark.cpp:113`). The children need no root. `man sudo`: *"SUDO_UID Set to the user-ID of the user who invoked sudo"*, *"SUDO_GID Set to the group-ID of the user who invoked sudo"*. Fix: in `jam_ChildProcess_posix.cpp`, the parent reads both values before `fork`. When `geteuid()` is 0 and both exist, the child calls `setgroups`, `setgid` and `setuid` before `execvp`, and `_exit (EXIT_FAILURE)` when one fails (`:28-32`, `:52-63`). The toolchain rows and the epoch command now run as the invoking user (`ps`: `jreng … ninja`). scout keeps root for the sampler.
    4. All temporary `debug::Log` lines are removed (CODING.md DIAGNOSTICS).
    5. `sudo -n scout …/pane-where.md` passed: exit 0, 5 epochs, `dropped` 0, `## sample` and `## cswitch` tables (`pane-where-report.md`, 18:49).

Pathfinder returns the exit code, the stderr and each report verbatim.
**Validation:** each report has the `panePrompt`, `newPane`, `sessionPlugin`, `graphNode` and `paneEditor` rows, and the `RUN` row `buildType` `Release`. `dropped` is 0. COUNSELOR records any jam zone row in the END report (for example `terminalProcess`). Such a row shows that the two jam copies share one profiler instance. On a `toolchain command failed`, COUNSELOR reads the failing row output and corrects the step that caused it (Failure Protocol).

### Phase C — Amendment (ARCHITECT gate)

### Step 10: Measured bottleneck table
**Action:** COUNSELOR adds a section `## Amendment — Measured Bottlenecks` to this PLAN. It holds these items:
1. The baseline numbers: the `panePrompt` median for the cold pane (`pane.md`) and for the split pane (`split.md`), and the `newPane` median.
2. One row for each bottleneck, ordered by measured time. Each row has the zone or the Where-lane frame, the median ms, the citation, the CONTRACT clause that the code breaks, the fix, and the expected gain. The rows include each inventory fact above that the measurement ranks.
3. The answers to the RFC Key points (RFC lines 45-49): why the message thread blocks, and any allocation, copy, scan or wait that is not necessary. Each answer has a measurement and a citation.
4. The `%` and OMP-help steps: the trace spec targets (Step 12) and ARCHITECT's Debug build request.

COUNSELOR presents the amendment. This is the planned stop of the approved plan shape. ARCHITECT approves the amendment, and execution continues.

### Phase D — Fixes (after the amendment is approved)

### Step 11: Remove each bottleneck
**Action:** for each amendment row, in order:
1. @Engineer applies the fix.
2. COUNSELOR validates the fix.
3. @Pathfinder runs `pane.md` and `split.md` again. Each manifest has `baseline` set to the previous report.

Constraints (RFC lines 34-36): the audio and terminal-processor thread stays lock-free. Work that blocks the message thread moves to parallel work when CONTRACT allows it.
**Validation:** each fix shows a change in its row with a small `probability` (scout report). The step ends when the `panePrompt` median is below 500 ms in both reports.

### Step 12: `%` and OMP-help bugs
**Action:**
1. ARCHITECT builds END Debug (`project-info.md` `debug` rows, `:116-122`). These rows build EVE Debug first.
2. COUNSELOR writes a trace spec in the scratchpad. The breakpoints are: the `forkpty` winsize (`jam_TerminalTTY_posix.cpp:252-260`), each `setWinsize` (`:357-370`), each `TerminalProcessor::reset` (`jam_TerminalProcessor.cpp:18`), and the bytes after each `tty.read` (`:82-93`).
3. @Pathfinder runs the `trace` driver.
4. COUNSELOR names the root cause with both citations. @Engineer fixes it from the CONTRACT clause.
**Validation:** in a new pane, ARCHITECT sees no `%` line and no OMP help above the first prompt.

### Phase E — Close

### Step 13: Audit
**Action:** @Auditor runs once over every file of Steps 1-12. COUNSELOR validates each finding (Design by Contract Filter). Each confirmed finding is resolved in this sprint.

### Step 14: Doxygen pass and documents
**Action:** @Pathfinder runs the doxygen pass on every new or changed member. COUNSELOR gives the exact targets. The pass also writes these items:
- the cast `description` cells of the new rows, including `exportDynamic` (`project-info.md:226`);
- the `promptStart` row text, which says *"fires promptRow"* (`jam/cast/terminal.md:906`), so that it states the real handler;
- `ARCHITECTURE.md` ENAX ids (`:728-737`) and `SPEC.md` §1.5 (`:232-236`), with `enaxPromptPainted`.

The sprint is logged on ARCHITECT's "log sprint".

## Amendment — Measured Bottlenecks

### Baseline (Release, `dropped` 0)

| Zone | `pane.md` (11 epochs, 18:35) | `pane-where.md` (5 epochs, 18:49, sampled) | `split.md` (11 epochs, 15:46) |
|---|---|---|---|
| `pane prompt` (RFC number, target < 500 ms) | 888 ms | 897 ms | 1780 ms |
| `new pane` (message thread blocked) | 756 ms | 753 ms | 1480 ms |
| `pane editor` | 277 ms | 270 ms | 545 ms |
| `session plugin` | 245 ms | 248 ms | 471 ms |
| `graph node` | 0.16 ms | 0.16 ms | — |

`split.md` was measured before corrections 8-11. It is measured again in Step 11.

### Where lane (`pane-where-report.md` `## sample`, 18:49)

The value is the median count of samples per epoch that hold the frame. The interval is 1 ms. The threads merge, thus a count is not a duration.

| Frame | Count |
|---|---|
| `tabview::add` (the `new pane` work) | 750 |
| `tabview::createchild` → `editorview::editorview` | 516 |
| `jam::plugineditorlayout::getorcreatesettings` | 664 |
| `jam::configdocument::addtables` | 498 |
| `EVEview::eveview` | 498 |
| `juceVSTwrapper::handlegeteditorbounds` (from `editorview::createprocessoreditor`) | 267 |
| `juceVSTwrapper::handleopeneditor` (from `juce::vstpluginwindow::openpluginwindow`) | 230 |
| `EVEprocessor::eveprocessor` (from `vstplugininstanceheadless::constructeffect`) | 241 |
| `juce::characterfunctions::compare` (UTF-8) | 203 |
| `jam::format::towords` | 117 |
| `jam::document::parse` | 100 |
| `jam::markdowndocument::parse` | 82 |
| `jam::markdownwriter::gettext` | 60 |
| zsh `winchanged` (shell process) | 133 |
| zsh `runinitscripts` (shell process) | 129 |

### Bottleneck rows (ordered by count)

1. **EVE builds its settings three times for each pane.** Count: `getorcreatesettings` 664 of the 750 in `tabview::add`.
   - Sites: `EVEProcessor.cpp:16`, and `EVEView.cpp:10` once for each editor build (row 4).
   - Each call derives the default document, parses it, reads and parses the user file, and validates it (`jam_PluginEditorLayout.cpp:50-67`).
   - Clause: MANIFESTO **S** *"A second copy of any truth is a bug waiting to happen"*. **E** Complete at Creation: *"no downstream consumer mutates it, copies its fields into a working buffer, or re-derives what that operation already produced"*.
   - Fix: the processor builds the settings one time. The settings tree is complete at that creation. EVEView reads that tree and does not call `getOrCreateSettings`. Step 11 reads `jam::PluginEditor::initialise` and `PluginEditorLayout::populateTree` first, for the exact read path.
   - Expected gain: two of the three builds, about two thirds of 664.
2. **The default settings document is derived at run time on each build.** `getDefaultSettings` runs the CSS cascade, writes markdown, and the caller parses the text again (`jam_PluginEditorLayout.cpp:52-53`, `:589-607`). Counts: `markdownwriter::gettext` 60, `markdowndocument::parse` 82, `document::parse` 100.
   - RFC: *"EVE loading is DETERMINISTIC, the binary, the plugin, the instance all paramters are KNOWN at build time"* (RFC:48).
   - Clause: **E** Complete at Creation: *"Each violation is a consumer that builds again what creation made"*.
   - Fix: the default document is built one time for each process and read in place by each instance.
3. **Settings validation scans text.** Counts: UTF-8 `compare` 203, `format::towords` 117. Sites in `jam_ConfigDocument.cpp`:
   - `:434-436`: each row compares its key text against every earlier row (quadratic).
   - `:445-458`: two `Format::toWords` calls per row, and a `juce::Identifier` built from text for each table of the default tree.
   - `:479-481`: the table match compares tag text with `compare (…) == 0`.
   - Clause: CODING.md *"No `==` / `!=` on strings … Dispatch by key instead (… `juce::Identifier` equality — an Identifier compare is a pointer compare …)"*. RFC:48, unnecessary iteration.
   - Fix: one pass with keys as `juce::Identifier`. The duplicate check and the table match use Identifier equality. Each row computes its words one time.
4. **The VST2 wrapper builds EVE's editor two times.** Count: `EVEview::eveview` 498 = 267 (`handlegeteditorbounds`) + 230 (`handleopeneditor`).
   - `effEditGetRect` creates the editor (`juce_audio_plugin_client_VST2.cpp:1500`). `effEditOpen` deletes it and creates it again (`:1524-1525`).
   - END hosts EVE as VST2 (`files::evePlugin`, `cast/files.md:31`).
   - The 230 are the `new pane` time that has no zone (756 − 277 − 245 = 234 ms).
   - With rows 1-3, each build costs less. This row stays as a separate measured row in Step 11.
5. **The pane chain is synchronous on the message thread.** `tabview::add` 750 holds `createchild` 516 and `openpluginwindow` 230. `session plugin` 248 is `constructeffect` → `EVEprocessor::eveprocessor` 241, under `createInstanceFromDescription`, which waits on `finishedSignal.wait()` (`juce_AudioPluginFormat.cpp:76`).
   - RFC:36, *"parallel job to shave time is preferable"*, and RFC:49.
   - Step 11 measures again after rows 1-4, and then shows the remaining wait with its citation.
6. **The terminal restarts several times while the pane grows.** The temporary log of the 18:35 run showed six `openTTY` calls in one pane. Each `winsize` change calls `restart()` (`EVETerminalProcessor.cpp:21-22`). Each restart reaches `prepare`, `openTTY` and `tty->setWinsize` (`:288-289`). The shell spends 133 samples in `winchanged`. This row is the cause path of Step 12.
7. **Not a bottleneck:** `graph node` 0.16 ms. The synchronous graph rebuild (`juce_AudioProcessorGraph.cpp:1958-1971`) costs less than 1 ms.
8. **After the block:** `pane prompt` − `new pane` = 144 ms. The zsh start (`runinitscripts` 129) runs in the shell process, in parallel with the message thread.

### RFC key points (RFC:45-49)

- **Why the message thread blocks.** The whole pane chain runs synchronously in the `newPane` action. The processor build waits on `finishedSignal.wait()` (`juce_AudioPluginFormat.cpp:76`). Inside the chain, 664 of 750 counts are the settings builds (rows 1-3), and the editor is built two times (row 4).
- **Unnecessary allocation and copying.** Each settings build allocates the default text, two parsed documents and three trees (`jam_PluginEditorLayout.cpp:52-63`). This happens three times per pane, and the result is the same each time.
- **Unnecessary iteration.** The duplicate scan and the text compares of row 3, for data that is known at build time (RFC:48).
- **Unnecessary waiting.** The editor delete-and-create of row 4, and the six terminal restarts of row 6.

### `%` and OMP-help (Step 12)

- Trace targets, unchanged from Step 12: the `forkpty` winsize (`jam_TerminalTTY_posix.cpp:252-260`), each `setWinsize` (`:357-370`), each `TerminalProcessor::reset` (`jam_TerminalProcessor.cpp:18`), and the bytes after each `tty.read` (`:82-93`).
- Measured input: six `openTTY` calls per pane (row 6) and the `SIGWINCH` path of correction 5.
- ARCHITECT builds END Debug (`project-info.md` `debug` rows, `:116-122`).

## BLESSED Alignment

- **B:** the profiler is owned by `ENDApplication`. The span is an RAII `Zone` owned by `ENDView`. The attachment is owned by `EVETerminalProcessor`. No guard and no `SafePointer`.
- **L:** one zone per stage. No new class, no new `.cpp`. The workload action has two branches.
- **E:** each zone and rule is under `JAM_USING_PROFILER`. The signal is a told event through ENAX. END does not ask EVE for state.
- **S (SSOT):** the prompt count lives once in the processor. It reaches the Model through the existing events map. END counts panes through `getChildCount`. END holds no counter.
- **S (Stateless):** no boolean flag. The once-per-instance signal comes from the Model change notification (0 → 1).
- **E (Encapsulation):** jam_gui and jam_terminal do not call ENAX. Only the plugin bootstrap and EVE call it. No layer change.
- **D:** each fix in Phase D is accepted only with a measured, repeatable change.
- **LANGUAGE.md:** no exception is used.

## Risks / Open Questions

- None open. The four rulings above close the design questions. The amendment (Step 10) is the planned ARCHITECT gate that the approved plan shape names.
- Side effect, stated as a fact: Step 8 toolchain row 4 (run in Step 9) replaces the tracked `Source/plugins/EVE v0.1.0 macOS.zip` with a profile EVE. The release toolchain (`project-info.md:106-110`) builds EVE first, and it rewrites the archive.
