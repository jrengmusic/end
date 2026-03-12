# SPRINT-LOG.md

**Project:** end  
**Repository:** /Users/jreng/Documents/Poems/dev/end  
**Started:** 2026-02-02

**Purpose:** Long-term context memory across sessions. Tracks completed work, technical debt, and unresolved issues. Written by PRIMARY agents only when user explicitly requests.

---

## 📖 Notation Reference

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

## Sprint 85: Comprehensive Doxygen Documentation + README + Grid Resize Crash Fix

**Date:** 2026-03-07
**Agents:** COUNSELOR, @pathfinder, @engineer

### Objective

Document the entire END codebase with Doxygen annotations (77 files across 6 phases), create a user-facing README.md, fix a crash in Grid::resize() that only resized the active screen buffer, and suppress the first-load resize overlay.

### Changes

**Grid Resize Crash Fix**

`Source/terminal/logic/GridReflow.cpp` (lines 132-148)
- `Grid::resize()` now resizes BOTH normal and alternate screen buffers on every resize
- Previously only resized the active buffer. When on alternate screen (vim/opencode), the normal buffer kept stale dimensions. On screen switch back + erase, `clearRow()` used `phys * newCols` to index into graphemes allocated with `oldCols` — out-of-bounds crash.

**First-Load Resize Overlay Suppressed**

`Source/component/TerminalComponent.h`
- Added `bool initialLayoutDone { false }` member

`Source/component/TerminalComponent.cpp`
- `resized()` skips showing the grid-size overlay on the very first layout pass
- Sets `initialLayoutDone = true` after first pass

**Comprehensive Doxygen Documentation (77 files)**

All source files documented with `@file`, `@brief`, `@class`/`@struct`, `@param`, `@return`, `@note`, `@see`, thread context annotations (`@note AUDIO THREAD` / `@note UI THREAD` equivalent for terminal threads).

Phase 1 — `Source/terminal/data/` (14 files):
- `Cell.h`, `Charset.h`, `CharProps.h`, `CharPropsData.h`, `Grapheme.h`, `Keyboard.h`, `Palette.h`, `State.h`, `State.cpp`, `StateFlush.cpp`, `ValueTreeUtilities.h`, `GlyphConstraint.h`, `GlyphConstraintTable.cpp` (header comment only — generated file), `Selection.h`

Phase 2 — `Source/terminal/logic/` (15 files):
- `Grid.h`, `GridCursor.cpp`, `GridEdit.cpp`, `GridReflow.cpp`, `GridScroll.cpp`, `Parser.h`, `Parser.cpp`, `ParserCSI.cpp`, `ParserOps.cpp`, `ParserSGR.cpp`, `ParserVT.cpp`, `Session.h`, `Session.cpp`, `SessionIO.cpp`, `SessionResize.cpp`

Phase 3 — `Source/terminal/tty/` (6 files):
- `TTY.h`, `UnixTTY.h`, `UnixTTY.cpp`, `WindowsTTY.h`, `WindowsTTY.cpp`, `ConPTYDefinitions.h`

Phase 4 — `Source/terminal/rendering/` (27 files):
- `Screen.h`, `Screen.cpp`, `ScreenRender.cpp`, `ScreenSnapshot.cpp`, `ScreenSelection.cpp`, `TerminalGLRenderer.h`, `TerminalGLRenderer.cpp`, `GlyphAtlas.h`, `GlyphAtlas.cpp`, `GlyphAtlas.mm`, `AtlasPacker.h`, `BoxDrawing.h`, `Fonts.h`, `Fonts.cpp`, `Fonts.mm`, `FontsMetrics.cpp`, `FontsShaping.cpp`, `FontCollection.h`, `FontCollection.cpp`, `FontCollection.mm`, `Shaders.h`, `Shaders.cpp`, `TextShader.h`, `TextShader.cpp`, `BGShader.h`, `BGShader.cpp`, `CursorShader.h`

Phase 5 — `Source/component/` + `Source/config/` + Main (9 files):
- `TerminalComponent.h`, `TerminalComponent.cpp`, `CursorComponent.h`, `MessageOverlay.h`, `Config.h`, `Config.cpp`, `Main.cpp`, `MainComponent.h`, `MainComponent.cpp`

Phase 6 — `modules/jreng_gui/glass/` (6 files):
- `jreng_background_blur.h`, `jreng_background_blur.mm`, `jreng_glass_window.h`, `jreng_glass_window.cpp`, `jreng_glass_component.h`, `jreng_glass_component.cpp`

**README.md — Created**
- User-friendly style matching CAKE project reference
- Sections: what/why, get started, configuration, keybinds, features, architecture overview, platform support

**SPEC.md — Updated**
- Selection overlay documented (transparent bg, `coloursSelection` config key)
- Shift+scroll implemented status
- State-only keys (`window.width`, `window.height`, `window.zoom`) documented
- `window.buttons` config key added
- Config persistence updated (writeState to state.lua)
- File tree updated (removed table_serializer.lua)
- Phase 2 checklist updated
- GlassWindow constructor signature updated
- Dates updated

**ARCHITECTURE.md — Updated**
- ScreenSelection overlay section added
- Font Resize (Zoom) section added
- Dates updated

### Alignment Check

- [x] LIFESTAR Lean: Doxygen annotations add zero runtime cost. Crash fix is 16 lines.
- [x] LIFESTAR Explicit: Every public API now has documented intent, parameters, and thread context.
- [x] LIFESTAR SSOT: Grid::resize() now maintains both buffers as single source of grid dimensions.
- [x] LIFESTAR Findable: Doxygen `@see` cross-references connect related components. `@file` + `@brief` on every file.
- [x] LIFESTAR Reviewable: Documentation follows consistent format across all 77 files.
- [x] CODING-STANDARD: No code style changes in documentation pass. Crash fix follows all conventions.
- [x] NAMING-CONVENTION: `initialLayoutDone` — semantic boolean name.

### Problems Solved

1. **Grid resize crash** — alternate buffer kept stale dimensions after resize, causing out-of-bounds access on screen switch back
2. **First-load overlay flash** — resize overlay appeared briefly on initial window layout
3. **Zero documentation** — entire codebase now has Doxygen annotations for IDE tooltips and future doc generation
4. **No README** — project now has user-facing documentation

### Technical Debt / Follow-up

1. **Doxygen config file** — No `Doxyfile` exists yet. Would enable HTML/PDF doc generation from the annotations.
2. **GlyphConstraintTable.cpp** — Only has `@file` header comment (generated file). Generator could emit Doxygen comments per switch arm.
3. **jreng_core/ and jreng_graphics/ modules** — Not documented in this pass. Lower priority (stable utility code).

---
