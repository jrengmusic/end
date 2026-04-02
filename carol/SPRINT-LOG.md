# SPRINT-LOG.md

**Project:** end
**Repository:** /Users/jreng/Documents/Poems/dev/end
**Started:** 2026-02-02

**Purpose:** Long-term context memory across sessions. Tracks completed work, technical debt, and unresolved issues. Written by PRIMARY agents only when user explicitly requests.

---

## CRITICAL RULES (DO NOT ROTATE)
- **YOU ARE NOT ALLOWED TO BUILD, UNLESS ARCHITECT EXPLICITLY ASKS**

---

<!-- SPRINT HISTORY — latest first, keep last 5, rotate older to git history -->

## Sprint 20: Popup PATH injection from active terminal

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR — root cause analysis, plan design, delegation, auditor coordination
- Pathfinder (x2) — popup/process spawning code discovery, shell integration scripts, Terminal::Component structure
- Engineer (x3) — TTY layer `getEnvVar` implementation, Session/TerminalComponent `getShellEnvVar`+`addExtraEnv`, MainComponent popup PATH wiring
- Auditor (x2) — TTY layer verification, full feature audit (found C1 noexcept+alloc, H1 null guard, H2 early returns)
- Machinist — fixed C1 (removed noexcept), fixed H1 (added tty null guard)

### Files Modified (8 total)
- `Source/terminal/tty/TTY.h:252-262` — added `getEnvVar` virtual (reads env var from PID, default returns 0)
- `Source/terminal/tty/UnixTTY.h:210-223` — added `getEnvVar` override declaration
- `Source/terminal/tty/UnixTTY.cpp:27-32` — added `<sys/sysctl.h>` to `__APPLE__` include block
- `Source/terminal/tty/UnixTTY.cpp:499-613` — implemented `getEnvVar` (macOS: `sysctl KERN_PROCARGS2` env parse, Linux: `/proc/pid/environ` parse)
- `Source/terminal/logic/Session.h:140-162` — added `getShellEnvVar` and `addExtraEnv` public declarations
- `Source/terminal/logic/Session.cpp:283-311` — implemented both methods (getShellEnvVar reads from foreground PID with tty null guard, addExtraEnv delegates to tty->addShellEnv)
- `Source/component/TerminalComponent.h:284-302` — exposed `getShellEnvVar` and `addExtraEnv` (delegates to Session)
- `Source/component/TerminalComponent.cpp` — delegating implementations
- `Source/MainComponent.cpp:564-572` — popup launch reads PATH from active terminal, injects into popup via `addExtraEnv`

### Alignment Check
- [x] BLESSED principles followed — Bound (TTY owns env reading, Session delegates, no floating state), Lean (minimal additions per layer), Explicit (semantic names, no magic values, `maxEnvValueLength` named constant), SSOT (PATH read from live process, no cached copy), Encapsulation (TerminalComponent→Session→TTY delegation chain, no internals poked)
- [x] NAMES.md adhered — `getEnvVar`, `getShellEnvVar`, `addExtraEnv`, `shellPath`, `maxEnvValueLength` follow verb+noun, semantic naming, consistency with `getCwd`/`getProcessName`/`setShellProgram`
- [x] MANIFESTO.md principles applied — follows existing `getCwd`/`getProcessName` pattern exactly, no new patterns invented
- [x] JRENG-CODING-STANDARD — brace init, `and`/`or`/`not` tokens, braces on new lines, space after function names, `.at()` not applicable (raw buffers)

### Problems Solved
- **Popup commands not found on macOS** — popups run `zsh -c "command"` which only sources `.zshenv` (non-interactive, non-login). If user's PATH additions are in `.zshrc`/`.zprofile`, popup can't find commands in `~/.local/bin` etc. Fixed by reading PATH from the active terminal's shell process (which has the full interactive PATH) and injecting it into the popup's environment before PTY open. Works on MSYS2 already because Windows GUI apps inherit full user env.

### Technical Debt / Follow-up
- Early returns in `getEnvVar` match pre-existing pattern in `getCwd`/`getProcessName` — accepted as TTY layer convention, not refactored
- `std::vector` heap allocation in `getEnvVar` (macOS path: variable-size `sysctl` buffer) — `noexcept` correctly removed, but stack allocation alternative could be considered if OOM is a concern
- Linux `/proc/pid/environ` reads up to 64KB (`envBufSize`) — sufficient for typical PATH but could theoretically truncate extreme environments
- WindowsTTY has no `getEnvVar` override (returns 0 from base) — not needed since Windows popup PATH works OOTB, but could be added for completeness

## Sprint 19: Font metrics — aspect ratio, DPI restore, popup sizing

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR — analysis of cell metrics vs Kitty/Ghostty/WezTerm, plan design, delegation, auditor coordination
- Pathfinder (x5) — font metrics code discovery, FreeType/CoreText paths, config key patterns, embedded font data, popup padding flow
- Librarian — JUCE Windows DPI awareness research
- Engineer (x3) — font.line_height config + calc(), font.cell_width config + calc(), popup sizing fix
- Auditor (x2) — verified line_height implementation (found placeholder mismatch C1), verified popup sizing (found rounding divergence M1, naming m1)

