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

## Sprint 84: Zoom Font Scaling — bgCacheCols + Shift Key + Selection + Window State + Full Font Resize

**Date:** 2026-03-07
**Agents:** COUNSELOR, @pathfinder, @engineer

### Objective

Fix multiple rendering and input bugs discovered during zoom implementation and general usage. Add window chrome config, selection overlay, and complete font resize for all font handles during zoom.

### Changes

**bgCacheCols Overflow Fix**

`Source/terminal/rendering/Screen.h`
- Added `int bgCacheCols { 0 }` member — tracks actual bg cache slot count per row (= cols * 2)

`Source/terminal/rendering/Screen.cpp`
- `bgCacheCols` set to `cols * 2` wherever `cachedBg` is allocated
- All bg row offset computations use `bgCacheCols` instead of `cols`

`Source/terminal/rendering/ScreenRender.cpp`
- `bgIdx` calculation uses `bgCacheCols` for row stride

`Source/terminal/rendering/ScreenSnapshot.cpp`
- `memcpy` offset uses `bgCacheCols` for bg row stride

**Shift Key Swallowing Fix**

`Source/component/TerminalComponent.cpp`
- Added `isScrollNav` bool guard — only matches Shift+PageUp/PageDown/Home/End
- Restructured `keyPressed` so Shift+printable characters (`:`, `!`, uppercase) pass through to terminal

**Selection Overlay**

`Source/terminal/rendering/Screen.h`
- Removed `applySelectionInverse` method

`Source/terminal/rendering/Screen.cpp`
- Removed `applySelectionInverse` implementation

`Source/terminal/rendering/ScreenRender.cpp`
- Selection rendering changed from XOR INVERSE to transparent bg overlay
- Uses `coloursSelection` config color (default: blueBikini `#8000C8D8`)
- Only renders on cells with content (`cell.hasContent()`)

`Source/config/Config.h`
- Added `selectionColour` to Theme struct

`Source/config/Config.cpp`
- Added `coloursSelection` default and schema entry

**Title Bar Buttons Config**

`Source/config/Config.h`
- Added `windowButtons` config key (default false)

`Source/config/Config.cpp`
- Added `windowButtons` default and schema entry

`modules/jreng_gui/glass/jreng_background_blur.h`
- Added `hideWindowButtons` declaration

`modules/jreng_gui/glass/jreng_background_blur.mm`
- Added `hideWindowButtons` implementation (hides close/minimize/zoom buttons)

`modules/jreng_gui/glass/jreng_glass_window.h`
- Added `showWindowButtons` parameter and member

`modules/jreng_gui/glass/jreng_glass_window.cpp`
- Accepts `showWindowButtons` param, calls `hideWindowButtons` when false
- Title bar height = 0 when buttons hidden

**Window State Save on Quit**

`Source/Main.cpp`
- Added `saveWindowSize` call in `systemRequestedQuit()`

**Window Size/Zoom Removed from Config Schema**

`Source/config/Config.cpp`
- `window.width`, `window.height`, `window.zoom` removed from `end.lua` config schema
- State-only via `state.lua`, stored in Config context with defaults
- `writeDefaults` no longer writes width/height to `end.lua`

**Zoom Infrastructure**

`Source/terminal/rendering/Screen.h`
- Added `setFontSize()` method

`Source/terminal/rendering/Screen.cpp`
- `setFontSize()` calls `resources.fonts.setSize()` then recalculates metrics/grid

`Source/component/TerminalComponent.h`
- Added `applyZoom()`, `zoomInProgress` flag

`Source/component/TerminalComponent.cpp`
- Cmd+/-/0 wired to `applyZoom()`
- Saved zoom applied at startup
- Message overlay suppressed during zoom via `zoomInProgress` flag

**Full Font Resize (NF + Identity + Fallback Cache)**

`Source/terminal/rendering/Fonts.mm:309-428` (setSize)
- Added resize for `identityFont` + `identityShapingFont` via `CTFontCreateCopyWithAttributes`
- Added resize for `nerdFont` + `nerdShapingFont` via `CTFontCreateCopyWithAttributes`
- Added `fallbackFontCache` cleanup — iterates and `CFRelease` all cached CTFontRefs, then clears map
- Updates FontCollection entries: slot 0 (identityFont) and slot 1 (nerdFont) with new font handles and hb_font pointers

