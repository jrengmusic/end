# END — Ephemeral Nexus Display

**Type:** GPU-accelerated audio plugin host, end to end (default plugin: EVE, the terminal, under the ENAX contract; formats VST2, VST3, AU one at a time; CLAP later)  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 96: END and EVE Config — Named Light/Dark Palette, One Load Path, Pane Padding, Canon Settings Lane, Keyword Key Form ✅ (2026-11-03).
- **Active debts (`DEBT.md`):**
  - `DEBT-20261025T135707` — END hosts EVE as CLAP (deferred by ARCHITECT; ENAX's CLAP format is future work)
  - `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (deferred by ARCHITECT)
- **Open PLANs:**
  - `PLAN-end-eve-config.md` — END and EVE config: lower-case keys, named light/dark palette, one jam load path, pane padding
  - Contract record: `~/Documents/Poems/dev/eve/PLAN-Grand-Scheme.md` §4.3 `[AMENDED 2026-11-01 — ENAX]`

## Layer Order (top → bottom)
`Application → Config → Nexus → Session → hosted audio plugins (EditorView panes)`  
Hosting: JUCE default formats (VST2, VST3, AU) plus jam_clap (`Nexus.cpp:42-48`); one audio graph; each plugin owns its clock; plugins live in `~/.config/end/plugins/`, offloaded from END's BinaryData  
Graphics: JAM `jam_graphics` + `jam_vulkan` (vulkan-hpp plain `vk::`, vendored SDK 1.4.350; owned by Application: `jam::CpuGraphicsEngine` — Typeface, GlyphAtlas, GlyphArrangement, ImageLoader, CPU image operations, CPU-lane context; `jam::VulkanGraphicsEngine` — Device, per-window Graphics, GPU forms of the image operations)  
Config: `Source/config/` — ConfigDirectory, ConfigModel (four-phase lifecycle)  
UI: `Source/ui/` — ENDView, SessionView, TabView, EditorView (jam Owner/Owned composite; panes = binary space graph, `jam::PaneEdge` EDGE rows); `Source/end/` — ENDModel; `Source/plugins/` — Nexus, Session  
Constants: `cast/*.md` (project-info, identifiers, bimaps, files) → `cast cast/spell.md` → `Source/generated/{ProjectInfo,Identifiers,Bimaps,Files,Generated}.h` (global `Id::`, `map::`, `files::`)

## Key Docs
| File | Purpose |
|------|---------|
| `ARCHITECTURE.md` | Architectural contracts, invariants, layer rules (SSOT) |
| `SPEC.md` | Requirements v0.0.1 |
| `DEBT.md` | Active debt ledger |
| `SHADERS.md` | Shader system guide — Shadertoy, RetroArch Slang, OBJ mesh + iMouse |
| `carol/SPRINT-LOG.md` | Cross-session memory (last 5 sprints) |

## Doxygen (mandatory before any code task)
- Project: `docs/xml/index.xml`
- JAM: `~/Documents/Poems/dev/jam/docs/xml/index.xml`
- JUCE: `~/Documents/Poems/JUCE/docs/xml/index.xml`
- KANJUT / CIUM: not used in this project

## Version Rule (ARCHITECT only — agents never change)
- NEVER change the `version:` field of a config file, a view layout (`ViewLayout.md`), or any other versioned document. The version is always `0.0.1`.
- Only ARCHITECT changes a version, and only by an explicit instruction that names the version.

## Build (ARCHITECT only — agents never run)
`cast cast/spell.md` regenerates `CMakeLists.txt` + `Source/generated/*` · `ninja` via `Builds/` · `ninja doxygen` to regenerate docs

Release (default flow, per host: macOS universal, Windows x64, Windows arm64): macOS `END.app` → `pkgbuild` → `productbuild` (`cast/installer/mac/distribution.xml`) → `productsign` → notarize → staple; Windows NSIS installer (`cast/installer/win/installer.nsi`, admin, `$PROGRAMFILES64/JRENG/END`, signtool with `identityWindows`). Archive `../Release/END v<version> <platform>.<format>`. The last toolchain row `cmake -P gh.cmake` creates or appends to the GitHub release `v<versionString>` of `jrengmusic/end`; notes from the generated `RELEASE.md` (`## release notes` rows). Installer artwork: `cast/installer/resources/`.
