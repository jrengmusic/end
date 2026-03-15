# PLAN: Terminal::ActionList

**Date:** 2026-03-15
**Status:** Pending

## Objective

Fuzzy-searchable command palette that lists all registered actions from Terminal::Action. Triggered by prefix + `?`. Lazily created on demand at MainComponent.

## Design

### What ActionList IS
- A `juce::Component` child of MainComponent
- Textbox (top) + dropdown list (below)
- Glass background (semi-transparent, blur)
- Lazily created on first trigger, reused after
- Reads `Terminal::Action::getEntries()` for content

### What ActionList is NOT
- Not a separate window
- Not a GL-rendered overlay
- Not always visible

### Trigger
- Prefix key + `?` (modal action `action_list`)
- Registered in Terminal::Action as a modal action
- Config key: `keys.action_list`, default: `?`

### Behavior
1. User presses prefix + `?`
2. ActionList appears (top or bottom of window, configurable)
3. Textbox has focus, cursor blinking
4. All actions listed in dropdown, 3 columns: name | description | shortcut
5. User types → fuzzy filter via `jreng::FuzzySearch`
6. Arrow keys navigate the list
7. Enter → execute selected action → close ActionList
8. Escape → close ActionList, no action
9. Click on entry → execute → close

### Layout
```
┌──────────────────────────────────────────────────┐
│ [search text box                               ] │
├──────────────────────────────────────────────────┤
│ Copy          Copy selection to clipboard  ctrl+c│
│ Paste         Paste from clipboard         ctrl+v│
│ Quit          Quit application             ctrl+q│
│ Zoom In       Increase font size           ctrl+=│
│ ...                                              │
└──────────────────────────────────────────────────┘
```

### Position
- Config key: `keys.action_list_position`, default: `"top"`
- `"top"` — anchored to top of MainComponent, full width, drops down
- `"bottom"` — anchored to bottom of MainComponent, full width, drops up

### Sizing
- Width: 60% of MainComponent width, centered horizontally
- Height: textbox (fixed ~30px) + list (up to 50% of MainComponent height, scrollable)
- Row height: based on font metrics

### Styling
- Glass background (semi-transparent, from Config window colour + alpha)
- Text colour from Config foreground
- Selected row highlighted with Config selection colour
- Shortcut column right-aligned or left-aligned in its column
- Font: overlay font family + size from Config

### Fuzzy Search
- On every keystroke: `jreng::FuzzySearch::getResult(query, dataset)`
- Dataset: vector of action names (rebuilt from entries on show)
- Results sorted by relevance (FuzzySearch handles this)
- Empty query: show all actions

## Files to Create
- [ ] `Source/terminal/action/ActionList.h`
- [ ] `Source/terminal/action/ActionList.cpp`

## Files to Modify
- [ ] `Source/config/Config.h` — add `keysActionList`, `keysActionListPosition`
- [ ] `Source/config/Config.cpp` — add defaults (`?`, `"top"`)
- [ ] `Source/config/default_end.lua` — add action_list entries
- [ ] `Source/terminal/action/Action.cpp` — add `action_list` to key table
- [ ] `Source/MainComponent.h` — add `std::unique_ptr<Terminal::ActionList>` member
- [ ] `Source/MainComponent.cpp` — register `action_list` action, lazy create + show

## Execution Order
1. Add Config keys + defaults + schema + lua template
2. Create ActionList.h + ActionList.cpp (standalone component)
3. Register `action_list` action in MainComponent, lazy create + show
4. Build and test

## Contracts
- ARCHITECTURAL-MANIFESTO: Explicit Encapsulation (ActionList is dumb — receives entries, shows UI, calls execute callback), Lean (one component, one job)
- NAMING-CONVENTION: `ActionList`, `show`, `hide`, `filterEntries` — semantic
- JRENG-CODING-STANDARD: Allman braces, `not`/`and`/`or`, brace init, no early returns
