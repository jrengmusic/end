# END — Ephemeral Nexus Display

**Type:** GPU-accelerated terminal emulator  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 76 — jam_clap In-House CLAP Wrapper Steps 1–6 ✅ (2026-07-13)
- **Active ODE:** `END.ode` (opened 2026-06-29)
- **Active debt:** none (ledger clean)
- **Open PLANs:**
  - `PLAN-jam-clap-wrapper.md` — Steps 1–6 executed (vendor prune, ClientExtensions, wrapper skeleton + PluginBuilder, params/bypass/state, process/transport/MIDI/latency/tail, GUI embed); Step 7 in progress (acceptance + docs sync)
  - `PLAN-END-plugin-host.md` — Phase 0 Step 1 superseded by jam_clap wrapper; Steps 2–23 pending
  - `PLAN-join-swap-navigation.md` — Steps 1–3 executed; Steps 4–8 pending

## Layer Order (top → bottom)
`Application → Config → Nexus → Session → terminal::Processor/View`  
Graphics: JAM `jam_vulkan` (vulkan-hpp plain `vk::`, vendored SDK 1.4.350; `jam::VulkanEngine` owned by Application — unified resource-ownership tree for Typeface/Stamp/Grapheme/Link, Device, GlyphAtlas)  
Config: `Source/config/` — Directory, Model (four-phase lifecycle)  
UI: `Source/end/` — ENDView, Window, SessionView, TabView (jam Owner/Owned composite; panes = binary space graph, `jam::PaneEdge` EDGE rows)  
Constants: `Identifier.h` · `Bimap.h`

## Key Docs
| File | Purpose |
|------|---------|
| `ARCHITECTURE.md` | Architectural contracts, invariants, layer rules (SSOT) |
| `SPEC.md` | Requirements v0.0.1 |
| `DEBT.md` | Active debt ledger |
| `SHADERS.md` | Shader system guide — Shadertoy, RetroArch Slang, OBJ mesh + iMouse |
| `carol/SPRINT-LOG.md` | Cross-session memory (last 5 sprints) |
| `END.ode` | Active ODE investigation |

## Doxygen (mandatory before any code task)
- Project: `docs/xml/index.xml`
- JAM: `~/Documents/Poems/dev/jam/docs/xml/index.xml`
- JUCE: `~/Documents/Poems/JUCE/docs/xml/index.xml`
- KANJUT / CIUM: not used in this project

## Build (ARCHITECT only — agents never run)
`ninja` via `Builds/` · `ninja doxygen` to regenerate docs
