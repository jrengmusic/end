# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260521T120000

**Observation:** entering tmux inside END, garbage bytes echoed to terminal: `^[[?62;4c%` and `/62;4c_`. Need to type random input until gaining full control of active prompt.
**Divergence:** entering tmux should start cleanly with no garbage byte echo — the CSI response bytes should be consumed by tmux, not displayed
**Expectation:** tmux session starts with clean prompt, no leaked escape sequence fragments visible to the user


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
