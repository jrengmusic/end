# PLAN: Tab Improvements — Rename, Drag-Reorder, SVG Button Graphics

**RFC:** RFC-tabs.md
**Date:** 2026-04-26
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no LANGUAGE.md overrides)

## Overview

Fork `juce::TabbedComponent` / `juce::TabbedButtonBar` / `juce::TabBarButton` into `jam_gui/layout/` with drag-reorder support and stripped content-component management. Migrate `Terminal::Tabs` to the fork. Add tab rename (inline editor + `userTabName` property + Lua API). Add SVG tab button graphics with group-ID-based slicing.

## ARCHITECT Decisions (locked)

1. **SVG edge width** — group-ID metadata, not fixed thirds. SVG contains named groups: `active-left`, `active-center`, `active-right`, `inactive-left`, `inactive-center`, `inactive-right`. Edge width determined by each group's rendered bounds.
2. **Rename editor** — inline `juce::Label` over tab button, using current tab font (`display.tab.family` / `display.tab.size`).
3. **Drag visual** — live reposition. `moveTab()` called during drag as mouse crosses tab boundaries.
4. **Lua API** — `display.rename_tab(name)` exposed now.

## Validation Gate

Each step validated by @Auditor against:
- MANIFESTO.md (BLESSED)
- NAMES.md (naming)
- JRENG-CODING-STANDARD.md (C++ standards)
- Locked PLAN decisions above (no deviation)

## Steps

### Step 1: jam fork — layout primitives

**Scope:** 5 new files in `jam_gui/layout/`, update `jam_gui.h`

**Files created:**
- `jam_gui/layout/jam_tab_bar_button.h` — `jam::TabBarButton`
- `jam_gui/layout/jam_tabbed_button_bar.h` — `jam::TabbedButtonBar`
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — impl
- `jam_gui/layout/jam_tabbed_component.h` — `jam::TabbedComponent`
- `jam_gui/layout/jam_tabbed_component.cpp` — impl

**Files modified:**
- `jam_gui/jam_gui.h` — add includes after line 94 (`jam_pane_manager.h`), dependency order: `jam_tab_bar_button.h`, `jam_tabbed_button_bar.h`, `jam_tabbed_component.h`

**Action:**

**`jam::TabBarButton`** (header-only, forked from `juce::TabBarButton`):
- Owner reference: `jam::TabbedButtonBar&` (not `juce::TabbedButtonBar&`)
- `mouseDrag()` — track mouse position along bar axis, compute target index from sibling button positions, call `owner.moveTab(currentIndex, targetIndex)` when crossing boundary (live reposition)
- `mouseUp()` — if drag distance below threshold, delegate to normal `Button::mouseUp` click path. Above threshold, drag already committed via live `moveTab()` — just clean up drag state.
- Drag threshold: `static constexpr int dragThreshold { 5 }` (pixels)
- Transient drag state: `bool isDragging { false }`, `int dragStartX { 0 }` — mouse down to mouse up only (BLESSED S: Stateless)
- `paintButton()` — calls `juce::TabbedButtonBar::LookAndFeelMethods::drawTabButton()` via dynamic_cast on `getLookAndFeel()` (preserves existing LookAndFeel dispatch — existing `Terminal::LookAndFeel` overrides compile unchanged)
- All other methods identical to `juce::TabBarButton`: `getExtraComponent()`, `setExtraComponent()`, `getBestTabLength()`, `getActiveArea()`, `resized()`, `clicked()`
- `clicked()` — right-click dispatches to `owner.popupMenuClickOnTab(getIndex(), getName())`

**`jam::TabbedButtonBar`** (forked from `juce::TabbedButtonBar`):
- `Orientation` enum: `TabsAtTop`, `TabsAtBottom`, `TabsAtLeft`, `TabsAtRight` (same values as JUCE)
- `LookAndFeelMethods` — NOT declared here. Fork uses `juce::TabbedButtonBar::LookAndFeelMethods` for all drawing dispatch. No new virtual drawing methods.
- `onTabMoved` callback: `std::function<void (int fromIndex, int toIndex)>` — fired after `moveTab()` completes
- `onTabRightClicked` callback: `std::function<void (int tabIndex)>` — alternative to virtual for non-inheriting consumers
- `createTabButton()` virtual returns `jam::TabBarButton*` by default
- `moveTab (int currentIndex, int newIndex)` — reorders internal button array, fires `onTabMoved`
- Inner `BehindFrontTabComp` preserved
- All existing public API preserved: `setOrientation`, `getOrientation`, `getNumTabs`, `addTab`, `removeTab`, `setCurrentTabIndex`, `getCurrentTabIndex`, `getTabButton`, `getTabNames`, `clearTabs`, `setTabName`, `getExtraComponent`, `setExtraComponent`
- Virtual `popupMenuClickOnTab (int tabIndex, const juce::String& tabName)` preserved
- Virtual `currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName)` preserved

