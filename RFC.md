# RFC — Tab Improvements: Rename, Drag-Reorder, SVG Button Graphics
Date: 2026-04-24
Status: Ready for COUNSELOR handoff

## Problem Statement

END's tab system lacks three features expected by power users:
1. Tabs cannot be renamed — auto-computed displayName is the only option.
2. Tabs cannot be reordered by dragging.
3. Tab button graphics are hardcoded (parallelogram path in `Terminal::LookAndFeel`), not user-customizable.

## Research Summary

### Current Tab Architecture

- `Terminal::Tabs` inherits `juce::TabbedComponent` (JUCE built-in).
- `juce::TabbedComponent` owns a `juce::TabbedButtonBar` internally with no injection point for a custom bar subclass.
- `juce::TabbedComponent` manages content components via `WeakReference<Component>` array — END bypasses this entirely, passing `nullptr` and managing `Panes` visibility itself in `currentTabChanged()`.
- Tab names driven by `juce::Value tabName` bound via `referTo` to active terminal's `App::ID::displayName`. `State::flushStrings()` computes displayName: foregroundProcess > cwd leaf.
- `juce::TabbedButtonBar` has `moveTab(currentIndex, newIndex)` built in, but no mouse gesture to trigger it.
- `juce::TabbedButtonBar::popupMenuClickOnTab()` virtual exists — right-click already dispatched from `TabBarButton::clicked()`.
- `juce::TabbedComponent::createTabButton()` virtual exists but routes through an inner `ButtonBar` struct that cannot be replaced without forking.
- JUCE's `Drawable::createFromSVGFile()` loads SVG to a drawable. `juce::Image` slicing is straightforward.

### Fork Justification

`juce::TabbedComponent` cannot be extended for drag-reorder without either:
- (a) Reimplementing from scratch (same work as forking, no proven starting point).
- (b) Forking the existing code and modifying it.

Option (b) is strictly better. The fork is small (~1150 lines total across 3 JUCE source files) and END already bypasses the content-component management that `TabbedComponent` provides. The fork strips dead code and adds the hooks needed for all three features.

### Files Examined

| File | Purpose |
|------|---------|
| `Source/component/Tabs.h/cpp` | Current tab container, inherits `juce::TabbedComponent` |
| `Source/component/LookAndFeel.h/cpp` | Tab button rendering (parallelogram), colour system |
| `Source/AppState.h` | ValueTree root, TAB node management |
| `Source/AppIdentifier.h` | ValueTree identifiers |
| `Source/action/Action.h` | Action registry, key dispatch |
| `Source/MainComponentActions.cpp` | Built-in action registration (`registerTabActions`) |
| `Source/config/default_action.lua` | Key binding defaults |
| `Source/scripting/Scripting.cpp` | Lua `display` API table |
| `JUCE/.../juce_TabbedButtonBar.h` | TabBarButton (90 lines), TabbedButtonBar (380 lines header) |
| `JUCE/.../juce_TabbedButtonBar.cpp` | Bar implementation (592 lines) |
| `JUCE/.../juce_TabbedComponent.h/cpp` | TabbedComponent (236 + 328 lines) |
| `jam_gui/layout/` | Existing PaneManager and PaneResizerBar |

## Principles and Rationale

### Fork to jam, not END

The fork lives in `jam_gui/layout/` alongside `PaneManager` and `PaneResizerBar`. These are layout primitives. If CAROLINE or future projects need tabs, the fork is available. YAGNI is satisfied because END needs it now — the fork isn't speculative.

### Strip content-component management

`juce::TabbedComponent` manages a `WeakReference<Component>` array and a `panelComponent` for showing/hiding tab content. END bypasses this entirely — it manages `Panes` visibility itself. The fork strips `contentComponents`, `panelComponent`, and the `deleteComponentId` helper. The fork keeps: bar ownership, depth, orientation, layout arithmetic, and virtual callbacks (`currentTabChanged`, `popupMenuClickOnTab`, `createTabButton`).

### Separate files per class

`jam::TabBarButton`, `jam::TabbedButtonBar`, and `jam::TabbedComponent` each get their own header. JUCE bundles `TabBarButton` and `TabbedButtonBar` in one header — the fork separates them because `TabBarButton` now has its own drag responsibility.

### SVG stays in END's LookAndFeel

The jam fork provides structural hooks (drag, reorder, button factory). Rendering (SVG 3-slice, parallelogram fallback, colours) stays in `Terminal::LookAndFeel::drawTabButton()`. The fork does not impose a visual style.

### User tab name wins unconditionally

`userTabName` property on the TAB ValueTree node. When non-empty, it overrides the auto-computed `displayName` from `State::flushStrings()`. No priority negotiation, no merge logic. User said it, that's what it is.

## Scaffold

### 1. jam Fork — 5 files in `jam_gui/layout/`

