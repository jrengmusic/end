# SPEC-details.md

## END: Future Implementation Specifications

**Purpose:** Detailed specs for features not yet implemented. Extracted from SPEC.md to keep the main spec lean and forward-looking.

**Last Updated:** 2026-03-21

---

## 1. Terminal State Serialization and Session Restore

### 1.1 Overview

Full session persistence across app restarts. On quit, every END instance saves its complete state — window geometry, tab layout, split pane tree, terminal parameters, cursor state, and scrollback content. On launch, all saved instances are restored as windows in a single process.

**Design principle:** Save is ephemeral — happens once on quit, loaded once on launch. No runtime cost. Binary scrollback files are fast (`memcpy`-level I/O).

### 1.2 File Layout

```
~/.config/end/                          (macOS/Linux)
%APPDATA%/end/                          (Windows)
  end.lua                               (user config — never touched by serialization)
  state/
    <instance-uuid>.xml                 (per-window state: tabs, panes, session params)
  sessions/
    <session-uuid>.bin                  (per-terminal scrollback binary)
```

- Each running END instance has a unique UUID assigned at launch
- Each terminal session has a unique UUID (already exists as `jreng::ID::id` on SESSION)
- `state/` directory holds one XML file per instance
- `sessions/` directory holds one binary file per terminal

### 1.3 Instance State XML Schema

Each `<instance-uuid>.xml` contains a full AppState ValueTree:

```xml
<END>
  <WINDOW width="1200" height="800" zoom="1.0" x="100" y="100"/>
  <TABS active="0" position="bottom" activeTerminalUuid="abc-123">
    <TAB name="dev">
      <PANES direction="vertical" ratio="0.5">
        <PANE>
          <SESSION id="abc-123" cwd="/Users/me/dev/end"
                   shellProgram="zsh" displayName="end"
                   activeScreen="0" cols="120" visibleRows="35">
            <MODES originMode="0" autoWrap="1" applicationCursor="0"
                   bracketedPaste="0" insertMode="0" cursorVisible="1"
                   mouseTracking="0" mouseMotionTracking="0"
                   mouseAllTracking="0" mouseSgr="0" focusEvents="0"
                   applicationKeypad="0" reverseVideo="0"/>
            <NORMAL cursorRow="24" cursorCol="0" wrapPending="0"
                    scrollTop="0" scrollBottom="34"
                    head="10023" scrollbackUsed="10000"/>
            <ALTERNATE cursorRow="0" cursorCol="0" wrapPending="0"
                       scrollTop="0" scrollBottom="34"
                       head="0" scrollbackUsed="0"/>
            <PEN style="0" fgRed="255" fgGreen="255" fgBlue="255" fgMode="0"
                 bgRed="0" bgGreen="0" bgBlue="0" bgMode="0"/>
            <STAMP style="0" fgRed="255" fgGreen="255" fgBlue="255" fgMode="0"
                   bgRed="0" bgGreen="0" bgBlue="0" bgMode="0"
                   cursorRow="0" cursorCol="0" originMode="0" autoWrap="1"/>
          </SESSION>
        </PANE>
        <PANE>
          <SESSION id="def-456" .../>
        </PANE>
      </PANES>
    </TAB>
  </TABS>
</END>
```

**New properties vs current AppState:**
- `WINDOW`: adds `x`, `y` (screen position for multi-window restore)
- `TAB`: adds `name` (persisted tab name, no longer stray)
- `NORMAL`/`ALTERNATE`: adds `head`, `scrollbackUsed` (ring buffer geometry)
- `PEN`: new node — current SGR pen state (style, fg, bg)
- `STAMP`: new node — DECSC saved cursor state (pen + cursor position + modes)

### 1.4 Scrollback Binary Format

Each `sessions/<session-uuid>.bin` stores the normal screen's scrollback and visible content. Alternate screen content is NOT saved (the app that created it is dead on restore).

**Binary layout (little-endian):**

```
Offset  Size     Field
0       4        Magic: "END\0" (0x454E4400)
4       4        Version: 1
8       4        cols (int32)
12      4        totalRows (int32, power of two)
16      4        head (int32)
20      4        scrollbackUsed (int32)
24      4        visibleRows (int32)
28      4        cellCount (int32, = totalRows * cols)
32      4        graphemeCount (int32, number of non-zero grapheme entries)
36      4        reserved (0)
40      N        Cell data: cellCount * 16 bytes (raw memcpy of HeapBlock<Cell>)
40+N    M        Grapheme data: sparse entries, each = 4 (index) + 29 (Grapheme) = 33 bytes
40+N+M  R        RowState data: totalRows * 1 byte
```

**Grapheme sparse encoding:** Only non-zero Grapheme entries are stored. Each entry is:
- `uint32_t index` — flat index into the cells array
- `Grapheme data` — 29 bytes (extraCodepoints[7] + count)

