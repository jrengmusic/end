# RFC — END as Plugin Host: CLAP Formalization of In-House Modules

Date: 2026-07-12
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END is nearly complete as a "host shell": jam::Model faithfully designed as an isomorphic
abstraction analogous to juce::AudioProcessorValueTreeState, Session-Tab-Pane hierarchy
landed (Sprint 71: binary-space pane substrate, jam::PaneEdge graph), mirror law in force
(component tree is a pure function of the state tree).

The next objective is deriving concrete objects as Pane tenants: terminal, whelmed, surf,
and future components. ARCHITECT's question: is it genuinely, objectively, technically
feasible and SOUND to formalize END as an **actual plugin host**, with in-house modules
authored as **actual audio plugins** (PluginProcessor/PluginEditor JUCE projects), never
loaded into a real DAW — END is always the DAW?

Motivations stated by ARCHITECT:
- Reuse the existing plugin pipeline: hot-path loop (processBlock as transport), state
  serialization (getState/setState), ephemeral UI (editor created/destroyed while
  processor persists), uniform threading contract, uniform API across feature modules.
- No hand-rolled ABI — use an existing plugin format.
- Virtual audio device END-side as the driver: END owns the clock ("full throttle clock"),
  and END as an actual host could be a legit host with actual plugins and actual audio
  hardware.
- **Discipline rationale (decisive):** with the AUDIO PLUGIN fixed and rigid architecture
  (designed lock-free, critical high-performance loaded binary), there is no choice except
  to design and implement correctly. No shortcut, no workaround as escape hatch; any
  deviation from CAROL agents is caught by the compiler. Deterministic feature development
  under the PluginProcessor/PluginEditor mental model: which thread must be free, what can
  be called from the message thread, etc.
- All moving parts (jam::Terminal, jam::CodeView) are unfinished/untested — which is
  desired: the rigid contract forces correct design of the unfinished parts.

**Verdict reached in session: SOUND, end to end, with every layer backed by shipped code
ARCHITECT authored** — lock-free processor (endless), host machinery (JUCE), software
clock (MockDevice pattern), model at both tiers (jam::Model), plugin-side Vulkan
(kuassa_vulkan). Format locked: **CLAP**.

---

## Research Summary

All findings below were produced this session by delegated research (Pathfinder on
codebases, Librarian on JUCE/CLAP internals, Researcher on ecosystem/precedent) with
citations. No training priors.

### 1. JUCE hosting capability (JUCE 8, ~/Documents/Poems/JUCE)

- Full hosting stack ships OOTB: `AudioPluginFormatManager`
  (`modules/juce_audio_processors_headless/format/juce_AudioPluginFormatManager.h:46`),
  `AudioPluginFormat`, `KnownPluginList`, `PluginDescription`,
  `AudioPluginInstance : AudioProcessor`
  (`juce_AudioPluginInstance.h:57`), `AudioProcessorGraph`, `AudioProcessorPlayer`
  (`juce_audio_utils/players/juce_AudioProcessorPlayer.h:53`).
- Lifecycle: scan (optional) → describe (`PluginDescription`) → instantiate
  (`createPluginInstance`, guaranteed message-thread, `juce_AudioPluginFormat.h:170`) →
  `prepareToPlay` → `processBlock` → `createEditorIfNeeded`.
- JUCE 8 split: `juce_audio_processors` (GUI) depends on new
  `juce_audio_processors_headless` (hosting core). Headless-only hosting is first-class
  (`addHeadlessDefaultFormatsToManager`, `juce_AudioPluginFormatManager.h:157-166`).
- State pipeline: `getStateInformation`/`setStateInformation` pure virtual
  (`juce_AudioProcessor.h:1154`, `:1190`); host save/restore pattern demonstrated in
  `extras/AudioPluginHost/Source/Plugins/PluginGraph.cpp:380-381`, `:474-477`.
- Threading contract: `processBlock` called by whatever thread drives the callback; only
  constraint is no UI interaction (`juce_AudioProcessor.h:211-217`). Editors and
  parameters are message-thread-affine. `AudioProcessorPlayer` calls `processBlock`
  directly on the device callback thread (`juce_AudioProcessorPlayer.cpp:314-319`).
- Editor hosting: `AudioProcessorEditor` is a plain `Component`; VST3 native-handle
  plumbing handled internally by `VST3PluginWindow` via `HWNDComponent` /
  `NSViewComponentWithParent` (`juce_VST3PluginFormat.cpp:235`, `:538`, `:563`). Host
  never supplies raw handles for stock formats.
- In-process loading: VST3 loads as dylib/bundle in-process
  (`juce_VST3PluginFormatImpl.h:1148`, `:1186`). No JUCE-native out-of-process hosting.
