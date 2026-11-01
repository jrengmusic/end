# PLAN: ENAX plugin host type — END registered as a JUCE host, one channel, registered rules

**RFC:** none — objective from ARCHITECT prompt, session 2026-11-01. Contract record: `eve/PLAN-Grand-Scheme.md` §4.3 `[AMENDED 2026-11-01 — ENAX]`.
**Date:** 2026-11-01
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.14 (jam `patch/` lane) / JAM. CAST generates `CMakeLists.txt` and `Source/generated/*`. Agents never build; ARCHITECT builds.
**Sequence:** Plan 1 of 2. `PLAN-enax-command-id.md` depends on it.

## Overview

END becomes a known JUCE host through `jam::PluginHostType`. END gives its hooks to the plugin through one export, `enax_get_extension`. The plugin side, `jam::ENAXClientExtensions`, holds every END-specific rule in one registry, selected by one `isEND()` probe. The engine, key-hook, transparency and focus rules work in VST2, VST3 and AU, one format at a time.

## Rulings (ARCHITECT, verbatim)

- *"our agreement, understanding in this session supersed all precedents rulings when conflicting"*
- *"jam::PluginHostType (derived from juce::PluginHostType). juce's is hardcoded to known types, we NEED END as one of the KNOWN type, this is the SSOT seam in order we could design an SSOT "registrable" rules for END-EVE (and future END's "in-house" plugin)."*
- *"just follow juce pattern, VERBATIM. this is not our handroll. this is juce::PluginHostType, juce native contract for host detection. we only "register" END as one of the known host"*
- *""magic strings" is an exception here because conforming to framework extension is topmost priority. at callsite, the check still could use static declared variable from cast manifest, just like other string/id type check"*
- *"as standalone, plugins that loaded into whatever known hosts other than END, EVE SPEC CONTRACT must stands as is"*
- *"the plugin(s) itself STILL not hardcoded to END … the contract formalised and enforced at RUNTIME."*
- *"these rules should not be scattered, BUT governs by ONE RULE. one method? one check? ONE SSOT? so later if we need to enforce new rule, we only need to register, not manually branching."*
- Name: *"ENAX: END Native API eXtension"*; *"Native means native to END"*; object *"ENAXClientExtensions"*; access *"Mixin + dynamic_cast"*; ids and export *"enax.* / enax export"*.
- *"ENAX by default will be using CLAP binary format (future work per grand scheme), but our development still need to be tested with ALL AUDIO PLUGIN FORMAT FIRST."*
- *"ALL formats means ONE at a time, for EVE. any valid audio format SHOULD WORK. any 3rd audio plugin with "ALL FORMAT" must be able to be loaded in END."*
- EVE format under test: *"cast files.md row"*.
- *"END is audio plugin host. through and through audio plugins host end-to-end. its default plugin is EVE with ENAX (developed with all audio plugin format, one by one), later will be formalised with CLAP"*
- Focus in this plan: *"Plan 1 (PluginHostType)"*. *"Focus is END responsibility"*. *"END as HOST should control the FOCUS. END already always control new pane created to be always the FOCUSED one … it should never manage its own focus, never steal focus."*; *"so END pane focus management actually only need to forward, focused PANE give the keyboard focus to its loaded plugin"*.
- *"NO HACK, NO WORKAROUND, NO PATCHING BULLSHIT without comprehensive understanding. must be DESIGN BY CONTRACT"*

## Language / Framework Constraints

- LANGUAGE.md C++/JUCE: one object, one TU. Non-template bodies go in the object's `.cpp`.
- CODING Header Discipline: jam submodule files include nothing. The include row goes in the module header.
- CODING `reinterpret_cast`: only the function-pointer ↔ `void*` case.
- JUCE patches follow the existing lane: `jam/patch/*.patch`, `patch` rows in `jam/user-modules-info.md:1350-1359`, applied by `jam/cast/juce.cast`. The same list is in END and EVE (`end/CMakeLists.txt:21-32`).

## Dependency & API Inventory

