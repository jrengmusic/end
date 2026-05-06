# RFC — Atlas Owns Packer
Date: 2026-05-06
Status: Ready for COUNSELOR handoff

## Problem Statement

`Glyph::Atlas` and `Glyph::Packer` are separate objects manually kept in sync:
- `packer.setAtlas(atlas)` wiring in TypefaceRegistry constructor
- Dual `setDimension` + `setAtlasSize` calls on both objects
- Two static accessors (`getPacker()` + `getAtlas()`) on Typeface
- Two params (`Packer&`, `Atlas&`) threaded through `drawGlyphs`
- Dimension mismatch bug on cold init (just fixed)

Packer is an implementation detail of Atlas. No consumer needs packing without the atlas it packs into.

## BLESSED Justification

- **SSOT** — one object, one dimension, one lifecycle
- **Encapsulation** — packer is internal; consumers talk to Atlas only
- **Lean** — eliminates `setAtlas()` wiring, dual accessors, dual params

## Change Specification

### 1. Atlas absorbs Packer

`Glyph::Atlas` gains a private `Packer packer` member. Packer header remains its own file (large class), but Atlas is the sole owner.

```cpp
class Atlas
{
public:
    // --- delegating public API (was external Packer calls) ---
    void advanceFrame() noexcept;                          // delegates to packer
    Region* getOrRasterize (Key, void* fontHandle,
                            bool isEmoji, Constraint,
                            int cellW, int cellH,
                            int baseline) noexcept;        // delegates to packer
    void setAtlasSize (AtlasSize size) noexcept;           // sets packer + dimension + rebuild
    void setEmbolden (bool) noexcept;                      // delegates to packer
    bool getEmbolden() const noexcept;                     // delegates to packer
    void setDisplayScale (float) noexcept;                 // delegates to packer

    // --- existing Atlas API (unchanged) ---
    void writePixels (...) noexcept;
    void rebuild() noexcept;
    juce::Image& getMonoAtlas() noexcept;
    juce::Image& getEmojiAtlas() noexcept;
    int getDimension() const noexcept;

private:
    Packer packer;       // <-- owned, not exposed
    juce::Image mono;
    juce::Image emoji;
    int dimension { ... };
};
```

Atlas constructor wires `packer.setAtlas(*this)` internally. No external wiring.

### 2. TypefaceRegistry slimmed

**Before:**
```cpp
struct TypefaceRegistry
{
    TypefaceRegistry() noexcept { packer.setAtlas (atlas); }
    jam::Owner<jam::Typeface> typefaces;
    juce::HashMap<juce::String, jam::Typeface*> nameMap;
    jam::Glyph::Packer packer;
    jam::Glyph::Atlas atlas;
};
```

**After:**
```cpp
struct TypefaceRegistry
{
    jam::Owner<jam::Typeface> typefaces;
    juce::HashMap<juce::String, jam::Typeface*> nameMap;
    jam::Glyph::Atlas atlas;
};
```

No constructor. No wiring. Both `.cpp` and `.mm`.

### 3. Typeface static API

**Remove:** `static Packer& getPacker() noexcept`
**Keep:** `static Atlas& getAtlas() noexcept`
**Keep:** `static void setAtlasSize (AtlasSize) noexcept` — delegates to `atlas.setAtlasSize()`

`setAtlasSize` implementation becomes:
```cpp
void jam::Typeface::setAtlasSize (jam::Glyph::AtlasSize size) noexcept
{
    getRegistry().atlas.setAtlasSize (size);
}
```

### 4. Call site rewiring

| File : line | Before | After |
|---|---|---|
| `jam_text_editor.cpp:124` | `auto& packer { jam::Typeface::getPacker() };` | remove |
| `jam_text_editor.cpp:125` | `auto& atlas { jam::Typeface::getAtlas() };` | stays |
| `jam_text_editor.cpp:127` | `packer.advanceFrame();` | `atlas.advanceFrame();` |
| `jam_text_editor.cpp:128` | `atlas.ensureImages();` | remove (absorbed into advanceFrame) |
| `jam_text_editor.cpp:1409` | `jam::Typeface::getPacker(),` | remove param |
| `jam_text_editor.cpp:1410` | `jam::Typeface::getAtlas(),` | stays |
| `jam_glyph_graphics.cpp:41` | `drawGlyphs (Packer&, Atlas&, ...)` | `drawGlyphs (Atlas&, ...)` |
| `jam_glyph_graphics.cpp:73` | `packer.getOrRasterize(...)` | `atlas.getOrRasterize(...)` |
| `jam_glyph_graphics.h:89` | `drawGlyphs (Packer&, Atlas&, ...)` | `drawGlyphs (Atlas&, ...)` |
| `jam_typeface.cpp:225-226` | `packer.setAtlas(atlas); atlas.setDimension(...)` | remove (internal) |
| `jam_typeface.cpp:257-258` | `getPacker()` definition | remove |
| `jam_typeface.cpp:270-272` | `packer.setAtlasSize(); atlas.setDimension(); atlas.rebuild()` | `atlas.setAtlasSize(size)` |
| `jam_typeface.mm:850-851` | `packer.setAtlas(atlas); atlas.setDimension(...)` | remove (internal) |
| `jam_typeface.mm:882-883` | `getPacker()` definition | remove |
| `jam_typeface.mm:895-897` | `packer.setAtlasSize(); atlas.setDimension(); atlas.rebuild()` | `atlas.setAtlasSize(size)` |
| `jam_typeface.h:533` | `static Packer& getPacker() noexcept;` | remove |

### 5. Packer changes

Packer class itself is unchanged — same header, same implementation. Only its visibility changes: private member of Atlas instead of public member of TypefaceRegistry. `setAtlas(Atlas&)` remains (called by Atlas constructor internally).

### 6. No END changes

END never references `getPacker()` or `getAtlas()` directly. `MainComponent::setAtlasSize` calls `jam::Typeface::setAtlasSize()` which is unchanged.

### 7. Glyph::Graphics rename: beginFrame/endFrame → push/pop

| File | Before | After |
|---|---|---|
| `jam_glyph_graphics.h` | `void beginFrame (int, int, int, int) noexcept;` | `void push (int, int, int, int) noexcept;` |
| `jam_glyph_graphics.h` | `void endFrame (juce::Graphics&, int, int) noexcept;` | `void pop (juce::Graphics&, int, int) noexcept;` |
| `jam_glyph_graphics.cpp` | `Graphics::beginFrame (...)` | `Graphics::push (...)` |
| `jam_glyph_graphics.cpp` | `Graphics::endFrame (...)` | `Graphics::pop (...)` |
| `jam_text_editor.cpp` | `owner.glyphGraphics.beginFrame (...)` | `owner.glyphGraphics.push (...)` |
| `jam_text_editor.cpp` | `owner.glyphGraphics.endFrame (...)` | `owner.glyphGraphics.pop (...)` |

Paint sequence becomes:
```
atlas.advanceFrame();
glyphGraphics.push (w, h, clipX, clipY);
drawContent (g);
glyphGraphics.pop (g, drawX, drawY);
```

## Open Questions

None. All resolved.

## Handoff Notes

- `ensureImages()` absorbed into `advanceFrame()` — decided by ARCHITECT
- Mechanical refactor — no behavior change, no new logic
- Both `.cpp` and `.mm` must be updated in lockstep
- Packer header stays its own file (it's large) — just no longer publicly accessible
- After this, `Glyph::Atlas` is the single surface for all atlas operations
