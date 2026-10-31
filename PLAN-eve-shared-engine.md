# PLAN: EVE hosted at END on the shared engine

**RFC:** none. The objective comes from the ARCHITECT discussion of 2026-10-31 (handoff in `carol/SPRINT-LOG.md`).
**Date:** 2026-10-31
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.14 (jam `patch/` lane) / JAM.
**Sequence:** This is sprint N+2. It depends on `jam/PLAN-cpu-graphics-engine.md` (sprint N+1). It continues the outstanding steps of `PLAN-eve-hosted-at-end.md`.

## Overview

END hosts EVE, and EVE paints through END's `VulkanGraphicsEngine` (one device). The EVE binary still carries its own engine for standalone and for third-party hosts. END owns the keys first. EVE is transparent only inside END. This plan also finishes the outstanding steps of `PLAN-eve-hosted-at-end.md`.

## Rulings (ARCHITECT, verbatim)

- *"EVE as an actual audio plugins (all format) and standalone must work as is, carry its own vulkan contract"*
- *"the vulkan creation and usage dictated by WHICH HOST. so this is init runtime check. if the host is END, then it should never create Vulkan engine at all. but using END (host) engine."*
- *"shared juce? shared vulkan with scattered dynamic lib? FUCK NO"*
- Channel: *"C-ABI symbol lookup"*
- Keys: *"Host first"*
- Transparency: *"Only in END"*
- Scope: *"this sprints outstanding works must be splitted into 2 exectutable PLAN. next sprint is CpuGrahicsEngine (and VulkanGraphicsEngine). its next sprint is continuation of this sprint outstandings that blocked by that, with dedicated PLAN."*
- From the current plan, still in force:
  - *"END load EVE as default is SPEC contract. not user configurable"*
  - *"i dont want to put "END's in-house plugin" into system audio plugins path"*
  - *"Ratify all"* (`Source/plugins/`, `Source/ui/`, layout glob `plugins`)

## Facts (read 2026-10-31)

- **No plugin format defines GPU device sharing** (CLAP `jam_clap/clap/ext/gui.h:102-207`; VST3 `IPlugView::attached`; AU view).
- **Vulkan cannot take in another user's device.** No Metal struct extends `VkDeviceCreateInfo` (`Vulkan/macOS/share/vulkan/registry/vk.xml:9713-9777`).
- **Painting crosses binaries through virtual calls.** `juce::LowLevelGraphicsContext` is all pure virtual (`juce_LowLevelGraphicsContext.h:54-77`). EVE's own copy of `juce::ComponentPeer::externalContextFactory` (`juce_ComponentPeer.h:493`, jam patch `juce-vulkan-engine-hook.patch:288-295`) sends EVE's paint into whichever factory is set.
- **jam already has the symbol-lookup pattern on both OSes:**
  - macOS: `dlsym (RTLD_DEFAULT, …)` (`jam_gui/desktop/native/jam_BackgroundBlur_mac.mm:64-66`)
  - Windows: `GetModuleHandleW` + `GetProcAddress` (`jam_core/utilities/jam_Platform.h:60-67`)
- **Keys today are plugin-first.**
  - The focused plugin view gets the key first. On macOS an unused key goes up the responder chain to END (`juce_NSViewComponentPeer_mac.mm:2306-2317`). On Windows it is posted to the parent HWND (`juce_Windowing_windows.cpp:3935-3941`, `:3179-3183`).
  - VST3 specifies host-first: "A view implementation must not handle keyboard events by the means of platform callbacks, but let the host pass them to the view" (`iplugview.h:126-130`). JUCE's host never calls `onKeyDown`, and `VST3PluginWindow::keyPressed` returns `true` (`juce_VST3PluginFormat.cpp:311-315`).
  - EVE has `editorWantsKeyboardFocus` `FALSE` (`eve/project-info.md:110`). On macOS the VST3 wrapper therefore adds `windowIgnoresKeyPresses` (`juce_VSTWindowUtilities.h:56-59`).
