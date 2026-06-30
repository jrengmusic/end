# RFC — System Typeface: Seamless JUCE Font Integration with Glyph Atlas Pipeline

Date: 2026-06-30
Status: Ready for COUNSELOR handoff

## Problem Statement

END's glyph rendering pipeline was designed around OpenGL — `jam::Font`, `jam::Typeface`, `TypefaceResources`, and `glyph::Graphics` form a parallel type system alongside JUCE's native `juce::Font` and `juce::Typeface`. This parallel system introduces redundant types, static registries, and a push/pop rendering ceremony forced by OpenGL's context model.

With Vulkan rendering under END's control (Peer → LLGC → full stack), the question is: can we eliminate the parallel type system and integrate seamlessly with JUCE's font infrastructure, routing everything through the glyph atlas and our LLGC?

## Research Summary

### JUCE Font System (Vanilla — No Patches)

- **`juce::FontOptions`** — modern constructor path (all `Font` constructors deprecated). Fluent builder: `.withFallbacks()`, `.withTypeface()`, `.withPointHeight()`.
- **`juce::Font`** — final value type. Lazy typeface resolution via `getTypefacePtr()`. `getNativeDetails()` → `hb_font_t*` (HarfBuzz handle). Size-independent typeface, sized HarfBuzz font.
- **`juce::Typeface`** — ref-counted base. `getLayersForGlyph()` returns `ColourLayer`/`ImageLayer` for rasterization. `createSystemTypefaceFor(data, size)` → auto-registers by family name while any `Ptr` is alive.
- **`TypefaceCache`** — JUCE-internal singleton, 10-entry LRU keyed on `{name, style}`. Resolved via `LookAndFeel::getTypefaceForFont()` (virtual hook).
- **`FontOptions::withFallbacks()`** — preferred fallback families. JUCE's text shaper auto-falls back for missing glyphs via `Typeface::createSystemFallback()`.
- **`ComponentPeer::externalContextFactory`** — static function pointer (JUCE-provided hook). Returns `unique_ptr<LowLevelGraphicsContext>`. How VulkanEngineRegistry plugs in.
- **`Graphics::getInternalContext()`** — public, returns `LowLevelGraphicsContext&`. The extensibility surface for custom rendering.
- **One LLGC per peer per frame.** Same `Graphics` (same LLGC) flows through entire component tree. State isolation via `saveState()`/`restoreState()`.
- **All text rendering converges** to `LLGC::setFont(Font)` + `LLGC::drawGlyphs(Span<uint16_t>, Span<Point<float>>, AffineTransform)`.

### Current Pipeline (What Exists)

Two completely separate text rendering paths:

**Path 1 — Terminal text (CodeView):**
`jam::Font` → `jam::Typeface` (FreeType/CoreText + HarfBuzz) → `glyph::Atlas` (CPU) → `glyph::Graphics` (push/drawGlyphs/pop) → GL/VK/SW backend. Bypasses `juce::Graphics` entirely.

**Path 2 — JUCE UI text (tabs, menus):**
`juce::Font` → `juce::Graphics::drawText()` → `VulkanLLGC::drawGlyphs()` → **STUB** (DEBT-20260629T100000).

**Static singletons:**
- `TypefaceResources` (`Instance<T>`) — owns `glyph::Atlas` + `jam::Typeface` registry. Owned by `end::LookAndFeel`.
- `VulkanEngineRegistry` (`Instance<T>`) — owns `VulkanContext` per window. Owned by `end::View`. Created/destroyed by `ID::gpu` config event.

### Why Not Just Use juce::GlyphArrangement

`juce::GlyphArrangement` is slow. `glyph::Arrangement` is fast — tested at CPU throughput matching OpenGL, even under extreme terminal raw byte load. Non-negotiable for terminal performance.

### Why push/pop Exists (and Why It Can Die)

Three reasons for the push/drawGlyphs/pop ceremony in `glyph::Graphics`:

1. **Snapshot accumulation** — batch glyph quads before issuing a single draw. NOT an OpenGL artifact. Needed regardless of backend.
2. **OpenGL state save/restore** — `popGL()` saves/restores GL state machine. IS an OpenGL artifact. Not needed with Vulkan.
3. **Software render target** — `popSoftware()` composites to a private `juce::Image` then blits via `g.drawImageAt()`. IS a software-path artifact.

