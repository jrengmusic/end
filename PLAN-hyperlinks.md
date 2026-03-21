# END — Open File & Hyperlink Feature

**Date:** 2026-03-21
**Status:** DRAFT

---

## Overview

Clickable and keyboard-navigable links in terminal output. Works with any app that produces file paths (ls, eza, find, tree, fd, rg, git status) and URLs. Two interaction modes share a single span index.

---

## Interaction Modes

### Click Mode (always-on)
Detected file paths and URLs are underlined. Click opens:
- Files: editor command sent to active PTY (in-place, like typing `nvim MySource.cpp`)
- URLs: default browser via `juce::URL::launchInDefaultBrowser()`

### Open File Mode (modal)
Triggered by prefix + `o` (configurable). Uses `ModalType::openFile` in `Terminal::State`.

1. Scan visible viewport for file paths and URLs
2. Overlay vimium-style hint labels (a-z, aa-zz) at each target
3. User types label character(s) to open
4. `Escape` exits modal
5. StatusBarOverlay shows `-- OPEN FILE --`

All keys intercepted via the existing modal gate (`State::isModal()` check in `keyPressed()` before Action system).

---

## Span Index

### Entry Types

| Type | Detection | Action |
|------|-----------|--------|
| File | Extension match against built-in set | Send `{editor} {path}\n` to active PTY |
| URL | Protocol prefix (`https://`, `http://`, `ftp://`) | `juce::URL::launchInDefaultBrowser()` |
| OSC 8 | Explicit hyperlink from application | Route by scheme: `file://` to editor, `http(s)://` to browser |

### Data Structure

```cpp
struct LinkSpan
{
    int row;
    int col;
    int length;
    juce::String uri;          // file:///path or https://...
    enum Type { file, url, osc8 } type;
    uint64_t blockId;          // for invalidation (0 = OSC 8, never purged)
};
```

Canonical `(row, col, len) -> URI` mapping. Serves both click mode (hit-test on mouse position) and open-file mode (enumerate for label overlay).

---

## Pipeline

### 1. Link Detection

Two paths, same output (span index entries):

**Path A: OSC 133 shell integration (when available)**

| OSC Sequence | Action |
|---|---|
| `OSC 133 ; C` | Open scan window. Generate block ID. Record buffer start offset. |
| `OSC 133 ; D` | Close window. Scan output block for links. |
| `OSC 7 ; file://host/path` | Update CWD in State (already implemented). |
| `OSC 8 ; params ; uri` | Write directly to span index, bypass scanner. |

On `D`: strip ANSI escapes, tokenize, check extensions/protocols, resolve paths against CWD, write to span index.

**Path B: On-demand scan (fallback, no OSC 133)**

When entering Open File mode (prefix + `o`): scan the entire visible viewport + scrollback visible rows. Same tokenize + extension/protocol check. Slower but works universally.

Path A is incremental (per command output block). Path B is full-viewport (on mode entry). Both produce the same span index entries.

### 2. Link Resolution

For file tokens:
1. Resolve against `State::getCwd()` -> absolute path
2. Build URI: `file:///absolute/path`
3. Write to span index

For URL tokens:
1. URI is the token itself
2. Write to span index

### 3. OSC 8 Priority

OSC 8 spans are authoritative. Heuristic spans never overwrite existing OSC 8 entries at the same position.

---

## Editor Dispatch

When a file link is activated (click or hint label):

1. Resolve the editor command from the fallback chain
2. Send `{editor} {path}\n` directly to the active PTY
3. No popup, no new PTY — in-place execution, as if the user typed it

### Editor Fallback Chain

Runtime: check each in order via `access(path, X_OK)` on `$PATH`. Cache the result.

Default chain: `nvim` -> `vim` -> `vi` -> `nano`

User override in config replaces the entire chain.

---

## Hint Label Rendering

Rendered in the existing GL pipeline, same layer as selection highlight:

1. `Render::Background` quad at the path start cell (colored background, e.g., yellow)
2. `Render::Glyph` instance for the label character (e.g., 'a') at the same cell position
3. If 2-char label: two consecutive cells

Labels assigned alphabetically by row order (top-left first). Single char `a-z` for first 26 targets. Two char `aa-az, ba-bz...` beyond 26.

The hint labels are built during `buildSnapshot()` when `ModalType::openFile` is active. A separate array in `Render::Snapshot` holds the hint overlay data. Drawn after backgrounds, before glyphs (so the label character is on top of the colored background).

### Click Mode Rendering (always-on)

Detected paths/URLs get a subtle underline. Implementation: set an underline flag or use a thin `Render::Background` quad at the bottom of the cell row. No glyph change.

