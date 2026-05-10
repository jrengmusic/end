# RFC — State Refactor: Declarative XML-Driven Typed Atomics

Date: 2026-05-10
Status: Ready for COUNSELOR handoff

## Problem Statement

`Terminal::State` is a manually-wired APVTS-style atomic store that has grown unwieldy. Adding a parameter requires touching 3–4 files. All values are forced through `std::atomic<float>` with pervasive `static_cast<float>()` on write and `toInt()`/`toBool()` on read. String transport uses a hand-rolled seqlock (`StringSlot`) with methods scattered across State. A 16.5 MB pre-allocated `linkUriTable[65536]` exists for a half-implemented hyperlink system. The `memory_order_relaxed` qualifier is repeated at every atomic access site despite being a system-invariant.

The goal: make State analogous to APVTS — declare a layout, get automatic typed atomic storage, ValueTree skeleton, and flush path. Single modification surface. Zero boilerplate.

## Research Summary

### juce::var Thread Safety (Source: JUCE 8.0.12)

`juce::var` is **not thread-safe**. Three non-atomic stores on write (`type->cleanUp`, `type = v.type`, `type->createCopy` — `juce_Variant.cpp:499`). `juce::String` uses COW with `std::atomic<int> refCount` on `StringHolder`, but the `CharPointerType text` field itself is not atomic for concurrent object access (`juce_String.cpp:271–275`). `ValueTree::setProperty` has no internal lock — `NamedValueSet::set` does `*v = newValue` directly (`juce_ValueTree.cpp:139–161`).

**Conclusion:** Cross-thread string transport cannot use `juce::var`, `juce::String`, or `ValueTree::setProperty` without external synchronization.

### std::atomic String Transport (C++17 Constraints)

`std::atomic<T>` requires trivially copyable `T`. `std::string` and `juce::String` are not trivially copyable. `std::atomic<std::shared_ptr<std::string>>` is C++20-only and not lock-free on any implementation (GCC: libatomic mutex; MSVC: LSB spinlock; libc++: not implemented as of May 2026).

Maximum lock-free atomic size across all three targets (macOS/Apple Clang, Windows/MSVC, Linux/GCC): **8 bytes** (`int64_t`/`uint64_t`). 16-byte atomics are not portable — MSVC lacks `__int128`, GCC never emits lock-free 128-bit ops.

**Solution:** `std::atomic<const char*>` — 8-byte pointer swap, universally lock-free. The pointed-to buffer is double-buffered and owned by the writer's context (Session-owned `TextBuffer`), same pattern as Grid. No custom seqlock type needed.

### APVTS Pattern (Source: juce_AudioProcessorValueTreeState.cpp)

APVTS declares a `ParameterLayout` — list of typed descriptors. Constructor consumes layout, creates a `ParameterAdapter` per parameter (owns `std::atomic<float>`), builds ValueTree skeleton, and provides `getRawParameterValue(id)` returning a stable `std::atomic<float>*`. Audio thread reads lock-free. Timer flushes to ValueTree. UI reads ValueTree.

### Kuassa XML-Driven Layout (Source: kuassa_parameter_layout.cpp)

`Parameters.xml` (BinaryData) declares all parameters — type, default, group. `Layout::get(xml)` parses at construction time, builds `ParameterLayout`, constructs APVTS. Adding a parameter = one XML line. Zero C++ changes. Battle-tested across the full Kuassa plugin lineup.

### Kuassa Page Selector Pattern (Source: kuassa_parameter_page.h)

`map::Page` declares an indexed parameter group count. `ParameterPage::attach()` duplicates the parameter set per page. Access by index, not compound key. Applies directly to NORMAL/ALTERNATE screen duplication.

### Terminal State — No Floats

Every value in `State` is semantically `int` or `bool`. Cursor row/col: int. Mode flags: bool. Cursor shape: int (0–6). Cursor color: int (0–255). Blink interval: int (ms). Selection coords: int. Signals: bool. The `float` storage was inherited from APVTS where everything is a normalized float. Terminal state has no normalized parameters, no continuous ranges, no float-domain values.