With the LLGC as compositor: snapshot accumulation happens via `LLGC::drawGlyphs()` calls during paint. Flush happens at LLGC destruction (`endFrame()`). The LLGC lifecycle IS the frame bracket. `glyph::Graphics` becomes redundant.

### Config Machinery

`config::Model` → ValueTree property change → listeners dispatch via `jam::Function::Map<Identifier, void> events` (single-key lookup: property first, tree type fallback). Pattern used by View, LookAndFeel, Tabs. Font config lives in `IDtype::code` tree (`ID::fontFamily`, `ID::fontSize`).

## Principles and Rationale

### Direction: Seamless Integration, Not Parallel Types

JUCE stays vanilla — zero source patches. JAM extends only where JUCE reaches its limit. Surface API is standard JUCE (`juce::Font`, `FontOptions`, `juce::Graphics`). Custom rendering is invisible — an LLGC implementation detail.

### What Gets Eliminated

| Type | Reason |
|------|--------|
| `jam::Font` | Replaced by `juce::Font` (via `FontOptions`) + terminal grid metrics struct |
| `jam::Typeface` (as surface type) | Replaced by `juce::Typeface` via `createSystemTypefaceFor`. Shaping service refactored, not eliminated. |
| `TypefaceResources` | JUCE handles font caching internally (`TypefaceCache`). Atlas moves to rendering engine. |
| `glyph::Graphics` | LLGC is the compositor. push/pop ceremony eliminated. |

### What Gets Kept

| Component | Why |
|-----------|-----|
| `glyph::Arrangement` | Fast terminal shaping. `juce::GlyphArrangement` is too slow. |
| `glyph::Atlas` | CPU-side rendering cache. SSOT for rasterized glyph pixels. |
| `Stamp` | Terminal cell styling (SGR flags). |
| `Grapheme` | Grapheme cluster table. |

### SSOT Rendering Path

One atlas, one arrangement method. Only the final blit diverges:

```
glyph::Arrangement shapes (fast, cached)
         │
         ▼
CPU atlas rasterizes on cache miss (SIMD/NEON)
         │
         ▼
    juce::Image (mono R8 + emoji ARGB)     ← SSOT
         │
    ┌────┴────┐
    ▼         ▼
 Vulkan    Software
 upload    blit from
 batch     same atlas
 quads     image
```

Both paths are equally fast. Software blit from the atlas is tested at OpenGL-equivalent throughput.

### Atlas Ownership

Rendering engine always exists. `VulkanEngineRegistry` (or renamed) is always created at startup — `ID::gpu` toggle enables/disables Vulkan within it, but the engine and its `externalContextFactory` persist. Factory always returns our LLGC (Vulkan or software variant). CPU atlas lives in the engine, shared to contexts.

Each `VulkanContext` (per window) owns its GPU atlas mirror (`VkImage`), uploaded from the shared CPU atlas when dirty. Software path accesses the same CPU atlas through the engine.

### Atlas Access (No Globals, No JUCE Patches)

Components access the atlas through JUCE's existing API:

```cpp
// In component paint():
auto& ctx = g.getInternalContext();
// ctx is our LLGC (Vulkan or software) — provides atlas access
```

VulkanLLGC holds `VulkanContext&` (already does this). VulkanContext owns the atlas. The chain is: `Graphics` → `getInternalContext()` → our LLGC → context → atlas.

No `static inline void*`. No `Instance<T>` for atlas. No JUCE source patches. No non-owning naked pointer members.

### Font Registration (JUCE Convention)

`end::LookAndFeel` holds `juce::Typeface::Ptr`s for embedded fonts (keeps them registered by family name). Already does this — the `typefaces` HashMap. `FontOptions` with `.withFallbacks()` handles emoji and nerd font fallback chains natively.

### Terminal Grid Metrics

`jam::Font` provided `cellWidth`, `cellHeight`, `baseline` — terminal-specific metrics derived from the font by scanning ASCII advance widths. These are not a `juce::Font` concept.

A replacement is needed: a thin struct or free function that computes grid metrics from a `juce::Font`. COUNSELOR proposes name, ARCHITECT approves during PLAN.

