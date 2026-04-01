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

## Sprint 12: Grid::Writer facade, cursor responsiveness, BackgroundBlur cleanup

**Date:** 2026-04-01

### Agents Participated
- COUNSELOR — comprehensive audit (Sprints 7-11), cursor lag root cause analysis (two-clock race between VBlank and State timer), Grid::Writer architecture planning, delegated all execution
- Pathfinder — cursor update data flow trace (parameterMap → flush → ValueTree → snapshot), focus tracking trace (focusGained/focusLost vs onVBlank toFront), enableWindowTransparency include chain analysis, Grid method inventory for Writer
- Oracle — Grid-Parser coupling analysis: cataloged all 20+ Grid methods Parser calls, categorized hot/cold/query, evaluated 6 decoupling options (virtual, template, callback, facade, split, status quo), recommended Option D (thin facade)
- Auditor — 4-dimension audit (dead code, stale docs, SSOT violations, refactoring), Grid::Writer migration verification (7 checks all PASS), BackgroundBlur early return verification
- Engineer — State::refresh(), isForegroundProcess guard, Grid::Writer class, Parser migration (6 files), State parameterMap registration (cols/visibleRows/scrollbackUsed), GroundOps struct, scrollback callback, enableWindowTransparency internal guard, BackgroundBlur early return elimination
- Machinist — clean sweep on State.h, StateFlush.cpp, TerminalComponent.cpp (orphaned doxygen, stale caller annotations)

### Files Modified (16 total)
- `Source/terminal/data/State.h` — `refresh()` public API; `flush()` back to private; `setScrollbackUsed()` added; `setDimensions()` doxygen updated for parameterMap
- `Source/terminal/data/State.cpp:152-154` — `cols`, `visibleRows`, `scrollbackUsed` registered as root-level PARAMs; `setDimensions()` writes parameterMap atomics alongside CachedValues
- `Source/terminal/data/StateFlush.cpp` — `refresh()` definition; `flush()` doxygen step order corrected; caller annotations updated
- `Source/terminal/data/Identifier.h` — added `scrollbackUsed` identifier
- `Source/terminal/logic/Grid.h:598-660` — `Grid::Writer` nested public class: 19 forwarding methods (write/scroll/erase), `onScrollbackChanged` callback
- `Source/terminal/logic/Parser.h` — `Grid& grid` → `Grid::Writer writer`; constructor takes `Grid::Writer`; removed `class Grid;` forward decl, added `#include "Grid.h"`; added `setScrollbackCallback()`
- `Source/terminal/logic/Parser.cpp` — constructor updated; `grid.getVisibleRows()` → `state.getRawValue<int>(ID::visibleRows)`
- `Source/terminal/logic/ParserVT.cpp` — anonymous namespace → `GroundOps` struct with `Cursor` nested type; all `grid.` → `writer.`; geometry reads through `state.getRawValue<int>()`; `setScrollbackCallback` implementation
- `Source/terminal/logic/ParserCSI.cpp` — all `grid.` → `writer.`; geometry reads through State
- `Source/terminal/logic/ParserEdit.cpp` — all `grid.` → `writer.`; geometry reads through State
- `Source/terminal/logic/ParserESC.cpp` — all `grid.` → `writer.`; geometry reads through State
- `Source/terminal/logic/ParserOps.cpp` — all `grid.` → `writer.`; geometry reads through State
- `Source/terminal/logic/Session.cpp:158` — `Grid::Writer { grid }` construction; scrollback callback wired to `state.setScrollbackUsed()`
- `Source/component/TerminalComponent.cpp:599` — `session.getState().refresh()` in onVBlank; `juce::Process::isForegroundProcess()` guard on `toFront(true)`
- `modules/jreng_gui/glass/jreng_background_blur.cpp` — `enableWindowTransparency()` internally guards with `isWindows10()`; `enable()` and `applyDwmGlass()` early returns eliminated (single return per function)
- `DEBT.md` — removed stale CursorComponent item; `enableWindowTransparency` marked resolved; `getTreeMode` naming marked resolved

