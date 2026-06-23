# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260623T212453

**Observation:** Post-processing shaders do not load on app startup. CRT effect only appears after editing display.lua while the app is running (hot reload). Background shaders load correctly on startup via the identical flow.
**Divergence:** The refreshParameters → loadShaders(postProcess, IDtype::postProcessing) → GL callback → Compilation::load flow is identical to background, but post-processing compilation does not produce loaded passes on first launch. The POST_PROCESSING config tree properties may be empty at the time the GL callback reads them, despite config::Model::loadFromPath having run during construction.
**Expectation:** Post-processing shaders load on startup identically to background shaders — same flow, same timing, same result. If post_processing = "crt" is set in display.lua, CRT effect should be visible from the first frame.

---
