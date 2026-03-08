# PLAN: KeyBinding System + ApplicationCommandManager + Terminal::Component Rename

**Date:** 2026-03-08
**Status:** IN PROGRESS
**Prerequisite for:** Split pane system, tabbing, WHELMED integration

---

## Overview

Three parts:
1. **Part A: KeyBinding class** — configurable keyboard shortcut storage with Config integration (DONE)
2. **Part C: ApplicationCommandManager integration** — unified command system, MainComponent as orchestrator
3. **Part B: Terminal::Component rename** — `TerminalComponent` -> `Terminal::Component`

**Priority chain:** `end.lua overrides > Config defaults > TTY passthrough`

---

## Part A: KeyBinding Class

### A1. Add key binding defaults and schema to Config

**File:** `Source/config/Config.h`
- Add `Config::Key` entries for each binding: `keys.copy`, `keys.paste`, `keys.quit`, `keys.close_tab`, `keys.reload`, `keys.zoom_in`, `keys.zoom_out`, `keys.zoom_reset`
- All type `string` in schema

**File:** `Source/config/Config.cpp`
- Add defaults in `initDefaults()`:
  ```
  values[Key::keysCopy]      = "cmd+c"
  values[Key::keysPaste]     = "cmd+v"
  values[Key::keysQuit]      = "cmd+q"
  values[Key::keysCloseTab]  = "cmd+w"
  values[Key::keysReload]    = "cmd+r"
  values[Key::keysZoomIn]    = "cmd+="
  values[Key::keysZoomOut]   = "cmd+-"
  values[Key::keysZoomReset] = "cmd+0"
  ```
- Add schema entries in `initSchema()` (all `T::string`)
- Platform note: `cmd` maps to Cmd on Mac, Ctrl on Windows/Linux

**Validate:** Config compiles, defaults load, `getString("keys.copy")` returns `"cmd+c"`

### A2. Create KeyBinding class header

**File:** `Source/config/KeyBinding.h`

```
struct KeyBinding : jreng::Context<KeyBinding>
{
    KeyBinding();

    // Rebuild bindings from Config (called on reload)
    void reload();

    // Dispatch a keypress. Returns true if handled as app shortcut.
    bool dispatch (const juce::KeyPress& key) const;

    // Register an action handler
    void registerAction (const juce::String& actionID, std::function<void()> handler);

    // Static helper: get the KeyPress bound to an action
    static juce::KeyPress getBinding (const juce::String& actionID);

    // Parse "cmd+c" -> juce::KeyPress
    static juce::KeyPress parse (const juce::String& shortcutString);

private:
    // action ID -> KeyPress
    std::unordered_map<juce::String, juce::KeyPress> bindings;

    // KeyPress -> action ID (reverse lookup)
    std::unordered_map<juce::KeyPress, juce::String> reverseBindings;

    // action ID -> callable
    jreng::Function::Map<juce::String, void> actions;

    void loadFromConfig();
};
```

Also add `std::hash<juce::KeyPress>` specialization in this header:
```cpp
namespace std
{
template<>
struct hash<juce::KeyPress>
{
    size_t operator() (const juce::KeyPress& k) const noexcept
    {
        auto h1 { std::hash<int>{} (k.getKeyCode()) };
        auto h2 { std::hash<int>{} (k.getModifiers().getRawFlags()) };
        return h1 ^ (h2 << 16);
    }
};
}
```

**Validate:** Header compiles (include in a .cpp, no linker errors)

### A3. Implement KeyBinding

**File:** `Source/config/KeyBinding.cpp`

**`parse()`** — static, converts string to KeyPress:
- Split on `+`
- Accumulate modifiers: `cmd`/`ctrl` -> command (Mac) or ctrl (Win/Linux), `shift`, `alt`/`opt`
- Last token is the key: single char, or special names (`pageup`, `pagedown`, `home`, `end`, `f1`-`f12`, `=`, `-`, `0`-`9`)
- Return `juce::KeyPress (keyCode, modifiers, 0)`

