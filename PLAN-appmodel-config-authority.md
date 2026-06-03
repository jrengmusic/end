# PLAN: AppModel Config Authority — Private Engine, CONFIG Tree, Event-Driven `referTo` Distribution

**RFC:** RFC-appmodel-config-authority.md (Step 8 of PLAN-architectural-hygiene-valuetree-attachment.md)
**Date:** 2026-06-02
**Last verified:** 2026-06-04
**BLESSED Compliance:** verified

**Ground truth (2026-06-04):**
- Step 1 (schema ids): DONE — `AppIdentifier.h` has all CONFIG/type-node ids + `colourType`
- Step 2 (Engine pure parser): PARTIAL — `Nexus`/`Display`/`Whelmed` structs retired; `Keys`/`Popup`/`Action` still in Engine
- Step 3 (AppModel config authority): DONE — Engine by-value member, single watcher, `fileChanged`, `getConfig()`, pass-throughs, `reload()`, `configGeneration` bump. CONFIG built from XML via two-pass `build` + `createAndAddParameter`. Colours stored as `Parameter<int>` (ARGB packed). Dead convenience API stripped. `addStr`/`addColour` lambdas deleted. `resolveAppLayoutDefault` deleted.
- Step 4 (delete applyConfig + configGeneration listener): NOT DONE
- Step 5 (repoint 65 getContext() sites): NOT DONE
- Step 6 (CodeView self-listen + referTo bridges): NOT DONE — font hot-reload fix
- Step 7 (whelmed repoint): NOT DONE
**Language Constraints:** C++17 / JUCE — reference implementation, no overrides. **Cross-repo:** touches both end and jam (`jam::CodeView` gains font+caret node properties + self-listen, Step 6). No build commands (ARCHITECT builds; @Auditor validates against CONTRACT). Refactor-Rewrite Discipline: delete first, implement after — one delete-first pass, intermediate breakage expected, compiler is ground truth.

---

## Context

Steps 0–7 of the architectural-hygiene refactor shipped (type separation, `jam::ValueTree` bag, `terminal::Model`/`AppModel`/`PaneView`, Attachment grafting, getter stripping, `registerAtomics`). The remaining fault is **config flow**:

- `lua::Engine::getContext()` is called at **65 sites / 27 files** — config read straight from the parser everywhere (Pathfinder enumeration, this session).
- `MainComponent::applyConfig()` (MainComponent.cpp:217–245) manually re-pushes config into AppModel setters on every reload; fired by a WINDOW `configGeneration`-bump listener (MainComponent.cpp:375–389).
- Config is double-sourced: AppModel mirrors ~10 config values as read-write PARAMs (mixed with runtime state, synced by the 60 Hz `flush()`/`restoreValues()` timer); the other ~190 Engine fields are read direct at each site.

**Outcome:** `AppModel` becomes the single config authority — owns `lua::Engine` privately, holds all config in a dedicated **CONFIG** `juce::ValueTree` (one subtree per lua file), and distributes config **fully event-driven via `juce::Value::referTo`**. Each reactive consumer carries the value on its **own** state node, `referTo`-bound to the CONFIG property by its orchestrator at Attachment time. Lua file changed → watcher → AppModel mutates CONFIG **in place** → bindings/listeners fire hierarchically. No manual push.

---

## Locked Decisions

From RFC (D8.1–D8.9) plus the ARCHITECT directive this session:

