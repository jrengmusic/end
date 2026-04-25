# PLAN: Unified Lua Config Engine

**RFC:** RFC-lua-engine.md
**Date:** 2026-04-26
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no LANGUAGE.md overrides)

## Overview

Unify three separate Lua states (`Config`, `Whelmed::Config`, `Scripting::Engine`) into a single `lua::Engine` that owns one persistent `jam::lua::state`, one file watcher, and one load/reload pipeline. Config and Whelmed::Config become thin reader facades. One entry point (`end.lua`), six `require()` modules, standard Lua composition.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

---

## Steps

### Step 1: Scaffold lua::Engine class + CMake

**Scope:** `Source/lua/Engine.h` (new), `Source/lua/Engine.cpp` (new), `CMakeLists.txt`

**Action:**

Create `lua::Engine` class skeleton in `namespace lua`. Inherits `jam::Context<Engine>` and `jam::File::Watcher::Listener`.

**Engine.h** public API (mirrors Scripting::Engine + absorbed Config orchestration):
- `DisplayCallbacks` struct (identical to current `Scripting::Engine::DisplayCallbacks`)
- `PopupCallbacks` struct (identical)
- `SelectionKeys` struct (identical)
- Constructor: `Engine (DisplayCallbacks, PopupCallbacks)` — but does NOT call `load()` yet (construction before window; api callbacks wired later)
- `void load()` — runs end.lua, parses all tables, feeds Config + Whelmed::Config
- `void reload()` — fresh state, re-register api, re-parse, re-feed
- `void setDisplayCallbacks (DisplayCallbacks)` — called by MainComponent after window ready
- `void setPopupCallbacks (PopupCallbacks)` — called by MainComponent after window ready
- `void registerActions (Action::Registry&)`
- `void buildKeyMap (Action::Registry&)`
- `const SelectionKeys& getSelectionKeys() const noexcept`
- `const juce::String& getLoadError() const noexcept`
- `void patchKey (const juce::String& key, const juce::String& value)`
- `juce::String getActionLuaKey (const juce::String& actionId) const`
- `const juce::String& getPrefixString() const noexcept`
- `juce::String getShortcutString (const juce::String& actionLuaKey) const`
- `std::function<void()> onReload` — single callback replaces onActionReload + onConfigReload

**Engine.h** private:
- `PopupEntry`, `CustomAction`, `KeyBinding`, `KeyMapping` structs (identical to Scripting::Engine)
- `static constexpr std::array<KeyMapping, 22> keyMappings` (identical)
- `jam::lua::state lua`
- `jam::File::Watcher watcher`
- `DisplayCallbacks displayCallbacks`
- `PopupCallbacks popupCallbacks`
- `std::unordered_map<juce::String, juce::var> configValues` — fed to Config
- `std::unordered_map<juce::String, juce::var> whelmedValues` — fed to Whelmed::Config
- `std::vector<KeyBinding> keyBindings`
- `std::vector<PopupEntry> popupEntries`
- `std::vector<CustomAction> customActions`
- `SelectionKeys selectionKeys`
- `juce::String prefixString`
- `int prefixTimeoutMs { 1000 }`
- `juce::String loadError`
- `void fileChanged (const juce::File&, jam::File::Watcher::Event) override`
- Parse method declarations (implemented in Step 3)
- `static void writeDefaults()` declaration (implemented in Step 2)

**Engine.cpp**: constructor initializes watcher (watches `~/.config/end/`), destructor default. Method stubs for compile.

**CMakeLists.txt**: `Source/lua/` directory auto-discovered by existing `file(GLOB_RECURSE)` — verify. If not, add explicitly.

**Validation:** Compiles. No consumers. `lua::Engine` class exists with full API surface. JRENG-CODING-STANDARD: braces on new line, `not`/`and`/`or` tokens, brace init, no anonymous namespaces. NAMES.md: all names match RFC decisions.

---

