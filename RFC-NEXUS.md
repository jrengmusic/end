# RFC — Nexus: Processor/Display Architecture
Date: 2026-04-07
Status: Ready for COUNSELOR handoff

## Problem Statement

With `nexus = true`, END shows a blank window. Without nexus, terminal works. The current implementation was built from poisoned context (wezterm/tmux hand-rolled IPC patterns) layered onto JUCE IPC modules. The result fights JUCE's threading model, leaks connection objects, has unsafe lifetime management, and never delivers terminal content to the client Display.

## Research Summary

### JUCE IPC API audit (from source)

**`InterprocessConnectionServer`** — creates `InterprocessConnection*` via virtual `createConnectionObject()`. JUCE does NOT own the returned pointer. Both working examples (juce_IPC_example.cpp, juce_rpc_example/) keep ownership in their own container. Our Server does not — every connection is leaked.

**`InterprocessConnection`** constructor parameter `callbacksOnMessageThread`:
- `true` (default): JUCE delivers `connectionMade()`, `connectionLost()`, `messageReceived()` on the message thread via internal `SafeAction` (shared_ptr + bool flag). Lifetime-safe — if `disconnect()` is called, pending callbacks are silently dropped.
- `false`: Callbacks fire on JUCE's internal reader thread. No safety net. Caller must handle threading.

Our ServerConnection uses `true`. Our Client uses `false` — then manually dispatches via `callAsync([this, ...])`, capturing raw `this`. This bypasses JUCE's SafeAction protection entirely.

**`ChildProcessCoordinator/Worker`** — JUCE's built-in sidecar IPC. 1:1 relationship. Coordinator owns worker lifetime — killing coordinator kills worker. We need the opposite (GUI dies, sidecar survives). Does not fit.

**`sendMessage()` thread safety** — `writeData()` takes a `ScopedReadLock` (not write lock). Concurrent `sendMessage()` calls from different threads can interleave bytes on the wire, corrupting JUCE's 8-byte framing header. All sends must be serialized.

**`ChangeBroadcaster`** — JUCE's sanctioned Processor→Editor communication pattern. Not polling. Internally: `sendChangeMessage()` → `triggerAsyncUpdate()` → atomic CAS → OS message queue → `handleAsyncUpdate()` → `callListeners()`. Zero polling. OS wakes the message thread. From JUCE `AudioProcessor::createEditor()` docs:

> "The correct way to handle the connection between an editor component and its processor is to use something like a **ChangeBroadcaster** so that the editor can register itself as a listener, and be told when a change occurs."

### The JUCE Processor/Editor contract (from source)

1. **Processor does not own Editor.** Processor holds a raw non-owning `activeEditor` pointer.
2. **Processor creates Editor** via `createEditor()` — virtual, returns raw pointer. Caller takes ownership.
3. **Editor holds `Processor&` reference** — Editor reaches UP to Processor. Processor doesn't reach down.
4. **Editor notifies Processor on death** — `editorBeingDeleted()` clears the non-owning pointer.
5. **Communication is ChangeBroadcaster/ChangeListener** — Processor broadcasts, Editor listens. Event-driven.
6. **APVTS is the bridge** — `ValueTree` + atomic parameters + Listener. Processor writes, listeners fire, Editor updates.

### Bugs found in current implementation (7 total)

**Bug 1 — Server doesn't own connections (BLESSED B violation).**
`Server::createConnectionObject()` returns `new ServerConnection(host)` — raw pointer, no owner. JUCE doesn't store it. Memory leak.

**Bug 2 — Client bypasses JUCE SafeAction (BLESSED B violation).**
`Client` constructed with `callbacksOnMessageThread = false`. All dispatch in `messageReceived` uses `callAsync([this, ...])` with raw `this` capture. Dangling pointer if Client is destroyed before async fires.

**Bug 3 — PTY open deferred past initial delta request.**
`Session::resized()` defers PTY open via `callAsync()`. When daemon processes `spawnSession` → `attachSession` → `getRenderDelta(0)` sequentially, all three execute before the callAsync fires. `getRenderDelta(0)` reads an empty grid. First response is always blank.

**Bug 4 — No InputHandler in client-mode Display.**
Client-mode constructor skips `initialise()`. No `inputHandler` created. Keyboard input never reaches PTY.

