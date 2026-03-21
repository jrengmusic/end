# PLAN: Text Selection Mode Redesign

**Date:** 2026-03-21
**Status:** IN PROGRESS — Steps 1-5 implemented, Step 2-3 need rework for ModalType

---

## Objective

Replace the current rectangle-only mouse drag selection with a vim-like modal selection system supporting visual (streaming), visual-line, and visual-block modes, with keyboard navigation, search, and configurable keybindings.

---

## Architecture Overview

### Modal State (SSOT)

`Terminal::State` owns a `ModalType` atomic — the single source of truth for whether the terminal is in a modal state. When modal, ALL keys are intercepted before the action system and PTY.

```cpp
enum class ModalType : uint8_t
{
    none,           // normal terminal operation
    selection,      // text selection mode (this feature)
    flashJump,      // file opener hint labels (SPEC Phase 3)
    uriAction       // actionable URIs (SPEC Phase 3)
};
```

This is a general-purpose modal gate. Selection mode is the first consumer. Flash-jump and URI actions reuse the same plumbing when implemented.

### Key Dispatch

```
keyPressed()
    |
    +-- State::isModal()?                 (NEW — FIRST check, before everything)
    |       |
    |       +-- handleModalKey()          (NEW — dispatches by ModalType)
    |               |
    |               +-- selection → handleSelectionKey()
    |               +-- flashJump → handleFlashJumpKey()   (future)
    |               +-- uriAction → handleUriActionKey()   (future)
    |
    +-- Action::handleKeyPress()          (existing — prefix + global bindings)
    |       |
    |       +-- "enter_selection"         (modal action, prefix + [)
    |       +-- "enter_flashjump"         (future)
    |
    +-- handleScrollNavigation()          (existing)
    +-- session.handleKeyPress()          (existing — PTY forward)
```

When `State::isModal()` returns true, ALL keys go to the modal handler. Nothing reaches the action system or PTY. This solves Ctrl+V conflicts — when in selection mode, Ctrl+V is visual-block, not paste.

### All Keys Are Configurable

Every keystroke in every modal state is:
1. Stored as a config key in `Config.h` (`keys.selection_up`, etc.)
2. Has a default value in `Config.cpp` `initDefaults()`
3. Has a schema entry in `Config.cpp` `initSchema()`
4. Has a Lua template entry in `default_end.lua`
5. Parsed into `juce::KeyPress` at startup and on config reload
6. Cached in a per-modal-type key struct (e.g., `SelectionKeys`)
7. Compared in the modal key handler — zero hardcoded characters

### New Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `ModalType` enum | `State.h` | Modal state type, stored as atomic in State |
| `SelectionMode` | `Source/terminal/selection/SelectionMode.h/cpp` | Selection cursor, anchor, type state machine |
| `SelectionOverlay` | `Source/terminal/selection/SelectionOverlay.h` | Status bar ("-- VISUAL --", etc.) |
| `SelectionFinder` | `Source/terminal/selection/SelectionFinder.h/cpp` | `/` search GlassWindow with TextEditor |

### Modified Components

| Component | Change |
|-----------|--------|
| `State` | `ModalType` atomic, `setModalType()`, `getModalType()`, `isModal()` |
| `TerminalComponent` | Modal intercept in `keyPressed()`, `handleModalKey()`, `SelectionKeys` cache |
| `ScreenSelection` | `SelectionType` enum, `containsCell()` dispatches by type |
| `Screen` / `ScreenRender` | Selection cursor rendering, type-aware hit test |
| `Config` | All selection key strings + defaults + schema |
| `default_end.lua` | All selection key entries in Lua template |
| `Theme` | Selection cursor color, status bar color |

---

## Config Keys (end.lua)

All keys are in the `keys` table. All are user-overridable.

```lua
keys = {
    -- Enter text selection mode. Prefix-mode key.
    enter_selection = "[",

    -- Selection mode keys (only active when in selection mode)
    selection_up = "k",
    selection_down = "j",
    selection_left = "h",
    selection_right = "l",
    selection_visual = "v",
    selection_visual_line = "shift+v",
    selection_visual_block = "ctrl+v",
    selection_copy = "y",
    selection_search = "/",
    selection_next_match = "n",
    selection_prev_match = "shift+n",
    selection_top = "g",              -- gg double-press
    selection_bottom = "shift+g",
    selection_line_start = "0",
    selection_line_end = "$",
    selection_exit = "escape",
}
```

`enter_selection` is a modal action (registered in Action system, requires prefix key). All other selection keys are NOT in the Action system — they are config-driven keys parsed into `SelectionKeys` and checked only by `handleSelectionKey()` when `ModalType == selection`.

---

## Implementation Steps

