# RFC — jam::TabbedComponent + Button::Group Integration

Date: 2026-05-02
Status: Ready for COUNSELOR handoff

## Problem Statement

END's tab system uses `jam::TabbedComponent` → `jam::TabbedButtonBar` → `jam::TabBarButton` (all forked from JUCE). The tab strip works but lacks the sliding indicator animation that `jam::button::Group` provides. ARCHITECT wants to unify: Button::Group becomes the tab strip, bringing its animated selection indicator to tabs while absorbing TabBarButton's drag-reorder and inline rename capabilities.

Additionally, `jam::button::Group` is out of date vs kuassa's latest (missing `isFreeButton` parameter, `ID::groupButton` property guards).

## Research Summary

### Current Architecture (jam)

| Class | LOC | Role |
|---|---|---|
| `jam::TabbedComponent` | 176h | Content-panel host, owns tab bar, delegates everything |
| `jam::TabbedButtonBar` | 244h | Button container, layout via `updateTabPositions`, drag/reorder callbacks |
| `jam::TabBarButton` | 143h | Individual tab: drag-reorder, inline rename, LookAndFeel delegation |
| `jam::button::Group` | 192h | Radio button strip with sliding indicator animation, Value-based state |

### Key Findings

1. **TabbedButtonBar duplicates what Group already does** — radio selection, layout, active-state tracking. But adds: overflow menu (`extraTabsButton`), `BehindFrontTabComp`, `updateTabPositions` compression algorithm (130 lines). Group uses `FlexBox::makeRow` — simpler, sufficient for tabs.

2. **TabbedComponent is thin** — ~120 lines of real content-panel logic. The rest is delegation to TabbedButtonBar. Already stripped of JUCE's content-component-in-addTab pattern.

3. **Group's Value IS the active tab state** — `juce::Value value` holds current selection. `valueChanged` manages radio toggle + indicator animation. This maps directly to "which tab is active."

4. **jam::button::Group is behind kuassa** — missing `isFreeButton` parameter in `addButton()`, missing `ID::groupButton` property guards in `valueChanged`/`snapIndicator`/`animateIndicator`.

5. **END's SVG 3-slice rendering** — loads 6 SVG paths (active/inactive × left/center/right) from user config, renders via LookAndFeel. Currently wired to `TabbedButtonBar::LookAndFeelMethods`. Must migrate to Group's drawing surface.

6. **Drag-reorder** — currently in `jam::TabBarButton` (mouse handling, calls `owner.moveTab`). Must move to Group's button subclass.

7. **Inline rename** — currently in `jam::TabBarButton` (`showRenameEditor`, `onRenameCommit`). Must move to Group's button subclass.

8. **Content ownership** — END uses `jam::Owner<Panes> panes` on `Terminal::Tabs`. Content is NOT managed by TabbedComponent — it's already external. TabbedComponent just needs to signal tab changes.

### kuassa vs jam Button::Group Delta

| Feature | kuassa | jam |
|---|---|---|
| `addButton(unique_ptr, bool isFreeButton)` | ✓ | ✗ (no isFreeButton param) |
| `ID::groupButton` property on group buttons | ✓ | ✗ (all buttons treated as group) |
| Property guard in `valueChanged` | ✓ | ✗ |
| Property guard in `snapIndicator` | ✓ | ✗ |
| Property guard in `animateIndicator` | ✓ | ✗ |
| `indicator.toBack()` after add | ✓ (line 159) | ✓ (line 147) |
| JUCE_MODULE conditional for data_structures | ✓ | ✓ |

## Principles and Rationale

**Why Group replaces TabbedButtonBar (not wraps it):**

- **S (SSOT):** Two radio-selection-with-active-state mechanisms in the same framework is duplication. Group IS the pattern.
- **L (Lean):** TabbedButtonBar's `updateTabPositions` (130 lines of compression/overflow logic) replaced by `FlexBox::makeRow` (1 call). `BehindFrontTabComp` replaced by `SlidingIndicator`.
- **B (Bound):** Group owns buttons via `Owner<juce::Button>` (RAII). TabbedButtonBar uses `OwnedArray<TabInfo>` with indirect ownership. Group's model is cleaner.
- **E (Encapsulation):** TabbedComponent tells Group "select tab N" via Value. Group handles animation internally. No orchestrator state tracking.

**Why fork TabbedComponent (not fight externally):**

- TabbedComponent's `tabs` member is `std::unique_ptr<TabbedButtonBar>` — can't swap to Group without forking.
- External composition (hide bar, wire externally) creates shadow state between two selection mechanisms. **S (SSOT) violation.**
- The fork is 120 lines of real logic. Maintenance cost is negligible.

