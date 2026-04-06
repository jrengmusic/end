# RFC — END Multiplex Server
**Date:** 2026-04-05
**Status:** Ready for COUNSELOR
**Author:** BRAINSTORMER (pre-flight shadow agent for ARCHITECT JRENG)

---

## 1. Problem Statement

END sessions die when the GUI closes. Every shell process, every running program
(`vim`, `ssh`, `htop`, whatever was live) is killed on window close. Relaunching END
starts fresh — all context is gone.

**Primary objective:** Session restoration. When END relaunches, every session is
exactly where it was. Same tabs, same splits, same shells, same running processes,
same scrollback.

**Architectural objective:** A correct, general-purpose multiplex server. Session
restoration is the first client. `end cli`, remote access over SSH tunnel, second
machine — all natural follow-ons once the daemon and wire protocol exist. The
architecture is either implemented correctly or not at all. No artificial scope
restrictions on what can connect to the socket.

This requires a **background daemon** that owns the PTYs independently of the GUI.
The GUI is a client that attaches on launch and detaches on close. When the GUI dies,
the daemon keeps every shell alive. When the GUI relaunches, it reconnects.

There is no alternative. Serializing the Grid to disk and relaunching the shell is
not restoration — it kills every running process. The daemon is load-bearing.

---

## 2. WezTerm Mux — Architecture Audit

### 2.1 Core Object Hierarchy

```
Mux (singleton, RwLock-guarded HashMaps)
  ├── Domain          (LocalDomain / RemoteSshDomain / ClientDomain)
  ├── Window          (HashMap<PaneId, …> + workspace string)
  │     └── Tab       (tree of Pane handles, split layout)
  │           └── Pane (trait object: LocalPane / ClientPane / TmuxPane)
  └── Subscriber registry (notification fan-out)
```

`Mux` is a `thread_local!` global with `Arc<Mux>`. `LocalPane` owns
`Arc<Mutex<Terminal>>` + `Box<dyn MasterPty>` + a background reader thread that
pipes PTY bytes through a `socketpair` into `parse_buffered_data()`, which delivers
`Vec<Action>` to `pane.perform_actions()`.

### 2.2 Server Transport

Unix domain socket. Each client connection maps to a `ClientSession` with a
`read_loop` / `write_loop`. Wire protocol:

```
Frame = LEB128(length | compressed_mask) + LEB128(serial) + LEB128(ident) + payload
```

Payload is `bincode`-serialized PDUs (~40 variants covering spawn, input, resize,
render-delta, clipboard, etc.). Server-side `SessionHandler` subscribes to
`Mux::subscribe()` and fans out `MuxNotification` events as render diffs to all
attached clients.

### 2.3 Render Delta Protocol

Clients do not receive raw PTY bytes. They receive render diffs: dirty line ranges +
cursor position + title + cwd + sequence number. Lines are serialized as
`termwiz::surface::Line` objects. Client re-renders locally from these.

### 2.4 BLESSED Audit of WezTerm's Design

| Issue | Pillar violated |
|---|---|
| `Mux` is a `thread_local!` global singleton | **B** + **S** (SSOT) |
| `parking_lot::RwLock` on all collections | **S** (Stateless) |
| `Arc<dyn Pane>` — god trait, 20+ methods | **E** (Encapsulation) |
| `LocalPane` holds `Arc<Mutex<Terminal>>` | **B** + **S** |
| Subscriber callbacks `Fn(MuxNotification) -> bool` | **E** (Explicit) — magic bool |
| `parse_buffered_data` socketpair indirection | **L** — unnecessary layer |
| `PerPane` per-client server-side shadow state | **S** (SSOT) + **S** (Stateless) |

WezTerm's concurrency model (Tokio async + `smol` + `parking_lot`) has no JUCE
equivalent and is not ported. Its wire protocol and render-delta model are correct
and worth adopting. Its object model is not.

---

## 3. END Current State

### 3.1 What Already Exists

