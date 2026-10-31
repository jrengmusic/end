# PLAN: END Host Contract — one host-detection SSOT, one host-extension channel, END-EVE rules

**RFC:** none — objective from ARCHITECT prompt, 2026-10-31 (session handoff in `carol/SPRINT-LOG.md`).
**Date:** 2026-10-31
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.14 (jam `patch/` lane) / JAM. CAST generates `CMakeLists.txt` and `Source/generated/*`. Agents never build; ARCHITECT builds.
**Sequence:** Continues `PLAN-eve-shared-engine.md`. That plan's Steps 1-7 and 10 are done (see handoff). This plan replaces its remaining host-rule work. Its Step 8 (gate C), Steps 9-11 (CLAP), Steps 12-13 (daemon) and Step 14 (docs) stay in that plan and run after this one.

## Overview

All host-aware detection moves into one object, `jam::PluginHostType`, derived from `juce::PluginHostType`. END is registered as a known JUCE host through the patch lane, in JUCE's own shape. END hands its hooks to EVE through one C-ABI export with id lookup. Every END-EVE rule (Vulkan engine, keys, focus, transparency) is registered under that single check, never branched by hand.

## Rulings (ARCHITECT, verbatim)

- *"consolidate all host-aware detection into our own jam::PluginHostType"*
- *"patch and derive? so downstream deterministically no scattered handroll method, but through a single SSOT"*
- *"then we could set rules for END-EVE contract, including so far vulkan, focus management, keyboard management (command manager) etc"*
- *"just follow juce pattern, VERBATIM. this is not our handroll. this is juce::PluginHostType, juce native contract for host detection. we only "register" END as one of the known host"*
- *""magic strings" is an exception here because conforming to framework extension is topmost priority. at callsite, the check still could use static declared variable from cast manifest, just like other string/id type check"*
- Hook channel: *"One export, id lookup"* (the option text: one `extern "C"` `jam_host_get_extension (id)` over `Nexus::extensions`, CLAP `get_extension` semantics; delete the 3 current exports).
- Chord collision: *"END's table governs"* (option text: hosted EVE resolves only keys END leaves unbound; EVE's `` `] `` is reached through END).
- *"END as HOST should control the FOCUS. END already always control new pane created to be always the FOCUSED one. this already tracked as END state machine. hence eve's contract as in-house default terminal plugin is when hosted at END, it should never manage its own focus, never steal focus."*
- *"these rules should not be scattered, BUT governs by ONE RULE. one method? one check? ONE SSOT? so later if we need to enforce new rule, we only need to register, not manually branching."*
- *"so END pane focus management actually only need to forward, focused PANE give the keyboard focus to its loaded plugin"*
- *"ONE flow, ONE path, ONE action, ONE SSOT, new pane = load eve."*; *"VST ONLY FIRST. no VST3, no AU. FOCUS."*
- *"NO HACK, NO WORKAROUND, NO PATCHING BULLSHIT without comprehensive understanding. must be DESIGN BY CONTRACT"*

## Language / Framework Constraints

- One object, one TU (CODING): non-template bodies in the object's `.cpp`; jam submodule files include nothing (CODING Header Discipline).
- `reinterpret_cast` only for the CODING-permitted function-pointer ↔ `void*` case.
- JUCE patches follow the existing lane: `jam/patch/*.patch`, listed as `patch` rows in `jam/user-modules-info.md:1350-1359`, applied by `jam/cast/juce.cast` (`git apply`), same list in END and EVE (`end/CMakeLists.txt:100-109`, `eve/CMakeLists.txt:125-134`).

## Dependency & API Inventory (read this session)

- **`juce::PluginHostType`** (tree `~/.cache/cast/juce/9a6d66f7…/modules/juce_audio_processors/utilities/`):
  - not `final` (`juce_PluginHostType.h:47`); `type (getHostType())` in ctor (`:51`); `HostType` closed enum (`:57-126`); `isX()` members (`:132-220`); `getHostDescription()` (`:224`, `.cpp:96-171`); `getHostPath()` public static (`:236-239`); `getHostType()` private static (`:265`), detection by host file name per OS (`.cpp:173-329`).
  - JUCE-shape precedent for an acronym product: enum `SADiE` (`:96`), `isSADiE()` (`:190`), description `"SADiE"` (`.cpp:136`), detection `hostFilename.containsIgnoreCase ("Sadie")` (`.cpp:310`). Case-sensitive prefix precedent: `hostFilename.startsWith ("Bitwig")` (`.cpp:224`).
  - JUCE consumers of the host type are the plugin wrappers through `detail::PluginUtilities::getHostType()` (`juce_PluginUtilities.h:64-66`), e.g. `resizeHostWindow` (`juce_audio_plugin_client_VST3.cpp:2392-2399`), `EditorCompWrapper` ctor (`juce_audio_plugin_client_VST2.cpp:938`).
  - Host path a plugin sees: macOS `_NSGetExecutablePath` (`juce_Files_mac.mm:238-246`); Windows `getModuleFileName (nullptr)` (`juce_Files_windows.cpp:752-753`).
- **Existing jam host-detection family** (to consolidate): `jam_core/utilities/jam_PluginHost.h` — `isAUSandboxHost()` (`:33-41`), `isProToolsHost()` (`:55-63`), `getHostScale()` (`:78-93`), stubs (`:42-44`, `:64-66`, `:94-96`); consumers `jam_PopupMenu.h:1966`, `jam_PluginEditor.cpp:45`, `jam_ViewEditor.cpp:76`, `:93`, `jam_ViewPanel.cpp:18`, `jam_ScaledContent.cpp:7`, `jam_AudioModel.cpp:126`; `AudioModel::getHostDescription()` (`jam_AudioModel.cpp:444-447`) published as `Id::daw` (`jam_ViewManager.cpp:870`).
- **Host-extension sibling (CLAP)**: contract `jam_clap/clap/host.h:20-25`; ids `"clap.<name>"` (`clap/ext/gui.h:47`); host registry `hostExtensions` (`jam_ClapPluginInstance.h:288`), `registerHostExtensions` (`.cpp:308-322`), `hostGetExtension` returning nullptr when absent (`.cpp:408-420`). END's registry: `Nexus::extensions` `jam::HashMap<juce::String, const void*>` (`Nexus.h:36`), passed to `ClapPluginFormat` (`Nexus.cpp:18-23`).
  - `jam::ClapClientExtensions` (`jam_ClapClientExtensions.h:10-33`) is dead (no writer; `ClapWrapper::clientExtensions` at `jam_ClapWrapper.h:253` never assigned) and breaks CODING (out-parameter `:23-26`; naked non-owning member `jam_ClapWrapper.h:253`). Not used by this plan.
- **Registry pattern** to follow: `jam::Function::Map` (`jam_Function.h:212-344`; `add` `:233-239`, `get` `:267-276`, `contains` `:332-335`); owner-table precedent `CpuGraphicsEngine::imageOperations` (`jam_CpuGraphicsEngine.h:122-126`, `:169-171`) filled by `VulkanGraphicsEngine` (`jam_VulkanGraphicsEngine.cpp:24-68`).
- **Current hook sites to replace**:
  - END exports `jam_host_context_factory`, `jam_host_peer_deleted_callback`, `jam_host_key_pressed_callback` (`end/Source/Main.cpp:29-63`).
  - EVE branch on three symbols (`jam_PluginEditorLayout.h:117-134`).
  - `ComponentPeer::getHostFunction` + ContentWrapperComponent check in `jam/patch/juce-plugin-transparency-hook.patch` (`:8-30`, `:49`).
  - Key hook `ComponentPeer::externalKeyPressedCallback` (`jam/patch/juce-key-pressed-hook.patch`).
  - EVE self-grab `EVEView.cpp:12`.
- **Focus facts**:
  - END focused pane aggregate updates only on `Id::focus == 1` (`jam_OwnerComponent.h:172-178`); `OwnedComponent::focusLost` writes 0 (`jam_OwnedComponent.h:77-81`), which does not clear `focusedPane`.
  - New pane gets focus in `MatrixComponent::childAdded` (`jam_MatrixComponent.cpp:353-360`).
  - Native focus primitive: `NSViewComponentPeer::grabFocus` → `[window makeFirstResponder: view]` (`juce_NSViewComponentPeer_mac.mm:1594-1605`); first responder → `viewFocusGain` → `handleFocusGain` → `component.grabKeyboardFocus()` (`:2174-2184`, `juce_ComponentPeer.cpp:362-378`) → traverser default child (`juce_Component.cpp:3006-3011`) → `EVEView::focusGained` → terminal (`EVEView.cpp:15-22`).
  - JUCE VST2 host window has no focus forwarding (`juce_VSTPluginFormat.cpp`, no `focusGained`); VST3 sibling `focusGained` → `view->onFocus (true)` (`juce_VST3PluginFormat.cpp:303-304`). Embedded parent view: `cocoaWrapper->getView()` (`juce_VSTPluginFormat.cpp:162`, `:361`).
- **Transparency facts**: VST2 client wrapper is opaque and fills black: `editor.setOpaque (true)` (`juce_audio_plugin_client_VST2.cpp:931`), `setOpaque (true)` (`:942`), `paint` `fillAll (Colours::black)` (`:956-959`). VST3 wrapper already covered by `juce-plugin-transparency-hook.patch` (`:44-61`).

## Validation Gate

COUNSELOR validates each step before the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and these locked decisions (no deviation, no scope drift). Every Engineer return is read at the reported lines (Design by Contract Filter). @Auditor runs ONCE, after the final step.

## Steps

### Step 1: Register END as a known JUCE host (patch lane)
**Scope:** NEW `jam/patch/juce-plugin-host-type-hook.patch`; `jam/user-modules-info.md` (one `patch` row after `juce-key-pressed-hook.patch`, line 1359).
**Action:** Produce the patch the same way as `juce-plugin-transparency-hook.patch` (orig/new scratch copies of the tree that already has all current patches applied, `diff -u` with `a/`/`b/` labels, `patch --dry-run -p1` verification, keep each file's line endings). JUCE shape verbatim, ARCHITECT ruling *"just follow juce pattern, VERBATIM"*, SADiE precedent:
1. `juce_PluginHostType.h`: enum value `END, /**< Represents END. */` in alphabetical position after `DigitalPerformer` (`:82`); member `/** Returns true if the host is END. */ bool isEND() const noexcept { return type == END; }` after `isDigitalPerformer()` (`:158`).
2. `juce_PluginHostType.cpp`: `case END: return "END";` after `DigitalPerformer` (`:122`); detection rows in each OS block, `if (hostFilename.startsWith ("END")) return END;`, placed before `UnknownHost` fall-through. Use the `startsWith` form (`:224` precedent) so that no other product name containing "end" matches.
**Validation:** dry-run applies with no fuzz; enum, `isX`, description, detection match JUCE shape at the cited lines; row padding matches `user-modules-info.md:1358`.

### Step 2: `jam::PluginHostType` — one host-detection SSOT (delete first)
**Scope:** NEW `jam/jam_core/utilities/jam_PluginHostType.h` + `.cpp` (replaces `jam_PluginHost.h`, deleted); `jam_core.h`/`.cpp` include rows; every consumer listed in the inventory; `jam_AudioModel.cpp:444-447`.
**Action:**
1. Delete `jam_PluginHost.h` and its include row first; the compiler lists the consumers.
2. `class PluginHostType : public juce::PluginHostType` under `JUCE_MODULE_AVAILABLE_juce_audio_processors`, keeping the existing stub behaviour for builds without that module (mirror `jam_PluginHost.h:42-44`, `:64-66`, `:94-96`).
3. Members keep the established names: `isAUSandboxHost()`, `isProToolsHost()`, `getHostScale()`; each answer is computed once per process as today (`static const`, `jam_PluginHost.h:35-38`).
4. Add `getExtension (juce::StringRef id) const` → `const void*`: one lookup of the END export `jam_host_get_extension` in the host executable (macOS/Linux `juce::DynamicLibrary` `open ({})` + `getFunction`, `juce_SharedCode_posix.h:1068-1087`; Windows `GetProcAddress (GetModuleHandleW (nullptr), …)`, `jam_Platform.h:52-67`), then the call with `id`. Asserts `isEND()` (Design by Contract: identity says END, so the export must exist).
5. Rewrite consumers to `jam::PluginHostType` members; `AudioModel::getHostDescription` uses `jam::PluginHostType`.
**Validation:** no free host function remains (`isAUSandboxHost`, `isProToolsHost`, `getHostScale` only as members); no other `juce::PluginHostType` construction in jam/EVE/END; CODING one object one TU; no includes in submodule files.

### Step 3: Extension ids table (cast, shared by both binaries)
**Scope:** one jam cast table that both END and EVE generate from (read `jam/cast/*.md` at step start; place rows in the table of the module both link — `jam_core`), regenerated by ARCHITECT.
**Action:** add `@string` rows in the CLAP id shape (`clap/ext/gui.h:47` family): `jam.context-factory`, `jam.peer-deleted`, `jam.key-pressed`, `jam.command-coalesce`. Follow the sibling row format of that table verbatim.
**Validation:** rows match the table's sibling format; names follow the CLAP family (NAMES Rule 5).

### Step 4: END — one export over `Nexus::extensions`
**Scope:** `end/Source/Main.cpp`, `end/Source/plugins/Nexus.h/.cpp`.
**Action:**
1. Delete the three exports and definitions (`Main.cpp:29-63`).
2. `Nexus` ctor registers into `extensions` (the same map CLAP plugins already read): the address of `juce::ComponentPeer::externalContextFactory`, the address of `juce::ComponentPeer::externalPeerDeletedCallback`, and the address of a static key callback variable whose value is today's lambda body (`Main.cpp:53-62`). Addresses of the variables are stable at Nexus construction, so the engine created later in `ENDApplication::initialiseVulkan` is read at use time. Keys from Step 3.
3. One `extern "C"` export `jam_host_get_extension (const char* id)` returning `extensions` lookup (nullptr when absent, `jam_ClapPluginInstance.cpp:408-420` semantics), inside the existing `/** @cond */` block form (`Main.cpp:29`, `:39`) with the per-OS export attribute.
**Validation:** one export; no inline id string (Step 3 constants); CLAP hosting unchanged (`Nexus.cpp:18-23` still passes `extensions`).

### Step 5: EVE — END rules registered under one check
**Scope:** `jam/jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h/.cpp`; `jam/patch/juce-plugin-transparency-hook.patch` (ContentWrapperComponent hunk); `jam/patch/juce-plugin-transparency-hook.patch` gains the VST2 client wrapper; delete `ComponentPeer::getHostFunction` from that patch.
**Action:**
1. `PluginEditorLayout` holds a `jam::Function::Map<juce::Identifier, void>` of END rules (registry pattern `jam_CpuGraphicsEngine.h:122-126`, `:169-171`): engine rule (create only `CpuGraphicsEngine`; set `externalContextFactory` and `externalPeerDeletedCallback` from the extensions), keys rule (set `externalKeyPressedCallback` from its extension). The constructor runs every registered rule when `jam::PluginHostType().isEND()`; otherwise it creates EVE's own `VulkanGraphicsEngine` as today (`jam_PluginEditorLayout.h:129-133`). One check, one place; a new rule is one `add`.
2. Transparency is JUCE-side and uses JUCE's own host query, no symbol lookup: ContentWrapperComponent `setOpaque (! detail::PluginUtilities::getHostType().isEND())` replacing `getHostFunction (…) == nullptr` (patch `:49`); same rule on the VST2 `EditorCompWrapper` (`juce_audio_plugin_client_VST2.cpp:931`, `:942`, `:956-959`: opaque only when not END, fill only when opaque). Delete `ComponentPeer::getHostFunction` (patch `:8-30`).
**Validation:** no `getHostFunction`, no `jam_host_*` symbol name outside `jam::PluginHostType::getExtension`; one `isEND()` check in jam bootstrap; dry-run of the regenerated patch.

### Step 6: Focus — END forwards, EVE never grabs
**Scope:** `eve/Source/EVEView.cpp`; `end/Source/ui/EditorView.h/.cpp`; NEW patch hunk for `juce_VSTPluginFormat.cpp` (VST2 host window) in a jam patch file; jam standalone host `jam/jam_plugin_bootstrap/standalone/jam_AudioStandaloneApp.cpp`; pane outline source (read `jam_PaneComponent.h` paint / `StyleCustom::drawPaneOutline` at step start).
**Action:**
1. Delete `juce::MessageManager::callAsync ([this] { grabKeyboardFocus(); });` (`EVEView.cpp:12`). It fires `jassert (isShowing() || isOnDesktop())` (`juce_Component.cpp:3033`) and is a timing guess.
2. END `EditorView`: when its pane row's `Id::focus` becomes 1 (its existing `valueTreePropertyChanged`, `EditorView.cpp:41-45`), tell the hosted editor `editor->grabKeyboardFocus()`.
3. JUCE patch, VST2 host window: `VSTPluginWindow` wants keyboard focus; its `focusGained` makes the embedded plugin view (`cocoaWrapper->getView()` subview, `juce_VSTPluginFormat.cpp:162`, `:361`) the native first responder — VST3 sibling `juce_VST3PluginFormat.cpp:303-304`.
4. END keeps its focused-pane state while the plugin holds native focus (`jam_OwnerComponent.h:172-178`). If the pane outline reads `Id::focus` instead of the focused-pane aggregate, the outline source is aligned to the aggregate (read first).
5. EVE standalone: the standalone host gives the editor focus when its window is shown (read `jam_AudioStandaloneApp.cpp` MainWindow at step start; `TopLevelWindow` takes focus itself, `juce_TopLevelWindow.cpp:51`).
**Validation:** no `grabKeyboardFocus` in EVE Source; focus reaches the terminal only through the host; END `focusedPane` unchanged while typing in EVE.

### Step 7: Keys — END's table governs (trace first)
**Scope:** `end/Source/ui/ENDView.cpp`, `jam/jam_gui/commands/jam_CommandManager.cpp`, the key rule from Step 5.
**Action:**
1. COUNSELOR writes a whatdbg `trace` spec on END Debug + EVE Debug (both Debug — Release/Debug pairing changes `juce::Component` layout through `JUCE_CHECK_MEMORY_LEAKS`, `juce_core.h:115-117`, `juce_MouseCursor.h:196`, `juce_Component.h:2669`, `:2715`) with breakpoints at `ENDView::keyPressed` (`ENDView.cpp:69-72`), `CommandManager::invokeKey` (`jam_CommandManager.cpp:25-50`) and EVE `TextEditor::keyPressed` (`jam_TextEditor.cpp:726-729`). ARCHITECT types `` `\ `` and visual-mode `y` and ESC during the run.
2. From the stops: fix why END's chord prefix fails (candidate from code: END sees the key twice — `performKeyEquivalent:` on END's peer, `juce_NSViewComponentPeer_mac.mm:2355-2394`, and the hook — so the second `invokeKey` clears the prefix, `jam_CommandManager.cpp:38-46`), and why visual `y` / ESC fail (`invokeDirectly` returns false without a target, `juce_ApplicationCommandManager.cpp:257-303`).
3. **Coalesced resolver, context-aware focus** (ARCHITECT: *"CommandManager should have "merge" or "coalesce" design. this definitely could be designed with context-aware focus."*; *"END's table governs"*). One resolver, END's `jam::CommandManager`, resolves one table: END's rows coalesced with the rows of the plugin in the focused pane.
   - **Why the plugin cannot keep its own resolver:** the key hook `bool (*) (const KeyPress&)` (`juce-key-pressed-hook.patch:8-9`) cannot hand a stored prefix back to the plugin. Two resolvers sharing the backtick prefix (END `keys.md:50-88`, EVE `ViewLayout.md:154`) each keep their own `Id::pendingChord` (`jam_CommandManager.cpp:27`, `:46`), so a chord split across them is lost.
   - **Registration on focus:** when END forwards focus (Step 6), the plugin view gains focus (`EVEView::focusGained`, `EVEView.cpp:15-22`). Under the END rule (Step 5 registry) it registers with END through one host extension (Step 3 id `jam.command-coalesce`, Step 4 registry): its chord rows (description, mode, command name — the `## keys` columns, `ViewLayout.md:44`), a perform entry `bool (*) (void* context, const char* commandName)` that runs its own `perform` by name (`jam_TextEditor.cpp:672-692` resolves by `getNameOfCommand`), and its current mode. On focus loss it unregisters. Only the focused plugin's rows are in the coalesced table at any time.
   - **Mode:** `CommandManager::isInMode` reads `mode` (`jam_CommandManager.cpp:52-55`); EVE's mode lives in the TextEditor state (`jam_TextEditor.cpp:107`, `:148`). The plugin pushes each mode change through the same extension; END's coalesced table filters the plugin rows by that mode.
   - **Resolution:** END's `invokeKey` (`jam_CommandManager.cpp:25-50`) runs over END rows plus the plugin rows. An END row wins a collision (*"END's table governs"*). A plugin row match calls the plugin's perform entry. A key that matches no row and is no prefix returns false, so the plugin's peer receives it as text (`juce-key-pressed-hook.patch:21-23`).
   - **Hosted EVE does not resolve chords itself:** the END rule (Step 5 registry) leaves EVE's `TextEditor` key path to plain text input when hosted; chord rows reach EVE only through END's perform call.
   - Names: `jam.command-coalesce` follows the CLAP extension-id family (Step 3); the perform entry and the registration function are values in END's extension registry, no new class.
**Validation:** every fix cites a trace stop; one resolver (END's `CommandManager`); EVE's `` `] ``, visual `y` and ESC resolve through END's coalesced table with EVE focused; END chords work with EVE focused.

### Step 8: Docs and audit
**Scope:** `ARCHITECTURE.md`, `SPEC.md` host sections; `PLAN-eve-shared-engine.md` Step 14 covers the rest.
**Action:** document `jam::PluginHostType`, the extension ids, the one export, the rule registry, focus forwarding. Then @Auditor once; then the doxygen pass (Pathfinder, doxygen-writer skill).
**Validation:** each document line cites code file:line.

## BLESSED Alignment

- **B:** END owns focus and the engine; EVE owns its engine only when not hosted by END.
- **L:** three exports, three lookups and two symbol checks collapse into one export, one lookup and one identity check.
- **E:** host identity through the framework's own API (`juce::PluginHostType`); hooks through one explicit channel; no inline strings (cast constants).
- **S (SSOT):** host detection in `jam::PluginHostType`; hook registry in `Nexus::extensions` (shared with CLAP); rules in one `Function::Map`.
- **S (Stateless):** EVE never tracks or grabs focus; END tells.
- **D:** no timing-dependent `callAsync` grab; focus and keys follow END's state machine.

## Risks / Open Questions

None. The chord question is resolved by ARCHITECT's coalesce ruling (Step 7.3).
