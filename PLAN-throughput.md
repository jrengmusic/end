# PLAN-throughput.md — PTY Read Throughput: Process Query Cleanup

**Project:** END
**Date:** 2026-03-23
**Status:** Done (process query move). FIFO investigation closed — architecture is sound.
**Contracts:** JRENG-CODING-STANDARD, ARCHITECTURAL-MANIFESTO

---

## Problem

Keypress latency degrades during heavy PTY output (e.g. Claude Code agents running tasks). The reader thread parses 64KB chunks inline via `parser.process()` before the next `read()` can drain the PTY. Keypress echo sits in the kernel buffer waiting for the current parse to complete.

### Benchmark context

`seq 1 10000000` on iMac 5K 2015, full-width terminal, **all release -O3:**

| Terminal | User  | System | CPU | Wall   |
|----------|-------|--------|-----|--------|
| kitty    | 6.41s | 6.70s  | 96% | 13.55s |
| wezterm  | 6.44s | 6.62s  | 92% | 14.05s |
| ghostty  | 6.60s | 6.32s  | 86% | 15.02s |
| **END**  | 6.46s | 5.67s  | 97% | **12.39s** |

END already wins on raw throughput at -O3. The problem is not throughput — it is input latency during concurrent output.

### Prior investigation

- 3 kernel calls (tcgetpgrp, proc_name, proc_pidinfo) removed from reader thread hot path — negligible impact at -O3. Kept as hygiene (process queries now fire at timer rate via `state.onFlush`).
- Reader thread hot path is lock-free. No mutex contention during normal output.
- The bottleneck is `parser.process()` compute time blocking the reader thread between `read()` calls, delaying keypress echo pickup.

---

## Architecture: Audio Plugin Pattern

The fix follows the JUCE APVTS audio plugin model — the same pattern END's State already uses:

**Audio plugin:**
Real-time thread fills ring buffer → message thread drains and renders UI.

**Terminal (new):**
Reader thread fills FIFO → message thread drains, parses, flushes, renders.

### Current path

```
READER THREAD                        MESSAGE THREAD
read(64KB)                           timerCallback (60-120Hz)
  → parser.process(64KB)  [SLOW]       → flush() atomics → ValueTree
  → state.consumePasteEcho()            → flushStrings()
  → setSnapshotDirty()                  → onFlush() → process queries
  → read(64KB)
                                     onVBlank
                                       → consumeSnapshotDirty()
                                       → screen.render()
```

### New path

```
READER THREAD                        MESSAGE THREAD
read(64KB)                           timerCallback (60-120Hz)
  → memcpy into FIFO     [FAST]       → drainPtyBuffer()
  → read(64KB)                             → parser.process(buffered data)
                                           → state.consumePasteEcho()
                                           → setSnapshotDirty()
                                       → flush() atomics → ValueTree
                                       → flushStrings()
                                       → onFlush() → process queries

                                     onVBlank
                                       → consumeSnapshotDirty()
                                       → screen.render()
```

Reader thread becomes pure I/O — `read()` → memcpy → `read()`. No parsing, no state machine, no grid writes. Message thread owns all mutable terminal state (Grid, Parser, State atomics).

### FIFO

