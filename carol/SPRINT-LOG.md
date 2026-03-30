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

---

## Sprint 3: SSOT Resize with CachedValue

**Date:** 2026-03-30

### Agents Participated
- COUNSELOR — root cause analysis, architecture plan, directed all execution
- Pathfinder — codebase inventory (dim read/write sites, resizeLock acquisition, Buffer struct, State flush system)
- Librarian — juce::CachedValue research (thread safety, referTo, listener behavior)
- Engineer — all code changes across 3 phases (6 invocations)
- Auditor — Phase 1 validation, final comprehensive audit (2 invocations)

### Files Modified (15 total)
- `Source/component/TerminalComponent.cpp:208-213` — `resized()` writes `state.setDimensions(cols, rows)` before `session.resized()`; dims computed into local vars
- `Source/terminal/data/State.h` — added `juce::CachedValue<int> cachedCols`/`cachedVisibleRows` members; removed `setCols()`/`setVisibleRows()` declarations; `getCols()`/`getVisibleRows()` doc updated to MESSAGE THREAD + CachedValue
- `Source/terminal/data/State.cpp` — `setDimensions()` writes CachedValue only (no atomics); `getCols()`/`getVisibleRows()` read from CachedValue; removed `setCols()`/`setVisibleRows()` impls; removed `addParam` for cols/visibleRows; CachedValue bound in constructor via `referTo()`
- `Source/terminal/logic/Grid.h` — `getCols()`/`getVisibleRows()` return `buffers.at(normal).allocatedCols`/`allocatedVisibleRows` (buffer intrinsic, not State); removed `needsResize()` declaration; thread ownership table updated
- `Source/terminal/logic/Grid.cpp` — `initBuffer()` stores `allocatedCols`/`allocatedVisibleRows` on Buffer; removed `needsResize()` impl; internal methods use `getCols()`/`getVisibleRows()` (buffer-based) instead of `state.getCols()`
- `Source/terminal/logic/GridReflow.cpp` — `resize()` reads old dims from Buffer (not State), no longer writes State; `reflow()` cursor fallback moved before head calc; `contentExtent = jmax(totalOutputRows, cursorOutputRow+1) - rowsToSkip` for bottom-aligned head; cursor formula `cursorOutputRow - rowsToSkip + newVisibleRows - contentExtent`; doc comments updated MESSAGE THREAD
- `Source/terminal/logic/GridErase.cpp` — `state.getCols()`/`getVisibleRows()` replaced with Grid's buffer-based `getCols()`/`getVisibleRows()`
- `Source/terminal/logic/GridScroll.cpp` — same migration as GridErase
- `Source/terminal/logic/Parser.h` — `activeScrollBottom()` declaration only (body moved out-of-line)
- `Source/terminal/logic/Parser.cpp` — `activeScrollBottom()` out-of-line definition using `grid.getVisibleRows()` (was `state.getVisibleRows()` inline)
- `Source/terminal/logic/Session.h` — removed `ttyOpened`/`ttyOpenPending` members
- `Source/terminal/logic/Session.cpp` — `onData` acquires `grid.getResizeLock()` before `process()`; `resized()` always calls `grid.resize()`+`parser.resize()` on message thread; `tty->isThreadRunning()` replaces manual flags; removed `onBeforeDrain` setup; `onDrainComplete` uses `grid.getCols()`/`grid.getVisibleRows()`
- `Source/terminal/tty/TTY.h` — removed `resize()` method, `onResize`/`onBeforeDrain` callbacks, `resizePending`/`pendingCols`/`pendingRows` atomics; `platformResize()` made public
- `Source/terminal/tty/TTY.cpp` — `run()` simplified: no `handleResize`, no `onBeforeDrain`; doc comments updated

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

