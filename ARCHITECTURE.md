# END - Architecture

**Purpose:** Single source of truth for project structure, patterns, and contracts.

**Status:** STABLE

**Last Updated:** 2026-06-01 (Terminal-as-editor model — jam::CodeModel multi-screen document, jam::CodeView pure view, jam::BufferSPSC drop-oldest transport, CellFifo two-ring SPSC, Display drains CellFifo into CodeModel, liveTailExtent drained-count boundary, D13a alt clear on entry, D15 caret through wrap fix)

---

## Project Overview

### Purpose

END (Ephemeral Nexus Display) is a GPU/CPU-rendered, fully-featured terminal emulator built with C++17 and JUCE. Tabs, split panes, Lua configuration, unified action registry with prefix-key modal input. Renders terminal output through an OpenGL pipeline or a SIMD-optimised CPU software renderer, with FreeType/HarfBuzz text shaping and a glyph atlas cache. Runtime GPU/CPU switching via config hot-reload.

### Architecture Philosophy

APVTS-inspired data flow. Reader thread writes atomics, timer flushes to ValueTree, UI pulls from ValueTree listeners. Render trigger is timer-driven flush (60/120 Hz) — timer flushes dirty atomics to ValueTree, listeners repaint. No thread pushes to another — all communication is pull-based.

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
  Main.cpp                          Application entry, owns lua::Engine + MainWindow
  Main.h                            ENDApplication declaration
  MainComponent.h/cpp               Root component, owns Fonts context, Tabs, Action, MessageOverlay, GLRenderer; receives lua::Engine& from ENDApplication
  MainComponentActions.cpp          Built-in action registration
  AppState.h/cpp                    Application state: ValueTree root, pwdValue (live cwd binding), active pane tracking
  AppIdentifier.h                   App-level ValueTree identifiers (app::id:: namespace: END, WINDOW, TABS, TAB, PANES, DOCUMENT) + pane type string constants + app::RendererType enum
  SelectionType.h                   App-level SelectionType enum (none, visual, visualLine, visualBlock)
  ModalType.h                       App-level ModalType enum (none, selection, openFile)
  Cursor.h/cpp                      Shared cursor descriptor used by whelmed::Screen

  lua/
    Engine.h                        lua::Engine — unified Lua config + scripting engine, Context<Engine>
    Engine.cpp                      Engine lifecycle: constructor, initDefaults, writeDefaults, load, reload, registerApiTable, registerActions, buildKeyMap, buildTheme, parseColour, dpiCorrectedFontSize, fileChanged
    EngineConfig.cpp                Engine config apply/patch helpers
    EngineDefaults.cpp              Default config generation
    EngineParse.cpp                 Lua table parsing: parseNexus, parseDisplay, parseWhelmed, parseKeys, parsePopups, parseActions, parseSelectionKeys
    EngineParseConfig.cpp           Config-specific parse helpers
    EngineParseDisplay.cpp          Display config parse helpers
    EnginePatch.cpp                 keys.lua file patching (remap), key lookup utilities

  config/
    default_end.lua                 Template entry point — require() for six modules
    default_nexus.lua               Template for nexus.lua (gpu, daemon, shell, terminal, hyperlinks)
    default_display.lua             Template for display.lua (window, colours, cursor, font, tab, pane, overlay, menu, action_list, status_bar, popup border)
    default_keys.lua                Template for keys.lua (prefix, bindings, selection keys)
    default_popups.lua              Template for popups.lua (defaults, popup entries)
    default_actions.lua             Template for actions.lua (custom Lua actions with api.* calls)
    default_whelmed.lua             Template for whelmed.lua (typography, colours, navigation)

  whelmed/
    Block.h                         Pure virtual base for all renderable block types
    GenericTokeniser.h/cpp          Generic lexer tokenizer used by Tokenizer
    InputHandler.h/cpp              Modal gate, selection keys, scroll nav for Whelmed pane
    MermaidBlock.h/cpp              SVG-rendered mermaid diagram block
    MermaidSVGParser.h              SVG path parser for mermaid output
    Parser.h/cpp                    Async markdown parser thread
    Screen.h/cpp                    Block-based document renderer, owns Block instances, handles mouse selection
    State.h/cpp                     Whelmed document state: ValueTree, atomic block count, parse complete
    TableBlock.h/cpp                Tabular layout block
    TextBlock.h/cpp                 Flowing styled text block (paragraphs, headings, lists)
    Tokenizer.h/cpp                 Markdown token stream
    component/
      Component.h/cpp               whelmed::Component — PaneComponent subclass, markdown viewer pane

  fonts/
    DisplayMono-Book.ttf            Embedded base font (BinaryData)
    DisplayMono-Bold.ttf            Embedded bold variant (BinaryData)
    DisplayMono-Medium.ttf          Embedded medium variant (BinaryData)
    SymbolsNerdFont-Regular.ttf     Embedded NF icon font (BinaryData)

  terminal/
    CSI.h                           CSI parameter accumulator
    CharProps.h                     Character property flags
    CharPropsData.h                 Character property lookup table
    Charset.h                       Character set tables (G0/G1)
    DispatchTable.h                 VT state machine transition table
    // History.h/cpp removed — byte replay deferred (DEBT-20260526T220000)
    Identifier.h                    ValueTree IDs + Identifier hash (terminal::id namespace)
    ImageDecode.h/cpp               Platform-independent BGRA→RGBA swizzle + ImageSequence struct
    ImageDecodeGif.h                GIF binary metadata parser (static, shared by platform TUs)
    ImageDecodeMac.mm               macOS: CGImageSource single + multi-frame decode with GIF disposal
    ImageDecodeWin.cpp              Windows: WIC single + multi-frame decode with GIF disposal; Linux stub
    Input.h/cpp                     Modal gate, selection keys, open-file keys, scroll nav
    ITerm2Decoder.h/cpp             iTerm2 inline image protocol decoder (OSC 1337)
    Keyboard.h/cpp                  Keypress → escape sequence mapping (progressive keyboard protocol, CSI u)
    KittyDecoder.h/cpp              Kitty graphics protocol decoder
    KittyDecoderDecode.cpp          Kitty decode internals
    LinkDetector.h                  Link detection heuristics
    LinkManager.h/cpp               Viewport scan, cell-native hyperlink scanning, hit-test, click dispatch
    LinkManagerScan.cpp             Link scan implementation
    LinkSpan.h                      Hyperlink span descriptor
    Mouse.h/cpp                     PTY forwarding, drag selection, click dispatch, wheel scroll
    Notifications.h/cpp             Cross-platform desktop notification API (terminal::showNotification())
    Notifications.mm                macOS: UNUserNotificationCenter with foreground delegate
    Palette.h                       256-color palette (std::array)
    Parameter.h                     APVTS-style parameter slot type
    Parser.h/cpp                    VT state machine + byte stream decoder
    ParserAction.cpp                Parser action dispatch
    Processor.h/cpp                 Pipeline orchestrator: owns Parser, Video, TTY (created by startTTY()), CellFifo (two-ring SPSC); references State received from Session; exposes suspendProcessing(), isSuspended(), getCallbackLock() (JUCE AudioProcessor pattern); prepare() resizes Video grid only — CellFifo and CodeView untouched; jam::Resizer owned by Session
    Map.h                           terminal::Map — jam::Map::Bool, jam::Map::Screen, jam::Map::Gpu typed map instances
    Session.h/cpp                   PTY orchestrator: owns jam::CodeModel (multi-screen document, numScreens=2) + jam::CodeView (textEditor) + Processor (unique_ptr) + jam::Resizer (unique_ptr); Processor owns TTY (via startTTY) and CellFifo (two-ring SPSC); Session is Value::Listener for winsize; wireResizer() called from both constructors
    SixelDecoder.h/cpp              Sixel graphics protocol decoder
    SixelDecoderParse.cpp           Sixel decode internals
    Skit.h/cpp                      SKiT (Sixel/Kitty/iTerm2) unified preview protocol entry point
    State.h/cpp                     APVTS-style atomic + timer + ValueTree
    StateFlush.cpp                  Timer flush implementation
    TextBuffer.h                    Text buffer utility
    Video.h/cpp                     VT command processor: cursor, pen, modes, Buffer<Row> writes; setCursor(CursorState) + setWrapPending() for screen state; setWinsize() is the single resize API (no doPlatformResize NVI)
    VideoCSI.cpp                    CSI dispatch (cursor, erase, mode)
    VideoDCS.cpp                    DCS dispatch (Sixel entry)
    VideoESC.cpp                    ESC dispatch (charset, OSC 0/2/7/8/9/12/52/112/133/777, DCS)
    VideoEdit.cpp                   Erase, scroll, screen switch
    VideoMode.cpp                   Mode (DECSET/DECRST) dispatch
    VideoOSC.cpp                    OSC handlers
    VideoOSCExt.cpp                 Extended OSC handlers
    VideoOps.cpp                    Cursor movement, tab, reset
    VideoSGR.cpp                    SGR (text attributes, color)

    action/                         Unified action registry + key dispatch
      Action.h/cpp                  action::Registry — action table, key map, prefix state machine, Context<Registry>
      ActionList.h/cpp              action::List — command palette component (jam::Window, fuzzy-searchable action list)
      ActionListBinding.cpp         ActionList binding mode logic
      ActionListSelection.cpp       ActionList selection/navigation logic
      ActionRow.h/cpp               Row component for ActionList (displays action name + keybinding)
      KeyHandler.h/cpp              Key event routing for ActionList modal input

    component/                      UI hosting layer (Display, ScreenSelection, Overlay, Panes, Tabs, LookAndFeel)
      Dialog.h/cpp                  Modal dialog component
      Display.h/cpp                 terminal::Display — UI host; delegates to Input + Mouse; resized() calls session.getTextEditor().setBounds(contentBounds) and writes pixel dims to State; no terminal::Screen class — Session owns jam::CodeModel + jam::CodeView directly; Display parents CodeView via addAndMakeVisible; drains CellFifo into CodeModel on screenDirty/activeScreen; per-screen liveTailExtent tracks drained active-row count; D13a clears alt screen on entry; D15 caret offset via viewportLineCount
      LoaderOverlay.h               Loading spinner overlay (used by whelmed::Component)
      LookAndFeel.h/cpp             Custom LookAndFeel: tab styling, popup menu, colour system
      LookAndFeelMenu.cpp           Menu LookAndFeel overrides
      LookAndFeelTab.cpp            Tab LookAndFeel overrides
      MessageOverlay.h              Transient overlay for status messages (reload, errors)
      ModalWindow.h/cpp             Modal window host
      Overlay.h/cpp                 terminal::Overlay — jam::animation::Base child of Display; owns juce::Image, border, animation timer
      PaneComponent.h               Pure virtual base for pane-hosted components (terminal::Display, whelmed::Component)
      Panes.h/cpp                   Per-tab pane container, owns Owner<PaneComponent> and PaneResizerBars
      Panes.cpp                     Panes implementation
      Popup.h/cpp                   Popup terminal component
      ScreenSelection.h             Selection anchor/end, contains() hit test, inversion rendering
      StatusBarOverlay.h            Overlay that listens to TABS subtree for modal/selection state display
      Tabs.h/cpp                    terminal::Tabs — tab container, manages one Panes instance per tab
      TabsActions.cpp               Tabs action registration and dispatch
      TabsClose.cpp                 Tab close cascade implementation
      TerminalWindow.h/cpp          Standalone terminal window host

    tty/                            Platform TTY abstraction
      TTY.h/cpp                     Abstract base + reader thread
      UnixTTY.h/cpp                 macOS/Linux: forkpty
      WindowsTTY.h/cpp              Windows: ConPTY via NtCreateNamedPipeFile, overlapped I/O, sideloaded conpty.dll

  nexus/                            Session manager + IPC transport layer
    Channel.cpp                     nexus::Daemon::Channel — server-side per-client connection (nested class, impl only)
    Daemon.h/cpp                    nexus::Daemon — JUCE InterprocessConnectionServer; owns Channel objects; broadcast + per-session subscriber registries; wireSessionCallbacks
    Daemon.mm                       macOS/Linux platform helpers: hideDockIcon(), spawnDaemon()
    DaemonWindows.cpp               Windows-specific platform helpers: Job Object (cascade-kill), spawnDaemon()
    EncoderDecoder.h/cpp            Binary wire-format encode/decode helpers (writeUint16/32/64, writeString, readUint16/32/64, readString, encodePdu)
    Link.h/cpp                      nexus::Link — client-side JUCE IPC connector; connect-retry timer; sends PDUs to daemon; dispatches incoming PDUs to Nexus
    Message.h                       nexus::Message — enum class Message : uint16_t — PDU kind identifiers with stable wire values
    Nexus.h/cpp                     Nexus (global scope) — jam::Context<Nexus> session container; create/remove/get/has/list; Mode enum (standalone/daemon/client); events ValueTree for lifecycle listeners