### jam::AnyMap (Source: jam_any_map.h)

Type-erased heterogeneous map. `add<T>(key, args...)` constructs and stores any type. `get<T>(key)` retrieves with runtime type assertion. String-keyed (`juce::String`). Already exists in `jam_core/utilities/`.

## Principles and Rationale

### BLESSED Compliance

**B (Bound):** Clear ownership chain. Session owns Grid and TextBuffer (write buffers). Processor owns State (atomic map + ValueTree + flush). Parser writes to buffers it doesn't own — same as audio DSP processor writing to AudioBuffer.

**L (Lean):** ~50 manual `addParam()` calls replaced by XML declaration. `buildParameterMap()`, `buildParamKey()`, `screenKey()`, `modeKey()` eliminated. Separate flush paths (`flushRootParams`, `flushGroupParams`, `flushStrings`) unified into one `forEach` + `flush()`.

**E (Explicit):** XML is the single readable declaration of the entire parameter schema. `Atom::load()`/`store()` encode the relaxed memory order invariant — no hidden `memory_order_relaxed` at callsites. `AtomBase::flush()` — each slot writes itself to ValueTree. Tell, don't ask.

**S (SSOT):** `State.xml` is the single source of truth for the parameter schema. ValueTree remains SSOT for UI-visible state. No parallel maps, no separate string map, no flat array alongside the map.

**S (Stateless):** Parser is a dumb processor. Writes to Grid and TextBuffer — transient buffers, not state machines. Never allocates. TextBuffer is working memory, same as Grid's HeapBlock.

**E (Encapsulation):** Each `Atom` flushes itself via virtual `flush()`. State builds itself from XML via static function. No external manager class. TextBuffer manages its own double-buffer swap — callers just call `write()`.

**D (Deterministic):** One `needsFlush` signal. One flush loop. Same path for all types. No type-dependent dispatch logic outside the slot.

### Why Not Float

APVTS forces `float` because audio parameters are continuous. Terminal state is discrete — integers and booleans only. `std::atomic<int>` eliminates all `static_cast<float>()` on write and `toInt()`/`toBool()` on read. Bool is 0/1 as int — no separate type.

### Why const char* Not Custom Seqlock

`std::atomic<const char*>` is a native type. 8 bytes. Lock-free. Fits in `AnyMap` alongside `std::atomic<int>`. The double-buffer lives in `TextBuffer` (Session-owned), same ownership pattern as Grid. No custom atomic type, no generation counter, no retry loop. The READER thread writes to the inactive buffer, atomically publishes the pointer. Flush loads the pointer (acquire), reads completed string (release guarantees visibility), writes to ValueTree.

### Why XML Not Code

Same rationale as Kuassa: adding a parameter should not require C++ code changes. The XML is embedded as BinaryData (compile-time fixed schema). The builder is a straight walk — each element maps 1:1 to a ValueTree node and an AnyMap entry. No magic expansion except `SCREEN` duplication (explicit in XML for both NORMAL and ALTERNATE).

### Why Indexed Screens Not Compound Keys

Compound keys (`"NORMAL_cursorRow"`) flatten a real hierarchy with string hacks. `buildParamKey()`, `screenKey()`, `modeKey()` are all key-construction boilerplate. The Kuassa Page Selector pattern proves indexed groups: `screens[s].get<Atom<int>>(ID::cursorRow)`. Clean access, no string concatenation, mirrors the ValueTree structure.

## Scaffold

### Pre-requisites (jam-level, must land first)

#### 1. std::hash<juce::Identifier>

Inject at `jam_core` level. Hash the interned string pointer for O(1).

