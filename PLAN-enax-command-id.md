# PLAN: ENAX command ids — owner-scoped command tables, one binder, END drives plugin state through host parameters

**RFC:** none — objective from ARCHITECT prompt, session 2026-11-01. Contract record: `eve/PLAN-Grand-Scheme.md` §4.3 `[AMENDED 2026-11-01 — ENAX]`.
**Date:** 2026-11-01
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.14 (jam `patch/` lane) / JAM. CAST generates `CMakeLists.txt` and `Source/generated/*`. Agents never build; ARCHITECT builds.
**Sequence:** Plan 2 of 2. It depends on `PLAN-enax-plugin-host-type.md` (`jam::ENAXClientExtensions`, the key hook, focus forwarding).

## Overview

A command id is a callback id, owned by the owner of the state its callback changes. jam widgets own their command tables and default bindings. Projects own their own commands, and config only overrides bindings by name. Each binary has one binder and one entry. END drives a hosted plugin's state (`mode`, `zoom`) through JUCE's host parameter contract, which works in every format. The TextEditor keeps resolving its own keys by its own mode.

## Rulings (ARCHITECT, verbatim)

- *"our agreement, understanding in this session supersed all precedents rulings when conflicting"*
- *"this is Id for command, a definitive SSOT for CALLBACKS, open seam for user driven config also for future API exposes, for example shell scripting with EVE Terminal protocol. all just wiring, direct lookup to registrable callback to what it's being told by, a parameterChanged, a state change, a keyboard input, mouse events, etc etc"*
- *"so CommandManager-commandId is actually not about framework-project ownership, neither about juce::Component widget at all. its relationship is with juce::ValueTree, APVTS, jam::Model, jam::AudioModel, etc etc so the real questions who own, and responsible to its own command? that's where the commandId belong."*
- *"the objective of registrable direct lookup dispatch is exactly to remove entirely scattered if-else branching anywhere. ALWAYS one entry point, always SSOT, downstream deterministically disciplined DESIGN BY CONTRACT."*
- *"hjkl for navigation for example, this is TextEditor built in core feature CONTRACT. registered by default by its commandId"*; *"could be overriden by config, when the orchestrator TELL to register from config, disk always win. widget doesnt care nor acknowledge about config on disk."*; *"END only tell EVE, EVE tell its TextEditor to enter NORMAL mode, VISUAL mode, etc. TextEditor accept the state, and it STILL HANDLE its own hjlk per its state. at TERMINAL mode, it's typing, at NORMAL it navigate, at VISUAL it navigate and select etc"*; *"these all are PURE EVENT DRIVEN"*.
- *"when EVE (with its TextEditor) hosted in END, it SHOULD NOT register any keyboard shortcut for any state-command, BUT we KEPT the state machine"*; *"END as host always win"*; *"END keyboard "handling" per instance must be CONTEXT SENSITIVE to be forwarded to whichever PANE FOCUSED"*.
- Key defaults: *"jam CommandId table column"*. Table name: *"jam: TextEditorCommandId"*. Hosted overrides: *"Yes, in every host"*.
- *"EVE could have `mode' AudioParameterInt, when that param changed it told its TextEditor to change mode, so ENAX host-client orchestration actually can be wired through APVTS, NOT by our handroll methods"*; *"editor mode is not terminal state. terminal state is VT spec state machine, cursor, command, etc Editor is widget, not "terminal""*; *"then use attachment?"*; *"zoom and mode are both attachable host automatable parameter"*.
- *"if it was jam wiget parameter then it must be at jam. … if the parameter exclusive to project then it must be at project (this is exactly like JFS parameters.md)"*
- *"end's keys.md should only exposes command name. parameter, commandId, etc are internal implementation, probably could be exposed for API later, but never to config"*; END rows: *"Row sets a parameter"*; lookup: *"By parameter name"*.
- Zoom: *"so END's zoom command is actually only tell children to set the parameter to "zoom". this later could be config driven at END level."*; *"how about EXCLUSIVE_ZOOM? default false, means all. true means focused only"*; *"Normalised step in END"*; *"normalisation should not be handrolled. all API already exists OOTB"*; *"parameters range could be any arbitrary number while you still can get the normalised value"*.
- Copy and paste: *"Yes, in Plan 2"* (option text: macOS `cmd+c` / `cmd+v`; Windows `ctrl+shift+c` / `ctrl+shift+v` so that `ctrl+c` stays `\x03`; paste wrapped in bracketed-paste markers when DEC 2004 is set).