~/Documents/Poems/dev/jam/
  jam_core/                         Shared utilities (Owner, identifiers, Context, BinaryData)
    concurrency/
      jam_buffer_spsc.h             Index-only drop-oldest SPSC ring (fork of juce::AbstractFifo)
  jam_graphics/                     Graphics utilities, blur, shadows, colours, font management, glyph atlas, typeface shaping, text layout
    fonts/                          Font management, glyph atlas, typeface shaping, text layout
    detail/                         Cell types, Row, packed descriptors
      jam_code_line.h               CodeLine — one logical line (HeapBlock<Char> + cellCount, getWrappedLines)
      jam_code_model.h              CodeModel — multi-screen dimensionless document model (Owner<Screen>)
  jam_gui/                          Window, layout utilities, PaneManager, PaneResizerBar
    code_editor/
      jam_code_view.h/cpp           CodeView — pure view rendering const CodeModel& via wrap-aware projection
    layout/
      jam_pane_manager.h/cpp        Binary tree ValueTree layout engine for split panes
      jam_pane_resizer_bar.h/cpp    Draggable divider bar between panes
```

### Module Inventory

| Module | Location | Responsibility | Dependencies |
|--------|----------|----------------|--------------|
| AppState | `Source/` | App-level config SSOT — font, cursor, padding, scrollback seeded from lua::Engine at init + hot reload. ValueTree root, pwd tracking, active pane type + UUID. Components listen and react — no manual config cascade. | JUCE ValueTree, terminal::id, lua::Engine (init only) |
| AppIdentifier | `Source/` | ValueTree node and property identifiers (app::id:: namespace); pane type string constants; app::RendererType enum | JUCE |
| lua::Engine | `lua/` | Unified Lua config + scripting engine. Sole owner of `jam::lua::state` — SSOT for all settings, keybindings, popup definitions, and custom actions. Six typed module structs (Nexus, Display, Whelmed, Keys, Popup, Action) replace string-keyed value maps. Unified colour parser handles `#RRGGBB`, `#RRGGBBAA`, and bare `RRGGBBAA` formats. File watcher triggers total reload on any `.lua` change (gated by `nexus.autoReload`). Provides parsed bindings to `action::Registry`, selection keys to `terminal::Input` / `whelmed::InputHandler`, and Theme to Screen. | sol2, jam::Context, jam::File::Watcher |
| Component | `terminal/component/` | JUCE UI hosting, tabs, panes, LookAndFeel, timer-driven render trigger | Session, TextEditor, PaneManager, AppState |
| Fonts | `fonts/` | Embedded TTF binaries (BinaryData) | — |
| Terminal | `terminal/` | Pure value types, state atomics, IDs, VT parsing, Video command processor, grid storage, session orchestration, preview decoders | JUCE ValueTree |
| Rendering | `terminal/component/` | TextEditor render host, GL/CPU draw, Fonts (Context-managed), Overlay (image preview component) | terminal/, FreeType, HarfBuzz, OpenGL, jam_graphics, jam_tui |
| Notifications | `terminal/` | Native desktop notification dispatch (`terminal::showNotification()`, OSC 9/777) | JUCE, UserNotifications (macOS) |
| TTY | `terminal/tty/` | Platform PTY abstraction, reader thread | JUCE Thread |
| jam_core | `~/Documents/Poems/dev/jam/jam_core/` | Shared utilities, identifiers, Context, BinaryData | JUCE core |
| jam_graphics | `~/Documents/Poems/dev/jam/jam_graphics/` | Graphics utilities, blur, shadows, colours; `fonts/` — font management, glyph atlas, typeface shaping, text layout; `detail/` — Cell types, Row, packed descriptors | jam_core, FreeType, HarfBuzz |
| jam_tui | `~/Documents/Poems/dev/jam/jam_tui/` | Terminal UI primitives: ANSI rendering, terminal metrics, raw input, ANSI markdown renderer | jam_core, jam_graphics |
| jam_gui/opengl | `~/Documents/Poems/dev/jam/jam_gui/opengl/` | GL mailbox, snapshot buffer, path tessellation, Graphics-like API | juce_opengl, jam_core |
| Action | `terminal/action/` | Unified action registry (`action::Registry`), key dispatch, prefix state machine, command palette (`action::List`) | lua::Engine, jam::Context |
| Nexus | `nexus/` | Session container (global scope). Owns `unordered_map<String, unique_ptr<terminal::Session>>`. Mode determined by `setMode(Mode)` — standalone/daemon/client. Fires session lifecycle events on public `events` ValueTree. `jam::Context<Nexus>` singleton owned by ENDApplication. | terminal::Session, jam::Context |
| IPC | `nexus/` | IPC transport layer. `nexus::Daemon` (TCP server), `nexus::Link` (client), `nexus::Daemon::Channel` (per-client server-side connection, nested class), `nexus::EncoderDecoder` (wire format), `nexus::Message` (PDU kind enum). Daemon owns broadcast + subscriber registries, wires session callbacks. Daemon/Link listen on Nexus::events ValueTree. | JUCE IPC, Nexus, terminal::Session, AppState |
| Panes | `terminal/component/` | Per-tab pane container, owns Owner<PaneComponent> and resizer bars | PaneManager, PaneComponent |
| Whelmed | `whelmed/` | Markdown viewer: whelmed::Component, Screen, block hierarchy, InputHandler | PaneComponent, jam_markdown |
| jam_gui | `~/Documents/Poems/dev/jam/jam_gui/` | Window, layout utilities: PaneManager binary tree, PaneResizerBar. `jam::ComponentAttachment` — APVTS-style component-to-ValueTree binding used by Display for the DISPLAY node (cellWidth, cellHeight, baseline, fontSize) and NORMAL/ALTERNATE screen nodes. | jam_core, jam_graphics |

---

## Nexus and IPC

### Nexus — Session Manager

`Nexus` is a pure session container in global scope. It inherits `jam::Context<Nexus>` and is owned as a value member of `ENDApplication`. It holds an `unordered_map<String, unique_ptr<terminal::Session>>` and exposes a lifecycle API: `create`, `remove`, `get`, `has`, `list`.

Data flow mode (standalone, daemon, client) is determined at runtime by `setMode(Mode)`:

- **`Mode::standalone`** — no IPC. Sessions fire exit signal via State shellExited parameter → `Panes::valueTreePropertyChanged` → `callAsync` → `Panes::closePane` → `Nexus::remove`. When the last session exits, `ENDApplication` detects removal via `valueTreeChildRemoved` on `Nexus::events` and initiates quit.
- **`Mode::daemon`** — `nexus::Daemon` registers as a `juce::ValueTree::Listener` on `Nexus::events`. When `Nexus::create` fires a child-added event, Daemon wires IPC broadcast callbacks on the new session.
- **`Mode::client`** — `nexus::Link` registers on `Nexus::events`. When `Nexus::create` fires a child-added event for a remote session, Link sends a `createSession` PDU to the daemon.

`Nexus::create(cwd, uuid, cols, rows)` is the mode-routing entry point used by `terminal::Panes` and `terminal::Tabs`. It returns an existing session immediately if the UUID already exists (idempotency guard for GUI reconnect). Nexus fires `juce::ValueTree::Listener` callbacks on the public `events` tree — child nodes are type "SESSION" with `jam::ID::id` property set to the session UUID.

