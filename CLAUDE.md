# END — Ephemeral Nexus Display

**Type:** GPU-accelerated terminal emulator  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 59 — Conformance Suite Green + PLAN/RFC Retirement ✅ (2026-07-03)
- **Active ODE:** `END.ode` (opened 2026-06-29)
- **Active debt:** DEBT-20260629T100000 (`drawLine` native-line-pipeline gap)
- **Open PLAN:** none — `PLAN-vt-correctness.md` and `RFC-vt-correctness.md` deleted (fully executed, audited, verified; conformance suite is the surviving artifact at `tests/`)

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