---

## State (parameterMap)

| Parameter | Type | Description |
|---|---|---|
| `modalType` | `ModalType` enum | `openFile` when active (existing infrastructure) |
| `scanPending` | `bool` | Set on OSC 133 C, cleared on D |
| `currentBlockId` | `uint64` | Monotonic counter, incremented each C |

CWD already tracked via existing `State::getCwd()`.

No manual callbacks. StatusBarOverlay polls `modalType` from State via existing `onRepaintNeeded` mechanism.

---

## Config (end.lua)

```lua
hyperlinks = {
    -- Editor fallback chain. First available on $PATH wins.
    -- Set to a single string for no fallback: editor = "nvim"
    editor = { "nvim", "vim", "vi", "nano" },

    -- Additional file extensions to detect (merged with built-in set).
    -- Built-in covers common source code extensions out of the box.
    extensions = {},

    -- URL protocols to detect. Built-in default shown.
    protocols = { "https", "http", "ftp" },
},

keys = {
    -- Enter open-file mode. Prefix-mode key.
    enter_open_file = "o",
},

colours = {
    -- Hint label background colour.
    hint_label_bg = "#FFDDAA00",

    -- Hint label text colour.
    hint_label_fg = "#FF000000",

    -- Underline colour for detected links (click mode).
    link_underline = "#8000C8D8",
},
```

### Built-in Extension Set

Compiled into the binary. User's `extensions` table merges on top.

```
Source code:  .c .cpp .h .hpp .cc .hh .rs .go .py .js .ts .jsx .tsx
              .java .kt .swift .rb .lua .sh .bash .zsh .fish .pl .zig
              .asm .s .cs .fs .ex .exs .clj .scala .r .m .mm
Markup/data:  .md .txt .json .yaml .yml .toml .xml .html .css .csv
              .sql .graphql .proto
Build/config: .makefile .cmake .dockerfile .gitignore .editorconfig
              .env .ini .cfg .conf
```

---

## Action Registration

| Action ID | Config Key | Default | isModal | Description |
|-----------|-----------|---------|---------|-------------|
| `enter_open_file` | `keys.enter_open_file` | `o` | true | Enter open-file modal mode |

Open-file-internal keys (a-z for hint labels, Escape to exit) are handled directly by `handleOpenFileKey()` in the modal dispatch — not registered as configurable actions (the hint labels are dynamic, not user-configurable keys).

---

## Key Dispatch (existing infrastructure)

```
keyPressed()
    |
    +-- State::isModal()?
    |       |
    |       +-- ModalType::selection  -> handleSelectionKey()   (existing)
    |       +-- ModalType::openFile   -> handleOpenFileKey()    (NEW)
    |
    +-- Action::handleKeyPress()      (prefix + o -> enter_open_file)
    +-- PTY forward
```

### handleOpenFileKey()

- `a-z`: single-char label match -> if unique, dispatch immediately. If ambiguous (2-char label prefix), wait for second char.
- `Escape`: exit modal, clear hint overlays
- All other keys: consumed (fully modal)

---

## StatusBarOverlay

Existing component. Add case for `ModalType::openFile`:

```cpp
void update (ModalType type, int selectionType)
{
    // existing selection cases...

    if (type == ModalType::openFile)
        label = "-- OPEN FILE --";
}
```

---

## Implementation Steps

| Step | Description | Size |
|------|-------------|------|
| 1 | Rename SelectionOverlay -> StatusBarOverlay, add openFile case | Small |
| 2 | Add `ModalType::openFile` handling in `handleModalKey()` | Small |
| 3 | Config keys + action registration (enter_open_file, colours, extensions) | Small |
| 4 | Built-in extension set + extension match utility | Small |
| 5 | On-demand viewport scanner (Path B — no OSC 133 dependency) | Medium |
| 6 | Span index data structure | Small |
| 7 | Hint label rendering in GL pipeline | Medium |
| 8 | Editor fallback chain resolution | Small |
| 9 | File dispatch (send editor command to PTY) | Small |
| 10 | URL dispatch (launch in browser) | Trivial |
| 11 | Click mode: underline rendering + mouse hit-test | Medium |
| 12 | OSC 133 integration (Path A — incremental scanning) | Medium |
| 13 | OSC 8 explicit hyperlink support | Small |

**Critical path:** 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 (minimum viable open-file mode)
**Parallel after step 6:** 10, 11, 12, 13

---

## Non-Goals

- Stat-based path validation (extension registry is the sole gate)
- Recursive directory expansion
- Shell command parsing / intent detection
- Editor plugin integration (just launches the editor with the file path)
