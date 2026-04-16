# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260416T091057

**Observation:** text selection, mouse, line, bleed to other terminal split
**Divergence:** when selecting text in one terminal, sometimes visually other terminal get selection too. although the copied text was correct
**Expectation:** text selection should be isoloated only to active terminal

---

## DEBT-20260416T083624

**Observation:** windows open file : UsersjrengDocumentsPoemsdevendCMakeLists.txt
**Divergence:** should preserve separator
**Expectation:** open file on windows should be handled with preserving separator

---

## DEBT-20260412T234116

**Observation:** need to add confirmation dialog with ctrl+q
**Divergence:** when daemon=true, confirmation dialog should ask whether to save session or not. save session keep daemon alive, not kill all
**Expectation:** confirmation dialog, modal

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

---
