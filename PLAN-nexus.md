# PLAN-nexus.md

## Sprint Objective

Unify `Terminal::Session` + `Nexus::Host` + `Nexus::RemoteSession` into one `Terminal::Processor` class with two constructors (local, hosted). Rename the wire protocol to JUCE-literal conventions (`getStateInformation` / `setStateInformation`, `Nexus::StateInfo`). Fix seven correctness bugs. Support N>1 clients per sidecar.

**Primary source RFCs:** `RFC-NEXUS.md`, `RFC-STATE.md`
**Preceding log:** `carol/SPRINT-LOG.md` (RFC-NEXUS consumption, Pathfinder recons, API-gap discoveries)

---

## Locked Decisions (from ARCHITECT discussion)

### Architecture

| # | Decision | Rationale |
|---|---|---|
| L1 | One class `Terminal::Processor`, two ctors (local, hosted). No separate `Proxy`/`Mirror`/`Stub` subclass. Proxy-ness is structural (constructor choice). | BLESSED-E (Explicit), matches JUCE `AudioProcessor` pattern (standalone vs plugin via same class). |
| L2 | `state` and `grid` are **public members** on `Terminal::Processor`. Display reads `processor.state.*` and `processor.grid.*` directly. | 1:1 with `juce::AudioProcessorEditor::processor` (`juce_AudioProcessorEditor.h:68`). No unnecessary getters (MANIFESTO Stateless, Encapsulation). |
| L3 | IPC state transfer methods: `Nexus::StateInfo Processor::getStateInformation (uint64_t sinceSeqno) const noexcept;` and `void Processor::setStateInformation (const Nexus::StateInfo& source) noexcept;` — 1:1 parallel to JUCE `getStateInformation` / `setStateInformation`. Mode-neutral (same signature on real and proxy Processor). | Constraint: "use JUCE convention and method exactly." |
| L4 | Wire payload type: `Nexus::StateInfo` (lives in `Source/nexus/StateInfo.h`). Replaces `Nexus::GridDelta`. | Names a bundle of processor state, not a transmission optimization. Honest noun. |
| L5 | `Nexus::StateInfo` uses `juce::HeapBlock<DirtyRow>` for rows and `juce::HeapBlock<Terminal::Cell>` / `juce::HeapBlock<Terminal::Grapheme>` for cell content. No `std::vector` in wire path. Two-pass count-then-fill allocation. | Project convention for trivially copyable types. |
| L6 | `Grid::buildDelta` deleted. Replaced by `Grid::collectDirtyRows (uint64_t sinceSeqno) const noexcept` which returns row data only — Grid no longer reads `Terminal::State` or `Nexus::*` types. Full `StateInfo` assembly moves to `Processor::getStateInformation`. | BLESSED-E (Encapsulation): Grid knows cells, not IPC payloads. Eliminates `const_cast<State&>` smell at `GridDelta.cpp:59`. |
| L7 | **New Grid API additions (both mandatory):** `Grid::writeScrollbackRow (int offset, const Cell* cells, const Grapheme* graphemes, int count) noexcept;` and `Grid::setSeqno (uint64_t value) noexcept;` — proxy requires both for correctness (scrollback sync and seqno alignment). | Gap discovered by Pathfinder — no public writer API for scrollback rows; `seqno` is private atomic with no setter. |
| L8 | `Source/terminal/logic/GridDelta.cpp` **deleted**. Its logic moves to `Processor::getStateInformation` in `Processor.cpp`. | Function no longer belongs on Grid. |
| L9 | `Source/nexus/Pdu.h` → `Source/nexus/Message.h`. `enum class PduKind` → `enum class Nexus::Message` (flat, no namespace/nested Kind). | JUCE vocabulary (`messageReceived`, `sendMessage`). Drops OSI networking jargon. |
| L10 | `ServerConnection` ownership: `jreng::Owner<ServerConnection> connections;` on `Nexus::Server`. `createConnectionObject()` does `return connections.add (std::make_unique<ServerConnection> (session)).get();`. `connectionLost()` → `server.removeConnection(this)` → `connections.remove (connections.indexOf (c))`. Derived `~ServerConnection()` calls `disconnect()` per JUCE jassert at `juce_InterprocessConnection.cpp:97`. | Librarian verified `InterprocessConnectionServer` base is ownership-opaque (`juce_InterprocessConnectionServer.cpp:87-88` fires-and-forgets). `jreng::Owner` is the project's modern smart-pointer equivalent to `juce::OwnedArray`. |
| L11 | `Nexus::Server` owns connections. `Nexus::Session` holds non-owning `std::vector<ServerConnection*> attached;` populated by `connectionMade(self)` → `session.attach(self)` and drained by `connectionLost(self)` → `session.detach(self)`. Broadcast fan-out iterates `attached`. | Separation of ownership and broadcast intent (BLESSED-E Encapsulation). |
| L12 | `Nexus::Client` uses `juce::InterprocessConnection (true, magicHeader)` — `callbacksOnMessageThread = true`. All raw-`this` `juce::MessageManager::callAsync` patterns in `Client::messageReceived` eliminated. JUCE's internal `SafeAction` marshals to message thread; lifetime safety comes from JUCE, not manual guards. | Fixes Bug 2 (shutdown use-after-free). JUCE-native safety. |
| L13 | `Terminal::Processor::startShell() noexcept;` — new explicit PTY-lifecycle method. Local-mode ctor builds TTY+Parser but does not open. `startShell()` opens PTY at the cols/rows currently in `state`. `resized()` becomes pure SIGWINCH on a running PTY (precondition: `tty->isThreadRunning()`, assert). `callAsync` deferral at `Session.cpp:251-281` deleted. | Fixes Bug 3 by construction. Eliminates implicit PTY-open-inside-resize coupling. |
| L14 | Multi-subscriber (N>1 clients per sidecar) is in scope. Per-subscriber `lastKnownSeqno` tracking lives as ValueTree subscriber children under each sidecar Processor's `Terminal::State`. Each attached `ServerConnection` corresponds to one child, written via new typed State setters. Migration from `Nexus::Host::lastFanoutSeqno` (`Host.h:290`, per-session watermark shared across subscribers). | Consistent with project idiom ("state lives in `Terminal::State` as ValueTree"). Sidecar exit decouples from connection count (exits when 0 Processors remain, not 0 connections). |
| L15 | `LoaderOverlay` reused for connection-wait UX. `show (0, "Connecting to nexus")` on nexus-mode startup before first `connectionMade`; `hide()` on `connectionMade`. Indeterminate mode (`totalCount == 0`) renders spinner + message without progress bar (`LoaderOverlay.h:59`). | Reuses existing component. No new UI work. |
| L16 | Thread annotation sweep scope: **entire codebase**. Every `// READER THREAD` / `// MESSAGE THREAD` comment → `// PRODUCER CONTEXT` / `// CONSUMER CONTEXT`. | Honest across both modes (hosted producer is message thread, local producer is reader thread — producer/consumer is the mode-neutral framing). |
| L17 | `AppState::isClientMode` **kept**. Panes (3 sites) and Tabs (1 site) read it at Processor-construction points to decide local vs hosted ctor. Main/MainComponent startup wiring (2 sites) unchanged. | Smaller blast radius than injecting a factory into every consumer. ARCHITECT ruling. |
| L18 | Wire protocol: **no backwards compatibility**. Single-binary deployment; sidecar and GUI always from the same build. | Simpler framing, no version negotiation. |