- **D8.1** — AppModel owns `lua::Engine` privately; `jam::Context<Engine>` dies. Engine never exposed.
- **D8.2** — Dedicated CONFIG `juce::ValueTree`, six lua-mirrored subtrees, **built once, NEVER structurally rebuilt**. Hot-reload mutates properties **in place** (`setProperty`). **Correctness law:** `removeChild`+recreate allocates a new `SharedObject` → orphans every `referTo` binding silently (juce_ValueTree.cpp:589,837,842; juce_Value.cpp:186–199). In-place only. Non-negotiable.
- **D8.3 (ARCHITECT-refined)** — CONFIG holds one TYPE node per lua file; **all values in a file are flat properties on that file's node** (no sub-table TYPE nodes). Lua-nesting collisions (`font.size` vs `tab.size`) are resolved by compound leaf names (`fontSize`, `tabSize`). Caps are for TYPE nodes only; properties are lowerCamel.
- **D8.4** — Engine writes CONFIG directly; the `Nexus`/`Display`/`Whelmed` structs are retired. `parse*` walks lua tables and `setProperty` straight into CONFIG. Computed methods read CONFIG.
- **D8.5 (ARCHITECT-overridden — supersedes RFC "CodeView gains no API")** — Distribution via `referTo`. The value the consumer needs **lives on the consumer's own state node**; the orchestrator (the parent that already grafts the child via `jam::ValueTree::Attachment`) **also** `referTo`-binds the consumer node's property Value to the CONFIG node's property Value. Shared source ⇒ the consumer's node listener fires on CONFIG change. **`jam::CodeView` gains font + caret node properties and self-listens** (becomes a `juce::ValueTree::Listener` on its own CODE_VIEW node), rebuilding `jam::Font` and driving `CaretComponent` on change. Display `referTo`-binds those props to `CONFIG/DISPLAY`. No push, no `applyFromAppModel` font/caret path.
- **D8.6** — AppModel delegates non-config Engine methods via thin pass-throughs (`registerActions`, `buildKeyMap`, `registerApiTable`, `setDisplayCallbacks`, `setPopupCallbacks`, `getLoadError`, `getShortcutString`, `getActionLuaKey`, `getPrefixString`). Engine stays hidden.
- **D8.7** — AppModel is the `jam::File::Watcher::Listener`; Engine's watcher role retires. Single watcher, single authority.
- **D8.8** — CONFIG read-only by consumer discipline. The **sole** consumer-initiated mutation is `AppModel::overrideShortcut` → `Engine::patchKey` (patches keys.lua **on disk**) → watcher → reload → CONFIG in place. Never writes the CONFIG node directly.
- **D8.9** — Aggregate handling (mechanism-forced hybrid). `referTo` binds only single-`var` properties. Scalars + single colours distribute via `referTo`/own-node. Genuine aggregates (16-colour ANSI palette, 15-KeyPress `SelectionKeys`, handlers map, clickable-extension set) are AppModel **computed accessors** (`buildTheme()`, `getSelectionKeys()`, `isClickableExtension`, `getHandler`, `dpiCorrectedFontSize`), recomputed in place on reload, pulled by listeners on their CONFIG subtree.
- **No config-derived PARAM mirror** (ARCHITECT) — the ~10 overlapping read-write PARAMs are removed from AppModel; their authority moves to CONFIG. Reactive consumers hold the value on their own node bound via `referTo`; transient consumers read `getConfig()` on demand.

### Distribution policy (three classes)

| Consumer class | Mechanism | Examples |
|---|---|---|
| Persistent reactive | value on own state node + self-listen, orchestrator `referTo`-binds to CONFIG | CodeView (font + caret props, self-listens — jam change), Display, whelmed |
| Transient one-shot | read `getConfig()` on demand at paint/ctor | LookAndFeel, ModalWindow, ActionList, overlays, Dialog |
| Aggregate | AppModel computed accessor, consumer listens on CONFIG subtree | theme (palette), selection keymap, handlers, extensions |

---

## NAMES — schema (extend `app::id`, one id per concept)

NAMES.md Rule -1: new names are gated. **Home: extend `app::id`** (AppIdentifier.h: `static const juce::Identifier x { "x" };`) — no `config::id` namespace, no ConfigIdentifier.h. One identifier per concept across the whole app tree; reuse existing `app::id` leaves directly.

**Structure (ARCHITECT-locked):** CONFIG is one TYPE node; under it, one TYPE node per lua file; all of that file's values are **flat properties** on the file node.
```
CONFIG (TYPE)
  NEXUS   (TYPE)   { flat props: program, args, integration, scrollbackLines, scrollStep,
                     paddingTop/Right/Bottom/Left, editor, gpu, daemon, autoReload }
  DISPLAY (TYPE)   { flat props: fontFamily, fontSize, cellWidth, lineHeight, ligatures, embolden,
                     desktopScale, foreground, background, cursorColour, selectionColour, ansi0..ansi15,
                     cursorCodepoint, cursorBlink, cursorBlinkInterval, cursorForce, cursorStyle,
                     windowTitle, windowWidth, windowHeight, windowOpacity, windowBlurRadius,
                     tabFamily, tabSize, tabPosition, scrollbarWidth, ... }
  WHELMED (TYPE)   { flat props: ... }
  KEYS    (TYPE)   { flat props: prefix, prefixTimeout, ...; selection keymap → computed accessor (D8.9) }
  POPUPS  (TYPE)   { flat props: defaultCols, defaultRows, defaultPosition }
  ACTIONS (TYPE)   { custom-action entries }
```
Caps = TYPE node. Properties = lowerCamel, compound where lua nesting collides (`fontSize`/`tabSize`). Existing `app::id` leaves (`fontFamily`, `fontSize`, `paddingTop`/`Right`/`Bottom`/`Left`, `scrollbackLines`, `position`, `cursorCodepoint`, `cursorStyle`, `cursorBlinkInterval`) are **reused** — not duplicated.

