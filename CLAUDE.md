# END — Ephemeral Nexus Display

**Type:** GPU-accelerated terminal emulator  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 56 — User Shader Pipeline — Background render() + Post-Process Chain ✅ (2026-07-02)
- **Active ODE:** `END.ode` (opened 2026-06-29)
- **Active debt:** DEBT-20260629T100000 (`drawLine` native-line-pipeline gap) · DEBT-20260702T152430 (shader opacity — fixed this sprint, drains at log)
- **Open PLAN:** `PLAN-vulkan-hpp-adoption.md` — Steps 1–7 executed (vk:: sweep + opacity fix), pending ARCHITECT build gate + log

## Layer Order (top → bottom)
`Application → Config → Nexus → terminal::Controller → Logic → Model → View → TTY`  
Graphics: JAM `jam_vulkan` (vulkan-hpp plain `vk::`, vendored SDK 1.4.350; Registry owned by Application)  
Config: `Source/config/` — Directory, Model (four-phase lifecycle)  
UI: `Source/end/` — View, Window, Tabs, Panes  
Constants: `Identifier.h` · `Bimap.h`

## Key Docs
| File | Purpose |
|------|---------|
| `ARCHITECTURE.md` | Architectural contracts, invariants, layer rules (SSOT) |
| `SPEC.md` | Requirements v0.0.1 |
| `DEBT.md` | Active debt ledger |
| `carol/SPRINT-LOG.md` | Cross-session memory (last 5 sprints) |
| `END.ode` | Active ODE investigation |

## Doxygen (mandatory before any code task)
- Project: `docs/xml/index.xml`
- JAM: `~/Documents/Poems/dev/jam/docs/xml/index.xml`
- JUCE: `~/Documents/Poems/JUCE/docs/xml/index.xml`
- KANJUT / CIUM: not used in this project

## Build (ARCHITECT only — agents never run)
`ninja` via `Builds/` · `ninja doxygen` to regenerate docs
