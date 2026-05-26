# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

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