**Bug 5 — No MouseHandler in client-mode Display.**
`switchRenderer()` skips `mouseHandler` when `remoteSession != nullptr`. No mouse events work.

**Bug 6 — Missing attach/register in splitImpl.**
`Panes::splitImpl()` client path omits `attachSession` and `registerRemoteSession`. Delta responses can't be routed.

**Bug 7 — applyDelta doesn't copy graphemes.**
`CellRange::graphemes` is serialized/deserialized but `RemoteSession::applyDelta()` only memcpy's cells, never graphemes.

## Naming

Current `Terminal::Session` is a single PTY instance (the Processor analog). "Session" should refer to the workspace — matching tmux terminology.

| Current | New | What it is |
|---|---|---|
| `Terminal::Session` | `Terminal::Processor` | One PTY + Grid + Parser + State. AudioProcessor analog. |
| `Terminal::Display` | `Terminal::Display` | Ephemeral UI. AudioProcessorEditor analog. Unchanged. |
| `Nexus::Host` | `Nexus::Session` | Collection of Processors. 1:1 with sidecar. The tmux "session". |
| `Nexus::RemoteSession` | Absorbed into `Terminal::Processor` hosted mode | Shadow Grid+State responsibilities move inside Processor. Class eliminated. |

File renames:

| Current | New |
|---|---|
| `Source/terminal/logic/Session.h/cpp` | `Source/terminal/logic/Processor.h/cpp` |
| `Source/nexus/Host.h/cpp` | `Source/nexus/Session.h/cpp` |
| `Source/nexus/HostFanout.cpp` | `Source/nexus/SessionFanout.cpp` |
| `Source/nexus/RemoteSession.h/cpp` | Deleted (absorbed into Processor) |

## UX Model (tmux analog)

| tmux | END |
|---|---|
| `tmux` session | `Nexus::Session` — one sidecar process, one workspace |
| `tmux` pane | `Terminal::Processor` — one PTY process |
| `tmux` pane UI | `Terminal::Display` — ephemeral JUCE component |
| `exit` in shell | Kills Processor → pane closes → if last in tab → tab closes → if last tab → window + sidecar die |
| Close terminal app | Window destroyed. Sidecar + all Processors survive. |
| Relaunch terminal app | Reconnect to sidecar. Reattach Displays to surviving Processors. |
| Cmd+W on pane | Same as `exit` — kills that Processor on sidecar, removes pane |

**Exit hierarchy:**
1. Cmd+W on active pane (or `exit` in shell) → kill that Processor on sidecar → remove pane
2. Last pane in tab → tab removed
3. Last tab, last pane, last Processor → window closes → sidecar has 0 Processors → sidecar exits

**Close window (Cmd+Q / close button)** → window destroyed, sidecar stays, Processors stay. Next launch reconnects.

## Architecture

### One Processor, Two Modes

`Terminal::Processor` is one class. Two construction modes. Honest about which mode it's in.

```cpp
class Processor : public juce::ChangeBroadcaster
{
public:
    /** Local mode — owns PTY, Grid, Parser, State.
     *  Reader thread writes Grid → sendChangeMessage().
     *  Used when nexus = false (single-process).
     *  Analog: AudioProcessor in standalone mode. */
    Processor();

    /** Hosted mode — shadow Grid + State, IPC via Client.
     *  Receives deltas from sidecar → applies → sendChangeMessage().
     *  writeToPty() forwards to sidecar. No PTY, no Parser.
     *  Used when nexus = true (sidecar manages the real engine).
     *  Analog: AudioProcessor in plugin mode (DAW provides buffers). */
    Processor (Nexus::Client& client, const juce::String& uuid);

    // Uniform API — Display uses these identically in both modes
    State& getState() noexcept;
    Grid& getGrid() noexcept;
    void writeToPty (const char* data, int len);
    void resized (int cols, int rows);
    std::unique_ptr<Display> createDisplay (jreng::Typeface& font);
    void displayBeingDeleted (Display* display) noexcept;

private:
    State state;
    Grid grid;

    // Local mode resources — null in hosted mode
    std::unique_ptr<Parser> parser;
    std::unique_ptr<TTY> tty;

    // Hosted mode resources — null in local mode
    Nexus::Client* client { nullptr };
    juce::String uuid;

    // Non-owning pointer to active Display
    Display* activeDisplay { nullptr };
};
```