`Source/terminal/rendering/Fonts.cpp:271-318` (setSize)
- Added `FT_Set_Char_Size` on `nfFace`
- Destroys and recreates `nerdShapingFont` via `hb_ft_font_create_referenced`
- Updates FontCollection slot 1 with new `ftFace` and `hbFont`

### Alignment Check

- [x] LIFESTAR Lean: Font resize follows existing pattern (CTFontCreateCopyWithAttributes for each font). No new abstractions.
- [x] LIFESTAR Explicit: All font handles explicitly resized. Fallback cache explicitly cleared. FontCollection entries explicitly updated.
- [x] LIFESTAR SSOT: `bgCacheCols` is single source for bg row stride. Zoom state in `state.lua` only. Selection color in config only.
- [x] LIFESTAR Testable: Each fix is isolated and independently verifiable.
- [x] LIFESTAR Findable: `isScrollNav` guard makes shift-key logic discoverable. `zoomInProgress` flag is explicit.
- [x] CODING-STANDARD: `and`/`or`/`not`, Allman braces, brace init, `.at()`, no early returns, positive checks.
- [x] NAMING-CONVENTION: `bgCacheCols`, `isScrollNav`, `zoomInProgress`, `selectionColour` — all semantic.

### Problems Solved

1. **bgCacheCols overflow** — Cells emitting 2 bg entries (colored bg + block char) could overflow into next row's bg slots
2. **Shift key swallowing** — Shift+printable characters (`:`, `!`, uppercase) were caught by scroll nav handler
3. **Selection rendering** — XOR INVERSE replaced with transparent overlay using configurable color
4. **Window state lost on Cmd+Q** — `saveWindowSize` now called in `systemRequestedQuit()`
5. **NF/identity icons not scaling with zoom** — `Fonts::setSize()` now resizes all font handles, not just mainFont and emojiFont
6. **Fallback font cache stale after zoom** — Cache cleared on every `setSize()` call, resolves Sprint 83 tech debt item #5

### Technical Debt / Follow-up

1. **Paste inversion** — Shell-side SGR 7 from OMP prompt. Same behavior in Kitty/WezTerm. NOT an END bug. Left as-is.
2. **Linux system font fallback** — Still deferred from Sprint 83. No FreeType equivalent of `CTFontCreateForString`.
3. **FreeType path: main face hb_font not recreated on setSize** — `hb_ft_font_create_referenced` caches internal state. Currently relies on FT_Set_Char_Size updating the face in-place. May need explicit recreation if shaping artifacts appear after zoom on Linux.

---

## Sprint 83: System Font Fallback + Half-Line Box Drawing Fix

**Date:** 2026-03-06
**Agents:** COUNSELOR, @pathfinder, @researcher, @engineer

### Objective

Fix two rendering issues discovered when running opencode TUI in END:
1. Arrow `→` (U+2192) renders as box — missing glyph, no system font fallback
2. Half-line characters like `╹` (U+2579) render as `┼` — missing handler in procedural box drawing

### Root Cause Analysis

**Missing glyph (U+2192):**
- Embedded DisplayMono-Book.ttf has 1,426 glyphs — standard Latin/ASCII only, no arrows
- SymbolsNerdFont-Regular.ttf covers NF private-use area (U+E000+) but not standard Unicode arrows
- `shapeHarfBuzz` returns glyph count 1 with glyph index 0 (.notdef) but doesn't detect it
- `result.count == 1` (not 0) so `shapeFallback` is never called
- Even if called, `shapeFallback` only tried `mainFont` (same font that already failed)
- No system font fallback existed

**Half-line box drawing (U+2574–U+257F):**
- `BoxDrawing::rasterize` dispatch had no handler for idx 0x74–0x7F
- These 12 codepoints fell into the `else` fallback: `{light, light, light, light}` → `┼`
- Characters affected: `╴╵╶╷╸╹╺╻╼╽╾╿`

