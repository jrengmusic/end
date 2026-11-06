# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---


## DEBT-20261103T195500

**Status:** Deferred by ARCHITECT (2026-11-04): *"add to @DEBT.md handling plugin editor size with pane. not yet decided how."*
**Observation:** END loads any VST3 or VST plugin into a new pane split. The editor of a plugin always renders at the plugin's own UI size, not at the pane size. ARCHITECT: *"current state we could any vst3, vst, only the UI always disagree with the pane size. it cretes new pane split. but the UI always rendered as the plugin UI size."* A resizable editor takes `setBounds` with the inner pane area. Any other editor sits at the inset position (`Source/ui/EditorView.cpp`, `setEditorBounds`). A plugin that has a `PluginState` also receives the `width` and `height` rows that `EditorView::resized` writes (`Source/ui/EditorView.cpp:56-60`); `Session::addPlugin` creates a `PluginState` only for a plugin of the same manufacturer (`Source/plugins/Session.cpp:108`). The `setTransform` fit is removed: JUCE asserts that the editor transform equals `hostScaleTransform` (`juce_AudioProcessorEditor.cpp:194-199`), and the VST3 host window sizes the editor through `getLocalArea (nullptr, …)` (`juce_VST3PluginFormat.cpp:342-347`), so JFS grew from 652×1178 to 1417×2560 (`~/.config/end/end.ode`, 2026-11-04 1:50 run). A resizable plugin can refuse the pane size through `checkSizeConstraint` (`juce_VST3PluginFormat.cpp:349-372`). A JUCE VST3 plugin returns `kResultFalse` from `setContentScaleFactor` on macOS (`juce_audio_plugin_client_VST3.cpp:2187-2211`).
**Divergence:** Step 0 of `PLAN-load-plugin.md`: *"by default (including EVE), resizable audio plugin resized into the target pane."*; *"any arbitrary plugin which usually have fixed size (or scaled), should be fitted into available pane bounds by keeping their aspect ratio, only scaled."* ARCHITECT: *"when they are tiled structurally within MatrixComponent, its content MUST FIT. this non-negotiable"*.
**Expectation:** Every plugin editor fits its pane, with the aspect ratio kept, on every platform and in every format. The method is not decided.

---


## DEBT-20261025T135707

**Status:** Deferred by ARCHITECT (2026-11-01): *"ENAX by default will be using CLAP binary format (future work per grand scheme)"*. Rewritten to its remainder: *"Rewrite the debt"*.
**Observation:** END hosts EVE as VST2, VST3 and AU (`project-info.md:371-375`; `files::evePlugin`, `cast/files.md:31`). END does not host EVE as CLAP: EVE has no CLAP target. The clock part is done: `Nexus::VirtualClock` is deleted (`carol/SPRINT-LOG.md:125`), and `ClapPluginInstance::hostRequestProcess` activates an inactive plugin on the message thread (`jam/jam_clap/format/jam_ClapPluginInstance.cpp:444-460`, `carol/SPRINT-LOG.md:36`).
**Divergence:** CLAP is ENAX's default binary format (`eve/PLAN-Grand-Scheme.md` §4.3 `[AMENDED 2026-11-01 — ENAX]`). EVE owns its clock in every host: *"VirtualDevice as full throttle clock is EVE SPEC contract"* (`eve/PLAN-Grand-Scheme.md` §8 `[AMENDED 2026-10-29]`).
**Expectation:** EVE gets a CLAP target. END hosts EVE as CLAP through the jam CLAP host contract (`jam/jam_clap/clap/host.h`), with ENAX rules identical to the other formats. `request_process` reaches the plugin's own `jam::VirtualDevice`. END has no host clock.

---


## DEBT-20260912T150000

**Status:** Deferred by ARCHITECT: *"windows build still miles ahead. defer."* (`carol/SPRINT-LOG.md:82`).
**Observation:** Sprint 86's jam changes are runtime-verified on macOS only. Each carries a Windows arm no run has exercised: the end-of-frame fence wait now also runs on the Windows swapchain branch (jam_VulkanGraphics.cpp:770-780, after the `#endif`); the Window default-glass dispatch has a `blurBehind` arm (jam_Window.cpp:84, :103); the VMA leak-only define compiles on both platforms.

**Divergence:** No Windows build or runtime check of these paths exists. The Windows composition branch always had its own post-submit wait (jam_VulkanGraphics.cpp:688-697); the swapchain branch's added wait and the default-glass arm are new behavior there.

**Expectation:** A Windows machine builds jam + END + one bootstrap standalone and verifies: rendering correct with the end-of-frame wait on the swapchain branch, default glass (blurBehind) on an unstyled window, quiet console. Divergence found there is fixed in that sprint.

---



