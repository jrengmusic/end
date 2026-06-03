# PLAN: Architectural Hygiene — ValueTree/Model Separation, View Attachment, Single Config Authority

**RFC:** RFC-terminal-state-foundation.md
**Date:** 2026-06-02
**Last verified:** 2026-06-04
**BLESSED Compliance:** verified

**Ground truth (2026-06-04):**
- Steps 0–7: DONE (shipped in prior sprints)
- Step 8 (AppModel config authority): see `PLAN-appmodel-config-authority.md` — Steps 1-3 DONE, Steps 4-7 remain
- Step 9 (delete ConfigListener workaround): NOT DONE (depends on Step 8 completion)
- Step 10 (ARCHITECTURE.md + sweep): NOT DONE (depends on Step 9)
- Track 3 (`PLAN-parameter-text.md`): DONE — ParameterText shipped, jam::Model templated verbs, AppModel XML-driven CONFIG build, colour int migration, Engine by-value
**Language / Framework Constraints:** C++17 / JUCE — LANGUAGE.md: C++/JUCE is the BLESSED reference, no overrides. No build commands (ARCHITECT only); @Auditor validates against CONTRACT, not compilation.

---

## Overview

Refactor the state foundation so views own typed nodes, grafts are explicit RAII, config has a single read-only authority, and listeners scope to their own subtree. Five faults are fixed: (1) `jam::ValueTree` overloaded as a stateful APVTS analog; (2) `terminal::State` / `AppState` misnamed — they are Models; (3) no formal view-graft primitive — Panes does manual `appendChild`; (4) `terminal::State&` threaded through Display/LinkManager/Processor; (5) config scattered across 66 `Engine::getContext()` sites and double-fired through root listeners. **DEBT-20260602T000000 (doubling bug) is a downstream debt; resolved once the foundation is clean, not before.**

---

## Language / Framework Constraints

C++/JUCE reference — no overrides. Refactor-Rewrite Discipline: **delete first, implement after.** Steps 2–8 are one delete-first pass; intermediate breakage is expected and correct; the compiler is the ground truth. No build commands — ARCHITECT tests build per step. The audio APVTS cast at `jam_view_manager_editor.cpp:278` is the only verified coupling to `juce::AudioProcessorValueTreeState` in jam and **must be preserved as a Step 0 gate** before any rename cascades.

---

## Sequencing — bottom-up, by compile-gate

The plan is structured so that **each step ends in a green build** (verified by ARCHITECT). The sequence is mandatory:

| Blast | Steps | Scope | Compile gate |
|-------|-------|-------|--------------|
| **A** | 0 | `jam::Model` → `jam::AudioModel` (audio APVTS rename) + file rename to `jam_audio_model.{h,cpp}` | jam compiles (audio plugins + jam_view_manager_editor) |
| **B** | 1–4 | Atomic rename blast: `jam::ValueTree` → `jam::Model` (jam side) + `terminal::State` → `terminal::Model` (end side) + `AppState` → `AppModel` (end side) + `ComponentAttachment` → `jam::Model`-aware + all end consumers of `jam::ValueTree` repointed | jam + end compile together |
| **C** | 5 | New `jam::ValueTree` bag: `Component` / `ComponentWithID` / `Attachment`; `PaneComponent` → `PaneView`; views adopt `ComponentWithID` | end compiles |
| **D** | 6 | Session owns SESSION graft via `jam::Model` API; Panes stops doing manual surgery | end compiles |
| **E** | 7 | Strip `terminal::Model&` refs from views; by-value handles via `findLeaf` | end compiles |
| **F** | 8 | `AppModel` owns `lua::Engine` privately; `getConfig()` (read-only) + 66 sites repointed | end compiles |
| **G** | 9 | Delete `ConfigListener` workaround; scoped listening only | end compiles |
| **H** | 10 | ARCHITECTURE.md + sweep + `getTextEditor()` → `getCodeView()` + remove diagnostic logs | end compiles |

Steps within a blast are sequential but share a single build gate at the end of the blast. No old/new coexistence within a blast. Intermediate breakage within a blast is expected and correct per Refactor-Rewrite Discipline.

---

## The Model — locked decisions (the correctness law)

These are the decisions reached with ARCHITECT via the RFC. Each is binding; deviation is a discrepancy to STOP on, not a choice.

### Type separation (fixes faults 1–2; E one-responsibility)

- **D1 — `jam::Value` / `jam::ValueTree` / `jam::Model` / `jam::AudioModel` taxonomy.**
  - `jam::Value` — `juce::Value` integration bag (already exists; carries widget-intrinsic value per derived class — pure virtual `getValueObject()`).
  - `jam::ValueTree` — `juce::ValueTree` integration bag. NOT a state owner. Bag = `Component` (owns node, concrete `getValueTree()`) + `ComponentWithID<Derived>` (CRTP, no data) + `Attachment` (RAII graft of `getValueTree()` into parent, seeds from `getProperties()`).
  - `jam::Model` — APVTS analog **owning** a `juce::ValueTree` by value. Renamed from old `jam::ValueTree`. Replaces the `std::unique_ptr<juce::ValueTree>` member with `juce::ValueTree state;` by value (APVTS pattern, `juce_AudioProcessorValueTreeState.h:417`). `get()` returns `juce::ValueTree` by value.
  - `jam::AudioModel` — old `jam::Model` (the audio APVTS). Renamed. Coherent taxonomy: `jam::Model` / `jam::AudioModel` / `jam::CodeModel` / `terminal::Model` / `AppModel`.

