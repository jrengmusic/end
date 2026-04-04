# RECOMMENDATION — Action::List (Command Palette)
Date: 2026-04-04
Status: Ready for COUNSELOR handoff

## Problem Statement

`ActionList` is a stub — inherits `jreng::ModalWindow`, renders a bare `TextEditor`,
enters modal state, and does nothing else. No list, no fuzzy filter, no keyboard
navigation, no remapping, no execution. The PLAN describes the full intent but the
implementation is ~5% complete.

Additionally, `Terminal::Action` is misplaced at the terminal layer. It is app-level
orchestration and must be decoupled.

---

## Research Summary

- `jreng::ModalWindow` → `GlassWindow` → `juce::DocumentWindow`. Separate native
  window peer. Escape and close button call `exitModalState(0)` + `onModalDismissed`.
  `inputAttemptWhenModal()` brings window to front (does not dismiss). Confirmed
  correct for this use case — child-component approach was tried previously and
  abandoned due to terminal keyboard focus conflicts.

- `jreng::ValueTree` (`modules/jreng_data_structures`) — already forked and available.
  `attach(ValueTree& state, Component* parent)` walks parent + direct children,
  binds each child's `juce::Value` (via `Value::getFrom`) to a tree node keyed by
  `componentID`, adds `&state` as `juce::Value::Listener`. `onValueChanged` fires
  on any property write.

- `jreng::Value::getFrom(juce::Label*)` → `label->getTextValue()`. Confirmed in
  `jreng_core/value/jreng_value.h`. No custom `Value::Object` implementation needed.

- `jreng::FuzzySearch::getResult(juce::String, Data::vector)` returns
  `vector<pair<int,string>>` (score + string). Available in `jreng_core`.

- `Terminal::LookAndFeel` — no `getLabelFont` override exists. Font dispatch for two
  label types requires `Action::LookAndFeel` subclass with `getLabelFont` override
  dispatching via `dynamic_cast` on `Action::NameLabel` / `Action::ShortcutLabel`.

- Config has no write-back path. Targeted line replacement in `end.lua` must be
  implemented as `Config::patchKey(key, value)`. Post-patch, only
  `Action::Registry::buildKeyMap()` is called — not the full `Config::onReload` chain.

- All `Terminal::Action` call sites confirmed:
  - `Source/component/InputHandler.cpp` — `Terminal::Action::getContext()`,
    `Terminal::Action::parseShortcut()`
  - `Source/whelmed/InputHandler.cpp` — same
  - `Source/MainComponent.cpp` — `Terminal::Action::getContext()`
  - `Source/terminal/action/ActionList.cpp` — include

---

## Principles & Rationale

**BLESSED mapping:**

| Pillar | Decision |
|---|---|
| **Bounds** | `jreng::ValueTree` owned by `Action::List`, lifetime matches the modal window |
| **Lean** | No speculative infrastructure. `Config::patchKey` does one thing only |
| **Explicit** | Font dispatch via `dynamic_cast` on named subclasses — no magic, no componentID abuse |
| **SSOT** | `Action::Registry::getEntries()` is the only source for action data. Tree is ephemeral mirror |
| **Stateless** | `Action::Row` holds no mutable state beyond its bound `juce::Value` |
| **Encapsulation** | `Action::List` owns tree and rows. `Action::KeyRemapDialog` receives only `juce::Value&` |
| **Deterministic** | `onValueChanged` → `patchKey` → `buildKeyMap` is a fixed, no-branch pipeline |

**Why ModalWindow over child component:** confirmed by ARCHITECT — child component
approach was implemented and abandoned. ModalWindow is correct.

**Why UUID as node ID:** action id strings (e.g. `"copy"`) must not double as tree
node identifiers — they are config keys, not UI identity. UUID keeps the tree purely
ephemeral with no semantic coupling to the action system.

**Why `Config::patchKey` instead of full reload:** remapping a shortcut must not
trigger font resize, renderer switch, tab rebuild, or LookAndFeel update. Only the
key map needs to change. Full `onReload` chain is too broad.

---

## Prerequisite: Action Namespace Restructure

This must land **before** the new files are written.

### 1. Move `terminal/action/` → `Source/action/`

`Action` is app-level orchestration, not terminal-specific. Nothing in the terminal
layer should depend on it.

