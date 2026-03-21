# PLAN: Text Selection Mode Redesign

**Date:** 2026-03-21
**Status:** DRAFT — awaiting ARCHITECT approval before implementation

---

## Objective

Replace the current rectangle-only mouse drag selection with a vim-like modal selection system supporting visual (streaming), visual-line, and visual-block modes, with keyboard navigation, search, and configurable keybindings.

---

## Architecture Overview

```
keyPressed()
    |
    +-- Action::handleKeyPress()          (existing — prefix + global bindings)
    |       |
    |       +-- "enter_selection_mode"    (NEW — modal action, prefix + [)
    |
    +-- selectionMode.isActive()?         (NEW — intercept before PTY)
    |       |
    |       +-- SelectionMode::handleKey() (NEW — hjkl, v, V, ctrl+v, y, /, esc)
    |
    +-- handleScrollNavigation()          (existing)
    +-- session.handleKeyPress()          (existing — PTY forward)
```

### New Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `SelectionMode` | `Source/terminal/selection/SelectionMode.h/cpp` | Modal state machine, cursor, key handling |
| `SelectionOverlay` | `Source/terminal/selection/SelectionOverlay.h` | Status bar ("-- VISUAL --", "-- V-BLOCK --") |
| `SearchBar` | `Source/terminal/selection/SearchBar.h/cpp` | `/` search input overlay |

### Modified Components

| Component | Change |
|-----------|--------|
| `TerminalComponent` | Mode check in `keyPressed()`, mouse handlers enter selection mode |
| `ScreenSelection` | Add `SelectionType` enum, `contains()` dispatches by type |
| `Screen` / `ScreenRender` | Selection cursor rendering, type-aware hit test |
| `Action` / `Config` | New action registrations and config keys |
| `Theme` | Selection mode cursor color, status bar color |

---

## Implementation Steps

### Step 1: SelectionMode state machine

**Files:** `Source/terminal/selection/SelectionMode.h`, `SelectionMode.cpp`

Create the modal state machine:

```cpp
enum class SelectionType { none, visual, visualLine, visualBlock };

class SelectionMode
{
public:
    bool isActive() const noexcept;
    void enter (int cursorRow, int cursorCol, int scrollOffset);
    void exit() noexcept;

    SelectionType getType() const noexcept;
    void setType (SelectionType type) noexcept;

    // Cursor (grid coordinates, scrollback-aware)
    int getCursorRow() const noexcept;
    int getCursorCol() const noexcept;

    // Navigation
    void moveUp (int count = 1) noexcept;
    void moveDown (int count, int maxRow) noexcept;
    void moveLeft (int count = 1) noexcept;
    void moveRight (int count, int maxCol) noexcept;
    void moveToTop() noexcept;
    void moveToBottom (int maxRow) noexcept;

    // Anchor (selection start point, set when entering visual/line/block)
    int getAnchorRow() const noexcept;
    int getAnchorCol() const noexcept;
};
```

**Validate:** Unit-testable state machine. No dependencies on Grid, State, or Screen.

---

### Step 2: Wire SelectionMode into keyPressed()

**File:** `TerminalComponent.cpp`

Insert mode check between Action dispatch and scroll nav:

```cpp
// In keyPressed():
if (Action::getContext()->handleKeyPress (key))
    return true;

if (selectionMode.isActive())
{
    if (handleSelectionKey (key))
        return true;
}

// ... existing scroll nav + PTY forward
```

Add `handleSelectionKey(const juce::KeyPress&)` — dispatches hjkl/v/V/ctrl+v/y/esc to SelectionMode methods. Returns true if consumed.

**Validate:** Enter selection mode via action, press Escape, verify exit. No selection rendering yet — just state transitions.

---

### Step 3: Register actions and config keys

**Files:** `MainComponent.cpp` (registration), `Config.h` (key strings)

