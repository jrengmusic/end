# RFC — Unified Lua Config Engine
Date: 2026-04-26
Status: Ready for COUNSELOR handoff

## Problem Statement

END has three separate Lua states loading three hardcoded files (`end.lua`, `whelmed.lua`, `action.lua`), each with its own load/reload/validate/writeDefaults pipeline. This prevents cross-file `require()`, duplicates infrastructure, and locks users into a rigid file layout. The generated config files serve as inline documentation — a strength worth preserving — but the architecture should let users compose freely with standard Lua.

## Research Summary

### Current Architecture (3 Lua states, 3 files)

| Owner | File | Lua state | Lifecycle |
|---|---|---|---|
| `Config` (ENDApplication) | `~/.config/end/end.lua` | Temporary — created, parsed, discarded | Constructed before window |
| `Whelmed::Config` (ENDApplication) | `~/.config/end/whelmed.lua` | Temporary — created, parsed, discarded | Constructed before window |
| `Scripting::Engine` (MainComponent) | `~/.config/end/action.lua` | Persistent — keeps `execute` functions alive | Constructed after window |

**Problems:**
1. Three Lua states — no cross-file references, no `require()` between configs.
2. Three copies of: state setup, file resolution, `writeDefaults()`, `load()`, `reload()`, `validateAndStore()`, template expansion.
3. Hardcoded filenames in file watcher (`"action.lua"`, `"end.lua"` string matches).
4. `whelmed.lua` has no auto-reload — missing branch in `fileChanged()`.
5. Users cannot reorganize config without modifying C++ source.

### Industry Pattern

No Lua-configured application generates monolithic default configs. The universal pattern is internal defaults + external docs (WezTerm, Neovim, Hammerspoon). However, END's generated-config-as-documentation is a genuine UX strength — users see every option with inline comments without leaving their editor.

### Resolution

Keep generating documented defaults. Make the engine filename-agnostic. One Lua state, one entry point (`end.lua`), `require()` for composition. Internal defaults cover anything the user omits.

## Principles and Rationale

### BLESSED Mapping

- **B (Bound):** `lua::Engine` is sole owner of the Lua state. Config and Whelmed::Config become reader facades with no lifecycle of their own. One owner, one state, deterministic destruction.
- **L (Lean):** Eliminates three parallel load/validate/write pipelines. One pipeline, one watcher, one reload path.
- **E (Explicit):** Key namespace gains hierarchy prefix (`display.colours.foreground`). Every key's module origin is explicit in its name.
- **S (SSOT):** `lua::Engine` is the single source of truth for all configuration. Config and Whelmed::Config are views, not sources. No shadow state — one Lua state produces all values.
- **S (Stateless):** Config and Whelmed::Config are stateless readers. They hold no Lua state, no file watchers, no reload logic. They read what `lua::Engine` gives them.
- **E (Encapsulation):** 40+ call sites of `Config::getContext()` remain unchanged. The refactor is invisible to consumers. `lua::Engine` orchestrates; consumers consume.
- **D (Deterministic):** Same `end.lua` always produces same parsed state. One load path eliminates the possibility of divergence between three independent Lua states.

### Design Decisions (ARCHITECT-approved)

1. **`lua::Engine` absorbs config orchestration.** Renamed from `Scripting::Engine`. Owns the persistent `jam::lua::state`, runs `end.lua`, parses all tables, feeds Config and Whelmed::Config.
2. **Config and Whelmed::Config become thin reader facades.** Same `getContext()` singleton pattern, same `Key::*` constants. 40+ call sites untouched. They no longer own Lua states, file resolution, or reload logic.
3. **`lua::Engine` constructs in `ENDApplication`** (before window). Loads and parses `end.lua` immediately. The `api` Lua table is registered later when `MainComponent` wires display callbacks — custom action `execute` functions resolve `api` at call time (global lookup), not at parse time.
4. **Everything nests under `END` table.** Six sub-tables: `nexus`, `display`, `whelmed`, `keys`, `popups`, `actions`.
5. **Reload is total.** Any `.lua` change in `~/.config/end/` triggers full reload — fresh Lua state, re-parse all tables, re-register actions, rebuild key map. No partial reloads.
6. **`display` Lua API renamed to `api`.** Avoids collision with `END.display` config namespace. Extensible beyond display operations.
7. **Popup defaults live in `popups.lua`** alongside popup entries. Popup border styling (visual) lives in `display.lua`.
8. **Window config absorbed into `display.lua`.** No split — all window settings in one module.
9. **Key names stay explicit.** `foreground` not `fg`. Per NAMES.md Rule 4: clarity over brevity.

## Scaffold