This avoids storing ~50MB of mostly-zero grapheme data. Typical terminal usage has <1% grapheme cells.

**File size estimates (10K scrollback, 200 cols):**

| Component | Raw size | Typical |
|-----------|----------|---------|
| Header | 40 bytes | 40 bytes |
| Cells (16,384 * 200 * 16) | 52 MB | 52 MB |
| Graphemes (sparse) | 0 | <100 KB |
| RowState (16,384 * 1) | 16 KB | 16 KB |
| **Total** | **~52 MB** | **~52 MB** |

**Optional compression (future):** LZ4 or zlib on the cell data block. Empty cells compress extremely well (>90% ratio). Deferred — raw memcpy is fast enough for NVMe.

### 1.5 Save Flow (App Quit)

Triggered by `systemRequestedQuit()` or last window close.

```
1. For each open window (MainComponent instance):
   a. Collect window geometry (x, y, width, height, zoom)
   b. For each tab:
      i.   Store tab name on TAB node
      ii.  PANES tree already has direction/ratio — no action needed
      iii. For each terminal (SESSION):
           - Flush State atomics -> ValueTree (force flush)
           - Store ring geometry (head, scrollbackUsed) on NORMAL/ALTERNATE nodes
           - Store Parser pen/stamp as PEN/STAMP child nodes
           - Serialize Grid normal buffer to sessions/<session-uuid>.bin
   c. Write instance state to state/<instance-uuid>.xml
2. Clean up orphaned .bin files not referenced by any .xml
```

**Force flush:** `State::flush()` is normally timer-driven. On quit, call it explicitly to ensure all atomics are written to the ValueTree before serialization.

**Atomic write:** Write to a temp file, then rename. Prevents corruption if the app crashes mid-write.

### 1.6 Load Flow (App Launch)

```
1. Scan state/ directory for *.xml files
2. If no files found: normal fresh launch (single window, single tab, $HOME)
3. For each <instance-uuid>.xml:
   a. Parse XML -> ValueTree
   b. Create a new window (MainComponent) with saved geometry
   c. For each TAB in TABS:
      i.   Create Panes, restore PANES tree (direction/ratio)
      ii.  For each PANE leaf -> SESSION:
           - Create Terminal::Component with saved cwd
           - Restore State params from SESSION ValueTree
           - Restore Parser pen/stamp from PEN/STAMP nodes
           - Load sessions/<session-uuid>.bin -> Grid normal buffer
           - Restore ring geometry (head, scrollbackUsed)
           - Graft SESSION into PANE node
           - Spawn shell in saved cwd
      iii. Set tab name from TAB node
   d. Restore active tab index, active terminal UUID
   e. Bind AppState pwd to active terminal
4. Delete all state/*.xml and sessions/*.bin after successful restore
5. If any .xml fails to parse: skip that instance, log warning
6. If any .bin is missing: restore the session without scrollback (empty grid)
```

**Delete after restore:** State files are consumed on launch. The running instance will write fresh state on quit. This prevents stale state accumulation and handles the multi-instance to single-instance collapse.

### 1.7 Multi-Instance Behavior

| Scenario | Save behavior | Restore behavior |
|----------|--------------|------------------|
| 1 instance, quit | 1 XML + N bins written | 1 window restored |
| 3 instances, quit all | 3 XMLs + N bins written | 3 windows restored in 1 process |
| 3 instances, quit 2, keep 1 | 2 XMLs written; running instance untouched | Next launch: running instance + 2 restored windows |
| Crash (no quit) | No state written for crashed instance | Other instances' state still valid |
| Launch while instance running | New instance ignores existing state files | Running instance will overwrite its own on quit |

**Instance UUID:** Generated at launch via `juce::Uuid`. Used as the filename for `state/<uuid>.xml`. Not persisted across restarts — each launch gets a new UUID.

**No locking:** Instances never read each other's state files. Each writes only its own. The `state/` directory is append-only during runtime, consumed on next launch.

### 1.8 Grid Serialization API

New methods on `Grid`:

```cpp
/** @brief Serialize the normal screen buffer to a MemoryBlock.
 *  @return MemoryBlock containing header + cells + sparse graphemes + rowstates.
 *  @note MESSAGE THREAD.
 */
juce::MemoryBlock serialize() const;

/** @brief Restore the normal screen buffer from a MemoryBlock.
 *  @param data  The serialized data from serialize().
 *  @return true if restore succeeded, false on format/size mismatch.
 *  @note MESSAGE THREAD.
 */
bool deserialize (const juce::MemoryBlock& data);
```

Serialize: `memcpy` cells, scan graphemes for non-zero entries, `memcpy` rowstates.
Deserialize: validate header (magic, version, cols match), `memcpy` cells back, rebuild graphemes from sparse entries, `memcpy` rowstates, set `head` and `scrollbackUsed`.

