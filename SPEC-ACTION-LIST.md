# SPEC — Action::List (Command Palette)

**Date:** 2026-04-04
**Status:** Active
**Source:** RECOMMENDATION-ACTION-LIST.md

---

## Overview

Build the command palette for END. Two-phase delivery:
1. Restructure `Terminal::Action` to `Action::Registry` (namespace + file move)
2. Implement `Action::List`, `Action::Row`, `Action::KeyRemapDialog`, `Action::LookAndFeel`

---

## BLESSED Compliance

| Pillar | Application |
|---|---|
| **Bounds** | `Action::List` owns ValueTree and all rows. Destroyed on modal exit. `setLookAndFeel(nullptr)` in destructor. |
| **Lean** | No speculative abstraction. `Config::patchKey` does one thing. No unnecessary getters. |
| **Explicit** | Font dispatch via `dynamic_cast` on `NameLabel`/`ShortcutLabel`. No componentID abuse. |
| **SSOT** | `Action::Registry::getEntries()` is authoritative. Tree is ephemeral derived mirror. |
| **Stateless** | `Action::Row` holds no mutable state beyond bound `juce::Value`. |
| **Encapsulation** | `KeyRemapDialog` receives only `juce::Value&`. No access to tree, registry, or config. |
| **Deterministic** | `onValueChanged` -> `patchKey` -> `buildKeyMap`. Fixed pipeline, no branches. |

---

## Step 1: Namespace Restructure — File Move

**Goal:** Move action files from terminal layer to app layer.

**Actions:**
1. Create directory `Source/action/`
2. Move `Source/terminal/action/Action.h` -> `Source/action/Action.h`
3. Move `Source/terminal/action/Action.cpp` -> `Source/action/Action.cpp`
4. Move `Source/terminal/action/ActionList.h` -> `Source/action/ActionList.h`
5. Move `Source/terminal/action/ActionList.cpp` -> `Source/action/ActionList.cpp`
6. Delete empty `Source/terminal/action/` directory (and any tombstone files in it)

**Validation:**
- `Source/action/` contains all 4 files
- `Source/terminal/action/` is gone
- No dangling includes yet (will fix in Step 2)

---

## Step 2: Rename Terminal::Action -> Action::Registry

**Goal:** Rename class and update namespace.

**File: `Source/action/Action.h`**
- Change `namespace Terminal` -> `namespace Action`
- Rename `class Action` -> `class Registry`
- Update CRTP: `jreng::Context<Action>` -> `jreng::Context<Registry>`
- Update `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Action)` -> `(Registry)`
- Fix relative include path: `#include "../../config/Config.h"` -> `#include "../config/Config.h"`
- Update all doxygen references from `Action` to `Registry`, `Terminal::Action` to `Action::Registry`

**File: `Source/action/Action.cpp`**
- Change `namespace Terminal` -> `namespace Action`
- Prefix all `Action::` -> `Registry::`
- Fix include: `#include "Action.h"` stays (same dir)

**File: `Source/action/ActionList.h`**
- Change `namespace Terminal` -> `namespace Action`
- Rename `class ActionList` -> `class List`
- Fix include: `#include "../../config/Config.h"` -> `#include "../config/Config.h"`
- Update leak detector

**File: `Source/action/ActionList.cpp`**
- Change `namespace Terminal` -> `namespace Action`
- Prefix `ActionList::` -> `List::`
- Fix include: `#include "../../Gpu.h"` -> `#include "../Gpu.h"`

**Validation:**
- All 4 files compile-clean within `namespace Action`
- No `Terminal::Action` or `Terminal::ActionList` references remain in `Source/action/`

---

## Step 3: Update All Call Sites

**Goal:** Every file that references `Terminal::Action` or `Terminal::ActionList` must be updated.

