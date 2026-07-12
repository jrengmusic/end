# INVENTORY — END Plugin Host: JUCE + JAM + CLAP API Survey

Date: 2026-07-12
Consumes: RFC-end-plugin-host.md
Purpose: ground-truth API inventory for the jam_clap PLAN phase. Every mechanism the
implementation touches is listed here with file:line citations from direct source reads.
Nothing is reinvented — JAM pattern first, best JUCE match where no JAM equivalent exists.
Line numbers are survey-time snapshots (2026-07-12); re-verify at implementation.

Sources read this session:
- ~/Documents/Poems/dev/jam (JAM framework) + END project sources
- ~/Documents/Poems/JUCE (8.0.14 + local working-tree patches)
- ~/Documents/Poems/dev/clap-juce-extensions (HEAD 16e9d4c, 2026-06-24)
- ~/Documents/Poems/dev/juce_clap_hosting (HEAD aa8a812, 2022-10-06)

---

## I. JAM — existing API to consume

### 1. jam::Model (`jam_data_structures/model/jam_Model.h`)

Base: `juce::Timer + juce::ValueTree::Listener`; owns `juce::ValueTree` by value.

- `createAndAddParameter<ParameterT> (state, id, defaultValue)` — MESSAGE thread only.
  Parameter taxonomy: `Parameter<int>`, `Parameter<int64_t>` (packed composites — UUID,
  Bounds, Size, Winsize), `Parameter<float>`, generic trivially-copyable T via `bit_cast`
  over `std::atomic<int64_t>`, `ParameterText` (lock-free double-buffered string, seqlock
  generation, no hot-path allocation).
- `getParameter<T> (groupId, parameterId)` / `getParameter<T> (state, id)` → `T*`
  (nullptr if absent). Usage: `Session.cpp:31`, `ENDModel.h:19`.
- Serialization: `getXml()`, `replaceState()` (rebinds all adapters), `writeToXml`,
  `setValuesFrom`, `getValuesFrom` — **this is the module-tier getStateInformation /
  setStateInformation body** (RFC "Why jam::Model at both tiers").
- `Model::Listener::parameterChanged (id, newValue)` — fires synchronously on the calling
  thread (the any-thread lane).
- `Model::ParameterAttachment (ParameterBase&, callback)` — delivers on MESSAGE thread
  via AsyncUpdater. Ctor/dtor MESSAGE thread only.
- `Model::Attachment` (jam_Model.h:96-124) — pure connector: ctor binds an already-placed
  row's component (asserts parent) + `sendInitialUpdate()`; dtor disconnects only, NEVER
  removeChild. State survives view death — the stateInformation contract.
- `Model::Component<Derived>` CRTP (jam_Model.h:48-93) — build-or-adopt 4-param ctor
  `(model, parentState, type, uuid)`; 2-param adopt-existing ctor for root-level adoption.
- `flush()` / timer: adaptive 60/120 Hz atomic→VT sync, MESSAGE thread only.
- Thread contract: parameter store/load any thread (lock-free); flush/get/create MESSAGE
  only.

### 2. Composite substrate (`jam_gui/layout/`)

- `jam::OwnedComponent` (jam_OwnedComponent.h) — `juce::Component +
  jam::Model::Component<OwnedComponent>`; auto-registers `jam::ID::focus` (int) +
  `jam::ID::bounds` (int64 bit_cast); self-reports via focusGained/focusLost;
  resized/moved publish bounds.
- `jam::OwnerComponent (model, parentState, type, uuid, focusedChildId)`
  (jam_OwnerComponent.h) — owns `jam::HashMap<UUID, std::unique_ptr<OwnedComponent>>
  children` + `attachments`; `add (uuid, child)` (lines 70-79): try_emplace + Attachment +
  addChildComponent + stamps focused-child param + `childAdded()` hook. Sole author of its
  focused-child parameter. Pure virtuals: `childAdded`/`childRemoved`/`layout`.
  Owner IS ownable — composite recursion.
- `jam::PaneComponent` (jam_PaneComponent.h) — pane leaf; TerminalView extends it
  (`TerminalView.h:5`). Constants: `minimumPaneExtent = 80`, `edgePadding = 8`,
  `cornerSize = 4.0f`, `lineThickness = 1.0f`. Paint delegates to
  `LookAndFeel::Custom::drawPaneOutline()`. **The plugin-editor pane shell generalizes
  exactly here.**
- `jam::PaneEdge (model, parentState, head, tail, orientation, proportions)`
  (jam_PaneEdge.h:29-44) — binary-space seam; getSeam/hitTest/mouse drag; accessors
  getHead/getTail/getProportions/isVertical.
- Strategies: `jam::TabbedComponent` (authors `jam::ID::focusedTab`),
  `jam::MatrixComponent` (EDGE rows + PaneEdge graph, authors `jam::ID::focusedPane`).
- `TabView::createChild (uuid) → std::unique_ptr<jam::OwnedComponent>` (TabView.h) — the
  override hook where a plugin-editor pane is born.

### 3. Concurrency / containers (`jam_core/`)

- `jam::BufferSPSC` (concurrency/jam_BufferSPSC.h:46) — index-only SPSC ring, verbatim
  juce::AbstractFifo interface with one signature change: `finishedRead (startIndex,
  numRead)` for two-writer CAS reconciliation. Producer-side drop-oldest in
  `prepareToWrite` (non-const). `validStart` = `std::atomic<int>` (CAS,
  acquire/release); `validEnd` = `juce::Atomic<int>` (single writer). Torn-read guard is
  the caller's (END: CellFifo per-entry seqlock).