### Step 2: Default Lua template files + writeDefaults

**Scope:** `Source/config/default_end.lua` (rewrite), `Source/config/default_nexus.lua` (new), `Source/config/default_display.lua` (new), `Source/config/default_keys.lua` (new), `Source/config/default_popups.lua` (new), `Source/config/default_actions.lua` (rename from default_action.lua), `Source/config/default_whelmed.lua` (format change), `Source/lua/Engine.cpp`

**Action:**

**default_end.lua** — thin entry point:
```lua
END = {
    nexus   = require("nexus"),
    display = require("display"),
    whelmed = require("whelmed"),
    keys    = require("keys"),
    popups  = require("popups"),
    actions = require("actions"),
}
```
No placeholders. This file is static.

**default_nexus.lua** — `return { gpu = "%%gpu%%", daemon = "%%daemon%%", ... }` with all nexus keys from RFC module partition. Inline documentation comments as banners.

**default_display.lua** — `return { window = { ... }, colours = { ... }, cursor = { ... }, font = { ... }, tab = { ... }, pane = { ... }, overlay = { ... }, menu = { ... }, action_list = { ... }, status_bar = { ... }, popup = { border_colour = ..., border_width = ... } }`. All display keys from RFC.

**default_keys.lua** — `return { prefix = "%%prefix%%", prefix_timeout = %%prefix_timeout%%, copy = "%%copy%%", ... }` with all key bindings + selection keys from current default_action.lua keys table.

**default_popups.lua** — `return { defaults = { cols = %%popup_cols%%, rows = %%popup_rows%%, position = "%%popup_position%%" }, ... }` with popup entries from current default_action.lua popups table.

**default_actions.lua** — `return { ... }` with custom action entries from current default_action.lua actions table. Rename file from default_action.lua.

**default_whelmed.lua** — Change from `WHELMED = { ... }` to `return { ... }`. Same content, wrapped in return.

**Implement `lua::Engine::writeDefaults()`:**
- Generates all 7 user files in `~/.config/end/` from BinaryData templates
- Consolidates placeholder substitution from all three current writeDefaults implementations
- Platform-conditional key defaults (cmd vs ctrl) — from current Scripting::Engine::writeDefaults
- Config value defaults — from current Config::writeDefaults
- Whelmed value defaults — from current Whelmed::Config::writeDefaults
- Only writes files that don't exist (first-launch behavior)

**Delete** `Source/config/default_action.lua` after creating default_actions.lua (rename).

**Validation:** All 7 templates compile into BinaryData. `writeDefaults()` generates correct files. Template placeholder names match RFC key namespace. BLESSED-L: no duplication between templates — each key appears in exactly one template.

---

### Step 3: lua::Engine::load() — full parsing pipeline

**Scope:** `Source/lua/EngineParse.cpp` (new), `Source/lua/Engine.cpp`

**Action:**

**Engine::load()** in Engine.cpp:
1. Clear all parsed state (configValues, whelmedValues, keyBindings, popupEntries, customActions, selectionKeys)
2. Recreate `lua` state: `lua = jam::lua::state {}`
3. Open base/string/table/math libs
4. Register `api` global table with display + popup callbacks (9 display lambdas + launchPopup lambda) — only if callbacks are set (nullptr-safe for pre-window load)
5. Set `package.path` to include `~/.config/end/?.lua`
6. Run `end.lua` via `lua.safe_script_file`
7. Call parse methods

**EngineParse.cpp** — all parse methods:

`parseConfig()` — extract `END.nexus.*` and `END.display.*` tables into `configValues` map. Key strings use the NEW module-prefixed dot-notation (e.g., `"display.font.family"`). Flattening logic: walk nested tables recursively, build dot-joined key string, store in map. Special cases:
- `terminal.padding` array → expand to 4 keys (padding_top/right/bottom/left)
- `action_list.padding` array → expand to 4 keys
- `hyperlinks.handlers` → parse into handler map (same structure as current Config)
- `hyperlinks.extensions` → parse into extension set