| File | Change |
|---|---|
| `Source/Main.cpp:287` | `Terminal::Action action;` -> `Action::Registry action;` |
| `Source/Main.cpp` | `#include` path if present (check) |
| `Source/MainComponent.h:46` | `#include "terminal/action/Action.h"` -> `#include "action/Action.h"` |
| `Source/MainComponent.h:47` | `#include "terminal/action/ActionList.h"` -> `#include "action/ActionList.h"` |
| `Source/MainComponent.h:160` | `std::unique_ptr<Terminal::ActionList>` -> `std::unique_ptr<Action::List>` |
| `Source/MainComponent.cpp:189` | `*Terminal::Action::getContext()` -> `*Action::Registry::getContext()` |
| `Source/MainComponent.cpp:475` | `std::make_unique<Terminal::ActionList>` -> `std::make_unique<Action::List>` |
| `Source/component/InputHandler.cpp:11` | `#include "../terminal/action/Action.h"` -> `#include "../action/Action.h"` |
| `Source/component/InputHandler.cpp:38` | `Terminal::Action::getContext()` -> `Action::Registry::getContext()` |
| `Source/whelmed/InputHandler.cpp:12` | `#include "../terminal/action/Action.h"` -> `#include "../action/Action.h"` |
| `Source/whelmed/InputHandler.cpp:22-48` | All `Terminal::Action::parseShortcut` -> `Action::Registry::parseShortcut` |
| `Source/whelmed/InputHandler.cpp:91` | `Terminal::Action::getContext()` -> `Action::Registry::getContext()` |

**Also check:** `Source/component/InputHandler.cpp` lines 114-147 for `Terminal::Action::parseShortcut` calls.

**Validation:**
- `grep -r "Terminal::Action" Source/` returns zero hits
- `grep -r "Terminal::ActionList" Source/` returns zero hits
- Build succeeds

---

## Step 4: Add Action::Registry::shortcutToString

**Goal:** Canonical inverse of `parseShortcut`. Produces `"cmd+c"` format that round-trips.

**File: `Source/action/Action.h`**
- Add declaration in public section:
```cpp
static juce::String shortcutToString (const juce::KeyPress& key);
```

**File: `Source/action/Action.cpp`**
- Implement: read `key.getModifiers()`, emit modifier tokens (`cmd`/`ctrl`, `shift`, `alt`), then key token from `getKeyCode()`. Use same token names as `parseShortcut` (lowercase, `pageup`, `pagedown`, `home`, `end`, `delete`, `insert`, `escape`, `return`, `tab`, `space`, `backspace`, `f1`-`f12`, single char for `a`-`z` and printable).
- Platform-aware: `commandModifier` -> `"cmd"` on Mac, `"ctrl"` on others. `ctrlModifier` -> `"ctrl"`.

**Validation:**
- `shortcutToString(parseShortcut("cmd+c"))` == `"cmd+c"`
- `shortcutToString(parseShortcut("ctrl+shift+["))` == `"ctrl+shift+["`
- `shortcutToString(parseShortcut("f1"))` == `"f1"`
- `shortcutToString(parseShortcut("?"))` == `"?"`
- Round-trip: `parseShortcut(shortcutToString(kp))` == `kp` for any valid KeyPress

---

## Step 5: Config Additions

**Goal:** Add new config keys for Action::List fonts and close-on-run behaviour.

### 5a. Config::Key constants

**File: `Source/config/Config.h`** — add after `keysActionListPosition` (line ~409):

```cpp
inline static const juce::String keysActionListCloseOnRun { "action_list.close_on_run" };
inline static const juce::String actionListNameFamily     { "action_list.name_font_family" };
inline static const juce::String actionListNameSize       { "action_list.name_font_size" };
inline static const juce::String actionListShortcutFamily { "action_list.shortcut_font_family" };
inline static const juce::String actionListShortcutSize   { "action_list.shortcut_font_size" };
```

### 5b. Config::initKeys() defaults

**File: `Source/config/Config.cpp`** — add after `keysActionListPosition` default (line ~213):

```cpp
addKey (Key::keysActionListCloseOnRun,  true,                  { T::boolean });
addKey (Key::actionListNameFamily,      "Display Medium",      { T::string });
addKey (Key::actionListNameSize,        13.0,                  { T::number, 6.0, 72.0, true });
addKey (Key::actionListShortcutFamily,  "Display Mono Medium", { T::string });
addKey (Key::actionListShortcutSize,    12.0,                  { T::number, 6.0, 72.0, true });
```

### 5c. default_end.lua additions

**File: `Source/config/default_end.lua`**

Inside `keys` block, after `action_list_position` (line ~505), add:
```lua
		-- Close the action list after running an action.
		-- When false, the list stays open after execution.
		action_list_close_on_run = %%action_list_close_on_run%%,
```

