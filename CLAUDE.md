# END — Ephemeral Nexus Display

**Type:** GPU-accelerated CLAP plugin host (terminal lives in the hosted `eve` plugin)  
**Stack:** C++17 · JUCE · JAM  
**Root:** `~/Documents/Poems/dev/end/`  
**Version:** 0.0.1

## Current State
- **Last sprint:** Sprint 90: Release Lane — pkg replaces dmg, Windows NSIS (x64, arm64, admin), gh Upload ✅ (2026-10-05) — generation is a fixpoint; Debug build fails at `ENDLookAndFeel.cpp:92` (4 arguments to `jam_ButtonSVG.h:81` `getState`, 3 parameters), predates Sprint 90 (per its log)
- **Active debts:** 
  - `DEBT-20261009T112447` — END keys do not use `jam::CommandManager` (ENDActions key maps, ENDActions.h:67-92, :152-153)
  - `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (end-of-frame wait on the Windows swapchain branch, default-glass blurBehind arm, VMA leak-only define)
- **Open PLANs:**
  - `PLAN-END-plugin-host.md` — remaining steps pending incl. Step 19 (focus loop)
  - `PLAN-terminal-editor.md` — APPROVED (2026-07-12); Step 4 (font seam) gated until the parallel `jam_vulkan` sprint lands

## Layer Order (top → bottom)
`Application → Config → Nexus → Session → hosted CLAP plugins (EditorView panes)`  
Hosting: jam_clap (in-house wrapper + host format); `Nexus::VirtualClock` per-plugin demand clock; plugins live in `~/.config/end/plugins/`  
Graphics: JAM `jam_graphics` + `jam_vulkan` (vulkan-hpp plain `vk::`, vendored SDK 1.4.350; owned by Application: `jam::CpuGraphicsEngine` — Typeface, GlyphAtlas, GlyphArrangement, ImageLoader, CPU image operations, CPU-lane context; `jam::VulkanGraphicsEngine` — Device, per-window Graphics, GPU forms of the image operations)  
Config: `Source/config/` — ConfigDirectory, ConfigModel (four-phase lifecycle)  
UI: `Source/end/` — ENDView, Window, SessionView, TabView (jam Owner/Owned composite; panes = binary space graph, `jam::PaneEdge` EDGE rows)  
Constants: `cast/*.md` (project-info, identifiers, bimaps, files) → `cast cast/spell.md` → `Source/generated/{ProjectInfo,Identifiers,Bimaps,Files,Generated}.h` (global `Id::`, `map::`, `files::`)

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
`cast cast/spell.md` regenerates `CMakeLists.txt` + `Source/generated/*` · `ninja` via `Builds/` · `ninja doxygen` to regenerate docs

Release (default flow, per host: macOS universal, Windows x64, Windows arm64): macOS `END.app` → `pkgbuild` → `productbuild` (`cast/installer/mac/distribution.xml`) → `productsign` → notarize → staple; Windows NSIS installer (`cast/installer/win/installer.nsi`, admin, `$PROGRAMFILES64/JRENG/END`, signtool with `identityWindows`). Archive `../Release/END v<version> <platform>.<format>`. The last toolchain row `cmake -P gh.cmake` creates or appends to the GitHub release `v<versionString>` of `jrengmusic/end`; notes from the generated `RELEASE.md` (`## release notes` rows). Installer artwork: `cast/installer/resources/`.