New config keys:
```
keys.enter_selection     = "prefix+["     (modal, default)
keys.selection_up        = "k"
keys.selection_down      = "j"
keys.selection_left      = "h"
keys.selection_right     = "l"
keys.selection_visual    = "v"
keys.selection_vline     = "shift+v"
keys.selection_vblock    = "ctrl+v"
keys.selection_copy      = "y"
keys.selection_search    = "/"
keys.selection_top       = "g g"          (defer — two-key sequence)
keys.selection_bottom    = "shift+g"
keys.selection_exit      = "escape"
```

Register actions in `MainComponent::registerActions()` following established pattern:
```cpp
action.registerAction ("enter_selection", "Enter Selection Mode", "...", "Selection", true, [this] { ... });
```

`enter_selection` is modal (`isModal=true`) — requires prefix key first. All selection-internal keys (hjkl, v, etc.) are NOT registered as actions — they are handled directly by `handleSelectionKey()` when selection mode is active. This avoids polluting the action system with mode-specific keys.

**Exception:** `y` and `Cmd+C`/`Ctrl+C` for copy are registered as global actions with a mode check — they should work both in and out of selection mode.

**Validate:** Prefix + `[` enters selection mode. Escape exits. Config override works.

---

### Step 4: Selection cursor rendering

**Files:** `Screen.h`, `ScreenRender.cpp`, `ScreenSnapshot.cpp`, `Theme` (Config.h)

Add to `Render::Snapshot`:
- `selectionCursorRow`, `selectionCursorCol` — grid position of the selection cursor
- `selectionCursorVisible` — true when selection mode is active
- `selectionCursorColour` — from Theme

Add to `Theme`:
- `selectionCursorColour` (default: bright cyan or similar)

In `buildSnapshot()`: if selection mode active, populate the selection cursor fields from `SelectionMode::getCursorRow/Col()`.

In `drawCursor()`: if `selectionCursorVisible`, draw a block cursor at the selection cursor position (suppress terminal cursor while in selection mode).

**Validate:** Enter selection mode, see cursor at current position. Move with hjkl, cursor follows.

---

### Step 5: Selection highlighting (visual mode)

**Files:** `ScreenSelection.h`, `ScreenRender.cpp`

Add `SelectionType` to `ScreenSelection`:
```cpp
enum class SelectionType { box, linear, line };
SelectionType type { SelectionType::box };
```

Update `processCellForSnapshot()` to dispatch by type:
- `box` → `containsBox()` (existing)
- `linear` → `contains()` (existing, unused until now)
- `line` → new `containsLine()` — full row if row is in range

When `v` is pressed: set `ScreenSelection` anchor to SelectionMode cursor, type to `linear`. On every cursor move, update `ScreenSelection::end`. Screen re-renders with streaming selection highlight.

When `V` is pressed: type = `line`, full rows highlighted.

When `Ctrl+V` is pressed: type = `box`, rectangle selection (existing behavior).

**Validate:** Enter selection mode, press `v`, move cursor, verify streaming highlight. Press `V`, verify line highlight. Press `Ctrl+V`, verify rectangle highlight.

---

### Step 6: Copy selected text

**File:** `TerminalComponent.cpp`

When `y` or `Cmd+C`/`Ctrl+C` is pressed in selection mode:
1. Determine selection type from `ScreenSelection::type`
2. For `linear`: call `Grid::extractText(start, end)` (existing, currently unwired)
3. For `line`: call `Grid::extractText()` with full-row start/end columns
4. For `box`: call `Grid::extractBoxText()` (existing)
5. Copy to `juce::SystemClipboard`
6. Exit selection mode
7. Flash selection briefly (optional — defer)

**Validate:** Select text in all three modes, press `y`, verify clipboard contents match.

---

### Step 7: Mouse integration

**File:** `TerminalComponent.cpp`

Modify mouse handlers:
- `mouseDown`: if not in selection mode, record anchor position
- `mouseDrag`: if dragging past threshold, enter selection mode with type `linear`, anchor at mouseDown position, cursor tracks mouse
- `mouseDoubleClick`: enter selection mode, select word at click position
- `mouseTripleClick`: enter selection mode with type `line`, select line at click position