`parseWhelmed()` — extract `END.whelmed` table into `whelmedValues` map. Flat key namespace (no module prefix per RFC: "Whelmed keys remain flat").

`parseKeys()` — extract `END.keys` table. Walk `keyMappings` array, build `keyBindings` vector. Extract `prefix`, `prefix_timeout`. Same logic as current ScriptingParse.cpp `parseKeys()`.

`parsePopups()` — extract `END.popups` table. Build `popupEntries` vector. Same logic as current ScriptingParse.cpp `parsePopups()`. Popup defaults from `END.popups.defaults`.

`parseActions()` — extract `END.actions` table. Build `customActions` vector. `execute` field stores `jam::lua::protected_function` from the live Lua state. Same logic as current ScriptingParse.cpp `parseActions()`.

`parseSelectionKeys()` — extract selection keys from `END.keys.selection_*`. Same logic as current ScriptingParse.cpp `parseSelectionKeys()`.

**Also create** `Source/lua/EnginePatch.cpp` (new) — move patchKey() and getActionLuaKey() logic from ScriptingPatch.cpp. Update file path references from `action.lua` to `keys.lua` (key bindings now live in keys.lua).

**Validation:** load() successfully parses a well-formed `end.lua` + modules. configValues map contains all keys with correct module-prefixed names. whelmedValues map contains all whelmed keys (flat). keyBindings, popupEntries, customActions, selectionKeys match current Scripting::Engine parse output. BLESSED-S (SSOT): one parse pipeline, no duplication. BLESSED-B: lua::Engine sole owner of state.

---

### Step 4: Wire lua::Engine into ENDApplication + Config facade

**Scope:** `Source/Main.cpp`, `Source/config/Config.h`, `Source/config/Config.cpp`

**Action:**

**Main.cpp — ENDApplication member changes:**
- Add `lua::Engine luaEngine` member BEFORE `Config config` in declaration order (constructs first, destroys last)
- lua::Engine constructor receives empty DisplayCallbacks/PopupCallbacks (wired later by MainComponent)
- In `initialise()`: call `luaEngine.load()` before Config construction completes — but Config constructs as value member before initialise(). Resolution: lua::Engine member declared first → constructs first → calls writeDefaults() + load() in its constructor. Config constructor then reads from lua::Engine.
- Actually: lua::Engine constructor does: writeDefaults() if needed, then load(). Config constructor calls `lua::Engine::getContext()` to get its values.

**Config.h changes:**
- Add `void feed (const std::unordered_map<juce::String, juce::var>& values)` — replaces internal values map
- Remove Lua-related includes (no more local lua state)
- `Config::Key` string values updated with module prefix per RFC key mapping table:
  - `"font.family"` → `"display.font.family"`
  - `"colours.foreground"` → `"display.colours.foreground"`
  - `"window.title"` → `"display.window.title"`
  - `"gpu"` → `"nexus.gpu"`
  - `"daemon"` → `"nexus.daemon"`
  - `"auto_reload"` → `"nexus.auto_reload"`
  - `"shell.program"` → `"nexus.shell.program"`
  - `"terminal.scrollback_lines"` → `"nexus.terminal.scrollback_lines"`
  - `"tab.family"` → `"display.tab.family"`
  - `"cursor.char"` → `"display.cursor.char"`
  - `"pane.bar_colour"` → `"display.pane.bar_colour"`
  - `"overlay.family"` → `"display.overlay.family"`
  - `"menu.opacity"` → `"display.menu.opacity"`
  - `"action_list.position"` → `"display.action_list.position"`
  - `"status_bar.position"` → `"display.status_bar.position"`
  - `"popup.cols"` → `"popups.cols"`
  - `"popup.rows"` → `"popups.rows"`
  - `"popup.position"` → `"popups.position"`
  - `"popup.border_colour"` → `"display.popup.border_colour"`
  - `"popup.border_width"` → `"display.popup.border_width"`
  - `"hyperlinks.editor"` → `"nexus.hyperlinks.editor"`
  - All `"colours.*"` → `"display.colours.*"`
  - (complete mapping — every Key:: member gets its module prefix)