### Process Configurations

```
Standalone:              ENDApplication + Nexus (no IPC)
Daemon process:          ENDApplication + Nexus + nexus::Daemon (headless, owns shells)
GUI connected to daemon: ENDApplication + Nexus + nexus::Link  (renders daemon's sessions)
```

The daemon process suppresses its Dock icon via `nexus::Daemon::hideDockIcon()` and writes its bound TCP port to `~/.config/end/nexus/<uuid>.nexus`. The GUI reads that file and begins connect attempts via `nexus::Link::beginConnectAttempts()`.

### IPC Transport Layer

The `nexus` namespace contains the TCP-based IPC transport between a daemon process and one or more GUI clients. It does not include any terminal emulation logic.

**Classes:**

| Class | Role |
|-------|------|
| `nexus::Daemon` | TCP server (`juce::InterprocessConnectionServer`). Owns `Channel` objects via `jam::Owner`. Holds the broadcast list (`attached`) and per-session subscriber registry (`subscribers`). Installs a Windows Job Object for cascade-kill of OpenConsole.exe grandchildren. Registers on `Nexus::events` to wire callbacks when sessions are created. |
| `nexus::Daemon::Channel` | Server-side connection representing one connected GUI client. Created by `Daemon::createConnectionObject()`. Dispatches incoming PDUs to `Nexus` and `Daemon`. Nested class — implementation in `Channel.cpp`. |
| `nexus::Link` | Client-side connector (`juce::InterprocessConnection`). Polls the nexus file for the daemon port and retries every 100 ms via an inner `ConnectTimer`. Dispatches incoming PDUs directly on the message thread. Registers on `Nexus::events` to send createSession PDUs when sessions are created in client mode. |
| `nexus::EncoderDecoder` | Binary wire-format helpers: `writeUint16/32/64`, `writeString`, `readUint16/32/64`, `readString`, `encodePdu`. Single source of truth for wire encoding — used by both `Daemon::Channel::sendPdu` and `Link::sendPdu`. |
| `nexus::Message` | `enum class Message : uint16_t` — PDU kind identifiers with stable wire values. |

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

`nexus::Daemon` observes `Nexus::events` via `juce::ValueTree::Listener`. On `valueTreeChildAdded`, it calls `wireSessionCallbacks(uuid, session)` to install two callbacks:

- `wireOnBytes` — calls `session.getProcessor().setBytesObserver` to broadcast `nexus::Message::output` to per-session subscribers. Runs on the reader thread; acquires `connectionsLock`.
- `wireOnExit` — registers Daemon as VT listener on session State; on shell exit broadcasts `nexus::Message::sessionKilled`, schedules async `Nexus::remove`, and re-broadcasts the sessions list. App quit on last session removal is handled by `ENDApplication::valueTreeChildRemoved` on `Nexus::events`.

State updates (cwd, foregroundProcess) are broadcast from `Daemon::valueTreePropertyChanged` — Daemon listens to each session's State ValueTree directly. No separate `wireOnStateFlush` callback.

### Snapshot Restore on Client Attach

When a GUI client sends `createSession` for an existing UUID, `nexus::Daemon::attachSession` registers the client as a subscriber. Byte replay (sending `nexus::Message::loading` from a history snapshot) is deferred — DEBT-20260526T220000. The lock is held across subscriber registration to prevent interleaving with the reader thread's output broadcast.

### Byte-Forward Flow (Live)

```
Daemon:  PTY → Processor::events[id::data] → setBytesObserver → nexus::Message::output → nexus::Daemon::Channel → nexus::Link
Link:    handleOutput → terminal::Session::process → Processor → Buffer<Row> → terminal::Display

Standalone:
         PTY → Processor::events[id::data] → Processor::process → Buffer<Row> → terminal::Display
```

### terminal::Session

`terminal::Session` is the singular owner of one terminal instance. It holds:
- `jam::CodeModel codeModel` — multi-screen document model (numScreens = Map::Screen::count = 2). Dimensionless — no viewport width or column dimension. Each screen is a bounded deque of `jam::CodeLine`. Primary (index 0): scrollback-bounded, autowrap-join on finalization. Alternate (index 1): capacity = viewport rows, no scrollback.
- `jam::CodeView textEditor` — pure view rendering the CodeModel. Holds `const CodeModel&`. Projects via `getWrappedLines` at paint time. Owned by Session, parented by Display for rendering.
- `unique_ptr<terminal::Processor>` — VT pipeline orchestrator. Processor owns the TTY created by `startTTY()` and the CellFifo (two-ring SPSC transport).
- `unique_ptr<jam::Resizer> resizer` — resize coordinator. Wired in `wireResizer()`, called from both constructors.
- `juce::Value winsize` — bound to TextEditor's viewport property in State; fires `valueChanged` when cell dimensions change.

No `History` — byte replay deferred (DEBT-20260526T220000).

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
| `start()` | Calls `processor->prepare()` then `processor->startTTY()` — deferred until after Display and TextEditor are in component hierarchy |
| `process(data, len)` | Feed raw bytes from daemon IPC into the Processor |
| `getStateInformation(block)` | Stubbed — state serialization pending (DEBT-20260526T220000) |
| `setStateInformation(data, size)` | Stubbed — state serialization pending |
| `getProcessor()` | Returns the owned `terminal::Processor` |
| `getTextEditor()` | Returns the owned `jam::CodeView` (Display calls `addAndMakeVisible(session.getTextEditor())`) |
| `getCodeModel()` | Returns the owned `jam::CodeModel` (Display mutates via append/remove/clear) |

### Windows Job Object (Cascade-Kill)

`nexus::Daemon::installPlatformProcessCleanup()` creates a Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` and assigns the daemon process to it. When the daemon process exits (normally or abnormally), the OS closes the Job Object handle and kills all child processes — including `OpenConsole.exe` grandchildren spawned by ConPTY. The handle is stored in `Daemon::jobObject` and released in `releasePlatformProcessCleanup()`.

---

## Layer Separation Rules

```
 Application (ENDApplication, MainComponent, terminal::Tabs, terminal::Panes)
    — wires Nexus + IPC; owns all top-level lifetimes
    |
    v
 IPC (nexus::Daemon, nexus::Link, nexus::Daemon::Channel, nexus::EncoderDecoder, nexus::Message)
    — IPC transport; includes Nexus.h; does NOT include terminal headers directly
    |
    v
 Nexus (session container, global scope)
    — includes terminal::Session; forward-declares nexus::Daemon and nexus::Link
    |
    v
 Terminal / Logic (Processor → Video → Buffer<Row>)   writes atomics on reader thread
    |
    v
 Terminal / Data (State/Buffer<Row>)                   pure types, atomic storage, timer flush
    |
    v
 Terminal / Component (terminal::Display)                TextEditor writes packed viewport to State via updateWinsize(); Session owns jam::Resizer — start trigger suspends processing, stop trigger calls prepare(); Display parents TextEditor via addAndMakeVisible, sets pixel bounds in resized()
    |
    v
 Terminal / TTY (platform)                     reader thread feeds raw bytes to Processor
