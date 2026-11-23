# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained
by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash.
No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs
are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt.
Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt
is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20261123T181545

**Observation:** In daemon mode, every END instance of one user reads and writes the same END.settings file. ARCHITECT: "Daemon mode still saves to END.settings: this must be really careful. END could be launched in multiple instance, different instance means different UNIQUE settings tree".
**Divergence:** The server mode has one settings file (files::settingsFileName in the config directory, Main.h:63), one port key in it (Main.cpp:289), and one server lock name (project name plus .server, Main.h:69-72). Two instances share one settings tree, so a second one overwrites or restores the tree of the first. There is no design for a tree per instance.
**Expectation:** Each daemon instance has its own unique settings tree. No instance reads or overwrites the settings of another instance.

---






