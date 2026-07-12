# PLAN: END Plugin Host — CLAP Formalization of In-House Modules

**RFC:** RFC-end-plugin-host.md (+ INVENTORY-end-plugin-host.md as API ground truth)
**Date:** 2026-07-12
**Status:** APPROVED (ARCHITECT, 2026-07-12) · AMENDED (ARCHITECT, 2026-07-13 — WHELMED viewer inserted as first plugin)
**BLESSED Compliance:** verified (see Alignment)
**Language Constraints:** C++17 / JUCE / JAM (LANGUAGE.md C++/JUCE — header-preferred, 300-LOC smell not portability constraint; 30/3 unchanged)

## Context

END is formalized as an actual plugin host; in-house modules are actual CLAP plugins
(RFC Decisions 1–13, closed). This PLAN covers ALL RFC phases (ARCHITECT-ratified this
session). The RFC's plan-time open questions were settled this session:

| RFC OQ | Decision (ARCHITECT, this session) |
|---|---|
| 1 Virtual-device owner | **Nexus** |
| 2 Audio topology | **AudioProcessorGraph** (+ one AudioProcessorPlayer driving it) |
| 3 Clock home in JAM | **New module `jam_audio_devices`**, `audio_io/` subdir |
| 4 Names | Ledger below; fixture = `dev/___boilerplate___/___vanilla___/` VANILLA `com.jreng.vanilla`; plugin projects at `dev/plugins/`; terminal = `dev/plugins/terminal/` TERMINAL `com.jreng.terminal` |
| 5 Vendor-pin mechanics | Vendored snapshot copies inside jam_clap (jam_vulkan/vulkan pattern); include-path order makes jam_clap/clap the SSOT for the vendored extensions wrapper |
| 6 Cross-domain bridge | Deferred (RFC decision 7, unchanged) |
| 7 ensureScratch bound | Superseded — terminal plugin is built FROM SCRATCH; drain-side storage designed preallocated from day one |
| 8 ARCHITECTURE.md amendment | Step 19 |
| 9 VST3 license posture | Step 19 documents hosting-only posture |

**RFC supersession (ARCHITECT, this session):** END's `Source/terminal/*` is stubs only
(5 files verified: TerminalProcessor.h/.cpp, TerminalModel.h, TerminalView.h/.cpp).
NOTHING relocates. Stubs are DELETED (delete-first discipline); "no more terminal at
END". The terminal plugin is built from scratch at `dev/plugins/terminal/`.

**Amendment (ARCHITECT, 2026-07-13 — first-plugin sequencing):** WHELMED
(markdown/mermaid viewer) is the FIRST real plugin, inserted as Phase 2 immediately
after the VANILLA fixture. TERMINAL remains the capstone (Phase 4). Grounds,
evidence-verified in session: jam_markdown (9,427 lines) and jam_mermaid (24,846
lines) are API-complete, already linked in END, with zero consumers and zero runtime
evidence — first consumption IS their validation; the viewer render path
(parse → layout → draw, juce::Graphics) needs no focus loop and no services crossing,
so it functions at Phase 1 host maturity; TERMINAL is gated on in-flight jam_terminal
state-management/transport work plus the focus loop. WHELMED EDITOR scope
(TextModel/CodeView editing) converges after the focus loop lands (Step 19) — out of
viewer-step scope. Zero-audio-port legality and mandatory process() verified against
tag-pinned CLAP 1.2.7 headers (ext/audio-ports.h:10, plugin.h:96).

## Names Ledger (Decision Gate — ratified with plan approval)

