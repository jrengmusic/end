# PLAN: Daemon Lifecycle — Clean Start, Clean Kill

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-04-12
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

Align END's daemon lifecycle with tmux's proven patterns: OS-level lock replaces crash-unsafe `connected` flag, explicit kill commands enable deterministic daemon cleanup. Three surgical changes to close the three gaps identified in session.

## Language / Framework Constraints

- `juce::InterProcessLock` — cross-platform file lock (flock on POSIX, named mutex on Windows). Auto-releases on process crash. RAII-compatible via `ScopedLockType`.
- JUCE IPC wire format: uint16_t kind + payload. New PDU values must not collide with existing Message enum.
- No early returns, positive nesting, alternative tokens (`and`, `or`, `not`), brace init per JRENG-CODING-STANDARD.md.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Add `killDaemon` PDU

**Scope:** `Source/interprocess/Message.h`
**Action:** Add `killDaemon = 0x18` to `Message` enum (client -> daemon: "shut down the daemon process"). Slot 0x18 follows `killSession = 0x17` naturally.
**Validation:** Enum value does not collide. Doc comment matches wire protocol section. No other files touched.

### Step 2: Handle `killDaemon` in daemon

**Scope:** `Source/interprocess/Channel.cpp`, `Source/interprocess/Daemon.cpp`, `Source/interprocess/Daemon.h`
**Action:**
- `Channel::messageReceived`: add `case Message::killDaemon` — calls `daemon.killAll()`.
- `Daemon::killAll()`: iterates `nexus.sessions`, calls `nexus.remove(uuid)` for each, then fires `onAllSessionsExited`. Reuses the existing quit path (`deleteNexusFile() + quit()`).
**Validation:** No new quit path invented — reuses existing `onAllSessionsExited` lambda. No early returns. Daemon destruction order unchanged.

### Step 3: Replace `connected` flag with `InterProcessLock`

**Scope:** `Source/Main.cpp`, `Source/AppState.h`, `Source/AppState.cpp`
**Action:**
- Add `std::unique_ptr<juce::InterProcessLock> clientLock` member to `ENDApplication` (Main.cpp).
- Lock name: `<uuid>` (plain UUID string) — acquired when client claims a daemon in `resolveNexusInstance`, held for process lifetime.
- In `resolveNexusInstance`: replace `connected` property check with `InterProcessLock::tryEnter()`. If lock acquired → daemon is unclaimed, proceed to TCP probe. If probe fails → release lock, delete stale files. If probe succeeds → keep lock held.
- Remove `AppState::setConnected(true)` from `Link::connectionMade()`.
- Remove `AppState::setConnected(false)` + `save()` from `systemRequestedQuit` client-with-sessions branch. Lock auto-releases on process exit.
- `AppState::save()` stops writing `connected` property to `.display` file.
**Validation:** No shadow state — lock IS the connected truth (SSOT). Auto-release on crash (Bound — deterministic lifecycle). No manual boolean flag (Stateless). `resolveNexusInstance` logic remains single positive-check flow.

### Step 4: CLI kill commands

**Scope:** `Source/Main.cpp`
**Action:**
- Parse `--nexus kill <uuid>` and `--nexus kill-all` in `initialise()`.
- `kill <uuid>`: read port from `<uuid>.nexus`, connect to daemon, send `Message::killDaemon`, disconnect, `quit()`.
- `kill-all`: scan all `*.nexus` files, for each live daemon (TCP probe succeeds) send `killDaemon`, then `quit()`.
- Both are ephemeral — no window, no nexus, no link. Connect → send → exit.
**Validation:** No new process types — same executable, different arg path. Reuses existing TCP connect pattern. Clean exit via `quit()`.

### Step 5: Dead code cleanup

**Scope:** `Source/AppState.h`, `Source/AppState.cpp`, `Source/interprocess/Link.cpp`, `Source/Main.cpp`
**Action:**
- Remove `setConnected()` method from AppState.
- Remove `connected` property from `.display` XML write/read.
- Remove `setConnected` calls from `Link::connectionMade()` and `Link::connectionLost()`.
- Remove connected check from `resolveNexusInstance()` (already replaced by lock in Step 3).
**Validation:** No references to `connected` remain. `.display` file still contains window/tab state — only `connected` property removed.

## BLESSED Alignment

- **B (Bound):** `InterProcessLock` has RAII lifecycle — acquired on claim, released on destruction or crash. `clientLock` owned by `ENDApplication`, destroyed in `shutdown()`. Daemon `killAll` reuses existing destruction path.
- **L (Lean):** No new classes. One new method (`killAll`), one new PDU, one new member (`clientLock`). Lock replaces flag — net code reduction.
- **E (Explicit):** Lock name is the UUID itself — minimal, unique, no invented prefix. `killDaemon` PDU name matches `killSession` pattern. No magic values.
- **S (SSOT):** Lock IS the truth. No shadow `connected` boolean. One quit path for daemon (via `onAllSessionsExited`).
- **S (Stateless):** Removed `connected` boolean flag from AppState. Lock is OS-level, not application state.
- **E (Encapsulation):** Daemon handles its own shutdown via `killAll`. Client doesn't poke daemon internals.
- **D (Deterministic):** Lock acquire/release is atomic OS operation. No TOCTOU between check and claim.

## Risks / Open Questions

1. **NAMES (Rule -1):** New names requiring ARCHITECT approval:
   - `Message::killDaemon` (PDU enum value)
   - `Daemon::killAll()` (method)
   - `clientLock` (member in ENDApplication)
   - Lock name = plain UUID string
   - CLI syntax: `end --nexus kill <uuid>` / `end --nexus kill-all`

2. ~~`killAll` vs iterating sessions~~ **DECIDED:** Clean iteration via `nexus.remove()` for each session. Kills active sessions deterministically.

3. ~~Response PDU~~ **DECIDED:** Fire-and-forget. No ack before death.
