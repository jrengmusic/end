# END - Architecture

**Purpose:** Single source of truth for project structure, patterns, and contracts.

**Status:** STABLE

**Last Updated:** 2026-04-11 (updated: Nexus refactor — Nexus class replaces Nexus::Session three-mode dispatcher; Interprocess namespace; EncoderDecoder replaces Wire; Terminal::Session two-factory overloads; daemonMode ValueTree ID; daemon config key; Windows Job Object; WindowsTTY getCwd PEB query; shouldTrackCwdFromOs; CHERE_INVOKING env var; layer separation rules)

---

## Project Overview

### Purpose

END (Ephemeral Nexus Display) is a GPU/CPU-rendered, fully-featured terminal emulator built with C++17 and JUCE. Tabs, split panes, Lua configuration, unified action registry with prefix-key modal input. Renders terminal output through an OpenGL pipeline or a SIMD-optimised CPU software renderer, with FreeType/HarfBuzz text shaping and a glyph atlas cache. Runtime GPU/CPU switching via config hot-reload.

### Architecture Philosophy

APVTS-inspired data flow. Reader thread writes atomics, timer flushes to ValueTree, UI pulls from ValueTree listeners. Render path bypasses the timer via VBlankAttachment polling a dirty atomic. No thread pushes to another — all communication is pull-based.

### Technology Stack

- **Language:** C++17
- **Framework:** JUCE 8
- **Text Rendering:** CoreText + HarfBuzz (macOS), FreeType + HarfBuzz (Linux/Windows)
- **Rendering:** OpenGL (GPU) or SIMD software renderer (CPU), runtime-switchable
- **Config:** Lua (sol2)
- **Build System:** JUCE / CMake
- **Platform:** macOS (primary), Linux, Windows

---

## Module Structure

### Module Map

```
Source/
  Main.cpp                          Application entry, owns Config + MainWindow
  MainComponent.h/cpp               Root component, owns Fonts context, Tabs, Action, MessageOverlay, GLRenderer
  AppState.h/cpp                    Application state: ValueTree root, pwdValue (live cwd binding), active pane tracking
  AppIdentifier.h                   App-level ValueTree identifiers (END, WINDOW, TABS, TAB, PANES, DOCUMENT) + pane type string constants
  SelectionType.h                   App-level SelectionType enum (none, visual, visualLine, visualBlock)
  ModalType.h                       App-level ModalType enum (none, selection, openFile)
  Cursor.h/cpp                      Shared cursor descriptor used by Whelmed::Screen

  config/
    Config.h/cpp                    Lua config loader, Context<Config> pattern (appearance only — keybindings migrated to action.lua)
    default_end.lua                 Template for ~/.config/end/end.lua (appearance, window, shell, terminal config)
    default_action.lua              Template for ~/.config/end/action.lua (keybindings, popups, custom actions)

  scripting/
    Scripting.h                     Scripting::Engine — Lua state, action.lua parser, file watcher, Context<Engine>
    Scripting.cpp                   Engine core: load, register, buildKeyMap, watcher callback
    ScriptingParse.cpp              Lua table parsing: keys, popups, actions, selection keys
    ScriptingPatch.cpp              action.lua file patching (remap), key lookup utilities

  component/
    PaneComponent.h                 Pure virtual base for pane-hosted components (Terminal, Whelmed)
    TerminalDisplay.h/cpp           UI host, VBlankAttachment render loop; delegates to Terminal::Input + Terminal::Mouse
    Terminal::Input.h/cpp           Modal gate, selection keys, open-file keys, scroll nav
    Terminal::Mouse.h/cpp           PTY forwarding, drag selection, click dispatch, wheel scroll
    Tabs.h/cpp                      Tab container, manages one Panes instance per tab
    Panes.h/cpp                     Per-tab pane container, owns Owner<PaneComponent> and PaneResizerBars
    LookAndFeel.h/cpp               Custom LookAndFeel: tab styling, popup menu, colour system
    CursorComponent.h               Cursor overlay, ValueTree-driven, uses Fonts::getContext()
    MessageOverlay.h                Transient overlay for status messages (reload, errors)
    StatusBarOverlay.h              Overlay that listens to TABS subtree for modal/selection state display

  whelmed/
    Component.h/cpp                 Whelmed::Component — PaneComponent subclass, markdown viewer pane
    Screen.h/cpp                    Block-based document renderer, owns Block instances, handles mouse selection
    Whelmed::Input.h/cpp            Modal gate, selection keys, scroll nav for Whelmed pane
    Block.h                         Pure virtual base for all renderable block types
    TextBlock.h/cpp                 Flowing styled text block (paragraphs, headings, lists)
    State.h/cpp                     Whelmed document state: ValueTree, atomic block count, parse complete

  fonts/
    DisplayMono-Book.ttf            Embedded base font (BinaryData)
    DisplayMono-Bold.ttf            Embedded bold variant (BinaryData)
    DisplayMono-Medium.ttf          Embedded medium variant (BinaryData)
    SymbolsNerdFont-Regular.ttf     Embedded NF icon font (BinaryData)

  terminal/
    data/                           Pure data types + state (no logic, no rendering)
      Cell.h                        16-byte trivially-copyable cell
      Color.h                       4-byte color (theme/palette/rgb)
      Pen.h (in Cell.h)             Current text attributes
      Grapheme.h (in Cell.h)        Multi-codepoint cluster
      CSI.h                         CSI parameter accumulator
      Charset.h                     Character set tables (G0/G1)
      Palette.h                     256-color palette (std::array)
      DispatchTable.h               VT state machine transition table
      Identifier.h                  ValueTree IDs + Identifier hash
      State.h/cpp                   APVTS-style atomic + timer + ValueTree
      StateFlush.cpp                Timer flush implementation
      ValueTreeUtilities.h          ValueTree traversal helpers
      Keyboard.h                    Keypress -> escape sequence mapping (progressive keyboard protocol, CSI u)

    logic/                          Terminal emulation (parser, grid, session)
      Session.h/cpp                 PTY orchestrator: owns TTY + History. Processor owns State, Grid, Parser.
      Parser.h/cpp                  VT state machine + dispatch (holds Grid::Writer, not Grid&)
      ParserVT.cpp                  Ground state: print, execute, LF (GroundOps fast-path struct)
      ParserCSI.cpp                 CSI dispatch (cursor, erase, mode)
      ParserESC.cpp                 ESC dispatch (charset, OSC 0/2/7/8/9/12/52/112/133/777, DCS)
      ParserSGR.cpp                 SGR (text attributes, color)
      ParserEdit.cpp                Erase, scroll, screen switch
      ParserOps.cpp                 Cursor movement, tab, reset
      Grid.h/cpp                    Ring buffer, dual screen, dirty tracking (nested Grid::Writer facade)
      GridScroll.cpp                Scroll region operations
      GridErase.cpp                 Erase operations
      GridReflow.cpp                Reflow on resize

    rendering/                      GPU/CPU pipeline (fonts, atlas, GL/SIMD)
      Screen.h/cpp                  Render coordinator, snapshot builder (reads Grid directly every frame)
      ScreenRender.cpp              buildSnapshot (reads Grid directly, no cell cache)
      ScreenSnapshot.cpp            updateSnapshot, publish to GLSnapshotBuffer
      ScreenSelection.h             Selection anchor/end, contains() hit test, inversion rendering
      selection/
        LinkManager.h/cpp           Viewport scan, OSC 8 span merging, hit-test, click dispatch
      Fonts.h                       Shared header (platform-agnostic API)
      Fonts.mm                      macOS: CoreText font loading, HarfBuzz shaping, CTFontDrawGlyphs rasterization
      Fonts.cpp                     Linux/Windows: FreeType font loading
      FontsMetrics.cpp              Cell metrics calculation
      FontsShaping.cpp              Text shaping (ASCII fast path, HarfBuzz, fallback) -- shared + FreeType path
      FontCollection.h              Codepoint-to-font-slot lookup API
      FontCollection.cpp            Flat int8_t[0x110000] dispatch table, up to 32 slots
      FontCollection.mm             macOS: platform font handle + hb_font_t construction
      GlyphConstraint.h             Per-codepoint NF icon scaling/alignment descriptor
      GlyphConstraintTable.cpp      Generated table: 10,470 codepoints, 88 switch arms
      BoxDrawing.h                  Procedural rasterizer: box drawing, block elements, braille
      AtlasPacker.h                 Shelf-based rectangle packer
      TerminalGLRenderer.cpp        OpenGL renderer (shaders, draw calls)
      TerminalGLDraw.cpp            Instance upload + draw
      GLShaderCompiler.h/cpp        Shader compilation
      GLVertexLayout.h/cpp          VAO/VBO layout
      shaders/                      GLSL vertex/fragment shaders

    tty/                            Platform TTY abstraction
      TTY.h/cpp                     Abstract base + reader thread
      UnixTTY.h/cpp                 macOS/Linux: forkpty
      WindowsTTY.h/cpp              Windows: ConPTY via NtCreateNamedPipeFile, overlapped I/O, sideloaded conpty.dll

    notifications/
      Notifications.h               Cross-platform desktop notification API
      Notifications.mm              macOS: UNUserNotificationCenter with foreground delegate
      Notifications.cpp             Windows: MessageBeep + stderr; Linux: notify-send

    action/                         Unified action registry + key dispatch
      Action.h/cpp                  Action table, key map, prefix state machine, Context<Registry>
      ActionList.h/cpp              Command palette component (jam::Window, fuzzy-searchable action list)
      ActionRow.h/cpp               Row component for ActionList (displays action name + keybinding)
      KeyHandler.h/cpp              Key event routing for ActionList modal input
      LookAndFeel.h/cpp             LookAndFeel overrides for ActionList styling
      KeyRemapDialog.h              Deprecated stub (inline remap now handled in ActionList)

    nexus/                          Session manager — owns all Terminal::Session instances
      Nexus.h/cpp                   jam::Context<Nexus> session container; create/remove/get/has/list; attach(Daemon&)/attach(Link&) for mode wiring

  interprocess/                     IPC transport layer (daemon/client process split)
      Daemon.h/cpp                  JUCE InterprocessConnectionServer; owns Channel objects; broadcast + per-session subscriber registries; wireSessionCallbacks
      DaemonWindows.cpp             Windows-specific platform helpers: Job Object (cascade-kill), spawnDaemon()
      Daemon.mm                     macOS/Linux platform helpers: hideDockIcon(), spawnDaemon()
      Link.h/cpp                    Client-side JUCE IPC connector; connect-retry timer; sends PDUs to daemon; dispatches incoming PDUs to Nexus
      Channel.h/cpp                 Server-side JUCE IPC connection (one per connected client); dispatches incoming PDUs to Nexus + Daemon
      EncoderDecoder.h/cpp          Binary wire-format encode/decode helpers (writeUint16/32/64, writeString, readUint16/32/64, readString, encodePdu)
      Message.h                     Protocol message-kind enumeration (uint16_t wire values: hello, createSession, output, loading, stateUpdate, sessionKilled, sessions, etc.)

~/Documents/Poems/dev/jam/
  jam_core/                         Shared utilities (Owner, identifiers, Context, BinaryData)
  jam_graphics/                     Graphics utilities
  jam_gui/                          Window, layout utilities, and OpenGL rendering (PaneManager, PaneResizerBar, GLRenderer)
    opengl/
      context/
        jam_gl_mailbox.h            Lock-free atomic pointer exchange (generic template)
        jam_gl_snapshot_buffer.h    Double-buffered snapshot with mailbox (generic template)
        jam_gl_graphics.h/cpp       juce::Graphics-like command buffer API for OpenGL
        jam_gl_component.h/cpp      Base class for GL-rendered components
        jam_gl_renderer.h/cpp       OpenGL renderer (shader management, draw dispatch)
        jam_gl_overlay.h            Integration overlay for JUCE components
      renderers/
        jam_gl_path.h/cpp           juce::Path tessellation to GL vertices
        jam_gl_vignette.h           Vignette edge effect component
      shaders/
        flat_colour.vert/frag       Vertex colour shader with optional alpha mask
    layout/
      jam_pane_manager.h/cpp        Binary tree ValueTree layout engine for split panes
      jam_pane_resizer_bar.h/cpp    Draggable divider bar between panes
```

