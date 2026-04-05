# RECOMMENDATION — Session Serialization & Restore
Date: April 2026
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END has working `Terminal::State` and `AppState` ValueTree persistence (`state.xml`),
but restore is explicitly disabled in `MainComponent::initialiseTabs()` with a TODO.
The goal is full session restore: layout, split ratios, CWD, foreground process
relaunch, and scrollback buffer per pane.

---

## Research Summary

### What state.xml already contains (verified from source)

```
END
└── TABS (active, position, activePaneID)
    └── TAB
        └── PANES (direction, ratio, x, y, width, height)   ← PaneManager tree
            ├── PANE (id=uuid)
            │   └── SESSION (cwd, foregroundProcess, title, displayName,
            │                cols, visibleRows, scrollOffset,
            │                MODES/*, NORMAL/*, ALTERNATE/*)
            └── PANE (id=uuid2)
                └── SESSION (...)
```

`cwd` and `foregroundProcess` are already persisted via `State::flush()` →
`flushStrings()` at every timer tick. The data is there. Restore just was never
wired.

### Why restore is currently a no-op

`MainComponent::initialiseTabs()` nukes saved tabs unconditionally:

```cpp
// TODO: State restoration disabled — fix after renderer and tabs cleanup
appState.getTabs().removeAllChildren (nullptr);
appState.setActiveTabIndex (0);
tabs->addNewTab();
```

### Process relaunch

`foregroundProcess` is captured from the PTY foreground PID at every State flush.
On restore, if `foregroundProcess` differs from the configured shell binary,
Session is told to launch that process instead. Session has no knowledge of
whether this is a restore — it just receives `setShellProgram()` before
`resized()`. Established pattern, unchanged contract.

### Scrollback buffer

`Cell` is 16 bytes, trivially copyable. `Grapheme` and `RowState` are
trivially copyable. Cell.h explicitly documents MemoryBlock storage as
the intended serialization path.

Grid's ring buffer: `HeapBlock<Cell>` + `HeapBlock<Grapheme>` +
`HeapBlock<RowState>`, sized `totalRows × cols` where `totalRows` is
the next power-of-two above `visibleRows + scrollbackCapacity`.

Scrollback is linearized on save (oldest → newest), compressed with zlib.
On restore with matching cols: written directly into the live ring.
On restore with mismatched cols (window resized between sessions):
loaded into a temp buffer at saved dims, then `Grid::reflow()` (existing
private machinery, unchanged) reflowed to current dims.

---

## Principles & Rationale

### BLESSED compliance

**Tell, not ask — the full chain:**

```
MainComponent   tells → Tabs::restoreFromState (savedTabsTree)
Tabs            tells → Panes::restoreFromState (savedPanesTree) per TAB
Panes           tells → paneManager.loadState (savedPanesTree)
Panes           tells → terminal->setScrollbackFile (scrollbackDir / uuid + ".bin")
Panes           tells → terminal->setWorkingDirectory (cwd)
Panes           tells → terminal->setShellProgram (fgProcess, {}) if needed
resized()       tells → grid.resize (cols, rows)
callAsync block tells → grid.loadScrollback (scrollbackFile)   ← deferred to settled dims
callAsync block tells → tty->open (...)
```

No orchestrator interrogates any subordinate. Each layer receives what it
needs and executes without asking.

**Established patterns extended, not replaced:**

`scrollbackFile` is a `juce::File` member on Session, mirroring `workingDirectory`,
`shellOverride`, and `shellArgsOverride` exactly. Same lifecycle: stored before
`resized()`, consumed once in the `callAsync` open block, never read again.

`paneManager.loadState(savedTree)` is one new method: `state = savedTree`. 
PaneManager is a layout engine. Panes tells it the tree. `layout()` does the rest.

**Why scrollback load is deferred to the callAsync block:**

Session's `resized()` fires multiple times during component layout before
`tty->isThreadRunning()` becomes true. The `callAsync` block already
re-runs `grid.resize(finalCols, finalRows)` with settled dims before
`tty->open()`. Loading scrollback here guarantees correct ring geometry.
The PTY hasn't opened. The reader thread doesn't exist. No lock contention.

