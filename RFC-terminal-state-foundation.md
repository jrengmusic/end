# RFC — Terminal State Foundation: Value / ValueTree / Model Separation, View Attachment, Single Config Authority

Date: 2026-06-02
Status: Ready for COUNSELOR handoff
Author: ORACLE (pre-flight)

---

## Problem Statement

The terminal-as-editor sprint exposed a structural defect. The initiating symptom was a **double-fire** (one `PARAM` change firing a `ValueTree::Listener` twice). The objective is the foundation, not the patch. Five faults, bottom up:

1. **`jam::ValueTree` is overloaded** — a stateful APVTS-analog base that holds its `juce::ValueTree` behind `std::unique_ptr` (jam_value_tree.h:216), diverging from the APVTS contract it models.
2. **`terminal::State` / `AppState` are misnamed** — `jam::ValueTree`-derived *Models* (State.h:63) called `State`.
3. **No formal full-view graft** — `ComponentAttachment` builds a bare flat node (jam_component_attachment.h:68–78); panes graft by **manual `appendChild`** (Panes.cpp:162, 459).
4. **References threaded everywhere** — `terminal::State&` through `Display` (Display.h:98), `LinkManager` (LinkManager.h:236), `Processor` (Processor.h:349); redundant handles; overlapping listeners.
5. **Config scattered / double-sourced** — `lua::Engine::getContext()` at **66 sites / 30 files**; `Display`/whelmed listen on the **AppState root** for config (Display.cpp:77; whelmed Component.cpp:41). Because SESSION is grafted deep under the root, that root listener receives every descendant SESSION `PARAM` change — **the double-fire source**, suppressed today by a parent-filter workaround (Display.cpp:88).

---

## Research Summary

### APVTS contract (`jam::Model` mirrors)
- APVTS holds its tree **by value**: `ValueTree state;` (juce_AudioProcessorValueTreeState.h:417), replaceable (:413). `juce::ValueTree` is already a ref-counted handle; the `unique_ptr` (jam_value_tree.h:216) is redundant heap indirection.
- APVTS attachments are separate objects the widget does not own: `SliderAttachment (APVTS&, parameterID, Slider&)` (:552).

### `jam::Value` (jam_value.h) — the integration-bag mirror, and WHY it uses a pure virtual
- `Value::Component` (jam_value.h:16): `virtual juce::Value& getValueObject() = 0` + `onAttachment`.
- `Value::ComponentWithID<Derived>` (jam_value.h:62): CRTP mix-in; carries **no data**; only `static_cast<Derived*>(this)->setComponentID(newID)` (:75) with `static_assert(is_base_of<juce::Component,Derived>)` (:72). The `juce::Value` lives in the **Derived** (example MyComponent: `juce::Value value; getValueObject(){return value;}`, :55–58).
- The pure virtual is **forced**: a component's Value is widget-intrinsic and varies by type — `getFrom()` dispatches Slider→`getValueObject`, Button→`getToggleStateValue`, Label→`getTextValue`, TextEditor→`getTextValue`, ComboBox→`getSelectedIdAsValue` (jam_value.h:181–204). The base cannot own a uniform Value, so it must abstract.
- `Value::ParameterAttachment` (jam_value.h:88): separate, RAII, binds a `Component`'s Value to an APVTS param.

### `juce::NamedValueSet`
- `NamedValueSet::NamedValue` public `{ Identifier name; var value; }` (juce_NamedValueSet.h:50–68); init-list ctor (:81); iterable (:92–93). `juce::Component::getProperties()` returns a `NamedValueSet`.

### Old `jam::Model` is the audio APVTS model (Step-0 rename)
- `class Model : public juce::AudioProcessorValueTreeState` (jam_model.h:11). Registry/view-manager cast it to APVTS: `bindToModel(juce::Component*, Model&)` (jam_view_manager.h:31), `static_cast<juce::AudioProcessorValueTreeState&>(model)` (jam_view_manager_editor.cpp:278), SFINAE (jam_registry_traits.h:163), forward decls (registry_traits.h:4, registry.h:126).
- **Sequencing hazard:** `ValueTree→Model` before `Model→AudioModel` rebinds those to the non-APVTS base and breaks the cast. Compiler gatekeeps; order mandatory.