### Module Inventory

| Module | Location | Responsibility | Dependencies |
|--------|----------|----------------|--------------|
| AppState | `Source/` | App ValueTree root, pwd tracking via Value::referTo, active pane type + UUID | JUCE ValueTree, Terminal::ID |
| AppIdentifier | `Source/` | ValueTree node and property identifiers; pane type string constants (paneTypeTerminal, paneTypeDocument) | JUCE |
| Config | `config/` | Lua config load/save, Context-managed, platform config paths. Appearance and shell config only — keybindings and popups migrated to `action.lua` via `Scripting::Engine`. Colour values parsed as RRGGBBAA hex strings. Colour cache invalidated on reload. | sol2, jam::Context |
| Scripting | `scripting/` | Lua scripting engine. Owns persistent `jam::lua::state` for `action.lua` — SSOT for all keybindings, popup definitions, and custom user actions. File watcher auto-reloads on save (gated by `auto_reload`). Provides parsed bindings to `Action::Registry` and selection keys to `Terminal::Input` / `Whelmed::InputHandler`. | sol2, jam::Context, jam::File::Watcher |
| Component | `component/` | JUCE UI hosting, tabs, panes, LookAndFeel, VBlank render trigger | Session, Screen, Config, PaneManager, AppState |
| Fonts | `fonts/` | Embedded TTF binaries (BinaryData) | — |
| Data | `terminal/data/` | Pure value types, state atomics, IDs | JUCE ValueTree |
| Logic | `terminal/logic/` | VT parsing, grid storage, session orchestration | Data |
| Rendering | `terminal/rendering/` | Font shaping, glyph atlas, GL/CPU draw, Fonts (Context-managed) | Data, FreeType, HarfBuzz, OpenGL, jam_graphics |
| Notifications | `terminal/notifications/` | Native desktop notification dispatch (OSC 9/777) | JUCE, UserNotifications (macOS) |
| TTY | `terminal/tty/` | Platform PTY abstraction, reader thread | JUCE Thread |
| jam_core | `~/Documents/Poems/dev/jam/jam_core/` | Shared utilities, identifiers, Context, BinaryData | JUCE core |
| jam_graphics | `~/Documents/Poems/dev/jam/jam_graphics/` | CPU text renderer, SIMD compositing (SSE2/NEON), glyph atlas, typeface | jam_core, FreeType, HarfBuzz |
| jam_gui/opengl | `~/Documents/Poems/dev/jam/jam_gui/opengl/` | GL mailbox, snapshot buffer, path tessellation, Graphics-like API | juce_opengl, jam_core |
| Action | `action/` | Unified action registry (`Action::Registry`), key dispatch, prefix state machine, command palette (`Action::List`) | Config, jam::Context |
| Nexus | `nexus/` | Session container. Owns `unordered_map<String, unique_ptr<Terminal::Session>>`. Mode determined by `attach(Daemon&)` / `attach(Link&)` / no attachment. `jam::Context<Nexus>` singleton owned by ENDApplication. | Terminal::Session, jam::Context |
| Interprocess | `interprocess/` | IPC transport layer. Daemon (TCP server), Link (client), Channel (per-client server-side connection), EncoderDecoder (wire format), Message (PDU kind enum). Daemon owns broadcast + subscriber registries, wires session callbacks. | JUCE IPC, Nexus, Terminal::Session, AppState |
| Panes | `component/` | Per-tab pane container, owns Owner<PaneComponent> and resizer bars | PaneManager, PaneComponent |
| Whelmed | `whelmed/` | Markdown viewer: Component, Screen, block hierarchy, Whelmed::Input | PaneComponent, jam_markdown |
| jam_gui | `~/Documents/Poems/dev/jam/jam_gui/` | Window, layout utilities, and OpenGL rendering: PaneManager binary tree, PaneResizerBar, GLRenderer, GLComponent | juce_opengl, jam_core, jam_graphics |

---

## Nexus and Interprocess

### Nexus — Session Manager

`Nexus` is a pure session container.  It inherits `jam::Context<Nexus>` and is owned as a value member of `ENDApplication`.  It holds an `unordered_map<String, unique_ptr<Terminal::Session>>` and exposes a lifecycle API: `create`, `remove`, `get`, `has`, `list`.

Data flow mode (standalone, daemon, client) is determined at runtime by which `attach` overload is called — not by a constructor tag:

- **No attachment** — standalone.  Sessions fire `onExit` locally; when the last session exits `onAllSessionsExited` is called.
- **`attach(Interprocess::Daemon&)`** — daemon mode.  After `Nexus::create(cwd, uuid, cols, rows)` succeeds, Nexus calls `Daemon::wireSessionCallbacks(uuid, session)` to wire IPC broadcast.
- **`attach(Interprocess::Link&)`** — client mode.  `Nexus::create(cwd, uuid, cols, rows)` creates a no-TTY session and sends a `createSession` PDU to the daemon via Link.

`Nexus::create(cwd, uuid, cols, rows)` is the mode-routing entry point used by `Panes` and `Tabs`.  It returns an existing session immediately if the UUID already exists (idempotency guard for GUI reconnect).

### Process Configurations

```
Standalone:              ENDApplication + Nexus (no IPC)
Daemon process:          ENDApplication + Nexus + Interprocess::Daemon (headless, owns shells)
GUI connected to daemon: ENDApplication + Nexus + Interprocess::Link  (renders daemon's sessions)
```

The daemon process suppresses its Dock icon via `Daemon::hideDockIcon()` and writes its bound TCP port to `~/.config/end/nexus/<uuid>.nexus`.  The GUI reads that file and begins connect attempts via `Link::beginConnectAttempts()`.

### Interprocess — IPC Transport Layer

The `Interprocess` namespace contains the TCP-based IPC transport between a daemon process and one or more GUI clients.  It does not include any terminal emulation logic.

**Classes:**

