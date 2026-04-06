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

### Files Modified (4 total)
- `Source/terminal/logic/GridReflow.cpp:119-165` -- Height-only resize fast path: when only visibleRows changes (cols unchanged), skip full reflow and adjust buffer metadata in-place (scrollbackUsed, allocatedVisibleRows, cursor). Existing full reflow path unchanged (now `else if`).
- `Source/terminal/logic/GridReflow.cpp:168` -- Sync `State::scrollbackUsed` with `Buffer::scrollbackUsed` after full reflow, ensuring OSC 133 markers computed by the shell use correct scrollback depth.
- `Source/terminal/logic/GridReflow.cpp:680-683` -- Cursor fallback: when cursor falls beyond content walk range, use `totalOutputRows - 1` (last content row) and preserve `cursorCol` instead of inflating `contentExtent` by 1.
- `Source/terminal/data/State.h:373-378` -- Updated `setScrollbackUsed` doc comment to reflect dual-thread usage (READER THREAD or MESSAGE THREAD).

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **Active prompt 1-row misalignment after pane close**: When a split pane was closed and the surviving terminal reclaimed height, the active prompt rendered 1 row above its correct position. Root cause: the full reflow algorithm (designed for column-width changes that re-wrap content) produced a 1-row viewport shift when only height changed, due to the cursor fallback inflating `contentExtent` by 1 and floor-division rounding losses (107/2=52, 52*2=104!=107) compounding through double reflow (shrink+expand). Fix: bypass the full reflow for height-only changes — adjust viewport metadata directly (scrollbackUsed, allocatedVisibleRows, cursor) without touching the ring buffer content. Content layout is identical when cols are unchanged.
- **State::scrollbackUsed stale after reflow**: After full reflow, `Buffer::scrollbackUsed` was updated but `State::scrollbackUsed` (atomic) was not synced. Shell's OSC 133;A computed `promptRow = staleScrollbackUsed + cursorRow` producing wrong absolute row. Fix: sync State from Buffer after reflow.
- **SSOT violation**: Initial approach tracked OSC 133 markers (promptRow, blockTop, blockBottom) through the reflow, making Grid a second writer for values owned by the shell via parser. Removed all marker tracking — only the shell (via OSC 133) writes prompt/block markers. Grid writes only what it owns: cursor position, scrollbackUsed.

### Technical Debt / Follow-up
- None. PLAN-fix-active-prompt.md at project root can be removed (plan is superseded by the actual implementation).