New `action_list` table block after `keys` closing brace (before `popup` block, line ~509):
```lua
	-- ========================================================================
	-- ACTION LIST
	-- ========================================================================

	action_list = {
		-- Font family for action name labels.
		name_font_family = "%%action_list_name_font_family%%",

		-- Font size for action name labels in points (6 - 72).
		name_font_size = %%action_list_name_font_size%%,

		-- Font family for keyboard shortcut labels. Should be monospace.
		shortcut_font_family = "%%action_list_shortcut_font_family%%",

		-- Font size for keyboard shortcut labels in points (6 - 72).
		shortcut_font_size = %%action_list_shortcut_font_size%%,
	},
```

**Validation:**
- Build succeeds
- Config loads without warnings
- `Config::getContext()->getBool(Config::Key::keysActionListCloseOnRun)` returns `true`
- `Config::getContext()->getString(Config::Key::actionListNameFamily)` returns `"Display Medium"`

---

## Step 6: Config::patchKey

**Goal:** Targeted line replacement in `end.lua` for shortcut remapping.

**File: `Source/config/Config.h`** — add public method:
```cpp
void patchKey (const juce::String& key, const juce::String& value);
```

**File: `Source/config/Config.cpp`** — implement:

1. Read `end.lua` as text (full file, `juce::File::loadFileAsString`)
2. Convert dot-notation key (e.g. `"keys.copy"`) to Lua table path:
   - Split on `.` -> table name (`"keys"`) + leaf name (`"copy"`)
3. Locate the table block in the file text
4. Within the table block, find the line containing `leaf_name =`
5. Replace the value token in-place:
   - String values: wrap in quotes (`"cmd+c"`)
   - Boolean/number values: unquoted
6. If key not found in file (user never set it): append to the relevant table block before the closing `}`
7. Write back to `end.lua` (`juce::File::replaceWithText`)
8. Does NOT call `onReload`. Caller handles post-patch actions.

**Edge cases:**
- Key not present in end.lua -> append
- String values must be quoted in Lua
- Boolean/number values must not be quoted
- Preserve existing indentation (use tab indentation matching the block)
- Handle backslash escaping for Lua string values

**Validation:**
- Patch `keys.copy` from `"cmd+c"` to `"cmd+shift+c"` -> verify file content
- Patch a key not in file -> verify it appends correctly
- Verify `onReload` is NOT called
- After `patchKey` + `buildKeyMap()`, the new shortcut is active

---

## Step 7: Action::LookAndFeel

**Goal:** Font dispatch for Action::List labels.

**File: `Source/action/LookAndFeel.h`** (new)

```cpp
namespace Action {

class LookAndFeel : public Terminal::LookAndFeel
{
public:
    juce::Font getLabelFont (juce::Label& label) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

}
```

**File: `Source/action/LookAndFeel.cpp`** (new)

`getLabelFont` implementation:
- `dynamic_cast<NameLabel*>` -> Config actionListNameFamily + actionListNameSize
- `dynamic_cast<ShortcutLabel*>` -> Config actionListShortcutFamily + actionListShortcutSize
- Fallback: `LookAndFeel_V4::getLabelFont(label)`

Requires `#include "ActionRow.h"` for the label types.

**Validation:**
- Build succeeds
- LookAndFeel can be instantiated

---

## Step 8: Action::Row

**Goal:** Single row in the action list — name label + shortcut label.

**File: `Source/action/ActionRow.h`** (new)

Declare inside `namespace Action`:

```cpp
class NameLabel     : public juce::Label {};
class ShortcutLabel : public juce::Label {};

class Row : public juce::Component
{
public:
    Row (const Action::Registry::Entry& entry, const juce::String& uuid);

    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

    NameLabel     nameLabel;
    ShortcutLabel shortcutLabel;
    juce::String  actionConfigKey;
    std::function<bool()> run;
    std::function<void(Row*)> onExecuted;
    std::function<void(Row*)> onShortcutClicked;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Row)
};
```

**File: `Source/action/ActionRow.cpp`** (new)

Constructor:
- `setComponentID(uuid)`
- `nameLabel.setText(entry.name, dontSendNotification)`
- `shortcutLabel.setText(Registry::shortcutToString(entry.shortcut), dontSendNotification)`
- `shortcutLabel.setComponentID("shortcut")`
- Resolve config key: `entry.id` -> `"keys." + entry.id` (lookup from `actionKeyTable` or derive from registry entry)
- `run = entry.execute`
- `addAndMakeVisible(nameLabel)`, `addAndMakeVisible(shortcutLabel)`
- Wire `shortcutLabel.mouseDoubleClick` -> `onShortcutClicked(this)` (or override in Row)

