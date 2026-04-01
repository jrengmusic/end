# SPEC-MODAL.md — App-Level Modal Handling, StatusBarOverlay, Whelmed Selection

**Project:** END
**Date:** 2026-04-01
**Author:** COUNSELOR
**Status:** Draft — awaiting ARCHITECT approval

**Contracts:** BLESSED, NAMES.md, MANIFESTO.md

---

## Overview

Move modal state from Terminal::State to AppState (SSOT). Modal handling becomes app-level, contextually dispatched to the active pane. StatusBarOverlay becomes an AppState listener. Whelmed gains selection infrastructure reusing the same modal/selection types.

---

## Goals

1. **SSOT**: `ModalType` and `SelectionType` owned exclusively by AppState
2. **Contextual dispatch**: active pane type determines which handler receives modal keys
3. **StatusBarOverlay**: event-driven from AppState, pane-agnostic
4. **Whelmed selection**: keyboard selection mode using shared ModalType/SelectionType, pane-local coordinates
5. **No regression**: Terminal keyboard handling unchanged in behavior

---

## Architecture

### State Ownership

```
BEFORE:
  Terminal::State (atomic)  -->  ModalType, SelectionType
  StatusBarOverlay          -->  polls Terminal::State via VBlank
  Whelmed                   -->  no modal state

AFTER:
  AppState (ValueTree)      -->  ModalType, SelectionType (SSOT)
  StatusBarOverlay          -->  listens to AppState ValueTree
  Terminal::InputHandler    -->  reads/writes AppState
  Whelmed::InputHandler     -->  reads/writes AppState
```

### AppState Changes

New properties on the `TABS` subtree (modal state is per-window, not per-tab):

| Property | Type | Values |
|----------|------|--------|
| `modalType` | int | 0=none, 1=selection, 2=openFile, 3=uriAction |
| `selectionType` | int | 0=none, 1=visual, 2=visualLine, 3=visualBlock |

New identifiers in `AppIdentifier.h`:
```cpp
static const juce::Identifier modalType     { "modalType" };
static const juce::Identifier selectionType { "selectionType" };
```

New methods on `AppState`:
```cpp
void setModalType (int type);
int  getModalType() const noexcept;

void setSelectionType (int type);
int  getSelectionType() const noexcept;
```

These are message-thread ValueTree property writes. No atomics needed — modal transitions are user-initiated (keyboard), always on message thread.

### ModalType Enum

`ModalType` moves from `Terminal::State.h` to a shared location.

New file: `Source/ModalType.h`
```cpp
enum class ModalType : uint8_t
{
    none,       // Normal input — keys flow to Action system and PTY/Whelmed navigation
    selection,  // Vim-style keyboard selection mode
    openFile,   // Open file / hyperlink hint label mode (Terminal only)
    uriAction   // URI picker overlay (reserved)
};
```

`SelectionType` moves from `Source/terminal/selection/SelectionType.h` to `Source/SelectionType.h`.

Both enums are app-level, not Terminal-specific.

### Terminal::State Cleanup

Remove from `Terminal::State`:
- `modalType` atomic slot from parameterMap
- `setModalType()`, `getModalType()`, `isInModalMode()`
- `selectionType` atomic slot
- `setSelectionType()`, `getSelectionType()`
- ValueTree flush logic for these two properties

`Terminal::Component` delegates: `getModalType()` and `getSelectionType()` now read from `AppState::getContext()`.

### Terminal::InputHandler Rewire

Currently reads/writes `state.getModalType()` / `state.setModalType()`.

Change to read/write `AppState::getContext()->getModalType()` / `AppState::getContext()->setModalType()`.

No behavioral change. Same dispatch logic. Different SSOT.

### Whelmed::InputHandler (New)

New file: `Source/whelmed/InputHandler.h` (header-only, follows Terminal pattern)

