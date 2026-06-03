# RFC — AppModel Config Authority: Private Engine, CONFIG Tree, Event-Driven Distribution via referTo

Date: 2026-06-02
Status: Ready for COUNSELOR handoff
Author: ORACLE (pre-flight)
Predecessor: RFC-terminal-state-foundation.md (Steps 0–7 shipped); this RFC details **Step 8** of PLAN-architectural-hygiene-valuetree-attachment.md.

---

## Problem Statement

Steps 0–7 of the architectural-hygiene plan are shipped and building clean (jam::Model/AudioModel separation, jam::ValueTree bag, terminal::Model, AppModel, PaneView, Attachment-based grafting, getter stripping, registerAtomics). The remaining debt is **config flow**:

- `lua::Engine::getContext()` is called at **59 code sites across 27 files** (audited this session) — config read directly from the parser everywhere.
- `MainComponent::applyConfig()` (MainComponent.cpp:217-245) manually re-pushes ~13 config values into AppModel setters on every reload.
- Config is double-sourced: AppModel mirrors ~13 values as PARAMs (mixed with runtime state); the other ~190 Engine fields are read direct at each site.
- Hot-reload is a manual cascade: Engine owns the watcher → `fileChanged` → `reload()` → bumps `configGeneration` on the WINDOW node → MainComponent's WINDOW listener → `applyConfig()` push.

**Objective:** make `AppModel` the single config authority — own `lua::Engine` privately, hold all config in a dedicated read-only **CONFIG** ValueTree, and distribute config to consumers **fully event-driven via `juce::Value::referTo`**. No manual push. Top-to-bottom: config file changed → file-watcher listener → AppModel updates CONFIG in place → referTo/listeners propagate hierarchically.

---

## Research Summary

### Audit — `lua::Engine::getContext()` call sites
- **59 code sites / 27 files** (+6 comment/doc references). Categories: **CONFIG_READ 54**, **COMPUTED 5** (`dpiCorrectedFontSize` ×3, `getHandler` ×1, `isClickableExtension` ×1, `getSelectionKeys` ×2), **THEME 1** (`buildTheme`), **ACTION_KEY 0**, **RUNTIME_EVAL 0**.
- Heaviest files: MainComponent.cpp (9), LookAndFeel.cpp (7), whelmed/Screen.cpp (7), action/ActionList.cpp (5), whelmed/component/Component.cpp (4), ModalWindow.cpp (3), Main.cpp (3).
- 92% are direct CONFIG_READ. Most are **one-shot paint/ctor reads** (LookAndFeel paint, ModalWindow ctor, ActionList ctor, overlays, Dialog) — not persistent listeners.

### Engine API surface (Engine.h)
- ~200 typed scalar config fields across `Nexus` / `Display` / `Whelmed` structs (+ `Keys` / `Popup` / `Action`).
- Computed methods: `buildTheme()→Theme`, `getSelectionKeys()→const SelectionKeys&`, `dpiCorrectedFontSize()`, `getHandler()`, `isClickableExtension()`, static `parseColour()`, static `getConfigPath()`.
- Action/key lifecycle: `registerActions(Registry&)`, `buildKeyMap(Registry&)`, `registerApiTable()`, `setDisplayCallbacks()`, `setPopupCallbacks()`, `getLoadError()`, `patchKey()`, `isKeyFileRemappable()`, `getActionLuaKey()`, `getShortcutString()`, `getPrefixString()`.
- `Theme` struct (16 ANSI + named colours + cursor) and `SelectionKeys` struct (15 KeyPress) already exist as resolved aggregates.
- Engine inherits `jam::Context<Engine>` + `jam::File::Watcher::Listener` and owns `jam::File::Watcher watcher` (Engine.h:1319). The persistent `jam::lua::State lua` (Engine.h:1316) holds custom-action `execute` Functions that fire via action::Registry at keypress — **Engine must stay alive** (this is not a config read).

### Current hot-reload chain
`Engine::Engine()` adds the watcher (Engine.cpp:32-39) → `fileChanged` (Engine.cpp:342-346) → `reload()` (Engine.cpp:138-151): `load()` re-parses, then bumps `app::id::configGeneration` on the WINDOW node → `MainComponent::valueTreePropertyChanged` (MainComponent.cpp:375-391) → `applyConfig()` + glass + DWM. `reload_config` action does `luaEngine.reload()` + `showReloadMessage()`.