- Sideband beyond params/state/buffers: only the format escape hatch —
  `getVST3Client()->getIComponentPtr()` (raw COM `IComponent*`,
  `juce_AudioPluginExtensions.h:87-93`) reachable for vendor `queryInterface`.

### 2. Software clock without hardware — precedented inside JUCE

- Custom `AudioIODevice`/`AudioIODeviceType` registrable via
  `AudioDeviceManager::addAudioDeviceType`.
- Direct internal precedent: `MockDevice`/`MockDeviceType` in
  `modules/juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp:1853`, `:1932` —
  `start()` only announces; the host invokes
  `callback->audioDeviceIOCallbackWithContext(...)` on any thread/timing it owns. This is
  the exact "host owns the clock in software" shape (test-only code — mirror the shape,
  do not depend on it).
- `AudioProcessor::setNonRealtime(bool)` (`juce_AudioProcessor.h:992`) is the standard
  host signal for non-wall-clock driving.

### 3. Precedent and the anti-precedent (Researcher, web)

- **Zero precedent** for using VST3/CLAP binaries as a generic non-audio module ABI. All
  real-world "non-DAW hosts VST" cases carry genuine audio DSP (Unity/Unreal hosts) or
  target real DAWs (Camomile/plugdata, Cabbage).
- JUCE's own answer to "uniform module contract without binary format" is the
  `InternalPluginFormat` pattern
  (`extras/AudioPluginHost/Source/Plugins/InternalPlugins.h`) — `AudioPluginFormat`
  backed by in-code factories, `canScanForPlugins() == false`. Bitwig likewise implements
  internal devices natively, not via the external ABI.
- Audio-callback-as-scheduler literature (Bencina "time waits for nothing"): no
  allocation/locks/blocking syscalls in the callback. kitty (poll-driven ChildMonitor
  thread) and Alacritty (mio event loop) both use dedicated I/O threads.
- **Session correction (ARCHITECT, upheld by evidence):** the producer/consumer split is
  not a defect of the plugin model — it IS the plugin model (disk-streaming samplers,
  convolution engines: background I/O threads feed lock-free FIFOs; processBlock is the
  clocked consumer). The RT contract governs the callback only, not the module's worker
  threads. Descriptive citations of kitty/Alacritty are not normative bounds — treating
  "existing implementations use X" as a limit on what is achievable is a logical fallacy.

### 4. Proven lock-free terminal architecture (ARCHITECT's prior END — evidence)

Source: `~/Documents/Poems/dev/endless/` (remnants of pre-rewrite END).

- Data path: PTY fd → Reader thread (`poll(POLLIN)` + `O_NONBLOCK` read,
  `UnixTTY.h:151-158`, `TTY.h:259-272`) → Parser/Video → `Buffer<Row>` → CellFifo →
  message-thread drain → CodeModel → GPU.
- Synchronization: per-entry seqlock (odd/even epoch stamps, acquire/release,
  torn-read rejection — `CellFifo.h:373-415`, `:455-475`); `jam::BufferSPSC` rings with
  CAS drop-oldest on `validStart` (`jam_BufferSPSC.h:147-150`) and single-writer
  `validEnd` (`:190`). Drop-oldest ⇒ reader never stalls.
- Preallocated storage (`CellFifo.h:81-91`); zero reader-side hot-path allocation.
- Full mutex disclosure: two hits, neither a data-path mutex — `callbackLock`
  (`Processor.h:327`, acquired `ProcessorEvents.cpp:422`) is an entry gate serializing
  `process()` against `suspendProcessing()` (contended only during resize); the resize
  gate itself (`Processor.cpp:366`, message thread).
- Matches END `ARCHITECTURE.md:110` verbatim: "Lock-free architecture, unidirectional
  data flow. No mutex on any hot path. No wait, no stall, no yield."
- **Implication:** the architecture already satisfies the audio-callback contract on the
  consumer side. Moving the drain from message-thread listener into processBlock is a
  relocation, not a redesign. One datum: `ensureScratch()` (`CellFifo.h:337-344`) can
  realloc during drain — preallocate to worst case if drain moves to the clocked consumer.

### 5. jam::Model vs APVTS (Pathfinder on JAM + Librarian on JUCE)

jam::Model (`~/Documents/Poems/dev/jam/jam_data_structures/model/jam_Model.h`):
- Owns `juce::ValueTree state` + `jam::AnyMap params` + `ParameterAdapter` map
  (bidirectional atomic↔VT, loopback-guarded, equality-gated) + `LockedListeners`.
- Parameter taxonomy richer than APVTS: `Parameter<int>`, `Parameter<int64_t>` (packed
  composites — UUID, Bounds, Winsize), `Parameter<float>`, generic trivially-copyable T
  via `bit_cast` over `std::atomic<int64_t>`, and `ParameterText` (lock-free
  double-buffered string, seqlock generation, no hot-path allocation). Native types
  end-to-end — no 0..1 normalization.