| Class | Role |
|-------|------|
| `Interprocess::Daemon` | TCP server (`juce::InterprocessConnectionServer`).  Owns `Channel` objects via `jam::Owner`.  Holds the broadcast list (`attached`) and per-session subscriber registry (`subscribers`).  Installs a Windows Job Object for cascade-kill of OpenConsole.exe grandchildren. |
| `Interprocess::Channel` | Server-side connection representing one connected GUI client.  Created by `Daemon::createConnectionObject()`.  Dispatches incoming PDUs to `Nexus` and `Daemon`. |
| `Interprocess::Link` | Client-side connector (`juce::InterprocessConnection`).  Polls the nexus file for the daemon port and retries every 100 ms via an inner `ConnectTimer`.  Dispatches incoming PDUs directly on the message thread. |
| `Interprocess::EncoderDecoder` | Binary wire-format helpers: `writeUint16/32/64`, `writeString`, `readUint16/32/64`, `readString`, `encodePdu`.  Single source of truth for wire encoding — used by both `Channel::sendPdu` and `Link::sendPdu`. |
| `Interprocess::Message` | `enum class Message : uint16_t` — PDU kind identifiers with stable wire values. |

**Wire format:** Every JUCE IPC frame payload begins with a `uint16_t` kind (LE), followed by kind-specific payload bytes.

**PDU kinds:**

| Kind | Direction | Payload |
|------|-----------|---------|
| `hello` / `helloResponse` | client↔host | version |
| `createSession` | client→host | cwd, uuid, cols (uint16), rows (uint16) |
| `loading` | host→client | uuid + raw PTY history bytes |
| `output` | host→client | uuid + raw PTY bytes |
| `input` | client→host | uuid + raw bytes to PTY stdin |
| `resizeSession` | client→host | uuid, cols (uint16), rows (uint16) |
| `detachSession` | client→host | uuid (stop forwarding, session keeps running) |
| `killSession` | client→host | uuid (destroy shell) |
| `sessionKilled` | host→client | uuid (shell exited) |
| `sessions` | host→client | count + N × length-prefixed UUID strings |
| `stateUpdate` | host→client | uuid, cwd, fgProcess |

### Daemon Session Callback Wiring

`Daemon::wireSessionCallbacks(uuid, session)` is called by `Nexus::create` in daemon mode after each `Terminal::Session` is constructed.  It installs three callbacks via three helper methods:

- `wireOnBytes` — wires `session.onBytes` to broadcast `Message::output` to per-session subscribers.  Runs on the reader thread; acquires `connectionsLock`.
- `wireOnStateFlush` — wires `session.onStateFlush` to broadcast `Message::stateUpdate` (cwd + foreground process).  Fires on the message thread; suppresses redundant broadcasts via shared-ptr captured previous values.
- `wireOnExit` — wires `session.onExit` to broadcast `Message::sessionKilled`, schedule async `Nexus::remove`, re-broadcast sessions list, and fire `onAllSessionsExited` if empty.

### Snapshot Restore on Client Attach

When a GUI client sends `createSession` for an existing UUID, `Daemon::attachSession` sends the current byte history as `Message::loading`, then registers the client as a subscriber.  The lock is held across both operations to prevent the reader thread's `onBytes` broadcast from interleaving between history send and subscriber registration.

```
Daemon:  Terminal::Session::snapshotHistory() → Message::loading → Link
Link:    handleLoading → Terminal::Session::process → Processor → Grid → Display
```

### Byte-Forward Flow (Live)

```
Daemon:  PTY → Session::onBytes → Message::output → Channel → Link
Link:    handleOutput → Terminal::Session::process → Processor → Grid → Display

Standalone:
         PTY → Session::onBytes → Processor::processWithLock → Grid → Display
```

### Terminal::Session

`Terminal::Session` is the singular owner of one terminal instance.  It holds:
- `unique_ptr<TTY>` — the platform PTY (null in client mode).
- `History` — ring buffer of raw PTY bytes.
- `unique_ptr<Terminal::Processor>` — Parser + Grid + State pipeline.
- `bool shouldTrackCwdFromOs` — when true, `onFlush` queries the OS for cwd via the PTY's PEB (Windows) or `/proc` (Linux).  Set to `false` when shell integration is active (OSC 7 provides cwd directly).

**Factory — two overloads:**

```cpp
// PTY-backed session (standalone / daemon mode)
static unique_ptr<Session> create(cwd, cols, rows, shell, args, seedEnv, uuid);

// No-TTY session (GUI connected to daemon)
static unique_ptr<Session> create(cols, rows, cwd, shell, uuid);
```

**Public API:**

| Method | Purpose |
|--------|---------|
| `process(data, len)` | Feed raw bytes from daemon IPC into the Processor |
| `sendInput(data, len)` | Write raw bytes to PTY stdin |
| `resize(cols, rows)` | Signal PTY resize (SIGWINCH) |
| `getStateInformation(block)` | Serialize Processor state for daemon → GUI sync |
| `setStateInformation(data, size)` | Restore Processor state from daemon snapshot |
| `getProcessor()` | Returns the owned `Terminal::Processor` |
| `snapshotHistory()` | Returns a `MemoryBlock` of buffered PTY output (for `Message::loading`) |

**Callbacks (set by Nexus or Interprocess layer):**

| Callback | Thread | Purpose |
|----------|--------|---------|
| `onBytes` | Reader | PTY output chunk — broadcast in daemon mode, process locally in standalone |
| `onExit` | Message | Shell process exited |
| `onStateFlush` | Message | cwd + foreground process updated in State — daemon mode broadcasts `stateUpdate` |

### Windows Job Object (Cascade-Kill)

`Daemon::installPlatformProcessCleanup()` creates a Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` and assigns the daemon process to it.  When the daemon process exits (normally or abnormally), the OS closes the Job Object handle and kills all child processes — including `OpenConsole.exe` grandchildren spawned by ConPTY.  The handle is stored in `Daemon::jobObject` and released in `releasePlatformProcessCleanup()`.

---

## Layer Separation Rules

```
 Application (ENDApplication, MainComponent, Tabs, Panes)
    — wires Nexus + Interprocess; owns all top-level lifetimes
    |
    v
 Interprocess (Daemon, Link, Channel, EncoderDecoder, Message)
    — IPC transport; includes Nexus forward declaration; does NOT include Terminal headers directly
    |
    v
 Nexus (session container)
    — includes Terminal::Session; forward-declares Interprocess::Daemon and Interprocess::Link
    |
    v
 Terminal / Logic (Parser→Writer→Grid)   writes atomics on reader thread
    |
    v
 Terminal / Data (State/Cell)            pure types, atomic storage, timer flush
    |
    v
 Terminal / Rendering (Screen/GL)        reads from Grid + State, builds GPU snapshots
    |
    v
 Terminal / TTY (platform)               reader thread feeds raw bytes to Parser
```

**Header inclusion rules:**
- `terminal/` headers MUST NOT include `Nexus.h` or any `interprocess/` header.
- `nexus/Nexus.h` forward-declares `Interprocess::Daemon` and `Interprocess::Link`; includes `terminal/logic/Session.h`.
- `interprocess/` headers forward-declare `Nexus`; include `terminal/logic/Session.h` only from `.cpp` files as needed.
- `Application` layer (`Main.cpp`, `MainComponent`, `Tabs`, `Panes`) includes all layers.

### Communication Contracts

**TTY -> Logic:**
- TTY reader thread calls `Parser::process(data, length)` directly
- Parser writes to Grid and State atomics on reader thread
- No allocation, no locks on this path

**Logic -> Data:**
- Parser calls `Grid::Writer` methods — cell writes, scroll, erase, dirty tracking
- Parser reads geometry (cols, visibleRows, scrollbackUsed) from `State` parameterMap atomics
- All calls are `noexcept`, reader thread safe

**Data -> Component (timer path):**
- `State::timerCallback()` runs on message thread (60-120Hz)
- Flushes atomics to ValueTree via `flush()`
- ValueTree fires `valueTreePropertyChanged` -> CursorComponent updates
- `State::refresh()` (public) also flushes atomics — called by `onVBlank` before rendering

**Data -> Component (render path):**
- `VBlankAttachment` fires on every display vsync (CVDisplayLink)
- Calls `State::consumeSnapshotDirty()` — one atomic exchange
- Calls `State::refresh()` to flush pending atomics before render
- If dirty: `Screen::render()` reads Grid + State, builds snapshot, publishes to GLSnapshotBuffer

**Component -> Rendering (GL path):**
- `GLSnapshotBuffer::write()` — message thread publishes snapshot
- `GLSnapshotBuffer::read()` — GL thread acquires latest snapshot (retains previous if none new)
- Lock-free: double-buffered with atomic pointer exchange via `GLMailbox`

**Panes/Tabs -> Nexus (session lifecycle):**
- `Panes::createTerminal(cwd)` calls `Nexus::getContext()->create(cwd, uuid, cols, rows)` — mode-routing entry point
- `Tabs::closeSession(uuid)` calls `Nexus::getContext()->remove(uuid)`
- In client mode, `Nexus::create` additionally calls `Link::sendCreateSession(cwd, uuid, cols, rows)`
- In daemon mode, `Nexus::create` additionally calls `Daemon::wireSessionCallbacks(uuid, session)` after the PTY session is constructed

**Interprocess::Link -> Nexus (incoming PDU dispatch):**
- `Link::handleOutput(uuid, bytes)` → `Nexus::get(uuid).process(bytes, len)`
- `Link::handleLoading(uuid, bytes)` → `Nexus::get(uuid).process(bytes, len)` (initial snapshot)
- `Link::handleStateUpdate(uuid, cwd, fgProcess)` → `Nexus::get(uuid).getProcessor().getState()` ValueTree write
- `Link::handleSessionKilled(uuid)` → destroys local no-TTY session via `Nexus::remove(uuid)`
- `Link::handleSessions(uuids)` → creates no-TTY sessions for any UUIDs not yet present

**Interprocess::Channel -> Daemon/Nexus (incoming PDU dispatch, daemon side):**
- `createSession` PDU → `Nexus::getContext()->create(cwd, uuid, cols, rows)` + `Daemon::attachSession(uuid, channel, sendHistory, cols, rows)`
- `input` PDU → `Nexus::get(uuid).sendInput(data, len)`
- `resizeSession` PDU → `Nexus::get(uuid).resize(cols, rows)`
- `killSession` PDU → `Nexus::get(uuid).stop()` + `Nexus::remove(uuid)`
- `detachSession` PDU → `Daemon::detachSession(uuid, channel)`

### Layer Violations (FORBIDDEN)

- Rendering must NEVER call Parser or Grid mutators
- TTY must NEVER call UI/Component code
- Parser must NEVER allocate on reader thread
- GL thread must NEVER write to Grid or State
- `terminal/` headers must NEVER include `Nexus.h` or any `interprocess/` header

---

## Threading Model

### Threads

| Thread | QoS | Owns | Reads | Writes |
|--------|-----|------|-------|--------|
| **Reader** (TTY) | high | TTY fd | raw bytes | State atomics, Grid cells, dirty bits, scrollbackUsed |
| **Timer** (JUCE) | default | — | `needsFlush` atomic | ValueTree properties |
| **Message** (main) | user-interactive | Component, Screen | ValueTree, `snapshotDirty` atomic, Grid cells | Snapshot (reads Grid cells directly) |
| **GL** (OpenGL) | user-interactive | OpenGL context | Snapshot (via GLSnapshotBuffer), staged bitmaps | GPU textures, framebuffer |

### Data Flow: Keystroke to Pixel

```
Keystroke -> Message Thread -> TTY::write()
         -> Reader Thread reads response -> Parser::process()
         -> Grid cells written, State atomics set, snapshotDirty = true
         -> VBlank fires on Message Thread -> consumeSnapshotDirty()
         -> State::refresh() -> Screen::render() reads Grid + State -> builds Snapshot -> GLSnapshotBuffer::write()
          -> GL Thread -> GLSnapshotBuffer::read() -> uploadStagedBitmaps() -> draw
