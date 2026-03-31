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

## Sprint 9: CSI 3J scrollback clear, lock unlocked grid reads

**Date:** 2026-03-31

### Agents Participated
- COUNSELOR — root cause analysis (scrollback duplication via captured PTY logs, DWM timing race eliminated, ring buffer arithmetic verified, threading verified, CC escape sequence flow decoded), plan, directed all execution
- Pathfinder — traced ring buffer architecture (head/scrollbackUsed/physicalRow), identified all unlocked `getScrollbackUsed()`/`scrollbackRow()`/`activeVisibleRow()` call sites (13 total across 4 files)
- Librarian — compared scrollback implementations across kitty, WezTerm, Windows Terminal (data structures, CSI 2J behavior, thread safety patterns)
- Researcher (×2) — researched Ink rendering patterns, CSI 22J adoption, alternate screen spec; decoded CC binary (`LcH` custom renderer, `clearTerminal` sequence: `CSI 2J + CSI 3J + CSI H`, offscreen detection trigger)
- Oracle — brutal assessment of proposed State refactor (correctly killed the approach — head must stay with buffer for cell data consistency)
- Engineer — CSI 3J implementation, ScopedLock wrapping for all unlocked call sites

### Files Modified (7 total)
- `Source/terminal/logic/ParserEdit.cpp:126-134` — separated CSI 2J (case 2) from CSI 3J (case 3); mode 3 now erases visible rows AND calls `grid.clearScrollback()` to reset `scrollbackUsed` to 0
- `Source/terminal/logic/Grid.h:173` — added `clearScrollback() noexcept` declaration; added `getResizeLock() const noexcept` overload (line 136); marked `resizeLock` as `mutable` (line 689)
- `Source/terminal/logic/Grid.cpp:96-99` — `clearScrollback()` implementation: resets `bufferForScreen().scrollbackUsed = 0`; added const overload of `getResizeLock()` (line 68)
- `Source/component/TerminalComponent.cpp:304,777` — added `ScopedLock` to `enterSelectionMode()` and `setScrollOffsetClamped()`
- `Source/component/InputHandler.cpp:82,171,410` — added `ScopedLock` to `handleScrollNav()`, `handleSelectionKey()` (covers lines 171 and 352), `setScrollOffsetClamped()`
- `Source/component/MouseHandler.cpp:102,284` — added `ScopedLock` to `toAbsoluteRow()` and `handleDoubleClick()`
- `Source/terminal/selection/LinkManager.cpp:scan(),scanForHints()` — added `ScopedLock` at entry points covering all `scanViewport()`/`buildPages()` grid reads

### Alignment Check
- [x] LIFESTAR principles followed — SSOT (scrollbackUsed written by reader thread, read under resizeLock by all consumers); Explicit Encapsulation (Grid::clearScrollback is a Grid instruction, Parser tells Grid what to do); Lean (minimal change — one new method, lock additions only)
- [x] NAMING-CONVENTION.md adhered — `clearScrollback` is verb, consistent with `clearBuffer`
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no layer violations; mutable resizeLock is standard idiom for synchronization primitives in const contexts
- [x] JRENG-CODING-STANDARD.md followed — zero early returns, brace init, `not`/`and`/`or`, Allman braces

### Problems Solved
- **Scrollback duplication with Claude Code:** CC's custom renderer sends `CSI 2J + CSI 3J + CSI H` during offscreen full resets to wipe scrollback before reprinting. END treated mode 3 identically to mode 2 (erase visible only), ignoring the scrollback clear. Old scrollback persisted, CC reprinted the same conversation, duplicate content appeared in scroll buffer. Confirmed via `script` capture: 3 occurrences of `[2J[3J` in session log matching the "couple of times" duplication observed. Fix: mode 3 now calls `grid.clearScrollback()`.
- **Unlocked grid reads on message thread:** 13 call sites across TerminalComponent, InputHandler, MouseHandler, and LinkManager read `getScrollbackUsed()`, `scrollbackRow()`, or `activeVisibleRow()` without holding `resizeLock` — racing against the reader thread's `scrollUp()` writes. Fix: all call sites now acquire `ScopedLock` on `resizeLock`.

### Technical Debt / Follow-up
- Parser still holds `Grid&` directly (Explicit Encapsulation violation identified during investigation). Parser should not know Grid — discussed but deferred as a larger architectural refactor
- `head` and `scrollbackUsed` remain as plain `int` in `Grid::Buffer` (not in State). Oracle assessment confirmed this is correct — head must be consistent with cell data under the same lock. Moving to State would split head from cells across time domains.
- Sprint 6 debt item "CPU rendering on Windows" resolved in Sprint 8 (GlassWindow glass lifecycle)