- **JUCE host type.** `juce::PluginHostType` is not `final` (`juce_PluginHostType.h:47`). It sets `type (getHostType())` in its ctor (`:51`), has a closed `HostType` enum (`:57-126`) and `isX()` members (`:132-220`). Detection is by host file name per OS (`.cpp:173-329`). Acronym precedent: `SADiE` (`:96`, `:190`, `.cpp:136`, `.cpp:310`). Case-sensitive prefix precedent: `hostFilename.startsWith ("Bitwig")` (`.cpp:224`).
- **jam host family to replace.** `jam_core/utilities/jam_PluginHost.h:33-96`: `isAUSandboxHost`, `isProToolsHost`, `getHostScale`, each a `static const` computed once (`:35-38`), with stubs (`:42-44`, `:64-66`, `:94-96`). Consumers: `jam_PopupMenu.h:1966`, `jam_PluginEditor.cpp:45`, `jam_ViewEditor.cpp:76`, `:93`, `jam_ViewPanel.cpp:18`, `jam_ScaledContent.cpp:7`, `jam_AudioModel.cpp:126`. `AudioModel::getHostDescription` (`jam_AudioModel.cpp:444-447`).
- **Current hand-written branch.** `jam_PluginEditorLayout.h:110-135`: three `getHostFunction` lookups, then `CpuGraphicsEngine::getOrCreate()` and three `external…` assignments; else `VulkanGraphicsEngine::getOrCreate (extent)`. The ctor takes no processor (`:110-111`). Its only known creator is `EVEProcessor.h:96-101`.
- **Processor bases.** `EVEProcessor : juce::AudioProcessor` plus `juce::AudioProcessorARAExtension` under `JucePlugin_Enable_ARA` (`EVEProcessor.h:4-8`).
- **JUCE client-extension family.** `getAAXClientExtensions`, `getVST2ClientExtensions`, `getVST3ClientExtensions`, `getARAClientExtensions` (`juce_AudioProcessor.h:1254-1284`). The default *"will attempt to dynamic-cast this AudioProcessor"* (`:1259-1262`). jam sibling: `jam::ClapClientExtensions` (`jam_clap/wrapper/jam_ClapClientExtensions.h:10-33`).
- **Registry pattern.** `jam::Function::Map` (`jam_Function.h:212-344`). Owner-table precedent: `CpuGraphicsEngine::imageOperations` (`jam_CpuGraphicsEngine.h:122-126`).
- **END exports to replace.** `end/Source/Main.cpp:29-63`: `jam_host_context_factory`, `jam_host_peer_deleted_callback`, `jam_host_key_pressed_callback`. The key lambda is at `:53-62`. The `/** @cond */` block form is at `:29`, `:39`.
- **END extension registry.** `Nexus::extensions`, a `jam::HashMap<juce::String, const void*>` (`Nexus.h:30`). It is passed to `jam::ClapPluginFormat` (`Nexus.cpp:14-19`). Absent-id semantics: `jam_ClapPluginInstance.cpp:408-420`.
- **Transparency.** `jam/patch/juce-plugin-transparency-hook.patch` (read in full):
  - It adds `ComponentPeer::getHostFunction` (header hunk, plus a `.cpp` body using `GetProcAddress (GetModuleHandleW (nullptr), …)` on Windows and `DynamicLibrary host; host.open ({}); host.getFunction (…)` elsewhere).
  - It makes the VST3 `ContentWrapperComponent` `setOpaque (ComponentPeer::getHostFunction ("jam_host_context_factory") == nullptr)` and fills only when opaque.
  - It makes the host VST3 and AU windows and `NSViewComponentWithParent` non-opaque.
  - The VST2 client wrapper is not covered: `editor.setOpaque (true)` (`juce_audio_plugin_client_VST2.cpp:931`), `setOpaque (true)` (`:942`), `fillAll (Colours::black)` (`:956-959`). The JUCE host query is `detail::PluginUtilities::getHostType()` (used at `:938`).
- **Focus.**
  - The VST3 host window forwards focus: `focusGained → view->onFocus (true)` (`juce_VST3PluginFormat.cpp:308-309`).
  - The VST2 host window (`juce_VSTPluginFormat.cpp`, 828 lines) and the AU host window (`juce_AudioUnitPluginFormat.mm`, 290 lines) have no `focusGained`, `grabKeyboardFocus` or `makeFirstResponder` outside the Linux XEmbed blocks (full reads).
  - EVE grabs focus itself: `EVEView.cpp:12`. It forwards gained focus to its default child: `EVEView.cpp:15-22`.
  - END pane focus: `OwnedComponent::focusGained` writes `Id::focus` 1 (`jam_OwnedComponent.h:71-75`), and `focusLost` writes 0 (`:77-81`). The owner records the focused child only on 1 (`jam_OwnerComponent.h:174-178`). A new pane grabs focus (`jam_MatrixComponent.cpp:358-359`). `EditorView` has no focus code (`EditorView.cpp:1-82`).