### Changes

**System Font Fallback**

`Source/terminal/rendering/Fonts.h:83,135`
- `ShapeResult` gains `void* fontHandle { nullptr }` — carries the actual font used for shaping
- `std::unordered_map<uint32_t, void*> fallbackFontCache` added to macOS private section

`Source/terminal/rendering/Fonts.mm:58-64` (destructor)
- Iterates `fallbackFontCache`, releases all cached CTFontRefs

`Source/terminal/rendering/Fonts.mm:761-795` (shapeHarfBuzz)
- After `hb_shape`, scans all returned glyph infos for .notdef (glyph index 0)
- If ALL glyphs are .notdef, returns count=0, triggering `shapeFallback`

`Source/terminal/rendering/Fonts.mm:799-936` (shapeFallback)
- Tries `mainFont` first (existing behavior)
- On miss: checks `fallbackFontCache` for cached result
- On cache miss: calls `CTFontCreateForString(mainFont, str, range)` to find system font
- Validates PostScript name is not "LastResort"
- Verifies glyph exists via `glyphForCodepoint`
- Caches result (CTFontRef or nullptr on failure) per codepoint
- Sets `result.fontHandle` when fallback font was used

`Source/terminal/rendering/FontsShaping.cpp:50-68` (Linux shapeHarfBuzz)
- Same .notdef detection — returns count=0 if all glyphs are .notdef

`Source/terminal/rendering/ScreenRender.cpp:378-383,410-413`
- All 3 `shapeText` call sites check `shaped.fontHandle != nullptr`
- Substitutes fallback font handle for `renderHandle` before `emitShapedGlyphsToCache`
- Emoji path excluded from override

**Half-Line Box Drawing**

`Source/terminal/rendering/BoxDrawing.h:59-63`
- Added `else if (idx >= 0x74 and idx <= 0x7F)` branch before the `else` fallback
- Dispatches to `HALF_TABLE.at(idx - 0x74)` → `drawLines`

`Source/terminal/rendering/BoxDrawing.h:648-662`
- Added `static constexpr std::array<Lines, 12> HALF_TABLE` with correct entries for all 12 half-line/mixed-weight characters

### Alignment Check

- [x] LIFESTAR Lean: System fallback uses CoreText's built-in cascade — no manual font scanning
- [x] LIFESTAR Explicit: Fallback cache is per-codepoint, deterministic. LastResort font explicitly rejected.
- [x] LIFESTAR SSOT: Fallback logic lives in `shapeFallback` only. `ShapeResult.fontHandle` propagates font identity through existing pipeline.
- [x] LIFESTAR Testable: .notdef detection is deterministic. HALF_TABLE is constexpr.
- [x] CODING-STANDARD: `and`/`or`/`not`, Allman braces, brace init, `.at()`, no early returns

### Problems Solved

1. **U+2192 `→` renders as box** — System font fallback finds a system font (e.g. Menlo, Helvetica) that has the glyph
2. **U+2579 `╹` renders as `┼`** — HALF_TABLE provides correct `{heavy, none, none, none}` entry
3. **All 12 half-line chars (U+2574–U+257F)** — Now render correctly instead of as `┼`
4. **HarfBuzz .notdef not detected** — `shapeHarfBuzz` now returns count=0 for all-.notdef, triggering fallback

### Technical Debt / Follow-up

1. **Color artifacts** — Background overflow bug in `ScreenRender.cpp:153`: `bgCount[row]` has no bounds check. Block chars + colored backgrounds can overflow `cachedBg` into next row's slots. **Next priority.**
2. **U+254C–U+254F** (double-dash lines) — Still hit the `┼` fallback. Uncommon characters, low priority.
3. **Linux system font fallback** — .notdef detection added but no FreeType equivalent of `CTFontCreateForString`. `shapeFallback` on Linux still only tries the primary font.
4. **Multi-codepoint fallback** — `shapeFallback` sets `result.fontHandle` to the last fallback font used. If different codepoints in a cluster need different fallback fonts, only the last one is propagated. Rare edge case.
5. **Fallback cache invalidation** — Cache is never cleared (except on Fonts destruction). Font size changes don't invalidate. CTFontCreateForString inherits size from mainFont, so size changes would need cache rebuild.

