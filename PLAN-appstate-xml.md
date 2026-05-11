# PLAN: AppState XML-Driven Defaults

**RFC:** none -- objective from ARCHITECT prompt
**Date:** 2026-05-11
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides)

## Overview
Replace AppState::initDefaults() hand-wired property setup with declarative XML schema. Walk XML at construction to build ValueTree structure, then overlay Lua config runtime defaults. Follows Terminal::Layout::build() pattern -- XML declares structure + types + defaults, walker sets VT properties.

Key difference from Terminal::State: AppState is message-thread only. No Atoms, no AnyMap, no flush. Properties are direct VT attributes (not PARAM child nodes). The PARAM element convention is shared for XML schema consistency and type resolution.

## Validation Gate
Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Add appMetadata constant to App::ID
**Scope:** `Source/AppIdentifier.h`
**Action:** Add `static const juce::String appMetadata { "AppParameters.xml" };` to `App::ID` namespace (alongside existing `rendererGpu`/`rendererCpu` string constants).
**Validation:** Compiles. Identifier follows existing naming pattern.

### Step 2: Create AppParameters.xml schema
**Scope:** `Source/AppParameters.xml` (new)
**Action:** Declare full WINDOW + TABS property schema using PARAM elements with id/type/default attributes. Root `<END>` mirrors VT root type. Types: int, float, bool, string. Properties declared:
- Root: `port` (int, 0)
- WINDOW: `width` (int, 0), `height` (int, 0), `zoom` (float, 1.0), `fontFamily` (string, ""), `fontSize` (float, 0), `renderer` (string, "gpu"), `gpuAvailable` (bool, false), `daemonMode` (bool, false)
- TABS: `active` (int, 0), `position` (string, ""), `activePaneID` (string, ""), `activePaneType` (string, "terminal"), `modalType` (int, 0), `selectionType` (int, 0)

Auto-discovered by CMakeLists.txt glob (`Source/*.xml`). No CMake changes needed.
**Validation:** Valid XML. All properties match existing App::ID constants. Types match existing getter/setter cast expectations.

### Step 3: Create AppLayout with static build()
**Scope:** `Source/AppLayout.h`, `Source/AppLayout.cpp` (new)
**Action:** `struct AppLayout` with `static void build (const juce::XmlElement& xml, juce::ValueTree& state)`. Walks XML:
- Creates child VT nodes per group tag (WINDOW, TABS)
- For each PARAM element: resolves default by type, calls `node.setProperty(id, resolvedValue, nullptr)`
- Root-level PARAMs set on state directly
- Type resolution via lookup (not branches) returning `juce::var` -- handles int/float/bool/string. Boolean map uses `jam::Map::Instance<Boolean>` pattern (same as Terminal::Layout).
**Validation:** Compiles. BLESSED: L (type lookup, not branch chain), S (SSOT schema), E (explicit types in XML).

### Step 4: Refactor initDefaults() to use AppLayout::build()
**Scope:** `Source/AppState.cpp`
**Action:**
- `initDefaults()` calls `jam::XML::getFromBinary(App::ID::appMetadata)` then `AppLayout::build(*xml, state)`
- After build: overlay 4 Lua config values onto VT (width, height from `cfg->display.window`, zoom from `lua::Engine::zoomMin`, position from `cfg->display.tab.position`)
- Remove all hand-wired ValueTree/setProperty construction
**Validation:** Compiles. Existing save/load behavior unchanged. VT structure identical to before (verify with debugger or VT dump). All getters return same values as before.

## BLESSED Alignment
- **B (Bound):** AppLayout::build scope -- creates and populates, no dangling refs
- **L (Lean):** Type resolution via lookup, not 4-branch chain. AppLayout is minimal.
- **E (Explicit):** Schema in XML declares every property, type, and default visibly
- **S (SSOT):** XML is single schema declaration. No duplicate property lists.
- **S (Stateless):** AppLayout is pure function -- walks input, writes output
- **E (Encapsulation):** AppLayout knows only XML and VT, not AppState internals
- **D (Deterministic):** Same XML always produces same VT structure

## Risks / Open Questions
None. Scope confirmed by ARCHITECT: XML-driven defaults, message-thread only, no Atoms/AnyMap/flush.
