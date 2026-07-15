# PLAN: jam_clap In-House CLAP Wrapper

**RFC:** none — objective from ARCHITECT prompt (session decision, 2026-07-13)
**Date:** 2026-07-13
**BLESSED Compliance:** verified (see Alignment)
**Language Constraints:** C++17 / JUCE / JAM (LANGUAGE.md C++/JUCE — header-preferred; 300-LOC = smell detector not portability constraint; 30/3 unchanged)

## Context

The upstream clap-juce-extensions wrapper is unfinished (FIXME nag asserts — proven root
cause of the REAPER debug-session hang at `clap-juce-wrapper.cpp:1806`, sampled evidence)
and its mixin surface (`clap_juce_audio_processor_capabilities` + `clap_properties` base
classes) breaks the JUCE AudioProcessor convention. JUCE 9 will ship native CLAP.
Decision: **fork to jam** — in-house jam_clap wrapper as a temporary bridge, pinned
reference kept, dropped entirely when JUCE supports CLAP OOTB. Plugin sources stay pure
JUCE convention: when JUCE 9 lands, remove jam_clap from MODULES, add CLAP to FORMATS,
plugins recompile untouched.

This plan supersedes the vendored-upstream-wrapper role of PLAN-END-plugin-host.md
Step 1 and amends Step 9's plugin-side glue to consume the new surface. Host-side
(Steps 4–9 format/) is untouched — same module, same vendored CLAP headers.

## Names Ledger (Decision Gate — ratified with plan approval)

- `jam::clap::ClientExtensions` — plugin-side capability interface, mirror of JUCE's
  ClientExtensions family (`juce_AudioProcessor.h:1254-1274`); discovery by one
  dynamic_cast at wrapper bind — JUCE's own documented fallback (`:1259-1262`)
  — `jam_clap/wrapper/jam_ClapClientExtensions.h`
- `jam::clap::Wrapper` — the `clap::helpers::Plugin` subclass (protocol core)
  — `jam_clap/wrapper/jam_ClapWrapper.h`
- `jam::clap::WrapperEditor` — plugin-side editor container component (upstream
  EditorWrapperComponent analog) — `jam_clap/wrapper/jam_ClapWrapperEditor.h`
- Entry/factory/descriptor — `jam_clap/wrapper/jam_ClapWrapperEntry.h`
- CLAP translation unit compiled only into `${target}_CLAP`:
  `jam_clap/wrapper/jam_ClapWrapper.cpp` (NOT part of jam_clap.cpp module unity)
- `wrapper/` joins the ratified jam_clap layout (`clap/ format/ services/ editor/`);
  upstream `extensions/` + `clap-juce-extensions/` vendor dirs are DELETED
- jam_clap module deps += `jam_data_structures` (jam::BufferSPSC for the UI→audio
  param event queue — framework OOTB, replaces upstream hand-rolled PushPopQ)

## Pinned Reference

`dev/clap-juce-extensions` checkout (16e9d4c wrapper / CLAP 1.2.7 / helpers a61bcdf —
pins recorded in jam_clap.h header comment) is the translation reference and regression
oracle. Read-only from here on. BLESSED translation, not byte-to-byte.

## Wrapper Scope (core CLAP set — YAGNI-bounded)

- Entry / factory / descriptor from `JucePlugin_*` + `CLAP_ID`/`CLAP_FEATURES` defines
- Audio ports from JUCE buses — **zero-port legal** (WHELMED/Step 16 dependency,
  ext/audio-ports.h:10); note ports from acceptsMidi/producesMidi
- Params: JUCE param → clap_id (paramID hashCode scheme, upstream-compatible);
  **bypass implemented properly** — plugin `getBypassParameter()` or wrapper-owned
  `AudioParameterBool`, flagged `CLAP_PARAM_IS_BYPASS` (clap/ext/params.h:152),
  process branch per VST3 wrapper parity (`juce_audio_plugin_client_VST3.cpp:674-689,
  3718-3730`). No FIXME nags — implemented, not deferred.
- State over clap streams ↔ get/setStateInformation
- Process: block-split event loop, float + double, transport → AudioPlayHead
  (wrapper is the playhead), MIDI in/out, suspended → clear, latency/tail
