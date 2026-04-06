# PLAN: Mux Server

**RFC:** RFC-MUX.md
**Date:** 2026-04-06
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE (reference implementation, no overrides)

## Overview

Transform END from "GUI owns PTY" to "in-process mux server owns PTY, GUI is a client." First GUI instance runs the mux listener thread + Unix socket. On window close, process stays alive as headless daemon. Relaunch discovers existing daemon via socket, connects as client, reattaches sessions. Processes never die.

## Language / Framework Constraints

C++17 / JUCE — MANIFESTO.md reference implementation. No LANGUAGE.md overrides.

JUCE-specific constraints relevant to this plan:
- `juce::Thread` for reader loops (established TTY pattern)
- `juce::MessageManager::callAsync` for reader-to-message-thread dispatch (established pattern)
- `Context<T>` for `Mux::Sessions` and `MuxServer` (established pattern)
- No external serialization — hand-written binary in `jreng_core`
- AF_UNIX on all platforms (Windows 10+ supports it; wezterm proves this works)

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output is correct and BLESSED-compliant.

## Steps

### Step 1: Mux::Sessions — Extract Session Ownership

**Scope:** New `Source/mux/Sessions.h/cpp`, modify `TerminalComponent.h/cpp`, `Panes.cpp`, `Main.cpp`

**Action:**
Create `Mux::Sessions : jreng::Context<Mux::Sessions>`, owned by `ENDApplication` as a value member (constructed before `mainWindow`, destroyed after).

All mux types live in the `Mux` namespace: `Source/mux/`.

Mux::Sessions API:
```cpp
namespace Mux
{
    struct Sessions : jreng::Context<Sessions>
    {
        // Create a new session, return its UUID
        juce::String create (int cols, int rows,
                             const juce::String& shell,
                             const juce::String& cwd,
                             const juce::String& uuid = {});

        // Get session by UUID (jassert if not found)
        Session& get (const juce::String& uuid);

        // Remove session (shell exit only — not GUI detach)
        void remove (const juce::String& uuid);

        // Enumerate all live session UUIDs
        std::vector<juce::String> list() const;

    private:
        std::map<juce::String, std::unique_ptr<Session>> sessions;
    };
}
```

Refactor `Terminal::Component`:
- Remove `Session session;` value member
- Add `juce::String sessionId;` — holds UUID
- Add `Session& getSession()` — returns `Mux::Sessions::getContext()->get(sessionId)`
- All existing `session.xxx()` calls route through `getSession()`
- Constructor calls `Mux::Sessions::getContext()->create(...)` instead of constructing Session in-place

Refactor `Panes::createTerminal()`:
- Session creation now goes through `Mux::Sessions::getContext()->create()`
- UUID returned, passed to Terminal::Component constructor

Refactor `Panes::closePane()`:
- On shell exit: calls `Mux::Sessions::getContext()->remove(uuid)` (process died)
- On GUI detach (future): does NOT call remove

**Constraint:** After this step, END must behave identically to before. `Mux::Sessions` is the only owner of Session objects, but everything is still in-process. No network, no daemon.

**Validation:**
- Builds and runs on macOS
- All terminal operations work: spawn, type, split, close, tabs
- Session lifetime correct: created on terminal spawn, destroyed on shell exit
- No double-free, no dangling reference
- BLESSED: B (clear ownership), S (SSOT — Mux::Sessions is the one owner)

---

### Step 2: SeqNo + GridDelta Builder

**Scope:** Modify `Grid.h/cpp`, new `Source/mux/GridDelta.h`, new `Source/mux/GridDelta.cpp`

**Action:**
Add monotonic sequence number to Grid:
```cpp
// Grid.h — alongside dirtyRows[4]
std::atomic<uint64_t> seqno { 0 };
```

Increment `seqno` in `markRowDirty()` (reader thread, relaxed order — same as dirtyRows writes).

Add row-level seqno tracking — each row records the seqno at which it was last dirtied:
```cpp
// Inside Grid::Buffer
juce::HeapBlock<uint64_t> rowSeqnos;  // totalRows entries, parallel to rowStates
```

Written by `markRowDirty()`: `rowSeqnos[physicalRow] = seqno.load(relaxed)`.

