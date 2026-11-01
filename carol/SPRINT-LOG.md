# SPRINT-LOG

---

## Sprint 92: ENAX Host–Client Contract Planned — Grand-Scheme Amendment, Two Executable Plans, Plan Hygiene ✅

**Date:** 2026-11-01
**Duration:** one session. The start and end times are not recorded.

### Agents Participated
- COUNSELOR — discussion with ARCHITECT, every ruling recorded verbatim, the Grand-Scheme amendment, both plans, the debt rewrite, the CLAUDE.md refresh, this log.
- Pathfinder (haiku) — reads: plan and log inventory over end, jam and eve; the key and command path; the Grand-Scheme; the JFS and KANJUT attachment pattern; the glyph raster path under a transform; the JUCE hosted-parameter, focus and transparency code; the AU registration facts.
- Engineer (sonnet) — `rm` of 11 plan files.
- Auditor — not run. This sprint wrote documents only; each plan runs @Auditor at its own end.

### Files Modified (17 total)
- `PLAN-enax-plugin-host-type.md` (new) — Plan 1, 10 steps: `jam::PluginHostType` (END registered in JUCE's shape), ENAX ids, one `enax_get_extension` export over `Nexus::extensions`, `jam::ENAXClientExtensions` (mixin, rule registry, one `isEND()` probe), engine and key-hook rules, transparency through JUCE's host query, focus forwarded by END, AU in-process registration, docs. Its gate runs once per format.
- `PLAN-enax-command-id.md` (new) — Plan 2, 10 steps: trace first, EditorMode as a key, owner-scoped command tables (`map::TextEditorCommandId`, `map::PluginEditorCommandId`) with default bindings, one binder per binary, the hosted key rule, `mode` as an EVE host parameter with `jam::Model::ValueAttachment`, END driving the focused plugin by parameter name, END zoom through the plugin's `zoom` parameter with `exclusiveZoom`, copy and paste, docs.
- `DEBT.md:20-25` — DEBT-20261025T135707 rewritten to its CLAP-format remainder and marked deferred. The `VirtualClock` part is done (`:125`, `:36` of this log).
- `CLAUDE.md:3`, `:9-21` — END named an audio plugin host end to end. Last sprint, the four debts, the two open plans and the Layer Order follow the working tree.
- `carol/SPRINT-LOG.md` — this entry.
- `~/Documents/Poems/dev/eve/PLAN-Grand-Scheme.md` (§4.3, after `:986`) — `[AMENDED 2026-11-01 — ENAX, the END host–client contract, ARCHITECT verbatim]`.
- Deleted (done or superseded): `PLAN-eve-hosted-at-end.md`, `PLAN-END-plugin-host.md`, `PLAN-terminal-editor.md`, `PLAN-eve-shared-engine.md`, `PLAN-end-host-contract.md`; eve `PLAN-column-reflow.md`, `PLAN-eve-beat-endless.md`, `PLAN-terminal-clock-rhythm.md`, `PLAN-terminal-clock.md`, `PLAN-seq-profiling.md`; jam `PLAN-cpu-graphics-engine.md`.

### Alignment Check
- [x] BLESSED principles followed — each design point was checked against SSOT, Encapsulation and Explicit, and each scattered site is cited in the plans.
- [x] NAMES.md adhered — every new name was ratified by ARCHITECT or derived from a cited family. The full list is in each plan's rulings.
- [x] MANIFESTO.md principles applied
- [ ] No build and no runtime check: this sprint changed no code.

### Problems Solved
- **Plan sprawl.** 13 plans were spread over three repos. 11 are deleted. The kept plans are `eve/PLAN-Grand-Scheme.md` (founding RFC), `eve/PLAN-eve-terminal-protocol.md` (Step 14 waits for ARCHITECT's runtime test) and the two new END plans.
- **A vague key and command contract.** The old Step 7.3 pushed the plugin's key rows into a coalesced END table. The session replaced it:
  - A command id is a callback id, owned by the owner of the state it changes. This is the `ColourId` isomorph (`jam/cast/style.md:66`).
  - Widgets carry their default bindings. Config overrides by name, and the widget never reads config.
  - One binder and one entry per binary.
  - The TextEditor keeps resolving its own keys by its own mode (`jam_TextEditor.cpp:726-729`).
- **END driving plugin state.** The state goes through JUCE's host parameter contract, which every format carries.
  - Hosted parameter ids are format-specific (`juce_VSTPluginFormatImpl.h:979-981`, `juce_VST3PluginFormatImpl.h:2106-2108`, `juce_AudioUnitPluginFormatImpl.h:758-760`). So END finds a parameter by name. The name comes from one jam function (`jam_ParameterLayout.cpp:26-31`).
  - END works only in normalised values, with JUCE's own API (`juce_AudioProcessorParameter.h:105-159`).
- **Zoom.** The JUCE view transform reflows, because the view bounds are divided by the zoom (`jam_ViewEditor.cpp:96`). Glyphs rasterise at the scaled size (`jam_GlyphRasterizer.cpp:55-61`, `:463-465`). So zoom stays the transform over the `zoom` parameter, and the font-size multiplier is dropped.
- **Contract name.** ARCHITECT ratified ENAX (*"END Native API eXtension"*) and `jam::ENAXClientExtensions`. The latter is the isomorph of JUCE's per-format client extensions (`juce_AudioProcessor.h:1254-1284`).

### State for Continuation
- Next: `/go` on `PLAN-enax-plugin-host-type.md`, then `PLAN-enax-command-id.md`. Both state "Risks / Open Questions: None".
- Before Plan 1 Step 6, read every `PluginEditorLayout::create` caller (LSP). The known caller is `eve/Source/EVEProcessor.h:96-97`.
- Before Plan 1 Step 7, read the AU client editor wrapper (`juce_audio_plugin_client_AU_1.mm`). It was not read this session.
- Before Plan 2 Step 3, read the generated form of a `static-value-lookup-table` section (`jam/cast/spell.md:2711-2721`).
- The two 2026-10-31 handoffs below record the previous sprint's completed work. That work is committed but not logged as a sprint.
- `eve/PLAN-eve-terminal-protocol.md` waits for ARCHITECT's Step 14 runtime test. Its sprint is not logged (`eve/carol/SPRINT-LOG.md:144`).

### Debts Paid
- None

### Debts Deferred
- `DEBT-20261025T135707` — END hosts EVE as CLAP; ENAX's CLAP format is future work (ARCHITECT, 2026-11-01).
- `DEBT-20260912T150000` — Windows conformance of Sprint 86's jam changes (ARCHITECT: *"windows build still miles ahead. defer."*).

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

## Handoff to COUNSELOR: EVE Hosted at END — Two Consecutive PLANs

**From:** COUNSELOR
**Date:** 2026-10-31
**Status:** Ready for Implementation

### Read First
1. `~/Documents/Poems/dev/jam/PLAN-cpu-graphics-engine.md` — sprint N+1, executed first.
2. `PLAN-eve-shared-engine.md` — sprint N+2. It depends on PLAN 1 and continues the outstanding steps of PLAN 3.
3. `PLAN-eve-hosted-at-end.md` — this sprint's locked plan. Steps 1-11 are done except 11.2. It records the rulings and amendments of this sprint.
4. `~/Documents/Poems/dev/eve/PLAN-Grand-Scheme.md:1332-1349` — EVE owns its clock (`jam::VirtualDevice`), with "one wiring in every host".

### Context
- The objective was to host EVE in END and run it as fast as standalone, with END a true audio plugin host.
- Steps 1-11 of `PLAN-eve-hosted-at-end.md` landed. END builds, and EVE Release builds in VST, VST3 and AU. END loads `EVE.vst3` from `~/.config/end/plugins/`, which the 2026-10-31 whatdbg run log confirms.
- The runtime showed three defects:
  - EVE draws on solid black.
  - Keys go to the wrong owner.
  - Splitting a pane crashes.
- Research traced the first two to a design defect. Each binary carries its own JUCE, jam and MoltenVK copy. jam's CPU fast lane is owned by `VulkanEngine`, so component and LookAndFeel code names the engine.
- ARCHITECT ruled the fix and split the remaining work into two consecutive PLANs.

### Completed
- **PLAN-eve-hosted-at-end, Step 1:** VST2 SDK copied to `jam/sdk/VST2/`.
- **Step 2:** EVE `vst` format and toolchain rows (`eve/project-info.md:150-155`, `:176`). `VST` added to `signFormats` and `installFormats` (`eve/cast/signing.md:24-26`).
- **Step 3:** `Nexus::VirtualClock` deleted (`Source/plugins/Nexus.h`).
- **Step 4:**
  - Each plugin is a node of the one Nexus graph, with device input, output and MIDI connected (`Source/plugins/Session.cpp:24-59`).
  - `removePlugin` skips bare panes (`:61-68`).
- **Step 6:** `jucePluginHostVst` define row added (`project-info.md`), plus `juce_set_vst2_sdk_path` (`cast/cmake.cast:77`).
- **Step 7:** `Source/plugins/` and `Source/ui/` created, with all includes rewritten.
- **Step 8 (amended: the project owns the command map):**
  - jam: `CommandId` deleted from jam. `CommandManager`, `TextEditor` and `PluginEditor` read JUCE's command registry and key their commands by name (`jam_CommandManager.cpp` `getCommandForName`).
  - EVE: owns `## CommandId` (`eve/cast/identifiers.md`) and registers it in place (`EVEView.cpp:76-86`).
  - END: owns `## CommandId` (`cast/bimaps.md`). `ENDView` is the command owner. `ENDActions` is the action list only, and modal keys are two-character chords (`Source/config/keys.md`).
- **Step 9:** EVE's Release post-build runs `cast --pack` into `end/Source/plugins/EVE v0.1.0 macOS.zip`. This is the JFS pattern (`eve/cast/cmake.cast` post-build block; `eve/project-info.md` `## pack`).
- **Steps 10-11 (VST2, VST3):**
  - `Nexus::offloadPlugins` writes BinaryData to `~/.config/end/plugins/`.
  - `createPlugin` loads `Nexus::evePluginId` (`"EVE.vst3"`) through whichever format claims the file. `whelmedPluginId` and the CLAP-only lookup are deleted.
- **Build fixes found this sprint:**
  - `jam/cast/gui.md`: four identifier rows that had been placed inside the `ButtonState` table were moved out.
  - `eve/cast/cmake.cast`: the missing `pack-value` slot was added.
  - `end/cast/identifiers.md`: the zoom rows were restored, because END does not link `jam_plugin_bootstrap`.
  - `ENDActions() = default`.
  - The `Id::keys` duplicate was removed from `cast/identifiers.md`.
  - The jam doxygen params were fixed (`jam_TerminalProcessor.h:335`, `jam_TerminalSwap.h:58`).

### Remaining
1. Sprint N+1: `jam/PLAN-cpu-graphics-engine.md`, Steps 1-8 and its gate (COUNSELOR, Engineer, ARCHITECT build).
2. Sprint N+2: `PLAN-eve-shared-engine.md`, Phases A-D:
   - the split crash (whatdbg)
   - jam's Objective-C class names
   - the C-ABI factory symbol
   - host-first keys
   - transparency only in END
   - the old Steps 11.2, gate C, CLAP, daemon and docs
3. Scratchpad backups from this sprint (`jam_TextEditor.cpp.bak`, `spell.md.bak`) were left by an Engineer. They are not in the project tree.
4. DEBT ledger:
   - Pay at N+2: DEBT-20261025T135707 (CLAP clock) and DEBT-20261025T214955 / DEBT-20261009T112447 (CommandManager, done in Step 8). Clear them at `/log`.
   - Deferred by ARCHITECT: DEBT-20260912T150000 (Windows): *"windows build still miles ahead. defer."*

### Key Decisions
- *"the goal of this sprint is to ensure eve loaded and run as fast as standalone."*
- *"END is still true and true audio plugin host. EVE still an audio plugin but drives its own clock"*
- *"END load EVE as default is SPEC contract. not user configurable."*
- *"i dont want to put "END's in-house plugin" into system audio plugins path"*
- *"it carry into END's binary, runtime offload to disk, then load. READ how endless use the pattern for conpty"*
- *"EVE target release build is only post-build copy, just like how JFS copy to release directory, the exact pattern"*
- *"command manager should be project specific id. built by its own table"*; *"Owner (PROJECT) need to define its map, then tell the widget as consumer."*; *"NO COPY"*
- *"END owned ActionList. it defined its own map. EVE wrong design put everything on framework."*
- *"EVE as an actual audio plugins (all format) and standalone must work as is, carry its own vulkan contract"*
- *"shared juce? shared vulkan with scattered dynamic lib? FUCK NO"*
- *"at juce::Component level (including LAF) must be juce native contract … never acknowledge about rendering engine"*
- *"my rulings as principles never contradicts … the code implementations written with WRONG RATIONALE"*
- Names: `jam::CpuGraphicsEngine`, `jam::VulkanGraphicsEngine`, `jam::CpuLowLevelGraphicsContext` (*"CpuLLGC"*).
- Channel: *"C-ABI symbol lookup"*. Keys: *"Host first"*. Transparency: *"Only in END"*.
- *"this sprints outstanding works must be splitted into 2 exectutable PLAN. next sprint is CpuGrahicsEngine (and VulkanGraphicsEngine). its next sprint is continuation of this sprint outstandings that blocked by that, with dedicated PLAN."*

### Files Modified
- **jam:**
  - `jam_clap`: none.
  - `cast/{gui,core,markdown,plugin_bootstrap,spell}.md`
  - `jam_gui/commands/jam_CommandManager.*`, `jam_gui/widgets/jam_TextEditor.*`, `jam_plugin_bootstrap/view/jam_PluginEditor.*`
  - `jam_terminal/processor/jam_TerminalProcessor.h`, `jam_terminal/document/jam_TerminalSwap.h`
  - `sdk/VST2/` (new)
  - `PLAN-cpu-graphics-engine.md` (new)
- **EVE:**
  - `project-info.md`, `cast/{signing,identifiers,spell}.md`, `cast/cmake.cast`
  - `Source/EVEView.{h,cpp}`
- **END:**
  - `project-info.md`, `cast/{identifiers,bimaps,spell}.md`, `cast/cmake.cast`
  - `Source/plugins/{Nexus.h,Session.h,Session.cpp}`, `Source/ui/*` (moved), `Source/action/ENDActions.{h,cpp}`, `Source/Main.h`, `Source/config/keys.md`
  - `PLAN-eve-hosted-at-end.md`, `PLAN-eve-shared-engine.md` (new)

### Open Questions
None.
- The symbol name is ratified: *"jam_host_context_factory"*.
- MoltenVK linked twice is a validation check in `PLAN-eve-shared-engine.md` Step 4.

### Next Steps
- Start sprint N+1 with `/go` on `~/Documents/Poems/dev/jam/PLAN-cpu-graphics-engine.md`.

---

## Sprint 91: Model::Component Binds a Node — END Conforms ✅

**Date:** 2026-10-24
**Duration:** part of jam Sprint 171 (the primary record; plan `eve/PLAN-texteditor-nvim-parity.md`, Step 1)

### Decisions (ARCHITECT, verbatim)
1. *"just add an overload? ValueTree is just ValueTree no?"*; *"what need to change? jam::Model? or all its family so could also attach to APVTS? END should conform"*.

### Files Modified
- `Source/end/ENDView.h/.cpp` — the `ENDModel& model` member, from the `ENDModel` instance, is the single route. `messageOverlay.registerParameters (model)`.
- `Source/Main.cpp` — conforms to the `Model::Component` change.
- `project-info.md` — `jam_terminal` row. `CMakeLists.txt` is regenerated with `jam_terminal`.
- `TabView.cpp`, `ENDLookAndFeel.h/.cpp` (`juce::FontOptions`) and `CLAUDE.md` changed in the working tree before this sprint. This log does not claim them.

### State for Continuation
- `cast cast/spell.md --debug`: 0 errors, 0 warnings.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 90: Release Lane — pkg Replaces dmg, Windows NSIS (x64, arm64, admin), gh Upload ✅

**Date:** 2026-10-05
**Duration:** part of one session (cast sprint `release-lane` is the primary record)
**Plan:** `dev/cast/PLAN-release-lane.md` (locked)

### Decisions (ARCHITECT)
1. *"i don't understand why END is separate lane"*; *"installer is installer. only different on default install path"*.
2. Windows **"Program Files, admin"**; icon *"whatever icon used for the binary per juce project. SSOT"*.

### Files Modified
- `project-info.md` — `## pack layout` and `linkDirectory` deleted; `## pack` `pkg | exe`; `## release notes`; `## cmake` `repository`, `installerResourceDirectory`, `installDirectoryWindows` (`$PROGRAMFILES64\JRENG\END`); `description` cell plain; `## toolchain` `cmake -P gh.cmake`.
- `cast/cmake.cast` — dmg pack/sign deleted; bundle pkg → productbuild → productsign → notarize → staple; NSIS at admin level with `JUCE_ICON_FILE`; `DESCRIPTION ":::description:::"`. `cast/installer.cast` (new), `cast/spell.md`, `cast/signing.md` (`installerIdentity`, `identityWindows`, `signArgumentsWindows`), `cast/installer/resources/` (placeholders). `Source/icons/dmg_background_600x680pt.tiff` deleted. `CLAUDE.md`.

### State for Continuation
- Generation is a fixpoint. The Debug build fails to compile, before this sprint's lane: `ENDLookAndFeel.cpp:92` passes 4 arguments to `jam_ButtonSVG.h:81` `getState` (3 parameters).
- CLAUDE.md says agents never run builds; this sprint ran the Debug toolchain under the approved plan's generation step.

### Debts Paid
- None

### Debts Deferred
- None