Ratified in session:
- Module: `jam_clap` (RFC) · Module: `jam_audio_devices` with `audio_io/`
- `jam::clap::Services` — services struct, extension ID `com.jreng.host-services/1`
- Fixture: `~/Documents/Poems/dev/___boilerplate___/___vanilla___/`, product VANILLA, `com.jreng.vanilla` (mirrors `kuassa/___boilerplate___/___vanilla___/` shape)
- Plugin projects root: `~/Documents/Poems/dev/plugins/`; terminal: `dev/plugins/terminal/`, TERMINAL, `com.jreng.terminal`
- Namespace: `jam::clap::` (follows `jam::vulkan::` house pattern)
- END: `EditorView` : jam::PaneComponent (replaces TerminalView — the pane shows the plugin's editor)

Ratified by plan approval:
- `jam::clap::PluginFormat` : juce::AudioPluginFormat — `format/jam_ClapPluginFormat.h`
- `jam::clap::PluginInstance` : juce::AudioPluginInstance — `format/jam_ClapPluginInstance.h`
- `jam::clap::PluginWindow` : juce::AudioProcessorEditor — `format/jam_ClapPluginWindow.h` (embed shell; VST3PluginWindow / AudioPluginHost PluginWindow analog)
- `jam::clap::Editor` — plugin-side glue base (injected-engine / own-engine branch) — `editor/jam_ClapEditor.h`
- `jam::VirtualDevice` / `jam::VirtualDeviceType` — `jam_audio_devices/audio_io/jam_VirtualDevice.h`
- END: Session map member `plugins`; verbs `newPlugin`/`removePlugin` (replace newTerminal/removeTerminal); identifiers registered in Identifier.h
- jam_clap layout: `clap/` `helpers/` `extensions/` `format/` `services/` `editor/`
- Extension registry = plain member table (`jam::HashMap<juce::String, const void*>`) on the host side — a table, not a class (Lean)

Ratified with 2026-07-13 amendment:
- Plugin: `dev/plugins/whelmed/`, product WHELMED, `com.jreng.whelmed`

**Accepted deviation (RFC-ratified, confined):** `services/jam_ClapServices.h` uses
forward-declared pointer types ONLY (RFC scaffold: jam_clap takes NO hard dependency on
jam_vulkan; both sides include the same header; the compiler is the contract). This is a
deliberate, documented exception to JRENG "No Forward Declarations", scoped to this one
header.

## Validation Gate

Each step is validated by @Auditor before the next begins, against: MANIFESTO.md
(BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and this PLAN's locked
decisions. ARCHITECT builds (`ninja`) — agents never run builds. Engineer delegation
carries: doxygen-first reading (JAM/JUCE/project indexes), Librarian findings prepended,
no doxygen authoring, no plan/RFC-citing comments, zero identifier latitude.

## Steps

### Phase 0 — jam_clap + jam_audio_devices (JAM capital expenditure)

**Step 1: jam_clap module skeleton + vendor pins**
**Scope:** `~/Documents/Poems/dev/jam/jam_clap/` (new), `jam/cmake/AppBuilder.cmake`
**Action:** Create module per house anatomy (topmost `jam_clap.h` with
BEGIN_JUCE_MODULE_DECLARATION, zero-include submodules). Vendor pinned snapshots:
`clap/` = CLAP 1.2.7 headers (from clap-juce-extensions pin `29ffcc2`), `helpers/` =
clap-helpers `a61bcdf`, `extensions/` = clap-juce-extensions `16e9d4c` wrapper sources.
AppBuilder.cmake gains `if(_ca_mod STREQUAL "jam_clap")` include-path block (BEFORE
ordering so `jam_clap/clap` is header SSOT for the vendored wrapper — same isolation as
the shaderc block, AppBuilder.cmake:576-590 precedent; header-only, no IMPORTED lib).
**Validation:** module anatomy matches jam_core/jam_vulkan pattern; pins recorded; END
still builds with module linked but unused.

**Step 2: jam_audio_devices module + VirtualDevice**
**Scope:** `~/Documents/Poems/dev/jam/jam_audio_devices/` (new)
**Action:** Module skeleton (deps: juce_audio_devices, jam_core).
`audio_io/jam_VirtualDevice.h`: `jam::VirtualDevice` : juce::AudioIODevice +
`jam::VirtualDeviceType` : juce::AudioIODeviceType — MockDevice shape
(juce_AudioDeviceManager.cpp:1888): `start()` stores callback +
`audioDeviceAboutToStart`, no thread; owner clocks blocks by invoking
`audioDeviceIOCallbackWithContext`. Public clocking verb (e.g. `clock (numSamples)`) —
demand-driven, caller-owned timing.
**Validation:** no bail-outs, RAII, MockDevice shape mirrored not depended on; NAMES.

**Step 3: Services header**
**Scope:** `jam_clap/services/jam_ClapServices.h`
**Action:** `jam::clap::Services` — pointer fields for VulkanEngine, GlyphAtlas,
Typeface, Stamp, Grapheme, Link (forward-declared — accepted deviation above) + the
`com.jreng.host-services/1` ID constant. Both sides include this one header.
**Validation:** zero hard jam_vulkan dependency; ID constant named, no magic strings.

**Step 4: Host format — DSO/entry/factory + type discovery**
**Scope:** `jam_clap/format/`
**Action:** `jam::clap::PluginFormat`: DSO loader (DynamicLibrary / CFBundle →
`clap_entry` → init → `get_factory(CLAP_PLUGIN_FACTORY_ID)`; PoC map
CLAPPluginFormat.cpp:107-287), `findAllTypesForFile` (whole-factory enumeration),
`fileMightContainThisPluginType` (.clap + existence), `searchPathsForPlugins`,
known-path loading (no scanning required — Decision 10), PluginDescription fill.
**Validation:** clean-room vs spec headers (PoC as map only); result returns; lookup
tables over branch chains.

**Step 5: PluginInstance — lifecycle + process + multi-bus**
**Scope:** `jam_clap/format/jam_ClapPluginInstance.h`
**Action:** Full CLAP lifecycle mapping (init→activate→start_processing→process→
stop_processing→deactivate→destroy) on `juce::AudioPluginInstance`. `clap_process`
populated honestly: steady_time accumulated, transport wired (nullable = free-running),
event queues via vendored helpers. **Multi-bus:** port config from `clap.audio-ports`
(counts/channel maps → BusesLayout; isBusesLayoutSupported/applyBusLayouts real) —
third-party conformance bar, PoC's 1-in/1-out explicitly rejected (RFC handoff).
**Validation:** thread contract (activate/extension queries main-thread asserted;
process audio-thread clean); no allocation in process path.

**Step 6: PluginInstance — params + state**
**Scope:** same
**Action:** Params → `HostedAudioProcessorParameter` bridges; full rescan-flag handling
(VALUES/TEXT/INFO/ALL); RT-safe host→plugin param queue (vendored
reducing-param-queue). State over clap_istream/ostream ↔ MemoryBlock (PoC map
:900-961). `mark_dirty` → `updateHostDisplay (withNonParameterStateChanged (true))`.
**Validation:** no shadow state; event payload consumed (no re-derive).

**Step 7: Host callbacks + extension registry**
**Scope:** same + format host-struct wiring
**Action:** `clap_host_t` population; `get_extension` answered through a string-ID
table (`jam::HashMap<juce::String, const void*>`) — standard extensions (log →
`jam::debug::Log`, thread-check, params, state, timer-support, posix-fd, gui,
audio-ports, note-ports) AND `com.jreng.*` IDs through the same table; no strcmp
chains. `request_callback` → message-thread pump; **`request_process` → owning clock
hook (the demand-driven crossing — new code, no PoC reference)**; `request_restart` →
deactivate/reactivate cycle.
**Validation:** registry is data not branches (3-branch rule); every host extension
struct actually defined (no PoC-style commented stubs).

**Step 8: GUI embed — PluginWindow**
**Scope:** `jam_clap/format/jam_ClapPluginWindow.h`
**Action:** `jam::clap::PluginWindow` : AudioProcessorEditor over HWNDComponent
(Windows) / NSViewComponentWithParent (macOS): resolve native handle →
`clap.gui` is_api_supported → create → set_scale → get_size → **set_parent
(clap_window)** → show; two-way resize (`clap_host_gui.request_resize` ↔
componentMovedOrResized) per VST3PluginWindow reference
(juce_VST3PluginFormat.cpp:460-497, :354/:415).
**Validation:** lifecycle bound (destroy on editor death); no leaked native views.

**Step 9: Plugin-side glue — jam::clap::Editor**
**Scope:** `jam_clap/editor/jam_ClapEditor.h`
**Action:** Editor base for in-house modules: queries
`getExtension ("com.jreng.host-services/1")` (clap-juce-extensions capabilities
crossing, working example HostSpecificExtensionsPlugin.cpp:10-12); injected-engine
mode registers HOST VulkanEngine* as dylib-local `externalContextFactory`; own-engine
fallback via `SharedResourcePointer` Shared lifecycle (**arrives with the KANJUT
jam_vulkan sync — ARCHITECT's workstream; this step consumes the seam, does not build
it**); branch selected at editor construction.
**Validation:** one-time injection only (no runtime handles in state blob); B ownership.

### Phase 1 — END host skeleton + VANILLA fixture

**Step 10: END build additions**
**Scope:** `end/CMakeLists.txt` (configure_app :81-114)
**Action:** MODULES += juce_audio_basics, juce_audio_devices, juce_audio_formats,
juce_audio_processors_headless, juce_audio_processors, juce_audio_utils;
`JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1` (macOS); JAM_MODULES += jam_clap,
jam_audio_devices.
**Validation:** builds; flags via user-settable macros only (never JUCE_INTERNAL_HAS_*).

**Step 11: Nexus host machinery**
**Scope:** `Source/Nexus.h/.cpp`
**Reference implementation:** `JUCE/extras/AudioPluginHost/` — MainHostWindow
(FormatManager + KnownPluginList + AudioDeviceManager wiring), PluginGraph
(Graph + Player + state round-trip), Source/UI/PluginWindow.h (hosted-editor
lifecycle). Engineer reads it before writing.
**Action:** Nexus owns: `AudioPluginFormatManager` (addFormat: jam::clap::PluginFormat +
VST3PluginFormat + AudioUnitPluginFormat); sidecar resolution (known Resources paths via
findAllTypesForFile); `jam::clap::Services` answered through the host extension table
(Application fills pointers once at init — it owns them all); **VirtualDevice + demand
pump (Nexus-owned — ratified)**; hardware domain: AudioDeviceManager +
AudioProcessorGraph + one AudioProcessorPlayer (ratified). Two clock domains coexist
(Decision 7); no cross-domain bridge.
**Validation:** ownership tree explicit; host state save/restore follows PluginGraph
pattern (Base64 blob into ENDModel row — PluginGraph.cpp:379-382/:474-477).

**Step 12: Session-tier substitution + terminal stub deletion (DELETE FIRST)**
**Scope:** `Source/terminal/*` (DELETE all 5 files), `Source/end/Session.h/.cpp`,
`Source/end/TabView.*`, `Source/action/ENDActions.cpp`,
`Source/end/EventRegistration.cpp`, `Source/Identifier.h`
**Action:** Delete `Source/terminal/` entirely. Session:
`jam::HashMap<jam::UUID, std::unique_ptr<juce::AudioPluginInstance>> plugins`;
verbs `newPlugin`/`removePlugin` = try_emplace/erase (instantiation via Nexus
FormatManager — main-thread, verb chain already there). PANE leaf gains plugin identity
+ opaque blob property. Compiler errors are the removal inventory (Refactor-Rewrite
Discipline).
**Validation:** zero remaining `terminal` references in END sources; uuid keying,
engines-persist contract, SESSION authorship untouched (RFC "Unchanged" list).

**Step 13: View tier — EditorView**
**Scope:** `Source/end/EditorView.h/.cpp` (new), `Source/end/TabView.cpp`
**Reference implementation:** AudioPluginHost `Source/UI/PluginWindow.h` —
hosted-editor creation/ownership/teardown (adapted: pane-embedded, not top-level
window).
**Action:** `EditorView` : jam::PaneComponent — creates the hosted editor
(jam::clap::PluginWindow / stock format editor via createEditorAndMakeActive), parents
its native container, contributes its container handle to the handle→pane map.
`TabView::createChild` returns EditorView.
**Validation:** mirror law (component tree pure function of state tree); Attachment
contract; editor ephemeral over persistent processor.

**Step 14: VANILLA conformance fixture (permanent)**
**Scope:** `~/Documents/Poems/dev/___boilerplate___/___vanilla___/` (new project)
**Action:** Complete clonable JUCE plugin project mirroring
`kuassa/___boilerplate___/___vanilla___/` shape — PluginProcessor/PluginEditor on jam
modules, built as `.clap` via vendored clap-juce-extensions
(`clap_juce_extensions_plugin`, CLAP_ID `com.jreng.vanilla`) + VST3/AU + Standalone
dev target (own-engine branch exercised by construction — Decision 8). Acceptance
sequence per format: instantiate → clock → editor-in-pane → state round-trip → destroy.
**Validation:** house standard, not throwaway (permanent fixture + template);
Auditor full-contract pass.

### Phase 2 — WHELMED viewer plugin (first plugin — 2026-07-13 amendment)

**Step 15: WHELMED project scaffold**
**Scope:** `~/Documents/Poems/dev/plugins/whelmed/` (new — clone of ___vanilla___)
**Action:** Clone VANILLA; CLAP_ID `com.jreng.whelmed`, product WHELMED. Links
jam_markdown + jam_mermaid (jam_markdown's declared dependency) on top of the template
modules.
**Validation:** clone discipline (metadata edits only at this step).

**Step 16: Processor side — document source**
**Scope:** WHELMED Source/
**Action:** Processor owns the document source: file path + content snapshot as state
(`getStateInformation`/`setStateInformation` via copyXmlToBinary/getXmlFromBinary
pattern). Zero audio ports — `clap.audio-ports` not implemented (legal:
ext/audio-ports.h:10); `process()` present and trivial (structurally mandatory:
plugin.h:96). Document loading is main-thread; no RT machinery invented for a workload
that has none (Lean).
**Validation:** state round-trip; no bail-outs; no shadow state.

**Step 17: Editor side — viewer, first consumption of jam_markdown/jam_mermaid**
**Scope:** WHELMED editor
**Action:** Editor on jam::clap::Editor base. Read-only render path:
`jam::markdown::Document` lifecycle (parse → layout → draw → getBounds) inside a
`juce::Viewport`; mermaid fences render through `jam::mermaid::Diagram`. Scroll only —
no caret, no editing, no keyboard requirement. Acceptance: CommonMark + GFM corpus
(table, strikethrough, tasklist, autolink) plus at least one diagram per implemented
mermaid type renders without assert/crash. Defects surfaced are jam findings — fixed
at source in jam_markdown/jam_mermaid; this plugin is the first-run harness for
34,273 previously unexecuted lines.
**Validation:** viewer functions with NO services crossing and NO focus loop — Phase 1
host maturity proven by a real plugin; editor ephemeral over persistent processor.

### Phase 3 — services/rendering + focus crossings proven

**Step 18: HostServices crossing + visual parity**
**Scope:** VANILLA editor glue, Nexus extension table, Application init
**Action:** VANILLA's Editor (jam::clap::Editor base) receives host `Services` through
`com.jreng.host-services/1`; registers host engine as dylib-local factory; fixture text
renders through the HOST GlyphAtlas. Acceptance: visual parity GPU path, injected vs
own-engine; externalContextFactory patch regression observable here.
**Validation:** sharing per-process for in-house, per-binary isolation for third-party;
state blob carries zero runtime handles.

**Step 19: Focus loop + ARCHITECTURE.md amendment + license posture**
**Scope:** `Source/end/ENDView.h/.cpp`, `ARCHITECTURE.md`
**Action:** Outward: `focused_pane` change dispatches OS keyboard focus to the pane's
embedded native view (existing contract carries it). Inward: ENDView inherits
`juce::FocusChangeListener`; `globalFocusChanged (nullptr)` → OS query (GetFocus /
key-window firstResponder) → parent-chain walk → handle→pane map → proxy-write that
PANE's focus report; equality gate kills the spiral. **FIRST acceptance test: click
into embedded editor, nullptr transition fires, correct pane resolves — both
platforms.** Platform misbehaves → fallback `com.jreng.focus/1` plugin-side report
(clap-helpers `extension()` hook, vendored wrapper is the patch point).
ARCHITECTURE.md: dual-writer focus rule (JUCE-tenant self-report XOR native-tenant
proxy), host-tier documentation, VST3 posture note (hosting-only; no VST3-packaged END
modules — Steinberg surface accepted for hosting per Decision 6).
**Validation:** no second writer of any `focused_*` parameter; ARCHITECTURE.md mirrors
landed code only.

### Phase 4 — TERMINAL plugin from scratch

**Step 20: TERMINAL project scaffold**
**Scope:** `~/Documents/Poems/dev/plugins/terminal/` (new — clone of ___vanilla___)
**Action:** Clone VANILLA; CLAP_ID `com.jreng.terminal`, product TERMINAL. Links
jam_terminal (TTY), jam modules.
**Validation:** clone discipline (metadata edits only at this step).

**Step 21: Processor side — built from scratch**
**Scope:** TERMINAL Source/
**Action:** New processor implementation on the proven lock-free shape (endless
architecture as evidence, not source): PTY reader thread (jam_terminal) → parser/video →
SPSC transport (jam::BufferSPSC) → **drain as the clocked consumer inside processBlock**;
reader signals `request_process` (continuous under flood — back-to-back blocks,
drop-oldest guarantees reader never stalls; idle = zero blocks). All drain-side storage
preallocated at prepareToPlay (supersedes RFC OQ 7). `getStateInformation` /
`setStateInformation` = model `getXml()`/`replaceState()` via
copyXmlToBinary/getXmlFromBinary.
**Validation:** RT contract — zero allocation/locks/blocking in process; thread
bindings ABI-enforced; BLESSED Bounds.

**Step 22: Editor side — jam::Terminal / jam::CodeView to the fullest**
**Scope:** TERMINAL editor + jam_gui/jam_terminal modules as needed
**Action:** Editor on jam::clap::Editor base; jam::Terminal / jam::CodeView implemented
to the fullest against the proven boundary; keyboard input lands on the editor natively
(standard plugin input path — no host→plugin input channel).
**Validation:** CodeView TETRIS E-contract (dumb widget, cell-space API only).

**Step 23: END integration + closing docs**
**Scope:** END Resources sidecar, ARCHITECTURE.md/CLAUDE.md sync, dedicated doxygen task
**Action:** TERMINAL sidecar-bundled (ConPTY.dll pattern); default pane plugin wired
through newPlugin. Docs sync to landed reality. THEN one dedicated doxygen delegation
(headers only, zero warnings) — last, after audit.
**Validation:** full-system Auditor pass; surf inherits the skeleton (no work, just
verified cloneability); WHELMED editor scope (editing on TextModel/CodeView) follows
the focus loop per Context amendment.

## BLESSED Alignment

- **B** — thread bindings compiler/ABI-enforced at the plugin boundary; every resource
  owner named per step; RAII throughout; VirtualDevice clock single-owner (Nexus).
- **L** — zero invented abstractions: shipped JUCE + spec'd CLAP + JAM patterns;
  extension registry is a table, not a class; no speculative bridge (Decision 7);
  WHELMED viewer invents no RT machinery for a non-RT workload.
- **E** — every crossing a declared, versioned channel; one documented deviation
  (services header forward declarations, RFC-ratified, single file).
- **S (SSOT)** — one pinned CLAP header set both sides; blob = sole module-state
  serialization; handle→pane map single-owner.
- **S (Stateless)** — editors ephemeral over persistent processors; mirror law
  untouched.
- **E (Encapsulation)** — module internals opaque (blob under PANE); tell-don't-ask
  verb chains preserved verbatim.
- **D** — demand-driven clock single owner per domain; equality-gated focus loop;
  compiler as boundary gatekeeper.

## Risks

- `externalContextFactory` JUCE patch is uncommitted — pin rides ARCHITECT's KANJUT
  sync; Step 18 is its regression surface.
- `globalFocusChanged (nullptr)` mechanism-asserted only — Step 19 FIRST acceptance
  test; fallback path named.
- Step 9 own-engine fallback depends on the KANJUT jam_vulkan sync landing the Shared
  lifecycle — sequencing owned by ARCHITECT.
- jam_markdown (9,427 lines) + jam_mermaid (24,846 lines) have zero runtime evidence —
  Step 17 is deliberately their first-run harness; defect volume unknown, fixed at
  source in jam. Known open item: flowchart label-collision system unimplemented
  (layout/jam_MermaidFlowchartEdges.cpp, comment-only, non-blocking for acceptance).