---

## Sprint 8: Fix CPU rendering on Windows, GlassWindow glass lifecycle

**Date:** 2026-03-31

### Agents Participated
- COUNSELOR — root cause analysis (DWM timing race, incomplete accent reset, GlassWindow state model), plan, directed all execution
- Pathfinder — traced full CPU/GPU rendering divergence path, viewport pipeline, DWM glass lifecycle, window opacity chain
- Librarian — JUCE peer creation timing research (synchronous via addToDesktop, not lazy)
- Engineer — GlassWindow rewrite (OS-divergent glass lifecycle), BackgroundBlur rename, disableWindowTransparency accent reset
- Auditor — verified all new code against contracts (1 Critical pre-existing, 0 in new code)

### Files Modified (10 total)
- `modules/jreng_gui/glass/jreng_glass_window.h` — AsyncUpdater inheritance macOS-only (`#if JUCE_MAC`); removed `blurApplied`, added macOS-only `isBlurApplied` one-shot guard; added `windowColour` member; added `setGlassEnabled (bool)` public API; removed `visibilityChanged`/`handleAsyncUpdate` from Windows path
- `modules/jreng_gui/glass/jreng_glass_window.cpp` — OS-divergent constructor: Windows starts opaque with real colour, macOS starts transparent; `setGlassEnabled(bool)` stateless instruction (enable/disable blur + opacity); macOS async first-show path preserved with one-shot guard; Windows meta-drag unchanged
- `modules/jreng_gui/glass/jreng_background_blur.h` — `apply` → `enable`, `disableWindowTransparency` → `disable` (both platforms)
- `modules/jreng_gui/glass/jreng_background_blur.cpp` — `disable()` now resets DWM accent policy to `ACCENT_DISABLED` via `SetWindowCompositionAttribute`; preserves Win11 rounded corners via `DwmSetWindowAttribute(DWMWCP_ROUND)`; definitions renamed
- `modules/jreng_gui/glass/jreng_background_blur.mm` — definitions renamed `apply` → `enable`, `disableWindowTransparency` → `disable`; doc comments updated
- `modules/jreng_gui/glass/jreng_glass_component.h` — doc comment references updated
- `modules/jreng_gui/glass/jreng_glass_component.cpp` — call site renamed
- `Source/Main.cpp` — Windows: calls `setGlassEnabled(isGpu)` synchronously after construction; config reload: calls `setGlassEnabled` on both platforms
- `Source/MainComponent.cpp` — removed `BackgroundBlur::apply/disableWindowTransparency` from `applyConfig()` — glass is GlassWindow's concern, driven by Main.cpp
- `Source/component/Popup.cpp` — call site renamed
- `Source/component/LookAndFeel.cpp` — call site renamed

### Alignment Check
- [x] LIFESTAR principles followed — Lean (no state machine, `setGlassEnabled` is stateless instruction); Explicit Encapsulation (glass lifecycle owned by GlassWindow, MainComponent no longer pokes glass APIs); SSOT (`gpu_acceleration` config is single source for glass/renderer decisions)
- [x] NAMING-CONVENTION.md adhered — `enable`/`disable` verb pair; `setGlassEnabled` is verb + adjective; `isBlurApplied` is boolean naming convention; `windowColour` is semantic
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no layer violations; module (jreng_gui) has no dependency on app layer (AppState); Main.cpp (app layer) drives glass state
- [x] JRENG-CODING-STANDARD.md followed — zero early returns in new code, brace init, `not`/`and`/`or`, Allman braces

### Problems Solved
- **CPU rendering blank on Windows:** Root cause was DWM timing race — `handleAsyncUpdate` applied acrylic blur AFTER `disableWindowTransparency` cleanup ran (cleanup posted first, fired first, glass applied second with no further cleanup). Fix: Windows path is synchronous via `setGlassEnabled`, no async race.
- **Incomplete DWM cleanup:** `disableWindowTransparency` only reset DWM margins, left `ACCENT_ENABLE_ACRYLICBLURBEHIND` accent policy active. Fix: `disable()` now resets accent to `ACCENT_DISABLED`.
- **Window lost rounded corners in CPU mode:** `DwmSetWindowAttribute(DWMWCP_ROUND)` was only called inside `applyDwmGlass`. Fix: `disable()` also sets rounded corners on Win11.
- **BackgroundBlur API naming:** `apply`/`disableWindowTransparency` asymmetric pair → `enable`/`disable` symmetric pair.
- **Glass concern leaked into MainComponent:** `applyConfig()` directly called `BackgroundBlur::apply/disable`. Fix: glass lifecycle moved to GlassWindow, driven by Main.cpp via `setGlassEnabled`.