Responsibilities:
- Receives `keyPressed()` from `Whelmed::Component`
- Checks `AppState::getContext()->getModalType()`
- If `ModalType::none`: handle Whelmed navigation keys (existing j/k/gg/G/scroll logic, moved from inline `Component::keyPressed`)
- If `ModalType::selection`: handle Whelmed selection keys (h/j/k/l cursor movement, v/V/Ctrl-V toggle, y copy, Escape exit)
- Writes `ModalType` and `SelectionType` to AppState on mode transitions
- Reads/writes pane-local selection coordinates on `Whelmed::State`

### Whelmed Selection State (Pane-Local)

New properties on `Whelmed::State` ValueTree (DOCUMENT subtree):

| Property | Type | Purpose |
|----------|------|---------|
| `selAnchorBlock` | int | Anchor position: block index |
| `selAnchorChar` | int | Anchor position: character index within block |
| `selCursorBlock` | int | Cursor position: block index |
| `selCursorChar` | int | Cursor position: character index within block |

These are pane-local coordinates meaningful only within Whelmed's block structure. Terminal keeps its own row/col selection coordinates in Terminal::State (unchanged).

### StatusBarOverlay Rewire

**Before:** Polled by VBlank lambda in `MainComponent::initialiseTabs()`. Reads `terminal->getModalType()`, `terminal->getSelectionType()`.

**After:** `StatusBarOverlay` implements `juce::ValueTree::Listener`. Listens to AppState TABS subtree. Updates on `valueTreePropertyChanged` for `modalType` and `selectionType` properties.

Changes:
1. `StatusBarOverlay` constructor takes `juce::ValueTree tabsTree` and adds itself as listener
2. `update()` signature changes — no longer takes ModalType/SelectionType params, reads from the listened tree directly
3. `valueTreePropertyChanged` calls internal `refresh()` which reads `modalType`, `selectionType` from the tree and updates label/visibility
4. Remove the VBlank polling lambda for modal state in `MainComponent::initialiseTabs()`
5. `StatusBarOverlay` no longer includes `Terminal::State.h` or `Terminal::SelectionType.h` — includes `Source/ModalType.h` and `Source/SelectionType.h`
6. `openFile` hint page/total: these remain Terminal-specific. StatusBarOverlay reads them from AppState if we add `hintPage`/`hintTotalPages` properties, or from active terminal if we keep those pane-local. **Recommendation:** keep hint page/total pane-local on Terminal::State. StatusBarOverlay reads them only when `modalType == openFile` and `activePaneType == "terminal"`.

### Dispatch Flow

```
Key pressed on active pane
  |
  v
PaneComponent::keyPressed()
  |
  +-- Terminal::Component -> Terminal::InputHandler::handleKey()
  |     |
  |     +-- reads AppState::modalType
  |     +-- if none: Action system -> PTY
  |     +-- if selection: handleSelectionKey() (Terminal grid coords)
  |     +-- if openFile: handleOpenFileKey()
  |
  +-- Whelmed::Component -> Whelmed::InputHandler::handleKey()
        |
        +-- reads AppState::modalType
        +-- if none: Whelmed navigation (j/k/gg/G/scroll)
        +-- if selection: handleSelectionKey() (Whelmed block/char coords)
```

### Modal Transition Guard

When switching active pane (tab change, Whelmed open/close), if `modalType != none`:
- Reset `modalType` to `none` in AppState
- Reset `selectionType` to `none` in AppState
- Clear pane-local selection state on the pane being deactivated

This prevents stale modal state bleeding across pane switches. Implemented in `PaneComponent::focusGained()` or `Panes::visibilityChanged()`.

---

## Hit Testing (Whelmed Selection Rendering)

Selection rendering requires two capabilities on `TextBlock`:

```cpp
// Convert pixel coordinate to logical position
Pos hitTest (juce::Point<float> pixel) const;

// Get glyph bounding rectangles for a character range (for highlight rendering)
juce::RectangleList<float> getSelectionRects (int startChar, int endChar) const;
```

