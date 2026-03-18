# SPRINT-LOG.md

**Project:** end  
**Repository:** /Users/jreng/Documents/Poems/dev/end  
**Started:** 2026-02-02

**Purpose:** Long-term context memory across sessions. Tracks completed work, technical debt, and unresolved issues. Written by PRIMARY agents only when user explicitly requests.

---

## Sprint 91 — WindowsTTY Rewrite + ConPTY Mouse Investigation

**Date:** 2026-03-15  
**Agents:** COUNSELOR, @engineer, @pathfinder, @researcher, @auditor

### Problem

Windows implementation had 5 critical issues: mouse outputting garbage, poor performance (`seq 1M` = 4m16s), cursor twitching, crash on forced quit, OMP extra newlines. Root cause: `WindowsTTY` was built without a working reference — two anonymous pipes, `PeekNamedPipe` + `Sleep(1)` polling, no overlapped I/O. Fundamentally different pipe topology from Microsoft Terminal.

### What Was Done

**1. WindowsTTY rewritten from scratch**
- Ported Microsoft Terminal's `ConptyConnection` pipe topology exactly
- Single duplex unnamed pipe via `NtCreateNamedPipeFile` (same NT API Microsoft Terminal uses)
- Client opened via `NtCreateFile` relative to server handle — true full-duplex, no contention
- Overlapped I/O for both read and write — zero CPU when idle, instant wake on data
- `read()` issues immediate overlapped reads with zero-timeout to drain multiple chunks without re-entering `waitForData()`
- Clean shutdown: `ClosePseudoConsole` while reader alive → `stopThread` → `TerminateProcess` as last resort
- Performance: `seq 1M` improved from 4m16s to 2m33s

**2. Parser::resize() — stale wrapPending fix**
- Added `setWrapPending (false)` + `cursorClamp()` for both screens before resetting scroll regions
- Fixes OMP extra newline at full terminal width on resize

**3. State::getActiveScreen() — message-thread ValueTree reader**
- Added `getActiveScreen()` that reads from ValueTree (post-flush)
- Fixed `mouseWheelMove`, `shouldForwardMouseToPty()`, `getTreeKeyboardFlags()`, `getCursorState()`, `onVBlank` to use `getActiveScreen()` instead of `getScreen()` (atomic)
- `getScreen()` remains for reader-thread callers (Parser, Grid, ScreenRender, ScreenSnapshot)

**4. timeBeginPeriod(1) — Windows timer resolution**
- Added `timeBeginPeriod(1)` in `initialise()`, `timeEndPeriod(1)` in `shutdown()` (after teardown)
- Unlocks 1ms timer resolution for state flush timer

### ConPTY Mouse Investigation — UNRESOLVED

**Finding:** ConPTY on Windows 10 22H2 (build 19045) intercepts ALL of the following from the child's output and never forwards them to the terminal emulator:
- `ESC[?1049h/l` (alternate screen) — `activeScreen` is always `normal`
- `ESC[?1000h/l` (mouse tracking) — `isMouseTracking()` is always `false`
- `ESC[?1002h/l` (motion tracking) — never seen
- `ESC[?1003h/l` (all tracking) — never seen
- `ESC[?1006h/l` (SGR mouse) — never seen

**Confirmed via file logging:** Parser's `setScreen()` is never called. `applyPrivateModeTable` never receives mode 1000/1002/1003/1006/1049. ConPTY renders alternate screen internally and sends the result as normal-screen output.

**Forwarding blindly (`return true`)** causes ConPTY to echo SGR sequences back as raw text — rendered as red boxes (unknown glyphs).

**Status:** Mouse on Windows requires a fundamentally different approach. Possible directions:
- Win32 Input Mode (`?9001`) — ConPTY's own input protocol
- `WriteConsoleInput` with `MOUSE_EVENT_RECORD` — bypass pipe, use console API
- Heuristic screen detection (full-screen redraw patterns)
- Research how Windows Terminal's `ControlInteractivity` handles mouse → ConPTY

### Files Modified

- `Source/terminal/tty/WindowsTTY.h` — complete rewrite (277 lines)
- `Source/terminal/tty/WindowsTTY.cpp` — complete rewrite (974 lines)
- `Source/terminal/logic/Parser.cpp:179-189` — resize wrapPending + cursorClamp
- `Source/terminal/logic/ParserEdit.cpp` — diagnostic added/removed (clean)
- `Source/terminal/logic/ParserCSI.cpp` — diagnostic added/removed (clean)
- `Source/terminal/data/State.h` — added `getActiveScreen()` declaration
- `Source/terminal/data/State.cpp` — added `getActiveScreen()` implementation, fixed `getTreeKeyboardFlags()` + `getCursorState()`
- `Source/component/TerminalComponent.cpp` — `mouseWheelMove`, `shouldForwardMouseToPty()`, `onVBlank` use `getActiveScreen()`, mouse handlers gated, diagnostics added/removed (clean)
- `Source/Main.cpp` — `timeBeginPeriod(1)` / `timeEndPeriod(1)` with extern declarations

### Alignment Check

- **LIFESTAR:** Lean (single pipe replaces two), Explicit (overlapped I/O model documented in header), Single Source of Truth (`getActiveScreen` reads ValueTree, `getScreen` reads atomic — clear separation), Reviewable (docstrings match UnixTTY pattern)
- **NAMING-CONVENTION:** `getActiveScreen` — semantic name (Rule 3), no data-source encoding (Rule 2). Previous `getTreeMode`/`getTreeKeyboardFlags` violate Rule 2 (encode "Tree" in name) — pre-existing debt, not introduced here
- **ARCHITECTURAL-MANIFESTO:** TTY layer stays dumb — no knowledge of parser, grid, or UI. Session writes bytes, TTY delivers bytes. Explicit Encapsulation preserved.

### Technical Debt / Follow-up