### User-facing file layout (`~/.config/end/`)

```
end.lua          <- entry point (only require() calls)
nexus.lua        <- terminal behavior
display.lua      <- all visual: window, colours, cursor, font, tab, pane, overlay, menu, action_list, status_bar, popup borders
whelmed.lua      <- whelmed viewer config
keys.lua         <- keybindings, prefix, selection keys
popups.lua       <- popup terminals + popup defaults (cols, rows, position)
actions.lua      <- custom actions with execute functions
```

### `end.lua` (generated entry point)

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

### Module format (each generated file)

Each module is a self-contained Lua file that `return`s a table. Example `nexus.lua`:

```lua
-- [banner + documentation comments]
return {
    gpu = "auto",
    daemon = "false",
    auto_reload = "true",
    shell = {
        program = "zsh",
        args = "-l",
        integration = "true",
    },
    terminal = { ... },
    hyperlinks = { ... },
}
```

### Config key namespace migration

Keys gain module prefix. Internal `Config::Key` constants update:

| Current key | New key |
|---|---|
| `gpu` | `nexus.gpu` |
| `daemon` | `nexus.daemon` |
| `auto_reload` | `nexus.auto_reload` |
| `font.family` | `display.font.family` |
| `colours.foreground` | `display.colours.foreground` |
| `window.title` | `display.window.title` |
| `window.opacity` | `display.window.opacity` |
| `tab.family` | `display.tab.family` |
| `shell.program` | `nexus.shell.program` |
| `terminal.scrollback_lines` | `nexus.terminal.scrollback_lines` |
| `popup.cols` | `popups.cols` |
| `popup.rows` | `popups.rows` |
| `popup.border_colour` | `display.popup.border_colour` |
| `action_list.position` | `display.action_list.position` |

Whelmed keys remain flat (no module prefix needed — Whelmed::Config is already namespaced by class).

### Module partition detail

**`nexus.lua`** — terminal behavior:
- `gpu`, `daemon`, `auto_reload`
- `shell` — program, args, integration
- `terminal` — scrollback_lines, scroll_step, padding, drop_multifiles, drop_quoted
- `hyperlinks` — editor, handlers, extensions

**`display.lua`** — all visual and window:
- `window` — title, width, height, colour, opacity, blur_radius, always_on_top, save_size, confirmation_on_exit, buttons, force_dwm
- `colours` — foreground, background, cursor, selection, selection_cursor, ANSI 0-15, status_bar, status_bar_label_bg, status_bar_label_fg, status_bar_spinner, status_bar_font_family, status_bar_font_size, status_bar_font_style, hint_label_bg, hint_label_fg
- `cursor` — char, blink, blink_interval, force
- `font` — family, size, ligatures, embolden, line_height, cell_width, desktop_scale
- `tab` — family, size, foreground, inactive, position, line, active, indicator
- `pane` — bar_colour, bar_highlight
- `overlay` — family, size, colour
- `menu` — opacity
- `action_list` — close_on_run, position, name_font_family, name_font_style, name_font_size, shortcut_font_family, shortcut_font_style, shortcut_font_size, padding, name_colour, shortcut_colour, width, height, highlight_colour
- `status_bar` — position
- `popup` — border_colour, border_width

**`whelmed.lua`** — unchanged content, wrapped in `return { ... }`.

**`keys.lua`** — all keybinding config:
- `prefix`, `prefix_timeout`
- Built-in keys: copy, paste, quit, close_tab, reload, zoom_in, zoom_out, zoom_reset, new_window, new_tab, prev_tab, next_tab, split_horizontal, split_vertical, pane_left, pane_down, pane_up, pane_right, newline, action_list, enter_selection, enter_open_file, open_file_next_page
- Selection keys: selection_up, selection_down, selection_left, selection_right, selection_visual, selection_visual_line, selection_visual_block, selection_copy, selection_top, selection_bottom, selection_line_start, selection_line_end, selection_exit

**`popups.lua`** — popup entries + defaults:
- `defaults` — cols, rows, position
- Named popup entries (tit, cake, btop, etc.)

**`actions.lua`** — custom actions with `execute` functions. References `api` global for display operations.

### C++ architecture

```
lua::Engine (ENDApplication)          <- owns jam::lua::state, runs end.lua
    |
    |-- feeds --> Config              <- thin reader facade, same Key::* / getContext()
    |-- feeds --> Whelmed::Config     <- thin reader facade, same getContext()
    |
    |-- owns file watcher             <- watches ~/.config/end/, any .lua triggers reload
    |-- owns parsed state             <- keyBindings, popupEntries, customActions, selectionKeys
    |-- exposes registerActions()     <- called by MainComponent after built-ins registered
    |-- exposes buildKeyMap()         <- called after all actions registered
    |
    |-- api table                     <- registered in Lua state when MainComponent wires callbacks
```

