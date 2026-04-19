# PLAN: Glyph Atlas Ownership Refactor + Config-Reload Rebuild

**RFC:** none — objective from ARCHITECT prompt (session 2026-04-19)
**Date:** 2026-04-19
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (LANGUAGE.md)

---

## Overview

Relocate GPU glyph atlas handles from `jreng::Typeface` (machinery — BLESSED-S violation) to a new `jreng::GlyphAtlas` class in `jreng_gui/opengl/context/`. Atlas shared by all `GLAtlasRenderer` instances (main window, popup, future splits). Add `setFontFamily`/`setFontSize` reconfiguration on `Typeface`. Wire config-reload through AppState ValueTree properties; a listener applies font changes to `Typeface` and raises an `atlasDirty` atomic signal on AppState. App-layer consumers (GPU lambda + CPU paint path) drain the signal and call `glyphAtlas.rebuildAtlas()`.

CPU atlas images (`cpuMonoAtlas`/`cpuEmojiAtlas`) **stay on Typeface** — CPU path never had the cross-renderer bug; layer boundary prohibits jreng_graphics from reaching jreng_gui.

---

## Language / Framework Constraints

C++ / JUCE reference implementation. All BLESSED principles enforced as written.
`jreng::TextLayout` must remain drop-in compatible with `juce::TextLayout` — signatures unchanged.

---

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

---

## Locked Decisions

| Decision | Value |
|---|---|
| New type | `jreng::GlyphAtlas` — GPU atlas resource holder (GL handles + atlasSize) |
| New type location | `modules/jreng_gui/opengl/context/jreng_glyph_atlas.h/.cpp` (sibling of `jreng_gl_atlas_renderer.*`, `jreng_gl_context.*`) |
| Filename conflict | Existing `modules/jreng_graphics/fonts/jreng_glyph_atlas.h/.cpp/.mm` holds CPU packer class `jreng::Glyph::Atlas`. **Renamed** to `jreng_glyph_packer.h/.cpp/.mm`, class renamed to `jreng::Glyph::Packer` (Step 0). |
| Typeface `atlas` member | Renamed to `packer` (type `jreng::Glyph::Packer`) alongside class rename |
| CPU atlas images | **Stay on Typeface** (`cpuMonoAtlas`, `cpuEmojiAtlas`, `ensureCpuAtlas`). Not moved. |
| Ownership of `GlyphAtlas` | `MainComponent` — value member, constructed after `typeface` |
| Popup shares | same `GlyphAtlas&` passed into popup's `GLAtlasRenderer` ctor |
| AppState properties | `App::ID::fontFamily`, `App::ID::fontSize` on WINDOW node |
| AppState atomic | `std::atomic<bool> atlasDirty { false };` |
| Atomic API | `markAtlasDirty()` (release-store), `consumeAtlasDirty()` (acquire-exchange) — State.h:1052-1066 pattern |
| Font accessors on AppState | `getFontFamily/setFontFamily(const juce::String&)`, `getFontSize/setFontSize(float)` |
| Typeface API additions | `setFontFamily(const juce::String&)`, `setFontSize(float)` — update font handles, call `packer.clear()` |
| Config-reload wiring | `applyConfig` writes AppState only; WINDOW listener applies to Typeface and calls `markAtlasDirty` |
| Consumer (GPU) | `setRenderables` lambda in `MainComponent` calls `glyphAtlas.rebuildAtlas()` when signal set |
| Consumer (CPU) | `Screen::renderPaint` calls `glyphAtlas.rebuildAtlas()` when signal set |
| TextLayout signatures | Unchanged — drop-in with `juce::TextLayout`. `GlyphAtlas&` lives on `GLContext` (member), not threaded through `TextLayout::draw`. |

---

## Out of Scope (Deferred)

- CPU atlas symmetry refactor (separate concern, no current bug)
- Image atlas (`ImageAtlas` per PLAN-IMAGE) — independent landing
- Dialog popup (plain `GLRenderer`, no atlas dependency)

---

## Steps

### Step 0: Rename `jreng::Glyph::Atlas` → `jreng::Glyph::Packer`

**Scope:**
- Files: `modules/jreng_graphics/fonts/jreng_glyph_atlas.h/.cpp/.mm` → `jreng_glyph_packer.h/.cpp/.mm`
- Class: `jreng::Glyph::Atlas` → `jreng::Glyph::Packer`
- Typeface member: `atlas` → `packer` (type updated)
- All references across codebase