### HarfBuzz Shaping Service

`jam::Typeface` currently provides HarfBuzz shaping (`shapeText`, `shapeEmoji`, `shapeFallback`), HarfBuzz scratch buffer management, emoji detection, fallback resolution, and style variant font handle management. `glyph::Arrangement::buildArrangements` calls these directly.

This shaping service must continue to exist for the fast terminal path. But it should take `juce::Font` as input (extract `hb_font_t*` via `getNativeDetails()`), not be a parallel type system. The service refactors from a type into a utility — internal to the glyph module, not a surface API.

`juce::Font::getNativeDetails()` provides `hb_font_t*`. Style variants become separate `juce::Font` objects (one per style). Emoji font becomes a separate `juce::Font`. Fallback resolution uses `FontOptions::withFallbacks()` + `Font::findSuitableFontForText()`.

## Scaffold

### Flow: JUCE UI Text (After)

```
FontOptions ("FamilyName", "Regular", height)
    .withFallbacks ({"NerdFont", "Emoji"})
  → Font → lazy Typeface resolution (JUCE-internal TypefaceCache)

juce::Graphics::drawText (text, area, ...)
  → GlyphArrangement shapes text (JUCE HarfBuzz, auto-fallback)
  → LLGC::setFont (Font)
  → LLGC::drawGlyphs (glyphs, positions, transform)
    → atlas.getOrRasterize (font, glyph)
      → cache hit: return atlas rect
      → cache miss: typeface->getLayersForGlyph() → rasterize → pack into atlas
    → accumulate textured quad
  → LLGC destruction → flush batch → Vulkan draw / software blit
```

### Flow: Terminal Text (After)

```
CodeView::ContentView::paint (Graphics& g)
  → atlas.advanceFrame()
  → arrangement.shape (document, visibleLines, font, ...)
      → juce::Font → extract hb_font_t* via getNativeDetails()
      → HarfBuzz shaping (custom, fast, cached per visible line range)
      → Font::findSuitableFontForText() for fallback
  → for each run:
      LLGC::drawGlyphBatch (run, atlas)    // custom method on our LLGC
        → atlas.getOrRasterize per glyph
        → accumulate textured quads
  → LLGC destruction → flush → Vulkan draw / software blit
```

No push(). No pop(). No `glyph::Graphics`. The LLGC lifecycle IS the frame.

### Flow: Font Registration (After)

```
end::LookAndFeel::registerTypeface()
  // JUCE side: create Ptrs from embedded binaries (existing pattern — unchanged)
  auto ptr = juce::Typeface::createSystemTypefaceFor (data, size);
  typefaces.addOrReplace (ptr->getName(), ptr);

  // Terminal font: create juce::Font with fallbacks
  auto terminalFont = juce::Font (
      FontOptions (fontFamily, "Regular", fontSize)
          .withFallbacks ({"NerdFont", "AppleColorEmoji"}));

  // Grid metrics computed from terminalFont
  // (via free function or thin struct — naming gated)
```

### Ownership (After)

```
end::Application                             [app lifetime]
├── end::LookAndFeel                         [styling + font registration]
│   ├── juce::Typeface::Ptrs (HashMap)       [keeps embedded fonts registered]
│   ├── Stamp instance                       [terminal cell styling]
│   └── Grapheme instance                    [grapheme clustering]
│
├── end::Window
│   └── end::View
│       └── RenderingEngine                  [always exists — renamed from VulkanEngineRegistry]
│           ├── glyph::Atlas                 [CPU-side SSOT — shared across all contexts]
│           ├── externalContextFactory       [always set — returns our LLGC]
│           └── VulkanContext (per window)   [optional — only when GPU enabled]
│               ├── GPU atlas mirror         [VkImage, uploaded from CPU atlas]
│               ├── VkDevice, swapchain      [Vulkan resources]
│               └── our LLGC (per paint)     [Vulkan or software variant]
│                   ├── drawGlyphs()         [JUCE UI text → atlas → batch]
│                   ├── drawGlyphBatch()     [terminal fast path → atlas → batch]
│                   └── ~LLGC()              [flush + present]
```

## Blast Radius — Per-File Transformation

### END Project (6 files)