### Technical Debt / Follow-up
- `LookAndFeel.h` has CRLF→LF line ending changes in the diff (not intentional, git autocrlf artifact)
- Hot reload on macOS: `config.onReload` calls `setGlassEnabled` which calls `BackgroundBlur::enable` — re-applies glass including potential duplicate `NSVisualEffectView`. `applyNSVisualEffect` is not idempotent (adds subview each call). Not a regression (same pre-existing behavior as old `apply` path).
- Sprint 6 debt item resolved: "CPU rendering on Windows — grid is empty" — root cause was DWM glass, not grid dimensions

---

## Sprint 7: Fix copy buffer scroll drift, modal exit on yank

**Date:** 2026-03-31

### Agents Participated
- COUNSELOR — root cause analysis (coordinate space mismatch, modal exit gap), plan, directed execution, applied trivial fix
- Pathfinder — traced full mouse selection → copy → extractText coordinate flow, identified drift root cause
- Engineer — scrollOffset parameter addition, caller updates
- Auditor — verified all 6 call sites updated, no missed callers, backward compatibility confirmed

### Files Modified (4 total)
- `Source/terminal/logic/Grid.h:416,437` — `extractText` and `extractBoxText` declarations: added `int scrollOffset` parameter, updated doc comments
- `Source/terminal/logic/Grid.cpp:720-771,792-826` — both functions use `scrollbackRow(row, scrollOffset)` and `scrollbackGraphemeRow(row, scrollOffset)` instead of `activeVisibleRow(row)` and `activeReadGrapheme(row, col)`
- `Source/component/TerminalComponent.cpp:225,231,237,245,252` — `copySelection()` reads `scrollOffset` from state, passes to all three extract call sites; added `setModalType(none)` so yank from `keyPressed` shortcut exits modal selection
- `Source/component/InputHandler.cpp:287,293,305` — keyboard yank path passes existing `scrollOffset` to all three extract call sites

### Alignment Check
- [x] LIFESTAR principles followed — Explicit Encapsulation (scroll offset passed through API, not reached into); SSOT (single coordinate translation point in Grid)
- [x] NAMING-CONVENTION.md adhered — `scrollOffset` matches existing parameter naming in `scrollbackRow`/`scrollbackGraphemeRow`
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no layer violations, no new accessors needed
- [x] JRENG-CODING-STANDARD.md followed — zero early returns, brace init, existing code style preserved

### Problems Solved
- **Copy drifts from visual selection when scrolled back:** `extractText`/`extractBoxText` called `activeVisibleRow(row)` which maps to the live viewport (scrollOffset=0). `screenSelection` coordinates were computed with scroll offset in `onVBlank`. When scrolled back N lines, copied text drifted by exactly N lines. Fix: both functions now take `scrollOffset` and use `scrollbackRow`/`scrollbackGraphemeRow` — when scrollOffset=0, formula is identical to `activeVisibleRow` (backward compatible).
- **Modal selection not exiting after yank:** `keyPressed` intercepts copy key when `screenSelection != nullptr`, calls `copySelection()` and returns before `handleSelectionKey` runs. `copySelection()` cleared `selectionType` and `dragActive` but not `modalType`. Fix: added `setModalType(none)` to `copySelection()`.

### Technical Debt / Follow-up
- None identified from this sprint

---

## Sprint 6: Popup fixes, yank from mouse selection, Windows drag-drop fix

**Date:** 2026-03-30

### Agents Participated
- COUNSELOR — root cause analysis (GL context sharing, selection clear, OLE drag-drop), plan, directed all execution
- Pathfinder — typeface cell dimension API, Config Value::Type enum, default_end.lua popup section, AppState cwd API, selection/copy flow, InputHandler key access, CMake definitions
- Librarian — JUCE FileDragAndDropTarget + OpenGL + Windows OLE research
- Engineer — all code changes (GL shared context, cols/rows migration, cwd passthrough, yank wiring, selection clear, JUCE_DLL_BUILD)