**`loadFromConfig()`** — reads all `keys.*` from Config, parses each, populates `bindings` and `reverseBindings`

**`KeyBinding()`** — calls `loadFromConfig()`

**`reload()`** — clears bindings + reverseBindings, calls `loadFromConfig()`. Actions map is NOT cleared (handlers persist across reload).

**`registerAction()`** — adds to `actions` map via `Function::Map::add`

**`dispatch()`**:
```
auto it = reverseBindings.find (key);
if (it == reverseBindings.end()) return false;
if (not actions.contains (it->second)) return false;
actions.get (it->second);
return true;
```

**`getBinding()`** — static, reads from `Context<KeyBinding>` instance

**Validate:** Unit-testable: `parse("cmd+c")` returns correct KeyPress, `parse("cmd+shift+d")` works, `parse("cmd+=")` works

### A4. Wire KeyBinding into application lifecycle

**File:** `Source/Main.cpp`
- Add `KeyBinding keyBinding;` member to `ENDApplication` (after Config, before MainWindow)
- KeyBinding constructor reads from Config::getContext()

**File:** `Source/MainComponent.h` / `.cpp`
- No changes needed — KeyBinding is accessed via Context

**Validate:** App launches, KeyBinding::getContext() is valid

### A5. Register actions in Terminal::Component

**File:** `Source/component/TerminalComponent.cpp` (will become Terminal::Component in Part B)

In constructor, register all actions:
```cpp
auto* kb { KeyBinding::getContext() };
kb->registerAction ("keys.copy",       [this] { /* copy logic */ });
kb->registerAction ("keys.paste",      [this] { /* paste logic */ });
kb->registerAction ("keys.quit",       [] { JUCEApplication::getInstance()->systemRequestedQuit(); });
kb->registerAction ("keys.close_tab",  [] { JUCEApplication::getInstance()->systemRequestedQuit(); });
kb->registerAction ("keys.reload",     [this] { /* reload logic */ });
kb->registerAction ("keys.zoom_in",    [this] { /* zoom in logic */ });
kb->registerAction ("keys.zoom_out",   [this] { /* zoom out logic */ });
kb->registerAction ("keys.zoom_reset", [this] { /* zoom reset logic */ });
```

Extract the existing inline logic from `keyPressed()` into named private methods:
- `doCopy()`
- `doPaste()`
- `doReload()`
- `doZoomIn()`
- `doZoomOut()`
- `doZoomReset()`

**Validate:** Each action callable independently

### A6. Refactor keyPressed() to use KeyBinding::dispatch()

**File:** `Source/component/TerminalComponent.cpp`

Replace the if/else chain with:
```cpp
bool TerminalComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const int code { key.getKeyCode() };
    const auto mods { key.getModifiers() };

    // App shortcuts (configurable)
    if (KeyBinding::getContext()->dispatch (key))
        return true;

    // Platform-specific close (not configurable)
    #if JUCE_WINDOWS
    if (mods.isAltDown() and code == juce::KeyPress::F4Key)
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        return true;
    }
    #endif

    // Scroll navigation (Shift+PageUp/Down/Home/End)
    const bool isScrollNav { mods.isShiftDown() and not mods.isCommandDown()
        and (code == juce::KeyPress::pageUpKey or code == juce::KeyPress::pageDownKey
             or code == juce::KeyPress::homeKey or code == juce::KeyPress::endKey) };

    if (isScrollNav)
    {
        handleScrollNavigation (code);
        return true;
    }

    // Clear selection + reset scroll on any other key
    clearSelectionAndScroll();
    session.handleKeyPress (key);
    cursor->resetBlink();
    return true;
}
```

Extract scroll navigation into `handleScrollNavigation (int code)`.
Extract selection clearing into `clearSelectionAndScroll()`.