```

### Synchronization Primitives

| Mechanism | Between | Purpose |
|-----------|---------|---------|
| `std::atomic<float>` | Reader -> Timer/Message | State parameter transport |
| `std::atomic<bool> snapshotDirty` | Reader -> Message (VBlank) | Render trigger |
| `std::atomic<bool> needsFlush` | Reader -> Timer | ValueTree flush trigger |
| `jam::GLSnapshotBuffer` (atomic exchange) | Message -> GL | Double-buffered snapshot handoff |
| `std::mutex` (uploadMutex) | Message -> GL | Staged bitmap queue |
| `juce::CriticalSection` (resizeLock) | Message <-> Reader | Grid resize + data processing safety |

---

## Design Patterns in Use

### Pattern: APVTS-Style State

**Used for:** Cross-thread state synchronization without locks on the hot path.

**Implementation:** `State.h/cpp`, `StateFlush.cpp`

Reader thread writes to `std::atomic<float>` via `storeAndFlush()`. Timer polls `needsFlush` and copies atomics to ValueTree. UI reads from ValueTree listeners. Render path calls `State::refresh()` to force-flush atomics before each frame, ensuring the snapshot reads current values without waiting for the timer.

WINDOW-subtree properties `App::ID::fontFamily` and `App::ID::fontSize` drive font changes. A `ValueTree::Listener` on the WINDOW subtree detects changes, applies `fontFamily`/`fontSize` to `Typeface`, then calls `AppState::markAtlasDirty()`. `AppState::atlasDirty` is a `std::atomic<bool>`. Two consumers:
- **GL thread** — GPU lambda calls `consumeAtlasDirty()` and invokes `GlyphAtlas::getContext()->rebuildAtlas()` to tear down and re-upload GPU textures.
- **Message thread** — `renderPaint` CPU path calls `consumeAtlasDirty()` before rasterizing to avoid stale CPU atlas.

`markAtlasDirty()` / `consumeAtlasDirty()` are the only public API. No caller reads `atlasDirty` directly.

### Pattern: Double-Buffered Snapshot (GLSnapshotBuffer)

**Used for:** Lock-free handoff of render data from message thread to GL thread.

**Implementation:** `~/Documents/Poems/dev/jam/jam_gui/opengl/context/` — `jam::GLSnapshotBuffer<T>`, `jam::GLMailbox<T>`

`GLSnapshotBuffer` owns two snapshot instances and a `GLMailbox`. Message thread calls `getWriteBuffer()`, fills the snapshot, calls `write()`. GL thread calls `read()` — returns latest snapshot, or retains the previous one if nothing new. Double-buffer rotation is encapsulated. Zero allocation, zero locking. Forked from KANJUT `kuassa_opengl`.

### Pattern: Context<T> (Responsible Global)

**Used for:** Global access without Meyer's singleton.

**Implementation:** `Config.h` inherits `jam::Context<Config>`, `Fonts.h` inherits `jam::Context<Fonts>`

Config lifetime owned by `ENDApplication` (Main.cpp). Fonts lifetime owned by `MainComponent`. Access via `Config::getContext()->` / `Fonts::getContext()->`. Fail-fast jassert if accessed before construction.

`GlyphAtlas` (`~/Documents/Poems/dev/jam/jam_gui/opengl/context/jam_glyph_atlas.h`) also inherits `jam::Context<GlyphAtlas>`. Pure GL texture handle holder: `monoAtlas`, `emojiAtlas`, `atlasSize`. Owned by `MainComponent` (value member). Shared by all `GLAtlasRenderer` instances (main + popup). `rebuildAtlas()` runs on the GL thread — calls `glDeleteTextures` and zeroes handles, triggering re-upload. CPU atlas images remain on `Typeface`; `GlyphAtlas` owns only the GPU handles.

### Pattern: Shelf-Based Atlas Packing

**Used for:** Packing variable-size glyphs into a fixed texture atlas (4096x4096 GPU, 2048x2048 CPU).

**Implementation:** `AtlasPacker.h`

Horizontal shelves, best-fit allocation. Separate packers for mono and emoji. LRU eviction when cache is full.

### Pattern: File Decomposition by Concern

**Used for:** Keeping files under 300 lines while maintaining logical cohesion.

Parser.cpp -> ParserCSI, ParserESC, ParserSGR, ParserVT, ParserEdit, ParserOps
Grid.cpp -> GridScroll, GridErase, GridReflow
State.cpp -> StateFlush
Screen.cpp -> ScreenRender, ScreenSnapshot
Fonts.cpp -> FontsMetrics, FontsShaping
TerminalGLRenderer.cpp -> TerminalGLDraw

All split files define member functions of the parent class. No separate classes needed.

---

## Split Pane System

### Architecture

Each tab owns a `Panes` component that manages split pane layout via a `PaneManager`. The layout is a binary tree stored as a JUCE `ValueTree`:

```
TAB
  PANES (direction="vertical"|"horizontal", ratio=0.5, x, y, width, height)
    PANE (uuid="abc")
      SESSION (uuid="abc", displayName, ...)          -- terminal pane
    PANE (uuid="def")
      SESSION (uuid="def", displayName, ...)          -- terminal pane with Whelmed open
      DOCUMENT (filePath, displayName, scrollOffset)  -- grafted when Whelmed opens; removed on close
