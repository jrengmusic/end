# Technical Debt

**Project:** END  
**Purpose:** Long-lived technical debt items that span multiple sprints.

---

## Windows 10 Regression Test

**Priority:** Medium  
**Sprint:** 100  
**Context:** The Windows 10 blur path (`ACCENT_ENABLE_BLURBEHIND` + `AccentFlags=2`) is preserved but untested after Sprint 100 changes and Sprint 8 GlassWindow refactor (`setGlassEnabled`, `BackgroundBlur::enable/disable` rename, accent reset in `disable()`). `isWindows10()` guard in `disable()` skips rounded corners on Win10.

**Needs:** Build and test on Windows 10 22H2 to verify blur, tint, ConPTY, mouse, and terminal functionality.

---

## Resolved Debt

- ~~**`getTreeMode()` / `getTreeKeyboardFlags()` naming**~~ — resolved: renamed to `getMode()` / `getKeyboardFlags()`.
- ~~**Parser holds Grid& directly**~~ — resolved: Grid::Writer facade decouples Parser, geometry reads through State parameterMap.
- ~~**`enableWindowTransparency()` redundancy on Windows 11**~~ — resolved: guarded with `isWindows10()` in `glContextCreated()`.
- ~~**`seq 1M` performance gap**~~ — resolved: END matches Windows Terminal (~55s at 4K half-width). IO-bound on ConPTY middleware.
- ~~**Font Fallback: Arrows Block (U+2190-U+21FF)**~~ — resolved: DirectWrite `IDWriteFontFallback::MapCharacters` in `shapeFallback()`.
- ~~**GLContext static refcount (B violation)**~~ — resolved Sprint 23: atlas ownership moved to Typeface (mirroring CPU pattern). `GLAtlasRenderer` subclass manages GL atlas lifecycle. Static refcount eliminated.
- ~~**GLContext::~GLContext() = default (resource leak)**~~ — resolved Sprint 23: destructor calls `closeContext()`.
- ~~**titleBarHeight magic number**~~ — resolved Sprint 23: extracted to `App::titleBarHeight` constant.
- ~~**Dead stub files**~~ — resolved Sprint 23: `git rm` of `Gpu_windows.cpp`, `Gpu_mac.mm`, `RendererType.h`, `PLAN-font-metrics.md`.
- ~~**drawVertices early return**~~ — resolved Sprint 23: converted to positive nested check.
- ~~**CoreText calcMetrics space glyph only**~~ — resolved Sprint 23: now scans max across ASCII 32-127, matching FreeType.
- ~~**getEnvVar on base TTY (leaky abstraction)**~~ — resolved Sprint 23: platform-guarded to `#if ! JUCE_WINDOWS`. Unix-only feature no longer exposed on Windows.
- ~~**WindowsTTY missing getEnvVar**~~ — resolved Sprint 23: not debt, Windows doesn't need it. Full chain platform-guarded.
- ~~**getEnvVar early returns**~~ — not debt: audit found no early returns in `getEnvVar`, `getCwd`, `getProcessName`. All use positive nested checks.
