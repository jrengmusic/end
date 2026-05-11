# PLAN: State Refactor — jam::ValueTree as Universal SSOT State Machine

**RFC:** RFC-state-refactor.md (scope superseded — now jam::ValueTree-level)
**Date:** 2026-05-11
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE (reference implementation, no overrides)

## Overview

Lift the proven APVTS-like atomic state pattern from `Terminal::` into `jam::ValueTree`, making it the universal lock-free SSOT state machine for all BLESSED GUI apps. XML-driven declarative layout via `jam::ValueTree::Parameter::Layout(xml)`. Generic `Atom<T>` for any trivially copyable type. Adaptive timer-driven flush. `Terminal::State` and `AppState` both inherit `jam::ValueTree` and implement only their specifics. `AppLayout` eliminated entirely. `Terminal::Layout` shrinks to SCREEN expansion.

## Current State (Already Landed)

| Component | Location | Status |
|---|---|---|
| `std::hash<juce::Identifier>` | `jam_core/utilities/jam_identifier_hash.h` | DONE |
| `jam::AnyMap` (Identifier keys + forEach) | `jam_core/utilities/jam_any_map.h` | DONE |
| `Terminal::TextSlot` + `TextBuffer` | `Source/terminal/data/TextBuffer.h` | DONE |
| `Terminal::AtomBase` + `Atom<int>` + `Atom<const char*>` | `Source/terminal/data/Atom.h` | DONE (explicit specializations) |
| `Terminal::Layout::build(xml, state, textBuffer)` | `Source/terminal/data/Layout.cpp` | DONE |
| `Terminal::State` (AnyMap + atoms + flush + timer) | `Source/terminal/data/State.h/cpp + StateFlush.cpp` | DONE |
| `AppLayout::build(xml, state)` | `Source/AppLayout.cpp` | DONE |
| `AppState` (VT + direct properties) | `Source/AppState.h/cpp` | DONE |
| `jam::ValueTree` (thin wrapper) | `jam_data_structures/value_tree/jam_value_tree.h` | DONE (to be reshaped) |
| `map::Screen` (Context-managed screen index) | `Source/terminal/data/Screen.h` | DONE |

## APVTS 1:1 Mapping

| APVTS | jam::ValueTree |
|---|---|
| `ParameterLayout` (builder input) | XML schema via `jam::XML::getFromBinary(...)` |
| `ParameterAdapter` (atomic + VT node + self-flush) | `jam::Atom<T>` (generic, trivially copyable) |
| `adapterTable` (map of adapters) | `jam::AnyMap params` (nested for hierarchy) |
| `ValueTree state` (SSOT) | `juce::ValueTree state` (direct member) |
| `getRawParameterValue(id)` → `std::atomic<float>*` | `getRawParameterValue<T>(id)` → `std::atomic<T>*` |
| `flushParameterValuesToValueTree()` | `flush()` — `forEach<AtomBase>`, each Atom writes itself |
| `private juce::Timer` + adaptive cadence | `private juce::Timer` + configurable adaptive cadence |
| `ParameterAdapter::flushToTree()` (no-arg) | `Atom<T>::flush()` (no-arg, self-contained) |

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: jam::Atom<T> — generic atomic adapter

**Scope:** `jam/jam_data_structures/value_tree/jam_atom.h` (new)
**Source:** Extract and generalize from `Source/terminal/data/Atom.h`
**Action:**

`AtomBase` — virtual base with no-arg `flush()`. Zero VT dependency.

```cpp
struct AtomBase
{
    virtual ~AtomBase() = default;
    virtual void flush() noexcept = 0;
};
```

`Atom<T>` — single generic template. Constraints enforced at compile time.