- **CRITICAL: Mouse on Windows is non-functional.** ConPTY intercepts all DECSET mouse/screen sequences. Needs research into Win32 Input Mode or WriteConsoleInput approach. Reference: `terminal/src/cascadia/TerminalControl/ControlInteractivity.cpp`
- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** violates NAMING-CONVENTION Rule 2 — encodes data source ("Tree") in name. Should be renamed to semantic names in a future sprint.
- **`seq 1M` is 2m33s vs Windows Terminal's 1m12s** — 2x gap remains. Reader thread CPU is 33% (should be higher). The zero-timeout `WaitForSingleObject` in `read()` exits the drain loop too early when data hasn't arrived in that instant. Needs a short timeout (1-2ms) or a different drain strategy.
- **`shouldForwardMouseToPty()` docstring** still references alternate screen fallback that was removed. Needs update.
- **`CursorComponent` does not call `setInterceptsMouseClicks(false, false)`** — cursor child swallows clicks on the cursor cell. Trivial fix but blocked by mouse being non-functional anyway.

---


**[N]** = Sprint Number (e.g., `1`, `2`, `3`...)

**Sprint:** A discrete unit of work completed by one or more agents, ending with user approval ("done", "good", "commit")

---

## ⚠️ CRITICAL RULES

**AGENTS BUILD CODE FOR USER TO TEST**
- Agents never build.
- USER build and tests and provides feedback
- Agents wait for user approval before proceeding

**AGENTS NEVER RUN GIT COMMANDS**
- Write code changes without running git commands
- Agent runs git ONLY when user explicitly requests
- Never autonomous git operations
- **When committing:** Always stage ALL changes with `git add -A` before commit
  - ❌ DON'T selectively stage files (agents forget/miss files)
  - ✅ DO `git add -A` to capture every modified file

**SPRINT-LOG WRITTEN BY PRIMARY AGENTS ONLY**
- **COUNSELOR** or **SURGEON** write to SPRINT-LOG
- Only when user explicitly says: `"log sprint"`
- No intermediate summary files
- No automatic logging after every task
- Latest sprint at top, keep last 5 entries

**NAMING RULE (CODE VOCABULARY)**
- All identifiers must obey project-specific naming conventions (see NAMING-CONVENTION.md)
- Variable names: semantic + precise (not `temp`, `data`, `x`)
- Function names: verb-noun pattern (initRepository, detectCanonBranch)
- Struct fields: domain-specific terminology (not generic `value`, `item`, `entry`)
- Type names: PascalCase, clear intent (CanonBranchConfig, not BranchData)

**Contract (STRICT — enforced on ALL code):**
- Pre-increment: ++i not i++
- Brace initialization: int x { 0 }; not int x = 0;
- Use .at() for array/container access, NEVER [] (raw pointer [] access is OK for HeapBlock, C APIs)
- Use keywords and/or/not instead of &&/||/!
- Allman braces, space after function name ONLY when it has arguments: foo() but foo (x, y), */& stick to type, const before type
- noexcept where applicable, explicit single-arg constructors
- 300-30-3: max ~300 lines per file (~400 tolerable), 30 lines per function (33 = 10% tolerance), 3 conditional branches per function
- NO trailing underscore suffixes on member names
- NO improvisation. Follow the plan exactly. Ask ARCHITECT if uncertain.
- ValueTree is the only SSOT. No shadow structs, no duplicate state.
- Do NOT create new data types without ARCHITECT approval.
- No unnecessary classes.

**BEFORE CODING: ALWAYS SEARCH EXISTING PATTERNS**
- ❌ NEVER invent new states, enums, or utility functions without checking if they exist
- ✅ Always grep/search the codebase first for existing patterns
- ✅ Check types, constants, and error handling patterns before creating new ones
- **Methodology:** Read → Understand → Find SSOT → Use existing pattern

**TRUST THE LIBRARY, DON'T REINVENT**
- ❌ NEVER create custom helpers for things the library/framework already does
- ✅ Trust the library/framework - it's battle-tested

**FAIL-FAST RULE (CRITICAL)**
- ❌ NEVER silently ignore errors (no error suppression)
- ❌ NEVER use fallback values that mask failures
- ❌ NEVER return empty strings/zero values when operations fail
- ❌ NEVER use early returns
- ✅ ALWAYS check error returns explicitly
- ✅ ALWAYS return errors to caller or log + fail fast

**⚠️ NEVER REMOVE THESE RULES**
- Rules at top of SPRINT-LOG.md are immutable
- If rules need update: ADD new rules, don't erase old ones

---

## Quick Reference

### For Agents

**When user says:** `"log sprint"`

1. **Check:** Did I (PRIMARY agent) complete work this session?
2. **If YES:** Write sprint block to SPRINT-LOG.md (latest first)
3. **Include:** Files modified, changes made, alignment check, technical debt

### For User

**Activate PRIMARY:**
```
"@CAROL.md COUNSELOR: Rock 'n Roll"
"@CAROL.md SURGEON: Rock 'n Roll"
```

**Log completed work:**
```
"log sprint"
```

**Invoke subagent:**
```
"@oracle analyze this"
"@engineer scaffold that"
"@auditor verify this"
```

**Available Agents:**
- **PRIMARY:** COUNSELOR (domain specific strategic analysis), SURGEON (surgical precision problem solving)
- **Subagents:** Pathfinder, Oracle, Engineer, Auditor, Machinist, Librarian

---

<!-- SPRINT HISTORY STARTS BELOW -->
<!-- Latest sprint at top, oldest at bottom -->
<!-- Keep last 5 sprints, rotate older to git history -->

## SPRINT HISTORY

## Sprint 100 — Windows 11: DWM Blur, ConPTY Sideload, GL Compositing

**Date:** 2026-03-19
**Agents:** COUNSELOR, @engineer, @pathfinder, @researcher, @librarian, @auditor

### Problem

END crashed on Windows 11 — black window, shell exits immediately, no blur. Three root causes discovered through incremental testing:

