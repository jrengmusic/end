# SPEC.md

## END: Ephemeral Nexus Display

**Version:** 0.2.0
**Last Updated:** 2026-03-07

---

## Overview

**Purpose:** GPU-accelerated, minimalistic, configurable terminal emulator.

**Target User:** Power users who use tmux for multiplexing. END is the display layer.

**Core Workflow:** Launch → shell ready → work in tmux/vim/TUIs → configure with Lua.

**Design Philosophy:** Performance and beauty. No multiplexing (tmux handles that). Ephemeral display, not a shell manager.

**End Game (Phase 2+):** Integrate WHELMED — terminal that renders markdown and mermaid diagrams inline.

---

## Technology Stack

| Component | Choice |
|-----------|--------|
| Language | C++17 |
| Framework | JUCE 8 |
| Terminal State | Custom VT parser (no libtsm) |
| PTY | Native (forkpty on Unix, ConPTY on Windows — in progress) |
| GPU Rendering | JUCE OpenGL (glyph atlas + instanced quads) |
| Text Shaping | HarfBuzz (JUCE bundled, 10.1.0) |
| Font Loading | CoreText (macOS), FreeType (Linux/Windows) |
| Glyph Rasterization | CoreText CTFontDrawGlyphs (macOS), FreeType FT_Render_Glyph (Linux) |
| Configuration | Lua via sol2 |
| Build System | CMake + JUCE |

**Platform:** macOS (primary), Linux, Windows (in progress)

---

## Core Principles

1. **Performance First**
   - Input latency < 5ms
   - 120fps rendering under heavy output
   - GPU glyph atlas (no CPU text rendering per frame)
   - Zero UI thread blocking

2. **Beauty Second**
   - Ligature support (Display Mono, configurable)
   - Grayscale antialiasing
   - Configurable fonts, colors, cursor
   - Clean, minimal chrome with native glass blur

3. **Ephemeral Display**
   - Terminal = dumb pipe with pixels
   - No tabs, no splits, no session management
   - tmux handles multiplexing
   - END handles display

4. **Lua Configuration**
   - Single config file: `~/.config/end/end.lua`
   - Hot reload with Cmd+R
   - Everything configurable

5. **Fail Fast**
   - PTY failure → exit
   - Invalid config → silent fallback to compiled-in defaults

---

## Out of Scope

| Feature | Reason |
|---------|--------|
| Tabs | tmux handles this |
| Splits/panes | tmux handles this |
| Session persistence | tmux handles this |
| Shell integration (prompt detection) | Complexity, low value |
| Semantic zones | Phase 2+ maybe |
| Hyperlinks (OSC 8) | Phase 2 |
| Notifications (OSC 9/777) | Phase 2 |
| SPSC ring buffers | Direct callback used instead (Parser::process called from reader thread) |

---

## 1. Architecture

### 1.1 Thread Model

```
Reader Thread (TTY)
  - Blocking read() on PTY fd
  - Calls Parser::process() directly (no FIFO)
  - Writes to Grid cells + State atomics
  - Never touches UI

Message Thread (JUCE main)
  - VBlankAttachment polls snapshotDirty atomic every vsync
  - If dirty: Screen::render() reads Grid -> builds Snapshot -> Mailbox::publish()
  - Timer (60-120Hz) flushes State atomics -> ValueTree
  - ValueTree listeners update CursorComponent
  - Handles keyboard/mouse input -> encodes -> TTY::write()

GL Thread (JUCE OpenGL)
  - Mailbox::acquire() gets latest Snapshot
  - Drains staged bitmap upload queue -> glTexSubImage2D
  - Draws instanced quads (3 draw calls: background + mono + emoji)
  - Zero shaping, zero allocation
```

**Critical:** `juce::OpenGLContext` creates its own background thread.
`renderOpenGL()` runs on the GL thread, NOT the message thread.
Never take `MessageManagerLock` inside `renderOpenGL()` on macOS (deadlock).

### 1.2 Data Flow

```
[Keyboard/Mouse] -> [Encode xterm sequences] -> [TTY::write()]
                                                       |
                                                       v
                                                   [Shell]
                                                       |
                                                       v
[PTY read()] -> [Parser::process()] -> [Grid cells + State atomics]
                                                |
                                    MESSAGE THREAD (VBlankAttachment):
                                    |- Screen::render() reads Grid
                                    |- HarfBuzz shaping (per dirty row)
                                    |- GlyphAtlas cache miss -> rasterize
                                    |- Build RenderSnapshot
                                    |- Push StagedBitmaps to upload queue
                                    |- Mailbox::publish(snapshot)
                                                |
                                    std::atomic<Snapshot*> exchange
                                                |
                                    GL THREAD (renderOpenGL):
                                    |- Mailbox::acquire() -> latest snapshot
                                    |- Drain upload queue -> glTexSubImage2D
                                    |- 3 draw calls: background, mono, emoji
```