```cpp
// jam_core — e.g. jam_core/utilities/jam_identifier_hash.h
namespace std
{
template<>
struct hash<juce::Identifier>
{
    size_t operator() (const juce::Identifier& id) const noexcept
    {
        return std::hash<const void*>{} (id.getCharPointer().getAddress());
    }
};
}
```

#### 2. jam::AnyMap — Identifier internals + forEach

Change internal storage key from `juce::String` to `juce::Identifier`. Add `juce::String` overloads that auto-intern. Add `forEach` for flush iteration.

```cpp
class AnyMap
{
public:
    // Existing API — now with Identifier key
    template<typename AnyType, typename... Args>
    AnyType* add (const juce::Identifier& key, Args&&... args);

    template<typename AnyType>
    AnyType* get (const juce::Identifier& key);

    template<typename AnyType>
    const AnyType* get (const juce::Identifier& key) const;

    // String overloads — auto-intern
    template<typename AnyType, typename... Args>
    AnyType* add (const juce::String& key, Args&&... args)
    {
        return add<AnyType> (juce::Identifier { key }, std::forward<Args> (args)...);
    }

    template<typename AnyType>
    AnyType* get (const juce::String& key) { return get<AnyType> (juce::Identifier { key }); }

    // Iteration — type-erased callback
    template<typename Base>
    void forEach (std::function<void (const juce::Identifier&, Base&)> callback);

    bool contains (const juce::Identifier& key) const noexcept;
    bool contains (const juce::String& key) const noexcept;
    size_t size() const noexcept;
    void clear() noexcept;

private:
    using Deleter = void (*) (void*);

    template<typename T>
    static void deleter (void* p) { delete static_cast<T*> (p); }

    std::unordered_map<juce::Identifier, std::unique_ptr<void, Deleter>> entries;
    std::unordered_map<juce::Identifier, std::type_index> types;
};
```

#### 3. jam::Function::Map — same treatment

Internal key from `String` to `Identifier`. `String` overloads preserved.

### State Refactor (END-level)

#### State.xml — Parameter Schema

