# PLAN: Extract Shared Parameter Store to jam

**RFC:** RFC-state-refactor.md (updated understanding)
**Date:** 2026-05-11
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides)

## Overview
Extract the APVTS-pattern infrastructure (Atom, AnyMap, addParameter, getRawParameterValue, flush, Timer) from Terminal::State into a jam_data_structures base class `jam::ValueTreeState`. Both Terminal::State and AppState inherit it. AppState gains the full pattern verbatim -- PARAM child nodes, Atom<int> adapters, flush timer. Shadow members eliminated.

**Supersedes:** PLAN-appstate-xml.md (wrong approach -- direct VT properties, no Atoms). Current AppLayout.h/cpp will be rewritten.

## Language / Framework Constraints
C++ / JUCE reference implementation. No LANGUAGE.md overrides.

## Validation Gate
Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Move Atom to jam_core
**Scope:** `jam/jam_data_structures/state/jam_atom.h` (new), `Source/terminal/data/Atom.h` (becomes thin include)
**Action:**
- Create `jam_data_structures/state/` directory
- Move `AtomBase` and `Atom<int>` to `jam/jam_data_structures/state/jam_atom.h` under `namespace jam`
- Fix the only Terminal coupling: `Atom<int>::flush()` line 34 uses `Terminal::ID::value` -- replace with `jam::ID::value` (already exists at `jam_identifier_parameters.h:18`)
- `Atom<const char*>` stays at Terminal level (cross-thread text transport, Terminal-only)
- Terminal's `Atom.h` becomes `#include <JuceHeader.h>` + the `Atom<const char*>` specialization only
- Add `#include "state/jam_atom.h"` to `jam_data_structures/jam_data_structures.h`
**Validation:** Compiles. Terminal::State unchanged behavior. `jam::AtomBase` and `jam::Atom<int>` accessible from any jam consumer.

### Step 2: Create jam::ValueTreeState base class
**Scope:** `jam/jam_data_structures/state/jam_value_tree_state.h` (new), `jam/jam_data_structures/state/jam_value_tree_state.cpp` (new)
**Action:** Base class providing APVTS-pattern infrastructure:
- Inherits `juce::Timer`
- Owns `mutable jam::AnyMap params` (nested groups)
- Owns `juce::ValueTree state`
- Owns `std::function<void()> onFlush`
- `addParameter(id, defaultValue, targetMap, parentNode)` -- creates PARAM child VT node + `jam::Atom<int>`, returns `std::atomic<int>*`. Identical to current Terminal::State::addParameter.
- `getRawParameterValue(groupId, paramId)` -- generic group+param lookup. Returns `std::atomic<int>*`.
- `flushGroup(groupId)` -- helper: `params.get<AnyMap>(groupId)->forEach<AtomBase>(flush)`
- `getValueTree()` -- returns state ref
- `virtual bool flush() noexcept = 0` -- derived classes implement ordering
- `timerCallback()` -- calls `flush()`, adaptive 60/120 Hz, calls `onFlush`
- Constructor: `ValueTreeState(juce::Identifier rootType)` -- initializes `state(rootType)`
- Destructor: stops timer
**Validation:** Compiles. No Terminal dependencies. Pure jam-level utility.

### Step 3: Move layout utilities to jam_core
**Scope:** `jam/jam_data_structures/state/jam_layout_utils.h` (new)
**Action:**
- Move `resolveDefault()` (type-string to int resolver via lookup) to `jam::LayoutUtils::resolveDefault`
- Move `Boolean` map (`jam::Map::Instance<Boolean>` for "true"/"false" resolution) to `jam::LayoutUtils::Boolean`
- Uses `jam::ID::value` and new schema identifiers at jam level (if needed -- or keep using string literals for XML attribute access like "type", "default", "id" since these are XML-domain, not VT-domain)
- Terminal::Layout and AppLayout both use these shared utilities
**Validation:** Compiles. Terminal::Layout::resolveDefault replaced by jam utility.

### Step 4: Refactor Terminal::State to inherit jam::ValueTreeState
**Scope:** `Source/terminal/data/State.h`, `Source/terminal/data/State.cpp`, `Source/terminal/data/StateFlush.cpp`
**Action:**
- `Terminal::State : public jam::ValueTreeState` (was: `public juce::Timer`)
- Remove members now in base: `params`, `state`, `onFlush`, Timer inheritance
- Remove methods now in base: `addParameter`, `timerCallback`
- Keep Terminal-specific: `addTextParameter`, `getRawParameterValue(id)` (SESSION convenience), `getRawParameterValue(screen, id)` (screen convenience), `getModeParameterValue(id)`, `storeAndFlush`, all domain setters/getters, keyboard stack, textBuffer ref
- Terminal convenience `getRawParameterValue(id)` delegates to base `getRawParameterValue(ID::SESSION, id)`
- `flush()` override: implements Terminal ordering (MODES -> screens -> SESSION -> displayName)
- Constructor: `State(TextBuffer& tb) : ValueTreeState(ID::SESSION), textBuffer(tb) { ... }`
**Validation:** Compiles. All existing Terminal behavior unchanged. All callers of State API unchanged.