| File | Class | Lines (est) |
|------|-------|-------------|
| `jam_tab_bar_button.h` | `jam::TabBarButton` | ~120 |
| `jam_tabbed_button_bar.h` | `jam::TabbedButtonBar` | ~180 |
| `jam_tabbed_button_bar.cpp` | impl | ~350 |
| `jam_tabbed_component.h` | `jam::TabbedComponent` | ~100 |
| `jam_tabbed_component.cpp` | impl | ~150 |

**`jam::TabBarButton`** — forked from `juce::TabBarButton`. Additions:
- `mouseDrag()` — tracks mouse position along bar axis, computes target index from button positions, visual feedback (reposition during drag).
- `mouseUp()` — if dragged past threshold, calls `owner.moveTab(fromIndex, toIndex)`. Below threshold = normal click.
- Drag threshold constant to distinguish click from drag intent.
- Owner reference type changes from `juce::TabbedButtonBar&` to `jam::TabbedButtonBar&`.

**`jam::TabbedButtonBar`** — forked from `juce::TabbedButtonBar`. Additions:
- `onTabMoved` callback (`std::function<void(int fromIndex, int toIndex)>`) — fired after `moveTab()` so the owner (`Tabs`) can mirror the reorder in its data structures.
- `onTabRightClicked` callback (`std::function<void(int tabIndex)>`) — alternative to virtual `popupMenuClickOnTab` for non-inheriting consumers.
- `createTabButton()` override returns `jam::TabBarButton` by default.
- Inner `BehindFrontTabComp` preserved.

**`jam::TabbedComponent`** — forked from `juce::TabbedComponent`. Changes:
- Stripped: `contentComponents` array, `panelComponent`, `deleteComponentId`, `getTabContentComponent()`, content-component parameter from `addTab()`.
- `addTab()` signature simplified: `addTab(name, colour, insertIndex)` — no content component.
- Inner `ButtonBar` struct creates `jam::TabbedButtonBar`.
- `createTabButton()` virtual preserved.
- All virtual callbacks preserved: `currentTabChanged()`, `popupMenuClickOnTab()`.
- `moveTab()` delegates to bar's `moveTab()`.

### 2. Tab Rename — END changes

**ValueTree property:**
- `App::ID::userTabName` added to `AppIdentifier.h`.
- Stored on the TAB node (not SESSION — tab name is a tab concern).
- Persisted via existing `save()` / `load()` path (property lives in ValueTree, serialization is automatic).

**Display name resolution** in `Tabs::valueChanged()`:
```cpp
// Current: always uses displayName from State::flushStrings()
// New: userTabName on TAB node wins when non-empty

const auto tabNode { AppState::getContext()->getTab (getCurrentTabIndex()) };
const auto userOverride { tabNode.getProperty (App::ID::userTabName).toString() };
const auto name { userOverride.isNotEmpty() ? userOverride : tabName.toString() };
```

**Rename triggers:**

1. **Right-click** — `popupMenuClickOnTab()` override shows an inline `juce::Label` editor on the tab button. On commit, writes to `App::ID::userTabName`. On empty submit, clears the override (reverts to auto name).

2. **Action** — `rename_tab` registered in `registerTabActions()`:
   - id: `"rename_tab"`
   - name: `"Rename Tab"`
   - category: `"Tabs"`
   - isModal: `true`
   - Callback: triggers the same inline editor as right-click, on the active tab.

3. **Key binding** in `default_action.lua`:
   ```lua
   rename_tab = "shift+t",   -- modal: prefix + Shift+T
   ```

4. **Command palette** — appears in Action::List automatically (all registered actions do).

**Inline editor implementation:**
- Create a `juce::Label` with `setEditable(true)`, position it over the tab button text area.
- Pre-fill with current tab name.
- `Label::onTextChange` or `textEditorReturnKeyPressed` → write to `App::ID::userTabName` on the TAB node.
- Empty text → clear `userTabName` property (revert to auto).
- Escape → cancel without writing.
- Editor destroyed on focus loss or commit.

### 3. Tab Drag-Reorder — END changes

**`Tabs` mirrors reorder** via `jam::TabbedButtonBar::onTabMoved`:
```cpp
bar.onTabMoved = [this] (int fromIndex, int toIndex)
{
    // Mirror in panes vector
    auto movedPane { std::move (panes[fromIndex]) };
    panes.erase (panes.begin() + fromIndex);
    panes.insert (panes.begin() + toIndex, std::move (movedPane));

    // Mirror in TABS ValueTree
    auto tabsTree { AppState::getContext()->getTabs() };
    tabsTree.moveChild (fromIndex, toIndex, nullptr);

    // Persist
    if (AppState::getContext()->isDaemonMode())
        AppState::getContext()->save();
};
```

### 4. SVG Tab Button — END changes

**Config key:**
- `Config::Key::tabButtonSvg` → `"tab.button_svg"` (string path, default empty).
- Empty = use built-in parallelogram. Non-empty = load SVG from path.
- Path relative to `~/.config/end/` or absolute.