Follow existing mouse → grid coordinate conversion pattern.

**Validate:** Click-drag selects text (streaming, not rectangle). Double-click selects word. Triple-click selects line.

---

### Step 8: Selection mode status overlay

**File:** `Source/terminal/selection/SelectionOverlay.h`

Lightweight `juce::Component` child of `TerminalComponent` (follow `MessageOverlay` pattern):
- Shows `-- VISUAL --`, `-- VISUAL LINE --`, `-- VISUAL BLOCK --`
- Positioned at bottom of terminal (configurable)
- Themed colors (configurable via `colours.selection_status_bg`, `colours.selection_status_fg`)
- `setInterceptsMouseClicks(false, false)` — non-interactive overlay

**Validate:** Enter each mode, verify correct label displayed. Exit, label disappears.

---

### Step 9: Search (`/`)

**Files:** `Source/terminal/selection/SearchBar.h`, `SearchBar.cpp`

When `/` is pressed in selection mode:
1. Show search input overlay at bottom (above status bar)
2. User types search query
3. On Enter: find next match in grid (scan rows), move selection cursor to match
4. On Escape: close search bar, stay in selection mode
5. `n` / `N` for next/previous match (after search bar closes)

Grid search: iterate `Grid::activeVisibleRow()` and scrollback rows, scan cell codepoints for substring match. Highlight all matches with a distinct color.

**Validate:** Enter selection mode, press `/`, type query, verify cursor jumps to match. Press `n` for next.

---

### Step 10: gg/G navigation

`gg` requires a two-key sequence (press `g` once, wait, press `g` again). Options:
- **Simple:** treat single `g` as a pending state with short timeout (like prefix key)
- **Simpler:** use `Home`/`End` as aliases for gg/G

Implement the pending-`g` state inside `handleSelectionKey()`:
- First `g`: set `pendingG = true`, start timer
- Second `g` within timeout: `moveToTop()`, clear pending
- Timeout: clear pending
- `G` (shift+g): `moveToBottom()` immediately

**Validate:** `gg` scrolls to top of scrollback. `G` scrolls to bottom.

---

## Execution Order

| Step | Dependency | Estimated Size |
|------|-----------|----------------|
| 1 | None | Small — pure state machine |
| 2 | Step 1 | Small — keyPressed intercept |
| 3 | Step 2 | Small — action registration |
| 4 | Step 1 | Medium — rendering pipeline |
| 5 | Step 4 | Medium — selection type dispatch |
| 6 | Step 5 | Small — clipboard wiring |
| 7 | Step 5 | Medium — mouse handler rework |
| 8 | Step 2 | Small — overlay component |
| 9 | Step 1 | Large — search from scratch |
| 10 | Step 1 | Small — two-key state |

**Critical path:** 1 → 2 → 3 → 4 → 5 → 6 (minimum viable selection mode)
**Parallel:** 7, 8, 9, 10 can be done independently after step 5

---

## Discrepancy Checkpoints

Before proceeding at each step, verify:
- [ ] Does the established keyPressed routing pattern match my understanding?
- [ ] Does the action registration pattern match Config.h key naming?
- [ ] Does the ScreenSelection rendering path match the snapshot pipeline?
- [ ] Does Grid text extraction handle scrollback coordinates correctly?
- [ ] Does the mouse coordinate conversion match existing mouseDown/mouseDrag?

**If ANY discrepancy is found: STOP and discuss with ARCHITECT.**

---

## Contracts

- LIFESTAR: Lean (no over-engineering), SSOT (one SelectionMode, one ScreenSelection type enum), Explicit Encapsulation (SelectionMode is a dumb state machine, knows nothing about Grid/Screen)
- NAMING-CONVENTION: SelectionMode (PascalCase class), selectionMode (camelCase member), isActive/enter/exit (verb functions)
- JRENG-CODING-STANDARD: brace init, `not`/`and`/`or`, no early returns, `.at()` for containers, braces on new lines
- CAROL: incremental steps, validate each, stop on discrepancy
