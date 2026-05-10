# PLAN: State Refactor — Declarative XML-Driven Typed Atomics

**RFC:** RFC-state-refactor.md
**Date:** 2026-05-10
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)

## Overview

Replace the manual APVTS-style atomic store with a declarative XML-driven system that mirrors APVTS 1:1. `Parameters.xml` declares the schema. State constructor walks it uniformly, creates one Atom (adapter) per parameter — each Atom holds `std::atomic<int>` + its own ValueTree child node ref. One AnyMap (nested for hierarchy), one ValueTree, one flush loop. `getRawParameterValue(id)` returns `std::atomic<int>*`. `map::Screen` (jam::Map::Instance) replaces scattered `ActiveScreen` enum for indexed screen access.

## APVTS 1:1 Mapping

| APVTS | Terminal State |
|---|---|
| `ParameterLayout` | `Parameters.xml` via `jam::XML::getFromBinary(jam::ID::pluginMetadata)` |
| `ParameterAdapter` (atomic + VT node ref + self-flush) | `Atom` |
| `adapterTable` (one flat map) | `jam::AnyMap params` (one, nested for hierarchy) |
| `ValueTree state` (one root) | `juce::ValueTree state` (one root) |
| `getRawParameterValue(id)` → `std::atomic<float>*` | `getRawParameterValue(id)` → `std::atomic<int>*` |
| `flushParameterValuesToValueTree()` | `flush()` — one `forEach`, each Atom writes itself |
| `map::Page` (Context-managed page index) | `map::Screen` (Context-managed screen index) |
| `ParameterPage::attach()` | `<SCREEN>` duplication per `map::Screen` entry |

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: std::hash<juce::Identifier> — move to jam_core

**Scope:** `jam/jam_core/utilities/jam_identifier_hash.h`, `Source/terminal/data/Identifier.h`
**Action:** Already landed. Hash specialization at jam_core level, removed from END's Identifier.h.
**Status:** DONE

### Step 2: jam::AnyMap — Identifier keys + forEach

**Scope:** `jam/jam_core/utilities/jam_any_map.h`
**Action:** Already landed. Internal key `juce::Identifier`, String overloads, `forEach<Base>()`.
**Status:** DONE

### Step 3: map::Screen — jam::Map::Instance for screen indexing

**Scope:** `Source/terminal/data/Screen.h` (new), `Source/Main.cpp`
**Action:** Create `Terminal::map::Screen : jam::Map::Instance<Screen>` with `{normal: "NORMAL", alternate: "ALTERNATE"}` and `getDefault()` returning NORMAL. Owned by `ENDApplication` (value member in Main.cpp), accessible via `map::Screen::getContext()`. Remove `ActiveScreen` enum from Identifier.h — replaced by `map::Screen` enum values.
**Validation:** `map::Screen::getContext()->get(0)` returns `"NORMAL"`. `map::Screen::getContext()->get("ALTERNATE")` returns `1`. Compiles. No `ActiveScreen` enum remains.

### Step 4: Atom — the adapter (atomic + VT node ref + self-flush)

**Scope:** `Source/terminal/data/Atom.h` (rewrite)
**Action:** Rewrite Atom to mirror APVTS ParameterAdapter:
- `Atom<int>`: holds `std::atomic<int> value` + `juce::ValueTree tree` (ref to own PARAM child node). `load()`, `store()` with relaxed default. `raw()` for exception callsites. `flush()` no-arg — writes `value` to `tree.setProperty(ID::value, ...)`.
- `Atom<const char*>`: holds `std::atomic<const char*> ptr` + `juce::ValueTree tree` (ref to SESSION root) + `juce::Identifier property` (the key to write to). `load()` acquire, `store()` release. `flush()` no-arg — writes string to `tree.setProperty(property, ...)`.
- `AtomBase`: virtual base with `flush()` no-arg.
**Validation:** Each Atom is self-contained. `flush()` takes no arguments. No external routing needed. JRENG-CODING-STANDARD compliant.

### Step 5a: Layout — static tree builder from XML

**Scope:** `Source/terminal/data/Layout.h` (new)
**Action:** Two layers, like APVTS:

**Layer 1 — `State::addParameter()`** — the SSOT method on State. Analogous to APVTS's `addParameterAdapter()`. Creates Atom, creates VT PARAM child, assigns VT node ref to Atom, adds to AnyMap. ONE method. Everything flows through it — XML builder at construction AND future runtime additions (image, scrollback state machines).