- Serialization: `getXml()` (`jam_Model.cpp:151`), `replaceState()` (`:157`, rebinds all
  adapters), `writeToXml`, `setValuesFrom`.
- Divergences from APVTS: no `beginChangeGesture`/`endChangeGesture` (host automation
  gestures), no host parameter exposure, no normalized-value semantics.

APVTS host-visibility facts (JUCE 8):
- **APVTS is never host-visible.** Constructor requires `AudioProcessor&`
  (`juce_AudioProcessorValueTreeState.h:261-264`); nothing in any format wrapper exposes
  the ValueTree across the ABI. Host sees exactly: flattened parameters, opaque state
  blob, `updateHostDisplay(ChangeDetails::withNonParameterStateChanged)` flag
  (`juce_AudioProcessor.h:1076`).
- All ABI parameters are normalized float 0..1 (`juce_AudioProcessorParameter.h:113`,
  `:129`); Choice/Bool/Int are sugar. No string/blob/tree parameters exist.
- In-process same-binary hosts may `dynamic_cast` `Node::getProcessor()`
  (`juce_AudioProcessorGraph.h:122`) to concrete types — no JUCE barrier.
- Sidecar loading without scanning: `fileMightContainThisPluginType` checks only
  extension+existence (`juce_VST3PluginFormatHeadless.cpp:116-121`);
  `findAllTypesForFile` runs on arbitrary known paths (`:46-82`);
  `createInstanceFromDescription` loads directly from `fileOrIdentifier`
  (`juce_VST3PluginFormatImpl.h:3593-3627`). Bundled paths (macOS Resources, Windows
  sidecar dll) accepted. ConPTY.dll pattern confirmed.

### 6. kuassa_vulkan — Vulkan inside hosted plugin editors is solved, shipped code

`~/Documents/Poems/kuassa/___lib___/kuassa_vulkan/` (KANJUT):
- Surface from host-owned native view: `createContext()` → `peer.getNativeHandle()` →
  `vk::SurfaceKHR` (CAMetalLayer macOS / HWND Windows / X11 Linux —
  `kuassa_VulkanGraphicsSetupSurface.cpp:16-28`). Own swapchain per editor window;
  resize on extent mismatch; survives DAW reparenting (Graphics keyed by native handle,
  `kuassa_VulkanEngine.h:249-260`).
- Ephemeral-editor lifecycle: `juce::SharedResourcePointer<VulkanEngine::Shared>`
  declared FIRST in Editor (`kuassa_Editor.h:76`) — first editor constructs engine+device,
  last destroys; `removePeer()` discipline before release.
- N plugin instances per binary share ONE `vk::Device`/GlyphAtlas/interning tables
  (`kuassa_VulkanEngine.h:436-504`). Sharing stops at the dylib wall (per-binary, not
  per-process).
- Same never-null contract as jam_vulkan (CPU `LowLevelGraphicsGlyphRenderer` fallback,
  `kuassa_VulkanEngine.h:316-339`). kuassa_vulkan and jam_vulkan are independent siblings
  in shape. **ARCHITECT: jam_vulkan upstream sync fork from KANJUT already in progress.**

### 7. CLAP ecosystem (Researcher + Librarian, web + repo inspection)

- **JUCE ships zero CLAP hosting** (verified in tree — zero hits). JUCE 9 roadmap
  (Q3-2025 post) commits to CLAP **authoring** only; no official hosting statement.
- `free-audio/clap-wrapper`: CLAP→VST3/AU plugin-side wrapper, built per plugin — not a
  host library.
