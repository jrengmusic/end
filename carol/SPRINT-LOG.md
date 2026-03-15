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

## Sprint 90: Document Reorganization + Roadmap Expansion

**Date:** 2026-03-12
**Role:** COUNSELOR

### Agents Participated
- COUNSELOR: Planning, document restructuring, roadmap authoring
- Pathfinder (x1): Discovered current codebase state for ARCHITECTURE.md updates (AppState, Tabs, Identifier, State, Session, Keyboard, Config, Panes, TerminalComponent, AppIdentifier)

### Objective

Reorganize project documentation: separate implemented features (ARCHITECTURE.md) from future plans (SPEC.md). Extract detailed future specs to SPEC-details.md. Add 7 new feature specs to roadmap.

### Files Modified (3)

- **SPEC.md** — Rewritten as forward-looking roadmap (v0.4.0). Stripped all implemented feature documentation. Added 7 new feature specs: Keybinding Reorganization (unified action registry, global + modal, fully configurable), Command Palette (fuzzy search, native OS dialog), Tmux-Style Popup (user-defined popup terminals), File Opener / Flash-Jump (ls integration, hint labels like flash.nvim), Inline Image Rendering (Sixel + iTerm2 OSC 1337), Generalized GL Text Rendering Module (jreng_text extraction), WHELMED Integration (markdown + mermaid as juce::Component in split panes). Phases reorganized: Phase 3 = Keybinding + UX, Phase 4 = Rendering + Protocol, Phase 5 = Module Extraction + WHELMED. Updated overview: "fully-featured" replaces "minimalistic".

- **SPEC-details.md** — NEW. Section 21 (Terminal State Serialization) extracted verbatim from old SPEC.md, renumbered as Section 1. Standalone detailed spec for future implementation.

- **ARCHITECTURE.md** — Updated with recent session work:
  - Module map: added `AppState.h/cpp`, `AppIdentifier.h`, updated `Keyboard.h` description
  - Module inventory: added AppState row, updated Config and Component dependencies
  - New section: Working Directory Tracking (AppState::pwdValue, Value::referTo pattern, binding lifecycle, Panes::createTerminal with workingDirectory)
  - New section: Tab Name Management (Value::Listener pattern, displayName computation priority, shellProgram on SESSION, SESSION node identification via jreng::ID::id)
  - New section: Input Encoding (Shift+Enter CSI u `\x1b[13;2u`)
  - New section: Platform Configuration (config file paths per OS, Windows %APPDATA%)
  - Glossary: added AppState, displayName, pwdValue, shellProgram; updated Tabs description
  - Overview updated: "fully-featured" replaces "minimalistic"

### Alignment Check
- [x] LIFESTAR Single Source of Truth: ARCHITECTURE.md = current code, SPEC.md = future plans, no overlap
- [x] LIFESTAR Findable: clear document hierarchy (SPEC -> SPEC-details -> ARCHITECTURE)
- [x] LIFESTAR Explicit: each document has stated purpose and scope
- [x] NAMING-CONVENTION adhered

### Architecture Decisions
- **Document separation principle**: implemented features documented in ARCHITECTURE.md only. SPEC.md is forward-looking only. Detailed future specs in SPEC-details.md.
- **Phase reorganization**: Phases 3-5 restructured around feature clusters (UX, Protocol, Module Extraction) rather than arbitrary numbering.

### Problems Solved
1. **SPEC.md bloat** — was 1395 lines documenting both implemented and unimplemented features. Now ~500 lines, forward-looking only.
2. **Stale SPEC** — implemented features in SPEC.md were not reflected in code and diverged from ARCHITECTURE.md. Eliminated by removing them from SPEC.md entirely.
3. **Missing ARCHITECTURE.md sections** — pwd tracking, tab name management, Shift+Enter, Windows config path now documented.

### Technical Debt / Follow-up
1. **SPEC-details.md** — only contains serialization spec. Future detailed specs should be added here as sections.
2. **Build not verified** — documentation-only changes, no code modified.

---

## Sprint 89: Split Pane Bug Fixes + Codebase Audit + Documentation

**Date:** 2026-03-12
**Role:** COUNSELOR

