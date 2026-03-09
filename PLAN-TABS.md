# PLAN: Tab System

**Date:** 2026-03-09
**Status:** NOT STARTED
**Prerequisite for:** Split pane system, WHELMED integration
**Depends on:** KeyBinding + ApplicationCommandManager (DONE)

---

## Overview

Add a tab system to END. Each tab contains a Terminal::Component with its own
Session, PTY, Grid, Screen, and OpenGL context.

**Hierarchy (current sprint):**
```
MainComponent (ApplicationCommandTarget)
  └── Terminal::Tabs (TabbedComponent subclass)
        ├── Tab 0: Terminal::Component (session 0)
        ├── Tab 1: Terminal::Component (session 1)
        └── Tab 2: Terminal::Component (session 2)
```

**Hierarchy (future — split panes):**
```
MainComponent
  └── Terminal::Tabs
        ├── Tab 0: Terminal::PaneContainer
        │     ├── Terminal::Component (session 0)
        │     └── Terminal::Component (session 1)
        └── Tab 1: Terminal::PaneContainer
              └── Terminal::Component (session 2)
```

---

## Architecture

### Tab lifecycle
- All Terminal::Components stay as children (no remove/add from hierarchy)
- Override `currentTabChanged()` — toggle visibility only
- VBlank stops naturally when not visible (no ComponentPeer)
- On tab switch-in: `visibilityChanged()` fires, Screen re-attaches, all cells
  marked dirty, full re-render on next VBlank
- Session + Grid + State + PTY stay alive on hidden tabs

### Tab bar
- Visible only when > 1 tab
- Position configurable: top, bottom, left, right (via `tabs.position` config)
- Appearance controlled by Terminal::LookAndFeel
- No close buttons — user uses cmd+w shortcut
- Tab title = current working directory (cwd)

### Commands
- `cmd+t` — new tab (fresh shell)
- `cmd+w` — cascading close: active pane -> active tab -> quit app
- `cmd+[` — previous tab
- `cmd+]` — next tab

### Close cascade (cmd+w)
```
if (active tab has multiple panes)
    close active pane
else if (tab container has multiple tabs)
    close active tab (kills PTY, destroys Terminal::Component)
else
    quit application
```

---

## New Files

### T1. Terminal::LookAndFeel

**File:** `Source/component/LookAndFeel.h`
**File:** `Source/component/LookAndFeel.cpp`

Subclass of `juce::LookAndFeel_V4`. Overrides tab drawing methods:
- `drawTabButton()` — minimal glass-style tab
- `drawTabButtonText()` — tab title text
- `drawTabbedButtonBarBackground()` — translucent bar background
- `createTabButtonShape()` — simple rectangle (no rounded tab shape)
- `fillTabButtonShape()` — fill with config colours
- `getTabButtonBestWidth()` — calculate width from title text
- `getTabButtonFont()` — use overlay font from Config

Wired to Config for appearance:
- Tab bar background colour (reuse `window.colour` with opacity)
- Active tab highlight colour (reuse `colours.cursor` or new config key)
- Tab text colour (reuse `colours.foreground`)
- Tab bar height (new config key `tabs.height`, default 28)

```cpp
namespace Terminal
{

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawTabButton (juce::TabBarButton&, juce::Graphics&,
                        bool isMouseOver, bool isMouseDown) override;
    void drawTabButtonText (juce::TabBarButton&, juce::Graphics&,
                            bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&,
                                        juce::Graphics&) override;
    void createTabButtonShape (juce::TabBarButton&, juce::Path&,
                               bool isMouseOver, bool isMouseDown) override;
    void fillTabButtonShape (juce::TabBarButton&, juce::Graphics&,
                             const juce::Path&,
                             bool isMouseOver, bool isMouseDown) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    juce::Font getTabButtonFont (juce::TabBarButton&, float height) override;

    void reloadFromConfig();
};

}
```

### T2. Terminal::Tabs

**File:** `Source/component/Tabs.h`
**File:** `Source/component/Tabs.cpp`

Subclass of `juce::TabbedComponent`. Manages tab lifecycle.

```cpp
namespace Terminal
{

class Tabs : public juce::TabbedComponent
{
public:
    explicit Tabs (juce::TabbedButtonBar::Orientation orientation);
    ~Tabs() override;

    void addNewTab();
    void closeActiveTab();
    void selectPreviousTab();
    void selectNextTab();

    int getTabCount() const noexcept;
    Component* getActiveTerminal() noexcept;

private:
    void currentTabChanged (int newIndex, const juce::String& name) override;
    void updateTabBarVisibility();

    jreng::Owner<Component> terminals;
    LookAndFeel lookAndFeel;
};

}
```