- **`jatinchowdhury18/juce_clap_hosting`** (ARCHITECT-supplied lead): a real
  `CLAPPluginFormat : juce::AudioPluginFormat` + `CLAPPluginInstance`, MIT, 1362 lines.
  Works: DSO/entry/factory, lifecycle, params→`AudioProcessorParameter` (rescan partially
  stubbed), state save/load (`CLAPPluginFormat.cpp:900-960`, `mark_dirty` →
  `updateHostDisplay`), single-bus process (hardcoded 1-in/1-out, `:571-573`), events
  wired. Missing: **GUI embedding commented out** (`:829-844`, falls to generic parameter
  editor), **no generic extension passthrough** (hardcoded strcmp chain, `:1036-1055`),
  multi-bus. Last substantive commit 2022; maintainer on record (clap-juce-extensions
  discussion #152): unfinished, not actively developed. Treat as translation map, not
  production code.
- CLAP spec facts (free-audio/clap headers): minimal known-path in-process host =
  `clap_entry` (dlopen) → `get_factory(CLAP_PLUGIN_FACTORY_ID)` → `create_plugin(host)` →
  `init` → `activate(sr, min, max)` → `start_processing` → `process(clap_process)` →
  teardown. `clap_process`: `steady_time`, `frames_count`, nullable `transport`
  (null = free-running host), audio buffer arrays, sorted in/out event queues.
- **`get_extension` is bidirectional with arbitrary string IDs by design** — plugin-side
  and host-side both. Custom vendor extensions are documented, sanctioned mechanism
  ("make sure the extension identifier includes versioning... unique identifier").
- GUI: `clap.gui` `set_parent(clap_window)` — `clap_window` is a union of NSView* /
  HWND / X11 Window / UIView*. Embedding supported win32/x11/cocoa/uikit; Wayland
  floating-only (irrelevant: END targets macOS+Windows).
- Host callbacks: `request_callback` (main-thread pump, ~30Hz aim),
  **`request_process`** (plugin asks host to clock a block — the demand-driven hook),
  `request_restart`.

### 8. clap-juce-extensions (authoring wrapper) — load-bearing verification

- **Plugin→host custom extension: first-class, shipped.**
  `clap_juce_audio_processor_capabilities::getExtension(const char*)` reaches
  `clap_host_t::get_extension` (PR #136, merged 2023-10-17; two-phase `clapHostStatic` /
  `extensionGet` lambda, `src/wrapper/clap-juce-wrapper.cpp:491-493`). Working in-repo
  example: `examples/HostSpecificExtensionsPlugin` querying `cockos.reaper_extension`
  and casting to a vendor struct. **This is exactly the HostServices crossing.**
- Host→plugin custom extension responder: NOT wired. `ClapJuceWrapper` (closed class,
  .cpp-only, directly instantiated at `clap-juce-wrapper.cpp:2589`) never overrides
  `clap::helpers::Plugin::extension()` — though the hook exists one layer down and is
  checked FIRST (`clap-helpers plugin.hxx:496-500`). Exposing it = small patch to the
  vendored wrapper. Not needed by the design as drawn.
- `CLAP_SUPPORTS_CUSTOM_FACTORY` exists (entry-factory level — distinct concept, do not
  conflate).
- Maintenance: active (last commit 2026-06-24), MIT, CLAP 1.2.7 pinned, explicit
  JUCE 8.0.5+ compatibility work. Low risk.

---

## Principles and Rationale

### Why formalize as Host-Plugin (ARCHITECT's rationale, evidence-backed)

The codebase already IS this architecture in analog form — END `ARCHITECTURE.md` in its
own words: `terminal::Processor` = "AudioProcessor analog", `terminal::Model` = "Per-pane
APVTS bridge", `jam::Model` = "1:1 APVTS analog for multi-type parameters",
`jam::Model::Attachment` = "exactly as a PluginEditor owns SliderAttachments".
Formalization renames reality; it does not redesign it. What it adds is the ABI boundary:
nothing crosses except parameters, state blob, editor, buffers, events, and explicitly
declared extensions — deviation becomes unrepresentable, caught by the compiler. This is
the Definitive Correctness Foundation applied structurally.

### Why CLAP (format rationalization, locked)

1. **License:** MIT end to end. No Steinberg agreement on END, ever — the founding
   rationale of CLAP matches END's need exactly. (VST3 SDK is GPLv3/proprietary
   dual-licensed; shipping END as VST3-module product would carry Steinberg terms
   indefinitely for a terminal emulator.)
2. **Authoring parity:** module code is PluginProcessor/PluginEditor either way;
   clap-juce-extensions compiles the same JUCE plugin to `.clap`. Binary format is a
   packaging flag, not an architecture.
3. **Services crossing:** CLAP's `get_extension` with vendor string IDs is first-class
   design intent — cleaner than the VST3 COM shim would have been, and the needed
   direction (plugin→host) ships today in the authoring wrapper with a working example.
4. **Hosting cost:** the one build item — a JAM module clean-room translated from the
   CLAP headers (spec) with the MIT PoC as map. This is the established house pattern
   (vulkan, mermaid, markdown). Cost accounting: under VST3 the hand-rolled item is a COM
   shim per project forever plus license terms; under CLAP it is one owned framework
   module reusable by TIT/CAKE/CAROLINE. Capital expenditure in JAM.
5. **Multi-format host preserved:** `CLAPPluginFormat` rides `AudioPluginFormatManager`
   alongside stock VST3/AU. Real-world plugins host through JUCE's shipped code untouched.

### Why jam::Model at both tiers (no APVTS anywhere)

- **Host tier:** APVTS is structurally impossible there — it binds to an AudioProcessor;
  END-the-host has none. A DAW's host-tier state is its hand-rolled project model.
  ENDModel-as-jam::Model already is that object. Zero change.
