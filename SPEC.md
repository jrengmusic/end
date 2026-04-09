# SPEC.md

## END: Ephemeral Nexus Display — Roadmap

**Version:** 0.4.0
**Last Updated:** 2026-03-21

**Purpose:** Forward-looking plan. What is NOT yet implemented. For current codebase documentation, see ARCHITECTURE.md. For detailed future feature specs, see SPEC-details.md.

---

## Overview

**Purpose:** GPU-accelerated, fully-featured, Lua-configurable terminal emulator.

**Target User:** Power users who live in the terminal.

**Core Workflow:** Launch -> shell ready -> work in tmux/vim/TUIs -> configure everything with Lua.

**Design Philosophy:** Performance and beauty. Opinionated defaults, everything overridable.

**End Game:** WHELMED — WYSIWYG Hybrid Encoder Lightweight Markdown Editor with mermaid renderer — is integrated as `Whelmed::Component` in split panes. Both END and WHELMED share a common GL text rendering module (shared via `jreng_graphics` module (`jreng::TextLayout`)).

---

## Unimplemented Features

### Input

| Feature | Status | Notes |
|---------|--------|-------|
| Focus events (`\x1b[I`/`\x1b[O`) | **Done** | `session.writeFocusEvent()` on `focusGained`/`focusLost` |
| Unified keybinding system | **Done** | Action registry, global + modal, configurable via end.lua |
| Command palette | **Done** | ActionList with GlassWindow |
| Cross-platform modifier mapping | **Done** | `parseShortcut` handles cmd/ctrl mapping per platform |

### Selection

| Feature | Status | Notes |
|---------|--------|-------|
| Auto-copy on mouse release | Not implemented | User copies with y or cmd+c |
| Word selection (double-click) | **Done** | |
| Line selection (triple-click) | **Done** | |
| Keyboard selection mode | **Done** | Vim-like modal: visual/visual-line/visual-block |
| Selection in scrollback | **Done** | Selection cursor navigates scrollback |

### OSC

| Feature | Status | Notes |
|---------|--------|-------|
| OSC 52 base64 decode | **Done** | Fixed Latin-1 constructor |
| OSC 7 (cwd tracking) | **Done** | Shell scripts emit OSC 7; OS query as fallback |
| OSC 8 (hyperlinks) | **Done** | Parsed in oscDispatch, spans merged with heuristic links |
| OSC 133 (shell integration) | **Done** | A/B/C/D markers, output block tracking, automatic injection (zsh/bash/fish/pwsh) |
| OSC 9/777 (notifications) | **Done** | Native desktop notification (macOS UNUserNotificationCenter, Windows/Linux fallback) |

### Bell

BEL character (`\x07`) — **Done**. Passes BEL to stderr.

### Rendering

| Feature | Status | Notes |
|---------|--------|-------|
| Software renderer fallback | **Done** | GraphicsTextRenderer + SIMD compositing (Plan 3 + optimization sprint) |
| Sixel / inline images | Not implemented | See Inline Image Rendering below |

### Shell Integration

| Feature | Status | Notes |
|---------|--------|-------|
| Shell integration (OSC 133) | **Done** | Auto-inject via ZDOTDIR/ENV/XDG_DATA_DIRS/pwsh args |
| File opener (open-file mode) | **Done** | Hint labels, click mode, editor dispatch via PTY |
| Clickable hyperlinks | **Done** | Underlines on OSC 133 output rows, mouse click dispatch |

### Platform

| Feature | Status | Notes |
|---------|--------|-------|
| Windows ConPTY | **Done** | NT API duplex pipe, overlapped I/O, Windows 10/11 |
| Configurable shell in Lua | **Done** | `shell.program` and `shell.args` in end.lua |
| Error display on invalid config | **Done** | MessageOverlay on startup and reload errors |

### State Persistence

| Feature | Status | Notes |
|---------|--------|-------|
| Terminal state serialization | **Done** | Grid+State snapshot via Processor::getStateInformation/setStateInformation |
| Session restore on launch | Spec written | See SPEC-details.md Section 1 |
| Multi-window support | **Done** | Cmd+N spawns new instance; `moreThanOneInstanceAllowed = true` |