**No needless duplication (ARCHITECT):** a value lives in CONFIG and is read there. The only value carried onto a consumer node is where a jam-generic consumer owns the concept in its own vocabulary — **CodeView font** — and that is `referTo`-shared (single source, not a copy). Padding, scrollback, etc. are read from CONFIG by their consumers; no second copy.

**Step-1 gate:** the new TYPE node ids (CONFIG, NEXUS, DISPLAY, WHELMED, KEYS, POPUPS, ACTIONS) + new flat-property leaves (enumerated from the lua files) are gated names — COUNSELOR brings the full new-id list to ARCHITECT for ratification before @Engineer writes AppIdentifier.h.

**jam CodeView ids (gated, jam):** the new CODE_VIEW font + caret property ids (e.g. `fontFamily`, `fontSize`, caret codepoint/style/blinkInterval) added to `jam::CodeView::properties` are jam names — ratified with ARCHITECT, added to the `CodeView` enum + `properties` array.

---

## Sequencing — single delete-first blast, green at the end

Delete-first discipline forbids old structs coexisting with CONFIG: once the Engine structs are deleted, all 65 sites break until repointed. The blast is large but ordered. ARCHITECT builds at the end of the blast.

### Step 1 — Lock schema, extend `app::id`
**Scope:** `Source/AppIdentifier.h`. **Action:** enumerate every value across the lua files; COUNSELOR brings the new-id list (CONFIG + 6 file TYPE nodes + new flat-property leaves) to ARCHITECT for ratification; @Engineer adds the ratified ids to `app::id`, reusing existing leaves. **Validation:** @Auditor — every identifier traces to the ratified list; one id per concept (no duplicate concept); NAMES.md Rule 0–5; no improvised names. **Gate:** ARCHITECT ratifies the id list before @Engineer writes.

### Step 2 — Engine becomes a pure parser writing CONFIG in place (D8.4)
**Scope:** `Source/lua/Engine.h/cpp`, `EngineParse*.cpp`, `EngineConfig.cpp`.
**Action:** delete `Nexus`/`Display`/`Whelmed`/`Keys`/`Popup`/`Action` structs; `parse*` methods take a CONFIG node and `setProperty` in place via `getOrCreateChildWithName` for groups; rewrite `buildTheme`/`getSelectionKeys`/`dpiCorrectedFontSize`/`isClickableExtension`/`getHandler` to read CONFIG; **remove** `jam::Context<Engine>` base and the owned `jam::File::Watcher` + `fileChanged` (Engine.h:39,40,1319,1267; ctor Engine.cpp:32–46); delete the `configGeneration` bump in `reload()` (Engine.cpp:138–151). Engine keeps `lua::State`, `load()`/`parse()`, action/key lifecycle, callbacks.
**Validation:** @Auditor — no struct remains; every `setProperty` is in-place on a permanent node (D8.2 law); no `removeChild` on a CONFIG node; Engine exposes no Context, owns no watcher. **Note:** breaks all 65 sites — expected (delete-first).

