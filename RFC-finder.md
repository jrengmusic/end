# RFC — END Native Finder
Date: 2026-05-01
Status: Ready for COUNSELOR handoff

## Problem Statement

END runs fzf as an external process — a separate binary with its own TUI renderer, its own event loop, its own PTY. This works but wastes everything END already owns: GPU rendering, the action system, shell integration, image preview (SKiT), and the entire terminal state.

ARCHITECT wants fzf's exact interactive experience — same scoring, same query syntax, same feel — but native to END. No subprocess. The algorithm lives in `jam::Fuzzy`. END provides the body: UI, sources, actions, preview, configuration.

## What jam::Fuzzy Provides (see RFC-fuzzy-finder.md in jam/)

| Component | What It Does |
|-----------|-------------|
| `Fuzzy::matchV1/V2` | Pure scoring functions (static, dual-mode ASCII/Unicode) |
| `Fuzzy::exactMatch/prefixMatch/suffixMatch/equalMatch` | Non-fuzzy match variants |
| `Fuzzy::Pattern` | Parsed query with AND/OR token groups, fzf syntax |
| `Fuzzy::Slab` | Per-thread arena, zero allocation in hot path |
| `Fuzzy::Finder` | Concurrent pipeline — owns Slabs, threads, publishes sorted results via `jam::Mailbox` |
| `FuzzyScoring` | Constants, CharClass, BonusMatrix — fzf's exact scoring engine |

END does NOT reimplement any of this. END consumes `Fuzzy::Finder` as a black box: feed items, set pattern, read results.

## Existing END Patterns (Reused, Not Reinvented)

| Pattern | Location | How Finder Uses It |
|---------|----------|-------------------|
| `Terminal::Popup` + `ModalWindow` | `component/Popup.h`, `ModalWindow.h` | Hosts Finder as glass overlay, JUCE modal state blocks PTY input |
| `Action::Registry` | `action/Action.h` | Registers Finder actions, Lua-configurable keybindings |
| `Action::List` (command palette) | `action/ActionList.h` | Architectural model — row-based list, search input, key dispatch |
| `Action::KeyHandler` | `action/KeyHandler.h` | Two-table dispatch (search mode / navigation mode) |
| `lua::Engine` | `lua/Engine.h` | Configuration: sources, keybindings, appearance, preview commands |
| `jam::Mailbox<T>` | `jam_core/concurrency/jam_mailbox.h` | Lock-free result delivery from Finder threads to UI |
| SKiT / `PreviewScreen` | `rendering/PreviewScreen.h` | Image preview for file results (existing pipeline) |

**No new ModalType needed.** `Popup`/`ModalWindow` enters JUCE modal state via `enterModalState()` — PTY input is already blocked. `ModalType` is for terminal-level interception; Popup-level modality is handled by JUCE.

## Architecture

### Component Structure

```
Terminal::Finder : juce::Component, juce::Timer
│
├── queryInput          ← juce::TextEditor (single line, bottom)
├── resultList          ← custom painted list (scrollable, match highlighting)
├── infoLine            ← match count / total count / spinner
├── previewPane         ← juce::Component (text preview, optional right panel)
│
├── jam::Fuzzy::Finder  ← owned, concurrent pipeline
├── source              ← item provider (async)
└── onSelect            ← std::function callback, fires on Enter
```

Hosted via `popup.show (*this, std::move (finder), width, height, renderer)`.

### Data Flow

```
1. Action fires          → MainComponent creates Terminal::Finder with source config
2. popup.show()          → ModalWindow enters modal state, PTY blocked
3. Source populates      → Finder::setItems (async for external commands)
4. User types            → queryInput.onTextChange → Pattern::parse → Finder::setPattern
5. Timer tick (16ms)     → Finder::readResults via Mailbox → update resultList
6. User navigates        → Arrow keys move cursor, scroll follows
7. User selects (Enter)  → onSelect fires with selected item
8. Action performed      → paste to PTY / scroll to line / cd / open
9. Escape                → ModalWindow exits modal state, Popup dismissed
```

### Sources

Each source is a configuration: where items come from, what action to perform on selection, what preview to show.

| Source | Items | On Select | Preview |
|--------|-------|-----------|---------|
| Files | External command output (`fd`, `find`, `rg`) | Paste path / open / cd | File content (head -N) or SKiT image |
| Scrollback | `Grid::extractText` per line, full history | Scroll terminal to line | Surrounding context lines |
| History | Shell history file (`~/.zsh_history`, etc.) | Paste command to PTY | None |
| Actions | `Action::Registry::getEntries()` | Run action | None |
| Custom | Lua-configured command | Lua-configured action | Lua-configured preview |

