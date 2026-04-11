# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260411T125015

**Observation:** windows: tab name not updated according to active terminal
**Divergence:** tab label always show shell name i.e zsh
**Expectation:** tab label should always fresh, updated from last active terminal on particular tab, either cwd or running tui

---

## DEBT-20260411T100058

**Observation:** mermaid rendering is broken
**Divergence:** nothing render at all, just stuck
**Expectation:** loader overlay when mermaid still loading async, render correctly in given space

---

## DEBT-20260411T095809

**Observation:** whelmed loading blocks stuck when block counts total height less than available height
**Divergence:** loading bar stuck, should never be rendered when documents total height less than viewport height
**Expectation:** loader overlay should only be rendered when documents blocks total height exceed viewport height

---

## DEBT-20260411T083120

**Observation:** action list on windows rendered solid white
**Divergence:** there are some tweaks to jreng_window module, popup looks correct, action list need to be exactly setup like popup
**Expectation:** all modal window rendering with blur and transparency must be rendered like main window

---
