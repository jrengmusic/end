# PLAN-PANES.md — Application ValueTree + Split Panes

**Date:** 2026-03-10  
**Status:** Planning  
**Depends on:** Sprint 87 (tabs complete)

---

## Objective

Replace `state.lua` with an application-level `juce::ValueTree` that is the Single Source of Truth for all UI state. Build `Terminal::Pane` and `Terminal::Panes` to support split panes within each tab. Serialize the entire application state to `state.xml` on quit, restore on launch.

---

## ValueTree Schema

```
END                                         <- application root
+-- WINDOW
|   +-- width = 1200
|   +-- height = 800
|   +-- zoom = 1.0
+-- TABS
    +-- active = 0                          <- selected tab index
    +-- position = "left"                   <- tab bar orientation
    +-- TAB                                 <- first tab
    |   +-- PANES                           <- split tree root
    |       +-- type = "leaf"
    |       +-- SESSION                     <- Terminal::State tree (grafted)
    |           +-- PARAM id="cols" ...
    |           +-- PARAM id="visibleRows" ...
    |           +-- MODES ...
    |           +-- NORMAL ...
    |           +-- ALTERNATE ...
    +-- TAB                                 <- second tab (split example)
        +-- PANES
            +-- type = "split"
            +-- direction = "horizontal"
            +-- ratio = 0.5
            +-- PANES                       <- left/top child
            |   +-- type = "leaf"
            |   +-- SESSION ...
            +-- PANES                       <- right/bottom child
                +-- type = "leaf"
                +-- SESSION ...
```

### Key Design Decisions

- `SESSION` tree from `Terminal::State` is grafted as child of leaf `PANES` node after construction (approach A — minimal disruption to existing State)
- `PANES` is the recursive node type for both splits and leaves, distinguished by `type` property
- `ratio` is 0.0-1.0, resolution-independent, synced from `StretchableLayoutManager` after drag
- Config (`end.lua`) stays separate — user-facing configuration. ValueTree is runtime state.
- Serialization: XML (`state.xml`) — human-readable, debuggable, trivially switchable to binary later

---

## New Identifiers

Namespace `App::ID` (new file `Source/AppIdentifier.h`):

```
END, WINDOW, TABS, TAB, PANES
width, height, zoom, active, position, type, direction, ratio
```

`Terminal::ID` stays unchanged — terminal-level identifiers only.

---

## New Classes

### `Terminal::Pane` — Leaf wrapper component

```
Terminal::Pane : public juce::Component,
                 public jreng::Value::ObjectID<Terminal::Pane>
```

- Wraps one `Terminal::Component` (composition, not inheritance)
- ComponentID = UUID string (for ValueTree binding)
- Owns the terminal, forwards resize to it
- Thin — no logic beyond layout delegation

### `Terminal::Panes` — Split tree component (one per tab)

```
Terminal::Panes : public juce::Component
```

- Owns the split tree as a `juce::ValueTree` subtree (reference into application tree)
- Recursively builds component hierarchy from ValueTree:
  - Leaf node -> creates `Terminal::Pane` with a `Terminal::Component` inside
  - Split node -> creates two child `Panes` + `StretchableLayoutManager` + `StretchableLayoutResizerBar`
- Rebuilds on split/close mutations
- `resized()` calls `layoutManager.layOutComponents()`

### `AppState` — Application ValueTree owner

```
AppState : public jreng::Context<AppState>
```

- Owns the root `END` ValueTree
- Constructed in `ENDApplication` (before MainComponent)
- `save()` — writes `state.xml` to `~/.config/end/`
- `load()` — reads `state.xml`, returns the tree
- Replaces `Config::loadState()`, `Config::saveWindowSize()`, `Config::saveZoom()`

---

## jreng Module Port

Port from `kuassa::ValueTree` to `jreng_core/value_tree/`:

| Utility | Purpose |
|---|---|
| `applyFunctionRecursively` | Recursive visitor |
| `getChildWithName` | Recursive find by type |
| `getOrCreateChildWithName` | Find or create |
| `loadState` | Recursive property merge from XML |
| `buildUniqueNodeMap` | O(1) lookup map for listener callbacks |

These go in `modules/jreng_core/value_tree/jreng_value_tree.h/.cpp` as static functions in `jreng::ValueTree` struct.

---

## Execution Phases

### Phase 1: Foundation (no visible change)

1. **Port ValueTree utilities** to `jreng_core`
2. **Create `App::ID`** identifiers
3. **Create `AppState`** — builds the ValueTree skeleton, save/load XML
4. **Wire `AppState`** into `ENDApplication` — construct before MainComponent
5. **Migrate window state** — `AppState` reads/writes WINDOW subtree, remove `state.lua` code from Config

**Checkpoint:** App launches, saves/restores window size via `state.xml` instead of `state.lua`. No other behavioral change.

