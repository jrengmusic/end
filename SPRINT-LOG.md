# SPRINT-LOG

## Sprint 1: Fix Active Prompt Misalignment After Pane Close

**Date:** 2026-04-06
**Duration:** ~4h

### Agents Participated
- COUNSELOR: Requirements analysis, root cause investigation, plan, orchestration
- Pathfinder: Codebase exploration (resize path tracing, reflow architecture, rendering pipeline, buffer struct analysis)
- Engineer: Code changes (height-only fast path, diagnostics, cleanup)
- Auditor: Validation (alternate buffer overflow, BLESSED compliance)
- Oracle: (not invoked)

### Files Modified (2 total)
- `Source/terminal/logic/GridReflow.cpp:119-170` -- Height-only resize fast path with pinToBottom branching: when only visibleRows changes (cols unchanged, both buffers large enough), skip full reflow and adjust buffer metadata in-place. pinToBottom: head unchanged, scrollbackUsed decreases, cursor moves down (viewport expands from top). Not pinToBottom: head advances by heightDelta, scrollbackUsed unchanged, cursor stays (viewport expands from bottom). Sync State::scrollbackUsed after both paths. Existing full reflow path unchanged (now `else if`). Cursor fallback in full reflow: use `totalOutputRows - 1` and preserve `cursorCol`.
- `Source/terminal/data/State.h:373-378` -- Updated `setScrollbackUsed` doc comment to reflect dual-thread usage (READER THREAD or MESSAGE THREAD).

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **Active prompt 1-row misalignment after pane close**: When a split pane was closed and the surviving terminal reclaimed height, the active prompt rendered 1 row above its correct position. Root cause: the full reflow algorithm (designed for column-width changes that re-wrap content) produced a 1-row viewport shift when only height changed, due to the cursor fallback inflating `contentExtent` by 1 and floor-division rounding losses (107/2=52, 52*2=104!=107) compounding through double reflow (shrink+expand). Fix: bypass the full reflow for height-only changes — adjust viewport metadata directly (scrollbackUsed, allocatedVisibleRows, cursor) without touching the ring buffer content. Content layout is identical when cols are unchanged.
- **Prompt at wrong position after external window resize**: Height-only fast path initially assumed pinToBottom unconditionally (cursor + heightDelta). When the cursor was NOT at the bottom (e.g., fresh launch, Hammerspoon resize), the prompt slid to the middle instead of staying at its content position. Fix: branch on pinToBottom — when true, viewport expands from top (head stays, scrollback revealed); when false, viewport expands from bottom (head advances, cursor stays).
- **State::scrollbackUsed stale after reflow**: After full reflow, `Buffer::scrollbackUsed` was updated but `State::scrollbackUsed` (atomic) was not synced. Shell's OSC 133;A computed `promptRow = staleScrollbackUsed + cursorRow` producing wrong absolute row. Fix: sync State from Buffer after reflow.
- **SSOT violation**: Initial approach tracked OSC 133 markers (promptRow, blockTop, blockBottom) through the reflow, making Grid a second writer for values owned by the shell via parser. Removed all marker tracking — only the shell (via OSC 133) writes prompt/block markers. Grid writes only what it owns: cursor position, scrollbackUsed.

### Technical Debt / Follow-up
- None. PLAN-fix-active-prompt.md at project root can be removed (plan is superseded by the actual implementation).