### Step 1: SelectionMode state machine -- DONE

**Files:** `Source/terminal/selection/SelectionMode.h`, `SelectionMode.cpp`

Pure value-type state machine. Tracks: active flag, SelectionType (none/visual/visualLine/visualBlock), cursor row/col, anchor row/col, pendingG state. No dependencies on Grid, State, Screen, or JUCE.

---

### Step 2: ModalType in State + keyPressed intercept -- REWORK NEEDED

**Files:** `State.h`, `State.cpp`, `TerminalComponent.cpp`

#### A. State — ModalType atomic

```cpp
// State.h — public
enum class ModalType : uint8_t { none, selection, flashJump, uriAction };

void setModalType (ModalType type) noexcept;
ModalType getModalType() const noexcept;
bool isModal() const noexcept;  // type != none

// State.h — private
std::atomic<uint8_t> modalType { 0 };
```

`setModalType` calls `setSnapshotDirty()` to trigger re-render (cursor changes).

#### B. keyPressed — modal intercept FIRST

```cpp
bool Terminal::Component::keyPressed (const juce::KeyPress& key)
{
    if (session.getState().isModal())
    {
        if (handleModalKey (key))
            return true;
    }

    // ... existing: onProcessExited, Action dispatch, scroll nav, PTY forward
}
```

`handleModalKey` dispatches by `ModalType`:
```cpp
bool Terminal::Component::handleModalKey (const juce::KeyPress& key) noexcept
{
    const auto type { session.getState().getModalType() };

    if (type == ModalType::selection)
        return handleSelectionKey (key);

    // future: flashJump, uriAction

    return false;
}
```

#### C. enterSelectionMode / exitSelectionMode

```cpp
void enterSelectionMode()
{
    selectionMode.enter (row, col);
    session.getState().setModalType (ModalType::selection);
}
```

In `handleSelectionKey()` on Escape:
```cpp
selectionMode.exit();
session.getState().setModalType (ModalType::none);
```

**Validate:** Modal intercept fires before Action system. Ctrl+V in selection mode triggers visual-block, not paste. Escape exits cleanly.

---

### Step 3: Config keys + SelectionKeys cache -- DONE (needs ModalType wiring)

**Files:** `Config.h`, `Config.cpp`, `default_end.lua`, `TerminalComponent.h/cpp`

All 17 selection keys stored in Config with defaults. `SelectionKeys` struct caches parsed `juce::KeyPress` objects. `buildSelectionKeyMap()` called from constructor and `applyConfig()`. `handleSelectionKey()` compares against cached keys — zero hardcoded characters.

---

### Step 4: Selection cursor rendering -- DONE

**Files:** `Screen.h`, `Screen.cpp`, `ScreenSnapshot.cpp`, `Config.h`, `Config.cpp`, `default_end.lua`

When selection mode active: terminal cursor suppressed, selection cursor drawn as filled block in configurable `colours.selection_cursor` (default bright cyan). `Screen::setSelectionCursor(active, row, col)` called from `onVBlank()`.

---

### Step 5: Selection highlighting -- DONE

**Files:** `ScreenSelection.h`, `ScreenRender.cpp`, `TerminalComponent.h/cpp`

`ScreenSelection` has `SelectionType` enum (linear/line/box). `containsCell()` dispatches to `contains()`, `containsLine()`, or `containsBox()`. `updateSelectionHighlight()` syncs SelectionMode state to ScreenSelection on every key/cursor change.

---

### Step 6: Copy selected text

**File:** `TerminalComponent.cpp`

When `selection_copy` key pressed in selection mode:
1. Determine selection type from `ScreenSelection::type`
2. `linear` → `Grid::extractText(start, end)` (existing, currently unwired)
3. `line` → `Grid::extractText()` with full-row start/end columns
4. `box` → `Grid::extractBoxText()` (existing)
5. Copy to `juce::SystemClipboard`
6. Exit selection mode (`setModalType(none)`)

**Validate:** Select text in all three modes, press configured copy key, verify clipboard.

---

### Step 7: Mouse integration

**File:** `TerminalComponent.cpp`

- `mouseDown`: record anchor position
- `mouseDrag`: enter selection mode (`setModalType(selection)`), type `linear`, anchor at mouseDown, cursor tracks mouse
- `mouseDoubleClick`: enter selection mode, select word at click position
- `mouseTripleClick`: enter selection mode, type `line`, select line at click position

**Validate:** Click-drag = streaming select. Double-click = word. Triple-click = line.

---

### Step 8: Selection mode status overlay

**File:** `Source/terminal/selection/SelectionOverlay.h`