**Column mismatch:** If saved cols != current terminal cols, discard scrollback (start fresh). Reflowing text across different column widths is out of scope.

### 1.9 Parser State Serialization

New methods on `Parser`:

```cpp
/** @brief Store pen and stamp state as child nodes of the SESSION ValueTree.
 *  @param session  The SESSION ValueTree to write PEN/STAMP nodes to.
 *  @note MESSAGE THREAD.
 */
void serializeState (juce::ValueTree& session) const;

/** @brief Restore pen and stamp state from SESSION ValueTree child nodes.
 *  @param session  The SESSION ValueTree containing PEN/STAMP nodes.
 *  @note MESSAGE THREAD.
 */
void deserializeState (const juce::ValueTree& session);
```

### 1.10 Session Restore API

New method on `Session`:

```cpp
/** @brief Restore session state from a ValueTree and optional scrollback data.
 *  @param sessionTree  SESSION ValueTree with params, modes, pen, stamp.
 *  @param scrollback   Optional MemoryBlock with serialized grid data.
 *  @note MESSAGE THREAD. Must be called before first resized().
 */
void restore (const juce::ValueTree& sessionTree, const juce::MemoryBlock& scrollback);
```

### 1.11 AppState Changes

```cpp
// New members
juce::Uuid instanceUuid;           // Generated at construction

// New methods
juce::File getStateDir() const;    // state/ directory
juce::File getSessionsDir() const; // sessions/ directory
void saveInstance();                // Write this instance's XML + bins
void loadAllInstances();            // Read all XMLs, return list of ValueTrees
void cleanupOrphans();             // Delete bins not referenced by any XML
```

### 1.12 Tab Name Persistence

Tab name stored as a property on the TAB node:

```cpp
// AppIdentifier.h
static const juce::Identifier name { "name" };

// On save: TAB node gets name property from Tabs::tabName value
// On load: tab created with saved name, tabName bound to displayName
```

### 1.13 Edge Cases

| Case | Behavior |
|------|----------|
| state.xml corrupt / unparseable | Skip instance, log to MessageOverlay |
| .bin file missing | Restore session without scrollback |
| .bin file corrupt (bad magic/version) | Restore session without scrollback |
| Column count changed (font size / window resize) | Discard scrollback, start fresh |
| Shell program changed in config | Spawn new shell program, scrollback still restored |
| Config file missing | Use defaults, restore state normally |
| Disk full on save | Write to temp first, fail silently if rename fails |
| Session UUID collision | Impossible (juce::Uuid is 128-bit random) |

### 1.14 What Is NOT Restored

| Item | Reason |
|------|--------|
| Alternate screen content | App that created it is dead |
| Running processes (vim, htop) | PTY dies on quit, processes die |
| Selection state | Transient UI state |
| Scroll position | Reset to bottom on restore |
| Prefix key mode | Transient input state |
| Glyph atlas / texture cache | Rebuilt on demand by renderer |
| Font handles | Recreated from config |

### 1.15 Performance Budget

| Operation | Target | Method |
|-----------|--------|--------|
| Save per terminal | <100ms | memcpy cells to MemoryBlock, write to file |
| Save total (10 terminals) | <1s | Sequential, no parallelism needed |
| Load per terminal | <50ms | Read file, memcpy to HeapBlock |
| Load total (10 terminals) | <500ms | Sequential |
| Startup overhead (no state) | 0ms | No state files = skip entirely |

### 1.16 Implementation Order

1. **Tab name persistence** — store `name` on TAB node, bind on restore
2. **Window position save/restore** — add x, y to WINDOW node
3. **Instance UUID + state directory** — `state/<uuid>.xml` write/read
4. **Force flush on quit** — explicit `State::flush()` before save
5. **Ring geometry on NORMAL/ALTERNATE** — `head`, `scrollbackUsed` properties
6. **Parser pen/stamp serialization** — PEN/STAMP child nodes
7. **Grid::serialize/deserialize** — binary format, memcpy cells
8. **Session::restore** — reconstruct from ValueTree + binary
9. **AppState::loadAllInstances** — scan state/, create windows
10. **Orphan cleanup** — delete unreferenced .bin files
11. **Multi-window support** — multiple MainComponent instances

Step 11 is the prerequisite blocker — END currently supports only one window. Multi-window requires `MainComponent` to not be a singleton, and the JUCE `DocumentWindow` to be instantiated multiple times. This is an architectural change that should be planned separately.

**Recommended phased approach:**
- **Phase A (single instance):** Steps 1-10, single window. Save/restore one instance only. Delete state/ on launch after restore.
- **Phase B (multi-instance):** Step 11. Multiple windows per process. Full multi-instance save/restore.

---
