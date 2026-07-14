# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260713T230500

**Observation:** With a hosted plugin editor embedded in a pane, keyboard input lands on the plugin's native view; END's own key handling (ENDActions via ENDView::keyPressed) no longer receives keystrokes while the plugin holds focus.

**Divergence:** Keyboard focus is taken over by the plugin — END's action keybindings (pane navigation, split/join, zoom, closePane) stop working once the embedded editor's native view becomes first responder.

**Expectation:** END retains its host-level keybindings while a plugin editor is focused — the Step 19 focus loop (outward dispatch + inward native-focus proxy, handle→pane map) governs which tier consumes which keys.