```xml
<STATE>
    <!-- Root-level scalars -->
    <PARAM id="activeScreen"       type="int"  default="0"/>
    <PARAM id="scrollbackUsed"     type="int"  default="0"/>
    <PARAM id="hintPage"           type="int"  default="0"/>
    <PARAM id="hintTotalPages"     type="int"  default="0"/>
    <PARAM id="scrollPosition"     type="int"  default="0"/>
    <PARAM id="selectionCursorRow" type="int"  default="0"/>
    <PARAM id="selectionCursorCol" type="int"  default="0"/>
    <PARAM id="selectionAnchorRow" type="int"  default="0"/>
    <PARAM id="selectionAnchorCol" type="int"  default="0"/>
    <PARAM id="dragAnchorRow"      type="int"  default="0"/>
    <PARAM id="dragAnchorCol"      type="int"  default="0"/>
    <PARAM id="dragActive"         type="bool" default="false"/>

    <!-- Transient signals -->
    <PARAM id="needsFlush"         type="bool" default="false"/>
    <PARAM id="snapshotDirty"      type="bool" default="false"/>
    <PARAM id="fullRebuild"        type="bool" default="false"/>
    <PARAM id="pasteEchoRemaining" type="int"  default="0"/>
    <PARAM id="syncOutputActive"   type="bool" default="false"/>
    <PARAM id="syncResizePending"  type="bool" default="false"/>

    <!-- OSC 133 output block tracking -->
    <PARAM id="outputBlockTop"     type="int"  default="-1"/>
    <PARAM id="outputBlockBottom"  type="int"  default="-1"/>
    <PARAM id="outputScanActive"   type="bool" default="false"/>
    <PARAM id="promptRow"          type="int"  default="-1"/>

    <!-- Image erase bounding box -->
    <PARAM id="eraseTop"           type="int"  default="999999"/>
    <PARAM id="eraseLeft"          type="int"  default="999999"/>
    <PARAM id="eraseBottom"        type="int"  default="-1"/>
    <PARAM id="eraseRight"         type="int"  default="-1"/>

    <!-- Cursor blink -->
    <PARAM id="cursorBlinkOn"       type="bool" default="true"/>
    <PARAM id="cursorBlinkElapsed"  type="int"  default="0"/>
    <PARAM id="prevFlushedCursorRow" type="int" default="0"/>
    <PARAM id="prevFlushedCursorCol" type="int" default="0"/>
    <PARAM id="cursorBlinkInterval" type="int"  default="500"/>
    <PARAM id="cursorBlinkEnabled"  type="bool" default="true"/>
    <PARAM id="cursorFocused"       type="bool" default="false"/>

    <!-- Link URI counter -->
    <PARAM id="linkUriCount"       type="int"  default="0"/>

    <!-- Terminal mode flags -->
    <MODES>
        <PARAM id="originMode"         type="bool" default="false"/>
        <PARAM id="autoWrap"           type="bool" default="true"/>
        <PARAM id="applicationCursor"  type="bool" default="false"/>
        <PARAM id="bracketedPaste"     type="bool" default="false"/>
        <PARAM id="insertMode"         type="bool" default="false"/>
        <PARAM id="mouseTracking"      type="bool" default="false"/>
        <PARAM id="mouseMotionTracking" type="bool" default="false"/>
        <PARAM id="mouseAllTracking"   type="bool" default="false"/>
        <PARAM id="mouseSgr"           type="bool" default="false"/>
        <PARAM id="focusEvents"        type="bool" default="false"/>
        <PARAM id="applicationKeypad"  type="bool" default="false"/>
        <PARAM id="reverseVideo"       type="bool" default="false"/>
        <PARAM id="win32InputMode"     type="bool" default="false"/>
    </MODES>

    <!-- Per-screen params (identical schema, two instances) -->
    <SCREEN id="NORMAL">
        <PARAM id="cursorRow"      type="int"  default="0"/>
        <PARAM id="cursorCol"      type="int"  default="0"/>
        <PARAM id="cursorVisible"  type="bool" default="true"/>
        <PARAM id="wrapPending"    type="bool" default="false"/>
        <PARAM id="scrollTop"      type="int"  default="0"/>
        <PARAM id="scrollBottom"   type="int"  default="0"/>
        <PARAM id="cursorShape"    type="int"  default="0"/>
        <PARAM id="cursorColorR"   type="int"  default="-1"/>
        <PARAM id="cursorColorG"   type="int"  default="-1"/>
        <PARAM id="cursorColorB"   type="int"  default="-1"/>
        <PARAM id="keyboardFlags"  type="int"  default="0"/>
    </SCREEN>

    <SCREEN id="ALTERNATE">
        <PARAM id="cursorRow"      type="int"  default="0"/>
        <PARAM id="cursorCol"      type="int"  default="0"/>
        <PARAM id="cursorVisible"  type="bool" default="true"/>
        <PARAM id="wrapPending"    type="bool" default="false"/>
        <PARAM id="scrollTop"      type="int"  default="0"/>
        <PARAM id="scrollBottom"   type="int"  default="0"/>
        <PARAM id="cursorShape"    type="int"  default="0"/>
        <PARAM id="cursorColorR"   type="int"  default="-1"/>
        <PARAM id="cursorColorG"   type="int"  default="-1"/>
        <PARAM id="cursorColorB"   type="int"  default="-1"/>
        <PARAM id="keyboardFlags"  type="int"  default="0"/>
    </SCREEN>

    <!-- Cross-thread strings -->
    <TEXT id="title"              maxlen="256"/>
    <TEXT id="cwd"                maxlen="4096"/>
    <TEXT id="foregroundProcess"  maxlen="256"/>
</STATE>
```

#### Atom — Typed Atomic Wrapper

