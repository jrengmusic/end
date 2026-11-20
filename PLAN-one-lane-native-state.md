# PLAN: One Lane, Native State

**RFC:** none — objective from ARCHITECT prompt (`RFC-SURF.md` is a different objective)
**Date:** 2026-11-20
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM

It replaces `PLAN-end-ipc-server.md` and `PLAN-eve-state-in-end.md`.

## Context

EVE in END starts with a wrong `winsize`, and an END window resize does not reach it. ARCHITECT: *"winsize init already WRONG. DONT HACK, DONT PATCH. DESIGN BY CONTRACT"*.

Causes, read this session:
- The server connection calls `onAttach (tree)` before it creates the synchroniser (`jam_ValueTreeConnection.cpp:56-58`). The attach writes never reach EVE (`Nexus.cpp:167`, `EditorView.cpp:230-233` → `:64-70`).
- EVE writes its own size in its constructor (`EVEProcessor.cpp:28-33`). Its editor sizes itself from those rows (`EVEView.cpp:450-456`). The first `winsize` comes from that size (`jam_TextEditor.cpp:1976`, `:1985-2001`).
- `onAttach`, `onLost` and `onWindowRequested` are manual callbacks where a listener pattern exists (MANIFESTO E, *"a manual callback … invented where a listener pattern already exists is not a solution"*).
- `cwd` uses ENAX and the other terminal rows use IPC. These are two lanes.
- END never calls `setStateInformation`. Its only state call is `getChunkData` (`Session.cpp:160`).

ARCHITECT's rulings, verbatim:
- *"nothing wrong with IPC, it's our lane for parameters that is not AudioParameter, since we need state that were not float, bool, int. we need arbitrary state including string"*
- *"why we cant simply call setStateInfo() and getStateInfo() as is? why create workaround it? instead integrate seamlessly in adherence to THAT?"*
- *"enax extension or IPC, CHOOSE ONE FOR ALL"*. The answer is IPC for all terminal state (this session).
- *"AudioParameter lane IS NOT THE WAY. we need consistent lane for all"*
- *"why use change broadcaster? NO"*; *"why dont we trust ValueTreeSynchroniser? ValuTree::Listener? APVTS::Listener?"*
- AskUserQuestion: scope *"Part A and Part B"*. Restored pane start directory: *"Own saved cwd"*.
- Standing: *"END will be the sole author of winsize"*; *"END has no cwd config. END runtime state track last focused eve cwd, and tell eve to start at cwd, default always eve config. there is no fallback."*; *"eve has its own config. nothing about that changed whatever the host is"*; *"END is the host. it holds its plugin instance stateInfo."*; *"session restoration with daemon, definitely NEED ALL"* (`SPEC.md:356-357`).

Corrections to this session's chat answers:
- EVE's state pair stays as it is. EVE holds two Models: the APVTS root and the terminal `jam::Model`, whose tree is a child of the root (`EVETerminalProcessor.cpp:22`). `flush` and the rejoin (`:40-59`) do for the terminal Model what APVTS does in `copyState` (`juce_AudioProcessorValueTreeState.cpp:389-394`) and in `replaceState` (`:400`). The claim "one line each, as in JFS" was wrong.
- A restored blob needs no size strip. EVE's `width` and `height` are the size that END set, which `resizeEnd` writes back (`EVEView.cpp:102-111`). So a restore carries END's own size.

## Overview

Every host-to-EVE value goes over one IPC lane: `cwd`, `width`, `height`, `focus`, `mode`, `markdownFile`. The lane uses only `juce::ValueTreeSynchroniser` and `juce::ValueTree::Listener` events, including `valueTreeRedirected` for a root swap. Under END, EVE writes no size, so its shell waits for END's size. END persists each plugin's state through `getStateInformation` and `setStateInformation`, used as is, and restores sessions from `END.settings`.

## Language / Framework Constraints