```cpp
template<typename T>
struct Atom final : AtomBase
{
    static_assert (std::is_trivially_copyable_v<T>);
    static_assert (std::atomic<T>::is_always_lock_free);

    Atom (T defaultValue, juce::ValueTree node,
          const juce::Identifier& property) noexcept
        : value { defaultValue }, tree { node }, key { property } {}

    T load() const noexcept { return value.load (loadOrder()); }
    void store (T v) noexcept { value.store (v, storeOrder()); }
    std::atomic<T>& raw() noexcept { return value; }

    void flush() noexcept override
    {
        if constexpr (std::is_pointer_v<T>)
        {
            auto p { value.load (std::memory_order_acquire) };
            if (p != nullptr)
                tree.setProperty (key, juce::String::fromUTF8 (p), nullptr);
        }
        else
        {
            tree.setProperty (key, juce::var (value.load (std::memory_order_relaxed)), nullptr);
        }
    }

private:
    static constexpr std::memory_order loadOrder() noexcept
    {
        if constexpr (std::is_pointer_v<T>)
            return std::memory_order_acquire;
        else
            return std::memory_order_relaxed;
    }

    static constexpr std::memory_order storeOrder() noexcept
    {
        if constexpr (std::is_pointer_v<T>)
            return std::memory_order_release;
        else
            return std::memory_order_relaxed;
    }

    std::atomic<T> value;
    juce::ValueTree tree;
    juce::Identifier key;
};
```

Valid types: `int`, `float`, `double`, `bool`, `const char*`, any enum, any POD ≤ 8 bytes. Compile-time rejection for everything else.

**Validation:** `Atom<int>`, `Atom<float>`, `Atom<const char*>` compile. `Atom<std::string>` fails static_assert. Memory ordering correct: scalars relaxed, pointers acquire/release. `flush()` writes to VT.

### Step 2: jam::TextBuffer — move to jam_core

**Scope:** `jam/jam_core/concurrency/jam_text_buffer.h` (new)
**Source:** Move from `Source/terminal/data/TextBuffer.h`
**Action:**
- `jam::TextSlot` — double-buffered char storage. Same implementation, `jam::` namespace.
- `jam::TextBuffer` — `unordered_map<Identifier, unique_ptr<TextSlot>>` with `addSlot()` and `write()`.
- Lives alongside `jam::Mailbox` and `jam::SnapshotBuffer` in `jam_core/concurrency/`.
- Only depends on `juce_core` (HeapBlock, Identifier, jmin) + `<atomic>`.
- Delete `Source/terminal/data/TextBuffer.h`. Update includes in State.h, Layout.cpp.

**Validation:** Compiles in jam_core. Terminal::State includes from jam. No Terminal::TextBuffer remains.

### Step 3: jam::ValueTree — reshape as SSOT state machine

**Scope:** `jam/jam_data_structures/value_tree/jam_value_tree.h` + `.cpp`

**Remove:**
- `juce::Value::Listener` inheritance and all Listener-related methods
- `onValueChanged` callback
- `getValue()` / `setValue()` (old property access — replaced by atom-based access)
- `std::unique_ptr<juce::ValueTree>` indirection → direct `juce::ValueTree state` member

**Add:**
- `private juce::Timer` inheritance (APVTS pattern). Module dependency: `juce_events`.
- `mutable jam::AnyMap params` — nested group structure, public for Layout access.
- `std::atomic<int> needsFlush { 0 }` — dirty flag (standalone machinery, not a parameter).

Registration API:
- `addParameter(id, defaultValue, targetGroup, parentNode)` → creates PARAM VT child + `Atom<int>` in targetGroup. Returns `std::atomic<int>*`. Same as current `Terminal::State::addParameter()`.
- `addTextParameter(id, rootNode)` → creates `Atom<const char*>` in root group. Flushes to direct property on rootNode.
- `addGroup(groupId)` → creates nested `jam::AnyMap` in params + child VT node under state root. Returns `jam::AnyMap&`.

Access API:
- `getRawParameterValue(id)` → `std::atomic<int>*` from root group
- `getRawParameterValue(groupId, paramId)` → `std::atomic<int>*` from named group
- `getValueTree()` → `juce::ValueTree&` (raw access, const + non-const)