- Consumer call sites (285 references to Config::Key::*) are **UNCHANGED** — they reference constant names, not string values

**Config.cpp changes:**
- `Config()` constructor: call `initKeys()` (unchanged — sets defaults), then `feed()` from `lua::Engine::getContext()->getConfigValues()`
- Remove `load()` method's Lua state creation — Config no longer creates `jam::lua::state`
- Remove `writeDefaults()` — now in lua::Engine
- Remove `getConfigFile()` creation logic — lua::Engine handles file management
- `reload()` becomes: `initKeys()`, `feed()` from lua::Engine, fire `onReload`
- `validateAndStore()` / schema validation stays in Config (or moves to lua::Engine parseConfig — ARCHITECT decides during implementation if needed)
- `getHandler()` and `isClickableExtension()` — receive handler/extension data from lua::Engine via feed or dedicated setter
- Keep `colourCache`, `buildTheme()`, all getter methods (getString, getInt, getFloat, getBool, getColour)

**Add to lua::Engine public API:**
- `const std::unordered_map<juce::String, juce::var>& getConfigValues() const noexcept`
- `const HyperlinkData& getHyperlinkData() const noexcept` (or equivalent for handlers + extensions)

**Validation:** Config::getContext() returns correct values for all Key:: constants. All 55 consumer call sites compile unchanged. Config no longer creates a Lua state. lua::Engine is the sole Lua state owner. BLESSED-B: single owner. BLESSED-E (Encapsulation): 40+ call sites unchanged.

---

### Step 5: Convert Whelmed::Config to reader facade

**Scope:** `Source/config/WhelmedConfig.h`, `Source/config/WhelmedConfig.cpp`

**Action:**

Same pattern as Step 4 for Config:
- Add `void feed (const std::unordered_map<juce::String, juce::var>& values)`
- Remove Lua state creation from `load()`
- Remove `writeDefaults()` — now in lua::Engine
- Remove `getConfigFile()` creation logic
- `Whelmed::Config()` constructor: `initKeys()`, then `feed()` from `lua::Engine::getContext()->getWhelmedValues()`
- `reload()` becomes: `initKeys()`, `feed()` from lua::Engine, fire `onReload`
- Whelmed::Config::Key string values stay FLAT (no module prefix per RFC)
- Keep all getter methods, colour parsing, schema validation

**Add to lua::Engine public API:**
- `const std::unordered_map<juce::String, juce::var>& getWhelmedValues() const noexcept`

**Validation:** Whelmed::Config::getContext() returns correct values. All 12 consumer call sites compile unchanged. Whelmed::Config no longer creates a Lua state. BLESSED-S (Stateless): Config and Whelmed::Config are now pure readers.

---

### Step 6: Absorb Scripting::Engine into lua::Engine

**Scope:** `Source/lua/Engine.h`, `Source/lua/Engine.cpp`, `Source/MainComponent.h`, `Source/MainComponent.cpp`, `Source/MainComponentActions.cpp`, `Source/action/ActionList.cpp` (or wherever Action::List references Scripting::Engine)

**Action:**

**lua::Engine already has** registerActions, buildKeyMap, getSelectionKeys, patchKey, getActionLuaKey, getPrefixString, getShortcutString from Step 1 scaffold. Step 3 implemented the parse methods. Now wire consumers.

**MainComponent.h changes:**
- Remove `#include "../scripting/Scripting.h"`
- Add `#include "../lua/Engine.h"`
- Remove `std::unique_ptr<Scripting::Engine> scriptingEngine` member
- Add `lua::Engine& luaEngine` reference member (Engine lives in ENDApplication, MainComponent gets reference)