**Local mode** (`Processor()`):
- Constructor creates TTY + Parser. Wires internal callbacks: `tty->onData` → `Parser::process()` → Grid writes. `tty->onDrainComplete` → `sendChangeMessage()`. These are Processor-internal wiring (TTY is owned by Processor), not cross-object API.
- `writeToPty()` → `tty->write()` directly.
- `resized()` → `grid.resize()` + `parser.resize()` + PTY open/resize.
- Display holds `Processor&`, listens for changes.

**Hosted mode** (`Processor(client, uuid)`):
- Constructor creates shadow Grid + State only. No TTY, no Parser.
- Client's `messageReceived` routes incoming `renderDelta` PDUs to this Processor via uuid lookup.
- `applyDelta()` → writes cells + graphemes to shadow Grid → `sendChangeMessage()`.
- `writeToPty()` → `client->sendInput(uuid, data, len)`.
- `resized()` → `grid.resize()` + `client->sendResize(uuid, cols, rows)`.
- Display holds `Processor&`, listens for changes. Same code path.

### Display: the Editor analog

```cpp
class Display : public PaneComponent,
                public juce::ChangeListener
{
public:
    Display (Processor& processor, jreng::Typeface& font);
    ~Display() override;

    // ChangeListener — fired by Processor::sendChangeMessage()
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // The Processor this Display renders (public, like AudioProcessorEditor::processor)
    Processor& processor;
};
```

- Constructor: `processor.addChangeListener(this)`. Creates InputHandler, MouseHandler, Screen. All modes.
- Destructor: `processor.removeChangeListener(this)` + `processor.displayBeingDeleted(this)`. RAII.
- `changeListenerCallback()` → `setSnapshotDirty()` (the signal).
- `VBlankAttachment` → `consumeSnapshotDirty()` → `render()` (the clock).
- `writeToPty()` calls `processor.writeToPty()` — Processor routes internally.
- InputHandler and MouseHandler route through `writeToPty()` in all modes.
- Display does not know local vs hosted. One code path.

### Sidecar Architecture

Sidecar is a separate process: same `end` binary invoked with `--nexus`. Paired 1:1 with a window. GUI is always a client when nexus is enabled.

**Sidecar runs `Terminal::Processor` in local mode** — real PTY engines. Same class, local constructor. `ChangeBroadcaster` has no Display listeners on the sidecar — `Nexus::Session` listens instead.

```
Sidecar Process:
  Nexus::Session (ChangeListener + AsyncUpdater)
    owns: jreng::Owner<Terminal::Processor> (local mode, real PTY engines)
    owns: Nexus::Server (InterprocessConnectionServer)
    listens to each Processor via ChangeBroadcaster
    coalesces dirty into batched delta fan-out via AsyncUpdater

GUI Process:
  Nexus::Client (InterprocessConnection, callbacksOnMessageThread = true)
  jreng::Owner<Terminal::Processor> (hosted mode, shadow Grid+State)
    each Processor receives deltas from Client → sendChangeMessage()
  Terminal::Display (ChangeListener on Processor)
    reads Processor's Grid/State → renders
```

### Nexus::Session as Listener-Orchestrator

`Nexus::Session` on the sidecar is the sole orchestrator. It LISTENS. It never initiates.

```cpp
class Session : public juce::ChangeListener,
                public juce::AsyncUpdater
{
public:
    juce::String createProcessor (/* shell, cwd, cols, rows */);
    void removeProcessor (const juce::String& uuid);

    // Connection events (called by ServerConnection)
    void connectionMade (ServerConnection& connection);
    void connectionLost (ServerConnection& connection);
    void attachProcessor (const juce::String& uuid, ServerConnection& connection);
    void detachProcessor (const juce::String& uuid, ServerConnection& connection);
    void handleInput (const juce::String& uuid, const void* data, int size);
    void handleResize (const juce::String& uuid, int cols, int rows);

private:
    // ChangeListener — Processor went dirty
    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        triggerAsyncUpdate();
    }

    // AsyncUpdater — batched delta push to all subscribers
    void handleAsyncUpdate() override
    {
        // iterate Processors with subscribers
        // build deltas from dirty Grids
        // push to subscribed ServerConnections
        // check for exited Processors (atomic flag), handle cleanup
    }

    jreng::Owner<Terminal::Processor> processors;
};
```