### Naming

| Old | New |
|---|---|
| `Terminal::Session` | `Terminal::Processor` |
| `Nexus::Host` | `Nexus::Session` |
| `Nexus::HostFanout` | `Nexus::SessionFanout` |
| `Nexus::RemoteSession` | *(absorbed into `Terminal::Processor` hosted ctor; file deleted)* |
| `Nexus::GridDelta` | `Nexus::StateInfo` |
| `Nexus::CellRange` | `Nexus::DirtyRow` |
| `Grid::buildDelta` | `Grid::collectDirtyRows` |
| `Processor::applyDelta` *(never existed; was Mirror/Frame intermediate)* | `Processor::setStateInformation` |
| *(new)* | `Processor::getStateInformation` |
| *(new)* | `Processor::startShell` |
| *(new)* | `Grid::writeScrollbackRow`, `Grid::setSeqno` |
| `Source/nexus/Pdu.h` | `Source/nexus/Message.h` |
| `enum class PduKind` | `enum class Nexus::Message` |
| `PduKind::renderDelta` | `Message::stateInfo` |
| `PduKind::getRenderDelta` | *deleted* (tell-don't-ask) |
| `PduKind::listSessions` / `listSessionsResponse` | *deleted* — replaced by push-on-connect `Message::processorList` |
| `PduKind::spawnSession` / `attachSession` / `detachSession` / `sessionExited` | `Message::spawnProcessor` / `attachProcessor` / `detachProcessor` / `processorExited` |
| `// READER THREAD`, `// MESSAGE THREAD` (annotations) | `// PRODUCER CONTEXT`, `// CONSUMER CONTEXT` |

### New State setters to add (L14 + setStateInformation)

Following existing `Terminal::State` setter idiom (typed setters with `ActiveScreen` parameter where applicable, direct mutators otherwise):

| Setter | Purpose |
|---|---|
| `State::setLastKnownSeqno (uint64_t)` | Proxy-side bookkeeping. Called from `Processor::setStateInformation`. |
| `State::setSubscriberSeqno (juce::StringRef subscriberId, uint64_t)` | Sidecar per-subscriber tracking. Writes into `ID::subscribers` ValueTree child. |
| `State::attachSubscriber (juce::StringRef subscriberId)` | Adds child under `ID::subscribers` with `lastKnownSeqno = 0`. |
| `State::detachSubscriber (juce::StringRef subscriberId)` | Removes child. |
| `State::getSubscriberSeqno (juce::StringRef subscriberId) const` | Read seqno for fan-out build (only getter added — proven caller in `SessionFanout`). |

New `App::ID::*` identifiers: `subscribers`, `subscriberId`, `lastKnownSeqno`.

---

## Bug Inventory (from RFC-NEXUS.md)

| # | Name | Fix location | Status |
|---|---|---|---|
| Bug 1 | `ServerConnection` leak / stale broadcast pointers | Step 4 | `jreng::Owner` + event-driven attach/detach |
| Bug 2 | `Client::messageReceived` raw-`this` lambda UAF on shutdown | Step 5 | `callbacksOnMessageThread = true`, no manual async |
| Bug 3 | PTY open race (empty first `StateInfo` on spawn) | Step 12 | Explicit `startShell()`, delete `callAsync` deferral |
| Bug 4 | Reflow delta loss (rows dirtied before client connects) | Step 13 | `getStateInformation(0)` builds full snapshot on first `attachProcessor` — verified by L6 reshape |
| Bug 5 | Graphemes not serialized across process | Step 13 | `setStateInformation` iterates `LAYOUT_GRAPHEME` cells and calls `activeWriteGrapheme` (spec in Step 8) |
| Bug 6 | Wide-char boundary corruption (`LAYOUT_WIDE_CONT` torn) | Step 13 | Full-row `activeWriteRun` writes wide-cont cells atomically — verified by L7 Grid API audit |
| Bug 7 | Grapheme sidecar not written in `applyDelta` (the current `RemoteSession.cpp:88-94` bug) | Step 8 | `setStateInformation` writes graphemes inside row loop (spec below) |

---

## Per-step Auditor Checklist (contract L19)

Applied to every step:

### Mechanical (all steps)
- Clean build, zero warnings
- Grep forbidden tokens list — zero matches in touched files
- Grep `return;` / early `return` in touched files — zero outside final statement
- Grep `callAsync` in touched `Nexus::*` files — zero outside SafeAction contexts
- Grep `new ` without immediate `unique_ptr` / `jreng::Owner::add` / JUCE base-class transfer — zero
- Grep `\\[\\]` on containers — zero; `.at()` required (JRENG-CODING-STANDARD L492)
- Grep `&&`, `||`, `!` as logical operators — zero; `and`, `or`, `not` required
- `{` on new line (JRENG formatting)
- `noexcept` on non-throwing leaf functions
- Brace initialization: `int x { 0 }` not `int x = 0`
- `// READER THREAD` / `// MESSAGE THREAD` survivors — zero (after Step 0)

### Per-step semantic checks — see each step below.

### Forbidden tokens (running list, zero tolerance after the step that eliminates each)

| Token | Eliminated by |
|---|---|
| `Terminal::Session` | Step 1 |
| `Nexus::Host` (as type) | Step 2 |
| `Pdu`, `PduKind` | Step 3 |
| `RemoteSession`, `Nexus::RemoteSession` | Step 8 |
| `GridDelta`, `Nexus::GridDelta`, `buildDelta`, `renderDelta` | Step 6–8 |
| `applyDelta` | Step 8 |
| `getRenderDelta`, `listSessions`, `listSessionsResponse` | Step 9 |
| `lastFanoutSeqno` | Step 10 |
| `Thread::sleep` in `Main.cpp` startup path | Step 14 |
| `// READER THREAD`, `// MESSAGE THREAD` (annotations) | Step 0 |
| `CellRange` (as `Nexus::CellRange`) | Step 7 |

### Human smoke test (after every step)
Build + launch + spawn a terminal + type a command + scroll + split pane + close. Step passes only if this loop works.

---

## Execution Plan

Each step: objective, files touched, delegation target (Engineer unless noted), Auditor semantic checklist specific to the step.

---

### Step 0 — Thread annotation sweep (codebase-wide)

**Objective:** Rename every `// READER THREAD` and `// MESSAGE THREAD` comment annotation to `// PRODUCER CONTEXT` and `// CONSUMER CONTEXT` respectively. Honest across both modes (hosted producer = message thread, local producer = reader thread). No behavioral change.

**Delegate:** @Pathfinder first (enumerate every annotation site in the codebase), then @Engineer (mechanical rename).

**Files touched:** every file with the annotations. Pathfinder sweep will enumerate. Known sites include `Source/terminal/logic/Grid.h`, `Source/terminal/data/State.h`, and others.

**Auditor semantic checks:**
- Grep `// READER THREAD` — zero matches
- Grep `// MESSAGE THREAD` — zero matches
- Grep `// PRODUCER CONTEXT` — present on every writer method of Grid/State
- Grep `// CONSUMER CONTEXT` — present on every reader method of Grid/State
- No code changes — only comments (diff should be comment-only)

---

### Step 1 — Rename `Terminal::Session` → `Terminal::Processor`

**Objective:** Pure mechanical rename of the class, its header/source files, and every consumer. No behavioral change. No signature change. No member additions.

**Delegate:** @Engineer.

**Files touched** (from Pathfinder sweep):
- `Source/terminal/logic/Session.h` → `Source/terminal/logic/Processor.h`
- `Source/terminal/logic/Session.cpp` → `Source/terminal/logic/Processor.cpp`
- `Source/nexus/Host.h` (type references)
- `Source/nexus/Host.cpp:51,103,116`
- `Source/nexus/HostFanout.cpp:51` (include + type refs)
- `Source/nexus/ServerConnection.cpp:12`
- `Source/component/TerminalDisplay.h:44`
- `Source/component/MouseHandler.cpp:13` + `MouseHandler.h:169` (member type)
- `Source/component/InputHandler.cpp:9` + `InputHandler.h:183` (member type)

**Auditor semantic checks:**
- Grep `Terminal::Session` — zero matches codebase-wide
- Grep `terminal/logic/Session.h` in includes — zero; replaced by `Processor.h`
- Build succeeds
- No function signatures changed (only type names)

---

### Step 2 — Rename `Nexus::Host` → `Nexus::Session`, `Nexus::HostFanout` → `Nexus::SessionFanout`

**Objective:** Mechanical rename. No behavioral change.

**Delegate:** @Engineer.

**Files touched:**
- `Source/nexus/Host.h` → `Source/nexus/Session.h`
- `Source/nexus/Host.cpp` → `Source/nexus/Session.cpp`
- `Source/nexus/HostFanout.cpp` → `Source/nexus/SessionFanout.cpp`
- `Source/Main.cpp:52,193,245,252,388`
- `Source/MainComponent.cpp:30,546,547`
- `Source/nexus/Server.cpp:11` + `Server.h:20,21,26` (include + forward decl + member)
- `Source/nexus/ServerConnection.cpp:10`
- `Source/component/Tabs.cpp:14,278`
- `Source/component/Panes.cpp:14,65,66,361,405,406`
- `Source/component/Popup.cpp:15,172`
- `Source/component/TerminalDisplay.h:50`
- `Source/component/TerminalDisplay.cpp:84,96`

**Auditor semantic checks:**
- Grep `Nexus::Host` — zero matches
- Grep `HostFanout` — zero matches
- Grep `nexus/Host.h` — zero matches
- Build succeeds

---

### Step 3 — Rename `Source/nexus/Pdu.h` → `Message.h`, `PduKind` → `Nexus::Message`

**Objective:** Mechanical rename of the wire-discriminator enum and its header. Flatten `enum class PduKind` → `enum class Nexus::Message` (no nested `Kind`).

**Delegate:** @Engineer.

**Files touched:**
- `Source/nexus/Pdu.h` → `Source/nexus/Message.h`
- `Source/nexus/Pdu.cpp` → `Source/nexus/Message.cpp` (if exists)
- Every `#include "nexus/Pdu.h"` → `#include "nexus/Message.h"`
- Every `PduKind::xxx` → `Message::xxx`
- Enum values updated per naming table:
  - `spawnSession` → `spawnProcessor`
  - `attachSession` → `attachProcessor`
  - `detachSession` → `detachProcessor`
  - `sessionExited` → `processorExited`
  - `renderDelta` → `stateInfo`
  - `getRenderDelta`, `listSessions`, `listSessionsResponse` — **not deleted yet** (Step 9); retained for now with renamed prefixes if needed, or kept as-is since this step is mechanical only.

**Clarification:** Step 3 renames values that map 1:1. Values that will be deleted in Step 9 (`getRenderDelta`, `listSessions*`) are left untouched in Step 3 to keep the step purely mechanical.

**Auditor semantic checks:**
- Grep `Pdu`, `PduKind` — zero matches
- Grep `nexus/Pdu.h` in includes — zero
- `Source/nexus/Message.h` exists and declares `enum class Nexus::Message`
- Build succeeds

---

### Step 4 — Bug 1: `jreng::Owner<ServerConnection>` on `Nexus::Server`

**Objective:** Fix connection leak and stale broadcast pointers. Introduce event-driven attach/detach.

**Delegate:** @Engineer.

**Files touched:**
- `Source/nexus/Server.h` — add `jreng::Owner<ServerConnection> connections;` member, add `removeConnection (ServerConnection*)` method, include `<jreng_owner.h>`
- `Source/nexus/Server.cpp:133-136` — rewrite `createConnectionObject()` to `return connections.add (std::make_unique<ServerConnection> (session)).get();`
- `Source/nexus/ServerConnection.h` — add `~ServerConnection() override;` if not present, add `void connectionLost() override;`
- `Source/nexus/ServerConnection.cpp` — implement:
  ```cpp
  ServerConnection::~ServerConnection()
  {
      disconnect();
  }

  void ServerConnection::connectionLost()
  {
      session.detach (*this);
      server.removeConnection (this);
  }

  void ServerConnection::connectionMade()
  {
      session.attach (*this);
  }
  ```
- `Source/nexus/Session.h` — replace `Host::connections` raw vector with `std::vector<ServerConnection*> attached;` (non-owning), add `attach (ServerConnection&)` / `detach (ServerConnection&)` methods. Locking unchanged (`connectionsLock` mutex preserved).
- `Source/nexus/Session.cpp` — implement attach/detach.

**ServerConnection** now holds `Nexus::Server& server;` reference in addition to `Nexus::Session& session;`. Ctor updated.

**Auditor semantic checks:**
- `Nexus::Server` has exactly one field owning connections: `jreng::Owner<ServerConnection> connections`
- `Nexus::Session` has no owning storage for connections
- `createConnectionObject()` uses `connections.add(...).get()` idiom (one line)
- `~ServerConnection()` calls `disconnect()` (satisfies jassert at `juce_InterprocessConnection.cpp:97`)
- `connectionLost()` removes from both Session's broadcast set and Server's owner
- `connectionMade()` adds to Session's broadcast set
- No `new ServerConnection` appears except inside `connections.add (std::make_unique<...>)` expression
- Launch sidecar, connect, disconnect, repeat 10x — no leaks (valgrind or leak-detector macro)
- Launch two clients simultaneously — both receive fan-out

---

### Step 5 — Bug 2: `Client::InterprocessConnection (true, ...)`, eliminate raw-`this` lambdas

**Objective:** Fix shutdown UAF in `Client::messageReceived`. Use JUCE's built-in `callbacksOnMessageThread = true` and `SafeAction` marshaling instead of manual `callAsync([this,...])`.

**Delegate:** @Engineer.

**Files touched:**
- `Source/nexus/Client.cpp:126-128` — change ctor from `InterprocessConnection (false, magicHeader)` to `InterprocessConnection (true, magicHeader)`.
- `Source/nexus/Client.cpp:569,594` — remove both `juce::MessageManager::callAsync` calls from `messageReceived`. Inline the lambda bodies directly since we're already on the message thread.
- `Source/nexus/Client.cpp:213,220` — delete `listSessionsEvent.reset()` and `listSessionsEvent.wait(...)` — the synchronous wait pattern is incompatible with message-thread callbacks (would deadlock the message thread waiting for itself). The `listSessions` request path is deleted in Step 9; in this step, leave a `jassert (false && "listSessions eliminated — see Step 9")` placeholder and ensure no current caller hits it.
- `Source/nexus/Client.h:231` — remove `juce::WaitableEvent listSessionsEvent;`
- `Source/nexus/Client.h` — remove `pendingListResult` / `listSessionsLock` members if present.

**Note:** Steps 5 and 9 are tightly coupled. Step 5 removes the async machinery; Step 9 eliminates the listSessions protocol that needed it. The jassert placeholder bridges the gap until Step 9 lands.

**Auditor semantic checks:**
- `Client::Client()` initializer uses `InterprocessConnection (true, magicHeader)`
- `Client::messageReceived` contains zero `MessageManager::callAsync` calls
- `Client::messageReceived` contains no raw-`this` captures in lambdas (because there are no lambdas)
- `listSessionsEvent`, `pendingListResult`, `listSessionsLock` — zero references
- Shutdown stress test: launch, spawn 5 terminals, close window — no UAF, no crash (run with ASan)

---

### Step 6 — Grid additions: `writeScrollbackRow`, `setSeqno`, `collectDirtyRows`; delete `buildDelta`

**Objective:** Add the two new Grid writer APIs required for proxy correctness, introduce the new collector that replaces `buildDelta`, and delete the old `buildDelta` path. Grid stops reading `Terminal::State` and stops knowing about `Nexus::*` types.

**Delegate:** @Engineer.

**Files touched:**
- `Source/terminal/logic/Grid.h` — add three public methods (annotations per Step 0 vocabulary):
  ```cpp
  // PRODUCER CONTEXT
  void writeScrollbackRow (int offset,
                           const Terminal::Cell* cells,
                           const Terminal::Grapheme* graphemes,
                           int count) noexcept;

  // PRODUCER CONTEXT
  void setSeqno (uint64_t value) noexcept;

  // CONSUMER CONTEXT (no State reads; returns a HeapBlock of DirtyRow)
  juce::HeapBlock<Nexus::DirtyRow> collectDirtyRows (uint64_t sinceSeqno,
                                                      int& rowCountOut) const noexcept;
  ```
  Remove `Grid::buildDelta` declaration.

- `Source/terminal/logic/Grid.cpp`:
  - Implement `writeScrollbackRow`: compute `physRow = (buffer.head - visibleRows + 1 - offset) & buffer.rowMask;` (mirrors `GridDelta.cpp:67` read path), `std::memcpy` cells and graphemes into `buffer.cells` / `buffer.graphemes` at the physical offset, set `buffer.rowSeqnos[physRow]` by incrementing `seqno` and writing new value, and if `offset > buffer.scrollbackUsed` set `buffer.scrollbackUsed = offset`. Guard: `offset >= 1 and count == cols`. `jassert` on violation.
  - Implement `setSeqno`: `seqno.store (value, std::memory_order_relaxed);`
  - Implement `collectDirtyRows`: two-pass. Pass 1 counts rows where `rowSeqnos[physRow] > sinceSeqno` (scrollback + visible). Pass 2 allocates `juce::HeapBlock<Nexus::DirtyRow> result (count);`, fills each `DirtyRow` with allocated `HeapBlock<Cell>(cols)` / `HeapBlock<Grapheme>(cols)` + memcpy from buffer. Returns via out param + ptr.
- `Source/terminal/logic/GridDelta.cpp` — **delete file entirely**. Remove from any Projucer/CMake file list.
- `Source/nexus/StateInfo.h` — must exist before Step 6 can declare `collectDirtyRows` return type. **Dependency:** Step 7 (StateInfo type definition) logically precedes Step 6. Reorder: **Step 6 executes after Step 7.** (Renumbered below in "Execution order" note.)

**Note on ordering:** because `Grid::collectDirtyRows` signature depends on `Nexus::DirtyRow` (defined in StateInfo.h), the actual dependency is:
1. Step 7 creates `Source/nexus/StateInfo.h` with `DirtyRow` declaration (no Grid dependency).
2. Step 6 adds Grid methods that return `HeapBlock<DirtyRow>`, includes `nexus/StateInfo.h`.

Alternative to avoid Grid including a Nexus header: use a pure Terminal-domain struct `Terminal::ChangedRow { int logicalRow; int cols; HeapBlock<Cell> cells; HeapBlock<Grapheme> graphemes; };` as Grid's return type. Processor's `getStateInformation` converts to `Nexus::DirtyRow` (which can be an alias or structurally identical struct). This keeps Grid with zero Nexus dependency (BLESSED-E stronger).

**ARCHITECT decision needed (before Step 6):** does Grid include `nexus/StateInfo.h` (simpler, one less struct), or does Grid use a pure `Terminal::ChangedRow` struct that Processor converts (stronger encapsulation)? **Pending.**

**Auditor semantic checks:**
- `Grid::writeScrollbackRow` and `Grid::setSeqno` declared public, implemented, tested by round-trip (write scrollback row, read back via existing `scrollbackRow` getter, verify byte-identical)
- `Grid::buildDelta` — zero references codebase-wide
- `GridDelta.cpp` file does not exist
- Grid.cpp does not include `terminal/data/State.h` in its `collectDirtyRows` function (no state reads)
- Grid.cpp does not include `nexus/StateInfo.h` (if ChangedRow option chosen) OR includes it (if direct option chosen)

---

### Step 7 — `Nexus::StateInfo` type (HeapBlock-based)

**Objective:** Define the new wire payload type. Replace `Nexus::GridDelta` and `Nexus::CellRange` definitions.

**Delegate:** @Engineer.

**Files touched:**
- `Source/nexus/GridDelta.h` → `Source/nexus/StateInfo.h`. New contents:
  ```cpp
  #pragma once
  #include <JuceHeader.h>
  #include "../terminal/data/Cell.h"

  namespace Nexus
  {
      struct DirtyRow
      {
          int                              logicalRow { 0 };  // negative = scrollback (-1 = 1 above viewport)
          int                              cols       { 0 };
          juce::HeapBlock<Terminal::Cell>      cells;
          juce::HeapBlock<Terminal::Grapheme>  graphemes;
      };

      struct StateInfo
      {
          uint64_t                    seqno          { 0 };
          uint16_t                    cols           { 0 };
          uint16_t                    rows           { 0 };
          uint16_t                    cursorRow      { 0 };
          uint16_t                    cursorCol      { 0 };
          bool                        cursorVisible  { true };
          uint8_t                     screenIndex    { 0 };
          int                         scrollbackUsed { 0 };
          juce::String                title;
          juce::String                cwd;
          int                         dirtyRowCount  { 0 };
          juce::HeapBlock<DirtyRow>   dirtyRows;
      };
  }
  ```
- Any include `#include "nexus/GridDelta.h"` → `#include "nexus/StateInfo.h"`. Any `Nexus::GridDelta` → `Nexus::StateInfo`. Any `Nexus::CellRange` → `Nexus::DirtyRow`.

**Auditor semantic checks:**
- `Nexus::GridDelta` — zero matches
- `Nexus::CellRange` — zero matches
- `std::vector` — zero matches inside `StateInfo.h`
- `Nexus::StateInfo` uses `juce::HeapBlock` for `dirtyRows`, `DirtyRow::cells`, `DirtyRow::graphemes`
- Brace initialization for every field

---

### Step 8 — `Terminal::Processor` two-mode (absorb `RemoteSession`)

**Objective:** The largest architectural step. Fold `Nexus::RemoteSession` into `Terminal::Processor` via a second constructor. Introduce public `state` / `grid` members. Implement `getStateInformation` / `setStateInformation` / `startShell`. Change `resized()` to pure SIGWINCH. Make Processor a `juce::ChangeBroadcaster`. Bug 5, 6, 7 fixed inside `setStateInformation`.

**Delegate:** @Engineer with Pathfinder support if existing Processor members need reshuffling.

**Files touched:**
- `Source/terminal/logic/Processor.h` (the post-Step-1 rename):
  - Make `Processor` inherit `juce::ChangeBroadcaster`.
  - Move `state` and `grid` to **public** section. Preserved types (`Terminal::State`, `Terminal::Grid`).
  - Add `const juce::String uuid;` public member (const, set in ctor). Local ctor generates via `juce::Uuid().toString()`; hosted ctor receives from caller.
  - Add two constructors:
    ```cpp
    /** Local-mode ctor: owns a PTY + Parser. Does NOT open PTY — call startShell() explicitly. */
    Processor (Terminal::State& appState,
               int cols, int rows,
               const juce::String& shell);

    /** Hosted-mode ctor: no PTY, no Parser. Proxies state from a remote Processor. */
    Processor (Terminal::State& appState,
               int cols, int rows,
               const juce::String& existingUuid);
    ```
  - Add methods:
    ```cpp
    // PRODUCER CONTEXT (reader thread in local mode; message thread in hosted mode)
    Nexus::StateInfo getStateInformation (uint64_t sinceSeqno) const noexcept;

    // CONSUMER CONTEXT (message thread — called from Client::messageReceived on GUI side)
    void setStateInformation (const Nexus::StateInfo& source) noexcept;

    /** Open the PTY and spawn the shell. Local-mode only. Hosted-mode: no-op + jassert. */
    void startShell() noexcept;

    /** SIGWINCH to running PTY (local mode) or forward resize message (hosted mode). */
    void resized (int cols, int rows) noexcept;
    ```
  - Remove `onHostDataReceived`, `onHostShellExited` — replaced by `juce::ChangeBroadcaster::sendChangeMessage()` (Display subscribes via `addChangeListener`).
  - Remove `getSession()` / `getDisplayState()` / `getDisplayGrid()` private helpers — replaced by public `state` / `grid`.

- `Source/terminal/logic/Processor.cpp`:
  - Local ctor: construct State, Grid, TTY, Parser. Do not open PTY.
  - Hosted ctor: construct State, Grid with given cols/rows. Do not construct TTY/Parser.
  - `startShell()` body:
    ```cpp
    void Processor::startShell() noexcept
    {
        jassert (tty != nullptr);  // precondition: local mode
        if (tty != nullptr)
        {
            const auto cols { grid.getCols() };
            const auto rows { grid.getVisibleRows() };
            tty->open (cols, rows, shell);
        }
    }
    ```
  - `resized (cols, rows)`:
    ```cpp
    void Processor::resized (int cols, int rows) noexcept
    {
        grid.resize (cols, rows);

        if (tty != nullptr)
        {
            if (parser != nullptr)
                parser->resize (cols, rows);

            if (tty->isThreadRunning())
                tty->platformResize (cols, rows);
        }
    }
    ```
    **No `callAsync`. No first-call branch.** Precondition: if `tty != nullptr`, caller has already invoked `startShell()`.
  - `getStateInformation(sinceSeqno)` body — absorbs the old `GridDelta.cpp` logic:
    ```cpp
    Nexus::StateInfo Processor::getStateInformation (uint64_t sinceSeqno) const noexcept
    {
        Nexus::StateInfo info;
        info.seqno          = grid.currentSeqno();
        info.cols           = static_cast<uint16_t> (grid.getCols());
        info.rows           = static_cast<uint16_t> (grid.getVisibleRows());
        info.cursorRow      = static_cast<uint16_t> (state.getCursorRow (state.getActiveScreen()));
        info.cursorCol      = static_cast<uint16_t> (state.getCursorCol (state.getActiveScreen()));
        info.cursorVisible  = state.isCursorVisible (state.getActiveScreen());
        info.screenIndex    = static_cast<uint8_t> (state.getActiveScreen());
        info.scrollbackUsed = state.getScrollbackUsed();
        info.title          = state.getTitle();
        info.cwd            = state.getCwd();

        int rowCount { 0 };
        info.dirtyRows     = grid.collectDirtyRows (sinceSeqno, rowCount);
        info.dirtyRowCount = rowCount;
        return info;
    }
    ```
  - `setStateInformation(source)` body — the receiver spec locked in D5 (Bug 5, 6, 7 fix):
    ```cpp
    void Processor::setStateInformation (const Nexus::StateInfo& source) noexcept
    {
        // 1. Geometry
        if (source.cols != grid.getCols() or source.rows != grid.getVisibleRows())
            grid.resize (source.cols, source.rows);

        // 2. Buffer switch
        state.setScreen (static_cast<Terminal::ActiveScreen> (source.screenIndex));

        // 3. Scalar State updates via typed setters
        const auto s { state.getActiveScreen() };
        state.setCursorRow       (s, source.cursorRow);
        state.setCursorCol       (s, source.cursorCol);
        state.setCursorVisible   (s, source.cursorVisible);
        state.setScrollbackUsed  (source.scrollbackUsed);
        state.setTitle           (source.title.toRawUTF8(), source.title.length());
        state.setCwd             (source.cwd.toRawUTF8(),   source.cwd.length());
        state.setLastKnownSeqno  (source.seqno);   // new setter added in Step 10

        // 4. Row content
        for (int r { 0 }; r < source.dirtyRowCount; ++r)
        {
            const auto& row { source.dirtyRows[r] };

            if (row.logicalRow >= 0)
            {
                // Visible row — cells first, then graphemes (order matters, see L7 audit)
                grid.activeWriteRun (row.logicalRow, 0, row.cells.getData(), row.cols);

                for (int c { 0 }; c < row.cols; ++c)
                {
                    if ((row.cells[c].layout bitand Terminal::Cell::LAYOUT_GRAPHEME) != 0)
                        grid.activeWriteGrapheme (row.logicalRow, c, row.graphemes[c]);
                }
            }
            else
            {
                // Scrollback row
                grid.writeScrollbackRow (- row.logicalRow,
                                         row.cells.getData(),
                                         row.graphemes.getData(),
                                         row.cols);
            }
        }

        // 5. Align local seqno with sidecar's
        grid.setSeqno (source.seqno);

        // 6. Notify Display
        sendChangeMessage();
    }
    ```
  - Note: no early returns anywhere. Positive nested checks as the JRENG contract requires. The `if (source.cols != grid.getCols() or ...)` guard is the only branch; its body is the positive action, no `else return`.

- `Source/nexus/RemoteSession.h`, `Source/nexus/RemoteSession.cpp` — **deleted** in Step 16 (cleanup). In Step 8, their contents are absorbed; Panes/TerminalDisplay references migrated in Step 11.

- `Source/component/TerminalDisplay.h` — add `Terminal::Processor& processor;` public member, make Display inherit `juce::ChangeListener`, implement `changeListenerCallback` to call `setSnapshotDirty` / trigger repaint. Remove `remoteSession` member (absorbed). Remove `getSession`/`getDisplayState`/`getDisplayGrid` helpers.
- `Source/component/TerminalDisplay.cpp` — Display ctor takes `Processor&` instead of `Session&` + uuid; calls `processor.addChangeListener(this);` in ctor, `removeChangeListener(this);` in dtor.

**Auditor semantic checks:**
- `Terminal::Processor` has public `state`, `grid`, `uuid` members
- `Terminal::Processor` inherits `juce::ChangeBroadcaster`
- Both ctors present with exact signatures above
- `getStateInformation` / `setStateInformation` / `startShell` implemented
- `resized()` contains zero `callAsync` calls, zero `isThreadRunning` first-call branch, zero `jassert (not tty->isThreadRunning())` nested-callAsync pattern
- `setStateInformation` writes cells before graphemes (Bug 7 fix verified against L7 Pathfinder order)
- `setStateInformation` handles scrollback rows via `writeScrollbackRow`
- `setStateInformation` contains zero early returns
- `Display` inherits `juce::ChangeListener` and has public `Terminal::Processor& processor;`
- `Display` contains no `sessionId` member (reads `processor.uuid` instead)
- `Display::getSession` / `getDisplayState` / `getDisplayGrid` — zero references
- `onHostDataReceived` / `onHostShellExited` — zero references
- Build succeeds
- Smoke: spawn terminal, type, see output — local mode working

---

### Step 9 — Tell-don't-ask wire protocol

**Objective:** Eliminate request/response pairs `getRenderDelta` and `listSessions` / `listSessionsResponse`. Sidecar pushes state unsolicited. On `connectionMade`, sidecar pushes `processorList` containing the uuid list of currently-existing Processors. On any state change, sidecar pushes `stateInfo` per subscriber.

**Delegate:** @Engineer.

**Files touched:**
- `Source/nexus/Message.h` — enum value updates:
  - Delete `getRenderDelta`, `listSessions`, `listSessionsResponse`
  - Add `processorList`
  - Final enum: `spawnProcessor`, `attachProcessor`, `detachProcessor`, `input`, `resize`, `stateInfo`, `processorList`, `processorExited`, plus preserved control values (`hello`, `ping`, `pong`, `shutdown`, etc.)
- `Source/nexus/Client.cpp` — delete `requestListSessions` / any synchronous request helper. `messageReceived` dispatch: handle `Message::processorList` (populate local proxy list), `Message::stateInfo` (route by uuid to the target Processor's `setStateInformation`), `Message::processorExited` (remove proxy), delete `Message::getRenderDelta` handling.
- `Source/nexus/ServerConnection.cpp` — in `connectionMade` (or equivalent post-handshake point), iterate `Nexus::Session::processors` and send a `processorList` message containing every current uuid. Delete any `getRenderDelta` handler; delete `listSessions` handler.
- `Source/nexus/SessionFanout.cpp` — on processor creation (via `spawnProcessor` handler), after Processor is constructed and shell started, push `processorList` update to all attached connections (or a delta-form announcing just the new uuid). Choice: either full-list push on every change, or per-processor `processorCreated` announcement. **Simpler: full `processorList` push on any change.**
- `Source/Main.cpp`, `Source/component/Panes.cpp` — remove any call to `client->requestListSessions()` or equivalent. Client's internal proxy list is populated by unsolicited push.

**Auditor semantic checks:**
- Grep `getRenderDelta` — zero
- Grep `listSessions` — zero
- Grep `listSessionsResponse` — zero
- `Message::processorList` declared and dispatched in both Client and ServerConnection
- `Client::messageReceived` has no synchronous request/wait pattern
- Smoke: launch sidecar with 2 pre-existing Processors, connect client, client shows both without requesting

---

### Step 10 — Multi-subscriber seqno tracking (ValueTree-based)

**Objective:** Migrate from `Nexus::Session::lastFanoutSeqno` (per-session map at `Host.h:290`) to per-subscriber tracking via ValueTree subscriber children on each sidecar Processor's `Terminal::State`. Enables N>1 clients.

**Delegate:** @Engineer.

**Files touched:**
- `Source/AppIdentifier.h` — add `DECLARE_ID (subscribers)`, `DECLARE_ID (subscriberId)`, `DECLARE_ID (lastKnownSeqno)`.
- `Source/terminal/data/State.h` / `State.cpp` — add new public methods:
  ```cpp
  // CONSUMER CONTEXT (message thread)
  void     attachSubscriber       (juce::StringRef subscriberId);
  void     detachSubscriber       (juce::StringRef subscriberId);
  void     setSubscriberSeqno     (juce::StringRef subscriberId, uint64_t seqno);
  uint64_t getSubscriberSeqno     (juce::StringRef subscriberId) const;

  // PRODUCER CONTEXT (proxy side only — single subscriber model)
  void     setLastKnownSeqno      (uint64_t seqno);
  uint64_t getLastKnownSeqno      () const;
  ```
  Implementation operates on `ID::subscribers` ValueTree child (a tree of `subscriberId, lastKnownSeqno` leaves). Follows existing State idiom: sidecar methods run on message thread, write directly to ValueTree.

- `Source/nexus/Session.cpp` — delete `lastFanoutSeqno` member (Host.h:290 equivalent). In `handleAsyncUpdate`:
  ```cpp
  for (auto* subscriber : attached)   // Nexus::Session::attached
  {
      for (auto& processor : processors)
      {
          const auto sub { subscriber->getId() };
          const auto since { processor.state.getSubscriberSeqno (sub) };
          const auto info  { processor.getStateInformation (since) };
          subscriber->sendStateInfo (processor.uuid, info);
          processor.state.setSubscriberSeqno (sub, info.seqno);
      }
  }
  ```
- `Source/nexus/ServerConnection.cpp` — in `connectionMade`: iterate `session.processors`, call `processor.state.attachSubscriber (this->getId())` for each. In `connectionLost`: reverse — `detachSubscriber` on every processor.
- `Source/nexus/Session.cpp` (spawnProcessor handler) — on new Processor creation, for every currently-attached subscriber, call `processor.state.attachSubscriber (sub->getId())` so the new processor enters fan-out with `lastKnownSeqno = 0`.
- Sidecar exit condition: `Nexus::Session` exits when `processors.empty()`, not when `attached.empty()`. Update `Main.cpp` daemon-exit logic accordingly.

**Auditor semantic checks:**
- Grep `lastFanoutSeqno` — zero
- `State::attachSubscriber` / `detachSubscriber` / `setSubscriberSeqno` / `getSubscriberSeqno` declared and implemented
- `setLastKnownSeqno` / `getLastKnownSeqno` declared and implemented (proxy side)
- `Nexus::Session::handleAsyncUpdate` iterates per-subscriber, not per-session
- `connectionMade` / `connectionLost` call attach/detach on every processor
- Sidecar exit logic: `processors.empty()`, not `attached.empty()`
- Smoke: launch sidecar, connect client A, spawn processor, connect client B — both receive state updates independently. Disconnect A, B still receives. Spawn another processor, both see it.

---

### Step 11a — Consumer updates: `Panes.h/cpp`

**Objective:** Update `Panes::createTerminal`, `Panes::splitImpl`, `Panes::closePane` to use `Terminal::Processor` with the correct ctor chosen via `AppState::isClientMode()`. Delete any `RemoteSession` references.

**Delegate:** @Engineer.

**Files touched:**
- `Source/component/Panes.h` — any member type updates (Processor& references, etc.).
- `Source/component/Panes.cpp:62` — host/local path: `auto processor { std::make_unique<Terminal::Processor> (appState, cols, rows, shell) }; processor->startShell();`
- `Source/component/Panes.cpp:62` — client path: `auto processor { std::make_unique<Terminal::Processor> (appState, cols, rows, existingUuid) };` (no startShell — hosted).
  **Replaces** current `auto remoteSession { std::make_unique<Nexus::RemoteSession>(...) }` at Panes.cpp:83,422.
- `Source/component/Panes.cpp:359` — session destruction: local mode removes from `Nexus::Session::processors`; client mode sends `Message::detachProcessor` via Client. The branch on `isClientMode()` is preserved (L17).
- `Source/component/Panes.cpp:402` — split pane: same ctor selection as :62.
- Delete `#include "../nexus/RemoteSession.h"` at `Panes.cpp:16`.

**Auditor semantic checks:**
- Grep `RemoteSession` in Panes — zero
- Grep `remoteSession` (variable) in Panes — zero
- Every `std::make_unique<Terminal::Processor>` is followed by `->startShell()` in the local branch, and not in the client branch
- `isClientMode()` consulted at every Processor-construction point
- Build + smoke: split pane in single-process mode works; split pane in client mode also works

---

### Step 11b — Consumer updates: `Tabs.h/cpp`, `Popup.h/cpp`

**Objective:** Same as 11a for Tabs and Popup.

**Delegate:** @Engineer.

**Files touched:**
- `Source/component/Tabs.cpp:275` — tab close session removal (host vs client branch per `isClientMode()`).
- `Source/component/Tabs.cpp:278` — any `Nexus::Session` (was Host) references.
- `Source/component/Popup.cpp:172` — popup dismiss session cleanup.

**Auditor semantic checks:**
- Grep `RemoteSession` in Tabs/Popup — zero
- Build + smoke: open tab, type, close tab (both modes)

---

### Step 11c — Consumer updates: `Main.cpp`, `MainComponent.h/cpp`

**Objective:** Startup wiring for sidecar-mode vs single-process mode. Preserve `AppState::setClientMode` at `Main.cpp:262`.

**Delegate:** @Engineer.

**Files touched:**
- `Source/Main.cpp:193-252` — daemon mode launch (`Nexus::Session` + `Nexus::Server`), client mode launch (`Nexus::Client`), mode decision.
- `Source/Main.cpp:262` — `appState.setClientMode(client != nullptr)` preserved (L17).
- `Source/Main.cpp:388` — shutdown path.
- `Source/MainComponent.cpp:30,546,547` — any Session/Host references.
- `Source/MainComponent.cpp:688` — saved tab layout gated on `isClientMode()`, preserved.

**Auditor semantic checks:**
- Grep `Nexus::Host` — zero
- Grep `Terminal::Session` — zero
- `Main.cpp` still calls `setClientMode` at :262 equivalent (line number may shift)
- Build + smoke: both launch modes

---

### Step 12 — Bug 3 cleanup: wire `startShell()` at construction sites, delete `callAsync` deferral

**Objective:** Now that Step 8 introduced `startShell()` and Step 11 updated consumers to call it, verify the old deferral is fully gone.

**Delegate:** @Engineer (mostly verification; minor cleanup).

**Files touched:**
- `Source/terminal/logic/Processor.cpp` (post-rename) — confirm `resized()` has no `callAsync`, no `isThreadRunning` first-call branch, no deferred `open()`.
- Any remaining `Source/Main.cpp` or sidecar spawn handler that constructs a Processor must call `startShell()` immediately after.

**Auditor semantic checks:**
- Grep `callAsync` in `Processor.cpp` — zero
- Grep `isThreadRunning` in `Processor::resized` — zero
- Every local-mode `Processor` construction site has a subsequent `startShell()` call
- Smoke: Bug 3 regression test — spawn terminal, first frame shows shell prompt (not empty grid)

---

### Step 13 — Bugs 4/5/6/7 verification

**Objective:** Confirm each remaining bug is fixed by the refactor. Bug 5, 6, 7 fixed by `setStateInformation` spec in Step 8. Bug 4 fixed by `getStateInformation(0)` returning full snapshot on attach.

**Delegate:** @Auditor verification first; @Engineer only if a gap is found.

**Verification tests:**
- **Bug 4** (reflow delta loss): launch sidecar, spawn processor, type until scrollback has content, resize window (triggers reflow), connect a fresh client — client receives full grid including scrollback via `getStateInformation(0)`.
- **Bug 5** (graphemes cross-process): type a multi-byte grapheme (emoji, combined diacritic). Verify client displays identical glyph.
- **Bug 6** (wide-char boundary): type a CJK wide character. Verify `LAYOUT_WIDE_CONT` cells render correctly across the boundary.
- **Bug 7** (grapheme sidecar write): verify `activeWriteGrapheme` is called inside the `setStateInformation` row loop for every cell with `LAYOUT_GRAPHEME` flag set. This is the fix for the current `RemoteSession.cpp:88-94` bug.

**Auditor semantic checks:**
- Review `setStateInformation` implementation against each bug's fix criterion
- Run the four smoke tests above
- Report findings to COUNSELOR; if any fail, spawn a corrective Step 13.x

---

### Step 14 — Async startup (eliminate blocking poll, wire `LoaderOverlay`)

**Objective:** Replace the 5-second blocking `Thread::sleep` poll loop at `Main.cpp:221-231` with async connection logic. Show `LoaderOverlay` during the wait.

**Delegate:** @Engineer.

**Files touched:**
- `Source/Main.cpp:219-239` — rewrite:
  ```cpp
  // Fire window up immediately.
  showMainWindow();
  mainComponent->getLoaderOverlay().show (0, "Connecting to nexus");

  // Kick off async connect. Client::connectionMade / connectionFailed fire on message thread.
  client = std::make_unique<Nexus::Client>();
  client->onConnectionMade    = [this] { mainComponent->getLoaderOverlay().hide(); };
  client->onConnectionFailed  = [this] { /* retry or exit — ARCHITECT decides future sprint */ };
  client->beginConnectAttempts (lockfilePath);   // new method: reads lockfile, attempts connect, retries with exponential backoff, fires one of the two callbacks
  ```
- `Source/nexus/Client.h/cpp` — add `onConnectionMade` / `onConnectionFailed` std::function members (or promote to `ChangeBroadcaster` if listener pattern is already present). Add `beginConnectAttempts` method that uses a `juce::Timer` (or `juce::Thread` that posts back via `callAsync` — but prefer Timer on the message thread for JUCE-native behavior) to poll the lockfile at 100ms intervals up to 5 seconds max. On success, calls `connectToSocket(port)` and fires `onConnectionMade`. On final failure, fires `onConnectionFailed`.
- `Source/component/MainComponent.h/cpp` — expose `LoaderOverlay& getLoaderOverlay()` (proven caller: `Main.cpp`).

**Auditor semantic checks:**
- Grep `Thread::sleep` in `Main.cpp` — zero
- `LoaderOverlay::show(0, "Connecting to nexus")` called on nexus-mode startup
- `LoaderOverlay::hide()` called from `onConnectionMade`
- Window appears within 1 frame of launch (no 5-second blocking wait)
- Smoke: launch with no sidecar running — window appears immediately with spinner, sidecar spawns, connection lands, spinner disappears, UI usable

---

### Step 15 — `InputHandler` / `MouseHandler` type cascade

**Objective:** Update the session-holding components to reference `Terminal::Processor&` instead of `Terminal::Session&`. Type rename only; no behavioral change.

**Delegate:** @Engineer. (Could have been folded into Step 1 but kept separate because these classes hold the reference by member and constructor signatures need explicit update.)

**Files touched:**
- `Source/component/InputHandler.h:53-54, 183` — ctor param and member: `Session&` → `Processor&`.
- `Source/component/InputHandler.cpp` — include + member refs updated.
- `Source/component/MouseHandler.h:68-70, 169` — ctor param and member.
- `Source/component/MouseHandler.cpp` — include + member refs.

Note: Step 1 may have already handled these at the include-and-type level; this step is insurance for any missed signature references.

**Auditor semantic checks:**
- Grep `Terminal::Session` in InputHandler/MouseHandler — zero
- Build succeeds
- Smoke: keyboard input (InputHandler) and mouse click/selection (MouseHandler) work in both modes

---

### Step 16 — Final cleanup (file deletions, forbidden-token final sweep)

**Objective:** Delete dead files and confirm zero forbidden-token residue.

**Delegate:** @Engineer for file deletions; @Auditor for final sweep.

**Files deleted:**
- `Source/nexus/RemoteSession.h`
- `Source/nexus/RemoteSession.cpp`
- `Source/nexus/GridDelta.h` (or already renamed to StateInfo.h in Step 7 — verify)
- `Source/terminal/logic/GridDelta.cpp` (already deleted in Step 6 — verify)
- Old `Source/nexus/Pdu.h` if left as a stub from Step 3 — final removal.
- Projucer/CMake file-list updates to drop the deleted files.

**Final forbidden-token sweep (Auditor runs ripgrep against every token in the Forbidden Tokens table and reports zero):**
```
Terminal::Session
Nexus::Host
Pdu
PduKind
RemoteSession
GridDelta
Nexus::CellRange
buildDelta
renderDelta
applyDelta
getRenderDelta
listSessions
listSessionsResponse
lastFanoutSeqno
// READER THREAD
// MESSAGE THREAD
```

All must return zero matches outside comments referencing historical context (which should also be zero at this point).

**Auditor semantic checks:**
- All files in deletion list do not exist
- All forbidden tokens return zero ripgrep matches
- Build clean
- Full smoke test: both modes, all features (type, scroll, split, close, tab, popup, multi-client)

---

## BLESSED Alignment Notes

**B — Bound:**
- `Terminal::Processor` is the sole owner of its `TTY`, `Parser`, `State`, `Grid`. Hosted-mode Processor omits TTY/Parser — still owns State and Grid.
- `Nexus::Server` is the sole owner of `ServerConnection` instances via `jreng::Owner` (L10, Step 4).
- `Nexus::Session` holds non-owning references to `ServerConnection*` for broadcast (L11) — Server outlives Session.connections by contract.
- `Nexus::Client` is owned as `std::unique_ptr` on `ENDApplication` (Main.cpp:209).
- Every Grid write (local and hosted) goes through a writer bound to the Processor (Parser in local; `setStateInformation` method in hosted). Writer lifetime matches Processor lifetime.

**L — Lean:**
- No wrapper classes (no Mirror, no Stub, no ProxyProcessor subclass).
- `GridDelta.cpp` deleted (logic moves where it belongs).
- Client's synchronous-wait infrastructure deleted (`listSessionsEvent`, `listSessionsLock`).
- `callAsync` deferral in `Processor::resized` deleted.
- 300/30/3 enforced per file — Engineer's responsibility per step.

**E — Explicit:**
- `Processor` mode is constructor choice — no boolean flag, no mode enum inside the class.
- `startShell()` is an explicit named lifecycle method. PTY-open no longer hidden inside `resized()`.
- `getStateInformation` / `setStateInformation` named after their exact JUCE analogs.
- `Nexus::StateInfo` names what it carries, not how it's transmitted.
- `// PRODUCER CONTEXT` / `// CONSUMER CONTEXT` annotations are mode-honest in both local and hosted.
- No early returns anywhere (JRENG-CODING-STANDARD L325-341, MANIFESTO-E L83-113).
- `.at()` for container access throughout (L492).
- `not`, `and`, `or` alternative tokens throughout (L493).

**S — Single Source of Truth:**
- `Terminal::State` is the SSOT for cursor, title, cwd, geometry, scrollback, subscriber seqnos.
- `Grid` is the SSOT for cell content.
- `Terminal::Processor` owns one instance of each; no shadow copies.
- Wire payload `StateInfo` is a transient serialization — does not duplicate state persistently.
- Per-subscriber seqno lives in one place (`ID::subscribers` ValueTree children) — no parallel map in Session or ServerConnection (replaces `Host::lastFanoutSeqno`).

**S — Stateless:**
- Processor is told `resized(cols, rows)`, `startShell()`, `setStateInformation(source)`. Caller never asks "are you ready?" or "did you start?".
- Display is told via `juce::ChangeBroadcaster` → `changeListenerCallback`. No polling.
- No boolean flags tracking Processor state on behalf of the caller.

**E — Encapsulation:**
- Grid knows cells. It does not read State. It does not know about Nexus types (ARCHITECT to rule on `ChangedRow` vs direct `Nexus::DirtyRow` return — see Step 6 note).
- State knows its fields. It does not know about IPC.
- Client is the IPC layer. It routes messages by uuid to Processors. It does not own Processors or inspect their internals beyond `setStateInformation`.
- Server owns connections. Session broadcasts. Neither reaches into the other's storage.
- Display reads `processor.state.*` and `processor.grid.*` directly — exactly the JUCE `AudioProcessorEditor::processor` pattern. This is NOT a BLESSED-E violation; it is the established pattern the project is matching.

**D — Deterministic:**
- Single-producer / single-consumer invariant holds on every Grid (local mode: reader thread + message thread; hosted mode: message thread for both, trivially same-thread safe).
- No cross-thread direct calls.
- No hidden state mutations — every write goes through a named method on a bound owner.

---

## Step Dependency Summary

```
Step 0  (annotation sweep — foundation)
  │
  ▼
Step 1  (rename Session→Processor)
Step 2  (rename Host→Session)
Step 3  (rename Pdu→Message)
  │
  ▼
Step 7  (StateInfo type — required before Step 6's Grid methods can compile)
  │
  ▼
Step 6  (Grid additions + buildDelta deletion)
  │
  ▼
Step 4  (Bug 1 — connection ownership)
Step 5  (Bug 2 — Client threading)
  │
  ▼
Step 8  (Terminal::Processor two-mode — the heart)
  │
  ▼
Step 9  (tell-don't-ask protocol)
Step 10 (multi-subscriber seqno)
  │
  ▼
Step 11a (Panes)
Step 11b (Tabs + Popup)
Step 11c (Main + MainComponent)
  │
  ▼
Step 12 (Bug 3 wiring verification)
Step 13 (Bug 4/5/6/7 verification)
Step 15 (InputHandler / MouseHandler cascade — could be in parallel with 11)
  │
  ▼
Step 14 (async startup + LoaderOverlay — needs Client async infra from Step 5)
  │
  ▼
Step 16 (final cleanup + forbidden-token sweep)
```

Steps 1, 2, 3 are fully independent and can run in any order after Step 0. Steps 4 and 5 are independent. Step 6 depends on Step 7. Step 8 depends on 1, 2, 6, 7. Steps 11a/b/c depend on Step 8. Step 13 can only execute after Step 8 is in.

---

## Open Items

None. All decisions locked.

**Grid return type (locked):** `Grid::collectDirtyRows` returns `juce::HeapBlock<Nexus::DirtyRow>`. `Grid.h` includes `nexus/StateInfo.h`. Processor's `getStateInformation` uses the HeapBlock directly without conversion.

---

*End of PLAN-nexus.md*