**Why no `if (scrollbackFile.existsAsFile())` guard in Session:**

That is a boolean poll — a BLESSED **Stateless** violation. Session calls
`grid.loadScrollback(scrollbackFile)` unconditionally. Grid encapsulates
the "does this file exist and is it valid" decision. That is Grid's
boundary, not Session's concern.

---

## Scaffold

### 1. On-disk format

One binary file per pane: `~/.config/end/scrollback/<uuid>.bin`

```
Header (24 bytes, uncompressed):
  uint32_t  magic          = 0x454E4453   // "ENDS"
  uint32_t  version        = 1
  int32_t   cols
  int32_t   visibleRows
  int32_t   scrollbackRows                // scrollbackUsed at save time
  int32_t   totalRows                     // scrollbackRows + visibleRows

Payload (zlib-compressed, level 6):
  For each row [oldest scrollback → newest visible row]:
    Cell[cols]        — cols * sizeof(Cell)
    Grapheme[cols]    — cols * sizeof(Grapheme)
    RowState          — sizeof(RowState)
```

### 2. New API surface — minimal

#### `Grid`

```cpp
// Grid.h — two new public methods
void saveScrollback (const juce::File& file) const;  // MESSAGE THREAD, acquires resizeLock
void loadScrollback (const juce::File& file);         // MESSAGE THREAD, no lock needed
```

`saveScrollback` — linearizes ring from oldest scrollback to newest visible row
using direct Buffer access (private method, has full ring geometry). Writes
header then zlib-compressed payload. Acquires `resizeLock` to serialise
against the reader thread.

`loadScrollback` — reads header, validates magic + version. If cols match:
writes rows directly into the live ring using the same physical index math
as save. If cols differ: allocates a temp Buffer at saved dims, loads into
it, calls `reflow(tempBuffer, savedCols, savedVisibleRows, buffers[normal],
finalCols, finalRows)`. Sets `buffer.scrollbackUsed`. Calls
`state.setScrollbackUsed(count)`. Calls `markAllDirty()`. Returns without
loading if file is missing, unreadable, or has wrong magic/version.

Ring linearization math (inside `Grid::saveScrollback`, has Buffer access):

```cpp
// k = 0: oldest scrollback row, k = total-1: bottom visible row
const int physIdx = (buf.head - buf.allocatedVisibleRows + 1 - sbkRows + k) & buf.rowMask;
```

#### `Session`

```cpp
// Session.h — one new member, one new setter
juce::File scrollbackFile;                                     // mirrors workingDirectory
void setScrollbackFile (const juce::File& file);              // called by Panes before resized()
void saveScrollback (const juce::File& file) const;           // delegates → grid.saveScrollback
```

`resized()` callAsync block — modified to consume `scrollbackFile` at the
correct moment (after second `grid.resize`, before `tty->open`):

```cpp
juce::MessageManager::callAsync ([this]
{
    if (not tty->isThreadRunning())
    {
        const int finalCols { state.getCols() };
        const int finalRows { state.getVisibleRows() };

        grid.resize (finalCols, finalRows);
        parser.resize (finalCols, finalRows);

        grid.loadScrollback (scrollbackFile);   // ← NEW: unconditional, Grid handles invalid

        // ... applyShellIntegration, tty->open unchanged ...
    }
});
```

#### `Terminal::Component`

```cpp
void saveScrollback (const juce::File& file) const;      // delegates → session
void setScrollbackFile (const juce::File& file);         // delegates → session
```

#### `Panes`

```cpp
void saveScrollback (const juce::File& scrollbackDir) const;  // iterates panes, saves per UUID
// restoreFromState — calls setScrollbackFile + setWorkingDirectory + setShellProgram per terminal
// closePane — deletes scrollbackDir/uuid.bin on pane close
```

#### `Tabs`

```cpp
void saveScrollback (const juce::File& scrollbackDir) const;  // iterates all Panes
```

#### `MainComponent`

```cpp
void saveScrollback();   // tabs->saveScrollback (configDir / "scrollback")
```

#### `Main.cpp — ENDApplication::systemRequestedQuit()`