```
Terminal::Session          — orchestrator (State + Grid + Parser + TTY)
Terminal::TTY              — abstract PTY: open/close/write/read (reader thread)
Terminal::State            — APVTS-style atomic + ValueTree cross-thread bridge
Terminal::Grid             — ring-buffer cell store + dirty tracking (atomic bitmask)
AppState                   — root ValueTree: TABS > TAB > PANES > PANE > SESSION
AppState::save()           — serializes full ValueTree to ~/.config/end/state.xml
AppState::load()           — deserializes on launch (layout already restored)
jreng::GLSnapshotBuffer    — lock-free double-buffer message→GL snapshot handoff
jreng::GLMailbox           — lock-free atomic pointer exchange
```

### 3.2 What state.xml Already Persists

On quit, `state.xml` already captures:
- Window dimensions + zoom
- Tab layout + active tab index
- Full PANES binary tree (splits, ratios, directions)
- Per-PANE SESSION node with: `cwd`, `displayName`, `shellProgram`, UUID

Layout restoration is already implemented. The live process side is what's missing.

### 3.3 What Is Missing

1. **Daemon process** — owns `Terminal::Session` objects independently of the GUI.
2. **IPC transport** — Unix domain socket, PDU framing, read/write loops.
3. **Session ownership transfer** — `Session` moves from `TerminalComponent` into
   `SessionPool` inside the daemon.
4. **Render delta fan-out** — daemon sends `GridDelta` (dirty cells since seqno N)
   to attached clients. Clients apply to a shadow `Grid` and render normally.
5. **Scrollback in memory** — daemon holds the full `Grid` ring buffer for the
   lifetime of the session, regardless of whether any client is attached.

---

## 4. Proposed Architecture

### 4.1 Process Model

```
First launch:
  ENDApplication → spawns daemon → daemon owns SessionPool + MuxServer
  → GUI connects as client → attaches sessions → renders normally

GUI close:
  GUI sends Detach per session → disconnects → daemon keeps all PTYs alive

GUI relaunch:
  ENDApplication → discovers daemon (socket probe) → connects
  → ListSessions → match UUIDs from state.xml → AttachSession per pane
  → GetRenderDelta(0) → full scrollback → render live

end cli (future):
  same socket, same PDU protocol, no GUI

SSH tunnel (future):
  forward daemon socket over SSH — no daemon-side changes required
```

The daemon is a headless process: no JUCE GUI, no rendering, no OpenGL. It owns
`SessionPool` and the Unix socket listener. It exits only when all sessions have
exited (all shells dead) or on explicit `Shutdown` PDU.

### 4.2 Object Hierarchy

```
Daemon process:
  MuxServer (Context<MuxServer>)
    ├── SessionPool          — owns Terminal::Session objects by UUID
    └── ClientRegistry       — MuxClientSession per connected fd

GUI process:
  MuxClient                  — connects to daemon socket
    └── ShadowSessionPool    — per-session: shadow Grid + State (read-only replica)

Any future client (CLI, SSH, remote GUI):
  MuxClient                  — same interface, same protocol
```

`TerminalComponent` holds a `SessionId` (UUID). It reads from `ShadowSessionPool`
exactly as it currently reads from a local `Session`. Render pipeline is unchanged.

### 4.3 Session Ownership Transfer

Currently: `TerminalComponent` owns `Terminal::Session` (via `Panes::createTerminal`).

Proposed: `SessionPool` (inside daemon) owns `Terminal::Session`.

```
SessionPool::create (cols, rows, shell, cwd, uuid) -> SessionId
SessionPool::get    (SessionId) -> const Session&
SessionPool::remove (SessionId) -> void   // shell exit only — not GUI detach
```

GUI detach does not call `remove`. It unsubscribes the `MuxClientSession` from
`GridDelta` fan-out. The session keeps running.

### 4.4 Wire Protocol — END-PDU

LEB128 framing from WezTerm's `codec`. Serialization hand-written in `jreng_core`
(extend `jreng_binary_data.h`). No external serialization dependency.

**Frame format:**
```
| LEB128(len) | LEB128(serial) | LEB128(kind) | payload bytes |
```
`len` covers serial + kind + payload. Compression bit reserved, unused in v1.

**PDU kinds:**