- GUI: create/destroy/set_parent (cocoa/win32)/set_scale/size/resize negotiation;
  JUCE ≥ 8 path only (no VST_Wrapper.mm stub, no JUCE 6/7 guards, no Linux)
- ClientExtensions surface (minimal, proven consumers only):
  - `getExtension (const char* id)` — plugin answers jam extension IDs
    (host → plugin channel)
  - `hostExtension` query function — plugin queries host IDs
    (`com.jreng.host-services/1`, Step 9 consumer)
  - `requestProcess` function — demand-clock nudge (Step 16 consumer)
- Dropped: note expressions, voice info, remote controls, preset-load, param
  indication, direct process, timer/posix-fd — no consumer exists (YAGNI)

## Validation Gate

Each step validated by @Auditor before the next begins: MANIFESTO.md (BLESSED),
NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, locked PLAN decisions. ARCHITECT builds
(`ninja`) — agents never build. Engineer prompts carry: doxygen-first reading, no
doxygen authoring, no plan/RFC-citing comments, zero identifier latitude, jam house
anatomy (umbrella-owned includes, zero-include submodule headers, unity cpp,
`/*____*/` separators, END OF NAMESPACE closure).

## Steps

### Step 1: Vendor prune + plugin purity (DELETE FIRST)
**Scope:** `jam_clap/extensions/` (DELETE 3 files), `jam_clap/clap-juce-extensions/`
(DELETE dir), `jam_clap/jam_clap.h`, WHELMED + VANILLA `PluginProcessor.h/.cpp`
**Action:** Delete upstream wrapper sources and mixin header from jam_clap. jam_clap.h:
deps += jam_data_structures; pin record comment. WHELMED/VANILLA processors drop the
two mixin bases and the direct `<clap-juce-extensions/...>` include — pure
`juce::AudioProcessor`. Bypass parameter stays (real param, exercises the param path).
VST3/AU/Standalone still build; CLAP target intentionally broken until Step 3.
**Validation:** zero references to `clap_juce_extensions::` outside pinned reference
repo; plugin classes pure JUCE.

### Step 2: ClientExtensions surface
**SUPERSEDED (Sprint 76, 2026-07-13):** The virtual-setter shape below
(`getExtension`/`setHostExtension`/`setRequestProcess`) was reshaped to a
`jam::Function::Map`-backed surface — `jam::clap::ClientExtensions` now exposes
`queryHostExtension` and `requestProcess` accessor methods over a
`jam::Function::Map<juce::String, void> extensions` member, in
`jam_ClapClientExtensions.h`.
**Scope:** `jam_clap/wrapper/jam_ClapClientExtensions.h`, `jam_clap/jam_clap.h`
**Action:** `jam::clap::ClientExtensions` per Names Ledger — mirrors JUCE
VST3ClientExtensions exactly (struct of virtuals, plugin overrides, default no-ops;
wrapper injects host access via setter-virtuals — the `setIHostApplication` idiom,
`juce_VST3ClientExtensions.h:89-97`). Members (ARCHITECT-ratified form, 2026-07-13):
- `virtual const void* getExtension (const char* id)` — host → plugin (plugin overrides)
- `virtual void setHostExtension (std::function<const void*(const char*)>)` — wrapper
  injects the host-query fn (plugin → host); ratified name
- `virtual void setRequestProcess (std::function<void()>)` — wrapper injects the
  demand-clock nudge (plugin → host); ratified name
Zero includes (house anatomy — std::function via umbrella). Umbrella header includes it.
**Validation:** mirror of JUCE ClientExtensions family shape; no clap C types leak
into the plugin-facing surface; zero submodule-header includes.

