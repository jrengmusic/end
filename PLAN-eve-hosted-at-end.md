# PLAN: EVE hosted at END

**RFC:** none — objective from ARCHITECT prompt. Context: `eve/PLAN-Grand-Scheme.md`.
**Date:** 2026-10-31
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.14 / JAM. CAST generates `CMakeLists.txt` and `Source/generated/*`. Do not edit generated files by hand.
**Delivery:** At approval, COUNSELOR writes this file to `end/PLAN-eve-hosted-at-end.md`. ARCHITECT commits it.

## Context

ARCHITECT: *"the goal of this sprint is to ensure eve loaded and run as fast as standalone."*
ARCHITECT: *"i doubt clap implementation is sound."* The sprint order, verbatim:

1. *"fix end project structure"*
2. *"ensure known format hosted correctly VST, VST3, AU"*
3. *"end can host actual audio plugin, with actual audio device"*
4. *"target eve release build to end source, so end carry the binary and load them to its pane"*
5. *"eve must be build and pass all format VST, VST3, AU FIRST."*
6. *"then we could build clap, and "FIX" END as working clap host CORRECTLY, design by contract"*

Rulings from this session:

- **Formats.** VST means VST2 (*"Add VST2"*). END hosts EVE in *"all of them VST2, VST3, AU"*. VST2 SDK: *"can copy from kuassa to jam"*.
- **Structure.** The structure answer was *"all the above"*. This covers three things:
  - AudioPluginHost parts in the GUI-less Nexus, with VirtualClock deleted and a `KnownPluginList` added.
  - A daemon skeleton.
  - An AudioPluginHost-like directory layout.
- **Clock.** *"we should also need to remove VirtualClock. since we now have jam::VirtualDevice. and eve carry its own clock"*. Grand Scheme:1334-1343 confirms this: `EVETerminalProcessor` owns `jam::VirtualDevice`, with *"One wiring in every host"*.
- **Host identity.** *"END is still true and true audio plugin host. EVE still an audio plugin but drives its own clock"*.
- **Daemon.** *"configurable daemon server with lock file so Nexus cam always run in bg even when the GUI quits (tmux parity)"*. Also: *"endless daemon works, but … probably only as reference, and we also could learn from decades battle tested tmux"*.
- **Binary.** *"preferably from its own BinaryData. if must load from disk then the design is per SPEC. carry from binary, put in config, sideload. this the pattern with endless windows conpty / openconsole"*.
- **Debts.** *"debt paying isnt as simple as porting command manager but also to fix the whole design"*. DEBT-20261025T214955 and DEBT-20261009T112447 enter this sprint (JRENG law).
- **Deferred.** *"windows build still miles ahead. defer."* This covers DEBT-20260912T150000.
- **Build.** *"you dont build, only ARCHITECT build"*. Each phase ends at an ARCHITECT build and test.

**Fact: BinaryData answer.** JUCE loads every format from a file on disk:

- VST3: `File (description.fileOrIdentifier)` (`juce_VST3PluginFormatHeadless.cpp:92`).
- AU: through the system registry, with `AudioComponentFindNext` (`juce_AudioUnitPluginFormatImpl.h:533`).

Thus the SPEC path applies: carry the binary, write it to disk, sideload it. The precedent is endless `WindowsTTY.cpp:247-276`. It writes `BinaryData::conpty_dll` to disk when the file is missing or its size differs, then loads it.

## Dependency & API Inventory

- **JUCE hosting.** These members exist in `Nexus.h:216-219` and only Nexus.h references them (LSP findReferences):
  - `juce::AudioPluginFormatManager`, with `addDefaultFormatsToManager` at `Nexus.h:18`
  - `juce::AudioProcessorGraph`
  - `juce::AudioProcessorPlayer`
  - `juce::AudioDeviceManager`

  Add `juce::KnownPluginList`, following AudioPluginHost `MainHostWindow.h:140`. Graph nodes follow AudioPluginHost `PluginGraph.h:73-120`.