### Files Modified (12 total)
- `modules/jreng_opengl/context/jreng_gl_renderer.h` — added `setSharedRenderer (GLRenderer& source)` declaration
- `modules/jreng_opengl/context/jreng_gl_renderer.cpp` — `setSharedRenderer` implementation: wraps `openGLContext.setNativeSharedContext()` for GL texture namespace sharing
- `Source/component/Popup.h` — `show()` signature: `int width, int height, GLRenderer& sharedRenderer`; `ContentView` takes `GLRenderer&`; added `sharedSource` member; `Window` takes `GLRenderer&`; removed stale popupWidth/popupHeight doc refs
- `Source/component/Popup.cpp` — `ContentView` stores `sharedSource` ref, calls `glRenderer.setSharedRenderer (sharedSource)` before `attachTo()`; `show()` receives pixel dimensions directly; `Window` passes `sharedRenderer` through
- `Source/MainComponent.cpp` — `launchPopup` lambda: resolves cols/rows from entry (fallback to global Config), computes pixel size via `typeface.calcMetrics()`, resolves cwd from `appState.getPwd()` when empty
- `Source/config/Config.h` — `PopupEntry`: `float width/height` → `int cols/rows`; Keys: `popupWidth/popupHeight` → `popupCols/popupRows`
- `Source/config/Config.cpp` — `addKey` defaults: `popupCols = 70`, `popupRows = 20` (range 1–640, 1–480); Lua parsing reads `cols`/`rows` as int
- `Source/config/default_end.lua` — popup section: `width`/`height` → `cols`/`rows`; tit example: `cols = 80, rows = 24`; inline docs updated
- `Source/component/InputHandler.h` — added `isSelectionCopyKey (const juce::KeyPress&)` declaration
- `Source/component/InputHandler.cpp` — `isSelectionCopyKey` implementation: checks against `selectionKeys.copy` and `selectionKeys.globalCopy`
- `Source/component/TerminalComponent.cpp` — `keyPressed`: intercepts copy key when mouse selection active; `copySelection()`: added `setSelectionType (none)` to clear State so VBlank doesn't rebuild selection
- `CMakeLists.txt` — `JUCE_DLL_BUILD=1` added then reverted (broke `WebBrowserComponent` private inheritance `operator new`/`delete`)
- `.gitignore` — added `.end/`