`resized()`:
- Name label gets majority of width (e.g. 70%)
- Shortcut label gets remainder, right-aligned

`mouseDoubleClick`:
- If click is on shortcutLabel area -> `onShortcutClicked(this)`
- Otherwise -> `run()` + `onExecuted(this)`

**Validation:**
- Build succeeds
- Row can be instantiated with a dummy Entry

---

## Step 9: Action::KeyRemapDialog

**Goal:** Modal key capture for remapping shortcuts.

**File: `Source/action/KeyRemapDialog.h`** (new)

```cpp
namespace Action {

class KeyRemapDialog : public jreng::ModalWindow
{
public:
    KeyRemapDialog (juce::Value& shortcutValue,
                    const juce::String& actionName,
                    juce::Component& caller);

    bool keyPressed (const juce::KeyPress& key) override;

private:
    juce::Value& shortcutValue;
    juce::Label  promptLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyRemapDialog)
};

}
```

**File: `Source/action/KeyRemapDialog.cpp`** (new)

Constructor:
- Create ModalWindow with `&promptLabel` as content
- Set prompt text: `"Press a key combination for: " + actionName`
- Glass styling from Config (same as ActionList)
- `enterModalState(true)`

`keyPressed`:
- Modifier-only -> return true (ignore, keep waiting)
- Escape -> `exitModalState(0)`, no write, return true
- Complete KeyPress -> `shortcutValue.setValue(Registry::shortcutToString(key))` -> `exitModalState(0)`, return true

Writing to `shortcutValue` fires `state.onValueChanged` on `Action::List` automatically.

**Validation:**
- Build succeeds
- Dialog opens, captures a key, writes to Value, closes

---

## Step 10: Action::List (Full Implementation)

**Goal:** Replace the stub with the full command palette.

**File: `Source/action/ActionList.h`** — rewrite:

```cpp
namespace Action {

class List : public jreng::ModalWindow
{
public:
    explicit List (juce::Component& caller);
    ~List() override;

    bool keyPressed (const juce::KeyPress& key) override;

private:
    static constexpr int searchBoxHeight { 24 };
    static constexpr int rowHeight       { 28 };

    jreng::ValueTree          state { "ACTION_LIST" };
    Action::LookAndFeel       lookAndFeel;
    juce::TextEditor          searchBox;
    juce::Viewport            viewport;
    juce::Component           rowContainer;
    jreng::Owner<Action::Row> rows;

    int selectedIndex { -1 };

    void buildRows();
    void filterRows (const juce::String& query);
    void layoutRows();
    void selectRow (int index);
    void executeSelected();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (List)
};

}
```

**File: `Source/action/ActionList.cpp`** — rewrite:

Constructor sequence:
1. `ModalWindow (nullptr, "", true, false)` — no content component, we manage layout
2. `setLookAndFeel(&lookAndFeel)`
3. Build search box (existing TextEditor setup from current stub, same colours)
4. Add viewport with rowContainer inside
5. `buildRows()` — creates Row per `Registry::getEntries()`, wires ValueTree
6. Wire `searchBox.onTextChange` -> `filterRows(searchBox.getText())`
7. Glass styling from Config
8. Size: 60% of caller width, height = searchBoxHeight + min(rowCount * rowHeight, maxVisibleRows * rowHeight)
9. `centreAroundComponent`, `setVisible(true)`, `enterModalState(true)`
10. `searchBox.grabKeyboardFocus()`

Destructor:
- `setLookAndFeel(nullptr)` (before members destroyed)

`buildRows()`:
- For each `Registry::getEntries()` entry:
  - `uuid = juce::Uuid().toString()`
  - Create `Row(entry, uuid)`, append to `rows`, add to `rowContainer`
  - Append child `juce::ValueTree("ACTION")` to `state.get()` with `jreng::ID::id = uuid`
- `jreng::ValueTree::attach(state, row.get())` per row
- Wire `state.onValueChanged`:
  - Find changed UUID by walking tree nodes
  - Match to row via `row->getComponentID()`
  - `Config::getContext()->patchKey(row->actionConfigKey, newValue)`
  - `Registry::getContext()->buildKeyMap()`
- Wire each row's `onExecuted`:
  - If `Config::getBool(Key::keysActionListCloseOnRun)` -> `exitModalState(0)`