1. **DWM blur black on Windows 11:** `ACCENT_ENABLE_BLURBEHIND` (3) with `AccentFlags=2` (GradientColor) produces black. `WS_EX_LAYERED` (added by JUCE `setOpaque(false)`) is incompatible with DWM backdrop effects and rounded corners on Windows 11.
2. **Inbox ConPTY kills child processes:** Windows 11 inbox `kernel32.dll` ConPTY sends `STATUS_CONTROL_C_EXIT` (0xC000013A) to child processes immediately after spawn. Sideloaded `conpty.dll` + `OpenConsole.exe` works correctly on both Windows 10 and 11.
3. **GL compositing covers JUCE tint:** GL framebuffer on Windows is composited as opaque by DWM — `glClearColor(0,0,0,0)` alpha is ignored. Tint must go through OS native API, not JUCE paint.

### What Was Done

**1. Windows 11 DWM blur path** (`jreng_background_blur.cpp`)
- Strip `WS_EX_LAYERED` on Windows 11 — incompatible with DWM backdrop and rounded corners
- `DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND` (attribute 33) — native rounded corners
- `DwmExtendFrameIntoClientArea({-1})` — sheet of glass
- `ACCENT_ENABLE_ACRYLICBLURBEHIND` (4) + `AccentFlags=2` + `GradientColor=tint` — acrylic blur with tint
- `isWindows10()` is the only OS branch — Windows 11 is canon, Windows 10 is special case