### Alignment Check
- [x] LIFESTAR principles followed — Explicit Encapsulation (`setSharedRenderer` hides GL internals; `isSelectionCopyKey` encapsulates key config; orchestrator tells, doesn't ask); SSOT (per-entry cols/rows with global fallback); Lean (minimal API additions)
- [x] NAMING-CONVENTION.md adhered — `setSharedRenderer`, `isSelectionCopyKey` are verbs; `sharedSource` is semantic noun
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no layer violations, no poking internals
- [x] JRENG-CODING-STANDARD.md followed — zero early returns, brace init, alternative tokens

### Problems Solved
- **Popup text not rendering (GPU mode):** Static `GLuint` atlas handles invalid in popup's separate `juce::OpenGLContext`. Fix: `GLRenderer::setSharedRenderer()` shares GL texture namespace via `setNativeSharedContext()`.
- **Per-popup dimensions ignored:** `Popup::show()` read global fractions, ignoring per-entry values. Fix: MainComponent computes pixel size from entry cols/rows * cell dimensions.
- **Dimension unit changed:** Fractions (0.1–1.0) replaced with cols/rows (integers) — natural unit for terminal popups.
- **Popup cwd inheritance:** Empty `entry.cwd` now resolves to active terminal's cwd via `appState.getPwd()`.
- **Yank from mouse selection:** `y` (and `cmd+c`/`ctrl+c`) now copies mouse-selected text without entering modal mode. `InputHandler::isSelectionCopyKey()` encapsulates key comparison.
- **Selection persists after copy:** `copySelection()` was missing `setSelectionType (none)` — VBlank rebuilt `screenSelection` from stale State params.
- **Windows release drag-drop broken:** Root cause is UAC elevation blocking OLE drops from non-elevated Explorer. `JUCE_DLL_BUILD=1` attempted but reverted — it changes `operator new`/`delete` visibility and breaks `WebBrowserComponent` private inheritance.

### Technical Debt / Follow-up
- CPU rendering on Windows — grid is empty (nonEmpty:0), wrong dimensions (46x14 instead of ~87x31). `isRowIncludedInSnapshot` is a CPU-only filter in `updateSnapshot` (Screen.h:870) — GPU always returns true. Investigation incomplete.
- Windows drag-drop only fails when running elevated (UAC). Non-elevated works. No fix applied.
- `Popup.h:13` stale comment: "Content size is computed from Config fractions in show()"
- `setNativeSharedContext` WGL behaviour untested on Windows — popup GL confirmed on Mac only
- CAROL SPRINT-LOG rotated: sprints 122-123 moved to git history

---

## Sprint 5: Reflow truncation, pane fixes, Uuid rename

**Date:** 2026-03-30

### Agents Participated
- COUNSELOR — root cause analysis for all three issues, directed execution
- Pathfinder — pane close/resizer bar flow tracing, focusPane navigation tracing, Uuid identifier inventory
- Engineer — all code changes (4 invocations: pinToBottom, non-wrapped truncation, orphaned resizer cleanup, focusPane visibility, Uuid rename)

### Files Modified (13 total)
- `Source/terminal/logic/GridReflow.cpp` — `pinToBottom` condition: bottom-align only when cursor was at last visible row; non-wrapped lines (`runLen == 1`) truncated to newCols instead of reflowed via `effectiveLen` cap in `countOutputRows`, cursor tracking, and `writeReflowedContent`
- `Source/component/Panes.cpp` — orphaned resizer bar cleanup after `paneManager.remove()` (scan for detached split nodes); `focusPane()` visibility guards on both loops (skip hidden panes under Whelmed overlay)
- `Source/AppIdentifier.h` — `activePaneUuid` renamed to `activePaneID`
- `Source/AppState.h` — `getActivePaneUuid`/`setActivePaneUuid` renamed to `getActivePaneID`/`setActivePaneID`
- `Source/AppState.cpp` — same renames in definitions + property references
- `Source/component/PaneComponent.h` — `setActivePaneUuid` call renamed
- `Source/component/Tabs.cpp` — all `*Uuid` calls/locals renamed to `*ID`
- `Source/component/TerminalComponent.cpp` — `getActivePaneUuid` call renamed (juce::Uuid untouched)
- `Source/config/Config.cpp` — renamed references
- `Source/config/Config.h` — renamed references
- `Source/config/default_end.lua` — renamed references
- `modules/jreng_gui/layout/jreng_pane_manager.h` — `newUuid`/`nodeUuid` params renamed to `newID`/`nodeID`
- `modules/jreng_gui/layout/jreng_pane_manager.cpp` — same renames

### Alignment Check
- [x] LIFESTAR principles followed — SSOT (pinToBottom uses actual cursor position), Lean (truncation simpler than full reflow for non-wrapped lines), Explicit Encapsulation (resizer bar cleanup checks own state via getParent)
- [x] NAMING-CONVENTION.md adhered — `*Uuid` renamed to `*ID` (Rule 2: no type encoding; UUID is implementation detail, ID is semantic)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md followed — zero early returns, brace init, alternative tokens

### Problems Solved
- **Active prompt pinned to bottom on empty screen:** `pinToBottom = (cursorRow >= oldVisibleRows - 1)` — only bottom-aligns when cursor was at the last row of old viewport. Empty screen / sparse output keeps position.
- **Columnar ls output mangled after split:** Non-wrapped lines (ended with newline, `runLen == 1`) now truncated to `newCols` instead of reflowed to multiple rows. Soft-wrapped lines still reflow correctly.
- **Orphaned resizer bar after 3-pane close:** After `paneManager.remove()` restructures tree, orphaned bars (detached split node) are cleaned up in a second pass.
- **Focus traversal skips Whelmed pane:** `focusPane()` now filters by `isVisible()` — hidden terminal under Whelmed overlay excluded from navigation.
- **Uuid naming convention:** All user-defined `*Uuid` identifiers renamed to `*ID` across 9 files (41 occurrences).

### Technical Debt / Follow-up
- Non-wrapped line truncation loses characters beyond `newCols` permanently — acceptable for disposable output (ls), but long non-wrapped lines won't restore on re-widen
- `activePaneID` property string change breaks existing state.xml on first launch (falls back to default, non-destructive)

---

## Sprint 4: Comprehensive docs audit and debt cleanup

**Date:** 2026-03-30

### Agents Participated
- COUNSELOR — directed audit scope and fix delegation
- Auditor — comprehensive docs audit across 16 source files + ARCHITECTURE.md + SPEC.md + PLAN.md (17 findings: 8 High, 6 Medium, 3 Low)
- Machinist — applied all doc fixes across 9 files, deleted PLAN.md
- Engineer — fixed TTY.cpp contract violations (early returns, brace init, missing braces)
- Pathfinder — TTY.cpp debt inventory, clearBuffer call site analysis

### Files Modified (9 total)
- `Source/terminal/data/State.h` — ValueTree structure diagram: removed PARAM lines for cols/visibleRows, added as direct SESSION properties; thread ownership table: dims reader column corrected to "Grid buffer (resizeLock)"; flushRootParams doc: removed cols/visibleRows from handled params
- `Source/terminal/data/State.cpp` — constructor doc steps 1 and 4: cols/visibleRows described as CachedValue properties, not PARAMs
- `Source/terminal/data/StateFlush.cpp` — removed cols/visibleRows from "Parameters handled here" list; changed "reader-thread API" to "reader-thread setters"; removed dims from flush() step 2 description
- `Source/terminal/logic/Grid.h` — thread ownership table: split resize/clearBuffer into separate rows with correct threads; resizeLock docs updated for message-thread resize + reader-thread data processing; reflow @note corrected
- `Source/terminal/logic/Grid.cpp` — constructor @note: READER THREAD to MESSAGE THREAD; initBuffer @note: documents both thread call sites
- `Source/terminal/logic/Parser.h` — Parser::resize @note: READER THREAD to MESSAGE THREAD
- `Source/terminal/logic/Parser.cpp` — Parser::resize @note: same fix
- `Source/terminal/tty/TTY.cpp` — drainPty lambda: eliminated early return via isEof single return; brace init for n; all if bodies braced; main loop: break eliminated via shellDone predicate
- `ARCHITECTURE.md` — resizeLock purpose: "Grid resize + data processing safety"; Grid Ring Buffer section: documents buffer-based dim accessors and message-thread resize
- `PLAN.md` — deleted (Sprint 134 execution plan, fully implemented)

### Alignment Check
- [x] LIFESTAR principles followed — documentation now matches code reality (SSOT for docs)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — Explicit thread annotations corrected
- [x] JRENG-CODING-STANDARD.md followed — TTY.cpp now contract-compliant (zero early returns, brace init, braces on all if bodies)

### Problems Solved
- 8 High-severity stale thread annotations fixed (resize/reflow/Parser::resize all annotated as READER THREAD, now correctly MESSAGE THREAD)
- ValueTree structure diagram in State.h showed cols/visibleRows as PARAM children (removed in Sprint 134)
- flushRootParams docs claimed to handle cols/visibleRows (they no longer exist as PARAMs)
- TTY.cpp contract violations: early return in drainPty, copy init, break in loop, missing braces
- PLAN.md deleted to prevent confusion with completed plan

### Technical Debt / Follow-up
- None identified from this sprint


### Alignment Check
- [x] LIFESTAR principles followed — SSOT enforced, Explicit Encapsulation (objects manage own state via `isThreadRunning()`), Lean (removed 5 shadow stores)
- [x] NAMING-CONVENTION.md adhered — `contentExtent`, `allocatedCols`, `allocatedVisibleRows` are semantic
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no state shadowing, no manual boolean flags, no layer violations
- [x] JRENG-CODING-STANDARD.md followed — zero early returns, brace init, `not`/`and`/`or`, `.at()`

### Problems Solved
- **Cursor off-by-1 after split close:** Content was top-aligned with empty space below. Fixed: `contentExtent` includes cursor row when past content; `head` positions content at bottom of viewport
- **Text outside bounds after horizontal split:** Parser read new dims from State while grid buffer had old dims (race condition). Fixed: `grid.resize()` runs on message thread; parser acquires `resizeLock` per data chunk — zero transition window
- **Prompt jumps to row 0 on split create:** Same race — parser cursor calculations used wrong dims during transition. Fixed by same resizeLock approach
- **5 shadow stores for dims removed:** `Screen::numRows`, `TTY::pendingCols`/`pendingRows`/`resizePending`, `Session::ttyOpened`/`ttyOpenPending` — all eliminated
- **Manual callbacks removed:** `tty->onResize`, `handleResize` lambda — replaced by direct `grid.resize()` call on message thread
- **Dims migrated to CachedValue:** `cols`/`visibleRows` removed from atomic parameterMap; CachedValue provides instant ValueTree sync on message thread; reader thread reads from grid buffer under resizeLock

### Technical Debt / Follow-up
- `Grid::scrollbackCapacity` is still a member, not on State — immutable after construction
- `clearBuffer()` doc says READER THREAD but may now be called from message thread contexts — verify all call sites
