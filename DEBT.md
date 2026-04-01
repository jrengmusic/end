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

## Pre-existing Debt (from Sprint 91+)

- ~~**`getTreeMode()` / `getTreeKeyboardFlags()` naming**~~ — resolved: renamed to `getMode()` / `getKeyboardFlags()`.
- ~~**Parser holds Grid& directly**~~ — resolved: Grid::Writer facade decouples Parser, geometry reads through State parameterMap.
- ~~**`enableWindowTransparency()` redundancy on Windows 11**~~ — resolved: guarded with `isWindows10()` in `glContextCreated()`.
- ~~**`seq 1M` performance gap**~~ — resolved: END matches Windows Terminal (~55s at 4K half-width). Investigated double-buffer pre-fetch and INFINITE drain wait — no improvement. Both terminals sit at ~45% CPU, IO-bound on ConPTY middleware. The bottleneck is ConPTY, not the drain loop.

- ~~**Font Fallback: Arrows Block (U+2190-U+21FF)**~~ — resolved: added Windows system font fallback via DirectWrite `IDWriteFontFallback::MapCharacters` in `shapeFallback()`, mirroring macOS `CTFontCreateForString`. Segoe UI Symbol provides arrow glyphs. Covers all missing Unicode, not just arrows.
