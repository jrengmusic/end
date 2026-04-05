# PLAN: Action::List Refactor — Value-Driven Row Selection

**Date:** 2026-04-05
**Status:** Pending ARCHITECT approval
**Goal:** Eliminate manual state tracking. Row selection driven by ValueTree. One Row type for all indices.

---

## Problem

Current Action::List has manual orchestration overhead:
- `selectedIndex` / `insertMode` as ValueTree properties with getter/setter wrappers
- `enterInsertMode()` / `enterNormalMode()` — manual mode switching
- `setHighlighted()` — List tells Row to highlight from outside
- Focus management — List manually calls `grabKeyboardFocus` / `giveAwayKeyboardFocus`
- KeyHandler tracks `isInsertMode` callback — redundant with selection state
- Row 0 (search) is not a Row — it's a separate `juce::TextEditor` member on List

All symptoms of not trusting the data model. BLESSED violations: Encapsulation (poking Row internals), Stateless (manual bool flags), SSOT (selection truth scattered).

## Solution

Each Row is a `jreng::Value::ObjectID<Row>`. Its Value = `selected` (bool). ValueTree owns all selection state. Row reacts to its own Value — paints highlight, manages its own focus. List sets one Value to true, the rest handle themselves.

Row 0 is the same Row class. Index 0 = contains TextEditor (search). Index > 0 = contains NameLabel + ShortcutLabel. Same selection mechanism, different content.

---

## Step 1: Row becomes Value-aware

**File: `Source/action/ActionRow.h`**

```cpp
class Row : public juce::Component,
            public jreng::Value::ObjectID<Row>,
            private juce::Value::Listener
```

Members:
```cpp
public:
    Row (int index, const juce::String& uuid);
    Row (int index, const juce::String& uuid, const Registry::Entry& entry);

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Value& getValueObject() noexcept override;

    juce::Colour  highlightColour;
    juce::String  actionConfigKey;
    std::function<bool()>     run;
    std::function<void(Row*)> onShortcutClicked;

    int getIndex() const noexcept;
    bool isSelected() const noexcept;
    juce::TextEditor* getSearchBox();

private:
    static constexpr int shortcutWidthDivisor { 3 };

    int            rowIndex;
    juce::Value    selected;

    // Row 0 content
    std::unique_ptr<juce::TextEditor> searchBox;

    // Row > 0 content
    std::unique_ptr<NameLabel>     nameLabel;
    std::unique_ptr<ShortcutLabel> shortcutLabel;

    void valueChanged (juce::Value& value) override;
```

**Key design decisions:**
- `ObjectID<Row>` sets componentID from the uuid via CRTP constructor
- `getValueObject()` returns `selected` — this is what `jreng::ValueTree::attach` wires
- `valueChanged()` — Row listens to its own Value. When `selected` becomes true: Row paints highlight and grabs focus (searchBox focus for index 0, component focus for index > 0). When false: clears highlight.
- Row 0 constructor (2-arg): creates TextEditor, no entry needed
- Row > 0 constructor (3-arg): creates NameLabel + ShortcutLabel from entry
- `NameLabel` / `ShortcutLabel` as unique_ptr — only created for index > 0
- `searchBox` as unique_ptr — only created for index 0
- `setHighlighted()` API eliminated — Row manages its own visual state
- `onExecuted` callback eliminated — List handles close-on-run in `executeSelected()`
- `mouseDoubleClick` stays on Row > 0 for shortcut remap

**File: `Source/action/ActionRow.cpp`**

Row 0 constructor:
```cpp
Row::Row (int index, const juce::String& uuid)
    : ObjectID<Row> (uuid)
    , rowIndex (index)
{
    searchBox = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (searchBox.get());
    selected.addListener (this);
}
```

Row > 0 constructor:
```cpp
Row::Row (int index, const juce::String& uuid, const Registry::Entry& entry)
    : ObjectID<Row> (uuid)
    , rowIndex (index)
{
    nameLabel = std::make_unique<NameLabel>();
    nameLabel->setText (entry.name, juce::dontSendNotification);
    nameLabel->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel.get());

    shortcutLabel = std::make_unique<ShortcutLabel>();
    shortcutLabel->setText (Registry::shortcutToString (entry.shortcut), juce::dontSendNotification);
    shortcutLabel->setComponentID ("shortcut");
    shortcutLabel->setJustificationType (juce::Justification::centredRight);
    shortcutLabel->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (shortcutLabel.get());

    actionConfigKey = Registry::configKeyForAction (entry.id);
    run = entry.execute;

    selected.addListener (this);
}
```