### 1.3 Component Ownership

| Component | Owns | Thread |
|-----------|------|--------|
| TTY | File descriptor, fork/exec, resize ioctl | Reader |
| Parser | VT state machine, UTF-8 decode, dispatch | Reader |
| Grid | Ring buffer cells, dual screen, dirty tracking | Reader writes, Message reads |
| State | Atomic parameters, timer flush, ValueTree bridge | Reader writes, Timer/Message reads |
| HarfBuzz | Text shaping, ligature detection | Message |
| GlyphAtlas | Rasterization, LRU cache, staged uploads | Message (rasterize), GL (upload) |
| Screen | Snapshot builder, cell cache, render coordinator | Message |
| OpenGL | Atlas textures, shaders, instanced draw | GL |
| Config | Lua config state, Context<Config> pattern | Message |

---

## 2. PTY Backend

### 2.1 Initialization

1. App launches
2. Config loaded from `~/.config/end/end.lua`
3. Shell read from `$SHELL` environment variable, login shell (`-l`)
4. `openpty()` + `fork()` + `execl()` with initial size
5. Set `TERM=xterm-256color`
6. Reader thread starts blocking read loop
7. Shell prompt appears

### 2.2 Shell Configuration

Shell is read from `$SHELL` environment variable. Not yet configurable via Lua.

### 2.3 Error Handling

| Condition | Action |
|-----------|--------|
| Shell exits | `onShellExited` callback -> `systemRequestedQuit()` |
| User closes window | TTY destructor sends SIGTERM, waits, SIGKILL if needed |

### 2.4 Resize

1. `resized()` -> `screen.setViewport()` -> `session.resized(cols, rows)`
2. Session stores new size in atomic; reader thread picks up via `requestResize`
3. `ioctl(TIOCSWINSZ)` + `SIGWINCH` sent on reader thread

---

## 3. Input Handling

### 3.1 xterm Key Sequences

Implemented in `Keyboard.h`. All keys match xterm exactly.

#### Core Keys

| Key | Sequence |
|-----|----------|
| Enter | `\r` |
| Backspace | `\x7f` |
| Tab | `\t` |
| Escape | `\x1b` |

#### Navigation Keys

| Key | Sequence |
|-----|----------|
| Up | `\x1b[A` |
| Down | `\x1b[B` |
| Right | `\x1b[C` |
| Left | `\x1b[D` |
| Home | `\x1b[H` |
| End | `\x1b[F` |
| Insert | `\x1b[2~` |
| Delete | `\x1b[3~` |
| PageUp | `\x1b[5~` |
| PageDown | `\x1b[6~` |

#### Function Keys

| Key | Sequence |
|-----|----------|
| F1 | `\x1bOP` |
| F2 | `\x1bOQ` |
| F3 | `\x1bOR` |
| F4 | `\x1bOS` |
| F5 | `\x1b[15~` |
| F6 | `\x1b[17~` |
| F7 | `\x1b[18~` |
| F8 | `\x1b[19~` |
| F9 | `\x1b[20~` |
| F10 | `\x1b[21~` |
| F11 | `\x1b[23~` |
| F12 | `\x1b[24~` |

#### Modifier Rules

| Modifier | Rule |
|----------|------|
| Alt | ESC prefix: `\x1b` + key |
| Ctrl | ASCII mask: `key & 0x1f` |
| Shift | Modify sequence parameter |

#### Modified Navigation Keys

Format: `\x1b[1;{mod}{key}`

| Modifier Combo | Code |
|----------------|------|
| Shift | 2 |
| Alt | 3 |
| Shift+Alt | 4 |
| Ctrl | 5 |
| Shift+Ctrl | 6 |
| Alt+Ctrl | 7 |
| Shift+Alt+Ctrl | 8 |

**Example:** Ctrl+Right = `\x1b[1;5C`

### 3.2 Mouse (SGR 1006)

tmux + vim require SGR 1006.

#### Button Codes

| Button | Code |
|--------|------|
| Left | 0 |
| Middle | 1 |
| Right | 2 |
| Scroll up | 64 |
| Scroll down | 65 |

#### Sequence Format