- The VST2 host calls the creation callback inside `createPluginInstance` (`juce_VSTPluginFormatImpl.h:2814`). The format posts that call (`juce_AudioPluginFormat.cpp:93-105`). So EVE's constructor and `Session::addPlugin` run in one message.
- A connection posts `connectionMade` (`juce_InterprocessConnection.cpp:290-291`) and each data delivery (`:332-335`). So an `addPlugin` restore always comes before EVE's first sync.
- One socket keeps the send order: `sendMessage` writes to one socket (`juce_InterprocessConnection.cpp:212-235`), and one thread reads it (`:401-415`).
- END reads EVE's identity from the raw chunk through `AudioPluginExtensions::VSTClient::getChunkData` (`juce_AudioPluginExtensions.h:123`). This is the only JUCE API that returns the plugin's own bytes. `getStateInformation` returns the FXB wrapper (`juce_VSTPluginFormatImpl.h:1581`). The read works for VST2 only. `VST3Client` has `getPreset` (`juce_AudioPluginExtensions.h:91`), and `AudioUnitClient` has no state call (`:95-100`).
- `ChangeBroadcaster::addChangeListener` needs the message manager locked (`juce_ChangeBroadcaster.cpp:47-55`). Not used: ARCHITECT ruled it out.

## Framework Inventory

### 1. API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| Sync one tree over a connection | juce_data_structures | `juce::ValueTreeSynchroniser`: `sendFullSyncCallback`, `applyChange` (static), `stateChanged` (pure) | `juce_ValueTreeSynchroniser.cpp:104-249` | 1 |
| Apply a delta and skip the own synchroniser | jam patch | member `applyChange (data, size, undo)` | `jam/patch/juce-value-tree-synchroniser-hook.patch:203-206` | 1 |
| Follow a swapped root | juce_data_structures | `ValueTree::Listener::valueTreeRedirected` — *"When operator= is used to make a ValueTree refer to a different object, this callback will be made."* | `juce_ValueTree.h:545-549`; APVTS uses it at `juce_AudioProcessorValueTreeState.cpp:454-458` | 1, 4 |
| Swap of the APVTS root on restore | juce_audio_processors | `replaceState` → `state = newState` | `juce_AudioProcessorValueTreeState.cpp:396-404`; `jam_AudioModel.cpp:181-194` | 4, 6 |
| Child events with payload | juce_data_structures | `valueTreeChildAdded`, `valueTreeChildRemoved`; listeners run on the tree, then on each parent up | `juce_ValueTree.cpp:102-119` | 1-3 |
| Find a descendant by id | jam_data_structures | `jam::Model::getChildWithID` (recursive) | `jam_Model.h:492-499` | 1, 6 |
| Server and connections | juce_events | `InterprocessConnectionServer::createConnectionObject` (listener thread), `run` | `juce_InterprocessConnectionServer.cpp:35-92` | 1 |
| Lost and data callbacks on the message thread | juce_events | `ConnectionStateMessage`, `DataDeliveryMessage`, `SafeActionImpl::ifSafe` | `juce_InterprocessConnection.cpp:47-83`, `:261-336` | 1 |
| Host value for a plugin | jam_plugin_bootstrap | `ENAXClientExtensions::set (rule, fallback, args)`, `rules` | `jam_ENAXClientExtensions.h:36-43`, `:60` | 4 |
| Plugin state, host side | juce_audio_processors | `AudioPluginInstance::getStateInformation`, `setStateInformation` | `juce_AudioProcessor.h:1146-1218`; VST2 `juce_VSTPluginFormatImpl.h:1581-1585` | 6, 7 |
| Plugin state, client side | juce_audio_plugin_client | `handleGetData` → `getStateInformation`; `handleSetData` → `setStateInformation` | `juce_audio_plugin_client_VST2.cpp:1550-1603` | 6 |
| Raw chunk for the identity | juce_audio_processors | `VSTClient::getChunkData` | `juce_AudioPluginExtensions.h:123`; `Session.cpp:157-162` | 3 |
| Settings file | juce_core | `PropertySet::getValue`, `setValue`, `containsKey` | `juce_PropertySet.h:79`, `:136`, `:159`; `Main.h:60-66` | 7 |
| Binary tree in a string | juce_core | `MemoryBlock::toBase64Encoding`, `fromBase64Encoding`; `ValueTree::writeToStream`, `readFromData` | `juce_MemoryBlock.h:268`, `:281` | 7 |