- **Module tier:** the internal model is plugin-private by definition (APVTS is never
  host-visible; only flattened float params + opaque blob cross). jam::Model's rich
  taxonomy cannot cross the parameter ABI and does not need to — it is what the state
  blob is for. Contract per module: `getStateInformation` = `getXml()` →
  `copyXmlToBinary`; `setStateInformation` = `getXmlFromBinary` → `replaceState()`
  (adapter rebind handled). Host-visible automatable knobs, only where meaningful as
  float, get thin `RangedAudioParameter` bridges — the sole place gesture semantics live.

### Communication channels — existing infrastructure only

| Traffic | Channel | Custom code |
|---|---|---|
| Persistent state | getState/setState (blob = jam::Model XML) | none |
| Bulk hot path | AudioBuffer (host-allocated float arrays) | none |
| Events/verbs | event queues (CLAP events; MIDI SysEx carries arbitrary binary) | none |
| Scalar automation | parameters (normalized float) | none |
| Shared resources | `com.jreng.host-services/1` via get_extension (plugin→host) | one header |
| Focus report (inward) | ENDView FocusChangeListener + native query (host-side only) | small, host-side |
| Ephemeral UI | clap.gui set_parent; editor paints via injected host LLGC | factory shim |

Owning both sides guarantees ABI compatibility (one toolchain, one JUCE, one JAM):
pointers that cross are safe C++. The state blob is NOT used for runtime handles
(serialization/injection conflation = forbidden workaround category).

### Shared rendering — the catch and its collapse

The binary boundary severs implicit sharing: each dylib has its own JUCE/JAM statics,
so `ComponentPeer::externalContextFactory` (a static per JUCE copy) cannot be reached by
the host's registration. Resolution: one pointer crossing — module queries
`getExtension("com.jreng.host-services/1")`, receives the host's `VulkanEngine*` /
`GlyphAtlas*` etc., and the plugin-side glue registers the HOST engine as the dylib-local
factory. From then on every child of the PluginEditor paints through ordinary
`juce::Graphics` backed by the host's Vulkan LLGC — shared Device, shared GlyphAtlas,
construction-guaranteed visual parity. Vulkan handles are process-global; kuassa_vulkan
proves the surface-in-hosted-view path. Sharing is per-process for in-house modules
(injected), per-binary for third-party (their own renderers — correct isolation).

### Clock — demand-driven, two domains

- Demand-driven IS full throttle when it matters: under PTY flood the reader signals
  `request_process` continuously, blocks clock back-to-back, throughput bounded only by
  drain speed (drop-oldest guarantees the reader never stalls regardless); at idle, zero
  blocks, zero wake-ups. It is the existing screenDirty → drain pattern crossing the
  boundary through a stock CLAP channel.
- Hardware is a clock and cannot be subordinated (a "virtual master" bridging to hardware
  forces drift compensation). Hence two coexisting domains — see Decisions.

### Focus — closed loop, two directions

- **State → view (END-initiated):** keybinding/tab-switch/verb → `focused_pane` change →
  ENDView (already listening) dispatches OS keyboard focus to the pane's embedded native
  view. Existing contract carries it.
- **View → state (pointer-initiated):** click inside a plugin's native view gives the OS
  focus directly; END's JUCE sees nothing (two JUCE copies = two focus managers; JUCE
  focus callbacks structurally cannot fire). ENDView inherits
  `juce::FocusChangeListener`; `globalFocusChanged(nullptr)` = "a native tenant took
  focus"; resolve identity by OS query (`GetFocus()` / key window `firstResponder`) +
  parent-chain walk to a known container handle (handle→pane map falls out of jam_clap's
  embed path); write that PANE row's focus report; the entire existing chain (owner
  aggregation, `focused_*` singulars, type-filtered app-singular authorship) runs
  untouched.
- No feedback spiral: dispatch triggers the listener, resolves to the already-focused
  pane, dies at jam::Model's equality gate (ParameterAdapter: loopback-guarded,
  equality-gated).
- Contract amendment (one line, for ARCHITECTURE.md when it lands): for native-tenant
  panes, ENDView proxy-writes the PANE focus report — the report channel gains a second,
  mutually exclusive writer (JUCE-tenant self-reports XOR native-tenant proxied).

### BLESSED pillar mapping

- **Bounds:** thread bindings unchanged and now compiler/ABI-enforced; preallocated
  rings; drop-oldest bounded transport; per-entry seqlock bounded retries (reject, never
  wait).
- **Lean:** zero new abstractions invented — every mechanism is either shipped JUCE,
  spec'd CLAP, or existing jam/kuassa code relocated.
- **Explicit:** every crossing is a declared channel (table above); the one-time services
  injection is a named, versioned extension ID; no implicit sharing survives.