### Agents Participated
- COUNSELOR: Planning, delegation, audit coordination, doc review
- Engineer (x2): ARCHITECTURE.md updates, README.md updates
- Auditor (x2): Verified ARCHITECTURE.md and README.md edits
- Pathfinder (x1): Discovered doc gaps and current API surface

### Files Modified (20 total)

**Bug Fixes + Refactoring:**
- `modules/jreng_gui/layout/jreng_pane_manager.h` — findLeaf made public, full doxygen, namespace closing fixed
- `modules/jreng_gui/layout/jreng_pane_manager.cpp` — remove() fixed (parent==state check, sibling removeChild before re-parenting), namespace closing fixed
- `modules/jreng_gui/layout/jreng_pane_resizer_bar.h` — namespace format fixed, doxygen added
- `modules/jreng_gui/layout/jreng_pane_resizer_bar.cpp` — namespace format fixed, destructor=default, if-init brace init fixed
- `Source/component/Panes.h` — findPaneNode removed, splitImpl added, doxygen updated
- `Source/component/Panes.cpp` — splitImpl extracted, findPaneNode removed (uses PaneManager::findLeaf), closePane reordered (ungraft SESSION first), redundant jassert+if removed
- `Source/component/Tabs.h` — focusLastTerminal added, doxygen updated/added
- `Source/component/Tabs.cpp` — all implicit pointer checks fixed, focusLastTerminal extracted, focus-after-close uses adjacent index
- `Source/MainComponent.h` — bindModalActions added
- `Source/MainComponent.cpp` — bindModalActions extracted, implicit pointer checks fixed, split commands removed from commandActions
- `Source/config/ModalKeyBinding.h` — splitHorizontal/splitVertical actions added, doxygen added
- `Source/config/ModalKeyBinding.cpp` — file-scope actionKeys array, implicit std::function check fixed, split config keys added
- `Source/config/KeyBinding.h` — splitHorizontal/splitVertical removed from CommandID
- `Source/config/KeyBinding.cpp` — file-scope actionIDs array, array sizes 13->11, loop bounds updated
- `Source/config/Config.h` — keys changed to snake_case (keys.split_horizontal, keys.split_vertical)
- `Source/config/Config.cpp` — defaults changed to `\\` and `-`, writeDefaults updated

**Documentation:**
- `ARCHITECTURE.md` — Added: Split Pane System section (PaneManager, PaneResizerBar, Panes, ModalKeyBinding, Close Cascade), 3 design decisions (Binary Tree ValueTree, Prefix Key, SESSION Grafting), module map/inventory updates (jreng_gui, KeyBinding, ModalKeyBinding, Panes), 4 glossary entries, Tabs entry corrected
- `README.md` — Split panes: "planned"/"in progress" -> "implemented", added UI feature bullets, pane keybinds subsection, config example with pane/keys blocks, intro updated

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered (snake_case config keys, semantic method names)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [x] All implicit bool checks converted to explicit null/validity checks
- [x] DRY: 4 extractions (splitImpl, focusLastTerminal, bindModalActions, file-scope arrays)

### Architecture Decisions
- **Split naming convention finalized**: splitHorizontal = left/right layout, splitVertical = top/bottom layout. Internal direction string describes divider orientation.
- **Split commands moved to prefix key**: removed from KeyBinding::CommandID (Cmd+Shift chords), now exclusively ModalKeyBinding actions
- **Resizer bar matching by identity**: `getSplitNode() == node` instead of sequential index, survives tree restructuring

### Problems Solved
1. **Re-parenting assert on close** — sibling must be removeChild'd before appendChild to new parent
2. **State ripped from AppState on close** — parent==state check instead of grandparent.isValid()
3. **SESSION re-parenting assert** — ungraft SESSION before PaneManager::remove() restructures tree
4. **Wrong resizer bar removed on close** — identity matching instead of index-based
5. **Focus jumps to first terminal on close** — now picks adjacent terminal
6. **Split naming confusion** — swapped to match user expectation (horizontal = left/right)
7. **Stale docs** — ARCHITECTURE.md and README.md now reflect implemented split pane system

### Technical Debt / Follow-up
1. **State persistence/restore** — not tested with new tree structure
2. **No visual indicator** for prefix mode active state
3. **Sprint 88 tech debt resolved**: ModalKeyBinding added to CMake (done), build validated (done)