**Action:**
- `git mv` to preserve history
- Rename class declaration + definitions + all call sites
- Keep other types in `jreng::Glyph::` namespace untouched (`AtlasSize`, `AtlasPacker`, `StagedBitmap`, `AtlasGlyph`, `LRUGlyphCache`, `Region`, `Key`, `Constraint`, `Type`)
- Update module build-list references (CMakeLists / jreng_graphics module declaration)
- Update `Typeface::atlas` → `Typeface::packer` including all 100+ delegation wrappers (jreng_typeface.h:702-815)

**Validation:**
- @Auditor: grep for old class name / old filename — zero references
- @Auditor: build passes
- NAMES Rule 3 (semantic): `Packer` accurately describes shelf-packer + LRU + staging role

---

### Step 1: Introduce `jreng::GlyphAtlas`

**Scope:** `modules/jreng_gui/opengl/context/jreng_glyph_atlas.h/.cpp` (new)

**Action:**
- Create `jreng::GlyphAtlas` class
- Private members:
  - `uint32_t monoAtlas { 0 };` — GL texture handle (mono)
  - `uint32_t emojiAtlas { 0 };` — GL texture handle (emoji)
  - `jreng::Glyph::AtlasSize atlasSize { jreng::Glyph::AtlasSize::standard };`
- Public API:
  - `GlyphAtlas() noexcept;`
  - `~GlyphAtlas() = default;`
  - GL handle accessors: `uint32_t getMonoAtlas() const noexcept;` / `getEmojiAtlas() const noexcept;` / `setMonoAtlas(uint32_t)` / `setEmojiAtlas(uint32_t)`
  - `setAtlasSize(jreng::Glyph::AtlasSize)` / `getAtlasSize() const noexcept`
  - `void rebuildAtlas() noexcept;` — **GL THREAD** — `glDeleteTextures` on non-zero handles + zero them
- Pure resource holder — no atomics, no listeners, no ValueTree awareness

**Validation:**
- @Auditor: NAMES — noun class, verb methods
- BLESSED-B: owns handles; explicit teardown via `rebuildAtlas`
- BLESSED-S (Stateless): holds only calculation inputs (handles, size). No history, no flags.
- BLESSED-L: file ≤ 300 lines

---

### Step 2: `Typeface::setFontFamily` + `setFontSize`

**Scope:** `modules/jreng_graphics/fonts/jreng_typeface.h`, `.mm`, `.cpp`

**Action:**
- Public API:
  - `void setFontFamily (const juce::String& family);`
    - Update `mainFont`, `shapingFont`, etc. (reapply ctor logic for family)
    - `packer.clear()` (drops stale CPU-rasterized glyphs)
  - `void setFontSize (float size);`
    - Update size-dependent handles and metrics
    - `packer.clear()`
- Both setters: no-op fast path when new value equals current.

**Validation:**
- @Auditor: CPU glyph cache cleared on change — stale glyphs cannot survive
- BLESSED-S: Typeface still holds only calculation inputs
- JRENG: `not`/`and`/`or`, no early returns, positive nesting

---

### Step 3: Strip GL handles from `Typeface`

**Scope:** `modules/jreng_graphics/fonts/jreng_typeface.h`, `.mm`, `.cpp`

**Action:**
- Remove members: `glMonoAtlas`, `glEmojiAtlas` (jreng_typeface.h:1217-1218)
- Remove public API: `getGlMonoAtlas`, `getGlEmojiAtlas`, `setGlMonoAtlas`, `setGlEmojiAtlas`, `resetGlAtlas`
- Ctor parameter `jreng::Glyph::AtlasSize atlasSize` (line 454) — **keep** (CPU packer still takes it)
- `Typeface::setAtlasSize()` (line 784) — **keep** (forwards to internal `packer`, CPU-side only)
- **CPU atlas images and API remain** — `cpuMonoAtlas`, `cpuEmojiAtlas`, `ensureCpuAtlas`, `getCpuMonoAtlas`, `getCpuEmojiAtlas` are untouched.

**Validation:**
- @Auditor: grep codebase for removed GL API — zero references
- BLESSED-S: Typeface holds no GL state
- Module builds in isolation

---

### Step 4: `GLContext` holds `GlyphAtlas&`; reads/writes handles via it

**Scope:** `modules/jreng_gui/opengl/renderers/jreng_gl_context.h/.cpp`

**Action:**
- `GLContext` ctor takes `GlyphAtlas& glyphAtlas` — store as `GlyphAtlas& glyphAtlasRef` member.
- `uploadStagedBitmaps(Typeface&)` signature unchanged.
- Inside body: read/write handles via `glyphAtlasRef` instead of `typeface.getGlMonoAtlas`/etc.
  - If `glyphAtlasRef.getMonoAtlas() == 0` → `createAtlasTexture()` → `glyphAtlasRef.setMonoAtlas(handle)`
  - Cache per-frame `monoAtlas`/`emojiAtlas` from `glyphAtlasRef` at top of function