- **SSOT:** one pinned CLAP header set for both authoring and hosting (skew impossible by
  construction); lua files remain config SSOT; CodeModel remains document SSOT;
  jam::Model remains state SSOT per tier; blob is the single serialization of
  module-internal state.
- **Stateless:** views remain pure functions of state trees (mirror law untouched);
  editors are ephemeral over persistent processors — the stateInformation contract.
- **Encapsulation:** module internals leave END's visible tree entirely (opaque blob
  under PANE); objects stay dumb, communicate via the declared APIs only.
- **Deterministic:** demand-driven clocking from a single owner; registration-order and
  equality-gate invariants preserved; the compiler is the gatekeeper of every boundary.

---

## Decisions Ledger (ARCHITECT-ratified this session)

1. **Concept:** END formalized as actual plugin host; in-house modules as actual plugins. SOUND.
2. **Format:** CLAP for in-house modules.
3. **Module:** `jam_clap/` — ONE JAM module, both authoring and hosting.
4. **Namespace:** `com.jreng.*` extension IDs (e.g. `com.jreng.host-services/1`, `com.jreng.focus/1` if ever needed plugin-side).
5. **Conformance target:** **third-party CLAP from day one** — full host bar: multi-bus port config, standard-extension coverage, graceful degradation for arbitrary plugins.
6. **Host formats v1:** **CLAP + stock VST3/AU** — any plugin becomes a pane. (Accepts VST3 SDK licensing surface for hosting + per-format embed testing in the conformance fixture.)
7. **Clock topology:** **two coexisting domains** — in-house modules on END-owned demand-driven virtual device (`request_process`); third-party audio plugins on the real hardware device via AudioDeviceManager. Cross-domain audio bridge deferred until a feature needs it.
8. **Standalone:** **yes** — every module project builds a standalone dev target (own engine via KANJUT-synced Shared fallback); injected-vs-own engine branch exercised by construction.
9. **Models:** jam::Model at both tiers; APVTS nowhere; state crosses via blob only.
10. **Shipping:** sidecar embedded binaries (ConPTY.dll pattern), known paths, no scanning for in-house; scanning available for user "extras".
11. **Sequencing:** jam_clap → END host skeleton + conformance test plugin (PERMANENT fixture) → services/rendering crossing proven → jam::Terminal/jam::CodeView terminal plugin to the fullest.
12. **jam_vulkan:** upstream sync fork from KANJUT already in progress (ARCHITECT-driven); injected-engine seam is a sync addendum, not a port.
13. Names used by ARCHITECT and treated as ratified vocabulary: `jam_clap`, `jam::Terminal`, `jam::CodeView`. Remaining names (fixture plugin, HostServices struct, internal layout) are Decision Gate items at scaffold time.

---

## Scaffold

