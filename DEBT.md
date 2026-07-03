# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---


## DEBT-20260629T100000

**O:** `LowLevelGraphicsContext::drawLine` still delegates to `fillPath` with a 1px-thick stroked path instead of a native Vulkan line-rasterisation pipeline. `clipToPath`, `fillPath`, `beginTransparencyLayer`/`endTransparencyLayer`, and `clipToImageAlpha` were all resolved with real Vulkan implementations across Sprint 51/52/54 (stencil pre-pass, earcut tessellation, offscreen composite, and stencil-write alpha-test respectively).

**D:** `drawLine` (`jam_VulkanLowLevelGraphicsContext.cpp:275-281`) rasterises every line as a filled triangulated path via `fillPath`, which is correct but not a dedicated native line pipeline — extra earcut triangulation cost per line draw instead of a direct `VK_PRIMITIVE_TOPOLOGY_LINE_LIST`/`LINE_STRIP` pipeline.

**E:** `drawLine` gets a native Vulkan line-rasterisation pipeline (dedicated `Pipelines::ID` + shader stage pair using `VK_PRIMITIVE_TOPOLOGY_LINE_LIST`), removing the `fillPath` delegation and its triangulation overhead.
