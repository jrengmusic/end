# PLAN: Nexus Naming Cleanup

**RFC:** none -- objective from ARCHITECT prompt
**Date:** 2026-04-10
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

Rename Nexus classes, files, and message types to match what they actually are. Fold redundant platform files into their owning class. Remove wrapper methods that add indirection without value. Rename state files on disk to align with END product vocabulary (Ephemeral Nexus Display).

## Naming Contract (ARCHITECT-approved)

| Current | New | Rationale |
|---|---|---|
| `Nexus::Server` | `Nexus::Daemon` | It IS the daemon's IPC listener |
| `Nexus::Client` | `Nexus::Link` | The link between client and daemon |
| `Nexus::ServerConnection` | `Nexus::Channel` | Communication channel per connected client |
| `NexusDaemon.h/mm/win.cpp` | Fold into `Nexus::Daemon` | Platform methods belong to the object, not a separate file |
| `Message::createProcessor` | `Message::createSession` | Creates a session, not a processor |
| `Message::removeProcessor` | `Message::killSession` | Kills a session |
| `Message::detachProcessor` | `Message::detachSession` | Detaches from a session |
| `Message::processorExited` | `Message::sessionKilled` | Session was killed |
| `Message::processorList` | `Message::sessions` | List of sessions |
| `<uuid>.state` | `<uuid>.display` | Display state -- aligns with END (Ephemeral Nexus Display) |

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Rename files and classes

**Scope:** `Source/nexus/Server.h/cpp` -> `Daemon.h/cpp`, `Source/nexus/Client.h/cpp` -> `Link.h/cpp`, `Source/nexus/ServerConnection.h/cpp` -> `Channel.h/cpp`
**Action:**
- Rename files on disk
- Rename classes inside: `Server` -> `Daemon`, `Client` -> `Link`, `ServerConnection` -> `Channel`
- Update all `#include` directives across the codebase (6 files for Server, 3 for Client, 6 for ServerConnection)
- Update all doc comments referencing old names
- Update `Nexus::Session` members: `server` -> `daemon`, `client` -> `link`
- Update all references in `Session.cpp`, `SessionFanout.cpp`, `Main.cpp`, `Panes.h`

**Validation:** No references to old class names or file names remain. Grep for `Server.h`, `Client.h`, `ServerConnection.h`, `Nexus::Server`, `Nexus::Client`, `Nexus::ServerConnection` returns zero hits.

### Step 2: Fold NexusDaemon into Daemon

**Scope:** `Source/nexus/NexusDaemon.h`, `NexusDaemon.mm`, `NexusDaemon_win.cpp` -> merge into `Daemon.h/cpp/mm`
**Action:**
- Move `hideDockIcon()` and `spawnDaemon(uuid)` into `Nexus::Daemon` as static methods
- `Daemon.cpp` handles Windows `spawnDaemon` implementation (was `NexusDaemon_win.cpp`)
- macOS Obj-C code (`hideDockIcon` with `NSApplicationActivationPolicyAccessory`) needs `.mm` -- use `Daemon.mm` for macOS build, `Daemon.cpp` for Windows
- Delete `NexusDaemon.h`, `NexusDaemon.mm`, `NexusDaemon_win.cpp`
- Update `Main.cpp` include and call sites: `Nexus::hideDockIcon()` -> `Nexus::Daemon::hideDockIcon()`, `Nexus::spawnDaemon(uuid)` -> `Nexus::Daemon::spawnDaemon(uuid)`

**Validation:** No references to `NexusDaemon`. `Daemon.h` declares both static methods. `Main.cpp` compiles with new call sites.

### Step 3: Rename Message enum values

**Scope:** `Source/nexus/Message.h` + 6 files that reference the values
**Action:**
- `createProcessor` -> `createSession`
- `removeProcessor` -> `killSession`
- `detachProcessor` -> `detachSession`
- `processorExited` -> `sessionKilled`
- `processorList` -> `sessions`
- Update all reference sites: `Client.cpp` (now `Link.cpp`), `ServerConnection.cpp` (now `Channel.cpp`), `Session.cpp`, `SessionFanout.cpp`, `Session.h`, `MainComponent.cpp`

**Validation:** No references to old enum values remain. Grep for `createProcessor`, `removeProcessor`, `detachProcessor`, `processorExited`, `processorList` in Message context returns zero hits.

### Step 4: Remove lockfile wrappers from Daemon

**Scope:** `Source/nexus/Daemon.h/cpp`
**Action:**
- Remove `getLockfile()`, `writeLockfile()`, `deleteLockfile()` from `Daemon`
- Callers call AppState directly: `AppState::getContext()->setPort()`, `AppState::getContext()->deleteNexusFile()`, `AppState::getContext()->getNexusFile()`
- Update `Daemon::start()` to call AppState directly for port write
- Update any doc references

**Validation:** `Daemon` has no lockfile methods. All port/nexus-file operations go through AppState.

### Step 5: Rename state file extension

**Scope:** `Source/AppState.h/cpp`, `Source/Main.cpp`
**Action:**
- `getSessionStateFile()` returns `<uuid>.display` instead of `<uuid>.state`
- Update startup scan in `Main.cpp` to glob `*.display` instead of `*.state`
- Update stale cleanup to delete `*.display`
- Update all doc comments

**Validation:** No references to `.state` file extension in nexus context. Only `.nexus` and `.display`.

### Step 6: Clean dead code

**Scope:** `Source/nexus/Link.h/cpp` (was Client)
**Action:**
- Remove `connectToHost()` if trivial wrapper -- inline `connectToSocket` at call site
- Remove `onPdu` callback if unused (check all assignment sites)
- Remove any other dead members flagged during session

**Validation:** No dead methods on `Link`. Every public method has at least one caller.

## BLESSED Alignment

- **B (Bound):** Each class owns its concern. Daemon owns IPC listening. Link owns IPC connecting. Channel owns per-client communication. No cross-ownership.
- **L (Lean):** Wrapper methods removed. Platform files folded. No redundant indirection.
- **E (Explicit):** Names match reality. `Daemon` is a daemon. `Link` is a link. `Channel` is a channel. `createSession` creates a session.
- **S (SSOT):** Port file owned by daemon only. Display file owned by client only. No shared files.
- **S (Stateless):** No change -- machinery stays stateless.
- **E (Encapsulation):** Platform code lives inside the class it belongs to, not in separate free-function files.
- **D (Deterministic):** Correct names make data flow traceable. Bugs become visible.

## Risks / Open Questions

- CMake may need updating if file rename isn't picked up by AUTO_GLOB. Verify after Step 1.
- macOS `.mm` file for Daemon -- CMake must compile `Daemon.mm` on macOS, `Daemon.cpp` on Windows. Check existing pattern (NexusDaemon already does this).