```
Press:   \x1b[<{button};{col};{row}M
Release: \x1b[<{button};{col};{row}m
```

Coordinates are 1-based.

### 3.3 Focus Events

NOT YET IMPLEMENTED. `focusGained`/`focusLost` only call `repaint()`. No `\x1b[I`/`\x1b[O` sent to PTY.

### 3.4 Bracketed Paste

Implemented. `Session::paste()` wraps content in `\x1b[200~...\x1b[201~` when mode enabled.

---

## 4. OSC Handling

### 4.1 Implemented

| OSC | Status |
|-----|--------|
| OSC 0/2 | Window title parsed, `onTitleChanged` callback |
| OSC 52 | Clipboard payload extracted, `onClipboardChanged` callback |

### 4.2 Known Gaps

- OSC 52: base64 decode not implemented (raw bytes passed through)
- OSC 7: not implemented

---

## 5. Rendering (GPU)

### 5.1 Dual Atlas System

Two separate texture atlases.

**Mono Atlas (R8 grayscale):**
- Text glyphs + NF icons
- 4096x4096, shelf-packed, LRU eviction (~19,000 glyph capacity)
- Shader samples alpha channel, applies foreground color

**Emoji Atlas (RGBA8 color):**
- Color emoji (Apple Color Emoji, Noto Color Emoji)
- 4096x4096, shelf-packed, LRU eviction (~4,000 glyph capacity)
- Shader samples RGBA directly

### 5.2 Glyph Cache Key

```
GlyphKey = { glyphIndex, fontFace, fontSize, cellSpan }
```

Color is NOT in the key. Shader applies foreground color at draw time.

### 5.3 Cell Layout

Unified `Cell` struct (16 bytes, trivially copyable):

```
| codepoint (4B) | style (1B) | layout (1B) | width (1B) | reserved (1B) | fg (4B) | bg (4B) |
```

Style bits: BOLD, ITALIC, UNDERLINE, STRIKE, BLINK, INVERSE
Layout bits: wide continuation, emoji, has grapheme

Separate `Grapheme` struct (28 bytes) for multi-codepoint clusters, stored in sparse map.

Grid is a flat `HeapBlock<Cell>` ring buffer. Dirty tracking via `std::atomic<uint64_t> dirtyRows[4]` (256-bit bitmask, one bit per row, supports up to 256 rows).

### 5.4 Text Shaping (HarfBuzz)

Per-row shaping on message thread:

1. ASCII fast path (skip HarfBuzz for pure ASCII runs)
2. HarfBuzz shaping with `hb_buffer_guess_segment_properties()`
3. Emoji runs routed to separate emoji shaper
4. FontCollection O(1) lookup for codepoint -> font slot

### 5.5 Instanced Quad Rendering

Per-instance data (`Render::Glyph`):

| Attribute | Type |
|-----------|------|
| screenPosition | vec2 |
| glyphSize | vec2 |
| textureCoordinates | vec4 |
| foregroundColor | vec4 |

3 draw calls per frame: background quads + mono glyph quads + emoji glyph quads.

### 5.6 Shaders

4 shader files in `Source/terminal/rendering/shaders/`:

| File | Purpose |
|------|---------|
| `background.vert` | Cell background quad vertices |
| `background.frag` | Cell background fill |
| `glyph.vert` | Shared vertex shader for mono + emoji |
| `glyph_mono.frag` | `alpha = texture(atlas, uv).r; color = fg * alpha` |
| `glyph_emoji.frag` | `color = texture(atlas, uv)` (pre-colored) |

### 5.7 Damage Tracking

Row-level dirty bitmask (256 bits). Per-row granularity. No sub-row span merging, no area threshold, no temporal coalescing. Cursor blink does NOT dirty cells — cursor is a separate `CursorComponent` overlay.

### 5.8 RenderSnapshot + Mailbox

Double-buffered `Render::Snapshot` (snapshotA/snapshotB). Message thread builds, publishes via `std::atomic<Snapshot*>` exchange. GL thread acquires. Zero allocation, zero locking on hot path.

Staged bitmaps live in a separate mutex-guarded queue (not in snapshot). Prevents lost uploads when snapshots are dropped.

### 5.9 Upload Queue

Message thread rasterizes cache misses -> pushes `StagedBitmap` to queue. GL thread drains queue -> `glTexSubImage2D`. Atlas clear on font change via sentinel.

### 5.10 Repaint Strategy

`juce::VBlankAttachment` (CVDisplayLink) fires every vsync. Polls `consumeSnapshotDirty()`. Only renders when dirty. Idle terminal = no GPU work.