Key behaviors:
- `addNewTab()` — creates Terminal::Component, adds tab, switches to it
- `closeActiveTab()` — destroys Terminal::Component + PTY, removes tab,
  switches to adjacent tab. If last tab, returns (MainComponent handles quit).
- `currentTabChanged()` — toggles visibility (no remove/add child).
  All terminals added via `addChildComponent()` at creation, only active one
  is `setVisible(true)`.
- `updateTabBarVisibility()` — `setTabBarDepth(0)` when 1 tab,
  `setTabBarDepth(configHeight)` when > 1.
- Owns `Terminal::LookAndFeel` and calls `setLookAndFeel()` on itself.

---

## Modified Files

### T3. Config — add tab keys

**File:** `Source/config/Config.h`

Add to `Key`:
```cpp
inline static const juce::String keysNewTab     { "keys.new_tab" };
inline static const juce::String keysPrevTab    { "keys.prev_tab" };
inline static const juce::String keysNextTab    { "keys.next_tab" };
inline static const juce::String tabsPosition   { "tabs.position" };
inline static const juce::String tabsHeight     { "tabs.height" };
```

**File:** `Source/config/Config.cpp`

Add defaults:
```cpp
values[Key::keysNewTab]   = "cmd+t"
values[Key::keysPrevTab]  = "cmd+["
values[Key::keysNextTab]  = "cmd+]"
values[Key::tabsPosition] = "top"
values[Key::tabsHeight]   = 28
```

Add schema entries (keys = string, position = string, height = int).

### T4. KeyBinding — add tab CommandIDs

**File:** `Source/config/KeyBinding.h`

Add to `CommandID` enum:
```cpp
enum class CommandID : int
{
    copy = 1, paste, quit, closeTab, reload,
    zoomIn, zoomOut, zoomReset,
    newTab, prevTab, nextTab
};
```

**File:** `Source/config/KeyBinding.cpp`

Update `actionIDForCommand()` and `commandForActionID()` arrays (size 8 -> 11).
Update `loadFromConfig()` to read the 3 new keys.
Update `applyMappings()` loop range to include new commands.

### T5. MainComponent — wire TabContainer

**File:** `Source/MainComponent.h`

Replace:
```cpp
std::unique_ptr<Terminal::Component> terminal;
```
With:
```cpp
std::unique_ptr<Terminal::Tabs> tabs;
```

Add `#include "component/Tabs.h"`.

**File:** `Source/MainComponent.cpp`

Constructor:
- Create `Tabs` with orientation from Config
- `addAndMakeVisible (tabs.get())`
- `tabs->addNewTab()` (initial tab)

`resized()`:
- `tabs->setBounds (getLocalBounds())`

`perform()`:
- `closeTab` — call `tabs->closeActiveTab()`, if no tabs left then quit
- `newTab` — call `tabs->addNewTab()`
- `prevTab` — call `tabs->selectPreviousTab()`
- `nextTab` — call `tabs->selectNextTab()`
- `copy/paste/zoom/reload` — route to `tabs->getActiveTerminal()->`

`getAllCommands()` — add 3 new CommandIDs.
`getCommandInfo()` — add 3 new command infos.

### T6. Terminal::Component — cwd for tab title

**File:** `Source/component/TerminalComponent.h`

Add public method:
```cpp
juce::String getCurrentWorkingDirectory() const;
```

**File:** `Source/component/TerminalComponent.cpp`

Implementation reads cwd from the PTY process. On macOS/Linux:
`/proc/<pid>/cwd` (Linux) or `proc_pidinfo` (macOS).
Fallback: return shell name or "Terminal".

---

## Execution Order

```
T1 -> T2 -> T3 -> T4 -> T5 -> T6
```

T1: LookAndFeel (independent, no existing code changes)
T2: Tabs (depends on T1 for LookAndFeel)
T3: Config keys (additive, no breakage)
T4: KeyBinding commands (additive, extends existing)
T5: MainComponent wiring (integrates everything)
T6: Tab title from cwd (polish)

Build + test after each step.

---

## Edge Cases

- **Tab switch while process is outputting:** Grid keeps accumulating data via
  PTY reader thread. On switch-in, all cells are dirty, full re-render. No data
  lost.
- **Resize while tab is hidden:** Hidden Terminal::Component gets `resized()`
  from TabbedComponent (it calls resized on all content components). Grid
  reflows to new size. Screen re-attaches on visibility.
- **OpenGL context on hidden tab:** Detached when not visible. Re-attached via
  `visibilityChanged()` which already calls `screen.attachTo(*this)`.
- **Last tab closed:** MainComponent calls `systemRequestedQuit()`.
- **Config reload:** All tabs get `reloadConfig()` called (iterate terminals).
  LookAndFeel also reloads.
- **Tab container uses `jreng::Owner<Component>`** for terminal ownership.

---

*This plan is the source of truth for this sprint. Update status inline as steps complete.*
