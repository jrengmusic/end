# PLAN: Split Panes Implementation

**Date:** 2026-03-12
**Objective:** Implement vertical and horizontal split panes with draggable resizer bars.

**Prerequisite:** Panes extraction from Tabs — COMPLETE.

---

## Design

One split direction per tab. First split picks the direction, subsequent splits add more terminals in that same direction.

**Vertical split** = terminals side by side (columns). PaneManager lays out horizontally.
**Horizontal split** = terminals stacked (rows). PaneManager lays out vertically.

### PaneManager Item Layout

For N terminals, PaneManager manages `2N - 1` items (alternating terminal + resizer):

```
Item 0: terminal    (preferred: -1.0/N proportional)
Item 1: resizerBar  (fixed: 4px)
Item 2: terminal    (preferred: -1.0/N proportional)
Item 3: resizerBar  (fixed: 4px)
Item 4: terminal    (preferred: -1.0/N proportional)
```

### Components

- `PaneResizerBar` — already exists in `jreng_gui`. Takes `PaneManager*`, item index, vertical flag. On drag calls `parent->resized()`.
- `PaneManager` — already exists. `layOutComponents()` with `ComponentAccessor` callback.

### Panes Changes

New members:
- `bool isVertical { true }` — split direction (true = vertical/columns, false = horizontal/rows)
- `bool hasSplitDirection { false }` — whether direction is locked

New public methods:
- `void splitVertical()` — split active terminal vertically (add column)
- `void splitHorizontal()` — split active terminal horizontally (add row)
- `void closeTerminal (const juce::String& uuid)` — close specific terminal

Changed methods:
- `resized()` — if single terminal: fill bounds. If multiple: use PaneManager with alternating terminals + resizers.
- `createTerminal()` — now also creates resizer bar (if not first terminal), configures PaneManager item layouts.

### Tabs Changes

New forwarding methods:
- `void splitVertical()` — forwards to active Panes
- `void splitHorizontal()` — forwards to active Panes

### MainComponent Changes

- Re-add split commands to `commandDefs[]`
- Re-add split actions to `buildCommandActions()`

### KeyBinding Changes

- Re-add `splitHorizontal` and `splitVertical` to `CommandID` enum
- Re-add default key bindings in Config

### AppIdentifier Changes

- Keep `splitVertical` (currently dead, will be used for state persistence later)

---

## Steps

### Step 1: Implement split in Panes

Add to `Panes.h`:
- `bool isVertical { true }`
- `bool hasSplitDirection { false }`
- `void splitVertical()`
- `void splitHorizontal()`
- Private: `void rebuildLayout()` — reconfigures PaneManager item layouts and creates/destroys resizer bars

Add to `Panes.cpp`:

`splitVertical()`:
1. If `hasSplitDirection` and not `isVertical` — reject (can't mix directions)
2. Set `isVertical = true`, `hasSplitDirection = true`
3. Call `createTerminal()`
4. Call `rebuildLayout()`

`splitHorizontal()`:
1. If `hasSplitDirection` and `isVertical` — reject (can't mix directions)
2. Set `isVertical = false`, `hasSplitDirection = true`
3. Call `createTerminal()`
4. Call `rebuildLayout()`

`rebuildLayout()`:
1. Clear `resizerBars` and `paneManager`
2. For N terminals, create N-1 `PaneResizerBar` instances
3. Configure PaneManager: each terminal gets `-1.0/N` preferred, each resizer gets fixed 4px
4. Add resizer bars as children (`addAndMakeVisible`)

`resized()`:
1. If single terminal: `terminal->setBounds (getLocalBounds())`
2. If multiple: call `paneManager.layOutComponents()` with accessor that interleaves terminals and resizerBars, using `isVertical` for direction

**Validate:** Build. No behavior change yet (split not wired to UI).

### Step 2: Wire split through Tabs

Add to `Tabs.h`:
- `void splitVertical()`
- `void splitHorizontal()`

Add to `Tabs.cpp`:
- Forward to `getActivePanes()->splitVertical()` / `splitHorizontal()`
- After split, set `activeTerminalUuid` to the new terminal

**Validate:** Build.

### Step 3: Wire split to MainComponent commands

Add to `KeyBinding.h`:
- `splitHorizontal` and `splitVertical` back to `CommandID` enum

Add to `Config.h` / `Config.cpp`:
- Re-add `keysSplitHorizontal` and `keysSplitVertical` keys + defaults

Add to `MainComponent.h`:
- Add split entries to `commandDefs[]`

Add to `MainComponent.cpp`:
- Add split actions to `buildCommandActions()`

**Validate:** Build + run. Press split shortcut — new terminal appears beside/below the current one. Drag resizer bar to resize. Both terminals render via GL.

### Step 4: Handle focus in split panes

- Click on a terminal in a split → that terminal gets focus → `globalFocusChanged` updates `activeTerminalUuid`
- Copy/paste/zoom operate on the focused terminal (already works via `getActiveTerminal()`)

**Validate:** Click between split terminals, verify focus switches, verify copy/paste works on focused terminal.

### Step 5: Close pane in split

When a terminal in a split exits or is closed:
- Remove terminal from Owner
- Remove its SESSION from Panes' ValueTree
- Remove associated resizer bar
- Call `rebuildLayout()`
- If last terminal in tab, close the tab

**Validate:** Close one terminal in a split — remaining terminal fills the space. Close last terminal — tab closes.