| Event | Source | Session reacts by |
|---|---|---|
| PTY output parsed | Processor `sendChangeMessage()` | `changeListenerCallback()` → `triggerAsyncUpdate()` |
| Coalesced dirty | `handleAsyncUpdate()` | Build deltas, push to subscribers |
| Shell exits | Processor sets exited flag + `sendChangeMessage()` | `handleAsyncUpdate()` detects flag → push exitNotification, removeProcessor, if empty → quit |
| Client connects | `ServerConnection::connectionMade()` | Push processor list |
| Client attaches | `ServerConnection::messageReceived()` | Push full snapshot (`buildDelta(0)`), register subscriber |
| Client sends input | `ServerConnection::messageReceived()` | Forward to `Processor::writeToPty()` |
| Client sends resize | `ServerConnection::messageReceived()` | Forward to `Processor::resized()` |
| Client disconnects | `ServerConnection::connectionLost()` | Detach all, remove connection |

## Principles

### Use JUCE's threading model, don't fight it

`callbacksOnMessageThread = true` on both Client and ServerConnection. JUCE's `SafeAction` protects all callbacks. No manual `callAsync`, no raw `this` capture. All dispatch on message thread. Sends serialized.

### Server owns connections

`jreng::Owner<ServerConnection>` on Server. Cleanup in `connectionLost()` — event-driven, no timer, no polling.

### Tell, don't ask

The sidecar TELLS. The client never asks for data.

**Client sends intent:**
- Connect → "I exist"
- `attachProcessor(uuid)` → "I care about this processor"
- `input(uuid, bytes)` → "user typed this"
- `resize(uuid, cols, rows)` → "viewport changed"
- `detachProcessor(uuid)` → "I no longer care"

**Sidecar tells based on intent:**
- Connection made → pushes processor list
- Subscriber appears (attach) → pushes full snapshot (`buildDelta(0)`)
- Content changes (dirty) → pushes incremental delta
- Processor exits → pushes exitNotification

**Eliminated PDUs:**
- `getRenderDelta` — gone. Sidecar pushes on attach and on dirty.
- `listSessions` / `listProcessors` — gone. Sidecar pushes on connect.
- `ping` / `pong` — evaluate if still needed (JUCE connection handles liveness via socket state).

### Notification: right tool for each context

- **Sidecar:** `ChangeBroadcaster` per Processor → `Nexus::Session` (ChangeListener) → `AsyncUpdater` coalesces into batched fan-out. Lean, batch-efficient for IPC.
- **GUI:** `ChangeBroadcaster` per Processor → `Terminal::Display` (ChangeListener) → `setSnapshotDirty()`. VBlank renders.

### Render loop: signal + clock separated

- `changeListenerCallback()` → `setSnapshotDirty()` (the signal)
- `VBlankAttachment` → `consumeSnapshotDirty()` → `render()` (the clock)

### Async startup (event-driven, no polling)

**Existing sidecar:** Lockfile exists → read port → `connectToSocket()` → instant. `connectionMade()` fires on message thread → sidecar pushes processor list → GUI creates Processors + Displays.

**Fresh spawn:** Spawn sidecar → show UI immediately → `connectToSocket()` on a background `juce::Thread` with timeout → `connectionMade()` fires on message thread when ready → tab creation begins. UI responsive from frame 1.

## Data Flow

```
Sidecar PTY output (local-mode Processor):
  PTY → Parser → Grid writes (reader thread)
  → tty->onDrainComplete → sendChangeMessage() (reader thread, internal wiring)
  → Nexus::Session changeListenerCallback() → triggerAsyncUpdate()
  → handleAsyncUpdate() (message thread) → build delta → push via ServerConnection

GUI receives delta (hosted-mode Processor):
  Client::messageReceived() (message thread, callbacksOnMessageThread = true)
  → routes to Processor by uuid → Processor::applyDelta()
  → writes cells + graphemes to shadow Grid → sendChangeMessage()
  → Display::changeListenerCallback() → setSnapshotDirty()
  → VBlank → consumeSnapshotDirty() → Screen::render()

User keystroke:
  Display::keyPressed() → InputHandler → Processor::writeToPty()
  → hosted: Client::sendInput(uuid, data, len) → IPC → sidecar
  → local: tty->write(data, len) directly
```

## BLESSED Compliance