- `jam::HashMap<K,V>` (map/jam_HashMap.h) — clean-room ankerl::unordered_dense fork;
  try_emplace/emplace/at/find/erase; requires `std::hash<K>` (exists for jam::UUID).
  **`Session.h:28` `HashMap<UUID, std::unique_ptr<TerminalProcessor>>` is the exact slot
  for `std::unique_ptr<AudioPluginInstance>`** (RFC Session-tier substitution).
- `jam::Owner<T>` (utilities/jam_Owner.h) — vector<unique_ptr<T>> wrapper; MESSAGE only.
- `jam::Function::Map<juce::Identifier, void>` — events-map dispatch (the mandated direct
  lookup, never branch chains). Usage: ENDView.h:65, Session.h.

### 4. jam_vulkan (`jam_vulkan/engine/jam_VulkanEngine.h`)

- `VulkanEngine (double targetFrameBudgetMs, const juce::File& pipelineCacheFile,
  bool gpuEnabled)` (lines 56-66); sets `juce::ComponentPeer::externalContextFactory =
  &createContext` at construction (line 65).
- `removePeer (peer)` (lines 86-94) — must run while peer alive; `getCurrentGraphics()`,
  `findResidentGraphics (ImagePixelData&)`.
- `setGpuEnabled (bool)` — dispatch branch only, never reconstructs.
- Owns: shared Device, GlyphAtlas, Typeface/Stamp/Grapheme/Link interning tables,
  per-window Graphics keyed by native handle. `end::Application` owns the engine
  (ENDApplication.h:87, `initialiseVulkan()`). **Application already holds every pointer
  the HostServices struct carries.**
- Module deps (jam_vulkan.h:10-25): juce_graphics, juce_gui_basics, jam_core,
  jam_graphics, jam_freetype, jam_data_structures.

### 5. JAM module anatomy — jam_clap template

- Topmost header carries `BEGIN_JUCE_MODULE_DECLARATION` block (jam_core.h:1-17: ID,
  vendor "JRENG! Architectural Modules", version, dependencies, OSXFrameworks).
- Include discipline (jam_core.h:44-101): system → JUCE → debug → JAM components in
  dependency order. Topmost header drives ALL includes; submodule .h/.cpp are
  zero-include (platform .mm exception).
- Vendoring precedent (the established house pattern):
  - `jam_vulkan/vulkan/` — pinned SDK matched pair (C + .hpp), `VK_HEADER_VERSION`
    static_assert enforces pairing.
  - `jam_vulkan/shaderc/<platform>/` — AppBuilder.cmake IMPORTED static target gated on
    `_ca_mod STREQUAL "jam_vulkan"`; no machine SDK lookup. **This is the mechanism for
    pinning CLAP headers/helpers in jam_clap.**
- `jam::debug::Log` (jam_core/debug/jam_Log.h): static `write (const juce::String&)`
  (lines 57-61), `path()` (63-65), RAII `Log::Scope (const juce::File&)` (33-48, exactly
  one at a time, JUCEApplication member). Not gated on JUCE_DEBUG. ASCII-only strings
  (lines 53-55). DBG forbidden.

### 6. END integration surfaces

- `ENDModel : jam::Model, jam::Instance<ENDModel>` (Source/end/ENDModel.h); `setMessage()`
  via ParameterText (lines 17-22).
- `Nexus : jam::Instance<Nexus>` (Source/Nexus.h) — owns ENDModel + all Sessions
  (`HashMap<UUID, std::unique_ptr<Session>>`, line 66); createSession/getSession/
  removeSession/getActiveSession. **Nexus = the host tier; FormatManager + formats +
  get_extension registry land here per RFC.**
- `Session : jam::Model::Listener` (Source/end/Session.h) — processors map (line 28),
  newTerminal/removeTerminal = try_emplace/erase, events map.
- `SessionView : jam::TabbedComponent`, `TabView : jam::MatrixComponent`,
  `TerminalView : jam::PaneComponent` — the mirror-law view chain.
- `ENDView` (Source/end/ENDView.h) — sessions + attachments maps (63-64), events map
  (65), `juce::Value focusedPane` conduit (referTo last-changed TAB row). **Inherits
  `juce::FocusChangeListener` here for the RFC focus loop.**
- `Identifier.h` — X-macro system (lines 21-145): ID / IDref / IDtag / IDtype parallel
  views. New identifiers register here.
- `Bimap.h` — `jam::Bimap<Derived>` string↔enum SSOT + `getValidator()` for config
  (DropMode lines 15-58, FontRasterizerBackend 75-).
- `ConfigModel` (Source/config/ConfigModel.h) — four-phase lifecycle, validator
  registration pattern.

### 7. Confirmed ABSENT in JAM (all-new build surface)

No audio device, clock, transport, plugin-format, or hosting API anywhere in JAM or END.
- `jam_core/utilities/jam_PluginHost.h` is `juce::PluginHostType` sugar for menu/popup
  dispatch only (isAUSandboxHost/isProToolsHost/getHostScale) — NOT hosting.
- `jam_Value.h` has a conditional APVTS ParameterAttachment interop ctor
  (`#if JUCE_MODULE_AVAILABLE_juce_audio_processors`) — interop bridge, unused by END.
- jam_clap is greenfield exactly as the RFC scopes it.

---

## II. JUCE 8 (8.0.14 + local patches) — hosting stack OOTB