---

## Sprint 82: Full Codebase Audit Sweep — 71-File Clean

**Date:** 2026-03-06
**Agents:** COUNSELOR, @pathfinder, @engineer, @oracle, @researcher

### Objective

Execute a full codebase clean sweep of END based on a 71-file audit report. Fix every finding: critical bugs, SSOT violations, coding standard violations, naming convention violations, dead code removal, and low-priority issues. Mandate: fix all, never assume — stop and discuss any discrepancy between audit and actual code.

### Changes

**Phase 1-2: Critical Bugs (6 audited, 4 real, 2 false positives)**

`Source/terminal/rendering/FontsShaping.cpp:87`
- Added null check on `face` in `shapeFallback()` — `getFace(Style::regular)` could differ from caller's style
- Audit #1 was false positive at caller level (shapeText already null-checks) but user decided to add defensive check anyway

`Source/terminal/rendering/GlyphAtlas.cpp:358,396`
- Added `true` for zero-init to both `HeapBlock<uint8_t> buf(bufSize)` calls — Critical Bug #2

`Source/terminal/rendering/GlyphAtlas.cpp` + `GlyphAtlas.mm`
- Fixed `adaptiveScale` to `std::min(1.0f, uniform)` (never upscale) — Critical Bug #3
- Research confirmed: `adaptiveScale` = NF patcher `pa` without `!` flag = min(min(scaleX,scaleY), 1.0)

`Source/terminal/tty/UnixTTY.cpp:73-74`
- Added `if (flags != -1)` guard around `fcntl(F_SETFL)` — Critical Bug #5

`Source/component/MessageOverlay.h`
- Removed stale `backgroundColour` and `font` members, now reads config fresh in `paint()` — Critical Bug #6

Critical Bug #4 (`||` in `#if JUCE_MAC || JUCE_LINUX`) — false positive, preprocessor syntax is correct

**Phase 3: SSOT Violations**

`Source/component/TerminalComponent.h:31` + `TerminalComponent.cpp`
- Extracted `isMouseTracking()` helper — replaced 4 duplicate 3-condition checks

`Source/config/Config.h:7-8`
- Added `static constexpr float zoomMin { 1.0f }` and `zoomMax { 4.0f }`
- Replaced 4 hardcoded zoom clamp values across Config.cpp and TerminalComponent.cpp

`Source/config/Config.h:109`
- Added `static constexpr const char* configErrorPrefix` — replaced 2 duplicate string literals in Config.cpp

`Source/MainComponent.cpp`
- Removed duplicate `saveWindowSize` call from destructor (close callback already handles it)

`Source/component/TerminalComponent.h` + `.cpp`
- Renamed `gridOverlay` → `messageOverlay` (semantic naming)

Platform-guarded SSOT items (getDisplayScale, ceil26_6ToPx, HarfBuzz loops, texture coords, scale-mode) — skipped as inherent to platform-specific `.mm`/`.cpp` split

**Phase 4: Coding Standard Violations**

`Source/terminal/tty/UnixTTY.cpp` — `waitForData` early return → positive check, added braces
`Source/terminal/logic/Session.cpp` — added braces on callbacks
`Source/terminal/logic/ParserVT.cpp` — added braces
`Source/terminal/logic/ParserCSI.cpp` — `\033` → `\x1b` standardization
`Source/terminal/logic/ParserSGR.cpp` — if/continue chain → else-if ladder
`Source/terminal/logic/Grid.h` — public section reordered
`Source/terminal/logic/GridScroll.cpp` — early returns → if/else
`Source/terminal/rendering/GlyphAtlas.cpp` — 3 early returns → positive checks, added braces
`Source/terminal/rendering/GlyphAtlas.mm` — added braces on alignment block
`Source/terminal/rendering/FontsMetrics.cpp` — added braces
`Source/terminal/rendering/FontsShaping.cpp` — added braces
`Source/terminal/rendering/Screen.cpp` — 2 early returns → single-exit
`Source/terminal/rendering/ScreenRender.cpp` — 3 early returns → single-exit
`Source/terminal/rendering/TerminalGLRenderer.cpp` — comment spacing
`Source/component/TerminalComponent.cpp` — `keyPressed` converted to single-return, all mouse handlers converted from early returns to positive checks