```cpp
// AtomBase — virtual base for type-erased flush
struct AtomBase
{
    virtual ~AtomBase() = default;
    virtual void flush (juce::ValueTree& node, const juce::Identifier& property) noexcept = 0;
};

// Atom<int> — scalar transport (int and bool unified)
template<>
struct Atom<int> final : AtomBase
{
    explicit Atom (int defaultValue) noexcept : value { defaultValue } {}

    int  load()  const noexcept { return value.load (std::memory_order_relaxed); }
    void store (int v) noexcept { value.store (v, std::memory_order_relaxed); }

    // Explicit-order access for exception callsites (snapshotDirty, pasteEchoRemaining, etc.)
    std::atomic<int>& raw() noexcept { return value; }

    void flush (juce::ValueTree& node, const juce::Identifier& property) noexcept override
    {
        node.setProperty (property, value.load (std::memory_order_relaxed), nullptr);
    }

private:
    std::atomic<int> value;
};

// Atom<const char*> — text transport (pointer into TextBuffer double-buffer)
template<>
struct Atom<const char*> final : AtomBase
{
    const char* load() const noexcept { return ptr.load (std::memory_order_acquire); }
    void store (const char* p) noexcept { ptr.store (p, std::memory_order_release); }

    void flush (juce::ValueTree& node, const juce::Identifier& property) noexcept override
    {
        const char* p = ptr.load (std::memory_order_acquire);

        if (p != nullptr)
            node.setProperty (property, juce::String::fromUTF8 (p), nullptr);
    }

private:
    std::atomic<const char*> ptr { nullptr };
};
```

#### TextBuffer — Session-Owned Double-Buffered String Storage

```cpp
// One text slot: two buffers, atomic index
struct TextSlot
{
    TextSlot (int maxlen) : bufferSize (maxlen)
    {
        buffers[0].allocate (maxlen, true);
        buffers[1].allocate (maxlen, true);
    }

    // READER THREAD — write to inactive buffer, publish pointer
    const char* write (const char* src, int length) noexcept
    {
        const int next = 1 - active.load (std::memory_order_relaxed);
        const int len  = juce::jmin (length, bufferSize - 1);

        std::memcpy (buffers[next].getData(), src, static_cast<size_t> (len));
        buffers[next][len] = '\0';
        active.store (next, std::memory_order_release);

        return static_cast<const char*> (buffers[next].getData());
    }

private:
    juce::HeapBlock<char> buffers[2];
    std::atomic<int> active { 0 };
    int bufferSize;
};

// TextBuffer — collection of named text slots, sized from XML
struct TextBuffer
{
    // Constructed from XML <TEXT> elements
    void addSlot (const juce::Identifier& id, int maxlen);

    // READER THREAD — returns published const char* for atomic store
    const char* write (const juce::Identifier& id, const char* src, int length) noexcept;

private:
    std::unordered_map<juce::Identifier, std::unique_ptr<TextSlot>> slots;
};
```

#### State — Self-Built from XML

```cpp
struct State : public juce::Timer
{
    // Static builder reads XML, constructs State
    State (const juce::XmlElement& xml, TextBuffer& textBuffer);

    // Unified typed access — no compound keys
    jam::AnyMap root;               // root-level params
    jam::AnyMap modes;              // terminal mode flags
    jam::AnyMap screens[2];         // indexed by ActiveScreen (0=normal, 1=alternate)

    // Single write path — READER THREAD
    void storeAndFlush (const juce::Identifier& key, int value) noexcept
    {
        root.get<Atom<int>> (key)->store (value);
        root.get<Atom<int>> (ID::needsFlush)->raw().store (1, std::memory_order_release);
    }

    // Flush — one loop, all types
    // forEach calls AtomBase::flush() on every entry
    bool flush() noexcept;

    // ValueTree — SSOT for UI
    juce::ValueTree state;

private:
    void timerCallback() override;
    TextBuffer& textBuffer;
};
```

#### Construction Flow

