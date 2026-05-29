# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260530T100000

**Observation:** Resize destroys TextLineArray history content. Upsize/downsize eventually creates active prompt at bottom with garbage-filled scrollback. Content that was rendered correctly before resize disappears or is replaced with garbage from Video's zeroed buffer.
**Divergence:** The full rebuild path (`drainCount > 0 or totalRows < contentRows`) removes old live zone entries and re-adds from Video. After resize, Video buffer is cleared (setWinsize re-allocates) but the rebuild copies zeroed/garbage rows into TLA before the shell redraws. CellFifo in-flight departures may also be stale (captured at old dimensions). The coupling between Buffer<Row> lifecycle and TLA content is the root cause — TLA should be independent of Buffer<Row> resize.
**Expectation:** Resize should NOT affect TLA content. Buffer<Row> is Video's scratch — its lifecycle is irrelevant to TLA. Only dirty rows (actually written by Video after shell redraw) should overwrite TLA entries. The full rebuild path should not copy from Video unless Video has real content (dirty-flagged rows). History must survive resize intact.

---

---

## DEBT-20260529T020000

**Observation:** SPSC pipeline (CellFifo + mutable tail + OSC 133 lifecycle) partially implemented but broken. Three symptoms: (1) ls output sometimes doubled — C handler COMMIT_PROMPT pushes rows to CellFifo AND mutable tail re-adds same rows from Video. (2) `clear` sometimes renders only bottom half of OMP prompt. (3) `seq 1000` output not fully rendered — content lost during high-throughput drain.
**Divergence:** Root causes identified from diagnostic log: (a) Flush ordering — screenDirty fires before promptRow Parameter flushes, handler reads stale promptRow from ValueTree. (b) mutableRows computed from stale promptRow overlaps with CellFifo drain content. (c) `mutableRows > totalRows` guard fails → no REMOVE → content accumulates. (d) Coordinate mapping between Video rows and TLA indices uses manual arithmetic (cursorRow - promptRow + 1) which drifts when State values are stale.
**Expectation:** Replace manual arithmetic with Value::map for all coordinate translation. terminal::Viewport type (packed uint64_t: cell Bounds + pixel Bounds) as single Parameter<uint64_t> — Display sole author. Flush ordering resolved by reading atomics directly (not VT properties) or by ensuring Parameter flush order. All DIAG logging must be removed. Screen caret positioning (historyRows=0 placeholder) must be fixed.

---

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