### 1. Module split (critical)

- `juce_audio_processors_headless` (juce_audio_processors_headless.h:45-164; deps:
  juce_audio_basics, juce_events) owns AudioProcessor, AudioPluginInstance,
  AudioProcessorGraph, AudioPluginFormat, AudioPluginFormatManager, PluginDescription —
  no UI dependency.
- `juce_audio_processors` (juce_audio_processors.h:54; deps: juce_gui_extra,
  juce_audio_processors_headless) adds AudioProcessorEditor, KnownPluginList,
  PluginDirectoryScanner, PluginListComponent, concrete format headers.
- `juce_audio_devices` (deps: juce_audio_basics, juce_events).
- `juce_audio_utils` (deps: juce_audio_processors, juce_audio_formats,
  juce_audio_devices). **END links juce_audio_utils; the rest arrive transitively.**

### 2. Format/instantiation

`AudioPluginFormatManager` (format/juce_AudioPluginFormatManager.h:46-155):
- `addFormat (std::unique_ptr<AudioPluginFormat>)` :83
- `createPluginInstance (const PluginDescription&, double, int, String& error)` :97
- `createPluginInstanceAsync (...)` :123
- **`addDefaultFormats() = delete`** :57 — replaced by free functions:
  `addHeadlessDefaultFormatsToManager (AudioPluginFormatManager&)` :167 (hasEditor false)
  and `addDefaultFormatsToManager` (juce_AudioPluginFormatManagerHelpers.h:42, UI module).

`AudioPluginFormat` (format/juce_AudioPluginFormat.h:46-181) — jam_clap's format
implements:
- `findAllTypesForFile (OwnedArray<PluginDescription>&, const String&) = 0` :67
- `createInstanceFromDescription (...)` :73/:80
- `fileMightContainThisPluginType (const String&) = 0` :104
- `canScanForPlugins() const = 0` :119; `isTrivialToScan() const = 0` :125
- `searchPathsForPlugins (const FileSearchPath&, bool, bool = false) = 0` :138
- `requiresUnblockedMessageThreadDuringCreation (...) const = 0` :150
- protected `createPluginInstance (const PluginDescription&, double, int,
  PluginCreationCallback) = 0` :173 (async-callback override point; VST3/AU override at
  format_types/juce_VST3PluginFormat.h:47, juce_AudioUnitPluginFormat.h:48).

`PluginDescription` (processors/juce_PluginDescription.h:51-187): data struct;
`createXml()` :174, `loadFromXml()` :181, `createIdentifierString()` :167.

`KnownPluginList` (scanning/juce_KnownPluginList.h:49-247, UI module): `scanAndAddFile`
:100, `getTypesForFormat` :76, `createXml`/`recreateFromXml` :171/:174 — user-extras
scanning path only (in-house = known-path sidecar, Decision 10).

`PluginDirectoryScanner` (scanning/juce_PluginDirectoryScanner.h:47-145): ctor :74,
`scanNextFile` :102, `applyBlacklistingsFromDeadMansPedal` :127.

### 3. AudioPluginInstance : AudioProcessor

`AudioPluginInstance` (processors/juce_AudioPluginInstance.h:57-189):
- `fillInPluginDescription (PluginDescription&) const = 0` :70
- `addHostedParameter (std::unique_ptr<HostedParameter>)` :108;
  `getHostedParameter (int)` :133
- Format-client accessors: `getVST3Client()` :98 →
  `AudioPluginExtensions::VST3Client` (utilities/juce_AudioPluginExtensions.h:87 —
  `getIComponentPtr()`, `getPreset()`, `setPreset()`); also getARAClient :83,
  getAudioUnitClient :88, getVSTClient :93.

`AudioProcessor` (processors/juce_AudioProcessor.h):
- `prepareToPlay (double, int) = 0` :139; `releaseResources() = 0` :145
- `processBlock (AudioBuffer<float>&, MidiBuffer&) = 0` :221 (audio thread, no UI —
  doc :211-217); double overload :292; bypassed :304/:316
- `suspendProcessing (bool)` :932; `isSuspended()` :937
- `setNonRealtime (bool) noexcept` :992 (the standard non-wall-clock host signal)
- `getStateInformation (MemoryBlock&) = 0` :1154; `setStateInformation (const void*,
  int) = 0` :1190; program-state variants :1167/:1201
- Editor: `createEditor()` private virtual :1023; **`createEditorAndMakeActive()` :1051
  is canonical — `createEditorIfNeeded()` :1059 is DEPRECATED (RFC snapshot correction)**;
  `getActiveEditor()` :1043 (message thread only, doc :1036); `editorBeingDeleted` :1316
- Bus API: `BusesLayout` :324; `getBusCount` :526; `setBusesLayout` :606;
  `setBusesLayoutWithoutEnabling` :615; `getBusesLayout` :618; `enableAllBuses` :647;
  `checkBusesLayoutSupported` :685; override points `isBusesLayoutSupported` :1415
  (default true), `canApplyBusesLayout` :1448, `applyBusLayouts` :1455
- `getParameters()` :1117; `addListener/removeListener` :1214/:1217
- `updateHostDisplay (const ChangeDetails& = getDefaultFlags())` :1076; ChangeDetails
  (juce_AudioProcessorListener.h:73-150): latency/parameterInfo/program/
  nonParameterState; **defaults do NOT include nonParameterStateChanged** :129
