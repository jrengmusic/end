# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260629T100000

**O:** `LowLevelGraphicsContext::clipToImageAlpha` still needs native Vulkan GPU work (stencil mask from an image's alpha channel). `clipToPath`, `fillPath`, `beginTransparencyLayer`/`endTransparencyLayer` were resolved in Sprint 51/52 with real Vulkan implementations (stencil pre-pass, earcut tessellation, offscreen composite respectively).

**D:** `clipToImageAlpha` is a no-op stub: it calls `juce::ignoreUnused (sourceImage, transform)` and returns without writing to the stencil buffer, without intersecting `deviceSpaceClipList`, and without binding any pipeline. Any caller relying on image-alpha clipping gets an unclipped (full) region instead of the masked one.

**E:** `clipToImageAlpha` gets a native Vulkan implementation via stencil buffer pre-pass reading the source image's alpha channel + `vkCmdSetStencilReference`, mirroring the pattern already established by `clipToPath`.
