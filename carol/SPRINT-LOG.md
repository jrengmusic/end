# SPRINT-LOG

---

## Sprint 110: One Focus Command for Every Pane, Standalone END Never Writes Settings, Rename Editor Focus ✅

**Date:** 2026-11-24
**Duration:** Not measured. One session from 2026-11-23 21:43 (+07:00), joined by context compactions.
**Scope (ARCHITECT):**
- Prompt and winsize on Windows (`DEBT-20261121T004127`), the new tab crash, and settings on disk: *"standalone with its own internal state, internal server should not save settings to disk"*.
- Focus: *"visual feedback and focus still not always agree… identify code smells… ONE PATH, ONE COMMAND ID, ONE LANE, ONE SSOT … dont fix the symptoms, fix the cause, fix the design. DESIGN BY CONTRACT"*, then `/go`.
- *"dont audit without my runtime check."* Runtime: *"active prompt no longer being erased"*, *"focused issues fixed"*, *"vim ctrl+direction works. drag image works. ctrl+shift+v paste works (keep it). rename tab works"*.
- ling: *"update the RFC, what actually correct, what actually completed, what actually still outstanding"*.
- *"daemon settings per instance shoule be added to DEBT"*. *"if it not writing/reading settings anymore, leave it."*

### Agents Participated
- COUNSELOR: the cause reads, the focus design, every validation read, the ling RFC, SPEC and ARCHITECTURE edits, the patch design, this log.
- Pathfinder (haiku): discovery, whatdbg and window probes, the ling, conhost and pty experiments, the doxygen pass, the debt entry.
- Librarian (haiku): the Claude Code, Neovim and console research of the ctrl+g quit.
- Engineer (haiku): the focus redesign, the audit fixes, the rename editor fix.
- Auditor (opus) ×1: 11 findings on the focus redesign.

### Files Modified (38 total: 16 jam, 15 END, 6 EVE, 1 ling)
**jam:**
- `cast/gui.md:274`, `generated/jam_Gui.h`: `MatrixCommandId::focusPane`.
- `jam_gui/layout/jam_MatrixComponent.h/.cpp`: `perform` is the only writer of the focused pane, for one command whose target is `InvocationInfo::originatingComponent`. The matrix listens to the mouse of its descendants (`mouseDown`). `split`, directions and `childRemoved` use `focusPane`. `childAdded` and `addChildrenFromState` write no focus. The EDGE `proportions` reaction moved here from `OwnerComponent`.
- `jam_gui/layout/jam_OwnerComponent.h/.cpp`: no mouse listener. The fan-out writes 0 to the other children first, then 1. `setFocusedChild` is protected.
- `jam_gui/layout/jam_TabbedComponent.h/.cpp`: layout on `focusedTab` before the fan-out; tab buttons take no keyboard focus.
- `jam_terminal/document/jam_TerminalSwap.h:31-32, 127-157`, `jam_gui/widgets/jam_TextEditor.cpp:1567`, `.h`: `numLiveSlots` 3, `numHeldLiveSlots` 1. The live frame stays held until the next one, so a prompt stays on screen over a resize.
- `jam_terminal/tty/native/jam_TerminalCygwinTTY_windows.cpp:167-173`: the assert on the size-pipe write is removed. It tested whether another process is alive (MANIFESTO B, Guard Rule).
- `patch/juce-vulkan-engine-hook.patch`: `RedrawWindow` with `RDW_NOCHILDREN` for invalidate and validate, so a parent paint does not clear the pending paint of EVE.
- `patch/juce-plugin-window-focus-hook.patch`: `handleFocusLoss` after `SetFocus` on Windows, `mouseActivateFlags = 0` in `WM_KILLFOCUS`, and `ComponentPeer::setLastFocusedSubcomponent`, which `Component::takeKeyboardFocus` calls before it takes native focus.
- `patch/juce-mouse-event-hook.patch`: the click grab needs no external mouse callback or a focused peer. `user-modules-info.md:1404, 1412, 1413`.

**END:**
- `Source/Main.h/.cpp`: `isServer()`. `settings` names no file when the `server` row is false.
- `Source/ui/EditorView.h/.cpp`: the `Id::focus` event repaints and grabs; `createProcessorEditor` runs that event; `fileDragEnter` uses `focusPane`.
- `Source/ui/SessionView.h/.cpp`, `TabView.cpp`, `ENDView.h/.cpp`: no own focus grab, no second writer of `focusedTab`; the rename editor hands focus to the active tab.
- `Source/config/theme/gfx/style.css`: `corbeau`, the `Label … WhenEditing` colours, the missing `TextEditor` colours.
- `SPEC.md`, `ARCHITECTURE.md`: focus. `DEBT.md`. `CMakeLists.txt` and `.project`: generated.

**EVE:**
- `Source/EVEView.h/.cpp`: the `focus` handler is removed; `resizeEnd` writes the size through the `size` rule. `Source/layout/parameters.md:103`. `CMakeLists.txt`, `cygwinPTY/CMakeLists.txt` and the helper zip: build output.

**ling:**
- `RFC-mcp-empty-responses-and-manifest-cwd.md`: rewritten with the completed and the outstanding items.

### Alignment Check
- [x] BLESSED principles followed. S: one writer per focused parameter. E: the owner tells, the pane reacts to its own event. B: no raw TabView pointer in the rename callbacks.
- [x] NAMES.md adhered. `focusPane`, `setLastFocusedSubcomponent` and `isServer` join existing families.
- [x] MANIFESTO.md principles applied.
- [ ] The audit ran before the audit fixes, the assert removal and the rename editor fix. ARCHITECT ran those in the last build.

