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

## Sprint 95 — Terminal::Popup: Modal Glass Dialog with GL Rendering

**Date:** 2026-03-16
**Agents:** COUNSELOR, @engineer, @pathfinder

### Problem

ActionList (command palette) scaffolded in Sprint 94 could never grab keyboard focus when added as a child of MainComponent. The tmux-style popup from SPEC.md was identified as the correct primitive — a modal glass window that hosts any component, grabs focus, and blocks the parent.

### What Was Done

**1. Terminal::Popup** (`Source/component/Popup.h` + `Popup.cpp`)
- Modal glass dialog using `juce::DialogWindow` with `escapeKeyTriggersCloseButton = true`
- `jreng::BackgroundBlur` applied via deferred `juce::AsyncUpdater` (identical pattern to `jreng::GlassWindow` and `kuassa::GlassDialogWindow`)
- Escape dismisses (JUCE built-in), click outside brings to front (does not dismiss)
- `Popup::show (caller, content)` — reads Config fractions, sizes content, creates window, centres on caller, enters modal state
- `Popup::dismiss()` — exits modal state, releases window
- `onDismiss` callback for cleanup

**2. ContentView pattern** (mirrors MainComponent)
- `Popup::ContentView` — intermediate content component inside the DialogWindow
- Owns `jreng::GLRenderer` attached to itself (not to the DialogWindow)
- Sets `componentIterator` to yield the single GL content component
- Wires `Terminal::Component::onRepaintNeeded` → `glRenderer.triggerRepaint()`
- `initialiseGL()` called after window is visible (native peer exists)
- Architecture: `Window (DialogWindow) → ContentView → Terminal::Component` — identical to `GlassWindow → MainComponent → Terminal::Component`

**3. Config keys**
- Added: `popup.width` (0.6), `popup.height` (0.5), `popup.position` ("center"), `popup.action` ("action_list")
- Renamed: `keys.action_list` → `keys.popup`, default `"?"`
- Removed: `keys.action_list_position` (replaced by `popup.position`)
- Schema: `popup.width`/`popup.height` range-validated [0.1, 1.0]

**4. Action system updated**
- Action ID `"action_list"` → `"popup"`
- Action key table entry: `Config::Key::keysPopup`
- MainComponent registration: creates `Terminal::Component`, passes to `popup.show()`

**5. default_end.lua updated**
- New `popup` section with `width`, `height`, `position`, `action`
- `keys.popup` replaces `keys.action_list`

### Key Discovery: GL Context Cannot Cross Native Window Boundaries

`juce::OpenGLContext` is attached to one native window. A `DialogWindow` creates a separate native OS window. The main app's GLRenderer (attached to MainComponent) cannot render into the popup's window. Solution: each popup window gets its own GLRenderer attached to its own ContentView — identical architecture to the main app.

### Files Created
- `Source/component/Popup.h` (231 lines)
- `Source/component/Popup.cpp` (172 lines)

### Files Modified
- `Source/config/Config.h` — `keysActionList` → `keysPopup`, removed `keysActionListPosition`, added `popupWidth`/`popupHeight`/`popupPosition`/`popupAction`
- `Source/config/Config.cpp` — updated `initDefaults()` and `initSchema()`
- `Source/config/default_end.lua` — new `popup` section, `keys.popup` replaces `keys.action_list`
- `Source/terminal/action/Action.cpp` — action key table: `keysActionList`/`"action_list"` → `keysPopup`/`"popup"`
- `Source/terminal/action/ActionList.cpp:59` — `keysActionListPosition` → `popupPosition`
- `Source/MainComponent.h` — added `#include "component/Popup.h"`, added `Terminal::Popup popup` member
- `Source/MainComponent.cpp` — `"popup"` action creates `Terminal::Component` and calls `popup.show()`

### Alignment Check
- **LIFESTAR Lean:** Popup is a thin controller (~170 lines). ContentView is minimal. No unnecessary abstractions.
- **LIFESTAR Explicit Encapsulation:** Popup knows nothing about Terminal::Component. ContentView detects GL content via `dynamic_cast` — generic for any `jreng::GLComponent`. Window handles its own glass blur. Each object has one job.
- **LIFESTAR SSOT:** Config is the sole source for popup size/position. `Config& config` member reference resolved once, used everywhere.
- **LIFESTAR Findable:** `Source/component/Popup.h` — alongside Tabs, Panes, MessageOverlay.
- **LIFESTAR Reviewable:** Architecture documented in header doxygen. Pattern explicitly mirrors MainComponent.
- **NAMING-CONVENTION:** `Popup`, `ContentView`, `initialiseGL`, `popupWidth`, `popupAction` — all semantic, no data-type encoding.
- **ARCHITECTURAL-MANIFESTO:** Tell don't ask. `show()` is an instruction. Popup doesn't poke Window internals. Window doesn't poke ContentView internals.

### Technical Debt / Follow-up
- **`popup.action` not yet interpreted** — currently hardcoded to spawn `Terminal::Component`. Needs dispatch logic: if value matches END keyword (`"action_list"`), spawn ActionList; otherwise spawn terminal running the shell command.
- **ActionList not wired as popup content** — ActionList.h/cpp exist but are not used. Next step: ActionList inside Popup when `popup.action == "action_list"`.
- **Multiple popup configs** — SPEC.md describes per-popup entries (lazygit, htop, etc.). Current Config supports only one popup. Future: `popups` array in Lua.
- **Popup not resizable** — `setResizable (false, false)`. May want draggable/resizable in future.
- **`close_pane` action** still blocked on missing Config key.