- **D2 — `jam::ValueTree::Component` owns its node concretely; `getValueTree()` is concrete, NOT pure virtual.** Justified asymmetry with `jam::Value`: a `juce::Value` is widget-intrinsic and polymorphic (different per derived class — `jam_value.h:181-204`), so the base **must** abstract. A `juce::ValueTree` view node is **uniform** (a `juce::ValueTree` of the view's type, identically) — the base owns the node, exposes `getValueTree()` concretely, and a pure virtual would be pointless busywork the derived class would re-implement identically.

- **D3 — `ComponentWithID<Derived>` carries NO data (mirrors `jam::Value::ComponentWithID`).** CRTP mix-in; ctor sets `Derived::setName(type.toString())` (Name = TYPE) and optionally `setComponentID(uuid)` (UUID, topmost only). `static_assert(is_base_of<juce::Component, Derived>)` enforces the contract.

### IDENTITY STRUCTURE — TYPE always, UUID only at the topmost (S-SSOT)

- **D4 — TYPE couples `Name` ⇄ node type. UUID couples `componentID` ⇄ PANE leaf id ⇄ SESSION root id.** Children carry **no** UUID. A view reaches its session subtree via `jam::PaneManager::findLeaf(uuid) → SESSION → getChildWithName(TYPE)`. `ComponentWithID` takes UUID **optionally**: pane/topmost views pass it (→ `componentID`); child views (`CodeView`) pass TYPE only. `juce::Identifier` is lossless with `juce::String`, so the TYPE coupling and UUID coupling cannot drift.

- **D5 — `ComponentAttachment` migrates to `jam::ValueTree::Attachment`.** New `Attachment` ctor: `(juce::ValueTree& parent, jam::ValueTree::Component& view)` — adopts `view.getValueTree()`, seeds from `view.getProperties()` (the `juce::Component::NamedValueSet` SSOT for view-state schema — verified unused on these views today, per RFC), RAII graft/ungraft. No more `type` parameter, no more `NamedValueSet` param (the seed IS the property set). `onAttachment` callback fires post-graft.

### Scoped listening eliminates the double-fire by construction (D; fixes fault 5)

- **D6 — Each view listens ONLY on its own grafted subtree (content) + the dedicated `CONFIG` subtree (config) — disjoint scopes, never the root.** The `ConfigListener` workaround (Display.cpp:88) is deleted in Step 9. Non-determinism was a symptom of listening above one's responsibility. Producer/consumer race that drives DEBT-20260602T000000 is a separate, downstream issue: Video's flush ordering — addressed in a follow-up sprint, not here.

### Single, READ-ONLY config authority (S-SSOT, S-Stateless, L; fixes fault 5)

- **D7 — `AppModel` is the sole config authority.** Owns `lua::Engine` **privately** (never exposed as a member reference — only resolved values reach consumers). Owns the single `jam::File::Watcher` (Engine's watch role retires). On config file change: watcher → tells Engine (dumb parser) to parse → `AppModel` rebuilds **its own CONFIG subtree** (all config is hot-reloadable) → recomputes resolved accessors. Listeners on `getConfig()` (read-only) propagate.

- **D8 — Config is read-only / immutable to consumers.** Consumers read resolved accessors (`AppModel::getFontFamily()`, etc.) OR register listeners on `getConfig()`. They never receive a mutable CONFIG handle. The only write paths are (1) `AppModel`'s internal hot-reload update, (2) the **Action List keyboard-shortcut override** (the sole consumer-initiated mutation, scoped to shortcuts, via a dedicated `AppModel::overrideShortcut(...)` method). Read-only is enforced by API discipline (`const` accessors + listener registration; the CONFIG node itself is not handed out).

- **D9 — All 66 `Engine::getContext()` sites repoint to `AppModel`.** List of files: `display.*` (22), `nexus.*` (5), `whelmed.*` (5), `keys.*` (2), and others — see Step 8 for full enumeration. `AppModel` is the config SSOT; Engine is the parser, owned privately.

### Bounded Context access, not threaded refs (E vs L/B; fixes fault 4)

- **D10 — Objects reach state via by-value `juce::ValueTree` handles at their init list.** App roots via `AppModel::getContext()`. Session subtrees via `jam::PaneManager::findLeaf(uuid)`. `Context::getContext()` is explicit/named (sanctioned by the 122 existing `AppState::getContext()` refs). Deep ref-threading would push unused refs through intermediate layers (§L) and widen coupling (§B).

- **D11 — `Processor` keeps `terminal::Model&` for reader-thread atomics (`storeValue`/`loadValue`, Processor.h:349).** `LinkManager` and `Display` lose their `terminal::State&` refs — they reach their session's SESSION subtree via `findLeaf(uuid)` at init (D10). Handles are message-thread only.

### Pure views, Model-mediated input (E tell/don't-ask; fixes fault 3)

- **D12 — Each view `getValueTree()` returns its own node.** Selection written into `CODE_VIEW` is Model state in the SSOT tree (Controller→Model→View). `jam::ValueTree::Attachment` is **parent/orchestrator-owned** (one level up the same chain), RAII graft/ungraft. **No View or Model ownership moves**; "Session grafts SESSION via `jam::Model` API" moves a graft **responsibility** (Panes.cpp:162 → Session), not ownership. View-before-Model teardown (Panes.cpp:337-338, 388) preserved.

### Considered and rejected (RFC §"Considered and rejected")

- Change the APVTS State model — rejected; only its name and the `unique_ptr` change.
- Distribute config per object — shadow/shared-mutable (§S-SSOT/§B).
- Expose `AppModel`'s `lua::Engine` — leaks parsing detail; resolved values only.
- Second watcher in `AppModel` — duplicate truth; Engine's watch role retires.
- UUID on child nodes / pure-virtual `getValueTree` / node inside `ComponentWithID` — rejected: children are TYPE-only; the node is uniform so the base owns it concretely; the ID mix-in carries no data (mirrors `jam::Value`).

---

## Reused existing infrastructure (no reinvention)

- **`jam::Context<T>`** (`jam_core/context/jam_context.h:42-43`) — `getContext()` static, standalone/plugin branches (h:85, h:125). Sanctioned by 122 `AppState::getContext()` sites.
- **`jam::PaneManager`** (`jam_gui/layout/jam_pane_manager.h`) — `findLeaf` is the UUID-to-SESSION lookup. `addLeaf` sets UUID on PANE node.
- **`jam::File::Watcher`** — Engine owns one; AppModel will own one (Engine's retires).
- **`jam::Value`** — integration bag, `juce::Value` carrier. Already correctly namespaced. No change.
- **`ComponentAttachment`** → `jam::ValueTree::Attachment`. Ctor signature change only.

---

## New names (Decision Gate — locked in RFC, no further introduction in this sprint)

- `jam::ValueTree::Component` / `ComponentWithID<Derived>` / `Attachment` — bag types.
- `jam::Model` — old `jam::ValueTree` renamed; APVTS-style state owner.
- `jam::AudioModel` — old `jam::Model` renamed; audio APVTS.
- `terminal::Model` (was `terminal::State`), files `Model.{h,cpp}`.
- `AppModel` (was `AppState`).
- `PaneView` (was `PaneComponent`).
- `AppModel::getConfig()` — read-only CONFIG subtree handle.
- `AppModel::overrideShortcut(...)` — sole consumer-initiated config mutation.

---

## Validation Gate

Each step is validated by @Auditor before the next against: MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and these locked decisions (D1–D12). No reintroduction of root-listener double-listening (D6); no threaded `terminal::State&` to views (D11); no `getValueTree()` override (D2); no new `setValue`/`getValue` convenience getters on `terminal::Model` (D10/Strip in Step 7); no UUID on child nodes (D4); no manual `appendChild` for SESSION/DOCUMENT (D12).

---

## Steps

### Step 0: `jam::Model` → `jam::AudioModel` (audio APVTS rename)

**Scope:** `jam_data_structures/model/jam_model.h/cpp`, `jam_data_structures/view/jam_view_manager.h`, `jam_view_manager_editor.cpp`, `jam_data_structures/registry/jam_registry.h`, `jam_data_structures/registry/jam_registry_traits.h`, audio plugin consumers.

**Action:** Rename `jam::Model` → `jam::AudioModel` class. The SFINAE trait `HasBindToModel` (`jam_registry_traits.h:163-167`) becomes `HasBindToAudioModel` — the API is audio-only, no standalone reach. `Manager::bindToModel` (`jam_view_manager.h:31`) becomes `Manager::bindToAudioModel`. The APVTS cast at `jam_view_manager_editor.cpp:278` (`static_cast<juce::AudioProcessorValueTreeState&>(model)`) must continue to compile because the class still derives APVTS. SFINAE forward decl at `jam_registry_traits.h:4` (`class Model;`) → `class AudioModel;`. All call sites of `Model&` in `jam_view_manager_editor.cpp` and `jam_registry.h:272,284,474,480,678,683,733,739` are repointed.

**Validation:** every audio plugin and jam_gui sample project compiles (the only `static_cast<APVTS&>(model)` site — `jam_view_manager_editor.cpp:278` — must continue to be the sole APVTS escape hatch and still work). `@Auditor` confirms no other APVTS coupling emerges. ARCHITECT does the build; @Auditor validates naming + references + the cast preservation.

**Pre-step 1 gate:** Step 1 cannot begin until Step 0 is green.

---

### Step 1: `jam::ValueTree` (old) → `jam::Model` (new) — by-value tree owner

**Scope:** `jam_data_structures/value_tree/jam_value_tree.h/cpp` (file rename to `jam_model.h/cpp`), all includers (8 sites: `jam_view_panel.h:43`, `jam_code_view.h:74`, `jam_code_view.cpp:190`, `jam_view_manager.h:58/66`, `jam_view_manager_content.cpp:34/63`, `jam_view_settings.h:26`).

**Action:** Rename class `jam::ValueTree` → `jam::Model`. Replace the `std::unique_ptr<juce::ValueTree> state` member (`jam_value_tree.h:216`) with `juce::ValueTree state` by value. Update ctor (`jam_value_tree.cpp:5-8`) to construct `state { newTreeID }` directly. Update `get()` (`jam_value_tree.cpp:21-24`) to `return state;` (returning by value, `juce::ValueTree` is ref-counted — cheap). Update header file to `jam_model.h`. All call sites that do `juce::ValueTree& x = model.get()` convert to `auto x = model.get()`. The `CriticalSection valueTreeChanging` and `UniqueNodeMap` members move with the rename. `flush()`, `restoreValues()`, `addParameter`, `getRawParameterValue`, `getValue`, `setValue`, `attach`, all statics — all move verbatim. The `juce::Timer` base stays.

**Validation:** every includer site compiles. `terminal::State` (now `terminal::Model` in Step 3) derives `jam::Model` and calls the inherited methods unchanged. `@Auditor` confirms no `state.get()` is rebound to `auto&` (the by-value return is a deliberate contract change). `ComponentAttachment` and `jam::Value` are unaffected at this step.

---

### Step 2: New `jam::ValueTree` bag — `Component` / `ComponentWithID` / `Attachment`

**Scope:** new header `jam_data_structures/value_tree/jam_value_tree.h` (re-claimed name), new `jam_value_tree_component.h`, new `jam_value_tree_attachment.h`. Migrate `jam_component_attachment.h` calls to the new `jam::ValueTree::Attachment`.

**Action:** Introduce the integration bag namespace `jam::ValueTree` (re-claimed — the old `jam::ValueTree` is now `jam::Model` from Step 1). Three types:
- `jam::ValueTree::Component` — ctor `(juce::Identifier type)`, owns `juce::ValueTree node { type }`, virtual `~Component() = default`, `juce::ValueTree& getValueTree() noexcept { return node; }` (CONCRETE, D2), `std::function<void()> onAttachment` (D5 hook).
- `jam::ValueTree::ComponentWithID<Derived>` (CRTP) — ctor `(juce::Identifier type)` calls `static_cast<Derived*>(this)->setName(type.toString())` (Name = TYPE, D4); ctor `(juce::Identifier type, juce::String uuid)` additionally calls `setComponentID(uuid)`. `static_assert(std::is_base_of_v<juce::Component, Derived>)`.
- `jam::ValueTree::Attachment` — ctor `(juce::ValueTree& parent, jam::ValueTree::Component& view)` adopts `view.getValueTree()` (D5). Seeds from `dynamic_cast<juce::Component&>(view).getProperties()` (D5: `juce::NamedValueSet`, verified unused on these views today). RAII: `~Attachment()` ungrafts if `node.getParent().isValid()`. `setValue`, `getNode` API. `onAttachment` callback fires post-graft.

Delete `jam_component_attachment.h/cpp` (subsumed). Migrate Display's `ComponentAttachment` to `jam::ValueTree::Attachment` in a single pass.

**Validation:** the Display constructor still produces an equivalent `DISPLAY` node with the same four seed properties (cellWidth/cellHeight/baseline/fontSize) on `SESSION`. The new Attachment ctor signature compiles against `terminal::Model` (which is now `jam::Model` and exposes `get()` returning `juce::ValueTree` by value). `@Auditor` confirms the seed-property path reads from `getProperties()` (D5), not from a passed `initializer_list<Property>` (no backwards-compat shim).

---

### Step 3: `terminal::State` → `terminal::Model` (rename only)

**Scope:** `Source/terminal/State.{h,cpp}` → rename to `Model.{h,cpp}`. Update base class to `jam::Model` (renamed in Step 1). All `terminal::State` references updated.

**Action:** Rename the class to `terminal::Model`. Rename files. Update `#include "State.h"` in all consumers. Update `Processor.h:349` (`State& state` → `Model& model`). Update all `getState()`/`getStateRef()` returns. Update `LinkManager.h:236` (`State& state` → `Model& model` for now; stripped in Step 7). Update `Display.h:98` (`State& state` → `Model& model`; stripped in Step 7). Update all ~201 `terminal::State` references.

**Validation:** every file in `Source/terminal/` and consumers compile. The class is functionally identical; only the name changed. `@Auditor` confirms no class semantics changed. No convenience getters are removed in this step (Step 7).

---

### Step 4: `AppState` → `AppModel` (rename only)

**Scope:** `Source/AppState.{h,cpp}` → rename to `AppModel.{h,cpp}`. Update all `AppState::getContext()` callers (122 sites). The `jam::Context<AppState>` mixin → `jam::Context<AppModel>`.

**Action:** Rename the class to `AppModel`. Rename files. Repoint every `AppState::getContext()` site to `AppModel::getContext()`. The base class is `jam::Model` (renamed in Step 1). The `Context<AppModel>` mixin gives the static `AppModel::getContext()`. Update all 122 code-only `AppState::getContext()` references across: `MainComponent.{h,cpp}`, `MainComponentActions.cpp`, `AppModel.{h,cpp}`, `terminal/LinkManager.cpp`, `terminal/Processor.cpp`, `terminal/State.cpp` (now `terminal/Model.cpp`), `terminal/Session.cpp`, `terminal/ProcessorEvents.cpp`, `terminal/component/Display.cpp`, `terminal/component/TabsClose.cpp`, `terminal/component/Tabs.cpp`, `terminal/component/TabsActions.cpp`, `terminal/component/PaneComponent.h` (will become `PaneView.h` in Step 5), `terminal/component/Panes.cpp`, `whelmed/InputHandler.cpp`, `whelmed/Screen.cpp`, `lua/Engine.cpp`, `nexus/Daemon.{h,cpp}`, `nexus/Link.{h,cpp}`, `whelmed/component/Component.cpp`.

**Validation:** all 122 sites compile. No behavior change. `@Auditor` confirms the `Context<AppModel>` mixin still works (gated `JUCE_STANDALONE_APPLICATION` branch).

---

### Step 5: `PaneComponent` → `PaneView`; views adopt `jam::ValueTree::ComponentWithID`

**Scope:** `Source/terminal/component/PaneComponent.h` → `PaneView.h`; `whelmed/component/Component.{h,cpp}` uses `PaneView`. `Display` derives `PaneView(DISPLAY, sessionUuid)`; `CodeView` derives `jam::ValueTree::ComponentWithID<CodeView>(CODE_VIEW)`. Declare seed props on each view's `getProperties()`.

**Action:**
1. Rename `PaneComponent` → `PaneView`. Update virtuals: `getPaneType`, `getValueTree` becomes **inherited from `jam::ValueTree::Component`** (D2 — no longer overridden on derived views; the base returns the view's own node).
2. `Display` derives `PaneView` and the new mix-in: `class Display : public PaneView, public juce::KeyListener, public juce::ValueTree::Listener`. The ctor: `PaneView(terminal::id::DISPLAY, sessionUuid)`. The session UUID becomes the `componentID` (topmost contract, D4).
3. `CodeView` derives `jam::ValueTree::ComponentWithID<CodeView>`. Ctor: `ComponentWithID(jam::CodeView::properties.at(codeViewId))` — TYPE only, no UUID (D4).
4. Move all declared seed properties (DISPLAY: cellWidth/cellHeight/baseline/fontSize; CODE_VIEW: selection/caret/viewport) from ctor `initializer_list<Property>` into each view's `juce::Component::getProperties()` via the `juce::Component::setComponentProperty`-equivalent in the ctor. The new `Attachment` (D5) reads them at graft time.
5. Migrate the existing `Display::ComponentAttachment` to `jam::ValueTree::Attachment` (D5).
6. Update `whelmed::Component` to use `PaneView`. `whelmed::State` is **not** `jam::Model`-derived (State.h:9); it stays as-is (RFC: `whelmed` is rename/repoint only; internals deferred).

**Validation:** the `DISPLAY` node is grafted into `SESSION` by the new `jam::ValueTree::Attachment` and contains the four seed properties. `CODE_VIEW` grafts into `SESSION` and contains selection/caret/viewport. The ctor→getValueTree() path returns each view's own node. `@Auditor` confirms no `getValueTree()` is overridden in the derived views (D2 — inherited concretely from the mix-in). The `whlmed` `Component` derives `PaneView` and its `getValueTree()` is also inherited.

---

### Step 6: Session owns SESSION graft; Panes stops doing manual surgery

**Scope:** `Source/terminal/Session.{h,cpp}` (gain SESSION graft via `jam::Model` API), `Source/terminal/component/Panes.cpp` (delete `appendChild` at L160-162, L207-212, L457-459).

**Action:** Session's ctor grafts the per-session `terminal::Model` SESSION subtree into the PANE leaf via the `jam::Model` API. Concretely: Session receives the PANE leaf (or the UUID) at construction; the SESSION node (already constructed in the `terminal::Model` ctor) is appended to the PANE leaf. Panes calls `session->graftInto(paneLeaf)` (or passes the leaf to the Session factory). On teardown, Session ungrafts before `PaneManager::remove()` (preserves the existing ungraft-before-restructure invariant). The `findLeaf` + `appendChild` calls in Panes.cpp at L160-162 (createTerminal), L207-212 (createWhelmed — DOCUMENT), L457-459 (splitAt) are deleted; Panes now only does `PaneManager::remove()` (D12: no ownership moves, only responsibility moves).

**Validation:** every existing test of createTerminal/closePane/split still produces a properly grafted SESSION. View-before-Model teardown is preserved (Panes.cpp:337-338, 388). The reader thread does not start until after the graft (existing invariant, L164-165 comment). `@Auditor` confirms no `appendChild` remains for SESSION/DOCUMENT in Panes.cpp.

---

### Step 7: Strip `terminal::Model&` from views; by-value handles via `findLeaf`

**Scope:** `Source/terminal/component/Display.h/cpp`, `Source/terminal/LinkManager.h/cpp`, `Source/terminal/Processor.h/cpp`. **Strip ALL convenience getters from `terminal::Model` per D10** — `getMode`, `getActiveScreen`, `getCols`, `getVisibleRows`, `getTitle`, `getCwd`, `getForegroundProcess`, `getOutputBlockTop/Bottom`, `getPromptRow`, `hasOutputBlock`, `consumeSnapshotDirty`, `isSyncOutputActive`, `isPreviewActive`, `getSplitCol`, `getHintPage`, `getHintTotalPages`, `getModalType`, `isModal`, `getActiveScreen` (replaced by `jam::ValueTree::getValueFromChildWithID` at call sites).

**Action:**
1. **Display:** drop `terminal::Model& state;` member (Display.h:98). At ctor, capture `auto sessionNode { jam::PaneManager::findLeaf (AppModel::getContext()->getTabs(), getComponentID()) .getChildWithName (id::SESSION) };` — this is the per-session SESSION subtree handle. Replace every `state.getActiveScreen()` / `state.getValueFromChildWithID(...)` call with a direct `jam::ValueTree::getValueFromChildWithID(sessionNode, ...)` or read on `sessionNode` directly. Display's `ConfigListener` is deleted in Step 9.
2. **LinkManager:** drop `terminal::Model& state;` (LinkManager.h:236). At ctor, capture `sessionNode` via the same pattern. Replace `state.get()` with `sessionNode`. Listener targets (`promptRowNode`, `activeScreenNode`) become `jam::ValueTree::getChildWithID(sessionNode, id)` (D11, scoped).
3. **Processor:** **keeps** `terminal::Model& model;` (D11) for reader-thread atomics (`storeValue`/`loadValue` — Processor.h:349). The reader-thread path does NOT need a by-value handle because atomics are a different transport.
4. **Strip `terminal::Model` convenience getters:** every `getX()`/`isX()`/`hasX()` method listed above is deleted. Every call site migrates to `jam::ValueTree::getValueFromChildWithID(sessionNode, id::X)` or `getProperty(id::X)` direct read. **This is the architectural cleanup ARCHITECT identified** — no API convenience layer over a value the caller can read with two lines.
5. **Caller file migrations (sample list — full audit in @Auditor step):** `Source/terminal/Display.cpp:97,178,175` (read activeScreen, cursor, screenId), `Source/terminal/LinkManager.cpp:20,150-161` (activeScreenNode, AppState writes), `Source/terminal/Input.cpp:67,140-145,170,213,236` (cursor/screen reads), `Source/terminal/Mouse.cpp:52,70,121,161` (CodeView selection reads), `Source/terminal/Processor.cpp:210` (DISPLAY node read), `Source/terminal/Session.cpp:306` (getValueTree — already converted), `Source/terminal/ProcessorEvents.cpp:328,368-369,374-378` (atomic load/store — unchanged because Processor keeps the ref).

**Validation:** every call site compiles. No `terminal::Model::get*` convenience getter remains. No view holds `terminal::Model&`. Processor still has the ref. `@Auditor` confirms the by-value `sessionNode` handle is captured at ctor (init list) and used throughout; the pattern mirrors `jam::PaneManager::findLeaf` already in use.

---

### Step 8: `AppModel` config authority — private Engine, read-only `getConfig()`, 66 sites repointed

**Scope:** `Source/AppModel.{h,cpp}` (gain private `lua::Engine`, CONFIG subtree, watcher). 66 `Engine::getContext()` call sites repointed to `AppModel`.

**Action:**
1. `AppModel` owns `lua::Engine` as a `std::unique_ptr<lua::Engine> engine;` private member. Constructed in `AppModel::AppModel()`; `lua::Engine::getContext()` is no longer wired — Engine is owned-by-AppModel, not global.
2. Engine's `jam::File::Watcher` role retires (Engine still watches internally for its own non-config concerns, if any; ARCHITECT verifies). `AppModel` owns the single config `jam::File::Watcher`. On change: `AppModel::onConfigChanged()` → `engine->parse()` → rebuild CONFIG subtree → recompute resolved accessors.
3. `AppModel::getConfig()` returns `const juce::ValueTree&` (read-only) — the CONFIG subtree.
4. `AppModel::overrideShortcut(const juce::String& key, const juce::String& action)` — the sole consumer-initiated config mutation, Action-List-only.
5. Repoint all 65 code-only `lua::Engine::getContext()` sites to `AppModel::getContext()`. Full file list: `Source/Main.cpp` (3), `Source/MainComponent.cpp` (11), `Source/MainComponentActions.cpp` (1), `Source/AppModel.cpp` (1), `Source/terminal/Session.cpp` (2), `Source/terminal/Input.cpp` (1), `Source/terminal/LinkManager.cpp` (1), `Source/terminal/Mouse.cpp` (1), `Source/terminal/component/Panes.cpp` (1), `Source/terminal/component/ModalWindow.cpp` (3), `Source/terminal/component/Dialog.h` (1), `Source/terminal/component/LookAndFeel.cpp` (7), `Source/terminal/component/LookAndFeelTab.cpp` (2), `Source/terminal/component/LookAndFeelMenu.cpp` (1), `Source/terminal/component/StatusBarOverlay.h` (2), `Source/terminal/component/MessageOverlay.h` (1), `Source/terminal/component/LoaderOverlay.h` (1), `Source/terminal/component/Tabs.cpp` (1), `Source/terminal/action/ActionList.cpp` (5), `Source/terminal/action/ActionListSelection.cpp` (1), `Source/terminal/LinkDetector.h` (1), `Source/whelmed/Screen.cpp` (8), `Source/whelmed/Tokenizer.cpp` (1), `Source/whelmed/Parser.cpp` (1), `Source/whelmed/InputHandler.cpp` (1), `Source/whelmed/component/Component.cpp` (4), `Source/nexus/Nexus.cpp` (1). For each call site: read resolved accessor from `AppModel` (e.g. `getFontFamily()`, `getCursorStyle()`) or use the config-derived state machinery. Lua-scripting consumers (e.g. `api.*` calls in custom actions) need a separate evaluation path — ARCHITECT decision: the cleanest is `AppModel::evaluate(const juce::String& script)` for that one site, OR keep `lua::Engine` reachable for runtime script evaluation (NOT for config reads). Verify per-site during this step.
6. Whelmed `Component.cpp:41` root listener is converted to a CONFIG-subtree listener (preview of Step 9 — Whelmed is also affected).

**Validation:** no `Engine::getContext()` call remains. `AppModel` config is read-only from all consumer paths. `overrideShortcut` is the only public mutation. `@Auditor` confirms each repointed site reads the correct resolved value (D9). Hot-reload works (modify a `.lua` file, `AppModel` rebuilds CONFIG, listeners fire, UI updates).

**Critical:** this is the largest single delete-first pass (66 sites in 30 files). Step 8 is itself broken into 8a (private Engine + watcher), 8b (repoint sites file-by-file). The full pass ends in green.

---

### Step 9: Delete `ConfigListener` workaround; scoped listening only

**Scope:** `Source/terminal/component/Display.h/cpp` (delete `ConfigListener` inner struct, lines 118-128, 30, 42, 75-90). `Source/whelmed/component/Component.cpp` (delete root listener at L41, ungraft at L51).

**Action:**
1. `Display::ConfigListener` is deleted entirely. The inner struct, its `start`/`stop`/`valueTreePropertyChanged` methods, the `appState` member, the parent-filter workaround at `Display.cpp:88` (`if (tree.getParent() == appState)`) — all gone.
2. `Display::applyFromAppState()` becomes the only way Display picks up config changes: it listens on `AppModel::getConfig()` (D7, read-only) via a plain `juce::ValueTree::Listener` registered in ctor, ungrafted in dtor. **D6: scoped listening by construction** — Display never receives a SESSION PARAM change via this listener.
3. `Display`'s own listener (line 34, `terminalState.addListener(this)`) remains — but `terminalState` is now the per-session SESSION subtree (D6, scoped to its own grafted subtree, not the AppState root). The double-fire is structurally impossible.
4. `whelmed::Component` deletes its AppState root listener (L41, L51) and converts to a `getConfig()` listener. `whelmed::State` keeps its local `state` listener as before.
5. **The workaround that produced DEBT-20260602T000000's listener-side symptom is removed.** The producer-side race (Video's `pushLine` + `screenDirty` ordering) is **not** addressed here — that is a separate concern in `Source/terminal/Video.cpp` / `ProcessorEvents.cpp` flush ordering, and remains on the DEBT ledger.

**Validation:** build clean. `Display` no longer references `AppState` (its only contact with config is `AppModel::getConfig()`). The double-fire suppression is no longer a workaround — it is a structural invariant. `@Auditor` confirms no root listener remains on any view.

**DEBT-20260602T000000 — the doubling bug:** this step removes the listener-side double-fire contributor. The actual race (pushLine and screenDirty arriving at the same tick, same row arriving in both rings) is **not** fixed by this plan. The bug will continue to manifest (the cosmetic one-row duplicate that disappears on next keystroke). ARCHITECT instruction acknowledged: "priority now is ARCHITECTURAL HYGIENE first. pay debt later when ARCHITECTURAL CLEAN." The debt remains. After this plan ships, a follow-up sprint fixes the producer-side ordering.

---

### Step 10: ARCHITECTURE.md + sweep + remove diagnostic logs

**Scope:** repo-wide, `Source/Main.{h,cpp}` (delete `Log::Scope logScope`), `Source/jam_code_view.cpp` (delete CALC/RESIZED/PAINT logs), `Source/terminal/component/Display.cpp` (delete DRAIN logs), `Source/terminal/ProcessorEvents.cpp` (delete PUSH logs).

**Action:**
1. **Sweep superseded names:** `LineTarget`, commit/live, grid-sync, two-target `CellFifo` doc, `id::liveRows` (DONE), `PaneComponent` → `PaneView`, `AppState` → `AppModel`, `terminal::State` → `terminal::Model`, `jam::ValueTree` → `jam::Model` (new), `jam::Model` → `jam::AudioModel`, `ComponentAttachment` → `jam::ValueTree::Attachment`. Grep-zero verification.
2. **Rename `getTextEditor()` → `getCodeView()` in `Session.h/cpp` and all consumers.** No deferral. Stale `TextEditor` naming is part of the architectural cleanup. Repoint all call sites in this step.
3. **Fix stale doxygen:** `Processor.*` "TextEditor" references.
4. **Remove diagnostic instrumentation:** all `jam::debug::Log::write(...)` calls added during the doubling-bug investigation. Files: `Main.h` (the `logScope` member), `jam_code_view.cpp` (CALC/RESIZED/PAINT), `Display.cpp` (DRAIN), `ProcessorEvents.cpp` (PUSH). The `jam::debug::Log` library itself stays (it's a framework utility). The diag.log file at project root is removed.
5. **Update ARCHITECTURE.md** to reflect: scoped listening (D6), single config authority (D7-D9), three-type separation (D1-D3), by-value handles (D10-D11), view attachment via `jam::ValueTree::Attachment` (D5/D12). Glossary updates: `jam::Model`, `jam::AudioModel`, `jam::ValueTree`, `jam::ValueTree::Component`, `jam::ValueTree::Attachment`, `AppModel`, `terminal::Model`, `PaneView`, `getConfig()`.

**Validation:** no superseded symbol or comment remains. ARCHITECTURE.md mirrors the implemented architecture. No diagnostic logs in production code. `@Auditor` confirms grep-zero for `LineTarget`, `commit-live`, `PaneComponent`, `jam::ValueTree` (old) where it should not appear.

---

## BLESSED Alignment

- **B (Bedrock/simplicity):** one type per role; one config authority; one watcher; one config write path.
- **L (Lean/YAGNI):** no `LineTarget`; `ComponentWithID` carries no data; `Attachment` reads `getProperties()` (no parallel parameter list); `getValueTree()` is concrete (no virtual tax on derived views).
- **E (Explicit):** `Context::getContext()` is explicit/named; config is read-only at the API; scoped listening; positive checks (no early returns — Step 7 strips getters that hid early-return patterns).
- **S (SSOT):** single tree owner per Model; single config authority; single config watcher; single config write path; TYPE ⇄ Name and UUID ⇄ componentID each coupled to one source.
- **S (Stateless):** `Attachment` is a dumb RAII graft; `ComponentWithID` is a dumb mix-in; `Engine` is a dumb parser; resolved config is state in `AppModel`, derivation bounded to the update path.
- **E (Encapsulation):** `getValueTree()` is concrete (uniform node, not polymorphic); one responsibility per type; `terminal::Model` exposes no convenience getters after Step 7 — handles are explicit.
- **D (Deterministic):** scoped listening makes the double-fire impossible by construction; single config write path means hot-reload is deterministic.

---

## Risks / Open Questions

- **DEFERRED — DEBT-20260602T000000 (doubling bug).** Per ARCHITECT: pay debt later. This plan removes the listener-side contributor (the workaround). The producer-side race (`pushLine` + `screenDirty` same-tick row duplication in `Source/terminal/Video.cpp` / `ProcessorEvents.cpp`) is structurally still present. The cosmetic one-row duplicate will continue to manifest. A follow-up sprint fixes the producer-side ordering — gated on this plan shipping first.

- **Blast radius (recap):** `AppState` 122 refs / 19 files; `terminal::State` ~201 refs; `jam::ValueTree` (old) 8 includers; `jam::Model` (old) 1 cast site + 8 SFINAE/registry refs; `Engine::getContext()` 65 code-only sites / 30 files.

- **Verified couplings to preserve:**
  - APVTS cast at `jam_view_manager_editor.cpp:278` (the sole escape hatch).
  - `Processor` keeps `terminal::Model&` (D11) for reader-thread atomics.
  - View-before-Model teardown at `Panes.cpp:337-338, 388`.
  - `whelmed::State` is NOT `jam::Model`-derived (kept as-is).

---

## BLESSED constraints carried into PLAN

- **Identity:** TYPE (`Name` ⇄ node type) always; UUID (`componentID` ⇄ PANE/SESSION id) at the topmost only — UUID is a non-negotiable contract; children TYPE-only (D4).
- **Config:** read-only to consumers; sole writes = hot-reload + Action-List shortcut override; config-derived results are `AppModel` state, derivation bounded to the update path (D7-D8).
- **Teardown:** View-before-Model teardown preserved (D12, Panes.cpp:337-338, 388); no ownership re-parenting.
- **Processor:** retains `terminal::Model&` for reader-thread atomics (D11); handles are message-thread only.
- **`whelmed`:** rename/repoint only; internals deferred; `whelmed::State` untouched.
- **Refactor-Rewrite Discipline:** delete first; old and new do not coexist; compiler errors are ground truth for what remains to fix.