```cpp
enum class PduKind : uint16_t
{
    // Handshake
    Hello                = 0x01,  // C→S: client version
    HelloResponse        = 0x02,  // S→C: server version + capabilities
    Ping                 = 0x03,
    Pong                 = 0x04,

    // Session lifecycle
    SpawnSession         = 0x10,  // C→S: shell, cwd, cols, rows, uuid
    SpawnSessionResponse = 0x11,  // S→C: confirm uuid
    AttachSession        = 0x12,  // C→S: attach existing uuid
    AttachSessionResponse= 0x13,  // S→C: current cols, rows, seqno
    DetachSession        = 0x14,  // C→S: detach (server keeps session alive)
    ResizeSession        = 0x15,  // C→S: cols × rows
    Input                = 0x16,  // C→S: key/paste bytes

    // Render
    GetRenderDelta       = 0x30,  // C→S: since seqno N
    RenderDelta          = 0x31,  // S→C: GridDelta
    ClipboardChanged     = 0x32,  // S→C: OSC 52 payload
    Bell                 = 0x33,  // S→C: BEL
    TitleChanged         = 0x34,  // S→C: new display name
    CwdChanged           = 0x35,  // S→C: new cwd

    // Session state
    SessionExited        = 0x40,  // S→C: shell process died
    ListSessions         = 0x50,  // C→S: enumerate
    ListSessionsResponse = 0x51,  // S→C: [{uuid, title, cwd, cols, rows, running}]

    // Server control
    Shutdown             = 0x60,  // C→S: kill server (rejected if sessions running)
};
```

### 4.5 Render Delta — GridDelta

Daemon builds `GridDelta` from `Grid`'s dirty bitmask + monotonic `seqno` atomic
(new, added to `Grid`). Client applies to shadow `Grid`. Render pipeline reads from
shadow `Grid` — no change to `Screen`, `ScreenRender`, or GL.

```cpp
struct CellRange
{
    uint32_t                    row;
    uint16_t                    colStart;
    uint16_t                    colEnd;
    std::vector<Terminal::Cell> cells;  // [colStart, colEnd)
};

struct GridDelta
{
    uint64_t               seqno;
    uint16_t               cols;         // current geometry
    uint16_t               rows;
    uint16_t               cursorRow;
    uint16_t               cursorCol;
    bool                   cursorVisible;
    uint8_t                screenIndex;  // 0 = normal, 1 = alternate
    juce::String           title;
    juce::String           cwd;
    std::vector<CellRange> dirtyLines;
};
```

**Resize synchronization:** `GridDelta` carries `cols` + `rows`. Client checks
geometry before applying dirty cells. If geometry changed: resize shadow `Grid`
first, then apply cells. Eliminates bounds-violation race.

**Seqno replaces PerPane:** Client sends last-seen seqno. Server returns all dirty
lines since that seqno. No per-client server-side shadow state. No drift. No
`PerPane`.

**Full scrollback on attach:** Client sends `GetRenderDelta(seqno=0)`. Server
returns entire `Grid` ring buffer as the initial delta. No disk replay needed.

### 4.6 Transport — MuxServer / MuxClientSession / MuxClient

```
MuxServer
  socket path: ~/.config/end/end.sock  (macOS / Linux)
  accept loop on message thread
  spawns MuxClientSession (juce::Thread) per connection
  owned by daemon ENDApplication (Context<MuxServer>)

MuxClientSession (one per connected client)
  owns: socket fd, reader juce::Thread, write queue (juce::AbstractFifo + buffer)
  reader thread: reads PDU frames → callAsync to message thread → dispatch
  message thread: builds GridDelta → writes to queue → reader flushes to socket

MuxClient (any connecting process)
  connects to end.sock
  Hello + ListSessions on connect
  AttachSession per session of interest
  GetRenderDelta driven by VBlank (GUI) or on-demand (CLI)
  applies RenderDelta to ShadowSessionPool
```

**Discovery:** `MuxClient::discover()` — `Ping` on socket, expect `Pong` within
500ms. Dead or absent: clean up stale socket, spawn daemon, wait, connect.

**Threading (daemon):**