```cpp
void systemRequestedQuit() override
{
    if (mainWindow != nullptr)
    {
        if (auto* content { dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) })
        {
            appState.setWindowSize (content->getWidth(), content->getHeight());
            content->saveScrollback();   // ← NEW: before appState.save(), before quit()
        }
    }

    appState.save();
    quit();
}
```

Save fires while components are alive. TTY reader thread is still running.
`resizeLock` correctly serialises Grid reads against it.

### 3. Restore path — `MainComponent::initialiseTabs()`

Remove the TODO block. Replace with:

```cpp
auto savedTabs { appState.getTabs() };
const int savedTabCount { /* count TAB children in savedTabs */ };

if (savedTabCount > 0)
    tabs->restoreFromState (savedTabs, scrollbackDir);
else
    tabs->addNewTab();
```

`Tabs::restoreFromState` iterates TAB children, creates one Panes per TAB,
calls `Panes::restoreFromState(savedPanesTree, scrollbackDir)`, grafts
`panes.getState()` into the AppState TAB node, adds the JUCE tab entry.

`Panes::restoreFromState` walks PANE leaves. Per leaf:
- Reads `cwd` and `foregroundProcess` from the SESSION child
- Creates `Terminal::Component::create(font, cwd)`
- Calls `setShellProgram(foregroundProcess, {})` if `foregroundProcess`
  is non-empty and differs from the configured shell binary
- Calls `setScrollbackFile(scrollbackDir / uuid + ".bin")`
- Wires callbacks
- Creates `PaneResizerBar` for each internal PANES node with 2 children
- Calls `paneManager.loadState(savedPanesTree)` once after all leaves

`resized()` fires naturally from component layout. The callAsync block
loads scrollback and opens PTY.

### 4. File cleanup

Pane closed normally → `closePane()` deletes `scrollbackDir / uuid + ".bin"`.
Stale files from crashes → orphaned, harmless, never referenced again.

---

## BLESSED Compliance Checklist

- [x] **Bounds** — Grid owns save/load. File lifecycle tied to pane UUID.
  No shared pointers, no aliasing, clear destruction order.
- [x] **Lean** — Two methods on Grid, one method per layer up the stack.
  `PaneManager::loadState` is one line. No new abstraction layers.
- [x] **Explicit** — Magic + version header. Format is self-describing.
  `scrollbackFile` is a named member with a named setter. No hidden paths.
- [x] **SSOT** — One file per UUID. Grid is sole reader/writer of buffer
  data. AppState remains sole owner of layout tree.
- [x] **Stateless** — `scrollbackFile` is transient init data consumed once.
  Session carries no restore-mode state. Grid decides validity internally.
- [x] **Encapsulation** — Buffer internals never leave Grid. External callers
  see only `File`. PaneManager owns geometry. Panes owns component lifecycle.
  Neither crosses the other's boundary.
- [x] **Deterministic** — Same buffer state → same file. Same file + matching
  dims → identical buffer. Mismatched dims → reflow path, deterministic output.

---

## Open Questions

None. All decisions settled by ARCHITECT during session.

---

## Handoff Notes

- `ttyOpened` is not a flag in Session — the guard is `tty->isThreadRunning()`.
  The callAsync open block already re-runs `grid.resize(finalCols, finalRows)`
  with coalesced dims. `loadScrollback` inserts after that second resize.

- `reflow()` is a private Grid method. `loadScrollback` (also a Grid method)
  calls it directly. No visibility change needed.

- The existing `addNewTab()` path is completely untouched. Restore is a
  parallel entry path in `initialiseTabs()` that converges at the same
  post-construction state.

- Scrollback dir: `Config::getContext()->getConfigFile().getParentDirectory()
  .getChildFile("scrollback")`. Created on first save, no pre-creation needed.

- `foregroundProcess` relaunch applies unconditionally when the saved value
  differs from the shell binary. No denylist. User-facing behavior.

- COUNSELOR must not touch `Grid::reflow()` — it is existing private machinery
  called from `Grid::resize()`. `loadScrollback` calls it via the same path.
  Modify signature only if it becomes necessary to pass the temp buffer.
  Verify access pattern before touching.

---

*Rock 'n Roll!*
**JRENG!**

*Version 0.1 — April 2026*