**Validate:** All existing keyboard shortcuts work identically. Copy/paste, zoom, reload, quit all functional.

### A7. Wire reload to KeyBinding

**File:** `Source/component/TerminalComponent.cpp`

In the reload action handler, also call `KeyBinding::getContext()->reload()` so updated keybindings take effect.

**Validate:** Change a keybinding in end.lua, press Cmd+R, new binding works.

---

## Part C: ApplicationCommandManager Integration

**Architecture:**
- `KeyBinding` — storage only. Owns bindings map, `parse()`, `loadFromConfig()`, `reload()`. Constructor takes `ApplicationCommandManager&` and manages key mappings on it internally.
- `MainComponent` — owns `ApplicationCommandManager`, owns `KeyBinding`, implements `ApplicationCommandTarget`. Orchestrates everything.
- `TerminalComponent` — zero knowledge of KeyBinding or commands. Public action methods called by MainComponent's `perform()`. `keyPressed()` only handles scroll nav + TTY passthrough.

```
end.lua -> Config -> KeyBinding (storage + ACM key mapping)
                          |
                   MainComponent (ACM owner + ApplicationCommandTarget)
                          |
                   TerminalComponent (public action methods + TTY)
```

### C1. Define CommandIDs and rewrite KeyBinding.h

**File:** `Source/config/KeyBinding.h`

Remove:
- `jreng::Context<KeyBinding>` inheritance
- `Function::Map<juce::String, void> actions`
- `reverseBindings` map
- `dispatch()` method
- `registerAction()` method
- `getBinding()` static method

Add:
- `enum CommandID : int { copy = 1, paste, quit, closeTab, reload, zoomIn, zoomOut, zoomReset }`
- Constructor takes `juce::ApplicationCommandManager&`
- `void reload()` — re-reads Config, re-applies key mappings on the ACM
- `static juce::KeyPress parse (const juce::String& shortcutString)` — unchanged
- `static const juce::String& actionIDForCommand (CommandID)` — returns Config::Key string
- `static CommandID commandForActionID (const juce::String&)` — reverse lookup
- `juce::KeyPress getBinding (CommandID) const` — returns current binding for a command

Keep:
- `std::hash<juce::KeyPress>` specialization
- `bindings` map (actionID -> KeyPress)
- `loadFromConfig()` private method

New private:
- `juce::ApplicationCommandManager& acm` — reference to MainComponent's ACM
- `void applyMappings()` — clears all key mappings on ACM, re-adds from bindings map

```cpp
struct KeyBinding
{
    enum CommandID : int
    {
        copy = 1, paste, quit, closeTab,
        reload, zoomIn, zoomOut, zoomReset
    };

    explicit KeyBinding (juce::ApplicationCommandManager& acm);

    void reload();

    juce::KeyPress getBinding (CommandID cmd) const;

    static juce::KeyPress parse (const juce::String& shortcutString);
    static const juce::String& actionIDForCommand (CommandID cmd);
    static CommandID commandForActionID (const juce::String& actionID);

private:
    juce::ApplicationCommandManager& acm;
    std::unordered_map<juce::String, juce::KeyPress> bindings;

    void loadFromConfig();
    void applyMappings();
};
```

**Validate:** Header compiles

### C2. Rewrite KeyBinding.cpp

**File:** `Source/config/KeyBinding.cpp`

- Constructor: store ACM ref, call `loadFromConfig()`, call `applyMappings()`
- `loadFromConfig()` — same as before: reads Config, parses strings, populates `bindings` map
- `applyMappings()` — iterates all CommandIDs, calls `acm.getKeyMappings()->clearAllKeyPresses (cmdID)` then `acm.getKeyMappings()->addKeyPress (cmdID, binding)` for each
- `reload()` — clears bindings, calls `loadFromConfig()`, calls `applyMappings()`
- `parse()` — unchanged
- `getBinding()` — looks up CommandID -> actionID -> bindings map
- `actionIDForCommand()` / `commandForActionID()` — static lookup tables mapping CommandID <-> Config::Key strings