### Step 3: Wrapper skeleton + PluginBuilder rewire (buildable)
**Scope:** `jam_clap/wrapper/jam_ClapWrapperEntry.h`, `jam_ClapWrapper.h`,
`jam_ClapWrapper.cpp`, `jam/cmake/PluginBuilder.cmake`
**Action:** `jam::clap::Wrapper` : `clap::helpers::Plugin<Ignore, Minimal>` —
descriptor/factory/entry, create via `createPluginFilter()`, init/activate/
deactivate/reset, audio ports from buses (zero-port legal), note ports.
PluginBuilder: delete `add_subdirectory` of source repo + ClapTargetHelpers include;
CLAP section creates `${target}_CLAP` MODULE compiling `jam_ClapWrapper.cpp`,
CLAP_ID/CLAP_FEATURES/version as compile definitions, macOS bundle from
`jam_clap/cmake/macos_bundle/`, post-build copy to `~/.config/end/plugins/` kept.
**Validation:** VANILLA.clap loads in REAPER (no params, no GUI yet — legal);
clap-helpers defaults cover unimplemented extensions.

### Step 4: Params + bypass + state
**Scope:** `jam_ClapWrapper.h/.cpp`
**Action:** Param registration (hashCode clap IDs), params info/value/text/flush,
UI→audio event queue on `jam::BufferSPSC`, gesture begin/end, bypass per Wrapper
Scope (owned-or-plugin param, `CLAP_PARAM_IS_BYPASS`), state save/load over clap
streams.
**Validation:** no shadow state; queue lock-free; bypass parity with VST3 wrapper
semantics; REAPER shows bypass + plugin params, state round-trips.

### Step 5: Process + transport + MIDI + latency/tail
**Scope:** `jam_ClapWrapper.h/.cpp`
**Action:** Block-split process loop honoring event timestamps, float/double,
suspended → clear, bypass branch → `processBlockBypassed`, transport population
(AudioPlayHead), MIDI in/out, latency + tail extensions. `requestProcess` wired.
**Validation:** RT contract — zero allocation in process path; audio passthrough
audible in REAPER; no debug-trap under DAP (the original defect, dead by design).

### Step 6: GUI embed
**Scope:** `jam_ClapWrapperEditor.h`, `jam_ClapWrapper.h/.cpp`
**Action:** `jam::clap::WrapperEditor` container; gui extension: api support,
create/destroy, set_parent (cocoa/win32 via juce detail utilities), set_scale,
get_size/adjust/set_size honoring editor constrainer, two-way resize
(`host.gui_request_resize` ↔ childBoundsChanged).
**Validation:** WHELMED editor renders markdown in REAPER; resize negotiation stable;
editor lifecycle bound (B).

### Step 7: Acceptance + docs sync
**Scope:** REAPER conformance pass; `PLAN-END-plugin-host.md`, `CLAUDE.md`
**Action:** VANILLA + WHELMED full sequence: instantiate → params/bypass → state
round-trip → GUI → audio → destroy; DAP session clean. PLAN-END-plugin-host.md gains
supersession note (Step 1 vendored-wrapper role, Step 9 glue consumes
ClientExtensions). Doxygen = separate dedicated delegation LAST, after audit,
on ARCHITECT's word.
**Validation:** full-contract Auditor pass over jam_clap/wrapper + PluginBuilder.

## BLESSED Alignment

- **B** — wrapper owns processor (unique_ptr, RAII); editor container owns hosted
  editor; one TU per CLAP binary; thread bindings per CLAP spec threads
- **L** — core set only, dropped extensions have zero consumers (YAGNI); helpers do
  protocol plumbing (no hand-rolled boilerplate); single-responsibility files
- **E** — no mixin magic on AudioProcessor; capability surface explicit
  (ClientExtensions); no bail-outs — clap-helpers boundary returns are result returns
- **S (SSOT)** — one vendored CLAP header set (jam_clap/clap) for wrapper AND host
  format; pins recorded once
- **S (Stateless)** — wrapper holds protocol-transient state only; param truth lives
  in JUCE params
- **E (Encapsulation)** — plugins ignorant of CLAP; wrapper ignorant of plugin
  internals; jam::BufferSPSC reused not reinvented
- **D** — deterministic event-split process loop; same input → same blocks

## Risks

- GUI resize negotiation is the fiddliest surface (upstream comments cite
  sub-pixel host loops) — translated with the constrainer clamp, tested in REAPER
  before acceptance
- REAPER is the only third-party conformance host on hand; Bitwig/other host quirks
  deferred until encountered
- Double-precision path has no in-house consumer — kept for honest translation,
  exercised only if a host requests CLAP_AUDIO_PORT_SUPPORTS_64BITS