### 2. Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| ValueTree listener with an `events` map | `Session::registerEvents`, `valueTreePropertyChanged` | `Session.cpp:57-104`; `EditorView.cpp:185-228` | 2, 3 |
| Child-added reaction that consumes the payload | `SessionView::valueTreeChildAdded` | `SessionView.cpp:25-29` | 2, 3 |
| Owner listens to `model.state` and stops in its destructor | `model.state.addListener (this)` / `removeListener (this)` | `EVEProcessor.cpp:44`, `:55` | 4 |
| ENAX rule with a fallback | before: `set (Id::server, [] (juce::ValueTree&) {}, model.state)`; as built: `set (Id::server, [] (jam::ValueTreeConnection&) {}, connection)`; `rules.add<juce::ValueTree&> (Id::keys, …)` | `EVEProcessor.cpp:44`; `jam_ENAXClientExtensions.cpp:33-51` | 4 |
| Adopt constructor that binds an existing row | `EditorView (model, row)`, `TabView (model, existingState)` | `EditorView.cpp:18-22`; `TabView.cpp:4-10` | 6 |
| Window state in `END.settings` | `settings.setValue (Id::window…)`, `restoreWindowStateFromString` | `Main.cpp` `destroyWindow`, `getOrCreateWindow` | 7 |
| Positive check with a named threat | `if (window != nullptr) destroyWindow()` | `Main.cpp` `shutdown` | 7 |

### 3. Names

| Family | Members | Intent | Step |
|---|---|---|---|
| jam lane | `jam::ValueTreeConnection`, `jam::ValueTreeServer` (`jam_ValueTreeConnection.h:5`, `jam_ValueTreeServer.h:5`) | client syncs its tree; server attaches each synced tree | 1 |
| JUCE server connection object | `createConnectionObject` (`juce_InterprocessConnectionServer.cpp:86-87`) | new member `jam::ValueTreeServer::Connection`: the server end of one lane | 1 |
| JUCE owner-told connection | `juce_ConnectedChildProcess.cpp:122` `owner.handleConnectionLost()`, `:135` `ChildProcessCoordinator& owner;` | `Connection` holds `ValueTreeServer& owner` and calls `owner.removeConnection (*this)` from `connectionLost` (replaces the withdrawn `isAttachedTo`) | 1 |
| ENAX rule ids | `engine`, `keys`, `server`, `promptPainted`, `manifestFile` (`jam_ENAXClientExtensions.cpp:9-71`) | the `cwd` rule is deleted. New rule `size` (jam `cast/core.md:147`). Under END, END authors the editor size | 4 |
| EVEProcessor members | `layout`, `model`, `terminalProcessor` (`EVEProcessor.h:117`, `:132`, `:138`) | new member `connection`, the JUCE noun (`juce::InterprocessConnection`) | 4 |
| Session load family | `loadPlugin`, `addPlugin`, `removePlugin` (`Session.h`) | new members `loadPlugins (const juce::ValueTree&)` and `setPluginStates (juce::ValueTree&) const` | 6 |
| Nexus get/set | `getSession`, `getActiveSession`, `getServerPort` (`Nexus.h`) | new members `getSessionsState()` and `setSessionsState (juce::ValueTree)` | 7 |
| END identifiers | `sessions`, `server`, `port` (`cast/identifiers.md:106-124`) | new row `state`. The JUCE family is `getStateInformation` | 6, 7 |

### 4. Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| "Consume the event payload." | the listener reads the `parentTree` and `child` it receives | `SessionView.cpp:25-29` |
| "No bail-out guards." | positive nesting | `Session.cpp:74-85` |
| "Structured bindings for all pair/tuple results" | `auto [entry, inserted] { …try_emplace… }; jassert (inserted);` | `Session.cpp:149-150`, `Nexus.cpp:180-183` |
| "Uniform signature in the type." | `jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, void (juce::ValueTree&)> events;` | `EditorView.h:198` |
| "No non-owning naked pointers as members." | the member is declared after the object it reads, so it ends first | `EVEProcessor.h:132`, `:138` |
| "Use the framework API fully." | JUCE sync, listener, state and settings API only | API table |

### 5. Absence

| Need | Query | Result |
|---|---|---|
| A ValueTree descendant walk in jam | `jam_Model.h` grep `apply\w*\s*\(`; `jam_core/**/*.h` grep `apply\w*\s*\(` | none in `jam_Model.h`. jam_core has `Document`, `AnyMap`, `Xml` and `AttributedString` walks only (`jam_Document.h:387`, `jam_AnyMap.h:308`, `jam_Xml.h:61`, `jam_AttributedString.h:258`) |
| A JUCE call that tells a server about a lost connection | `juce_InterprocessConnectionServer.cpp:35-92` read whole | `run` only creates connections |
| A format-neutral raw plugin state read | `juce_AudioPluginExtensions.h:82-148` read | VST2 `getChunkData` only |
| Session persistence in END | `Main.cpp`, `Nexus.cpp`, `Session.cpp` read whole; `SPEC.md:493` (*"session persistence"* on the roadmap) | none |

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against
MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions
(no deviation, no scope drift). No @Auditor run in this sprint (ARCHITECT, Step 9).

