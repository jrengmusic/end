# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260602T000000

**Observation:** Content doubling on live output (cd, ls, or any shell command). The previous line's content appears duplicated right after a line feed. The double vanishes once the user types — next tick replaces it with correct content.
**Divergence:** `id::pushLine` (departing row → history ring) and `id::screenDirty` (viewport rows `[0..cursorRow]` → active ring) both fire from the same `Video::flush()` call per LF. The departing row enters history AND the same row is still present in the active viewport at the moment of the screenDirty push (Video has not yet completed the ring shift for the new state). Display drains history first (permanently appending the row), then drains active (re-appending the same row to the live tail). The row is now in the document twice. Next screenDirty tick, Video has shifted and the live region is rebuilt with fresh content — the duplicate disappears. The history copy stays forever, but the active duplicate is cosmetic-only.
**Expectation:** The departing row should be removed from the active ring before screenDirty pushes. Either: (a) the ring shift in Video completes before `events.get(id::screenDirty)` fires, or (b) Display's drain logic excludes the first `liveTailExtent` rows from the active ring on the tick when history departures arrived (the rows that just left are now in history, don't re-append). The simpler invariant: pushLine and the live region must not overlap by row index.

---

## DEBT-20260530T100000

**Observation:** Resize destroys TextLineArray history content. Upsize/downsize eventually creates active prompt at bottom with garbage-filled scrollback. Content that was rendered correctly before resize disappears or is replaced with garbage from Video's zeroed buffer.
**Divergence:** The full rebuild path (`drainCount > 0 or totalRows < contentRows`) removes old live zone entries and re-adds from Video. After resize, Video buffer is cleared (setWinsize re-allocates) but the rebuild copies zeroed/garbage rows into TLA before the shell redraws. CellFifo in-flight departures may also be stale (captured at old dimensions). The coupling between Buffer<Row> lifecycle and TLA content is the root cause — TLA should be independent of Buffer<Row> resize.
**Expectation:** Resize should NOT affect TLA content. Buffer<Row> is Video's scratch — its lifecycle is irrelevant to TLA. Only dirty rows (actually written by Video after shell redraw) should overwrite TLA entries. The full rebuild path should not copy from Video unless Video has real content (dirty-flagged rows). History must survive resize intact.

---

## DEBT-20260526T220000

**Observation:** `getStateInformation`/`setStateInformation` are stubbed. Daemon attach sends no state to connecting clients. History byte ring removed — raw byte replay path is dead.
**Divergence:** Daemon client attach should reconstruct the terminal display from the daemon's live state. With TextEditor owning winsize and Buffer<Row> owning scrollback, the serialization surface is now ValueTree + Buffer — not raw byte replay.
**Expectation:** Daemon attach serializes ValueTree state + Buffer<Row> content directly. Client deserializes and renders. No manual callbacks — ValueTree is the transport. Revisit when TextEditor rendering pipeline is fully working.

---

---

## DEBT-20260501T193217

**Observation:** split third V/H from action list did not split viewport symetrically
**Divergence:** split third with shortcut is correct, why different than action triggered by Action List
**Expectation:** any split  action from any trigger should be identical

---









## DEBT-20260411T100058

**Observation:** mermaid rendering is broken
**Divergence:** nothing render at all, just stuck
**Expectation:** loader overlay when mermaid still loading async, render correctly in given space

---