- [x] **Bounds** — Server owns connections via `jreng::Owner`. JUCE SafeAction owns callback safety. Display RAII: registers in ctor, unregisters + `displayBeingDeleted()` in dtor. Processor owns its resources per mode (TTY+Parser or shadow Grid).
- [x] **Lean** — One Processor class replaces Session + RemoteSession + dual-mode Display routing. Two constructors, uniform API. Eliminated: WaitableEvent, pendingListResult, listSessionsLock, probe loop, callAsync dispatch.
- [x] **Explicit** — Constructor choice makes mode explicit. `client != nullptr` is structural presence, not a boolean flag. Tell-don't-ask: intent PDUs name what client wants to DO, not GET.
- [x] **SSOT** — Grid in Processor is sole source for Display rendering. Push fan-out is sole source of delta delivery. Sidecar owns the truth.
- [x] **Stateless** — ServerConnection and Client are stateless dispatchers. No accumulated state between PDUs.
- [x] **Encapsulation** — Display only knows `Processor&`. Panes/Tabs only call `createDisplay()`. IPC hidden inside Processor. `AppState::isClientMode()` routing eliminated.
- [x] **Deterministic** — `sendChangeMessage()` in both modes. Same thread for all IPC callbacks. Push delivers content when it exists. No request/response races. Signal/clock separation prevents over-rendering.

## What's Eliminated

| Old | Replaced by |
|---|---|
| `Terminal::Session` | `Terminal::Processor` (one class, two ctors) |
| `Nexus::RemoteSession` | Absorbed into Processor hosted mode |
| `Display::remoteSession` member | Gone. Display holds `Processor&`. |
| `Display::getSession()` / `getDisplayState()` / `getDisplayGrid()` | `display.processor.getState()` / `display.processor.getGrid()` |
| `AppState::isClientMode()` routing in Panes/Tabs | Gone. Construct the right Processor. |
| `callAsync([this, ...])` in Client | JUCE `callbacksOnMessageThread = true` + `sendChangeMessage()` |
| `std::function<void()> onHostDataReceived` | `ChangeBroadcaster::sendChangeMessage()` from Processor |
| `std::function<void()> onHostShellExited` | Processor sets exited flag + `sendChangeMessage()` |
| `std::function<void()> onAllSessionsExited` | Session checks `processors.empty()` after removeProcessor |
| `WaitableEvent` / `pendingListResult` / `listSessionsLock` | Async startup. Sidecar pushes unsolicited. |
| `getRenderDelta` PDU | Sidecar pushes on attach (full) and on dirty (incremental) |
| `listSessions` / `listProcessors` PDU | Sidecar pushes processor list on connect |
| 5-second blocking probe loop | Background thread connect with timeout |
| Client-mode Display without InputHandler/MouseHandler | Display always creates both. Routes through `Processor::writeToPty()`. |

## Handoff Notes for COUNSELOR

**Sprint order:** Rename first (mechanical, low-risk), then fixes.

**Read priority:** This RFC is the single authority. PLAN-mux-server.md "Remaining Work" section is superseded.

**Bug dependencies:** Bug 1 (connection ownership) and Bug 2 (callbacksOnMessageThread) are prerequisites. Fix them first → connection stability resolves → unblocks Bug 3 (blank screen). Bugs 4+5 (InputHandler/MouseHandler) are independent. Bug 6 (splitImpl) is two missing calls. Bug 7 (graphemes) requires verifying `Grid` API for grapheme row access.

**Shell exit mechanism:** Processor sets an atomic `exited` flag + `sendChangeMessage()`. Session detects it in `handleAsyncUpdate()` during the normal coalesced dirty check. No separate callback mechanism. Piggybacks on existing fan-out path.

**Internal Processor wiring:** `tty->onData` and `tty->onDrainComplete` use std::function inside Processor. This is Processor-internal wiring (TTY is owned by Processor), not cross-object API. Cross-object communication uses JUCE listener interfaces exclusively.

**GridDelta encoding:** Existing binary encoding works correctly. Keep it. The bug was in the consumer (`applyDelta` not copying graphemes), not the encoder/decoder.

**All JUCE source read directly:** `juce_InterprocessConnection.cpp`, `juce_InterprocessConnectionServer.cpp`, `juce_ConnectedChildProcess.cpp`, `juce_AudioProcessor.h/cpp`, `juce_AudioProcessorEditor.h/cpp`, `juce_ChangeBroadcaster.cpp`. Findings from source, not docs or training data.