## Language / Framework Constraints

- LANGUAGE.md C++/JUCE: one object, one TU.
- CODING: no `==` / `!=` on strings; no out-parameters; no bail-out guards; brace initialisation; alternative tokens.
- CODING Header Discipline: jam submodule files include nothing.
- Layering (MANIFESTO E): `jam_gui` never names `jam_plugin_bootstrap`. END does not link `jam_plugin_bootstrap` (`end/carol/SPRINT-LOG.md:142`).

## Dependency & API Inventory

- **CommandManager.** `jam_gui/commands/jam_CommandManager.h` / `.cpp` (read in full). `setKeyPresses` stores the tree, clears every key press, and adds a key press for each child longer than `chordLength` (`.cpp:5-23`; clear at `:9-10`). `invokeKey` resolves two-character chords against `Id::pendingChord` (`:25-50`). `isInMode` compares the row's mode text with `mode` (`:52-55`). `getCommandForName` (`:57-67`).
- **One manager per binary.** `SharedInstance<CommandManager>` joins the live object (`jam_SharedInstance.h:49-57`). Holders: `jam_PluginEditor.h:230`, `jam_TextEditor.h:782`, `ENDView.h:69`. Each binary has its own JUCE and jam (`end/carol/SPRINT-LOG.md:119`).
- **Key paths.** END: `ENDView.cpp:69-72` (key mappings, then `invokeKey`). TextEditor: `keyPressed` dispatches by mode (`jam_TextEditor.cpp:726-729`). Normal and visual resolve as commands (`:506-522`). Terminal mode resolves chords, then sends the rest to `onKeyPress` (`:524-544`).
- **Perform by name.** `ENDView.cpp:116-119` → `actions.run`. `jam_TextEditor.cpp:672-710` → `getMotions()` / `getActions()`. `jam_PluginEditor.cpp:90-99` → `registry->callbacks`.
- **Binders today (scattered).** `PluginEditor::addCommands` (`jam_PluginEditor.cpp:256-265`) and its reload (`:366`). `TextEditor` binds again: `mode.referTo` + `addKeyListener` (`jam_TextEditor.cpp:107-108`), `removeKeyListener` (`:113`), `setKeyPresses` from `StyleManager` (`:590-593`), called from `bindToModel` (`:150`). END: `ENDView.cpp:17-18`, `:86`.
- **Tables today.**
  - EVE `## CommandId` (`eve/cast/identifiers.md:51-88`): `zoomIn zoomOut zoomReset settings left down up right wordForward wordBackward wordEnd lineStart lineEnd documentStart documentEnd halfPageUp halfPageDown clear normal visualCharacter visualLine visualBlock yank terminal preview`.
  - EVE keys (`eve/Source/layout/ViewLayout.md:35-155`), with a `mode` column (`:44`).
  - END `## CommandId` (`end/cast/bimaps.md:56-98`). END keys (`end/Source/config/keys.md`), with no `mode` column.
