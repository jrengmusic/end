# PLAN: Phase 2 — jam_gui Tab System Rewrite + PaneManager Fix

**RFC:** none — objective from SPEC.md Phase 2
**Date:** 2026-06-04
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides per LANGUAGE.md)

## Overview

Rewrite jam_gui tab system: delete TabbedButtonBar + TabBarButton, extend button::Group (sync from kuassa + new API), create button::Options + button::TabButton, rewrite TabbedComponent backed by Group. Fix PaneManager resizer bar lifecycle. SVG 3-slice LookAndFeel infrastructure.

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md, and this PLAN.

## Steps

### Step 1: Delete old tab system

**Scope:** jam_gui, jam_look_and_feel
**Action:**
- Delete files: `jam_gui/layout/jam_tab_bar_button.h`, `jam_gui/layout/jam_tab_bar_button.cpp`, `jam_gui/layout/jam_tabbed_button_bar.h`, `jam_gui/layout/jam_tabbed_button_bar.cpp`, `jam_gui/layout/jam_tabbed_component.h`, `jam_gui/layout/jam_tabbed_component.cpp`
- Remove 3 TU includes from `jam_gui/jam_gui.cpp` (lines 6-8: jam_tab_bar_button.cpp, jam_tabbed_button_bar.cpp, jam_tabbed_component.cpp)
- Remove 3 header includes from `jam_gui/jam_gui.h` (layout section: jam_tab_bar_button.h, jam_tabbed_button_bar.h, jam_tabbed_component.h)
- Remove `drawTabButton(juce::TabBarButton&, ...)` override and `getTabButtonBestWidth(juce::TabBarButton&, ...)` override from `jam_look_and_feel/theme/jam_look_and_feel_theme.h` (lines 99-100) and their implementations in `.cpp` (lines 498-650+)

**Validation:** jam compiles with the tab system removed. No dangling references. jam_look_and_feel compiles without the old drawTabButton.

### Step 2: LookAndFeel — new draw methods + 3-slice infrastructure

**Scope:** `jam_look_and_feel/theme/jam_look_and_feel_theme.h` + `.cpp`
**Action:**
- Add new virtual: `void drawTabButton (juce::Graphics&, juce::Button& button, bool isMouseOver, bool isMouseDown)` — takes `juce::Button&` (not the concrete TabButton type, following existing Group draw pattern with `juce::Component&`)
- Update existing virtuals: `drawButtonGroupSlidingIndicator` and `drawButtonGroupTrack` signatures stay as-is (Component&)
- Add 3-slice SVG infrastructure:
  - `void setTabSVG (const void* data, size_t size)` — parses SVG, extracts 6 named elements: `button-left`, `button-center`, `button-right`, `indicator-left`, `indicator-center`, `indicator-right`
  - Private: 6 `std::unique_ptr<juce::Drawable>` members for the SVG slices
  - Private: `void drawThreeSlice (juce::Graphics&, juce::Rectangle<float> bounds, juce::Drawable* left, juce::Drawable* center, juce::Drawable* right, bool filled)` — the 3-slice stretch algorithm: `scaleFactor = drawArea.height / svgRowHeight`, left cap + right cap fixed, center stretches
- drawTabButton implementation: if SVG set -> 3-slice with button slices (hover: filled, not hover: stroked); if no SVG -> fallback rounded rect
- drawButtonGroupSlidingIndicator implementation: if SVG set -> 3-slice with indicator slices; if no SVG -> existing rounded rect fallback (from kuassa)
- drawButtonGroupTrack: keep existing rounded rect implementation

**Validation:** LookAndFeel compiles. Draw methods callable with fallback rendering.

### Step 3: Extend button::Group

**Scope:** `jam_gui/button/jam_button_group.h`
**Action — sync from kuassa:**
- `addButton(std::unique_ptr<juce::Button>, bool isFreeButton = false)` — isFreeButton buttons excluded from radio group. Guard `ID::groupButton` property (already exists in jam_core, already set on line 142 — add the isFreeButton conditional).
- `valueChanged`, `snapIndicator`, `animateIndicator` — guard on `ID::groupButton` property (skip free buttons)