Flush API:
- `virtual bool flush() noexcept` — default: iterate all groups via `forEach<AtomBase>`, returns true if dirty. Override for custom ordering.
- `void flushGroup(const Identifier& groupId) noexcept` — flush one specific group.
- `void markDirty() noexcept` — set needsFlush with release ordering.
- `void storeAndFlush(std::atomic<int>&, int) noexcept` — store + markDirty. Convenience for the dominant int case.

Timer:
- `timerCallback()` — calls `flush()`, adaptive cadence, fires `onFlush` callback.
- `int idleHz { 60 }` / `int activeHz { 120 }` — configurable, caller sets before `startTimerHz()`.
- `std::function<void()> onFlush` — callback after flush cycle.

**Keep:**
- `get()` → `juce::ValueTree&` (renamed semantics, was dereference of unique_ptr)
- All static utilities: `getChildWithName`, `getOrCreateChildWithName`, `loadState`, `buildUniqueNodeMap`, `writeToXml`, `getXml`, `findAndRemoveChild`, `getChildWithID`, `getValueFromChildWithID`, `getValueFromChildWithName`, `applyFunctionRecursively`
- `UniqueNodeMap` type + accessor
- `replaceState()`
- `#if JUCE_MODULE_AVAILABLE_juce_gui_basics` attach overloads

**Validation:** Compiles. No Value::Listener. Timer works. flush() iterates all groups. Static utilities intact. Existing jam::ValueTree consumers (if any outside END) still compile with kept API.

### Step 4: jam::ValueTree::Parameter::Layout

**Scope:** Nested struct inside `jam::ValueTree` (in `jam_value_tree.h`)

```cpp
class ValueTree : private juce::Timer
{
public:
    struct Parameter
    {
        static void Layout (const juce::XmlElement* xml, ValueTree& target);
        static void Layout (const juce::XmlElement* xml, ValueTree& target,
                            jam::TextBuffer& textBuffer);
    };
    // ...
};
```

**Action:**
- Single entry point: `Parameter::Layout(xml, target)` or `Parameter::Layout(xml, target, textBuffer)` when TEXT parameters are present.
- Generic XML walk, three tag types:

| XML Tag | Action |
|---|---|
| `PARAM` at root | `target.addParameter(id, resolvedDefault, rootGroup, rootNode)` |
| Named group tag | `target.addGroup(tag)` → iterate PARAM children into that group |
| `TEXT` | `target.addTextParameter(id, rootNode)` + `textBuffer.addSlot(id, maxlen)` |

- `Boolean` resolver deduplicated here — `struct Boolean : jam::Map::Instance<Boolean>` with `{0: "false", 1: "true"}`. Private nested struct.
- Type resolution: `bool` via Boolean map → `Atom<int>(0 or 1)`. `int` via `getIntAttribute`. `float` via `getDoubleAttribute` → `Atom<float>`. `string` → direct VT property (no atom, no TextSlot — message-thread only).
- Caller shapes XML before calling Layout. Layout is a dumb executor.

Calling convention (follows `ku::parameter::Layout::get(xml)` pattern):
```cpp
// AppState — direct, no pre-processing
jam::ValueTree::Parameter::Layout (xml.get(), *this);

// Terminal::State — expand SCREEN first, then delegate
auto expanded { Terminal::Layout::expandScreens (*xml) };
jam::ValueTree::Parameter::Layout (expanded.get(), *this, textBuffer);
```

**Validation:** `AppParameters.xml` fed directly → correct VT + AnyMap. `Parameters.xml` after SCREEN expansion → correct VT + AnyMap. Boolean resolution matches current behavior. No AppLayout needed. No Terminal::Layout::Boolean needed.

### Step 5: Terminal::State — inherit jam::ValueTree

**Scope:** `Source/terminal/data/State.h/cpp`, `Source/terminal/data/StateFlush.cpp`, `Source/terminal/data/Layout.h/cpp`, `Source/terminal/data/Atom.h`

**Action:**

State inherits jam::ValueTree:
```cpp
struct State : public jam::ValueTree { ... };
```