- **Where the black comes from.** The plugin side is `ContentWrapperComponent`: `setOpaque (true)` (`juce_audio_plugin_client_VST3.cpp:2285`) and `fillAll (Colours::black)` (`:2336-2339`). The host side is `VST3PluginWindow`: `setOpaque (true)` (`juce_VST3PluginFormat.cpp:250`) and fill black (`:298-301`). The AU host fills white (`juce_AudioUnitPluginFormat.mm:68`, `:102-105`). The AU wrapper has no fill anywhere in `juce_audio_plugin_client_AU_1.mm`.
- **Duplicate Objective-C classes appear at run time:** `MVKBlockObserver` (MoltenVK), `MenuTarget` (`jam_gui/menu/jam_Menu.mm:4`), `JAMSheetContentView` and `JAMSheetDelegate` (`jam_gui/windows/native/jam_ModalSheet_mac.mm:12`, `:27`). The source is the whatdbg run of 2026-10-31. JUCE's `ObjCClass` gives each binary a random class name (`juce_ObjCHelpers_mac.h:313-317`, `:386-389`).
- **EVE's CPU-lane code keeps its own `CpuGraphicsEngine`** after sprint N+1, for layout and metrics. Only the paint goes to END's factory. Glyph identity crosses the boundary in `CpuLowLevelGraphicsContext`: `Typeface::getInstance()->registerSystemTypeface (this->getFont().getTypefacePtr())` (today `jam_LowLevelGraphicsGlyphRenderer.h:247`).

## Validation Gate

COUNSELOR validates each step against MANIFESTO.md, NAMES.md, CODING.md and the rulings. @Auditor runs once, at the end. ARCHITECT builds and tests at each gate.

## Steps

### Phase A — open defects from sprint N

**Step 1: Split crash.** ARCHITECT reported "spliiting crash" with END Debug hosting EVE VST3.
1. COUNSELOR writes a `trace` spec: launch END Debug, empty breakpoints, `stackDepth` 32, `timeoutMs` 300000.
2. Pathfinder runs it while ARCHITECT splits a pane.
3. COUNSELOR reads the stop frames, then fixes the cause found there.

**Step 2: jam Objective-C classes get unique runtime names.**
- `MenuTarget`, `JAMSheetContentView` and `JAMSheetDelegate` become `juce::ObjCClass<…>` subclasses, the framework API for this (`juce_ObjCHelpers_mac.h:313-317`).
- Scope: `jam_Menu.mm`, `jam_ModalSheet_mac.mm`.
- Validation: the whatdbg run log shows no `implemented in both` line for jam classes.

### Phase B — END's engine shared through the C-ABI symbol

**Step 3: END exports its context factory.**
- END defines one `extern "C"` function, `jam_host_context_factory` (ARCHITECT: *"jam_host_context_factory"*). It returns END's `VulkanGraphicsEngine::createContext` as a plain function pointer.
- Exports: default visibility on macOS; `__declspec(dllexport)` on the executable on Windows.
- Delete `jam::ClapServices` and its uses (`Nexus.h:17-18`, `:65-75`, `:167`; `jam_clap/services/jam_ClapServices.h`). No plugin side reads it, and the symbol replaces it (YAGNI).

**Step 4: EVE's jam looks up the symbol at engine creation.**
- At the point where the plugin creates its engine (`jam_PluginEditorLayout.h:115-116`), one jam call checks for the host symbol, using the existing native files.
- **Present:** set `juce::ComponentPeer::externalContextFactory` to END's factory and create no `VulkanGraphicsEngine`. Only `CpuGraphicsEngine` exists.
- **Absent:** `VulkanGraphicsEngine::getOrCreate` as today.
- Standalone and third-party hosts never find the symbol, so their path is unchanged.
- **Validation:**
  - The whatdbg run log shows no `MVKBlockObserver … implemented in both` from EVE's Vulkan use, and END holds one device.
  - EVE paints identically, and the glyph identity check (Fact above) passes. COUNSELOR reads `CpuLowLevelGraphicsContext`'s typeface registration path before Step 4.

