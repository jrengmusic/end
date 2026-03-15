# PLAN: Terminal::Action

**Date:** 2026-03-15
**Status:** In Progress

## Objective

Replace scattered keyboard/action handling (KeyBinding, ModalKeyBinding, MainComponent::buildCommandActions, TerminalComponent::keyPressed Ctrl+C/V) with a single `Terminal::Action` object that owns all user-actionable dispatch.

## Design

### What Terminal::Action IS
- Single owner of all user-performable actions
- Owns the action table (fixed set of actions with ID, name, description, category, callback)
- Owns the key map (KeyPress -> action ID, rebuilt on config reload)
- Owns the prefix state machine (absorbed from ModalKeyBinding)
- Global singleton via `jreng::Context<Action>`
- Lives at `Source/terminal/action/Action.h` + `Action.cpp`

### What Terminal::Action is NOT
- Not an ApplicationCommandTarget
- Not a KeyListener (callers call `handleKeyPress()` explicitly)
- Does not know about Terminal::Component, Session, Grid, or any other object
- Callbacks are `std::function<bool()>` — wired by MainComponent at construction

### Action Entry (POD struct)
```
struct Entry
{
    juce::String id;            // "copy", "paste", "split_horizontal"
    juce::String name;          // "Copy"
    juce::String description;   // "Copy selection to clipboard"
    juce::String category;      // "Edit", "View", "Panes", "Tabs"
    juce::KeyPress shortcut;    // resolved from config (hot-reloadable)
    bool isModal;               // true = prefix key required
    std::function<bool()> execute;
};
```

### Lifecycle
1. MainComponent constructs Terminal::Action
2. MainComponent registers all actions with callbacks (lambdas capturing `tabs`)
3. Terminal::Action reads end.lua, builds key map (KeyPress -> action ID)
4. TerminalComponent::keyPressed calls Action::handleKeyPress()
5. If Action consumes the key -> return true
6. If Action does not consume -> forward to session (PTY)
7. On "reload_config" action: re-read end.lua, rebuild key map only (callbacks unchanged)

### Key Resolution Order
1. Check if key is the prefix key -> enter modal state
2. If in modal state: check modal bindings -> execute action -> return to idle
3. If in idle state: check global bindings -> execute action
4. If no match: return false (caller forwards to PTY)

### Actions (complete set)
| ID | Name | Category | Default Shortcut | Modal |
|----|------|----------|-----------------|-------|
| copy | Copy | Edit | Ctrl+C (with selection gate) | No |
| paste | Paste | Edit | Ctrl+V | No |
| quit | Quit | Application | Ctrl+Q / Alt+F4 (Win) | No |
| close_tab | Close Tab | Tabs | Ctrl+W | No |
| reload_config | Reload Config | Application | Ctrl+R | No |
| zoom_in | Zoom In | View | Ctrl+= | No |
| zoom_out | Zoom Out | View | Ctrl+- | No |
| zoom_reset | Zoom Reset | View | Ctrl+0 | No |
| new_tab | New Tab | Tabs | Ctrl+T | No |
| prev_tab | Previous Tab | Tabs | Ctrl+Shift+[ | No |
| next_tab | Next Tab | Tabs | Ctrl+Shift+] | No |
| split_horizontal | Split Horizontal | Panes | \ | Yes |
| split_vertical | Split Vertical | Panes | - | Yes |
| pane_left | Focus Left Pane | Panes | h | Yes |
| pane_down | Focus Down Pane | Panes | j | Yes |
| pane_up | Focus Up Pane | Panes | k | Yes |
| pane_right | Focus Right Pane | Panes | l | Yes |
| close_pane | Close Pane | Panes | x | Yes |

### Copy action special behavior
- If box selection is active: copy to clipboard, clear selection, return true
- If no selection: return false (falls through to PTY which receives Ctrl+C as \x03)

## Files to Create
- [ ] `Source/terminal/action/Action.h`
- [ ] `Source/terminal/action/Action.cpp`

## Files to Modify
- [ ] `Source/MainComponent.h` — remove ApplicationCommandTarget, commandDefs, commandActions, buildCommandActions, bindModalActions, commandManager, keyBinding, modalKeyBinding. Add Terminal::Action member.
- [ ] `Source/MainComponent.cpp` — replace buildCommandActions/bindModalActions with Action registration. Remove getAllCommands, getCommandInfo, perform. Constructor wires callbacks to Action.
- [ ] `Source/component/TerminalComponent.h` — remove isMouseTracking, shouldForwardMouseToPty declarations if moved. Remove Ctrl+C/V handling from keyPressed.
- [ ] `Source/component/TerminalComponent.cpp` — keyPressed delegates to Action::handleKeyPress() first. Remove inline Ctrl+C/V logic.

## Files to Delete
- [ ] `Source/config/KeyBinding.h`
- [ ] `Source/config/KeyBinding.cpp`
- [ ] `Source/config/ModalKeyBinding.h`
- [ ] `Source/config/ModalKeyBinding.cpp`

## Execution Order
1. Create Action.h + Action.cpp (standalone, no dependencies on existing code)
2. Wire Action into MainComponent (register callbacks, replace buildCommandActions)
3. Wire TerminalComponent::keyPressed to delegate to Action
4. Remove ApplicationCommandTarget from MainComponent
5. Delete KeyBinding.h/cpp and ModalKeyBinding.h/cpp
6. Build and test each step

## Contracts
- ARCHITECTURAL-MANIFESTO: Explicit Encapsulation (Action is dumb, knows nothing about terminals), Lean (one object, one job), SSOT (single action registry)
- NAMING-CONVENTION: `Action`, `Entry`, `handleKeyPress`, `reload` — all semantic
- JRENG-CODING-STANDARD: Allman braces, `not`/`and`/`or`, brace init, `.at()`, no early returns