| Thread | Owns | Does |
|---|---|---|
| Reader (TTY) | session PTY fd | writes Grid cells + atomics |
| Message (JUCE) | MuxServer, write queues, fan-out | PDU dispatch, GridDelta build |
| MuxClientSession reader | socket fd | reads frames → `callAsync` to message thread |

No new primitives. Reader thread → atomics → `callAsync` to message thread is the
existing TTY → State → ValueTree pattern verbatim.

**Fan-out:** `consumeSnapshotDirty()` fires on message thread (existing path).
Daemon iterates `std::vector<std::weak_ptr<MuxClientSession>>` for the session and
queues `RenderDelta` for each attached client. List guarded by `juce::CriticalSection`
— written on connect/disconnect, read on notification.

### 4.7 Scrollback

Daemon holds the full `Grid` ring buffer for each session for its entire lifetime.
Never evicted while session is alive, regardless of client attachment state.
Bounded by existing config-driven scrollback line count.

On attach: `GetRenderDelta(0)` returns the entire ring buffer as the initial delta.
No disk serialization of Grid state. Memory only.

### 4.8 State Persistence Integration

`AppState::save()` already writes the full layout tree to `state.xml` on GUI quit.
`AppState::load()` already reads it back on launch.

On GUI relaunch:
1. Load `state.xml` — layout restored as today
2. Connect to daemon
3. `ListSessions` — get live UUID list
4. For each PANE SESSION node: `AttachSession(uuid)`
   - UUID found → `GetRenderDelta(0)` → render live scrollback
   - UUID not found → `SpawnSession` with persisted `cwd` (fallback, fresh shell)
5. Render normally

`state.xml` is SSOT for layout. Daemon is SSOT for session state. They are independent.

---

## 5. BLESSED Compliance

| Pillar | Compliance | Notes |
|---|---|---|
| **B** | ✅ | `MuxServer` owned by daemon `ENDApplication`. `SessionPool` owns `Session` objects. `MuxClientSession` owns its fd. RAII throughout. No raw owning pointers. No cross-thread direct calls. |
| **L** | ✅ | `MuxServer`, `SessionPool`, `MuxClientSession`, `MuxClient`, `ShadowSessionPool` — five focused objects. No god object. `GridDelta` is a plain value type. |
| **E** | ✅ | All IDs typed (`SessionId` = `juce::String` UUID). PDU kinds named enum. `GridDelta::seqno` is the explicit version token. No magic values. Thread contracts documented per object. |
| **S** (SSOT) | ✅ | `SessionPool` is the one owner of `Session`. `Grid` state lives in one place on the daemon. Shadow `Grid` on client is a derived read-only copy — not a second truth. `state.xml` is SSOT for layout. |
| **S** (Stateless) | ✅ | `MuxClientSession` is stateless between PDU requests. No `PerPane` shadow. Seqno on `Grid` replaces it entirely. |
| **E** (Encapsulation) | ✅ | `SessionPool` knows nothing about sockets. `MuxClientSession` knows nothing about `Grid`. `TerminalComponent` knows nothing about whether session is local or remote. Same `Grid` interface either way. |
| **D** | ✅ | `GetRenderDelta(seqno)` → deterministic `GridDelta`. Same input, same output. Resize geometry in delta eliminates race. |

**WezTerm patterns explicitly rejected:**
- `PerPane` server-side shadow state — **SSOT + Stateless violation**. Seqno replaces it.
- `thread_local!` singleton — replaced by `Context<MuxServer>`.
- `Arc<dyn Pane>` god trait — `Session` is already decomposed correctly.
- `socketpair` indirection in reader thread — direct callback is sufficient.
- `bincode` / `serde` / external serialization — hand-written in `jreng_core`.

---

## 6. Open Questions

**Q1 — Daemon binary**
Separate headless binary (`end-server`) or main `end` binary with `--server` flag?
Separate binary is cleaner — no GUI framework, no OpenGL linked into daemon.
Recommendation: separate binary. Confirm.

**Q2 — Daemon auto-start**
GUI auto-starts daemon on first launch if socket not found. Daemon double-forks and
detaches (Unix). Confirm vs. explicit `end --server` invocation requirement.