Lightweight `juce::Component` child (follow `MessageOverlay` pattern):
- Shows `-- VISUAL --`, `-- VISUAL LINE --`, `-- VISUAL BLOCK --`
- Configurable position, colors (`colours.selection_status_bg`, `colours.selection_status_fg`)
- `setInterceptsMouseClicks(false, false)`

**Validate:** Enter each mode, verify correct label. Exit, label disappears.

---

### Step 9: Search (`/`)

**Files:** `Source/terminal/selection/SelectionFinder.h`, `SelectionFinder.cpp`

`SelectionFinder` is a `GlassWindow` with `juce::TextEditor` — duplicates `ActionList` glass pattern (different UX: text input + next/prev buttons, no dropdown).

1. `selection_search` key pressed → show `SelectionFinder` as modal glass overlay
2. User types query
3. Enter → find next match in grid, move cursor, dismiss finder
4. Escape → dismiss finder, stay in selection mode
5. `selection_next_match` / `selection_prev_match` for next/prev after dismiss
6. Next/prev buttons for mouse users

Grid search: scan `Grid::activeVisibleRow()` + scrollback rows for substring match. Highlight matches with `colours.search_match`.

**Validate:** `/` opens finder, type query, cursor jumps to match. `n`/`N` cycle.

---

### Step 10: gg/G navigation

Pending-g state inside `handleSelectionKey()`:
- First `selection_top` key: set `pendingG = true`, start timer
- Second press within timeout: `moveToTop()`, clear pending
- Timeout or different key: clear pending
- `selection_bottom`: `moveToBottom()` immediately

**Validate:** `gg` to top of scrollback. `G` to bottom.

---

## Execution Order

| Step | Status | Dependency | Size |
|------|--------|-----------|------|
| 1 | DONE | None | Small |
| 2 | REWORK | Step 1 | Small — add ModalType to State, move intercept before Action |
| 3 | DONE | Step 2 | Small — config keys + SelectionKeys cache |
| 4 | DONE | Step 1 | Medium — cursor rendering |
| 5 | DONE | Step 4 | Medium — selection type highlighting |
| 6 | TODO | Step 5 | Small — clipboard wiring |
| 7 | TODO | Step 5 | Medium — mouse rework |
| 8 | TODO | Step 2 | Small — overlay |
| 9 | TODO | Step 1 | Large — search from scratch |
| 10 | DONE | Step 1 | Small — pendingG in SelectionMode |

**Critical path:** 2 (rework) → 6 → 7 (minimum viable)
**Parallel after step 5:** 8, 9

---

## Discrepancy Checkpoints

Before proceeding at each step, verify:
- [ ] Does `State::isModal()` intercept BEFORE `Action::handleKeyPress()`?
- [ ] Are ALL selection keys read from Config, not hardcoded?
- [ ] Does `parseShortcut()` handle single characters ("k", "/", "$")?
- [ ] Does Ctrl+V in selection mode bypass paste action?
- [ ] Does mouse drag set `ModalType::selection` in State?
- [ ] Does the ScreenSelection rendering path match the snapshot pipeline?
- [ ] Does Grid text extraction handle scrollback coordinates correctly?

**If ANY discrepancy is found: STOP and discuss with ARCHITECT.**

---

## Future Modal Types (same plumbing)

| ModalType | Trigger Action | Keys | Description |
|-----------|---------------|------|-------------|
| `selection` | `enter_selection` (prefix+[) | `keys.selection_*` | This feature |
| `flashJump` | `enter_flashjump` (prefix+f) | `keys.flashjump_*` | File opener hint labels |
| `uriAction` | `enter_uri_action` | `keys.uri_*` | Clickable URIs in terminal output |

Each modal type:
1. Registers its enter action in the Action system (modal, prefix-based)
2. Has its own config keys in `keys.*` section of `end.lua`
3. Has its own `*Keys` cache struct in TerminalComponent
4. Has its own `handle*Key()` method dispatched by `handleModalKey()`
5. Sets `ModalType` in State on enter, resets to `none` on exit

---

## Contracts

- LIFESTAR: Lean (no over-engineering), SSOT (`ModalType` in State, one `SelectionKeys` cache, one `ScreenSelection` type enum), Explicit Encapsulation (SelectionMode is dumb — knows nothing about State/Grid/Screen; State owns the modal flag; TerminalComponent owns the dispatch)
- NAMING-CONVENTION: ModalType/SelectionMode (PascalCase), selectionMode/selectionKeys (camelCase), isModal/enter/exit (verbs), SelectionKeys (noun struct)
- JRENG-CODING-STANDARD: brace init, `not`/`and`/`or`, no early returns, `.at()` for containers, braces on new lines
- CAROL: incremental steps, validate each, stop on discrepancy, all keys user-configurable via end.lua