- **END defines.** `JUCE_PLUGINHOST_VST3=1` and the Darwin-only `JUCE_PLUGINHOST_AU=1` (`end/project-info.md:371-373`). VST2 hosting needs `JUCE_PLUGINHOST_VST=1` and the VST2 SDK path, after the `eve/cast/cmake.cast:109-110` shape.
- **JUCE zip.** Use `juce::ZipFile` to extract a bundle that is carried as a zip. BinaryData gets its files from `## layout glob` rows (`end/project-info.md:400-416`).
- **jam_audio_devices.** Use `jam::VirtualDevice` (`jam_VirtualDevice.h:19-102`) only inside EVE. END does not instantiate it.
- **jam_clap.**
  - Format: `jam::ClapPluginFormat`, a `juce::AudioPluginFormat` (`jam_ClapPluginFormat.h:23-118`).
  - Wrapper: `jam::ClapWrapper` (`jam_ClapWrapper.h:17-31`), entered through `jam_ClapWrapperEntry.h:52-61`.
  - `hostRequestProcess` has an empty body (`jam_ClapPluginInstance.cpp:443-446`).
- **EVE formats.**
  - `eve/project-info.md:163-172` has rows for standalone, vst3 and au. Toolchain rows are at `:144-160`. Signing rows: `signFormats` and `installFormats` (`eve/cast/signing.md:24-26`).
  - VST2 sibling row, from JFS (the plugin precedent named in Grand Scheme:63-73): `jreng-filter-strip/project-info.md:162-166` (toolchain) and `:194` (`| vst | VST | vst | dll | /Library/Audio/Plug-Ins/VST | $PROGRAMFILES64\Steinberg\VstPlugins | … | Steinberg VST2 plugin |`).
  - VST2 SDK source: `kuassa/user_modules/sdk/VST2/`. Destination: `jam/sdk/VST2/`, because EVE's `@user-module` is `../jam` (`eve/project-info.md:18`).
- **Existing verbs.** `Nexus::createPlugin` (`Nexus.h:78-98`) and `Session::newPlugin`/`removePlugin` (`Session.h:40-45`) are the call sites in `ENDActions.cpp:66-85`, `:141-160`, `:242-258`.
  - Today the plugin id is hard-coded: `whelmedPluginId` (`ENDActions.cpp:3`).
  - Today only CLAP is scanned: `Nexus::getPluginDescription` (`Nexus.h:145-166`).

## Validation Gate

COUNSELOR validates each step before the next one. The checks are MANIFESTO.md (BLESSED), NAMES.md, CODING.md and the locked decisions of this PLAN. Scope must not drift. @Auditor runs once, after the last step.

## Steps

### Phase A — EVE passes VST2, VST3, AU (ARCHITECT item 5, "FIRST")

**Step 1: VST2 SDK into JAM**
- **Scope:** `jam/sdk/VST2/`.
- **Action:** Engineer copies `kuassa/user_modules/sdk/VST2/` to `jam/sdk/VST2/` as it is.
- **Validation:** The tree under `jam/sdk/VST2/pluginterfaces/vst2.x/` holds `aeffect.h`, `aeffectx.h` and `vstfxstore.h`.

**Step 2: EVE VST2 format rows**
- **Scope:** `eve/project-info.md`, `eve/cast/signing.md`.
- **Action:**
  1. Add a `vst` row to `## plugin format`, in EVE's column set, from the JFS row at `:194`.
  2. Add `vst` cast, cmake and ninja toolchain rows, from the EVE `vst3` rows at `:150-154`, with target `EVE_VST`.
  3. Add `VST` to `signFormats` and `installFormats`.
- **Validation:** Every row matches its sibling's shape. CODING has no rule on these tables, so COUNSELOR reads the generated `CMakeLists.txt` after ARCHITECT runs cast. `CAST_FORMATS` must hold `VST`, and the `juce_set_vst2_sdk_path` block (`cmake.cast:109-110`) must be active.

**ARCHITECT gate A:** ARCHITECT builds EVE Release in VST, VST3 and AU, and validates the three formats.

### Phase B — END structure (ARCHITECT item 1, "all the above")

**Step 3: Delete VirtualClock (DELETE FIRST)**
- **Scope:** `Source/Nexus.h`, `Source/action/ENDActions.cpp`.
- **Action:** Delete everything below. Compiler errors list what remains.
  - `struct VirtualClock`, `virtualClocks`, `createVirtualClock`, `removeVirtualClock` and `blockPeriodMilliseconds` (`Nexus.h:113-142`, `:170`, `:173-212`)
  - the three call sites (`ENDActions.cpp:77`, `:153`, `:254`)
- **Validation:** LSP findReferences on the deleted names returns nothing. No `jam::VirtualDevice` remains in END.