**Delete from State** (now in base):
- `juce::Timer` inheritance → base
- `jam::AnyMap params` → base
- `juce::ValueTree state` → base
- `addParameter()` / `addTextParameter()` → base
- `getRawParameterValue()` overloads → base (root + group)
- `storeAndFlush()` → base
- `markDirty()` / `needsFlush` → base
- `timerCallback()` → base (State no longer overrides — configures cadence + onFlush in constructor)

**Delete files:**
- `Source/terminal/data/Atom.h` → replaced by `jam_atom.h` (include redirect or delete)

**Shrink Terminal::Layout** to one job — SCREEN expansion:
```cpp
struct Layout
{
    // Reads <SCREEN> schema once, duplicates per map::Screen entry.
    // Returns expanded XML with NORMAL + ALTERNATE as separate group tags.
    static std::unique_ptr<juce::XmlElement> expandScreens (const juce::XmlElement& xml);
};
```
All PARAM/group/TEXT handling deleted from Terminal::Layout — now in `Parameter::Layout`.

**Override flush()** — Terminal::State needs specific ordering (MODES → screens → SESSION → displayName):
```cpp
bool State::flush() noexcept override
{
    if (getRawParameterValue (ID::needsFlush)->exchange (0, std::memory_order_acquire) != 0)
    {
        flushGroup (ID::MODES);

        for (const auto& [index, screenName] : map::Screen::getContext()->get())
            flushGroup (juce::Identifier { screenName });

        flushGroup (ID::SESSION);

        // displayName computation (Terminal-specific)
        // ...

        return true;
    }
    return false;
}
```

**Keep on State** (domain-specific, not base concern):
- `TextBuffer&` reference
- Keyboard mode stack (`keyboardModeStack`, `keyboardModeStackSize`)
- All domain setters/getters (setCursorRow, setMode, setTitle, etc.)
- `onFlush` callback wiring (set in constructor, base fires it)
- Screen-indexed `getRawParameterValue(int screen, id)` convenience (calls base `getRawParameterValue(screenId, id)` with map::Screen lookup)
- `getModeParameterValue(id)` convenience (calls base with MODES group)

**Constructor becomes:**
```cpp
State::State (TextBuffer& tb) : jam::ValueTree (ID::SESSION), textBuffer (tb)
{
    auto xml { jam::XML::getFromBinary (jam::IDref::pluginMetadata) };
    auto expanded { Layout::expandScreens (*xml) };
    Parameter::Layout (expanded.get(), *this, textBuffer);

    keyboardModeStack.allocate (2 * maxKeyboardStackDepth, true);
    keyboardModeStackSize.allocate (2, true);

    idleHz = 60;
    activeHz = 120;
    startTimerHz (idleHz);
}
```

**Validation:** Compiles. Same behavior as current. All parameter access via base API. Atom.h gone or redirects to jam. Terminal::Layout is SCREEN expansion only.

### Step 6: AppState — inherit jam::ValueTree

**Scope:** `Source/AppState.h/cpp`, `Source/AppLayout.h`, `Source/AppLayout.cpp`

**Action:**

AppState inherits jam::ValueTree:
```cpp
struct AppState : public jam::ValueTree, public jam::Context<AppState> { ... };
```

**Delete files:**
- `Source/AppLayout.h` — eliminated entirely
- `Source/AppLayout.cpp` — eliminated entirely

**Constructor becomes:**
```cpp
void AppState::initDefaults()
{
    auto xml { jam::XML::getFromBinary (App::ID::appMetadata) };
    Parameter::Layout (xml.get(), *this);

    // Lua config overlay on top of XML defaults (existing logic)
    // ...

    // Timer idles — no cross-thread writes, nothing dirty, backs off
    idleHz = 60;
    activeHz = 120;
    startTimerHz (idleHz);
}
```

**`std::atomic<bool> atlasDirty`** stays as standalone member — GPU signaling, not a VT parameter.

**Keep:**
- `jam::Context<AppState>` (singleton accessor)
- Serialization (save/load) — uses `getValueTree()` for XML export, `replaceState()` for import
- `instanceUuid`, `pwdValue`
- Domain getters/setters — refactored to use `getRawParameterValue()` for atomic params, direct VT access for message-thread-only properties