### Existing AppModel PARAM mirror (the overlap)
- AppModel (AppModel.cpp) builds PARAM children from `AppParameters.xml` (binary metadata) + `Parameter<int>` adapters; ctor overlays Lua defaults reading `Engine::getContext()` once (AppModel.cpp:19-35).
- PARAMs **mix** config-derived values (fontFamily, fontSize, cellWidth, lineHeight, cursor*, padding*, scrollbackLines, tabPosition) with genuine runtime state (width, height, zoom, renderer, gpuAvailable, daemonMode, port, active tab, activePane*, modalType, selectionType, atlasDirty), under root/WINDOW/TABS nodes. The config-derived PARAMs are read-write today.
- `flush()`/`restoreValues()` 60Hz timer syncs `Parameter<int>` ↔ tree.

### Per-session config duplication (what reaches a Session today)
- **Font metrics** (family, size, cellWidth, cellHeight, baseline, lineHeight) → `CodeView::font` (jam::Font), **DISPLAY node** props, `Mouse::physCellWidth/Height` — written by `Display::applyFromAppModel()`.
- **Cursor** (codepoint, style, blinkInterval) → `CodeView` caret.
- **Selection keys** (15 KeyPress + openFileNextPage) → `Input::selectionKeys`, rebuilt via `input.buildKeyMap()`.
- **scrollbackLines** → `Processor::cellFifo` capacity (read-through at ctor + resize).
- Read-through (not duplicated): `scrollStep` (Mouse per wheel), modal/selection type (app-level).
- **Conclusion:** no Session needs its own config copy. A single app-level CONFIG node suffices; Sessions bind/listen or receive computed results.

### Cell-dimension derivation (verified)
- `cellWidth = maxAdvance × cellWidthMultiplier`; `cellHeight = (ascent+descent+leading) × fontSize × lineHeightMultiplier`; `baseline = ascent × fontSize × lineHeightMultiplier` — `jam::Font::resolveMetrics`, jam_font.cpp:63-65. Pure function of fontSize + the two multipliers.
- `dpiCorrectedFontSize()` applies DPI only (Windows `/scale`), never zoom (EngineConfig.cpp).
- **Zoom is stored but unapplied:** `app::id::zoom` set by increase/decrease/resetZoom (TabsActions.cpp:13-55) fans out `pane->applyZoom()`, but `Display::applyZoom` is an empty stub (Display.cpp:184); whelmed's too. The "(and zoom)" in cell dimension is intended but not wired.

