<div align="center">
  <img src="___display___/end-icon.svg" alt="END">
</div>

# END

**Ephemeral Nexus Display**

The long and winding road finding the truly best opinionated cross-platform, dual-backend renderer, rich-featured, modern terminal emulator with non-web stack true Markdown and Mermaid renderer that can run on your grandma's PC finally comes to END.

---

## What is END?

A GPU/CPU-rendered terminal emulator built from scratch in C++17 and JUCE. OpenGL instanced rendering or SIMD-optimised software fallback, HarfBuzz shaping, full Unicode support, sub-frame latency. Runtime-switchable between GPU and CPU with a config reload.

END is opinionated: it does terminal rendering well. Tabs, split panes, popup terminals, command palette, vim-like selection, file opener with flash-jump hints — all built in. Glass blur UI on macOS and Windows. Lua-configurable everything. You use tmux for session management. END handles the pixels.

And when you open a Markdown file, END doesn't shell out to a browser. It renders it natively in a split pane — headings, tables, code blocks, Mermaid diagrams — same rendering stack, same window. That's WHELMED.


## Features

**Dual Renderer**
- GPU: OpenGL instanced text rendering — glyph atlas, instanced quads, 3 draw calls per frame at 120fps
- CPU: SIMD-optimised software renderer (SSE2/NEON) — same quality, no GPU required
- Runtime GPU/CPU switching via config hot-reload (Cmd+R)
- Dual texture atlas: mono glyphs (R8) + colour emoji (RGBA8)
- Shelf-packed atlas with LRU eviction

**Text**
- CoreText on macOS, FreeType on Linux/Windows — native quality on each platform
- HarfBuzz text shaping with ligature support
- Nerd Font icons with per-glyph constraint scaling (ported from NF patcher v3.4.0)
- Procedural box drawing, block elements, and braille — pixel-perfect at any cell size, no font dependency
- System font fallback via CoreText cascade for missing glyphs
- Colour emoji (Apple Color Emoji, Noto, system fonts)
- Configurable cell width, line height, and emboldening

**Terminal**
- Full xterm-256color + 24-bit true colour
- Unicode grapheme segmentation (UAX #29 state machine, Unicode 17.0)
- Wide character support (CJK, East Asian Width)
- Kitty keyboard protocol (progressive enhancement, per-screen flag stacks)
- SGR mouse tracking (modes 1000/1002/1003/1006 — tmux and vim just work)
- Bracketed paste, focus events, bell
- Alternate screen buffer (vim, htop, less, TUI apps)
- DECTCEM cursor visibility
- Scrollback with configurable history
- Ring buffer grid with reflow-on-resize

**OSC and Shell Integration**
- OSC 7: working directory tracking
- OSC 8: hyperlinks (parsed and merged with heuristic link detection)
- OSC 9/777: native desktop notifications (macOS UNUserNotificationCenter, Windows/Linux fallback)
- OSC 52: clipboard access (base64)
- OSC 133: shell integration markers (A/B/C/D output block tracking)
- Automatic shell integration injection (zsh, bash, fish)
- Clickable hyperlinks on command output rows

**UI**
- Tabbed interface with configurable position (top, bottom, left, right)
- Split panes with binary tree layout — horizontal and vertical, draggable dividers
- Prefix-key pane navigation (tmux-style) — fully configurable keys and timeout
- Command palette: fuzzy-searchable action list with glass blur overlay
- Popup terminals: user-defined modal floating terminals for TUI apps (lazygit, htop, tit, etc.)
- File opener with flash-jump hint labels — keyboard-jumpable file paths from command output
- Vim-like selection mode: visual, visual-line, visual-block with keyboard cursor navigation in scrollback
- Word selection (double-click), line selection (triple-click)
- Status bar with modal state display
- Configurable cursor: any character, Nerd Font icon, or colour emoji — with optional blink
- Text selection with transparent overlay
- Drag-and-drop file paths with configurable quoting

**Platform**
- macOS: CoreText rendering, native glass blur, UNUserNotificationCenter
- Linux: FreeType rendering, notify-send notifications
- Windows: ConPTY backend (NT API duplex pipe, overlapped I/O), glass blur (Win10 DWM / Win11 system effect)
- Borderless window with configurable title bar buttons
- Multi-window support (Cmd+N)
- Window state persistence (size, zoom saved across sessions)
- Configurable zoom (Cmd+/-/0) with full font resize

**Configuration**
- Lua hot-reload (Cmd+R, no restart)
- Lock-free render pipeline — reader thread writes atomics, VBlank polls dirty flags, GL thread acquires snapshot via atomic pointer exchange
- Zero allocations on the render path
- Unified action registry: every keybinding is configurable, global or modal, or both


## WHELMED

**WYSIWYG Hybrid Encoder Lightweight Markdown/Mermaid**

WHELMED is END's built-in Markdown and Mermaid renderer. Click a `.md` hyperlink in the terminal and it opens as a native split pane — no browser, no electron, no external process. Same window, same rendering stack.

**Rendering:**
- Headings (h1-h6), paragraphs, lists with full styled text
- Inline code and fenced code blocks with syntax colouring
- Tables with header rows, column alignment, alternating row colours
- Mermaid diagrams rendered from SVG parse — viewbox-driven scaling
- Vim-style keyboard navigation and text selection

WHELMED shares END's font system, glyph atlas, and GL/CPU renderer. It runs as a `PaneComponent` — the same interface as a terminal pane.

**Status:** Headings, paragraphs, lists, code blocks, tables, and basic Mermaid rendering are working. Mermaid support is being expanded.


## Get Started

```bash
cmake -S . -B Builds/Xcode -G Xcode
cmake --build Builds/Xcode --config Release
```

Requirements: C++17 compiler, CMake, JUCE 8


## Configuration

Everything lives in `~/.config/end/`:

| File | Purpose |
|------|---------|
| `end.lua` | Terminal configuration: font, colours, cursor, window, tabs, panes, keybindings, popups, shell, hyperlinks |
| `whelmed.lua` | Markdown viewer configuration: typography, heading sizes, colours, code block syntax colours, layout |
| `state.lua` | Auto-saved window state (size, zoom) — not user-edited |

Both config files are auto-generated with documented defaults on first launch. Every value has inline comments explaining what it does. Edit any value, press Cmd+R to reload. Invalid or missing values fall back to defaults silently.

Keybindings are fully configurable through `end.lua`. Every action — copy, paste, zoom, split, navigate, command palette, popups — is assignable as a global shortcut (`cmd+c`), a prefix-mode key (backtick then `h`), or both. The config file documents the format and all available actions.


## Roadmap

| Feature | Status |
|---------|--------|
| WHELMED Mermaid | In progress — basic rendering works, expanding coverage |
| Sixel inline images | Planned |
| iTerm2 inline images (OSC 1337) | Planned |
| Terminal state serialization | Spec written |
| `jreng_text` module extraction | Planned — shared GL text rendering for END + WHELMED |
| WHELMED standalone | Planned — same code, separate entry point |


## Documentation

| Doc | Contents |
|-----|----------|
| [SPEC.md](SPEC.md) | Roadmap and feature specifications |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, threading model, data flow, module map |
| Source code | Doxygen annotations across all source files |


## Platform Support

| Platform | Status |
|----------|--------|
| macOS | Primary — CoreText, native glass blur, desktop notifications |
| Linux | Supported — FreeType rendering |
| Windows | Supported — ConPTY backend, glass blur |


## License

Proprietary — JRENG

---

Rock 'n Roll!

**JRENG!**

---
conceived with [CAROL](https://github.com/jrengmusic/carol)