**Q3 — Daemon exit policy**
Daemon exits when all sessions have exited (all shells dead). Never exits while any
session is running, even if no client is connected. Confirm.

**Q4 — SeqNo placement**
`std::atomic<uint64_t> seqno` on `Grid` alongside existing `dirtyRows[4]`.
Reader thread increments on every dirty-row write. Confirm vs. `State`.

**Q5 — Windows named pipe**
Unix domain socket covers macOS + Linux. Windows transport (named pipe) deferred.
Confirm macOS-first scope for v1.

**Q6 — Authentication**
Local socket: filesystem permissions only (socket in `~/.config/end/` — user-only).
Remote (future SSH tunnel): SSH provides the auth layer — daemon has no PKI in v1.
Confirm no PKI requirement for v1.

---

## 7. Implementation Sequence

### Phase A — Session Ownership Transfer
Move `Terminal::Session` ownership from `TerminalComponent` into `SessionPool`
(`Context<SessionPool>`, owned by `ENDApplication`). `TerminalComponent` holds
`SessionId`, looks up shadow session. No network, no daemon — structural refactor.
Existing behavior identical. BLESSED audit required before Phase B.

### Phase B — SeqNo + GridDelta Builder
Add `std::atomic<uint64_t> seqno` to `Grid`. Implement
`Grid::buildDelta(uint64_t sinceSeqno) -> GridDelta`. Unit test: seqno in = empty
delta; writes after seqno = correct dirty lines in delta.

### Phase C — Daemon Process + MuxServer
Daemon binary. `MuxServer` accept loop. `MuxClientSession` reader thread. LEB128
PDU framing. `Hello` / `HelloResponse` / `Ping` / `Pong` / `ListSessions`.
`MuxClient` connect + discovery. Validate round-trip before proceeding.

### Phase D — Attach + Render Delta + Fan-out
`AttachSession` / `AttachSessionResponse` / `DetachSession`. `GetRenderDelta` /
`RenderDelta`. Fan-out from `consumeSnapshotDirty`. GUI applies `GridDelta` to shadow
`Grid`. VBlank renders from shadow `Grid`. Resize sync via geometry in `GridDelta`.
**Auditor pass required on resize sync correctness before Phase E.**

### Phase E — Session Restoration on Relaunch
GUI relaunch: discover daemon → connect → `ListSessions` → match UUIDs from
`state.xml` → `AttachSession` per pane → `GetRenderDelta(0)` → render live.
Fallback: UUID not found → `SpawnSession` in persisted `cwd`.

### Phase F — Input + Resize
`Input` PDU (key/paste bytes from GUI → daemon → PTY). `ResizeSession` PDU with
resize sync correctness (geometry propagates through `GridDelta`). Full interactive
session. End-to-end: type in GUI → shell responds → renders live.

### Phase G — CLI Client (`end cli`)
`end --cli list`, `end --cli send-text`, `end --cli spawn`. Same socket, same PDU
protocol, no GUI dependency. Natural follow-on — no daemon changes required.

---

## 8. Handoff Notes for COUNSELOR

- **ARCHITECTURE.md**, **Session.h**, **TTY.h**, **AppState.h/cpp**, **Grid.h**
  are the authoritative current-state documents. Read all before touching anything.
- `AppState::save()` / `load()` handle layout persistence today. Phase E integrates
  with this — do not replace it.
- `Grid::dirtyRows[4]` atomics already exist. Phase B adds `seqno` only — additive.
- `Context<T>` is the established singleton pattern. Use for `MuxServer` and
  `SessionPool`. Not Meyer's singletons.
- `PerPane` server-side shadow state is **explicitly not ported**. Seqno replaces it.
- LEB128: read one byte at a time, accumulate 7-bit groups, high bit = more. ~30 lines.
  Extend `jreng_binary_data.h` in `jreng_core`.
- No external serialization dependency. Hand-written only.
- macOS-first. Linux second. Windows named pipe: future RFC.
- Q1–Q6 must be resolved by ARCHITECT before Phase A begins.
- Auditor pass on Phase D resize sync is non-negotiable.
- Phase G (`end cli`) requires no daemon changes — it's a client sprint only.

---

*JRENG! Rock 'n Roll.*