### Initialization sequence

1. `lua::Engine` constructs in `ENDApplication` (before window).
2. `lua::Engine::load()` — sets `package.path` to include `~/.config/end/?.lua`, runs `end.lua`, parses `END.*` tables into internal state. Feeds Config and Whelmed::Config.
3. `Config` and `Whelmed::Config` construct as reader facades — read values from `lua::Engine`, no Lua state of their own.
4. `AppState` constructs (reads from Config as before).
5. Window + `MainComponent` created.
6. `MainComponent` wires `api` table into `lua::Engine`'s Lua state (display callbacks + popup callbacks).
7. `MainComponent::registerActions()` — registers built-in actions, then calls `lua::Engine::registerActions()` for popups + custom actions, then `buildKeyMap()`.

### Reload flow

1. File watcher detects any `.lua` change in `~/.config/end/`.
2. Guard: `lua::Engine` checks `nexus.auto_reload` from its own parsed state.
3. Fresh Lua state created, `api` table re-registered with stored callbacks, `end.lua` re-executed.
4. All tables re-parsed, Config and Whelmed::Config re-fed.
5. `onReload` callback fires — MainComponent re-registers actions, rebuilds key map, applies config to all terminals.

### Source file changes

| Current | New |
|---|---|
| `Source/scripting/Scripting.h` | `Source/lua/Engine.h` |
| `Source/scripting/Scripting.cpp` | `Source/lua/Engine.cpp` |
| `Source/scripting/ScriptingParse.cpp` | `Source/lua/EngineParse.cpp` |
| `Source/scripting/ScriptingPatch.cpp` | `Source/lua/EnginePatch.cpp` |
| `Source/config/default_end.lua` | `Source/config/default_end.lua` (thin require-only entry point) |
| `Source/config/default_action.lua` | `Source/config/default_actions.lua` |
| — | `Source/config/default_nexus.lua` (new) |
| — | `Source/config/default_display.lua` (new) |
| — | `Source/config/default_keys.lua` (new) |
| — | `Source/config/default_popups.lua` (new) |
| `Source/config/default_whelmed.lua` | `Source/config/default_whelmed.lua` (format change: `return {}` instead of `WHELMED = {}`) |

### BinaryData changes

Seven embedded templates instead of three. CMakeLists.txt resource list updated. `BinaryData::getString()` calls updated to new filenames.

## BLESSED Compliance Checklist

- [x] Bound — lua::Engine sole owner of Lua state, deterministic lifecycle
- [x] Lean — one pipeline replaces three, no duplicate infrastructure
- [x] Explicit — hierarchical key names, no magic filenames, visible module origin
- [x] SSOT — lua::Engine is the single config source, facades are views
- [x] Stateless — Config/Whelmed::Config are pure readers, no state of their own
- [x] Encapsulation — 40+ consumer call sites unchanged, refactor invisible to them
- [x] Deterministic — one load path, one Lua state, no divergence possible

## Resolved Questions

1. **`lua::Engine` namespace.** Confirmed no collision with `jam::lua` — `jam::lua` is nested inside `jam`, `lua::Engine` is top-level.

2. **First-launch error handling.** No `pcall` wrapping. Lua surfaces file:line errors naturally. `require()` failure shows which file, which line — same pattern as current error handling.

3. **Migration.** No migration logic. END has no public binary release. Clean slate.

4. **File patching (keybinding remap via Action List).** Regen from `default_keys.lua` template, rewriting the entire file with current in-memory key values. On load, `lua::Engine` checks whether `keys.lua` matches the generated structure (marker comment or hash). If it does not match (user customized the Lua structure), the Action List remap feature is disabled — immediately show message: "Binding via Action List only supported when keys.lua matches the default table structure." User edits `keys.lua` manually in that case.

## Handoff Notes

- The Pathfinder audit mapped every consumer of Config (40+ call sites), Whelmed::Config, and Scripting::Engine with file:line precision. COUNSELOR should reference that audit when writing PLAN.md task breakdown.
- RFC-tabs.md exists as a separate in-flight RFC — no interaction with this work.
- ARCHITECTURE.md section "Module Structure" references `scripting/` directory and `config/` directory — needs updating after this refactor.
- The `display` API table (now `api`) currently registers 9 functions. ARCHITECT noted this is extensible — future APIs beyond display operations can be added to the `api` table.
- `Whelmed::Config` currently has a gap: no auto-reload via file watcher. This refactor fixes it for free — any `.lua` change triggers total reload.