### IDENTITY STRUCTURE — top-down; TYPE everywhere, UUID at the top only (verified)
**TYPE** — the `juce::ValueTree` node tag; locates a node *within* a session:
| Node | TYPE | Source |
|---|---|---|
| session model root | `SESSION` | Identifier.h:82 |
| Display's node | `DISPLAY` | Identifier.h:100 |
| CodeView's node | `CODE_VIEW` | jam_code_view.cpp:177 |

Located by `getChildWithName(TYPE)` (Input.cpp:67, Mouse.cpp:52/70/121/161, Display.cpp:197, Processor.cpp:210, Session.cpp:306).

**UUID** — instance identity, lives at the **topmost** only:
- PANE leaf node id: `addLeaf` → `leaf.setProperty(id, uuid)` (jam_pane_manager.cpp:34); `findLeaf` matches it (:178). The actual topmost node.
- SESSION root `jam::ID::id` = UUID (`State::setId`, State.cpp:284) — model root, one level down, same UUID.
- `Display::componentID` = UUID — read at 10+ pane-lookup sites (Tabs.cpp:254/272/356/487, Panes.cpp:193/242/288/341/505/538).

**Children carry no UUID** (verified): no `jam::ID::id` on `DISPLAY`/`CODE_VIEW` nodes; `CodeView` has **no `componentID`** at all. Lookup = `findLeaf(UUID) → SESSION → getChildWithName(TYPE)`.

`juce::Identifier` allows `a-zA-Z0-9_-:#@$%`, no first-char rule (juce_Identifier.cpp:83); `Uuid::toString()` is 32 hex (juce_Uuid.cpp:101) — a UUID is a valid `Identifier`; `Identifier`↔`String` lossless.

**`juce::Component` carriers:** `Name` (unused on these views today), `componentID` (= UUID), `getProperties()` → `NamedValueSet` (used elsewhere, Dialog.cpp:23).

### Config (audit of `lua::Engine::getContext()`)
- **66 sites / 30 files** across `display.*` (22), `nexus.*` (5), `whelmed.*` (5), `keys.*` (2).
- Engine exposes **computed methods**: `buildTheme`/`setGlass`/`getPreferredHeight`/`getHandler` (LookAndFeel\*.cpp), `isClickableExtension` (LinkDetector.h), `getSelectionKeys` (Input), `dpiCorrectedFontSize` (Panes.cpp).
- Engine already owns a `jam::File::Watcher` (Engine.h:10, 1318; `reload()` :1046, 1266). Config is **hot-reloaded** — every key is mutable at runtime.
- `AppState` already mirrors config into typed accessors seeded from Engine (AppState.cpp:19–35; AppState.h:86–171).

### View state-reach audit
Display + helpers reach **beyond their own node**: SESSION model state (`activeScreen`, `cursor`, `screenDirty`, `preview`/`splitCol`, `snapshotDirty`, pixel `width`/`height`, `bracketedPaste`, `promptRow`, `hint*`, `outputBlock`/`cwd`); APP `modalType` (→ AppState, State.cpp:481/488); sibling `CODE_VIEW` node read+written (cols/visibleRows State.cpp:248–260; selection Mouse.cpp:52/70/121/161, Input.cpp:67/140–145; caret Display.cpp:180). (Cited inline in prior sections.) Conclusion: views are orchestrators; access is rewired (Layer 3), not eliminated.

### Reachability / teardown
- `AppState : jam::Context<AppState>` (AppState.h:64); app roots :71–177; modal on TABS (:189); `findLeaf` (jam_pane_manager.h:87). View destroyed **before** Model (Panes.cpp:337–338, 388).

---

## Principles and Rationale

### Three-type separation (faults 1–2; E one-responsibility)
| Type | Role | Status |
|---|---|---|
| `jam::Value` | `juce::Value` integration bag | exists |
| `jam::ValueTree` | `juce::ValueTree` integration bag (`Component`, `ComponentWithID`, `Attachment`) | **NEW — name reused** |
| `jam::Model` | APVTS analog **owning** a `juce::ValueTree` by value | **renamed from old `jam::ValueTree`** |

