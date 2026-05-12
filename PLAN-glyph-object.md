# PLAN: jam::Glyph Object Formalization

**RFC:** none -- objective from ARCHITECT prompt
**Date:** 2026-05-13
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)

## Overview

Restructure `namespace jam::glyph` (scattered infrastructure) into `struct jam::Glyph` (formalized object). Glyph becomes the real text rendering primitive -- pure identity value type with associated nested types (Atlas, Graphics, ShapedText). Foundation for renderer-agnostic text pipeline supporting both monospace and proportional layout.

## Validation Gate

Each step MUST be validated before proceeding.
Validation = @Auditor confirms step output complies with:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions (no deviation, no scope drift)

## Steps

### Step 1: Create jam::Glyph struct header

**Scope:** `jam_graphics/jam_glyph.h` (new file), `jam_graphics/jam_graphics.h` (include order)
**Action:** Create the Glyph struct with:
- Properties: `codepoint`, `glyphIndex`, `fontHandle`, `style`, `isEmoji`, `advanceWidth`
- `isValid()` method
- Forward declarations of all nested public types: `class Atlas`, `class Graphics`, `class ShapedText`
- Forward declarations of all nested private types: `struct Key`, `struct Region`, `struct Constraint`, `struct Box` (inside Atlas), `struct Arrangement`, `struct Run` (inside ShapedText)
- Static `getImage()` declaration
- Add `#include "jam_glyph.h"` as FIRST include in `jam_graphics.h` module header (before all existing glyph headers)
**Validation:** Header compiles. Forward declarations match existing types. No circular dependencies.

### Step 2: Migrate private internal types into Glyph

**Scope:** `fonts/jam_glyph_key.h`, `fonts/jam_atlas_glyph.h`, `fonts/jam_glyph_constraint.h`, `fonts/jam_constraint_transform.h`, `fonts/jam_box_drawing.h`, `fonts/jam_atlas_packer.h`, `fonts/jam_lru_glyph_cache.h`, `rendering/jam_glyph_render.h`
**Action:** Change each file from `namespace jam::glyph { ... }` to defining its type as a nested type inside `jam::Glyph`. These types are internal (Key, Region, Constraint, ConstraintTransform, BoxDrawing, AtlasPacker, LRUCache, Render). Rename per approved design:
- `Key` -> `Glyph::Atlas::Key`
- `Region` -> `Glyph::Atlas::Region`
- `Constraint` -> `Glyph::Constraint` (private, owns `getConstraint`; accessed by Atlas, Graphics, ShapedText)
- `BoxDrawing` -> `Glyph::Atlas::Box`
- `AtlasPacker`, `LRUCache`, `Render` -> internal to their parent types
- `PositionedGlyph` -> `Glyph::ShapedText::Arrangement`
- `GlyphDrawRun` -> `Glyph::ShapedText::Run`
Update all internal cross-references.
**Validation:** Module compiles. All internal references resolve. No `jam::glyph::` references remain in migrated files.

### Step 3: Migrate public types into Glyph

**Scope:** `glyph/jam_glyph_atlas.h`, `fonts/jam_glyph_packer.h`, `rendering/jam_glyph_graphics.h`, `rendering/jam_glyph_shaped_text.h`
**Action:** Change Atlas, Packer, Graphics, ShapedText from `namespace jam::glyph` to nested class definitions inside `jam::Glyph`. Packer is private inside Atlas. Update all implementation files (.cpp, .mm) to use `jam::Glyph::` scope.
**Validation:** Module compiles. Public API is `jam::Glyph::Atlas`, `jam::Glyph::Graphics`, `jam::Glyph::ShapedText`.

### Step 4: Update jam_gui references

**Scope:** `jam_gui/text_editor/jam_text_editor.h`, `jam_gui/text_editor/jam_text_editor.cpp`, `jam_gui/text_editor/jam_caret_component.h`
**Action:** Replace all `jam::glyph::` references with `jam::Glyph::`. Member types: `jam::Glyph::ShapedText`, `jam::Glyph::Graphics`. Method calls: `jam::Glyph::Atlas&`, etc.
**Validation:** jam_gui module compiles. TextEditor and CaretComponent functional.

### Step 5: Update END project references

**Scope:** All END `Source/` files referencing `jam::glyph::`
**Action:** Replace all `jam::glyph::` with `jam::Glyph::` across Screen.h/cpp, TerminalGLRenderer files, and any other consumers.
**Validation:** END project compiles. Terminal renders correctly.

### Step 6: Implement getImage()

**Scope:** `jam_glyph.h` (or new `jam_glyph.cpp`), `jam_caret_component.h`
**Action:** Implement `Glyph::getImage()` static method -- shapes a Cell through the Typeface, renders via push/drawGlyphs(count=1)/pop on a local Graphics instance, returns the resulting juce::Image. Wire CaretComponent to cache the image from `getImage()` and blit on each paint. Remove the broken `rasterizeCursorGlyph()` from Screen.
**Validation:** Cursor renders as configured codepoint. Blinks correctly. Emoji, NF icons, standard characters all work.

### Step 7: Cleanup

**Scope:** `jam_core/metrics/jam_glyph_metrics.h`, `jam_core/jam_core.h`, stale `glyph/` duplicates, END diagnostic instrumentation
**Action:**
- Delete `jam::metrics::Glyph` struct and its include from jam_core.h
- Delete stale duplicate files in `jam_graphics/glyph/` (jam_glyph_key.h, jam_atlas_glyph.h, jam_atlas_packer.h, jam_glyph_constraint_table.cpp)
- Remove diagnostic instrumentation (Log::Scope, frame counters, cell dumps) from Main.cpp and Screen.cpp
**Validation:** Clean build. No dead files. No diagnostic code. `jam::glyph` namespace fully eliminated.

## BLESSED Alignment

- **B (Bound):** Glyph is a value type with clear ownership. Atlas is Context-managed singleton. Graphics owned by rendering components. No ambiguous lifetimes.
- **L (Lean):** Glyph struct is minimal -- identity properties only. No god object. Associated types in separate files.
- **E (Explicit):** `advanceWidth` makes layout multiplier visible. No hidden assumptions about monospace vs proportional. All parameters visible in `getImage()` signature.
- **S (SSOT):** One Glyph type. One rendering path (`getImage()` for single, Graphics for batch). No duplicate cell metrics scattered across Screen/TextEditor.
- **S (Stateless):** Glyph is a pure value type. Graphics is a stateless compositor (push/pop per frame). No persistent machinery state.
- **E (Encapsulation):** Key, Region, Constraint, Box are private. Atlas, Graphics, ShapedText are public associated types with clear single responsibility.
- **D (Deterministic):** Same Cell + Typeface always produces the same Glyph. Same Glyph always rasterizes to the same pixels.

## Risks / Open Questions

- **C++ nested class forward declaration:** Private nested types need to be forward-declared in the Glyph struct but defined in separate headers. This requires the Glyph struct to be incomplete until all headers are included. The unity-build module header handles this -- include order in jam_graphics.h is critical.
- **Atlas::Box (1108 lines):** BoxDrawing is the largest file. Nesting it inside Atlas::Box doesn't change its content, only its namespace. The file stays separate.
- **Constraint table (11542 lines):** Generated file. Stays in its own .cpp. Only the namespace wrapper changes.
- **Stale glyph/ directory:** Contains duplicates of fonts/ headers. Must be resolved -- delete stale copies, keep only the active files referenced by jam_graphics.h.
