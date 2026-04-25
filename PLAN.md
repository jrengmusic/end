# PLAN: Typeface Architecture Refactor

**Objective:** Strip jam::Typeface to font handle management only. Mirror juce::FontOptions pattern for font specification. Remove all BLESSED violations. No JUCE alteration.

**Scope:** jam_graphics (fonts/) + END (Source/)

**Constraint:** `juce::Typeface::Native` (holds `hb_font_t*`) is opaque outside JUCE's translation unit. jam::Typeface retains ownership of platform font handles and HarfBuzz shaping. No JUCE fork modifications.

---

## Architecture

| Type | Role | Location |
|------|------|----------|
| `juce::Typeface` | Font identity and fallback resolution only. Created from embedded data or system name. Used by `jam::Font::findSuitableFontForText()` for glyph coverage and OS fallback. | JUCE |
| `jam::Font` | Value type mirroring `juce::FontOptions`. Name, style, size, fallbacks, typeface pointer. Fluent builder. `findSuitableFontForText()`. | `jam_graphics/fonts/jam_font.h` |
| `jam::Typeface` | **Stripped down.** Font handle management only: platform handles (CTFontRef/FT_Face), HarfBuzz fonts, shaping, metrics, style variants. No Registry, no Packer ownership, no font selection, no BinaryData, no hardcoded names. | `jam_graphics/fonts/jam_typeface.h` |
| `jam::Glyph::Packer` | Renderer-agnostic glyph cache. Rasterization via platform handles from `jam::Typeface`. Atlas packing. Box drawing. Staged bitmap queuing. Consumed by both GPU and CPU paths. | `jam_graphics/fonts/` |

### Data Flow

```
Application (END)
  |
  |  registers embedded typefaces via juce::Typeface::createSystemTypefaceFor()
  |    - Display Mono (3 weights, from jam_fonts module)
  |    - Symbols Nerd Font (from END BinaryData)
  |
  |  constructs jam::Font (name, size, style, fallbacks)
  |  constructs jam::Typeface (font handle management, receives font specs from application)
  |
  v
jam::Font
  |
  |  findSuitableFontForText(codepoint)
  |    1. primary typeface (Display Mono)
  |    2. preferred fallbacks ("Symbols Nerd Font")
  |    3. OS createSystemFallback (emoji, CJK)
  |
  v
jam::Typeface (font handles + shaping)
  |
  |  shapeText(style, codepoints, count) -> GlyphRun
  |  shapeEmoji(codepoints, count) -> GlyphRun
  |  calcMetrics(heightPx) -> Metrics
  |  getFontHandle(style) -> void* (CTFontRef / FT_Face)
  |
  v
jam::Glyph::Packer
  |
  |  rasterization: CTFontDrawGlyphs / FT_Render_Glyph / DirectWrite
  |  atlas: mono (R8) + emoji (BGRA)
  |  box drawing: procedural, no font
  |  staged upload: MESSAGE THREAD rasterize -> GL THREAD glTexSubImage2D
  |
  v
GPU Atlas Textures
```

### END Application Flow

```cpp
// Main.cpp — application registers embedded fonts with system
juce::Typeface::createSystemTypefaceFor (
    jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);

juce::Typeface::createSystemTypefaceFor (
    BinaryData::SymbolsNerdFontRegular_ttf, BinaryData::SymbolsNerdFontRegular_ttfSize);

// MainComponent — Display Mono is THE font
jam::Font font ("Display Mono", config.dpiCorrectedFontSize())
    .withFallbacks ({ "Symbols Nerd Font" })
    .withFallbackEnabled (true);

// User overrides font family via config -> Display Mono becomes first fallback
jam::Font font (userFamily, userSize)
    .withFallbacks ({ "Display Mono", "Symbols Nerd Font" })
    .withFallbackEnabled (true);

// jam::Typeface — application provides font specs, no hardcoded names in module
jam::Typeface typeface (font);
```

### ScreenRender Flow (per cell)