```

**Header inclusion rules:**
- `terminal/` headers MUST NOT include `Nexus.h` or any `nexus/` header.
- `Nexus.h` forward-declares `nexus::Daemon` and `nexus::Link`; includes `terminal/Session.h`.
- `nexus/` headers forward-declare `Nexus`; include `terminal/Session.h` only from `.cpp` files as needed.
- `Application` layer (`Main.cpp`, `MainComponent`, `terminal::Tabs`, `terminal::Panes`) includes all layers.

### Cross-Thread Data Contract (MANDATORY)

Lock-free architecture, unidirectional data flow. READER thread always writes to atomics; MESSAGE thread always reads from State. Two data profiles, two mechanisms, one direction:

- READER thread always reads/writes to atomics (raw value).
- MESSAGE thread always reads/writes to juce::ValueTree property/value.
- UNIDIRECTIONAL data flow. No hacks. No workaround. No manual state tracking. No shadow state.
- Objects are stateless — hold only transient values for calculation, never mutate state machine.
- Top to bottom, always tell never ask. Virtually no getters. EXCEPT SSOT reads from State machine.

**Scalar data** — parameters, mode flags, strings, metadata (cursor position, URIs, title, cwd). Sparse, low-volume, consumed by UI listeners.

```
READER → atomic slots on State → timer flush → ValueTree → MESSAGE reads
```

ValueTree is the SSOT for all scalar state. `State::flush()` copies dirty atomics to ValueTree properties. MESSAGE thread reads exclusively from ValueTree (directly or via `CachedValue` / `Value::Listener`). `StringSlot` pattern for cross-thread strings: intrinsic seqlock (`generation` atomic + `lastFlushedGeneration`), READER writes via `writeSlot()`, MESSAGE reads via `flushSlot()` (title, cwd, foregroundProcess → ValueTree properties) or `readSlot()` (URI table — polled per-frame, too numerous for ValueTree children).

**Bulk data** — cell content (25,000+ entries at 5K fullscreen, updated every frame). High-volume, consumed by render path.

```
READER → Video writes Buffer<Row> (owned by Video) → CellFifo two-ring transport (pushHistory / pushActive) → timer flush (screenDirty) → Display drains CellFifo into CodeModel → CodeView::calc() → repaint
```

`Video` owns `jam::Buffer<jam::Row>` — dual channel (normal + alternate), flat indexed. Processor owns `CellFifo` — two independent `jam::BufferSPSC` rings (history + active) with producer-side drop-oldest for lock-free cross-thread row delivery. History ring carries departed scrollback lines with continued-row join; active ring carries live viewport rows 1:1. Display (message thread) drains both rings into `jam::CodeModel` — history appended permanently, active rows laid down as the live tail (tracked by `liveTailExtent`, a per-screen drained count). `jam::Resizer` start trigger (owned by Session) calls `processor->suspendProcessing(true)` then `processor->prepare()` then `processor->suspendProcessing(false)`. `prepare()` resizes Video only — CellFifo and CodeView untouched. No dirty tracking on Buffer — render trigger is timer flush via screenDirty Parameter.

**Classification rule:** if the data is one-per-cell (O(rows × cols)), it is bulk → `jam::Buffer<jam::Row>` (owned by Video). If the data is sparse/scalar (O(1) or O(small N)), it is scalar → State ValueTree.

**Image preview** — file-based image display triggered by hyperlink click or SKiT protocol (Sixel/Kitty/iTerm2).

```
READER → Parser → Skit → onPreviewFile(filepath, row) → SpinLock slot on Display → MESSAGE thread → consumePendingPreview() → handleOpenImage() → terminal::Overlay component
```

Preview is a Display-side concern. The READER thread writes a filepath + trigger row into a SpinLock-guarded slot on Display via `onPreviewFile`. The MESSAGE thread consumes it via `consumePendingPreview()`, loads the file via `loadImageNative()`, downscales if needed, and creates an ephemeral `terminal::Overlay` child component. Overlay is a `jam::animation::Base` that renders `juce::Image` directly — no atlas, no FIFO, no staging pipeline. Display::resized() splits the content area: Overlay gets the right portion, TextEditor reflows into the remaining space via PTY resize. Dismiss destroys the Overlay and restores TextEditor to full width.

### Communication Contracts

**TTY -> Logic:**
- TTY reader thread calls `Processor::process(data, length)` → `Parser::process()` directly
- Video writes to Buffer<Row> and State atomics on reader thread
- No allocation, no locks on this path

**Logic -> Data:**
- Video calls Buffer<Row> write methods — cell writes, scroll, erase
- Video reads geometry (cols, visibleRows, scrollbackUsed) from `State` parameterMap atomics
- All calls are `noexcept`, reader thread safe

**Data -> Component (timer path):**
- `State::timerCallback()` runs on message thread (60-120Hz)
- Flushes atomics to ValueTree via `flush()`
- ValueTree fires `valueTreePropertyChanged` → Display::valueTreePropertyChanged reads screenDirty/activeScreen → drains CellFifo (history + active rings) into CodeModel → CodeView::calc() → repaint; cursor updates separately via setCaretPosition

**Data -> Component (render path):**
- Timer-driven flush (60/120 Hz) on the message thread flushes dirty atomics to ValueTree
- Display::valueTreePropertyChanged handles screenDirty/activeScreen: removes previous live tail (liveTailExtent[screen]), drains history ring into CodeModel (permanent append), drains active ring as new live tail (counting rows), stores drained count as liveTailExtent, calls CodeView::calc() → repaint
- Session owns jam::CodeModel (dimensionless multi-screen document) + jam::CodeView (textEditor, pure view) directly — no terminal::Screen class; CodeView renders from CodeModel via getWrappedLines at paint time; grafts only its own `state` node (selection, caret, viewport mode); no external node creation or ownership
- Display owns `jam::ComponentAttachment` for the DISPLAY node (Font::cellWidth/cellHeight/baseline/fontSize); reads config from AppState via listener, writes computed font metrics to session State via attachment

**Resize path:**
- Display::resized() → `session.getTextEditor().setBounds(contentBounds)` → TextEditor writes packed `id::viewport` to State via `updateWinsize()`
- Session::valueChanged (Value::Listener on winsize property) fires → calls `resizer->set(jam::ID::start, cols, rows)`
- Resizer start trigger → `processor->suspendProcessing(true)` → `processor->prepare(dims)` → `processor->suspendProcessing(false)` → alt screen capacity set to new viewport rows. `prepare()` resizes Video grid only (CellFifo and CodeView untouched — D12). SIGWINCH delivered after Resizer stop trigger.
- `suspendProcessing()` / `callbackLock` gate the reader thread during resize
- History is never touched during resize — re-wrapped at paint via `getWrappedLines` (D12). Live tail re-mirrored from Video's reflowed viewport on the next screenDirty tick. Cursor re-derived via `cursorClamp` in `Video::setWinsize`.

**Panes/Tabs -> Nexus (session lifecycle):**
- `terminal::Panes::createTerminal(cwd)` calls `Nexus::getContext()->create(cwd, uuid, cols, rows)` — mode-routing entry point
- `terminal::Tabs::closeSession(uuid)` calls `Nexus::getContext()->remove(uuid)`
- In client mode, `Nexus::create` fires child-added on `events`; `nexus::Link` observes and sends `createSession` PDU to daemon
- In daemon mode, `Nexus::create` fires child-added on `events`; `nexus::Daemon` observes and calls `wireSessionCallbacks(uuid, session)`

**nexus::Link -> Nexus (incoming PDU dispatch):**
- `Link::handleOutput(uuid, bytes)` → `Nexus::get(uuid).process(bytes, len)`
- `Link::handleLoading(uuid, bytes)` → `Nexus::get(uuid).process(bytes, len)` (initial snapshot)
- `Link::handleStateUpdate(uuid, cwd, fgProcess)` → `Nexus::get(uuid).getProcessor().getState()` ValueTree write
- `Link::handleSessionKilled(uuid)` → destroys local no-TTY session via `Nexus::remove(uuid)`
- `Link::handleSessions(uuids)` → creates no-TTY sessions for any UUIDs not yet present

**nexus::Daemon::Channel -> Daemon/Nexus (incoming PDU dispatch, daemon side):**
- `createSession` PDU → `Nexus::getContext()->create(cwd, uuid, cols, rows)` + `Daemon::attachSession(uuid, channel, sendHistory, cols, rows)`
- `input` PDU → `Nexus::get(uuid).sendInput(data, len)`
- `resizeSession` PDU → `Nexus::get(uuid).resize(cols, rows)`
- `killSession` PDU → `Nexus::get(uuid).stop()` + `Nexus::remove(uuid)`
- `detachSession` PDU → `Daemon::detachSession(uuid, channel)`

### Layer Violations (FORBIDDEN)

- Rendering must NEVER call Video or Buffer<Row> mutators
- TTY must NEVER call UI/Component code
- Video must NEVER allocate on reader thread
- GL thread must NEVER write to Buffer<Row> or State
- `terminal/` headers must NEVER include `Nexus.h` or any `nexus/` header

---

## Threading Model

### Threads

| Thread | QoS | Owns | Reads | Writes |
|--------|-----|------|-------|--------|
| **Reader** (TTY) | high | TTY fd | raw bytes | State atomics, Buffer<Row> writes, scrollbackUsed |
| **Timer** (JUCE) | default | — | `needsFlush` atomic | ValueTree properties |
| **Message** (main) | user-interactive | Component, CodeView | ValueTree, CodeModel | CodeModel mutations (Display drains CellFifo) |
| **GL** (OpenGL) | user-interactive | OpenGL context | — | background clear only (`renderOpenGL` calls `OpenGLHelpers::clear`). JUCE component paint routed through GL context. |

### Data Flow: Keystroke to Pixel

```
Keystroke -> Message Thread -> TTY::write()
         -> Reader Thread reads response -> Processor::process() -> Parser -> Video
         -> Buffer<Row> written, State atomics set
         -> Timer flush (60/120 Hz) on Message Thread -> State flushes dirty atomics to ValueTree
         -> Display::valueTreePropertyChanged() drains CellFifo (history + active rings) into CodeModel → CodeView::calc() -> repaint
         -> JUCE composites component paint through GL context when GPU renderer active.
            glyph::Graphics::pop() blits renderTarget juce::Image via g.drawImageAt() inside paint().