### 5.11 Ligatures

Config: `font.ligatures = true/false`. HarfBuzz shapes multi-char sequences. First cell gets ligature glyph, continuation cells skipped. Small lookahead for ligature detection.

### 5.12 Procedural Box Drawing

All handled in `BoxDrawing.h` — no font lookup, no atlas:

| Range | Content |
|-------|---------|
| U+2500-U+257F | Box drawing (light/heavy lines, dashed, double, rounded corners via SDF, diagonal via AA) |
| U+2580-U+259F | Block elements (fractional blocks, quadrants, shades) |
| U+2800-U+28FF | Braille patterns (2x4 dot grid) |

### 5.13 Font Fallback Chain (FontCollection)

O(1) codepoint -> font slot via flat lookup table (1.1M entries). Up to 32 font slots. Resolution order:

1. Primary font (Display Mono)
2. Nerd Font (Symbols NF, loaded from BinaryData)
3. System fonts via cmap population

### 5.14 NF Glyph Constraint System

Generated from NF patcher v3.4.0 data. 10,470 codepoints across 88 switch arms. Per-glyph scaling, alignment, padding applied at rasterization time.

Scale modes: none, fit, cover, adaptiveScale, stretch.

### 5.15 Embolden

Config: `font.embolden = true/false`. macOS: `kCGTextFillStroke` (1px stroke). Linux: `FT_Outline_Embolden`. Hot-reloadable — atlas cleared + grid marked all dirty on toggle.

---

## 6. Scrollback

### 6.1 Ring Buffer

Dual buffers (normal + alternate). Flat `HeapBlock<Cell>` with ring-buffer row indexing. `head` tracks logical top row. Configurable capacity: `scrollback.num_lines = 10000`.

### 6.2 Scroll Input

| Input | Action |
|-------|--------|
| Mouse wheel up | Scroll up (configurable step, default 5) |
| Mouse wheel down | Scroll down |

Shift+PageUp/Down/Home/End: Implemented. Scrolls viewport without sending to PTY.

### 6.3 Mouse Wheel in Mouse Tracking Mode

When mouse tracking is enabled (vim/tmux), scroll events are forwarded as button 64/65 to PTY instead of scrolling the viewport.

---

## 7. Selection

### 7.1 Implemented

- Character selection via click + drag
- Selection highlighted via transparent overlay (`colours.selection`)
- Cmd+C copies selection text
- Any keypress clears selection
- Mouse tracking modes bypass selection (forward to PTY)

### 7.2 Not Yet Implemented

- Auto-copy on mouse release
- Word selection (double-click)
- Line selection (triple-click)
- Keyboard selection mode
- Selection in scrollback (unverified)

---

## 8. Cursor

### 8.1 Configuration

```lua
END = {
    cursor = {
        char = "\u{2588}",     -- any Unicode codepoint
        blink = true,
        blink_interval = 500,  -- ms
    },
}
```

### 8.2 Rendering

Cursor is a glyph image rasterized via `Fonts::rasterizeToImage()`. Supports any Unicode character as cursor shape. Color emoji cursors rendered without tinting. Grayscale cursors tinted with `colours.cursor`.

Cursor is a separate `CursorComponent` overlay — NOT a cell dirty. Blink via JUCE Timer.

### 8.3 DECTCEM

`\e[?25l` / `\e[?25h` respected. VBlank checks `isCursorVisible(activeScreen)`.

---

## 9. Bell

NOT IMPLEMENTED. BEL character (`\x07`) silently discarded.

---

## 10. Colors

### 10.1 Support

- xterm-256color palette (full)
- True color 24-bit RGB via escape sequences
- RGBA for END-controlled elements

### 10.2 Color Struct

4 bytes, trivially copyable. Mode: theme (0), palette (1), rgb (2).

### 10.3 Color Format in Lua

- `"#RRGGBB"` — opaque RGB
- `"#AARRGGBB"` — ARGB with alpha
- `"rgba(r, g, b, a)"` — functional notation

### 10.4 Default Palette

Named color scheme (sea/frost theme):

| Key | Value | Name |
|-----|-------|------|
| foreground | `#FFB3F9F5` | frostbite |
| background | `#E0090D12` | bunker, 88% opacity |
| cursor | `#CCB3F9F5` | frostbite, 80% opacity |

Full ANSI 16 colors with named variants defined in Config defaults.

---

## 11. Font Configuration

### 11.1 Lua Configuration