**New location:**
```
Source/
  action/
    Action.h / Action.cpp          <- Action::Registry (renamed from Action class)
    ActionList.h / ActionList.cpp  <- Action::List
    ActionRow.h / ActionRow.cpp    <- Action::Row           (new)
    KeyRemapDialog.h / .cpp        <- Action::KeyRemapDialog (new)
    LookAndFeel.h / .cpp           <- Action::LookAndFeel   (new)
```

### 2. Rename `Terminal::Action` class → `Action::Registry`

`namespace Action` replaces `namespace Terminal` for all action types.
`Action::Registry` inherits `jreng::Context<Action::Registry>` and `juce::Timer`.

### 3. Update all call sites

| File | Old | New |
|---|---|---|
| `Source/component/InputHandler.cpp` | `Terminal::Action::getContext()` | `Action::Registry::getContext()` |
| `Source/component/InputHandler.cpp` | `Terminal::Action::parseShortcut()` | `Action::Registry::parseShortcut()` |
| `Source/whelmed/InputHandler.cpp` | `Terminal::Action::getContext()` | `Action::Registry::getContext()` |
| `Source/whelmed/InputHandler.cpp` | `Terminal::Action::parseShortcut()` | `Action::Registry::parseShortcut()` |
| `Source/MainComponent.cpp` | `Terminal::Action::getContext()` | `Action::Registry::getContext()` |
| `Source/MainComponent.h` | `#include "terminal/action/Action.h"` | `#include "action/Action.h"` |
| `Source/MainComponent.h` | `#include "terminal/action/ActionList.h"` | `#include "action/ActionList.h"` |
| `Source/MainComponent.h` | `std::unique_ptr<Terminal::ActionList>` | `std::unique_ptr<Action::List>` |
| `Source/component/InputHandler.cpp` | `#include "../terminal/action/Action.h"` | `#include "../action/Action.h"` |
| `Source/whelmed/InputHandler.cpp` | `#include "../terminal/action/Action.h"` | `#include "../action/Action.h"` |

### 4. Add `Action::Registry::shortcutToString(juce::KeyPress)` — new static method

Canonical inverse of `parseShortcut`. Reconstructs modifier tokens from
`KeyPress::getModifiers()` and key token from `getKeyCode()`. Produces
`"cmd+c"` format that round-trips through `parseShortcut`.

---

## Scaffold

### Config additions

**`Config::Key` — new constants:**
```cpp
inline static const juce::String keysActionListCloseOnRun { "action_list.close_on_run" };
inline static const juce::String actionListNameFamily     { "action_list.name_font_family" };
inline static const juce::String actionListNameSize       { "action_list.name_font_size" };
inline static const juce::String actionListShortcutFamily { "action_list.shortcut_font_family" };
inline static const juce::String actionListShortcutSize   { "action_list.shortcut_font_size" };
```

**`Config::initKeys()` additions:**
```cpp
addKey (Key::keysActionListCloseOnRun,  true,                  { T::boolean });
addKey (Key::actionListNameFamily,      "Display Medium",      { T::string });
addKey (Key::actionListNameSize,        13.0,                  { T::number, 6.0, 72.0, true });
addKey (Key::actionListShortcutFamily,  "Display Mono Medium", { T::string });
addKey (Key::actionListShortcutSize,    12.0,                  { T::number, 6.0, 72.0, true });
```

**`Config::patchKey(const juce::String& key, const juce::String& value)` — new method:**

Reads `end.lua` as text. Converts dot-notation key (e.g. `"keys.copy"`) to its Lua
table leaf name. Scans for the line within the correct table block. Replaces the
value token in-place. Writes back. Does NOT call `onReload`. Caller is responsible
for any post-patch action (e.g. `buildKeyMap()`).

Edge case: key not present in `end.lua` (user never explicitly set it) — append to
the relevant table block. Must handle string values (quoted) and boolean/number
values (unquoted). Must preserve backslash escaping for Lua string values.

**`default_end.lua` additions:**

Inside `keys` block, after `action_list_position`:
```lua
		-- Close the action list after running an action.
		-- When false, the list stays open after execution.
		action_list_close_on_run = %%action_list_close_on_run%%,
```

New `action_list` table block (after `keys` block):
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

---

### Component structure

#### `Action::NameLabel` / `Action::ShortcutLabel`

Pure type-tag subclasses of `juce::Label`. No members, no overrides. Used solely
for font dispatch in `Action::LookAndFeel::getLabelFont`.