## Scaffold

### Phase 1: Update jam::button::Group from kuassa

Sync `jam_button_group.h` to kuassa's latest. Changes:
- `addButton(std::unique_ptr<juce::Button>, bool isFreeButton = false)`
- `ID::groupButton` property guards in `valueChanged`, `snapIndicator`, `animateIndicator`
- Add `removeButton(int index)` (new — needed for dynamic tabs, not in kuassa)

### Phase 1.5: Fork jam::button::Options from kuassa

Fork `kuassa::button::Options` → `jam::button::Options`. Strips:
- `bindToPanel` / Registry / Descriptor (kuassa plugin panel infrastructure)
- AU-specific native menu codepath (`#if JUCE_MAC and JucePlugin_Build_AU`)

Keeps:
- `std::map<int, String>` menu model
- `std::unique_ptr<juce::Button>` owned trigger button
- `wireButton()` → `onClick` shows `PopupMenu` async
- `juce::Value selectedItem` + `onValueChanged` callback
- `PopupDirection` enum
- `Value::ObjectID<Options>` CRTP

Group integration: added via `addButton(std::move(optionsButton), true /* isFreeButton */)`. Group's `resized()` checks total tab-button width vs bounds — if overflow, shows Options button and hides clipped tabs. Options map rebuilt on every add/remove/resize with entries for hidden tabs.

### Phase 2: New jam::button::TabButton

```cpp
namespace jam::button
{
class TabButton : public juce::Button
{
public:
    TabButton (const juce::String& name, Group& ownerGroup);

    // --- Drag-reorder (migrated from jam::TabBarButton) ---
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // --- Inline rename (migrated from jam::TabBarButton) ---
    void showRenameEditor();
    juce::Label& getLabel() noexcept;
    std::function<void (const juce::String&)> onRenameCommit;

    // --- Drawing delegation ---
    void paintButton (juce::Graphics&, bool isMouseOver, bool isMouseDown) override;

    Group& getOwnerGroup() const noexcept { return owner; }
    int getIndex() const;
    bool isFrontTab() const;

private:
    Group& owner;
    juce::Label label;
    static constexpr int dragThreshold { 5 };
    bool isDragging { false };
    int dragStartPos { 0 };
};
}
```

### Phase 3: Extend jam::button::Group for Tab Use

```cpp
namespace jam::button
{
class Group : public juce::Component
            , public Value::ObjectID<Group>
            , public juce::Value::Listener
{
public:
    // ... existing interface ...

    // --- New: tab-specific extensions ---
    void removeButton (int index);
    void moveButton (int fromIndex, int toIndex);

    // --- Callbacks ---
    std::function<void (int, int)> onButtonMoved;       // drag reorder
    std::function<void (int)> onButtonRightClicked;     // context menu

    // --- Orientation (new for vertical tabs) ---
    enum Orientation { Horizontal, Vertical };
    void setOrientation (Orientation newOrientation);

    // --- Index-based selection (supplement to string-based Value) ---
    void setCurrentIndex (int index);
    int getCurrentIndex() const noexcept;
};
}
```

### Phase 4: Rewrite jam::TabbedComponent

```cpp
namespace jam
{
class TabbedComponent : public juce::Component
{
public:
    explicit TabbedComponent (button::Group::Orientation orientation);
    ~TabbedComponent() override;

    // --- Tab management (delegates to Group) ---
    void addTab (const juce::String& tabName, int insertIndex = -1);
    void removeTab (int tabIndex);
    void moveTab (int currentIndex, int newIndex);
    void setTabName (int tabIndex, const juce::String& newName);
    void clearTabs();

    int getNumTabs() const;
    juce::StringArray getTabNames() const;

    // --- Selection (delegates to Group's Value) ---
    void setCurrentTabIndex (int index, bool sendChangeMessage = true);
    int getCurrentTabIndex() const;
    juce::String getCurrentTabName() const;

    // --- Bar configuration ---
    void setTabBarDepth (int newDepth);
    int getTabBarDepth() const noexcept;
    void setOrientation (button::Group::Orientation orientation);

    // --- Content area (host manages panels, this provides geometry) ---
    juce::Rectangle<int> getContentArea() const;

    // --- Override points ---
    virtual void currentTabChanged (int newIndex, const juce::String& newName);
    virtual std::unique_ptr<button::TabButton> createTabButton (const juce::String& name, int index);

    // --- Access ---
    button::Group& getTabBar() noexcept { return tabBar; }

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    button::Group tabBar;

private:
    int tabDepth { 30 };
};
}
```