```lua
END = {
    font = {
        family = "Display Mono",
        size = 14,
        ligatures = true,
        embolden = true,
    },
}
```

### 11.2 Font Stack

```
Display Mono (embedded, 3 weights: Book/Medium/Bold)
  -> Symbols NF (embedded, loaded from BinaryData via CGDataProvider/FT_New_Memory_Face)
  -> System fonts (via FontCollection cmap resolution)
  -> Apple Color Emoji / Noto Color Emoji (system)
```

### 11.3 Platform Font Dispatch

| Platform | Loading | Shaping | Rasterization |
|----------|---------|---------|---------------|
| macOS | CoreText | HarfBuzz (hb_coretext_font_create) | CoreText (CTFontDrawGlyphs) |
| Linux | FreeType | HarfBuzz (hb_ft_font_create) | FreeType (FT_Render_Glyph) |

### 11.4 Bold/Italic

4 style slots: regular, bold, italic, boldItalic. Each gets own face + `hb_font_t`. All share the same atlas.

---

## 12. Lua Configuration

### 12.1 Config Location

`~/.config/end/end.lua`

### 12.2 Config Namespace

`END = { group = { key = value } }` — uppercase `END` because `end` is a Lua keyword.

### 12.3 Hot Reload

`Cmd+R` -> `Config::reload()` -> `applyConfig()` -> `MessageOverlay::showMessage("RELOADED")`.

Applies: ligatures, embolden, theme colors. Clears atlas cache + marks all dirty on embolden toggle.

### 12.4 State Persistence

`saveWindowSize()` and `saveZoom()` write to `~/.config/end/state.lua` via `writeState()`. State is separate from config — `end.lua` holds user preferences, `state.lua` holds runtime state (window size, zoom).

### 12.5 Error Handling

Invalid config silently falls back to compiled-in defaults. No error message displayed to user.

### 12.6 Config Keys

All keys are in `Config::Key`:

| Key | Type | Default |
|-----|------|---------|
| `font.family` | string | "Display Mono" |
| `font.size` | int | 14 |
| `font.ligatures` | bool | true |
| `font.embolden` | bool | true |
| `cursor.char` | string | "\u{2588}" |
| `cursor.blink` | bool | true |
| `cursor.blink_interval` | int | 500 |
| `colours.foreground` | colour | `#FFB3F9F5` |
| `colours.background` | colour | `#E0090D12` |
| `colours.cursor` | colour | `#CCB3F9F5` |
| `colours.selection` | colour | configurable |
| `colours.black` | colour | ANSI 0 |
| `colours.red` | colour | ANSI 1 |
| `colours.green` | colour | ANSI 2 |
| `colours.yellow` | colour | ANSI 3 |
| `colours.blue` | colour | ANSI 4 |
| `colours.magenta` | colour | ANSI 5 |
| `colours.cyan` | colour | ANSI 6 |
| `colours.white` | colour | ANSI 7 |
| `colours.bright_black` | colour | ANSI 8 |
| `colours.bright_red` | colour | ANSI 9 |
| `colours.bright_green` | colour | ANSI 10 |
| `colours.bright_yellow` | colour | ANSI 11 |
| `colours.bright_blue` | colour | ANSI 12 |
| `colours.bright_magenta` | colour | ANSI 13 |
| `colours.bright_cyan` | colour | ANSI 14 |
| `colours.bright_white` | colour | ANSI 15 |
| `window.title` | string | "END" |
| `window.colour` | colour | `#090D12` |
| `window.opacity` | float | 0.75 |
| `window.blur_radius` | float | 32.0 |
| `window.always_on_top` | bool | true |
| `window.buttons` | bool | false |
| `overlay.family` | string | "Display Mono" |
| `overlay.size` | int | 14 |
| `overlay.colour` | colour | configurable |
| `scrollback.num_lines` | int | 10000 |
| `scrollback.step` | int | 5 |

#### State-Only Keys (not in `end.lua`, persisted in `state.lua`)

| Key | Type | Default |
|-----|------|---------|
| `window.width` | int | 640 |
| `window.height` | int | 480 |
| `window.zoom` | float | 1.0 |

These are saved/loaded via `~/.config/end/state.lua` and are not part of the config schema.

---

## 13. Keybinds

### 13.1 Current Keybinds (hardcoded, macOS Cmd)