**Validate:** Compiles, `parse()` still works

### C3. MainComponent becomes ApplicationCommandTarget

**File:** `Source/MainComponent.h`

- Add `#include "config/KeyBinding.h"`
- Inherit `juce::ApplicationCommandTarget`
- Add members (in this order):
  ```cpp
  juce::ApplicationCommandManager commandManager;
  KeyBinding keyBinding { commandManager };
  std::unique_ptr<TerminalComponent> terminal;
  ```
- Declare ApplicationCommandTarget overrides:
  ```cpp
  juce::ApplicationCommandTarget* getNextCommandTarget() override;
  void getAllCommands (juce::Array<juce::CommandID>& commands) override;
  void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
  bool perform (const InvocationInfo& info) override;
  ```

**File:** `Source/MainComponent.cpp`

- `getNextCommandTarget()` — return `nullptr`
- `getAllCommands()` — add all 8 CommandIDs
- `getCommandInfo()` — switch on CommandID, fill shortName + description, call `result.addDefaultKeypress()` using `keyBinding.getBinding (cmd)`
- `perform()` — switch on CommandID:
  - `copy` -> `terminal->doCopy()`
  - `paste` -> `terminal->doPaste()`
  - `quit` -> `JUCEApplication::getInstance()->systemRequestedQuit()`
  - `closeTab` -> `JUCEApplication::getInstance()->systemRequestedQuit()`
  - `reload` -> `keyBinding.reload()` then `terminal->doReload()`
  - `zoomIn` -> `terminal->doZoomIn()`
  - `zoomOut` -> `terminal->doZoomOut()`
  - `zoomReset` -> `terminal->doZoomReset()`
- Constructor: after creating terminal, call:
  ```cpp
  commandManager.registerAllCommandsForTarget (this);
  addKeyListener (commandManager.getKeyMappings());
  ```

**Validate:** Compiles, commands registered

### C4. Clean up TerminalComponent

**File:** `Source/component/TerminalComponent.h`

- Remove `#include "config/KeyBinding.h"`
- Remove `registerKeyActions()` declaration
- Move `doCopy()`, `doPaste()`, `doReload()`, `doZoomIn()`, `doZoomOut()`, `doZoomReset()` from private to public
- `doReload()` no longer calls `KeyBinding::getContext()->reload()` — MainComponent handles that

**File:** `Source/component/TerminalComponent.cpp`

- Remove `registerKeyActions()` method entirely
- Remove `registerKeyActions()` call from constructor
- Remove `KeyBinding::getContext()->dispatch (key)` from `keyPressed()`
- Remove `#if JUCE_WINDOWS` Alt+F4 block from `keyPressed()` (now a configurable binding)
- `doReload()` — remove `KeyBinding::getContext()->reload()` line, keep `Config::getContext()->reload()` + `applyConfig()` + overlay
- `keyPressed()` becomes:
  ```cpp
  bool TerminalComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
  {
      const int code { key.getKeyCode() };
      const auto mods { key.getModifiers() };

      const bool isScrollNav { mods.isShiftDown() and not mods.isCommandDown()
          and (code == juce::KeyPress::pageUpKey or code == juce::KeyPress::pageDownKey
               or code == juce::KeyPress::homeKey or code == juce::KeyPress::endKey) };

      if (isScrollNav)
      {
          handleScrollNavigation (code);
          return true;
      }

      clearSelectionAndScroll();
      session.handleKeyPress (key);
      cursor->resetBlink();
      return true;
  }
  ```

**Validate:** Compiles

### C5. Remove KeyBinding from ENDApplication

**File:** `Source/Main.cpp`

- Remove `#include "config/KeyBinding.h"`
- Remove `KeyBinding keyBinding;` member
- Remove doc comment about KeyBinding