No code was written this session (ORACLE is read-only; all structures below are the
session's ratified shapes for COUNSELOR to plan against).

### jam_clap/ module layout (internal naming to be ratified at scaffold)

```
jam_clap/
  clap/                    — vendored CLAP headers (pinned, SSOT for BOTH sides; 1.2.7 at research time)
  helpers/                 — vendored clap-helpers (pinned; Plugin<h,l> template layer)
  extensions/              — vendored clap-juce-extensions (pinned; patch point for
                             plugin-side extension() override if ever needed;
                             its submodule pins REDIRECTED at the jam_clap SSOT headers)
  format/                  — clean-room host:
                               PluginFormat  : juce::AudioPluginFormat
                               PluginInstance: juce::AudioPluginInstance
                               - DSO/entry/factory (dlopen/LoadLibrary/CFBundle)
                               - lifecycle: init→activate→start_processing→process→…→destroy
                               - params → HostedAudioProcessorParameter (+ rescan flags)
                               - state over clap_istream/ostream ↔ MemoryBlock
                               - known-path findAllTypesForFile (no scanning required)
                               - MULTI-BUS port config (third-party conformance)
                               - GUI embed: set_parent(clap_window) wrapped in an
                                 AudioProcessorEditor shell over HWNDComponent /
                                 NSViewComponentWithParent (JUCE's own VST3-hosting
                                 components as reference implementation)
                               - generic extension REGISTRY (string-ID table replacing
                                 strcmp chains; standard + com.jreng.* answer through it)
                               - host callbacks: request_callback pump (~30Hz),
                                 request_process → owning clock, request_restart
  services/                — HostServices contract header:
                               extension ID "com.jreng.host-services/1"
                               struct of forward-declared pointers ONLY
                               (VulkanEngine*, GlyphAtlas*, Typeface*, Stamp*,
                                Grapheme*, Link*) — jam_clap takes NO hard dependency
                               on jam_vulkan; both sides include the same header;
                               the compiler is the contract
  [plugin-side glue]       — Editor base for in-house modules:
                               - queries getExtension("com.jreng.host-services/1")
                               - injected-engine mode: registers HOST VulkanEngine* as
                                 the dylib-local externalContextFactory (~10 lines)
                               - own-engine fallback: SharedResourcePointer Shared
                                 lifecycle (arrives with the KANJUT sync) for the
                                 standalone dev target
                               - branch selection at editor construction
```

### END restructuring inventory (additive; ARCHITECTURE and data structures unchanged)

**Application tier:**
- Link `juce_audio_processors` / `juce_audio_devices` / `juce_audio_utils` (build addition).
- HostServices provisioning: Application already owns every pointer the struct carries
  (`vulkanEngine` members); fill once at init.

**Nexus tier (= the host, already named as such):**
- `AudioPluginFormatManager`; register jam_clap PluginFormat + stock VST3/AU formats.
- Sidecar resolution: known paths in Resources via `findAllTypesForFile`; scanning path
  available for user extras.
- Host `get_extension` answering `com.jreng.host-services/1` (and future com.jreng IDs)
  through the registry.
- Virtual AudioIODevice + demand-driven pump (placement open — see Open Questions).
- Real-device domain: AudioDeviceManager for third-party audio plugins.

**Session tier — one type substitution, shape preserved:**
- `jam::HashMap<UUID, std::unique_ptr<terminal::Processor>>` →
  `std::unique_ptr<AudioPluginInstance>`. Verbs survive verbatim
  (`newTerminal`/`removeTerminal` = try_emplace/erase); uuid keying, engines-persist
  contract, SESSION row authorship untouched. CLAP instantiation is main-thread; the verb
  chain already runs there.

**View tier:**
- `TerminalView : jam::PaneComponent` generalizes to plugin-editor pane shell: creates
  the hosted editor (jam_clap embed shell), parents its native container, contributes its
  container handle to the handle→pane map.
- ENDView inherits `juce::FocusChangeListener`; `globalFocusChanged` orchestration per
  the focus loop above.

**Unchanged (verified against ARCHITECTURE.md):**
ENDModel/SESSIONS topology, mirror law, Owner/Owned composite, binary-space PaneEdge
graph (Sprint 71), config::Model, jam::Model, focus singular parameters, drain sequence,
document model, resize paths. Eventually the PANE leaf's paired TERMINAL subtree is
replaced by plugin identity + opaque blob property — at terminal-extraction time, not
before.

### Terminal plugin project (final phase)

- Ordinary JUCE plugin project on jam modules, built as `.clap` via vendored
  clap-juce-extensions + standalone dev target.
- Processor side: TTY / Parser / Video / CellFifo / terminal::Model — relocated
  UNCHANGED (proven lock-free; content does not care which binary it lives in). Reader
  thread signals `request_process`; drain becomes the clocked consumer (preallocate
  `ensureScratch` worst case).
- Editor side: jam::Terminal / jam::CodeView view surface; keyboard input lands on the
  editor natively (standard plugin input path — no host→plugin input channel needed).
- `getStateInformation` = terminal::Model + CodeModel serialization; `setStateInformation`
  restores both.
- whelmed / surf inherit the identical skeleton.

### Sequencing (de-risk order: prove the boundary before moving the terminal)

```
Phase 0  jam_clap module: vendor pins (clap SSOT), clean-room format, registry,
         GUI embed, services header, plugin-side glue.
         jam_vulkan sync addendum: injected-engine seam (flag to sync driver NOW so
         the engine-acquisition point lands as a branch, not a hardcoded construct).
Phase 1  END host skeleton: build additions, FormatManager + formats, sidecar
         resolution, virtual device + demand pump, real-device domain.
         CONFORMANCE FIXTURE plugin (permanent, not throwaway — it is also the module
         project template): instantiate → clock → editor-in-pane → state round-trip →
         destroy, per format (CLAP + VST3/AU embed quirks).
Phase 2  Services crossing: HostServices through getExtension; injected engine renders
         fixture text through the HOST atlas; visual parity acceptance.
         Focus loop: dispatch outward + globalFocusChanged inward; FIRST acceptance
         test = click into embedded editor, assert nullptr transition fires and OS
         query resolves the correct pane, both platforms.
Phase 3  Terminal extraction: Source/terminal/* relocates to the plugin project;
         jam::Terminal / jam::CodeView implemented to the fullest against the proven
         boundary; PANE leaf switches to plugin identity + blob.
```

---

## BLESSED Compliance Checklist

- [x] Bounds — thread bindings preserved and ABI-enforced; bounded transports (drop-oldest, seqlock reject-not-wait); preallocation mandate carried (ensureScratch worst-case noted)
- [x] Lean — zero invented abstractions; shipped JUCE + spec'd CLAP + relocated proven code
- [x] Explicit — every crossing a declared, versioned channel; no implicit sharing
- [x] SSOT — single pinned CLAP headers both sides; per-tier model SSOT unchanged; blob as sole module-state serialization
- [x] Stateless — mirror law untouched; ephemeral editors over persistent processors
- [x] Encapsulation — module internals opaque to host; API-only communication
- [x] Deterministic — single clock owner per domain; equality-gated feedback; compiler as boundary gatekeeper

---

## Open Questions

For COUNSELOR/ARCHITECT to settle at plan time (none block design integrity):

1. **Virtual-device ownership placement** — Application vs Nexus tier owns the virtual
   AudioIODevice + demand pump. (Clock is JAM-level reusable; the owning seat in END is
   the open half.)
2. **Audio-domain processing topology** — `AudioProcessorGraph` vs per-instance
   `AudioProcessorPlayer` for third-party routing on the hardware domain.
3. **Clock device home in JAM** — which jam module owns the virtual device type (existing
   module vs jam_clap sibling concern).
4. **Names (Decision Gate at scaffold):** conformance fixture plugin name; HostServices
   struct name; jam_clap internal directory/class names; terminal plugin project name.
5. **Vendor-pin mechanics** — exact mechanism for redirecting clap-juce-extensions'
   submodule pins at the jam_clap SSOT CLAP headers (build-system detail, AppBuilder.cmake
   territory).
6. **Cross-domain audio bridge** — explicitly deferred until a feature routes audio
   between the virtual and hardware domains (drift-compensated ring buffer + resampler
   when it exists).
7. **`ensureScratch` preallocation bound** — worst-case sizing when drain relocates to
   the clocked consumer (`CellFifo.h:337-344`).
8. **ARCHITECTURE.md focus-contract amendment wording** — the mutually-exclusive
   dual-writer rule for the PANE focus report (self-report XOR proxy).
9. **VST3 hosting license posture** — decision 6 re-accepts Steinberg surface for
   *hosting* stock formats; confirm posture (hosting-only, no VST3-packaged END modules)
   is documented when the format manager lands.

---

## Handoff Notes

- **Fidelity:** every point in this RFC is in scope per RFC Fidelity Protocol — no
  filtering, no silent drops. The Decisions Ledger items are ARCHITECT-ratified and
  closed (PP-5); do not re-open them.
- **Session verification debt (fixture, not production):** the claim that END's peer
  fires `globalFocusChanged(nullptr)` when focus moves to a child native view within the
  same top-level window (WM_KILLFOCUS on peer / resignFirstResponder) is
  mechanism-asserted, NOT source-verified. It is the first acceptance test of Phase 2.
  Fallback if a platform misbehaves: plugin-side focus report through
  `com.jreng.focus/1` (the clap-helpers `extension()` hook exists one layer down;
  vendored wrapper is the patch point).
- **Coordinate with the jam_vulkan KANJUT sync (in progress):** the injected-engine seam
  (accept host `VulkanEngine*`, register as dylib-local factory) is NEW code, not part of
  the port — kuassa_vulkan only knows own-engine. Flag to the sync driver so the
  engine-acquisition point lands as a branch.
- **The conformance fixture is permanent** and doubles as the module project template —
  build it to house standard, not throwaway standard. It regression-tests every future
  jam_clap change and every new module skeleton, per format (CLAP + VST3 + AU embed).
- **Third-party conformance bar** (decision 5) means multi-bus, standard-extension
  coverage, and graceful degradation are v1 jam_clap scope — the MIT PoC's single-bus
  shortcut is explicitly NOT acceptable.
- **State-blob discipline:** never carry runtime handles in getState/setState —
  HostServices is the injection channel; the blob is serialization only.
- **END project state at handoff:** Sprint 71 landed (binary-space pane substrate);
  `PLAN-binary-space.md` open with queued work (join/swap/pane navigation, corner join
  gesture, popup menu + glassmorphism); active ODE `END.ode`; active debt
  DEBT-20260629T100000 (drawLine native-line-pipeline gap). This RFC's work is a new
  workstream — sequencing against the open PLAN queue is ARCHITECT's call.
- **Doxygen protocol applies** to all implementation phases (JAM/JUCE/project indexes,
  index-first); doxygen prose is written LAST per Writing Discipline.
- **Research provenance:** all citations in this RFC were produced this session from
  direct source reads of ~/Documents/Poems/JUCE (8.x), ~/Documents/Poems/dev/jam,
  ~/Documents/Poems/dev/endless, ~/Documents/Poems/kuassa/___lib___/kuassa_vulkan, END
  ARCHITECTURE.md, and repo inspection of free-audio/clap, clap-helpers,
  clap-juce-extensions, jatinchowdhury18/juce_clap_hosting. Line numbers are
  research-time snapshots; re-verify against current checkouts at implementation.