| Keybind | Action |
|---------|--------|
| Cmd+C | Copy selection to clipboard |
| Cmd+V | Paste from clipboard |
| Cmd+Q | Quit application |
| Cmd+R | Reload config, show "RELOADED" overlay |
| Cmd+= / Cmd++ | Zoom in (+0.25, max 4.0) |
| Cmd+- | Zoom out (-0.25, min 1.0) |
| Cmd+0 | Reset zoom to 1.0 |

### 13.2 Scroll Keybinds

| Keybind | Action |
|---------|--------|
| Shift+PageUp | Scroll viewport up one page |
| Shift+PageDown | Scroll viewport down one page |
| Shift+Home | Scroll to top of scrollback |
| Shift+End | Scroll to bottom |

### 13.3 Not Yet Implemented

- Configurable keybinds via Lua
- Cross-platform modifier mapping (Ctrl+Shift on Linux/Windows)

---

## 14. Window

### 14.1 Configuration

```lua
END = {
    window = {
        title = "END",
        colour = "#090D12",
        opacity = 0.75,
        blur_radius = 32.0,
        always_on_top = true,
        buttons = false,
    },
}
```

Note: `width`, `height`, and `zoom` are state, not config. They are persisted in `~/.config/end/state.lua`.

### 14.2 Glass Component (macOS)

Native glassmorphism via `jreng::GlassWindow`. Two blur methods:

1. CoreGraphics private API (`CGSSetWindowBackgroundBlurRadius`) — variable radius
2. NSVisualEffectView fallback — system-managed blur

Auto-selects CG if available, falls back to NSVisualEffect. Non-macOS: opaque background.

### 14.3 MessageOverlay

Reusable overlay component for transient messages:

- Shows grid dimensions during resize: `"80 col * 24 row"`
- Shows `"RELOADED"` on config hot-reload
- 60ms fade-in, 1000ms display, fade-out
- Reads `overlay.family`, `overlay.size`, `overlay.colour` from Config

---

## 15. Performance Targets

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
| Draw calls per frame | 3 (background + mono + emoji) |
| Render loop allocations | 0 (pre-allocated buffers) |
| 5K fullscreen (25k cells) | 120fps locked |

---

## 16. Phase Breakdown

### Phase 1: Core Terminal — COMPLETE

- [x] PTY backend (Unix: forkpty)
- [x] Custom VT parser (state machine + dispatch)
- [x] xterm key sequences (full table)
- [x] SGR mouse (1006)
- [x] OSC 0/2 window title
- [x] OSC 52 clipboard (partial — no base64 decode)
- [x] Resize propagation
- [x] Basic Lua config

### Phase 2: GPU + Polish — MOSTLY COMPLETE

- [x] Dual glyph atlas (R8 mono + RGBA8 emoji)
- [x] Atlas shelf packer + LRU eviction
- [x] Font rasterization (CoreText macOS, FreeType Linux)
- [x] HarfBuzz text shaping (auto-detect script, ligatures)
- [x] Emoji detection + color emoji rendering
- [x] Cell layout (unified 16-byte Cell)
- [x] RenderSnapshot + atomic mailbox
- [x] Instanced quad rendering (3 draw calls)
- [x] Vertex + fragment shaders
- [x] Row-level dirty tracking
- [x] Scrollback ring buffer
- [x] Font discovery + fallback chain (FontCollection)
- [x] Mouse selection (character)
- [x] Cursor (glyph-based, configurable char, blink)
- [x] Hot reload config (Cmd+R)
- [x] True color support
- [x] Procedural box drawing (lines, blocks, braille)
- [x] NF glyph constraint system
- [x] Embolden toggle
- [x] Glass window (macOS)
- [ ] Focus events (not sending to PTY)
- [ ] Bell
- [ ] Word/line selection
- [x] Scroll keybinds (Shift+PageUp/Down/Home/End)
- [ ] Configurable keybinds
- [ ] Software renderer fallback

### Phase 3: Advanced

- [ ] Windows ConPTY support
- [ ] OSC 7 (cwd tracking)
- [ ] OSC 52 base64 decode
- [ ] Configurable shell in Lua
- [ ] Error display on invalid config

### Phase 4: WHELMED Integration

- [ ] Markdown rendering inline
- [ ] Mermaid diagram rendering inline

---

## 17. Success Criteria

### Correctness

- [x] tmux attach/detach works
- [x] tmux panes render correctly
- [x] nvim with plugins works
- [x] fzf fuzzy finder works
- [x] htop renders correctly
- [x] Mouse selection in vim works
- [x] Ctrl+C interrupt works
- [x] Resize during vim -> no corruption
- [x] Shell exit -> clean exit

### Performance