### Step 5: Rewrite AppState with jam::ValueTreeState
**Scope:** `Source/AppState.h`, `Source/AppState.cpp`, `Source/AppLayout.h` (rewrite), `Source/AppLayout.cpp` (rewrite), `Source/AppParameters.xml` (update)
**Action:**
- `AppState : public jam::ValueTreeState, public jam::Context<AppState>`
- AppParameters.xml: int/bool params use PARAM elements (same schema as Terminal). Float and string properties declared separately (direct VT attributes, message-thread only, no Atom).
- int/bool params via addParameter -> Atom<int>: `port`, `active`, `modalType`, `selectionType`, `gpuAvailable`, `daemonMode`, `atlasDirty`
- float/string properties stay as direct VT attributes: `width`, `height`, `zoom`, `fontSize`, `fontFamily`, `renderer`, `position`, `activePaneID`, `activePaneType`, `instanceUuid`
- AppLayout::build() calls addParameter for int/bool, setProperty for float/string
- `flush()` override: flushes WINDOW group then TABS group
- Constructor: `AppState() : ValueTreeState(App::ID::END) { ... }`
- Overlay Lua config after XML build (width, height, zoom, fontFamily, fontSize, position)
- Eliminate shadow members: `atlasDirty` -> Atom<int> parameter. `instanceUuid` -> direct VT string property. `pwdValue` -> direct VT string property.
- `markAtlasDirty()` / `consumeAtlasDirty()` -> use getRawParameterValue
- `startTimerHz(60)` at end of constructor
**Validation:** Compiles. All existing AppState callers unchanged. Getters return same values.

### Step 6: Update AppState serialization for PARAM-child VT structure
**Scope:** `Source/AppState.cpp` (save/load/saveWindowState/loadWindowState)
**Action:**
- `save()` -- works as-is (writes full VT tree including PARAM children)
- `load()` -- update to walk parsed PARAM children: for each PARAM in parsed WINDOW/TABS, find matching PARAM child in current tree by id, update its `value` property. Float/string properties loaded as direct attributes (unchanged).
- `loadWindowState()` -- same PARAM-child update for width/height
- `saveWindowState()` -- works as-is
**Validation:** Round-trip: save -> load produces identical VT state. Existing `.display` files silently ignored (parse failure falls back to XML defaults -- acceptable, files are ephemeral).

### Step 7: Final audit -- build + comprehensive doxygen
**Scope:** All new jam_data_structures/state/ files, all modified Terminal and App files
**Action:**
- Full build must compile clean (both END and jam module)
- Comprehensive doxygen for every new jam module file:
  - `jam_atom.h` -- file-level, AtomBase, Atom<int> class, every public method
  - `jam_value_tree_state.h/cpp` -- file-level, ValueTreeState class, every public/protected method, member documentation, thread-safety notes, usage example in class doc
  - `jam_layout_utils.h` -- file-level, LayoutUtils struct, resolveDefault, Boolean
- Verify doxygen on modified files: Terminal's Atom.h (now Atom<const char*> only), State.h (inheritance change), AppState.h (inheritance change), AppLayout.h/cpp
- @Auditor comprehensive pass: BLESSED, NAMES.md, JRENG-CODING-STANDARD, plan compliance, dead code, stale docs
**Validation:** Build compiles. All new jam modules have complete doxygen. Auditor reports zero findings.

## BLESSED Alignment
- **B (Bound):** ValueTreeState owns AnyMap + VT + Timer. Clear lifecycle: construction builds, destruction stops timer.
- **L (Lean):** Shared base eliminates duplication. Two consumers, one infrastructure.
- **E (Explicit):** XML schema is the visible declaration. Atom API encodes memory order.
- **S (SSOT):** addParameter is the single creation path. ValueTreeState is the single infrastructure. No shadow state on AppState.
- **S (Stateless):** Layout walkers are pure functions. Atoms are dumb adapters.
- **E (Encapsulation):** ValueTreeState knows nothing about Terminal or App domains. Each derived class implements its own flush ordering and domain logic.
- **D (Deterministic):** Same XML + same Lua config = same tree, always.

## Risks / Open Questions
- **Naming:** `jam::ValueTreeState` for the base class. `jam_data_structures/state/` for the directory. Approve?
- **Float/string split:** Int/bool go through Atom<int>; float/string stay as direct VT properties. This is because Atom<int> stores int, and AppState has genuine float values (zoom, fontSize). No forced conversion.