**2. Tint via OS native API on all platforms** (`jreng_background_blur.cpp`, `jreng_background_blur.mm`)
- GL framebuffer on Windows is composited as opaque by DWM — alpha channel ignored
- Tint must go through OS native API, not JUCE `DocumentWindow` background paint
- macOS: `[window setBackgroundColor:tint]` — unchanged from Windows port
- Windows 11: `ACCENT_ENABLE_ACRYLICBLURBEHIND` + `GradientColor=tint` — DWM handles tint
- Windows 10: `ACCENT_ENABLE_BLURBEHIND` + `GradientColor=tint` — DWM handles tint
- `GlassWindow` `DocumentWindow` background = `transparentBlack` (JUCE doesn't tint)
- All platforms consistent: OS handles blur + tint, GL renders terminal content on top

**3. ConPTY sideload on all Windows versions** (`WindowsTTY.cpp`)
- Removed `isWindows10()` guard from `loadConPtyFuncs()` — always sideload
- Inbox Windows 11 ConPTY sends `STATUS_CONTROL_C_EXIT` to child processes; sideloaded DLL works correctly
- Sprint 99 assumption ("inbox ConPTY on Win11 is sufficient") proven wrong

**4. Shared `isWindows10()`** (`jreng_platform.h`)
- Moved from `WindowsTTY.cpp` to `jreng_core/utilities/jreng_platform.h`
- Single definition used by both `WindowsTTY.cpp` and `jreng_background_blur.cpp`
- Removed `isWindows11_22H2OrLater()` from `jreng_background_blur.cpp`

**5. `scaleNotifier` null guard** (`MainComponent.h`)
- Added `tabs != nullptr` check in `NativeScaleFactorNotifier` lambda
- Prevents crash when DPI change fires before `initialiseTabs()`

### Key Discovery: Windows 11 DWM + WS_EX_LAYERED

`WS_EX_LAYERED` windows are fundamentally incompatible with DWM backdrop effects on Windows 11. DWM treats layered windows as flat textures — no blur behind, no rounded corners. Windows Terminal explicitly warns: "WS_EX_LAYERED acts REAL WEIRD... activating the window will remove our DWM frame entirely" (IslandWindow.cpp:147). The fix: strip `WS_EX_LAYERED` after JUCE adds it, then use DWM attributes for rounding and blur.

### Key Discovery: Windows 11 Inbox ConPTY Broken

The inbox `kernel32.dll` `CreatePseudoConsole` on Windows 11 sends `STATUS_CONTROL_C_EXIT` (0xC000013A) to child processes immediately after spawn. All shells (cmd.exe, powershell.exe, zsh.exe) affected. The sideloaded `conpty.dll` + `OpenConsole.exe` from Microsoft Terminal works correctly on both Windows 10 and 11.

### Key Discovery: ACCENT_ENABLE_BLURBEHIND AccentFlags

On Windows 11, `ACCENT_ENABLE_BLURBEHIND` (3) behavior depends on `AccentFlags`:
- `AccentFlags=0` — transparent blur (works, closest to macOS CGS blur)
- `AccentFlags=2` (use GradientColor) — black/opaque (broken on Win11)
- `ACCENT_ENABLE_ACRYLICBLURBEHIND` (4) + `AccentFlags=0` — acrylic blur (works but different look)

### Files Modified

- `modules/jreng_gui/glass/jreng_background_blur.cpp` — `applyDwmGlass()` rewritten: Win11 canon path (strip WS_EX_LAYERED, rounded corners, sheet of glass, acrylic blur + tint via GradientColor), Win10 special case preserved. Removed `isWindows11_22H2OrLater()` and DWMWA constants.
- `modules/jreng_gui/glass/jreng_background_blur.mm` — `applyBackgroundBlur()` and `applyNSVisualEffect()`: tint via `[window setBackgroundColor:tint]` (restored, consistent with Windows path)
- `modules/jreng_gui/glass/jreng_glass_window.cpp` — `DocumentWindow` background = `transparentBlack` (tint handled by OS API)
- `modules/jreng_core/utilities/jreng_platform.h` — NEW: shared `isWindows10()` static function
- `Source/terminal/tty/WindowsTTY.cpp` — always sideload conpty.dll (removed `isWindows10()` guard), removed local `isWindows10()` definition, includes `jreng_platform.h`
- `Source/MainComponent.h` — `scaleNotifier` null guard for `tabs`

### Alignment Check

- **LIFESTAR Lean:** One OS branch (`isWindows10()`), no nested version checks. Tint architecture identical on all platforms — OS handles blur + tint, GL renders on top.
- **LIFESTAR Explicit Encapsulation:** OS API handles blur + tint. GL renders terminal content. Each layer has one job. `enableGLTransparency()` handles GL-specific DWM setup only.
- **LIFESTAR SSOT:** `isWindows10()` defined once in `jreng_platform.h`, used everywhere. Tint flows through one path: config → GlassWindow → BackgroundBlur::apply() → OS API.
- **LIFESTAR Reviewable:** Win11 path documented with DWM attribute values and rationale. AccentFlags findings documented.
- **NAMING-CONVENTION:** `isWindows10()` — boolean predicate with `is*` prefix, semantic name.

### Technical Debt / Follow-up

- **Windows 10 blur path untested after changes:** The Win10 special case path is preserved but the sideload change (always sideload) and `isWindows10()` relocation need verification on Win10.
- **`if (true)` in loadConPtyFuncs:** Temporary — should be cleaned up to remove the dead `isWindows10()` branch entirely.
- **`enableGLTransparency()` on Windows 11:** Still strips `WS_EX_LAYERED` and calls `DwmExtendFrameIntoClientArea` — both already done by `applyDwmGlass()`. Redundant but harmless. Could be simplified.
- **Blur radius not controllable on Windows:** `ACCENT_ENABLE_ACRYLICBLURBEHIND` does not expose a blur radius parameter. DWM controls intensity. Config `window.blur_radius` is accepted but unused on Windows.
- **Windows 11 UTM/no-GPU:** Untested. DWM calls should fail gracefully (opaque fallback) but not verified.

---

## Sprint 99 — Windows 11 ConPTY Guard + build.bat Fixes

**Date:** 2026-03-18
**Agents:** COUNSELOR, @engineer, @pathfinder, @auditor

### Problem

END crashed on Windows 11 with a warning about conpty. The sideloaded `conpty.dll` + `OpenConsole.exe` (embedded as BinaryData, extracted to `~/.config/end/conpty/` at runtime) were designed for Windows 10 where the inbox `conhost.exe` doesn't support `PSEUDOCONSOLE_WIN32_INPUT_MODE`. On Windows 11, the inbox ConPTY already supports this flag natively, and the sideloaded Win10-era binaries are version-incompatible with Win11's console subsystem — causing a crash.

Additionally, `build.bat` had three bugs: (1) parentheses in `%PATH%` after `vcvarsall.bat` broke `cmd.exe` block parsing, (2) switching between Debug/Release required manual `clean` because Ninja is single-config, (3) `vcvarsall.bat` caused the script to re-enter and run twice.

### What Was Done

**1. `isWindows10()` — OS version gate for ConPTY sideload** (`WindowsTTY.cpp:206-238`)
- Static function with cached IIFE (`static const bool`)
- Uses `RtlGetVersion` from `ntdll.dll` via `GetProcAddress` + `reinterpret_cast` (matches file's existing NT API pattern)
- Returns `true` when `dwBuildNumber < 22000` (Windows 10)
- Safe default: `false` — if version undetectable, skip sideload, use inbox ConPTY
- Nested positive checks, single `return result`, brace initialization — fully compliant with JRENG-CODING-STANDARD

**2. `loadConPtyFuncs()` — sideload path guarded** (`WindowsTTY.cpp:259-313`)
- Wrapped entire "Attempt 1: sideloaded conpty.dll" block inside `if (isWindows10())`
- `extractConPtyBinaries()` never called on Windows 11+ — no files dumped to disk
- Kernel32 fallback gated by `if (not result.isValid())` instead of pre-existing early return
- Fixed pre-existing coding standard violations: early return removed, `= []()` → brace init `{ []()...() }`, `mod` → `conptyModule`

**3. `build.bat` — delayed expansion fix** (`build.bat`)
- `setlocal enabledelayedexpansion` — all `%VAR%` → `!VAR!` inside `if` blocks
- Prevents `cmd.exe` parser crash when `%PATH%` contains parentheses (e.g. `C:\Program Files (x86)\...`) after `vcvarsall.bat` runs
- Echo messages use `[Config]` brackets instead of `(Config)` parentheses

**4. `build.bat` — automatic reconfigure on config change** (`build.bat`)
- Marker file `Builds/Ninja/.build_config` stores active config type
- On every run: reads marker, compares to requested config, reconfigures if different
- `build.bat Release` after a Debug build now works without manual `clean`

**5. `build.bat` — re-entry guard** (`build.bat`)
- `_END_BUILD_RUNNING` environment variable prevents double execution
- `vcvarsall.bat` can cause `cmd.exe` to re-enter the calling script; guard exits immediately on re-entry

### Files Modified

- `Source/terminal/tty/WindowsTTY.cpp:206-238` — added `isWindows10()` static function
- `Source/terminal/tty/WindowsTTY.cpp:259-313` — `loadConPtyFuncs()` IIFE restructured: sideload guarded by `isWindows10()`, early return eliminated, brace init, `mod` → `conptyModule`
- `build.bat` — `enabledelayedexpansion` + `!VAR!` syntax, config change detection with marker file, re-entry guard

### Alignment Check

- **LIFESTAR Lean:** `isWindows10()` is 32 lines including docstring. One `if` guard in `loadConPtyFuncs()`. No new abstractions, no shared utilities — the version check is local to the one static function that needs it.
- **LIFESTAR Explicit Encapsulation:** `isWindows10()` is a pure static function with no parameters and no external dependencies beyond Win32 API. It manages its own cached state. Callers don't track any flags on its behalf. `loadConPtyFuncs()` doesn't know or care about the OS version check implementation.
- **LIFESTAR SSOT:** Build number threshold `22000` appears exactly once (line 230). Not duplicated with `isWindows11_22H2OrLater()` in `jreng_background_blur.cpp` — different threshold (22621), different purpose (Mica blur), different predicate.
- **LIFESTAR Immutable:** Both functions use `static const` locals — computed once per process, deterministic, no hidden mutation.
- **LIFESTAR Reviewable:** Docstring explains why (Win10 vs Win11), the threshold (22000), and the safe default (false). Inline comments mark the two-attempt strategy.
- **NAMING-CONVENTION:** `isWindows10` — boolean predicate with `is*` prefix (Rule 1), semantic name (Rule 3). `FnRtlGetVersion` — type alias with `Fn` prefix distinguishing from the API function it wraps. `conptyModule` — semantic name for the loaded DLL handle (Rule 3).
- **JRENG-CODING-STANDARD:** Nested positive checks (no early returns), brace initialization, `not`/`and`/`or` tokens, `reinterpret_cast`, `const` before type, `noexcept`, explicit nullptr checks. Audited and passed all three contracts.

### Technical Debt / Follow-up

- **Sprint 92 debt resolved:** "Windows 11: The inbox conhost on Windows 11 may support PSEUDOCONSOLE_WIN32_INPUT_MODE natively. The sideload is harmless (same behavior) but could be skipped on newer OS versions." — Now guarded. Sideload is Windows 10 exclusive.
- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** — still violates NAMING-CONVENTION Rule 2. Pre-existing debt from Sprint 91.
- **`seq 1M` performance gap** — still 2m33s vs Terminal's 1m12s. Pre-existing debt from Sprint 91.
- **`CursorComponent` missing `setInterceptsMouseClicks(false, false)`** — pre-existing debt from Sprint 92.
- **Sideloaded binaries still embedded in BinaryData** — on Windows 11 they are dead weight (~1.2 MB) in the executable. Could be excluded from BinaryData via CMake platform/version guard, but the complexity isn't worth the savings right now.

---

## Sprint 98 — Configurable Padding, SGR Mouse Wheel, DPI Cell Hit-Test, Resize Ruler, Bit-Font Logo

**Date:** 2026-03-17
**Agents:** SURGEON, @pathfinder, @explore

### Problems Solved

1. **Text selection offset worsened toward bottom** — `cellAtPoint` used logical integer `cellWidth`/`cellHeight` for hit-testing but the GL renderer places rows at `row * physCellHeight` physical pixels. At fractional DPI scales (125%, 150%), `physCellHeight / scale ≠ cellHeight` due to integer truncation. Error accumulated per row — 6px drift at row 10, 12px at row 20.
2. **Mouse wheel sent arrow keys on alternate screen** — `mouseWheelMove` sent `\x1b[A`/`\x1b[B` (arrow key sequences) instead of SGR mouse wheel events. TUI apps that handle mouse wheel natively (button 64/65) never received them.
3. **Terminal padding hardcoded** — `horizontalInset`/`verticalInset` were `static constexpr int { 10 }`. No way to configure per-side padding.
4. **`scrollbackStep` set in config but never read** — `mouseWheelMove` used `static constexpr int scrollLines { 3 }` ignoring the config value entirely.
5. **`scrollback.*` table orphaned** — two-key table with no natural home. Belongs in a `terminal` table alongside new padding keys.
6. **Resize overlay showed plain centred text** — replaced with Path-based crossed ruler lines with inline gap labels, padding-aware, aligned to actual grid edges.
7. **`shouldForwardMouseToPty` docstring wrong** — claimed ConPTY intercepts DECSET sequences on Windows; sideloaded ConPTY (Sprint 92) makes `isMouseTracking()` fully reliable on Windows.

### What Was Done

**1. `Screen::cellAtPoint` — DPI-accurate cell hit-testing** (`Screen.cpp`)
- Rewritten to use physical-pixel round-trip: `physX = (x - viewportX) * scale`, then `col = physX / physCellWidth`
- Exactly inverts `getCellBounds()` — click-to-cell is now symmetric with cell-to-pixel
- Eliminates per-row drift at all fractional DPI scales

**2. `mouseWheelMove` — SGR mouse wheel** (`TerminalComponent.cpp`)
- Alternate screen path now calls `session.writeMouseEvent(button, cell.x, cell.y, true)` with button 64 (up) or 65 (down)
- `scrollLines` reads `Config::Key::terminalScrollStep` — fixes pre-existing bug where config value was set but ignored
- Primary screen scrollback path also reads `terminalScrollStep`

**3. `terminal` config table** (`Config.h`, `Config.cpp`, `default_end.lua`)
- `scrollback.num_lines` → `terminal.scrollback_lines` (`terminalScrollbackLines`)
- `scrollback.step` → `terminal.scroll_step` (`terminalScrollStep`)
- New `terminal.padding` — 4-element Lua array `{ top, right, bottom, left }` (CSS order)
- Dedicated array parser in `Config::load()`: when field is a table, reads indices 1–4, clamps to [0, 200], stores as 4 flat keys `terminal.padding_top/right/bottom/left`
- Default: `{ 10, 10, 10, 10 }`

**4. `TerminalComponent` padding** (`TerminalComponent.h`, `TerminalComponent.cpp`)
- Removed `static constexpr int verticalInset { 10 }` and `horizontalInset { 10 }`
- Added 4 `const int padding*` members read from config at construction
- `resized()` uses 4 individual `removeFrom*` calls instead of `reduced()`

**5. Resize ruler overlay** (`MessageOverlay.h`, `MainComponent.cpp`)
- `showResize(cols, rows, padTop, padRight, padBottom, padLeft)` replaces `showMessage(...)` on resize
- `paintRulers()` static free function: two `juce::Path` rulers crossing near bottom-right (resize handle location)
- Horizontal ruler at `y = 2/3 * gridHeight`, vertical at `x = 2/3 * gridWidth` — both inset by padding to align with actual grid edges
- Each ruler: two Path strokes flanking a label gap, perpendicular tick marks at grid edges
- Labels: `"N col"` and `"N row"` — horizontal text only, no rotation
- `MainComponent::showMessageOverlay()` reads config padding for accurate col/row calculation

**6. `shouldForwardMouseToPty` docstring corrected** (`TerminalComponent.h`, `TerminalComponent.cpp`)
- Removed incorrect ConPTY interception claim
- Documents that `isMouseTracking()` is reliable on Windows via sideloaded ConPTY

**7. Bit-font logo + version stamp** (`default_end.lua`)
- 11-line pixel-art `END` logo added as Lua comments at top of `default_end.lua`
- `Ephemeral Nexus Display  v%versionString%` subtitle line
- `Config::writeDefaults()` substitutes `%%versionString%%` → `ProjectInfo::versionString` before config key loop

### Files Modified

- `Source/terminal/rendering/Screen.cpp` — `cellAtPoint()` rewritten (physical-pixel round-trip)
- `Source/component/TerminalComponent.h` — removed `verticalInset`/`horizontalInset`, added 4 `const int padding*` members
- `Source/component/TerminalComponent.cpp` — `resized()` uses 4 `removeFrom*`, `mouseWheelMove` uses SGR + config scroll step, `shouldForwardMouseToPty` docstring corrected
- `Source/config/Config.h` — `scrollbackNumLines` → `terminalScrollbackLines`, `scrollbackStep` → `terminalScrollStep`, added 4 `terminalPadding*` constants
- `Source/config/Config.cpp` — `initDefaults()` + `initSchema()` updated, `terminal.padding` array parser added, `writeDefaults()` substitutes `%%versionString%%`
- `Source/config/default_end.lua` — `scrollback` table → `terminal` table, `padding` array with full CSS-order comment, bit-font logo + version stamp
- `Source/terminal/logic/Grid.cpp:57` — `scrollbackNumLines` → `terminalScrollbackLines`
- `Source/MainComponent.cpp` — `showMessageOverlay()` reads 4 config padding values, calls `showResize()` with padding args
- `Source/component/MessageOverlay.h` — `showResize()` accepts 4 padding params, `paintRulers()` insets ruler bounds by padding, 4 `resizePad*` members added

### Alignment Check

- **LIFESTAR Lean:** `cellAtPoint` fix is 4 lines replacing 2. Padding is 4 flat keys parsed by a single dedicated block. No new abstractions.
- **LIFESTAR Explicit Encapsulation:** Padding lives in `terminal.*` — terminal behaviour, not window chrome. `paintRulers` is a static free function — no state, no coupling. `writeDefaults` substitutes version before config loop — explicit ordering.
- **LIFESTAR SSOT:** `terminal.padding` array is the sole source — parsed once into 4 flat keys, read from those keys everywhere. No shadow copies.
- **LIFESTAR Findable:** All terminal behaviour config under `terminal.*`. Ruler drawing in `MessageOverlay.h` alongside `showResize`.
- **LIFESTAR Reviewable:** `cellAtPoint` docstring explains the physical round-trip and why it matches `getCellBounds`. `paintRulers` docstring explains grid inset and Path gap approach.
- **NAMING-CONVENTION:** `terminalScrollbackLines`, `terminalScrollStep`, `terminalPaddingTop/Right/Bottom/Left`, `paintRulers`, `showResize`, `resizePadTop` — all semantic, no data-source encoding.
- **ARCHITECTURAL-MANIFESTO:** `cellAtPoint` is a pure coordinate transform — no side effects. `paintRulers` is a pure paint function — no state mutation. Config parser is additive — new array path doesn't touch existing scalar path.

### Technical Debt / Follow-up

- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** — still violates NAMING-CONVENTION Rule 2 (encodes "Tree" in name). Pre-existing debt from Sprint 91.
- **`seq 1M` performance gap** — still 2m33s vs Terminal's 1m12s. Pre-existing debt from Sprint 91.
- **`terminal.padding` is read at construction only** — `TerminalComponent` members are `const int` initialized at construction. Hot-reload (`Cmd+R`) does not update padding until next session restart. To support live reload, padding members would need to be non-const and `applyConfig()` would need to call `resized()`.
- **`CursorComponent` missing `setInterceptsMouseClicks(false, false)`** — cursor cell swallows clicks. Pre-existing debt from Sprint 92.

---

## Sprint 97 — BackgroundBlur Architecture Fix: Unified macOS/Windows Glass

**Date:** 2026-03-16
**Agents:** COUNSELOR, @engineer, @pathfinder, @oracle, @researcher, @auditor

### Problem

TextEditor inside GlassWindow disappeared on Windows. GL terminal rendered correctly but any software-rendered JUCE component (TextEditor for command palette) was invisible. Same root cause made kuassa plugin dialog windows show flat white instead of glass blur.

### Root Cause

`applyDwmGlass()` stripped `WS_EX_LAYERED` from the window and called `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})`. JUCE's software renderer for transparent windows (`setOpaque(false)` + no native title bar) uses `TransparencyKind::perPixel` mode, painting via `UpdateLayeredWindow()`. Stripping `WS_EX_LAYERED` caused `UpdateLayeredWindow` to silently fail on every repaint — content painted into an offscreen bitmap that never reached the screen.

GL windows survived because OpenGL bypasses JUCE's software paint pipeline entirely (`wglSwapBuffers` writes directly to the framebuffer). PopupMenu blur worked by accident — painted once before async blur fired, then never needed repainting (short-lived).

### What Was Done

**1. BackgroundBlur architecture unified across platforms**

Split the Windows implementation to match macOS architecture:

| | macOS | Windows |
|---|---|---|
| `apply()` | `CGSSetWindowBackgroundBlurRadius` / `NSVisualEffectView` | `SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND)` |
| `enableGLTransparency()` | `NSOpenGLContextParameterSurfaceOpacity = 0` | Strip `WS_EX_LAYERED` + `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})` |

`apply()` is rendering-agnostic — safe for any window (GL or software). `enableGLTransparency()` is GL-only — called from `Screen::glContextCreated()`.

**2. `applyDwmGlass()` — safe for all windows**
- Removed `WS_EX_LAYERED` stripping
- Removed `DwmExtendFrameIntoClientArea` call
- Win11 Mica attributes set but fall through to accent policy (Mica needs frame extension which only GL windows get)
- Accent policy (`SetWindowCompositionAttribute`) always applied — works with `WS_EX_LAYERED` windows

**3. `enableGLTransparency()` — GL-specific DWM setup**
- Was a no-op on Windows, now performs the invasive DWM operations
- Gets HWND via `wglGetCurrentDC()` → `WindowFromDC()` → `GetAncestor(GA_ROOT)` (JUCE creates internal GL child window; must walk up to top-level)
- Strips `WS_EX_LAYERED` (GL doesn't use `UpdateLayeredWindow`)
- Calls `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})`

**4. ActionList fixes**
- Explicit TextEditor colours from Config (background, text, caret, outline)
- Removed unused `searchBox` member
- Added Escape key dismissal (`keyPressed` override)

**5. Kuassa library fork**
- `kuassa_background_blur.cpp` updated with identical fix (namespace `kuassa` instead of `jreng`)

### Files Modified

- `modules/jreng_gui/glass/jreng_background_blur.cpp` — `applyDwmGlass()` rewritten (removed WS_EX_LAYERED strip + frame extension), `enableGLTransparency()` rewritten (GL-specific DWM setup with GetAncestor walk)
- `Source/terminal/action/ActionList.h` — removed unused `searchBox` member, added `keyPressed` override
- `Source/terminal/action/ActionList.cpp` — explicit TextEditor colours, Escape dismissal
- `~/Documents/Poems/kuassa/___lib___/kuassa_graphics/glass/kuassa_background_blur.cpp` — identical fix forked from jreng module

### Files NOT Modified

- `modules/jreng_gui/glass/jreng_background_blur.h` — API surface unchanged
- `modules/jreng_gui/glass/jreng_background_blur.mm` — macOS implementation untouched

### Alignment Check

- **LIFESTAR Lean:** Fix is minimal — moved two operations between two functions. No new abstractions.
- **LIFESTAR Explicit Encapsulation:** `apply()` is rendering-agnostic. `enableGLTransparency()` is GL-specific. Each function has one clear responsibility. Callers don't need to know the rendering mode.
- **LIFESTAR SSOT:** One blur API surface, two platform implementations, identical structure.
- **LIFESTAR Findable:** Same file, same function names, same call sites on both platforms.
- **LIFESTAR Reviewable:** Doxygen on both functions explains the split and why each operation lives where it does.
- **NAMING-CONVENTION:** No new identifiers. Existing names preserved.
- **ARCHITECTURAL-MANIFESTO:** Tell don't ask. `apply()` tells the window to blur. `enableGLTransparency()` tells the GL context to composite. Neither queries the other.

### Key Discovery: JUCE perPixel Transparency + WS_EX_LAYERED

When a JUCE window has `setOpaque(false)` + no native title bar, JUCE calculates `TransparencyKind::perPixel` and adds `WS_EX_LAYERED`. All painting goes through `UpdateLayeredWindow()` with an ARGB bitmap. Stripping `WS_EX_LAYERED` externally (via `SetWindowLongPtrW`) while JUCE's internal state still says `perPixel` causes every subsequent repaint to silently fail — `UpdateLayeredWindow` returns `FALSE` on a non-layered window but JUCE doesn't check the return value.

### Key Discovery: JUCE GL Child Window

`juce::OpenGLContext::attachTo()` creates an internal child window for the GL surface. `wglGetCurrentDC()` → `WindowFromDC()` returns this child HWND, not the top-level window. `WS_EX_LAYERED` and `DwmExtendFrameIntoClientArea` are top-level window attributes — must use `GetAncestor(hwnd, GA_ROOT)` to walk up.

### Technical Debt / Follow-up

- **Win11 Mica on software-rendered windows:** Mica requires `DwmExtendFrameIntoClientArea` which is only called for GL windows. Software-rendered windows on Win11 get accent policy blur instead. Visual parity between GL and software windows on Win11 not yet achieved.
- **Apple Silicon:** `NSOpenGLContext` deprecated. `enableGLTransparency()` needs Metal equivalent (`CAMetalLayer.opaque = NO`). macOS `CGSSetWindowBackgroundBlurRadius` deprecated on Monterey+ but `NSVisualEffectView` fallback catches it.
- **Kuassa plugin build not yet tested** — forked code needs verification in plugin host context.

---

## Sprint 96 — Configurable Popup Terminals + Action Ownership + Ctrl+C Fix

**Date:** 2026-03-16
**Agents:** COUNSELOR, @engineer, @researcher, @librarian, @pathfinder

### Problem

Sprint 95 delivered a hardcoded popup spawning a default shell. Needed: configurable popup entries from Lua (command, args, cwd, modal/global keys), Action ownership moved to Main.cpp, popup auto-dismiss on process exit, and Ctrl+C not reaching TUI apps.

### What Was Done

**1. Config: `popups` table**
- `Config::PopupEntry` struct: command, args, cwd, width, height, modal, global
- `Config::getPopups()` accessor, `clearPopups()` for reload
- Three-level Lua parsing: `END.popups.<name>.<field>` with validation (command required, at least one key binding)
- Per-popup width/height with global `popup.width`/`popup.height` fallback
- `default_end.lua` updated: comprehensive commented example block (tit, lazygit, htop)
- Removed: `keys.popup`, `popup.action` (single-popup design replaced by `popups` table)

**2. Config: `onReload` callback**
- `Config::onReload` — `std::function<void()>` fired at end of `reload()`
- Wired in `Main.cpp::initialise()` → calls `MainComponent::applyConfig()`
- `MainComponent::applyConfig()` — public method: `registerActions()` + `tabs->applyConfig()` + LookAndFeel + orientation
- reload_config action simplified: just calls `config.reload()`, shows message

**3. Action ownership moved to Main.cpp**
- `Terminal::Action action` moved from MainComponent to ENDApplication (alongside Config, AppState, FontCollection)
- All Contexts now owned by the app, constructed before the window
- `Action::clear()` — public method, wipes entries + bindings
- `Action::buildKeyMap()` — made public, called after registration
- MainComponent accesses via `Action::getContext()`, no member
- `registerActions()` calls `action.clear()`, registers all fixed + popup actions, calls `action.buildKeyMap()`

**4. Popup actions from Config**
- Each `popups` entry registers as `"popup:<name>"` (modal) and/or `"popup_global:<name>"` (global)
- `Action::buildKeyMap()` resolves popup modal/global keys from `Config::getPopups()`
- Shared `launchPopup` lambda per entry — DRY, no duplicate callbacks
- Shell wrapping: `config.shellProgram -c command` (e.g. `zsh -c tit`)

**5. Session shell override**
- `Session::setShellProgram (program, args)` — overrides Config default
- `Terminal::Component (program, args, cwd)` constructor — calls `setShellProgram` + `setWorkingDirectory` before `initialise()`
- Session stays dumb — receives shell + args, launches them

**6. Popup auto-dismiss on process exit**
- `Terminal::Component::onProcessExited` — public callback, replaces default quit-app behavior
- `Popup::show()` wires `terminal->onProcessExited = [this] { dismiss(); }` — Popup owns its own dismissal
- `WindowsTTY::waitForData()` — `WaitForMultipleObjects` on both `readEvent` and `process` handle. When child exits, cancels pending read and signals EOF. Fixes ConPTY keeping pipe alive after process exit.

**7. Popup terminal input: tmux overlay model**
- Popup terminals bypass `Terminal::Action` entirely in `keyPressed`
- All keys go directly to PTY — no copy interception, no prefix handling
- `escapeKeyTriggersCloseButton` set to `false` — Escape goes to TUI, not dismiss
- Matches tmux's `popup_key_cb` pattern: overlay owns all input while active

**8. Ctrl+C fix (Win32 Input Mode bypass)**
- `Keyboard::encodeWin32Input()` — Ctrl+C sends raw `\x03` (ETX) instead of Win32 Input Mode sequence
- Root cause: zsh enables Win32 Input Mode (`?9001h`). Go/Rust TUI apps (tit, lazygit) don't understand Win32 Input Mode sequences. They expect standard VT input for signal-generating keys.
- `\x03` is universal — PTY line discipline generates SIGINT. Works on all platforms.

### Files Created
- None

### Files Modified
- `Source/config/Config.h` — removed `keysPopup`/`popupAction`, added `PopupEntry` struct, `popups` map, `getPopups()`, `clearPopups()`, `onReload` callback
- `Source/config/Config.cpp` — removed keysPopup/popupAction from defaults+schema, added `popups` table parsing, `getPopups()`, `clearPopups()`, `onReload` fired from `reload()`
- `Source/config/default_end.lua` — removed `keys.popup`/`popup.action`, added `popup` defaults section, added commented `popups` example block
- `Source/Main.cpp` — added `Terminal::Action action` member, `#include Action.h`, wired `config.onReload`
- `Source/MainComponent.h` — removed `Terminal::Action action` member, added public `applyConfig()`
- `Source/MainComponent.cpp` — `applyConfig()` method, `registerActions()` uses `Action::getContext()`, popup actions from `config.getPopups()`, reload_config simplified
- `Source/terminal/action/Action.h` — `clear()` public, `buildKeyMap()` public
- `Source/terminal/action/Action.cpp` — `clear()` implementation, popup key resolution in `buildKeyMap()`
- `Source/terminal/logic/Session.h` — `setShellProgram()`, `shellOverride`/`shellArgsOverride` members
- `Source/terminal/logic/Session.cpp` — `setShellProgram()` implementation, `resized()` uses override
- `Source/component/TerminalComponent.h` — `onProcessExited` callback, `Component (program, args, cwd)` constructor
- `Source/component/TerminalComponent.cpp` — new constructor, `onProcessExited` in `initialise()`, popup bypass in `keyPressed`
- `Source/component/Popup.cpp` — `escapeKeyTriggersCloseButton = false`, `onProcessExited` wired in `show()`
- `Source/terminal/tty/WindowsTTY.cpp` — `WaitForMultipleObjects` on process handle in `waitForData()`
- `Source/terminal/data/Keyboard.cpp` — Ctrl+C sends `\x03` bypassing Win32 Input Mode

### Alignment Check
- **LIFESTAR Lean:** Shell wrapping is one line in MainComponent. Session stays dumb. Popup wires its own dismissal.
- **LIFESTAR Explicit Encapsulation:** Action owned by Main (Context). Config fires `onReload`, doesn't know about Action. Popup wires `onProcessExited` itself — MainComponent doesn't manage popup lifecycle. Each object has one job.
- **LIFESTAR SSOT:** `Config::getPopups()` is the sole source for popup entries. Action registry rebuilt from scratch on every reload.
- **LIFESTAR Findable:** `PopupEntry` in Config.h. Popup actions prefixed `"popup:"`.
- **NAMING-CONVENTION:** `PopupEntry`, `getPopups`, `clearPopups`, `onReload`, `onProcessExited`, `setShellProgram`, `shellOverride` — all semantic.
- **ARCHITECTURAL-MANIFESTO:** Tell don't ask. Config tells listeners via `onReload`. Popup tells itself to dismiss via `onProcessExited`. tmux overlay model: popup owns all input.

### Key Discovery: Win32 Input Mode + TUI Apps

Go/Rust TUI apps don't understand Win32 Input Mode (`?9001h`). When zsh enables it and a TUI launches inside zsh without disabling it, signal-generating keys (Ctrl+C) are sent as Win32 Input Mode sequences that the TUI ignores. Fix: Ctrl+C always sends raw `\x03` regardless of Win32 Input Mode.

### Key Discovery: ConPTY Pipe Stays Open After Process Exit

ConPTY (sideloaded `OpenConsole.exe`) keeps the pipe alive after the child process exits. `ReadFile` never returns `ERROR_BROKEN_PIPE`. Fix: `WaitForMultipleObjects` on both the read event and the process handle. When the process exits, cancel the pending read and signal EOF.

### Technical Debt / Follow-up
- **Multiple popup configs not tested with reload** — hot-reload should re-register popup actions correctly via `onReload` chain
- **Per-popup width/height fallback** — registered in Config but not consumed in MainComponent's `launchPopup` (always uses global defaults)
- **`onProcessExited` naming** — used as a flag to detect popup terminals in `keyPressed`. Should have a dedicated `isPopupTerminal` flag instead of overloading callback presence.
- **Ctrl+C bypass is Ctrl+C only** — other signal keys (Ctrl+Z, Ctrl+\) may have the same Win32 Input Mode issue. Should audit all signal-generating keys.
- **`escapeKeyTriggersCloseButton = false`** — Escape goes to TUI. No way to dismiss popup except process exit. May want a configurable dismiss key in future.

---