Data layer (CharProps.h, Palette.h, Keyboard.h, Cell.h, Charset.h, ValueTreeUtilities.h, State.cpp, StateFlush.cpp) — all already compliant, no changes needed

**Phase 5: Naming Convention Violations**

`Source/terminal/rendering/Fonts.h` — 14 member renames:
- `ctFont`→`mainFont`, `emojiCtFont`→`emojiFont`, `nfCtFont`→`nerdFont`
- `hbFont`→`shapingFont`, `emojiHbFont`→`emojiShapingFont`, `nfHbFont`→`nerdShapingFont`
- `dmIdentityCtFont`→`identityFont`, `dmIdentityHbFont`→`identityShapingFont`
- `cellW26_6`→`fixedCellWidth`, `cellH26_6`→`fixedCellHeight`, `baseline26_6`→`fixedBaseline`
- `shapeScratch`→`shapingBuffer`, `shapeScratchCapacity`→`shapingBufferCapacity`
- All references updated in Fonts.mm, Fonts.cpp, FontsShaping.cpp, FontsMetrics.cpp

`Source/component/CursorComponent.h` — `colorEmoji`→`colourEmoji`
`Source/terminal/logic/ParserVT.cpp` — `dst`→`cellRow`
`Source/terminal/tty/UnixTTY.h` + `.cpp` — `masterFd`→`master`, `childPid`→`childProcess`, `KILL_TIMEOUT_ITERATIONS`→`killTimeoutIterations`, `KILL_POLL_INTERVAL_US`→`killPollInterval`
`Source/terminal/tty/WindowsTTY.h` + `.cpp` — `hPC`→`pseudoConsole`, `hReadOut`→`outputReader`, `hWriteIn`→`inputWriter`, `hProcess`→`process`, `hReadIn`→`pipeReadEnd`, `hWriteOut`→`pipeWriteEnd`
`Source/Main.cpp` — empty constructor → `= default`
`Source/config/Config.h` — `fontEmbolden` alignment fixed, `minVal`→`minValue`, `maxVal`→`maxValue` in ValueSpec (+ Config.cpp references)
`Source/terminal/logic/Parser.h` + `Parser.cpp` — member `intermediates`→`intermediateBuffer` (resolved shadow with parameter names in csiDispatch/dcsHook declarations)

**Phase 6: Dead Code Removal**

`Source/resources/table_serializer.lua` — DELETED (confirmed not in CMakeLists.txt or .jucer)
`Source/terminal/logic/ParserOps.cpp` — `clearTabStop()`/`clearAllTabStops()` marked `// TODO: unused — verify before removal`
`Source/terminal/logic/Parser.cpp` — `performExitAction` marked `// TODO: implement or remove`

**Phase 7-8: Architectural + Low Issues**

`Source/component/MessageOverlay.h` — magic numbers replaced: `0.8f`→`backgroundAlpha`, `20`(padding)→`textPadding`, `20`(lines)→`maxLines`
`Source/MainComponent.cpp:21` — long line broken, extracted `cfg` local variable

Keyboard.h struct-as-namespace — cancelled (has private members, struct pattern is correct)
ScreenRender.cpp indentation error — cancelled (false positive, no issue found after full file review)
AtlasPacker.h `shelfCapacity = 64` — skipped (comment already adequate)
CharPropsData.h `Regional_Indicator` snake_case — skipped (generated file)

### Alignment Check