**Source abstraction:**

```cpp
struct FinderSource
{
    juce::String name;
    std::function<std::vector<juce::String>()> loadItems;
    std::function<void (const juce::String& selected)> onSelect;
    std::function<juce::String (const juce::String& selected)> getPreview;
    Fuzzy::Scheme scheme { Fuzzy::Scheme::standard };
};
```

`loadItems` may be blocking (file I/O, subprocess) — called on a background thread. Items delivered to Finder via `setItems` on completion. For streaming sources (long-running commands), items can be fed incrementally.

### Key Dispatch

Follows `Action::KeyHandler` two-table pattern:

**Search mode** (query input focused):
- Character keys → query input
- Enter → select current item
- Escape → dismiss
- Ctrl+N / Ctrl+P or Arrow Down/Up → move selection
- Ctrl+U → clear query
- Tab → toggle selection (multi-select mode)

**Navigation mode** (result list focused):
- Arrow keys → cursor movement
- Enter → select
- Escape → dismiss or return to search mode
- / → return to search mode

Lua-configurable. Default bindings mirror fzf.

### Preview

Two preview modes, selected per source:

1. **Text preview** — rendered in `previewPane` as a `juce::TextEditor` (read-only, monospaced) or custom painted component. Shows file content, command output, or context lines. Spawns preview command on background thread, streams output.

2. **Image preview** — routes through SKiT. `PreviewScreen` already handles split-viewport image rendering. Finder sets `State::setPreviewActive(true)` with image metadata. Existing GL pipeline draws the quad.

