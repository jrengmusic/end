# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260629T100000

**O:** VulkanLLGC has 5 methods that still need native Vulkan GPU work: `clipToPath` (stencil mask from tessellated Path), `clipToImageAlpha` (stencil from alpha channel), `beginTransparencyLayer`/`endTransparencyLayer` (offscreen color attachment + composite), `fillPath` (path tessellation via earcut or equivalent).

**D:** These methods currently forward to `softwareRenderer` which writes into a `juce::Image` that is never presented (blitToSwapchain deleted). Output is invisible. END's tab bar does not exercise these paths, but any JUCE component using rounded rects, ellipses, path clips, or transparency layers will produce no visible output.

**E:** Each method gets a native Vulkan implementation: `clipToPath`/`clipToImageAlpha` via stencil buffer pre-pass + `vkCmdSetStencilReference`; `fillPath` via earcut tessellation to triangle list + indexed draw; `beginTransparencyLayer`/`endTransparencyLayer` via secondary offscreen color attachment + composite blit back to main. `drawLine` routes through `fillPath` (thick) or `fillRect` (thin 1px quad).