### Files Modified (8 total)
- `Source/config/Config.h:195-198` — added `fontLineHeight` ("font.line_height") and `fontCellWidth` ("font.cell_width") key declarations
- `Source/config/Config.cpp:102-103` — registered both keys: number, range 0.5–3.0, default 1.0
- `Source/config/default_end.lua:71-77` — added `line_height = %%font_line_height%%` and `cell_width = %%font_cell_width%%` to font section
- `Source/terminal/rendering/Screen.h:373,379,921-922` — added `setLineHeight()` and `setCellWidth()` declarations, `lineHeightMultiplier` and `cellWidthMultiplier` members (default 1.0f)
- `Source/terminal/rendering/Screen.cpp:66-86` — `calc()` applies both multipliers: height with baseline centering (extra/2 shift), width with truncation
- `Source/terminal/rendering/Screen.cpp:271-285` — `setLineHeight()` and `setCellWidth()` implementations (guard + recalc pattern matching `setFontSize`)
- `Source/component/TerminalComponent.cpp:712-713` — `applyScreenSettings()` reads config, calls `setLineHeight()` and `setCellWidth()`
- `Source/MainComponent.cpp:544-557` — popup pixel sizing now includes terminal padding (all 4 sides), titleBarHeight, and cell multipliers
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:247-249` — added render DPI restore after metric computation (implements documented step 8)

### Alignment Check
- [x] BLESSED principles followed — SSOT (cell multipliers from Config, popup sizing reads same Config keys as TerminalComponent), Explicit (multiplier names semantic, all params visible), Encapsulation (Screen owns multipliers, told via setLineHeight/setCellWidth API), Lean (minimal additions)
- [x] NAMES.md adhered — `lineHeightMultiplier`, `cellWidthMultiplier`, `effectiveCellW/H`, `paddingTop/Right/Bottom/Left` match established patterns
- [x] MANIFESTO.md principles applied — no new patterns invented, follows existing setFontSize guard+recalc pattern
- [x] JRENG-CODING-STANDARD — brace init, `not`/`and`/`or` tokens, no early returns, space after function names

### Problems Solved
- **Cell aspect ratio** — terminal had no user control over cell dimensions. Added `font.line_height` (height multiplier) and `font.cell_width` (width multiplier), matching Kitty/Ghostty/WezTerm pattern. Extra height centered via baseline shift (half above, half below).
- **DPI restore bug** — `calcMetrics()` doc promised step 8 (restore face to renderDpi) but code never did it. Face left at baseDpi (96) after every cache miss. On HiDPI (scale > 1.0), glyphs rasterized at wrong DPI — 43% too small at 175% scaling (UTM Windows 11). Fixed by adding `FT_Set_Char_Size` restore after metric read.
- **Popup sizing** — popup pixel dimensions computed from raw `cols * cellW` without accounting for terminal padding (default 10px all sides) or titleBarHeight. Grid always smaller than configured cols/rows. Fixed by adding padding + titleBar + multipliers to pixel calculation.

### Analysis Data (for future reference)
- Display Mono Book: UPM=1003, hhea ascender=881, descender=-122, lineGap=250
- Competitor cell width: all use max ASCII advance (32-127), same as end
- Competitor cell height: Kitty/WezTerm use `face->height` (design units, includes lineGap), Ghostty reads hhea tables directly. End uses `face->size->metrics.height` (should be equivalent per FreeType internals)
- DPI restore bug: only affects non-Mac (FreeType path). Severity = `1 - 1/scale` (20% at 125%, 43% at 175%)
- Mac calcMetrics (CoreText) measures only space glyph for width; Kitty measures max across ASCII 32-127. For true monospace fonts these should be equal.

### Technical Debt / Follow-up
- `PLAN-font-metrics.md` at project root — should be removed after commit (analysis artifact)
- Mac CoreText `calcMetrics` measures space glyph only for width vs competitors measuring max ASCII — investigate if this causes width mismatch for non-monospace fallback fonts
- The `24` magic number for titleBarHeight appears in TerminalComponent.h:699 and now MainComponent.cpp — consider extracting to a named constant

## Sprint 18: Menu font sizing, popup scaling fix

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR — diagnosis, plan design, delegation, research coordination
- Pathfinder (x3) — LookAndFeel code discovery, BackgroundBlur caller search, popup scaling investigation
- Researcher (x2) — Mac NSWindow corner radius approaches, NSView tint layer over NSVisualEffectView
- Librarian (x2) — JUCE Font scaling on retina displays, JUCE popup menu text rendering architecture
- Engineer (x6) — NSVisualEffectView + cornerRadius implementation, CGS restore, font sizing fixes, tabFontRatio iterations, revert
- Auditor — verified blur path and font changes

### Files Modified (3 total)
- `Source/config/Config.cpp:146` — `tabSize` default 24.0 → 12.0 (was bar height, now font size)
- `Source/component/LookAndFeel.h:109-110,202,210` — added `shouldPopupMenuScaleWithTargetComponent` override (returns false); added `tabFontRatio { 0.5f }` constant; updated `getTabBarHeight` doc
- `Source/component/LookAndFeel.cpp:98-105,117-125,265-271` — `getTabButtonFont` uses `tabSize` directly; `getTabBarHeight` computes bar from font height / tabFontRatio; `getPopupMenuFont` uses `tabSize` directly

### Alignment Check