Create `GridDelta` value type:
```cpp
// Source/mux/GridDelta.h
namespace Mux
{
    struct CellRange
    {
        uint32_t row;           // logical row (0 = top of scrollback)
        uint16_t colStart;
        uint16_t colEnd;
        std::vector<Terminal::Cell> cells;
    };

    struct GridDelta
    {
        uint64_t seqno;
        uint16_t cols;
        uint16_t rows;
        uint16_t cursorRow;
        uint16_t cursorCol;
        bool     cursorVisible;
        uint8_t  screenIndex;   // 0 = normal, 1 = alternate
        juce::String title;
        juce::String cwd;
        std::vector<CellRange> dirtyRanges;
    };
}
```

Add `Grid::buildDelta(uint64_t sinceSeqno) -> Mux::GridDelta`:
- Iterates ring buffer rows
- Any row with `rowSeqnos[physicalRow] > sinceSeqno` is included
- Returns full cell data for dirty rows
- Reads cursor/title/cwd from State
- Does NOT consume dirty bits (that's still the render path's job)

**Validation:**
- Builds
- `buildDelta(0)` returns entire visible grid + scrollback
- `buildDelta(currentSeqno)` returns empty delta (nothing new)
- Write some cells, `buildDelta(previousSeqno)` returns exactly those rows
- BLESSED: L (GridDelta is a focused value type), E (seqno is explicit version token)

---

### Step 3: LEB128 + PDU Framing

**Scope:** New `modules/jreng_core/binary/jreng_leb128.h`, new `Source/mux/Pdu.h`, new `Source/mux/PduCodec.h/cpp`

**Action:**
Implement LEB128 encode/decode in `jreng_core`:
```cpp
namespace jreng
{
    // Returns number of bytes written (max 10 for uint64_t)
    int encodeLEB128 (uint64_t value, uint8_t* out) noexcept;

    // Returns number of bytes consumed, writes value. Returns 0 on error.
    int decodeLEB128 (const uint8_t* data, int available, uint64_t& value) noexcept;
}
```

Define PDU enum and frame format:
```cpp
// Source/mux/Pdu.h
namespace Mux
{
    enum class PduKind : uint16_t
    {
        Hello                = 0x01,
        HelloResponse        = 0x02,
        Ping                 = 0x03,
        Pong                 = 0x04,

        SpawnSession         = 0x10,
        SpawnSessionResponse = 0x11,
        AttachSession        = 0x12,
        AttachSessionResponse= 0x13,
        DetachSession        = 0x14,
        ResizeSession        = 0x15,
        Input                = 0x16,

        GetRenderDelta       = 0x30,
        RenderDelta          = 0x31,
        TitleChanged         = 0x34,
        CwdChanged           = 0x35,

        SessionExited        = 0x40,
        ListSessions         = 0x50,
        ListSessionsResponse = 0x51,

        Shutdown             = 0x60,
    };

    struct PduFrame
    {
        uint64_t serial;
        PduKind  kind;
        juce::MemoryBlock payload;
    };
}
```

Implement `PduCodec`:
```cpp
namespace Mux
{
    struct PduCodec
    {
        // Encode frame to wire bytes
        static juce::MemoryBlock encode (const PduFrame& frame);

        // Decode from stream buffer. Returns bytes consumed, 0 if incomplete.
        static int decode (const uint8_t* data, int available, PduFrame& frame);
    };
}
```

Frame wire format: `LEB128(len) | LEB128(serial) | LEB128(kind) | payload`

**Validation:**
- LEB128 round-trips for edge cases: 0, 127, 128, 16383, 16384, UINT64_MAX
- PduCodec round-trips all PduKind values with various payload sizes
- Partial frame decode returns 0 (incomplete)
- BLESSED: E (explicit framing, named enum), L (codec is a focused object)

---

### Step 4: MuxServer + Socket Listener

**Scope:** New `Source/mux/MuxServer.h/cpp`, new `Source/mux/MuxClientSession.h/cpp`, modify `Main.cpp`

**Action:**
Create `MuxServer : jreng::Context<MuxServer>`, owned by `ENDApplication`:
- Binds Unix domain socket at `~/.config/end/end.sock`
- Accept loop on a `juce::Thread` (listener thread)
- Each accepted connection creates a `MuxClientSession`

Socket path: platform-determined.
- macOS/Linux: `~/.config/end/end.sock`
- Windows: same path (AF_UNIX, Windows 10+)

`MuxServer`:
```cpp
struct MuxServer : jreng::Context<MuxServer>, juce::Thread
{
    MuxServer();   // binds socket
    ~MuxServer();  // closes socket, stops thread

    void run() override;  // accept loop

private:
    int listenFd { -1 };
    juce::String socketPath;
    std::vector<std::unique_ptr<MuxClientSession>> clients;
    juce::CriticalSection clientsLock;
};
```