- **AU in-process registration** (design from the retired plan, `PLAN-eve-shared-engine.md:92-94`): `CFBundleCreate`, `CFBundleGetFunctionPointerForName` with the plist `factoryFunction`, then `AudioComponentRegister`. EVE's plist key: `factoryFunction` = `EVEAUFactory` (`eve/Builds/Debug/EVE_artefacts/Debug/AU/EVE.component/Contents/Info.plist:35-36`). JUCE matches a `.component` directory (`juce_AudioUnitPluginFormatHeadless.mm:153-165`).
- **Formats END hosts.** `JUCE_PLUGINHOST_VST=1`, `JUCE_PLUGINHOST_VST3=1`, and `JUCE_PLUGINHOST_AU=1` on macOS (`project-info.md:371-375`). Plus `juce::addDefaultFormatsToManager` (`Nexus.cpp:13`).
- **jam cast identifiers.** The `@string` and `@id` row shape is at `jam/cast/core.md:98-111`.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

## Steps

### Step 1: Register END as a known JUCE host (patch lane)
**Scope:** NEW `jam/patch/juce-plugin-host-type-hook.patch`; `jam/user-modules-info.md` (one `patch` row after `juce-key-pressed-hook.patch`, `:1359`).
**Action:**
1. Make the patch the same way as `juce-plugin-transparency-hook.patch`. Use scratch copies of a tree that has all current patches applied, `diff -u` with `a/` and `b/` labels, and check with `patch --dry-run -p1`. Keep the line endings of each file.
2. `juce_PluginHostType.h`: add `END, /**< Represents END. */` after `DigitalPerformer` (`:82`). Add `/** Returns true if the host is END. */ bool isEND() const noexcept { return type == END; }` after `isDigitalPerformer()` (`:158`).
3. `juce_PluginHostType.cpp`: add `case END: return "END";` after `DigitalPerformer` (`:122`). In each OS block, add `if (hostFilename.startsWith ("END")) return END;` before the `UnknownHost` fall-through.
**Validation:** the dry run applies with no fuzz. The enum value, `isEND`, the description and the detection rows match JUCE's shape at the cited lines. The row padding matches `user-modules-info.md:1358`.

### Step 2: `jam::PluginHostType` — one detection SSOT (delete first)
**Scope:** delete `jam/jam_core/utilities/jam_PluginHost.h` and its include row; NEW `jam/jam_core/utilities/jam_PluginHostType.h` + `.cpp` and their rows in `jam_core.h` / `jam_core.cpp`; every consumer in the inventory; `jam_AudioModel.cpp:444-447`.
**Action:**
1. Delete `jam_PluginHost.h` and its include row. The compiler lists the consumers.
2. `class PluginHostType : public juce::PluginHostType`, under `JUCE_MODULE_AVAILABLE_juce_audio_processors`. Keep the stub behaviour for builds without that module (`jam_PluginHost.h:42-44`, `:64-66`, `:94-96`).
3. Members keep their names: `isAUSandboxHost()`, `isProToolsHost()`, `getHostScale()`. Each answer is computed once per process (`static const`, `jam_PluginHost.h:35-38`). The class does detection only.
4. Rewrite each consumer to the members. `AudioModel::getHostDescription` uses `jam::PluginHostType`.
**Validation:** no free host function remains. No other `juce::PluginHostType` is constructed in jam, EVE or END. One object, one TU. No include in a submodule file.

### Step 3: ENAX ids (cast)
**Scope:** `jam/cast/core.md` `## identifiers`. ARCHITECT regenerates.
**Action:** add three `@string` rows in the sibling row format (`core.md:98-111`):
- `enaxContextFactory` = `enax.context-factory`
- `enaxPeerDeleted` = `enax.peer-deleted`
- `enaxKeyPressed` = `enax.key-pressed`
**Validation:** the rows match the sibling format. The values follow the CLAP id shape `clap.<name>`.

### Step 4: END — one export over `Nexus::extensions`
**Scope:** `end/Source/Main.cpp`; `end/Source/plugins/Nexus.h`, `Nexus.cpp`.
**Action:**
1. Delete the three exports, their declarations and their definitions (`Main.cpp:29-63`).
2. The `Nexus` ctor adds three entries to `extensions` (`Nexus.h:30`), keyed by the Step 3 ids:
   - the address of `juce::ComponentPeer::externalContextFactory`;
   - the address of `juce::ComponentPeer::externalPeerDeletedCallback`;
   - the address of a static key-callback variable whose value is today's lambda (`Main.cpp:53-62`).

   The variables have stable addresses at `Nexus` construction. The plugin therefore reads the engine that `ENDApplication::initialiseVulkan` creates later at use time.