- Drain staging via `typeface.consumeStagedBitmaps()` (unchanged).

**Validation:**
- @Auditor: no Typeface GL API referenced in GLContext
- @Auditor: `GraphicsContext` (CPU path, jreng_graphics module) untouched by this step
- Build: module + app compile

---

### Step 5: `GLAtlasRenderer` takes `GlyphAtlas&`; wires through to `GLContext`; removes `managedTypefaces`

**Scope:** `modules/jreng_gui/opengl/context/jreng_gl_atlas_renderer.h/.cpp`

**Action:**
- Ctor: `GLAtlasRenderer(Typeface& typeface, GlyphAtlas& glyphAtlas)`. Initializer_list removed.
- Members: `Typeface& typefaceRef`. `glyphContext` (value member) constructed with `glyphAtlas` reference.
- `renderText`: call `glyphContext.uploadStagedBitmaps(typefaceRef)` (GLContext already holds `glyphAtlas`).
- Remove `managedTypefaces` member (dead code).
- **No `rebuildAtlas()` method on GLAtlasRenderer.** Consumers call `glyphAtlas.rebuildAtlas()` directly.

**Validation:**
- @Auditor: no remaining `managedTypefaces`
- BLESSED-E: renderer holds references, forwards per frame
- BLESSED-L: smaller class

---

### Step 6: `MainComponent` owns `GlyphAtlas`

**Scope:** `Source/MainComponent.h`, `Source/MainComponent.cpp`

**Action:**
- Private value member: `jreng::GlyphAtlas glyphAtlas;` after `typeface` (line ~156)
- Initial atlas size in ctor: `glyphAtlas.setAtlasSize(jreng::Glyph::AtlasSize::compact)` (matches current default)
- `setRenderer`:
  - `glyphAtlas.setAtlasSize(atlasSize)` alongside existing `typeface.setAtlasSize(atlasSize)` (CPU-side via Typeface, GPU-side via GlyphAtlas)
  - `GLAtlasRenderer` ctor: `std::make_unique<jreng::GLAtlasRenderer>(typeface, glyphAtlas)`

**Validation:**
- @Auditor: `glyphAtlas` lifetime matches `typeface`
- BLESSED-B: clear owner, clear lifetime

---

### Step 7: Popup passes shared `GlyphAtlas&`

**Scope:** `Source/MainComponentActions.cpp:490-504`

**Action:**
- Popup's `GLAtlasRenderer` ctor: `std::make_unique<jreng::GLAtlasRenderer>(typeface, glyphAtlas)` — main's instances.
- Popup holds no atlas; does not listen, mark, or rebuild.

**Validation:**
- @Auditor: popup receives main's `glyphAtlas` by reference
- Behavior: popup open/close leaves main's atlas untouched — original bug gone

---

### Step 8: AppState font properties + atomic signal

**Scope:** `Source/AppIdentifier.h`, `Source/AppState.h`, `Source/AppState.cpp`

**Action:**
- `AppIdentifier.h`: add Identifiers next to `renderer` (line 77):
  ```cpp
  static const juce::Identifier fontFamily { "fontFamily" };
  static const juce::Identifier fontSize   { "fontSize" };
  ```
- `AppState.h` public API additions:
  - `juce::String getFontFamily() const noexcept;`
  - `void setFontFamily (const juce::String& family);`
  - `float getFontSize() const noexcept;`
  - `void setFontSize (float size);`
  - `void markAtlasDirty() noexcept;`
  - `bool consumeAtlasDirty() noexcept;`
- `AppState.h` private member: `std::atomic<bool> atlasDirty { false };`
- `AppState.cpp`: getters/setters mirror `getWindowWidth`/`setWindowSize`. Atomic methods: release-store / acquire-exchange.

**Validation:**
- @Auditor: atomic pattern matches `setFullRebuild`/`consumeFullRebuild`
- NAMES Rule 5: `atlasDirty` (no `is` prefix — State.h precedent)
- BLESSED-S (SSOT): font values on ValueTree once; atomic is signal-only

---

### Step 9: Writer in `applyConfig`

**Scope:** `Source/MainComponent.cpp` — `applyConfig()` (line 86)

**Action:**
```cpp
appState.setFontFamily (config.getString (Config::Key::fontFamily));
appState.setFontSize   (static_cast<float> (config.dpiCorrectedFontSize()));
appState.setRendererType (config.getString (Config::Key::gpuAcceleration));
setRenderer (appState.getRendererType());
```

**Validation:**
- @Auditor: `applyConfig` does not reach Typeface directly
- BLESSED-Explicit: writer paths visible

---

### Step 10: Listener on WINDOW node

**Scope:** `Source/MainComponent.cpp` — ctor + dtor + `valueTreePropertyChanged`