**MainComponent.cpp changes:**
- Constructor: receive `lua::Engine&` parameter (from ENDApplication)
- Build DisplayCallbacks and PopupCallbacks — call `luaEngine.setDisplayCallbacks()` and `luaEngine.setPopupCallbacks()`
- After callbacks set, call `luaEngine.registerApiTable()` (registers `api` global in Lua state with the callbacks)
- Wire `luaEngine.onReload` callback (replaces separate onActionReload + onConfigReload)
- `registerActions()`: call `luaEngine.registerActions(action)` then `luaEngine.buildKeyMap(action)`
- `applyConfig()`: unchanged (still reads from Config::getContext())

**MainComponentActions.cpp changes:**
- `reload_config` action: call `luaEngine.reload()` — which internally re-feeds Config and Whelmed::Config, fires their onReload callbacks
- Remove explicit `config.reload()` and `Whelmed::Config::getContext()->reload()` calls — lua::Engine handles this
- Action::List constructor: receives `lua::Engine&` instead of `Scripting::Engine&`

**Main.cpp changes:**
- Remove `config.onReload` and `whelmedConfig.onReload` wiring from `initialise()` — lua::Engine owns reload flow
- Pass `luaEngine` to MainComponent constructor
- Wire `luaEngine.onReload` → `mainComponent->applyConfig()` + window glass update

**lua::Engine additions:**
- `void registerApiTable()` — registers `api` global table in Lua state using stored callbacks. Called by MainComponent after callbacks are set. Also called during reload() to re-register in fresh state.
- `void reload()` — fresh Lua state, registerApiTable(), re-run end.lua, re-parse, re-feed Config (via Config::getContext()->feed()), re-feed Whelmed::Config (via Whelmed::Config::getContext()->feed()), fire Config::onReload, fire Whelmed::Config::onReload, fire own onReload

**Validation:** MainComponent compiles with lua::Engine reference. All actions register correctly. Custom Lua actions execute through api table. Selection keys resolve. BLESSED-B: lua::Engine sole owner. BLESSED-S (SSOT): single load/register pipeline.

---

### Step 7: Unified file watcher + reload flow

**Scope:** `Source/lua/Engine.cpp`

**Action:**

**fileChanged()** implementation:
- Any `.lua` file change in `~/.config/end/` triggers `reload()`
- Guard: check `nexus.auto_reload` from lua::Engine's own `configValues` map (key: `"nexus.auto_reload"`) — no dependency on Config::getContext()
- Coalesce timer: 300ms (same as current Scripting::Engine)
- No filename dispatch — total reload on every change (RFC decision 5)

**reload() flow** (finalize from Step 6):
1. Store current display/popup callbacks (survive reload)
2. Clear all parsed state
3. `lua = jam::lua::state {}` — fresh state
4. Open base/string/table/math libs
5. Call `registerApiTable()` — re-register `api` global with stored callbacks
6. Set `package.path`
7. Run `end.lua` via safe_script_file
8. Parse all tables (parseConfig, parseWhelmed, parseKeys, parsePopups, parseActions, parseSelectionKeys)
9. Feed Config: `Config::getContext()->feed (configValues)` + set hyperlink data
10. Feed Whelmed::Config: `Whelmed::Config::getContext()->feed (whelmedValues)`
11. Fire `Config::getContext()->onReload` (triggers Config consumers)
12. Fire `Whelmed::Config::getContext()->onReload` (triggers Whelmed consumers)
13. Fire `onReload` (triggers MainComponent → registerActions + buildKeyMap + applyConfig)

**Whelmed hot-reload is now free** — any `.lua` change reloads everything, including whelmed.lua (fixes the current gap where whelmed.lua had no file watcher).

**Validation:** Edit any .lua file in ~/.config/end/ → total reload fires. Config and Whelmed::Config receive updated values. Actions re-register. Key map rebuilds. auto_reload gate works. BLESSED-D: same end.lua always produces same state.