3. One `extern "C"` export, `enax_get_extension (const char* id)`. It returns the `extensions` entry, or nullptr when the id is absent (`jam_ClapPluginInstance.cpp:408-420` semantics). Use the per-OS export attribute inside the `/** @cond */` block form (`Main.cpp:29`, `:39`).
**Validation:** END has one export. No inline id string is used; the ids come from Step 3. CLAP hosting still receives `extensions` (`Nexus.cpp:14-19`).

### Step 5: `jam::ENAXClientExtensions` — mixin, rule registry, channel
**Scope:** NEW `jam/jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.h` + `.cpp`; include rows in `jam_plugin_bootstrap.h` / `.cpp`; `eve/Source/EVEProcessor.h` (base list).
**Action:**
1. The object holds `jam::Function::Map<juce::Identifier, void> rules` (`jam_Function.h:212-344`; owner-table precedent `jam_CpuGraphicsEngine.h:122-126`).
2. Its constructor calls `jam::PluginHostType().isEND()` once. Only when the answer is true does it add the END arm of each rule (Step 6). This is the one probe.
3. `apply (const juce::Identifier& rule, …ownArm)` runs the registered END arm for `rule`, or else `ownArm`. This is the one dispatch site.
4. `getExtension (juce::StringRef id) const` looks up `enax_get_extension` in the host executable and calls it with `id`. The body moves from the transparency patch's `getHostFunction`:
   - posix: `juce::DynamicLibrary`, `open ({})`, then `getFunction`;
   - Windows: `GetProcAddress (GetModuleHandleW (nullptr), …)`.

   It asserts `isEND()`, because the identity says END, so the export must exist.
5. `EVEProcessor` gains the base `jam::ENAXClientExtensions`, beside `juce::AudioProcessorARAExtension` (`EVEProcessor.h:6-8`).
6. Editor-side consumers reach it with `dynamic_cast<const jam::ENAXClientExtensions*> (getAudioProcessor())` and assert non-null. The contract: every plugin_bootstrap plugin is an ENAX client.
**Validation:** jam calls `isEND()` in one place. No `jam_host_*` name remains. One object, one TU.

### Step 6: Engine and key-hook rules
**Scope:** `jam/jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h` / `.cpp`; `jam_ENAXClientExtensions.cpp`; `eve/Source/EVEProcessor.h:96-101`.
**Action:**
1. Read every `PluginEditorLayout::create` caller first (LSP `findReferences`; known: `EVEProcessor.h:96-97`).
2. `PluginEditorLayout::create` gains `const jam::ENAXClientExtensions&`. `EVEProcessor` passes `*this`. The base is constructed before the `layout` member.
3. Replace `jam_PluginEditorLayout.h:118-133` with:
   - `CpuGraphicsEngine::getOrCreate()` (every host);
   - `extensions.apply (Id::engine, ownArm)`, where `ownArm` is today's `VulkanGraphicsEngine::getOrCreate (extent)` (`:131-132`). The END arm sets `externalContextFactory` and `externalPeerDeletedCallback` from `getExtension (Id::enaxContextFactory)` and `getExtension (Id::enaxPeerDeleted)`;
   - `extensions.apply (Id::keys, ownArm)`, where `ownArm` does nothing. The END arm sets `externalKeyPressedCallback` from `getExtension (Id::enaxKeyPressed)`.
4. Rule ids: `Id::keys` exists (`jam/generated/jam_Gui.cpp:39`). Read the jam cast tables for `engine`; add an `@id` row in `jam/cast/plugin_bootstrap.md` only if it is absent.
**Validation:** `jam_PluginEditorLayout.h` has no host branch. The END-arm casts are the CODING function-pointer case only. Standalone EVE still creates its own `VulkanGraphicsEngine`.

### Step 7: Transparency through JUCE's own host query
**Scope:** `jam/patch/juce-plugin-transparency-hook.patch` (regenerated); `juce_audio_plugin_client_AU_1.mm` (read first).
**Action:**
1. The VST3 `ContentWrapperComponent` uses `setOpaque (! detail::PluginUtilities::getHostType().isEND())`. This replaces the `getHostFunction (…) == nullptr` check.
2. The VST2 `EditorCompWrapper` follows the same rule: `editor.setOpaque` and `setOpaque` are true only when the host is not END (`:931`, `:942`). `paint` fills only when opaque (`:956-959`).
3. Read the AU client editor wrapper in `juce_audio_plugin_client_AU_1.mm`. Apply the same rule wherever it sets opaque or fills.
4. Delete `ComponentPeer::getHostFunction`, both the header hunk and the `.cpp` hunk.
**Validation:** the dry run applies with no fuzz. `getHostFunction` is absent in jam, EVE and END. In a non-END host the wrappers stay opaque, as JUCE ships them.