**Action — new API (SPEC-driven):**
- Selection model change: `juce::Value value` stores **int index** (not string). Breaking change from kuassa's string-based model.
- `void setCurrentIndex (int index)` — sets value
- `int getCurrentIndex () const noexcept` — reads value
- `void removeButton (int index)` — removes button at index, adjusts selection if needed
- `void moveButton (int from, int to)` — reorders button in `Owner<Button> buttons`, fires `onButtonMoved`
- `std::function<void (int, int)> onButtonMoved` — callback (fromIndex, toIndex)
- `std::function<void (int)> onButtonRightClicked` — callback (index)
- `SlidingIndicator` — z-ordered behind buttons: `indicator.toBack()` after addAndMakeVisible
- Keep `onValueChanged` callback — fires when index changes

**Source reference:** kuassa version at `kuassa_gui/button/kuassa_button_group.h`

**Validation:** Group compiles. Index-based selection works. isFreeButton exclusion works.

### Step 4: Create button::Options

**Scope:** `jam_gui/button/jam_button_options.h` (new file)
**Action:**
- Fork from `kuassa_gui/button/kuassa_button_options.h`
- Adapt to jam namespace: `jam::button::Options`
- Replace `kuassa::menu::showNative()` / `kuassa::menu::createOptions()` with standard `juce::PopupMenu`
- Core API:
  - `std::map<int, juce::String>` menu model
  - Owned trigger button (`std::unique_ptr<juce::Button>`)
  - `wireButton()` -> `button->onClick` shows PopupMenu async
  - `juce::Value selectedItem`
  - `std::function<void()> onValueChanged`
- Remove kuassa-specific binding (`bindToPanel`, `Registry&`, `Descriptor&`, `Model&`)
- Added to Group as free button (`isFreeButton = true`)

**Source reference:** kuassa version at `kuassa_gui/button/kuassa_button_options.h`

**Validation:** Options compiles. Popup menu appears on click. Selection updates selectedItem.

### Step 5: Create button::TabButton

**Scope:** `jam_gui/button/jam_button_tab.h` (new file)
**Action:**
- `jam::button::TabButton : public juce::Button`
- Drag-reorder: `mouseDown`/`mouseDrag`/`mouseUp`, `dragThreshold = 5`, calls `owner.moveButton(from, to)` on group
- Inline rename: `showRenameEditor()` — shows `juce::Label` overlay, `onRenameCommit` callback
- `paintButton` delegates to `LookAndFeel::Theme::drawTabButton(g, *this, isMouseOver, isMouseDown)`
- Constructor: `TabButton (const juce::String& name, button::Group& ownerGroup)`
- Public: `button::Group& getGroup() const noexcept`
- Private: `juce::Label label`, `bool isDragging { false }`, `int dragStartPos { 0 }`

**Source reference:** Old jam TabBarButton at `jam_gui/layout/jam_tab_bar_button.h` for drag-reorder and rename patterns (deleted in Step 1, reference from git). Adapt to work with Group instead of TabbedButtonBar.

**Validation:** TabButton compiles. paintButton delegates to theme. Drag handlers wire to Group::moveButton.

### Step 6: Rewrite TabbedComponent

**Scope:** `jam_gui/layout/jam_tabbed_component.h` + `.cpp` (new files, same paths as deleted)
**Action:**
- `jam::TabbedComponent : public juce::Component`
- Backed by `button::Group` (replaces TabbedButtonBar entirely)
- Owns: `button::Group group` (value member)
- Content area: `juce::Rectangle<int> getContentArea() const` — bounds minus tab bar depth
- Tab bar depth: `setTabBarDepth (int)`, `getTabBarDepth () const noexcept`, default 30
- Tab management: `addTab (const juce::String& name, juce::Colour, int insertIndex = -1)`, `removeTab (int index)`, `moveTab (int from, int to)`, `setTabName (int index, const juce::String&)`
- Selection: `setCurrentTabIndex (int, bool sendChangeMessage = true)`, `getCurrentTabIndex () const`
- Virtual: `currentTabChanged (int newIndex, const juce::String& newName)` — callback
- Virtual: `TabButton* createTabButton (const juce::String& name, int index)` — override point for custom buttons
- `resized()` — positions group at top (tab bar depth height), content area below
- Group's `onValueChanged` -> calls `currentTabChanged`
- Group's `onButtonMoved` -> fires through to TabbedComponent
- Tabs at top only (SPEC says nothing about orientation; END uses top only)

