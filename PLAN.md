# PLAN: SSOT Resize with CachedValue

**Objective:** Fix cursor positioning after split close/create by making dims
truly SSOT. grid.resize() runs on message thread. Dims use CachedValue.

**Root cause:** Dims flow through shadow stores before reaching State. Parser
sees new dims while grid buffer has old dims. Content wraps wrong, cursor jumps.

**Fix:** Message thread owns dims exclusively. grid.resize() runs on message
thread under resizeLock. Parser acquires resizeLock per data chunk. CachedValue
provides typed, instant ValueTree integration for dims.

---

## Phase 1: Fix the race (grid.resize on message thread)

### Step 1.1 — Parser acquires resizeLock during data processing

**File:** `Source/terminal/logic/Session.cpp`

Change `tty->onData` in `setupCallbacks()`:
```
tty->onData = [this] (const char* data, int len)
{
    const juce::ScopedLock lock (grid.getResizeLock());
    process (data, len);
};
```

**Validate:** Build. Run. Type commands, `ls`, `cat` a file. Verify terminal
works normally. No deadlocks, no freezes.

---

### Step 1.2 — Session::resized() always calls grid.resize() directly

**File:** `Source/terminal/logic/Session.cpp`

`Session::resized()` — remove the `tty->isThreadRunning()` branch for grid.
ALWAYS call grid.resize() + parser.resize() on message thread. Call
platformResize only when TTY is running.

```
void Session::resized (int cols, int rows)
{
    grid.resize (cols, rows);
    parser.resize (cols, rows);

    if (tty->isThreadRunning())
    {
        tty->platformResize (cols, rows);
    }
    else if (cols > 0 and rows > 0)
    {
        juce::MessageManager::callAsync ([this]
        {
            if (not tty->isThreadRunning())
            {
                const int c { state.getCols() };
                const int r { state.getVisibleRows() };
                grid.resize (c, r);
                parser.resize (c, r);
                // ... open TTY (existing startup code)
            }
        });
    }
}
```

**Validate:** Build. Run. Create split (horizontal + vertical). Close split.
Resize window. Verify:
- Prompt stays at bottom after split close
- Text wraps correctly after split create
- No content outside bounds
- Window resize works

---

### Step 1.3 — Remove onBeforeDrain from TTY and Session

**Files:**
- `Source/terminal/tty/TTY.h` — remove `onBeforeDrain` member
- `Source/terminal/tty/TTY.cpp` — remove `onBeforeDrain` call from run()
- `Source/terminal/logic/Session.cpp` — remove `onBeforeDrain` setup from setupCallbacks()
- `Source/terminal/logic/Grid.h` — remove `needsResize()` declaration
- `Source/terminal/logic/Grid.cpp` — remove `needsResize()` implementation

**Validate:** Build. Run. Same checks as Step 1.2.

---

## Phase 2: CachedValue for dims

### Step 2.1 — Add CachedValue<int> members to State

**Files:**
- `Source/terminal/data/State.h` — add two `juce::CachedValue<int>` members
- `Source/terminal/data/State.cpp` — bind them to SESSION tree in constructor

CachedValue binds to DIRECT properties on SESSION node (not PARAM children):
```
cachedCols.referTo (state, ID::cols, nullptr, 0);
cachedVisibleRows.referTo (state, ID::visibleRows, nullptr, 0);
```

This adds `cols="0"` and `visibleRows="0"` as properties directly on the
SESSION ValueTree node. These coexist with the PARAM children temporarily.

**Validate:** Build. Run. Verify dims still work (the PARAM system still
operates in parallel).

---

### Step 2.2 — setDimensions() writes CachedValue

**File:** `Source/terminal/data/State.cpp`

Replace the current setDimensions() (storeAndFlush + flushRootParams) with
CachedValue writes:
```
void State::setDimensions (int cols, int rows) noexcept
{
    cachedCols = cols;
    cachedVisibleRows = rows;
}
```

CachedValue writes update the ValueTree synchronously. Listeners fire
immediately on the message thread.

**Also update the atomic** for reader-thread reads (parser still reads atomics):
```
void State::setDimensions (int cols, int rows) noexcept
{
    cachedCols = cols;
    cachedVisibleRows = rows;
    storeAndFlush (ID::cols, static_cast<float> (cols));
    storeAndFlush (ID::visibleRows, static_cast<float> (rows));
}
```

The atomic write ensures getCols()/getVisibleRows() return correct values on
the reader thread. The CachedValue write ensures ValueTree listeners fire
immediately.

**Validate:** Build. Run. Verify resize works. Verify ValueTree listeners fire
(dims update in title bar or similar UI that reads from ValueTree).

---

### Step 2.3 — grid.getCols()/getVisibleRows() return from buffer

**Files:**
- `Source/terminal/logic/Grid.h` — change inline getters

