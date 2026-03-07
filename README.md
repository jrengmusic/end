# END

The journey of finding the best terminal is finally comes to END.

---

## What is END?

**Ephemeral Nexus Display** -- a GPU-accelerated terminal emulator built from scratch in C++17 and JUCE. OpenGL-rendered text, HarfBuzz shaping, full Unicode support, sub-frame latency.

END is opinionated: it does terminal rendering and nothing else. No tabs, no splits, no session management. You use tmux for multiplexing. END handles the pixels.


## Features

**Rendering**
- GPU-instanced text rendering -- glyph atlas, instanced quads, 3 draw calls per frame at 120fps
- Dual texture atlas: mono glyphs (R8) + color emoji (RGBA8)
- CoreText on macOS, FreeType on Linux -- native quality on each platform
- HarfBuzz text shaping with ligature support
- Nerd Font icons with per-glyph constraint scaling (ported from NF patcher v3.4.0)
- Procedural box drawing, block elements, and braille -- pixel-perfect at any cell size, no font dependency
- System font fallback via CoreText cascade for missing glyphs
- Color emoji (Apple Color Emoji, Noto, system fonts)
- Configurable zoom (Cmd+/-/0) with full font resize across all font handles

**Terminal**
- Full xterm-256color + 24-bit true color
- Unicode grapheme segmentation (UAX #29 state machine, Unicode 17.0)
- Wide character support (CJK, EAW)
- SGR mouse tracking (modes 1000/1002/1003/1006 -- tmux and vim just work)
- Bracketed paste
- Alternate screen buffer (vim, htop, less, TUI apps)
- DECTCEM cursor visibility
- Scrollback with configurable history
- Text selection with transparent overlay

**Platform**
- Native glass blur on macOS -- your desktop bleeds through
- Borderless window with configurable title bar buttons
- Window state persistence (size, zoom saved across sessions)
- Lua configuration with hot-reload (Cmd+R)
- Lock-free render pipeline -- reader thread writes atomics, VBlank polls dirty flags, GL thread acquires snapshot via atomic pointer exchange
- Zero allocations on the render path

**Architecture**
- C++17 + JUCE 8 cross-platform framework
- CoreText + HarfBuzz on macOS, FreeType + HarfBuzz on Linux
- OpenGL instanced rendering with shelf-packed dual atlas
- VT100/xterm parser (ground, escape, CSI, DCS, OSC state machine)
- Ring buffer grid with reflow-on-resize
- ValueTree as single source of truth for all terminal state


## Roadmap

- **Windows support** -- ConPTY backend in progress, rendering pipeline ready
- **Tabs** -- TerminalComponent is already a self-contained JUCE component, wrapping into TabbedComponent is straightforward
- **Inline images** -- Sixel protocol support


## Get Started

```bash
cmake -S . -B Builds/Xcode -G Xcode
cmake --build Builds/Xcode --config Release
```

Requirements: C++17 compiler, CMake, JUCE 8, macOS (primary), Linux (supported), Windows (in progress)


## Configuration

Everything lives in `~/.config/end/end.lua`:

```lua
END = {
    font = {
        family = "Display Mono",
        size = 14,
        ligatures = true,
        embolden = true,
    },
    cursor = {
        char = "\u{2588}",
        blink = true,
        blink_interval = 500,
    },
    colours = {
        foreground = "#FFB3F9F5",
        background = "#E0090D12",
        cursor = "#CCB3F9F5",
        selection = "#8000C8D8",
    },
    window = {
        title = "END",
        colour = "#090D12",
        opacity = 0.75,
        blur_radius = 32.0,
        always_on_top = true,
        buttons = false,
    },
    scrollback = {
        num_lines = 10000,
        step = 5,
    },
}
```

Window size and zoom are saved automatically in `~/.config/end/state.lua`.


## Keybinds

| Key | Action |
|-----|--------|
| Cmd+C | Copy selection |
| Cmd+V | Paste |
| Cmd+R | Reload config |
| Cmd+Q | Quit (saves window state) |
| Cmd+= | Zoom in |
| Cmd+- | Zoom out |
| Cmd+0 | Reset zoom |
| Shift+PageUp | Scroll up |
| Shift+PageDown | Scroll down |
| Shift+Home | Scroll to top |
| Shift+End | Scroll to bottom |


## What END Does Not Handle

| Feature | Why |
|---------|-----|
| Tabs | tmux (planned) |
| Splits | tmux |
| Sessions | tmux |
| Shell integration | Complexity, low value |
| Hyperlinks | Not yet |
| Sixel images | Planned |


## Documentation

| Doc | Contents |
|-----|----------|
| [SPEC.md](SPEC.md) | Complete technical specification |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, threading model, data flow |
| Source code | Comprehensive Doxygen annotations on all 77 source files |


## Platform Support

| Platform | Status |
|----------|--------|
| macOS | Primary -- CoreText rendering, native glass blur |
| Linux | Supported -- FreeType rendering |
| Windows | In progress -- ConPTY backend |


## License

Proprietary -- JRENG

---

Rock 'n Roll!

**JRENG!**

---
conceived with [CAROL](https://github.com/jrengmusic/carol)