```

### Synchronization Primitives

| Mechanism | Between | Purpose |
|-----------|---------|---------|
| `std::atomic<float>` | Reader -> Timer/Message | State parameter transport |
| `std::atomic<bool> needsFlush` | Reader -> Timer | ValueTree flush trigger |
| `AppState::atlasDirty` (`Parameter<int>`, storeRelease/exchangeAcquire) | Message | Atlas rebuild signal on font/size change |

---

## Design Patterns in Use

### Pattern: APVTS-Style State

**Used for:** Cross-thread state synchronization without locks on the hot path.

**Implementation:** `terminal/State.h/cpp`, `terminal/StateFlush.cpp`

Reader thread writes to `std::atomic<float>` via `storeAndFlush()`. Timer polls `needsFlush` and copies atomics to ValueTree. UI reads from ValueTree listeners. Display drains CellFifo into CodeModel — CodeView renders from the CodeModel at paint time via `getWrappedLines`.

Viewport is stored as a packed `Bounds` integer in `id::viewport` on State. Cursor is stored as a packed `CursorState` integer in `id::cursor`. Both use the same atomic slot → ValueTree flush path.

### Pattern: AudioProcessor-Analogous Processor Lifecycle

**Used for:** Safe reader-thread suspension during resize.

**Implementation:** `terminal/Processor.h/cpp`, `terminal/Session.h/cpp`

Mirrors JUCE AudioProcessor: `suspendProcessing()` / `isSuspended()` = AudioProcessor suspend pattern; `getCallbackLock()` = critical section guarding the reader thread during resize. `prepare()` is the single resize API — resizes Video grid only (CellFifo and CodeView untouched). Called from Session's Resizer start/stop triggers while processing is suspended.

WINDOW-subtree properties `app::id::fontFamily` and `app::id::fontSize` drive font changes. A `ValueTree::Listener` on the WINDOW subtree detects changes, applies `fontFamily`/`fontSize` to `Typeface`, then calls `AppState::markAtlasDirty()`. `AppState::atlasDirty` is a `Parameter<int>` using `storeRelease`/`exchangeAcquire`. Consumer: message thread calls `consumeAtlasDirty()` to detect font/size changes before the next paint cycle, then calls `jam::Typeface::setAtlasSize()` to clear and rebuild the atlas.

`markAtlasDirty()` / `consumeAtlasDirty()` are the only public API. No caller reads `atlasDirty` directly.

### Pattern: Context<T> (Responsible Global)

**Used for:** Global access without Meyer's singleton.

**Implementation:** `lua::Engine` inherits `jam::Context<Engine>`, `Fonts.h` inherits `jam::Context<Fonts>`

lua::Engine lifetime owned by `ENDApplication` (Main.cpp). Fonts lifetime owned by `MainComponent`. Access via `lua::Engine::getContext()->` / `Fonts::getContext()->`. Fail-fast jassert if accessed before construction.

`jam::glyph::Atlas` is the CPU-side image store: owns `juce::Image mono` (SingleChannel) and `juce::Image emoji` (ARGB) plus an internal `Packer`. All MESSAGE THREAD. Accessed via `jam::Typeface::getAtlas()`. `Atlas::rebuild()` resets both images. `Atlas::Packer` holds the shelf packers and `LRUCache` instances, rasterizes glyphs via platform backends (CoreText/FreeType), and writes pixels directly to the atlas images.

### Pattern: Shelf-Based Atlas Packing

**Used for:** Packing variable-size glyphs into a fixed-size CPU image atlas (4096×4096 standard, 2048×2048 compact).

**Implementation:** `AtlasPacker.h`

Horizontal shelves, best-fit allocation. Separate packers for mono and emoji. LRU eviction when cache is full.

### Pattern: File Decomposition by Concern

**Used for:** Keeping files under 300 lines while maintaining logical cohesion.

Video.cpp -> VideoCSI, VideoDCS, VideoESC, VideoSGR, VideoEdit, VideoMode, VideoOps, VideoOSC, VideoOSCExt
State.cpp -> StateFlush
SixelDecoder.cpp -> SixelDecoderParse
KittyDecoder.cpp -> KittyDecoderDecode
LinkManager.cpp -> LinkManagerScan
Tabs.cpp -> TabsActions, TabsClose
ActionList.cpp -> ActionListBinding, ActionListSelection
lua/Engine.cpp -> EngineConfig, EngineDefaults, EngineParse, EngineParseConfig, EngineParseDisplay, EnginePatch

All split files define member functions of the parent class. No separate classes needed.

### Pattern: Config Distribution via AppState

**Used for:** Distributing config values (font, cursor, padding, scrollback) to terminal components without direct config reads.

**Implementation:** `AppState.h/cpp`, `MainComponent.cpp`, `Display.cpp`

Config values flow unidirectionally: `lua::Engine` → `AppState` → reactive listeners.

1. **Init:** `AppState` constructor reads `lua::Engine::getContext()` and seeds all config PARAMs (fontFamily, fontSize, cellWidth, lineHeight, cursorCodepoint, cursorStyle, cursorBlinkInterval, scrollbackLines, paddingTop/Right/Bottom/Left).
2. **Hot reload:** `MainComponent::applyConfig()` writes updated values to `AppState` via typed setters. No downstream cascade — listeners react automatically.
3. **Distribution:** `terminal::Display` and `whelmed::Component` register as `juce::ValueTree::Listener` on `AppState::getContext()->get()`. On any property change, they re-apply config to their owned components (TextEditor, Mouse, Input, attachment).
4. **Atomic reads:** `Processor` reads `scrollbackLines` directly from AppState's atomic Parameter via `getRawParameterValue<int>(app::id::scrollbackLines)->load()` — lock-free, any thread. No member variable caching.

**Guarantee:** AppState is constructed by `ENDApplication` before any Session or Display exists. Config values are always available when components initialize. No init sequence issues.

**SSOT contract:** Components never read `lua::Engine` for config values consumed by the render/UI path. `lua::Engine` is the config parser; `AppState` is the config SSOT for runtime.

---

## Split Pane System

### Architecture

Each tab owns a `terminal::Panes` component that manages split pane layout via a `PaneManager`. The layout is a binary tree stored as a JUCE `ValueTree`:

```
TAB
  PANES (direction="vertical"|"horizontal", ratio=0.5, x, y, width, height)
    PANE (uuid="abc")
      SESSION (uuid="abc", displayName, ...)          -- terminal pane
    PANE (uuid="def")
      SESSION (uuid="def", displayName, ...)          -- terminal pane with Whelmed open
      DOCUMENT (filePath, displayName, scrollOffset)  -- grafted when Whelmed opens; removed on close