**`Source/end/Panes.cpp` (lines 23-25, 59-61)**
- Currently: constructs `jam::Font` from config values, passes to `Nexus::create()`.
- After: construct `juce::Font` via `FontOptions` from same config values. Compute grid metrics. Pass `juce::Font` + metrics to `Nexus::create()`.

**`Source/Nexus.h` (line 28)**
- Currently: `Session& create (jam::UUID, const jam::Font&)`.
- After: parameter changes to `juce::Font` (+ grid metrics struct).

**`Source/terminal/Session.h` (line 24-25)**
- Currently: constructor takes `const jam::Font&`, forwards to CodeView.
- After: takes `juce::Font` (+ grid metrics). Forwards to CodeView.

**`Source/lookAndFeel/LookAndFeel.h` (lines 170, 186)**
- Currently: owns `jam::TypefaceResources typefaceResources` (line 170) and `typefaces` HashMap (line 186).
- After: `TypefaceResources` member removed. `typefaces` HashMap retained (JUCE convention). `Stamp` and `Grapheme` instances retained.

**`Source/lookAndFeel/LookAndFeel.cpp` (line 9)**
- Currently: calls `registerTypeface()` in constructor.
- After: same call, but implementation changes (see EventRegistration.cpp below).

**`Source/lookAndFeel/EventRegistration.cpp` (lines 7-49)**
- Currently: creates `jam::Typeface`, adds fallbacks/styles, calls `jam::Typeface::registerTypeface()`.
- After: JUCE side unchanged (createSystemTypefaceFor for embedded fonts). JAM side: create `juce::Font` via `FontOptions` with `.withFallbacks()` for emoji + nerd font. No `jam::Typeface` construction. No static registration. Grid metrics computed from the resolved font.

### JAM Font Core (2 files)

**`jam_vulkan/fonts/font/jam_Font.h`** — **DELETE or REPLACE**
- Currently: struct with family, fontSize, cellWidth, cellHeight, baseline, styleFlags, getImage().
- After: eliminated. Replaced by `juce::Font` (via `FontOptions`) + terminal grid metrics (free function or thin struct — COUNSELOR proposes name, ARCHITECT approves during PLAN).
- Grid metric computation (`resolveMetrics()` logic: scan ASCII 32-127 advance widths) moves to the replacement.

**`jam_vulkan/fonts/font/jam_Font.cpp`** — **DELETE or REPLACE**
- Currently: resolveMetrics() + getImage() implementations. Both call `Typeface::findTypeface()`.
- After: resolveMetrics logic moves to grid metrics replacement. getImage() moves to atlas or is eliminated (CaretComponent is the only consumer — can use atlas directly).

### JAM Typeface (2 files)

**`jam_vulkan/fonts/typeface/jam_Typeface.h`** — **REFACTOR**
- Currently: full font handle manager + static registry + static atlas access.
- After: static registry methods (`registerTypeface`, `findTypeface`, `getAtlas`, `setAtlasSize`) eliminated. Instance methods (`shapeText`, `shapeEmoji`, `getFontHandle`, etc.) refactored into a shaping utility that takes `juce::Font` as input. COUNSELOR proposes name, ARCHITECT approves during PLAN.
- `getDisplayScale()` static method needs a new home (utility or LLGC).

**`jam_vulkan/fonts/typeface/jam_TypefaceResources.h`** — **DELETE**
- Currently: `SharedResources<TypefaceResources>` owning `glyph::Atlas`.
- After: eliminated entirely. Atlas moves to rendering engine.

### JAM Glyph Core (5+ files)

**`jam_vulkan/fonts/font/glyph/jam_GlyphGraphics.h`** — **DELETE**
- Currently: per-component compositor with push/drawGlyphs/pop, GL/VK/SW backends.
- After: eliminated. Snapshot accumulation moves to the LLGC. Software compositing (SIMD mono/emoji blending) moves to software LLGC path.

**`jam_vulkan/fonts/font/glyph/jam_GlyphGraphics.cpp`** — **DELETE**
- Software composite logic (`compositeMonoGlyph`, `compositeEmojiGlyph`, `fillBackground`) moves to software LLGC if retained.

**`jam_vulkan/fonts/font/glyph/jam_GlyphGL.cpp`** — **DELETE**
- OpenGL instanced draw path. Eliminated with OpenGL departure.