```
cell.codepoint
  |
  +--> isBoxDrawing? --> packer.rasterizeBoxDrawing() --> mono atlas
  |
  +--> cell.isEmoji()?
  |      yes --> typeface.shapeEmoji(codepoints, count)
  |      no  --> typeface.shapeText(style, codepoints, count)
  |
  +--> shaped.count > 0?
  |      yes --> packer.getOrRasterize(glyphId, fontHandle, isColor, ...)
  |      no  --> font.findSuitableFontForText(codepoint) --> resolve fallback
  |              --> shape with fallback typeface --> rasterize
  |
  v
atlas (mono R8 or emoji BGRA)
```

---

## Eliminated from jam::Typeface

- `jam::Typeface::Registry` — entire class (jam_typeface_registry.cpp/mm). Replaced by `jam::Font` fallback chain.
- `#include <BinaryData.h>` — boundary violation. NF font data injected by application.
- `#ifdef BinaryData_SymbolsNerdFontRegular_ttf_Size` — dead code guard (root cause of NF bug).
- Hardcoded font names ("Menlo", "Apple Color Emoji", "Display Mono") — application decides.
- `discoverEmojiFont()` — application provides emoji font via fallback chain.
- `discoverFallbackFace()` — OS fallback via `jam::Font::findSuitableFontForText()`.
- `registerEmbeddedFonts()` — application registers via `juce::Typeface::createSystemTypefaceFor()`.
- Packer ownership — Packer is a separate object, not owned by Typeface.
- Atlas delegation wrappers (getOrRasterize, consumeStagedBitmaps, etc.) — callers access Packer directly.
- `setAtlasSize`, `setAtlasDisplayScale`, `clearAtlas`, `getCacheStats` — Packer API, not Typeface.
- `setEmbolden`, `getEmbolden` — Packer API.
- `rasterizeToImage` — dead code (zero callers).

## Stays on jam::Typeface (font handle management)

- Platform font handle loading (CTFontRef / FT_Face) for main + emoji + identity fonts
- HarfBuzz font creation and management
- `shapeText(style, codepoints, count)` → GlyphRun
- `shapeEmoji(codepoints, count)` → GlyphRun
- `calcMetrics(heightPx)` → Metrics
- `getFontHandle(style)` / `getEmojiFontHandle()` → void*
- `getHbFont(style)` → hb_font_t*
- `setSize(pointSize)` / `setFontFamily(family)` / `setFontSize(size)`
- Style enum, Glyph struct, GlyphRun struct, Metrics struct
- `getDisplayScale()` — static utility
- `isValid()`

## Created

- `jam::Font` — value type in `jam_graphics/fonts/jam_font.h` (mirrors `juce::FontOptions`)

## Modified

### jam_graphics
- `jam_font.h/cpp` — rewritten as `jam::Font` value type (done)
- `jam_typeface.h` — strip Packer ownership, Registry, atlas wrappers, BinaryData, hardcoded names
- `jam_typeface.mm` — remove dead `#ifdef` guard, remove NF loading, remove `registerEmbeddedFonts`, remove `discoverEmojiFont` hardcoding. Application injects font specs.
- `jam_typeface.cpp` — same cleanup as .mm for non-macOS path
- `jam_typeface_shaping.cpp` — remove `discoverFallbackFace`. Fallback via `jam::Font`.
- `jam_glyph_packer.cpp` — remove `jam::Typeface` references (done)
- `jam_text_layout.h/cpp` — uses `jam::Font` + `jam::Typeface` (stripped)
- `jam_glyph_graphics_context.h/cpp` — uses Packer directly instead of via Typeface

### END (Source/)
- `Main.cpp` — registers embedded fonts via `juce::Typeface::createSystemTypefaceFor()`
- `MainComponent.h/cpp` — owns `jam::Font` + `jam::Typeface` + Packer separately
- `Screen.h/cpp` — receives `jam::Typeface&` + `Packer&` + `jam::Font&`
- `ScreenRender.cpp` — uses `jam::Font` fallback chain (replaces Registry), Packer for rasterization
- `ScreenSnapshot.cpp` — same
- `TerminalDisplay.h/cpp` — updated references
- `Tabs.h/cpp` — updated references
- `Panes.h/cpp` — updated references
- `Processor.h/cpp` — updated references
- `Config.cpp` — `getDisplayScale()` stays on `jam::Typeface` (static)