### Problems Solved
- **Erased prompt and blank pane.** The END window has no `WS_CLIPCHILDREN`, and JUCE's `ValidateRgn` cleared the pending paint of EVE's child window. The live slot was also released at the read.
- **Focus and feedback disagreed.** Several writers (owner mouse, SessionView, ENDView, child add) raced. One command now writes the focused pane, and one event gives keyboard focus.
- **Settings on disk.** The standalone `PropertiesFile` has no file, so JUCE neither reads nor writes one (`juce_PropertiesFile.cpp:145, 183-187`).
- **Rename editor without keys.** `SetFocus` on END gave focus back to the plugin window through `handleFocusGain`. The asking component is now set first. Magenta: the theme set no `WhenEditing` colour (`jam_StyleCustom.cpp:11`).
- **File drop into an old build.** A probe shows layered pane windows there, so Windows finds END's window; the current build has none.
- **Claude Code quits on ctrl+g.** Not END: Windows `conhost.exe` ends with `0xC0000005` when `nvim` starts while Claude runs with `"tui": "fullscreen"`. `"tui": "default"` works inline (measured in a plain MSYS2 pty).
- **ling `[]` results.** One daemon run failed 10 of END's 16 units and keeps that failure; a new daemon indexed all of them from the same files. Not shown to be Windows or MSVC specific.
- **Audit.** Resolved: F1, F3 to F11. Rejected: F2 (EVE's `parentHierarchyChanged` grab): the locked plan keeps it and `ARCHITECTURE.md` names it.
- **Mistakes owned.** I opened `notepad` on ARCHITECT's desktop in a test; gave inferences as causes and withdrew them (the `nvim` console state, the kitty protocol, the assert that "protects" memory); wrote a wrong hunk number; listed `DEBT-20261121T004125` as open although Sprint 108 paid it.

### State for Continuation
- Regenerate doxygen in jam, END and EVE, and read the warning counts. The build of END and EVE that ARCHITECT ran is Debug; the Release tree on disk still used the JUCE tree `d612f5cc…` at 2026-11-24 00:50.
- `~/.config/dev/themes/gfx/theme.md:142` holds `bunker` for `label background when editing` and wins over the new default `corbeau` until it is deleted or edited.
- Unknown: why one `eve-cygwinPTY` helper ended in a new tab. It does not come back, and 68 starts outside END all lived. A dead helper now closes its pane (`EditorView.cpp:198-206`).
- Unknown: the crash at launch of the Debug build of 00:32 until `~/.config/dev` was deleted. No dump, WER report or event exists. The old directory held no `END.settings`, and its `server` row was false.
- Not analysed: `END.exe.23432.dmp` (2026-11-23 21:33).
- ling: `D:/Poems/dev/ling/RFC-mcp-empty-responses-and-manifest-cwd.md`, 5 open questions.

### Debts Paid
- `DEBT-20261121T004127` — a new pane no longer erases the active prompt. ARCHITECT: *"active prompt no longer being erased"*.
- `DEBT-20261121T004129` — ARCHITECT: *"no issue with active prompt race with seq anymore, that have been paid."*
- `DEBT-20261121T004125` — paid in Sprint 108; drained now.

### Debts Deferred
- `DEBT-20261123T181545` — each daemon instance needs its own settings tree (ARCHITECT's command).

---

## Sprint 109: Windows DirectComposition, Static CRT Virtual Listeners, Direct Hex Theme Aliases, and CygwinPTY Build Chain ✅

**Date:** 2026-11-23
**Duration:** Full session

### Agents Participated
- COUNSELOR: root-cause triage, `whatdbg` tracing, four execution plans (`PLAN-windows-external-render-layered-contract.md`, `PLAN-windows-winsize-and-overlay-contract.md`, `PLAN-windows-crt-hex-alias-and-cygwinpty-chain.md`, `PLAN-windows-hex-validator-and-dcomp-clip.md`), step validations, Auditor triage, `SPEC.md` sync, sprint log.
- Pathfinder (flash): codebase discovery, `whatdbg` traces, Win32/DComp/ConfigValidator reads, post-audit doxygen pass across 7 headers.
- Librarian (flash): Microsoft Learn DirectComposition parent-child window clipping and visual tree contract research.
- Engineer (inherit): implementation of all plan steps, Auditor Findings 1 and 2 fixes, and deletion of 5 completed `PLAN-windows-*.md` files.
- Auditor (inherit): comprehensive sprint audit (4 findings; 2 code bugs fixed, 1 `SPEC.md` sync applied, 1 PLAN pivot recorded).

### Files Modified (21 total)
**jam:**
- `patch/juce-vulkan-engine-hook.patch:50-59,254-261` — `HWNDComponentPeer::computeTransparencyKind()` returns `TransparencyKind::opaque` when `externalContextFactory != nullptr`, preventing un-updated `WS_EX_LAYERED` hit-test transparency; `handlePaintMessage()` calls `setLayeredWindow()` on fallback.
- `patch/juce-image-subsection-hook.patch:19-59` — added `virtual void addListener (Listener*)` and `virtual void removeListener (Listener*)` to `juce::ImagePixelData` so cross-DLL calls stay inside the owning static `/MT` CRT heap (ARCHITECT rejected `/MD` pivot from `PLAN-windows-crt-hex-alias-and-cygwinpty-chain.md` Step 1).
- `user-modules-info.md:1403-1404` — updated descriptions for `juce-image-subsection-hook.patch` and `juce-vulkan-engine-hook.patch`.
- `jam_gui/widgets/jam_TextEditor.cpp:2017`, `jam_TextEditor.h:415-424` — `lookAndFeelChanged()` calls `viewport.lookAndFeelChanged()` before `getTerminalBounds()` and `resized()`, fixing initial scrollbar width before `setWinsize()`.
- `jam_vulkan/context/native/jam_VulkanComposition_windows.cpp:32-36`, `jam_VulkanComposition_windows.h:22-34` — clears `WS_CLIPCHILDREN` and flushes styles via `SetWindowPos` before `CreateTargetForHwnd (hwnd, FALSE, ...)` so parent Layer 2 backgrounds composite behind transparent child HWNDs (`EVE`).
- `jam_vulkan/resource/jam_VulkanBindlessRegistry.cpp:18,57,86,137` — switched `ImagePixelData` listener registration to virtual `addListener` / `removeListener`.
- `jam_vulkan/resource/jam_VulkanTextureCache.cpp:7,13,43-59,207,256-291`, `jam_VulkanTextureCache.h:157-161` — switched listener registration to virtual `addListener` / `removeListener`, and fixed `evictUnusedEntries()` iterator invalidation by passing and returning `jam::HashMap::iterator` in `retireEntry()`.
- `jam_markdown/document/jam_ConfigValidator.cpp:313-314,416-425`, `jam_ConfigValidator.h:31-33,50-51` — `isAliasTarget()` accepts `ConfigDocument::isHexColour()`, and `getReferenceFailure()` excludes `Id::alias` rows so `getAliasFailures()` is the sole validator.
- `jam_markdown/document/jam_ConfigDocument.cpp:395-413,844-850`, `jam_ConfigDocument.h:165-176,225-237` — `addAliasProperties()` and `getAliasWords()` support direct `0xAARRGGBB` hex colours on `alias` rows in both `getValueTree()` and `toDocument (const CssDocument&)`.
- `jam_markdown/document/jam_CssWriter.cpp:53-67`, `jam_CssWriter.h:22-24` — `getAliasDeclaration()` writes `#RRGGBBAA` when `ConfigDocument::isHexColour()` matches.
- `jam_web/css/jam_Css.cpp:404-419,2093-2103`, `jam_Css.h:193,217-218,366-372,449-450` — `addAppearance()` stores hash-token colours as `juce::uint32`; `addAliases()` writes them as `juce::int64` to `ValueTree`.

**END:**
- `Source/ui/ENDView.cpp:16-19,48-54,161-163`, `Source/ui/ENDView.h:27-33,38-42,262-263` — attached `messageOverlay` as a sibling desktop peer via `addToDesktop` + `peer->getAreaCoveredBy (*this)`.
- `SPEC.md:173-174` — documented direct `0xAARRGGBB` hex colours on `alias` rows.
- Deleted completed sprint plans: `PLAN-windows-external-render-layered-contract.md`, `PLAN-windows-winsize-and-overlay-contract.md`, `PLAN-windows-crt-hex-alias-and-cygwinpty-chain.md`, `PLAN-windows-hex-validator-and-dcomp-clip.md`, `PLAN-windows-focus-and-headless-pty.md`.

**EVE:**
- `cast/cmake.cast:632-636`, `cast/spell.md:18,103-104`, `project-info.md:118-223` — wired `cygwinPtyBundle.cmake` (`if(WIN32)`) into all 13 `## toolchain` lanes so building from `END` chains `cygwinPTY` -> `eve` -> `end`.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **Windows mouse selection / drag / wheel blocked under ExternalRenderContext:** `WS_EX_LAYERED` without `UpdateLayeredWindow` made `ViewComponent::Inner` hit-test transparent. Fixed in `juce-vulkan-engine-hook.patch`.
- **Windows initial scrollbar width mismatch and `messageOverlay` occluded by `EVE`:** Fixed `TextEditor::lookAndFeelChanged()` ordering and promoted `ENDView::messageOverlay` to a sibling desktop peer.
- **Cross-DLL heap crash (`__acrt_first_block == header`) under static `/MT` CRT:** Dispatched `ImagePixelData` listener add/remove through virtual methods on `juce::ImagePixelData` so allocations and frees stay in the owning binary's heap.
- **Stale `eve-cygwinPTY.exe` erasing active prompt:** Chained `cygwinPtyBundle.cmake` into all `EVE` cast toolchain lanes.
- **Direct `0xAARRGGBB` hex colours rejected on `alias` rows:** Added end-to-end hex support in `ConfigValidator`, `ConfigDocument`, `CssWriter`, and `CssDocument`, and excluded `Id::alias` from `getReferenceFailure()`.
- **Focused pane background clipped behind transparent `EVE` child HWND:** Cleared `WS_CLIPCHILDREN` before `CreateTargetForHwnd (hwnd, FALSE, ...)` in `VulkanComposition::create()`.
- **Audit clean sweep:** Fixed `VulkanTextureCache::evictUnusedEntries()` iterator invalidation via `HashMap::erase (iterator)` and `ConfigDocument::getAliasWords()` `juce::uint32` round-trip formatting.

### State for Continuation
- ARCHITECT confirmed all runtime fixes prior to `/audit`. After the two post-audit fixes (`jam_VulkanTextureCache.cpp` and `jam_ConfigDocument.cpp`), rebuild and run `<leader>bd` in `jam` to regenerate doxygen XML/HTML and confirm zero warnings.

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261121T004127` — a new pane erases the active prompt of the last focused pane.
- `DEBT-20261121T004129` — the prompt shows before the `time seq` output has finished on screen.

---

## Sprint 108: Cross-Platform VST2 Focus Orchestration, Headless Cygwin PTY, and Audit Clean Sweep ✅

**Date:** 2026-11-22
**Duration:** 04:10

### Agents Participated
- COUNSELOR: root-cause synthesis, `PLAN-windows-focus-and-headless-pty.md`, step validations, Auditor triage, root docs sync (`ARCHITECTURE.md`, `SPEC.md`, `CLAUDE.md`, `README.md`), sprint log.
- Pathfinder (flash): project state discovery, `eve-cygwinPTY.exe` spawn trace, END/EVE/JAM/JUCE Windows focus pipeline trace, Framework Inventory reads, post-audit doxygen passes (`jam_TerminalConsole_windows.h`, `EditorView.h`, `jam_FileWatcher.cpp`).
- Engineer (inherit): Steps 1–3 implementation, sprint audit Finding 1 fix (`EditorView.cpp:277`), `/audit` fixes (`jam_FileWatcher.h:6`, `jam_FileWatcher.cpp:7,13,432-433`).
- Auditor (inherit) ×2: sprint audit (1 finding, resolved) and `/audit` sweep (6 findings + 1 bug, all resolved or noted).

### Files Modified (11 total)
**jam:**
- `jam_terminal/tty/native/jam_TerminalConsole_windows.cpp:221-225` — passes `CREATE_NO_WINDOW` in `CreateProcessW` when `reference == nullptr`, so `eve-cygwinPTY.exe` runs headless without opening Windows Terminal.
- `jam_terminal/tty/native/jam_TerminalConsole_windows.h:54-72` — doxygen updated for `createProcess` `CREATE_NO_WINDOW` behavior.
- `patch/juce-plugin-window-focus-hook.patch:1-56` — sets `setWantsKeyboardFocus (true)` unconditionally in `VSTPluginWindow` constructor and adds one cross-platform `focusGained (FocusChangeType) override` (macOS `makeFirstResponder`, Windows `SetFocus (pluginHWND)`, Linux/BSD `xembedComponent.grabKeyboardFocus()`).
- `user-modules-info.md:1412` — updated `juce-plugin-window-focus-hook.patch` description row.
- `jam_core/file/jam_FileWatcher.h:6`, `jam_core/file/jam_FileWatcher.cpp:7,13,419-435` — replaced `or` with `||` in `#if` directives (`CODING.md:38,1072`), added `<errno.h>` and `else if (numBytes == 0 or errno != EINTR) signalThreadShouldExit();` in Linux `Source::run()`, and updated its doxygen block.

**END:**
- `Source/ui/EditorView.cpp:277-278`, `Source/ui/EditorView.h:209-217` — `createProcessorEditor()` calls `editor->grabKeyboardFocus()` when `editor != nullptr`, `isShowing()`, and `Id::focus` is true; doxygen updated.
- `ARCHITECTURE.md:758,845-851`, `SPEC.md:26,37,121-130,520-523`, `CLAUDE.md:9-13,62`, `README.md:4,94,117` — synced focus contract, removed stale `DEBT-20261025T135707` and fixed-size editor debt citations, removed WHELMED-as-plugin references.
- `PLAN-windows-focus-and-headless-pty.md` — sprint plan.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [ ] Build and runtime verification on Windows, macOS, and Linux pending ARCHITECT test (`cast cast/spell.md` in END to refresh patched JUCE cache).

### Problems Solved
- **Windows Terminal spawned per EVE instance:** `eve-cygwinPTY.exe` (`/subsystem:console`) was spawned by `TerminalCygwinTTY::open` with `reference = nullptr` and without `CREATE_NO_WINDOW`, causing Windows to open a visible default console and steal focus. Fixed in `TerminalConsole::createProcess`.
- **Windows focus lost (`DEBT-20261121T004125`):** `juce-plugin-window-focus-hook.patch` was `#if JUCE_MAC`-only, so on Windows `VSTPluginWindow` rejected keyboard focus and never called `SetFocus (getPluginHWND())`, and every click forwarded by `externalMouseEventCallback` stole Win32 focus back to END's top-level HWND. Unified `setWantsKeyboardFocus (true)` and `focusGained` cross-platform on `VSTPluginWindow` and handed focus to newly created focused editors in `EditorView::createProcessorEditor()`.
- **`/audit` clean sweep:** Fixed `#if ... or ...` preprocessor violations in `jam_FileWatcher.h:6` and `jam_FileWatcher.cpp:13`; fixed Linux `::read` EOF/error spin in `jam_FileWatcher.cpp:432-433`; synced `ARCHITECTURE.md`, `SPEC.md`, `CLAUDE.md`, and `README.md`.

### State for Continuation
- ARCHITECT runs `cast cast/spell.md` in END to re-apply `juce-plugin-window-focus-hook.patch`, builds END + jam, and verifies headless `eve-cygwinPTY.exe` and Windows pane click/focus behavior at runtime.
- `ninja doxygen` in jam Builds and END Builds to regenerate XML/HTML and confirm zero warnings.
- Open PLAN files kept at END root awaiting ARCHITECT confirmation to delete: `PLAN-windows-focus-and-headless-pty.md`, `PLAN-one-lane-native-state.md`, `PLAN-style-driven-theme.md`.

### Debts Paid
- `DEBT-20261121T004125` — unified cross-platform `VSTPluginWindow` focus contract (`setWantsKeyboardFocus (true)` + `focusGained`) and `EditorView::createProcessorEditor()` initial focus grab.

### Debts Deferred
- `DEBT-20261121T004127` — a new pane erases the active prompt of the last focused pane.
- `DEBT-20261121T004129` — the prompt shows before the `time seq` output has finished on screen.

---

## Sprint 107: jam::File::Watcher Rewritten Clean-Room, Gin Remnants Removed, No Kind Prefix in NAMES ✅

**Date:** 2026-11-21
**Duration:** Not measured. One session, after Sprint 106 in the same session.
**Scope (ARCHITECT):**
- The Gin license file: *"delete please"*.
- The Gin watcher: *"that's exactly how we work here. we vendor, when it reached its limit, not suitable for our domain contract, we evolve, we rewrite our own, but we still keep attributes when we genuienely use it as is. clean it up"*; *"also fix linux?"*; `/go DESIGN BY CONTRRACT` on the plan.
- The contract files: *"kFSEventStreamEventFlagItemModified fix the @carol/NAMES.md @carol/CODING.md"*.
- Runtime: *"built clean. reload works."*; *"linux test be tested on linux machine"*.
- *"add to DEBT"*: three entries. *"/log when you're done"*.

### Agents Participated
- COUNSELOR: the reads of the old watcher, the owner APIs (`juce::AsyncUpdater`, `juce::Timer`, `jam::AsyncUpdater::Source`) and the consumers; the plan; the rewrite brief; every validation read; the audit and doxygen triage; the registry repair; the NAMES.md and CODING.md edits; this log.
- Pathfinder (haiku): the consumer discovery, and the doxygen pass of the watcher.
- Engineer (haiku): the license deletion, the rewrite, the audit fixes, the `carol debt add` run.
- Auditor (opus) ×1: 10 findings.

### Files Modified (13 total)
**jam:**
- `jam_core/file/jam_FileWatcher.h`, `jam_FileWatcher.cpp` (new): `jam::File::Watcher`, a clean-room rewrite. One platform `Source` per folder (macOS FSEvents on its own serial queue; Windows overlapped `ReadDirectoryChangesW` through `jam::AsyncUpdater::Source`; Linux inotify on a `juce::Thread`). Each Source hands events to `addEvent`, which keeps the latest `Event` per file under `pendingLock`. A private `juce::AsyncUpdater` moves the delivery to the message thread, where one `juce::Timer` window delivers all pending events. `sources` is the last member, so the Sources die before the delivery bases. Kept API: `addFolder`, `coalesceEvents`, `addListener`, `removeListener`, `Listener::fileChanged`, the unscoped `Event` values.
- `jam_core/file/jam_Listener.h`, `jam_Listener.cpp`, `jam_core/file/LICENSE_Gin.txt`: deleted.
- `jam_core/jam_core.h:112`, `jam_core/jam_core.cpp:7`: the includes of `jam_FileWatcher`.
- `user-modules-info.md`: the `LICENSE_Gin.txt` row deleted; the rows at `:260`, `:278` renamed.
- `jam_core/file/jam_File.h:398-399`: `class Watcher;`, and the comment names `jam_FileWatcher.h`.

**CAROL:**
- `~/.carol/NAMES.md` Rule 2: "No kind prefix" (`k`, `m_`, `g_`, `s_`) and "External names stay as they are" (SDK names such as `kFSEventStreamEventFlagItemModified`).
- `~/.carol/CODING.md:620`, `:625`: the example `kSomeConstant { 42 }` is now `numBufferedEvents { 16 }`.

**END:**
- `DEBT.md`: three entries. `CLAUDE.md`: Current State.

### Alignment Check
- [x] BLESSED principles followed. B: ownership order replaces the `WeakReference` and `callAsync` lifetime guards. E: the Windows wait uses the framework API `jam::AsyncUpdater::Source`; the OS flags map to `Event` through lookup tables. S: one pending map and one coalescing rule for all platforms.
- [x] NAMES.md adhered. Names come from the jam and JUCE families (`addEvent`, `applyToChanges`, `isWatching`, `fileEvents`, `numBufferedEvents`).
- [x] MANIFESTO.md principles applied.
- [ ] Built and run on Windows only (*"built clean. reload works."*). macOS is not built. Linux cannot build (State for Continuation).

### Problems Solved
- **A verbatim fork under jam's name.** The old header said *"Forked verbatim from FigBug/Gin gin_filesystemwatcher"* (`jam_Listener.h:4-8`, deleted). ARCHITECT's rule keeps an attribution only while the code is used as is. The rewrite holds none of Gin's code, so the license file and the attribution are gone.
- **Lifetime guards.** Every platform callback posted through `juce::MessageManager::callAsync` with a `juce::WeakReference` (old `jam_Listener.cpp:64`, `:360`). Now the owner's declaration order guarantees that no event reaches a destroyed watcher (MANIFESTO B).
- **The Linux branch did not compile.** It used `LockFreeQueue<Event>` (old `jam_Listener.cpp:266`), which nothing in jam or JUCE defines. Its mask also left out `IN_CLOSE_WRITE`, which it then tested.
- **Unused API removed:** `removeFolder`, `removeAllFolders`, `getWatchedFolders`, `Listener::folderChanged` (no caller, no override).
- **Audit.** Resolved: F1 (Windows read re-issued after every completion, so an aborted read cannot stop the watcher in silence), F2 (no second name buffer), F4 (Linux ends its loop on `IN_IGNORED`), F5 (macOS reports a missing path as deleted), F7 (`or` in the guards), F8 (`class`, not `struct`), F9 for `jam_File.h:398`. Rejected:
  - F3 (`IN_MODIFY`): the plan added `IN_CLOSE_WRITE` and did not remove `IN_MODIFY`. macOS and Windows also report writes in progress.
  - F6 (Linux watches one level): ARCHITECT accepted *"It stays non-recursive, as now"* before `/go`.
  - F9 PLAN citations: they record the state that was read at the time.
  - F10 (`removeListener` in `~PluginEditor`): existing kernel code outside the sprint; ARCHITECT: *"this is kernel modules. it will be synced to KANJUT"*.
- **Doxygen pass.** Rejected two Pathfinder findings: the `class Source;` PIMPL declaration (CODING.md:666) and the `-1` of `waitForThreadToExit`, the JUCE "wait for ever" value, as at `Nexus.cpp:154`.
- **Mistakes owned.** I cited `LICENSE_Gin.txt` as an attribution practice before I read which code it covered. An Engineer edit removed the newline after `user-modules-info.md:298`, which joined the `jam_core.cpp` row to it. I put the line break back.

### State for Continuation
- **Linux.** jam_core does not compile on Linux. `jam_AsyncUpdater.h:119` declares `open (const AsyncUpdater*, Handle)`, and `Handle` exists only for macOS and Windows (`:75-81`). So the Linux `Source` (`jam_FileWatcher.cpp`) is not built. ARCHITECT: *"linux test be tested on linux machine"*. Also, `run()` reads again with no pause when `::read` keeps failing (`jam_FileWatcher.cpp:423-432`).
- **macOS.** Build and run: the FSEvents `Source` is new code.
- Open PLAN files still cite the deleted `jam_Listener.h`: `eve/PLAN-message-thread-no-choke.md:51`, `:65`; `ling/PLAN-ling-6-manifest.md:104`, `:215`; `ling/PLAN-ling-8-daemon.md:75`, `:453`.
- Regenerate doxygen in jam and read the warning count.

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261121T004125`: input lost in a pane until a tab closes or opens.
- `DEBT-20261121T004127`: a new pane erases the active prompt of the last focused pane.
- `DEBT-20261121T004129`: the prompt shows before the `time seq` output has finished on screen.

---

## Sprint 106: Windows END Hosts EVE with Shared Vulkan — No Null Factory Call, No Release Hang, One Shell per Pane at Its Real Size ✅

**Date:** 2026-11-21
**Duration:** Not measured. Several sessions, joined by context compactions.
**Scope (ARCHITECT):**
- `/pay`, `vulkan` (`DEBT-20260912T150000`). The debt text was stale: *"the issue is EVE loaded at END with shared Vulkan"*; *"i have no issue with mac"*; *"one by one. fix the crash first"*.
- The Release hang: *"release build, hangs at the time terminal created"*. The `/GL` warning: *"fix it and probe the crash"*. PDB flags: *"just add it"*; *"if you need to read eve pdb just add flags"*.
- The leaked `%` before the prompt. Ruling: *"ViewEditor contract is with plugin_bootstrap. this is kernel modules. it will be synced to KANJUT. JFS will break"*, then *"ViewEditor ctor overload with explicit (width, height)"*.
- The double shell: *"double shell itself is wrong. why double?"*; *"no fake cwd"*, *"no fake size"*, *"must be deterministic"*; *"TextEditor write to Model isnt it?"*; *"so where is the absolute seam? that works as SSOT?"*, then `/go`.
- Binding rulings: *"MUST CHECK THOROUGHLY, NEVER GUESS AND SUGGEST MACRO GUARD AT ARBITRARY PLACE"*; *"NEVER FIX THE SYMPTOMS. fix the cause, fix the design, design by contract"*; *"be careful with those changes, i had no issues with mac, ensure it was windows only"*.
- `/audit doxygen pass then /log no-gate`. *"all DEBT paid"*.

### Agents Participated
- COUNSELOR: the probe specs and every cause read (VST2 host and client, JUCE graph and player, VirtualDevice, Model restore), every validation read, the audit triage, the SPEC, ARCHITECTURE, CLAUDE.md and `parameters.md` edits, this log.
- Pathfinder (haiku): whatdbg traces and samples of Release END and EVE, discovery reads, two doxygen passes.
- Engineer (haiku): the Vulkan, Nexus, ViewEditor, EVEView and EVE shell-seam edits, and the audit fixes.
- Auditor (opus) ×1: 3 code findings and 9 document findings.

### Files Modified (19 total)
**jam:**
- `jam_vulkan/engine/jam_VulkanGraphicsEngine.cpp:133`: `getOrCreateGraphics (juce::ComponentPeer&)` creates Graphics only for a component with non-empty local bounds, on every platform. The Windows `createContext` (`:430-497`) has no check of its own. `.h`: the `getOrCreateGraphics` and `createContext` blocks.
- `jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp:23-31`: the context factory forwards to the host factory while it is set. When the host factory is null, it sets the process factory to null, repaints the peer's component and returns nullptr. The patch then gives the peer its default engine. `.h`: the constructor block.
- `jam_plugin_bootstrap/view/jam_ViewEditor.h`: a `create` overload and a private constructor with explicit `width` and `height`. The existing `create` and constructor are unchanged (kernel, synced to KANJUT).

**END:**
- `Source/plugins/Nexus.h/.cpp`: Nexus also derives privately from `juce::AsyncUpdater`. `run()` opens the audio devices on the Nexus thread, then triggers the update. `handleAsyncUpdate()` adds the player on the message thread.
- `project-info.md` `## release`: the Windows `/GL` cell is empty (clang-cl ignored it; LTO comes from `-flto=thin`). The probe rows `debugSymbols` (`/Zi`) and `linkDebugSymbols` (`/debug`) were added for the PDB traces, then removed. ARCHITECT: *"remove /debug and /zi for release as they were"*.
- `SPEC.md` (EVE's shell start, `cwd` fallback), `ARCHITECTURE.md` (Nexus line refs, the factory set and clear, EVE's factory forward), `CLAUDE.md` (Current State).

**EVE:**
- `project-info.md`: the probe rows `programDatabase` (Release, Windows, `/Zi` and `/debug`) were added for the PDB traces, then removed. Net: no change.
- `Source/EVEProcessor.h/.cpp`: the constructor writes no `cwd`. Only `Id::winsize` calls `terminalProcessor.prepare()`.
- `Source/EVETerminalProcessor.h/.cpp`: the `prepare()` gate is `winsize.isValid()` only. `releaseResources()` stops and closes the clock and keeps the TTY and `ttyResize`. RAII closes them, because they are declared after `device`.
- `Source/EVEView.h/.cpp`: `initialiseView` builds the view at `jam::ViewEditor::getViewBounds` of the terminal `width`, `height` and `zoom` rows, which is the formula of `resized()`.
- `Source/layout/parameters.md`: the `width` and `height` descriptions name their three writers. `SPEC.md`: the shell start.

### Alignment Check
- [x] BLESSED principles followed. D: the readiness of a peer is checked once, at the owner of the create. S: the first valid `winsize` in the Model is the one seam of the shell. The view size has one formula. E: the restore and the host tell; EVE does not ask a host lifecycle for its shell. B: the shell's TTY lives and dies with `EVETerminalProcessor`.
- [x] NAMES.md adhered. No new names except `viewBounds` (the `getViewBounds` family).
- [x] MANIFESTO.md principles applied.
- [ ] Not built after the shell seam, the audit fixes and the owner move. ARCHITECT ran the Release lane after the crash, hang and `%` fixes.

### Problems Solved
- **Null call at `0x0` (the debt's stack, `jam_ENAXClientExtensions.cpp:23`).** A 0×0 EVE peer reached the Windows `createContext`. The composition create failed, `setGpuAvailable (false)` cleared END's factory (`jam_VulkanGraphicsEngine.cpp:301-303`), and EVE's forward called it. The owner now skips an empty component. EVE's forward also honours a cleared host factory, so any other close of END's gate no longer crashes EVE.
- **Release hang at terminal creation.** `addAudioCallback` ran on the Nexus thread, and a whatdbg sample pinned the deadlock. It now runs on the message thread through `handleAsyncUpdate`, after the device opens on the Nexus thread. The ruling *"On a background thread"* (sound card) still holds.
- **The `%` before the prompt.** The first zsh started at EVE's config size, 97×37, which the kernel `ViewEditor` constructor gave the view, while the grid was 74 columns. PROMPT_SP padded 96 spaces and wrapped. EVE now builds the view through the explicit-size overload, at the Model's size. Verified in a Release run: first open at 74×26, 73 spaces, no wrap.
- **Two shells per pane.** The shell lived between the host's `prepareToPlay` and `releaseResources`. The player attached after EVE's first shell opened. Its `audioDeviceAboutToStart` released and prepared the graph (`juce_AudioProcessorPlayer.cpp:364-371`, `juce_AudioProcessorGraph.cpp:459-483`), and `tty.reset()` killed shell 1. The seam is now the first valid `winsize`, and a host release does not close the shell.
- **Fake `cwd` and its loop.** The constructor always wrote the config `cwd`, and `openTTY`'s `cwd` write re-triggered `prepare`. Both are gone. `openTTY` falls back to the config start directory when the Model has no local `cwd` (`EVETerminalProcessor.cpp:475-476`).
- **Audit.** Resolved: F1 (null host factory), F2 (empty bounds at the owner), F3 (one view-size formula), documents F4 to F8, F10 to F12. F9 (stale `DEBT-20260912T150000` text) resolved by the drain. F2 was first rejected against *"ensure it was windows only"*, then corrected after ARCHITECT's *"isnt single check whether peer size is ready crossplatform?"*.
- **Doxygen.** Two passes, each block read against the code. Pathfinder's "own factory" correction was accepted: `externalContextFactory` is one static pointer for the process (patch `:325`).
- **Mistakes owned.** A Pathfinder sent with the LSP; scope widened to the Editor debt; a race guessed and a Windows macro guard proposed; `project-info.md` edited by COUNSELOR; two wrong explanations of the `%`; removal of the kernel `setSize` proposed; F2 rejected on a wrong reading of a ruling.

### State for Continuation
- Build: `cast cast/spell.md --no-sign` (Release) or `cast cast/spell.md --debug` in END. The default lane signs and runs `gh.cmake`.
- Runtime checks on Windows: one `openTTY` per pane at load; a split keeps one shell per pane; a GPU gate close in END paints EVE with its default engine, with no crash.
- Runtime check on macOS: a 0×0 peer now gets the CPU context for that frame (`jam_VulkanGraphicsEngine.cpp:133`, `:425-426`).
- Not addressed (ARCHITECT's report, not ruled): a split resize erases a pane's active prompt until the next input. When a prompt is active and the grid size changes, `TerminalProcessor::reset` erases the screen (`jam_TerminalProcessor.cpp:37-40`). Each resize goes through `device.stop`, `device.open` and `prepare (winsize, history)` (`EVETerminalProcessor.cpp:370`, `:392`, `:536-572`).
- Restore order: `Model::setValuesFrom` sets one property at a time and notifies at once (`jam_Model.cpp:261-267`). `parameters.md` creates `winsize` (`:57`) before `cwd` (`:59`). So a restored pane can open its shell before its saved `cwd` arrives. Check it against END `SPEC.md` *"Own saved cwd"*.
- KANJUT `kuassa_ViewEditor.h:35` has no explicit-size `create`. The kernel sync is ARCHITECT's.
- Regenerate doxygen in jam, END and EVE, and read the warning counts.

### Debts Paid
- `DEBT-20260912T150000`: Windows END hosting EVE with shared Vulkan. The null factory call, the Release hang, the `%` and the double shell are fixed.
- `DEBT-20261103T195500`: ARCHITECT: *"all DEBT paid"*.
- `DEBT-20261025T135707`: ARCHITECT: *"all DEBT paid"*.

### Debts Deferred
- None

---

## Handoff to COUNSELOR: END Host Contract — EVE Hosted at END as VST2

**From:** COUNSELOR
**Date:** 2026-10-31
**Status:** Ready for Implementation (plan written, not locked, not executed)

### Read First
1. `PLAN-end-host-contract.md` — the next plan, 8 steps, with every ruling and fact cited. ARCHITECT approves it before execution.
2. `PLAN-eve-shared-engine.md` — the running sprint. Steps 1-7 and 10 are done. Its Step 8 (gate C), Steps 9-11 (CLAP), Steps 12-13 (daemon), Step 14 (docs) run after the new plan.
3. The previous handoff below (two consecutive PLANs), for the sprint-N context.

### Context
- Objective: END hosts EVE as an in-house plugin and paints it through END's `VulkanGraphicsEngine` (one device), with END owning focus and keys.
- First pane renders EVE's terminal with a transparent background (ARCHITECT: "plugin loaded, on first pane works. terminal working, transparent background", END Release + EVE Release, VST3 at that time).
- END now loads only `EVE.vst` (VST2), one flow per pane. That build has not been run by ARCHITECT yet.
- The rest of the in-house rules (engine, keys, focus, transparency) are scattered over five sites. ARCHITECT ruled one SSOT: `jam::PluginHostType` derived from `juce::PluginHostType`, END registered as a known JUCE host, one extension export.

### Completed
- **PLAN-eve-shared-engine Step 1:** no split crash. A whatdbg run exited with code 0 and no exception stop.
- **Step 2:** `MenuTarget`, `JAMSheetDelegate`, `JAMSheetContentView` are `juce::ObjCClass` subclasses (`jam_Menu.mm:7-73`, `jam_ModalSheet_mac.mm:15-69`). `jam_gui.mm:1-2` defines `JUCE_CORE_INCLUDE_NATIVE_HEADERS` / `JUCE_CORE_INCLUDE_OBJC_HELPERS` (sibling `jam_clap.mm:1-2`).
- **Step 3:** END exports `jam_host_context_factory`, `jam_host_peer_deleted_callback`, `jam_host_key_pressed_callback` (`end/Source/Main.cpp:29-63`, inside `/** @cond */`). `jam::ClapServices` is deleted (`jam_clap.h`, `Nexus`).
- **Step 4:**
  - EVE's host branch (`jam_PluginEditorLayout.h:117-134`).
  - Typeface identity: both contexts intern the typeface of the current font (`jam_CpuLowLevelGraphicsContext.h:267-291`, `jam_VulkanLowLevelGraphicsContext.cpp:1695-1706`).
  - CPU context temporary images are software (`jam_CpuLowLevelGraphicsContext.h:293-296`).
- **Step 5 (partial):**
  - `jam/patch/juce-key-pressed-hook.patch` (`ComponentPeer::externalKeyPressedCallback`).
  - `ENDView::keyPressed` is the single END resolver (`ENDView.cpp:69-72`); `addKeyListener` removed from `ENDView`.
  - EVE `editorWantsKeyboardFocus TRUE` (`eve/project-info.md:110`).
- **Step 6:** `jam/patch/juce-plugin-transparency-hook.patch`: `ComponentPeer::getHostFunction`, VST3 ContentWrapperComponent transparent under END, host VST3/AU windows and `InnerNSView` non-opaque. VST2 client wrapper NOT covered.
- **Step 7:** `Nexus` split into `Nexus.h` + `Nexus.cpp`. `registerAudioUnit` was added and then deleted by ARCHITECT's VST-only ruling.
- **Step 10:** `ClapPluginInstance::hostRequestProcess` activates an inactive plugin on the message thread (`jam_ClapPluginInstance.cpp:443-459`).
- **One flow, VST only:**
  - `Session` is a `ValueTree::Listener` on its own state. Each pane row added creates the pane's plugin synchronously (`Session.cpp:31-90`).
  - `files::evePlugin` = `EVE.vst` from `cast/files.md:31`.
  - Deleted: `Nexus::evePluginId`, `Nexus::createPlugin`, `Id::newPlugin` (action and `cast/identifiers.md` row), `registerAudioUnit`.
- **EVE pack:** Release and Debug both pack into `end/Source/plugins/` (`eve/cast/cmake.cast:490-522`, Release gate removed). Both `<leader>br` (`--no-sign`) and `<leader>bb` (`--debug`) build the `post-build ALL` target (`~/.config/nvim/lua/core/cast-build.lua:22-32`, `eve/project-info.md:132-142`).
- **Doxygen warning fix:** `Main.cpp:29`, `:37` `/** @cond */ … /** @endcond */` (JUCE `juce_ObjCHelpers_mac.h:395`, `:437`).

### Remaining
1. ARCHITECT approves `PLAN-end-host-contract.md` (`/go`). COUNSELOR executes Steps 1-8 with @Engineer, validating each step; @Auditor once at the end.
2. ARCHITECT runs `cast cast/spell.md` in END to generate `files::evePlugin` and drop `Id::newPlugin`, then builds END. The one-flow change (`Session.cpp:31-90`) is untested at runtime.
3. Known defects the new plan addresses:
   - `jassert` at `juce_Component.cpp:3033` from `EVEView.cpp:12` (Plan Step 6).
   - END chords fail with EVE focused; visual `y` and ESC fail (Plan Step 7, trace first).
   - VST2 client wrapper is opaque/black (`juce_audio_plugin_client_VST2.cpp:931`, `:942`, `:956-959`) (Plan Step 5).
4. Not yet attributed, need runtime evidence:
   - **OMP help at shell start, intermittent.** Shell spawn is identical in standalone and hosted (`jam_TerminalTTY_posix.cpp:46-62`, `:155-169`); only the inherited process environment differs. Capture the hosted zsh environment.
   - **Foreground looks 0.5 opacity.** No code path dims glyphs: scene composite `1.0f` (`jam_VulkanGraphics.cpp:1419`); no `postProcessing` row in `~/.config/end/display.md`; `backgroundOpacity` 0.5 only scales END's background shader (`background_combine.frag:43`). Needs a pixel measurement or screenshot with a known colour.
5. Gate C, CLAP, daemon and docs remain in `PLAN-eve-shared-engine.md` Steps 8-14 (the CLAP target premise is wrong: whelmed has no CLAP target, `grep -i clap` over the whelmed tree is empty).
6. Scratch backups left by Engineers are outside the project tree (previous handoff).
7. DEBT: pay DEBT-20261025T135707 (CLAP clock) and DEBT-20261025T214955 / DEBT-20261009T112447 at `/log`; DEBT-20260912T150000 deferred by ARCHITECT.

### Key Decisions
- *"consolidate all host-aware detection into our own jam::PluginHostType"*
- *"patch and derive? so downstream deterministically no scattered handroll method, but through a single SSOT"*
- *"then we could set rules for END-EVE contract, including so far vulkan, focus management, keyboard management (command manager) etc"*
- *"just follow juce pattern, VERBATIM. this is not our handroll. this is juce::PluginHostType, juce native contract for host detection. we only "register" END as one of the known host"*
- *""magic strings" is an exception here because conforming to framework extension is topmost priority. at callsite, the check still could use static declared variable from cast manifest, just like other string/id type check"*
- Hook channel: *"One export, id lookup"*.
- Chord collision: *"END's table governs"*.
- *"END as HOST should control the FOCUS. END already always control new pane created to be always the FOCUSED one. this already tracked as END state machine. hence eve's contract as in-house default terminal plugin is when hosted at END, it should never manage its own focus, never steal focus."*
- *"these rules should not be scattered, BUT governs by ONE RULE. one method? one check? ONE SSOT? so later if we need to enforce new rule, we only need to register, not manually branching."*
- *"so END pane focus management actually only need to forward, focused PANE give the keyboard focus to its loaded plugin"*
- *"ONE flow, ONE path, ONE action, ONE SSOT, new pane = load eve."*
- *"VST ONLY FIRST. no VST3, no AU. FOCUS. and no harcode, no magic strings, DESIGN BY CONTRACT."*
- *"loading EVE.vst == new pane. is SPEC. but the string name, the path, etc table driven, never inline on the code. follow established pattern VERBATIM"*
- *"NO HACK, NO WORKAROUND, NO PATCHING BULLSHIT without comprehensive understanding. must be DESIGN BY CONTRACT"*
- *"what shared juce means in that context is, compiling juce as dynamic library, and both host and client use that. that's BAD DESIGN"*
- *"is EVE debug build also run post build to copy to END plugins? check its cast manifest, ensure both release and debug build always run post build copy to end repo"*

### Files Modified
- **jam:**
  - `jam_gui/jam_gui.mm`, `jam_gui/menu/jam_Menu.mm`, `jam_gui/windows/native/jam_ModalSheet_mac.mm`
  - `jam_clap/jam_clap.h`; `jam_clap/services/` deleted; `jam_clap/format/jam_ClapPluginInstance.cpp`
  - `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h/.cpp`
  - `jam_graphics/context/jam_CpuLowLevelGraphicsContext.h`, `jam_vulkan/context/jam_VulkanLowLevelGraphicsContext.cpp`
  - `patch/juce-plugin-transparency-hook.patch` (new), `patch/juce-key-pressed-hook.patch` (new), `user-modules-info.md` (two patch rows)
- **EVE:** `project-info.md` (`editorWantsKeyboardFocus TRUE`), `cast/cmake.cast` (post-build ungated)
- **END:**
  - `Source/Main.cpp` (exports)
  - `Source/plugins/Nexus.h`, `Source/plugins/Nexus.cpp` (new), `Source/plugins/Session.h/.cpp`
  - `Source/ui/ENDView.cpp`, `Source/action/ENDActions.cpp`
  - `cast/files.md`, `cast/identifiers.md`
  - `PLAN-end-host-contract.md` (new)

### Open Questions
None. Chord handling is resolved by ARCHITECT: *"CommandManager should have "merge" or "coalesce" design. this definitely could be designed with context-aware focus."* The design is in `PLAN-end-host-contract.md` Step 7.3: one resolver (END's `jam::CommandManager`) over END rows coalesced with the focused plugin's rows, registered on focus through extension `jam.command-coalesce`; END rows win collisions.

### Next Steps
- When ARCHITECT says `/go` on `PLAN-end-host-contract.md`, start Step 1 (the `juce-plugin-host-type-hook.patch`).
- Before Step 7, ARCHITECT builds END Debug and EVE Debug (same configuration on both sides).

---

## Handoff to COUNSELOR: Relative Toolchain Paths in EVE and END

**From:** COUNSELOR
**Date:** 2026-11-09
**Status:** Blocked — waits for the cast working-directory change

### Read First
1. `~/Documents/Poems/dev/cast/RFC-toolchain-working-directory.md` — the rule cast will implement, the data rows after the change ("The data after the change"), and the 5 open questions.
2. Sprint 98 above — the lanes and flags that the rows carry now.
3. `~/Documents/Poems/dev/eve/project-info.md` `## toolchain` (`:119-201`) and `~/Documents/Poems/dev/end/project-info.md` `## toolchain` (`:105-135`) — the rows to rewrite.

### Context
- ARCHITECT's rule: *"the implementation must be written now, so it was written UNIVERSALLY. no hardcoding of any platform"*.
- The EVE rows and three END rows hold macOS absolute paths, for example `eve/project-info.md:126` `-S /Users/jreng/Documents/Poems/dev/eve -B /Users/jreng/Documents/Poems/dev/eve/Builds/Release …` and `end/project-info.md:120` `cast | /Users/jreng/Documents/Poems/dev/eve/spell --vst-debug`.
- They are absolute because each toolchain row runs in the caller's folder (`cast/Source/Toolchain.cpp:21-22`, `cast/SPEC.md:939-941`), so EVE's rows run in END's folder when END calls EVE.
- ARCHITECT selected the fix in cast: *"B. write RFC to cast repo"*. A cast COUNSELOR session works on it (ARCHITECT: *"counselor at cast just started"*).

### Completed
- `cast/RFC-toolchain-working-directory.md` written, with the inventory of all 24 `## toolchain` tables and their relative-path base.
- Sprint 98: every EVE `cmake` row states its architecture and format; END's debug and no-sign rows call `--vst-debug` and `--vst-no-sign` (`end/project-info.md:120`, `:128`).

### Remaining
1. Cast COUNSELOR — implement the working-directory rule that ARCHITECT selects (RFC Open Question 1).
2. COUNSELOR (END/EVE) — rewrite the EVE toolchain table (`eve/project-info.md:119-201`): every `/Users/jreng/Documents/Poems/dev/jam/cast/spell.md` → `../jam/cast/spell.md`; `-S /Users/jreng/Documents/Poems/dev/eve -B /Users/jreng/Documents/Poems/dev/eve/Builds/<X>` → `-S . -B Builds/<X>`; `-C /Users/jreng/Documents/Poems/dev/eve/Builds/<X>` → `-C Builds/<X>`. All other cells stay. This holds under RFC option (a). Under option (b) the paths are one level up (`../../jam/cast/spell.md`, `-S .. -B ../Builds/<X>`, `-C ../Builds/<X>`).
3. COUNSELOR (END/EVE) — rewrite `end/project-info.md:110`, `:120`, `:128` to `../eve/cast/spell.md`, `../eve/cast/spell.md --vst-debug`, `../eve/cast/spell.md --vst-no-sign` (RFC Open Question 5: the direct path, not the `eve/spell` link). Under option (b), END's relative rows (`:112`, `:114`, `:116`, `:118`, `:122`-`:134`) also move one level up.
4. ARCHITECT — fresh `cast cast/spell.md` in EVE, then in END, from any folder; then the Sprint 98 checks (`State for Continuation`).

### Key Decisions
- *"the implementation must be written now, so it was written UNIVERSALLY. no hardcoding of any platform. correct foundation, the actual testing/hardening will eventually get on actual windows machine"*
- *"my DX is identical on any machine, any platform"*
- *"B. write RFC to cast repo"*
- *"ARCHETYPE is for ANY PROJECT. why hardcode eve into its docs?"*
- *"machinist done its job. counselor at cast just started. are you done with yours?"*

### Files Modified
- `~/Documents/Poems/dev/cast/RFC-toolchain-working-directory.md` — new.

### Open Questions
- Which working directory cast uses: the folder of the file that declares the table (a) or the manifest's folder (b). Decider: ARCHITECT (cast RFC Open Question 1). The row text of Remaining 2 and 3 depends on it.
- jreng-filter-strip's `cast/signing.md:48-52` changes under (a). Decider: ARCHITECT (cast RFC Open Question 2). It is not in the END/EVE scope.

### Next Steps
- Start when ARCHITECT says the cast working-directory change is in. Read the cast SPEC §6.9 text that lands, then do Remaining 2 and 3 with @Engineer, and validate each row by a read.

---

## Handoff to COUNSELOR: Style-Driven Theme — END style.css as SSOT

**From:** COUNSELOR
**Date:** 2026-11-09
**Status:** In Progress — code landed, one runtime crash fixed, awaiting ARCHITECT's cast + build

### Read First
1. `PLAN-style-driven-theme.md` (END root) — the locked plan, all rulings and the canon shape.
2. `jam_PluginEditorLayout.cpp:83-116`, `:683-690` and `kuassa_PluginEditorLayout.cpp:59-77`, `:599-617` — the two theme lanes; the KANJUT one is the canon shape (BinaryData default + newline + css-built markdown; `ConfigDocument::addTables` merge).
3. `kuassa_ConfigDocument.cpp:370-392` — the emit contract jam's twin must mirror.
4. `eve/Source/EVEView.cpp:217-219` and `kuassa/jreng-filter-strip/Source/FilterStripView.cpp:107-114` — the StyleManager registers the css tree directly; JFS runs the XML lane, no markdown round trip for style.

### Context
The sprint converts END's gfx theme to css-driven styling per the JFS/KANJUT canon: `gfx.css` is the SSOT for the style lane (palette, appearance ids, metrics, fonts); the StyleManager registers the css tree directly (EVE's shape); `theme.md` stays the authored settings default (window/style/font/scrollbar/tab/overlay/pane/menu), disk wins. Three earlier approaches were rejected by ARCHITECT as hacks (runtime css→markdown round trip, jam emit contract changes, duplicate font intern paths) — the working tree now holds the corrected canon shape; jam's emit is reverted to the KANJUT twin.

### Completed
- **EVE css** — obsidian scaffold removed; all 50 uses re-pointed to EVE's own named palette; zero obsidian anywhere (`eve/Source/layout/style.css`).
- **END gfx.css** — `:root` palette, `.appearance`/`.appearance.DARK` (scrollbar/TextButton/Label/PopupMenu/PaneComponent/Ansi/TextEditor/Markdown/MermaidDiagram ids + the two PopupMenu rows), `.metrics` (`--ScrollBar--thickness`), 84 `@font-face` rules with the four attributes on the first face (`--rasterizer: freetype; --gamma: 2.2; --contrast: 0.0; --embolden: false;`).
- **END theme.md** — recreated as the authored settings default: `version: 0.0.1`, `## window`, `## style`, `## font`, `## scrollbar`, `## tab`, `## overlay` (font rows), `## pane`, `## menu`, original values word-for-word.
- **jam** — `cast/graphics.md` obsidian rows deleted; `addTable` reverted to the KANJUT twin's two-branch shape with the boundary `jassert` (`jam_ConfigDocument.cpp:555-577` vs `kuassa_ConfigDocument.cpp:370-392`); `getTypeName` keeps no assert (ConfigDocument trusts the Validator); `jam::Model::toStrings` (was `toStringArray`) returns `jam::Strings`; `jam_Css` gained the universal numbers-list attribute (`getStyleAttributes`/`getStyleAttributeType` private statics, bare-number tokens only).
- **END wiring** — `ConfigDirectory`/`ConfigTheme` reverted to BinaryData defaults; `files::themeStyleSheet` (`cast/files.md`); the three dead font ids dropped (`cast/identifiers.md`); glob rows ship both `*.md` and `*.css` (`project-info.md:421-423`); Main's direct style reads deleted; `ENLookAndFeel`: `initialiseStyle()` creates the StyleManager over the css tree (`ENDLookAndFeel.cpp:492-502`), `applyColours` = the one `setAppearance` flow (:504-510), `applyFontStyles` = the StyleTheme shape (:512-523), `getTypefaceForFont` resolves through the manager's interned faces (:178-204), `registerTypeface`/`typefaces` map deleted (one font path), settings read via the original `getRowValue`; the default-LAF install moved into `initialiseStyle`'s tail so the engine's ctor font resolution falls to juce's default (the last crash fix, EVE's canon order).
- **display.md** — `font rasterizer`/`font gamma`/`font contrast` rows deleted (the css owns them).

### Remaining
1. ARCHITECT: `cast cast/spell.md` in jam (graphics.md) and end (files.md, identifiers.md, glob rows); build jam and END.
2. ARCHITECT: reseed — delete `~/.config/end/themes/gfx/theme.md` and `~/.config/end/display.md` (old content fails validation; defaults win; same version never overwrites).
3. Runtime checks: laf colours from the GFX sheet; fonts and the four attributes through the manager (no double intern); settings from the disk theme.md; no startup assertions.
4. COUNSELOR: the post-audit doxygen pass (Pathfinder) over the touched headers — `ENDLookAndFeel.h` (ctor/getTypefaceForFont/applyColours/registerEvents/applyFontStyles/initialiseStyle docs stale), `Main.h`, `ConfigDirectory.h`, `ConfigTheme.h`; then @Auditor once, then the sprint log.
5. EVE build + its runtime check (markdown preview renders with the new named palette).

### Key Decisions
- *"JFS with KANJUT is the canon contract shape. although JAM is now ahead, JAM shape must conformed to JFS, thus later sync should not break JFS with KANJUT"*
- *"no such thing as obsidian, no such thing as obsidian colour, everything should be style.css driven as SSOT, exactly like JFS"*
- *"END gains style.css"*; scope *"END + EVE + jam"*
- *"project like END, only need to define what it actually need, NOT EVERYTHING, unless END uses EVERYTING for its theme. gfx/theme.md is the shipped default theme"*
- *"Component always carry its colourId (enum int), StyleManager, StyleCustom build the map, map keyword-int-colour, materialise for usage when setAppearance event driven by init, file watcher from config changes, etc, one SSOT, one flow, one path, unidirectional"*
- *"no handrolling anything, no hand-authored anything"*
- theme.md keys: no `colour id`/`font id` words; no bare hex outside `## colours`; no SCREAMING_SNAKE anywhere; exception: XML-conformant parameter values (`AUTO`, `MEDIUM`)
- *"why round trip? runtime always from theme.md"*; *"there are no guard with ConfigDocument, garbage defensive programming is not allowed. means all check must through Validator. ConfigDocument must trust Validator"*
- *"NO. this is fucking hack. your implementation is garbage, and you change the whole contract. no duplication allowed. you will break JFS-KANJUT if you changed StyleManager. must be design by contract"*
- *"what's the established pattern? you only need to extend, why this need to be asked?"*

### Files Modified
- `eve/Source/layout/style.css` — obsidian removed, named palette.
- `end/Source/config/theme/gfx/gfx.css` — new; the style lane.
- `end/Source/config/theme/gfx/theme.md` — recreated; settings default only.
- `end/Source/config/ConfigDirectory.h/.cpp`, `ConfigTheme.h/.cpp` — reverted to BinaryData flow.
- `end/Source/lookAndFeel/ENDLookAndFeel.h/.cpp` — css-tree StyleManager, one setAppearance flow, manager font path, original settings reads.
- `end/Source/Main.cpp` — registerTypeface call removed; initialiseStyle/applyFontStyles kept.
- `end/cast/files.md`, `cast/identifiers.md`, `project-info.md`, `Source/config/display.md` — data edits above.
- `jam/cast/graphics.md`, `jam_markdown/document/jam_ConfigDocument.cpp`, `jam_web/css/jam_Css.h/.cpp`, `jam_data_structures/model/jam_Model.h` — lexicon rows out, emit reverted to the twin, numbers-list support, `toStrings`.

### Open Questions
None. The design is settled by the canon reads and ARCHITECT's rulings.

### Next Steps
- Trigger: ARCHITECT runs the cast + build (Remaining 1) and reports the result. On a clean run, COUNSELOR continues with the doxygen pass, the audit, and the sprint log.

## Handoff to COUNSELOR: Windows Runtime Defects of END Hosting EVE — Pane Size, Keyboard Routes, Build Warning, OMP

**From:** COUNSELOR
**Date:** 2026-11-13
**Status:** In Progress — pane-size fix, PCH fix and OMP fix landed, not yet built; keyboard design not yet planned

### Read First
1. `PLAN-file-uri-one-flow.md` (END root) — the locked and executed plan of this session's first part (one `file://` URI form, one reader `juce::URL::getLocalFile()`).
2. `Source/ui/EditorView.cpp:41-70`, `:97-125` and `EditorView.h:157`, `:166`, `:226` — the pixel-grid placement that makes END the one author of the hosted editor's size.
3. `jam/jam_plugin_bootstrap/enax/jam_ENAXClientExtensions.cpp:5-39` and `Source/plugins/Nexus.cpp:6-15`, `:54-64` — the ENAX key route, the one route that the keyboard fix keeps.
4. Build JUCE copy `C:\Users\jreng\AppData\Local\cast\juce\b1501cf634ada382e5d91018efaae42c68178593a6a4f839fcb8ea0c1dcb554b\modules`: `juce_gui_basics/native/juce_WindowsHooks_windows.cpp:54-66`, `juce_gui_basics/native/juce_Windowing_windows.cpp:1856-1918`, `:2983-3125`, `:3877-3898`, `juce_audio_processors/format_types/juce_VSTPluginFormat.cpp:412-431`, `:578-608` — the four Windows key routes.

### Context
ARCHITECT reported runtime defects of END hosting EVE (VST2) on Windows: black lines at the pane edges, a markdown-link crash, a Ctrl+W assert, a `TRAPWINCH: command not found: omp_repaint_prompt` line in panes, a `CAST_PLUGINS_HASH` PCH warning, keyboard commands that fail (prefix "`" then "-" or "\" does not split; Ctrl+T opens fzf in zsh, not a new tab), and slow pane creation and close. ARCHITECT states that focus and keyboard have no issue on macOS. The markdown crash was fixed earlier in the session (the URI plan). ARCHITECT reported after a build: *"split only works with dragging from corner. keyboard is still messy. previous pane still showing OMP error. new instance pane and closing is SUPER FUCKING UNBEARABLY SLOW"*.

### Completed
- **URI one flow (earlier in this session, per `PLAN-file-uri-one-flow.md`).** Shell OSC 7 lines write `file:///<path>` with no host. The encoders escape `% ; # ?` and control bytes. `jam_TerminalProcessor.cpp` stores the raw URI, and `getUriPath` is deleted. The readers use `juce::URL::getLocalFile()`: `jam_TextEditor.cpp:1040`, `EVEView.cpp:579-582`, `EVETerminalProcessor.cpp:440-441`. `SessionView.cpp:133` uses `juce::URL::removeEscapeChars (juce::URL {...}.getFileName())`. ARCHITECT confirmed that markdown links open.
- **`cwd` is a URI from session start.** `EVETerminalProcessor.cpp:442-444` writes `juce::URL { startDirectory }.toString (false)` into `Id::cwd`.
- **The rounding patch is withdrawn.** `jam/patch/juce-plugin-window-size-hook.patch` is deleted. Its row in `jam/user-modules-info.md` is removed (the row was the last of the patch table). ARCHITECT: *"no fucking patch and hack"*.
- **Pane size, one author (END).**
  - Cause, cited: END wrote the `width`/`height` rows (`EditorView.cpp:62-68`, `ARCHITECTURE.md:743`), but sized only the editor position (`setTopLeftPosition`, old `EditorView.cpp:97-103`).
  - The host editor took its size from EVE's `audioMasterSizeWindow` reply. The physical size passed through three roundings: `toNearestIntEdges` (`juce_PluginScaleFactorUtilities.h:127`), `getSmallestIntegerContainer` (`juce_VSTPluginFormat.cpp:127-130`, `juce_Rectangle.h:1025`), and floor/ceil with position (`juce_HWNDComponent_windows.cpp:60`).
  - The chain is synchronous (`EditorView.cpp:64-68` → `PluginState.cpp:158-166` → `juce_VSTPluginFormatImpl.h:1953-1960` → `EVEProcessor.cpp:168-187` → `EVEView.cpp:179-182` → `VST2.cpp:1015-1061` → `juce_VSTPluginFormat.cpp:140-151`).
  - Fix: `EditorView::getEditorBounds (int inset)` (`EditorView.cpp:97-117`) snaps the editor rectangle to whole physical pixels. It works in peer coordinates (`peer->getAreaCoveredBy`) with `gridStep = 96 / gcd (roundToInt (scale × 96), 96)`, so every JUCE rounding gets whole numbers.
  - `setEditorBounds (juce::Rectangle<int>)` (`:119-125`) and the rows take the same rectangle (`:59-68`).
  - `juce::NativeScaleFactorNotifier scaleNotifier` (`EditorView.h:226`) calls `resized()` on a scale change or a peer change.
  - EVE code is unchanged, so EVE in other hosts and standalone keeps its behaviour. COUNSELOR read every line.
- **PCH warning.** `cast/cmake.cast:397` sets `SKIP_PRECOMPILE_HEADERS ON` on `Nexus.cpp`, next to its per-source `CAST_PLUGINS_HASH` define. Cause: the define is on `Nexus.cpp` only, while the target PCH is built without it (`CMakeLists.txt:285`, `build.ninja:52-55`, `:188-190`).
- **OMP `TRAPWINCH` error.**
  - Cause: zsh resolved `oh-my-posh` to `C:\Users\jreng\.local\bin\oh-my-posh.exe` 29.9.0, which defines no `omp_repaint_prompt`.
  - The winget copy 32.0.0 (`WindowsApps`) defines it at init lines 744-747. `zshrc:181` was written for it.
  - The stale copy came from `bootstrap-windows.sh` (curl download, skipped when the file exists).
  - Fix: the stale exe is deleted. The bootstrap download block and the unused `OMP_ARCH` values are removed. Step 12 lists `winget install JanDeDobbeleer.OhMyPosh` (`bootstrap-windows.sh:677`). `WINDOWS-SETUP.md:61` no longer lists oh-my-posh among the script installs.
- **Keyboard — evidence collected, no code change.** A whatdbg trace with ARCHITECT's real typing (Debug END, 2026-11-13 12:08) shows one keystroke ("t") reaching `ENDView::keyPressed` → `CommandManager::invokeKey` four times, by four routes:
  1. EVE's `WindowsHooks` hook → `offerKeyMessageToJUCEWindow` → `doKeyDown` → ENAX callback → END.
  2. `vstHookWndProc` `SendMessage` to END's window → END `doKeyChar`.
  3. `vstHookWndProc` `CallWindowProc` → EVE `doKeyChar` → ENAX → END.
  4. EVE `forwardMessageToParent` `PostMessage` → END's queue → END `doKeyChar`.
- **Key route rules, by code.**
  - Routes 2-4 run only when route 1 does not consume the message (`juce_WindowsHooks_windows.cpp:58-63`).
  - Route 2 exists because EVE answers `effKeysRequired` with 1 (`eve/project-info.md:108` → `juce_audio_plugin_client_VST2.cpp:1856` → `juce_VSTPluginFormat.cpp:412`, `:427-431`). The macOS branch installs no hook (`:357-359`).
  - Each `invokeKey` call writes the layer/sequence state (`jam_CommandManager.cpp:57-58`, `:111-119`).
- **Ruled out by reads.** `keys.md` cells reach END as `` ` ``, `\`, `-` (`jam_MarkdownInlineParser.cpp:378-407`, `:1293-1306`; `jam_ConfigDocument.cpp:59-60`). The keyboard layout is US (KBDUS.DLL), so backtick is not a dead key.

### Remaining
1. ARCHITECT: run `cast cast/spell.md` in END and in EVE. Both `CMakeLists.txt` still list the deleted `juce-plugin-window-size-hook.patch` (END `CMakeLists.txt:136`, EVE `CMakeLists.txt:158`), so configure fails until cast runs. Then build END (it builds EVE).
2. ARCHITECT runtime check: pane edges show no black line at a fractional Windows scale; the `CAST_PLUGINS_HASH` warning is gone; new shells show no `TRAPWINCH` error.
3. COUNSELOR, keyboard (Windows only):
   - Pathfinder reads EVE's key-to-PTY path: `EVEView::keyPressed` (`EVEView.cpp:82-87`), the focused `jam::TextEditor` `keyPressed`/`TextInputTarget`, the PTY write, and the return value for a printable key. The dispatch was stopped by ARCHITECT and must run again.
   - Then write a PLAN that keeps the ENAX callback as the one route and closes routes 2-4 at their JUCE seams, with no patch, and no change to EVE in other hosts. Gate the PLAN with ARCHITECT.
   - Do not run more click/key tests. ARCHITECT's observation is the evidence.
4. COUNSELOR, slowness: profile pane creation and close on Windows with scout (profile skill). No Windows manifest exists: every `benchmark/*.md` command is the macOS `END.app` path. Log evidence: in the Debug build under the debugger, END's log starts at 12:27:13 and EVE's at 12:27:56.
5. COUNSELOR: fix the validated audit items of the URI sprint:
   - `SessionView.cpp:133` turns `+` into a space (`removeEscapeChars`, `juce_URL.cpp:963`).
   - `EVEView.cpp:542`/`:546` parse `Id::url` twice, with a File↔string round trip into `openFile`.
6. COUNSELOR: the post-audit doxygen pass (Pathfinder). Stale blocks: `EditorView.h` docs of `resized` and `setEditorBounds` (still describe `inset`), `EVEView.h:289-291`, `EVETerminalProcessor.h:298`, `jam_TextEditor.h:1022-1023`, `eve/SPEC.md:227` (OSC 7 row says "decoded path"). Then @Auditor once, after ARCHITECT's runtime test, then the sprint log on ARCHITECT's `/log`.
7. Pending from earlier: scout launch-time optimisation (item 3 of the session's first request).

### Key Decisions
- *"use juce API to its fullest extent, fix any terminal command that use it crossplatform, SSOT"*
- *"UNIDIRECTIONAL, ONE FLOW"*
- *"ok add one to fix black lines. resizing must be SSOT, should not disagree between host and client. must be crossplatform."*
- *"no fucking patch and hack. i told you no patch, you insist on patch."*
- *"i thought it was explicitly RULED OUT, when END is the host, END always TOLD EVE THE SIZE. there's no disagreement, this is ENAX contract, not the EVERYTHING PLUGIN lane"*
- *"our DESIGN with working with juce ALWAYS SEAMLESS INTEGRATION. we never want to fight juce. patch is the only last inevitably option when there is no other seam, juce reached its design limit"*
- *"be careful with the design. this only END-EVE host client contract? what will you break when EVE loaded on other host? standalone?"*
- *"fix it, why do i always need to repeat"* (the PCH warning)
- *"fix this so no more confusion"* (the oh-my-posh version)
- *"FACTS: - no issue with focus on mac - no issue with keyboard on mac"*
- *"DESIGN BY CONTRACT NON NEGOTIABLE"*
- *"dont waste my tokens for audit. i havent even tested runtime"*

### Files Modified
- `end/Source/ui/EditorView.h`, `EditorView.cpp` — `getEditorBounds`, `setEditorBounds (juce::Rectangle<int>)`, `scaleNotifier`; rows from the snapped rectangle.
- `end/cast/cmake.cast` — `SKIP_PRECOMPILE_HEADERS ON` on `Nexus.cpp`.
- `end/Source/ui/SessionView.cpp` — directory name from `juce::URL` (URI plan).
- `end/PLAN-file-uri-one-flow.md` — new; the URI plan.
- `eve/Source/shell/zsh_integration.zsh`, `bash_integration.bash`, `powershell_integration.ps1`, `eve` — OSC 7 `file:///` form; encoders escape `% ; # ?`.
- `eve/Source/EVEView.cpp` — `getUrlFile` through `juce::URL::getLocalFile()`.
- `eve/Source/EVETerminalProcessor.cpp` — start directory from the `cwd` URI; `cwd` written as a URI at `openTTY`.
- `jam/jam_terminal/processor/jam_TerminalProcessor.h`, `.cpp` — OSC 7 stores the raw URI; `getUriPath` deleted.
- `jam/jam_gui/widgets/jam_TextEditor.cpp` — `cwd` read through `juce::URL::getLocalFile()`.
- `jam/patch/juce-plugin-window-size-hook.patch` — created, then deleted (no net change); its `jam/user-modules-info.md` row is also gone.
- `~/.config/bootstrap-windows.sh` — oh-my-posh curl block and `OMP_ARCH` removed; winget line added; comments updated.
- `~/.config/WINDOWS-SETUP.md` — oh-my-posh removed from the script-installed list.
- `C:\Users\jreng\.local\bin\oh-my-posh.exe` — deleted (stale 29.9.0).

### Open Questions
- The seam for each of key routes 2-4 (EVE's `effKeysRequired` reply and the VST2 host hook, the `CallWindowProc` re-entry, EVE's `forwardMessageToParent`). Decider: ARCHITECT, through the keyboard PLAN.
- Saved sessions from the old build hold per-block `cwd` rows in path form. There is no migration (CAROL forbids legacy compatibility). Decider: ARCHITECT, if the Ctrl+W assert still shows.

### Next Steps
- Trigger: ARCHITECT runs cast and the build (Remaining 1) and reports the runtime result (Remaining 2).
- In parallel, COUNSELOR runs again the EVE key-to-PTY read (Remaining 3) and presents the keyboard PLAN for approval.

---

## Handoff to COUNSELOR: END Loading EVE Under One Second — Profiling Paused for the Document Contract

**From:** COUNSELOR
**Date:** 2026-11-13
**Status:** Blocked — on ARCHITECT's document contract enforcement (macOS session)

### Read First
1. `jam/RFC-config-document-parse-once.md` — the ConfigDocument findings handed to the document contract work. Its scout table is the baseline for the config zones.
2. This entry, `### Remaining`.
3. `~/.carol/skills/profile/SKILL.md` `## Builds` — the cast-only profile build, rewritten this session.
4. `end/benchmark/pane-windows-report.md` and `eve/benchmark/launch-windows-report.md` — the latest medians.

### Context
ARCHITECT asked for EVE launch and END-loading-EVE under one second, prompt ready to type, with no black pane. Sprint 102 cut EVE's layout parse. This session moved END's audio device open to a thread, built the Vulkan device in parallel on Windows, made the VST2 host panes transparent in END, and profiled the rest. ARCHITECT then took the config document path to the macOS session: *"config document optimization delegated. out of your scope."* and *"i will fix Document contract enforcement first, this also profiled and measured so will affect our work here. we will continue to optimize EVE and END after that."*

### Completed
- **Vulkan device in parallel on Windows.** `jam_VulkanGraphicsEngine.cpp:18` sets `externalContextFactory` on every OS. The Windows wait in `getOrCreate` is gone (`.h`). `createContext` marks the GPU unavailable and repaints when no device exists yet (`.cpp:489-493`).
- **No black host pane in END.** `jam/patch/juce-plugin-transparency-hook.patch` hunk on `juce_VSTPluginFormat.cpp`: `ViewComponent` and `Inner` are not opaque under `PluginHostType().isEND()` and paint black only when opaque. A PrintWindow capture showed the theme background (`scratchpad/end-profile-window.png`). ARCHITECT has not checked it at runtime.
- **END audio device on its own thread.** `Nexus` is a `juce::Thread`. `Nexus::run` opens `jam::ComApartment`, then `initialiseWithDefaultDevices (2, 2)` and `addAudioCallback` (`end/Source/plugins/Nexus.cpp:69-81`). `~Nexus` waits for it (`:142`). Device types stay on the message thread: the WASAPI type owns a `DeviceChangeDetector` (`juce_WASAPI_windows.cpp:1886`).
- **`jam::ComApartment`** moved to `jam_core/native/jam_ComApartment_windows.h/.cpp`, rows in `jam/user-modules-info.md`.
- **Window added to the desktop after its setup.** `jam_gui/windows/jam_Window.cpp`: the `DocumentWindow` base gets `addToDesktop = false`, and the constructor calls `addToDesktop()` at its end.
- **Profile builds go only through cast.** New `end/profile-info.md` (lane `profile`: `pluginBundle.cmake` with `CAST_LANE=profile`, jam spell, `Builds/Profile`). New `eve/profile-info.md` (lanes `vst-profile`, `standalone-profile`). `@profile-info` index rows in `end/cast/spell.md` and `eve/cast/spell.md`. Each scout manifest has one toolchain row: `cast | ../cast/spell.md --profile` (`end/benchmark/pane-windows.md`, `pane-windows-build.md`) or `--standalone-profile` (`eve/benchmark/launch-windows.md`). Cast reads every `## toolchain` table (`cast/Source/Processor.cpp:66`). Both runs exited 0; END's run packed `EVE v0.0.1 Windows scout.zip` through EVE's `--vst-profile` lane. `Source/generated/*` now comes from cast in both projects.
- **Measurements (scout, Release, 11 epochs, Windows, medians).**
  - END: epoch 2.77 s; application members 684 ms; config save 3.5 ms; config tables 2.8 ms; theme save 127 ms; theme load 289 ms; config resources 412 ms; nexus device types 49 ms; application window 234 ms; pane prompt (view construction → prompt painted) 240 ms; graph node 128 ms; pane editor 120 ms; nexus audio device 1.62 s (own thread).
  - The prompt shows at about 1.0 s after the members zone starts. The rest of the epoch is the quit: `~Nexus` `waitForThreadToExit (-1)` waits for the 1.6 s device open.
  - EVE standalone: epoch 1.27 s; launch prompt 0.82 s; processor layout 170 ms; processor prepare 175 ms = buffer prepare 125 ms (rows prepare 116 ms, swap prepare 10 ms, processor reset 0.01 ms) + tty open 48 ms; device open ~0; queues prepare 0.13 ms; processor delete 81 ms; standalone shutdown 186 ms.
- **Cause of `rows prepare` (read, not measured per part).** `virtualBlockSize` is 65536 on every OS (`eve/Source/EVETerminalProcessor.cpp:5`). The ring holds `blocksPerVblank × 65536 + rows` transit rows (`:528`), and `prepareRows` reserves every slot's arrays (`jam_TerminalBuffer.cpp:46-48`, `jam_AttributedString.cpp:157-164`). 65536 is the TTY read chunk on every OS: the processor asks `65536 - pendingBytes` (`jam_TerminalProcessor.cpp:72-73`); Windows `ReadFile` asks `readBuffer.getSize()` = 65536 (`jam_TerminalTTY_windows.cpp:850`); POSIX `::read` takes `maxBytes` (`jam_TerminalTTY_posix.cpp:320`). Rationale: `eve/SPEC.md:125-135` and commit `d1e2679`.

### Remaining
1. ARCHITECT: document contract enforcement (macOS). It covers `jam/RFC-config-document-parse-once.md`.
2. COUNSELOR, after the pull: re-measure both. Commands: `scout D:/Poems/dev/end/benchmark/pane-windows.md` from `end/benchmark`, and `scout D:/Poems/dev/eve/benchmark/launch-windows.md` from `eve/benchmark`. Compare `theme save`, `theme load`, `config resources`, `application members`, `processor layout`.
3. ARCHITECT: the decision on EVE's 116 ms `rows prepare` (Open Questions 1).
4. COUNSELOR: the remaining END zones — `application window` 234 ms, `graph node` 128 ms, `pane editor` 120 ms. EVE's zones are not visible in END's run: `Profiler::writeSamples` replaces the samples file (`jam_Profiler.cpp:174`), and only the standalone app writes EVE's.
5. COUNSELOR: the zsh marks in END. zsh in END emits no OSC 7 and no 133;A that reaches EVE (`EVE VST.ode`). With END's environment, zsh registers `_eve_precmd`, which prints them. Next read: a whatdbg trace of a Debug END-hosted EVE at the OSC dispatch (`jam_TerminalProcessor.cpp:430-436`).
6. Engineer, before the sprint log: remove every temporary profiler zone and its id.
   - END: `applicationBackground`, `applicationInitialise`, `applicationMembers`, `applicationShutdown`, `applicationWindow`, `graphicsEngine`, `nexusAudioDevice`, `nexusDeviceTypes`, `nexusFormats`, `nexusRegisterEVE`, `configResources`, `configSave`, `configTables`, `themeLoad`, `themeSave`, and the `panePrompt.emplace` in `ENDView::registerProfilerActions`.
   - EVE: `deviceOpen`, `deviceStart`, `queuesPrepare`, `bufferPrepare`, `rowsPrepare`, `swapPrepare`, `processorReset`.
   - jam: `ttyClose`, `ttyClosePseudoConsole`, `ttyExitPseudoConsole`, `ttyTerminate`.
   - The earlier launch zones of Sprint 102.
7. Pathfinder, post-audit doxygen pass. Stale blocks:
   - `jam_VulkanGraphicsEngine.h:47-50`, `:125-129`.
   - `end/Source/plugins/Nexus.h` (class, constructor, destructor).
   - `jam_TerminalTTY.h:76-77` and `eve/Source/EVETerminalProcessor.h:304`: both say `bufferSize` bounds one read; POSIX reads `maxBytes` (`jam_TerminalTTY_posix.cpp:320`).
   - `jam_TerminalSwap.h:40` vs `:45`.
   - `jam_TerminalBuffer.h:186-187` (`write` ready "when the handle goes out of scope"; `jam_TerminalBuffer.cpp:33` drops the handle at once).
8. ARCHITECT: runtime check of the transparent pane in END (Completed 2).
9. @Auditor once, then the sprint log on ARCHITECT's `/log`. Earlier threads still open: the intermittent quit hang with real zsh; the Ctrl+W markdown preview crash with EVE in END.

### Key Decisions
- *"let's fix eve loading time, then end loading eve time. must be as FAST as we can. profile with scout"*
- *"HOW DO WE MAKE THIS LOAD ENTIRELY LESS THAN 1s? Why we still have black panel edges?"*; *"THE REAL USAGE IS END HOST EVE"*; *"there are no black pane should ever exists"*
- *"ensure the transparency patch is small surface and END only"*
- END opens its sound card *"On a background thread"* (AskUserQuestion answer).
- *"DOCUMENT CONTRACT ALWAYS NON-NEGOTIABLE: built complete AST once, memoized, cached, state update, 0 rules"*
- *"config document optimization delegated. out of your scope."*
- *"you build project with cast, jam always automatically built"*; *"NEVER FUCKING USE MANUAL NINJA. ALWAYS WITH CAST"*
- *"NO. no touching project-info.md"*; *"you only need to write profile-info.md whatever as the manifest to build the profiling build with cast"*
- *"i will fix Document contract enforcement first, this also profiled and measured so will affect our work here. we will continue to optimize EVE and END after that."*

### Files Modified
- `jam/jam_vulkan/engine/jam_VulkanGraphicsEngine.h`, `.cpp` — parallel device build on Windows.
- `jam/patch/juce-plugin-transparency-hook.patch` — END-only transparent `ViewComponent` and `Inner`.
- `jam/jam_core/native/jam_ComApartment_windows.h`, `.cpp` (new); `jam/jam_core/jam_core.h`, `.cpp`; `jam/user-modules-info.md` — `ComApartment` in jam_core.
- `jam/jam_terminal/image/native/jam_TerminalImageSequence_windows.cpp` — local `ComApartment` deleted.
- `jam/jam_gui/windows/jam_Window.cpp` — `addToDesktop` after setup.
- `jam/jam_terminal/tty/native/jam_TerminalTTY_windows.cpp`, `jam/cast/terminal.md`, `jam/generated/jam_Terminal.*` — temporary TTY close zones.
- `jam/RFC-config-document-parse-once.md` (new).
- `end/Source/plugins/Nexus.h`, `.cpp` — audio device thread; temporary zones.
- `end/Source/Main.h`, `.cpp`, `end/Source/config/ConfigModel.cpp`, `ConfigTheme.cpp`, `end/Source/ui/ENDView.cpp`, `end/cast/identifiers.md`, `end/Source/generated/Identifiers.*` — temporary zones.
- `end/profile-info.md` (new), `end/cast/spell.md` — the `profile` lane.
- `end/benchmark/pane-windows.md`, `pane-windows-build.md` (new), `pane-windows.sh` (new) — the END benchmark, cast toolchain row, and `133;A` shell.
- `eve/profile-info.md` (new), `eve/cast/spell.md` — the `vst-profile` and `standalone-profile` lanes.
- `eve/benchmark/launch-windows.md` — cast toolchain row.
- `eve/Source/EVETerminalProcessor.cpp`, `eve/cast/identifiers.md`, `eve/Source/generated/Identifiers.*` — temporary prepare zones.
- `~/.carol/skills/profile/SKILL.md` — cast-only toolchain example and `## Builds` text.

### Open Questions
1. EVE's 116 ms `rows prepare`. Options: a smaller `virtualBlockSize` (`EVETerminalProcessor.cpp:5`); a cap on the rows one `process` writes into the ring, so the ring holds rows plus a fixed transit count; or keep the sizing and move `prepare` off the path to the prompt. Decider: ARCHITECT.
2. The quit waits up to 1.6 s for the device thread (`Nexus.cpp:142`). Whether that matters is ARCHITECT's call.

### Next Steps
- Trigger: ARCHITECT says the document contract work is pulled to this machine.
- Then COUNSELOR runs Remaining 2, and continues with Remaining 3 and 4.