---

## Sprint 88: Binary Tree Split Panes + Prefix Key System

**Date:** 2026-03-12
**Role:** COUNSELOR

### Agents Participated
- COUNSELOR: Architecture decisions, planning, delegation, audit review
- Engineer (x8): PaneResizerBar, PaneManager, Panes, Tabs, AppIdentifier, ModalKeyBinding, LookAndFeel, Config
- Auditor (x1): Reviewed Panes rewrite — caught 3 critical bugs, 5 major style violations
- Pathfinder (x1): Discovered all Panes API consumers before rewrite
- Librarian (x1): Researched juce::StretchableLayoutResizerBar pattern

### Files Modified (18 total)
- `modules/jreng_gui/layout/jreng_pane_resizer_bar.h` — Rewritten: JUCE StretchableLayoutResizerBar pattern (PaneManager pointer + splitNode + isVertical), removed onDrag callback, added getSplitNode() getter
- `modules/jreng_gui/layout/jreng_pane_resizer_bar.cpp` — Rewritten: mouseDown stores position from manager, mouseDrag calls setItemPosition, hasBeenMoved calls parent->resized()
- `modules/jreng_gui/layout/jreng_pane_manager.h` — Rewritten: binary tree ValueTree, static templated layOut stores bounds (ID::x/y/width/height) on PANES nodes, getItemCurrentPosition/setItemPosition for drag, public identifiers
- `modules/jreng_gui/layout/jreng_pane_manager.cpp` — Rewritten: addLeaf, split, remove (mutates in-place, never replaces state), getItemCurrentPosition/setItemPosition (pixel-to-ratio conversion), findLeaf recursive search
- `Source/component/Panes.h` — Rewritten: removed ValueTree::Listener, isVertical, hasSplitDirection, rebuildLayout, findSplitBounds; added closePane, focusPane, findPaneNode
- `Source/component/Panes.cpp` — Rewritten: createTerminal grafts SESSION into PANE node, splitVertical/splitHorizontal create PaneResizerBar with manager+splitNode, closePane removes stale bar via getRoot() check, focusPane spatial nearest-neighbour lookup
- `Source/component/Tabs.h` — Added focusPaneLeft/Down/Up/Right declarations
- `Source/component/Tabs.cpp` — Updated valueTreePropertyChanged (ancestor walk for nested SESSION), closeActiveTab (pane>tab>window close order), added focusPaneLeft/Down/Up/Right forwarding
- `Source/AppIdentifier.h` — Updated schema comment to reflect PANES > PANE > SESSION tree structure
- `Source/component/TerminalComponent.cpp` — Added ModalKeyBinding intercept at top of keyPressed
- `Source/MainComponent.h` — Added ModalKeyBinding include and member
- `Source/MainComponent.cpp` — Wired modalKeyBinding actions (paneLeft/Down/Up/Right), reload re-wires actions
- `Source/component/LookAndFeel.h` — Added paneBarColourId/paneBarHighlightColourId to ColourIds, drawStretchableLayoutResizerBar override
- `Source/component/LookAndFeel.cpp` — Added setColour for pane bar colours, drawStretchableLayoutResizerBar draws centred line with colour/highlight
- `Source/config/Config.h` — Added keys: keysPrefix, keysPrefixTimeout, keysPaneLeft/Down/Up/Right, paneBarColour, paneBarHighlight
- `Source/config/Config.cpp` — Added defaults + schema for all new keys, writeDefaults rewritten to output full documented end.lua with all config keys and comments
- `Source/config/ModalKeyBinding.h` — NEW: prefix-key modal keybinding, Context singleton, Timer timeout, state machine (idle/waiting), Action enum, setAction
- `Source/config/ModalKeyBinding.cpp` — NEW: handleKeyPress state machine, loadFromConfig parses prefix + action keys via KeyBinding::parse, timerCallback returns to idle

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Architecture Decisions
- **PaneManager owns ValueTree as SSOT** for binary tree split structure
- **PANE > SESSION grafting**: SESSION (terminal state) grafted as child of PANE (layout leaf) at graft time. PaneManager ignores SESSION children during layout
- **PaneManager::remove() mutates in-place**: never replaces root state object, preserving listener registrations
- **JUCE StretchableLayoutResizerBar pattern**: PaneResizerBar holds PaneManager pointer + splitNode ValueTree. layOut stores bounds on PANES nodes. getItemCurrentPosition/setItemPosition convert between pixels and ratio
- **ModalKeyBinding as Context singleton**: parallel to KeyBinding, handles two-key prefix sequences with configurable timeout
- **Spatial focus navigation**: Panes::focusPane uses component centre distances, no tree-based slot computation
- **Cmd+W close order**: pane > tab > window, handled in Tabs::closeActiveTab