### Step 3 — AppModel config authority (D8.1/D8.6/D8.7)
**Scope:** `Source/AppModel.h/cpp`.
**Action:** add private `std::unique_ptr<lua::Engine> engine` + `jam::File::Watcher watcher`; add `fileChanged` override (D8.7: `engine->parse(getConfig())` in place → recompute aggregates); ctor constructs Engine, **builds the CONFIG node structure once** (`getOrCreateChildWithName` for every TYPE), then parses; add `getConfig()` (read-only handle for orchestrators to bind against), the computed aggregate accessors (D8.9), the delegation pass-throughs (D8.6), and `overrideShortcut` (D8.8); **remove** the config-derived PARAMs and their seed in `AppParameters.xml`, drop their `flush()`/`restoreValues()` participation; `save()` excludes the CONFIG child (rebuilt from lua each launch, as it already excludes NEXUS).
**Validation:** @Auditor — CONFIG built once never rebuilt; single watcher; Engine private; aggregates recomputed not cached as drift copies; `save()` excludes CONFIG; no config-derived PARAM remains; BLESSED S-SSOT/S-Stateless/E-Encapsulation.

### Step 4 — Init sequencing split (RFC derived constraint)
**Scope:** `Source/Main.cpp`, `Source/MainComponent.cpp`.
**Action:** CONFIG parse runs at AppModel construction (CONFIG-first → removes Main.cpp config reads at 55/88/284); action/callback registration (`registerActions`/`buildKeyMap`/callbacks) runs **after `tabs` exists**, through AppModel delegates (MainComponent.cpp:211–214 path); **delete** `applyConfig()` (MainComponent.cpp:217–245) and the WINDOW `configGeneration` listener (MainComponent.cpp:375–389). Reload signalling is now the watcher→parse→`referTo` path.
**Validation:** @Auditor — no `configGeneration` reference remains; DisplayCallbacks still close over `tabs` (registration after tabs); no manual config push path survives.

### Step 5 — Repoint 65 `getContext()` sites → AppModel
**Scope:** the 27 files Pathfinder enumerated (MainComponent 9, whelmed/Screen 7, LookAndFeel 7, ActionList 5, whelmed/Component 4, Main 3, ModalWindow 3, …).
**Action:** per the distribution policy — transient one-shot consumers read `AppModel::getConfig()` (+ child lookups) on demand; aggregate consumers call the AppModel computed accessor; reactive consumers are handled in Step 6. Lua runtime-eval / custom-action paths keep reaching the private Engine **through AppModel** only (no config read). Delete `lua::Engine::getContext()` entirely once zero sites remain.
**Validation:** @Auditor — grep-zero `Engine::getContext()`; each repointed site reads the correct resolved value; no consumer holds a mutable CONFIG handle.

### Step 6 — CodeView self-listen + orchestrator `referTo` bridges + computed cell-dim producer (D8.5)
**Scope:** `jam` — `jam_gui/code_editor/jam_code_view.h/cpp` (CodeView change). `end` — `Source/terminal/component/Display.cpp`, whelmed orchestrator.
**Action (jam CodeView):** add font + caret property ids to the `CodeView` enum + `properties` array; make `CodeView` a `juce::ValueTree::Listener` on its own `state` node (registered in ctor, removed in dtor); on font/caret property change, rebuild `jam::Font` (`resolveMetrics`) → internal `setFont` path, and drive `CaretComponent` (shape/char/blink) — replaces the external `setFont`/`setCaret*` push API as the change channel. **Action (orchestrator):** at the Attachment site, Display `referTo`-binds each CODE_VIEW font/caret property to the matching `CONFIG/DISPLAY` property via `getPropertyAsValue` on both trees; Display owns the bound Value lifetime (RAII). Remove the font/caret push from `Display::applyFromAppModel()`. Display **remains** the sole author of DISPLAY-node `cellWidth`/`cellHeight`/`baseline` for the reader thread (computed producer), recomputed via a CONFIG-subtree listener — `referTo` distributes only the inputs (fontSize + multipliers).
**Validation:** @Auditor — no manual font/caret push; CodeView self-listens on its own node only (scoped, D6); bindings in-place-safe (D8.2 law); Display still produces computed cell dims for the reader thread; CodeView gains no `juce::Value`/`getValueObject` (node properties only, bridge uses `getPropertyAsValue`).

### Step 7 — whelmed repoint + scoped CONFIG listening
**Scope:** `Source/whelmed/component/Component.cpp` (root listener at 43; getContext at 43/90/100/106), `whelmed/Screen.cpp`, `Tokenizer.cpp`, `Parser.cpp`, `InputHandler.cpp`.
**Action:** whelmed reads `CONFIG/WHELMED` + the whelmed theme accessor; convert the root listener to a CONFIG-subtree listener. `whelmed::State` is not `jam::Model`-derived — untouched structurally (rename/repoint only).
**Validation:** @Auditor — no root listener; whelmed reads CONFIG/WHELMED; no struct field reads remain.