**`jam::TabbedComponent`** (forked from `juce::TabbedComponent`):
- **Stripped:** `contentComponents` array, `panelComponent`, `deleteComponentId`, `getTabContentComponent()`, content-component param from `addTab()`
- `addTab (const juce::String& name, juce::Colour colour, int insertIndex)` — simplified, no content component
- Inner `ButtonBar` struct creates `jam::TabbedButtonBar` (not `juce::TabbedButtonBar`)
- `moveTab (int currentIndex, int newIndex)` — delegates to bar
- All virtual callbacks preserved: `currentTabChanged()`, `popupMenuClickOnTab()`, `createTabButton()`
- Layout arithmetic preserved: `setTabBarDepth`, `getTabBarDepth`, `setOrientation`, `getOrientation`, `resized`
- Owns the `jam::TabbedButtonBar` via inner struct (same ownership pattern as JUCE)

**Validation:**
- Each file under 300 lines (L)
- No content-component dead code (L: stripped)
- `jam::TabBarButton` drag state is transient — `isDragging` + `dragStartX` reset on mouseUp (S: Stateless)
- No new LookAndFeelMethods — existing dispatch preserved (E: Encapsulation)
- `juce::TabbedButtonBar::LookAndFeelMethods` used for drawing — no layer violation, no new dependency
- Orientation enum uses same values as JUCE for zero-friction migration
- No anonymous namespaces, `static` for file-local symbols per JRENG-CODING-STANDARD
- Brace initialization, `not`/`and`/`or` tokens, no early returns

---

### Step 2: Terminal::Tabs migration

**Scope:** `Source/component/Tabs.h`, `Source/component/Tabs.cpp`

**Action:**
- Change base class: `juce::TabbedComponent` → `jam::TabbedComponent`
- Change orientation type: all `juce::TabbedButtonBar::Orientation` → `jam::TabbedButtonBar::Orientation`
- `addTab()` calls: remove `nullptr` content-component argument (already passing nullptr)
- `orientationFromString()` return type: `jam::TabbedButtonBar::Orientation`
- `getTabbedButtonBar()` calls: returns `jam::TabbedButtonBar&` now
- No logic changes — mechanical namespace swap

**Validation:**
- Compiles with zero logic changes beyond namespace + removed nullptr args
- All existing method signatures preserved (getNumTabs, setCurrentTabIndex, etc.)
- `Terminal::LookAndFeel` overrides still compile (drawing methods dispatched via `juce::TabbedButtonBar::LookAndFeelMethods`)
- Tab bar visibility, orientation, depth all unchanged

---

### Step 3: Tab drag-reorder (END-side wiring)

**Scope:** `Source/component/Tabs.cpp`

**Action:**
Wire `onTabMoved` callback on the `jam::TabbedButtonBar` (accessed via `getTabbedButtonBar()`). Callback mirrors reorder in:
1. `panes` vector (`jam::Owner<Panes>`) — move element from `fromIndex` to `toIndex`
2. TABS ValueTree — `tabsTree.moveChild (fromIndex, toIndex, nullptr)`
3. Persist if daemon mode

```cpp
getTabbedButtonBar().onTabMoved = [this] (int fromIndex, int toIndex)
{
    auto movedPane { std::move (panes.at (fromIndex)) };
    panes.erase (panes.begin() + fromIndex);
    panes.insert (panes.begin() + toIndex, std::move (movedPane));

    auto tabsTree { AppState::getContext()->getTabs() };
    tabsTree.moveChild (fromIndex, toIndex, nullptr);

    if (AppState::getContext()->isDaemonMode())
        AppState::getContext()->save();
};
```

**Validation:**
- Panes vector stays index-parallel with JUCE tab indices after reorder
- ValueTree child order matches visual tab order (D: Deterministic)
- No shadow state — panes vector and ValueTree are the only two structures tracking order (S: SSOT)
- Daemon persistence on reorder (existing save pattern)
- `.at()` for vector access per JRENG-CODING-STANDARD

---

### Step 4: Tab rename

**Scope:** `Source/AppIdentifier.h`, `Source/component/Tabs.h`, `Source/component/Tabs.cpp`, `Source/MainComponentActions.cpp`, `Source/lua/Engine.h`, `Source/lua/Engine.cpp`, `Source/config/default_keys.lua`