Both methods are TextLayout-dependent internally but expose a stable API. When `juce::TextLayout` is replaced with `jreng::TextLayout` in Phase 2, only these method bodies change.

**Phase 1 implementation** (juce::TextLayout):
- `hitTest`: iterate `TextLayout::getNumLines()`, find line by y, iterate glyphs by x
- `getSelectionRects`: iterate lines/glyphs in range, collect glyph rects

---

## File Changes Summary

### New Files
| File | Purpose |
|------|---------|
| `Source/ModalType.h` | App-level ModalType enum |
| `Source/SelectionType.h` | App-level SelectionType enum |
| `Source/whelmed/InputHandler.h` | Whelmed keyboard modal handler |

### Modified Files
| File | Change |
|------|--------|
| `Source/AppIdentifier.h` | Add `modalType`, `selectionType` identifiers |
| `Source/AppState.h/.cpp` | Add modal getters/setters |
| `Source/component/StatusBarOverlay.h` | ValueTree listener, remove Terminal includes |
| `Source/MainComponent.cpp` | Pass TABS tree to StatusBarOverlay, remove VBlank modal polling |
| `Source/component/InputHandler.h/.cpp` | Read/write AppState instead of Terminal::State |
| `Source/component/TerminalComponent.h/.cpp` | Remove modal getters, delegate to AppState |
| `Source/terminal/data/State.h/.cpp` | Remove modalType/selectionType atomics and flush |
| `Source/whelmed/Component.h/.cpp` | Own InputHandler, delegate keyPressed |
| `Source/whelmed/State.h/.cpp` | Add selection coordinate properties |
| `Source/whelmed/TextBlock.h/.cpp` | Add hitTest, getSelectionRects |
| `Source/whelmed/Screen.h/.cpp` | Selection highlight rendering in paint() |

### Deleted Enums (moved)
| Old Location | New Location |
|---|---|
| `Terminal::ModalType` in `State.h` | `ModalType` in `Source/ModalType.h` |
| `Terminal::SelectionType` in `SelectionType.h` | `SelectionType` in `Source/SelectionType.h` |

---

## Execution Order

1. Create `Source/ModalType.h` and `Source/SelectionType.h` (move enums)
2. Add `modalType`/`selectionType` to AppIdentifier.h and AppState
3. Rewire `Terminal::InputHandler` to read/write AppState
4. Remove modalType/selectionType from `Terminal::State`
5. Update all Terminal::Component modal getters to delegate to AppState
6. Rewire `StatusBarOverlay` to ValueTree listener on AppState
7. Remove VBlank modal polling from MainComponent
8. Add modal transition guard on pane switch
9. Create `Whelmed::InputHandler`, move navigation keys from Component::keyPressed
10. Add selection coordinates to Whelmed::State
11. Implement TextBlock::hitTest and getSelectionRects
12. Implement Whelmed selection key handling in InputHandler
13. Implement selection highlight rendering in Whelmed::Screen::paint()

Steps 1-8: Terminal refactor, no new features, must not regress.
Steps 9-13: Whelmed selection, new feature.

---

## Acceptance Criteria

- [ ] ModalType and SelectionType owned exclusively by AppState — no duplicates
- [ ] Terminal keyboard selection works identically to current behavior
- [ ] StatusBarOverlay shows correct mode for both Terminal and Whelmed
- [ ] StatusBarOverlay updates via ValueTree listener, not polling
- [ ] Whelmed selection mode: enter with v/V, navigate with h/j/k/l, copy with y, exit with Escape
- [ ] Pane switch clears modal state
- [ ] openFile/uriAction modes unaffected (Terminal-only, still work)
- [ ] Builds clean on Windows and macOS
- [ ] Zero early returns in all new code
- [ ] BLESSED compliance verified by Auditor