```cpp
class NameLabel     : public juce::Label {};
class ShortcutLabel : public juce::Label {};
```

Both declared in `ActionRow.h` inside `namespace Action`.

#### `Action::LookAndFeel`

Inherits `Terminal::LookAndFeel`. Overrides `getLabelFont` only. All other tab,
popup, and resizer behaviour inherited unchanged. Set on `Action::List` at
construction; all children inherit via JUCE's LookAndFeel propagation.

```cpp
juce::Font getLabelFont (juce::Label& label) override
{
    auto* cfg { Config::getContext() };

    if (dynamic_cast<NameLabel*> (&label) != nullptr)
        return juce::Font { juce::FontOptions()
            .withName (cfg->getString (Config::Key::actionListNameFamily))
            .withPointHeight (cfg->getFloat (Config::Key::actionListNameSize)) };

    if (dynamic_cast<ShortcutLabel*> (&label) != nullptr)
        return juce::Font { juce::FontOptions()
            .withName (cfg->getString (Config::Key::actionListShortcutFamily))
            .withPointHeight (cfg->getFloat (Config::Key::actionListShortcutSize)) };

    return LookAndFeel_V4::getLabelFont (label);
}
```

#### `Action::Row`

`juce::Component`. Flat — `NameLabel` and `ShortcutLabel` are direct children
(required for `jreng::ValueTree::attach` single-level walk).

Members:
```cpp
NameLabel     nameLabel;
ShortcutLabel shortcutLabel;    // componentID = "shortcut"
juce::String  actionConfigKey;  // e.g. "keys.copy" — for Config::patchKey
std::function<bool()> run;      // copy of Action::Registry::Entry::execute
```

Construction:
```cpp
Row (const Action::Registry::Entry& entry, const juce::String& uuid)
{
    setComponentID (uuid);
    nameLabel.setText (entry.name, juce::dontSendNotification);
    shortcutLabel.setText (
        Action::Registry::shortcutToString (entry.shortcut),
        juce::dontSendNotification);
    shortcutLabel.setComponentID ("shortcut");
    actionConfigKey = resolveConfigKey (entry.id); // "copy" -> "keys.copy"
    run = entry.execute;
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (shortcutLabel);
}
```

`shortcutLabel` is not user-editable directly. Click on it opens
`Action::KeyRemapDialog`.

Double-click anywhere on the row (outside `shortcutLabel`) → `run()` → owner
notified via callback. `Action::Row` does not close itself. `Action::List` decides
whether to call `exitModalState(0)` based on
`Config::getBool(Key::keysActionListCloseOnRun)`.

#### `Action::List`

`jreng::ModalWindow` subclass. Owns:
```cpp
jreng::ValueTree          state { "ACTION_LIST" };
Action::LookAndFeel       lookAndFeel;
juce::TextEditor          searchBox;
juce::Viewport            viewport;
juce::Component           rowContainer;
jreng::Owner<Action::Row> rows;
```

Construction sequence:
1. `setLookAndFeel (&lookAndFeel)`
2. For each `Action::Registry::Entry`:
   - `uuid = juce::Uuid().toString()`
   - Create `Action::Row (entry, uuid)`, append to `rows`, add to `rowContainer`
   - Append child `juce::ValueTree ("ACTION")` to `state.get()` with properties
     `jreng::ID::id = uuid` and `jreng::ID::value = shortcutToString(entry.shortcut)`
3. `jreng::ValueTree::attach (state, row.get())` per row — binds
   `shortcutLabel.getTextValue()` via `referTo` to the tree node, adds `&state`
   as `juce::Value::Listener`
4. Wire `state.onValueChanged`:
   - Walk tree nodes to find changed UUID
   - Match UUID to row via `row->getComponentID()`
   - `Config::patchKey (row->actionConfigKey, newShortcutString)`
   - `Action::Registry::getContext()->buildKeyMap()`
5. Wire `searchBox.onTextChange` → `filterRows (searchBox.getText())`
6. Set glass, size, centre around caller, `setVisible (true)`, `enterModalState (true)`

`filterRows (const juce::String& query)`:
- Empty → all rows visible, re-layout
- Non-empty → `FuzzySearch::getResult (query, nameDataset)` → show matching rows,
  hide others, re-layout `rowContainer` height