**Layer 2 — `Layout::build()`** — static free function analogous to `kuassa::parameter::Layout::get(xml)`. Walks the XML, calls `State::addParameter()` for each element. Does NOT create atoms or VT nodes directly — flows through the SSOT method.
- `static void build (const juce::XmlElement& xml, State& state)`
- Walks XML uniformly. Tag dispatch via `Terminal::ID` constants.
- For each `<PARAM>`: calls `state.addParameter(id, defaultValue, targetMap, targetVTNode)`
- `<MODES>` → creates nested AnyMap + VT group node, walks children calling addParameter into the group.
- `<SCREEN>` → reads schema once, duplicates per `map::Screen::getContext()->get()` entry. Calls addParameter per screen param.
- `<TEXT>` → calls addParameter for text atoms + `textBuffer.addSlot()`.
- Private nested `struct Boolean : jam::Map::Instance<Boolean>` with `{0: "false", 1: "true"}` for bool default resolution. No string comparison.

**Future usage (image, scrollback):** Runtime code calls `state.addParameter()` directly for dynamic parameters. Same SSOT method. Same flow. No new pattern.

**Validation:** `addParameter()` is the sole creation path. Layout flows through it. Future code flows through it. Nothing bypasses it. BLESSED-S (SSOT). State ctor just calls `Layout::build(xml, *this)`. Clean.

### Step 5: TextBuffer — Session-owned double-buffered string storage

**Scope:** `Source/terminal/data/TextBuffer.h`
**Action:** Already landed. TextSlot + TextBuffer.
**Status:** DONE

### Step 6: State — XML-driven constructor, one map, one tree, APVTS API

**Scope:** `Source/terminal/data/State.h`, `Source/terminal/data/State.cpp`, `Source/terminal/data/StateFlush.cpp`
**Action:** Complete rewrite of State following APVTS pattern:

**Header:**
- `explicit State (TextBuffer& textBuffer)` constructor
- `getRawParameterValue (const juce::Identifier& id)` → `std::atomic<int>*` (APVTS API)
- `getRawParameterValue (int screen, const juce::Identifier& id)` → `std::atomic<int>*` (screen-indexed)
- One `jam::AnyMap params` — nested: root atoms at top, `ID::MODES` nested AnyMap, `ID::NORMAL`/`ID::ALTERNATE` nested AnyMaps (driven by `map::Screen`)
- One `juce::ValueTree state`
- Standalone `std::atomic<int> needsFlush`, `std::atomic<int> snapshotDirty` — machinery, not parameters
- Keyboard mode stack stays (internal bookkeeping, `keyboardFlags` is the parameter)
- NO stray members: no `scrollbackCapacity`, no sentinels, no cached VT refs
- All public setters use `getRawParameterValue()` internally + set needsFlush
- All public getters read from ValueTree (message thread) or via `getRawParameterValue()` (any thread)

**Constructor:**
- Load XML via `jam::XML::getFromBinary (jam::ID::pluginMetadata)`
- Private nested `struct Boolean : jam::Map::Instance<Boolean>` with `{0: "false", 1: "true"}`. Used for `getBoolAttribute` conversion — `boolMap.get(attr)` returns 0/1. No string comparison anywhere.
- One uniform walk: for each `<PARAM>`, create Atom with VT child node ref, add to AnyMap. Tag dispatch via `Terminal::ID` constants (no magic strings). Bool defaults resolved via `map::Boolean` lookup, int defaults via `getIntAttribute`.
- `<MODES>` → nested AnyMap keyed `ID::MODES`, VT child node `ID::MODES`
- `<SCREEN>` — read schema once, duplicate per `map::Screen::getContext()->get()` entry. Each screen gets nested AnyMap + VT child node. Same logic as `ParameterPage::attach()`.
- `<TEXT>` → `Atom<const char*>` in root AnyMap + `TextBuffer::addSlot()`

**Flush (StateFlush.cpp):**
- One `forEach<AtomBase>` on root params. Each Atom writes itself (no-arg flush).
- Recursive: nested AnyMaps (MODES, screens) — `forEach` on each nested map.
- displayName computation after flush.
- No `flushImages`, no `flushStrings`, no `flushRootParams`, no `flushGroupParams`.

**Validation:** One tree, one map, self-flushing adapters, `getRawParameterValue()` API, no shadow state, no magic strings, no sentinels.

### Step 7: Remove scrollback + image state

