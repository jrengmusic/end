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

## Parser holds Grid& directly (Explicit Encapsulation violation)

**Priority:** Medium  
**Sprint:** 9  
**Context:** Parser holds `Grid&` via dependency injection and calls Grid methods directly (`scrollUp`, `eraseRowRange`, `activeWriteCell`, etc.). Per ARCHITECTURAL-MANIFESTO, objects should be dumb and communicate via API — Parser should tell, not poke.

Cell buffer writes (hot path) must stay on Grid for performance — millions of cells/second. But `getCols()`, `getVisibleRows()`, `getScrollbackUsed()` are state queries that could route through State.

**Why deferred:** Large refactor with no immediate functional benefit. Ring buffer `head` must stay with Grid for consistency with cell data (Oracle assessment confirmed). The current `resizeLock` serialization is correct.

**Needs:** Architectural decision on where to draw the line between "buffer operation" (stays on Grid) and "state query" (moves to State).

---

## Pre-existing Debt (from Sprint 91+)

- ~~**`getTreeMode()` / `getTreeKeyboardFlags()` naming**~~ — resolved: renamed to `getMode()` / `getKeyboardFlags()`.
- ~~**`enableWindowTransparency()` redundancy on Windows 11**~~ — resolved: guarded with `isWindows10()` in `glContextCreated()`.
- **`seq 1M` performance gap** — 2m33s vs Windows Terminal's 1m12s. Reader thread drain loop exits too early.

---

### Font Fallback: Arrows Block (U+2190-U+21FF) Missing

**Severity:** Medium
**Found:** 2026-03-31 (whatdbg integration testing)
**Reporter:** whatdbg COUNSELOR

**Problem:**
Unicode character U+2192 (RIGHTWARDS ARROW `→`) renders as blank in END's terminal. This character is used by nvim-dap as the `DapStopped` sign to indicate the current execution line during debugging. The sign is placed correctly by nvim-dap but is invisible because the glyph is missing.

**Expected behavior:**
When `DisplayMono` (primary font) does not contain a glyph, END should fall back to `SymbolsNerdFont-Regular.ttf` (bundled) or system fonts. The Arrows Unicode block (U+2190-U+21FF, 112 glyphs) should be covered by the fallback chain.

**Reproduction:**
1. In END terminal, run nvim with nvim-dap
2. Set a breakpoint and trigger it
3. The `DapStopped` sign (`→`) should appear in the gutter — it renders blank
4. Alternative test: `echo -e '\u2192'` in terminal — should show `→`, shows blank

**Workaround:**
Replace `→` with ASCII `>>` in nvim-dap sign config (`dapui_config.lua`).

**Investigation notes:**
- `SymbolsNerdFont-Regular.ttf` is bundled in `Source/fonts/` — verify it contains the Arrows block
- If it does, the font fallback lookup is not triggering for codepoints in U+2190-U+21FF
- If it does not, the Arrows block needs to be added to the symbol font or a separate fallback font

**Related codepoints that should be verified:**
- U+2190 `←` LEFTWARDS ARROW
- U+2191 `↑` UPWARDS ARROW
- U+2192 `→` RIGHTWARDS ARROW
- U+2193 `↓` DOWNWARDS ARROW
- U+2194 `↔` LEFT RIGHT ARROW
- U+2195 `↕` UP DOWN ARROW
