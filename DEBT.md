# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260426T101948

**Observation:** action list search box failure
**Divergence:** after typing keywords and result shown, i cant pressed esc to enter list selection and run action, so search box is useless
**Expectation:** esc to enter select mode from list should always work regardless, list empty? clear text box. list exist select

---



## DEBT-20260420T062717

**Observation:** Sometimes closing vim via Ctrl+C (mapped to :qa) does not return to shell. Vim exits but terminal pane becomes dead — empty screen, static cursor. Enter does nothing. Only Cmd+W closes the pane.
**Divergence:** END's TTY::run (TTY.cpp:62-91) treats any read() == -1 on PTY master as child exit, dispatching onExit → Nexus::remove(uuid) which destroys the Session. No SIGCHLD handler, no waitpid(WNOHANG) confirmation. On macOS, PTY master can return EIO transiently when slave fd has no open holders for a moment (race between nvim closing slave fd and shell re-opening/writing prompt).
**Expectation:** Session tear-down should be gated on actual child-process exit confirmation (SIGCHLD or waitpid), not on a transient read error. Vim exit must leave shell alive and pane interactive.

---



## DEBT-20260411T100058

**Observation:** mermaid rendering is broken
**Divergence:** nothing render at all, just stuck
**Expectation:** loader overlay when mermaid still loading async, render correctly in given space

---