### `juce::Value::referTo` ↔ `ValueTree` mechanics (DECISIVE — verified against JUCE source)
- `Value::referTo(other)` makes `this` share `other`'s `ValueSource`; any holder's mutation fires `Value::Listener::valueChanged` on all bound Values (juce_Value.cpp:186-199, 52-73). Repeated re-bind is safe (no one-shot restriction).
- `ValueTree::getPropertyAsValue(id, um)` returns a Value backed by private `ValueTreePropertyValueSource` (juce_ValueTree.cpp:819-858). It fires when the **same SharedObject's** property changes via **any handle** (`tree == changedTree` is a SharedObject pointer compare, juce_ValueTree.cpp:842,650).
- **The binding is to a specific `SharedObject*`.** In-place `setProperty` preserves it. `removeChild` + freshly-constructed node allocates a **new** SharedObject (juce_ValueTree.cpp:589) → the source-pointer compare never matches → **every referTo binding is silently orphaned.** The ValueSource holds the tree by value (ref-counted, juce_ValueTree.cpp:837), so even a detached-but-held node still notifies via in-place `setProperty`.
- **Hard law:** an event-driven config model on `referTo` **requires CONFIG nodes to be mutated in place** on hot-reload. Structural rebuild is fatal. (This contradicts the predecessor RFC's "rebuild CONFIG subtree" wording — superseded here.)

### referTo prior art (already in the codebase)
- END: `Session.cpp:287` (`winsize.referTo(teNode.getPropertyAsValue(viewportId))`), `Tabs.cpp:93/146` (`tabName.referTo(sessionTree.getPropertyAsValue(displayName))`), `AppModel::setPwd` (`pwdValue.referTo(...cwd)`).
- jam: `jam_view_manager_content.cpp:98,105`, `jam_value_tree_utils.cpp:195,222,227,266,273,309,318` — `value.referTo(node.getPropertyAsValue(...))` is the established attach pattern.
- `jam::Value::ParameterAttachment` binds via callback+listener (not referTo); `jam::ValueTree::Attachment` does tree graft only (no Value binding). The referTo bridge is hand-written at the orchestrator today.

### CodeView state shape
- `jam::CodeView` inherits `jam::ValueTree::ComponentWithID<CodeView>` — **no `juce::Value`, no `getValueObject()`**. It holds a `jam::Font font` pushed by `Display.setFont()`. Its CODE_VIEW node carries selection/caret/viewport int props (jam_code_view.cpp:176-186). `Display` is sole author of viewport cell dims.

---

## Principles and Rationale

### Locked decisions (this session, with ARCHITECT)

- **D8.1 — AppModel owns `lua::Engine` privately; `Context<Engine>` dies.** Engine is never exposed. All 59 `Engine::getContext()` sites repoint to AppModel (CONFIG reads) or to AppModel delegating methods (non-config). (S-SSOT, S-Stateless, E.)

- **D8.2 — Dedicated CONFIG ValueTree, six lua-mirrored subtrees, built once, NEVER structurally rebuilt.** Children: `NEXUS`, `DISPLAY`, `WHELMED`, `KEYS`, `POPUPS`, `ACTIONS` (end.lua = require root). Structure created once at AppModel construction. Hot-reload mutates **properties in place** (`setProperty`); nodes are permanent. **Justification: the referTo SharedObject-identity law (juce_ValueTree.cpp:589,837,842) — rebuild orphans every binding.** (D, S-SSOT.)

- **D8.3 — CONFIG nesting mirrors the lua table structure; leaf values are properties on the group nodes.** e.g. `CONFIG/DISPLAY/FONT` carries `{size, family, cellWidth, lineHeight}`; `CONFIG/DISPLAY/COLOURS` carries `{foreground, background, ansi0..15, ...}`; `CONFIG/NEXUS/SHELL` carries `{program, args}`. The "each lua its own subtree" principle extends fractally to nested tables. (`juce::Identifier` disallows `.` — nesting avoids compound dotted ids.)

- **D8.4 — Engine writes CONFIG directly; typed structs retired.** Engine's `parse*` methods `setProperty` straight into the CONFIG tree (in place) by walking the lua tables; the `Nexus`/`Display`/`Whelmed` structs are deleted. Computed methods (`buildTheme`, `getSelectionKeys`, `dpiCorrectedFontSize`, `isClickableExtension`, `getHandler`) read the CONFIG tree. No struct→tree transcription layer — the parser emits tree. (S-Stateless: Engine is the parser; CONFIG is the state.)

- **D8.5 — Distribution via `juce::Value::referTo`; the orchestrator owns the bridge.** The parent component (e.g. `terminal::Display`), which already grafts its child's node via `jam::ValueTree::Attachment`, also bridges config: it `referTo`s the consumer node's property Value to the CONFIG node's property Value using `getPropertyAsValue` on both trees (jam::ValueTree helpers). Shared source ⇒ the consumer's **existing** node listener fires on CONFIG change. No push, no CodeView API change, no `getValueObject()` added. (E tell-don't-ask; reuses prior art.)

- **D8.6 — AppModel delegates non-config Engine methods via thin pass-throughs.** AppModel exposes `registerActions`, `buildKeyMap`, `registerApiTable`, `setDisplayCallbacks`, `setPopupCallbacks`, `getLoadError`, `overrideShortcut` (wraps `patchKey`), `getShortcutString`, `getActionLuaKey`, `getPrefixString` — each forwarding to the private Engine. Engine stays fully hidden. (E, encapsulation.)

- **D8.7 — AppModel is the `jam::File::Watcher::Listener`; Engine's watcher role retires.** On `.lua` change: AppModel's watcher → `engine->parse()` (writes CONFIG in place) → recompute resolved aggregate accessors → referTo/listeners propagate. Single watcher, single authority. (S-SSOT.)

- **D8.8 — CONFIG read-only by consumer discipline.** `referTo` shares the source, so it is inherently bidirectional; read-only is held by consumers never writing their bound Value (they only read it). The **sole** consumer-initiated mutation is the Action-List keyboard override: `AppModel::overrideShortcut` → `Engine::patchKey` (patches keys.lua **on disk**) → watcher → reload → CONFIG in-place update. The override never writes the CONFIG node directly; it flows through the file→watcher→parse path. (S-SSOT, D.)

- **D8.9 — Aggregate config handling (ORACLE choice; mechanism-forced).** `referTo` binds only single-`var` properties. The ANSI palette (16), `SelectionKeys` (15 KeyPress), the handlers map, and the clickable-extension set have no single-`var` form. Scalars + single colours distribute via `referTo`; genuine aggregates are AppModel computed accessors (`buildTheme()→Theme`, `getSelectionKeys()→SelectionKeys`, `isClickableExtension`, `getHandler`), recomputed in place on reload, pulled by consumers that listen on their CONFIG subtree. Hybrid, because `referTo` structurally cannot bind an aggregate. (ARCHITECT: no preference — ORACLE selects the mechanism-forced hybrid.)

### Derived constraints (not separate decisions)

- **CONFIG is never serialized.** It is rebuilt from the lua files each launch. `AppModel::save()` (which persists WINDOW+TABS, excludes NEXUS) must also **exclude CONFIG**.
- **Init sequencing splits in two.** CONFIG parse runs at AppModel construction (CONFIG-first, eliminates Main.cpp config reads). Action/callback registration (`registerActions`/`buildKeyMap`/callbacks) runs **after `tabs` exists** — DisplayCallbacks close over `tabs`. The current single `luaEngine.load()` at the end of MainComponent's ctor splits: parse early (in AppModel), action wiring later (through AppModel delegates).
- **Computed cell dims still need a producer.** referTo distributes the *inputs* (fontSize, cellWidthMultiplier, lineHeightMultiplier); the *computed* cellWidth/cellHeight on the DISPLAY node — consumed by the reader thread (Processor/Video grid math) — still requires a producer writing the DISPLAY node. The message→reader bridge does not vanish.

### Considered and rejected
- **Rebuild CONFIG subtree on reload** — rejected; orphans every referTo binding (juce_ValueTree.cpp:842). In-place `setProperty` only.
- **Keep Engine structs + AppModel transcription** — rejected by ARCHITECT in favour of Engine-writes-CONFIG-directly (D8.4); no transcription layer.
- **Flat node per file with compound camelCase ids** — rejected by ARCHITECT in favour of nested subtrees (D8.3).
- **CodeView gains `getValueObject()` / a `juce::Value` member** — unnecessary; the orchestrator bridges via `getPropertyAsValue` on the CODE_VIEW node directly (D8.5).
- **Expose Engine (Context<Engine> survives)** — rejected; leaks the parser (D8.1).
- **Per-session config copy** — rejected; one app-level CONFIG node suffices (per-session audit).

### Out of frame (not raised in scope by ARCHITECT)
- Wiring zoom into the cell-dimension chain (`Display::applyZoom` stub). Stated as a fact, not proposed as work.

---

## Scaffold

> Real code, representative. Identifiers under a new `config::id` namespace are NAMES.md decisions for COUNSELOR to confirm. All node TYPEs and property ids below are illustrative of the nested schema (D8.3).

### CONFIG node shape (D8.2 / D8.3)
```
END
  CONFIG                                  (built once; never removeChild'd)
    NEXUS
      SHELL      { program, args, integration }
      TERMINAL   { scrollbackLines, scrollStep, paddingTop, paddingRight, paddingBottom, paddingLeft }
      HYPERLINKS { editor }               (handlers map + extensions set -> computed accessor, D8.9)
      gpu, daemon, autoReload             (scalars direct on NEXUS)
    DISPLAY
      WINDOW   { title, width, height, colour, opacity, blurRadius, ... }
      COLOURS  { foreground, background, cursor, selection, ansi0..ansi15, ... }
      CURSOR   { codepoint, blink, blinkInterval, force, style }
      FONT     { family, size, ligatures, embolden, lineHeight, cellWidth, desktopScale }
      TAB      { family, size, foreground, position, ... }
      ACTIONLIST { closeOnRun, position, nameFamily, paddingTop, ... }
      STATUSBAR  { position, fontFamily, fontSize, fontStyle }
      scrollbarWidth
    WHELMED    { fontFamily, fontSize, lineHeight, background, ansi/token colours, scrollKeys, ... }
    KEYS       { prefix, prefixTimeout }  (bindings + selection keymap -> computed, D8.9)
    POPUPS     { defaultCols, defaultRows, defaultPosition }  (entries -> child nodes)
    ACTIONS    (custom action entries -> child nodes; execute Functions live in Engine's lua::State)
  WINDOW   (runtime PARAMs: width, height, zoom, renderer, gpuAvailable, daemonMode)
  TABS     (runtime PARAMs: active, activePaneID, activePaneType, modalType, selectionType)
  NEXUS    (sessions/loading — runtime)
```

### AppModel — config authority (D8.1 / D8.6 / D8.7)
```cpp
struct AppModel : public jam::Model,
                  public jam::Context<AppModel>,
                  public jam::File::Watcher::Listener
{
    AppModel();   // constructs Engine, builds CONFIG structure once, parses lua in place

    // CONFIG access — read-only handle for orchestrators to bind against.
    juce::ValueTree getConfig() noexcept { return state.getOrCreateChildWithName (config::id::CONFIG, nullptr); }

    // Resolved aggregates (D8.9) — recomputed on reload, read CONFIG.
    lua::Engine::Theme        buildTheme() const;
    lua::Engine::SelectionKeys getSelectionKeys() const;
    bool   isClickableExtension (const juce::String& ext) const noexcept;
    juce::String getHandler (const juce::String& ext) const noexcept;
    float  dpiCorrectedFontSize() const noexcept;

    // Non-config Engine delegation (D8.6).
    void registerActions (action::Registry& r)  { engine->registerActions (r); }
    void buildKeyMap     (action::Registry& r)  { engine->buildKeyMap (r); }
    void registerApiTable()                      { engine->registerApiTable(); }
    void setDisplayCallbacks (lua::Engine::DisplayCallbacks c) { engine->setDisplayCallbacks (std::move (c)); }
    void setPopupCallbacks   (lua::Engine::PopupCallbacks c)   { engine->setPopupCallbacks (std::move (c)); }
    const juce::String& getLoadError() const     { return engine->getLoadError(); }
    juce::String getShortcutString (const juce::String& k) const { return engine->getShortcutString (k); }
    juce::String getActionLuaKey   (const juce::String& a) const { return engine->getActionLuaKey (a); }

    // Sole consumer-initiated config mutation (D8.8): disk patch -> watcher -> reload -> CONFIG in place.
    void overrideShortcut (const juce::String& key, const juce::String& value) { engine->patchKey (key, value); }

private:
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override; // D8.7: parse in place

    std::unique_ptr<lua::Engine> engine;   // PRIVATE — never exposed (D8.1)
    jam::File::Watcher watcher;            // single watcher (D8.7)
};
```

### Engine — parser writes CONFIG in place (D8.4)
```cpp
// Engine no longer owns a watcher and exposes no Context. It receives the CONFIG
// node and writes parsed lua tables straight into it via setProperty (in place).
void lua::Engine::parseDisplay (juce::ValueTree configRoot)
{
    auto display { configRoot.getOrCreateChildWithName (config::id::DISPLAY, nullptr) };
    auto font    { display.getOrCreateChildWithName (config::id::FONT, nullptr) };

    // In-place setProperty — preserves every referTo binding (D8.2 law).
    font.setProperty (config::id::family,     readLuaString ("display.font.family"), nullptr);
    font.setProperty (config::id::size,       readLuaNumber ("display.font.size"),   nullptr);
    font.setProperty (config::id::cellWidth,  readLuaNumber ("display.font.cellWidth"), nullptr);
    font.setProperty (config::id::lineHeight, readLuaNumber ("display.font.lineHeight"), nullptr);
    // ... COLOURS/CURSOR/WINDOW/... walked identically.
}
```

### Orchestrator bridge — Display binds CONFIG -> CodeView node (D8.5)
```cpp
// Display already grafts the CODE_VIEW node into SESSION via jam::ValueTree::Attachment.
// Alongside the graft, Display bridges config by sharing CONFIG's ValueSource into the
// consumer node's property — CodeView's existing node listener then fires on reload.
void terminal::Display::bindConfig()
{
    auto config   { AppModel::getContext()->getConfig() };
    auto fontCfg  { config.getChildWithName (config::id::DISPLAY).getChildWithName (config::id::FONT) };
    auto codeNode { session.getTextEditor().getValueTree() };   // CODE_VIEW node

    // Shared source: writing CONFIG/DISPLAY/FONT.size now updates the CODE_VIEW node prop,
    // firing CodeView's node listener. jam::ValueTree helpers fetch the Value from any tree.
    fontSizeBridge.referTo (config_fontSizeValue);   // held Value; orchestrator owns lifetime
    // (representative: in practice Display referTo's codeNode.getPropertyAsValue(id)
    //  to fontCfg.getPropertyAsValue(id), per ARCHITECT — get any property as value from any tree.)
}
```

---

## BLESSED Compliance Checklist
- [x] **Bounds** — CONFIG structure created once, RAII referTo bindings owned by the orchestrator; hot-reload bounded to in-place property writes on a fixed node set.
- [x] **Lean** — Engine structs retired (no parallel representation); no transcription layer; no per-session config copy; CodeView gains no API; referTo reuses existing prior art.
- [x] **Explicit** — config reaches consumers via named `getConfig()` + orchestrator-owned referTo; non-config via named AppModel delegates; single watcher, single write path.
- [x] **SSOT** — one config authority (AppModel), one parser (Engine, private), one CONFIG tree, one watcher, one consumer-write path (overrideShortcut→disk→reload). CONFIG excluded from serialization.
- [x] **Stateless** — Engine is a pure parser emitting tree; CONFIG holds the state; computed aggregates recomputed on reload, not cached as drift-prone copies.
- [x] **Encapsulation** — Engine fully hidden behind AppModel; consumers hold read-only bound Values; orchestrator owns the bridge (tell-don't-ask).
- [x] **Deterministic** — in-place CONFIG update + referTo shared-source propagation makes hot-reload a single deterministic fan-out; the SharedObject-identity law removes the rebuild-orphan failure mode by construction.

---

## Open Questions
None. All session decisions locked (D8.1–D8.9). Aggregate handling resolved as the mechanism-forced hybrid (ARCHITECT: no preference → ORACLE choice).

---

## Handoff Notes (for COUNSELOR → PLAN)

1. **Decisive correctness law (do not violate):** CONFIG nodes are built once and mutated **in place** (`setProperty`). Never `removeChild`+recreate a CONFIG node — it orphans every `referTo` binding silently (juce_ValueTree.cpp:589,837,842; juce_Value.cpp:186-199). This **supersedes** the predecessor RFC's "rebuild CONFIG subtree" / PLAN D7 wording.
2. **Blast radius:** 59 `Engine::getContext()` code sites / 27 files to repoint. `Context<Engine>` removal + `jam::Context<Engine>` base removal from Engine. Engine struct deletion touches every `EngineParse*.cpp` and all ~190 field reads.
3. **Engine rewrite (D8.4):** `parse*` methods take a CONFIG node and `setProperty` in place; delete `Nexus`/`Display`/`Whelmed` structs; rewrite `buildTheme`/`getSelectionKeys`/`dpiCorrectedFontSize`/`isClickableExtension`/`getHandler` to read CONFIG. Engine loses its `jam::File::Watcher` and `jam::Context<Engine>`.
4. **AppModel surface (D8.6):** add private `unique_ptr<lua::Engine>`, `jam::File::Watcher`, `fileChanged` override, `getConfig()`, the resolved-aggregate accessors, the delegation pass-throughs, and `overrideShortcut`.
5. **Init sequencing:** AppModel ctor constructs Engine + builds CONFIG structure + parses (CONFIG-first, removes Main.cpp config reads). Action/callback registration runs after `tabs` exists, through AppModel delegates. `configGeneration`-bump reload signalling is replaced by the watcher→parse→referTo path; the MainComponent WINDOW `configGeneration` listener and `applyConfig()` push are deleted.
6. **Serialization:** `AppModel::save()` must exclude the CONFIG child (rebuilt from lua each launch), as it already excludes NEXUS.
7. **Consumer distribution policy (scaffold-time):** persistent reactive components (Display/CodeView/whelmed) bind via orchestrator-owned `referTo` and/or listen on their CONFIG subtree; transient paint/ctor consumers (LookAndFeel, ModalWindow, ActionList, overlays, Dialog) read `getConfig()` on demand. Aggregates (theme/keymap/handlers/extensions) via AppModel computed accessors (D8.9).
8. **Computed cell-dim producer:** preserve a producer that writes the DISPLAY-node cellWidth/cellHeight for the reader thread; referTo distributes only the inputs.
9. **Custom-action lua:** Engine's `lua::State` and parsed `execute` Functions stay alive inside the AppModel-owned Engine; zero RUNTIME_EVAL repoints needed.
10. **whelmed:** rename/repoint only (per predecessor RFC). `whelmed::State` is not `jam::Model`-derived — untouched. whelmed reads CONFIG/WHELMED + the whelmed theme accessor.
11. **NAMES.md:** all `config::id` node TYPEs and property identifiers are new names — Decision Gate; COUNSELOR proposes to ARCHITECT before introduction.