## Steps

### Phase A — one lane

### Step 1: jam — a lane of listeners only
**Scope:** `jam_data_structures/connection/jam_ValueTreeConnection.h/.cpp`, `jam_ValueTreeServer.h/.cpp`.
**Action:**
1. Delete the attach-and-lost constructor, `onAttach` and `onLost` (`jam_ValueTreeConnection.h`; `.cpp:20-26`, `:42-46`). Delete the server's `attach` parameter and its `onAttach` (`jam_ValueTreeServer.h:8`, `:14`; `.cpp:3-6`).
2. `jam::ValueTreeConnection` is the client end. It also derives from `juce::ValueTree::Listener`.
   - `explicit ValueTreeConnection (juce::ValueTree newTree)` stays.
   - `connectionMade` stays: it creates the synchroniser on `tree` and sends a full sync.
   - `messageReceived` applies a delta through the member `applyChange`.
   - `connectionLost` does nothing.
   - `valueTreeRedirected (juce::ValueTree& redirected)` asserts that no synchroniser exists yet, then sets `tree = redirected`.
   - `tree`, `synchroniser` and `Synchroniser` become `protected`.
3. `jam::ValueTreeServer::Connection` (private nested class, `final`, derives from `ValueTreeConnection`) is the server end. It holds `root`.
   - First message, in this order:
     1. Read the full sync into `tree` (static `applyChange`).
     2. Create the synchroniser on `tree`.
     3. Attach `tree`. When it has `Id::id`, replace the descendant of `root` with that id at the same index, through `jam::Model::getChildWithID`, `removeChild` and `addChild`. Otherwise append it to `root`.
   - Because the synchroniser exists before the attach, every listener write during the attach reaches the peer.
   - Later messages use the base `messageReceived`.
   - `connectionLost` removes `tree` from its parent, then calls `owner.removeConnection (*this)`. It holds `ValueTreeServer& owner`, the JUCE pattern (`juce_ConnectedChildProcess.cpp:122`, `:135`).
4. `jam::ValueTreeServer`:
   - It takes `juce::ValueTree newRoot`. The destructor calls `stop()`.
   - `createConnectionObject` adds a `Connection (*this, root)` to `connections` under `connectionsLock`.
   - `removeConnection (const Connection&)` removes that connection under `connectionsLock`.
   - Correction after the runtime test: the first form dropped a connection in `valueTreeChildRemoved` when its tree lay under a removed row. A split moves the pane row (`jam_MatrixComponent.cpp:72`, `:341-342`), so the first pane lost its lane. A connection now ends only when its peer disconnects.
**Inventory:** API rows 1-7; names "jam lane", "JUCE server connection object", "`is` answers"; synthesis "Consume the event payload".
**Validation:** no `std::function` and no `ChangeBroadcaster` in the lane. One owner per connection. A replaced held tree does not drop the attaching connection, because the new tree is not yet in `root` when the child-removed event runs.

### Step 2: END — Nexus and the application listen
**Scope:** `Source/plugins/Nexus.h/.cpp`, `Source/Main.h/.cpp`.
**Action:**
1. Nexus constructs `server` with `model.state` (`Nexus.cpp:35`).
2. Delete these from Nexus:
   - `attachState` and `attachPluginState` (`Nexus.cpp:149-168`, `Nexus.h`);
   - `onWindowRequested`;
   - `getStartDirectory` (`Nexus.cpp:225-230`) and `startDirectoryGetter` (`:31`);
   - the `enaxStartDirectory` extension (`:97`).