### Phase C — keys and transparency

**Step 5: Host-first keys (VST3 contract).**
1. A Librarian (library mode) reads how a JUCE host app on macOS and Windows sees a key before a native child view (NSWindow `sendEvent:` / `performKeyEquivalent:` order; Win32 message loop and `WM_KEYDOWN` routing to the focused child HWND), with quotes.
2. COUNSELOR then writes the delegation. END resolves its `CommandManager` key mappings (`Source/ui/ENDView.cpp`) first, and only a key that END does not bind reaches EVE's view.

**Step 6: Transparent only in END.**
1. Through jam's JUCE patch lane:
   - Plugin side: `ContentWrapperComponent` is non-opaque and paints nothing when the host symbol is present. The symbol check is the same one as Step 4.
   - Host side: END's `VST3PluginWindow` is non-opaque and paints nothing.
   - Third-party hosts keep JUCE's black.
2. COUNSELOR reads the AU path for black before the delegation. The AU host window fills white, so AU needs the same host-side change.

### Phase D — outstanding steps from `PLAN-eve-hosted-at-end.md`

- **Step 7 (old 11.2): AU in-process registration.**
  - `Nexus` becomes one header and one `Source/plugins/Nexus.cpp` (CODING one object, one TU). Its macOS-only `registerAudioUnit (const juce::File&)` is ratified: `"Nexus.cpp"`, `registerAudioUnit`.
  - The steps: `CFBundleCreate`, `CFBundleGetFunctionPointerForName` with the plist `factoryFunction` (`EVEAUFactory`, `EVE.component/Contents/Info.plist:35-36`), then `AudioComponentRegister` (`AudioComponent.h:586-591`). `offloadPlugins` calls it.
- **Step 8 (old gate C):** ARCHITECT runs `seq 1 100000000` in EVE standalone and in END-hosted EVE (VST3, AU, VST2). Each hosted median must be at or below the standalone median. The baseline is 53.668 s (jam `carol/SPRINT-LOG.md:101`).
- **Steps 9-11 (old Phase D, CLAP):** EVE gets a CLAP target from the whelmed sibling. The END CLAP host contract follows `jam_clap/clap/host.h`, including `hostRequestProcess` (`jam_ClapPluginInstance.cpp:443-446`). No host clock (Grand Scheme:1334-1343). DEBT-20261025T135707 is paid here.
- **Steps 12-13 (old Phase E, daemon):** follow tmux's process model, read from `~/Documents/Poems/dev/tmux/` (ARCHITECT: *"~/Documents/Poems/dev/tmux/"*). The constraint is `ARCHITECTURE.md:722`, an in-process plugin editor.
- **Step 14 (old 17): Docs.**
  - `ARCHITECTURE.md:198-220` and `:705-800`, `SPEC.md:56-59` and `:151-160` match the code.
  - VirtualClock, ClapServices and the CLAP-only text go. The graph, offload, AU registration, shared engine and key ownership come in.

## BLESSED Alignment

- **B:** END owns the one device in the process. EVE owns its own only when it is not hosted by END.
- **E:** EVE's components never know which engine draws them. The symbol check is one jam call at engine creation.
- **S:** one device and one key resolver, END's, when END hosts EVE.
- **D:** gate C's `seq` measurement and the identical-UX check.

## Risks / Open Questions

None.
- **MoltenVK linked twice is a Step 4 validation check, not an open question.** EVE links MoltenVK (ruling: EVE carries its own Vulkan), so its static Objective-C class `MVKBlockObserver` can still appear twice in END's process.
- The MoltenVK SDK headers and the runtime guide say nothing about two copies. The whatdbg run log at Step 4 is the evidence.