Preview updates on cursor movement. Previous preview cancelled (same pattern as fzf's `previewCancelWait=500ms`).

### Rendering

Finder is a **JUCE component**, not part of the GL pipeline. Same pattern as `Action::List`:
- `paint(g)` draws query input, result rows, info line, preview
- Match position highlighting via `Fuzzy::matchV2` with `positions` output
- Scrollbar for long result lists
- Hosted inside `ModalWindow` glass overlay (blur, opacity from Lua config)

Result rows are custom-painted (not child components) for performance with 100k+ items. Only visible rows are painted. Virtual scrolling — `offset` tracks top visible row, `cursor` tracks selected row.

### Lua Configuration

```lua
-- finder.lua (loaded by Engine)
finder = {
    width = 0.8,         -- fraction of window
    height = 0.6,
    preview = {
        enabled = true,
        position = "right",  -- right, top, bottom
        ratio = 0.5,
    },
    sources = {
        files = {
            command = "fd --type f --hidden --follow --exclude .git",
            scheme = "path",
            bind = { "ctrl+t" },
            on_select = "paste",       -- paste, open, cd
        },
        history = {
            command = "cat ~/.zsh_history",
            scheme = "history",
            bind = { "ctrl+r" },
            on_select = "paste",
        },
        scrollback = {
            scheme = "standard",
            bind = { "ctrl+f" },
            on_select = "jump",
        },
        actions = {
            scheme = "standard",
            bind = { "ctrl+shift+p" },  -- replaces ActionList
            on_select = "run",
        },
    },
    keys = {
        ["ctrl+n"] = "next",
        ["ctrl+p"] = "prev",
        ["ctrl+u"] = "clear-query",
        ["tab"]    = "toggle",
        ["enter"]  = "accept",
        ["escape"] = "dismiss",
    },
}
```

Actions registered in `MainComponentActions.cpp`:
```cpp
registry.registerAction ("finder-files", "Find Files", "Fuzzy file finder",
    "Finder", false, [this] { return showFinder ("files"); });
registry.registerAction ("finder-history", "Find History", "Fuzzy history search",
    "Finder", false, [this] { return showFinder ("history"); });
registry.registerAction ("finder-scrollback", "Find in Scrollback", "Fuzzy scrollback search",
    "Finder", false, [this] { return showFinder ("scrollback"); });
registry.registerAction ("finder-actions", "Find Actions", "Fuzzy action finder",
    "Finder", false, [this] { return showFinder ("actions"); });
```

### Scrollback Source (Special Case)

Scrollback is the only source that reads END's internal state rather than an external command.

```
Grid::extractText (startRow, endRow, scrollOffset) → juce::String per line
```

Called on MESSAGE THREAD with `resizeLock` held. Items = all lines from history head to current viewport. Line index preserved in `Fuzzy::Finder::RankedResult::itemIndex` — maps back to grid row for scrolling.

On select: `State::setScrollOffset (targetRow)` → terminal scrolls to the matched line. Match highlighted briefly (flash overlay or selection).

### Multi-Select

Optional per source. When enabled:
- Tab toggles item selection (same as fzf)
- `selected: std::unordered_map<int, juce::String>` keyed by item index
- Enter with selections → `onSelect` fires once per selected item (or once with all)
- Info line shows selection count

### Actions Source (ActionList Replacement)

When source is `"actions"`, items come from `Action::Registry::getEntries()` — the same data `Action::List` uses. This makes `Terminal::Finder` a superset of `Action::List`:
- Same action entries
- Better scoring (fzf algorithm vs substring + Levenshtein)
- Same popup pattern
- Preview not needed

Migration path: `Action::List` continues to work. `finder-actions` is an alternative binding. If ARCHITECT decides Finder fully replaces ActionList, ActionList can be removed in a future sprint.

## File Structure

```
Source/
  finder/
    Finder.h                  ← Terminal::Finder component
    Finder.cpp                ← component implementation, paint, key handling
    FinderSource.h            ← FinderSource struct, built-in source factories
    FinderSource.cpp          ← source implementations (scrollback, files, history, actions)
```

Four files, clean separation: component vs data sources.

## BLESSED Compliance Checklist

- [x] **B (Bound)** — Finder owns its Fuzzy::Finder. Popup owns ModalWindow. Source callbacks own their captures. No dangling references — Popup lifetime contains Finder lifetime.
- [x] **L (Lean)** — 4 files, 2 concerns (component, sources). Finder component under 300 lines (custom paint, key handling, timer). Sources under 300 lines (4 source factories + source struct).
- [x] **E (Explicit)** — All configuration via Lua with named fields. No magic keys. Source struct declares all callbacks explicitly. Match positions visible in result rendering.
- [x] **S (SSOT)** — Scoring lives in jam::Fuzzy only. Source config lives in Lua only. Action registration lives in MainComponentActions only. No shadow state.
- [x] **S (Stateless)** — Finder component is transient (created on show, destroyed on dismiss). No persistent state between invocations. Fuzzy::Finder is owned and destroyed with the component.
- [x] **E (Encapsulation)** — Finder is a juce::Component, knows nothing about Screen/GL/Parser. Sources are std::function callbacks, know nothing about Finder internals. Fuzzy::Finder is a black box — feed items, read results.
- [x] **D (Deterministic)** — Same items + same query → same results in same order. Source loading is the only async path, and results are deterministic once items arrive.

## Open Questions

| # | Question | Context |
|---|----------|---------|
| 1 | **Component name** | `Terminal::Finder`? Conflicts linguistically with `Fuzzy::Finder` (different namespace, but same word). Alternatives: `Terminal::FuzzyOverlay`, `Terminal::FinderView`, or keep `Terminal::Finder` (namespaces disambiguate). |
| 2 | **ActionList replacement** | Should Finder with `actions` source fully replace `Action::List` in the long term? Or coexist? ActionList has binding mode (rebind keys inline) that Finder would not replicate. |
| 3 | **Streaming sources** | Long-running commands (`rg` on large codebases) produce items over time. Feed incrementally to `Fuzzy::Finder::setItems` (replacing full set each time), or add `appendItems` to the Finder API? Incremental requires jam::Fuzzy RFC amendment. |
| 4 | **Text preview component** | Custom painted (consistent look, match highlighting in preview) vs `juce::TextEditor` (less code, scroll for free, no highlighting)? |
| 5 | **Scrollback flash** | When jumping to a scrollback match, briefly highlight the matched line. Flash overlay? Selection highlight? Timer-based fade? |

## Handoff Notes

- Depends on `jam::Fuzzy` module (see `~/Documents/Poems/dev/jam/RFC-fuzzy-finder.md`). Fuzzy must be implemented first.
- `Action::List` pattern at `Source/action/ActionList.h` is the closest existing analog — study its row model, key handler, and Popup integration.
- `Terminal::Popup::show()` at `Source/component/Popup.h` is the hosting mechanism.
- Scrollback extraction via `Grid::extractText()` at `Source/terminal/logic/Grid.h` — MESSAGE THREAD, needs `resizeLock`.
- SKiT image preview at `Source/terminal/rendering/PreviewScreen.h` — existing pipeline for image file preview.
- Shell integration scripts at `Source/terminal/shell/` — history file paths vary by shell (zsh, bash, fish, pwsh).
- Lua Engine at `Source/lua/Engine.h` — `Engine::Display` struct would gain a `Finder` section for configuration.
- Action registration at `Source/MainComponentActions.cpp` — add finder actions alongside existing action registrations.