**Step 4: Plugin graph in Nexus**
- **Scope:** `Source/Nexus.h`, `Source/end/Session.h/.cpp`.
- **Action:**
  1. Each hosted instance becomes an `AudioProcessorGraph` node on the one `AudioDeviceManager` and `AudioProcessorPlayer` (`Nexus.h:25-27`). The shape follows `PluginGraph.h:73-120`.
  2. `Session` keeps its uuid-to-plugin ownership, as node ids into the Nexus graph. All parts stay in the GUI-less Nexus, so they outlive the GUI.
- **Validation:** One device callback clocks the graph. No getter is added for a caller decision (MANIFESTO S, E). State stays in `ENDModel` (MANIFESTO S).

**Step 5: [AMENDED — merged into Step 11]**
- ARCHITECT: *"no need table. END load terminal as default is SPEC contract … whelmed will be dropped, it will be absorbed to eve MarkdownComponent … END load EVE as default is SPEC contract. not user configurable. however we could open an API later for user configurable action to load any plugin to any pane, not this sprint."*
- **Consequences:**
  - No config row.
  - No `KnownPluginList` this sprint. Its only consumer is the later any-plugin API.
  - `whelmedPluginId` (`ENDActions.cpp:3`) and the CLAP-only `getPluginDescription` (`Nexus.h:122-143`) are deleted in Step 11. EVE becomes the default plugin there.

**Step 6: VST2 hosting**
- **Scope:** `end/project-info.md`.
- **Action:** Add a `JUCE_PLUGINHOST_VST=1` define row in the shape of `:371`. Add the VST2 SDK path in the `cmake.cast:109-110` shape.
- **Validation:** COUNSELOR reads the generated CMakeLists after ARCHITECT's cast run.

**Step 7: Directory layout**
- **Scope:** `Source/`, `end/project-info.md` `## source glob`.
- **Action:** Move host files (`Nexus.h`, `end/Session.*`) to `Source/plugins/`. Move view files (`end/*View*`) to `Source/ui/`. Update the source-glob rows.
- **Validation:** Each object stays one header and one `.cpp` (CODING "One object, one translation unit").

**Step 8 (original — superseded by the amendment below): CommandManager replaces the Action List and key maps (DEBT-20261025T214955, DEBT-20261009T112447)**
- **Scope:** `Source/action/ENDActions.*`, the keys config.
- **Action:**
  1. Delete the `ENDActions` key maps and keys reading (`ENDActions.h:67-92`, `:152-153`).
  2. Use `jam::CommandManager : juce::ApplicationCommandManager` (`jam_gui/commands`) and `juce::ApplicationCommandTarget`, with ids from `map::CommandId`.
  3. COUNSELOR reads the `jam::CommandManager` doxygen and header before the delegation.
- **Validation:** No hand-rolled key map remains. Every command goes through the framework.

**Step 8 [AMENDED — the project owns the command map]**
- ARCHITECT: *"adding to jam gui is wrong, it's END's own table, just like EVE's table, or any project"*; *"command manager should be project specific id. built by its own table"*; *"Owner (PROJECT) need to define its map, then tell the widget as consumer."*
- **Facts:**
  - `map::CommandId` is jam's `jam::Bimap<int, juce::Identifier>` (`jam/generated/jam_Gui.h:303`), generated from `jam/cast/gui.md:203-240`.
  - Consumers bound to it: `jam_CommandManager.h:65`, `.cpp:14,16,45,56`; `jam_TextEditor.cpp:230-347` (30 uses, compile-time enumerators); `jam_PluginEditor.h:273`, `.cpp:78,90,106-118,229-250`; EVE `EVEView.cpp:155`, `EVEViewPanelCallbacks.cpp:11,17`.
- **Scope widened by ARCHITECT's ruling:** jam `jam_gui/commands`, `jam_gui/widgets/jam_TextEditor.*`, `jam_plugin_bootstrap/view/jam_PluginEditor.*`, `jam/cast/gui.md`; EVE cast and callbacks; END.
- **8a (jam):**
  1. Delete the `CommandId` section from `jam/cast/gui.md`.
  2. [AMENDED] ARCHITECT: *"NO COPY"*; *"why overcomplicate with template?"*. JUCE's `ApplicationCommandManager` is the registry (`juce_ApplicationCommandManager.h:124`, `:134`, `:157-177`). The owner's command target answers `getAllCommands` / `getCommandInfo` from its own `map::CommandId`, read in place. jam's `CommandManager` deletes `SharedInstance<map::CommandId>` (`jam_CommandManager.h:65`). It turns an id into a name with `getNameOfCommand`, and a name into an id by walking `getCommandForIndex`. Widgets keep `SharedInstance<CommandManager>` as it is.
  3. Each jam consumer keys its commands by name (`juce::Identifier`, `Id::…`). In `perform` it reads the name with `getNameOfCommand (info.commandID)`. A name that no owner registered is not bound.