---

## Feature Specs

### Keybinding Reorganization

**Goal:** Fully configurable keybinding system. Nothing hardcoded except defaults. All actions assignable globally (direct shortcut) or modally (prefix key + action key), or both. User overrides everything via `end.lua`.

**Implemented.** Single unified action registry (`Action::Registry`) with global and modal bindings, fully configurable via `end.lua`. Prefix key system with configurable timeout. Command palette (`Action::List`) for discovery and inline shortcut remapping.

**Design:**

- **Action registry** — central table of all actions with string names:
  - `copy`, `paste`, `quit`, `reload_config`
  - `zoom_in`, `zoom_out`, `zoom_reset`
  - `split_horizontal`, `split_vertical`
  - `pane_left`, `pane_right`, `pane_up`, `pane_down`
  - `close_pane`, `close_tab`, `new_tab`
  - `scroll_up`, `scroll_down`, `scroll_top`, `scroll_bottom`
  - User-defined popup actions (see Popup section)
  - Any future actions

- **Binding modes:**
  - **Global** — direct shortcut, no prefix. e.g., `Cmd+C` -> `copy`
  - **Modal** — prefix key + action key. e.g., `` ` `` then `\` -> `split_horizontal`
  - **Both** — same action can have global AND modal bindings simultaneously

- **Prefix key** — optional. If user sets `keys.prefix = ""` or omits it, modal mode is disabled entirely. All actions must then have global bindings.

- **Config schema:**
  ```lua
  END = {
      keys = {
          prefix = "`",              -- optional, "" disables modal mode
          prefix_timeout = 1000,     -- ms

          -- global bindings (no prefix needed)
          copy = "cmd+c",
          paste = "cmd+v",
          quit = "cmd+q",
          reload_config = "cmd+r",
          zoom_in = "cmd+=",
          zoom_out = "cmd+-",
          zoom_reset = "cmd+0",
          new_tab = "cmd+t",
          close_tab = "cmd+w",
          command_palette = "cmd+p",

          -- modal bindings (prefix + key)
          split_horizontal = "\\",
          split_vertical = "-",
          pane_left = "h",
          pane_right = "l",
          pane_up = "k",
          pane_down = "j",
          close_pane = "x",

          -- user can override anything:
          -- split_horizontal = "cmd+shift+\\",  -- global, no prefix needed
      },
  }
  ```

- **Resolution order:** User config overrides defaults. If a key string contains `+` modifiers (cmd, ctrl, shift, alt), it's a global binding. If it's a single character, it's a modal binding (requires prefix). User can force global by writing `"cmd+h"` instead of `"h"`.

- **Replaces:** Current `KeyBinding` (ApplicationCommandManager wrapper) and `ModalKeyBinding` (prefix-key system) merge into one unified system.

---

### Command Palette

**Goal:** Searchable action launcher. Fuzzy text box with dropdown menu. Lists all registered actions + user-defined popup commands.

**Trigger:** Configurable shortcut (default: `Cmd+P`).

**Behavior:**

1. User presses command palette shortcut
2. Modal glass window appears with fuzzy search dropdown
3. All registered actions listed (from action registry)
4. Fuzzy search filters as user types
5. Each entry shows: action name + current keybinding (if any)
6. User-defined popup commands (see Popup section) also listed
7. Select entry -> execute action
8. Escape -> dismiss

**Content sources:**
- All actions from the keybinding action registry
- User-defined popup entries from `end.lua`
- Any configurable action that has a string name

**Implementation note:** Implemented as a JUCE GlassWindow component (not a native OS dialog). Fuzzy-searchable list of all registered actions.

---

### Tmux-Style Popup

**Goal:** User-configurable modal popup terminals. Spawn any TUI app or shell script in a floating window. Configurable size, working directory, and keyboard shortcut.

**Behavior:**

1. User presses popup shortcut (configurable per popup entry)
2. Floating terminal window spawns over the current terminal
3. New PTY opened in specified working directory
4. Specified command executed
5. Popup closes when:
   - The spawned process exits (user quits the TUI / script finishes)
   - END quits (all popup PTYs killed)
6. Popup is modal — input goes to popup until dismissed

**Config schema:**

```lua
END = {
    popups = {
        {
            name = "TIT",
            command = "nvim",
            cwd = "~/Documents/Poems/dev/TIT/",
            width = 0.8,       -- fraction of terminal width
            height = 0.6,      -- fraction of terminal height
            key = "t",         -- modal: prefix + t. or "cmd+shift+t" for global
        },
        {
            name = "lazygit",
            command = "lazygit",
            cwd = "",          -- empty = inherit current terminal cwd
            width = 0.9,
            height = 0.9,
            key = "g",
        },
        {
            name = "htop",
            command = "htop",
            cwd = "~",
            width = 0.7,
            height = 0.5,
            key = "p",
        },
    },
}
```

**Properties per popup entry:**

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Display name (shown in command palette) |
| `command` | string | Shell command or path to executable |
| `cwd` | string | Working directory. Empty = inherit active terminal's cwd |
| `width` | float | Fraction of parent terminal width (0.0-1.0) |
| `height` | float | Fraction of parent terminal height (0.0-1.0) |
| `key` | string | Keyboard shortcut (modal or global, same rules as keybinding) |

**Popup actions are registered in the action registry** and appear in the command palette as `popup:<name>` (e.g., `popup:lazygit`).

**Rendering:** Popup is a `juce::Component` child of the main terminal window. Contains its own `Terminal::Component` with its own PTY, Session, Grid, State. Uses the shared `Fonts` context and GL renderer. Drawn as an overlay with optional border/shadow.

---

### File Opener (ls Flash-Jump)

**Status: Implemented.** See Sprint 110 in SPRINT-LOG.md for implementation details.

**Goal:** Shell integration for opening files from terminal output. Files displayed by `ls` (or any command) become clickable and keyboard-jumpable.

**Inspiration:** `folke/flash.nvim` — overlay hint labels on jumpable targets.

**Two modes:**

**1. Click mode (always active):**
- END detects file paths in terminal output (ls output, find output, etc.)
- Detected paths are clickable — mouse click opens the file
- Open action: OS default app (`open` on macOS, `xdg-open` on Linux, `start` on Windows)

**2. Flash-jump mode (keyboard shortcut activated):**
- User presses file opener shortcut (configurable, e.g., prefix + `f`)
- END scans visible terminal content for file paths
- Each detected file gets a hint label overlay (single char or two-char label, like flash.nvim)
- User types the hint label -> file opens
- Escape -> dismiss hints
- Labels assigned by proximity to cursor or alphabetical order

**Detection heuristics:**
- Parse visible cells for path-like strings
- Context-aware: after `ls`, `find`, `tree`, `fd` commands
- Relative paths resolved against active terminal's cwd
- Absolute paths used as-is
- Validate: only label paths that exist on disk (async stat check)

**Config:**

```lua
END = {
    file_opener = {
        key = "f",                  -- modal: prefix + f
        open_command = "open",      -- macOS default. "xdg-open" on Linux
        labels = "asdfghjkl",       -- hint label characters
    },
}
```

---

### Inline Image Rendering (Sixel + iTerm2)

**Goal:** Render images inline in the terminal grid. Support Sixel protocol and iTerm2 inline images (OSC 1337).

**Protocols:**

| Protocol | Trigger | Format |
|----------|---------|--------|
| Sixel | DCS `P...q` ... ST | Sixel pixel data, palette-based |
| iTerm2 | `\x1b]1337;File=...` ST | Base64-encoded image (PNG, JPEG, GIF) |

**Rendering:**
- Decode image data to RGBA pixel buffer
- Upload as a texture region (separate from glyph atlas, or dedicated image atlas)
- Image occupies a rectangular cell region in the grid
- Cells covered by image marked with a layout flag (similar to wide-char continuation)
- GL pipeline renders image quads alongside glyph quads (additional draw call)
- Scrollback: images scroll with content, stored as cell references to texture regions

**Cell integration:**
- Image cells store a reference (image ID + position within image)
- When image scrolls off screen and out of scrollback, texture region freed
- LRU eviction for image textures if VRAM budget exceeded

**Performance budget:**
- Decode on message thread (async if large)
- Upload via staged bitmap queue (same as glyph atlas)
- No decode on GL thread
- Target: 60fps with multiple inline images visible

---

### Generalized GL Text Rendering Module (jreng_text)

**Goal:** Extract END's glyph atlas, HarfBuzz shaping, and instanced quad rendering into a reusable JUCE module. Provides `juce::AttributedString`-compatible surface API with OpenGL backend rendering.

**Parallel to existing modules:**
- `jreng_opengl` provides `GLGraphics` (like `juce::Graphics`) and `GLPath` (like `juce::Path`)
- `jreng_text` provides GL text rendering (like `juce::AttributedString` / `juce::TextLayout`)

**Two rendering modes:**

| Mode | Consumer | Layout | Font |
|------|----------|--------|------|
| Monospace cell grid | END terminal | Fixed cell grid, ring buffer rows | Monospace + NF + emoji |
| Attributed text | WHELMED | Proportional, line-wrapped, styled runs | Any font, mixed sizes |

**Shared infrastructure (module core):**
- GlyphAtlas (shelf-packed, LRU, dual mono/emoji)
- HarfBuzz shaping (per-run)
- FontCollection (codepoint -> font slot)
- Instanced quad renderer (glyph + background)
- StagedBitmap upload queue
- BoxDrawing procedural rasterizer
- GlyphConstraint system (NF icons)
- Platform font dispatch (CoreText / FreeType)

**Mode-specific layout:**
- **Monospace:** Current END layout — fixed cell width/height, row-based dirty tracking, ring buffer indexing
- **Attributed:** Line-wrapped text layout with styled runs (bold, italic, color, size). Each run shaped independently. Paragraph-level layout. Supports mixed font sizes within a line.

**API surface (attributed mode) — drop-in replacement for `juce::TextLayout`:**

```cpp
namespace jreng
{
    class TextLayout
    {
    public:
        void createLayout (const juce::AttributedString& text, float maxWidth);
        void draw (GLGraphics& g, juce::Rectangle<float> area) const;
        void draw (juce::Graphics& g, juce::Rectangle<float> area) const;
        float getHeight() const;
        int getNumLines() const;
    };
}
```

Input is `juce::AttributedString` — no custom string type. Two `draw()` overloads: GL (instanced quads) and CPU (`juce::Image` blit). Same layout, different surface.

**Current location:** `modules/jreng_graphics/fonts/jreng_text_layout.h`. Standalone `jreng_text` module extraction is a future organizational step.

**Dependencies:** `jreng_opengl`, `jreng_core`, `jreng_freetype`, `jreng_harfbuzz`, `juce_graphics`

**Migration path:**
1. Extract shared types (GlyphAtlas, FontCollection, Fonts, shaping) from `Source/terminal/rendering/` into `modules/jreng_text/`
2. END's `Screen` and `TerminalGLRenderer` become consumers of `jreng_text` monospace API
3. WHELMED consumes `jreng_text` attributed API
4. Both share the same atlas instance and font handles

---

### WHELMED Integration

**WHELMED:** WYSIWYG Hybrid Encoder Lightweight Markdown Editor with mermaid diagram renderer.

**Status:** `Whelmed::Component` is integrated into END's split pane system as a `PaneComponent` subclass. Opening a `.md` file from a terminal link (`onOpenMarkdown` callback) overlays a Whelmed pane on the active terminal. The terminal is restored when the Whelmed pane is closed.

**Integration model:** `Whelmed::Component` subclasses `PaneComponent`, the same base as `Terminal::Component`. `Panes::createWhelmed()` overlays it on the active pane. `Panes::closeWhelmed()` removes it. DOCUMENT ValueTree is grafted alongside SESSION in the PANE node.

**Shared infrastructure with END:**
- `jreng_text` module — GL text rendering (attributed mode for WHELMED, monospace for END)
- `jreng_opengl` module — `GLGraphics` for path/shape rendering, `GLComponent` base class
- Same glyph atlas, same font handles, same GL context

**WHELMED rendering stack:**

| Content | Renderer |
|---------|----------|
| Markdown text (headings, paragraphs, lists, inline code) | `jreng_text` attributed mode (proportional fonts, styled runs) |
| Code blocks | `jreng_text` monospace mode (same as END terminal) |
| Mermaid diagrams | Parsed to SVG -> `juce::Path` -> `GLPath` (tessellated via `jreng_opengl`) |
| Inline images | Same image rendering as END's Sixel/iTerm2 support |

**Standalone comes for free:** WHELMED as a standalone app is just a `MainComponent` wrapping the WHELMED component. Same code, different entry point.

---

## Phase Breakdown

### Phase 1: Core Terminal — COMPLETE

PTY backend, VT parser, xterm key sequences, SGR mouse, OSC 0/2, OSC 52 (partial), resize, Lua config.

### Phase 2: GPU + Polish — MOSTLY COMPLETE

Dual glyph atlas, font rasterization, HarfBuzz shaping, emoji, instanced rendering, scrollback, font fallback, selection, cursor, hot reload, true color, box drawing, NF constraints, embolden, glass window, scroll keybinds, split panes, prefix key system, tab management, working directory tracking.

**Remaining:**
- [x] Focus events (send to PTY)
- [x] Bell
- [x] Word/line selection
- [x] Software renderer fallback

### Phase 3: Keybinding + UX

- [x] Unified keybinding system (action registry, global + modal, fully configurable)
- [x] Command palette (fuzzy search, all actions + popups)
- [x] Tmux-style popup terminals (user-defined, configurable)
- [x] File opener / flash-jump (ls integration, hint labels)
- [x] Terminal state serialization (Grid+State snapshot via Processor::getStateInformation/setStateInformation)

### Phase 4: Rendering + Protocol

- [ ] Sixel inline image rendering
- [ ] iTerm2 inline images (OSC 1337)
- [x] OSC 7 (cwd tracking)
- [x] OSC 8 (hyperlinks)
- [x] OSC 52 base64 decode
- [x] Windows ConPTY support
- [x] Configurable shell in Lua
- [x] Error display on invalid config

### Phase 5: Module Extraction + WHELMED

- [x] GL text rendering infrastructure (jreng::TextLayout in jreng_graphics)
- [ ] Extract to standalone jreng_text module
- [x] Attributed text layout mode (jreng::TextLayout)
- [x] WHELMED component (markdown rendering, mermaid diagrams) — integrated as Whelmed::Component
- [x] WHELMED integration into END split panes — Panes::createWhelmed(), PaneComponent interface
- [ ] WHELMED standalone wrapper

---

## Performance Targets

| Metric | Target |
|--------|--------|
| Launch to shell prompt | < 100ms |
| Input latency | < 5ms |
| Frame time (GPU) | < 5.8ms (70% of 8.33ms @ 120fps) |
| Memory (base) | < 50MB |
| Memory (10k scrollback) | < 100MB |
| VRAM (mono atlas) | < 5MB |
| VRAM (emoji atlas) | < 5MB |
| Glyph cache hit rate | > 99% for ASCII |
| Draw calls per frame | 3-4 (background + mono + emoji + images) |
| Render loop allocations | 0 (pre-allocated buffers) |
| 5K fullscreen (25k cells) | 120fps locked |
| Command palette open | < 50ms |
| Popup terminal spawn | < 100ms |
| Flash-jump label overlay | < 50ms |
| Sixel decode (1MB image) | < 100ms |

---

## Success Criteria (Remaining)

### Correctness
- [x] Selection works in scrollback
- [x] Word selection (double-click)
- [x] Line selection (triple-click)
- [x] Focus events sent to PTY
- [x] Windows build compiles and runs
- [x] All keybindings overridable via end.lua
- [x] Command palette lists all actions
- [x] Popup terminals spawn and close correctly
- [x] Flash-jump labels resolve to correct files
- [ ] Sixel images render in correct cell positions
- [x] WHELMED markdown renders with correct typography — TextBlock, MermaidBlock, TableBlock

### Performance
- [x] State serialization < 1s for 10 terminals (implemented via getStateInformation)
- [x] State restore < 500ms for 10 terminals (implemented via setStateInformation)
- [ ] Command palette responsive with 100+ actions
- [ ] Inline images don't drop below 60fps

---

*For current codebase documentation: ARCHITECTURE.md*
*For detailed future specs: SPEC-details.md*