**Action:**
- Ctor: `appState.getWindow().addListener(this);`
- Dtor: `appState.getWindow().removeListener(this);`
- `valueTreePropertyChanged`:
  ```cpp
  void MainComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
  {
      if (tree.getType() == App::ID::WINDOW)
      {
          if (property == App::ID::fontFamily)
          {
              typeface.setFontFamily (appState.getFontFamily());
              appState.markAtlasDirty();
          }
          else if (property == App::ID::fontSize)
          {
              typeface.setFontSize (appState.getFontSize());
              appState.markAtlasDirty();
          }
          else if (property == App::ID::renderer)
          {
              appState.markAtlasDirty();
          }
      }
  }
  ```

**Validation:**
- @Auditor: listener registered/unregistered symmetrically
- @Auditor: 3-branch MANIFESTO-L limit respected
- JRENG: `not`/`and`/`or`, no early returns
- @Auditor: does not conflict with nexus/sessions listeners

---

### Step 11: GPU consumer — `setRenderables` lambda

**Scope:** `Source/MainComponent.cpp` — `setRenderer` lambda body (lines 113-129)

**Action:**
```cpp
if (appState.consumeAtlasDirty())
    glyphAtlas.rebuildAtlas();
// existing tabs/panes iteration
```

**Validation:**
- @Auditor: GL thread (existing contract)
- BLESSED-B: `this` capture gives lifetime-valid access
- JRENG: positive check, no early return

---

### Step 12: CPU consumer — `Screen::renderPaint`

**Scope:** `Source/terminal/rendering/ScreenGL.cpp` — `renderPaint` (around line 197)

**Action:**
- CPU path runs on message thread. CPU-mode atlas rebuild means: CPU atlas images on Typeface get cleared (via `typeface.setFontFamily`/`setFontSize` in listener) — and the GPU-side `GlyphAtlas::rebuildAtlas()` is a no-op when handles are 0 (CPU-only runtime never assigns GL handles). So the check is safe on message thread:
  ```cpp
  if (AppState::getContext()->consumeAtlasDirty())
      glyphAtlas.rebuildAtlas();  // safe no-op in CPU mode (no GL handles to delete)
  ```
- Reach `glyphAtlas` via `Screen::Resources` — add `jreng::GlyphAtlas& glyphAtlas` to Resources, set at Screen construction by MainComponent.

**Validation:**
- @Auditor: `Screen::Resources` threading is minimal (one reference added)
- @Auditor: `rebuildAtlas()` on message thread is safe when handles are 0 (no GL call issued)
- Manual: CPU-only runtime font change triggers Typeface packer clear → next frame rasterizes fresh

---

### Step 13: Final sweep + ARCHITECTURE.md update

**Scope:** Cross-cutting

**Action:**
- Grep removed Typeface GL API — zero references
- `managedTypefaces` fully removed
- Update `ARCHITECTURE.md`:
  - `GlyphAtlas` entry under `modules/jreng_gui/opengl/context/`
  - `Glyph::Packer` rename reflected
  - AppState: `fontFamily`, `fontSize`, `atlasDirty` atomic under APVTS section
  - Typeface: no longer holds GL state; still holds CPU atlas images
- Full @Auditor pass

**Validation:**
- @Auditor: grep checks pass
- @Auditor: ARCHITECTURE.md reflects implementation
- @Auditor: no dead code
- Manual: Cmd+R with font family change in end.lua rebuilds atlas. Popup open/close leaves main atlas untouched. CPU mode + font change rebuilds.

---

## BLESSED Alignment

- **B (Bound):** `GlyphAtlas` single owner (MainComponent). Popup non-owning ref. GL textures destroyed explicitly via `rebuildAtlas` on GL thread.
- **L (Lean):** One new class (~small), renames existing, removes dead `managedTypefaces`. No new abstraction layers. Listener at 3-branch boundary.
- **E (Explicit):** Writer → ValueTree → listener → atomic → consumer. No implicit context.
- **S (SSOT):** GL handles live once (`GlyphAtlas`). Font values live once (AppState ValueTree). Atomic on AppState is signal-only.
- **S (Stateless):** Typeface no GL state. `GlyphAtlas` is dumb resource holder. AppState carries SSOT per State.h precedent. CPU atlas images remain on Typeface as acknowledged scope bound.
- **E (Encapsulation):** `GLContext` holds `GlyphAtlas&`, forwards. `GLAtlasRenderer` holds references. Typeface no longer knows GL. TextLayout unchanged.
- **D (Deterministic):** Config → AppState → listener → atomic → consumer → rebuild.

---

## Risks / Open Questions

All gates resolved. Execution proceeds sequentially Step 0 → Step 13.