**Action (4a — ValueTree property):**
- Add `App::ID::userTabName` to `AppIdentifier.h` (property section, after `displayName`)
- Stored on TAB node — tab name is a tab concern, not session concern

**Action (4b — display name resolution):**
- In `Tabs::valueChanged()` (or wherever tab name is resolved from `tabName` Value): check `userTabName` on TAB node first. When non-empty, use it. Otherwise fall back to auto-computed `displayName`.
- `userTabName` wins unconditionally — no priority negotiation.

**Action (4c — inline editor):**
- New method `Tabs::showRenameEditor (int tabIndex)`:
  - Create `juce::Label` with `setEditable (true)`, position over tab button text area (from `getTabButton(tabIndex)->getScreenBounds()` mapped to local)
  - Font: tab font from `lua::Engine::getContext()->display.tab.family` / `.size`
  - Pre-fill with current tab name
  - `Label::onTextChange` or `textEditorReturnKeyPressed` → write to `App::ID::userTabName` on `AppState::getContext()->getTab (tabIndex)`
  - Empty text → clear `userTabName` property (revert to auto)
  - Escape → cancel without writing (via `textEditorEscapeKeyPressed`)
  - Destroy on focus loss or commit
  - Label owned as `std::unique_ptr<juce::Label>` member on Tabs — one editor at a time

**Action (4d — right-click trigger):**
- Override `popupMenuClickOnTab (int tabIndex, const juce::String&)` in `Tabs`
- Calls `showRenameEditor (tabIndex)`

**Action (4e — action + key binding):**
- Register `rename_tab` in `MainComponent::registerTabActions()`:
  - id: `"rename_tab"`, name: `"Rename Tab"`, description: `"Rename the active tab"`, category: `"Tabs"`, isModal: `true`
  - Callback: `tabs->showRenameEditor (tabs->getCurrentTabIndex())`
- Add to `Engine::keyMappings` array: `{ "rename_tab", "rename_tab", true }` — increment `keyMappingCount` to 23
- Add to `default_keys.lua` in Tabs section: `rename_tab = "shift+t"` (modal: prefix + Shift+T)

**Action (4f — Lua API):**
- Add `renameTab` to `DisplayCallbacks` struct: `std::function<void (const juce::String&)> renameTab`
- Wire in MainComponent: callback writes `App::ID::userTabName` on active TAB node. Empty string clears.
- Expose in Lua `display` table as `display.rename_tab(name)` — takes string argument
- `display.rename_tab("")` clears override (reverts to auto name)

**Validation:**
- `userTabName` on TAB node is SSOT for user override (S: SSOT)
- Auto-computed `displayName` on SESSION untouched — two distinct properties, no shadow (S: SSOT)
- Inline editor lifetime: created on trigger, destroyed on commit/escape/focus-loss — transient (S: Stateless, B: Bound)
- Single `std::unique_ptr<juce::Label>` member — one editor at a time, clear ownership (B: Bound)
- `rename_tab` action isModal = true — consistent with other modal actions in Tabs section
- NAMES.md: `userTabName` — semantic (Rule 3), noun (Rule 1), no type in name (Rule 2)
- NAMES.md: `showRenameEditor` — verb (Rule 1), semantic (Rule 3)
- NAMES.md: `renameTab` callback — verb+noun, consistent with existing `closeTab`, `newTab` pattern (Rule 5)

---

### Step 5: SVG tab button graphics

**Scope:** `Source/lua/Engine.h`, `Source/lua/Engine.cpp`, `Source/component/LookAndFeel.h`, `Source/component/LookAndFeel.cpp`

**Design principle:** `Terminal::LookAndFeel` always draws its own default tab button (current parallelogram + indicator). Custom SVG is a user opt-in override — only drawn when the user provides a valid SVG path in `display.tab.*` config. No hardcoded filenames. No baked-in group ID conventions in jam — group IDs are an END config contract between the user's SVG and END's LookAndFeel.

**Action (5a — config):**
- Add `buttonSvg` field to `Display::Tab` struct in `Engine.h`: `juce::String buttonSvg` (default empty)
- Parse in Engine.cpp from Lua: `tab.button_svg` — string path, relative to `~/.config/end/` or absolute
- Empty (default) = LookAndFeel draws built-in parallelogram. No SVG loaded, no validation needed.