`juce::AbstractFifo` — lock-free SPSC ring buffer, JUCE native. Manages indices only. Backing buffer: `char[1048576]` (1MB, matches kitty's `BUF_SZ`).

### Back-pressure

When FIFO is full: reader skips read, returns to `poll()`. Child process blocks on `write()` to PTY slave — natural back-pressure. Same as kitty removing POLLIN when its ring buffer is full. Terminal renders at the rate it can render.

### Input latency

Keypress echo arrives in kernel buffer → reader stuffs into FIFO instantly (no 64KB parse delay) → next timer tick drains and parses → vblank renders. Worst case: one timer interval (~8ms at 120Hz). Same order as kitty's `input_delay` (3ms default).

---

## Design Decisions

1. **FIFO owned by Session.** Session already bridges TTY and State. The FIFO is Session's internal communication channel. TTY does not know about it.

2. **Reader thread writes raw bytes only.** No parsing, no grid access, no state mutation. Pure `read()` → FIFO write loop.

3. **Message thread drains before flush.** `state.onDrain` callback fires at the start of `timerCallback()`, before `flush()`. Parsing produces fresh atomics that `flush()` then copies to ValueTree.

4. **Resize moves to message thread.** Currently `tty->onResize` fires on the reader thread and calls `grid.resize()` + `parser.resize()`. With FIFO, the reader thread no longer owns Grid or Parser. Reader thread stores pending dimensions in atomics. Message thread drain checks and applies resize before parsing.

5. **`onDrainComplete` logic moves to message thread.** `parser.flushResponses()` and `state.clearPasteEchoGate()` fire after each drain batch on the message thread, not after PTY drain on reader thread.

6. **`pasteEchoRemaining` simplifies.** Currently set on message thread, decremented on reader thread (cross-thread atomic). With FIFO, both set and decrement happen on message thread. No cross-thread access needed.

---

## Steps

### Step 0 — Add FIFO to Session

**File:** `Source/terminal/logic/Session.h`

Session gains:
- `static constexpr int ptyBufferSize { 1048576 };`
- `juce::AbstractFifo ptyFifo { ptyBufferSize };`
- `char ptyBuffer[ptyBufferSize];` (backing store)
- `void drainPtyBuffer();` (message thread consumer)

**File:** `Source/terminal/logic/Session.cpp`

Implement `drainPtyBuffer()`:
```cpp
void Session::drainPtyBuffer()
{
    // MESSAGE THREAD — drain FIFO, parse, flush responses
    const int numReady { ptyFifo.getNumReady() };

    if (numReady > 0)
    {
        auto scope = ptyFifo.read (numReady);

        if (scope.blockSize1 > 0)
        {
            parser.process (reinterpret_cast<const uint8_t*> (ptyBuffer + scope.startIndex1),
                            static_cast<size_t> (scope.blockSize1));
            state.consumePasteEcho (scope.blockSize1);
        }

        if (scope.blockSize2 > 0)
        {
            parser.process (reinterpret_cast<const uint8_t*> (ptyBuffer + scope.startIndex2),
                            static_cast<size_t> (scope.blockSize2));
            state.consumePasteEcho (scope.blockSize2);
        }

        parser.flushResponses();
        state.clearPasteEchoGate();
    }
}
```

**Validate:** Builds. No behavioral change yet (not wired).

---

### Step 1 — Reader thread writes to FIFO instead of parsing

**File:** `Source/terminal/logic/Session.cpp`

Change `Session::process()`:
```cpp
void Session::process (const char* data, int length) noexcept
{
    // READER THREAD — write raw bytes to FIFO, no parsing
    const int space { ptyFifo.getFreeSpace() };

    if (space >= length)
    {
        auto scope = ptyFifo.write (length);

        if (scope.blockSize1 > 0)
            std::memcpy (ptyBuffer + scope.startIndex1, data, static_cast<size_t> (scope.blockSize1));

        if (scope.blockSize2 > 0)
            std::memcpy (ptyBuffer + scope.startIndex2, data + scope.blockSize1, static_cast<size_t> (scope.blockSize2));
    }
    // FIFO full: drop this chunk. Reader returns to poll().
    // Child process back-pressures naturally via kernel PTY buffer.
}
```

Remove `parser.process()` and `state.consumePasteEcho()` from `process()`.

**Validate:** Builds. Terminal will appear frozen (FIFO fills, nothing drains yet).

---

### Step 2 — Wire drain to message thread timer

**File:** `Source/terminal/data/State.h`

Add public member:
```cpp
std::function<void()> onDrain;
```

**File:** `Source/terminal/data/State.cpp`

At the **start** of `timerCallback()`, before `flush()`:
```cpp
if (onDrain != nullptr)
    onDrain();
```

**File:** `Source/terminal/logic/Session.cpp`

In `setupCallbacks()`:
```cpp
state.onDrain = [this] { drainPtyBuffer(); };
```

**Validate:** Builds. Terminal works — data flows through FIFO. Reader thread is pure I/O.

---

### Step 3 — Move resize to message thread

**File:** `Source/terminal/logic/Session.h`

Add:
- `std::atomic<int> pendingResizeCols { 0 };`
- `std::atomic<int> pendingResizeRows { 0 };`

**File:** `Source/terminal/logic/Session.cpp`

Change `tty->onResize` in `setupCallbacks()`:
```cpp
tty->onResize = [this] (int cols, int rows)
{
    // READER THREAD — store dimensions, message thread applies
    pendingResizeCols.store (cols, std::memory_order_relaxed);
    pendingResizeRows.store (rows, std::memory_order_release);
};
```

Update `drainPtyBuffer()` — check resize before parsing:
```cpp
void Session::drainPtyBuffer()
{
    // MESSAGE THREAD
    const int resizeRows { pendingResizeRows.exchange (0, std::memory_order_acquire) };

    if (resizeRows > 0)
    {
        const int resizeCols { pendingResizeCols.load (std::memory_order_relaxed) };
        grid.resize (resizeCols, resizeRows);
        parser.resize (resizeCols, resizeRows);
    }

    // ... existing drain logic ...
}
```

**Validate:** Builds. Resize terminal window — grid and parser resize correctly.

---

### Step 4 — Remove `onDrainComplete` from reader thread

**File:** `Source/terminal/logic/Session.cpp`

`tty->onDrainComplete` currently calls `parser.flushResponses()`, `state.clearPasteEchoGate()`, and conditional `tty->requestResize()`. These now happen inside `drainPtyBuffer()` on the message thread.

Set `tty->onDrainComplete` to nullptr or minimal (only what still needs to run on reader thread, if anything).

**Validate:** Builds. Parser responses (DSR, DA) still work. Paste echo gate clears correctly.

---

### Step 5 — Clean up

- Remove `state.consumePasteEcho()` from any reader-thread path (moved to `drainPtyBuffer()`)
- Verify `pasteEchoRemaining` is only accessed from message thread
- Verify Grid is only accessed from message thread (parser writes) and message thread (onVBlank render) — no cross-thread grid access
- Consider whether `resizeLock` can be simplified (both parser and renderer now on message thread)

**Validate:** Full test pass. Keypress echo responsive during heavy output. `time seq 1 10000000` unchanged (~12.4s at -O3).

---

## Thread Safety

| Resource | Before | After |
|----------|--------|-------|
| Grid | Reader writes, message reads | Message thread only |
| Parser | Reader thread | Message thread only |
| State atomics | Reader writes, message reads | Message writes, message reads (simpler) |
| pasteEchoRemaining | Cross-thread atomic | Message thread only |
| FIFO | N/A | Reader writes, message reads (lock-free SPSC) |
| resizeLock | Reader ↔ message contention | Potentially removable |

Moving parse to the message thread eliminates all cross-thread mutable state access except the FIFO itself (which is lock-free by design).

---

## Files Changed

| File | Change |
|------|--------|
| `Session.h` | Add FIFO, backing buffer, drain method, resize atomics |
| `Session.cpp` | FIFO write in `process()`, `drainPtyBuffer()`, resize via atomics, wire `state.onDrain` |
| `State.h` | Add `std::function<void()> onDrain` |
| `State.cpp` | Call `onDrain()` at start of `timerCallback()` |

---

## Validation

1. Build succeeds
2. `time seq 1 10000000` — wall time unchanged (~12.4s at -O3)
3. Keypress echo responsive during heavy concurrent output
4. `nvim` — interactive latency acceptable
5. Terminal resize works correctly
6. Paste works correctly (echo gate)
7. Shell exit — close cascade fires
8. Parser responses (DSR, DA) work — `tput cols`, `tput lines`

---

## Rules for Execution

1. Read all modified files in full before making any change
2. Minimum-change edits only — no refactors, no renames beyond what is listed
3. Follow JRENG-CODING-STANDARD: Allman braces, `not`/`and`/`or`, spaces after function names, brace initialization, no early returns
4. Follow NAMING-CONVENTION: nouns for members (`ptyFifo`, `ptyBuffer`), verbs for methods (`drainPtyBuffer`)
5. Follow ARCHITECTURAL-MANIFESTO: Lean (minimum new code), Explicit Encapsulation (Session owns FIFO, State exposes callback)
6. Never guess — discrepancy between plan and code, stop and discuss with ARCHITECT
7. ARCHITECT runs all builds and git commands
8. Incremental execution — one step at a time, ARCHITECT confirms before next step