**`jam_vulkan/fonts/font/glyph/jam_GlyphVK.cpp`** — **REFACTOR → move into LLGC**
- Vulkan per-vertex VB pack + `vkCmdDrawIndexed` logic moves into `VulkanLLGC::drawGlyphBatch()` or the LLGC flush path.

**`jam_vulkan/fonts/font/glyph/jam_GlyphAtlas.h`** — **KEEP (relocate owner)**
- CPU-side atlas is unchanged. `getOrRasterize()`, LRU packing, mono/emoji images — all kept.
- Ownership moves from `TypefaceResources` to rendering engine (shared CPU atlas).
- Rasterization source: keep current platform APIs (FreeType/CoreText) via `void* fontHandle`. Works, already fast and SIMD/NEON optimized.

**`jam_vulkan/fonts/font/glyph/jam_Run.h`** — **REFACTOR**
- `Arrangement::shape()` signature: `const jam::Font&` → `juce::Font` (+ grid metrics).
- `buildArrangements()`: `jam::Typeface&` parameter → refactored shaping service.
- `Run::fontHandle (void*)` — unchanged (atlas key, opaque platform handle).
- `Entry::fontHandle (void*)` — unchanged.

**`jam_vulkan/fonts/font/glyph/jam_GlyphArrangementShape.cpp`** — **REFACTOR**
- `shape()` calls `Typeface::findTypeface(font.family)` → replaced by receiving shaping service.
- `buildArrangements()` takes `jam::Typeface&` → takes refactored shaping service.
- `shapeCodepoint()`, `resolveStyle()` — adapt to new shaping service API.
- Cell metric extraction (`font.cellWidth`, etc.) → from grid metrics struct.

**`jam_vulkan/fonts/font/glyph/jam_Glyph.h`** — **REFACTOR**
- `glyph::getImage()` free function: takes `Typeface&` → adapt or eliminate (only used by `Font::getImage()` which is only used by `CaretComponent`).

**`jam_vulkan/fonts/font/glyph/jam_GlyphGraphicsCells.cpp`** — **REFACTOR → move into LLGC**
- Cell-format rendering with per-glyph constraints, decorations, backgrounds. Logic moves into LLGC's `drawGlyphBatch()` implementation.

### JAM CodeView (2 files)

**`jam_gui/code_view/jam_CodeView.h` (lines 115, 160-161)**
- Currently: owns `jam::Font font`, `glyph::Arrangement arrangement`, `glyph::Graphics glyphGraphics`.
- After: `glyphGraphics` member eliminated. `font` becomes `juce::Font` + grid metrics. `arrangement` retained.

**`jam_gui/code_view/jam_CodeView.cpp` (lines 37-154, 165-192)**
- Currently: ContentView::paint() calls `Typeface::getAtlas().advanceFrame()`, shapes, pushes, draws, pops.
- After:
  ```
  paint (Graphics& g):
    atlas access via g.getInternalContext() → our LLGC → context → atlas
    atlas.advanceFrame()
    arrangement.shape (document, visibleLines, font, ...)
    for each run:
      our LLGC → drawGlyphBatch (run, atlas)
    // no push, no pop — LLGC lifecycle handles frame
  ```
- Constructor: `jam::Font` parameter → `juce::Font` + grid metrics.
- `setFont()`: same change.
- Viewport step sizes: `font.cellWidth` / `font.cellHeight` → from grid metrics.

**`jam_gui/code_view/jam_CaretComponent.h` (lines 121-161)**
- Currently: uses `Typeface::findTypeface()`, `Typeface::getAtlas()`, `font.getImage()`.
- After: atlas access through LLGC (available in paint context). Shaping via refactored service. `font.getImage()` replaced by direct atlas rasterization via `atlas.getOrRasterize()`.

### JAM Vulkan LLGC (1 file)

**`jam_vulkan/context/jam_VulkanLowLevelGraphicsContext.h` (lines 272-288)**
- Currently: `drawGlyphs()` is a stub (DEBT-20260629T100000).
- After: `drawGlyphs()` implemented — atlas lookup per glyph, accumulate textured quads.
- New method: `drawGlyphBatch()` — terminal fast path. Takes arrangement runs + atlas, accumulates quads.
- Both methods accumulate into snapshot. Flush at LLGC destruction.
- Atlas access: via `VulkanContext&` reference (already held).