### Alignment Check
- [x] LIFESTAR principles followed — Explicit Encapsulation (Grid::Writer restricts Parser's access surface; enableWindowTransparency decides internally); SSOT (geometry reads through State parameterMap, not Grid); Lean (Writer is ~30 lines of inline passthroughs, zero overhead)
- [x] NAMING-CONVENTION.md adhered — `Writer` (noun, nested in Grid); `GroundOps` (noun, describes ground-state operations); `refresh` (verb, signals intent); `Cursor` (noun, lightweight snapshot)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — APVTS pattern (atomics for reader thread, ValueTree for UI); tell-don't-ask (Writer writes, State reads); no layer violations
- [x] JRENG-CODING-STANDARD.md followed — zero early returns in all modified/new code; brace init; alternative tokens; Allman braces; `noexcept` preserved

### Problems Solved
- **Cursor sluggish by 1 char:** Two independent clocks — VBlank (display-rate) and State timer (60-120 Hz) — raced. VBlank read cursor from ValueTree before timer flushed atomics. Fix: `State::refresh()` (public) calls `flush()` internally; Component calls `refresh()` in onVBlank before rendering. Atomic exchange makes timer and VBlank idempotent — no fighting.
- **Cursor visible when window unfocused:** `onVBlank` called `toFront(true)` every frame when component lacked focus, re-stealing focus after OS window lost it. `focusGained` reset `cursorFocused = true`, making cursor reappear. Fix: `juce::Process::isForegroundProcess()` guard — only re-grab focus when the OS window is foreground.
- **Parser held Grid& directly (Explicit Encapsulation violation):** Parser had unrestricted access to all Grid methods including message-thread-only methods (resize, extractText, getResizeLock). Fix: `Grid::Writer` facade exposes only the 19 methods Parser needs (cell writes, scroll, erase, dirty tracking). Geometry reads (getCols, getVisibleRows, scrollbackUsed) route through State parameterMap — APVTS pattern, lock-free atomic reads on reader thread.
- **Anonymous namespace in ParserVT.cpp:** Violated coding standard (no blank namespaces). Fix: `GroundOps` struct with 4 static inline methods and nested `Cursor` type.
- **enableWindowTransparency redundant on Win11:** Called from TerminalComponent unconditionally, but on Win11 `applyDwmGlass` already did the same work. Fix: guard lives inside the function (Explicit Encapsulation — caller tells, function decides). No include poisoning.
- **Early returns in BackgroundBlur:** `enable()` and `applyDwmGlass()` had multiple early returns. Fix: restructured to single `return result` per function with nested positive checks.

### Technical Debt / Follow-up
- Comprehensive audit findings (Sprint 7-11) documented but not yet actioned: 12 dead code items, 17 stale doc items, 19 refactoring opportunities. See audit report in session context.
- `scrollbackUsed` initial value: Grid's ring buffer counter is the source of truth at startup. First `scrollRegionUp` via Writer fires the callback and syncs State. Before that, State reads 0. Verify this is acceptable for the startup path.
- `getScrollbackUsed()` remains on Grid (public) for message-thread consumers (TerminalComponent, InputHandler). Only Parser is decoupled via Writer. Full elimination would require those consumers to read from State ValueTree instead.
- Pre-existing: `applyNSVisualEffect` not idempotent on macOS hot reload; GPU probe approach unresolved; `seq 1M` performance gap; font fallback arrows block.

---

## Sprint 11: cursorVisible SSOT — remove MODES shadow

**Date:** 2026-04-01

### Agents Participated
- COUNSELOR — identified dual-storage SSOT violation, directed fix, root cause analysis of crash (parameterMap.at() throws on missing key)
- Pathfinder — exhaustive cursorVisible audit: all storage locations, all writers, all readers, full chain trace from CSI ?25h through atomic to ValueTree to snapshot

### Files Modified (3 total)
- `Source/terminal/data/State.cpp:211` — deleted `addParam (modesNode, ID::cursorVisible, 1.0f)` — removed MODES shadow PARAM
- `Source/terminal/logic/ParserCSI.cpp:976` — deleted `state.setMode (ID::cursorVisible, enable)` — removed MODES write from DECTCEM ?25 handler; `setCursorVisible (scr, enable)` is the sole write
- `Source/terminal/logic/ParserVT.cpp:858` — replaced `state.setMode (ID::cursorVisible, true)` with `state.setCursorVisible (normal, true)` in `resetModes()` — writes per-screen slot directly

### Alignment Check
- [x] LIFESTAR principles followed — SSOT (single per-screen storage, no shadow); Lean (removed dead storage)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no state shadowing
- [x] JRENG-CODING-STANDARD.md followed

### Problems Solved
- **cursorVisible dual storage:** `cursorVisible` existed in BOTH MODES subtree and per-screen (NORMAL/ALTERNATE) subtrees. `setCursorVisible` wrote per-screen, `setMode(ID::cursorVisible)` wrote MODES. `isCursorVisible()` read only per-screen. MODES slot was dead storage — written but never read. `resetModes()` wrote only MODES, leaving per-screen stale after soft-reset. Fix: removed MODES PARAM, all writes go through `setCursorVisible` (per-screen).
- **Previous crash root cause identified:** removing the MODES PARAM without removing `setMode(ID::cursorVisible)` callers caused `parameterMap.at("MODES_cursorVisible")` to throw `std::out_of_range` on the reader thread — crash on first CSI ?25h from the shell.

### Technical Debt / Follow-up
- GPU availability probe deferred — `attachTo` on 1x1 throwaway component causes crash on some configurations. Needs further investigation into JUCE OpenGLContext lifecycle.
- Check all other MODES params for similar shadow patterns — are any MODES values also stored per-screen with divergent read/write paths?

---

## Sprint 10: Eliminate State atomic getters, rename getTreeMode/getKeyboardFlags

**Date:** 2026-03-31

### Agents Participated
- COUNSELOR — identified architectural violation (State exposing atomic reads as public API, bypassing ValueTree SSOT), directed elimination of all atomic getters, resolved getMode ODR collision
- Pathfinder — exhaustive inventory of all 13 atomic getter methods, all call sites, thread ownership per caller, identified `getMode` duplicate declaration (ODR violation)
- Oracle — brutal assessment of proposed head/scrollbackUsed-to-State refactor (killed it — head must stay with buffer for cell data consistency under resizeLock)
- Engineer — eliminated all atomic getters, created ValueTree getters for cursor/shape/color, converted all Parser callers to getRawValue, converted Grid::bufferForScreen, renamed getTreeMode→getMode + getTreeKeyboardFlags→getKeyboardFlags

### Files Modified (21 total)
- `Source/terminal/data/State.h` — deleted 13 atomic getter declarations (getScreen, getMode atomic, getCursorRow/Col, isCursorVisible, isWrapPending, getScrollTop/Bottom, getCursorShape, getCursorColorR/G/B, getKeyboardFlags(screen)); added 7 ValueTree getters (getCursorRow/Col, isCursorVisible, getCursorShape, getCursorColorR/G/B); renamed getTreeMode→getMode, getTreeKeyboardFlags→getKeyboardFlags; made screenKey/modeKey/buildParamKey public
- `Source/terminal/data/State.cpp` — deleted 13 atomic getter implementations; added getScreenParamInt/getScreenParamFloat helpers + 7 ValueTree getter implementations; deleted getTreeMode/getTreeKeyboardFlags implementations (replaced by renamed getMode/getKeyboardFlags)
- `Source/terminal/logic/ParserVT.cpp` — all state.getScreen/getMode/getCursorRow/Col/isWrapPending/getScrollTop calls converted to getRawValue<T>(screenKey/modeKey)
- `Source/terminal/logic/ParserCSI.cpp` — same conversions
- `Source/terminal/logic/ParserEdit.cpp` — same conversions
- `Source/terminal/logic/ParserESC.cpp` — same conversions
- `Source/terminal/logic/ParserOps.cpp` — same conversions
- `Source/terminal/logic/Parser.cpp` — same conversions
- `Source/terminal/logic/Grid.cpp` — bufferForScreen() now uses getRawValue<ActiveScreen> for reader thread; added const overload of getResizeLock() for const Grid& holders
- `Source/terminal/logic/GridReflow.cpp` — reflow() cursor reads converted to getRawValue
- `Source/terminal/logic/GridScroll.cpp` — scrollUp() screen read converted to getRawValue
- `Source/terminal/logic/Session.cpp` — getMode→getMode (ValueTree), getKeyboardFlags→getKeyboardFlags (ValueTree)
- `Source/component/MouseHandler.cpp` — getMode→getMode (ValueTree), added ScopedLock to toAbsoluteRow + handleDoubleClick
- `Source/component/TerminalComponent.cpp` — enterSelectionMode uses no-arg getCursorRow/Col; added ScopedLock
- `Source/terminal/rendering/ScreenSnapshot.cpp` — all cursor reads converted to no-arg ValueTree getters
- `Source/terminal/rendering/ScreenRender.cpp` — previousCursorRow uses no-arg getCursorRow
- `ARCHITECTURE.md` — getTreeKeyboardFlags references updated to getKeyboardFlags
- `DEBT.md` — stale entries removed (CursorComponent, getTreeMode naming resolved), added Parser→Grid Explicit Encapsulation debt
- `modules/jreng_gui/glass/jreng_glass_window.cpp` — reverted Windows constructor divergence (both platforms: transparentBlack + setOpaque(false))

### Alignment Check
- [x] LIFESTAR principles followed — SSOT (all getters now read ValueTree, atomics are write-only transport); Explicit Encapsulation (State no longer exposes atomic internals, tell-not-ask); Lean (getRawValue is the single raw accessor, APVTS pattern)
- [x] NAMING-CONVENTION.md adhered — getMode (was getTreeMode, "Tree" was type encoding), getKeyboardFlags (was getTreeKeyboardFlags)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no layer violations; State atomic map is internal, never exposed; Parser uses getRawValue (APVTS pattern for reader thread)
- [x] JRENG-CODING-STANDARD.md followed — zero early returns, brace init, `not`/`and`/`or`, Allman braces

### Problems Solved
- **State atomic getters violated SSOT:** 13 public methods read raw atomics, bypassing ValueTree. Message-thread callers (ScreenSnapshot, TerminalComponent, MouseHandler, Session) read pre-flush values instead of post-flush SSOT. Fix: all named getters now read ValueTree. Parser uses getRawValue<T> directly (APVTS pattern).
- **getMode ODR violation:** Engineer's rename of getTreeMode→getMode created duplicate declaration (atomic at line 518, ValueTree at line 646). Both had identical signatures. Fix: deleted atomic version, kept ValueTree version as THE getMode.
- **getTreeMode/getTreeKeyboardFlags naming:** "Tree" prefix encoded storage type (violated NAMING-CONVENTION Rule 2). Renamed to getMode/getKeyboardFlags after atomic versions were eliminated.
- **GlassWindow white flash on Windows:** Sprint 8 made Windows constructor start opaque (setOpaque(true)), causing visible flash when setGlassEnabled switched to transparent. Fix: reverted to unified path — both platforms start setOpaque(false) + transparentBlack.

### Technical Debt / Follow-up
- GPU availability probe reverted — `attachTo` during MainComponent constructor causes side effects (flash, state corruption). Needs a different approach: probe timing must be after peer exists but before window visible, with no GL surface side effects.
- Parser still holds Grid& directly (Explicit Encapsulation violation from Sprint 9). Grid::bufferForScreen uses getRawValue internally — acceptable compromise but not ideal per ARCHITECT's tell-not-ask principle.
- `tickCursorBlink` in State.cpp still uses getRawValue internally (line 1076) — this is State reading its own atomics, acceptable.

---

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

<!-- Sprints 4-6 rotated to git history -->
