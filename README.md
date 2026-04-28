# END
**Ephemeral Nexus Display**

The long and winding road finding a cross-platform, dual-backend renderer, rich-featured, modern terminal emulator with a non-web stack native Markdown and Mermaid renderer that can run on your grandma's PC finally comes to END.

---

## Why

Most modern popular terminals available today (as this README is written) fall into two categories: boring and wrong.

**Boring:** no creativity drives the idea. Everything starts with a name — the way you do anything permeates the way you do everything. Most popular modern terminals are named with *term and *tty. Everyone is just trying to solve the same problem with a different language, slapping marketing hype on it as "fast and beautiful GPU-accelerated." The OOTB experience is mostly identical: black screen with grey monospace glyphs. Where is the beauty in that? Most users immediately set background blur and transparency.

**Wrong:** terminal core processing is half-century technology. Nothing novel. Unbounded and decoupled from the modern operating system. All default terminals shipped with any OS factually suck — probably no one who uses a terminal as their daily driver for a dev environment ever uses the one shipped with their chosen OS. Why limit support of your fancy terminal to decade-old Intel Macs? Why can't I use your popular native Mac renderer terminal on my old iMac? Why is there no working terminal that runs identically with a singular config on Windows 10? On Windows 11? MSYS2 faithfully delivers an identical shell experience without missing any of your chosen favorite CLI/TUI tools as native Windows binaries. Assuming your terminal should and can only be used with WSL2 on Windows is wrong. Using WSL2 on Windows for native binary development is wrong.

---

## Architecture

I really hate the term "AI slop." The generalization that every piece of open source software written, developed, and assisted by coding agents is slop is Idiocracy. Just because I could ask my clankers to translate a popular terminal emulator into my domain does not mean it would automagically manifest into the best terminal ever developed.

END's architecture is nowhere to be found in any published open source terminal project. This is beyond any training data any existing LLM could have been trained on. The peculiar choice of JUCE — a framework notoriously designed for building cross-platform audio plugins — may be counterintuitive to those unfamiliar with the stack.

This is my domain-specific expertise. A domain where lock-free threads and block-free message threads are non-negotiable. END is built upon that exact philosophy. No mutex anywhere. An architectural pattern that has tripped even the most advanced pattern-matching clankers from truly comprehending the machinery.

The state machine on the message thread is the true Single Source of Truth. All objects are designed as stateless dumb objects — no poking internals. Virtually no object has a getter, only setters. Orchestrator classes always tell, never ask. Orchestrators listen to state machine changes and react accordingly.

What gives? Everything is event-driven. No manual lambda callbacks, no manual boolean tracking, no manual orchestration with stateful objects dictating the state machine. Concurrency is guaranteed by atomic operations. The user interface is never blocked. You will never see a spinning wheel — even when a process is eating 99% CPU.

---

## What

END's development was hardened on an iMac 5K 2015 — tested intensively on both macOS Monterey and Windows 10 22H2 via Boot Camp with MSYS2, with a singular monorepo config. Everything works identically.

On more modern hardware, END is used daily to develop a cross-platform audio plugin, a cross-platform C++ debugger (whatdbg), and END itself, across:

- Windows 11 (Asus ROG Ally Z1 Extreme)
- MacBook Pro M4 (macOS Tahoe, Windows 11 via UTM without GPU)

The result: a consistent development experience with a singular monorepo configuration. With END as your nexus, I have almost never cared which OS I was on.

### Renderer

Performance is a metric genuinely difficult to measure. Every other terminal can show you a different benchmark — quantitative numbers that are ultimately unrelated to actual real-life usage. How about a simple raw byte test instead: side-by-side comparison, same machine, same window size, same font size, identical settings per terminal. Let it be your judge.

Modern terminals often glorify "GPU acceleration" as a jargon badge, claiming superiority by default. With correct architecture, software rendering relying on CPU alone can perform as fast as GPU. The overused term "modern computer" — with its generalization that all current machines can render with GPU — is inaccurate. xterm has proven to be a reliable terminal without GPU, and so does END. Software rendering is not just a fallback when GPU acceleration is unavailable. It is an option. A reliable rendering option with END.

On an iMac 5K 2015, END consistently finishes 150–200ms ahead of the field — GPU and CPU rendering both.