**Validate:** Compiles, app launches, all shortcuts work via ACM

### C6. Verify priority chain

**Test plan:**
1. Launch with default end.lua — all default shortcuts work (cmd+c, cmd+v, cmd+q, etc.)
2. Edit end.lua to remap `keys.zoom_in = "cmd+]"` — press cmd+r to reload — cmd+] zooms in, cmd+= passes to TTY
3. Edit end.lua to set `keys.quit = "cmd+shift+q"` — reload — cmd+shift+q quits, cmd+q passes to TTY
4. Single-key binding: `keys.reload = "F5"` — reload — F5 reloads
5. Remove all overrides from end.lua — reload — defaults restored

---

## Part B: Terminal::Component Rename

### B1. Rename class in header

**File:** `Source/component/TerminalComponent.h`

- Wrap class in `namespace Terminal {`
- Rename `class TerminalComponent` -> `class Component`
- Update JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR
- Update constructor/destructor names
- Close namespace

**Validate:** Header compiles

### B2. Rename class in implementation

**File:** `Source/component/TerminalComponent.cpp`

- Replace all `TerminalComponent::` with `Terminal::Component::`

**Validate:** Compiles

### B3. Update MainComponent references

**Files:** `Source/MainComponent.h`, `Source/MainComponent.cpp`

- `std::unique_ptr<TerminalComponent>` -> `std::unique_ptr<Terminal::Component>`
- `std::make_unique<TerminalComponent>()` -> `std::make_unique<Terminal::Component>()`
- Update doc comments

**Validate:** Compiles

### B4. Update doc comments across codebase

**Files:** All files referencing `TerminalComponent` in doc comments:
- `Source/component/CursorComponent.h`
- `Source/component/MessageOverlay.h`
- `Source/config/Config.h`
- `Source/config/Config.cpp`
- `Source/Main.cpp`
- `Source/terminal/rendering/Screen.h`

Replace `TerminalComponent` with `Terminal::Component` in doc comments only.

**Validate:** Grep for `TerminalComponent` returns zero results.

---

## Execution Order

```
A1-A7 (DONE) -> C1 -> C2 -> C3 -> C4 -> C5 -> C6 -> B1 -> B2 -> B3 -> B4
```

A1-A7: KeyBinding + Config integration (DONE).
C1-C2: Rewrite KeyBinding as storage + ACM key mapper.
C3: MainComponent becomes ApplicationCommandTarget.
C4: Clean up TerminalComponent (remove KeyBinding knowledge).
C5: Remove KeyBinding from ENDApplication.
C6: Verify priority chain (user tests).
B1-B4: Rename (mechanical, high file count but low risk).

---

## Files Created
- `Source/config/KeyBinding.h` (DONE, will be rewritten in C1)
- `Source/config/KeyBinding.cpp` (DONE, will be rewritten in C2)

## Files Modified
- `Source/config/Config.h` (DONE — Key entries added)
- `Source/config/Config.cpp` (DONE — defaults + schema added)
- `Source/Main.cpp` (DONE — KeyBinding member; C5 removes it)
- `Source/MainComponent.h` (C3 — ACM + ApplicationCommandTarget + KeyBinding member)
- `Source/MainComponent.cpp` (C3 — perform(), getAllCommands(), getCommandInfo(), wiring)
- `Source/component/TerminalComponent.h` (C4 — remove KeyBinding, public action methods; B1 — rename)
- `Source/component/TerminalComponent.cpp` (C4 — clean keyPressed; B2 — rename)
- `Source/component/CursorComponent.h` (B4 — doc comments)
- `Source/component/MessageOverlay.h` (B4 — doc comments)
- `Source/config/Config.h` (B4 — doc comments)
- `Source/config/Config.cpp` (B4 — doc comments)
- `Source/Main.cpp` (B4 — doc comments)

---

*This plan is the source of truth for this sprint. Update status inline as steps complete.*
