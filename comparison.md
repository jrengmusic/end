# Terminal Feature Parity Comparison
> END vs WezTerm vs Ghostty vs Kitty vs Alacritty vs foot
> Last updated: April 2026

---

## Platform Support

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Windows | ✓ 10 22H2+ | ✓ | ✗ | ✗ | ✓ | ✗ |
| macOS | ✓ 10.14+ | ✓ | ✓ 12+ | ✓ | ✓ | ✗ |
| Linux | WIP | ✓ | ✓ | ✓ | ✓ | ✓ |
| Wayland | WIP | ✓ | ✓ | ✗ | ✗ | ✓ only |

---

## Renderer

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| GPU backend | ✓ | ✓ | ✓ Metal/GL | ✓ OpenGL | ✓ OpenGL | ✓ OpenGL |
| CPU fallback | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Lock-free pipeline | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| VT conformance | ✓¹ | Good | Good | Good | Good | Best |

> ¹ Comprehensive unit tests covering all ANSI, DEC, OSC, CSI sequences. Hardware-specific sequences (DECLL, printer passthrough, serial control) correctly excluded — no software terminal implements these.

---

## Font Stack

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Shaper | HarfBuzz | HarfBuzz | HarfBuzz | HarfBuzz | HarfBuzz | HarfBuzz |
| Rasterizer | FreeType | FreeType | FreeType / CoreText | FreeType | FreeType | FreeType |
| Emoji / CJK delegation | CoreText + DirectWrite | ✗ | CoreText | ✗ | ✗ | ✗ |
| Ligatures | ✓ | ✓ | ✓ | ✓ | ✗ | ✓ |
| Nerd Fonts | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Full emoji / ZWJ | ✓ | ✓ | ✓ | ✓ | ~ | ✓ |
| Proprietary bundled font | ✓ Display + Mono² | ✗ | ✗ | ✗ | ✗ | ✗ |

> ² Display and Display Mono — proportional and monospace variants, 3 weights each (Book, Medium, Bold). Purpose-designed for END's rendering target.

---

## Multiplexer

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Built-in mux | ✓ | ✓ | ✗ | ✗ | ✗ | ✗ |
| Daemon / session persistence | ✓ | ~ | ✗ | ✗ | ✗ | ✗ |
| Tab management | ✓ | ✓ | ✓ | ✓ | ✗ | ✗ |
| Pane split | ✓ | ✓ | ✓ | ✓ | ✗ | ✗ |
| Scriptable splits | ✓ Lua | ✓ Lua | ✗ | ~ kittens | ✗ | ✗ |
| Modal keybinding | ✓ native | ✓ Lua | ✗ | ✗ | ✗ | ✗ |
| Popup (tmux-style) | ✓ customizable | ✗ | ✗ | ✗ | ✗ | ✗ |
| Action list / keybinder | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |

---

## Scripting & Config

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Scripting language | Lua | Lua | ✗ | Python | ✗ | ✗ |
| Full API surface | ✓ | ✓ | ✗ | ~ | ✗ | ✗ |
| Hot reload | ✓ file watcher | ~ | ✗ | ✓ | ✓ | ✓ |
| Config as inline docs | ✓ kickstart-style | ✗ | ✗ | ✗ | ✗ | ✗ |

---

## Image & Rich Content

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Kitty graphics protocol | WIP | ✓ | ✓ | ✓ | ✗ | ✗ |
| Sixel | WIP | ✓ | ✗ | ✓ | ✗ | ✓ |
| OSC 1337 | WIP | ✓ | ✗ | ✗ | ✗ | ✗ |
| Native Markdown renderer | ✓ no webstack | ✗ | ✗ | ✗ | ✗ | ✗ |
| Native Mermaid renderer | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |

---

## Visual & UI

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Native blur | ✓ CGSBackgroundBlur | ✗ | ~ NSVisualEffectView | ✗ | ✗ | ✗ |
| Glassmorphism Win 10+ | ✓ DwmExtendFrameIntoClientArea | ✗ | ✗ | ✗ | ✗ | ✗ |
| Glassmorphism Win 11 | ✓ Mica/Acrylic | ✗ | ✗ | ✗ | ✗ | ✗ |
| Tahoe NSGlassEffectView | WIP arm64 | ✗ | ✗ | ✗ | ✗ | ✗ |
| Shader background | ✓ multi-pass | ✗ | ✗ | ✗ | ✗ | ✗ |
| Shader post-pro overlay | ✓ single-pass | ✗ | ✗ | ✗ | ✗ | ✗ |
| Shadertoy compat + mouse | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Draggable tabs | WIP | ✓ | ✗ | ✗ | ✗ | ✗ |
| SVG tab buttons | WIP | ✗ | ✗ | ✗ | ✗ | ✗ |

---

## Distribution & Implementation

| Feature | END | WezTerm | Ghostty | Kitty | Alacritty | foot |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Language | C++17 / JUCE | Rust | Zig + Swift/GTK4 | C + Python | Rust | C |
| Notarized macOS binary | ✓ | ✓ | ✓ | ✓ | ✓ | N/A |
| Public binary | ✗ not yet | ✓ nightly only | ✓ | ✓ | ✓ | ✓ |
| License | Proprietary | MIT | MIT | GPL-2.0 | MIT | MIT |
| Maintenance | Active / dogfooded | Nightly, 1 maintainer | Active | Active | Active | Active |

---

## Legend

| Symbol | Meaning |
|---|---|
| ✓ | Fully supported |
| ~ | Partial / limited |
| ✗ | Not supported |
| WIP | In development |