### JAM Vulkan Context (1 file)

**`jam_vulkan/context/jam_VulkanContext.h`**
- Currently: owns `GlyphAtlas` (GPU-side VkImages) and `GlyphFrameVB` (per-frame vertex/index buffers).
- After: additionally owns `glyph::Atlas` (CPU-side). GPU atlas mirrors uploaded from CPU atlas when dirty. Snapshot accumulation data moves here from `glyph::Graphics`.

## BLESSED Compliance Checklist

- [x] **Bounds** — CPU atlas owned by rendering engine (always exists). GPU mirrors owned by VulkanContext. Deterministic lifecycle. RAII. No resource without an owner.
- [x] **Lean** — Three types eliminated (jam::Font, jam::Typeface surface type, TypefaceResources). One rendering object eliminated (glyph::Graphics). No speculative abstractions.
- [x] **Explicit** — No hidden globals (`Instance<T>` for atlas eliminated). No `static inline void*`. Atlas access through explicit reference chain (Graphics → LLGC → engine → atlas). All parameters visible.
- [x] **SSOT** — One CPU atlas (juce::Image). GPU mirrors are caches. One arrangement method. One compositing path (LLGC). No parallel type system.
- [x] **Stateless** — LLGC is ephemeral (per frame). Atlas is a cache. Arrangement shapes fresh per visible range. No persistent rendering state.
- [x] **Encapsulation** — Atlas is internal to rendering engine. Surface API is standard JUCE. Components call `g.drawText()` or access LLGC for fast path — never touch atlas directly. Layer topology preserved: lower layers (JAM) never know about higher layers (END).
- [x] **Deterministic** — Same font + same text = same glyph indices + same atlas entries + same pixel output. Rasterization is deterministic (CPU, cached). Compositing is deterministic (batch order preserved).

## Decisions Resolved

1. **Atlas ownership** — Rendering engine (persistent, always exists) owns shared CPU atlas. Each VulkanContext owns GPU mirror. Software path accesses same CPU atlas through engine.
2. **Software fallback** — Engine always exists. Factory always set. `ID::gpu` toggles Vulkan within persistent engine. Both Vulkan and software LLGCs provide atlas access.
3. **Terminal grid metrics naming** — COUNSELOR proposes, ARCHITECT approves during PLAN.
4. **Shaping service naming** — COUNSELOR proposes, ARCHITECT approves during PLAN.
5. **Atlas rasterization source** — Keep current platform APIs (FreeType/CoreText). Works, fast, SIMD/NEON optimized.
6. **CaretComponent** — Uses atlas through LLGC in paint context via `atlas.getOrRasterize()`. Standard pattern, no special API needed.

## Handoff Notes

### For COUNSELOR

- This RFC eliminates three JAM types and one rendering object. The blast radius is 20+ files across END and JAM. The transformation is mechanical for most files (parameter type changes) but architectural for the LLGC (new drawGlyphs implementation + drawGlyphBatch method) and the rendering engine lifecycle.
- `glyph::Arrangement` and `glyph::Atlas` are KEPT. Their interfaces change (Font parameter type, atlas ownership) but their core logic is preserved.
- The LLGC stub (DEBT-20260629T100000) is resolved by this work — `drawGlyphs()` gets a real implementation.
- OpenGL code (`jam_GlyphGL.cpp`, `initGL`, `popGL`) is deleted. GL is being left behind.
- The software SIMD compositing code (`compositeMonoGlyph`, `compositeEmojiGlyph`, `fillBackground`) from `glyph::Graphics` may need to move into a software LLGC implementation for the fast terminal blit path. This is the most complex migration piece.
- JUCE MUST stay vanilla. No source patches. All integration through JUCE's existing hooks (`externalContextFactory`, `getInternalContext()`, `LookAndFeel::getTypefaceForFont()`).
- JRENG-CODING-STANDARD applies: no non-owning naked pointer members, no anonymous namespaces, no `static inline void*`, brace init everywhere, `not`/`and`/`or` operators, `.at()` for container access.
- Doxygen discipline: all headers documented, zero warnings, `@param` matches signature.