**End of blast:** ARCHITECT builds. Green = blast complete.

---

## Reused infrastructure (no reinvention)
- `jam::ValueTree::Attachment` + `getPropertyAsValue` referTo bridge — prior art at Session.cpp:287, Tabs.cpp:93/146.
- `jam::File::Watcher::Listener` (jam_file_watcher.h:165–189; Event enum 121–129) — AppModel adopts the role Engine retires.
- `jam::ValueTree` statics (`getOrCreateChildWithName`, `getChildWithName`, `getValueFromChildWithID`) for CONFIG navigation.
- `jam::Context<AppModel>` — already AppModel's base; `getContext()` is the app-root reach.
- `jam::CodeView::ComponentWithID` node (jam_code_view.h:34, props 39–63) — extended with font+caret props; CodeView self-listens (jam change). No `juce::Value` member added (node properties only). Current font path: `jam::Font font` set via `setFont()` (jam_code_view.h:81,138) — becomes node-driven.

---

## Validation Gate

Each step validated by @Auditor before the next against: MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and these locked decisions (D8.1–D8.9 + no-PARAM-mirror). Specific invariants: CONFIG built once never structurally rebuilt (D8.2); no `removeChild` on a CONFIG node; Engine owns no watcher / exposes no Context; single watcher in AppModel; no manual config push (`applyConfig`/`configGeneration` deleted); no config-derived PARAM mirror; aggregates recomputed not cached; `save()` excludes CONFIG.

---

## BLESSED Alignment
- **B** — CONFIG built once, RAII `referTo` bindings owned by orchestrators; hot-reload bounded to in-place writes on a fixed node set.
- **L** — Engine structs retired (no parallel representation); no transcription layer; no per-session copy; no config-derived PARAM mirror; CodeView gains no API.
- **E (Explicit)** — named `getConfig()` + orchestrator-owned `referTo`; non-config via named delegates; single watcher, single write path.
- **S (SSOT)** — one authority (AppModel), one parser (Engine, private), one CONFIG tree, one watcher, one consumer-write path (`overrideShortcut`→disk→reload). CONFIG excluded from serialization.
- **S (Stateless)** — Engine is a pure parser; CONFIG holds state; aggregates recomputed, not cached.
- **E (Encapsulation)** — Engine hidden behind AppModel; consumers read-only; orchestrator owns the bridge (tell-don't-ask).
- **D** — in-place CONFIG + shared-source `referTo` makes hot-reload a single deterministic fan-out; the SharedObject-identity law removes the rebuild-orphan failure mode by construction.

---

## Risks / Open Questions
1. **`config::id` schema ratification (BLOCKING, Step 1 gate).** Proposed above; ARCHITECT approves TYPEs + leaf ids before code. Decide reuse-vs-new where an `app::id` leaf already exists (e.g. `fontFamily`, `fontSize`).
2. **Custom-action lua runtime eval** — Engine's `lua::State` + `execute` Functions stay alive inside AppModel-owned Engine; the one runtime-eval reach goes through an AppModel delegate (not a config read). Per-site verification during Step 5.
3. **Reader-thread cell-dim producer** — must survive (Display writes DISPLAY-node `cellWidth`/`cellHeight`); `referTo` distributes only inputs (Step 6).
4. **Zoom** — `Display::applyZoom` is a stub; cell-dim "(and zoom)" remains unwired. Stated as fact, **out of frame** (not raised in scope).
5. **DEBT-20260602T000000 (doubling bug)** — deferred per ARCHITECT; paid after foundation clean. Not in this sprint.

---

## Verification (ARCHITECT)
- Build green at end of blast (ARCHITECT runs the build).
- Hot-reload: edit a `.lua`, confirm UI updates via the watcher→CONFIG-in-place→`referTo` path (no `applyConfig`).
- Action-List shortcut override writes keys.lua, reloads, CONFIG updates — the only consumer-initiated mutation.
- @Auditor grep-zero: `Engine::getContext()`, `configGeneration`, retired struct names, config-derived PARAMs.