```

- **Leaves** (`PANE` type) — each maps to one `PaneComponent` (`terminal::Display` or `whelmed::Component`)
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

Ratio is clamped to `[0.1, 0.9]`. Bounds are stored as `app::id::x/y/width/height` properties on split nodes. Resizer bar width is 4px.

### PaneResizerBar (Draggable Divider)

`jam::PaneResizerBar` is a `juce::Component` subclass that acts as a draggable divider between panes. Each split node in the tree has a paired resizer bar.

On drag: queries `PaneManager::getItemCurrentPosition()`, computes desired position from mouse delta, calls `setItemPosition()`. `hasBeenMoved()` triggers `parent->resized()` to re-layout.

Rendering delegated to `LookAndFeel::drawStretchableLayoutResizerBar()`. Configurable via `pane.bar_colour` and `pane.bar_highlight`.

### Panes (Per-Tab Container)

`terminal::Panes` is the per-tab component that bridges `PaneManager` with `terminal::Display` instances:

- Owns `Owner<PaneComponent> panes` and `Owner<PaneResizerBar> resizerBars`
- `createTerminal()` — adds a leaf to the tree, creates a `terminal::Display`, grafts SESSION
- `createWhelmed(file)` — overlays a `whelmed::Component` on the active terminal pane, grafts DOCUMENT
- `closeWhelmed()` — removes `whelmed::Component` and DOCUMENT, restores terminal visibility
- `closePane(uuid)` — ungrafts SESSION, removes pane, removes resizer bar, calls `paneManager.remove()`
- `splitHorizontal()` — left/right layout (calls `splitImpl("vertical", true)`)
- `splitVertical()` — top/bottom layout (calls `splitImpl("horizontal", false)`)
- `focusPane(deltaX, deltaY)` — spatial navigation by component bounds

**Split naming convention:** `splitHorizontal` produces a left/right layout. The internal direction string `"vertical"` describes the divider orientation, not the layout direction.

### Action Registry (action::Registry)

`action::Registry` inherits `jam::Context<Registry>` and `juce::Timer`. It is the single owner of all user-performable actions, replacing the former `KeyBinding`, `ModalKeyBinding`, and `ApplicationCommandTarget` system.

**Architecture:**
- Dynamic action table: built-in actions registered by `MainComponent`, popup + custom Lua actions registered by `lua::Engine`
- Hot-reloadable key map: `lua::Engine` parses `keys.lua`, `popups.lua`, and `actions.lua`, passes bindings to `action::Registry::buildKeyMap()`. File watcher auto-reloads on save.
- Prefix state machine: tmux-style two-keystroke input (prefix key + action key with timeout)
- Global singleton via `jam::Context<action::Registry>`

**Key resolution order in `handleKeyPress()`:**
1. If in **waiting** state: match by text character in modal bindings → execute → idle
2. If in **idle** state and key equals prefix key: enter **waiting** → start timer
3. If in **idle** state: match in global bindings → execute
4. No match → return false (caller forwards to PTY)

**Modal character matching:** Shifted characters (e.g. `?` = Shift+/) are matched by text character, not keycode+modifiers. Falls back to exact KeyPress match for non-character keys.

Built-in actions are registered in `MainComponentActions.cpp`. Popup and custom Lua actions are registered by `lua::Engine::registerActions()`. All keybindings (built-in + popup + custom) are defined in `~/.config/end/keys.lua`, `popups.lua`, and `actions.lua`, parsed by `lua::Engine`, which passes them to `action::Registry::buildKeyMap()`.

**Copy action special behavior:** If box selection is active, copies to clipboard and returns true (consumed). If no selection, returns false — key falls through to PTY as `\x03` (SIGINT).

Prefix key and timeout configurable via `keys.prefix` and `keys.prefix_timeout` in `keys.lua`.

### Close Cascade

Two entry points feed into the same cascade: explicit close action and shell exit.

**Explicit close:** `action::close_pane` callback calls `terminal::Panes::closePane(uuid)`.

**Shell exit:** State shellExited parameter fires via VT flush chain → `terminal::Panes::valueTreePropertyChanged` → `callAsync` → `Panes::closePane(uuid)`.

Cascade from `closePane()`:

1. `terminal::Panes::closePane()` ungrafts SESSION, destroys terminal, removes resizer bar
2. `PaneManager::remove()` collapses the parent split node, promotes the sibling
3. If last pane in tab: `terminal::Tabs::closeTab()` removes the tab
4. If last tab: application quits

---

## Working Directory Tracking

### AppState::pwdValue (Live CWD Binding)

`AppState` holds a `juce::Value pwdValue` member that tracks the active terminal's current working directory via `Value::referTo`.

**`setPwd(sessionTree)`** — Binds `pwdValue` to the `terminal::id::cwd` property of the given SESSION ValueTree:
```cpp
void setPwd (juce::ValueTree sessionTree);
// Calls: pwdValue.referTo (sessionTree.getPropertyAsValue (terminal::id::cwd, nullptr))
```

**`getPwd()`** — Returns `pwdValue.toString()`, falls back to `$HOME` if empty.

**Binding lifecycle:**
- `terminal::Tabs::globalFocusChanged()` calls `setPwd(term->getValueTree())` when focus moves to a terminal
- `terminal::Tabs::addNewTab()` calls `setPwd()` on the new terminal
- New splits inherit cwd: `terminal::Panes::splitImpl()` passes `AppState::getContext()->getPwd()` to `createTerminal()`
- New tabs inherit cwd: `terminal::Tabs::addNewTab()` passes `getPwd()` to `createTerminal()`

**Critical pattern:** `Value::referTo` must bind to a **stored** `juce::Value` member, not a temporary. `getPropertyAsValue()` returns a temporary — calling `referTo` on a temporary does nothing.

### Panes::createTerminal with Working Directory

```cpp
juce::String createTerminal (const juce::String& workingDirectory = {});
```

Passes `workingDirectory` through to `terminal::Display::create()`, which constructs the terminal with the given cwd. Default empty string = inherit from environment.

---

## Tab Name Management

### Value::Listener Pattern

`terminal::Tabs` uses `juce::Value::Listener` (not `ValueTree::Listener`) to track the active terminal's display name:

- **Member:** `juce::Value tabName` — bound via `referTo` to active terminal's `app::id::displayName`
- **Binding:** `globalFocusChanged()` rebinds `tabName` when focus changes
- **Update:** `valueChanged()` calls `setTabName()` on the tab bar

### displayName Computation (Processor::valueTreePropertyChanged)

`Processor::valueTreePropertyChanged` runs on the message thread. When `foregroundProcess` or `cwd` properties change on the State ValueTree, it recomputes `displayName` stored as `app::id::displayName` with priority:

1. **foregroundProcess** — when non-empty (non-shell process running)
2. **cwd leaf** — `juce::File(cwdPath).getFileName()` (e.g., "end" from "/Users/me/dev/end")

`title` (OSC 0/2) is NOT used in displayName computation — it's shell prompt noise that overrides everything else.

### SESSION Node Identification

SESSION nodes use `jam::ID::id` (not a terminal-specific UUID identifier). This makes SESSION nodes compatible with `jam::ValueTree::getChildWithID()` — a recursive lookup utility in the jam_data_structures module.

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
- Stack storage lives on `Processor`: `std::array<uint32_t, 2 * maxKeyboardStackDepth>` (flat) and `std::array<int, 2>` (stack sizes per screen). No HeapBlock.
- Keyboard flags are read directly from the screen ValueTree node via `jam::ValueTree::getValueFromChildWithID`.
- No `State::getKeyboardFlags()` method — keyboard flags are not accessed via State.

#### Flag-Aware Keyboard Encoding (Keyboard::map)

`Processor::encodeKeyPress()` reads flags from the screen ValueTree node and passes them to `Keyboard::map()`.

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

`lua::Engine::getContext()` locates config at `userApplicationDataDirectory` on Windows, `userHomeDirectory/.config/end/` on macOS/Linux. Creates directory and writes defaults if absent.

`window.state` persists WINDOW width/height only (standalone mode, cross-instance geometry). In daemon client mode the client reads and writes `nexus/<uuid>.display` (full WINDOW + TABS state) for session restore.

The daemon's TCP port is written to `nexus/<uuid>.nexus` (plain text) by `nexus::Daemon::start()` via `AppState::setPort()`. GUI clients read this file during startup to discover the port before beginning connect attempts. The NEXUS directory/file subtree is `nexus/`, regardless of the config key rename.

The config key controlling daemon mode is `lua::Engine::nexus.daemon` (`"daemon"` in end.lua). The ValueTree property is `app::id::daemonMode` on the WINDOW subtree.

---

## Key Data Types

### Cell (8 bytes, packed u64, trivially copyable)

```
| codepoint (21 bits) | contentTag (2 bits) | wide (2 bits) | styleId (16 bits) | padding (23 bits) |
```

Packed into a single u64. `jam::Cell` in `jam_graphics/detail/`. Global alias: `using cell = jam::Cell::Unit;`

Nested types:
- `Cell::Unit` — coordinate scalar
- `Cell::Point` — cell-space 2D coordinate
- `Cell::Rectangle` — cell-space rectangle; `fromPixel()` / `toPixel()` convert between pixel and cell coordinates using explicit cellWidth/cellHeight. pack() to int64, unpack() from int64.
- `Cell::RowState` — per-row metadata
- `Cell::getKey()` — extract cache key from packed cell

### Color (4 bytes, trivially copyable)

```
| red/paletteIndex (1B) | green (1B) | blue (1B) | mode (1B) |
```

Mode: theme (0), palette (1), rgb (2)
Access: `setRGB()`, `setPalette()`, `setTheme()`, `paletteIndex()`

### Buffer<Row> Ring Buffer

Dual channels (normal + alternate). `jam::Buffer<jam::Row>` with ring-buffer row indexing via per-channel `head` positions. `head` tracks the logical top row per channel. No dirty tracking on Buffer — no `dirtyRows` bitmask. No `linkIds` sidecar.

**Owned by `Video`** (dual channel: normal + alternate). Video writes cells to Buffer<Row> via Block views. No atomic pointer exchange — Video owns the buffer directly. Processor reads Buffer<Row> on the message thread for live zone overwrite (tearing-tolerant, corrected on next tick).

Buffer API: `Block<Row>` constructor from Buffer returns a non-owning view with no copy. `getWritePointer()` returns a mutable `jam::Row*` for the reader thread; cells accessed via `row->cells[col]`. Ring addressing uses `% numRows` (any size, no power-of-two). Head preserved on resize. Resize is managed by `Session::resizer` (`jam::Resizer`). Lossless content preservation across column changes copies rows in ring order (oldest first).

Video reads geometry via `state.getRawValue<int>(terminal::id::cols)` etc. (lock-free atomics).

### terminal::Map

`terminal::Map` provides typed map instances via `jam::Map::Bool`, `jam::Map::Screen`, and `jam::Map::Gpu`. The `Map::Screen` instance provides the `normal`/`alternate` index mapping for Buffer<Row> channel access. Lives in `terminal/Map.h`.

### GlyphConstraint

Per-codepoint scaling and alignment descriptor for Nerd Font icons. Applied at rasterization time in `jam::glyph::Atlas::Packer`.

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

Anchor + end `Point<int>` pair. Uses `::SelectionType` (none/visual/visualLine/visualBlock). `containsCell()` dispatches to `contains()`, `containsLine()`, or `containsBox()`. Renders via transparent background overlay using `colours.selection` config color. Lives in `terminal/component/ScreenSelection.h`.

### CodeView (jam::CodeView)

Pure monospace view rendering an externally-owned `jam::CodeModel`. Owned directly by Session as `jam::CodeView textEditor` — no terminal::Screen wrapper class. Holds `const CodeModel&` — never mutates the document. Content rendered at paint time via wrap-aware projection walk over CodeModel lines (`getWrappedLines`). Single viewport mode: `juce::Viewport` with vertical scrollbar.

Properties accessed via static array + enum (`CodeView::properties`, `CodeView::PropertyIndex`). Selection is CodeView's responsibility. Input/Mouse write selection properties directly to CodeView's grafted node.

CodeView grafts only its own `state` node (selection, caret, viewport mode). Node creation belongs to CodeView itself — no external node seeding. `calc()` is public — Display calls it after draining CellFifo into CodeModel. `setViewportLineCount(int)` sets the number of document lines mapped to the terminal viewport for correct caret positioning (D15).

### ModalType

`ModalType` is an **app-level** enum stored as an integer property on the **TABS** subtree via `AppState::setModalType()` / `getModalType()`. Both `terminal::Display` and `whelmed::Component` read it to gate their key dispatch. `ModalType::none` means no modal is active.

```cpp
enum class ModalType : uint8_t { none, selection, openFile };
```

`terminal::State` also mirrors the active modal via its own atomic (for the render path). When non-none, ALL keys are intercepted by the active pane's key handler before the Action system or PTY.

**terminal::Display key dispatch chain:**

```
keyPressed()
    |
    +-- mouse copy shortcut? → copySelection()
    +-- terminal::Input::handleKey()
            +-- isPopupTerminal? → session.handleKeyPress()
            +-- State::isModal()? → handleModalKey()
            |       +-- selection → handleSelectionKey()
            |       +-- openFile  → handleOpenFileKey()
            +-- action::Registry::handleKeyPress()    (prefix state machine + global bindings)
            +-- isScrollNav? → handleScrollNav()
            +-- clearSelectionAndScroll() + session.handleKeyPress()
```

**whelmed::Component key dispatch chain:**

```
keyPressed()
    |
    +-- whelmed::InputHandler::handleKey()
            +-- modal == selection? → handleCursorMovement() + handleSelectionToggle()
            +-- mouse selection + copy key? → clipboard copy
            +-- action::Registry::handleKeyPress()
            +-- handleNavigation()
```

### PaneComponent

Pure virtual base (`terminal/component/PaneComponent.h`) shared between `terminal::Display` and `whelmed::Component`. Inherits `juce::Component`.

**Contract (all methods pure virtual unless noted):**

| Method | Description |
|--------|-------------|
| `getPaneType()` | Returns `app::id::paneTypeTerminal` or `app::id::paneTypeDocument` |
| `switchRenderer(type)` | Switches CPU/GPU backend at runtime |
| `getValueTree()` | Returns root ValueTree (SESSION or DOCUMENT) for grafting |
| `enterSelectionMode()` | Enters vim-style keyboard selection mode |
| `copySelection()` | Copies active selection to clipboard and clears it |
| `hasSelection()` | Returns true if a non-degenerate selection is active |
| `focusGained()` | (non-virtual) Sets activePaneID and activePaneType in AppState |
| `applyZoom(float zoom)` | Applies the given zoom factor to the pane's rendering |
| `onRepaintNeeded` | (non-virtual) Callback invoked after rendering to trigger a repaint |

### StatusBarOverlay

`StatusBarOverlay` is a `juce::Component` and `juce::ValueTree::Listener` that listens to the **TABS** subtree for `app::id::modalType` and `app::id::selectionType` property changes. Displays the active modal mode name (VISUAL / VISUAL LINE / VISUAL BLOCK) as a status bar.

### Cursor (Whelmed)

`Cursor` (`Source/Cursor.h/cpp`) is a shared descriptor for the Whelmed selection cursor. It stores pixel bounds, blink state, and block/character position. `whelmed::Screen::updateCursor()` builds and stores the cursor for the current selection position; `Screen::paint()` renders it.

### Whelmed

`whelmed::Component` is a `PaneComponent` subclass that hosts the markdown viewer. It owns:

- **State** — document ValueTree, atomic block count, parse-complete flag
- **Parser** — background markdown parse thread
- **Screen** — block renderer (owns `Block` instances, handles mouse selection)
- **InputHandler** — modal key dispatch, selection keys, scroll nav
- **LoaderOverlay** — shown during async parse

**Block hierarchy (`whelmed::Block`):**

```
Block (pure virtual)
  TextBlock     — juce::AttributedString + TextLayout; covers headings, paragraphs, list items, inline code
  MermaidBlock  — SVG-rendered diagram via jam_gui/opengl path tessellation
  TableBlock    — tabular layout