**Action (5b — load + validate pipeline):**
- New method `LookAndFeel::loadTabButtonSvg()` — called from `setColours()` (piggybacks on existing config reload path)
- Read `lua::Engine::getContext()->display.tab.buttonSvg`. If empty, clear cached images, return (default path — built-in drawing).
- If non-empty:
  1. Resolve path (relative to `~/.config/end/` or absolute)
  2. Parse SVG file to `juce::XmlElement` via `juce::XmlDocument::parse()`
  3. **Validate:** find 6 required groups using `jam::XML::getChildByID()`: `active-left`, `active-center`, `active-right`, `inactive-left`, `inactive-center`, `inactive-right`
  4. If parse fails OR any group ID missing → log warning via `MessageOverlay` (existing error display pattern), clear cached images, fall back to built-in drawing
  5. If all 6 groups found → render each group to `juce::Image` via `jam::SVG::draw (graphics, area, groupElement)` at native size
  6. Cache as 6 `juce::Image` members on LookAndFeel

**Action (5c — draw pipeline):**
- `drawTabButton()` default path: built-in parallelogram + indicator (current code, unchanged, always the baseline)
- Custom SVG path (only when cached images are valid):
  1. Select active/inactive set based on toggle state
  2. Scale factor: `tabButtonHeight / svgRowHeight`
  3. Scaled edge width from left/right image widths × scale
  4. Draw left edge at native aspect, scaled, left-aligned
  5. Draw right edge at native aspect, scaled, right-aligned
  6. Draw center stretched horizontally to fill gap between edges
  7. Draw text on top (unchanged)
  8. Hover: apply alpha/brightness modifier (consistent with current inactive hover)

**Action (5d — hot reload):**
- `setColours()` already called on config reload (`Cmd+R`). `loadTabButtonSvg()` inside it. SVG file changes picked up on reload. No new file watcher.

**Validation:**
- Built-in drawing is always the baseline — never removed, never conditional on SVG existence (L: Lean — no dead code, no conditional compilation)
- SVG is purely additive user override — absent config = built-in (E: Explicit — no magic defaults)
- No hardcoded filenames anywhere — path comes from user config only (E: Explicit)
- Group IDs are an END user contract, not a jam convention — `jam::XML::getChildByID` is a generic tool (E: Encapsulation)
- Invalid SVG → warning + built-in fallback, not silent fail (E: Explicit, fail fast)
- SVG images owned by LookAndFeel instance — clear ownership (B: Bound)
- Same SVG always produces same cached images (D: Deterministic)
- Hot reload via existing path — no new watcher (S: SSOT for reload mechanism)
- Uses established `jam::XML` and `jam::SVG` (E: Encapsulation — extend existing pattern)
- NAMES.md: `buttonSvg` — semantic (Rule 3), noun (Rule 1). `loadTabButtonSvg` — verb (Rule 1).

## BLESSED Alignment

| Principle | How satisfied |
|-----------|--------------|
| **B — Bound** | jam fork: inner ButtonBar struct owns TabbedButtonBar (JUCE ownership pattern preserved). TabBarButton owned by bar. SVG images owned by LookAndFeel. Inline rename Label owned by `unique_ptr` on Tabs. |
| **L — Lean** | Fork strips ~180 lines of unused content-component management. Each new file < 300 lines. No god objects. |
| **E — Explicit** | `userTabName` is an explicit property. SVG path is an explicit config field. Drag threshold is a named constant. Group IDs are explicit metadata. No magic numbers. |
| **S — SSOT** | `userTabName` on TAB node = SSOT for user override. `displayName` on SESSION = SSOT for auto names. Panes vector + ValueTree = only two structures tracking tab order (mirrored, not duplicated — vector is the runtime representation, ValueTree is the persisted representation). |
| **S — Stateless** | TabBarButton drag state is transient (mouseDown → mouseUp). Inline editor is transient (created → destroyed). No persistent drag or editor state. |
| **E — Encapsulation** | Drag logic internal to TabBarButton. SVG slicing internal to LookAndFeel. Tabs mirrors reorder via callback — never pokes bar internals. LookAndFeel dispatch preserved via existing `juce::TabbedButtonBar::LookAndFeelMethods`. |
| **D — Deterministic** | Same tab order → same ValueTree. Same SVG → same cached images. Same user name → same display. |

## Risks / Open Questions

1. ~~**SVG group discovery**~~ — **Resolved.** `jam::XML::getChildByID()` provides recursive `<g id="...">` lookup on raw `juce::XmlElement` trees. `jam::SVG::draw()` renders any `XmlElement*` fragment. No `juce::Drawable` tree traversal needed.
2. **Tab name binding** — current `tabName` is a `juce::Value` bound via `referTo` to active terminal's `displayName`. The `userTabName` override sits on the TAB node, not the SESSION. The resolution logic in `valueChanged()` needs to read both sources. If `valueChanged` only fires on `displayName` changes (the bound Value), a separate listener on the TAB node's `userTabName` property may be needed to trigger UI update when user renames.