- **8b (EVE):** EVE declares its own `CommandId` table in its cast. It tells `CommandManager`, `TextEditor` and `PluginEditor`.
- **8c (END):**
  1. END declares its own `CommandId` table in `cast/bimaps.md`.
  2. `jam::CommandManager` and `juce::ApplicationCommandTarget` replace the `ENDActions` key maps and keys reading (`ENDActions.h:67-92`, `:152-153`). This pays DEBT-20261025T214955 and DEBT-20261009T112447.
- **Validation:**
  - No jam module references a project symbol (MANIFESTO E, layer topology).
  - No consumer reaches for a global table.
  - One table per project (MANIFESTO S).

**ARCHITECT gate B:** ARCHITECT builds END Debug and tests that panes open and close.

### Phase C — END hosts EVE on a real device (ARCHITECT items 2, 3, 4)

**Step 9: EVE Release into END source — post-build only**
- ARCHITECT, amendment: *"EVE target release build is only post-build copy, just like how JFS copy to release directory, the exact pattern"*.
- **Scope:** `eve/cast/cmake.cast` (post-build block), `eve/project-info.md` (its pack rows), `end/project-info.md` `## layout glob`.
- **Action:**
  1. EVE's Release post-build packs each format bundle into `end/Source/plugins/`. The pattern is JFS `cmake.cast:502-523`, verbatim: `CAST_PACK_ITEMS` from `$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_${CAST_FORMAT}>`, then `add_custom_command(TARGET post-build POST_BUILD COMMAND cast --pack …)`.
  2. COUNSELOR reads EVE's own post-build/pack block before the delegation and pastes both blocks.
  3. Add an END layout-glob row `plugins` for `Source/plugins/*.zip`.
  4. The toolchain argument `end` is dropped.
- **Validation:** The block matches the JFS sibling. BinaryData carries the packed bundles.

**Step 10: Offload at runtime (the endless conpty pattern)**
- **Scope:** Nexus, `ConfigDirectory`.
- **Action:**
  1. Write each EVE bundle from BinaryData to `~/.config/end/plugins/` (SPEC.md:154) with `juce::ZipFile`.
  2. Write a bundle only when it is missing or its size differs from the zip entry's uncompressed size. This is the check of endless `WindowsTTY.cpp:253-272`.