**Validation:** TabbedComponent compiles. addTab creates TabButtons in Group. Selection changes fire currentTabChanged. Content area geometry correct.

### Step 7: PaneManager resizer bar lifecycle fix

**Scope:** `jam_gui/layout/jam_pane_manager.h` + `.cpp`, `jam_gui/layout/jam_pane_resizer_bar.h`
**Action:**
- Root cause: `remove()` restructures the tree (promotes sibling, collapses nodes). Resizer bars hold reference to old split node which becomes detached.
- Fix: resizer bar lifetime RAII-bound to the split node. `layout()` template creates/rebinds resizer bars as part of layout computation — not post-hoc scan.
  - `layoutNode()` template: when encountering a PANES node with 2 children, check if a matching resizer bar exists in `resizerBars`. If not, create one. If the existing bar's splitNode doesn't match, rebind or replace.
  - `remove()` does NOT manage resizer bars — it only restructures the ValueTree. The next `layout()` call naturally creates/destroys bars to match the new tree shape.
  - Bar creation paired with layout, bar destruction via Owner cleanup when bars no longer match any split node.

**Validation:** After remove(), layout() produces correct resizer bar set. No orphan bars. No stale splitNode references.

### Step 8: Module header + aggregator + doxygen + verify

**Scope:** `jam_gui/jam_gui.h`, `jam_gui/jam_gui.cpp`, all new files
**Action:**
- Update `jam_gui.h` include order:
  - Layout section: keep `jam_pane_resizer_bar.h`, `jam_pane_manager.h`. Add new `jam_tabbed_component.h`
  - Button section: add `jam_button_options.h`, `jam_button_tab.h` alongside existing `jam_button_group.h`
  - Ensure include order respects dependencies: button_group before button_options and button_tab, all before tabbed_component
- Update `jam_gui.cpp`: add new .cpp TU includes (TabbedComponent .cpp; Options and TabButton likely header-only)
- Doxygen: `@file` blocks, `@brief`, `@param` on all public methods. Zero warnings.
- Verify: jam compiles clean. No warnings.

**Validation:** Full @Auditor pass — MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md, this PLAN.

## BLESSED Alignment

- **B (Bound):** Group owns buttons via `Owner<Button>`. TabbedComponent owns Group. Resizer bars RAII-bound to layout tree.
- **L (Lean):** Each component one file. No god objects. Group, Options, TabButton, TabbedComponent — clean decomposition.
- **E (Explicit):** Index-based selection (no string matching). Named constants for drag threshold. Assert preconditions.
- **S (SSOT):** Group is the single selection authority. PaneManager ValueTree is the single layout truth.
- **S (Stateless):** TabButton is a dumb button — delegates paint to theme, drag to group.
- **E (Encapsulation):** drawTabButton takes `juce::Button&` — no type leakage across layers. Options wireButton encapsulates popup logic.
- **D (Deterministic):** Same index -> same button selected. Same SVG -> same rendering.

## Risks / Open Questions

1. **Orientation support:** SPEC mentions only `button::Group` horizontal layout and tabs-at-top. Old TabbedComponent supported 4 orientations. Plan assumes top-only. If other orientations needed later, Group::makeRow would need a vertical variant.

2. **Pabrik breaking change:** button::Group selection changes from string to int index. Pabrik (`dev/pabrik/Source/view/Manager.cpp:41`) uses `jam::button::Group`. The index-based change may break pabrik's value binding.

3. **SVG asset for Phase 2 testing:** The 3-slice algorithm is implemented in LookAndFeel but no default SVG exists at the framework level. Rendering uses fallback (rounded rect) until an application provides SVG via `setTabSVG()`. Full SVG rendering testable at Phase 3 when END wires BinaryData.