Old `jam::Model` (audio APVTS) → `jam::AudioModel`. Coherent taxonomy: `jam::Model` / `jam::AudioModel` / `jam::CodeModel` / `terminal::Model` / `AppModel`.

### `jam::ValueTree::Component` carries the node; `getValueTree()` is concrete (NOT pure virtual) — justified asymmetry with `jam::Value`
`jam::Value` uses a pure virtual because a component's `juce::Value` is **widget-intrinsic and polymorphic** (`getFrom()` dispatch, jam_value.h:181–204) — the base cannot own a uniform Value. A view's `juce::ValueTree` has **no such variation**: it is a plain node the view owns identically every time. So the base **owns the node** and provides `getValueTree()` **concretely**; a pure virtual would be pointless busywork the Derived must re-implement. The Derived (Display/CodeView) does **not** implement `getValueTree()`.

### ID enforcement — TYPE always, UUID only at the top (S-SSOT)
A `juce::ValueTree` is null without a TYPE — so TYPE is enforced for every view (node type + `Component::Name`). UUID is the **topmost** contract (PANE leaf + SESSION root + the pane component's `componentID`), set by existing mechanisms (`addLeaf`, `State::setId`); children are located by `findLeaf(UUID)→SESSION→getChildWithName(TYPE)` and carry **no** UUID. `ComponentWithID` therefore takes UUID **optionally**: the pane/topmost view passes it (→ `componentID`), child views (CodeView) pass TYPE only. `Identifier`↔`String` is lossless so the TYPE coupling (`Name` ⇄ node type) and the UUID coupling (`componentID` ⇄ PANE/SESSION id) cannot drift.

### Scoped listening eliminates the double-fire by construction (fault 5; D)
Each view listens only on its own grafted subtree (content) and the dedicated **CONFIG** subtree (config) — disjoint scopes, never the root. The `ConfigListener` workaround (Display.cpp:88) is deleted. Non-determinism was a symptom of listening above one's responsibility.

### Single, READ-ONLY config authority (fault 5; S-SSOT, S-Stateless, L)
`AppModel` is the sole config authority: owns `lua::Engine` **privately** (never exposed) and the single `jam::File::Watcher`. On change: watcher → tells Engine (dumb parser) to parse → AppModel rebuilds **its own state**; derivation runs once, bounded to the update path. All config is hot-reloadable and lives in the **CONFIG tree**; "listen vs read-on-demand" is each consumer's choice, not a key partition. Config-derived results (theme, clickable-extension set, selection keymap, dpi metrics) are AppModel accessors recomputed on reload. All 66 `Engine::getContext()` sites repoint to `AppModel`.

**Config is READ-ONLY / immutable to consumers** — they read accessors or register listeners; they never receive a mutable CONFIG handle. The only write paths are (1) AppModel's internal hot-reload update, (2) the **Action List keyboard-shortcut override** (the sole consumer-initiated mutation, scoped to shortcuts, via a dedicated AppModel method). Since `juce::ValueTree` handles are inherently mutable, read-only is enforced by API discipline (`const` accessors + listener registration; the CONFIG node itself is not handed out).

**Not a god object** (§S-Stateless: "State belongs exclusively to the Model"). Distributing config would be shadow / shared-mutable state (§S-SSOT, §B) — worse. L targets *logic* concentration, not data; the heavy per-session state machine stays in `terminal::Model`. **Constraint:** config-derived *results* are state; derivation does not accrete as unrelated methods on `AppModel`.

### Bounded Context access, not threaded refs (E vs L/B)
Objects reach state via by-value `juce::ValueTree` handles at their init list — app roots via `AppModel::getContext()`, session subtrees via `findLeaf(uuid)`. `Context::getContext()` is explicit/named (sanctioned, 279 AppState refs); deep ref-threading would push unused refs through intermediate layers (§L) and widen coupling (§B). **`Processor` keeps `terminal::Model&`** for reader-thread atomics (`storeValue`/`loadValue`, Processor.h:349) — handles are message-thread tree access only.

### Pure views, Model-mediated input (E tell/don't-ask); ownership (B)
Each view `getValueTree()` returns its own node. Selection written into `CODE_VIEW` is Model state in the SSOT tree (Controller→Model→View). `jam::ValueTree::Attachment` is **parent/orchestrator-owned** (one level up the same chain), RAII graft/ungraft. **No View or Model object ownership moves**; "Session grafts SESSION via `jam::Model` API" moves a graft *responsibility* (Panes.cpp:162 → Session), not ownership. View-before-Model teardown (Panes.cpp:337–338, 388) preserved.

### Considered and rejected
- Change the APVTS State model — rejected; only its name and the `unique_ptr` change.
- Distribute config per object — shadow/shared-mutable (§S-SSOT/§B).
- Expose AppModel's `lua::Engine` — leaks parsing detail; resolved values only.
- Second watcher in AppModel — duplicate truth; Engine's watch role retires.
- UUID on child nodes / pure-virtual `getValueTree` / node inside `ComponentWithID` — rejected: children are TYPE-only; the node is uniform so the base owns it concretely; the ID mix-in carries no data (mirrors `jam::Value`).

---

## Scaffold

### Layer 0 — `jam`

```cpp
namespace jam
{
struct ValueTree   // integration bag for juce::ValueTree (NOT a state owner)
{
    // Attachable view: owns its node; getValueTree() is CONCRETE (uniform node, no polymorphic source).
    struct Component
    {
        explicit Component (const juce::Identifier& type) : node (type) {}
        virtual ~Component() = default;
        juce::ValueTree& getValueTree() noexcept { return node; }
        std::function<void()> onAttachment;
    protected:
        juce::ValueTree node;
    };

    // CRTP ID-enforcement mix-in — carries NO data (mirrors jam::Value::ComponentWithID).
    // TYPE always (Name + node type); UUID optional (componentID, top/pane only).
    template <typename Derived>
    struct ComponentWithID : Component
    {
        explicit ComponentWithID (const juce::Identifier& type) : Component (type)
        {
            static_assert (std::is_base_of<juce::Component, Derived>::value,
                           "ComponentWithID requires a juce::Component subclass");
            static_cast<Derived*> (this)->setName (type.toString());     // Name = TYPE
        }
        ComponentWithID (const juce::Identifier& type, const juce::String& uuid) : ComponentWithID (type)
        {
            static_cast<Derived*> (this)->setComponentID (uuid);         // componentID = UUID (contract)
        }
    };

    // RAII graft of a view's OWN node into a parent juce::ValueTree.
    // Seeds from the view's juce::Component::getProperties() (SSOT). Orchestrator-owned.
    class Attachment
    {
    public:
        Attachment (juce::ValueTree& parent, Component& view) noexcept
            : parentTree (parent)
            , node (view.getValueTree())                       // adopt the view's typed node
        {
            // Seed from the view's own juce::Component property set (the view IS-A juce::Component;
            // Component is polymorphic, so reach it via the concrete view). SSOT for view-state schema.
            const auto& props { dynamic_cast<juce::Component&> (view).getProperties() };
            for (const auto& nv : props)                        // juce_NamedValueSet.h:92-93
                node.setProperty (nv.name, nv.value, nullptr);

            parentTree.appendChild (node, nullptr);
            if (view.onAttachment != nullptr) view.onAttachment();
        }
        ~Attachment() { auto p { node.getParent() }; if (p.isValid()) p.removeChild (node, nullptr); }
        void setValue (const juce::Identifier& id, const juce::var& v) noexcept { node.setProperty (id, v, nullptr); }
        juce::ValueTree& getNode() noexcept { return node; }
    private:
        juce::ValueTree& parentTree;
        juce::ValueTree  node;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Attachment)
    };
};

class Model : public juce::Timer    // renamed from old jam::ValueTree
{
public:
    explicit Model (const juce::Identifier& treeId);
    Model();
    juce::ValueTree get() const noexcept { return state; }   // BY VALUE — cheap ref-counted handle (Q1=b)
    // APVTS-pattern atomic store + flush + addParameter<> unchanged.
private:
    juce::ValueTree state;          // BY VALUE — juce_AudioProcessorValueTreeState.h:417
    mutable jam::AnyMap params;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
} // namespace jam
```
`get()` returns the handle by value (mutations still reach the shared object); callers binding `juce::ValueTree& x = m.get()` convert to `auto x = m.get()`.

### Layer 1 — `end`
- `terminal::Model` (was `terminal::State`), files `Model.{h,cpp}` : `jam::Model`. View-only convenience getters fall away as callers convert to handle reads.
- `AppModel` (was `AppState`) : `jam::Model` + `jam::Context<AppModel>`; owns `lua::Engine` privately + single `jam::File::Watcher`; `getConfig()` CONFIG subtree (read-only to consumers) + resolved-config accessors; `overrideShortcut(...)` the one Action-List write path.

### Layer 2 — views
```cpp
class PaneView : public juce::Component, public jam::ValueTree::ComponentWithID<PaneView>
{
public:
    PaneView (const juce::Identifier& type, const juce::String& uuid)        // pane/top: TYPE + UUID
        : jam::ValueTree::ComponentWithID<PaneView> (type, uuid) { setWantsKeyboardFocus (true); }
    virtual juce::String getPaneType() const noexcept = 0;
};
// terminal::Display : PaneView(terminal::id::DISPLAY, sessionUuid)
//   - inherits getValueTree() (the DISPLAY node) — does NOT implement it.
//   - declares its seed props (cell metrics) on its juce::Component::getProperties().
//   - Attachment (owned by Display's orchestrator) grafts the DISPLAY node into SESSION.
//   - listens ONLY on its SESSION subtree (content) + AppModel::getContext()->getConfig(). Root listen DELETED.
// jam::CodeView : jam::ValueTree::ComponentWithID<CodeView>(CODE_VIEW)        // child: TYPE only, no UUID
//   - inherits getValueTree() (the CODE_VIEW node); declares selection/caret/viewport props on getProperties().
```

### Layer 3 — access topology
```cpp
LinkManager::LinkManager (const juce::String& sessionUuid, WriteFn write)
    : sessionNode      (jam::PaneManager::findLeaf (AppModel::getContext()->getTabs(), sessionUuid)
                            .getChildWithName (terminal::id::SESSION))
    , promptRowNode    (jam::Model::getChildWithID (sessionNode, terminal::id::promptRow))
    , activeScreenNode (jam::Model::getChildWithID (sessionNode, terminal::id::activeScreen))
    , writeToPty (std::move (write))
{ promptRowNode.addListener (this); activeScreenNode.addListener (this); }   // scoped (LinkManager.cpp:22-23)
// modal: AppModel::getContext()->getModalType(); Processor keeps terminal::Model& for atomics.
```

### Layer 4 — config flow
```
config file changed -> AppModel watcher -> AppModel::onConfigChanged():
    engine->parse();                                   // dumb parser
    rebuild CONFIG subtree (all config — hot-reload)   // scoped listeners propagate
    recompute resolved accessors (theme/links/keys/metrics)
Consumers: read accessors OR listen on getConfig() (read-only). Sole writes: this path + Action-List shortcut override.
```

---

## BLESSED Compliance Checklist
- [x] **Bound** — `Attachment` RAII parent-owned; `jam::Model` owns its tree by value; View-before-Model teardown preserved.
- [x] **Lean** — deletes the workaround, redundant handles, view-side getters; `ComponentWithID` carries no data. *Constraint: config-derived logic stays state, not methods on `AppModel`.*
- [~] **Explicit** — bounded, named `Context` access; deliberate trade vs literal "no hidden globals" to avoid deep ref-threading (§L).
- [x] **SSOT** — single read-only config authority; single tree owner; single CONFIG source; single watcher; TYPE (`Name`⇄node type) and UUID (`componentID`⇄PANE/SESSION id) each coupled to one source.
- [x] **Stateless** — Engine → parser; views → own-node-only; state in the Model.
- [x] **Encapsulation** — concrete `getValueTree()` (uniform node) vs `jam::Value`'s pure virtual (polymorphic value) — justified; input Model-mediated; one responsibility per type.
- [x] **Deterministic** — scoped listening makes the double-fire impossible by construction.

---

## Open Questions
None — all resolved.

- **`getProperties()` conflation: none.** Verified — `Display`, `CodeView`, and `PaneView` use `juce::Component::getProperties()` nowhere today (only `Label`/`Button` widgets do: Dialog.cpp:23/26/27, LookAndFeel.cpp:231/262, ActionRow.cpp:28/34). The view-state seed set is the sole occupant on these components.
- **Diamond: avoided by construction.** A view reaches `jam::ValueTree::Component` through one path only — `PaneView : juce::Component, jam::ValueTree::ComponentWithID<PaneView>`, `CodeView : juce::Component, jam::ValueTree::ComponentWithID<CodeView>` (Layer 2). `juce::Component` and `jam::ValueTree::Component` are unrelated bases; the CRTP sits on the role-introducing class, as `jam::Value`'s Derived does. No second path exists.
- **Migration: Step 0 is the sole isolation gate; steps 2–9 are one delete-first pass.** Per Refactor-Rewrite Discipline, intermediate breakage is expected and correct; staging steps 2–9 into individually-green checkpoints would require forbidden old/new coexistence. COUNSELOR executes the single pass; it is not a discretionary re-sequencing.

---

## Handoff Notes

**Mandatory sequence (bottom-up):**
1. **Step 0 (isolated, compile green):** `jam::Model` → `jam::AudioModel` across audio/registry/view-manager/`bindToModel` (verify APVTS cast jam_view_manager_editor.cpp:278). Do not proceed until green.
2. `jam::ValueTree` (old, stateful) → `jam::Model`; member `std::unique_ptr<juce::ValueTree>` → `juce::ValueTree` by value; `get()` returns by value; convert `juce::ValueTree&`-binding callers to `auto`.
3. New `jam::ValueTree` bag: `Component` (owns node, concrete `getValueTree`), `ComponentWithID<Derived>` (no data; TYPE always, UUID optional), `Attachment` (adopts `getValueTree()`, seeds from `getProperties()`, RAII; no `type`/`NamedValueSet` params). Migrate `ComponentAttachment` → `jam::ValueTree::Attachment`.
4. `terminal::State` → `terminal::Model` (files `Model.{h,cpp}`); `AppState` → `AppModel`.
5. `PaneComponent` → `PaneView`; `PaneView`/`CodeView` derive `jam::ValueTree::ComponentWithID`; `Display` passes `(DISPLAY, sessionUuid)`, `CodeView` passes `(CODE_VIEW)`; declare seed props on each view's `getProperties()`; remove their `getValueTree()` implementations (inherited).
6. Move SESSION graft to Session (via `jam::Model` API); delete manual `appendChild` (Panes.cpp:162, 459, 212); relocate `Attachment` ownership to the view's owner.
7. Convert view-side `terminal::Model&`/`processor.getState()` reads to by-value handles via `AppModel::getContext()` / `findLeaf(uuid)`; repoint identity consumers (Tabs.cpp:168/172/174, Panes.cpp:289/576, Popup.cpp:34) via Session.
8. `AppModel` config authority: own `lua::Engine` privately + single `jam::File::Watcher`; CONFIG subtree (read-only) + resolved accessors + Action-List shortcut-override method; repoint all 66 `Engine::getContext()` sites (incl. whelmed, to compile).
9. Delete the double-fire workaround: remove root listening (Display.cpp:77; whelmed Component.cpp:41); each view listens only on its own subtree + CONFIG.

**Refactor-Rewrite Discipline:** delete-first; old and new do not coexist; compiler errors are ground of truth.

**Blast radius:** `AppState` 279 refs / 37 files; `terminal::State` ~201 refs; `jam::ValueTree` 80 refs / 24 files; `Engine::getContext()` 66 sites / 30 files.

**BLESSED constraints carried into PLAN:**
- Identity: TYPE (`Name`⇄node type) always; UUID (`componentID`⇄PANE/SESSION id) at the top only — UUID is a non-negotiable contract; children TYPE-only.
- Config is read-only to consumers; sole writes = hot-reload + Action-List shortcut override; config-derived results are AppModel state, derivation bounded to the update path.
- View-before-Model teardown preserved (Panes.cpp:337–388); no ownership re-parenting.
- `Processor` retains `terminal::Model&` for reader-thread atomics; handles are message-thread only.
- `whelmed` is rename/repoint only; internals deferred; `whelmed::State` (State.h:9, not `jam::Model`-derived) untouched.