`MuxClientSession`:
```cpp
struct MuxClientSession : juce::Thread
{
    MuxClientSession (int fd, MuxServer& server);
    ~MuxClientSession();

    void run() override;  // read loop: read frames, dispatch via callAsync
    void send (const PduFrame& frame);  // thread-safe write

private:
    int clientFd;
    MuxServer& server;
    juce::CriticalSection writeLock;
    std::vector<uint8_t> readBuffer;
};
```

PDU dispatch (on message thread via `callAsync`):
- `Hello` -> respond `HelloResponse`
- `Ping` -> respond `Pong`
- `ListSessions` -> query `Mux::Sessions`, respond `ListSessionsResponse`

**Constraint:** Only handshake + list in this step. No session spawn/attach/render yet.

`MuxServer` initialization in `Main.cpp`:
- Constructed after `Mux::Sessions`, before `mainWindow`
- On construction: check if socket exists, try `Ping`. If `Pong` received: another instance is running — this instance becomes a client (Step 7). If no response: remove stale socket, bind fresh.

**Validation:**
- Builds on macOS
- Socket created at `~/.config/end/end.sock` on launch
- Socket removed on clean shutdown
- Can connect with `nc -U ~/.config/end/end.sock` (or simple test client)
- `Hello` / `Pong` / `ListSessions` round-trip works
- BLESSED: B (MuxServer owns socket fd, MuxClientSession owns client fd), E (explicit dispatch)

---

### Step 5: SpawnSession + Input PDU

**Scope:** Modify `MuxClientSession.cpp`, `Sessions.h/cpp`, `Pdu.h`

**Action:**
Implement `SpawnSession` dispatch:
- Client sends `SpawnSession { shell, cwd, cols, rows, uuid }`
- Server calls `Mux::Sessions::getContext()->create(cols, rows, shell, cwd, uuid)`
- Responds `SpawnSessionResponse { uuid }`

Implement `Input` dispatch:
- Client sends `Input { uuid, bytes }`
- Server calls `Mux::Sessions::getContext()->get(uuid).writeToPty(bytes)`

Implement `ResizeSession` dispatch:
- Client sends `ResizeSession { uuid, cols, rows }`
- Server calls `Mux::Sessions::getContext()->get(uuid).resize(cols, rows)`

Implement `SessionExited` notification:
- Wire `Session::onShellExited` in `Mux::Sessions` on creation
- When shell exits: fan-out `SessionExited { uuid }` to all attached `MuxClientSession`s

**Validation:**
- SpawnSession creates a live PTY (visible via `ps`)
- Input PDU reaches the shell (type characters, see shell respond)
- ResizeSession changes the PTY window size
- Shell exit triggers SessionExited fan-out
- BLESSED: E (explicit lifecycle), S (Mux::Sessions is SSOT for session state)

---

### Step 6: RenderDelta Fan-out

**Scope:** Modify `MuxClientSession.h/cpp`, `Sessions.h/cpp`, `Grid.h/cpp`

**Action:**
Implement `AttachSession`:
- Client sends `AttachSession { uuid }`
- Server registers `MuxClientSession` as subscriber for that session's deltas
- Responds `AttachSessionResponse { cols, rows, seqno }`

Implement `DetachSession`:
- Client sends `DetachSession { uuid }`
- Server unregisters subscriber. Session keeps running.

Implement `GetRenderDelta`:
- Client sends `GetRenderDelta { uuid, sinceSeqno }`
- Server calls `grid.buildDelta(sinceSeqno)`
- Responds `RenderDelta { gridDelta }`

Implement server-push notification on dirty:
- `Mux::Sessions` wires into each session's `State::snapshotDirty` mechanism
- On dirty: build `GridDelta`, fan-out `RenderDelta` to all attached clients for that session
- Fan-out uses a subscriber list per session: `std::vector<std::weak_ptr<MuxClientSession>>`
- Fan-out happens on message thread (same as current VBlank path)

**Serialization of GridDelta:**
- Hand-written in `PduCodec` — serialize Cell array as raw bytes (Cell is 16-byte trivially copyable)
- Grapheme sidecar serialized separately (variable-length)

**Validation:**
- `AttachSession` + `GetRenderDelta(0)` returns full grid contents
- Typing in a session produces RenderDelta fan-out to attached client
- Detach stops fan-out, session keeps running
- Re-attach + `GetRenderDelta(lastSeqno)` returns only new changes
- BLESSED: S (no PerPane shadow — seqno replaces it), D (same seqno in = same delta out)

---

### Step 7: MuxClient — GUI as Client