3. `ENDApplication`:
   - In `host`, it listens to `model.state`. The removal is in `shutdown`, next to `destroyWindow`.
   - `valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child)`: when `parentTree == model.state` and `child` has the `end` type, it builds the window, shows the Dock icon and brings the process to the front (today's lambda body, `Main.cpp` `host`).
**Inventory:** pattern "Child-added reaction"; synthesis "Consume the event payload".
**Validation:** the launch window request is a tree event only. END keeps no copy of a start directory.

### Step 3: END — the pane tells EVE at attach
**Scope:** `Source/plugins/Session.cpp`, `Source/ui/EditorView.cpp`, `Source/ui/TabView.cpp`.
**Action:**
1. `Session::addPlugin` keeps one `getChunkData` read (`Session.cpp:156-162`). It appends a held row that has only the chunk's type and `Id::id`, not the whole tree. Before the attach, `Session::getTerminal` of the pane then returns nothing, so `resized` writes nothing to a copy (`EditorView.cpp:64-70`).
2. The new-row constructor `EditorView (jam::Model&, juce::ValueTree tabState, jam::UUID)` writes the `cwd` of the sessions row (`tabState.getParent().getParent()`) to its pane row, when that `cwd` is not empty. Every new pane goes through it (add, split, corner drag, file drop); a restored pane goes through the adopt constructor. ARCHITECT: *"Own saved cwd"* for a restored pane. (Audit F2: the first form wrote it in `TabView::addPane`, which a file drop does not run.)
3. `EditorView::valueTreeChildAdded` (`EditorView.cpp:230-234`), when `parentTree` is the `plugin` row of `state`, writes these to the attached terminal, in this order:
   1. `cwd` from the pane row, when the pane row has one. Then it removes the `cwd` of the pane row.
   2. `focus` from the pane row.
   3. `width` and `height`, through `resized()`.

   One listener and one socket keep this order (Language constraints).
**Inventory:** pattern "events map", "Child-added reaction"; API "Raw chunk for the identity".
**Validation:** EVE gets `cwd` before any size, so it holds END's `cwd` before any valid `winsize`. No write goes to a copy. Each write is a first tell on the attached tree.

### Step 4: jam and EVE — EVE authors no size under END, and has one lane
**Scope:** `jam/jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.h/.cpp`, `jam/cast/core.md` (and `jam/generated/jam_Core.h/.cpp`), `eve/Source/EVEProcessor.h/.cpp`, `eve/Source/layout/parameters.md`.
**Action:**
1. ENAX changes:
   - Delete the `cwd` rule (`jam_ENAXClientExtensions.cpp:53-56`) and the `enaxStartDirectory` row (`cast/core.md:81`, with its generated lines).
   - Delete `serverConnection` (`jam_ENAXClientExtensions.h:61`).
   - `rules` gains the signature `void (ValueTreeConnection&)`. The `server` rule takes `ValueTreeConnection&` and calls `connectToSocket` on it (`:41-51`).
   - Add the rule `size` (`void (juce::ValueTree&)`) under END. Its body is empty, because END authors the size.
2. EVE changes:
   - Add the member `jam::ValueTreeConnection connection { model.state };` after `terminalProcessor` (`EVEProcessor.h:138`).
   - The constructor calls `model.state.addListener (&connection)` next to `addListener (this)` (`EVEProcessor.cpp:44`). It calls `set (Id::server, [] (jam::ValueTreeConnection&) {}, connection)`.
   - The destructor removes that listener (`:55`).
3. EVE moves its layout-size write (`EVEProcessor.cpp:28-33`) into the fallback of `set (Id::size, …, terminal)`.
4. EVE deletes the start-directory read (`EVEProcessor.cpp:37-39`). It writes its own config `cwd` (`ShellIntegration::getStartDirectory()`, `:40`).
5. `parameters.md:59`: under END, END writes the start directory on the lane. Otherwise EVE uses its config.
**Inventory:** pattern "ENAX rule with a fallback", "Owner listens to `model.state`"; API "Follow a swapped root"; names "ENAX rule ids", "EVEProcessor members"; synthesis "No non-owning naked pointers".
**Validation:**
- Outside END, the connection never connects, and EVE keeps its size and its `cwd`.
- Under END, `width` and `height` are empty until END writes them. The first `winsize` is then invalid (`jam_TerminalWinsize.h:60`), so the shell waits (`EVETerminalProcessor.cpp:372`).
- The connection ends before `model`, by member order.

### Step 5: Documents (Phase A)
**Scope:** `end/SPEC.md` §1.5 and §1.7, `end/ARCHITECTURE.md` (the lane, the ENAX ids, the host rows), `eve/SPEC.md` (the host-to-EVE and shell-start bullets).
**Action:** state the lane as built. Each decision line quotes ARCHITECT.
**Validation:** each line mirrors the code; ASD-STE100.

### Phase B — native state

### Step 6: END — restore a plugin with `setStateInformation`
**Scope:** `Source/plugins/Session.h/.cpp`, `cast/identifiers.md` (row `state`).
**Action:**
1. `Session::addPlugin`, before `graph.addNode`: when the pane row has `Id::state`, call `instance->setStateInformation` with that block, then remove the property. On the EVE side:
   - `replaceState` swaps the root (`juce_AudioProcessorValueTreeState.cpp:400`).
   - The connection follows through `valueTreeRedirected` (Step 4).
   - This happens before its `connectionMade` (Language constraints).
2. The `getChunkData` read of Step 3 runs after the restore, so the held row carries the restored id.
3. Add an adopt constructor: `Session (const juce::ValueTree& row, ENDModel&, juce::ValueTree sessionsState, …)`.
   - It binds `state` to `row`.
   - It adds the identity parameter with the row's id.
   - It listens like the existing constructor.
   - It calls `loadPlugins (state)`.
4. `loadPlugins (const juce::ValueTree& row)` calls `loadPlugin` for each pane row under `row` that has `Id::plugin` (Absence row 1).
5. `setPluginStates (juce::ValueTree& sessions) const`. For each plugin of the session:
   - It finds the pane row with `jam::Model::getChildWithID`.
   - It writes `getStateInformation` there as `Id::state`.
   - It removes that pane's `plugin` child and its `Id::name`.
**Inventory:** API "Plugin state, host side", "Swap of the APVTS root", "Find a descendant by id"; pattern "Adopt constructor"; names "Session load family", "END identifiers".
**Validation:** END never parses the blob. `getStateInformation` and `setStateInformation` are called as is. The live tree holds no blob after the restore.

### Step 7: END — save and restore the sessions
**Scope:** `Source/plugins/Nexus.h/.cpp`, `Source/Main.cpp`.
**Action:**
1. `Nexus::getSessionsState()` returns a copy of the sessions tree. Each session writes its plugin states on the copy (`setPluginStates`).
2. `Nexus::setSessionsState (juce::ValueTree saved)`, by value, because it moves the rows out of `saved`. For each saved session row, it:
   - moves the row into the sessions tree;
   - creates the `Session` with the adopt constructor;
   - sets `focusedSession` from the saved value, through the parameter, as `createSession` does (`Nexus.cpp:188-193`).
3. `ENDApplication::shutdown`, when `nexus != nullptr` (the launch process has no Nexus):
   1. Write `getSessionsState()` with `writeToStream`.
   2. Store it with `toBase64Encoding` under the `sessions` key of `settings`.
   3. Call `saveIfNeeded`.
4. `ENDApplication::host`, after `nexus` exists and before the window: when `settings.containsKey (sessions)` and `fromBase64Encoding` succeeds, call `nexus->setSessionsState (juce::ValueTree::readFromData (…))`.
**Inventory:** API "Settings file", "Binary tree in a string"; pattern "Window state in `END.settings`", "Positive check"; names "Nexus get/set".
**Validation:**
- A relaunch shows the same sessions, tabs and panes.
- Each restored EVE pane starts in its own saved `cwd`. A new pane starts in END's last focused `cwd`.
- The window builds from the rows (`SPEC.md:408-411`).

### Step 8: Documents (Phase B)
**Scope:** `end/SPEC.md` §1.7 and §3 (the roadmap line for session persistence), `end/ARCHITECTURE.md`.
**Action:** state the save and the restore as built, with ARCHITECT's quotes.
**Validation:** each line mirrors the code; ASD-STE100.

### Step 9: Hand over for the runtime test
- Report the changed files. ARCHITECT builds and tests the runtime.
- No audit. ARCHITECT: *"NO AUDIT … dont fucking waste my tokens for audit untested runtime"*.

## BLESSED Alignment

- **B:**
  - The server owns each connection in one `jam::Owner`.
  - A connection ends when its peer disconnects. A tree move does not end it.
  - EVE's connection is a member after `model`, so it ends first.
- **L:** delete `onAttach`, `onLost`, `onWindowRequested`, the `cwd` ENAX rule, `enaxStartDirectory`, `getStartDirectory`, the forced focus write and the parsed held copy.
- **E:** every reaction is a `ValueTree::Listener` event on the payload it receives.
- **S (SSOT):** EVE's tree is the one state of its pane. END authors its size and its new-pane `cwd`. The blob exists only in `END.settings`.
- **S (Stateless):** no flags. A new pane is marked by data, the pane row `cwd`, which the attach removes.
- **E (Encapsulation):** END never parses the blob. jam knows no EVE row.
- **D:**
  - The restore comes before the first sync (one message).
  - `cwd` comes before the size (one listener, one socket).
  - The synchroniser exists before the attach.

## Risks / Open Questions

- None open.