`valueChanged`:
```cpp
void Row::valueChanged (juce::Value& value)
{
    if (value.refersToSameSourceAs (selected))
    {
        repaint();

        if (isSelected())
        {
            if (searchBox != nullptr)
                searchBox->grabKeyboardFocus();
            else
                grabKeyboardFocus();
        }
    }
}
```

`paint`:
```cpp
void Row::paint (juce::Graphics& g)
{
    if (isSelected() and searchBox == nullptr)
    {
        g.setColour (highlightColour);
        g.fillRect (getLocalBounds());
    }
}
```

Row 0 does not paint highlight — the cursor in the TextEditor IS the visual indicator.

`isSelected`:
```cpp
bool Row::isSelected() const noexcept
{
    return static_cast<bool> (selected.getValue());
}
```

`resized`:
```cpp
void Row::resized()
{
    if (searchBox != nullptr)
    {
        searchBox->setBounds (getLocalBounds());
    }
    else if (nameLabel != nullptr and shortcutLabel != nullptr)
    {
        auto bounds { getLocalBounds() };
        const int shortcutWidth { bounds.getWidth() / shortcutWidthDivisor };
        nameLabel->setBounds (bounds.removeFromLeft (bounds.getWidth() - shortcutWidth));
        shortcutLabel->setBounds (bounds);
    }
}
```

---

## Step 2: List simplified

**File: `Source/action/ActionList.h`**

Remove:
- `selectedIndexId`, `insertModeId` static identifiers
- `getSelectedIndex()`, `setSelectedIndex()`, `isInsertMode()`, `setInsertMode()` accessors
- `enterInsertMode()`, `enterNormalMode()`
- `juce::TextEditor searchBox` member — moved into Row 0
- `setupSearchBox()` method
- `handleValueChanged()` — replaced with ValueTree-driven approach

Add:
- `void selectRow (int index)` — clears all, sets one (already exists, simplified)
- `Row* getRow (int index)` — helper, returns `rows[index].get()` (or null)
- `Row* getSelectedRow()` — walks rows for selected

Simplified members:
```cpp
    jreng::ValueTree           state { "ACTION_LIST" };
    Action::LookAndFeel        lookAndFeel;
    juce::Viewport             viewport;
    juce::Component            rowContainer;
    jreng::Owner<Action::Row>  rows;

    KeyRemapDialog remapDialog;

    juce::Component&           callerRef;
    std::optional<KeyHandler>  keyHandler;
```

**File: `Source/action/ActionList.cpp`**

Constructor changes:
- Row 0 created first: `auto row0 = std::make_unique<Row>(0, uuid)` — the search row
- Row 0's TextEditor configured here (colours, font, placeholder, `setEscapeAndReturnKeysConsumed(false)`)
- Row 0's `searchBox->onTextChange` wired to `filterRows()`
- Rows 1+ created from `Registry::getEntries()`
- All rows: `jreng::ValueTree::attach(state, row.get())` — wires each row's `selected` Value
- `state.onValueChanged` wired to handle shortcut changes (same as current `handleValueChanged`)
- `selectRow(0)` to start with search selected

`selectRow` simplified:
```cpp
void List::selectRow (int index)
{
    for (auto& row : rows)
        row->getValueObject().setValue (false);

    if (index >= 0 and index < static_cast<int> (rows.size()))
    {
        if (rows[index]->isVisible())
            rows[index]->getValueObject().setValue (true);
    }
}
```

That's it. No highlight management, no focus management, no mode tracking. Row handles itself via `valueChanged`.

`filterRows` simplified — no mode tracking:
```cpp
void List::filterRows (const juce::String& query)
{
    // Row 0 always visible.
    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        // filter logic: visible if matches or query empty
    }

    layoutRows();
    selectRow (0);
}
```

`executeSelected`:
```cpp
void List::executeSelected()
{
    // Find selected row. If index 0, execute row 1 (first visible action).
    // If index > 0, execute that row.
}
```

---

## Step 3: KeyHandler simplified

**File: `Source/action/KeyHandler.h`**