```
int getCols () const noexcept { return buffers.at (normal).allocatedCols; }
int getVisibleRows () const noexcept { return buffers.at (normal).allocatedVisibleRows; }
```

These now return the buffer's intrinsic dims instead of reading State atomics.
With grid.resize() on the message thread, buffer dims and State dims are always
synchronized. Reader thread callers are under resizeLock.

**Validate:** Build. Run. Full check: splits, resize, scrollback, selection,
text output. Buffer dims must match State dims at all times.

---

### Step 2.4 — Remove cols/visibleRows from parameterMap

**Files:**
- `Source/terminal/data/State.cpp` — remove `addParam (state, ID::cols, 0.0f)`
  and `addParam (state, ID::visibleRows, 0.0f)` from constructor
- `Source/terminal/data/State.cpp` — update getCols()/getVisibleRows() to read
  from CachedValue instead of atomic:
  ```
  int State::getCols () const noexcept { return cachedCols; }
  int State::getVisibleRows () const noexcept { return cachedVisibleRows; }
  ```
- `Source/terminal/data/State.cpp` — remove setCols()/setVisibleRows() (dead code)
- `Source/terminal/data/State.cpp` — setDimensions() no longer calls storeAndFlush
  for dims (atomics removed)
- `Source/terminal/data/StateFlush.cpp` — cols/visibleRows PARAM children no
  longer exist, flush naturally skips them

**CRITICAL:** After this step, getCols()/getVisibleRows() read from CachedValue
(message-thread only). Reader thread callers (parser) MUST read from
grid.getCols()/grid.getVisibleRows() (buffer dims, under resizeLock) instead of
state.getCols()/state.getVisibleRows().

Audit ALL reader-thread call sites from the inventory:
- Parser*.cpp files: change `state.getCols()` -> `grid.getCols()` (already
  under resizeLock from Step 1.1)
- Grid*.cpp files: already use buffer internally or are under resizeLock
- Screen*.cpp files: message thread only, state.getCols() is fine

**Validate:** Build. Run. Grep for `state.getCols` and `state.getVisibleRows`
to confirm no reader-thread callers remain. Full functional check.

---

## Phase 3: Clean up

### Step 3.1 — Remove PARAM children for cols/visibleRows

**File:** `Source/terminal/data/State.cpp`

The `addParam (state, ID::cols, ...)` and `addParam (state, ID::visibleRows, ...)`
were already removed in 2.4. Verify the PARAM children are gone from the
SESSION tree. The CachedValue direct properties on SESSION are now the sole
storage.

**Validate:** Build. Run. Verify no regressions from PARAM removal.

---

### Step 3.2 — Update doc comments

All modified files: update Doxygen comments to reflect:
- grid.resize() runs on MESSAGE THREAD (not READER THREAD)
- Parser acquires resizeLock during data processing
- Dims use CachedValue (message thread) / buffer allocation (reader thread)
- setCols/setVisibleRows removed
- onBeforeDrain removed
- State dim lifecycle documented

---

### Step 3.3 — Audit

Invoke @Auditor to verify:
- Zero early returns
- Brace initialization
- Alternative tokens (not/and/or)
- .at() container access
- No dangling references to removed members
- LIFESTAR compliance (SSOT, Explicit Encapsulation)
- Thread safety: no CachedValue reads from reader thread

---

## Files Modified (expected)

| File | Changes |
|------|---------|
| `terminal/data/State.h` | CachedValue members, remove setCols/setVisibleRows |
| `terminal/data/State.cpp` | CachedValue binding, setDimensions rewrite, getCols/getVisibleRows rewrite, remove addParam for dims |
| `terminal/logic/Grid.h` | getCols/getVisibleRows return buffer dims, remove needsResize |
| `terminal/logic/Grid.cpp` | remove needsResize impl |
| `terminal/logic/GridReflow.cpp` | (already changed: head + cursor formula) |
| `terminal/logic/Session.cpp` | resized() direct grid.resize, onData acquires lock, remove onBeforeDrain |
| `terminal/tty/TTY.h` | remove onBeforeDrain |
| `terminal/tty/TTY.cpp` | remove onBeforeDrain from run() |
| `terminal/logic/Parser*.cpp` | state.getCols -> grid.getCols (reader thread) |
| `terminal/logic/GridErase.cpp` | state.getCols -> grid.getCols |
| `terminal/logic/GridScroll.cpp` | state.getCols -> grid.getCols |

---

## Invariants After Completion

1. State dims written ONLY by message thread (via CachedValue)
2. Grid buffer dims always match State dims (synchronized on message thread)
3. Parser reads buffer dims under resizeLock (never stale)
4. Renderer reads State dims via CachedValue (message thread, always current)
5. No atomics for dims (removed from parameterMap)
6. No cross-thread resize signaling (no pendingCols, no onBeforeDrain)
7. ValueTree listeners fire immediately on dim change