```

- **Leaves** (`PANE` type) — each maps to one `PaneComponent` (Terminal::Component or Whelmed::Component)
- **Internal nodes** (`PANES` type) — each represents a split with `direction` and `ratio`
- **SESSION** — terminal state, grafted as child of PANE at creation time
- **DOCUMENT** — Whelmed document state, grafted alongside SESSION when a .md file is opened; removed when Whelmed is closed

### PaneManager (Binary Tree Layout Engine)

`jam::PaneManager` owns the `PANES` ValueTree and provides:

| Method | Purpose |
|--------|---------|
| `addLeaf(uuid)` | Add a new pane leaf to the tree |
| `split(uuid, newUuid, direction)` | Split a leaf into two panes with given direction |
| `remove(uuid)` | Remove a pane, collapsing its parent split node |
| `layOut(state, bounds, components, resizerBars)` | Recursive layout: subdivide bounds, position components and resizer bars |
| `getItemCurrentPosition(splitNode)` | Get current pixel position of a split divider |
| `setItemPosition(splitNode, newPosition)` | Set pixel position of a split divider (updates ratio) |
| `findLeaf(node, uuid)` | Static: find a PANE leaf by UUID |

`layOut` is a static template method. It recursively walks the tree, subdivides the available bounds by direction and ratio, calls `setBounds` on components matched by `getComponentID()`, and positions `PaneResizerBar` instances matched by `getSplitNode()` identity.

Ratio is clamped to `[0.1, 0.9]`. Bounds are stored as `ID::x/y/width/height` properties on split nodes. Resizer bar width is 4px.

### PaneResizerBar (Draggable Divider)

`jam::PaneResizerBar` is a `juce::Component` subclass that acts as a draggable divider between panes. Each split node in the tree has a paired resizer bar.

On drag: queries `PaneManager::getItemCurrentPosition()`, computes desired position from mouse delta, calls `setItemPosition()`. `hasBeenMoved()` triggers `parent->resized()` to re-layout.

Rendering delegated to `LookAndFeel::drawStretchableLayoutResizerBar()`. Configurable via `pane.bar_colour` and `pane.bar_highlight`.

### Panes (Per-Tab Container)

`Terminal::Panes` is the per-tab component that bridges `PaneManager` with `Terminal::Component` instances:

- Owns `Owner<PaneComponent> panes` and `Owner<PaneResizerBar> resizerBars`
- `createTerminal()` — adds a leaf to the tree, creates a Terminal::Component, grafts SESSION
- `createWhelmed(file)` — overlays a Whelmed::Component on the active terminal pane, grafts DOCUMENT
- `closeWhelmed()` — removes Whelmed::Component and DOCUMENT, restores terminal visibility
- `closePane(uuid)` — ungrafts SESSION, removes pane, removes resizer bar, calls `paneManager.remove()`
- `splitHorizontal()` — left/right layout (calls `splitImpl("vertical", true)`)
- `splitVertical()` — top/bottom layout (calls `splitImpl("horizontal", false)`)
- `focusPane(deltaX, deltaY)` — spatial navigation by component bounds

**Split naming convention:** `splitHorizontal` produces a left/right layout. The internal direction string `"vertical"` describes the divider orientation, not the layout direction.

### Action Registry (Action::Registry)

`Action::Registry` inherits `jam::Context<Registry>` and `juce::Timer`. It is the single owner of all user-performable actions, replacing the former `KeyBinding`, `ModalKeyBinding`, and `ApplicationCommandTarget` system.

**Architecture:**
- Dynamic action table: built-in actions registered by `MainComponent`, popup + custom Lua actions registered by `Scripting::Engine`
- Hot-reloadable key map: `Scripting::Engine` parses `action.lua`, passes bindings to `Registry::buildKeyMap()`. File watcher auto-reloads on save.
- Prefix state machine: tmux-style two-keystroke input (prefix key + action key with timeout)
- Global singleton via `jam::Context<Action>`

**Key resolution order in `handleKeyPress()`:**
1. If in **waiting** state: match by text character in modal bindings → execute → idle
2. If in **idle** state and key equals prefix key: enter **waiting** → start timer
3. If in **idle** state: match in global bindings → execute
4. No match → return false (caller forwards to PTY)

**Modal character matching:** Shifted characters (e.g. `?` = Shift+/) are matched by text character, not keycode+modifiers. Falls back to exact KeyPress match for non-character keys.

Built-in actions are registered in `MainComponentActions.cpp`. Popup and custom Lua actions are registered by `Scripting::Engine::registerActions()`. All keybindings (built-in + popup + custom) are defined in `~/.config/end/action.lua` and parsed by `Scripting::Engine`, which passes them to `Registry::buildKeyMap()`.

**Copy action special behavior:** If box selection is active, copies to clipboard and returns true (consumed). If no selection, returns false — key falls through to PTY as `\x03` (SIGINT).

Prefix key and timeout configurable via `keys.prefix` and `keys.prefix_timeout` in `action.lua`.

### Close Cascade

Two entry points feed into the same cascade: explicit close action and shell exit.

**Explicit close:** `Action::close_pane` callback calls `Panes::closePane(uuid)`.

**Shell exit:** `TTY` reader thread detects EOF/process exit → posts `onProcessExited` lambda to message thread → `TerminalDisplay::onProcessExited()` → calls `Panes::closePane(uuid)`.

Cascade from `closePane()`:

1. `Panes::closePane()` ungrafts SESSION, destroys terminal, removes resizer bar
2. `PaneManager::remove()` collapses the parent split node, promotes the sibling
3. If last pane in tab: `Tabs::closeTab()` removes the tab
4. If last tab: application quits

---

## Working Directory Tracking

### AppState::pwdValue (Live CWD Binding)

`AppState` holds a `juce::Value pwdValue` member that tracks the active terminal's current working directory via `Value::referTo`.

**`setPwd(sessionTree)`** — Binds `pwdValue` to the `Terminal::ID::cwd` property of the given SESSION ValueTree:
```cpp
void setPwd (juce::ValueTree sessionTree);
// Calls: pwdValue.referTo (sessionTree.getPropertyAsValue (Terminal::ID::cwd, nullptr))
```

**`getPwd()`** — Returns `pwdValue.toString()`, falls back to `$HOME` if empty.

**Binding lifecycle:**
- `Tabs::globalFocusChanged()` calls `setPwd(term->getValueTree())` when focus moves to a terminal
- `Tabs::addNewTab()` calls `setPwd()` on the new terminal
- New splits inherit cwd: `Panes::splitImpl()` passes `AppState::getContext()->getPwd()` to `createTerminal()`
- New tabs inherit cwd: `Tabs::addNewTab()` passes `getPwd()` to `createTerminal()`

**Critical pattern:** `Value::referTo` must bind to a **stored** `juce::Value` member, not a temporary. `getPropertyAsValue()` returns a temporary — calling `referTo` on a temporary does nothing.

### Panes::createTerminal with Working Directory

```cpp
juce::String createTerminal (const juce::String& workingDirectory = {});
```

Passes `workingDirectory` through to `Terminal::Component::create()`, which constructs the terminal with the given cwd. Default empty string = inherit from environment.

---

## Tab Name Management

### Value::Listener Pattern

`Tabs` uses `juce::Value::Listener` (not `ValueTree::Listener`) to track the active terminal's display name:

- **Member:** `juce::Value tabName` — bound via `referTo` to active terminal's `App::ID::displayName`
- **Binding:** `globalFocusChanged()` rebinds `tabName` when focus changes
- **Update:** `valueChanged()` calls `setTabName()` on the tab bar

### displayName Computation (Session::onFlush + State::flushStrings)

`Session::onFlush` runs on the message thread (via `State::timerCallback`).  It calls
`tty->getForegroundPid()` and `tty->getShellPid()`.  When the two PIDs are equal (shell
at prompt), it writes an empty string to the `foregroundProcess` slot.  When they differ
(a TUI or child process is running), it writes the foreground process name.

`State::flushStrings()` then computes `displayName` stored as `App::ID::displayName` with priority:

1. **foregroundProcess** — when non-empty (set by `Session::onFlush` when fgPid != shellPid)
2. **cwd leaf** — `juce::File(cwdPath).getFileName()` (e.g., "end" from "/Users/me/dev/end")

`title` (OSC 0/2) is NOT used in displayName computation — it's shell prompt noise that overrides everything else.

### SESSION Node Identification

SESSION nodes use `jam::ID::id` (not a Terminal-specific UUID identifier). This makes SESSION nodes compatible with `jam::ValueTree::getChildWithID()` — a recursive lookup utility in the jam_data_structures module.

---

## Input Encoding

### Progressive Keyboard Protocol (CSI u)

Full implementation of the progressive keyboard enhancement system (CSI u protocol).

#### Protocol Handshake (Parser → State)

| Sequence | Action | Implementation |
|----------|--------|----------------|
| `CSI > flags u` | Push flags onto per-screen stack | `State::pushKeyboardMode()` |
| `CSI < count u` | Pop count entries from stack | `State::popKeyboardMode()` |
| `CSI ? u` | Query current flags | `Parser::handleKeyboardMode()` responds `CSI ? flags u` |
| `CSI = flags ; mode u` | Set flags (1=abs, 2=OR, 3=AND-NOT) | `State::setKeyboardMode()` |

Per-screen stacks (normal/alternate) with max depth 16. Stacks cleared on RIS (`resetModes()`).

#### State Storage

`keyboardFlags` is a per-screen parameter in the APVTS pattern:
- **Reader thread:** `State::getKeyboardFlags(screen)` reads the atomic.
- **Message thread:** `State::getKeyboardFlags()` reads the ValueTree (post-flush).
- Stack storage: `HeapBlock<uint32_t>` (flat) + `HeapBlock<int>` (stack sizes per screen).

#### Flag-Aware Keyboard Encoding (Keyboard::map)

`Session::handleKeyPress()` reads `getKeyboardFlags()` and passes to `Keyboard::map()`.

| Flag | Bit | Effect on encoding |
|------|-----|--------------------|
| 0 (legacy) | — | All keys use xterm legacy encoding. Shift+Enter = `\r`. |
| 1 (disambiguate) | 0 | Ctrl+key, Alt+key, Escape → CSI u. Enter/Tab/Backspace stay legacy when unmodified; modified (e.g. Shift+Enter) → CSI u. |
| 8 (all keys) | 3 | ALL keys including Enter/Tab/Backspace and plain text → CSI u. |
| 2 (event types) | 1 | Accepted/stored but not encoded (JUCE lacks key release events). |
| 4 (alternate keys) | 2 | Accepted/stored but not encoded (no base-layout-key info from JUCE). |
| 16 (associated text) | 4 | Accepted/stored but not encoded. |

CSI u format: `CSI keycode ; modifiers u` where modifiers = `1 + shift(1) + alt(2) + ctrl(4)`.

#### Key Dispatch by Flag

- **`mapPlain`:** Simple keys (Enter=13, Tab=9, Backspace=127, Escape=27) check `shouldUseCsiU()`. Text keys check `kbAllKeys` flag. Functional keys (cursor, F-keys, editing) always use legacy CSI encoding.
- **`mapCtrl`:** Under flags 1/8, Ctrl+letter sends `CSI lowercase_codepoint ; modifiers u` instead of control characters. Ctrl+Shift properly encoded with both modifier bits.
- **`mapAlt`:** Under flags 1/8, Alt+text-key sends `CSI lowercase_codepoint ; modifiers u` instead of ESC-prefix. Alt+functional-key keeps legacy ESC-prefix encoding.

---

## Platform Configuration

### Config File Paths

| Platform | Config path | Window geometry (standalone) | Full state (daemon client) | Daemon port |
|----------|------------|------------------------------|---------------------------|-------------|
| macOS/Linux | `~/.config/end/end.lua` | `~/.config/end/window.state` | `~/.config/end/nexus/<uuid>.display` | `~/.config/end/nexus/<uuid>.nexus` |
| Windows | `%APPDATA%\end\end.lua` | `%APPDATA%\end\window.state` | `%APPDATA%\end\nexus\<uuid>.display` | `%APPDATA%\end\nexus\<uuid>.nexus` |

`Config::getConfigFile()` uses `juce::File::userApplicationDataDirectory` on Windows, `userHomeDirectory/.config/end/` on macOS/Linux. Creates directory and writes defaults if absent.

`window.state` persists WINDOW width/height only (standalone mode, cross-instance geometry). In daemon client mode the client reads and writes `nexus/<uuid>.display` (full WINDOW + TABS state) for session restore.

The daemon's TCP port is written to `nexus/<uuid>.nexus` (plain text) by `Daemon::start()` via `AppState::setPort()`. GUI clients read this file during startup to discover the port before beginning connect attempts. The NEXUS directory/file subtree is `nexus/`, regardless of the config key rename.

The config key controlling daemon mode is `Config::Key::daemon` (`"daemon"` in end.lua).  The ValueTree property is `App::ID::daemonMode` on the WINDOW subtree.

---

## Key Data Types

### Cell (16 bytes, trivially copyable)

```
| codepoint (4B) | style (1B) | layout (1B) | width (1B) | reserved (1B) | fg (4B) | bg (4B) |
```

Style bits: BOLD, ITALIC, UNDERLINE, STRIKE, BLINK, INVERSE
Layout bits: wide continuation, emoji, has grapheme

### Color (4 bytes, trivially copyable)

```
| red/paletteIndex (1B) | green (1B) | blue (1B) | mode (1B) |
```

Mode: theme (0), palette (1), rgb (2)
Access: `setRGB()`, `setPalette()`, `setTheme()`, `paletteIndex()`

### Grid Ring Buffer

Dual buffers (normal + alternate). Each buffer is a flat `HeapBlock<Cell>` with ring-buffer row indexing. `head` tracks the logical top row. Dirty tracking via `std::atomic<uint64_t> dirtyRows[4]` (256-bit bitmask).

`getCols()` and `getVisibleRows()` return the buffer allocation dimensions. `resize()` runs on the message thread and holds `resizeLock` for the duration.

Parser reads geometry via `state.getRawValue<int>(ID::cols)` etc. (lock-free atomics). `getCols()`/`getVisibleRows()` on Grid remain for message-thread consumers.

### GlyphConstraint

Per-codepoint scaling and alignment descriptor for Nerd Font icons. Applied at rasterization time in `jam::Glyph::Packer`.

Fields:
- `ScaleMode` — none / fit / cover / adaptiveScale / stretch
- `AlignH` / `AlignV` — horizontal and vertical alignment within cell
- `HeightRef` — cell height or icon natural height as reference
- `padding` — inset from cell edges
- position/size overrides — pixel-level nudge for specific icons
- `maxAspectRatio` — clamp for very wide icons
- `maxCellSpan` — maximum columns an icon may occupy

Coverage: 10,470 codepoints across 88 switch arms, generated from NF patcher v3.4.0 data.

### FontCollection

O(1) codepoint-to-font-slot lookup. Flat `int8_t[0x110000]` dispatch table covering the full Unicode range. Up to 32 font slots.

Sentinel values: `UNRESOLVED` (-1) — not yet queried; `NOT_FOUND` (-2) — no font covers this codepoint.

Each slot holds: platform font handle + `hb_font_t*` + `hasColorGlyphs` flag.

NF icon font loaded from BinaryData, not the system font manager.

### BoxDrawing

Procedural rasterizer for three Unicode ranges — no font lookup for these codepoints:
- U+2500-U+257F — box drawing characters
- U+2580-U+259F — block elements
- U+2800-U+28FF — braille patterns

Uses SDF for rounded corners and anti-aliased diagonals. Produces pixel-perfect alignment at any cell size.

### ScreenSelection

Anchor + end `Point<int>` pair. `SelectionType` enum (linear/line/box). `containsCell()` dispatches to `contains()`, `containsLine()`, or `containsBox()`. Renders via transparent background overlay using `colours.selection` config color.

### ModalType

`ModalType` is an **app-level** enum stored as an integer property on the **TABS** subtree via `AppState::setModalType()` / `getModalType()`. Both Terminal::Component and Whelmed::Component read it to gate their key dispatch. `ModalType::none` means no modal is active.

```cpp
enum class ModalType : uint8_t { none, selection, openFile };
```

Terminal::State also mirrors the active modal via its own atomic (for the render path). When non-none, ALL keys are intercepted by the active pane's key handler before the Action system or PTY.

**Terminal key dispatch chain:**

```
keyPressed()
    |
    +-- mouse copy shortcut? → copySelection()
    +-- Terminal::Input::handleKey()
            +-- isPopupTerminal? → session.handleKeyPress()
            +-- State::isModal()? → handleModalKey()
            |       +-- selection → handleSelectionKey()
            |       +-- openFile  → handleOpenFileKey()
            +-- Action::handleKeyPress()       (prefix state machine + global bindings)
            +-- isScrollNav? → handleScrollNav()
            +-- clearSelectionAndScroll() + session.handleKeyPress()
