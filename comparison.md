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

---

## Architecture & Pipeline Topology
> Structural analysis — input-to-pixel path, thread model, synchronization, memory layout.
> Sources: direct codebase analysis of each project (May 2025).

### Thread Model & Hop Count

| | END | WezTerm | Ghostty | Kitty | Alacritty | xterm | Zed |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Hot-path threads | 2 | 4 | 4 | 2 | 2 | 1 | 2 + async |
| Keystroke → PTY hops | **0** | 1 | 2 | 1 | 0 (async) | 0 | 1 |
| PTY → Grid hops | 0 | 2 | 1 | 1 | 0 | 0 | 0 |
| Grid → Pixel hops | 0 | 1 | 0 | 0 | 0 | 0 | 1 |
| **Total round-trip hops** | **1** | **4** | **3** | **2** | **1** | **0** | **2** |

> Each hop = OS scheduler wakeup (1–10 ms typical) + synchronization cost.
> xterm's 0-hop `select()` loop blocks rendering during heavy parse — a constraint END avoids by splitting parse and render across two threads.

### Synchronization Model

| | END | WezTerm | Ghostty | Kitty | Alacritty | xterm | Zed |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Grid lock | **None** | Mutex | Mutex | 3× Mutex | FairMutex | N/A | FairMutex |
| Sync primitive | atomic release/acquire | parking_lot::Mutex + channels + socketpair | std.Thread.Mutex + SPSC queues | pthread_mutex + wakeup pipe | FairMutex + mpsc | select() | FairMutex + unbounded channel |
| Renderer stalls on parser | **No** | Yes | Yes | Yes | Yes | N/A (same thread) | Yes |
| Grid snapshot copy | **No** (non-owning view) | No | No | No (mapped GPU buffer) | No | No | **Yes** (full viewport clone) |

> END is the only modern GPU-accelerated terminal with a lock-free hot path. The atomic release/acquire contract between reader thread (Grid writes) and message thread (Grid reads) is the same pattern used in real-time audio plugin architectures — proven across decades of commercial deployment in DAW hosts where a mutex on the hot path is a fatal error.

### Cell Memory Layout

| | END | Ghostty | Kitty | Alacritty / Zed | WezTerm | xterm |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Cell size | **8 B** | **8 B** | 32 B (12 CPU + 20 GPU) | ~40+ B | Variable (heap) | 1–4 B |
| Structure | `uint64_t` packed | `packed struct(u64)` | `CPUCell` + `GPUCell` | `char` + 2× `Color` + `Flags` + `Option<Arc>` | `TeenyString` + `CellAttributes` + `Option<Box>` | `IChar` + attribs |
| Cells per cache line | **8** | **8** | 2 | 1–2 | 1–2 | 16–64 |
| Heap alloc per cell | **Never** | Never | Never | On extras (hyperlink, wide) | On >7-byte grapheme + fat attrs | Never |

> xterm's cells are smallest but carry no style inline — attributes are stored per-run, not per-cell. END and Ghostty achieve the densest modern cell layout at 8 bytes with full style ID, codepoint, width, and content tag packed into a single machine word.

### Keystroke Input Path

| | END | WezTerm | Ghostty | Kitty | Alacritty | xterm | Zed |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Input → PTY | Synchronous write, same thread | Channel → ThreadedWriter thread | SPSC mailbox → IO writer thread → xev Stream | Mutex → wakeup pipe → IO thread | mpsc channel → PTY thread poll | Direct `write()` in select loop | Channel → alacritty IO thread |
| Thread crossing | **None** | 1 | 2 | 1 | 1 (fire-and-forget) | None | 1 |
| Encoding location | Message thread | GUI thread | Main thread | Main thread | Main thread | Main thread | Main thread |

> END and xterm are the only terminals where keystroke-to-PTY-write is fully synchronous with zero thread crossing. Every other terminal enqueues the encoded bytes onto a channel or mailbox for an IO thread to drain.

### Render Trigger

| | END | WezTerm | Ghostty | Kitty | Alacritty | xterm | Zed |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Trigger mechanism | Event-driven (VT listener) | SpawnQueue → invalidate | xev.Async wakeup → renderer thread | Synchronous after parse | Frame timer (monitor-aligned) | X11 Expose + flush after parse | cx.notify() on terminal mutation |
| Polling interval | **None** | None | None | None | Monitor-aligned timer | None | None |
| VSync gate | Platform compositor | Platform default (on) | CVDisplayLink (macOS) / xev (Linux) | Platform-dependent | **Disabled** (SwapInterval::DontWait) | None | Compositor-paced |

### Rendering Backend

| | END | WezTerm | Ghostty | Kitty | Alacritty | xterm | Zed |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| GPU backend | JUCE OpenGL | Glium (GL) or wgpu | Metal (macOS) / GL (Linux) / WebGL | OpenGL 3.3 / 3.1 | OpenGL (glutin) | None | Metal (GPUI) |
| CPU fallback | **✓** | ✗ | ✗ | ✗ | ✗ | Xlib (only option) | ✗ |
| Draw model | Component paint (event-driven) | Demand-driven (invalidate) | Frame-driven (wakeup + CVDisplayLink) | Immediate after parse | Frame timer | Expose + flush | Scene graph (GPUI) |

### Conclusion

END's architecture is the leanest input-to-pixel pipeline of any modern GPU-accelerated terminal:

- **1 thread hop** total (reader → message), matching Alacritty and beating Kitty (2), Ghostty (3), WezTerm (4).
- **Zero locks on the hot path** — the only GPU terminal to achieve this. Every competitor holds a mutex while the parser mutates the grid, stalling the renderer.
- **Zero-hop synchronous keystroke delivery** — shared only with xterm's single-threaded design, without xterm's render-blocking constraint.
- **8-byte cells** — matched only by Ghostty. 4–5× denser than Alacritty/WezTerm/Zed, directly impacting cache performance on scroll, erase, and viewport iteration.
- **Event-driven repaint** — no polling timer, no VSync gate between state change and render scheduling.
- **CPU fallback** — the only GPU terminal that also ships a software renderer.

The lock-free atomic synchronization pattern is not a novel experiment — it is the standard architecture for real-time audio plugin pipelines, battle-tested across decades of commercial deployment in DAW hosts on every platform.