**Scope:** New `Source/mux/MuxClient.h/cpp`, new `Source/mux/ShadowSession.h/cpp`, modify `TerminalComponent.h/cpp`, `Main.cpp`

**Action:**
Create `MuxClient`:
```cpp
struct MuxClient : juce::Thread
{
    MuxClient (const juce::String& socketPath);

    bool connect();    // connect to socket, Hello handshake
    void disconnect(); // DetachSession all, close socket

    void spawnSession (int cols, int rows,
                       const juce::String& shell,
                       const juce::String& cwd,
                       const juce::String& uuid);

    void attachSession (const juce::String& uuid);
    void detachSession (const juce::String& uuid);
    void sendInput (const juce::String& uuid, const void* data, int size);
    void sendResize (const juce::String& uuid, int cols, int rows);
    void requestDelta (const juce::String& uuid, uint64_t sinceSeqno);

    // Incoming PDU callback (fires on message thread)
    std::function<void (const PduFrame&)> onPdu;

private:
    void run() override;  // read loop
    int socketFd { -1 };
    juce::CriticalSection writeLock;
};
```

Create `ShadowSession`:
- Holds a shadow `Grid` (read-only replica) + shadow `State` values
- `applyDelta(GridDelta)` — applies dirty ranges to shadow Grid, updates cursor/title/cwd
- Exposes same read interface that `Screen::render()` needs: `getGrid()`, `getState()` equivalents

Refactor `Terminal::Component` dual-mode:
- **Local mode** (first instance, is the server): reads from `Mux::Sessions` directly (current path, no IPC overhead)
- **Client mode** (second instance or reconnect): reads from `ShadowSession`, input goes through `MuxClient`
- Mode determined at construction: if `MuxServer::getContext()` exists and owns this session, local mode. Otherwise client mode.

**First instance startup flow:**
1. `ENDApplication` constructs `Mux::Sessions`, `MuxServer`
2. `MuxServer` binds socket, starts listener thread
3. `mainWindow` created, terminals created via `Mux::Sessions` directly (local mode)
4. VBlank reads Grid/State directly — zero IPC overhead, identical to current behavior

**Second instance startup flow:**
1. `ENDApplication` checks socket — `Ping` succeeds
2. Does NOT construct `Mux::Sessions` or `MuxServer`
3. Constructs `MuxClient`, connects to socket
4. `ListSessions` — gets live UUIDs
5. Matches against `state.xml` layout
6. `AttachSession` per pane — creates `ShadowSession` per session
7. `GetRenderDelta(0)` — populates shadow Grid
8. VBlank reads from `ShadowSession`

**Validation:**
- First instance works identically to before (local mode, no IPC)
- Second instance connects, sees sessions, renders live
- Typing in second instance reaches shell (Input PDU)
- Resize in second instance propagates (ResizeSession PDU)
- BLESSED: E (explicit local/client mode), S (ShadowSession is derived, not SSOT)

---

### Step 8: Headless Daemon Mode

**Scope:** Modify `Main.cpp`, `MuxServer.h/cpp`

**Action:**
When the last GUI window closes:
- Currently: `systemRequestedQuit()` shuts down the app
- New behavior: if `Mux::Sessions` has live sessions, drop the window but keep the message loop running
- `ENDApplication` becomes a headless daemon — no window, no GL, just `Mux::Sessions` + `MuxServer`

Implementation:
- `systemRequestedQuit()` checks `Mux::Sessions::getContext()->list().empty()`
- If sessions exist: destroy `mainWindow`, do NOT call `quit()`. Message loop continues.
- `MuxServer` keeps accepting connections. Sessions keep running.
- When all sessions exit (all shells dead) AND no clients connected: call `quit()`

On GUI relaunch:
- Socket probe succeeds — becomes client instance (Step 7 flow)
- `state.xml` layout restored, sessions reattached
- Full scrollback available via `GetRenderDelta(0)`

**Validation:**
- Close GUI window — END process still running (visible in `ps`)
- Shells still alive (visible in `ps` as children)
- Relaunch END — connects to daemon, reattaches, full scrollback rendered
- All shells still running, can type immediately
- Kill all shells — daemon exits
- BLESSED: B (clear lifecycle — daemon lives as long as sessions do)

---

### Step 9: Session Restoration Integration

**Scope:** Modify `Panes.cpp`, `Tabs.cpp`, `AppState.cpp`

**Action:**
Integrate mux with `state.xml` layout restoration:

