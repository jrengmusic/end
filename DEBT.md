# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20261009T112447

**Observation:** END maps key presses with its own map: ENDActions::buildKeyMap fills jam::HashMap<juce::KeyPress, juce::Identifier> keys and modalKeys from the config KEYS section (Source/action/ENDActions.h:67-92, :152-153). jam now has jam::CommandManager : juce::ApplicationCommandManager in jam_gui/commands. It fills the juce::KeyPressMappingSet from a config keys table, with command ids from map::CommandId.
**Divergence:** END does not use jam::CommandManager.
**Expectation:** Replace END's keyboard and config-key handling (the ENDActions key maps and the keys reading) with jam::CommandManager and juce::ApplicationCommandTarget.

---

## DEBT-20260912T150000

**Observation:** Sprint 86's jam changes are runtime-verified on macOS only. Each carries a Windows arm no run has exercised: the end-of-frame fence wait now also runs on the Windows swapchain branch (jam_VulkanGraphics.cpp:770-780, after the `#endif`); the Window default-glass dispatch has a `blurBehind` arm (jam_Window.cpp:87, :108); the VMA leak-only define compiles on both platforms.

**Divergence:** No Windows build or runtime check of these paths exists. The Windows composition branch always had its own post-submit wait (jam_VulkanGraphics.cpp:688-697); the swapchain branch's added wait and the default-glass arm are new behavior there.

**Expectation:** A Windows machine builds jam + END + one bootstrap standalone and verifies: rendering correct with the end-of-frame wait on the swapchain branch, default glass (blurBehind) on an unstyled window, quiet console. Divergence found there is fixed in that sprint.

---