- **EditorMode.** `jam/cast/gui.md:218-233`: `normal 1`, `visual 2`, `terminal 3`. `VisualMode` is 0-based (`:235-249`). `Id::mode` (`jam/cast/core.md:106`). The TextEditor node stores the mode name: `:102`, `:143-144`, `:157`, read at `:587`.
- **Owner-scoped id precedent.** `jam/cast/style.md:66` `## ColourId` holds JUCE and jam widget ids, with `noneColourId 0x0` (`:80`). EVE has no ColourId table. The project supplies values in CSS (`eve/Source/textEditor.css:264`).
- **Cast value table.** A `value` section with one row per index, `## c4Stereotypes` (`jam/cast/mermaid.md:2437-2457`), generates `@code:static-value-lookup-table` (`jam/cast/spell.md:2711-2721`) with alias type `jam::LookupTable<int, const char*, N>` (`:133`).
- **Attachment.** `jam::Model::ValueAttachment : juce::ParameterAttachment` binds a parameter to `Model::Object::getValueObject()` in both directions (`jam_Model.h:284-354`, `:54-71`). KANJUT's identical shape is used by JFS: `registerAttachment<kuassa::ImageComponent, kuassa::Model::ValueAttachment>` (`jreng-filter-strip/Source/FilterStripViewRegistration.cpp:206`).
- **Parameter layout.** Types `floatingPoint`, `choice`, `boolean` (`jam_ParameterLayout.cpp:84-148`). The host-visible name is `getParameterName`: for group `master`, `Format::correctKeywordCase (Format::toTitleCase (name))` (`:26-31`). The id is `Format::toScreamingSnakeCase (id)` (`:34-37`).
- **EVE tables.** `eve/Source/layout/parameters.md`: `## parameter` `zoom` (`:11`), `bypass` (`:13`); `## model` (`:16-46`), including `bracketedPaste` (`:23`). `component.md:6-8`: `| 0 | textEditor | winsize | … | terminalInput |`.
- **Zoom chain.** `applyZoom` (`jam_PluginEditor.cpp:101-138`). The `ZOOM` registry callback → `resized()` (`:217-221`, `:344-382`). The view transform reflows (`jam_ViewEditor.cpp:96`, `jam_PluginEditor.cpp:49-61`).
- **END zoom today.** `ENDActions.cpp:137-185` writes the focused pane's `Id::zoom` (created at `EditorView.cpp:7`, no reader), clamped by `EditorView.h:26-28`. `zoomStep` 0.1 (`end/Source/config/display.md:22`).
- **Host parameter API.** `getValue` / `setValue` are normalised 0 to 1 (`juce_AudioProcessorParameter.h:105-129`). `setValueNotifyingHost`, `beginChangeGesture`, `endChangeGesture` (`:141-156`). `getDefaultValue` (`:159`). `getName` (`:164`). Hosted ids are format-specific (`juce_VSTPluginFormatImpl.h:979-981`, `juce_VST3PluginFormatImpl.h:2106-2108`, `juce_AudioUnitPluginFormatImpl.h:758-760`). VST2 `getValueForText` parses a number only (`juce_VSTPluginFormatImpl.h:901-910`, `juce_AudioPluginInstance.cpp:232-247`).
- **Hosted plugin access.** `session.getPlugin (uuid)` (`EditorView.cpp:65`). Focused pane: `Id::focusedPane` (`ENDActions.cpp:145-146`).
- **Clipboard and keyboard.** `copySelection` → `SystemClipboard::copyTextToClipboard` (`jam_TextEditor.cpp:403-420`), reached only by `yank` (`:466-477`). No paste exists. `cmd+c` / `cmd+v` give no bytes (`jam_TerminalKeyboard.cpp:29`, `:33`, `:200`). `ctrl+c` gives `\x03` (`:75-77`). On Windows, `commandModifier` = `ctrlModifier` (`juce_ModifierKeys.h:165-166`). PTY write path: `EVEViewComponents.cpp:38-47`.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

## Steps