**On GUI quit (not daemon exit):**
1. `AppState::save()` writes layout as today (unchanged)
2. `MuxClient::disconnect()` sends `DetachSession` for all sessions (if client mode)
3. Window destroyed, daemon stays

**On GUI relaunch:**
1. `AppState::load()` reads `state.xml` — layout tree restored
2. `MuxClient` connects to daemon
3. `ListSessions` — get live UUID list from daemon
4. For each PANE in layout tree:
   - Extract UUID from SESSION node
   - If UUID found in daemon's list: `AttachSession(uuid)` — live session reattached
   - If UUID not found: `SpawnSession` with persisted cwd — fresh shell in same directory
5. `GetRenderDelta(0)` per session — full scrollback
6. Render normally

**Tab/pane ordering preserved** by `state.xml`. Session content preserved by daemon. Both are independent SSOTs for their respective domains.

**Validation:**
- Close and relaunch: tabs, splits, pane positions identical
- Running processes (btop, nvim, etc.) still alive and interactive
- Scrollback fully intact
- New pane in a tab where the old shell died: opens in same cwd
- BLESSED: S (state.xml = SSOT for layout, daemon = SSOT for sessions)

---

### Step 10: Windows AF_UNIX

**Scope:** Modify `MuxServer.h/cpp`, `MuxClient.h/cpp`

**Action:**
Abstract socket operations behind a platform layer:
- macOS/Linux: `socket(AF_UNIX, SOCK_STREAM, 0)`, `bind`, `listen`, `accept`, `connect`
- Windows: same API (AF_UNIX supported since Windows 10 1803), but may need `#include <afunix.h>` and slightly different error handling

wezterm's `wezterm-uds` crate proves this works. Follow the same pattern:
- Same socket path logic
- Same bind/listen/accept/connect API
- `#ifdef _WIN32` only for include paths and minor error code differences

Socket path on Windows: `%APPDATA%/end/end.sock` (or `~/.config/end/end.sock` if running under MSYS2/WSL — use `juce::File::getSpecialLocation` for platform-correct path)

**Validation:**
- Builds and runs on Windows
- Socket created, connections work
- Full mux flow works (spawn, attach, render, detach, reattach)
- BLESSED: E (platform abstraction is explicit, no #ifdef spaghetti)

## BLESSED Alignment

| Principle | How Satisfied |
|-----------|---------------|
| **B (Bound)** | Mux::Sessions owns Sessions. MuxServer owns socket. MuxClientSession owns client fd. ENDApplication owns all Context<T> objects. RAII throughout. |
| **L (Lean)** | Mux::Sessions, MuxServer, MuxClientSession, MuxClient, ShadowSession, GridDelta, PduCodec — focused objects, single responsibility each. No god object. |
| **E (Explicit)** | PduKind enum names all message types. SeqNo is explicit version token. Local vs client mode explicit at construction. No magic values. |
| **S (SSOT)** | Mux::Sessions is the one owner of Sessions. Grid is the one truth for cell data. ShadowSession is derived (not a second truth). state.xml is SSOT for layout. |
| **S (Stateless)** | MuxClientSession is stateless between PDUs. No PerPane shadow state. SeqNo replaces server-side per-client tracking. |
| **E (Encapsulation)** | Mux::Sessions knows nothing about sockets. MuxClientSession knows nothing about Grid. Terminal::Component doesn't know if session is local or remote. |
| **D (Deterministic)** | GetRenderDelta(seqno) always returns the same result for the same seqno. GridDelta carries geometry — no race on resize. |

## Risks / Open Questions

1. **Grid::buildDelta thread safety** — buildDelta reads Grid cells while reader thread writes them. Current render path has the same constraint (Screen::render reads Grid on message thread while TTY writes on reader thread). Same solution: `resizeLock` prevents structural changes, individual cell reads are atomic-width (Cell is 16 bytes, may need careful handling). Need to verify Cell read atomicity or add seqno-based snapshot mechanism.

2. **GridDelta size for full scrollback** — `GetRenderDelta(0)` with 10k scrollback lines at 200 cols = 32MB of cell data. May need chunked transfer or compression. Defer compression to post-v1, chunk if needed.

3. **VBlank polling vs server push** — Step 6 uses server push (dirty notification fan-out). Alternative: client polls with VBlank-driven `GetRenderDelta`. Push is more efficient (no wasted round-trips) but adds complexity. RFC recommends push. Follow RFC unless issues surface.

4. **JUCE message loop without window** — Step 8 requires JUCE's message loop to run headless. Need to verify `JUCEApplication` continues dispatching without a window. If not, may need a hidden window or custom run loop.