### Problems Solved
- PaneManager::remove() was replacing `state` member, orphaning ValueTree listeners — fixed to mutate in-place
- findSplitBounds was using manual boolean flag and union with (0,0,0,0) — eliminated entirely when switching to JUCE resizer pattern
- onDrag callback approach pushed ratio computation complexity into Panes — eliminated by adopting JUCE's setItemPosition pattern where PaneManager owns the conversion

### Technical Debt / Follow-up
- ModalKeyBinding.h/.cpp need adding to Projucer/CMake source list
- Not yet built or tested — needs build validation
- PLAN.md needs updating to reflect final architecture (SESSION grafting into PANE, ModalKeyBinding)
- State persistence/restore from XML not yet tested with new tree structure
- No visual indicator for prefix mode active state

---

## Sprint 87: Tab System + Fonts Context Refactor + LookAndFeel Colour System

**Date:** 2026-03-10
**Agents:** COUNSELOR, @pathfinder, @engineer, @oracle

### Objective

Implement tabbed terminal interface, refactor Fonts to global Context for SSOT and crash fix, build LookAndFeel colour system, move MessageOverlay to MainComponent.

### Changes

**Tab System**

- `Source/component/Tabs.h/cpp` — `Terminal::Tabs` subclasses `juce::TabbedComponent`, manages `Owner<GLComponent>` container, visibility toggling, tab bar auto-hide
- `Source/component/LookAndFeel.h/cpp` — Custom `Terminal::LookAndFeel` with `ColourIds` enum (`cursorColourId`, `tabBarBackgroundColourId`, `tabLineColourId`), `setColours()` reads all colours from Config, `drawTabButton()` simple line indicator on opposite edge, `drawTabbedButtonBarBackground()` no-op, popup menu glass blur
- `Source/MainComponent.cpp` — `commandDefs[]` static table + `commandActions` Function::Map for table-driven dispatch, `setDefaultLookAndFeel`, reload wiring with `setColours()` + `sendLookAndFeelChange()` + `applyOrientation()`
- Configurable tab position via `tab.position` config key (top/bottom/left/right, default left)
- `Tabs::applyOrientation()` + `orientationFromString()` for hot-reload
- `Tabs::resized()` handles all 4 orientations for content area trimming
- Close last tab quits app — removed `terminals.size() > 1` guard

**Config Keys Added**