Callbacks struct simplified:
```cpp
struct Callbacks
{
    std::function<void()>      executeSelected;
    std::function<void()>      dismiss;
    std::function<void(int)>   selectRow;
    std::function<int()>       visibleRowCount;
    std::function<int()>       selectedIndex;
};
```

Removed: `enterInsertMode`, `enterNormalMode`, `isInsertMode`.

**File: `Source/action/KeyHandler.cpp`**

Vim insert table (selected == 0):
- Escape → if visibleRowCount > 0: `selectRow(1)`, else dismiss
- Return → executeSelected

Vim normal table (selected > 0):
- j → `selectRow(selected + 1)`
- k → `selectRow(selected - 1)` — when result is 0, search gets focus automatically via Row's valueChanged
- i → `selectRow(0)`
- Escape → dismiss
- Return → executeSelected

Arrow table:
- Down → `selectRow(selected + 1)`
- Up → `selectRow(selected - 1)`
- Escape → dismiss
- Return → executeSelected

`handleVimKey` mode check: `callbacks.selectedIndex() == 0` replaces `isInsertMode()`.

---

## Step 4: SearchBox configuration

Row 0's TextEditor needs the same styling as the current `setupSearchBox()`. Move the styling into `buildRows()` after creating Row 0:

```cpp
auto* searchBox { row0->getSearchBox() };
jassert (searchBox != nullptr);

searchBox->setMultiLine (false);
searchBox->setReturnKeyStartsNewLine (false);
// ... all current setupSearchBox() logic
searchBox->setEscapeAndReturnKeysConsumed (false);
searchBox->onTextChange = [this, searchBox] { filterRows (searchBox->getText()); };
```

---

## Step 5: LookAndFeel update

`Action::LookAndFeel::getLabelFont` — no changes needed. `dynamic_cast<NameLabel*>` and `dynamic_cast<ShortcutLabel*>` still work since those types exist as children of Row > 0.

---

## Step 6: Colour configuration

Row colours set in `buildRows()`:
- Row 0: TextEditor colours from config (same as current)
- Row > 0: `highlightColour` from `getHighlightColour()`, label colours from config

---

## Step 7: Visibility and filtering

`filterRows` operates on rows[1..N] only. Row 0 is always visible.

`visibleRowCount` counts visible rows in rows[1..N] only — Row 0 is not an "action" row.

`layoutRows`: Row 0 at top (fixed height), then visible action rows below in viewport.

Wait — layout concern: Row 0 is inside the viewport? Or outside?

**Design decision:** Row 0 (search) should be OUTSIDE the viewport, fixed at top. Action rows scroll below. This matches Spotlight behavior — search bar doesn't scroll.

So Row 0 is added to `content` directly (not rowContainer). Rows 1+ are in rowContainer inside viewport.

This means Row 0 is NOT in the `rows` vector alongside action rows. OR it is in the vector at index 0 but laid out separately.

Simplest: Row 0 is in `rows[0]`. `layoutRows` skips index 0. `resized()` places `rows[0]` at top of content, viewport below.

```cpp
void List::resized()
{
    juce::DocumentWindow::resized();

    if (auto* content { getContentComponent() }; content != nullptr)
    {
        auto bounds { content->getLocalBounds() };
        // apply padding
        bounds.removeFromTop (paddingTop);
        bounds.removeFromRight (paddingRight);
        bounds.removeFromBottom (paddingBottom);
        bounds.removeFromLeft (paddingLeft);

        // Row 0 = search, fixed at top
        if (not rows.empty())
            rows[0]->setBounds (bounds.removeFromTop (rowHeight));

        viewport.setBounds (bounds);
        remapDialog.setBounds (content->getLocalBounds());
        layoutRows();
    }
}
```

`layoutRows` starts from index 1:
```cpp
void List::layoutRows()
{
    const int rowWidth { viewport.getWidth() - viewport.getScrollBarThickness() };
    int yPos { 0 };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows[i]->isVisible())
        {
            rows[i]->setBounds (0, yPos, rowWidth, rowHeight);
            yPos += rowHeight;
        }
    }

    rowContainer.setSize (rowWidth, yPos);
}
```

Row 0 is added to `content` directly. Rows 1+ are added to `rowContainer`.

---

## Step 8: selectRow index mapping