- [x] LIFESTAR Lean: No unnecessary abstractions added. Helpers only where 3+ duplicates existed.
- [x] LIFESTAR SSOT: Zoom constants, configErrorPrefix, isMouseTracking — all single source now.
- [x] LIFESTAR Explicit: All defensive checks added with clear intent. No hidden state.
- [x] LIFESTAR Findable: Semantic names throughout (mainFont, shapingBuffer, messageOverlay, etc.)
- [x] LIFESTAR Reviewable: All coding standard violations fixed — Allman braces, `and`/`or`/`not`, no early returns, brace init.
- [x] NAMING-CONVENTION: Rule 2 (no type in name), Rule 3 (semantic over literal), Rule 4 (clarity over brevity), Rule 5 (consistency) — all applied.
- [x] ARCHITECTURAL-MANIFESTO: Platform-guarded duplication preserved (not false SSOT). Layer boundaries respected.

### Problems Solved

1. **Uninitialized glyph atlas buffers** — HeapBlock zero-init added
2. **NF icons upscaling** — adaptiveScale now caps at 1.0
3. **fcntl on invalid fd** — flags checked before F_SETFL
4. **Stale config in MessageOverlay** — reads fresh on every paint
5. **4 duplicate mouse-tracking checks** — extracted to single helper
6. **4 hardcoded zoom bounds** — centralized in Config constants
7. **Variable shadowing in Parser** — member renamed to intermediateBuffer
8. **30+ coding standard violations** — early returns, missing braces, escape sequences, etc.
9. **20+ naming violations** — cryptic abbreviations replaced with semantic names

### False Positives in Audit (documented for future reference)

1. Critical Bug #1 (FontsShaping null check) — caller already guards, but defensive check added per user decision
2. Critical Bug #4 (`||` in `#if`) — preprocessor syntax, not C++ code
3. SSOT #3-7 (platform-guarded duplication) — inherent to `.mm`/`.cpp` split, not real duplication
4. SSOT #9 (ceilDiv) — only 2 occurrences, LIFESTAR Lean says skip
5. SSOT #10 (GL divisor reset) — different attribute counts, not identical code
6. ScreenRender.cpp indentation — no issue found in actual file
7. Data layer files — all already compliant

### Technical Debt / Follow-up

1. **ParserOps.cpp dead code** — `clearTabStop()`/`clearAllTabStops()` marked TODO. May be needed for future DECST/DECTBM support. Verify before removal.
2. **Parser.cpp `performExitAction`** — empty body, marked TODO. May be needed for DCS state machine.
3. **SPEC.md** still references `table_serializer.lua` in file tree (line 849) — update when next editing SPEC.
4. **Linux emoji cursor** — still deferred from Sprint 81.
5. **Build verification needed** — many files modified across all layers. User should build all platforms.

---

## Sprint 81: Phase 4 Complete + Cursor Fixes + Config Hot-Reload

**Date:** 2026-03-06
**Agents:** COUNSELOR, @pathfinder, @engineer

### Objective

Complete Phase 4 (block elements + braille), fix cursor bugs (DECTCEM visibility, emoji cursor rendering, Y-flip), add config hot-reload via keyboard shortcut, refactor Theme ownership into Config.

### Changes

**Phase 4c+4d: Block Elements + Braille**

`Source/terminal/rendering/BoxDrawing.h` (549 lines)
- `isProcedural` — extended to cover U+2580-U+259F (block elements) and U+2800-U+28FF (braille)
- `rasterize` — added dispatch to `drawBlockElement` and `drawBraille`
- `drawShade` — flat alpha memset (64/128/191 for light/medium/dark)
- `drawBlockElement` — full switch for all 32 block codepoints: fractional lower/left blocks via `h*N/8`/`w*N/8`, quadrant combos via multiple `fillRect`
- `drawBraille` — 8-bit pattern decode, 2×4 dot grid centered in cells, two constexpr arrays for col/row mapping

**Cursor DECTCEM Fix**

`Source/component/TerminalComponent.cpp:297-300`
- VBlank cursor visibility now respects DECTCEM mode (`\e[?25l` / `\e[?25h`)
- Was: `cursor->setVisible (not scrolledBack)` — unconditionally visible when not scrolled back
- Now: `cursor->setVisible (not scrolledBack and cursorMode)` where `cursorMode = state.isCursorVisible (activeScreen)`
- Root cause: TUI apps (ASCII animations) send DECTCEM hide, but VBlank overrode it every frame, causing cursor block to appear at random positions during animation