```
State.xml (BinaryData)
  → juce::parseXML()
  → State constructor walks XML:
      <PARAM type="int">   → root.add<Atom<int>>(id, default)
                           → addParam to ValueTree
      <PARAM type="bool">  → root.add<Atom<int>>(id, default ? 1 : 0)
                           → addParam to ValueTree
      <MODES>              → modes.add<Atom<int>>(id, ...) per child
                           → MODES child in ValueTree
      <SCREEN id="NORMAL"> → screens[0].add<Atom<int>>(id, ...) per child
                           → NORMAL child in ValueTree
      <SCREEN id="ALTERNATE"> → screens[1].add<...>
      <TEXT>               → root.add<Atom<const char*>>(id)
                           → (TextBuffer already sized from same XML by Session)
  → startTimerHz(60)
  → done
```

#### Access Patterns (Before → After)

```cpp
// BEFORE: write
storeAndFlush (screenKey (s, ID::cursorRow), static_cast<float> (row));

// AFTER: write
screens[s].get<Atom<int>> (ID::cursorRow)->store (row);

// BEFORE: read (reader thread)
getRawValue<int> (screenKey (s, ID::cursorRow))

// AFTER: read (reader thread)
screens[s].get<Atom<int>> (ID::cursorRow)->load()

// BEFORE: read (message thread, ValueTree)
// unchanged — still reads from ValueTree post-flush

// BEFORE: string write
writeSlot (stringMap[ID::cwd].get(), src, length);

// AFTER: string write
auto* p = textBuffer.write (ID::cwd, src, length);
root.get<Atom<const char*>> (ID::cwd)->store (p);

// BEFORE: raw atomic with explicit memory order
getRawParam (ID::snapshotDirty)->store (1.0f, std::memory_order_release);

// AFTER: raw atomic for exception callsites
root.get<Atom<int>> (ID::snapshotDirty)->raw().store (1, std::memory_order_release);
```

#### What Gets Eliminated

| Eliminated | Replacement |
|---|---|
| ~50 manual `addParam()` calls | State.xml |
| `buildParameterMap()` tree walker | XML builder in constructor |
| `parameterMap` (`StateMap<std::atomic<float>>`) | `jam::AnyMap` (root + modes + screens[2]) |
| `stringMap` (`StateMap<StringSlot>`) | Same AnyMap, `Atom<const char*>` entries |
| `linkUriTable[65536]` (16.5 MB) | Eliminated. OSC 8 URIs: dynamic `TextBuffer` slots + `Atom<const char*>` on demand. Heuristic links: purely MESSAGE thread (Screen/LinkManager), no State involvement. |
| `StringSlot` struct | `TextSlot` in TextBuffer (Session-owned) |
| `writeSlot()` / `readSlot()` / `flushSlot()` | `TextBuffer::write()` + `Atom<const char*>::flush()` |
| `buildParamKey()` / `screenKey()` / `modeKey()` | Direct map access by screen index |
| `flushRootParams()` / `flushGroupParams()` / `flushStrings()` | One `forEach` + `AtomBase::flush()` |
| `getRawValue<T>()` with `toInt`/`toBool` cast chain | `Atom<int>::load()` — typed, no cast |
| `getRawParam()->load(memory_order_relaxed)` | `Atom::load()` — order encoded |
| All `static_cast<float>(value)` on write | `Atom<int>::store(value)` — native int |

## BLESSED Compliance Checklist

- [x] Bounds — Session owns TextBuffer and Grid (write buffers). Processor owns State (atomics + ValueTree). Clear ownership chain. RAII throughout.
- [x] Lean — XML replaces ~50 lines of registration. Three separate flush paths become one. Compound key helpers eliminated.
- [x] Explicit — State.xml is the readable schema. `Atom::load()`/`store()` encode memory order. `AtomBase::flush()` — tell, don't ask.
- [x] SSOT — State.xml for schema. ValueTree for runtime state. One AnyMap structure (root + modes + screens[2]) instead of two parallel maps.
- [x] Stateless — Parser writes to Grid and TextBuffer. Both are transient buffers, not state machines. Parser never allocates.
- [x] Encapsulation — Each Atom flushes itself. State builds itself from XML. TextBuffer manages its own double-buffer swap.
- [x] Deterministic — One `needsFlush` signal. One flush loop. Same path for all types. Flush always does the same thing.