- Plugin-side extensions (module authoring): `getVST3ClientExtensions()` :1274,
  `getAAXClientExtensions()` :1254, `getVST2ClientExtensions()` :1264 — distinct from
  the host-side AudioPluginExtensions clients above
- `copyXmlToBinary (const XmlElement&, MemoryBlock&)` :1392 /
  `getXmlFromBinary (const void*, int)` :1398 — **plugin-side helpers** (doc :1385-1391):
  the in-house module's getState/setState body (jam::Model XML ↔ blob)

`HostedAudioProcessorParameter` (processors/juce_HostedAudioProcessorParameter.h:44-59):
`AudioProcessorParameter` + `getParameterID() const = 0` :58.

### 4. Playback/routing (RFC Open Question 2 inputs)

`AudioProcessorPlayer` (juce_audio_utils/players/juce_AudioProcessorPlayer.h:53-155):
`setProcessor` :69 (non-owning); `audioDeviceIOCallbackWithContext` :104 (device thread);
`audioDeviceAboutToStart/Stopped` :106/:108; `setDoublePrecisionProcessing` :95.

`AudioProcessorGraph` (processors/juce_AudioProcessorGraph.h:54-470): `NodeID` :68,
`Node` :112; `addNode (std::unique_ptr<AudioProcessor>, ...)` :283; `addConnection` :324;
`AudioGraphIOProcessor : AudioPluginInstance` :368 (audioInput/Output, midiInput/Output
:373); `rebuild()` :352 — always rebuilds on main thread, async-dispatches otherwise
(doc :348-350).

### 5. Device stack (virtual clock device)

`AudioDeviceManager` (audio_io/juce_AudioDeviceManager.h:78-584):
- `addAudioDeviceType (std::unique_ptr<AudioIODeviceType>)` :415
- `initialise (...)` :194; `initialiseWithDefaultDevices` :202; `setAudioDeviceSetup`
  :251; `getCurrentAudioDevice` :255; `addAudioCallback/removeAudioCallback` :309/:318
- `AudioDeviceSetup` struct :102

`AudioIODeviceType` (audio_io/juce_AudioIODeviceType.h:73-196): `scanForDevices() = 0`
:89; `getDeviceNames (bool) const = 0` :98; `createDevice (out, in) = 0` :124;
per-platform static factories :159-178. (No `createAudioIODeviceTypes` plural symbol
exists anywhere — RFC-adjacent phrasing correction.)

`AudioIODevice` (audio_io/juce_AudioIODevice.h:162-368): `open (inputs, outputs, sr,
bufferSize) = 0` :236; `start (AudioIODeviceCallback*) = 0` :258; `stop() = 0` :265;
`close() = 0` :242; `isOpen() = 0` :249.

`AudioIODeviceCallback` (same file :66-143): `audioDeviceIOCallbackWithContext (const
float* const* in, int, float* const* out, int, int numSamples, const
AudioIODeviceCallbackContext&)` :112; context struct = `const uint64_t* hostTimeNs` :45.

**MockDevice precedent** (audio_io/juce_AudioDeviceManager.cpp:1853-1985 — test-only,
mirror the shape, never depend):
- `open` :1875 stores config, returns `{}`
- `start (callback)` :1888 — stores callback, calls `audioDeviceAboutToStart (this)`,
  sets playing. **No thread spun up** — the owner invokes
  `callback->audioDeviceIOCallbackWithContext (...)` on any thread/timing it owns.
- `stop` :1895 — `audioDeviceStopped()`. `restart` :1915 = stop/close/open/start.
- `MockDeviceType::createDevice` :1965 validates names.
This is the exact demand-driven "host owns the clock in software" shape.

### 6. Editor hosting/embedding (jam_clap GUI shell reference)

**`VST3PluginWindow`** (format_types/juce_VST3PluginFormat.cpp:235-610) — THE reference
implementation: `AudioProcessorEditor + RunLoop + IPlugFrame + ComponentMovementWatcher +
ComponentBoundsConstrainer`.
- Per-platform embed member :535-573 — Windows: `ViewComponent : HWNDComponent` inner
  desktop component, `setHWND (peer->getNativeHandle())` :546; macOS:
  `NSViewComponentWithParent embeddedComponent`; Linux: `XEmbedComponent` with
  `withWantsKeyboardFocus(true).withAllowForeignWidgetToResizeComponent(false)
  .withIgnoreXembedMapped()` :566-568.
- `attachPluginWindow()` :460-497 — resolve native handle (getHWND :465 / getView :472 /
  getHostWindowID :474) → `addAndMakeVisible` :469 → `view->attached (handle, type)`
  :483. **CLAP analog: `gui->set_parent (clap_window)`.**
- Two-way resize negotiation: `componentMovedOrResized` :354 ↔ `resizeView` :415.

Components:
- `HWNDComponent` (juce_gui_extra/embedding/juce_HWNDComponent.h:38-98): `setHWND` :71,
  `getHWND` :78, `resizeToFit` :81, `updateHWNDBounds` :84.
- `NSViewComponentWithParent` — **lives in juce_audio_processors/utilities/
  juce_NSViewComponentWithParent.h:50-129** (hosting-specific, not gui_extra); ctor
  `explicit (AudioPluginInstance&)` :63; FabFilter nudge special-case :80.
- `XEmbedComponent` (juce_gui_extra/embedding/juce_XEmbedComponent.h).
- `AudioProcessorEditor` (processors/juce_AudioProcessorEditor.h:53-282): protected ctors
  :58/:61; `setResizable` :149; `setResizeLimits` :175; `setScaleFactor` :131;
  `getHostContext/setHostContext` :209/:216.