**String parameters** (`type="string"` in AppParameters.xml): stay as direct VT properties. No atom, no TextSlot. Message-thread-only — atomic transport unnecessary. `Parameter::Layout` handles this: string type → `setProperty()` on VT node directly, no AnyMap entry.

**Float parameters** (`type="float"` in AppParameters.xml): become `Atom<float>` via generic template. `std::atomic<float>` is 4 bytes, lock-free. Same flush path as `Atom<int>`.

**Validation:** Compiles. No AppLayout. AppParameters.xml produces same VT structure. All property access works. Timer idles (no cross-thread writes → nothing dirty → backs off to idle Hz).

### Step 7: Cleanup

**Scope:** Across Terminal::State callers
**Action:**
- Verify all Terminal::State callers use base API (`getRawParameterValue`, `markDirty`, etc.)
- Remove any leftover Terminal::Atom references
- Remove Terminal::TextBuffer references (now jam::TextBuffer)
- Verify flush ordering in Terminal::State override matches current StateFlush.cpp behavior

**Validation:** Full compile. No Terminal::Atom, no Terminal::TextBuffer, no AppLayout. Both State classes inherit jam::ValueTree.

## BLESSED Alignment

- **B (Bound):** jam::ValueTree owns VT + AnyMap + Timer. Each Atom owns its VT node ref. Session owns TextBuffer. RAII throughout.
- **L (Lean):** One generic `Atom<T>` replaces explicit specializations. One `Parameter::Layout` replaces two Layout classes. XML schema replaces manual registration. AppLayout eliminated.
- **E (Explicit):** `loadOrder()`/`storeOrder()` — memory ordering visible at call site. `static_assert` enforces `trivially_copyable + is_always_lock_free`. XML is the readable schema. No magic strings.
- **S (SSOT):** jam::ValueTree IS the state machine. XML for schema. ValueTree for runtime. One AnyMap. Atom is the sole transport — no shadow state.
- **S (Stateless):** Parser writes to Grid and TextBuffer. Both transient buffers. jam::ValueTree is the model, not machinery.
- **E (Encapsulation):** jam::ValueTree is self-managed — builds from XML, flushes on timer, adapts cadence. Derived classes add domain concern only. Atom flushes itself (no-arg).
- **D (Deterministic):** Same XML → same state tree. One flush loop. Each Atom writes to its own VT node. Configurable cadence, deterministic behavior. `flush()` virtual for domain-specific ordering.

## Locked Decisions

1. **jam::ValueTree is the base** — reshape the existing class. Remove Value::Listener, add atomic infrastructure.
2. **Generic `Atom<T>`** — one template, constrained by `trivially_copyable + is_always_lock_free`. No explicit specializations. `loadOrder()`/`storeOrder()` split for pointer vs scalar.
3. **`Parameter::Layout(xml)`** — nested static struct on jam::ValueTree. Kuassa pattern. Caller shapes XML, base executes. Boolean resolver deduplicated.
4. **Direct `juce::ValueTree` member** — no `unique_ptr` indirection.
5. **Private Timer inheritance** — APVTS pattern. Configurable cadence (default 60/120 Hz). Adaptive.
6. **TextBuffer to jam** — `jam_core/concurrency/`, alongside Mailbox and SnapshotBuffer.
7. **AppLayout eliminated** — AppState calls `Parameter::Layout` directly.
8. **Terminal::Layout shrinks** — SCREEN expansion only, then delegates to `Parameter::Layout`.
9. **`flush()` virtual** — base provides default (all groups). Terminal::State overrides for MODES → screens → SESSION ordering + displayName.
10. **Unused atomics cost zero** — AppState uses full Atom infrastructure for int/float/bool params. Timer idles when nothing dirty. No runtime cost.
11. **String PARAM = direct VT property** — message-thread-only strings need no atomic transport. TEXT = cross-thread string needing `Atom<const char*>` + TextSlot.
12. **`addGroup()` API** — base provides group creation (nested AnyMap + child VT node). Layout calls it. Terminal SCREEN expansion calls it for NORMAL/ALTERNATE.
