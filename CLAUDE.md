# END — Ephemeral Nexus Display

**Type:** GPU-accelerated CLAP plugin host (terminal lives in the hosted `eve` plugin)  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 85: Post-CAST Runtime Recovery — Window Style, Event-Driven Focus, Corner Bimap, Mesh Restore ✅ (2026-09-12) — runtime verified (tabs, splits, outlines, corner menus, mesh background); doxygen regen machinery not yet wired into generated CMakeLists (pre-Step 9 open item)
- **Active debts:** 
  - `DEBT-20260912T130000` — Frame-pacing race behind the waitIdle mask (jam_VulkanGraphics.cpp:770); paying sprint opens with ARCHITECT's read-first mandate (Vulkan API, JUCE API, architecture)
  - `DEBT-20260713T230500` — Hosted plugin editor steals keyboard focus from END's keybindings (Step 19 focus-loop scope)
- **Open PLANs:**
  - `PLAN-cast-migration.md` — Steps 1-8 landed; Step 9 (doxygen regen + docs final sync) carries forward
  - `PLAN-END-plugin-host.md` — remaining steps pending incl. Step 19 (focus loop — DEBT-20260713T230500)

## Layer Order (top → bottom)
`Application → Config → Nexus → Session → hosted CLAP plugins (EditorView panes)`  
Hosting: jam_clap (in-house wrapper + host format); `Nexus::VirtualClock` per-plugin demand clock; plugins live in `~/.config/end/plugins/`  
Graphics: JAM `jam_vulkan` (vulkan-hpp plain `vk::`, vendored SDK 1.4.350; `jam::VulkanEngine` owned by Application — unified resource-ownership tree for Typeface/Stamp/Grapheme/Link, Device, GlyphAtlas)  
Config: `Source/config/` — ConfigDirectory, ConfigModel (four-phase lifecycle)  
UI: `Source/end/` — ENDView, Window, SessionView, TabView (jam Owner/Owned composite; panes = binary space graph, `jam::PaneEdge` EDGE rows)  
Constants: `cast/*.md` (project-info, identifiers, bimaps, files) → `cast cast/CAST.md` → `Source/generated/{ProjectInfo,Identifiers,Bimaps,Files,Generated}.h` (global `Id::`, `map::`, `files::`)

## Key Docs
| File | Purpose |
|------|---------|
| `ARCHITECTURE.md` | Architectural contracts, invariants, layer rules (SSOT) |
| `SPEC.md` | Requirements v0.0.1 |
| `DEBT.md` | Active debt ledger |
| `SHADERS.md` | Shader system guide — Shadertoy, RetroArch Slang, OBJ mesh + iMouse |
| `carol/SPRINT-LOG.md` | Cross-session memory (last 5 sprints) |
| `PLAN-cast-migration.md` | Cast-toolchain + markdown-config migration plan |

## Doxygen (mandatory before any code task)
- Project: `docs/xml/index.xml`
- JAM: `~/Documents/Poems/dev/jam/docs/xml/index.xml`
- JUCE: `~/Documents/Poems/JUCE/docs/xml/index.xml`
- KANJUT / CIUM: not used in this project

## Build (ARCHITECT only — agents never run)
`cast cast/CAST.md` regenerates `CMakeLists.txt` + `Source/generated/*` · `ninja` via `Builds/` · `ninja doxygen` to regenerate docs