- `ComponentPeer::getNativeHandle()` — juce_ComponentPeer.h:169.
- AU editor wrapping: `AudioUnitPluginWindowCocoa` (juce_AudioUnitPluginFormat.mm:42,
  ctor :62; AUGenericView fallback :271-274).

**⚠ `ComponentPeer::externalContextFactory`** (juce_ComponentPeer.h:490:
`static inline std::unique_ptr<LowLevelGraphicsContext> (*externalContextFactory)
(ComponentPeer&) = nullptr;`) — **verified via git blame/diff to be an UNCOMMITTED local
working-tree patch on stock JUCE 8.0.14 (commit 2cdfca8feb). Does not exist upstream.**
Status (ARCHITECT, this session): patch pin intentionally rides the KANJUT→jam vulkan
engine sync/hardening workstream; initial testing shows no problems.

### 7. State plumbing — the actual host pattern

`extras/AudioPluginHost/Source/Plugins/PluginGraph.cpp` (does NOT use copyXmlToBinary —
that is plugin-side):
- Save (`createNodeXml` :350-396): `getStateInformation (m)` → `m.toBase64Encoding()`
  into host's own XmlElement `<STATE>` :379-382; `PluginDescription::createXml()`
  alongside :376; bus `<LAYOUT>` round-trip :385-389.
- Restore (`createNodeFromXml` :398-505): `PluginDescription::loadFromXml` :405 →
  `formatManager.createPluginInstance` :421 (KnownPluginList re-scan fallback :441-455)
  → `addNode` :470 → `setBusesLayout` :460-467 → `fromBase64Encoding` +
  `setStateInformation` :474-477.
- `MemoryBlock` (juce_core/memory/juce_MemoryBlock.h:44): `toBase64Encoding` :268,
  `fromBase64Encoding` :281. MemoryInputStream/MemoryOutputStream in juce_core/streams/.

### 8. Focus / shared resources / threading

- `FocusChangeListener::globalFocusChanged (Component*) = 0` — juce_Desktop.h:54;
  `Desktop::addFocusChangeListener/remove` :179/:185.
- `TopLevelWindow::focusOfChildComponentChanged (FocusChangeType)` —
  juce_TopLevelWindow.h:155 — second detection surface for native-child focus theft.
- `SharedResourcePointer` (juce_core/memory/juce_SharedResourcePointer.h:92-192):
  weak_ptr + SpinLock `lockOrCreate` :161-171; destroyed at last-ref (shared_ptr
  semantics); recursive same-type construction deadlocks (doc :54-56). Standalone
  own-engine fallback mechanism (Decision 8).
- Threading contracts: AudioProcessorListener callbacks may fire synchronously on the
  audio callback — must be thread-safe/fast, AsyncUpdater to reach message thread
  (juce_AudioProcessorListener.h:60-65, 155-160); `MessageManagerLock`
  (juce_MessageManager.h:475, re-entrant); `MessageManager::callAsync` host usage example
  juce_VST3PluginFormat.cpp:588-602.

### 9. Config flags / linkage

- User-settable: `JUCE_PLUGINHOST_VST3`, `JUCE_PLUGINHOST_AU` (+ VST/LADSPA/LV2/ARA),
  default 0 (juce_audio_processors_headless.h:75-122). Derived `JUCE_INTERNAL_HAS_VST3/
  AU/ARA` (format/juce_PluginFormatDefs.h:55-79) — hard `#error` if set directly :41-47.
- `VST3PluginFormat : VST3PluginFormatHeadless` (format_types/juce_VST3PluginFormat.h:
  38-52, gated JUCE_INTERNAL_HAS_VST3); `AudioUnitPluginFormat`
  (juce_AudioUnitPluginFormat.h:38-53, gated JUCE_INTERNAL_HAS_AU).
- Sidecar loading without scanning: `fileMightContainThisPluginType` checks
  extension+existence (juce_VST3PluginFormatHeadless.cpp:116-121); `findAllTypesForFile`
  on arbitrary known paths; `createInstanceFromDescription` loads directly from
  fileOrIdentifier (juce_VST3PluginFormatImpl.h:3593-3627, RFC snapshot).
- Reference build (extras/AudioPluginHost/CMakeLists.txt): `juce_add_gui_app (...
  PLUGINHOST_AU TRUE)` :38; `JUCE_PLUGINHOST_VST3=1` etc. :61-85; links
  `juce::juce_audio_utils` :87-95 (everything else transitive).

---

## III. CLAP repos

### A. clap-juce-extensions (authoring wrapper — active, HEAD 16e9d4c 2026-06-24)

Pins (.gitmodules + submodule SHAs): clap → **1.2.7** (`29ffcc2`, tag-verified,
2025-11-26); clap-helpers → `a61bcdf` (2025-11-28, untagged). Wrapper static_asserts
CLAP ≥ 1.2.0 (clap-juce-wrapper.cpp:56-58).

**Public plugin-author API** (include/clap-juce-extensions/clap-juce-extensions.h):
- `clap_properties` :42-61 — `is_clap` (flavor detection at construction from static
  `building_clap`, toggled around createPluginFilter at clap-juce-wrapper.cpp:2581/2587),
  `clap_transport`, `is_clap_active`/`is_clap_processing` atomics.
  **`is_clap_host` (RFC §7 phrasing) does not exist as a symbol.**
