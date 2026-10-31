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

**Observation:** END clocks each hosted plugin with `Nexus::VirtualClock`, a `juce::Thread` that waits `blockPeriodMilliseconds` (512 samples at 48 kHz, about 10 ms) and then calls `device.clock (blockSize)` (Source/Nexus.h:168-170, :173-207). END's CLAP host callback `hostRequestProcess` has an empty body (jam/jam_clap/format/jam_ClapPluginInstance.cpp:443-446). JAM's CLAP plugin wrapper receives the `clap_host` (jam/jam_clap/wrapper/jam_ClapWrapper.h:26) but never calls `request_process`.
**Divergence:** ARCHITECT ruled "Virtual audio device as clock" and "i don't like wake/sleep or period. why manually manage thread? why clock driver not event driven?". PLAN-END-plugin-host.md:300-302 states `request_process` → Nexus pumps `VirtualDevice.clock()` → processBlock; the code clocks on a fixed period instead. JAM Step 5c (jam/PLAN-terminal-swap.md) gives `jam::VirtualDevice` a thread-safe `requestProcess()` and deletes `jam::VirtualClock`.
**Expectation:** Precondition: the EVE standalone runs at or above the endless baseline (`seq 1 100000` in 0.072 s or less). Then END hosts EVE as a CLAP plugin with the exact same JAM modules. Each hosted plugin's `jam::VirtualDevice` is clocked by events: the plugin calls the host's `request_process`, and `hostRequestProcess` calls that plugin's `device.requestProcess()`. JAM's CLAP plugin wrapper gives the JUCE processor the path to `request_process`. `Nexus::VirtualClock` is deleted.

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