`selectRow(0)` = search row. `selectRow(1)` = first action row. Direct index into `rows` vector.

When navigating with j/k, need to skip hidden (filtered-out) rows:
- `selectRow` needs to find the next/previous VISIBLE row
- Or: `selectNextVisible(direction)` helper

Actually, KeyHandler calls `selectRow(selectedIndex + 1)`. List's `selectRow` should handle visibility:

```cpp
void List::selectRow (int index)
{
    // Clamp and find nearest visible row in requested direction.
    int target { juce::jlimit (0, static_cast<int> (rows.size()) - 1, index) };

    // If target is hidden (filtered), search forward/backward for visible.
    // Row 0 is always visible.

    for (auto& row : rows)
        row->getValueObject().setValue (false);

    if (rows[target]->isVisible())
        rows[target]->getValueObject().setValue (true);
}
```

---

## Step 9: handleValueChanged for shortcut remap

`state.onValueChanged` fires when ANY attached Value changes — both `selected` Values and `shortcutLabel` text Values.

Need to distinguish: was it a selection change or a shortcut edit?

Selection changes: `selected` Values on rows. These are row-level Values.
Shortcut changes: `shortcutLabel` text Values on rows > 0. These are child-component Values.

Both fire through the same `onValueChanged`. The current `handleValueChanged` approach works: walk rows, compare shortcutLabel text vs config, patch on diff. Selection changes don't affect shortcut text, so the diff check is a no-op for selection events.

Keep current `handleValueChanged` logic.

---

## Execution Order

1. **Step 1: ActionRow.h/cpp** — Row becomes ObjectID, two constructors, self-managing selection
2. **Step 2: ActionList.h/cpp** — Remove manual state, simplify selectRow, move searchBox to Row 0
3. **Step 3: KeyHandler.h/cpp** — Simplify Callbacks, remove mode tracking
4. **Step 4: SearchBox config** — Styling in buildRows
5. **Step 5: LookAndFeel** — Verify, no changes expected
6. **Step 6: Colours** — Applied in buildRows
7. **Step 7: Layout** — Row 0 fixed, rows 1+ in viewport
8. **Step 8: Navigation** — Visibility-aware selectRow
9. **Step 9: ValueTree handler** — Verify shortcut remap still works

Steps 1-3 are the core refactor. Steps 4-9 are wiring and verification.

---

## Files Modified

| File | Change |
|---|---|
| `Source/action/ActionRow.h` | Rewrite — ObjectID mixin, two constructors, self-managing |
| `Source/action/ActionRow.cpp` | Rewrite — two constructors, valueChanged, paint, resized |
| `Source/action/ActionList.h` | Significant edit — remove manual state, simplify API |
| `Source/action/ActionList.cpp` | Significant edit — simplified selectRow, buildRows, layout |
| `Source/action/KeyHandler.h` | Edit — simplified Callbacks struct |
| `Source/action/KeyHandler.cpp` | Edit — remove mode callbacks, use selectedIndex == 0 |

## Files Unchanged

| File | Reason |
|---|---|
| `Source/action/Action.h/cpp` | Registry unaffected |
| `Source/action/KeyRemapDialog.h/cpp` | Inline overlay unaffected |
| `Source/action/LookAndFeel.h/cpp` | Font dispatch unaffected |
| `Source/MainComponent.cpp` | Creation pattern unchanged |
| `Source/config/Config.h/cpp` | Config keys unchanged |
| `Source/config/default_end.lua` | Config template unchanged |

---

## BLESSED Compliance

| Pillar | Before | After |
|---|---|---|
| **Bound** | List owns rows but pokes their internals | List owns rows, Row manages itself |
| **Lean** | ActionList.cpp 465 lines, manual orchestration | Reduced — no mode methods, no highlight management |
| **Explicit** | `insertMode` bool hides meaning | `selectedIndex == 0` is self-documenting |
| **SSOT** | Selection scattered: tree property + Row highlight + focus state | One truth: Row's `selected` Value on the tree |
| **Stateless** | Manual bool flags (`insertMode`, `highlighted`) | No flags — Value is the state, Row is reactive |
| **Encapsulation** | List pokes `row->setHighlighted()`, `row->highlightColour` | Row manages its own visual state via valueChanged |
| **Deterministic** | Focus depends on which method was called last | Focus follows Value — same Value = same focus |
