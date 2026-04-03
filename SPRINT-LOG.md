# SPRINT-LOG

## Sprint 3: GPU Probe Auto-Detection

**Date:** 2026-04-03

### Agents Participated
- COUNSELOR: Led requirements gathering, designed resolution flow and truth table, planned all steps, directed all subagents
- Pathfinder: Discovered GPU acceleration code paths, Config/AppState patterns, Context CRTP pattern, GL header/link setup
- Librarian: Researched JUCE juce_opengl GL symbol namespace (`juce::gl::`) and function pointer loading
- Engineer: Implemented Gpu struct, platform probes, AppState changes, MainComponent refactor, App::RendererType enum migration
- Auditor: Three audit passes — caught SSOT violations (duplicated constants/functions, magic strings), stale includes, naming issues
- Machinist: Cleaned stale includes from MainComponent.h and TerminalComponent.h

### Files Modified (15 total)
- `Source/Gpu.h` — **NEW** struct with `probe()`, `isSoftwareRenderer()`, probe constants, `softwareRenderers` StringArray
- `Source/Gpu.cpp` — **NEW** Windows WGL probe: dummy HWND, 3.2 Core context bootstrap, pixel readback, RAII ProbeContext
- `Source/Gpu.mm` — **NEW** macOS NSOpenGLContext probe: NSOpenGLPFAAccelerated, pixel readback
- `Source/AppIdentifier.h` — Added `App::RendererType` enum (gpu/cpu), `gpuAvailable` identifier, `rendererGpu`/`rendererCpu` string constants
- `Source/AppState.h` — `getRendererType()` returns `App::RendererType` enum; added `setGpuAvailable(bool)`; `setRendererType()` takes raw config string
- `Source/AppState.cpp` — `setRendererType()` resolves internally (config setting AND gpuAvailable); `getRendererType()` maps ValueTree string to enum; removed renderer from `initDefaults()`
- `Source/Main.cpp` — Probe at startup before window creation, store result in AppState, glass/reload read AppState enum
- `Source/MainComponent.h` — Removed `ValueTree::Listener`, removed `windowState` member, added `setRenderer(App::RendererType)`
- `Source/MainComponent.cpp` — Removed all config ternaries, direct `setRenderer()` calls from `applyConfig()`, default compact atlas in constructor
- `Source/component/PaneComponent.h` — Removed `RendererType` enum (moved to App namespace), uses `App::RendererType`
- `Source/component/TerminalComponent.h` — Updated include chain, uses `App::RendererType`
- `Source/component/TerminalComponent.cpp` — `switchRenderer` uses `AppState::getContext()->getRendererType()`, updated stale comment
- `Source/component/Tabs.h` / `Tabs.cpp` — Updated to `App::RendererType`
- `Source/component/Popup.cpp` — Updated to `App::RendererType`, reads AppState directly
- `Source/whelmed/Component.h` / `Component.cpp` — Updated to `App::RendererType`

### Alignment Check
- [x] BLESSED principles followed — stateless probe, SSOT in AppState, RAII resource management, no shadow state
- [x] NAMES.md adhered — semantic naming, verbs for actions, no hungarian notation
- [x] MANIFESTO.md principles applied — unidirectional flow, tell-don't-ask, encapsulation

### Problems Solved
- `gpu.acceleration = "auto"` blindly selected GPU — no actual hardware detection existed. On Windows ARM64 UTM (no real GPU), OpenGL context creation failed silently and terminals never rendered
- GPU probe now creates a native GL context, queries GL_RENDERER against known software renderers (GDI Generic, llvmpipe, SwiftShader, etc.), runs FBO pixel readback verification
- Resolution truth table: `renderer = wantsGpu AND gpuAvailable` — encapsulated in `AppState::setRendererType()`
- Removed self-listening ValueTree pattern from MainComponent — was reacting to its own writes
- Moved `RendererType` enum from `PaneComponent` to `App` namespace — renderer type is an app concern, not a component concern

### Technical Debt / Follow-up
- `Source/Gpu_windows.cpp`, `Source/Gpu_mac.mm`, `Source/component/RendererType.h` — dead stub files, need `git rm`
- `PLAN-GPU-PROBE.md` — can be deleted after commit
- macOS probe untested (implemented, platform-guarded, mirrors Windows pattern)
- `AppIdentifier.h` uses `static const` instead of `inline const` — per-TU duplication (pre-existing, not introduced)

## Sprint 2: Build Architecture Auto-Detection

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR: Led analysis, planned fix, directed Engineer
- Pathfinder: Discovered build configuration — `build.bat` hardcoded x64, host is ARM64 Windows
- Engineer: Implemented architecture auto-detection in `build.bat`

### Files Modified (1 total)
- `build.bat:54-57` — Replaced hardcoded `vcvarsall.bat x64` with `%PROCESSOR_ARCHITECTURE%` detection. Defaults to `x64`, overrides to `arm64` on ARM64 Windows. `install.sh` verified clean — no changes needed.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- `build.bat` hardcoded `vcvarsall.bat x64` — ARM64 Windows host (UTM on M4 MBP) was building x64 binaries running under emulation instead of native ARM64
- `install.sh` verified to delegate arch decisions entirely to `build.bat` — no coupling

### Technical Debt / Follow-up
- Existing `Builds/Ninja` directory contains x64 CMake cache — needs `build.bat clean` to reconfigure for ARM64

## Sprint 1: README Rewrite

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR: Led requirements gathering, structured content, directed all edits
- Pathfinder: Discovered assets (icons, config files, MANIFESTO/NAMES locations), Whelmed block types and capabilities
- Engineer: Wrote initial README draft

### Files Modified (1 total)
- `README.md` — Full rewrite (242 lines). Replaced outdated feature list with comprehensive coverage of all implemented features. Added: "Why END?" rationale section (dual renderer, C++17/JUCE justification, built-in multiplexing, WHELMED, glass blur, Lua config). Dedicated "Popup Terminals" section with Lua config example. Dedicated "WHELMED" section. Expanded "Configuration" section with per-section breakdown of both `end.lua` and `whelmed.lua`. Updated platform support (Windows now "Supported", not "in progress"). Updated roadmap (removed completed items, removed jreng_text/WHELMED standalone). License changed from Proprietary to MIT.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- README undersold the project — missing dual renderer, command palette, popup terminals, file opener, vim selection, kitty keyboard protocol, shell integration, hyperlinks, notifications, status bar, WHELMED, ConPTY, and most OSC support
- Windows listed as "in progress" when ConPTY backend is complete
- Configuration section reproduced config examples instead of referencing self-documenting lua files
- No rationale for technology choices (C++17, JUCE, dual renderer)
- No dedicated sections for popup terminals or WHELMED

### Technical Debt / Follow-up
- No screenshots exist in the repo — README would benefit from visuals
- WHELMED section will need updating as Mermaid coverage expands