- `clap_juce_audio_processor_capabilities` :69-368 — isInputMain :77; voice-info :89-97;
  note-expressions :104; direct events :120/:134; outbound events :140/:158; **direct
  processing: `supportsDirectProcess` :182 / `clap_direct_process` :183-186** (wrapper
  short-circuit at clap-juce-wrapper.cpp:1603-1604); direct params-flush :187-191;
  note dialects :212-217; note-names :224-246 (JUCE ≥8.0.5 dual path,
  wrapper :1138-1196); param-indication :248-260; remote-controls :263-293; preset-load
  :296-328; `findParameterByParameterId` :334-339;
  **`getExtension (const char*)` :341-348 — THE plugin→host crossing**:
  `clapHostStatic` (set clap-juce-wrapper.cpp:2585, cleared :2588 — ctor window) then
  `extensionGet` lambda (wired :491-493 → `_host.host()->get_extension`).
- `clap_juce_parameter_capabilities` :375-402 — mono/poly modulation.
- `JUCEParameterVariant` :411-419 — the CLAP param event cookie.

**Wrapper internals** (src/wrapper/clap-juce-wrapper.cpp, 2627 lines):
- `ClapJuceWrapper : clap::helpers::Plugin<misbehaviour, checking> +
  AudioProcessorListener + AudioPlayHead + Parameter::Listener + ComponentListener`
  :395-405. Levels via macros, defaults Ignore/Minimal :158-164.
- **Does NOT override `Plugin::extension()`** — hook exists at clap-helpers plugin.hh:57,
  checked FIRST in trampoline (plugin.hxx:495-509). Host→plugin custom-extension patch
  point, exactly as RFC records. implementsX() overrides: timer :561, posix-fd :610,
  audio-ports :979, note-ports :1057, voice-info :1124, note-name :1138, track-info
  :1198, param-indication :1224, remote-controls :1271, preset-load :1319, params :1343,
  latency :1546, tail :1552, render :1573, gui :2031, state :2384.
- Param flags (paramsInfo :1349-1436): AUTOMATABLE from isAutomatable :1406; STEPPED
  from isBoolean only :1409-1417; modulation flags from parameter capabilities
  :1419-1433.
- `CLAP_SUPPORTS_CUSTOM_FACTORY` :89-91, consumed in clap_get_factory :2606-2608 —
  entry-factory level, distinct concept.
- Entry/factory: clap_create_plugin :2569-2591; exported `clap_entry` :2624-2626.

**Build integration** (cmake/ClapTargetHelpers.cmake):
- `clap_juce_extensions_plugin (TARGET ... CLAP_ID ... CLAP_FEATURES ...)` :192-207 —
  attaches to an existing `juce_add_plugin` target. Options: CLAP_ID (required :13),
  CLAP_FEATURES (default instrument :17), CLAP_MISBEHAVIOUR_HANDLER_LEVEL (Ignore :22),
  CLAP_CHECKING_LEVEL (Minimal :29), CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES (0 :36),
  CLAP_ALWAYS_SPLIT_BLOCK (0 :43), CLAP_SUPPORTS_CUSTOM_FACTORY (0 :50),
  CLAP_USE_JUCE_PARAMETER_RANGES (OFF :57). macOS `.clap` bundle plumbing :97-127
  (Info.plist template, Xcode ≥15 ld_classic workaround :120-126).
- README:12 — authoring-only by design, "does not support JUCE-based CLAP hosting".
  JUCE 6/7/8 compat; wrapperType reports `wrapperType_Undefined` (use is_clap).

**Working vendor-extension example** (examples/HostSpecificExtensionsPlugin/):
- .h:17-19 — inherits AudioProcessor + capabilities + protected clap_properties.
- .cpp:10-12 — `reaperPluginExtension = static_cast<const reaper_plugin_info_t*>
  (getExtension ("cockos.reaper_extension"));` — **the exact HostServices crossing
  pattern.**

**clap-helpers surface** (clap-libs/clap-helpers/include/clap/helpers/): plugin.hh/.hxx,
**host.hh/.hxx + plugin-proxy.hh/.hxx (host-side scaffolding — the clean-room jam_clap
host does not start from raw C structs)**, host-proxy.hh (per-extension canUseX gates),
event-list.hh, param-queue.hh, reducing-param-queue.hh, checking-level.hh
{None, Minimal, Maximal}, misbehaviour-handler.hh {Ignore, Terminate}.

### B. juce_clap_hosting (hosting PoC — translation map, HEAD aa8a812 2022-10-06)

**Path: `~/Documents/Poems/dev/juce_clap_hosting/` (underscore).** Vendors NO clap
headers — compiles against consumer-supplied `clap-core`; written pre-CLAP-1.2 surface.
Proper JUCE module (juce_clap_hosting.h:1-14, deps juce_audio_processors; unity-build
cpp). MIT.

`CLAPPluginFormat : AudioPluginFormat` (format_types/CLAPPluginFormat.h:9-40):
- Overrides: getName h:20; canScanForPlugins true h:21; isTrivialToScan false h:22;
  findAllTypesForFile h:24 / cpp:1217-1240 (enumerates whole factory — no shell
  indicator in CLAP); fileMightContainThisPluginType cpp:1293-1303 (.clap ext +
  existence, dir on mac/linux); pluginNeedsRescanning cpp:1310 (mod time);
  searchPathsForPlugins cpp:1320-1346; doesPluginStillExist cpp:1315;
  getDefaultLocationsToSearch cpp:1348-1361 (CLAP_PATH env TODO);
  createPluginInstance cpp:1277-1286 → createCLAPInstance :1242-1275;
  requiresUnblockedMessageThreadDuringCreation false cpp:1288.