### Step 1: Trace the key path (whatdbg)
**Scope:** COUNSELOR's trace spec in the scratchpad; END Debug and EVE Debug, built by ARCHITECT (both Debug: the pairing changes `juce::Component` layout through `JUCE_CHECK_MEMORY_LEAKS`).
**Action:** breakpoints at `ENDView::keyPressed` (`ENDView.cpp:69-72`), `CommandManager::invokeKey` (`jam_CommandManager.cpp:25-50`) and `TextEditor::keyPressed` (`jam_TextEditor.cpp:726-729`). With EVE focused, ARCHITECT types `` `\ ``, `` `] ``, visual `y` and ESC. @Pathfinder runs the `trace` driver.
**Validation:** every key fix in the later steps cites a trace stop. The candidate from the code (END sees a key twice: `performKeyEquivalent:` on END's peer, then the hook) is confirmed or rejected by the stops.

### Step 2: EditorMode is a key, not a string
**Scope:** `jam/cast/gui.md:227-233`; `jam_TextEditor.cpp` mode sites; `jam_CommandManager.cpp:52-55`.
**Action:**
1. EditorMode keys become 0-based: `normal 0`, `visual 1`, `terminal 2` (sibling `VisualMode`, `gui.md:243-249`). The first row stays the default.
2. The TextEditor node `Id::mode` holds the key, not the name (`jam_TextEditor.cpp:102`, `:143-144`, `:157`, `:587`).
3. `CommandManager::isInMode` maps the row's mode name to its key through `map::EditorMode`, then compares keys. An empty row mode applies in every mode.
**Validation:** no string comparison on mode (CODING). Every mode reader and writer uses the key.

### Step 3: jam command tables, scoped to the state owner
**Scope:** `jam/cast/gui.md` (after `## VisualMode`); `jam/cast/plugin_bootstrap.md`; `eve/cast/identifiers.md:51-88`; `eve/Source/layout/ViewLayout.md:35-155`. ARCHITECT regenerates.
**Action:**
1. `## TextEditorCommandId` (name | key | value; `map::TextEditorCommandId`). Key 0 is `none` (sibling `noneColourId`, `style.md:80`). Rows: `left down up right wordForward wordBackward wordEnd lineStart lineEnd documentStart documentEnd halfPageUp halfPageDown normal visualCharacter visualLine visualBlock yank terminal copy`.
2. Value sections `textEditorKeys` and `textEditorModes`, one row per key (sibling `c4Stereotypes`, `mermaid.md:2437-2457`; alias row as `spell.md:133`; generator row as `spell.md:2711-2721`).
   - They carry today's defaults from `ViewLayout.md:58-154`. A command with more than one key lists them separated by spaces (`terminal` = `escape i a I A o O`). Its modes are listed the same way (`normal visual`).
   - `normal` = `` `] `` in `terminal`.
   - `copy` = `cmd+c`, every mode, on macOS. On Windows it is `ctrl+shift+c`, in a platform section on the sibling form `## windowFX mac` / `## windowFX windows` (`gui.md:140-167`).
3. `## PluginEditorCommandId` and `pluginEditorKeys` in `jam/cast/plugin_bootstrap.md`: `zoomIn cmd+=`, `zoomOut cmd+-`, `zoomReset cmd+0`, `settings cmd+,`. The owner is `PluginEditor` (`jam_PluginEditor.cpp:217-253`, `launchSettings`).
4. Delete those rows from `eve/cast/identifiers.md:63-87` and from `ViewLayout.md:46-154`. EVE keeps `clear` and `preview` (`identifiers.md:80`, `:87`; `ViewLayout.md:54`, `:56`) and gains `paste` (Step 9).
**Validation:** each command is in exactly one table, the table of its state owner. No `map::CommandId` name collision: jam uses `TextEditorCommandId` and `PluginEditorCommandId`. The defaults are SSOT in jam cast.

### Step 4: One binder per binary
**Scope:** `jam_TextEditor.h` / `.cpp`; `jam_PluginEditor.h` / `.cpp`; `jam_CommandManager.h` / `.cpp`; `end/Source/ui/ENDView.cpp`.
**Action:**
1. `TextEditor::getDefaultKeys()` and `PluginEditor::getDefaultKeys()` each return a keys tree in the key-sheet shape (child type = command name, `Id::value` = keys, `Id::mode` = modes), built from their generated tables.
2. The binder — `PluginEditor::addCommands` in EVE (`jam_PluginEditor.cpp:256-265`) — registers every command target, joins the default trees of every target, and overlays the config rows by command name: a config row replaces the default row of the same name. *"disk always win"*. It makes the one `setKeyPresses` call. The reload at `:366` uses the same path.
3. `setKeyPresses` splits a multi-key cell with `juce::StringArray::fromTokens` and adds every key press.
4. Delete the TextEditor's own binding: `jam_TextEditor.cpp:107-108` (keep the `mode.referTo` only if Step 6 does not replace it), `:113`, `:590-593`, and the call at `:150`.
5. END: `ENDView` stays END's one binder (`ENDView.cpp:17-18`, `:86`). END keeps its own `map::CommandId` and `keys.md`.
**Validation:** one `setKeyPresses` caller per binary. No widget reads config from disk. A config row that names an unknown command is ignored, as today (`jam_CommandManager.cpp:13-14`).

### Step 5: Hosted key rule
**Scope:** `jam_ENAXClientExtensions.cpp` (keys rule from Plan 1); the binder.
**Action:** the keys rule's END callback also removes the `normal` command's default key (`` `] ``) from the joined defaults before `setKeyPresses`. END's own `normal` command drives that mode change (Step 7). The caller's `callback` leaves the defaults as they are. Hosted EVE still applies its disk overrides.
**Validation:** with EVE in END, `` `] `` reaches END first and runs END's `normal`. Standalone EVE keeps `` `] ``.

### Step 6: Mode as an EVE host parameter, attached to the TextEditor
**Scope:** `eve/Source/layout/parameters.md`; `jam_TextEditor.h` / `.cpp`; `eve/Source/EVEViewComponents.cpp:82`.
**Action:**
1. `parameters.md` `## parameter`: a row `mode | choice | master | mode | … | default 2 | choices normal, visual, terminal`. The host name is `Mode` (`jam_ParameterLayout.cpp:26-31`). The choice index equals the EditorMode key (Step 2).
2. `jam::TextEditor` gains the base `jam::Model::Object`. `getValueObject()` returns the mode `Value` on its node.
3. `TextEditor::bindToModel` (`jam_TextEditor.cpp:139-152`) owns a `jam::Model::ValueAttachment` on `Id::toTag (Id::mode)`, and asserts that the parameter exists. The `CommandManager` `mode` Value keeps referring to the same node property.
4. Delete `textEditor->setMode (map::EditorMode::terminal)` (`EVEViewComponents.cpp:82`). The parameter default holds terminal.
**Validation:** host automation of `Mode` changes the TextEditor mode. ESC or `i` in normal mode returns to terminal and the host sees the parameter change. There is one truth: the parameter.

### Step 7: END tells the focused plugin by parameter name
**Scope:** `jam/jam_core` `jam::Format`; `jam_ParameterLayout.cpp:26-31`; `end/cast/bimaps.md`; `end/Source/config/keys.md`; END actions.
**Action:**
1. NEW `jam::Format::toParameterName (group, name)` holds the body of `jam_ParameterLayout.cpp:28-31`. `ParameterLayout::getParameterName` calls it. END calls it too; END does not link plugin_bootstrap.
2. END `map::CommandId` gains `normal` (next free key after `actionList 0x470001e`). `keys.md` gains the row `normal | string | `` `] `` | … | Enter normal mode in the focused pane.`
3. END's `normal` action, in the `registerPaneActions` family (`ENDActions.cpp:187-272`):
   1. Get the focused pane's plugin: `Id::focusedPane` (`:145-146`), then `session.getPlugin (uuid)` (`EditorView.cpp:65`).
   2. Find its parameter whose `getName` equals `Format::toParameterName (master, Id::mode)`.
   3. Set `juce::NormalisableRange<float> { map::EditorMode::normal, map::EditorMode::terminal }.convertTo0to1 (map::EditorMode::normal)` through `beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture` (`juce_AudioProcessorParameter.h:141-156`; sibling `jam_PluginEditor.cpp:136-138`).

   A plugin without that parameter is left as it is.
**Validation:** END never names a format-specific parameter id. The conversion uses JUCE's `NormalisableRange` over jam's EditorMode keys.

### Step 8: END zoom through the plugin's `zoom` parameter
**Scope:** `end/Source/action/ENDActions.cpp:137-185`; `end/Source/ui/EditorView.h:26-28`, `EditorView.cpp:7`; `end/Source/config/display.md`.
**Action:**
1. `zoomIn` / `zoomOut` find each target pane's parameter by the name `Format::toParameterName (master, Id::zoom)`. They set `juce::jlimit (0.0f, 1.0f, getValue() ± zoomStep)` through the gesture calls. `zoomReset` sets `getDefaultValue()` (`juce_AudioProcessorParameter.h:113`, `:159`).
2. Target panes: a new `display.md` row `exclusiveZoom | bool | false`. `false` targets every pane of the session; `true` targets the focused pane only.
3. `zoomStep` (`display.md:22`) keeps `0.1`. Its description says it is a normalised step of the plugin's zoom parameter.
4. Delete the pane `Id::zoom` (`EditorView.cpp:7`) and `defaultZoom`, `zoomMin`, `zoomMax` (`EditorView.h:26-28`).
**Validation:** END holds no copy of the plugin's range. Each zoom change reaches the plugin's `ZOOM` callback and transform (`jam_PluginEditor.cpp:217-221`, `:49-61`).

### Step 9: Copy and paste
**Scope:** `jam_TextEditor.cpp` (actions table); `eve/cast/identifiers.md` (`paste`); `eve/Source/layout/ViewLayout.md` (`paste` default row); EVE command callbacks.
**Action:**
1. TextEditor `copy`: the selection goes to `juce::SystemClipboard::copyTextToClipboard`. It reuses the body of `copySelection` (`jam_TextEditor.cpp:403-420`); `yank` keeps calling it.
2. EVE `paste` (EVE owns the PTY input): `juce::SystemClipboard::getTextFromClipboard` goes to the PTY through the existing write callback (`EVEViewComponents.cpp:43-44`). It is wrapped in the bracketed-paste markers when the `bracketedPaste` Model row is true (`parameters.md:23`). The default key is `cmd+v` on macOS and `ctrl+shift+v` on Windows.
**Validation:** `ctrl+c` still sends `\x03` on Windows and macOS. Paste in zsh is bracketed.

### Step 10: Docs, debts, audit, doxygen
**Scope:** `end/SPEC.md` §2 (Actions and Input) and §1.3 keys; `end/ARCHITECTURE.md`; `eve/SPEC.md` keys; `end/DEBT.md` receipts at `/log`.
**Action:**
1. Describe the command tables, the binder, the mode and zoom parameters, and END's parameter rows from the code, with file:line.
2. At `/log`: receipts for DEBT-20261025T214955 and DEBT-20261009T112447. Both are paid: END uses `jam::CommandManager` (`ENDView.h:69`), and the Action List became the action map behind `perform` (`end/carol/SPRINT-LOG.md:158`).
3. @Auditor once. Then the doxygen pass (Pathfinder, doxygen-writer skill), with COUNSELOR's validation.
**Validation:** each document line cites code file:line.

## ARCHITECT Gate

For each EVE format, set by the `files::evePlugin` row:
- `` `] `` enters normal mode in the focused EVE pane. `h j k l`, `v`, `y` and ESC work there.
- END chords work with EVE focused.
- `cmd+=`, `cmd+-` and `cmd+0` zoom every pane, or the focused pane only when `exclusiveZoom` is true.
- Zoom and mode follow host automation.
- Standalone EVE keys are unchanged.
- Copy, paste and bracketed paste work in zsh.

## BLESSED Alignment

- **S (SSOT):** one command table per state owner. One binder and one entry (`invokeDirectly`) per binary. The parameter is the one truth for mode and zoom. Defaults are in jam cast, and config overrides by name.
- **E (Explicit / Encapsulation):** no string comparison on mode. `keys.md` names commands only. The widget never reads config.
- **B:** END tells and the plugin owns its state.
- **S (Stateless):** END holds no copy of a plugin's mode, zoom or range.
- **D:** the same key gives the same result in every format, because the path is JUCE's parameter contract.

## Risks / Open Questions

None.