**Cursor Emoji + Fallback Rendering**

`Source/terminal/rendering/Fonts.h:101`
- `rasterizeToImage` gains `bool& isColor` out-param

`Source/terminal/rendering/Fonts.mm:437-660`
- Font fallback chain: text font → emoji font → NF font
- Emoji: creates `CGBitmapContext` with `DeviceRGB` + `kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host`, BGRA pixel copy
- Non-emoji fallback (NF): grayscale alpha as before
- `isColor = true` only for emoji font path
- Removed Y-flip from both emoji and grayscale pixel copy loops (CG bitmap data is top-to-bottom in memory — Sprint 74 discovery)

`Source/terminal/rendering/Fonts.cpp:362`
- Signature updated, `isColor = false` (Linux emoji cursor deferred)

`Source/component/CursorComponent.h:48-65`
- Color emoji: use image directly, skip tinting
- Non-color: tint with cursor colour as before

**Config Hot-Reload**

`Source/component/TerminalComponent.h:31`
- Added `applyConfig()` declaration

`Source/component/TerminalComponent.cpp:96-101`
- `Ctrl+Shift+/` (or `Ctrl+Shift+?`) triggers `Config::reload()` + `applyConfig()`

`Source/component/TerminalComponent.cpp:313-319` — `applyConfig()`
- Re-applies: ligatures, theme. Triggers snapshot rebuild.
- Single API calls: `cfg->getBool()`, `cfg->buildTheme()` — no internal poking

**Theme Refactor (ARCHITECTURAL-MANIFESTO compliance)**

`Source/config/Config.h:7-12`
- Added `Config::Theme` struct (defaultForeground, defaultBackground, ansi[16])
- Added `buildTheme()` declaration

`Source/config/Config.cpp:172-197`
- `buildTheme()` — single place that maps 18 config colour keys to Theme struct

`Source/terminal/rendering/Screen.h:13,59`
- Added `#include "../../config/Config.h"`
- Replaced `Terminal::Theme` struct with `using Theme = Config::Theme;`

`Source/component/TerminalComponent.cpp:15,313-319`
- Constructor and `applyConfig()` both use `cfg->buildTheme()` — one call, no poking internals

### Alignment Check

- [x] LIFESTAR: Explicit Encapsulation — Config owns Theme, higher hierarchy sets instructions not pokes internals. "Objects manage their own state" — Config builds its own Theme.
- [x] LIFESTAR: SSOT — Theme definition lives in Config (single source), Screen uses alias
- [x] LIFESTAR: Lean — `applyConfig()` is 4 lines. `buildTheme()` centralizes the 18-key mapping.
- [x] LIFESTAR: Accessible — `font.ligatures` configurable and hot-reloadable via `Ctrl+Shift+/`
- [x] CODING-STANDARD: `and`/`or`/`not`, Allman braces, brace init, `noexcept`, no early returns

### Problems Solved

1. **Random cursor blocks during TUI animations** — DECTCEM mode not respected in VBlank
2. **Emoji cursor not rendering** — `rasterizeToImage` only tried text font, no fallback chain
3. **Cursor Y-flip** — both grayscale and emoji paths had unnecessary Y-flip (CG data already top-down)
4. **Theme poking violation** — TerminalComponent manually building Theme from 18 keys violated ARCHITECTURAL-MANIFESTO
5. **No config hot-reload** — had to restart app to apply config changes

### Technical Debt / Follow-up

1. **Hot-reload scope** — currently reloads: ligatures, theme colours. Does NOT hot-reload: font family, font size, cursor char, cursor blink, window opacity/colour. These require object recreation.
2. **Linux emoji cursor** — `isColor` always false on Linux path. Needs FreeType color emoji support.
3. **Phase 5 cancelled** — `build_monolithic.py` doesn't exist, Display Mono already has no NF glyphs.
4. **Dashed/dotted lines** (U+2504-U+250B) — currently render as solid lines. Could add dash pattern.
5. **Diagonal lines** (U+2571-U+2573) — not in current procedural range. Would need separate handler.

---