- DSO loading `DLLHandle` cpp:107-229: open → resolve `"clap_entry"` → `entry->init`;
  Windows/Linux `juce::DynamicLibrary` :188-194, macOS CFBundle :198-223; dtor
  `entry->deinit` :117-134; `getPluginFactory()` :140-149. `DLLHandleCache` singleton
  :231-287 (per-path caching; Linux arch-tagged .so resolution :259-277).

`CLAPPluginInstance : AudioPluginInstance, private Timer` (cpp:291-1211):
- Lifecycle: `initialise` :457-480 (init + extension queries CLAP_EXT_PARAMS/
  AUDIO_PORTS/GUI/TIMER_SUPPORT/POSIX_FD_SUPPORT/STATE :468-473 + refreshParameterList
  + startTimerHz(10)); `prepareToPlay` :523-534 → `activate (sr, 1, blockSize)` +
  `start_processing`; `releaseResources` :536-541 → stop_processing + deactivate;
  dtor → cleanup :447-454 → `destroy`.
- `processBlock` :549-587: one clap_audio_buffer :556-561; clap_process :563-573;
  event lists via clap::helpers::EventList; `plugin->process` :578;
  handlePluginOutputEvents :581.
- Params: nested `CLAPParameter : Parameter` :323-421 — setValue queues via
  hostToPluginParamQueue (RT-safe) :332-350; getNumSteps hardcoded 100 :402;
  rescan diffing `refreshParameterList` :1081-1170 gated on CLAP_PARAM_RESCAN_ALL.
- State: clap_ostream ↔ MemoryBlock `getStateInformation` :900-927 (static write →
  `data.append` :909-914); clap_istream `setStateInformation` :929-961 (cursor read
  :940-951).
- Host struct wiring :425-440: host_data=this, CLAP_VERSION, get_extension/
  request_callback/request_process/request_restart. `request_callback` :1021-1024 →
  atomic → 10 Hz Timer :674-675 → `on_main_thread`.

**Stub/hardcode ledger (v1 jam_clap scope per Decision 5 — these ARE the build items):**
- `clapRequestProcess` STUB :1026-1029 (the demand-clock hook — commented body)
- `clapRequestRestart` STUB :1031-1034 (scheduleRestart atomic exists :677-682, never set)
- GUI embed fully commented :829-846 (is_api_supported/makeClapWindow/set_parent);
  live path = `DummyEditor : GenericAudioProcessorEditor` :847-857; hasEditor
  misleadingly true when pluginGui exists :860-872
- Bus hardcoding: audio in/out count = 1/1 :571/:573 (same clapBuffer both ways
  :570/:572); description numInputs/Outputs = 2/2 :70; canAddBus/canRemoveBus false
  :730-731; canApplyBusesLayout false (logic commented) :757-769; isBusesLayoutSupported
  true (logic commented) :733-755
- Host get_extension strcmp chain :1036-1055 — exposes ONLY clap.log/clap.params/
  clap.state; gui/thread-check/timer-support/posix-fd commented, host structs never
  defined
- steady_time = -1 :567; transport = nullptr :568; process status ignored :579;
  double-precision processBlock empty :685-693; clapParamsClear/RequestFlush stubs
  :1183-1191; ExtensionsVisitor commented :482-515; 21 `@TODO`s total
- hostState = { clapMarkDirty } :1206-1208 → `updateHostDisplay
  (ChangeDetails{}.withNonParameterStateChanged (true))` :1199-1204 (the mark_dirty →
  host pattern, working)

### C. CLAP spec facts (vendored 1.2.7, clap-libs/clap/include/clap/)

- Entry flow (entry.h:61-132): `clap_plugin_entry_t { clap_version, init(path),
  deinit(), get_factory(id) }`; exported `clap_entry` :132.
  `CLAP_PLUGIN_FACTORY_ID = "clap.plugin-factory"` (factory/plugin-factory.h:7);
  `clap_plugin_factory_t { get_plugin_count, get_plugin_descriptor, create_plugin
  (factory, host, plugin_id) }` :18-38 — host callbacks forbidden inside create :32.
- `clap_process` (process.h:30-62): steady_time (int64, -1 unavailable), frames_count,
  transport (nullable — null = free-running host), audio_inputs/outputs arrays + counts,
  in_events (host-sorted) / out_events (plugin sample-sorted). Status enum :10-27:
  ERROR=0, CONTINUE=1, CONTINUE_IF_NOT_QUIET=2, TAIL=3, SLEEP=4.
- `clap_window` (ext/gui.h:76-84): `{ const char* api; union { cocoa (void*); x11
  (ulong); win32 (void*); ptr } }`; API constants WIN32 :54, COCOA :57 (logical size,
  no set_scale), X11 :61, WAYLAND :65 (embed unsupported — floating only; irrelevant,
  END targets macOS+Windows).
- `clap.gui` plugin table (ext/gui.h:102-207): is_api_supported :106,
  get_preferred_api :113, create :130, destroy :134, set_scale :147, get_size :154,
  can_resize :158, get_resize_hints :162, adjust_size :170, set_size :176,
  **set_parent :182**, set_transient :188, suggest_title :193, show :199, hide :206.
  Sequence doc :19-33. Host `clap_host_gui` :209-240: resize_hints_changed :212,
  request_resize :222, request_show :227, request_hide :232, closed :239.
