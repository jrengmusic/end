# END — Ephemeral Nexus Display

**Type:** GPU-accelerated terminal emulator  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 82 — WHELMED Vulkan Standalone + Hosted Rendering ✅ (2026-07-15)
- **Active ODE:** `END.ode` (opened 2026-06-29)
- **Active debt:** `DEBT-20260713T230500` — hosted plugin editor steals keyboard focus from END's keybindings (Step 19 focus-loop scope)
- **Open PLANs:**
  - `PLAN-jam-clap-wrapper.md` — Steps 1–6 executed; Sprint 77 wrapper clean sweep landed; Step 7 (acceptance + docs sync) open
  - `PLAN-END-plugin-host.md` — Step 1 superseded by jam_clap wrapper; host format layer (Sprint 78), Steps 2Δ/11/12/13-min (Sprint 79), Steps 6/7/8/13 + verbatim-JUCE hosting, WHELMED live in a pane (Sprint 80), WHELMED Vulkan standalone + hosted rendering (Sprint 82); remaining steps pending incl. Step 19 (focus loop — see active debt)
  - `PLAN-terminal-editor.md` — stale root doc, deletion pending ARCHITECT's word (with `PLAN-session-layer.md`, `RFC-terminal-editor.md`)

## Layer Order (top → bottom)
`Application → Config → Nexus → Session → hosted CLAP plugins (EditorView panes)`  
Hosting: jam_clap (in-house wrapper + host format); `Nexus::VirtualClock` per-plugin demand clock; plugins live in `~/.config/end/plugins/`  
Graphics: JAM `jam_vulkan` (vulkan-hpp plain `vk::`, vendored SDK 1.4.350; `jam::VulkanEngine` owned by Application — unified resource-ownership tree for Typeface/Stamp/Grapheme/Link, Device, GlyphAtlas)  
Config: `Source/config/` — Directory, Model (four-phase lifecycle)  
UI: `Source/end/` — ENDView, Window, SessionView, TabView (jam Owner/Owned composite; panes = binary space graph, `jam::PaneEdge` EDGE rows)  
Constants: `Source/lexicon.md` → `Source/generated/Lexicon.h/.cpp` (global `Id::`) · `Source/LexiconFiles.h` (`Id::Files`)

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