### Step 8: Focus — END forwards, the plugin never grabs
**Scope:** `eve/Source/EVEView.cpp`; `end/Source/ui/EditorView.h` / `.cpp`; a jam patch hunk for `juce_VSTPluginFormat.cpp` and `juce_AudioUnitPluginFormat.mm`; `jam/jam_plugin_bootstrap/standalone/jam_AudioStandaloneApp.cpp` (read first).
**Action:**
1. Delete `juce::MessageManager::callAsync ([this] { grabKeyboardFocus(); });` (`EVEView.cpp:12`). `EVEView::focusGained` (`:15-22`) stays: it passes gained focus to the default child.
2. END `EditorView::valueTreePropertyChanged` (`EditorView.cpp:41-45`): when its row's `Id::focus` becomes 1, tell `editor->grabKeyboardFocus()`.
3. JUCE patch: the VST2 `VSTPluginWindow` and the AU `AudioUnitPluginWindowCocoa` want keyboard focus. Their `focusGained` makes the embedded plugin view the native first responder (the VST3 sibling is `juce_VST3PluginFormat.cpp:308-309`).
4. Standalone: read the `MainWindow` of `jam_AudioStandaloneApp.cpp`. The window gives the editor focus when it is shown (`juce_TopLevelWindow.cpp:51`).
5. END keeps `focusedPane` while the plugin holds native focus (`jam_OwnerComponent.h:174-178`). Read the pane-outline source (`jam_PaneComponent.h`, `StyleCustom::drawPaneOutline`). If it reads `Id::focus`, align it to the focused-pane aggregate.
**Validation:** EVE Source has no `grabKeyboardFocus`. Focus reaches the terminal only through the host. END's `focusedPane` does not change while the user types in EVE.

### Step 9: AU in-process registration
**Scope:** `end/Source/plugins/Nexus.h` / `.cpp`.
**Action:** a macOS-only `Nexus::registerAudioUnit (const juce::File&)`:
1. `CFBundleCreate` on the `.component`.
2. `CFBundleGetFunctionPointerForName` with the plist `factoryFunction` (`EVEAUFactory`, `Info.plist:35-36`).
3. `AudioComponentRegister`.

`offloadPlugins` calls it for each `.component` it writes. The white AU host-window fill is already removed by the transparency patch's AU hunk.
**Validation:** with `files::evePlugin` set to `EVE.component`, END finds EVE without a system plug-in folder (ARCHITECT: *"i dont want to put "END's in-house plugin" into system audio plugins path"*).

### Step 10: Docs, audit, doxygen
**Scope:** `end/SPEC.md` §0 (`:5`, `:12`), §1.0 (`:49`), §1.5 (`:151-160`), §1.6; `end/ARCHITECTURE.md` host sections.
**Action:**
1. SPEC: END is an audio plugin host end to end. EVE is its default plugin, with ENAX. Formats are checked one at a time. ENAX is formalised later with CLAP. Delete `Nexus::VirtualClock` (`:57-59`, `:157`) and the CLAP-only text.
2. ARCHITECTURE: describe `jam::PluginHostType`, `jam::ENAXClientExtensions`, the export, the ids and the rules from the code, with file:line.
3. @Auditor once. Then the doxygen pass (Pathfinder, doxygen-writer skill), with COUNSELOR's validation.
**Validation:** each document line cites code file:line.

## ARCHITECT Gate

Set `files::evePlugin` (`end/cast/files.md:31`) to `EVE.vst`, `EVE.vst3` and `EVE.component` in turn. Regenerate and build each time. For each format:
- Every new pane loads EVE and renders it transparent.
- Focus follows the focused pane. Typing reaches the shell.
- END keys work with EVE focused.
- `seq 1 100000000`: the hosted median is at or below the standalone median. The baseline is 53.668 s (`jam/carol/SPRINT-LOG.md:101`).
- A whatdbg run log records how MoltenVK's `MVKBlockObserver` behaves with two copies in END's process.

## BLESSED Alignment

- **B:** END owns focus and the GPU device. EVE owns its CPU engine always, and its Vulkan engine only outside END.
- **L:** three exports, three symbol lookups and two symbol checks become one export, one probe and one dispatch site.
- **E:** host identity comes from the framework's own API. Hooks cross one explicit channel. Ids come from cast.
- **S (SSOT):** detection is in `jam::PluginHostType`. Rules are in one `Function::Map`. Host extensions are in `Nexus::extensions`, shared with CLAP.
- **S (Stateless):** EVE never grabs or tracks focus. END tells.
- **D:** no timing-dependent `callAsync` grab. Each format follows the same rules.

## Risks / Open Questions

None.