Under the hood, END runs a lock-free render pipeline. VT conformance is covered by a comprehensive unit test suite across all ANSI, DEC, OSC, and CSI sequences. Hardware-specific sequences — DECLL, printer passthrough, serial control — are correctly excluded. No software terminal implements them.

### Fonts

Beauty is in the END of the beholder. While users can choose whatever font suits their taste, END ships with its own proprietary typefaces: Display Mono, a monospace font, and Display, a proportional one.

Display Mono is not just about aesthetics — it is about correctness. It was designed with code readability and legibility as the first and foremost priority. Ligatures and special characters are purposefully shaped with distinguishable proportional sizing inside the monospace cell.

Most importantly, it is the only font that works out of the box without installation, never fails, and renders consistently across operating systems — providing thousands of Nerd Fonts symbols while gracefully delegating monospace and colored emoji, CJK, and ZWJ sequences to the native OS font rendering system via CoreText on macOS and DirectWrite on Windows.

### Multiplexer

I have always liked tmux — the consistent UX, portability, reliability, and of course session persistence. But its configurable flexibility also comes with a huge pain in the ass to set up.

END ships with a limitless number of tabs. Each tab can be split into multiple panes, each with an isolated terminal instance, with zero setup.

Don't like the tab button image? Drop in an SVG you like by following the convention of active/inactive state — left/right groups are the edge anchors, center is the stretchable area.

Fancy automation with split layouts? The example is in `action.lua` itself. The config is the documentation. A single keypress or a modal prefix shortcut à la tmux will bring you home. Modal keybinding is native — not scripted on top.

END also ships an action list and keybinder. Every action is discoverable and rebindable in one place.

Popup is one of the most underutilized tmux features — spawning a temporary terminal instance with any shell process you need: CLI, TUI, any shell script. END implements this as a modal window. After your process exits and returns to the normal screen, the popup dismisses itself automatically.

Do you really need session persistence? Keep terminal processes alive even after you quit the application, and resume whatever state was running when you left — enable the Nexus daemon server. END has you covered.

### Visuals

END's default config is inspired by nvim-kickstart. Everything you tweak is visible alongside comprehensive documentation about what it does and how.

Window glassmorphism is handled natively per OS. Auto mode picks the right default for you:

- **macOS** defaults to Core Graphics background blur — control blur amount, transparency, and tint
- **Windows 11** defaults to Acrylic
- **Windows 10 22H2** defaults to background blur

If that is not enough:

- macOS 10.14+ can choose NSVisualEffectView with WindowBackground and FullScreenUI materials
- Latest macOS has access to NSGlassEffectView with Regular and Clear
- Windows 11 can choose between Acrylic and Mica

The ricing on the cake. Most terminals with custom shader support stop at a single pass. END has two layers — a multi-pass background with ABCD buffers and opacity control, and a single-pass post-processing overlay on top of everything. Both respond to mouse input. It is a minified Shadertoy running inside your terminal.

Everything is hot-reloaded. Changes take effect the moment you save any Lua file.

### Markdown & Mermaid

Working side by side with your clankers, you will inevitably need to read documents — specs, plans, diagrams. Many CLI/TUI tools try to solve this with compromised monospace cells and forced beautification. END handles this natively with its own Markdown and Mermaid renderer. Exactly what you would expect reading from a web browser, without the bloated garbage of a web stack. Customize however you like — your choice of font, color, and sizes for each format element.

Headers, tokenized code blocks, bullet lists, tables, and Mermaid diagrams — all rendered natively. No fake header blocks. No fake tables. No fake raster diagram images. What You See Is What You Get — a hybrid encoder Markdown and Mermaid renderer, inside your terminal.

---

## How

**Requirements:** C++17 compiler, CMake, JUCE 8, Ninja, jam

```bash
./builds.sh          # Release
./builds.sh debug    # Debug
./builds.sh clean    # clean + rebuild Release
./builds.sh install  # clean build + install to system
```

Works identically on macOS, Linux, and Windows via MSYS2. On Windows, MSVC is located and configured automatically.

Config lives in `~/.config/end/`. Both files are auto-generated with documented defaults on first launch. Every value has inline comments. Changes are picked up automatically — no restart needed.

---

| Feature | Status |
|---------|--------|
| WHELMED Mermaid | In progress — basic rendering works, expanding coverage |

---

## License

MIT

---

*conceived with [CAROL](https://github.com/jrengmusic/carol)*

Rock 'n Roll!

**JRENG!**