---

### Step 8: File patching (keybinding remap via Action List)

**Scope:** `Source/lua/EnginePatch.cpp`

**Action:**

Update patchKey() to target `keys.lua` instead of `action.lua`:
- File path: `~/.config/end/keys.lua`
- Key format: flat keys in the return table (e.g., `copy = "cmd+c"`)
- Regen strategy (from RFC): regenerate entire `keys.lua` from `default_keys.lua` template, substituting current in-memory key values

**Structure match check:**
- On load(), check whether `keys.lua` matches the generated structure (marker comment at top of generated file, e.g., `-- END-GENERATED v1`)
- If marker present → remap allowed
- If marker absent or file modified beyond marker → remap disabled, show message: "Binding via Action List only supported when keys.lua matches the default table structure."
- Expose `bool isKeyFileRemappable() const noexcept` for Action::List to check

**Validation:** Action List remap writes to keys.lua. Reload picks up change. Customized keys.lua disables remap with user message. BLESSED-E (Explicit): clear user-facing error message.

---

### Step 9: Cleanup + ARCHITECTURE.md

**Scope:** `Source/scripting/` (delete directory), `ARCHITECTURE.md`

**Action:**

**Delete Source/scripting/:**
- `Scripting.h`
- `Scripting.cpp`
- `ScriptingParse.cpp`
- `ScriptingPatch.cpp`

Verify no remaining `#include` references to `scripting/` anywhere in the codebase.

**Update ARCHITECTURE.md:**
- Module Map: replace `scripting/` section with `lua/` section
- Update `config/` section to reflect facade pattern
- Add lua::Engine lifecycle description
- Update file list (new files, removed files)
- Reflect single Lua state, single file watcher, unified reload

**Validation:** Clean compile with no references to deleted files. ARCHITECTURE.md matches actual codebase structure. @Auditor full sweep: MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md, all PLAN decisions.

---

## BLESSED Alignment

- **B (Bound):** lua::Engine sole owner of `jam::lua::state` and `jam::File::Watcher`. Config/Whelmed::Config own no Lua state. Deterministic lifecycle: Engine constructs first in ENDApplication, destroys last.
- **L (Lean):** Three parallel load/validate/write pipelines → one. Three Lua states → one. Three file watchers (effectively) → one.
- **E (Explicit):** Hierarchical key names with module prefix. Every key's origin visible in its name. No magic filenames in watcher dispatch.
- **S (SSOT):** lua::Engine is the single source for all configuration. Config and Whelmed::Config are views. No shadow state.
- **S (Stateless):** Config and Whelmed::Config are stateless readers — they hold no Lua state, no file watchers, no reload logic. They read what lua::Engine gives them.
- **E (Encapsulation):** 55 Config consumer call sites and 12 Whelmed::Config consumer call sites remain unchanged. The refactor is invisible to them.
- **D (Deterministic):** One load path, one Lua state. Same `end.lua` always produces same parsed state. No divergence between independent states.

## Risks / Open Questions

1. **Config::validateAndStore() location.** Currently Config validates values during load. With lua::Engine parsing, validation can stay in Config::feed() or move to EngineParse.cpp. Implementation decision — both are BLESSED-compliant. Engineer should follow whichever minimizes code movement.

2. **Construction order timing.** lua::Engine must construct and load() BEFORE Config reads from it. Since both are value members of ENDApplication, declaration order controls this. Verify Config's constructor can safely call `lua::Engine::getContext()` — the Context singleton is set during lua::Engine's constructor.

3. **Hyperlink data transfer.** Config currently owns `hyperlinkHandlers` and `hyperlinkExtensions` parsed from Lua. These must transfer from lua::Engine to Config. Either via feed() parameter expansion or a dedicated setter. Engineer decides the cleaner interface during Step 4.