---

## Execution Sequence

### Phase 1: jam::Font (value type) — DONE
1. ~~Rewrite `jam_font.h` — fluent builder mirroring `juce::FontOptions`~~
2. ~~Rewrite `jam_font.cpp` — `findSuitableFontForText()` with fallback chain~~
3. ~~Style enum on `jam::Font` (regular/bold/italic/boldItalic)~~

### Phase 2: Packer decoupling — DONE (step 4)
4. ~~Remove `jam::Typeface` references from Packer~~
5. Add Windows DirectWrite rasterization path (deferred — separate sprint)

### Phase 3: Strip jam::Typeface
6. Remove Registry from jam::Typeface
7. Remove Packer ownership + atlas delegation wrappers from jam::Typeface
8. Remove `#include <BinaryData.h>` + NF loading from jam::Typeface
9. Remove `#ifdef BinaryData_SymbolsNerdFontRegular_ttf_Size` dead guard
10. Remove hardcoded font names ("Menlo", "Apple Color Emoji") — constructor receives specs
11. Remove `registerEmbeddedFonts()` — application registers fonts
12. Remove `discoverEmojiFont()` — application provides emoji font name
13. Remove `discoverFallbackFace()` — fallback via `jam::Font`
14. Remove `rasterizeToImage()` — dead code

### Phase 4: END migration
15. `Main.cpp` — register embedded fonts via `juce::Typeface::createSystemTypefaceFor()`
16. `MainComponent` — own `jam::Font` + `jam::Typeface` + Packer separately
17. `Screen` / `ScreenRender` / `ScreenSnapshot` — use `jam::Font` fallback, Packer direct
18. `TerminalDisplay` / `Tabs` / `Panes` / `Processor` — update references
19. Delete Registry files (jam_typeface_registry.cpp/mm)

### Phase 5: Validation
20. Build macOS — verify Display Mono, NF icons, emoji, box drawing, CJK all render
21. Auditor pass — BLESSED, NAMES.md, JRENG-CODING-STANDARD.md compliance

---

## BLESSED Alignment

| Principle | How |
|-----------|-----|
| **B (Bound)** | `jam::Font` value type (copy semantics). `jam::Typeface` manages font handles exclusively. Packer owns atlas exclusively. No shared ownership. |
| **L (Lean)** | `jam::Typeface` stripped from ~3500 to ~1500 lines (Registry, Packer, atlas, font selection removed). `jam::Font` is small value type. |
| **E (Explicit)** | No silent fallback failures. No `#ifdef` guards on font loading. No hardcoded font names in module. Application provides all font specs. |
| **S (SSOT)** | Font selection in one place (application via `jam::Font`). Fallback chain declared once. |
| **S (Stateless)** | `jam::Font` is stateless value type. `jam::Typeface` holds only font handle state (not font selection logic). |
| **E (Encapsulation)** | Module never reaches into project BinaryData. Font selection is application's concern. Typeface is a dumb font handle manager. Layer flow: application -> `jam::Font` -> `jam::Typeface` -> Packer -> GPU. |
| **D (Deterministic)** | Same font specs + same fallback chain = same result. Platform rasterization engines provide native hinting. |

---

## Naming Decisions (ARCHITECT-approved)

- `jam::Font` — value type, font specification (mirrors `juce::FontOptions`)
- `jam::Font::Style` — enum class (regular, bold, italic, boldItalic)
- `jam::Glyph::Packer` — keeps existing name, stays in `jam_graphics/fonts/`
- `findSuitableFontForText` — mirrors `juce::Font` method name
- `getOrRasterize` — stays (established)
- `rasterizeBoxDrawing` — stays (established)