### Phase 2: Graft Terminal::State into application tree

6. **Graft SESSION** — after each `Terminal::Component` construction, graft its `State::get()` tree as child of the corresponding `PANES` leaf node in the application tree
7. **TABS subtree** — `Tabs::addNewTab()` creates TAB + PANES children in the application tree
8. **Tab close** — removes TAB subtree from application tree

**Checkpoint:** Application tree reflects tab state. `state.xml` contains full terminal state on quit. No split panes yet — each tab has one leaf PANES node.

### Phase 3: Terminal::Pane wrapper

9. **Create `Terminal::Pane`** — thin wrapper with ObjectID, owns Terminal::Component
10. **Refactor `Tabs`** — replace direct `Owner<GLComponent>` with `Owner<Terminal::Pane>` (or Panes owns them)
11. **Verify** — everything works as before, just with Pane wrapper in between

**Checkpoint:** Same behavior, but terminals are wrapped in Pane components.

### Phase 4: Terminal::Panes (single pane, no splits yet)

12. **Create `Terminal::Panes`** — component that reads its ValueTree subtree and builds one Pane for a leaf node
13. **Integrate into Tabs** — each tab's content is a `Panes` instance instead of a raw terminal
14. **GLRenderer** — verify `getComponents()` still works (Panes exposes its terminals for GL iteration)

**Checkpoint:** Each tab has a Panes component containing one Pane containing one terminal. Functionally identical to before.

### Phase 5: Split operations

15. **`splitHorizontal()` / `splitVertical()`** — mutates the ValueTree (converts leaf to split + two leaves), Panes rebuilds its component tree
16. **`StretchableLayoutManager`** — one per split node, manages the two children + resizer bar
17. **`StretchableLayoutResizerBar`** — one per split, syncs ratio back to ValueTree on drag
18. **`closePane()`** — removes a leaf, collapses parent split to remaining child
19. **Commands + keybindings** — `splitH`, `splitV`, `closePane` added to commandDefs and KeyBinding

**Checkpoint:** Can split panes, resize with drag, close panes.

### Phase 6: Pane navigation

20. **`focusLeft/Right/Up/Down`** — spatial navigation between panes based on bounds
21. **Active pane indicator** — visual indicator on focused pane (line, like tab indicator)
22. **Commands + keybindings** — `focusLeft`, `focusRight`, `focusUp`, `focusDown`

**Checkpoint:** Full split pane system operational.

### Phase 7: State restore

23. **Launch restore** — `AppState::load()` reads `state.xml`, MainComponent rebuilds tabs/panes from tree
24. **Tab count** — count TAB children, create that many tabs
25. **Split layout** — recursively rebuild Panes from PANES subtree
26. **Terminal state** — each terminal's State attaches to its SESSION subtree (visual state restored, new PTY/shell spawned)
27. **Fallback** — if no `state.xml` or parse failure, create one tab with one leaf pane (current default)

**Checkpoint:** Full session layout persistence across launches.

---

## Files Modified (estimated)

| File | Change |
|---|---|
| `Source/AppIdentifier.h` | NEW — App::ID namespace |
| `Source/AppState.h/.cpp` | NEW — application ValueTree owner |
| `Source/component/Pane.h/.cpp` | NEW — Terminal::Pane wrapper |
| `Source/component/Panes.h/.cpp` | NEW — Terminal::Panes split tree |
| `Source/Main.cpp` | Add AppState member to ENDApplication |
| `Source/MainComponent.h/.cpp` | Use AppState for window size, wire Panes |
| `Source/component/Tabs.h/.cpp` | Integrate Panes, remove direct terminal ownership |
| `Source/config/Config.h/.cpp` | Remove state.lua code (loadState, saveWindowSize, saveZoom, getStateFile) |
| `Source/config/KeyBinding.h` | Add split/close/focus command IDs |
| `modules/jreng_core/value_tree/` | NEW — ported ValueTree utilities |
| `modules/jreng_core/jreng_core.h` | Include new value_tree header |

---

## Risk / Open Questions

1. **GLRenderer iteration** — currently iterates `Owner<GLComponent>` from Tabs. With Panes owning terminals, need to ensure GLRenderer can still find all visible GL components. May need Panes to expose a flat list.

2. **VBlank per terminal** — each terminal has its own VBlankAttachment. Multiple visible terminals (splits) means multiple VBlank callbacks per frame. Should be fine but verify no performance regression.

3. **Keyboard focus** — with splits, only one pane has focus at a time. Need clear focus management so keystrokes go to the right terminal.

4. **Grid size overlay** — currently computed in MainComponent::resized() for the single active terminal. With splits, each pane has its own grid size. MessageOverlay may need to show per-pane or just show for the focused pane.

5. **Zoom** — currently per-terminal. With splits, should zoom affect all panes in a tab, or just the focused pane? ARCHITECT decides.