- `tab.family` (Display Mono), `tab.size` (14), `tab.foreground` (#FF00C8D8 blueBikini), `tab.inactive` (#FF2E4D53 mallard), `tab.position` (left), `tab.line` (#FF8CC9D9 dolphin), `menu.opacity` (0.65)

**Fonts Context Refactor**

- `Source/terminal/rendering/Fonts.h` — `Fonts` inherits `jreng::Context<Fonts>`, globally accessible via `Fonts::getContext()`
- `Source/MainComponent.h/cpp` — MainComponent owns `std::unique_ptr<Fonts>`, constructed before Tabs, destroyed after
- `Source/terminal/rendering/Screen.h/cpp` — Removed `Fonts` from `Resources` struct, Screen uses `Fonts::getContext()` everywhere
- `Source/terminal/rendering/ScreenRender.cpp` — All `resources.fonts.` → `Fonts::getContext()->`
- `Source/component/CursorComponent.h` — Removed `Fonts&` constructor parameter, uses `Fonts::getContext()`
- `Source/component/TerminalComponent.cpp` — Screen default-constructed, cursor no longer passes `screen.getFonts()`

**MessageOverlay Moved to MainComponent**

- Removed from `Terminal::Component` (member, constructor, destructor, resized, reloadConfig)
- Added to `MainComponent` (member, constructor, resized, reload command)
- Removed `setGridSize()`, `show()`, `numRows`, `numCols` from MessageOverlay — now only has `showMessage()`
- Removed `onGridSizeChanged` callback chain (was Terminal::Component -> Tabs -> MainComponent)
- `reloadConfig()` removed from Tabs and Terminal::Component — Config::reload() called directly in MainComponent, `Tabs::applyConfig()` iterates all terminals

**Popup Menu Glass Blur**

- `preparePopupMenuWindow` — `setOpaque(false)` + `callAsync` with `BackgroundBlur::apply`
- `drawPopupMenuBackgroundWithOptions` — empty no-op
- `drawPopupMenuItem` — themed with `findColour()`
- Removed `applyToMenu` from BackgroundBlur (.h, .mm, .cpp)

**Other**

- Configurable shell path via Config
- Display Mono font registration in Main.cpp
- Tab font uses `withPointHeight()` for CoreText/FreeType point sizing
- `KeyBinding` for tab commands (new tab, close tab, prev/next tab)

### Files Modified (25+ total)

- `Source/MainComponent.h/cpp` — Fonts ownership, messageOverlay, command table, reload wiring, tab orientation
- `Source/component/Tabs.h/cpp` — NEW: tab container with orientation support
- `Source/component/LookAndFeel.h/cpp` — NEW: colour system, tab styling, popup menu
- `Source/component/TerminalComponent.h/cpp` — removed messageOverlay, removed reloadConfig, removed Fonts dependency
- `Source/component/CursorComponent.h` — removed Fonts& parameter
- `Source/component/MessageOverlay.h` — simplified to showMessage() only
- `Source/terminal/rendering/Fonts.h` — added Context<Fonts> base
- `Source/terminal/rendering/Screen.h/cpp` — removed Fonts from Resources, default constructor
- `Source/terminal/rendering/ScreenRender.cpp` — Fonts::getContext()
- `Source/config/Config.h/cpp` — tab config keys, menu opacity
- `Source/config/KeyBinding.h` — tab command IDs
- `Source/Main.cpp` — Display Mono registration
- `modules/jreng_gui/glass/jreng_background_blur.h/mm/cpp` — removed applyToMenu

### Alignment Check

- [x] LIFESTAR Lean: Fonts shared globally, no per-terminal duplication
- [x] LIFESTAR Explicit: All colours flow Config -> setColours() -> findColour(), no hidden state
- [x] LIFESTAR SSOT: Fonts is single instance, Config keys are sole source for all colours
- [x] LIFESTAR Findable: ColourIds enum, setColours() centralizes all colour wiring
- [x] LIFESTAR Reviewable: Table-driven command dispatch, orientation mapping
- [x] NAMING-CONVENTION: tabLineColourId, tabBarBackgroundColourId, applyOrientation — all semantic
- [x] ARCHITECTURAL-MANIFESTO: Tell don't ask — MainComponent tells Tabs to applyConfig/applyOrientation

### Problems Solved

1. **HarfBuzz crash on tab close** — Font destroyed while GL thread mid-render. Fixed by making Fonts a global Context owned by MainComponent.
2. **Multiple OpenGL contexts** — Single shared GLRenderer, Terminal::Component inherits GLComponent
3. **Popup menu opacity** — backgroundColourId must be non-opaque for JUCE to allow transparency
4. **Tab bar covers GL content** — TabbedComponent::backgroundColourId set to transparentBlack
5. **MessageOverlay on every new tab** — Moved to MainComponent, removed grid size trigger

### Technical Debt / Follow-up

1. **Grid size overlay** — removed grid size display from overlay. Need to recompute from Fonts::getContext()->calcMetrics() at MainComponent level for window resize display.
2. **Zoom applies to single Screen** — setFontSize() calls Fonts::getContext()->setSize() (global) but only recalcs one Screen's metrics. Other Screens need recalc too.
3. **Tab title from cwd** — tabs all show "Terminal", should show current working directory
4. **Tab outline colours** — currently hardcoded transparent, could be configurable
5. **Thread race in renderOpenGL** — GL thread iterates components while message thread can mutate container. Currently mitigated by Fonts outliving terminals, but container mutation is still unprotected.

## Sprint 86: Fork jreng_opengl Module + Replace Snapshot Mailbox with GLSnapshotBuffer

**Date:** 2026-03-08
**Agents:** COUNSELOR, Engineer (x12 subtasks), Pathfinder, Oracle
**Status:** Complete

### Objective

1. Fork KANJUT `kuassa_opengl` module into END as `jreng_opengl` (namespace `kuassa` -> `jreng`)
2. Replace END's hand-rolled `Render::Mailbox` + manual double-buffer rotation with `jreng::GLSnapshotBuffer<Render::Snapshot>`

### Context

KANJUT's `kuassa_opengl` had a cleaner encapsulation of the same lock-free snapshot exchange pattern that END implemented manually. END's `Screen` owned `snapshotA`/`snapshotB`/`writeSnapshot` and manually rotated pointers after `Mailbox::publish()`. KANJUT's `GLSnapshotBuffer<T>` encapsulates the double-buffer, slot rotation, and last-read retention in a generic template. Forking this module also brings `juce::Path` tessellation and a `juce::Graphics`-like OpenGL command buffer API — needed for future WHELMED markdown/mermaid renderer integration.

### Files Created (16 new files)

- `modules/jreng_opengl/jreng_opengl.h` — module header
- `modules/jreng_opengl/jreng_opengl.cpp` — module impl
- `modules/jreng_opengl/jreng_opengl.mm` — ObjC++ impl (Mac renderer)
- `modules/jreng_opengl/context/jreng_gl_mailbox.h` — `GLMailbox<T>` atomic exchange
- `modules/jreng_opengl/context/jreng_gl_snapshot_buffer.h` — `GLSnapshotBuffer<T>` double-buffer + added `isReady()` forwarding method
- `modules/jreng_opengl/context/jreng_gl_graphics.h` / `.cpp` — command buffer API
- `modules/jreng_opengl/context/jreng_gl_component.h` / `.cpp` — GL component base
- `modules/jreng_opengl/context/jreng_gl_renderer.h` / `.cpp` / `_mac.mm` — OpenGL renderer
- `modules/jreng_opengl/context/jreng_gl_overlay.h` — component overlay
- `modules/jreng_opengl/renderers/jreng_gl_path.h` / `.cpp` — Path tessellation
- `modules/jreng_opengl/renderers/jreng_gl_vignette.h` — vignette effect
- `modules/jreng_opengl/shaders/flat_colour.vert` / `.frag` — GLSL shaders

### Files Modified

- `CMakeLists.txt` — added `jreng_opengl` to JUCE_MODULES
- `Source/terminal/rendering/Screen.h` — removed `Render::Mailbox` class, removed `snapshotA`/`snapshotB`/`writeSnapshot` members, added `jreng::GLSnapshotBuffer<Render::Snapshot>` to Resources, updated `Render::OpenGL::setResources()` signature, replaced `getSnapshotMailbox()` with `getSnapshotBuffer()`, added `resize()` no-op to `Render::Snapshot`, updated all doxygen
- `Source/terminal/rendering/ScreenSnapshot.cpp` — `updateSnapshot()` now uses `getWriteBuffer()` + `write()` instead of manual publish + rotation, updated doxygen
- `Source/terminal/rendering/TerminalGLRenderer.cpp` — `renderOpenGL()` now uses `snapshotBuffer->read()` instead of `acquire()` + `currentSnapshot` tracking, updated doxygen
- `Source/terminal/rendering/Screen.cpp` — constructor wires `&resources.snapshotBuffer`, `hasNewSnapshot()` calls `isReady()`, replaced `getSnapshotMailbox()` with `getSnapshotBuffer()`, updated doxygen
- `ARCHITECTURE.md` — updated module map, module inventory, communication contracts, threading model, data flow, synchronization primitives, design patterns, glossary
- `Source/component/TerminalComponent.h` — `dpiCorrectedFontSize()` guarded to Windows-only (Mac font scaling fix)

### Skipped from KANJUT fork

- `component/kuassa_gl_analyzer.h` — audio plugin specific (depends on `ku::Function::Map`)
- `component/kuassa_gl_frequency_grid.h` — audio plugin specific
- `component/kuassa_gl_magnitude_plot.h` — audio plugin specific

### Alignment Check

- **LIFESTAR:** Lean (GLSnapshotBuffer encapsulates what Screen did manually), Explicit (template contract clear), Single Source of Truth (no duplicate buffer management), Testable (generic template), Reviewable (matches KANJUT pattern)
- **NAMING-CONVENTION:** `jreng::GLMailbox`, `jreng::GLSnapshotBuffer` — consistent with KANJUT naming, PascalCase types
- **ARCHITECTURAL-MANIFESTO:** Layer separation preserved — rendering layer owns snapshot exchange, no cross-layer leaks

### Technical Debt / Follow-up

- `GLSnapshotBuffer::resize()` calls `SnapshotType::resize()` on both buffers — END's `Render::Snapshot::resize()` is a no-op since capacity is managed per-frame via `ensureCapacity()`. Works but semantically loose.
- `jreng_gl_renderer.cpp` loads shaders via `BinaryData::getString()` — END's binary data system needs to include the `flat_colour.vert/frag` shaders if the renderer is ever used directly (currently only GLSnapshotBuffer is used by END)
- `GLPath::tessellateFill()` uses centroid triangulation — buggy for concave shapes (TODO in source: ear-clipping)
- Stale doc comment in `Screen.h` `Render::Snapshot` struct still references "Two `Snapshot` instances (`snapshotA`, `snapshotB`) are owned by `Screen`" at line ~237

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

## Sprint 93 — Terminal::Action: Unified Action Registry

**Date:** 2026-03-15
**Agents:** COUNSELOR, @engineer, @researcher, @auditor

### Problem

Keyboard/action handling scattered across 4 objects: `KeyBinding`, `ModalKeyBinding`, `MainComponent` (ApplicationCommandTarget), `TerminalComponent::keyPressed` (inline Ctrl+C/V). Copy/paste didn't work — Config defaults were `shift+ctrl+c/v` (Linux convention) not `ctrl+c/v`.

### What Was Done

**1. Terminal::Action** (`Source/terminal/action/Action.h` + `Action.cpp`)
- Single owner of all user-performable actions
- Fixed action table: 18 actions with ID, name, description, category, callback
- Hot-reloadable key map from `end.lua`
- Prefix state machine absorbed from ModalKeyBinding
- Global singleton via `jreng::Context<Action>`

**2. 18 actions:** copy (selection gate), paste, newline (Shift+Enter → `\n`), quit, close_tab, reload_config, zoom_in/out/reset, new_tab, prev/next_tab, split_horizontal/vertical, pane_left/down/up/right

**3. MainComponent stripped** — removed ApplicationCommandTarget, CommandDef, commandDefs[], commandActions, buildCommandActions, bindModalActions, ApplicationCommandManager, KeyBinding, ModalKeyBinding

**4. KeyBinding + ModalKeyBinding dissolved** — tombstoned

**5. Config defaults fixed** — ctrl+c/v on Windows, smart shell detection (zsh → pwsh → powershell), empty shell args, font 11pt

**6. VkKeyScanW fallback** for punctuation in Win32 Input Mode

**7. Session shutdown crash fix** — null onExit before close()

**8. Newline action** — Shift+Enter sends `\n` via `Session::writeToPty()`

### Files Created
- `Source/terminal/action/Action.h` + `Action.cpp`

### Files Modified
- `MainComponent.h/cpp`, `TerminalComponent.h/cpp`, `Tabs.h/cpp`, `Session.h/cpp`, `Keyboard.cpp`, `Config.h/cpp`, `default_end.lua`

### Files Tombstoned
- `Source/config/KeyBinding.h/cpp`, `Source/config/ModalKeyBinding.h/cpp`

### Alignment Check
- **LIFESTAR:** Lean (one object replaces four), Explicit Encapsulation (Action is dumb, callbacks injected), SSOT (one registry), Findable (`terminal/action/`)
- **NAMING-CONVENTION:** All semantic — `Action`, `Entry`, `handleKeyPress`, `registerAction`, `writeToPty`, `findDefaultWindowsShell`
- **ARCHITECTURAL-MANIFESTO:** Tell don't ask. Action doesn't know about Session, Grid, Tabs.

### Technical Debt
- `close_pane` action blocked on missing `Config::Key::keysClosePane`
- `getTreeMode()` naming violates Rule 2
- `seq 1M` performance gap remains
- Action List UI not built (Phase 3)

---