## Open Questions

1. **`cols` and `visibleRows`** — currently `CachedValue<int>` written exclusively on MESSAGE THREAD, not in the atomic pipeline. Do they move into the AnyMap as regular `Atom<int>` entries, or stay as CachedValues? They are the only MESSAGE-thread-written dimensions; the reader thread reads them from Grid under `resizeLock`, not from State.

2. **Preview state** — `preview` and `splitCol` are currently direct ValueTree properties on the SESSION root (MESSAGE THREAD only, no atomic). Do they become `PARAM` entries in XML, or stay as direct properties since they have no cross-thread transport need?

3. **Keyboard mode stack** — `keyboardModeStack` and `keyboardModeStackSize` are `HeapBlock` arrays with push/pop logic, not simple atomics. These stay as State members alongside the AnyMap, or get their own treatment?

4. **SUBSCRIBERS and IMAGES** — ValueTree container nodes with dynamic children. Not atomic parameters. Stay as direct ValueTree manipulation, or formalize in XML?

## Handoff Notes

- The jam-level pre-requisites (`std::hash<Identifier>`, AnyMap refactor, Function::Map refactor) must land before the State refactor begins. These are non-breaking changes — existing `String` callers continue to work via overloads.
- `linkUriTable[65536]` is removed in this refactor. Link URI re-implementation is a separate RFC (RFC-link-uri.md). Two link sources, two paths:
  - **OSC 8 (READER thread):** URI arrives via Parser → State atomic transport (`Atom<const char*>` + dynamic `TextBuffer` slot) → Screen reads ValueTree. Cross-thread — requires State.
  - **Heuristic scan (MESSAGE thread):** Screen scans visible cells → `LinkDetector::classify()` → `juce::File::isDirectory()`/`existsAsFile()` against cwd → `LinkSpan` → `LinkManager`. Purely MESSAGE thread — no State involvement, no atomic transport, no TextBuffer. `LinkSpan` holds `juce::String uri` directly.
  - Both paths converge at Screen → LinkManager for hit-test, hints, and dispatch.
  - Heuristic links include URLs, files with known extensions, and **directories** (new `LinkDetector::LinkType::directory`). Directory click sends `cd <path>\n` + optionally `ls\n` to PTY (config-driven via `links.list_directory`). Path resolution via `juce::File(cwd).getChildFile(token)` — shell-agnostic, cross-platform.
  - Scan trigger: Parser flags State (atomic), Display reacts in onVBlank, tells Screen to scan. Same unidirectional data flow as `snapshotDirty`.
- The `Atom<int>` wrapper stores bool as 0/1 int. The XML can declare `type="bool"` for readability — the builder maps it to `Atom<int>` with default 0 or 1.
- `Atom<const char*>::flush()` uses acquire on the pointer load. `Atom<const char*>::store()` uses release. This is the only place where non-relaxed ordering is the default — the pointer swap must establish happens-before with the buffer write. This is a property of the double-buffer pattern, not an exception to the relaxed-default rule.
- `TextBuffer::write()` returns the `const char*` that the caller stores into the `Atom`. This keeps the write path as: Parser calls TextBuffer, TextBuffer returns pointer, Parser stores pointer into Atom via State setter. Two calls, no indirection.
- The flush loop iterates all four AnyMaps (root, modes, screens[0], screens[1]) calling `forEach<AtomBase>`. Each `AtomBase::flush()` writes to the corresponding ValueTree node. The mapping from AnyMap to ValueTree node is established at construction time.