- [x] Heavy output -> smooth rendering
- [x] Input during heavy output -> no lag
- [x] Scrollback navigation via mouse wheel

### Beauty

- [x] Ligatures render correctly
- [x] Font rendering is crisp
- [x] Colors match config
- [x] Cursor blinks smoothly
- [x] Box drawing renders correctly
- [x] NF icons properly scaled/positioned
- [x] Glass blur effect on macOS

---

## 18. File Structure

```
end/
├── SPEC.md
├── ARCHITECTURE.md
├── CMakeLists.txt
├── Library/
│   ├── freetype/               # FreeType source
│   ├── lua/                    # Lua 5.x source
│   └── sol/                    # sol2 headers
├── Source/
│   ├── Main.cpp
│   ├── MainComponent.h
│   ├── MainComponent.cpp
│   ├── component/
│   │   ├── CursorComponent.h
│   │   ├── MessageOverlay.h
│   │   ├── TerminalComponent.h
│   │   └── TerminalComponent.cpp
│   ├── config/
│   │   ├── Config.h
│   │   └── Config.cpp
│   ├── fonts/
│   │   ├── DisplayMono-Book.ttf
│   │   ├── DisplayMono-Medium.ttf
│   │   ├── DisplayMono-Bold.ttf
│   │   └── SymbolsNerdFont-Regular.ttf
│   └── terminal/
│       ├── data/
│       │   ├── Cell.h
│       │   ├── CharProps.h
│       │   ├── CharPropsData.h
│       │   ├── Charset.h
│       │   ├── Color.h
│       │   ├── CSI.h
│       │   ├── DispatchTable.h
│       │   ├── Identifier.h
│       │   ├── Keyboard.h
│       │   ├── Palette.h
│       │   ├── State.h
│       │   ├── State.cpp
│       │   ├── StateFlush.cpp
│       │   └── ValueTreeUtilities.h
│       ├── logic/
│       │   ├── Grid.h
│       │   ├── Grid.cpp
│       │   ├── GridErase.cpp
│       │   ├── GridReflow.cpp
│       │   ├── GridScroll.cpp
│       │   ├── Parser.h
│       │   ├── Parser.cpp
│       │   ├── ParserCSI.cpp
│       │   ├── ParserESC.cpp
│       │   ├── ParserEdit.cpp
│       │   ├── ParserOps.cpp
│       │   ├── ParserSGR.cpp
│       │   ├── ParserVT.cpp
│       │   ├── Session.h
│       │   └── Session.cpp
│       ├── rendering/
│       │   ├── AtlasPacker.h
│       │   ├── BoxDrawing.h
│       │   ├── FontCollection.h
│       │   ├── FontCollection.cpp
│       │   ├── FontCollection.mm
│       │   ├── Fonts.h
│       │   ├── Fonts.cpp
│       │   ├── Fonts.mm
│       │   ├── FontsMetrics.cpp
│       │   ├── FontsShaping.cpp
│       │   ├── GLShaderCompiler.h
│       │   ├── GLShaderCompiler.cpp
│       │   ├── GLVertexLayout.h
│       │   ├── GLVertexLayout.cpp
│       │   ├── GlyphAtlas.h
│       │   ├── GlyphAtlas.cpp
│       │   ├── GlyphAtlas.mm
│       │   ├── GlyphConstraint.h
│       │   ├── GlyphConstraintTable.cpp
│       │   ├── Screen.h
│       │   ├── Screen.cpp
│       │   ├── ScreenRender.cpp
│       │   ├── ScreenSelection.h
│       │   ├── ScreenSnapshot.cpp
│       │   ├── TerminalGLDraw.cpp
│       │   ├── TerminalGLRenderer.h
│       │   ├── TerminalGLRenderer.cpp
│       │   └── shaders/
│       │       ├── background.vert
│       │       ├── background.frag
│       │       ├── glyph.vert
│       │       ├── glyph_mono.frag
│       │       └── glyph_emoji.frag
│       └── tty/
│           ├── TTY.h
│           ├── TTY.cpp
│           ├── UnixTTY.h
│           ├── UnixTTY.cpp
│           ├── WindowsTTY.h
│           └── WindowsTTY.cpp
└── modules/
    ├── jreng_core/             # Core utilities, Context<T>, BinaryData
    ├── jreng_graphics/         # Blur, colours, graphics utilities
    └── jreng_gui/
        └── glass/
            ├── jreng_background_blur.h
            ├── jreng_background_blur.mm
            ├── jreng_glass_component.h
            ├── jreng_glass_component.cpp
            ├── jreng_glass_window.h
            └── jreng_glass_window.cpp
```