### Phase 5: LookAndFeel Surface

Group's existing drawing methods become the tab rendering surface:

```cpp
// In jam::LookAndFeel::Theme (or Terminal::LookAndFeel)
struct Theme
{
    // --- Existing Group methods (become tab methods) ---
    virtual void drawButtonGroupSlidingIndicator (juce::Graphics&, juce::Component& indicator) = 0;
    virtual void drawButtonGroupTrack (juce::Graphics&, juce::Component& group) = 0;

    // --- New: tab-specific drawing ---
    virtual void drawTabButton (button::TabButton&, juce::Graphics&,
                                bool isMouseOver, bool isMouseDown) = 0;
    virtual juce::Font getTabButtonFont (button::TabButton&, float height) = 0;
    virtual int getTabButtonBestWidth (button::TabButton&, int barDepth) = 0;
};
```

END's `Terminal::LookAndFeel` implements these — migrates existing SVG 3-slice rendering from `LookAndFeelTab.cpp` into `drawTabButton`. The `SlidingIndicator` paint uses `drawButtonGroupSlidingIndicator` — renders the indicator 3-slice (from `Source/resources/default_tab_button.svg` or user override). No more parallelogram.

### Phase 6: END Migration (Terminal::Tabs)

```cpp
class Tabs : public jam::TabbedComponent
           , private juce::FocusChangeListener
           , private juce::Value::Listener
{
    // tabBar.onValueChanged wires to content switching (Panes visibility)
    // tabBar.onButtonMoved wires to panes reorder + AppState update
    // jam::Owner<Panes> panes — unchanged
};
```

Key wiring in constructor:
```cpp
tabBar.onValueChanged = [this] { switchToActivePane(); };
tabBar.onButtonMoved = [this] (int from, int to) { reorderPanes (from, to); };
```

### Deletion List

After migration, remove from jam:
- `jam_gui/layout/jam_tabbed_button_bar.h` + `.cpp`
- `jam_gui/layout/jam_tab_bar_button.h` + `.cpp`
- `jam_gui/layout/jam_tabbed_component.cpp` (rewritten)
- `jam_gui/layout/jam_tabbed_component.h` (rewritten)

## BLESSED Compliance Checklist

- [x] **B (Bound)** — `Owner<juce::Button>` for buttons, strict RAII. TabbedComponent owns Group by value. Content panels owned by host via `Owner<Panes>`.
- [x] **L (Lean)** — Eliminates 592 lines (TabbedButtonBar) + 143 lines (TabBarButton as separate class). Group + TabButton replaces both. FlexBox::makeRow replaces 130-line updateTabPositions.
- [x] **E (Explicit)** — Value-based selection, no hidden index tracking. Tab change fires through single `onValueChanged` path. No early returns.
- [x] **S (SSOT)** — Group's `juce::Value` is THE active-tab truth. No shadow index. No dual selection mechanism.
- [x] **S (Stateless)** — Group is a dumb worker. It animates on tell (value change). TabbedComponent tells, Group executes.
- [x] **E (Encapsulation)** — TabButton handles its own drag/rename internally. Group handles its own animation. TabbedComponent only knows "tab changed."
- [x] **D (Deterministic)** — Same value → same indicator position → same content panel. Follows from BLESSE compliance.

## Open Questions

All resolved. Decisions locked:

1. **Tab overflow** — `jam::button::Options` (forked from kuassa) added as a free button (`isFreeButton = true`) in Group. When total tab-button width exceeds Group bounds, overflow Options button appears with hidden tabs as popup menu items. Selecting from popup sets Group's Value (switches tab). Options is a popup-menu-on-click component driven by `std::map<int, String>`.

2. **Index-based selection** — Group's `juce::Value` stores int index (not string). `button::TabButton` matches itself by index. `juce::var` stores anything — no architectural change to Group's Value mechanism, just what goes in it.

3. **removeButton fallback** — select previous tab (wraps to last if removing first). Matches current END behavior.

## Handoff Notes

- **Prerequisite:** jam::button::Group must be synced from kuassa FIRST (Phase 1). All subsequent phases depend on the isFreeButton/groupButton property infrastructure.
- **Migration is incremental** — Phases 1–3 are jam-internal (no END changes). Phase 4 rewrites TabbedComponent. Phase 5–6 migrates END. Each phase is independently testable.
- **Existing tests:** None found for tab system. Manual verification via END's tab behavior (add/remove/reorder/rename/SVG rendering).
- **LookAndFeel overlap:** END currently implements BOTH `juce::TabbedButtonBar::LookAndFeelMethods` AND `jam::TabbedButtonBar::LookAndFeelMethods`. After migration, only Group's Theme methods remain. Significant simplification.
- **`indicator.toBack()`** — Group's indicator renders BEHIND buttons. For tabs, this means the active highlight is a background glow/fill behind the active tab shape. This matches END's current behavior (active tab = filled shape on track).