- **Validation:** No hand-rolled unzip. One owner writes the files. Nothing goes into a system plug-in folder (ARCHITECT: *"i dont want to put "END's in-house plugin" into system audio plugins path"*).

**Step 11: Load EVE from the offloaded path, three formats**
- **Scope:** Nexus, the plugin config row.
- **Action:**
  1. **VST2 and VST3.** `format->findAllTypesForFile (descriptions, offloadedPath)`, then `formatManager.createPluginInstanceAsync`. This is the existing pair at `Nexus.h:157` and `:85`. There is no scan, and EVE does not enter `KnownPluginList`.
  2. **AU (in-process registration).** This is the macOS analog of `LoadLibraryW` + `GetProcAddress` (`WindowsTTY.cpp:315-324`):
     - `CFBundleCreate` on the offloaded `EVE.component`.
     - `CFBundleGetFunctionPointerForName` with the bundle's `factoryFunction` (`EVEAUFactory`, `EVE.component/Contents/Info.plist:35-36`).
     - `AudioComponentRegister` with the plist description (`AudioComponent.h:586-591`; the registration is in-process only, `:568-570`).
     - Then the same `findAllTypesForFile` + `createPluginInstanceAsync` pair. `AudioComponentFindNext` finds the registered component (`juce_AudioUnitPluginFormatImpl.h:533`).
  3. EVE is the default plugin of every new pane (SPEC contract). There is no config row. A `static constexpr` in Nexus names the format that END loads EVE in, set to VST3, the shortest path (11.1). ARCHITECT edits it to test AU and VST2.
  4. Delete `whelmedPluginId` (`ENDActions.cpp:3`) and `getPluginDescription` (`Nexus.h:122-143`). This is the old Step 5.
- **Validation:** All loading goes through `formatManager`. The only format-specific code is the AU registration that JUCE does not provide.

**ARCHITECT gate C:** ARCHITECT runs `seq 1 100000000` in the EVE standalone and in END-hosted EVE (VST3, AU, VST2), in the same session. Acceptance: each hosted median is at or below the standalone median.

### Phase D — CLAP (ARCHITECT item 6)

**Step 12: EVE CLAP target**
- **Scope:** `eve/project-info.md`, `eve/cast/*`.
- **Action:**
  1. COUNSELOR reads the CLAP target rows of the whelmed project, the existing CLAP client of `jam::ClapWrapper`.
  2. COUNSELOR pastes them into the delegation. Engineer adds the EVE equivalents.
- **Validation:** The rows match the whelmed sibling.

**Step 13: END CLAP host contract**
- **Scope:** `jam_clap/format/jam_ClapPluginInstance.cpp`, `jam_ClapPluginFormat.*`.
- **Action:**
  1. COUNSELOR reads the contract of each `clap_host` callback in `jam_clap/clap/host.h`.
  2. Engineer implements the callbacks against the graph host. `hostRequestProcess` (`:443-446`) follows its documented contract. No host clock is added (Grand Scheme:1334-1343).
- **Validation:** Each callback cites its `host.h` contract line.

**Step 14: Update DEBT-20261025T135707**
- **Action:** Pay this debt with Steps 3 and 13, and record the receipt at /log. Its `request_process → device.requestProcess()` design and its 0.072 s precondition are replaced by Grand Scheme:1334-1343 and gate C.

**ARCHITECT gate D:** ARCHITECT runs the `seq` test of gate C with EVE as CLAP.

### Phase E — Daemon skeleton (ARCHITECT item 1, "all the above")

**Step 15: Daemon design read**
- ARCHITECT, on the process model: *"~/Documents/Poems/dev/tmux/"*.
- **Action:**
  1. Pathfinder reads the local tmux source in `~/Documents/Poems/dev/tmux/`. The read covers the server start, the client attach and detach, the socket and lock path, and what the server keeps alive when the last client goes.
  2. Pathfinder reads endless `Daemon.h` as reference only.
- **Validation:** The daemon design takes tmux's process model as read.
  - `EditorView` parents the plugin's own `AudioProcessorEditor` in-process (ARCHITECTURE.md:722).
  - If tmux's model and that constraint cannot both hold, COUNSELOR reports both citations in one line. This is the CAROL.md closed stop "two rulings … cannot both hold".

**Step 16: Daemon skeleton**
- **Action:** Implement the Step 15 design in Nexus: the lock file, and Nexus alive with no GUI.

**Step 17: Docs**
- **Action:** Bring `ARCHITECTURE.md:198-220`, `:705-800`, `SPEC.md:56-59` and `:151-160` up to date with the code.
  - VirtualClock rows go.
  - Graph, KnownPluginList and sideload rows come in.
  - The daemon section comes in.
- **Validation:** Each document line has a file:line in the code.

## BLESSED Alignment

- **B:** Nexus owns the format manager, graph, player, device and plugin list. Session owns its node ids. EVE owns its clock. No host-side per-plugin thread remains.
- **L:** VirtualClock deletion removes a second clock path. CommandManager deletes the hand-rolled key map.
- **E:** No format branch. Lookup goes through `KnownPluginList`. Framework API is used throughout.
- **S (SSOT):** One device clocks the graph. One plugin list holds the descriptions. One config row selects the plugin.
- **S (Stateless):** Views keep no host state. They react to `ENDModel`.
- **D:** Gate C measures hosted speed against standalone.

## Risks / Open Questions

1. **AU with the same description in the system folder.** EVE's build installs AU to the system folder (`eve/cast/signing.md:26`, `installFormats` VST3;AU). No source says which component `AudioComponentFindNext` returns when the in-process registration and the system copy share one description. ARCHITECT's gate C test covers this. The in-process route is untested until that run.
2. **Daemon process model.** The model comes from tmux (Step 15). The in-process editor constraint is ARCHITECTURE.md:722.

**Ratified names** (ARCHITECT: *"Ratify all"*):
- `Source/plugins/` (Nexus, Session) and `Source/ui/` (views)
- END layout-glob row `plugins` (`Source/plugins/*.zip`)
- EVE toolchain argument `end`
