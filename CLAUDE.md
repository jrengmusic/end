# END — Ephemeral Nexus Display

**Type:** GPU-accelerated terminal emulator  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 71 — Binary-Space Pane Substrate: PaneEdge Graph + Focus Contract Enforcement ✅ (2026-07-12)
- **Active ODE:** `END.ode` (opened 2026-06-29)
- **Active debt:** DEBT-20260629T100000 (`drawLine` native-line-pipeline gap)
- **Open PLAN:** `PLAN-binary-space.md` — Steps 1–5 executed (substrate, END retarget, audit, doxygen, docs sync); queued behind it: join/swap/pane navigation, corner join gesture, popup menu + glassmorphism

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