---

## Appendix A: SVG Tab Button Design

### Architecture Change

**Old model (TabbedButtonBar):** Each tab button draws itself with active/inactive variants. No separate moving part.

**New model (Button::Group):** Two distinct visual layers:
1. **Tab buttons** — always draw the same shape (the "track" / resting state)
2. **SlidingIndicator** — a separate Component that animates to the active button's bounds, z-ordered BEHIND buttons (`indicator.toBack()`)

The animation of the indicator sliding between positions IS the tab-switch visual effect.

### Visual Layer Stack (back → front)

| Z-order | Component | Draws | LookAndFeel method |
|---|---|---|---|
| 1 (back) | Group itself | Bar background / track | `drawButtonGroupTrack` |
| 2 | SlidingIndicator | Active highlight (moves) | `drawButtonGroupSlidingIndicator` |
| 3 (front) | TabButton × N | Static tab shape | `drawTabButton` |

### SVG Element Layout

Single SVG file, 6 elements (same count as before, renamed):

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 [width] [height]">
  <!-- Tab button shape (static, drawn on every tab) -->
  <g id="button-left">...</g>
  <g id="button-center">...</g>
  <g id="button-right">...</g>

  <!-- Indicator shape (animated, drawn behind active tab) -->
  <g id="indicator-left">...</g>
  <g id="indicator-center">...</g>
  <g id="indicator-right">...</g>
</svg>
```

### 3-Slice Rendering (unchanged algorithm)

Both button and indicator use the same 3-slice stretch:
1. Compute `scaleFactor = drawArea.height / svgRowHeight`
2. Left cap: fixed proportional width, placed at left edge
3. Right cap: fixed proportional width, placed at right edge
4. Center: stretches to fill remaining width

This handles variable tab widths (text-dependent via `getTabButtonBestWidth`).

### Drawing Rules

**TabButton::paintButton:**
- Always renders `button-{left,center,right}` 3-slice
- Hovered: filled (highlight colour)
- Not hovered: stroked (outline only)
- No active/inactive distinction — the indicator handles that

**SlidingIndicator::paint:**
- Renders `indicator-{left,center,right}` 3-slice
- Always filled (active colour)
- Bounds set by Group to match active button's bounds
- Animated via `ComponentAnimator` (120ms default)

**Group::paint (track):**
- Optional bar background (can be transparent)

### Default Fallback

**No more parallelogram.** Default is an embedded binary SVG compiled into the application from `Source/resources/default_tab_button.svg`. Shipped as `BinaryData::default_tab_button_svg`. Loaded at LookAndFeel construction, overridden at runtime if user provides `display.tab.button_svg` in config.

Loading priority:
1. User config `display.tab.button_svg` path → load from file
2. Fallback → `BinaryData::default_tab_button_svg` (compiled from `Source/resources/default_tab_button.svg`)

### Config Interface

```lua
-- end.lua
display = {
    tab = {
        family = "JetBrains Mono",
        size = 11,
        -- Single SVG file with both button and indicator elements
        -- Omit or set empty to use built-in default
        button_svg = "~/.config/end/themes/my-tabs.svg",
    },
}
```

Key renamed from `button_svg` (same name, but new element IDs inside the SVG). User SVGs designed for the old system (active/inactive) will fail gracefully — `hasSvgTabButton` stays false, falls back to embedded default.

### Migration from Old System

| Old | New | Notes |
|---|---|---|
| `active-left/center/right` | `indicator-left/center/right` | Was button-draws-itself-active → now indicator draws active |
| `inactive-left/center/right` | `button-left/center/right` | Was button-draws-itself-inactive → now button always draws this |
| Parallelogram fallback | Embedded SVG fallback | ARCHITECT provides default SVG as BinaryData |
| `hasSvgTabButton` bool | Always true (embedded guarantees availability) | Simplifies drawing path — no conditional |
| 6 cached `juce::Path` members | 6 cached `juce::Path` members | Same count, renamed |

### Deleted Code

- `getTabButtonShape()` (parallelogram generator)
- `getTabButtonIndicator()` (left-edge indicator parallelogram)
- All active/inactive branching in `drawTabButtonCore`
- The `else` fallback path in `drawTabButtonCore`
