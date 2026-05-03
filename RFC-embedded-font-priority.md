# RFC — Embedded Display Mono as Primary Font on macOS
Date: 2026-05-03
Status: Ready for COUNSELOR handoff

## Problem Statement

U+2194 (LEFT RIGHT ARROW) exists in the embedded Display Mono binary but renders from the system-installed copy instead. The macOS `loadFaces()` path resolves `mainFont` via CoreText system lookup (`CTFontDescriptorCreateWithNameAndSize`), ignoring the embedded binary entirely. The embedded copy — which IS the canonical Display Mono — loses to whatever stale version is installed in `~/Library/Fonts/`.

Non-macOS (Windows/Linux) already handles this correctly: `loadFaces()` calls `embeddedFontForFamily()` which returns the embedded binary data. macOS `embeddedFontForFamily()` is a no-op that always returns `nullptr`.

## Research Summary

### Three copies of Display Mono exist, none matching

| Location | Size | Role |
|---|---|---|
| `___display___/fonts/Display Mono/` | ~35KB | Source (ARCHITECT's rebuild) |
| `~/Library/Fonts/` | ~239KB | System-installed (what CoreText resolves) |
| `jam/jam_fonts/display_mono/` | ~240KB | Embedded binary (fallback only) |

All three have U+2194 in cmap, but different MD5 hashes — different builds.

### macOS font loading path (`jam_typeface.mm`)

- `loadFaces()` (line 230–263): `CTFontDescriptorCreateWithNameAndSize(userFamily)` → system lookup. Fallback: `CTFontCreateWithName(CFSTR("Display Mono"))` — also system lookup. Never tries embedded.
- `setFontFamily()` (line 509–570): identical pattern — system descriptor lookup, system name fallback.
- `embeddedFontForFamily()` (line 744–749): **no-op**, returns `nullptr` always.
- `addFallbackFont()` (line 340–368): `CGDataProviderCreateWithData` → `CGFontCreateWithDataProvider` → `CTFontCreateWithGraphicsFont` — proven path for creating valid `CTFontRef` from embedded binary.

### Non-macOS font loading path (`jam_typeface.cpp`)

- `embeddedFontForFamily()` (line 184–210): properly resolves `"display mono"` / `"display mono book"` / `"display mono medium"` / `"display mono bold"` → corresponding embedded binary.
- `loadFaces()` (line 230–263): tries file path first, falls back to `embeddedFontForFamily()` → `FT_New_Memory_Face`. Embedded is already the primary fallback.

### Downstream compatibility

All downstream consumers of `mainFont` work identically regardless of `CTFontRef` origin:
- `hb_coretext_font_create()` — HarfBuzz does not distinguish source
- `CTFontCreateCopyWithAttributes()` in `setSize()` — works on any `CTFontRef`
- `calcMetrics()` — `CTFontGetAscent/Descent/Leading/Advances` — all CoreText API, source-agnostic
- `shapeASCII()`, `shapeHarfBuzz()`, `shapeFallback()` — all use `CTFontRef` API

Proven by `addFallbackFont()` already creating functional `CTFontRef` handles from embedded data.

### Single Typeface instance

Only one `jam::Typeface` exists (`MainComponent.cpp:66`). No overlay/tab/status bar secondary instances.

## Principles and Rationale

**SSOT** — The embedded binary IS Display Mono for END. System-installed copies are uncontrolled duplicates. The embedded copy must be authoritative.

**Deterministic** — System font resolution depends on installed fonts, font caches, OS version. Embedded binary produces identical results across machines.

**Encapsulation** — END ships its own font. It should not depend on external system state for its default font.

**Symmetry** — Non-macOS already does this. macOS is the outlier.

## Scaffold

### Touch points (1 file, 3 methods)

**`jam_typeface.mm`:**

1. **`embeddedFontForFamily()`** (line 744–749) — implement with same matching logic as `jam_typeface.cpp:184–210`:
   - `"display mono"` / `"display mono book"` → `DisplayMonoBook_ttf`
   - `"display mono medium"` → `DisplayMonoMedium_ttf`
   - `"display mono bold"` → `DisplayMonoBold_ttf`

2. **`loadFaces()`** (line 230–263) — try `embeddedFontForFamily()` FIRST. If it returns data, use `CGDataProviderCreateWithData` → `CGFontCreateWithDataProvider` → `CTFontCreateWithGraphicsFont` (same technique as `addFallbackFont()`). Only fall through to system lookup if embedded returns `nullptr` (non-Display-Mono family).

3. **`setFontFamily()`** (line 509–570) — same treatment as `loadFaces()`: embedded first, system lookup only for non-Display-Mono families.

### Strategy: ALWAYS embedded first (LOCKED)

When `userFamily` matches Display Mono, load from embedded binary directly, skip system lookup entirely. For any other family, system lookup as before. This is ARCHITECT's decision — no system-installed copy ever overrides the embedded canonical font.

## BLESSED Compliance Checklist
- [x] Bounds — change scoped to `jam_typeface.mm`, 3 methods
- [x] Lean — no new abstractions, reuses existing `addFallbackFont` technique
- [x] Explicit — embedded font is explicitly loaded by family name match
- [x] SSOT — embedded binary becomes the single source for Display Mono on macOS
- [x] Stateless — no new state, same `mainFont`/`shapingFont` handles
- [x] Encapsulation — font resolution stays inside `Typeface`
- [x] Deterministic — embedded binary produces identical results across machines

## Open Questions

None. All decisions locked.

## Handoff Notes

- `jam_typeface.mm` is in the jam module at `/Users/jreng/Documents/Poems/dev/jam/jam_graphics/fonts/`
- The `JamFontsBinaryData.h` symbols (`jam::fonts::DisplayMonoBook_ttf` etc.) are already `#include`-d at line 31 of `jam_typeface.mm`
- `addFallbackFont()` at line 340–368 is the reference implementation for `CGDataProvider` → `CGFont` → `CTFont` from binary data
- After this fix, the embedded fallback in `addFallbackFont(DisplayMonoBook)` at `MainComponent.cpp:76` becomes redundant (mainFont IS embedded Display Mono), but removal is a separate scope decision
- The source fonts in `___display___/fonts/` still need to be copied to `jam/jam_fonts/display_mono/` to update the embedded binary — that's a build/asset step, not a code change