`keyPressed` on `Action::List`:
- Up/Down → move selection highlight between visible rows
- Enter → `selectedRow->run()` + conditional `exitModalState (0)`
- Escape → inherited `ModalWindow::keyPressed` handles it

#### `Action::KeyRemapDialog`

`jreng::ModalWindow` subclass.

Receives on construction:
- `juce::Value& shortcutValue` — already `referTo`'d to the tree node
- `const juce::String& actionName` — display only

Shows single centred label: `"Press a key combination for: <actionName>"`.

`keyPressed`:
- Modifier-only keypresses → ignored
- First complete `KeyPress` → `Action::Registry::shortcutToString (key)` →
  `shortcutValue.setValue (result)` → `exitModalState (0)`
- Escape → `exitModalState (0)`, no write

Writing to `shortcutValue` fires `state.onValueChanged` on `Action::List` →
write-back pipeline runs automatically.

#### `MainComponent` wiring

Member type change:
```cpp
// before
std::unique_ptr<Terminal::ActionList> actionList;
// after
std::unique_ptr<Action::List> actionList;
```

`action_list` callback (already registered, update to):
```cpp
action.registerAction ("action_list", ..., [this]() -> bool
{
    actionList = std::make_unique<Action::List> (*this);
    actionList->onModalDismissed = [this] { actionList.reset(); };
    return true;
});
```

---

## BLESSED Compliance Checklist

- [x] **Bounds** — `Action::List` owns the ValueTree and all rows. Both destroyed on modal exit.
- [x] **Lean** — No speculative abstraction. `Config::patchKey` is the minimum write-back needed.
- [x] **Explicit** — Font dispatch via `dynamic_cast` on named types. No hidden control flow.
- [x] **SSOT** — `Action::Registry::getEntries()` is authoritative. Tree is a derived ephemeral mirror.
- [x] **Stateless** — `Action::Row` has no mutable state. `Action::KeyRemapDialog` has no state.
- [x] **Encapsulation** — `Action::KeyRemapDialog` receives only `juce::Value&`. No access to tree, registry, or config directly.
- [x] **Deterministic** — `onValueChanged` → `patchKey` → `buildKeyMap`. Same input, same output, no side branches.

---

## Open Questions

None. All decisions settled by ARCHITECT.

---

## Handoff Notes

- **Prerequisite order is strict.** Restructure (`Terminal::Action` → `Action::Registry`,
  file move, call site updates) must land before any new files are written. Do not
  scaffold `Action::List`, `Action::Row`, `Action::KeyRemapDialog`, or
  `Action::LookAndFeel` until `Action::Registry` is in place and all call sites updated.

- **`Config::patchKey`** is net-new. Implement and verify in isolation before
  `Action::List` calls it. Key not present in `end.lua` (user never set it explicitly)
  → append to the relevant table block. Must handle string values (quoted) and
  boolean/number values (unquoted). Must preserve backslash escaping.

- **`Action::Registry::shortcutToString`** must round-trip with `parseShortcut`.
  Smoke test before `Action::KeyRemapDialog` ships.

- **`jreng::ValueTree::attach (ValueTree&, Component*)`** walks parent + direct
  children only — not recursive. `Action::Row` must be flat. `nameLabel` and
  `shortcutLabel` must be direct children, not nested inside another component.

- **`shortcutLabel` display value** uses `shortcutToString(entry.shortcut)` which
  produces canonical `"cmd+c"` format. `Config::patchKey` receives this same format.
  Label display and config value are the same string — no conversion at write-back time.

- **`action_list.close_on_run`** lives in its own `action_list` Lua table, not inside
  `keys`. Config key constant is `"action_list.close_on_run"`.

- **Double-click ownership:** `Action::Row::mouseDoubleClick` calls `run()` and
  notifies `Action::List` via callback. `Action::Row` does not call `exitModalState`
  itself — ownership boundary must not be crossed.

- **AppState not needed for double-click execution.** `Action::Registry::Entry::run`
  callbacks capture `tabs` at registration time in `MainComponent::registerActions`.
  `tabs->getActiveTerminal()` remains valid while `Action::List` is modal — it does
  not mutate tab state.

- **`Action::LookAndFeel`** instance is a member of `Action::List`, not static or
  global. Lifetime matches the modal window. `setLookAndFeel(nullptr)` must be called
  in `Action::List` destructor before `lookAndFeel` is destroyed.