---

## 19. Dependencies

### Required

| Dependency | Purpose | License |
|------------|---------|---------|
| JUCE 8 | UI, OpenGL, events, HarfBuzz | GPLv3 / Commercial |
| sol2 | Lua binding | MIT |
| Lua | Configuration | MIT |
| FreeType | Font rasterization (Linux/Windows) | FreeType License |

### Bundled

| Dependency | Purpose |
|------------|---------|
| HarfBuzz 10.1.0 | Text shaping (bundled with JUCE) |

---

## 20. Glass Component (macOS Native Blur)

### 20.1 Overview

Native glassmorphism via `jreng::GlassWindow`. Window replacement strategy with two blur methods.

| Class | Responsibility |
|-------|---------------|
| `BackgroundBlur` | Static utility. Applies blur via CG or NSVisualEffect. |
| `GlassComponent` | JUCE Component wrapper. Triggers blur on visibility. |
| `GlassWindow` | JUCE DocumentWindow with glass effect. Main window class. |

**Location:** `modules/jreng_gui/glass/`

**Platform:** macOS only. Other platforms use opaque background.

### 20.2 GlassWindow Constructor

```cpp
GlassWindow (juce::Component* mainComponent,
             juce::String const& name,
             juce::Colour colour,
             float opacity,
             float blur,
             bool alwaysOnTop,
             bool showWindowButtons = true);
```

### 20.3 Blur Methods

#### Primary: CoreGraphics (Private API)

Uses `CGSSetWindowBackgroundBlurRadius` via dynamic symbol lookup.

```cpp
typedef intptr_t CGSConnectionID;
using CGSMainConnectionID_Func = CGSConnectionID (*)();
using CGSSetWindowBackgroundBlurRadius_Func = int32_t (*)(CGSConnectionID, int32_t, int64_t);

auto CGSMainConnectionID = (CGSMainConnectionID_Func)dlsym (RTLD_DEFAULT, "CGSMainConnectionID");
auto CGSSetWindowBackgroundBlurRadius = (CGSSetWindowBackgroundBlurRadius_Func)dlsym (
    RTLD_DEFAULT, "CGSSetWindowBackgroundBlurRadius");

auto connection = CGSMainConnectionID();
CGSSetWindowBackgroundBlurRadius (connection, [window windowNumber], (int64_t)blurRadius);
```

Variable blur radius. Private API (may break in future macOS versions).

#### Fallback: NSVisualEffectView

```cpp
NSVisualEffectView* visualEffect = [[NSVisualEffectView alloc] initWithFrame:frame];
[visualEffect setMaterial:NSVisualEffectMaterialHUDWindow];
[visualEffect setState:NSVisualEffectStateActive];
[visualEffect setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
[newWindow setContentView:visualEffect];
```

System-managed blur. Public API (stable). No radius control.

#### Method Selection

Auto-selects CG if `CGSMainConnectionID` and `CGSSetWindowBackgroundBlurRadius` symbols are available at runtime. Falls back to NSVisualEffect otherwise.

### 20.4 Window Replacement Strategy

Both methods use window replacement:

1. Get peer's native NSView and its containing NSWindow
2. Create new NSWindow with blur-compatible configuration
3. Apply blur (CG API or NSVisualEffectView)
4. Move JUCE view from old window to new window
5. Close old window, show new window

```cpp
[newWindow setOpaque:NO];
[newWindow setBackgroundColor:[NSColor clearColor]];
[newWindow setLevel:NSFloatingWindowLevel];
[newWindow setTitleVisibility:NSWindowTitleHidden];
[newWindow setTitlebarAppearsTransparent:YES];
[newWindow setStyleMask:newWindow.styleMask | NSWindowStyleMaskFullSizeContentView];
[newWindow setMovableByWindowBackground:YES];
```

### 20.5 Error Handling

| Condition | Action |
|-----------|--------|
| CoreGraphics symbols unavailable | Fall back to NSVisualEffect |
| NSWindow nil | Skip blur, continue with opaque window |
| Component has no peer | Skip blur, no-op |
| Non-macOS platform | No-op |

Never crash. Gracefully degrade to opaque background.

### 20.6 Platform Support

| Platform | Status | Fallback |
|----------|--------|----------|
| macOS | Full support | CG -> NSVisualEffect |
| Windows | Not implemented | Opaque background |
| Linux | Not implemented | Opaque background |

---

Rock 'n Roll!
JRENG!