**Scope:** `Source/terminal/data/State.h`, `Source/terminal/data/State.cpp`, `Source/terminal/logic/Processor.cpp`, `Source/terminal/logic/VideoEdit.cpp`
**Action:** Remove all scrollback and image related state:
- Delete: `scrollbackCapacity`, `setScrollbackCapacity`, `setScrollbackUsed`, `getScrollbackUsed`, `setScrollPosition`, `getScrollPosition`
- Delete: `queueImageErase`, `flushImages`, `addImageNode`, `removeImageNode`, IMAGES container, erase bounding box params
- Delete: `linkUriCount`, `fullRebuild`/`consumeFullRebuild` (dead), SUBSCRIBERS (dead), seqno (dead)
- Stub `getScrollbackUsed()` → return 0 (has external callers in Input.cpp, Mouse.cpp, Processor.cpp)
- Remove `queueImageErase` event registration in Processor.cpp and fire sites in VideoEdit.cpp
- Keep `dismissPreview`/`isPreviewActive`/`getSplitCol` (live callers) — read/write via `getRawParameterValue()`
- Delete `activatePreview` (zero external callers)
**Validation:** Compiles. No scrollback/image state on State. No IMAGES container. No sentinels.

### Step 8: Session + Processor wiring

**Scope:** `Source/terminal/logic/Session.h/cpp`, `Source/terminal/logic/Processor.h/cpp`
**Action:** Session owns TextBuffer. Processor receives TextBuffer&, passes to State. State constructor signature: `State(TextBuffer&)`. Processor's `syncVideoToState()` uses `getRawParameterValue()`. All event handlers migrated. Dead mode syncs removed (originMode, autoWrap, insertMode, mouseSgr, applicationKeypad, reverseVideo — Video tracks internally).
**Validation:** Compiles. No `static_cast<float>`. No compound key helpers. Only live modes synced.

### Step 9: map::Screen ownership + ActiveScreen cleanup

**Scope:** `Source/Main.cpp`, `Source/terminal/data/Identifier.h`, all `ActiveScreen` callers
**Action:** Add `Terminal::map::Screen` member to `ENDApplication`. Remove `ActiveScreen` enum from Identifier.h. Migrate all `ActiveScreen` callers to `map::Screen` enum values. Screen index access via `map::Screen::getContext()->get(index)`.
**Validation:** No `ActiveScreen` enum anywhere. All screen indexing via `map::Screen`.

## BLESSED Alignment

- **B (Bound):** Session owns TextBuffer. State owns ValueTree + AnyMap. Each Atom owns its VT node ref. RAII throughout. 16.5 MB linkUriTable eliminated.
- **L (Lean):** Parameters.xml replaces ~50 addParam calls. One flush loop. No compound key helpers. Dead code removed.
- **E (Explicit):** Parameters.xml is the readable schema. `getRawParameterValue()` — same APVTS API. `Atom::load()`/`store()` encode memory order. No magic strings — `Terminal::ID` constants. No sentinels.
- **S (SSOT):** Parameters.xml for schema. ValueTree for runtime. One AnyMap. No shadow state — no stray members, no cached VT refs, no sentinel constants.
- **S (Stateless):** Parser writes to Grid and TextBuffer. Both transient buffers. State is the model, not machinery on machinery.
- **E (Encapsulation):** Each Atom flushes itself (no-arg). State builds itself from XML. Callers use `getRawParameterValue()` — never see Atom internals. `map::Screen` encapsulates screen indexing.
- **D (Deterministic):** One flush loop. Same path for all types. Each Atom writes to its own VT node. No routing, no dispatch.

## Locked Decisions

1. **APVTS 1:1** — no new patterns, no handrolling. Same logic, same API surface.
2. **One tree, one map** — State IS the ValueTree. One `jam::AnyMap params` (nested for hierarchy).
3. **Atom is the adapter** — holds atomic + own VT node ref. Self-flushing. Internal to State — callers use `getRawParameterValue()`.
4. **`map::Screen`** — `jam::Map::Instance<Screen>`, Context-managed, owned by ENDApplication. Replaces `ActiveScreen` enum.
5. **`<SCREEN>` declared once** — constructor duplicates per `map::Screen` entry (ParameterPage::attach pattern).
6. **Signals are machinery** — `needsFlush`, `snapshotDirty` are standalone `std::atomic<int>`, not parameters.
7. **Everything that is state is a parameter** — blink machinery, paste echo, sync output, cursor color, cursor visible. No stray members.
8. **Dead modes removed** — originMode, autoWrap, insertMode, mouseSgr, applicationKeypad, reverseVideo. Video tracks internally. No shadow state.
9. **YAGNI removed** — scrollback state, image state, erase bounding box, linkUriTable, SUBSCRIBERS, seqno, fullRebuild.
10. **Use the framework** — `jam::XML::getFromBinary(jam::ID::pluginMetadata)`, `jam::Map::Instance`, `jam::AnyMap`, `jam::Context`. No handrolled equivalents.