- Events (events.h): header {size, time, space_id, type, flags} :18-24;
  CLAP_CORE_EVENT_SPACE_ID = 0 :27; input list {ctx, size, get} :345-354; output list
  {ctx, try_push} :356-364.
- `clap.params` (ext/params.h): plugin {count, get_info, get_value, value_to_text,
  text_to_value, flush} :254+; host {rescan (flags), clear, request_flush} :358-378;
  rescan flags VALUES/TEXT/INFO/ALL :310-344; clear flags :348-356; param-info flags
  (STEPPED/PERIODIC/HIDDEN/READONLY/BYPASS/AUTOMATABLE±/MODULATABLE±/ENUM) :136-202.
- `clap.state` (ext/state.h:24-41): plugin {save (ostream), load (istream)}; host
  {mark_dirty}.
- `clap.audio-ports` (ext/audio-ports.h): plugin {count (is_input), get} :68-100; host
  {is_rescan_flag_supported, rescan} :102-115. `clap.note-ports` (ext/note-ports.h):
  plugin {count, get} :42-65; host {supported_dialects, rescan} :67-77.
- `clap.thread-check` (ext/thread-check.h:60-68): host-only {is_main_thread,
  is_audio_thread}. `clap.timer-support` (ext/timer-support.h:11-27): plugin {on_timer};
  host {register_timer (period_ms, *id), unregister_timer}. `clap.log` (ext/log.h:11-29):
  host-only, severities DEBUG..FATAL + HOST/PLUGIN_MISBEHAVING.
  `clap.posix-fd-support` (ext/posix-fd-support.h:13-49): plugin {on_fd}; host
  {register_fd, modify_fd, unregister_fd}.
- Standard extensions vendored (ext/, 27): ambisonic, audio-ports-activation,
  audio-ports-config, audio-ports, configurable-audio-ports, context-menu,
  event-registry, gui, latency, log, note-name, note-ports, param-indication, params,
  posix-fd-support, preset-load, remote-controls, render, state-context, state,
  surround, tail, thread-check, thread-pool, timer-support, track-info, voice-info.
  Drafts (ext/draft/): extensible-audio-ports, gain-adjustment-metering,
  mini-curve-display, project-location, resource-directory, scratch-memory,
  transport-control, triggers, tuning, undo, webview. Factory: plugin-factory,
  preset-discovery (+ drafts).

---

## IV. Divergences from RFC research-time snapshots (re-verified 2026-07-12)

1. `createEditorIfNeeded()` is DEPRECATED — canonical is `createEditorAndMakeActive()`
   (juce_AudioProcessor.h:1051).
2. `AudioPluginFormatManager::addDefaultFormats()` is `= delete` — the API is the free
   functions `addHeadlessDefaultFormatsToManager` / `addDefaultFormatsToManager`.
3. `ComponentPeer::externalContextFactory` is an uncommitted local JUCE patch, not
   upstream — pin rides the KANJUT→jam vulkan sync workstream (ARCHITECT-managed).
4. `NSViewComponentWithParent` lives in `juce_audio_processors/utilities/`, ctor requires
   `AudioPluginInstance&` — not a general gui_extra embedding utility.
5. Host-side state persistence pattern is Base64-into-host-XML (PluginGraph.cpp:379-382,
   :474-477); `copyXmlToBinary`/`getXmlFromBinary` are plugin-side helpers only.
6. `is_clap_host` — no such symbol; mechanisms are `clap_properties::is_clap` (flavor)
   and `getExtension` (host services query).
7. Hosting PoC repo path uses underscore (`juce_clap_hosting`); it pins no CLAP version
   of its own — consumer supplies `clap-core`.

## V. Feasibility assessment (session-ratified facts)

- Feasible with current state: every RFC layer maps to API existing on disk (hosting core
  OOTB; Session tier = one type substitution at Session.h:28; module-tier state = existing
  getXml/replaceState; clock = MockDevice shape; editor embed = VST3PluginWindow reference;
  services crossing = shipped getExtension + working example; vendoring = jam_vulkan
  shaderc mechanism).
- Sound: no structural contradiction found; all divergences are API-detail (§IV).
- Complexity concentrated in one item: the jam_clap clean-room host format (~80%) — the
  PoC's stubs are exactly the RFC-critical surface (request_process, GUI embed, multi-bus,
  extension registry), written fresh; clap-helpers host.hh/plugin-proxy.hh reduce the
  from-raw-C cost. Small items: virtual device + pump; injected-engine seam (serialized
  behind KANJUT sync); focus loop host-side.
- Risks, all with deterministic pass/fail: (1) externalContextFactory patch — visual-parity
  fixture; pin managed in sync workstream; (2) globalFocusChanged(nullptr) — Phase 2 FIRST
  acceptance test, fallback `com.jreng.focus/1` via clap-helpers extension() hook verified
  reachable; (3) multi-bus third-party conformance — per-format fixture (Decision 5);
  (4) PoC staleness — pinned 1.2.7 headers + compiler as gatekeeper, PoC is map never copy.

## VI. Session provenance notes

- Researcher subagent ran `git submodule update --init --recursive` in
  clap-juce-extensions/ to check out the registered-but-empty clap/clap-helpers
  submodules (non-destructive checkout of pinned SHAs; no commits, no edits). Disclosed
  to ARCHITECT in-session.
- JUCE working tree confirmed at 8.0.14 (2cdfca8feb) + local uncommitted modifications.