```

Blocks are not `juce::Component` instances — they are data objects that measure and paint themselves into a `juce::Graphics` context. Screen owns them in a flat `std::vector<BlockEntry>` and lays them out vertically.

**Selection architecture:** Mouse events on Screen write anchor/cursor coordinates to the DOCUMENT ValueTree (`app::id::selAnchorBlock`, `selCursorBlock`, etc.). `whelmed::InputHandler` reads these same properties to perform keyboard navigation and copy operations. `AppState::selectionType` and `modalType` on TABS are the SSOT for selection state visible to the status bar.

### MessageOverlay

Transient overlay component. Shows grid dimensions (columns x rows) during resize. Accepts arbitrary string messages on demand. Fade-in/out animation driven by `jam::Animator`. Replaces the earlier GridSizeOverlay.

### Atlas::Region

Cached rasterization result stored in the LRU map. Contains: texture UV rect, pixel dimensions, and bearing (horizontal + vertical offset from cell origin).

### LRUCache

Frame-stamped LRU map. Each entry records the last frame it was accessed. When capacity is exceeded, the oldest 10% of entries are evicted and their atlas regions returned to the packer.

Capacities: mono 19,000 glyphs; emoji 4,000 glyphs.

---

## Key Design Decisions

### Decision: Context<T> over Meyer's Singleton for lua::Engine

**Context:** Config was a Meyer's singleton (`static Config& get()`). This caused static initialization order issues and hid lifetime management.

**Decision:** lua::Engine inherits `jam::Context<Engine>`. Owned by `ENDApplication`. Accessed via `lua::Engine::getContext()->`.

**Rationale:** Explicit lifetime, fail-fast on misuse, no static init ordering problems.

### Decision: Packed 8-byte Cell over HotCell/ColdCell Split

**Context:** SPEC originally proposed 8B HotCell + 20B ColdCell SoA layout for cache optimization. Interim implementation used a 16-byte unified Cell with inline fg/bg Color.

**Decision:** Packed 8-byte u64 Cell (`jam::Cell`) — codepoint(21) + contentTag(2) + wide(2) + styleId(16) + padding(23). Grapheme stored separately in a sparse map. Style and color resolved via styleId lookup.

**Rationale:** 8 bytes fits 8 cells per cache line. Packing eliminates padding waste. styleId indirection keeps the hot path narrow. 95%+ of cells have no grapheme, so the sparse map handles the rare case efficiently.

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

**Decision:** TTY reader thread calls `Processor::process()` → `Parser::process()` directly. Video writes to Buffer<Row> and State atomics on the reader thread.

**Rationale:** Simpler, lower latency. The FIFO added a drain step on the message thread that was unnecessary — the parser is fast enough to run on the reader thread without blocking.

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

**Decision:** Generated constraint table (10,470 codepoints, 88 switch arms) from NF patcher v3.4.0 data. Applied at rasterization time in `jam::glyph::Atlas::Packer`.

**Rationale:** Matches NF patcher's own scaling logic. Icons render identically to how they appear in patched fonts, but with runtime flexibility for any cell size.

### Decision: Shared Typeface over Per-Terminal Font Ownership

**Context:** Each `terminal::Screen` owned its own `Fonts` instance. When closing tabs rapidly, the GL thread could access a destroyed font.

**Decision:** `jam::Typeface` is the global font owner. All terminals access the typeface and atlas via static methods (`jam::Typeface::findTypeface()`, `jam::Typeface::getAtlas()`). Font lifetime exceeds all terminal components.

### Decision: Binary Tree ValueTree for Split Pane Layout

**Context:** Split panes needed a recursive layout model that integrates with JUCE's ValueTree state management.

**Decision:** Binary tree where internal nodes are `PANES` (direction + ratio) and leaves are `PANE` (uuid). Tree stored as a JUCE ValueTree, enabling state persistence and listener-based reactivity.

**Rationale:** ValueTree provides free serialization, undo/redo capability, and listener notifications. Binary tree naturally models recursive splits. Each split operation wraps the target leaf in a new internal node with a sibling — O(1) split and remove.

### Decision: Prefix Key over Chord Modifiers for Pane Actions

**Context:** Pane navigation and splitting needed keyboard shortcuts. Options: Cmd+Shift chords, or tmux-style prefix key.

**Decision:** Prefix key system (`action::Registry`). Default prefix: backtick. Action keys: h/j/k/l for navigation, `\`/`-` for splitting.

**Rationale:** Chord modifiers (Cmd+Shift+H/J/K/L) conflict with terminal applications that use these sequences. Prefix key avoids conflicts entirely — the prefix is consumed, then the next key is unambiguously a pane action. Familiar to tmux users. Fully configurable.

### Decision: SESSION Grafted as Child of PANE

**Context:** Terminal state (SESSION ValueTree) needs to be associated with a specific pane in the split tree.

**Decision:** SESSION is grafted as a child of its PANE node in the ValueTree hierarchy. `terminal::Panes` manages the grafting at terminal creation time and ungrafts before tree restructuring.

**Rationale:** Co-locating SESSION under PANE enables future state persistence of the full split layout + terminal state in a single ValueTree. Ungrafting before `PaneManager::remove()` prevents re-parenting asserts when the tree restructures.

### Decision: Overlay as jam::animation::Base, No FIFO

**Context:** Previous image subsystem used a READER FIFO and MESSAGE drain pipeline. Buffer<Row> is pure text — images extracted.

**Decision:** `terminal::Overlay` inherits `jam::animation::Base` (`juce::Component + juce::Timer`). Owns `juce::Image` (static) or `std::vector<juce::Image>` (animated frames with `std::vector<int>` delays). Renders via standard `paint()`. No FIFO, no staging, no GL shaders. Display::resized() allocates side-by-side bounds; TextEditor reflows via PTY resize.

**Rationale:** One image at a time needs no pipeline. Standard `paint()` composited by JUCE is sufficient. Matches the Display→TextEditor ownership pattern.

### Decision: Nexus Mode via setMode() Enum, not attach() Overloads

**Context:** Previous design used `attach(Daemon&)` / `attach(Link&)` overloads to determine Nexus mode, storing pointers to the IPC objects.

**Decision:** `Nexus::setMode(Mode)` sets an enum. `nexus::Daemon` and `nexus::Link` register directly on `Nexus::events` ValueTree as `juce::ValueTree::Listener`. Nexus stores no pointers to IPC objects.

**Rationale:** Nexus has no IPC knowledge. Decoupling via the events ValueTree follows the APVTS observer pattern already used throughout the codebase. Daemon/Link lifecycle is independently managed by ENDApplication.

### Decision: Video replaces Parser as VT Command Processor

**Context:** The old `Parser` class conflated byte-stream decoding and VT command execution (cursor moves, grid writes, mode changes). The name was overloaded — it implied both parsing and interpretation.

**Decision:** `Parser` is the byte-stream state machine. `Video` is the VT command processor that receives decoded semantic actions from Parser and translates them into Buffer<Row> mutations and State writes. `Processor` is the pipeline orchestrator owning both.

**Rationale:** Clear single-responsibility boundary. Parser does syntax; Video does semantics. Processor routes. Matches the existing naming pattern (Session is the data source and Buffer<Row> owner, Processor is the pipeline, State is the parameter SSOT).

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

`GlyphConstraintTable.cpp` is a generated file. It maps NF icon codepoints to `GlyphConstraint` descriptors that replicate the scaling decisions made by the NF patcher. `jam::glyph::Atlas::Packer` applies the constraint before writing pixels to the atlas, so icons are positioned and scaled identically to how they appear in a patched font — but at any runtime cell size.

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

### jam::Font — Application-Level Font Specification

`jam::Font` is a value type carrying the application-level font specification: family name, point size, and style. It is the unit of font identity at the call site — passed to `setFont()` on renderers and used to resolve the underlying `jam::Typeface` handle.

`jam::Font` does not own any platform resource. It is trivially copyable and comparable. Font selection (which typeface to use) happens outside the `jam_graphics/fonts/` module — callers construct a `jam::Font` with the desired family/size/style and hand it to the renderer.

### jam::Typeface — Platform Font Handle Manager

`jam::Typeface` owns the platform font handles (FreeType `FT_Face` / CoreText `CTFontRef`), HarfBuzz shaping fonts, and the shaping buffers. It is the single owner of all per-font-size resources.

`jam::Typeface` has no font selection logic — it does not implement a registry or choose fonts. The caller loads a `Typeface` explicitly with the desired font data. Font selection was removed along with `Typeface::Registry`.

**Fallback fonts:** `addFallbackFont` registers additional typefaces (e.g. Nerd Font for PUA icon glyphs, Display Mono for supplementary coverage) that are tried in order when the primary face returns `.notdef`. This mechanism covers NF icons and Display Mono PUA glyphs without involving any registry or system font manager for the primary code path.

### jam::glyph::Atlas::Packer — Atlas Rasterization

`jam::glyph::Atlas::Packer` owns atlas rasterization. `Typeface` shapes text; `Packer` rasterizes glyphs and writes pixel data directly into the `Atlas` images via `Atlas::writePixels()`. `Packer` exposes `getOrRasterize()` — callers pass a `glyph::Atlas::Key` and receive an `Atlas::Region` (UV rect + bearing).

### Font Ownership

`jam::Typeface` is the global font owner. All terminals share the same typeface via `jam::Typeface::findTypeface()` and atlas via `jam::Typeface::getAtlas()`. This ensures font lifetime exceeds all terminal components.

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

## Component Extraction (terminal::Display)

`terminal::Display` delegates to three focused handlers:

| Class | File | Responsibility |
|-------|------|----------------|
| terminal::Input | terminal/Input.h/cpp | Modal gate, selection keys, open-file keys, scroll nav |
| terminal::Mouse | terminal/Mouse.h/cpp | PTY forwarding, drag selection, click dispatch, wheel scroll. Converts visible row to absolute via `toAbsoluteRow()` — **stub**: currently returns visibleRow unchanged, scrollback-aware conversion pending |
| terminal::LinkManager | terminal/LinkManager.h/cpp | Viewport scan, cell-native hyperlink scanning, hit-test, dispatch |

Selection state lives on CodeView's grafted ValueTree node. Input and Mouse write directly to CodeView's node via `jam::ID::` identifiers. ScreenSelection reads from CodeView's node.

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
| AppState | Application-level config SSOT — font, cursor, padding, scrollback seeded from lua::Engine at init + hot reload. ValueTree root; tracks active terminal UUID, pwd via Value::referTo. Components listen and react — no manual config cascade. |
| AtlasGlyph | See Atlas::Region. |
| BoxDrawing | Procedural rasterizer for box drawing, block elements, and braille — no font lookup |
| BoxSelection | Rectangle selection: anchor + end cell coordinates, rendered as overlay |
| ConPTY sideload | Runtime extraction of conpty.dll + OpenConsole.exe from BinaryData to ~/.config/end/conpty/ (all Windows versions — inbox ConPTY broken on both Win10 and Win11) |
| getActiveScreen | Message-thread ValueTree reader for active screen state (normal/alternate) |
| Cell | `jam::Cell` in `jam_graphics/detail/` — 8-byte packed u64 representing one terminal character position. Nested: `Cell::Unit` (coordinate scalar), `Cell::Point`, `Cell::Rectangle`, `Cell::RowState`, `Cell::getKey()`. Global alias: `using cell = jam::Cell::Unit;` |
| Row | `jam::Row` — FAM struct packing per-row metadata (`usedCols`, `flags`) and a C99 `Cell cells[]` flexible array into a single allocation. `FlexType = Cell` tells `Buffer<Row>` to compute stride as `sizeof(Row) + alignedCols * sizeof(Cell)`. Flags: `wrapped` (soft wrap at right margin, set by Video::resolveWrapPending()), `dead` (reflow tombstone), `justify { 1 << 2 }` (FLEX_GAP justification marker). |
| displayName | Derived tab label from `app::id::displayName`. Terminal panes: foregroundProcess (non-empty) > cwd leaf name — computed in `Processor::valueTreePropertyChanged`. Whelmed panes: file basename set at openFile time. |
| Embolden | Platform stroke-widening for bold: kCGTextFillStroke (macOS), FT_Outline_Embolden (Linux) |
| FontCollection | Flat int8_t[0x110000] codepoint-to-font-slot dispatch table, O(1) lookup |
| GlyphConstraint | Per-codepoint NF icon scaling/alignment descriptor applied at rasterization time |
| Grapheme | Multi-codepoint character cluster (e.g., flag emoji, combining marks) |
| Buffer<Row> | `jam::Buffer<jam::Row>` — ring-buffer storage for terminal cells, dual-channel (normal/alternate). Owned by `Video` (single `jam::Buffer<jam::Row> buffer` member with dual-channel access via `Map::Screen`). Ring addressing `% numRows` (any size). Head preserved on resize. Pure text — no image flags |
| jam::Resizer | Timer-driven resize coordinator (in jam_core): coalesces rapid dimension changes via 16ms timer. `jam::Resizer` owned by Session as `resizer`. Start trigger: suspend → `processor->prepare()` → resume → alt screen capacity = new viewport rows. Stop trigger: `processor->prepare()` (resizes Video grid only). History untouched on resize (D12). |
| jam::BufferSPSC | Index-only fork of `juce::AbstractFifo` (same interface) with producer-side drop-oldest. Lives in `jam_core/concurrency/`. Holds NO storage — caller (CellFifo) owns the byte buffer and seqlock guard. ONE behavioral change vs AbstractFifo: `prepareToWrite` on full evicts oldest via single CAS on `validStart`. |
| CellFifo | Two independent `jam::BufferSPSC` SPSC rings (history + active) for lock-free cross-thread cell row delivery. Owns `HeapBlock<uint64_t>` storage + per-slot seqlock epoch arrays. History ring: continued-row join (`pending` accumulator). Active ring: one row per entry, no join. `pushHistory`/`pushActive` (reader thread), `drainHistory`/`drainActive` (message thread). Drop-oldest under flood evicts the oldest ring entry — lossless w.r.t. retained scrollback. |
| liveTailExtent | Display-private `std::array<int, Map::Screen::count>` — per-screen count of document tail lines laid down as the live region last tick. Set to the drained active-row count each tick. NOT a State param, NOT shadow state — Display's private record of its own document mutation. Used to remove the previous live tail before appending the new one. |
| CodeLine | `jam::CodeLine` — one logical line of terminal output: `juce::HeapBlock<jam::Char> cells` + `int cellCount`. Width-free — `getWrappedLines(viewWidth)` computes wrap at projection time. Lives in `jam_graphics/detail/`. |
| CodeModel | `jam::CodeModel` — dimensionless multi-screen document model. Owns `jam::Owner<Screen>` (one per logical surface). Each screen is a bounded deque of `CodeLine` with FIFO scrollback eviction. Active-screen selector (`setActive`/`getActive`). Terminal uses 2 screens (normal + alternate); plain editor uses 1. Width enters once at paint via `CodeLine::getWrappedLines`. Lives in `jam_graphics/detail/`. |
| CodeView | `jam::CodeView` — pure view rendering a const-referenced `jam::CodeModel`. Wrap-aware projection walk at paint time shapes only the visible line sub-range. Lives in `jam_gui/code_editor/`. |
| History | Removed. Byte replay deferred — DEBT-20260526T220000. |
| Overlay | `jam::animation::Base` child of `terminal::Display`; ephemeral image preview. `jam::animation::Base` is `juce::Component + juce::Timer`. Owns `juce::Image` (static) or `std::vector<juce::Image>` frames. Renders via standard `paint()`. Created on demand by Display, destroyed by `dismissPreview()`. Side-by-side with TextEditor in Display::resized() |
| handleSkitFilepath | Shared parser helper for SKiT (Sixel/Kitty/iTerm2) file preview protocol. Extracts filepath from `END;` marker, calls `onPreviewFile` callback |
| CursorState | Packed struct for per-screen cursor save/restore. Carries cursor position, pen attributes, and origin mode. Used by Video via `setCursor(CursorState)` / `getCursor()` for DEC save/restore and screen switch. |
| Font::cellWidth/cellHeight | Plain int members on `jam::Font` — cell pixel dimensions (max advance * cellWidthMultiplier, line height * lineHeightMultiplier). Stored on the DISPLAY node via `jam::ComponentAttachment`. Source of truth for cell pixel dimensions; `Cell::Rectangle::fromPixel()` / `Cell::Point::fromPixel()` use these for pixel-to-cell conversion. |
| LRUCache | `jam::glyph::LRUCache` — frame-stamped LRU map inside `Atlas::Packer`; evicts oldest 10% when over capacity. Capacities: mono 19,000; emoji 4,000. MESSAGE THREAD only. |
| Atlas::Region | `jam::glyph::Atlas::Region` — cached rasterization result: `textureCoordinates` (UV rect), `widthPixels`, `heightPixels`, `bearingX`, `bearingY`, `type` (mono/emoji). |
| LookAndFeel | Custom JUCE LookAndFeel: tab line indicator, popup menu glass blur, colour system via lua::Engine |
| MessageOverlay | Transient overlay for status messages (reload confirmation, config errors) |
| Nexus | Session container (global scope): action table + session map + Mode enum + events ValueTree, Context-managed, owned by ENDApplication |
| action::Registry | Unified action registry: action table + key map + prefix state machine, Context-managed, owned by MainComponent |
| action::List | Command palette component: jam::Window with fuzzy-searchable list of all registered actions |
| PaneManager | Binary tree ValueTree layout engine for recursive split pane layout |
| PaneResizerBar | Draggable divider bar between split panes, paired with split tree nodes |
| Panes | `terminal::Panes` — per-tab component owning `terminal::Display` instances and managing split layout via PaneManager |
| Pen | Current text attributes (style + fg/bg color) applied to new cells |
| Processor | Pipeline orchestrator: owns Parser, Video, TTY (created by startTTY()), and CellFifo (two-ring SPSC transport); references State received from Session. Reader-thread event handlers push departed rows via `pushHistory` and live viewport rows via `pushActive`. Display drains via `drainHistory`/`drainActive` on the message thread. Exposes `suspendProcessing()`, `isSuspended()`, `getCallbackLock()`. `prepare()` is the single resize API — resizes Video grid only (CellFifo and CodeView untouched). jam::Resizer owned by Session. |
| pwdValue | juce::Value in AppState bound via referTo to active terminal's cwd property |
| Map::Screen | `jam::Map::Screen` — normal/alternate channel index map instance in `terminal::Map`; used for Buffer<Row> channel access. See also `Map::Bool` and `Map::Gpu`. Lives in `terminal/Map.h`. |
| ScreenSelection | Anchor + end Point<int> pair for text selection; contains() for hit testing |
| Skit | SKiT unified entry point for Sixel/Kitty/iTerm2 inline image preview protocol |
| Snapshot | Superseded. History is now dimensionless in CodeModel — resize re-wraps at paint via `getWrappedLines`, no content copy needed (D12). |
| State | APVTS-style atomic + ValueTree bridge for cross-thread terminal state. Includes: OSC 133 shell integration tracking, paste echo gate, sync output (mode 2026), preview split-viewport, hints, modal type, snapshot dirty signal. Per-screen methods removed — callers use `storeValue`/`loadValue` (READER) or VT API (MESSAGE). |
| Tabs | `terminal::Tabs` — TabbedComponent subclass; Value::Listener for tabName, manages Panes instances |
| VBlank | Not currently implemented. Render trigger is timer-driven flush (60/120 Hz). |
| Video | VT command processor: receives decoded semantic actions from Parser, writes Buffer<Row>, fires events for State writes |
| Atlas | `jam::glyph::Atlas` — CPU-side image store: `juce::Image mono` + `juce::Image emoji` + `Packer`. Accessed via `jam::Typeface::getAtlas()`. All MESSAGE THREAD. |

---

*This document reflects the codebase as implemented. If code diverges, update this document.*