---



## Sprint 92 — ConPTY Sideload: Mouse + Alternate Screen on Windows

**Date:** 2026-03-15  
**Agents:** COUNSELOR, @engineer, @researcher, @pathfinder

### Problem

Mouse was completely non-functional on Windows. ConPTY on Windows 10 22H2 (build 19045) intercepts all DECSET sequences (`?1000h`, `?1003h`, `?1006h`, `?1049h`) and does NOT re-emit them in the output stream. The terminal emulator has zero information about mouse tracking or alternate screen state. The inbox `conhost.exe` does not support `PSEUDOCONSOLE_WIN32_INPUT_MODE` (0x4 flag).

### Root Cause

Discovered by examining wezterm's source (`pty/src/win/psuedocon.rs`): wezterm sideloads `conpty.dll` + `OpenConsole.exe` from Microsoft Terminal's open-source project (MIT license). The sideloaded `conpty.dll` launches `OpenConsole.exe` (a newer conhost) instead of the inbox `conhost.exe`. This newer conhost supports `PSEUDOCONSOLE_WIN32_INPUT_MODE` and emits DECSET sequences in the output stream.

The fix was **one flag** (`0x4`) — but it only works with the sideloaded DLL, not the inbox conhost.

### What Was Done

**1. ConPTY sideload mechanism**
- Embedded `conpty.dll` (109 KB) and `OpenConsole.exe` (1.1 MB) from Microsoft Terminal as JUCE BinaryData
- On first `open()`, extracts both to `~/.config/end/conpty/` (skips if size matches — update-safe)
- Loads `CreatePseudoConsole`, `ResizePseudoConsole`, `ClosePseudoConsole` from sideloaded DLL
- Falls back to `kernel32.dll` if sideload fails
- Passes `PSEUDOCONSOLE_WIN32_INPUT_MODE` (0x4) flag to `CreatePseudoConsole`

**2. Mouse gate restored**
- `shouldForwardMouseToPty()` returns `isMouseTracking()` on all platforms
- With sideloaded ConPTY, `isMouseTracking()` now correctly returns true when TUI apps enable mouse (ConPTY emits `?1003;1006h` in output stream)

**3. State::getActiveScreen() (from Sprint 91)**
- Now works correctly — sideloaded ConPTY emits `?1049h` so `setScreen()` is called and `getActiveScreen()` returns `alternate` when appropriate

**4. Diagnostics removed**
- All temporary `std::cout`, file logging, and `DBG` diagnostics removed from Session.cpp, ParserCSI.cpp, ParserEdit.cpp, TerminalComponent.cpp

### Files Modified

- `Resources/windows/conpty.dll` — NEW (MIT-licensed binary from Microsoft Terminal)
- `Resources/windows/OpenConsole.exe` — NEW (MIT-licensed binary from Microsoft Terminal)
- `CMakeLists.txt:254-264` — Added explicit binary data entries for conpty.dll + OpenConsole.exe (WIN32 only)
- `Source/terminal/tty/WindowsTTY.cpp` — Added `ConPtyFuncs` struct, `extractConPtyBinaries()`, `loadConPtyFuncs()`. All ConPTY API calls now go through function pointers loaded from sideloaded DLL (or kernel32 fallback). `PSEUDOCONSOLE_WIN32_INPUT_MODE` flag passed.
- `Source/terminal/logic/Session.cpp` — Diagnostics removed, `onData` restored to clean one-liner
- `Source/component/TerminalComponent.cpp` — `shouldForwardMouseToPty()` cleaned up, diagnostics removed

### Alignment Check

- **LIFESTAR:** Lean (sideload is ~60 lines, single static initializer), Explicit (ConPtyFuncs struct makes function source traceable), Single Source of Truth (all three ConPTY functions from same DLL), Findable (extraction path documented in docstring)
- **NAMING-CONVENTION:** `ConPtyFuncs`, `extractConPtyBinaries`, `loadConPtyFuncs` — all semantic, no data-source encoding
- **ARCHITECTURAL-MANIFESTO:** TTY layer manages its own dependencies. No poking from Session or TerminalComponent. The sideload is an implementation detail of WindowsTTY — callers don't know or care.

### Technical Debt / Follow-up

- **Binary size increase:** ~1.2 MB added to END.exe from embedded conpty.dll + OpenConsole.exe. Acceptable trade-off for mouse support.
- **Update mechanism:** Size-based check for extraction. If Microsoft updates the binaries, END must be rebuilt with new versions. No version checking beyond file size.
- **Windows 11:** The inbox conhost on Windows 11 may support `PSEUDOCONSOLE_WIN32_INPUT_MODE` natively. The sideload is harmless (same behavior) but could be skipped on newer OS versions.
- **`seq 1M` performance gap** (from Sprint 91): Still 2m33s vs Terminal's 1m12s. Drain loop exits too early. Separate sprint.
- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** violates NAMING-CONVENTION Rule 2. Pre-existing debt.
- **`CursorComponent` missing `setInterceptsMouseClicks(false, false)`** — cursor cell swallows clicks. Now that mouse works, this should be fixed.

---