```

**Whelmed key dispatch chain:**

```
keyPressed()
    |
    +-- Whelmed::Input::handleKey()
            +-- modal == selection? → handleCursorMovement() + handleSelectionToggle()
            +-- mouse selection + copy key? → clipboard copy
            +-- Action::handleKeyPress()
            +-- handleNavigation()
```

### PaneComponent

Pure virtual base (`Source/component/PaneComponent.h`) shared between Terminal::Component and Whelmed::Component. Inherits `jam::GLComponent`.

**Contract (all methods pure virtual unless noted):**

| Method | Description |
|--------|-------------|
| `getPaneType()` | Returns `App::ID::paneTypeTerminal` or `App::ID::paneTypeDocument` |
| `switchRenderer(type)` | Switches CPU/GPU backend at runtime |
| `getValueTree()` | Returns root ValueTree (SESSION or DOCUMENT) for grafting |
| `applyConfig()` | Applies current Config to the component |
| `enterSelectionMode()` | Enters vim-style keyboard selection mode |
| `copySelection()` | Copies active selection to clipboard and clears it |
| `hasSelection()` | Returns true if a non-degenerate selection is active |
| `focusGained()` | (non-virtual) Sets activePaneID and activePaneType in AppState |

### StatusBarOverlay

`StatusBarOverlay` is a `juce::Component` and `juce::ValueTree::Listener` that listens to the **TABS** subtree for `App::ID::modalType` and `App::ID::selectionType` property changes. Displays the active modal mode name (VISUAL / VISUAL LINE / VISUAL BLOCK) as a status bar.

### Cursor (Whelmed)

`Cursor` (`Source/Cursor.h/cpp`) is a shared descriptor for the Whelmed selection cursor. It stores pixel bounds, blink state, and block/character position. `Whelmed::Screen::updateCursor()` builds and stores the cursor for the current selection position; `Screen::paint()` renders it.

### Whelmed

`Whelmed::Component` is a `PaneComponent` subclass that hosts the markdown viewer. It owns:

- **State** — document ValueTree, atomic block count, parse-complete flag
- **Parser** — background markdown parse thread
- **Screen** — block renderer (owns `Block` instances, handles mouse selection)
- **Whelmed::Input** — modal key dispatch, selection keys, scroll nav
- **LoaderOverlay** — shown during async parse

**Block hierarchy (`Whelmed::Block`):**

```
Block (pure virtual)
  TextBlock     — juce::AttributedString + TextLayout; covers headings, paragraphs, list items, inline code
  MermaidBlock  — SVG-rendered diagram via jam_gui/opengl path tessellation
  TableBlock    — tabular layout
