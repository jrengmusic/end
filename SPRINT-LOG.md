# SPRINT-LOG

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
