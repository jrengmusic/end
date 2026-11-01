# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20261025T214955

**Observation:** Action List should be replaced with CommandManager. Related entry: DEBT-20261009T112447.
**Divergence:** The Action List duplicates what CommandManager supplies.
**Expectation:** CommandManager replaces the Action List.

---


## DEBT-20261025T135707

**Status:** Deferred by ARCHITECT (2026-11-01): *"ENAX by default will be using CLAP binary format (future work per grand scheme)"*. Rewritten to its remainder: *"Rewrite the debt"*.
**Observation:** END hosts EVE as VST2, VST3 and AU (`project-info.md:371-375`; `files::evePlugin`, `cast/files.md:31`). END does not host EVE as CLAP: EVE has no CLAP target. The clock part is done: `Nexus::VirtualClock` is deleted (`carol/SPRINT-LOG.md:125`), and `ClapPluginInstance::hostRequestProcess` activates an inactive plugin on the message thread (`jam/jam_clap/format/jam_ClapPluginInstance.cpp:443-459`, `carol/SPRINT-LOG.md:36`).
**Divergence:** CLAP is ENAX's default binary format (`eve/PLAN-Grand-Scheme.md` §4.3 `[AMENDED 2026-11-01 — ENAX]`). EVE owns its clock in every host: *"VirtualDevice as full throttle clock is EVE SPEC contract"* (`eve/PLAN-Grand-Scheme.md` §8 `[AMENDED 2026-10-29]`).
**Expectation:** EVE gets a CLAP target. END hosts EVE as CLAP through the jam CLAP host contract (`jam/jam_clap/clap/host.h`), with ENAX rules identical to the other formats. `request_process` reaches the plugin's own `jam::VirtualDevice`. END has no host clock.

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