```

Blocks are not `juce::Component` instances — they are data objects that measure and paint themselves into a `juce::Graphics` context. Screen owns them in a flat `std::vector<BlockEntry>` and lays them out vertically.

**Selection architecture:** Mouse events on Screen write anchor/cursor coordinates to the DOCUMENT ValueTree (`App::ID::selAnchorBlock`, `selCursorBlock`, etc.). `Whelmed::Input` reads these same properties to perform keyboard navigation and copy operations. AppState::selectionType and modalType on TABS are the SSOT for selection state visible to the status bar.

### MessageOverlay

Transient overlay component. Shows grid dimensions (columns x rows) during resize. Accepts arbitrary string messages on demand. Fade-in/out animation driven by `jam::Animator`. Replaces the earlier GridSizeOverlay.

### StagedBitmap

Cross-thread upload packet passed from the message thread to the GL thread via the staged bitmap queue. Contains: pixel data, target atlas region, and kind (mono / emoji).

### AtlasGlyph

Cached rasterization result stored in the LRU map. Contains: texture UV rect, pixel dimensions, and bearing (horizontal + vertical offset from cell origin).

### LRUGlyphCache

Frame-stamped LRU map. Each entry records the last frame it was accessed. When capacity is exceeded, the oldest 10% of entries are evicted and their atlas regions returned to the packer.

Capacities: mono 19,000 glyphs; emoji 4,000 glyphs.

---

## Key Design Decisions

### Decision: VBlankAttachment for Render Trigger

**Context:** Timer-based render trigger (60-120Hz) suffered latency under CPU contention because macOS deprioritizes the JUCE timer thread.

**Decision:** Replace timer-driven render with CVDisplayLink-driven polling via `juce::VBlankAttachment`. State stays pure (timer + atomics only). `TerminalDisplay` polls `consumeSnapshotDirty()` on every vsync.

**Rationale:** CVDisplayLink runs at display-driver priority, immune to timer QoS coalescing. Worst-case latency is one frame (8-16ms), imperceptible for terminal text. State remains a pure data layer with no UI knowledge.

### Decision: Context<T> over Meyer's Singleton for Config

**Context:** Config was a Meyer's singleton (`static Config& get()`). This caused static initialization order issues and hid lifetime management.

**Decision:** Config inherits `jam::Context<Config>`. Owned by `ENDApplication`. Accessed via `Config::getContext()->`.

**Rationale:** Explicit lifetime, fail-fast on misuse, no static init ordering problems.

### Decision: Unified Cell (16B) over HotCell/ColdCell Split

**Context:** SPEC originally proposed 8B HotCell + 20B ColdCell SoA layout for cache optimization.

**Decision:** Unified 16-byte Cell with inline fg/bg Color. Grapheme stored separately in a sparse map.

**Rationale:** Simpler code, still fits 2 cells per cache line. The HotCell/ColdCell split added complexity for marginal cache benefit given typical terminal workloads. 95%+ of cells have no grapheme, so the sparse map handles the rare case efficiently.

### Decision: Procedural Box Drawing over Font Glyphs

**Context:** Box drawing characters (U+2500-U+257F) could come from the font or be drawn procedurally.

**Decision:** All box drawing, block elements, and braille rendered procedurally in `BoxDrawing.h`. No font lookup for these ranges.

**Rationale:** Pixel-perfect alignment at any cell size. Font glyphs often have inconsistent metrics causing visible gaps at cell boundaries. Other modern terminals (Ghostty, WezTerm) all use procedural rendering for these ranges.

### Decision: Glyph-Based Cursor over Geometric Shapes

**Context:** SPEC proposed block/underline/bar cursor styles as geometric quads.

**Decision:** Cursor renders any Unicode codepoint via `Fonts::rasterizeToImage()`. Default: U+2588 (full block). Configurable via `cursor.char`.

**Rationale:** More flexible — user can use any character. Color emoji cursors supported. Consistent with the font rendering pipeline.

### Decision: Direct Parser Callback over SPSC FIFO

**Context:** SPEC proposed 2x SPSC ring buffers (`juce::AbstractFifo`) between PTY and message thread.

**Decision:** TTY reader thread calls `Parser::process()` directly. Parser writes to Grid cells and State atomics on the reader thread.

**Rationale:** Simpler, lower latency. The FIFO added a drain step on the message thread that was unnecessary — the parser is fast enough to run on the reader thread without blocking. Grid access is protected by `resizeLock` CriticalSection only during resize.

### Decision: Sideloaded ConPTY (All Windows Versions)

**Context:** The inbox `conhost.exe` on Windows 10 does not support `PSEUDOCONSOLE_WIN32_INPUT_MODE`. The inbox ConPTY on Windows 11 sends `STATUS_CONTROL_C_EXIT` (0xC000013A) to child processes immediately after spawn.

**Decision:** Embed `conpty.dll` + `OpenConsole.exe` (MIT-licensed, from Microsoft Terminal) as JUCE BinaryData. Always extract to `~/.config/end/conpty/` at runtime on all Windows versions. Load `CreatePseudoConsole`, `ResizePseudoConsole`, `ClosePseudoConsole` from the sideloaded DLL. Fall back to `kernel32.dll` only if sideload fails. Pass `PSEUDOCONSOLE_WIN32_INPUT_MODE` (0x4) flag.

**Rationale:** The inbox ConPTY is broken on both OS versions (missing DECSET on Win10, STATUS_CONTROL_C_EXIT on Win11). The sideloaded DLL from Microsoft Terminal works correctly on both.

### Decision: NtCreateNamedPipeFile for ConPTY Pipe

**Context:** `CreateNamedPipeW` (public API) creates half-duplex named pipes with read/write contention on a single handle. Microsoft Terminal uses `NtCreateNamedPipeFile` (undocumented NT API) for a true full-duplex unnamed pipe.

**Decision:** Port Microsoft Terminal's `CreateOverlappedPipe` exactly — `NtCreateNamedPipeFile` + `NtCreateFile` via `GetProcAddress` on `ntdll.dll`. Single duplex pipe, overlapped I/O, same client handle for both hInput and hOutput to `CreatePseudoConsole`.

**Rationale:** True full-duplex eliminates read/write contention. Overlapped I/O provides zero-CPU blocking wait (no polling). Microsoft Terminal ships this in production — stable and tested.

### Decision: NF Glyph Constraint System

**Context:** Nerd Font icons have wildly varying aspect ratios and need per-glyph positioning to look correct in a monospace grid.

**Decision:** Generated constraint table (10,470 codepoints, 88 switch arms) from NF patcher v3.4.0 data. Applied at rasterization time in `jam::Glyph::Packer`.

**Rationale:** Matches NF patcher's own scaling logic. Icons render identically to how they appear in patched fonts, but with runtime flexibility for any cell size.

### Decision: Shared Fonts Context over Per-Terminal Font Ownership

**Context:** Each `Screen` owned its own `Fonts` instance inside a `Resources` struct. When closing tabs rapidly, the GL thread could access a destroyed font while mid-render, causing a HarfBuzz crash.

**Decision:** `Fonts` inherits `jam::Context<Fonts>`, owned by `MainComponent`. All terminals share a single instance via `Fonts::getContext()`.

**Rationale:** Font metrics are global (zoom is global via state.lua). Shared ownership ensures font lifetime exceeds all terminal components. Enables global grid size computation from `Fonts::getContext()->calcMetrics()` without querying individual terminals.

### Decision: Binary Tree ValueTree for Split Pane Layout

**Context:** Split panes needed a recursive layout model that integrates with JUCE's ValueTree state management.

**Decision:** Binary tree where internal nodes are `PANES` (direction + ratio) and leaves are `PANE` (uuid). Tree stored as a JUCE ValueTree, enabling state persistence and listener-based reactivity.

**Rationale:** ValueTree provides free serialization, undo/redo capability, and listener notifications. Binary tree naturally models recursive splits. Each split operation wraps the target leaf in a new internal node with a sibling — O(1) split and remove.

### Decision: Prefix Key over Chord Modifiers for Pane Actions

**Context:** Pane navigation and splitting needed keyboard shortcuts. Options: Cmd+Shift chords, or tmux-style prefix key.

**Decision:** Prefix key system (now `Terminal::Action`). Default prefix: backtick. Action keys: h/j/k/l for navigation, `\`/`-` for splitting.

**Rationale:** Chord modifiers (Cmd+Shift+H/J/K/L) conflict with terminal applications that use these sequences. Prefix key avoids conflicts entirely — the prefix is consumed, then the next key is unambiguously a pane action. Familiar to tmux users. Fully configurable.

### Decision: SESSION Grafted as Child of PANE

**Context:** Terminal state (SESSION ValueTree) needs to be associated with a specific pane in the split tree.

**Decision:** SESSION is grafted as a child of its PANE node in the ValueTree hierarchy. `Panes` manages the grafting at terminal creation time and ungrafts before tree restructuring.

**Rationale:** Co-locating SESSION under PANE enables future state persistence of the full split layout + terminal state in a single ValueTree. Ungrafting before `PaneManager::remove()` prevents re-parenting asserts when the tree restructures.

---

## Font Architecture

### Display Monolithic — Embedded Font

Single embedded TTF covering all modern terminal rendering needs. No exotic scripts — those delegate to OS system fonts at runtime.

**Build Pipeline (strict order):**

```
1. Display Mono (base)          -- 98 glyphs + 12 ligatures, advance width uniform
2. + Noto Sans Symbols 2        -- geometric, dingbats, block elements, braille,
                                   legacy computing (U+1FB00), miscellaneous symbols
3. + Noto Emoji (non-color)     -- monotone pictographs U+1F300-U+1F5FF
4. -> NF patcher                -- --complete --mono --careful
                                   careful = never overwrite existing glyphs