**SVG spec — 3×2 grid:**
```
┌──────────┬──────────────────┬──────────┐
│ left     │ center           │ right    │  ← top row: active state
│ edge     │ (stretchable)    │ edge     │
├──────────┼──────────────────┼──────────┤
│ left     │ center           │ right    │  ← bottom row: inactive state
│ edge     │ (stretchable)    │ edge     │
└──────────┴──────────────────┴──────────┘

Width divided into 3 equal horizontal regions.
Height divided into 2 equal vertical regions.
```

**Load pipeline** (`Terminal::LookAndFeel`):
1. New method `loadTabButtonSvg()` called from `setColours()`.
2. Read `Config::Key::tabButtonSvg`. If empty, clear cached images, return.
3. `Drawable::createFromSVGFile(path)` → rasterize to `juce::Image`.
4. Slice into 6 sub-images: `{left,center,right} × {active,inactive}`.
5. Edge width = image width / 3. Row height = image height / 2.
6. Cache as 6 `juce::Image` members on `LookAndFeel`.

**Draw pipeline** (`drawTabButton()`):
1. If no cached SVG images → existing parallelogram path (unchanged).
2. If cached: select active/inactive row based on toggle state.
3. Compute scale factor: `tabBarHeight / svgRowHeight`.
4. Scaled edge width: `svgEdgeWidth * scaleFactor`.
5. Draw left edge at native aspect (scaled), left-aligned in button area.
6. Draw right edge at native aspect (scaled), right-aligned.
7. Draw center stretched horizontally to fill gap between edges.
8. Draw text on top (unchanged).
9. Hover / mouse-over: apply alpha or brightness modifier to the images (consistent with current inactive hover behavior).

**Hot reload:** `setColours()` is already called on config reload. `loadTabButtonSvg()` runs inside it. SVG file changes picked up on manual reload (`Cmd+R`) or auto-reload.

### 5. Migration — `Terminal::Tabs`

`Terminal::Tabs` changes base class from `juce::TabbedComponent` to `jam::TabbedComponent`. All existing method calls (`setTabBarDepth`, `getNumTabs`, `setCurrentTabIndex`, `addTab`, `removeTab`, `getTabbedButtonBar`, etc.) have identical signatures in the fork. Migration is a namespace swap in the header + removing the content component parameter from `addTab()` calls (already `nullptr`).

## BLESSED Compliance Checklist

- [x] **Bounds** — jam fork owned by MainComponent lifetime (same as current JUCE TabbedComponent). SVG images owned by LookAndFeel. TabBarButton owned by TabbedButtonBar. No ambiguous lifetimes.
- [x] **Lean** — Fork strips ~180 lines of unused content-component management. Each new file under 300 lines. No god objects introduced.
- [x] **Explicit** — `userTabName` is an explicit property, not shadow state. SVG path is an explicit config key. Drag threshold is a named constant.
- [x] **SSOT** — `userTabName` on TAB node is the single source for user override. Auto-computed `displayName` on SESSION remains SSOT for auto names. No duplication.
- [x] **Stateless** — TabBarButton drag state is transient (mouse down to mouse up). No persistent drag state stored.
- [x] **Encapsulation** — jam fork exposes only public API (callbacks, virtuals). Drag logic is internal to TabBarButton. SVG slicing is internal to LookAndFeel. Tabs mirrors reorder via callback, never pokes bar internals.
- [x] **Deterministic** — Same tab order produces same ValueTree. Same SVG produces same slices. Same user name produces same display.

## Open Questions

1. **SVG edge width** — spec says "fixed sized divided by 3 horizontal area." Confirm: always exact thirds (width/3), or should the SVG encode edge width metadata (e.g. via a viewBox convention)?

2. **Rename editor style** — the inline Label editor needs font/colour. Use the same tab font from Config (`tab.family`, `tab.size`), or a distinct config key?

3. **Drag visual feedback** — during drag, should the dragged tab follow the mouse (live reposition), or show a ghost/indicator line at the drop target?

4. **`display.rename_tab(name)`** — should this be exposed in the Lua `display` API so custom actions can rename tabs programmatically? Not needed for MVP, but worth deciding before implementation.

## Handoff Notes

- The jam fork is the prerequisite. It must land before any END-side changes.
- Tab rename and SVG button are independent of each other — can be implemented in any order after the fork.
- Drag-reorder depends on the fork (needs `jam::TabBarButton` with mouse handling).
- `Terminal::Tabs` migration from `juce::TabbedComponent` to `jam::TabbedComponent` is a mechanical namespace swap — low risk but touches many call sites. Should be its own commit.
- The `popupMenuClickOnTab` virtual in the fork preserves the right-click hook. No new virtual needed for rename — the existing hook is sufficient.
- SVG hot-reload piggybacks on existing config reload path (`setColours()` called on reload). No new file watcher needed.
- The `LookAndFeel::LookAndFeelMethods` struct on `juce::TabbedButtonBar` declares drawing methods that END's LookAndFeel overrides. The fork must preserve this dispatch interface so existing overrides compile unchanged.