- Wire each row's `onShortcutClicked`:
  - Get `juce::Value` from tree node for this row's UUID
  - Create `KeyRemapDialog(value, row->nameLabel.getText(), *this)`

`filterRows(query)`:
- Empty -> all rows visible
- Non-empty -> `jreng::FuzzySearch::getResult(query, nameDataset)` -> show matching, hide others
- `layoutRows()` after filter

`layoutRows()`:
- Stack visible rows vertically in `rowContainer`
- Set `rowContainer` height = visibleCount * rowHeight
- Resize List window height = searchBoxHeight + min(viewport content, max visible area)

`keyPressed`:
- Up -> `selectRow(selectedIndex - 1)` (wrap or clamp)
- Down -> `selectRow(selectedIndex + 1)` (wrap or clamp)
- Return -> `executeSelected()`
- Escape -> inherited ModalWindow handler

`selectRow(index)`:
- Clamp to visible rows
- Visual highlight on selected row (background colour change)
- Scroll viewport to ensure selected row is visible

`executeSelected()`:
- If `selectedIndex >= 0` and row is visible: `rows[selectedIndex]->run()`
- If `Config::getBool(Key::keysActionListCloseOnRun)` -> `exitModalState(0)`

### MainComponent wiring update

**File: `Source/MainComponent.cpp`** — update `action_list` callback:
```cpp
actionList = std::make_unique<Action::List> (*this);
actionList->onModalDismissed = [this] { actionList.reset(); };
```

**Validation:**
- Build succeeds
- Action list opens on prefix + `?`
- Shows all registered actions with names and shortcuts
- Fuzzy search filters rows
- Up/Down navigation works
- Enter executes selected action
- Escape dismisses
- Double-click on row executes action
- Click on shortcut label opens KeyRemapDialog
- Key remap writes to end.lua and rebuilds key map
- close_on_run config respected
- `actionList.reset()` on dismiss (no leak)

---

## Execution Order

Steps must be executed strictly in order. Each step is validated before proceeding.

```
Step 1  -> File move
Step 2  -> Rename class + namespace
Step 3  -> Update call sites (build must pass)
Step 4  -> shortcutToString (build must pass)
Step 5  -> Config additions (build must pass)
Step 6  -> Config::patchKey (build must pass)
Step 7  -> Action::LookAndFeel (new file, build must pass)
Step 8  -> Action::Row (new file, build must pass)
Step 9  -> Action::KeyRemapDialog (new file, build must pass)
Step 10 -> Action::List full rewrite + MainComponent wiring (build must pass)
```

Steps 1-3 are the prerequisite restructure. Must land clean before Steps 4-10.
Steps 4-6 are infrastructure. Can be validated independently.
Steps 7-9 are new components. Each builds on prior.
Step 10 is the integration step that wires everything together.

---

## Files Summary

**Moved (Step 1):**
- `Source/terminal/action/Action.h` -> `Source/action/Action.h`
- `Source/terminal/action/Action.cpp` -> `Source/action/Action.cpp`
- `Source/terminal/action/ActionList.h` -> `Source/action/ActionList.h`
- `Source/terminal/action/ActionList.cpp` -> `Source/action/ActionList.cpp`

**Modified (Steps 2-6, 10):**
- `Source/action/Action.h` — rename, new method
- `Source/action/Action.cpp` — rename, new method
- `Source/action/ActionList.h` — full rewrite
- `Source/action/ActionList.cpp` — full rewrite
- `Source/Main.cpp` — type rename
- `Source/MainComponent.h` — includes + type rename
- `Source/MainComponent.cpp` — namespace + type renames + onModalDismissed wiring
- `Source/component/InputHandler.cpp` — include + namespace rename
- `Source/whelmed/InputHandler.cpp` — include + namespace rename
- `Source/config/Config.h` — new Key constants
- `Source/config/Config.cpp` — new initKeys + patchKey method
- `Source/config/default_end.lua` — new keys + action_list block

**New (Steps 7-9):**
- `Source/action/LookAndFeel.h`
- `Source/action/LookAndFeel.cpp`
- `Source/action/ActionRow.h`
- `Source/action/ActionRow.cpp`
- `Source/action/KeyRemapDialog.h`
- `Source/action/KeyRemapDialog.cpp`

**Deleted:**
- `Source/terminal/action/` directory and any tombstone files