```

`fonttools merge` step 1-3 with Display Mono first (first-file-wins on conflicts). NF patch runs last on the merged result.

**What Display Monolithic covers:**
- All ASCII + extended Latin
- Developer ligatures
- Nerd Font icons (complete)
- Powerline symbols
- Geometric shapes, dingbats, arrows, misc symbols
- Block elements, braille, legacy computing sextants
- Monotone emoji/pictographs

**What END handles internally (not in font):**
- Box drawing U+2500-U+257F — synthesized via BoxDrawing procedural rasterizer
- Block elements U+2580-U+259F — synthesized via BoxDrawing
- Braille U+2800-U+28FF — synthesized via BoxDrawing
- Color emoji — delegated to system font (Apple Color Emoji / Segoe UI Emoji / Noto Color Emoji)
- CJK and exotic scripts — delegated to OS system fonts via CoreText (Mac) / DirectWrite (Windows) / Fontconfig (Linux)

**Donor font licenses:**
- Noto Sans Symbols 2 — OFL
- Noto Emoji — OFL
- Symbols Nerd Font — OFL (NF patcher source)
- Display Mono — proprietary (JRENG), embedded binary only

### FontCollection Subsystem

`FontCollection` owns the flat `int8_t[0x110000]` codepoint dispatch table. On first access for a codepoint, it queries the font stack and caches the result. Subsequent lookups are a single array read.

The NF icon font (`SymbolsNerdFont-Regular.ttf`) is loaded from BinaryData, not the system font manager. This ensures consistent icon rendering regardless of what fonts the user has installed.

### GlyphConstraint Subsystem

`GlyphConstraintTable.cpp` is a generated file. It maps NF icon codepoints to `GlyphConstraint` descriptors that replicate the scaling decisions made by the NF patcher. `jam::Glyph::Packer` applies the constraint before writing pixels to the atlas, so icons are positioned and scaled identically to how they appear in a patched font — but at any runtime cell size.

### BoxDrawing Subsystem

`BoxDrawing.h` intercepts codepoints in the box drawing, block element, and braille ranges before any font lookup occurs. It rasterizes directly to a pixel buffer using:
- Integer arithmetic for straight lines and solid blocks
- SDF for rounded box corners
- Anti-aliased Bresenham for diagonal lines

This eliminates the font-metric inconsistencies that cause visible seams between adjacent box drawing characters.

### Embolden

Bold variants use platform-native stroke widening rather than a separate bold font file:
- macOS: `kCGTextFillStroke` with stroke width proportional to cell size
- Linux: `FT_Outline_Embolden` applied to the FreeType outline before rasterization

### Font Stack at Runtime

```
Display Monolithic -> OS system fonts (CJK/exotic) -> OS color emoji
```

### Font Ownership

`Fonts` inherits `jam::Context<Fonts>` — single global instance owned by `MainComponent`. All terminals share the same font handles, shaping buffers, and metrics. `Screen` and `CursorComponent` access fonts via `Fonts::getContext()`. This ensures font lifetime exceeds all terminal components, preventing use-after-free when closing tabs.

### Platform Font Dispatch

Same header (`Fonts.h`), different implementations. Caller site identical.

| Platform | Font Loading | Text Shaping | Emoji Shaping | Glyph Rasterization |
|----------|-------------|--------------|---------------|---------------------|
| macOS | CoreText (`CTFontCreateWithName`) | HarfBuzz (`hb_coretext_font_create`) | HarfBuzz (`emojiHbFont`) | CoreText (`CTFontDrawGlyphs`) |
| Linux/Win | FreeType (`FT_New_Face`) | HarfBuzz (`hb_ft_font_create`) | TBD | FreeType (`FT_Render_Glyph`) |

HarfBuzz is JUCE's bundled version (10.1.0, `HAVE_CORETEXT=1` on macOS).

### Font Resize (Zoom)

`Fonts::setSize()` resizes all font handles when zoom changes (Cmd+/-/0):

**macOS:** `CTFontCreateCopyWithAttributes` on each font (mainFont, emojiFont, identityFont, nerdFont). Destroys and recreates all HarfBuzz shaping fonts. Clears the `fallbackFontCache` (releases all cached CTFontRefs). Updates FontCollection entry pointers (slot 0 = identityFont, slot 1 = nerdFont).

**Linux:** `FT_Set_Char_Size` on all FT_Face handles (4 style faces, emojiFace, nfFace). Destroys and recreates `nerdShapingFont`. Updates FontCollection slot 1.

Zoom state is persisted in `~/.config/end/state.lua`, not in `end.lua` config.

---

## Component Extraction (TerminalDisplay)

`TerminalDisplay` delegates to three focused handlers:

| Class | File | Responsibility |
|-------|------|----------------|
| Terminal::Input | component/Terminal/Input.h/cpp | Modal gate, selection keys, open-file keys, scroll nav |
| Terminal::Mouse | component/Terminal/Mouse.h/cpp | PTY forwarding, drag selection, click dispatch, wheel scroll |
| LinkManager | terminal/selection/LinkManager.h/cpp | Viewport scan, hit-test, dispatch, OSC 8 span merging |

All selection/gesture state in State parameterMap. ScreenSelection rebuilt from State in onVBlank.

---

## Shell Integration

Automatic OSC 133 injection via shell-specific mechanisms:

| Shell | Mechanism | Env var |
|-------|-----------|---------|
| zsh | ZDOTDIR wrapper | ZDOTDIR, END_ORIG_ZDOTDIR |
| bash | ENV + --posix | ENV, END_BASH_INJECT |
| fish | XDG_DATA_DIRS prepend | XDG_DATA_DIRS, END_FISH_XDG_DATA_DIR |
| pwsh | Launch args | -NoLogo -NoProfile -NoExit -Command |

Scripts embedded as BinaryData, sideloaded to `~/.config/end/` at launch.
Controlled by `shell.integration` config (default true).

Parser handles OSC 133 A/B/C/D. Output block boundaries tracked in State.
Click-mode link underlines only render on OSC 133 output rows.

---

## Glossary

| Term | Definition |
|------|------------|
| AppState | Application-level ValueTree root; tracks active terminal UUID, pwd via Value::referTo |
| AtlasGlyph | Cached rasterization result: texture UV rect, pixel dimensions, bearing |
| BoxDrawing | Procedural rasterizer for box drawing, block elements, and braille — no font lookup |
| BoxSelection | Rectangle selection: anchor + end cell coordinates, rendered as overlay |
| ConPTY sideload | Runtime extraction of conpty.dll + OpenConsole.exe from BinaryData to ~/.config/end/conpty/ (all Windows versions — inbox ConPTY broken on both Win10 and Win11) |
| getActiveScreen | Message-thread ValueTree reader for active screen state (normal/alternate) |
| Cell | 16-byte struct representing one terminal character position |
| displayName | Derived tab label from `App::ID::displayName`.  Terminal panes: foregroundProcess (non-empty, set by Session::onFlush when fgPid != shellPid) > cwd leaf name.  Whelmed panes: file basename set at openFile time. |
| Embolden | Platform stroke-widening for bold: kCGTextFillStroke (macOS), FT_Outline_Embolden (Linux) |
| FontCollection | Flat int8_t[0x110000] codepoint-to-font-slot dispatch table, O(1) lookup |
| GlyphConstraint | Per-codepoint NF icon scaling/alignment descriptor applied at rasterization time |
| Grapheme | Multi-codepoint character cluster (e.g., flag emoji, combining marks) |
| Grid | Ring-buffer storage for terminal cells, dual-screen (normal/alternate) |
| LRUGlyphCache | Frame-stamped LRU map; evicts oldest 10% when over capacity |
| GLMailbox | Generic lock-free atomic pointer exchange template (`jam::GLMailbox<T>`) |
| GLSnapshotBuffer | Double-buffered snapshot owner with GLMailbox (`jam::GLSnapshotBuffer<T>`) |
| LookAndFeel | Custom JUCE LookAndFeel: tab line indicator, popup menu glass blur, colour system via Config |
| MessageOverlay | Transient overlay for status messages (reload confirmation, config errors) |
| Action | Unified action registry: action table + key map + prefix state machine, Context-managed, owned by MainComponent |
| ActionList | Command palette component: jam::Window with fuzzy-searchable list of all registered actions |
| PaneManager | Binary tree ValueTree layout engine for recursive split pane layout |
| PaneResizerBar | Draggable divider bar between split panes, paired with split tree nodes |
| Panes | Per-tab component owning Terminal::Component instances and managing split layout via PaneManager |
| Pen | Current text attributes (style + fg/bg color) applied to new cells |
| pwdValue | juce::Value in AppState bound via referTo to active terminal's cwd property |
| ScreenSelection | Anchor + end Point<int> pair for text selection; contains() for hit testing |
| Snapshot | Pre-built GPU instance data (glyphs + backgrounds) for one frame |
| StagedBitmap | Cross-thread upload packet: pixel data + atlas region + mono/emoji kind |
| State | APVTS-style atomic + ValueTree bridge for cross-thread terminal state |
| Tabs | TabbedComponent subclass; Value::Listener for tabName, manages Panes instances |
| VBlank | Display vertical blank — CVDisplayLink callback synced to monitor refresh |
| Atlas | 4096x4096 texture containing rasterized glyphs, shelf-packed |

---

*This document reflects the codebase as implemented. If code diverges, update this document.*
