# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20260912T130000

**Observation:** END presents correctly only with the `device.getDevice().waitIdle()` sync probe after present (jam_VulkanGraphics.cpp:770). Remove the line and the window goes blank — the sync bisect this sprint proved a CPU/GPU frame-pacing race.

**Divergence:** CPU-written frame resources are single instances shared by all frames in flight (inventory, to re-verify: projectionBuffer jam_VulkanGraphics.h:1518, primitiveRecordBuffer :1627, pathFrameBuffer :1623 — against the per-image pattern of swapchainFramebuffers :1464). The `waitIdle` is a mask that serializes the GPU every frame, not a fix. Deferred by ARCHITECT at Sprint 85 /log.

**Expectation:** ARCHITECT mandate for the paying sprint, verbatim: READ VULKAN API THOROUGHLY. READ JUCE API THOROUGHLY. UNDERSTAND OUR ARCHITECTURE THOROUGHLY. Then: isolate the racing resource from that reading, give it per-frame-in-flight ownership with correct fencing, remove the `waitIdle` line, and END renders correctly at full frame pacing.

---


## DEBT-20260713T230500

**Observation:** With a hosted plugin editor embedded in a pane, keyboard input lands on the plugin's native view; END's own key handling (ENDActions via ENDView::keyPressed) no longer receives keystrokes while the plugin holds focus.

**Divergence:** Keyboard focus is taken over by the plugin — END's action keybindings (pane navigation, split/join, zoom, closePane) stop working once the embedded editor's native view becomes first responder.

**Expectation:** END retains its host-level keybindings while a plugin editor is focused — the Step 19 focus loop (outward dispatch + inward native-focus proxy, handle→pane map) governs which tier consumes which keys.

---
