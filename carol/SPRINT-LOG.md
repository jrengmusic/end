# SPRINT-LOG.md

**Project:** end  
**Repository:** /Users/jreng/Documents/Poems/dev/end  
**Started:** 2026-02-02

**Purpose:** Long-term context memory across sessions. Tracks completed work, technical debt, and unresolved issues. Written by PRIMARY agents only when user explicitly requests.

---

## Sprint 120 — COUNSELOR: Hyperlinks, State Refactor, Audit Polish

**Date:** 2026-03-24

### Agents Participated
- COUNSELOR: hyperlinks architecture, State refactor (APVTS compliance), output block design, listener wiring
- @pathfinder: link detection architecture discovery, setSnapshotDirty call site audit, Cell struct layout
- @engineer: Config handlers/extensions, OUTPUT_BLOCK node, link filtering, State parameterMap migration, juce::Font→FontOptions migration, shared atlas textures
- @machinist: audit fix pass (dead code, magic numbers, naming, early exits)
- @auditor: production quality assessment (21 findings across 30 files)
- @librarian: juce::FontOptions API, JUCE APVTS storage pattern

### Files Modified (20+ total)

**State refactor — unified APVTS pattern:**
- `State.h` — StateMap template, fetchAdd/fetchSub, all stray atomics migrated to parameterMap, fullRebuild signal, StringSlot reduced to buffer-only, getRawValue if-constexpr cleanup
- `State.cpp` — all values as addParam (proper PARAM nodes in ValueTree), storeAndFlush for all setters, clearOutputBlock removed, getParamNode removed (use module utility)
- `StateFlush.cpp` — OUTPUT_BLOCK ephemeral node removed, flush simplified
- `Identifier.h` — added IDs for migrated atomics, removed stale IDs (outputBlock, blockTopRow, blockBottomRow, blockScanRequested, lastFlushedScanActive)

**Hyperlinks — configurable handlers + event-driven scanning:**
- `Config.h` — hyperlinkHandlers map, hyperlinkExtensions set, getHandler/isClickableExtension
- `Config.cpp` — loadHyperlinks table loader
- `default_end.lua` — handlers/extensions examples
- `LinkDetector.h` — hasKnownExtension checks Config
- `LinkManager.h` — ValueTree::Listener, promptRowNode + activeScreenNode members
- `LinkManager.cpp` — event-driven scan via valueTreePropertyChanged on promptRow/activeScreen nodes, file links gated by output block + normal screen, dispatch checks handlers
- `TerminalComponent.cpp` — simplified vblank link path (no needsScan polling), enterOpenFileMode gated by hasOutputBlock

**juce::Font → FontOptions migration:**
- `jreng_text_layout.h` — Run::fontOptions replaces Run::font, resolveStyle takes int flags
- `jreng_text_layout.cpp` — FontOptions construction from attr.font, resolveStyle uses style flags
- `MessageOverlay.h` — fontOptions member + parameter
- `SelectionOverlay.h` — direct getHeight on FontOptions
- `jreng_graphics_fader.h` — transient juce::Font at draw site
- `jreng_graphics_rotary.h/.cpp` — FontOptions parameter

**Shared atlas textures:**
- `jreng_gl_text_renderer.h/.cpp` — static shared texture IDs with refcount

**Audit polish:**
- `ScreenRender.cpp` — removed 4 dead pixelsPerEm, named constants for magic numbers, fixed indentation, renamed fc→fontRegistry, slot→registrySlot
- `LinkManager.cpp` — continue/break→positive checks, brace init in loops
- `MouseHandler.h/.cpp` — removed dead setScrollFn parameter

**Cleanup:**
- `ValueTreeUtilities.h` — deleted (SSOT violation, use jreng module)
- All call sites migrated to jreng::ValueTree:: from module

### Alignment Check
- [x] LIFESTAR principles followed (SSOT for State, event-driven listeners, no stray values)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied (APVTS pattern, ValueTree listeners)

### Problems Solved
- All stray atomics (18 total) migrated to parameterMap with proper PARAM nodes in ValueTree
- StringSlot generation counters moved to parameterMap
- Hyperlinks event-driven via ValueTree::Listener on promptRow/activeScreen PARAM nodes
- File links restricted to last output block on normal screen only
- URL links work everywhere
- Config extensible: handlers table + extensions array
- juce::Font deprecated API eliminated (except LookAndFeel overrides)
- Shared GL atlas textures fix split-pane rendering
- Dead code removed, magic numbers named, early exits fixed

### Technical Debt / Follow-up
- fzf jassert in JUCE text shaping — from Console widget, not from END changes
- StringSlot struct could be eliminated (buffer only, no other members)
- OutputBlock row indices are still visible-row-relative — may become stale on scroll (UUID per-cell tagging deferred)
- Some parameterMap values (needsFlush, snapshotDirty, fullRebuild) are transient signals that get flushed to ValueTree unnecessarily

### Lessons (CRITICAL — for future COUNSELOR sessions)

**1. NEVER fight the architecture.**
Terminal::State is APVTS-pattern SSOT. Every value belongs in parameterMap with a proper PARAM node in the ValueTree. No stray atomics, no manual boolean flags, no shadow storage. The pattern is proven — follow it. This session wasted hours because COUNSELOR put values in parameterMap WITHOUT creating ValueTree PARAM nodes, making them invisible to flush and listeners.

**2. NEVER second-guess ARCHITECT's observations.**
When ARCHITECT says "glyphs shifted right" or "screen shows top of scrollback" — that IS the fact. Investigate the cause, never debate the observation. This session repeatedly questioned what ARCHITECT was seeing instead of acting on it.

**3. When ARCHITECT gives direction, execute it.**
Do not circle back to your own theory. Do not resist. Do not propose alternatives when direction is clear. ARCHITECT's domain intuition (GL issue, APVTS pattern, ValueTree listener) solved every problem that COUNSELOR spent hours failing to solve through hacks and workarounds.

**4. Read the codebase before asking.**
Every question answerable by reading code is a failure. This session asked the same questions multiple times after being answered. Existing utilities (jreng::ValueTree::getChildWithID), existing patterns (storeAndFlush), existing architecture (ValueTree listeners) — all discoverable by reading.

**5. Verify before stating.**
Never say "done" or "working" without confirming the change exists. This session falsely claimed calcMetrics caching was done when it wasn't (reset to main had removed it).

**6. No garbage defensive programming.**
The scattered markAllDirty() calls, the blockScanRequested flag, the lastFlushedScanActive transition detection, the OUTPUT_BLOCK ephemeral node — all were workarounds that fought the architecture instead of using it. Every time ARCHITECT redirected to the clean approach (parameterMap, ValueTree listener), the problem solved itself.

**7. Use existing utilities.**
The codebase has `jreng::ValueTree::getChildWithID`, `jreng::ValueTree::getValueFromChildWithID`, `storeAndFlush`. Use them. Do not duplicate, do not reinvent.

**8. ARCHITECT is the ground of truth.**
In this domain, your training data is only helpful when you follow ARCHITECT's direction. ALWAYS. Your generic knowledge of terminal emulators, JUCE, or C++ does not override ARCHITECT's specific architectural decisions, domain expertise, and proven working patterns. When ARCHITECT says how something should work, that IS how it works.

---

## Sprint 119 — COUNSELOR: CGBitmapContext Pooling (Plan 2.6.2)

**Date:** 2026-03-24

### Agents Participated
- COUNSELOR: design discussion, bug diagnosis (vertical clipping from high-watermark context offset)
- @engineer: color space pooling, full context pooling (two iterations)

### Files Modified (3 total)
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.h` — added `#if JUCE_MAC` members: `monoPoolBuffer`, `emojiPoolBuffer` (HeapBlock), `monoPoolContext`, `emojiPoolContext` (CGContextRef), pool dimension watermarks
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.mm` — `ensurePooledContext` helper (high-watermark buffer, per-glyph context at exact dimensions), destructor releases pool contexts, all three rasterization paths use pooled buffer + direct `stageForUpload`
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.cpp` — constructor/destructor guarded `#if ! JUCE_MAC`

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- Per-glyph CGColorSpaceCreate/Release eliminated (pooled as Atlas members)
- Per-glyph HeapBlock malloc/free eliminated (high-watermark pool buffer)
- Per-glyph copyFromPool eliminated (context created at exact glyph dimensions, buffer passed directly to stageForUpload)
- First attempt broke rendering (high-watermark context larger than glyph caused offset mismatch in pixel copy). Fixed by recreating context per glyph at exact dimensions against the pooled buffer.

### Technical Debt / Follow-up
- CGBitmapContext still recreated per glyph (cheap against pre-allocated buffer, but could be pooled if context creation proves costly in profiling)
- SaveGState/RestoreGState retained per glyph for safety — unnecessary since context is recreated, could be removed

---

## Sprint 118 — COUNSELOR: Rendering Optimization (Plan 2.6) + State Refactor + Bug Fixes

**Date:** 2026-03-24

### Agents Participated
- COUNSELOR: throughput investigation, rendering analysis, State architecture redesign, plan writing
- @oracle: deep PTY throughput analysis, rendering pipeline hot path analysis (calcMetrics, buildSnapshot, atlas)
- @librarian: juce::AbstractFifo research, input path tracing, JUCE APVTS storage pattern research
- @pathfinder: Session/State/Grid data flow mapping, dirty-row infrastructure discovery, setSnapshotDirty call site audit
- @engineer: calcMetrics caching, dirty-row skipping, State refactor, shared atlas textures, glyph position fix, process query move
- @auditor: PLAN.md contract verification (found wrong file paths, missing macOS path, inaccurate cache description)

### Files Modified (15 total)

**State refactor — unified APVTS pattern:**
- `Source/terminal/data/State.h` — StateMap<unique_ptr> template, fetchAdd/fetchSub CAS utilities, migrated 7 stray atomics into parameterMap, StringSlot generation uint32_t to float, fullRebuild signal, removed onCursorBlinkRowDirty
- `Source/terminal/data/State.cpp` — constructor uses make_unique, stray atomic methods rewired to getRawParam, view-state setters call setFullRebuild, fetchAdd for string generation
- `Source/terminal/data/StateFlush.cpp` — updated iterator access for unique_ptr
- `Source/terminal/data/Identifier.h` — 7 new IDs: pasteEchoRemaining, syncOutputActive, syncResizePending, outputBlockTop, outputBlockBottom, outputScanActive, promptRow

**Rendering optimization (2.6.0 + 2.6.1):**
- `modules/jreng_graphics/fonts/jreng_typeface.h` — cachedMetricsSize + cachedMetrics members
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp` — calcMetrics cache hit/miss (non-macOS)
- `modules/jreng_graphics/fonts/jreng_typeface.mm` — calcMetrics cache hit/miss (macOS CoreText)
- `Source/terminal/rendering/ScreenRender.cpp` — buildSnapshot dirty-row skipping via consumeDirtyRows + fullRebuild override
- `Source/terminal/logic/Grid.h` — dirtyRows init to all-ones for first-frame full rebuild
- `Source/terminal/rendering/Screen.h` — render/buildSnapshot signature to State& (non-const for consumeFullRebuild)

**Bug fixes:**
- `Source/terminal/rendering/ScreenRender.cpp` — Font construction: baseFontSize not pixelsPerEm
- `Source/terminal/rendering/ScreenSnapshot.cpp` — same baseFontSize fix for cursor path
- `modules/jreng_graphics/fonts/jreng_atlas_packer.h` — removed 1px gutter (unnecessary, GL_NEAREST)
- `modules/jreng_graphics/fonts/jreng_text_layout.h` — juce::Font deprecated constructor fix
- `modules/jreng_opengl/renderers/jreng_gl_text_renderer.h` — shared atlas textures (static refcount)
- `modules/jreng_opengl/renderers/jreng_gl_text_renderer.cpp` — shared atlas lifecycle, uploadStagedBitmaps uses shared textures

**Cleanup:**
- `Source/terminal/logic/Session.cpp` — process queries moved to onFlush timer callback, removed onCursorBlinkRowDirty wiring
- `Source/component/TerminalComponent.cpp` — removed 5 scattered markAllDirty calls
- `Source/component/InputHandler.cpp` — removed 6 scattered markAllDirty calls
- `Source/component/MouseHandler.cpp` — removed 5 scattered markAllDirty calls
- `Source/MainComponent.cpp` — fixed broken lambda brace

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered (fetchAdd/fetchSub, StateMap, fullRebuild)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied (APVTS pattern for State, SSOT)

### Problems Solved
- calcMetrics called 4800 times/frame — now cached, one call on size change
- buildSnapshot rebuilt all rows every frame — now skips clean rows (90-98% reduction)
- 7 stray atomics (mixed int/bool types) scattered in State — unified into parameterMap as float
- StringSlot generation used uint32_t — now float with CAS-based fetchAdd
- Glyph position shifted right — baseFontSize vs pixelsPerEm mismatch in Font construction
- Split panes garbled — atlas textures created per-renderer but staging queue consumed destructively by first renderer; fixed with shared static textures
- 16 scattered markAllDirty calls across UI components — replaced with fullRebuild signal through State
- Process queries (tcgetpgrp, proc_name, proc_pidinfo) on reader thread hot path — moved to message thread timer

### Technical Debt / Follow-up
- 2.6.2 (CGBitmapContext pooling on macOS) not done — reverted after rendering bugs, low priority
- calcMetrics FreeType path: cache hit skips FT_Set_Char_Size restore — safe on macOS (CoreText), needs verification on Linux/Windows
- Dead pixelsPerEm variables remain in ScreenRender.cpp (computed but unused after baseFontSize fix)
- PLAN-throughput.md analysis was based on -O0 debug benchmarks — invalidated by -O3 results (END beats all competitors)

---

## Sprint 117 — COUNSELOR: Screen Rewire + Module Consolidation + Atlas Validation

**Date:** 2026-03-23

### Agents Participated
- COUNSELOR: architecture decisions, Screen rewire planning, atlas design discussion
- @pathfinder: ShapeResult usage mapping, Screen getOrRasterize call sites, GLTextRenderer interface
- @engineer: ShapeResult→GlyphRun rename, emitShapedGlyphsToCache rewire, ScreenSnapshot cursor rewire, AtlasPacker gutter, Font::getGlyph faceHandle resolution, module file moves, build verification
- @oracle: brutal assessment of sprite sheet atlas design (2 rounds — identified fatal flaws in uniform grid, validated current shelf packer + LRU as sound)

### Files Modified (30+ total)

**Module restructure:**
- `modules/jreng_graphics/jreng_graphics.h` — added jreng_freetype dep, CoreText frameworks, all fonts/ includes
- `modules/jreng_graphics/jreng_graphics.cpp` — added fonts/ unity build (libunibreak, typeface, atlas, layout)
- `modules/jreng_graphics/jreng_graphics.mm` — added macOS .mm includes (typeface, atlas)
- `modules/jreng_graphics/fonts/` — 22 files created (moved from jreng_glyph)
- `modules/jreng_graphics/fonts/linebreak/` — 13 files (vendored libunibreak)
- `modules/jreng_graphics/fonts/jreng_font.h` — NEW lightweight Font value type
- `modules/jreng_graphics/fonts/jreng_font.cpp` — Font::getGlyph() implementation with faceHandle resolution
- `modules/jreng_graphics/fonts/jreng_typeface.cpp` — fixed internal includes to renamed files
- `modules/jreng_graphics/fonts/jreng_typeface.mm` — fixed internal include to renamed registry
- `modules/jreng_graphics/fonts/jreng_text_layout.h` — templatized draw(), removed GL/Renderer draw overloads
- `modules/jreng_graphics/fonts/jreng_text_layout.cpp` — removed all draw implementations (now in header template)
- `modules/jreng_opengl/renderers/` — 4 files created (GL text renderer, shaders, render types)
- `modules/jreng_opengl/jreng_opengl.h` — added jreng_graphics dependency, renderer includes
- `modules/jreng_opengl/jreng_opengl.cpp` — added GL text renderer to unity build
- `modules/jreng_glyph/` — DELETED entirely
- `CMakeLists.txt` — removed jreng_glyph from JUCE_MODULES

**Rename ShapeResult → GlyphRun (6 files):**
- `fonts/jreng_typeface.h` — struct, all method signatures, doc comments
- `fonts/jreng_typeface.mm` — all method definitions, local variables
- `fonts/jreng_typeface_shaping.cpp` — all method definitions, local variables
- `fonts/jreng_text_layout.cpp` — local variable
- `Source/terminal/rendering/ScreenRender.cpp` — 3 local variables
- `Source/terminal/rendering/ScreenSnapshot.cpp` — 2 local variables

**Font::getGlyph() + Screen rewire:**
- `fonts/jreng_font.h` — added applyGlyphRun(), faceHandle private member
- `fonts/jreng_font.cpp` — getGlyph() uses faceHandle override when set
- `Source/terminal/rendering/ScreenRender.cpp` — emitShapedGlyphsToCache simplified (Font& replaces 8 params), all 4 call sites construct Font + applyGlyphRun, tryLigature loses fontHandle param
- `Source/terminal/rendering/ScreenSnapshot.cpp` — cursor path rewired to Font::getGlyph()
- `Source/terminal/rendering/Screen.h` — tryLigature signature updated

**Atlas gutter:**
- `modules/jreng_graphics/fonts/jreng_atlas_packer.h` — +1px horizontal and vertical gutter between packed glyphs

### Alignment Check
- [x] LIFESTAR principles followed (tell-don't-ask, explicit encapsulation, no poking internals)
- [x] NAMING-CONVENTION.md adhered (ShapeResult→GlyphRun: semantic over literal)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied (Font resolves face internally)

### Problems Solved
- Screen no longer constructs Glyph::Key or handles void* fontHandle — Font::getGlyph() encapsulates
- ShapeResult renamed to GlyphRun — semantic naming per convention
- jreng_glyph module eliminated — non-GL in jreng_graphics/fonts/, GL in jreng_opengl/renderers/
- Atlas sprite sheet design validated by Oracle — shelf packer + LRU confirmed as correct architecture
- 1px atlas gutter prevents GL_LINEAR texture bleeding between packed glyphs
- Font::applyGlyphRun() transfers face context from shaping result without exposing void* handles

### Technical Debt / Follow-up
- Box drawing path still calls `typeface.getOrRasterizeBoxDrawing()` directly — not through Font
- `Glyph::Renderer` dead interface file still in jreng_opengl — delete
- Stale `jreng_glyph` references in doc comments across codebase
- `typeface.advanceFrame()` called directly by Screen — should route through Font or stay on Typeface
- GLGraphics::setFont() + drawGlyphs() not yet added — needed for TextLayout template instantiation
- Render::Quad/Background/SnapshotBase still terminal-specific — may belong in END, not jreng_opengl
- `void* fontHandle` still exists in GlyphRun struct — migrate to faceSlot when fallback fonts register dynamically
- Performance: END 27.2s / 46% CPU vs kitty 13.2s / 99% CPU on `seq 1 10M` — Plan 2.6 target
- PLAN.md needs update to reflect completed module restructure

---

## Sprint 116 — COUNSELOR: Decouple GL from Glyph Pipeline + Font/Typeface Split

**Date:** 2026-03-23

### Agents Participated
- COUNSELOR: architecture decisions, plan authoring, all design discussions
- @pathfinder: codebase mapping (Atlas usage, Font touchpoints, GLTextRenderer interface, jreng_opengl structure)
- @engineer: file moves (jreng_glyph → jreng_graphics/fonts/), Font→Typeface rename, Atlas::Type refactor, cleanup
- @librarian: JUCE GlyphArrangement/TextLayout/LowLevelGraphicsContext rendering flow research
- @researcher: JUCE font rendering pipeline pattern analysis (drawGlyphs, LowLevelGraphicsContext)

### Files Modified (60+ total)

**New module location: `modules/jreng_graphics/fonts/` (22 files)**
- `jreng_font.h` — NEW lightweight `jreng::Font` value type (size, style, typeface ref)
- `jreng_typeface.h/.cpp/.mm` — renamed from `jreng_font.*` (`jreng::Font` → `jreng::Typeface`)
- `jreng_typeface_shaping.cpp` — renamed from `jreng_font_shaping.cpp`
- `jreng_typeface_metrics.cpp` — renamed from `jreng_font_metrics.cpp`
- `jreng_typeface_registry.cpp/.mm` — renamed from `jreng_font_registry.*`
- `jreng_atlas_packer.h` — moved from `atlas/`
- `jreng_glyph_key.h` — moved from `atlas/`
- `jreng_atlas_glyph.h` — moved from `atlas/`
- `jreng_staged_bitmap.h` — moved from `atlas/`, `AtlasKind` → `Glyph::Type`, `format` field removed
- `jreng_lru_glyph_cache.h` — moved from `atlas/`
- `jreng_glyph_constraint.h` — moved from `constraint/`
- `jreng_constraint_transform.h` — moved from `constraint/`
- `jreng_glyph_constraint_table.cpp` — moved from `constraint/`
- `jreng_box_drawing.h` — moved from `drawing/`
- `jreng_glyph_atlas.h/.cpp/.mm` — moved from `atlas_impl/`
- `jreng_text_layout.h/.cpp` — moved from `layout/`, `draw()` templatized
- `linebreak/*` — vendored libunibreak moved from `jreng_glyph/linebreak/`

**Modified: `modules/jreng_graphics/`**
- `jreng_graphics.h` — added dependencies (jreng_freetype, juce_graphics, CoreText), added all fonts/ includes
- `jreng_graphics.cpp` — added unity build includes for fonts/ .cpp and linebreak/ .c files
- `jreng_graphics.mm` — added macOS-specific .mm includes for typeface and atlas

**Modified: `modules/jreng_glyph/` (stripped to GL-only)**
- `jreng_glyph.h` — stripped to GL-only render includes, depends on jreng_graphics
- `jreng_glyph.cpp` — stripped to GL text renderer only
- `jreng_glyph.mm` — stripped (no macOS-specific files remain)

**Deleted from `modules/jreng_glyph/`:**
- `atlas/`, `atlas_impl/`, `constraint/`, `drawing/`, `font/`, `layout/`, `linebreak/` — all moved to jreng_graphics/fonts/

**Renamed across 24 Source/ files:**
- `jreng::Font` → `jreng::Typeface` in: Screen.h/.cpp, ScreenRender.cpp, ScreenSnapshot.cpp, ScreenGL.cpp, MainComponent.h/.cpp, Main.cpp, TerminalComponent.h/.cpp, Panes.h/.cpp, Tabs.h/.cpp
- Member variables `font` → `typeface` where referring to the heavy resource manager

**Modified in `modules/jreng_glyph/render/`:**
- `jreng_gl_text_renderer.h/.cpp` — `Font&` → `Typeface&`, `Font::atlasDimension()` → `Typeface::atlasDimension()`

### Alignment Check
- [x] LIFESTAR principles followed (Font god object split, tell-don't-ask, explicit encapsulation)
- [x] NAMING-CONVENTION.md adhered (Typeface = resource manager, Font = lightweight value type)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied (DIP, module separation)

### Problems Solved
- `jreng::Font` was a god object — split into `jreng::Typeface` (heavy) + `jreng::Font` (lightweight)
- `jreng_glyph` module mixed rendering-agnostic code with GL-specific code — non-GL code moved to `jreng_graphics/fonts/`
- `TextLayout::draw()` had GL dependency — templatized, works with any graphics context via duck typing
- `Glyph::Renderer` abstract interface replaced by template deduction — no virtual overhead, no dependency
- `StagedBitmap::format` (GL constant) replaced by `Glyph::Type` enum — rendering-agnostic
- Atlas absorbed into Typeface — rendering layer never touches Atlas directly
- Researched JUCE's `LowLevelGraphicsContext::drawGlyphs` pattern — parallel spans (SOA), setFont per-run

### Technical Debt / Follow-up
- `jreng_glyph` module still exists with GL-only render files — pending move to `jreng_opengl` (Step 2.5.4)
- `jreng::Font::getGlyph()` not yet implemented — needs atlas integration via Typeface
- `GLGraphics::setFont()` + `GLGraphics::drawGlyphs()` not yet added — Step 2.5.4
- Screen still uses `GLTextRenderer` directly with pre-built `Render::Quad` arrays — pending rewire
- `Render::Quad`, `Render::Background`, `SnapshotBase` still in `jreng_glyph` — move to `jreng_opengl`
- `Glyph::Renderer` interface file (`jreng_glyph_renderer.h`) still exists — dead code, delete
- `juce::Font` deprecation warning in `TextLayout::Run` default constructor — pre-existing
- Rendering optimization (Plan 2.6) must complete before Plan 3 (CPU rendering)
- PLAN.md needs update to reflect module restructure and Font/Typeface split

---

## Sprint 115 — COUNSELOR: jreng_glyph Module Extraction + TextLayout

**Date:** 2026-03-22

### Agents Participated
- COUNSELOR: architecture decisions, extraction planning, audit review
- @pathfinder: codebase pattern discovery (module structure, FreeType/HarfBuzz integration, rendering code map)
- @engineer: all implementation (module creation, code extraction, TextLayout, GLTextRenderer, audit fixes)
- @librarian: JUCE TextLayout/AttributedString/GlyphArrangement API research
- @researcher: UAX #14 line breaking options (libunibreak selected)
- @auditor: comprehensive contract compliance audit (21 findings, all resolved)

### Files Modified (60+ total)

**New module: `modules/jreng_glyph/` (26 files)**
- `jreng_glyph.h` — module header, dependencies: jreng_opengl, jreng_core, jreng_freetype, juce_graphics, juce_opengl
- `jreng_glyph.cpp` — unity build
- `jreng_glyph.mm` — macOS unity build
- `atlas/jreng_atlas_packer.h` — `jreng::Glyph::AtlasPacker` (shelf bin packer)
- `atlas/jreng_glyph_key.h` — `jreng::Glyph::Key` (cache key)
- `atlas/jreng_atlas_glyph.h` — `jreng::Glyph::Region` (atlas region descriptor)
- `atlas/jreng_lru_glyph_cache.h` — `jreng::Glyph::LRUCache` (frame-based eviction)
- `atlas/jreng_staged_bitmap.h` — `jreng::Glyph::StagedBitmap` (cross-thread upload)
- `atlas_impl/jreng_glyph_atlas.h/.cpp/.mm` — `jreng::Glyph::Atlas` (dual atlas + rasterization)
- `constraint/jreng_glyph_constraint.h` — `jreng::Glyph::Constraint` (icon scaling)
- `constraint/jreng_glyph_constraint_table.cpp` — generated lookup table
- `constraint/jreng_constraint_transform.h` — shared `computeConstraintTransform()` (DRY fix)
- `drawing/jreng_box_drawing.h` — `jreng::Glyph::BoxDrawing` (procedural rasterizer)
- `font/jreng_font.h/.cpp/.mm` — `jreng::Font` (loading, shaping, metrics)
- `font/jreng_font_registry.cpp/.mm` — `jreng::Font::Registry` (codepoint-to-slot resolver)
- `font/jreng_font_metrics.cpp` — metrics calculation
- `font/jreng_font_shaping.cpp` — HarfBuzz shaping pipeline
- `render/jreng_glyph_render.h` — `jreng::Glyph::Render::Quad`, `Background`, `SnapshotBase`
- `render/jreng_glyph_shaders.h` — embedded shader sources
- `render/jreng_gl_text_renderer.h/.cpp` — `jreng::Glyph::GLTextRenderer` (instanced quad renderer)
- `layout/jreng_text_layout.h/.cpp` — `jreng::TextLayout` (proportional text layout engine)
- `linebreak/*` — vendored libunibreak (UAX #14, Unicode 15.0)
- `shaders/*` — glyph + background shader source files

**New module: `modules/jreng_freetype/` (3 files + vendored source)**
- `jreng_freetype.h/.cpp/.mm` — vendored FreeType 2.13.3
- `freetype/` — full FreeType source (forked from git submodule)

**Modified in `Source/`:**
- `Screen.h` — `Render::Snapshot` inherits `SnapshotBase`; GL members replaced with `GLTextRenderer`; `blockFirst/blockLast/blockTable` renamed
- `ScreenGL.cpp` — delegates to `textRenderer` for all GL operations
- `ScreenRender.cpp` — uses `jreng::Glyph::*` types
- `ScreenSnapshot.cpp` — uses `jreng::Glyph::*` types
- `Main.cpp` — owns `jreng::Font::Registry`
- `MainComponent.h/.cpp` — threads `Font&` reference
- `TerminalComponent.h/.cpp`, `Panes.h/.cpp`, `Tabs.h/.cpp` — `Font&` reference chain
- `CMakeLists.txt` — `jreng_freetype` and `jreng_glyph` added to JUCE_MODULES; old FreeType integration removed

**Deleted from `Source/terminal/rendering/`:**
- `AtlasPacker.h`, `BoxDrawing.h`, `GlyphConstraint.h`, `GlyphConstraintTable.cpp`
- `GlyphAtlas.h/.cpp/.mm`, `FontCollection.h/.cpp/.mm`
- `Fonts.h/.cpp/.mm`, `FontsShaping.cpp`, `FontsMetrics.cpp`

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered (audit-verified, all violations fixed)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced (audit-verified, all 21 findings resolved)

### Problems Solved
- FreeType vendored as JUCE module — no git submodule dependency
- HarfBuzz: using JUCE's internal copy (ARCHITECT decision — skip vendoring)
- Entire glyph rendering pipeline extracted into reusable `jreng_glyph` module
- `jreng::Font` singleton removed — lifecycle owned by application, passed by reference
- `jreng::Font::Registry` singleton removed — same pattern
- DRY violation fixed: constraint transform logic shared between FreeType/CoreText backends
- All control flow violations fixed (early returns, continue, break, boolean flags)
- All naming violations fixed (UNICODE_MAX, BLOCK_FIRST/LAST, etc.)
- libunibreak vendored for UAX #14 Unicode line breaking
- `jreng::TextLayout` created — HarfBuzz shaping + UAX #14 line breaking + GL instanced rendering

### Technical Debt / Follow-up
- `TextLayout::draw(juce::Graphics&)` is a stub — Plan 3 implements CPU rendering backend
- Emoji routing in `TextLayout::draw(GLTextRenderer&)` not implemented — all glyphs go to mono atlas
- `calcMetrics()` called per-glyph in GL draw inner loop — can be hoisted per-run
- Shader BinaryData wiring: shaders embedded as constexpr strings in module, but END's `ScreenGL.cpp` still loads from BinaryData (two copies)
- Fix 13 partial: 3 `.mm`/`.cpp` files have static helpers wrapped in namespace via qualified calls rather than full file namespace restructure
- `displayScale` only set from `setFontSize` — may need update from display-change handler
- Plan 2 residual: END already consumes module (destructive move), but shader loading path is dual
- Plans 3–4 (CPU rendering backend + END fallback) not started

---

## Sprint 114 — COUNSELOR: Resize Dead Space Fix + Link Underline Cleanup

**Date:** 2026-03-22

### Agents Participated
- COUNSELOR: root cause analysis, architecture decision
- @engineer: implementation

### Files Modified (5 total)
- `Source/terminal/data/State.h` — added `promptRow` atomic, `setPromptRow()`/`getPromptRow()` methods
- `Source/terminal/data/State.cpp` — implemented prompt row accessors
- `Source/terminal/logic/ParserESC.cpp` — OSC 133 A now stores cursor row as prompt position (was no-op)
- `Source/terminal/logic/Grid.h` — declared `fillDeadSpaceAfterGrow()`
- `Source/terminal/logic/GridReflow.cpp` — implemented `fillDeadSpaceAfterGrow()`: after resize grow on normal screen with OSC 133 active, pulls scrollback into visible area by adjusting ring buffer head backward, shifts cursor row down. Called between reflow and markAllDirty in `resize()`.
- `Source/component/TerminalComponent.cpp` — clear link underlay when scrolled back (`scrollOffset > 0`), prevents stale underline artifacts from viewport-relative spans surviving scroll

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- Resize dead space: inline TUI apps (Claude Code/Ink) left empty rows below the prompt after terminal grows. Now fills from scrollback using OSC 133 A prompt position as anchor.
- Link underline trails: stale LinkSpan row indices persisted across scroll, rendering underlines on wrong rows. Now cleared when scrollOffset > 0.

### Technical Debt / Follow-up
- Fresh session with zero scrollback won't benefit from dead space fill (no content to pull). Acceptable — scrollback fills quickly after first command.
- Prompt row tracking is per-session (State atomic), resets on session restart.

---

## Sprint 113 — COUNSELOR: Config Refactor + RRGGBBAA

**Date:** 2026-03-22

### Agents Participated
- COUNSELOR: architecture decisions, delegation
- @pathfinder: mapped Config load/colour/key systems
- @machinist: refactored Config.cpp, colour parser, default_end.lua

### Files Modified (3 total)
- `Source/config/Config.h` — added `addKey()` helper, `ValueSpec` moved to public, `mutable colourCache` member, removed `initDefaults()`/`initSchema()` declarations
- `Source/config/Config.cpp` — replaced `initDefaults()`+`initSchema()` with single `initKeys()` using `addKey()`, ANSI colours looped in `buildTheme()`, `load()` flattened (extracted `validateAndStore`/`loadPadding`/`loadPopups`, switch for type dispatch), `getColour()` caches parsed results, `parseColour()` reads RRGGBBAA (alpha at end), added `#RGBA` 4-char shorthand
- `Source/config/default_end.lua` — colour format docs updated to `#RRGGBB`/`#RRGGBBAA`, all default values use new format

### Alignment Check
- [x] LIFESTAR principles followed (SSOT: one `addKey()` per key, no parallel structures)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- Parallel `initDefaults()`/`initSchema()` eliminated — every key declared once
- 16 ANSI colour fan-out in `buildTheme()` replaced with array + loop
- `load()` 5-level nesting flattened to 3 extracted helpers + switch
- `getColour()` re-parsed strings on every call — now cached
- Colour format `#AARRGGBB` → `#RRGGBBAA` (CSS convention, alpha at end)

### Technical Debt / Follow-up
- Breaking change: existing end.lua with `#AARRGGBB` colours produce wrong results — delete and regenerate

---

## Sprint 112 — COUNSELOR: Nuke Per-Row Cell Cache

**Date:** 2026-03-22

### Agents Participated
- COUNSELOR: architecture decision (nuke vs fix)
- @pathfinder: mapped all dirty tracking and cache sites
- @machinist: removed cell cache, rewired buildSnapshot to read Grid directly

### Files Modified (4 total)
- `Source/terminal/rendering/Screen.h` — removed `hotCells`, `hotCellCount`, `coldGraphemes`, `hadSelection`, `wasScrolled`, `hadHintOverlay`, `hadLinkUnderlay`, `isRowDirty()`, `applyScrollOptimization()`, `populateFromGrid()`, renamed `allocateRowCache` → `allocateRenderCache`
- `Source/terminal/rendering/Screen.cpp` — `render()` simplified: no `consumeDirtyRows`/`consumeScrollDelta`, no scroll optimization, no dirty bit logic. Calls `buildSnapshot(state, grid)` directly.
- `Source/terminal/rendering/ScreenRender.cpp` — `buildSnapshot` reads `Grid::activeVisibleRow()`/`scrollbackRow()` directly every frame, every row. `processCellForSnapshot`/`buildCellInstance`/`tryLigature` receive row pointers as params.
- `Source/terminal/rendering/ScreenSnapshot.cpp` — signature updates for Grid parameter

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- Stale cell rendering artifacts from Ink CUU+EL+rewrite cycles — dirty bits consumed before new content arrived, cached cells shown until scroll forced full dirty sweep
- Eliminated entire class of dirty tracking bugs (races, scroll optimization shifts, transition bool workarounds)

### Technical Debt / Follow-up
- Grid::consumeDirtyRows/consumeScrollDelta still exist but unused by renderer — can be removed if no other callers
- Thread safety: message thread reads Grid cells while reader thread writes — same race as before, mitigated by ScopedTryLock on resize

---

## Sprint 111 — COUNSELOR: TerminalComponent Refactor

**Date:** 2026-03-22

### Agents Participated
- COUNSELOR: architecture decisions (State as SSOT, tell-don't-ask, extraction boundaries)
- @pathfinder: mapped all TerminalComponent responsibilities (9 groups, ~1600 lines)
- @machinist: extracted InputHandler, MouseHandler, LinkManager, moved state to parameterMap

### Files Modified (12 total)
- `Source/component/InputHandler.h/cpp` — NEW: modal key dispatch, selection keys, open-file keys, scroll nav, SelectionKeys cache, pendingG, reset()
- `Source/component/MouseHandler.h/cpp` — NEW: PTY forwarding, drag selection, double/triple-click, wheel scroll, hover cursor, scrollAccumulator
- `Source/terminal/selection/LinkManager.h/cpp` — NEW: viewport scan, hit-test, dispatch (editor/browser), OSC 8 span merging, hint label management
- `Source/component/TerminalComponent.h/cpp` — thin delegation shell (~300 lines from ~1600), JUCE overrides delegate to handlers, onVBlank builds ScreenSelection from State
- `Source/terminal/data/State.h/cpp` — added dragAnchorRow/Col, dragActive to parameterMap
- `Source/terminal/data/Identifier.h` — added drag state identifiers

### Alignment Check
- [x] LIFESTAR principles followed (SSOT: all state in State parameterMap)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- TerminalComponent god object split into 4 focused classes
- BoxSelection struct deleted — unified with State params
- Link dispatch duplicated in mouseDown + handleOpenFileKey — unified in LinkManager::dispatch
- Link underlines now driven by OSC 133 output block state in onVBlank, not mouseMove

### Technical Debt / Follow-up
- setScrollOffsetClamped duplicated between Component and InputHandler (different ownership domains)
- Mouse selection and keyboard VISUAL share State params but have different coordinate conversion needs

---

## Sprint 110 — COUNSELOR: Open File Mode, Hyperlinks, Shell Integration

**Date:** 2026-03-22

### Agents Participated
- COUNSELOR: plan updates, architecture decisions, step-by-step delegation
- @pathfinder: rendering pipeline insertion points, mouse/underline/cursor patterns
- @researcher: kitty/ghostty/wezterm shell integration mechanisms (ZDOTDIR, ENV, XDG_DATA_DIRS)
- @engineer: all implementation steps

### Files Modified (22 total)
- `Source/terminal/selection/SelectionOverlay.h` — renamed class SelectionOverlay → StatusBarOverlay, general-purpose modal status bar, added openFile case, terminal font, dynamic height
- `Source/terminal/selection/LinkDetector.h` — NEW: built-in extension set (60+), URL protocol detection, classify utility
- `Source/terminal/selection/LinkSpan.h` — NEW: link span data struct with labelCol for hint positioning
- `Source/terminal/rendering/Screen.h/cpp` — setHintOverlay, setLinkUnderlay for hint labels and click-mode underlines
- `Source/terminal/rendering/ScreenRender.cpp` — hint label cell override in processCellForSnapshot, link underline Background quads
- `Source/terminal/logic/Parser.h` — Osc8Span struct, OSC 8 span storage, handleOsc8/handleOsc133 declarations
- `Source/terminal/logic/ParserESC.cpp` — OSC 8 and OSC 133 A/B/C/D dispatch
- `Source/terminal/logic/ParserEdit.cpp` — clear OSC 8 spans on alternate screen switch
- `Source/terminal/data/State.h/cpp` — ModalType::openFile, output block tracking (top/bottom/scanActive)
- `Source/terminal/tty/UnixTTY.h/cpp` — addShellEnv/clearShellEnv for generalized env injection before execvp
- `Source/terminal/logic/Session.h/cpp` — applyShellIntegration (zsh/bash/fish/pwsh), getParser non-const overload
- `Source/component/TerminalComponent.h/cpp` — enterOpenFileMode, handleOpenFileKey, scanViewportForLinks, hint label assignment (filename-char-based), click-mode link dispatch, mouseMove hover cursor, drag threshold (2-cell), mouse selection separated from modal, copySelection by ScreenSelection type
- `Source/MainComponent.h/cpp` — StatusBarOverlay member, exitActiveTerminalSelectionMode on tab/pane switch
- `Source/component/LookAndFeel.h/cpp` — ColourIds renamed selectionBar → statusBar
- `Source/config/Config.h/cpp` — 8 new Key entries (hyperlinks.editor, shell.integration, hint label colours, status bar colours/position, enter_open_file)
- `Source/config/default_end.lua` — hyperlinks section, shell integration config, status bar colours, hint label colours, enter_open_file key
- `Source/terminal/action/Action.cpp` — enter_open_file action table entry
- `CMakeLists.txt` — .bash/.fish/.ps1 extensions added to binary data glob
- `Source/terminal/shell/zsh_zshenv.zsh` — NEW: ZDOTDIR wrapper
- `Source/terminal/shell/zsh_end_integration.zsh` — NEW: zsh autoload hooks
- `Source/terminal/shell/bash_integration.bash` — NEW: ENV/POSIX mode integration
- `Source/terminal/shell/fish/vendor_conf.d/end-shell-integration.fish` — NEW: XDG_DATA_DIRS vendor conf
- `Source/terminal/shell/powershell_integration.ps1` — NEW: prompt/PSReadLine hooks

### Architecture Decisions
- **Hint labels use filename characters** — first unique char from the filename, shift right on conflict. Single keystroke, always readable.
- **Click-drag separate from modal VISUAL** — no ModalType, direct ScreenSelection, 2-cell threshold
- **OSC 133 gates click mode** — underlines only on command output rows, prevents false positives on prompts
- **Shell integration via ZDOTDIR/ENV/XDG_DATA_DIRS** — same mechanisms as kitty/ghostty, zero visible output
- **Editor dispatch via direct PTY write** — `writeToPty("{editor} {path}\r")`, in-place execution
- **StatusBarOverlay polls State** — no manual callbacks, reads ModalType via onRepaintNeeded

### Alignment Check
- [x] LIFESTAR principles followed (SSOT: State owns modal/selection/output block state)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [x] All keys configurable via end.lua

### Problems Solved
- Open-file mode with vimium-style hint labels
- Always-on clickable hyperlinks with underlines and pointer cursor
- OSC 133 semantic prompt parsing for output block detection
- Automatic shell integration for 4 shells (zsh, bash, fish, PowerShell)
- OSC 8 explicit hyperlink protocol
- Mouse selection sensitivity (2-cell threshold) and modal separation

### Technical Debt / Follow-up
- TerminalComponent is becoming a god object — mouse/key/link handling should be extracted into InputHandler, MouseHandler, LinkManager
- Editor fallback chain (config currently single string, plan spec'd table of fallbacks)
- SelectionFinder (search in selection mode) — removed, needs proper reimplementation
- Link underline artifacts on screen transitions — cleared on dirty but may need per-row tracking
- OSC 133 output block boundaries are approximate after scroll — absolute position tracking deferred

---

## Sprint 109 — COUNSELOR: Vim-like Text Selection Mode

**Date:** 2026-03-21

### Agents Participated
- COUNSELOR: requirements, plan (PLAN-TEXT-SELECTION.md), architecture decisions, step-by-step delegation
- @pathfinder: existing selection/action/keybinding/scrollback/search/overlay/theme/cursor patterns
- @oracle: wrap-pending grapheme edge case analysis (xterm/kitty comparison)
- @engineer: all 10 implementation steps + fixes
- @auditor: (via Sprint 108)
- @machinist: (via Sprint 108)

### Files Modified (18 total)
- `Source/terminal/selection/SelectionType.h` — NEW: SelectionType enum (visual/visualLine/visualBlock)
- `Source/terminal/selection/SelectionOverlay.h` — NEW: full-width status bar component (-- VISUAL -- etc.)
- `Source/terminal/data/State.h` — ModalType enum, selection state params (type/cursor/anchor), convenience methods
- `Source/terminal/data/State.cpp` — parameterMap entries for selection state, ModalType via storeAndFlush
- `Source/terminal/data/Identifier.h` — 6 new IDs (modalType, selectionType, selectionCursor/AnchorRow/Col)
- `Source/terminal/rendering/ScreenSelection.h` — SelectionType enum, containsLine(), containsCell() dispatch
- `Source/terminal/rendering/ScreenRender.cpp` — containsBox → containsCell (type-aware hit test)
- `Source/terminal/rendering/Screen.h` — setSelectionCursor() for modal cursor override
- `Source/terminal/rendering/Screen.cpp` — setSelectionCursor implementation
- `Source/terminal/rendering/ScreenSnapshot.cpp` — selection cursor rendering (block shape, configurable color, no blink)
- `Source/component/TerminalComponent.h` — SelectionKeys cache, handleModalKey/handleSelectionKey/enterSelectionMode/exitSelectionMode, pendingG
- `Source/component/TerminalComponent.cpp` — full modal key dispatch, all selection operations, mouse integration (drag/double/triple-click), copy to clipboard, updateSelectionHighlight, buildSelectionKeyMap
- `Source/component/Panes.h/cpp` — onLastPaneClosed callback, onShellExited wiring
- `Source/component/Tabs.h/cpp` — onLastPaneClosed wiring, exitActiveTerminalSelectionMode on tab switch
- `Source/MainComponent.h` — SelectionOverlay member, exitActiveTerminalSelectionMode
- `Source/MainComponent.cpp` — overlay positioning, poll State via onRepaintNeeded, exit modal on tab/pane switch, enter_selection action registration
- `Source/config/Config.h` — 20 new Key entries (selection keys, colours, bar position)
- `Source/config/Config.cpp` — defaults and schema for all new keys
- `Source/config/default_end.lua` — full selection mode key section, selection bar colours
- `Source/component/LookAndFeel.h/cpp` — 3 SelectionOverlay ColourIds

### Architecture Decisions
- **ModalType in State (parameterMap)** — general-purpose modal gate, not selection-specific. Future flashJump/uriAction reuse same enum and dispatch
- **All selection state in State** — no duplicate SelectionMode class. Type, cursor, anchor stored as parameterMap params. SSOT enforced
- **All keys user-configurable** — Config + Lua + SelectionKeys cache. Zero hardcoded characters
- **Modal intercept BEFORE Action system** — solves Ctrl+V conflict (visual-block, not paste)
- **No manual callbacks** — MainComponent polls State via existing onRepaintNeeded VBlank. ValueTree flush handles propagation
- **SelectionFinder deferred** — GlassWindow search bar had parenting/focus issues. Removed entirely, search keys are consumed stubs

### Alignment Check
- [x] LIFESTAR principles followed (SSOT: State owns all selection state, no duplicate)
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [x] All keys configurable via end.lua

### Problems Solved
- Vim-like modal text selection with visual/visual-line/visual-block modes
- Mouse integration: click-drag (streaming), double-click (word), triple-click (line)
- Copy to clipboard via all three selection types (extractText for linear/line, extractBoxText for block)
- Full-width status bar overlay with configurable position/colours/font
- Shell exit wired to Cmd+W hierarchy (pane → tab → window)
- Tab/pane switch exits modal state cleanly

### Technical Debt / Follow-up
- SelectionFinder (search in selection mode) — deferred, needs proper implementation
- Screen::setSelectionCursor still called from onVBlank — could read State directly in ScreenSnapshot
- `parseShortcut("ctrl+v")` maps ctrl→cmd on macOS — works because modal intercept catches it first, but parseShortcut should distinguish ctrl from cmd long-term

---

## Sprint 108 — COUNSELOR: Technical Debt Cleanup

**Date:** 2026-03-21

### Agents Participated
- COUNSELOR: audit coordination, debt triage
- @auditor: comprehensive codebase audit (contract adherence, SSOT, dead code, stale doxygen)
- @engineer: StringSlot SeqLock, shell exit wiring, performExitAction removal, SSOT extractions
- @machinist: 20-item production polish (DBG removal, early returns, brace init, operator tokens, doxygen)

### Files Modified (12 total)
- `Source/terminal/data/State.h/cpp` — StringSlot SeqLock (data race fix), writeStringSlot SSOT helper, snapshotDirty write path unified
- `Source/terminal/logic/Session.h/cpp` — removed buffer ownership (moved to State), stack-local buffers, shell exit callback
- `Source/terminal/logic/Parser.h/cpp` — removed performExitAction (dead code), removed titleBuffer/cwdBuffer
- `Source/terminal/logic/ParserESC.cpp` — OSC title/CWD pass data+length to State, stale doxygen updated
- `Source/terminal/logic/ParserCSI.cpp` — effectiveClampBottom SSOT helper
- `Source/terminal/logic/ParserVT.cpp` — wrap-pending grapheme targeting fix
- `Source/component/TerminalComponent.h/cpp` — setScrollOffsetClamped SSOT helper, mouseWheelMove early returns fixed, shell exit wiring
- `Source/component/Panes.h/cpp` — onShellExited/onLastPaneClosed callbacks
- `Source/component/Tabs.cpp` — onLastPaneClosed → closeActiveTab + quit
- `Source/terminal/tty/TTY.cpp` — brace init, `not` operator, lambda formatting

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- Data race on string passing (title/CWD/foreground process) — SeqLock-style StringSlot
- Shell exit kills entire app → now follows pane/tab/window hierarchy
- 3x duplicated scrollback offset logic → setScrollOffsetClamped SSOT
- 2x duplicated margin check → effectiveClampBottom SSOT
- snapshotDirty bypass → unified write path through setSnapshotDirty
- performExitAction dead code removed
- Wrap-pending grapheme append targeted wrong cell
- 14 diagnostic DBGs removed
- Early returns converted to nested positive checks

### Technical Debt / Follow-up
- Terminal::State serialization — deferred until END features complete
- `mouseWheelMove` discrete/smooth paths still have structural duplication (logic differs enough to not extract)

---

## Sprint 107 — COUNSELOR: CC Status Bar Artifact — SOLVED

**Date:** 2026-03-21

### Agents Participated
- COUNSELOR: investigation lead, root cause analysis, delegated to all specialists
- @oracle: VT handler audits (CUU/CUF/EL/ED/CUP, sync mode pipeline, scroll delta, new CSI handlers), rendering pipeline trace, dirty-flag-to-render analysis
- @pathfinder: character width lookup, cell struct, charprops table, wcwidth analysis
- @researcher: Claude Code status line mechanism, DECSSDT/DECSASD research, xterm alternate screen spec
- @librarian: xterm spec for alternate screen, DECSTBM, scroll region per-screen behavior
- @engineer: implementations (bug fixes, diagnostic DBGs)

### Root Cause

**8-bit C1 control 0x9C (ST) in oscString state conflicts with UTF-8 continuation bytes.**

Claude Code sets the window title via `OSC 0 ; ◜ Claude Code BEL`. The icon "◜" encodes in UTF-8 as bytes including 0x9C. The oscString dispatch table treated 0x9C as String Terminator (legacy VT220 C1 control), aborting the OSC mid-character. The remaining payload `" Claude Code"` leaked into ground state as printed text, shifting the cursor right by 13 columns. The built-in status bar was then written at col 15 instead of col 2, and the carol-statusline's `CSI 1C` gaps exposed the mispositioned content.

### Files Modified (6 total)
- `Source/terminal/data/DispatchTable.h:424-446` — removed 0x9C (8-bit ST) overrides from oscString, dcsPassthrough, dcsIgnore, sosPmApcString states; added `fillRange(0x80, 0xFF, oscPut)` to oscString for UTF-8 payload support
- `Source/terminal/logic/Parser.cpp:550` — reset `utf8AccumulatorLength` on escape state entry (sprint 106 carry-forward)
- `Source/terminal/logic/ParserCSI.cpp:840` — added DA2 response (`CSI > 65;100;0 c`)
- `Source/terminal/logic/ParserESC.cpp:324` — route `)` intermediate to `escDispatchCharset` (G1 charset)
- `Source/terminal/logic/ParserESC.cpp:419` — OSC title truncation respects UTF-8 character boundaries
- `Source/terminal/logic/ParserESC.cpp:517` — OSC 52 clipboard uses `String::fromUTF8()` instead of Latin-1 constructor
- `Source/component/TerminalComponent.cpp:745` — re-arm `snapshotDirty` when `ScopedTryLock` fails (frame drop prevention)
- `Source/terminal/tty/UnixTTY.cpp:84` — set `COLORTERM=truecolor` for child processes

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied

### Problems Solved
- CC status bar artifact: "C◈ CAROL dOpus 4.6" — OSC title payload with UTF-8 icon leaked into grid due to 0x9C being treated as 8-bit ST instead of UTF-8 continuation byte
- Frame drops on resize lock contention (VBlank consumed dirty flag without render)
- OSC strings with UTF-8 payloads (titles, hyperlinks) silently aborted
- DCS/SOS/PM/APC strings similarly vulnerable to 0x9C in UTF-8 content

### Technical Debt / Follow-up
- Remove all diagnostic DBGs before commit (PTY dump in TTY.cpp, cell dumps in ParserCSI.cpp, cursor traces in ParserVT.cpp/ParserCSI.cpp/ParserESC.cpp/ParserEdit.cpp, SYNC-END in State.cpp)
- Data race on cwdBuffer/titleBuffer (Session.cpp:440-448, State.cpp:912-916) — raw `char*` read on message thread without mutex while reader thread writes — not blocking but should be addressed
- Sprint 106 committed changes (REP/CBT/CHT/HPR/VPR/HPA, XTVERSION, drag-drop, LF/CUU fixes) remain valid

## Sprint 106 — COUNSELOR: Parser Handlers + Scroll Region Fixes + Drag-Drop + Artifact Investigation

**Date:** 2026-03-21

### Agents Participated
- COUNSELOR: investigation lead, root cause analysis
- @oracle: kitty vs END parsing comparison, byte-level trace
- @pathfinder: initialization sequence, dirty tracking
- @researcher: kitty/wezterm/ghostty architecture comparison
- @engineer: implementations

### Committed (independently good)
- REP (CSI b), CBT (CSI Z), CHT (CSI I), HPR (CSI a), VPR (CSI e), HPA (CSI `)
- XTVERSION response (ESC[>0q → DCS >|END(1.0) ST)
- File drag-and-drop (FileDragAndDropTarget, Config: drop_multifiles, drop_quoted)
- LF `>=` → `==` in handleLineFeed, flushPrintRun, resolveWrapPending, wide char wrap
- CUU/CUD margin-aware clamping (withinMargins check)
- cursorGoToNextLine handles row > scrollBottom
- cursorMoveUp uses effectiveScrollBottom (not raw sentinel)
- UTF-8 accumulator reset on escape state entry

### CC Rendering Artifact — SOLVED in Sprint 107

---

## Sprint 105: Remove hotCells Cache — Read Grid Directly Every Frame ✅

**Date:** 2026-03-21

### Agents Participated
- SURGEON: led implementation
- @Pathfinder: discovered Grid cell-read API, Cell struct, hotCells layout, populateFromGrid logic, buildSnapshot/updateSnapshot/tryLigature call chains
- @Engineer: n/a — SURGEON implemented directly

### Files Modified (3 total)
- `Source/terminal/rendering/Screen.h:961-963` — removed `hotCells`, `hotCellCount`, `coldGraphemes` member declarations
- `Source/terminal/rendering/Screen.h:792,811,825,843,862,883` — removed `applyScrollOptimization` and `populateFromGrid` declarations; updated signatures of `buildSnapshot` (added `const Grid&`), `processCellForSnapshot` (added `rowCells`, `rowGraphemes`), `buildCellInstance` (added `rowCells`), `tryLigature` (added `rowCells`)
- `Source/terminal/rendering/Screen.cpp:288-295` — `reset()`: removed hotCells/coldGraphemes fill; resets cacheRows/cacheCols/bgCacheCols only
- `Source/terminal/rendering/Screen.cpp:358-395` — `render()`: removed hotCellCount allocation block; removed `applyScrollOptimization` call; collapsed scroll cases — any scroll (`scroll > 0`) marks all rows dirty; passes `grid` to `buildSnapshot`
- `Source/terminal/rendering/Screen.cpp` — removed `applyScrollOptimization()` function entirely
- `Source/terminal/rendering/Screen.cpp` — removed `populateFromGrid()` function entirely
- `Source/terminal/rendering/ScreenRender.cpp:397-428` — `buildSnapshot()`: added `const Grid& grid` param; reads `activeVisibleRow`/`scrollbackRow` and grapheme equivalents per dirty row; passes `rowCells`/`rowGraphemes` to `processCellForSnapshot`
- `Source/terminal/rendering/ScreenRender.cpp:313-375` — `processCellForSnapshot()`: added `rowCells`, `rowGraphemes` params; reads grapheme from `rowGraphemes[col]` instead of `coldGraphemes[row*cacheCols+col]`; passes `rowCells` to `buildCellInstance`
- `Source/terminal/rendering/ScreenRender.cpp:494-701` — `buildCellInstance()`: added `rowCells` param; cellSpan lookahead uses `rowCells[col+1]` instead of `hotCells[nextIndex]` (removed `hotCellCount` bounds check, replaced with `col+1 < cacheCols`); passes `rowCells` to `tryLigature`
- `Source/terminal/rendering/ScreenRender.cpp:732-805` — `tryLigature()`: added `rowCells` param; removed `base` index computation; reads cells as `rowCells[col+i]` and `rowCells[col].style` directly

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied — no defensive flags, no manual booleans, positive checks only, no magic numbers

### Problems Solved
Stale `hotCells` render cache eliminated. Root causes:
1. Race condition between reader thread (Grid writes) and message thread (`populateFromGrid` reads) — gone: no copy, Grid read directly in `buildSnapshot` under the existing lock context
2. Scroll optimization propagating stale cached rows — gone: `applyScrollOptimization` removed; any scroll marks all rows dirty
3. Cursor blink mid-drain consuming dirty bits before data fully written — mitigated: no intermediate cell buffer to go stale; per-row glyph cache (cachedMono/cachedEmoji) is still incremental but is rebuilt from live Grid data

### Technical Debt / Follow-up
- Per-row glyph cache (cachedMono/cachedEmoji/cachedBg) is still incremental — unchanged rows are not reshaped. This is correct because dirty bits gate the rebuild. If cursor blink issue resurfaces (dirty bits consumed before data ready), the fix is in Session's drain gate, not here.
- Scroll now marks all rows dirty (no glyph cache shift). Slightly more reshape CPU on scroll vs the old scroll optimization. Acceptable per COUNSELOR analysis (kitty/wezterm model).
- Acceptance criteria from COUNSELOR handoff to be verified by ARCHITECT via manual testing.

---

## Handoff to SURGEON: Normal-Screen Rendering Artifacts

**From:** COUNSELOR
**Date:** 2026-03-21

### Problem
TUI apps (Claude Code) on the normal screen show stale cell artifacts — residue from overwritten text persists in the render cache. Characters like `C` and `d` from "Claude Code" appear where "CAROL" should have fully overwritten them. Pig art glyphs break on subsequent runs. Scrolling up/down fixes broken cells (forces full re-read), proving the grid is correct but the render cache is stale.

This NEVER happens on kitty, wezterm, or ghostty at the same cell dimensions.

### Root Cause Analysis (COUNSELOR findings)

**The design flaw:** END has a `hotCells` render cache with per-row dirty-bit partial updates + scroll optimization. The three working terminals do NOT:
- **kitty/wezterm:** No intermediate cell cache. Read all cells from screen buffer every frame.
- **ghostty:** Has row cache but uses page-level dirty flag — full rebuild on any page change.

END's `hotCells` cache can become stale when:
1. `populateFromGrid()` reads the grid while the reader thread is writing to it (no lock)
2. Scroll optimization shifts stale cached rows, propagating the staleness
3. Cursor blink timer triggers mid-drain renders that consume dirty bits before data is fully written

The scroll optimization (`applyScrollOptimization`) assumes the cache is always in sync with the grid. When it isn't, the optimization propagates stale data.

### Files to Checkout (clean baseline)

SURGEON must restore these 7 files to the last known-good state (`b5d0d85`)
before starting any fix. Current HEAD has broken experimental changes stacked
on top. The good parsers/features (REP, CBT, drag-drop, XTVERSION) are in
separate files and won't be affected.

```
git checkout b5d0d85 -- Source/terminal/logic/Grid.cpp
git checkout b5d0d85 -- Source/terminal/logic/Session.cpp
git checkout b5d0d85 -- Source/terminal/logic/Session.h
git checkout b5d0d85 -- Source/terminal/data/State.h
git checkout b5d0d85 -- Source/terminal/data/State.cpp
git checkout b5d0d85 -- Source/terminal/rendering/Screen.cpp
git checkout b5d0d85 -- Source/terminal/rendering/ScreenSnapshot.cpp
```

### Recommended Solution

Study the three reference implementations:
- kitty: `/Users/jreng/Documents/Poems/dev/kitty/kitty/screen.c` (screen_update_cell_data, no cache)
- wezterm: `/Users/jreng/Documents/Poems/dev/wezterm/wezterm-gui/src/` (dynamic viewport range, no cache)
- ghostty: `/Users/jreng/Documents/Poems/dev/ghostty/src/terminal/render.zig` (page-level dirty)

Two approaches:
- **A. Remove hotCells cache** — read all cells from Grid every frame (kitty/wezterm model). Correct by construction. Slightly more CPU.
- **B. Page-level dirty** — when ANY write happens, mark ALL rows dirty. Keep cache but disable scroll optimization. (ghostty model).

Both require proper locking between reader thread (writes to Grid) and message thread (reads from Grid in populateFromGrid). The `resizeLock` with `ScopedTryLock` in onVBlank was the correct pattern — needs to be held during `Session::process()`.

### Acceptance Criteria
- [ ] `cd end && claude` — clean rendering, no stale artifacts
- [ ] Exit CC, run again — no broken cells
- [ ] Scroll up — no previously broken cells in scrollback
- [ ] Same result at any window size
- [ ] No regression in alternate-screen TUIs (vim, htop)
- [ ] Performance acceptable (no visible frame drops during `seq 100000`)

### Notes
- The SSOT calc (physical pixel cell dimensions) is confirmed correct — keep it
- Mode 2026 parser handler is correct — keep it
- ICH/DCH markRowDirty is correct — keep it
- The problem is ONLY in the render pipeline: Screen.cpp, Grid→hotCells→snapshot path
- COUNSELOR attempted ~15 experimental fixes that were stacked without baseline testing. All reverted. Start clean.

---

## Sprint 104 — Terminal Rendering Pipeline: SSOT Cell Dimensions + Sync Mode 2026 + ICH/DCH Dirty ✅

**Date:** 2026-03-20

### Agents Participated
- COUNSELOR: root cause analysis, pipeline trace, design (SSOT from physical pixels)
- @pathfinder: cursor rendering pipeline, dirty tracking investigation, initialization trace
- @oracle: deterministic artifact investigation, PTY data analysis
- @researcher: kitty comparison, CC rendering architecture
- @engineer: all implementations

### Problems Solved

**1. SSOT cell dimensions (root cause)**
Screen::calc() computed numCols/numRows from logical pixels while GL renders in physical. Independent rounding between logical→physical paths caused grid dimensions to exceed what physically fits in the GL viewport. TUI apps received wrong dimensions, producing overlapping text. Fix: physical pixels are now the Single Source of Truth. numCols = glViewportWidth / physCellWidth. Logical derived backward.

**2. Cursor glyph .notdef**
shapeEmoji() returns count > 0 even for .notdef (glyph index 0). Non-emoji codepoints routed through emoji atlas, rendering Apple Color Emoji .notdef rectangle. Fix: isEmoji guard requires glyphIndex != 0. FontCollection resolution added for NF icons.

**3. ICH/DCH missing dirty marks**
shiftCellsRight() and removeCells() wrote directly to grid via activeVisibleRow() without calling markRowDirty(). Cells modified by Insert/Delete Characters never flagged dirty. Renderer showed stale cached content. Fix: markRowDirty() added after cell modifications.

**4. Synchronized output mode 2026**
END had no mode 2026 support. TUI apps using sync blocks (Claude Code) had intermediate render states visible between blocks. Fix: parser handles ESC[?2026h/l, updateSnapshot() holds GL snapshot publication during sync.

**5. Deferred TTY open**
tty->open() fired on first resized() with preliminary JUCE layout bounds. PTY received wrong initial dimensions. Fix: deferred via callAsync to ensure layout completes.

**6. Sync resize nudge**
First mode 2026 activation triggers requestResize on drain complete, correcting any PTY dimension drift from actual grid.

**7. XTVERSION response**
ESC[>0q now responds with DCS >|END(1.0) ST. Terminal identification for applications.

### Files Modified (8 total)
- `Source/terminal/rendering/Screen.cpp:54-76` — SSOT calc() from physical pixels
- `Source/terminal/rendering/ScreenSnapshot.cpp:168-226` — cursor glyph .notdef guard + FontCollection + sync gate
- `Source/terminal/logic/ParserEdit.cpp:372,427` — markRowDirty for ICH/DCH
- `Source/terminal/logic/ParserCSI.cpp:885-891,208` — mode 2026 handler + XTVERSION response
- `Source/terminal/logic/Grid.cpp:199,222,241` — setSnapshotDirty restored in dirty markers
- `Source/terminal/data/State.h` — syncOutputActive, syncResizePending, setSyncOutput, requestSyncResize, consumeSyncResize
- `Source/terminal/data/State.cpp` — sync method implementations
- `Source/terminal/logic/Session.cpp:191-224,68-73` — deferred tty->open + sync resize in onDrainComplete

### Alignment Check
- [x] LIFESTAR: Single Source of Truth (physical pixels), Lean (no redundant calculations)
- [x] NAMING-CONVENTION: no new naming violations
- [x] ARCHITECTURAL-MANIFESTO: GL thread reads only immutable snapshot, reader/message thread separation preserved

### Technical Debt / Follow-up
- `Fonts::calcMetrics()` still computes logical and physical independently — could derive logical from physical in the metrics itself for full SSOT at the font level
- Mode 2026 shadow buffer (kitty-style paused_rendering with full linebuf copy) not implemented — current hold-snapshot approach is sufficient but less robust
- The `ttyOpenPending` bool in Session is a manual flag — consider if lifecycle can be simplified

---

## Sprint 103 — Cursor Glyph Rendering: Any Codepoint as Cursor ✅

**Date:** 2026-03-20

### Agents Participated
- COUNSELOR: root cause analysis, pipeline trace, spec
- @pathfinder: full cursor rendering pipeline exploration
- @engineer: implementation

### Problem Solved

Cursor rendered as a small outlined black rectangle (.notdef from Apple Color Emoji) regardless of `cursor.char`. Root cause: `ScreenSnapshot::updateSnapshot()` called `shapeEmoji()` first for all codepoints. HarfBuzz always returns `count > 0` — even when the font lacks the codepoint it returns `.notdef` (glyph index 0). The `isEmoji` check trusted `count > 0` alone, routing every non-emoji codepoint (including plain "a") through the emoji atlas, producing .notdef.

Additionally, the non-emoji branch had no FontCollection resolution — NF icons would have been looked up against the wrong font.

### Files Modified (1 total)
- `Source/terminal/rendering/ScreenSnapshot.cpp:31` — added `#include "FontCollection.h"`
- `Source/terminal/rendering/ScreenSnapshot.cpp:169-172` — `isEmoji` guard now requires `glyphIndex != 0` to exclude .notdef
- `Source/terminal/rendering/ScreenSnapshot.cpp:182-226` — non-emoji branch: FontCollection resolution first (NF icons), shapeText fallback second

### Rendering Contract Enforced
- **Procedural** (box/braille U+2500–U+28FF): geometric quad — `drawCursor()` fallback, unchanged
- **True emoji**: `shapeEmoji` + `glyphIndex != 0` → RGBA atlas, native colour preserved
- **NF icons**: FontCollection `resolve()` → `hb_font_get_nominal_glyph()` → mono atlas
- **Regular chars**: `shapeText(regular)` → mono atlas, drawn in cursor theme colour

### Alignment Check
- [x] LIFESTAR: no new state, positive checks only, mirrors existing `buildCellInstance` priority chain
- [x] NAMING-CONVENTION: no new symbols
- [x] ARCHITECTURAL-MANIFESTO: GL thread reads only immutable snapshot data — cursor glyph resolved entirely on message thread

### Technical Debt / Follow-up
- `cursor.force = false` means shells can still override cursor shape via DECSCUSR — user glyphs only render when `cursorShape == 0`. This is correct behaviour per spec.
- NF icon cursor rendering untested (requires an NF icon as `cursor.char`).

---

## Sprint 91 — WindowsTTY Rewrite + ConPTY Mouse Investigation

**Date:** 2026-03-15  
**Agents:** COUNSELOR, @engineer, @pathfinder, @researcher, @auditor

### Problem

Windows implementation had 5 critical issues: mouse outputting garbage, poor performance (`seq 1M` = 4m16s), cursor twitching, crash on forced quit, OMP extra newlines. Root cause: `WindowsTTY` was built without a working reference — two anonymous pipes, `PeekNamedPipe` + `Sleep(1)` polling, no overlapped I/O. Fundamentally different pipe topology from Microsoft Terminal.

### What Was Done

**1. WindowsTTY rewritten from scratch**
- Ported Microsoft Terminal's `ConptyConnection` pipe topology exactly
- Single duplex unnamed pipe via `NtCreateNamedPipeFile` (same NT API Microsoft Terminal uses)
- Client opened via `NtCreateFile` relative to server handle — true full-duplex, no contention
- Overlapped I/O for both read and write — zero CPU when idle, instant wake on data
- `read()` issues immediate overlapped reads with zero-timeout to drain multiple chunks without re-entering `waitForData()`
- Clean shutdown: `ClosePseudoConsole` while reader alive → `stopThread` → `TerminateProcess` as last resort
- Performance: `seq 1M` improved from 4m16s to 2m33s

**2. Parser::resize() — stale wrapPending fix**
- Added `setWrapPending (false)` + `cursorClamp()` for both screens before resetting scroll regions
- Fixes OMP extra newline at full terminal width on resize

**3. State::getActiveScreen() — message-thread ValueTree reader**
- Added `getActiveScreen()` that reads from ValueTree (post-flush)
- Fixed `mouseWheelMove`, `shouldForwardMouseToPty()`, `getTreeKeyboardFlags()`, `getCursorState()`, `onVBlank` to use `getActiveScreen()` instead of `getScreen()` (atomic)
- `getScreen()` remains for reader-thread callers (Parser, Grid, ScreenRender, ScreenSnapshot)

**4. timeBeginPeriod(1) — Windows timer resolution**
- Added `timeBeginPeriod(1)` in `initialise()`, `timeEndPeriod(1)` in `shutdown()` (after teardown)
- Unlocks 1ms timer resolution for state flush timer

### ConPTY Mouse Investigation — UNRESOLVED

**Finding:** ConPTY on Windows 10 22H2 (build 19045) intercepts ALL of the following from the child's output and never forwards them to the terminal emulator:
- `ESC[?1049h/l` (alternate screen) — `activeScreen` is always `normal`
- `ESC[?1000h/l` (mouse tracking) — `isMouseTracking()` is always `false`
- `ESC[?1002h/l` (motion tracking) — never seen
- `ESC[?1003h/l` (all tracking) — never seen
- `ESC[?1006h/l` (SGR mouse) — never seen

**Confirmed via file logging:** Parser's `setScreen()` is never called. `applyPrivateModeTable` never receives mode 1000/1002/1003/1006/1049. ConPTY renders alternate screen internally and sends the result as normal-screen output.

**Forwarding blindly (`return true`)** causes ConPTY to echo SGR sequences back as raw text — rendered as red boxes (unknown glyphs).

**Status:** Mouse on Windows requires a fundamentally different approach. Possible directions:
- Win32 Input Mode (`?9001`) — ConPTY's own input protocol
- `WriteConsoleInput` with `MOUSE_EVENT_RECORD` — bypass pipe, use console API
- Heuristic screen detection (full-screen redraw patterns)
- Research how Windows Terminal's `ControlInteractivity` handles mouse → ConPTY

### Files Modified

- `Source/terminal/tty/WindowsTTY.h` — complete rewrite (277 lines)
- `Source/terminal/tty/WindowsTTY.cpp` — complete rewrite (974 lines)
- `Source/terminal/logic/Parser.cpp:179-189` — resize wrapPending + cursorClamp
- `Source/terminal/logic/ParserEdit.cpp` — diagnostic added/removed (clean)
- `Source/terminal/logic/ParserCSI.cpp` — diagnostic added/removed (clean)
- `Source/terminal/data/State.h` — added `getActiveScreen()` declaration
- `Source/terminal/data/State.cpp` — added `getActiveScreen()` implementation, fixed `getTreeKeyboardFlags()` + `getCursorState()`
- `Source/component/TerminalComponent.cpp` — `mouseWheelMove`, `shouldForwardMouseToPty()`, `onVBlank` use `getActiveScreen()`, mouse handlers gated, diagnostics added/removed (clean)
- `Source/Main.cpp` — `timeBeginPeriod(1)` / `timeEndPeriod(1)` with extern declarations

### Alignment Check

- **LIFESTAR:** Lean (single pipe replaces two), Explicit (overlapped I/O model documented in header), Single Source of Truth (`getActiveScreen` reads ValueTree, `getScreen` reads atomic — clear separation), Reviewable (docstrings match UnixTTY pattern)
- **NAMING-CONVENTION:** `getActiveScreen` — semantic name (Rule 3), no data-source encoding (Rule 2). Previous `getTreeMode`/`getTreeKeyboardFlags` violate Rule 2 (encode "Tree" in name) — pre-existing debt, not introduced here
- **ARCHITECTURAL-MANIFESTO:** TTY layer stays dumb — no knowledge of parser, grid, or UI. Session writes bytes, TTY delivers bytes. Explicit Encapsulation preserved.

### Technical Debt / Follow-up

- **CRITICAL: Mouse on Windows is non-functional.** ConPTY intercepts all DECSET mouse/screen sequences. Needs research into Win32 Input Mode or WriteConsoleInput approach. Reference: `terminal/src/cascadia/TerminalControl/ControlInteractivity.cpp`
- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** violates NAMING-CONVENTION Rule 2 — encodes data source ("Tree") in name. Should be renamed to semantic names in a future sprint.
- **`seq 1M` is 2m33s vs Windows Terminal's 1m12s** — 2x gap remains. Reader thread CPU is 33% (should be higher). The zero-timeout `WaitForSingleObject` in `read()` exits the drain loop too early when data hasn't arrived in that instant. Needs a short timeout (1-2ms) or a different drain strategy.
- **`shouldForwardMouseToPty()` docstring** still references alternate screen fallback that was removed. Needs update.
- **`CursorComponent` does not call `setInterceptsMouseClicks(false, false)`** — cursor child swallows clicks on the cursor cell. Trivial fix but blocked by mouse being non-functional anyway.

---


**[N]** = Sprint Number (e.g., `1`, `2`, `3`...)

**Sprint:** A discrete unit of work completed by one or more agents, ending with user approval ("done", "good", "commit")

---

## ⚠️ CRITICAL RULES

**AGENTS BUILD CODE FOR USER TO TEST**
- Agents never build.
- USER build and tests and provides feedback
- Agents wait for user approval before proceeding

**AGENTS NEVER RUN GIT COMMANDS**
- Write code changes without running git commands
- Agent runs git ONLY when user explicitly requests
- Never autonomous git operations
- **When committing:** Always stage ALL changes with `git add -A` before commit
  - ❌ DON'T selectively stage files (agents forget/miss files)
  - ✅ DO `git add -A` to capture every modified file

**SPRINT-LOG WRITTEN BY PRIMARY AGENTS ONLY**
- **COUNSELOR** or **SURGEON** write to SPRINT-LOG
- Only when user explicitly says: `"log sprint"`
- No intermediate summary files
- No automatic logging after every task
- Latest sprint at top, keep last 5 entries

**NAMING RULE (CODE VOCABULARY)**
- All identifiers must obey project-specific naming conventions (see NAMING-CONVENTION.md)
- Variable names: semantic + precise (not `temp`, `data`, `x`)
- Function names: verb-noun pattern (initRepository, detectCanonBranch)
- Struct fields: domain-specific terminology (not generic `value`, `item`, `entry`)
- Type names: PascalCase, clear intent (CanonBranchConfig, not BranchData)

**Contract (STRICT — enforced on ALL code):**
- Pre-increment: ++i not i++
- Brace initialization: int x { 0 }; not int x = 0;
- Use .at() for array/container access, NEVER [] (raw pointer [] access is OK for HeapBlock, C APIs)
- Use keywords and/or/not instead of &&/||/!
- Allman braces, space after function name ONLY when it has arguments: foo() but foo (x, y), */& stick to type, const before type
- noexcept where applicable, explicit single-arg constructors
- 300-30-3: max ~300 lines per file (~400 tolerable), 30 lines per function (33 = 10% tolerance), 3 conditional branches per function
- NO trailing underscore suffixes on member names
- NO improvisation. Follow the plan exactly. Ask ARCHITECT if uncertain.
- ValueTree is the only SSOT. No shadow structs, no duplicate state.
- Do NOT create new data types without ARCHITECT approval.
- No unnecessary classes.

**BEFORE CODING: ALWAYS SEARCH EXISTING PATTERNS**
- ❌ NEVER invent new states, enums, or utility functions without checking if they exist
- ✅ Always grep/search the codebase first for existing patterns
- ✅ Check types, constants, and error handling patterns before creating new ones
- **Methodology:** Read → Understand → Find SSOT → Use existing pattern

**TRUST THE LIBRARY, DON'T REINVENT**
- ❌ NEVER create custom helpers for things the library/framework already does
- ✅ Trust the library/framework - it's battle-tested

**FAIL-FAST RULE (CRITICAL)**
- ❌ NEVER silently ignore errors (no error suppression)
- ❌ NEVER use fallback values that mask failures
- ❌ NEVER return empty strings/zero values when operations fail
- ❌ NEVER use early returns
- ✅ ALWAYS check error returns explicitly
- ✅ ALWAYS return errors to caller or log + fail fast

**⚠️ NEVER REMOVE THESE RULES**
- Rules at top of SPRINT-LOG.md are immutable
- If rules need update: ADD new rules, don't erase old ones

---

## Quick Reference

### For Agents

**When user says:** `"log sprint"`

1. **Check:** Did I (PRIMARY agent) complete work this session?
2. **If YES:** Write sprint block to SPRINT-LOG.md (latest first)
3. **Include:** Files modified, changes made, alignment check, technical debt

### For User

**Activate PRIMARY:**
```
"@CAROL.md COUNSELOR: Rock 'n Roll"
"@CAROL.md SURGEON: Rock 'n Roll"
```

**Log completed work:**
```
"log sprint"
```

**Invoke subagent:**
```
"@oracle analyze this"
"@engineer scaffold that"
"@auditor verify this"
```

**Available Agents:**
- **PRIMARY:** COUNSELOR (domain specific strategic analysis), SURGEON (surgical precision problem solving)
- **Subagents:** Pathfinder, Oracle, Engineer, Auditor, Machinist, Librarian

---

<!-- SPRINT HISTORY STARTS BELOW -->
<!-- Latest sprint at top, oldest at bottom -->
<!-- Keep last 5 sprints, rotate older to git history -->

## SPRINT HISTORY

## Sprint 102 — Cursor Overlay VBlank Synchronization

**Date:** 2026-03-19
**Agents:** COUNSELOR, @pathfinder, @engineer

### Problem

On the alternate screen (vim, htop), the cursor overlay briefly flashed at a random cell for a single frame before snapping to the correct position. Visible on M4 ProMotion (120Hz), not on iMac 5K 2015 (60Hz).

### Root Cause

Cursor repositioning and GL screen content rendering were driven by two independent timers with no synchronization:

| Path | Trigger | Action |
|------|---------|--------|
| Cursor overlay | `State::timerCallback()` (60–120 Hz) → ValueTree flush → `valueTreePropertyChanged` → `updateCursorBounds()` → `cursor->setBounds()` | Immediate reposition |
| GL content | `VBlankAttachment` (display refresh) → `consumeSnapshotDirty()` → `screen.render()` | GL repaint |

At 60Hz (iMac 5K) the two timers land close enough that the glitch is invisible. At 120Hz (M4 ProMotion) there's a ~50% chance the timer fires between VBlanks, moving the JUCE cursor component over stale GL content for one frame.

### What Was Done

**1. Event-driven cursor reposition via `cursorBoundsDirty` flag** (`TerminalComponent.h`, `TerminalComponent.cpp`)
- `valueTreePropertyChanged` for `cursorRow`/`cursorCol`/`activeScreen` sets `cursorBoundsDirty = true` instead of calling `updateCursorBounds()` directly
- `onVBlank()` consumes the flag: if `cursorBoundsDirty`, clears it and calls `updateCursorBounds()`
- Event-driven (not polled) — no wasted `updateCursorBounds()` calls when cursor hasn't moved
- VBlank-synchronized — cursor overlay only moves on the next display frame, never over stale GL content

**2. `activeScreen` change still rebinds immediately** (`TerminalComponent.cpp`)
- `cursor->rebindToScreen()` stays in `valueTreePropertyChanged` (listener rebinding must happen before the next VBlank reads from the new screen's ValueTree node)
- Actual reposition deferred to VBlank via the same `cursorBoundsDirty` flag

### Files Modified

- `Source/component/TerminalComponent.h:580–588` — added `bool cursorBoundsDirty { false }` with docstring
- `Source/component/TerminalComponent.cpp:689–726` — `onVBlank()`: consumes `cursorBoundsDirty` flag after render block, calls `updateCursorBounds()` only when flag is set
- `Source/component/TerminalComponent.cpp:747–764` — `valueTreePropertyChanged()`: `cursorRow`/`cursorCol` set flag instead of calling `updateCursorBounds()` directly; `activeScreen` rebinds listener + sets flag

### Alignment Check

- **LIFESTAR Lean:** One boolean flag, no new abstractions. Producer sets, consumer clears.
- **LIFESTAR Explicit Encapsulation:** Flag is private to `Component`. ValueTree listener produces, VBlank consumes. Each has one job.
- **LIFESTAR SSOT:** `cursorBoundsDirty` is the single signal. No duplicate dirty tracking.
- **LIFESTAR Immutable:** Flag is set-and-consume — no partial states.
- **LIFESTAR Reviewable:** Docstrings on the flag, both functions, and `updateCursorBounds()` explain the event-driven pattern and why timer-driven repositioning was wrong.
- **NAMING-CONVENTION:** `cursorBoundsDirty` — semantic name describing what is dirty and what needs updating.

### Key Discovery: 120Hz Exposes Timer Desynchronization

The cursor glitch was always present but invisible at 60Hz because the State timer flush (~60–120 Hz) and VBlank (~60 Hz) landed close enough to appear synchronized. At 120Hz the VBlank interval halves to ~8ms, creating a ~50% chance of the timer firing between VBlanks. Any UI state driven by timer flush rather than event-driven VBlank consumption will exhibit similar frame-tearing artifacts at high refresh rates.

### Technical Debt / Follow-up

- **Snacks notifier cursor glitch** — vim snacks notification popups still occasionally show a cursor flash on the message box. Likely the same root cause (cursor position update for a transient UI region) but needs separate investigation.
- **`cursorBoundsDirty` is not atomic** — both producer and consumer run on the message thread so this is safe. If either path ever moves off the message thread, the flag would need to be `std::atomic<bool>`.

---

## Sprint 101 — Trackpad Scroll Sensitivity Fix

**Date:** 2026-03-19
**Agents:** COUNSELOR, @pathfinder

### Problem

Trackpad scrolling was massively oversensitive. A gentle swipe would fly through hundreds of lines of scrollback. Discrete mouse wheel was fine.

### Root Cause

`mouseWheelMove()` treated `wheel.deltaY` as a boolean — checked `> 0.0f` for direction, then scrolled a fixed `terminalScrollStep` (default 5) lines per event. The magnitude was discarded entirely. Trackpads emit many small delta events per gesture (e.g. `deltaY = 0.05` at 120Hz), so every micro-event triggered a full 5-line jump. `wheel.isSmooth` (JUCE's trackpad vs discrete wheel flag) was never checked.

### What Was Done

**1. Split `mouseWheelMove` into discrete and smooth paths** (`TerminalComponent.cpp`)
- Discrete mouse wheel (`!wheel.isSmooth`): unchanged — fixed `scrollLines` per notch
- Smooth trackpad (`wheel.isSmooth`): accumulates `deltaY * scrollLines * trackpadDeltaScale` into `scrollAccumulator`, only scrolls when whole-line thresholds are crossed, subtracts consumed lines from the accumulator
- Both paths handle primary screen (scrollback offset) and alternate screen (SGR mouse events)

**2. Added `trackpadDeltaScale` constant** (`TerminalComponent.h`)
- `static constexpr float trackpadDeltaScale { 8.0f }` — scaling factor that converts JUCE's normalised trackpad deltas to line-sized units
- Documented with rationale: JUCE deltas are ~0.05–0.15 per frame, multiplied by `scrollLines` alone is sub-line, multiplier bridges the gap

**3. Added `scrollAccumulator` member** (`TerminalComponent.h`)
- `float scrollAccumulator { 0.0f }` — collects fractional line amounts across frames
- Documented with rationale: prevents overscroll from micro-events

### Files Modified

- `Source/component/TerminalComponent.h:401` — added `static constexpr float trackpadDeltaScale { 8.0f }` with docstring
- `Source/component/TerminalComponent.h:570` — added `float scrollAccumulator { 0.0f }` with docstring
- `Source/component/TerminalComponent.cpp:386-466` — `mouseWheelMove()` rewritten: discrete path (unchanged behaviour) + smooth path (accumulator-based)

### Alignment Check

- **LIFESTAR Lean:** Minimal change — one function split into two paths, two members added. No new abstractions.
- **LIFESTAR Explicit Encapsulation:** Scroll accumulation is internal to `Component`. No external API change.
- **LIFESTAR SSOT:** `trackpadDeltaScale` defined once. `terminalScrollStep` config controls overall speed for both paths.
- **LIFESTAR Reviewable:** Both new members have docstrings explaining why they exist and how the values were chosen. No magic numbers.
- **NAMING-CONVENTION:** `trackpadDeltaScale` — semantic name describing what it scales and for what input type. `scrollAccumulator` — describes its role precisely.
- **JRENG-CODING-STANDARD:** Brace initialization, `static_cast`, `not` keyword, no early returns in the smooth path (single exit after accumulation check), `static constexpr` for compile-time constant.

### Technical Debt / Follow-up

- **`trackpadDeltaScale` is not user-configurable** — hardcoded at `8.0f`. If users on different hardware report sensitivity issues, could be exposed as a config value (e.g. `terminal.trackpad_sensitivity`).
- **`scrollAccumulator` not reset on focus loss** — if the component loses focus mid-gesture, residual accumulation could cause a small unexpected scroll on next gesture. Unlikely to be noticeable in practice.

---

## Handoff: Text Rendering Pipeline Extraction & CPU Fallback

**From:** COUNSELOR  
**Date:** 2026-03-19  
**Plan:** PLAN.md (project root)

### Objective

Extract END's glyph rendering pipeline into reusable JUCE modules. Provide `jreng::TextLayout` as drop-in replacement for `juce::TextLayout` — accepts `juce::AttributedString`, renders via GL (instanced quads) or CPU (`juce::Graphics`). Enable CPU rendering fallback for environments without GPU acceleration (UTM, remote desktop).

### Plans (5 sequential, each self-contained)

| Plan | Objective | Module | Steps |
|------|-----------|--------|-------|
| 0 | Vendor FreeType + HarfBuzz as JUCE modules | `jreng_freetype`, `jreng_harfbuzz` | 0.1–0.3 |
| 1 | Extract glyph pipeline + proportional text layout | `jreng_text` | 1.1–1.10 |
| 2 | Replace END rendering with `jreng_text` | END `Source/terminal/rendering/` | 2.1–2.5 |
| 3 | CPU rendering backend via `juce::Graphics` | `jreng_text` (CPU backend) | 3.1–3.4 |
| 4 | END CPU rendering fallback | END `Source/` | 4.1–4.5 |

### Surface API

```cpp
namespace jreng
{
    class TextLayout
    {
    public:
        void createLayout (const juce::AttributedString& text, float maxWidth);
        void draw (GLGraphics& g, juce::Rectangle<float> area) const;
        void draw (juce::Graphics& g, juce::Rectangle<float> area) const;
        float getHeight() const;
        int getNumLines() const;
    };
}
```

### Key Decisions Made

- **Input type:** `juce::AttributedString` — no custom string type. Drop-in replacement for `juce::TextLayout`.
- **Dependencies:** `jreng_freetype` + `jreng_harfbuzz` vendored as JUCE modules (FTL + MIT licenses). Replaces CMake submodule and JUCE internal HarfBuzz.
- **Architecture:** Extract working code, don't rewrite. One class (`TextLayout`) with two `draw()` overloads (GL + CPU). Shared `createLayout()` — same shaping, same layout, different surface.
- **Namespace:** Follow existing pattern — flat `jreng` namespace.

### Open Questions (for ARCHITECT at execution time)

- Step 1.2: Namespace — `jreng::text` or flat `jreng`?
- Step 1.3: `GlyphKey::cellSpan` — remove, keep as generic, or template?
- Step 1.5: `BoxDrawing` — move to module or keep in END?
- Step 1.5: Rasterization callback pattern for `GlyphConstraint` decoupling?
- Step 1.7: `shapeASCII` — move as monospace optimization or keep in END?
- Step 1.9: Line breaking — simple word-wrap or UAX #14?
- Step 1.9: Bidirectional text — defer or handle now?
- Step 2.1: `GlyphConstraint`/`BoxDrawing` integration — callback, subclass, or composition?
- Step 3.1: `CPUGlyphCache` — share `LRUGlyphCache` template or separate?
- Step 4.1: GPU detection flag — Config key, AppState, or module static?
- Step 4.3: Renderer selection — runtime or compile-time?

### Contracts

All execution must follow:
- `JRENG-CODING-STANDARD.md`
- `carol/NAMING-CONVENTION.md`
- `carol/ARCHITECTURAL-MANIFESTO.md` (LIFESTAR + LOVE)

### Execution Rules

1. Always invoke @pathfinder first — discover existing patterns before any code change
2. Validate each step before proceeding — ARCHITECT builds and tests
3. Never assume, never decide — discrepancies between plan and code must be discussed
4. No new types without ARCHITECT approval
5. Incremental execution — one step at a time
6. ARCHITECT runs all git commands

---

## Sprint 100 — Windows 11: DWM Blur, ConPTY Sideload, GL Compositing

**Date:** 2026-03-19
**Agents:** COUNSELOR, @engineer, @pathfinder, @researcher, @librarian, @auditor

### Problem

END crashed on Windows 11 — black window, shell exits immediately, no blur. Three root causes discovered through incremental testing:

1. **DWM blur black on Windows 11:** `ACCENT_ENABLE_BLURBEHIND` (3) with `AccentFlags=2` (GradientColor) produces black. `WS_EX_LAYERED` (added by JUCE `setOpaque(false)`) is incompatible with DWM backdrop effects and rounded corners on Windows 11.
2. **Inbox ConPTY kills child processes:** Windows 11 inbox `kernel32.dll` ConPTY sends `STATUS_CONTROL_C_EXIT` (0xC000013A) to child processes immediately after spawn. Sideloaded `conpty.dll` + `OpenConsole.exe` works correctly on both Windows 10 and 11.
3. **GL compositing covers JUCE tint:** GL framebuffer on Windows is composited as opaque by DWM — `glClearColor(0,0,0,0)` alpha is ignored. Tint must go through OS native API, not JUCE paint.

### What Was Done

**1. Windows 11 DWM blur path** (`jreng_background_blur.cpp`)
- Strip `WS_EX_LAYERED` on Windows 11 — incompatible with DWM backdrop and rounded corners
- `DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND` (attribute 33) — native rounded corners
- `DwmExtendFrameIntoClientArea({-1})` — sheet of glass
- `ACCENT_ENABLE_ACRYLICBLURBEHIND` (4) + `AccentFlags=2` + `GradientColor=tint` — acrylic blur with tint
- `isWindows10()` is the only OS branch — Windows 11 is canon, Windows 10 is special case

**2. Tint via OS native API on all platforms** (`jreng_background_blur.cpp`, `jreng_background_blur.mm`)
- GL framebuffer on Windows is composited as opaque by DWM — alpha channel ignored
- Tint must go through OS native API, not JUCE `DocumentWindow` background paint
- macOS: `[window setBackgroundColor:tint]` — unchanged from Windows port
- Windows 11: `ACCENT_ENABLE_ACRYLICBLURBEHIND` + `GradientColor=tint` — DWM handles tint
- Windows 10: `ACCENT_ENABLE_BLURBEHIND` + `GradientColor=tint` — DWM handles tint
- `GlassWindow` `DocumentWindow` background = `transparentBlack` (JUCE doesn't tint)
- All platforms consistent: OS handles blur + tint, GL renders terminal content on top

**3. ConPTY sideload on all Windows versions** (`WindowsTTY.cpp`)
- Removed `isWindows10()` guard from `loadConPtyFuncs()` — always sideload
- Inbox Windows 11 ConPTY sends `STATUS_CONTROL_C_EXIT` to child processes; sideloaded DLL works correctly
- Sprint 99 assumption ("inbox ConPTY on Win11 is sufficient") proven wrong

**4. Shared `isWindows10()`** (`jreng_platform.h`)
- Moved from `WindowsTTY.cpp` to `jreng_core/utilities/jreng_platform.h`
- Single definition used by both `WindowsTTY.cpp` and `jreng_background_blur.cpp`
- Removed `isWindows11_22H2OrLater()` from `jreng_background_blur.cpp`

**5. `scaleNotifier` null guard** (`MainComponent.h`)
- Added `tabs != nullptr` check in `NativeScaleFactorNotifier` lambda
- Prevents crash when DPI change fires before `initialiseTabs()`

### Key Discovery: Windows 11 DWM + WS_EX_LAYERED

`WS_EX_LAYERED` windows are fundamentally incompatible with DWM backdrop effects on Windows 11. DWM treats layered windows as flat textures — no blur behind, no rounded corners. Windows Terminal explicitly warns: "WS_EX_LAYERED acts REAL WEIRD... activating the window will remove our DWM frame entirely" (IslandWindow.cpp:147). The fix: strip `WS_EX_LAYERED` after JUCE adds it, then use DWM attributes for rounding and blur.

### Key Discovery: Windows 11 Inbox ConPTY Broken

The inbox `kernel32.dll` `CreatePseudoConsole` on Windows 11 sends `STATUS_CONTROL_C_EXIT` (0xC000013A) to child processes immediately after spawn. All shells (cmd.exe, powershell.exe, zsh.exe) affected. The sideloaded `conpty.dll` + `OpenConsole.exe` from Microsoft Terminal works correctly on both Windows 10 and 11.

### Key Discovery: ACCENT_ENABLE_BLURBEHIND AccentFlags

On Windows 11, `ACCENT_ENABLE_BLURBEHIND` (3) behavior depends on `AccentFlags`:
- `AccentFlags=0` — transparent blur (works, closest to macOS CGS blur)
- `AccentFlags=2` (use GradientColor) — black/opaque (broken on Win11)
- `ACCENT_ENABLE_ACRYLICBLURBEHIND` (4) + `AccentFlags=0` — acrylic blur (works but different look)

### Files Modified

- `modules/jreng_gui/glass/jreng_background_blur.cpp` — `applyDwmGlass()` rewritten: Win11 canon path (strip WS_EX_LAYERED, rounded corners, sheet of glass, acrylic blur + tint via GradientColor), Win10 special case preserved. Removed `isWindows11_22H2OrLater()` and DWMWA constants.
- `modules/jreng_gui/glass/jreng_background_blur.mm` — `applyBackgroundBlur()` and `applyNSVisualEffect()`: tint via `[window setBackgroundColor:tint]` (restored, consistent with Windows path)
- `modules/jreng_gui/glass/jreng_glass_window.cpp` — `DocumentWindow` background = `transparentBlack` (tint handled by OS API)
- `modules/jreng_core/utilities/jreng_platform.h` — NEW: shared `isWindows10()` static function
- `Source/terminal/tty/WindowsTTY.cpp` — always sideload conpty.dll (removed `isWindows10()` guard), removed local `isWindows10()` definition, includes `jreng_platform.h`
- `Source/MainComponent.h` — `scaleNotifier` null guard for `tabs`

### Alignment Check

- **LIFESTAR Lean:** One OS branch (`isWindows10()`), no nested version checks. Tint architecture identical on all platforms — OS handles blur + tint, GL renders on top.
- **LIFESTAR Explicit Encapsulation:** OS API handles blur + tint. GL renders terminal content. Each layer has one job. `enableGLTransparency()` handles GL-specific DWM setup only.
- **LIFESTAR SSOT:** `isWindows10()` defined once in `jreng_platform.h`, used everywhere. Tint flows through one path: config → GlassWindow → BackgroundBlur::apply() → OS API.
- **LIFESTAR Reviewable:** Win11 path documented with DWM attribute values and rationale. AccentFlags findings documented.
- **NAMING-CONVENTION:** `isWindows10()` — boolean predicate with `is*` prefix, semantic name.

### Technical Debt / Follow-up

- **Windows 10 blur path untested after changes:** The Win10 special case path is preserved but the sideload change (always sideload) and `isWindows10()` relocation need verification on Win10.
- **`enableGLTransparency()` on Windows 11:** Still strips `WS_EX_LAYERED` and calls `DwmExtendFrameIntoClientArea` — both already done by `applyDwmGlass()`. Redundant but harmless. Could be simplified.
- **Blur radius not controllable on Windows:** Documented in `default_end.lua`. `ACCENT_ENABLE_ACRYLICBLURBEHIND` does not expose a blur radius parameter. DWM controls intensity.
- **CRITICAL: Windows 11 UTM/no-GPU crashes.** Black window, no rendering, crash. Likely the Win11 DWM blur path (strip `WS_EX_LAYERED` + acrylic accent) fails without GPU acceleration. Needs dedicated debug session on UTM — add diagnostics, identify failure point, add graceful fallback to opaque.

---

## Sprint 99 — Windows 11 ConPTY Guard + build.bat Fixes

**Date:** 2026-03-18
**Agents:** COUNSELOR, @engineer, @pathfinder, @auditor

### Problem

END crashed on Windows 11 with a warning about conpty. The sideloaded `conpty.dll` + `OpenConsole.exe` (embedded as BinaryData, extracted to `~/.config/end/conpty/` at runtime) were designed for Windows 10 where the inbox `conhost.exe` doesn't support `PSEUDOCONSOLE_WIN32_INPUT_MODE`. On Windows 11, the inbox ConPTY already supports this flag natively, and the sideloaded Win10-era binaries are version-incompatible with Win11's console subsystem — causing a crash.

Additionally, `build.bat` had three bugs: (1) parentheses in `%PATH%` after `vcvarsall.bat` broke `cmd.exe` block parsing, (2) switching between Debug/Release required manual `clean` because Ninja is single-config, (3) `vcvarsall.bat` caused the script to re-enter and run twice.

### What Was Done

**1. `isWindows10()` — OS version gate for ConPTY sideload** (`WindowsTTY.cpp:206-238`)
- Static function with cached IIFE (`static const bool`)
- Uses `RtlGetVersion` from `ntdll.dll` via `GetProcAddress` + `reinterpret_cast` (matches file's existing NT API pattern)
- Returns `true` when `dwBuildNumber < 22000` (Windows 10)
- Safe default: `false` — if version undetectable, skip sideload, use inbox ConPTY
- Nested positive checks, single `return result`, brace initialization — fully compliant with JRENG-CODING-STANDARD

**2. `loadConPtyFuncs()` — sideload path guarded** (`WindowsTTY.cpp:259-313`)
- Wrapped entire "Attempt 1: sideloaded conpty.dll" block inside `if (isWindows10())`
- `extractConPtyBinaries()` never called on Windows 11+ — no files dumped to disk
- Kernel32 fallback gated by `if (not result.isValid())` instead of pre-existing early return
- Fixed pre-existing coding standard violations: early return removed, `= []()` → brace init `{ []()...() }`, `mod` → `conptyModule`

**3. `build.bat` — delayed expansion fix** (`build.bat`)
- `setlocal enabledelayedexpansion` — all `%VAR%` → `!VAR!` inside `if` blocks
- Prevents `cmd.exe` parser crash when `%PATH%` contains parentheses (e.g. `C:\Program Files (x86)\...`) after `vcvarsall.bat` runs
- Echo messages use `[Config]` brackets instead of `(Config)` parentheses

**4. `build.bat` — automatic reconfigure on config change** (`build.bat`)
- Marker file `Builds/Ninja/.build_config` stores active config type
- On every run: reads marker, compares to requested config, reconfigures if different
- `build.bat Release` after a Debug build now works without manual `clean`

**5. `build.bat` — re-entry guard** (`build.bat`)
- `_END_BUILD_RUNNING` environment variable prevents double execution
- `vcvarsall.bat` can cause `cmd.exe` to re-enter the calling script; guard exits immediately on re-entry

### Files Modified

- `Source/terminal/tty/WindowsTTY.cpp:206-238` — added `isWindows10()` static function
- `Source/terminal/tty/WindowsTTY.cpp:259-313` — `loadConPtyFuncs()` IIFE restructured: sideload guarded by `isWindows10()`, early return eliminated, brace init, `mod` → `conptyModule`
- `build.bat` — `enabledelayedexpansion` + `!VAR!` syntax, config change detection with marker file, re-entry guard

### Alignment Check

- **LIFESTAR Lean:** `isWindows10()` is 32 lines including docstring. One `if` guard in `loadConPtyFuncs()`. No new abstractions, no shared utilities — the version check is local to the one static function that needs it.
- **LIFESTAR Explicit Encapsulation:** `isWindows10()` is a pure static function with no parameters and no external dependencies beyond Win32 API. It manages its own cached state. Callers don't track any flags on its behalf. `loadConPtyFuncs()` doesn't know or care about the OS version check implementation.
- **LIFESTAR SSOT:** Build number threshold `22000` appears exactly once (line 230). Not duplicated with `isWindows11_22H2OrLater()` in `jreng_background_blur.cpp` — different threshold (22621), different purpose (Mica blur), different predicate.
- **LIFESTAR Immutable:** Both functions use `static const` locals — computed once per process, deterministic, no hidden mutation.
- **LIFESTAR Reviewable:** Docstring explains why (Win10 vs Win11), the threshold (22000), and the safe default (false). Inline comments mark the two-attempt strategy.
- **NAMING-CONVENTION:** `isWindows10` — boolean predicate with `is*` prefix (Rule 1), semantic name (Rule 3). `FnRtlGetVersion` — type alias with `Fn` prefix distinguishing from the API function it wraps. `conptyModule` — semantic name for the loaded DLL handle (Rule 3).
- **JRENG-CODING-STANDARD:** Nested positive checks (no early returns), brace initialization, `not`/`and`/`or` tokens, `reinterpret_cast`, `const` before type, `noexcept`, explicit nullptr checks. Audited and passed all three contracts.

### Technical Debt / Follow-up

- **Sprint 92 debt resolved:** "Windows 11: The inbox conhost on Windows 11 may support PSEUDOCONSOLE_WIN32_INPUT_MODE natively. The sideload is harmless (same behavior) but could be skipped on newer OS versions." — Now guarded. Sideload is Windows 10 exclusive.
- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** — still violates NAMING-CONVENTION Rule 2. Pre-existing debt from Sprint 91.
- **`seq 1M` performance gap** — still 2m33s vs Terminal's 1m12s. Pre-existing debt from Sprint 91.
- **`CursorComponent` missing `setInterceptsMouseClicks(false, false)`** — pre-existing debt from Sprint 92.
- **Sideloaded binaries still embedded in BinaryData** — on Windows 11 they are dead weight (~1.2 MB) in the executable. Could be excluded from BinaryData via CMake platform/version guard, but the complexity isn't worth the savings right now.

---

## Sprint 98 — Configurable Padding, SGR Mouse Wheel, DPI Cell Hit-Test, Resize Ruler, Bit-Font Logo

**Date:** 2026-03-17
**Agents:** SURGEON, @pathfinder, @explore

### Problems Solved

1. **Text selection offset worsened toward bottom** — `cellAtPoint` used logical integer `cellWidth`/`cellHeight` for hit-testing but the GL renderer places rows at `row * physCellHeight` physical pixels. At fractional DPI scales (125%, 150%), `physCellHeight / scale ≠ cellHeight` due to integer truncation. Error accumulated per row — 6px drift at row 10, 12px at row 20.
2. **Mouse wheel sent arrow keys on alternate screen** — `mouseWheelMove` sent `\x1b[A`/`\x1b[B` (arrow key sequences) instead of SGR mouse wheel events. TUI apps that handle mouse wheel natively (button 64/65) never received them.
3. **Terminal padding hardcoded** — `horizontalInset`/`verticalInset` were `static constexpr int { 10 }`. No way to configure per-side padding.
4. **`scrollbackStep` set in config but never read** — `mouseWheelMove` used `static constexpr int scrollLines { 3 }` ignoring the config value entirely.
5. **`scrollback.*` table orphaned** — two-key table with no natural home. Belongs in a `terminal` table alongside new padding keys.
6. **Resize overlay showed plain centred text** — replaced with Path-based crossed ruler lines with inline gap labels, padding-aware, aligned to actual grid edges.
7. **`shouldForwardMouseToPty` docstring wrong** — claimed ConPTY intercepts DECSET sequences on Windows; sideloaded ConPTY (Sprint 92) makes `isMouseTracking()` fully reliable on Windows.

### What Was Done

**1. `Screen::cellAtPoint` — DPI-accurate cell hit-testing** (`Screen.cpp`)
- Rewritten to use physical-pixel round-trip: `physX = (x - viewportX) * scale`, then `col = physX / physCellWidth`
- Exactly inverts `getCellBounds()` — click-to-cell is now symmetric with cell-to-pixel
- Eliminates per-row drift at all fractional DPI scales

**2. `mouseWheelMove` — SGR mouse wheel** (`TerminalComponent.cpp`)
- Alternate screen path now calls `session.writeMouseEvent(button, cell.x, cell.y, true)` with button 64 (up) or 65 (down)
- `scrollLines` reads `Config::Key::terminalScrollStep` — fixes pre-existing bug where config value was set but ignored
- Primary screen scrollback path also reads `terminalScrollStep`

**3. `terminal` config table** (`Config.h`, `Config.cpp`, `default_end.lua`)
- `scrollback.num_lines` → `terminal.scrollback_lines` (`terminalScrollbackLines`)
- `scrollback.step` → `terminal.scroll_step` (`terminalScrollStep`)
- New `terminal.padding` — 4-element Lua array `{ top, right, bottom, left }` (CSS order)
- Dedicated array parser in `Config::load()`: when field is a table, reads indices 1–4, clamps to [0, 200], stores as 4 flat keys `terminal.padding_top/right/bottom/left`
- Default: `{ 10, 10, 10, 10 }`

**4. `TerminalComponent` padding** (`TerminalComponent.h`, `TerminalComponent.cpp`)
- Removed `static constexpr int verticalInset { 10 }` and `horizontalInset { 10 }`
- Added 4 `const int padding*` members read from config at construction
- `resized()` uses 4 individual `removeFrom*` calls instead of `reduced()`

**5. Resize ruler overlay** (`MessageOverlay.h`, `MainComponent.cpp`)
- `showResize(cols, rows, padTop, padRight, padBottom, padLeft)` replaces `showMessage(...)` on resize
- `paintRulers()` static free function: two `juce::Path` rulers crossing near bottom-right (resize handle location)
- Horizontal ruler at `y = 2/3 * gridHeight`, vertical at `x = 2/3 * gridWidth` — both inset by padding to align with actual grid edges
- Each ruler: two Path strokes flanking a label gap, perpendicular tick marks at grid edges
- Labels: `"N col"` and `"N row"` — horizontal text only, no rotation
- `MainComponent::showMessageOverlay()` reads config padding for accurate col/row calculation

**6. `shouldForwardMouseToPty` docstring corrected** (`TerminalComponent.h`, `TerminalComponent.cpp`)
- Removed incorrect ConPTY interception claim
- Documents that `isMouseTracking()` is reliable on Windows via sideloaded ConPTY

**7. Bit-font logo + version stamp** (`default_end.lua`)
- 11-line pixel-art `END` logo added as Lua comments at top of `default_end.lua`
- `Ephemeral Nexus Display  v%versionString%` subtitle line
- `Config::writeDefaults()` substitutes `%%versionString%%` → `ProjectInfo::versionString` before config key loop

### Files Modified

- `Source/terminal/rendering/Screen.cpp` — `cellAtPoint()` rewritten (physical-pixel round-trip)
- `Source/component/TerminalComponent.h` — removed `verticalInset`/`horizontalInset`, added 4 `const int padding*` members
- `Source/component/TerminalComponent.cpp` — `resized()` uses 4 `removeFrom*`, `mouseWheelMove` uses SGR + config scroll step, `shouldForwardMouseToPty` docstring corrected
- `Source/config/Config.h` — `scrollbackNumLines` → `terminalScrollbackLines`, `scrollbackStep` → `terminalScrollStep`, added 4 `terminalPadding*` constants
- `Source/config/Config.cpp` — `initDefaults()` + `initSchema()` updated, `terminal.padding` array parser added, `writeDefaults()` substitutes `%%versionString%%`
- `Source/config/default_end.lua` — `scrollback` table → `terminal` table, `padding` array with full CSS-order comment, bit-font logo + version stamp
- `Source/terminal/logic/Grid.cpp:57` — `scrollbackNumLines` → `terminalScrollbackLines`
- `Source/MainComponent.cpp` — `showMessageOverlay()` reads 4 config padding values, calls `showResize()` with padding args
- `Source/component/MessageOverlay.h` — `showResize()` accepts 4 padding params, `paintRulers()` insets ruler bounds by padding, 4 `resizePad*` members added

### Alignment Check

- **LIFESTAR Lean:** `cellAtPoint` fix is 4 lines replacing 2. Padding is 4 flat keys parsed by a single dedicated block. No new abstractions.
- **LIFESTAR Explicit Encapsulation:** Padding lives in `terminal.*` — terminal behaviour, not window chrome. `paintRulers` is a static free function — no state, no coupling. `writeDefaults` substitutes version before config loop — explicit ordering.
- **LIFESTAR SSOT:** `terminal.padding` array is the sole source — parsed once into 4 flat keys, read from those keys everywhere. No shadow copies.
- **LIFESTAR Findable:** All terminal behaviour config under `terminal.*`. Ruler drawing in `MessageOverlay.h` alongside `showResize`.
- **LIFESTAR Reviewable:** `cellAtPoint` docstring explains the physical round-trip and why it matches `getCellBounds`. `paintRulers` docstring explains grid inset and Path gap approach.
- **NAMING-CONVENTION:** `terminalScrollbackLines`, `terminalScrollStep`, `terminalPaddingTop/Right/Bottom/Left`, `paintRulers`, `showResize`, `resizePadTop` — all semantic, no data-source encoding.
- **ARCHITECTURAL-MANIFESTO:** `cellAtPoint` is a pure coordinate transform — no side effects. `paintRulers` is a pure paint function — no state mutation. Config parser is additive — new array path doesn't touch existing scalar path.

### Technical Debt / Follow-up

- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** — still violates NAMING-CONVENTION Rule 2 (encodes "Tree" in name). Pre-existing debt from Sprint 91.
- **`seq 1M` performance gap** — still 2m33s vs Terminal's 1m12s. Pre-existing debt from Sprint 91.
- **`terminal.padding` is read at construction only** — `TerminalComponent` members are `const int` initialized at construction. Hot-reload (`Cmd+R`) does not update padding until next session restart. To support live reload, padding members would need to be non-const and `applyConfig()` would need to call `resized()`.
- **`CursorComponent` missing `setInterceptsMouseClicks(false, false)`** — cursor cell swallows clicks. Pre-existing debt from Sprint 92.

---

## Sprint 97 — BackgroundBlur Architecture Fix: Unified macOS/Windows Glass

**Date:** 2026-03-16
**Agents:** COUNSELOR, @engineer, @pathfinder, @oracle, @researcher, @auditor

### Problem

TextEditor inside GlassWindow disappeared on Windows. GL terminal rendered correctly but any software-rendered JUCE component (TextEditor for command palette) was invisible. Same root cause made kuassa plugin dialog windows show flat white instead of glass blur.

### Root Cause

`applyDwmGlass()` stripped `WS_EX_LAYERED` from the window and called `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})`. JUCE's software renderer for transparent windows (`setOpaque(false)` + no native title bar) uses `TransparencyKind::perPixel` mode, painting via `UpdateLayeredWindow()`. Stripping `WS_EX_LAYERED` caused `UpdateLayeredWindow` to silently fail on every repaint — content painted into an offscreen bitmap that never reached the screen.

GL windows survived because OpenGL bypasses JUCE's software paint pipeline entirely (`wglSwapBuffers` writes directly to the framebuffer). PopupMenu blur worked by accident — painted once before async blur fired, then never needed repainting (short-lived).

### What Was Done

**1. BackgroundBlur architecture unified across platforms**

Split the Windows implementation to match macOS architecture:

| | macOS | Windows |
|---|---|---|
| `apply()` | `CGSSetWindowBackgroundBlurRadius` / `NSVisualEffectView` | `SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND)` |
| `enableGLTransparency()` | `NSOpenGLContextParameterSurfaceOpacity = 0` | Strip `WS_EX_LAYERED` + `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})` |

`apply()` is rendering-agnostic — safe for any window (GL or software). `enableGLTransparency()` is GL-only — called from `Screen::glContextCreated()`.

**2. `applyDwmGlass()` — safe for all windows**
- Removed `WS_EX_LAYERED` stripping
- Removed `DwmExtendFrameIntoClientArea` call
- Win11 Mica attributes set but fall through to accent policy (Mica needs frame extension which only GL windows get)
- Accent policy (`SetWindowCompositionAttribute`) always applied — works with `WS_EX_LAYERED` windows

**3. `enableGLTransparency()` — GL-specific DWM setup**
- Was a no-op on Windows, now performs the invasive DWM operations
- Gets HWND via `wglGetCurrentDC()` → `WindowFromDC()` → `GetAncestor(GA_ROOT)` (JUCE creates internal GL child window; must walk up to top-level)
- Strips `WS_EX_LAYERED` (GL doesn't use `UpdateLayeredWindow`)
- Calls `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})`

**4. ActionList fixes**
- Explicit TextEditor colours from Config (background, text, caret, outline)
- Removed unused `searchBox` member
- Added Escape key dismissal (`keyPressed` override)

**5. Kuassa library fork**
- `kuassa_background_blur.cpp` updated with identical fix (namespace `kuassa` instead of `jreng`)

### Files Modified

- `modules/jreng_gui/glass/jreng_background_blur.cpp` — `applyDwmGlass()` rewritten (removed WS_EX_LAYERED strip + frame extension), `enableGLTransparency()` rewritten (GL-specific DWM setup with GetAncestor walk)
- `Source/terminal/action/ActionList.h` — removed unused `searchBox` member, added `keyPressed` override
- `Source/terminal/action/ActionList.cpp` — explicit TextEditor colours, Escape dismissal
- `~/Documents/Poems/kuassa/___lib___/kuassa_graphics/glass/kuassa_background_blur.cpp` — identical fix forked from jreng module

### Files NOT Modified

- `modules/jreng_gui/glass/jreng_background_blur.h` — API surface unchanged
- `modules/jreng_gui/glass/jreng_background_blur.mm` — macOS implementation untouched

### Alignment Check

- **LIFESTAR Lean:** Fix is minimal — moved two operations between two functions. No new abstractions.
- **LIFESTAR Explicit Encapsulation:** `apply()` is rendering-agnostic. `enableGLTransparency()` is GL-specific. Each function has one clear responsibility. Callers don't need to know the rendering mode.
- **LIFESTAR SSOT:** One blur API surface, two platform implementations, identical structure.
- **LIFESTAR Findable:** Same file, same function names, same call sites on both platforms.
- **LIFESTAR Reviewable:** Doxygen on both functions explains the split and why each operation lives where it does.
- **NAMING-CONVENTION:** No new identifiers. Existing names preserved.
- **ARCHITECTURAL-MANIFESTO:** Tell don't ask. `apply()` tells the window to blur. `enableGLTransparency()` tells the GL context to composite. Neither queries the other.

### Key Discovery: JUCE perPixel Transparency + WS_EX_LAYERED

When a JUCE window has `setOpaque(false)` + no native title bar, JUCE calculates `TransparencyKind::perPixel` and adds `WS_EX_LAYERED`. All painting goes through `UpdateLayeredWindow()` with an ARGB bitmap. Stripping `WS_EX_LAYERED` externally (via `SetWindowLongPtrW`) while JUCE's internal state still says `perPixel` causes every subsequent repaint to silently fail — `UpdateLayeredWindow` returns `FALSE` on a non-layered window but JUCE doesn't check the return value.

### Key Discovery: JUCE GL Child Window

`juce::OpenGLContext::attachTo()` creates an internal child window for the GL surface. `wglGetCurrentDC()` → `WindowFromDC()` returns this child HWND, not the top-level window. `WS_EX_LAYERED` and `DwmExtendFrameIntoClientArea` are top-level window attributes — must use `GetAncestor(hwnd, GA_ROOT)` to walk up.

### Technical Debt / Follow-up

- **Win11 Mica on software-rendered windows:** Mica requires `DwmExtendFrameIntoClientArea` which is only called for GL windows. Software-rendered windows on Win11 get accent policy blur instead. Visual parity between GL and software windows on Win11 not yet achieved.
- **Apple Silicon:** `NSOpenGLContext` deprecated. `enableGLTransparency()` needs Metal equivalent (`CAMetalLayer.opaque = NO`). macOS `CGSSetWindowBackgroundBlurRadius` deprecated on Monterey+ but `NSVisualEffectView` fallback catches it.
- **Kuassa plugin build not yet tested** — forked code needs verification in plugin host context.

---

## Sprint 96 — Configurable Popup Terminals + Action Ownership + Ctrl+C Fix

**Date:** 2026-03-16
**Agents:** COUNSELOR, @engineer, @researcher, @librarian, @pathfinder

### Problem

Sprint 95 delivered a hardcoded popup spawning a default shell. Needed: configurable popup entries from Lua (command, args, cwd, modal/global keys), Action ownership moved to Main.cpp, popup auto-dismiss on process exit, and Ctrl+C not reaching TUI apps.

### What Was Done

**1. Config: `popups` table**
- `Config::PopupEntry` struct: command, args, cwd, width, height, modal, global
- `Config::getPopups()` accessor, `clearPopups()` for reload
- Three-level Lua parsing: `END.popups.<name>.<field>` with validation (command required, at least one key binding)
- Per-popup width/height with global `popup.width`/`popup.height` fallback
- `default_end.lua` updated: comprehensive commented example block (tit, lazygit, htop)
- Removed: `keys.popup`, `popup.action` (single-popup design replaced by `popups` table)

**2. Config: `onReload` callback**
- `Config::onReload` — `std::function<void()>` fired at end of `reload()`
- Wired in `Main.cpp::initialise()` → calls `MainComponent::applyConfig()`
- `MainComponent::applyConfig()` — public method: `registerActions()` + `tabs->applyConfig()` + LookAndFeel + orientation
- reload_config action simplified: just calls `config.reload()`, shows message

**3. Action ownership moved to Main.cpp**
- `Terminal::Action action` moved from MainComponent to ENDApplication (alongside Config, AppState, FontCollection)
- All Contexts now owned by the app, constructed before the window
- `Action::clear()` — public method, wipes entries + bindings
- `Action::buildKeyMap()` — made public, called after registration
- MainComponent accesses via `Action::getContext()`, no member
- `registerActions()` calls `action.clear()`, registers all fixed + popup actions, calls `action.buildKeyMap()`

**4. Popup actions from Config**
- Each `popups` entry registers as `"popup:<name>"` (modal) and/or `"popup_global:<name>"` (global)
- `Action::buildKeyMap()` resolves popup modal/global keys from `Config::getPopups()`
- Shared `launchPopup` lambda per entry — DRY, no duplicate callbacks
- Shell wrapping: `config.shellProgram -c command` (e.g. `zsh -c tit`)

**5. Session shell override**
- `Session::setShellProgram (program, args)` — overrides Config default
- `Terminal::Component (program, args, cwd)` constructor — calls `setShellProgram` + `setWorkingDirectory` before `initialise()`
- Session stays dumb — receives shell + args, launches them

**6. Popup auto-dismiss on process exit**
- `Terminal::Component::onProcessExited` — public callback, replaces default quit-app behavior
- `Popup::show()` wires `terminal->onProcessExited = [this] { dismiss(); }` — Popup owns its own dismissal
- `WindowsTTY::waitForData()` — `WaitForMultipleObjects` on both `readEvent` and `process` handle. When child exits, cancels pending read and signals EOF. Fixes ConPTY keeping pipe alive after process exit.

**7. Popup terminal input: tmux overlay model**
- Popup terminals bypass `Terminal::Action` entirely in `keyPressed`
- All keys go directly to PTY — no copy interception, no prefix handling
- `escapeKeyTriggersCloseButton` set to `false` — Escape goes to TUI, not dismiss
- Matches tmux's `popup_key_cb` pattern: overlay owns all input while active

**8. Ctrl+C fix (Win32 Input Mode bypass)**
- `Keyboard::encodeWin32Input()` — Ctrl+C sends raw `\x03` (ETX) instead of Win32 Input Mode sequence
- Root cause: zsh enables Win32 Input Mode (`?9001h`). Go/Rust TUI apps (tit, lazygit) don't understand Win32 Input Mode sequences. They expect standard VT input for signal-generating keys.
- `\x03` is universal — PTY line discipline generates SIGINT. Works on all platforms.

### Files Created
- None

### Files Modified
- `Source/config/Config.h` — removed `keysPopup`/`popupAction`, added `PopupEntry` struct, `popups` map, `getPopups()`, `clearPopups()`, `onReload` callback
- `Source/config/Config.cpp` — removed keysPopup/popupAction from defaults+schema, added `popups` table parsing, `getPopups()`, `clearPopups()`, `onReload` fired from `reload()`
- `Source/config/default_end.lua` — removed `keys.popup`/`popup.action`, added `popup` defaults section, added commented `popups` example block
- `Source/Main.cpp` — added `Terminal::Action action` member, `#include Action.h`, wired `config.onReload`
- `Source/MainComponent.h` — removed `Terminal::Action action` member, added public `applyConfig()`
- `Source/MainComponent.cpp` — `applyConfig()` method, `registerActions()` uses `Action::getContext()`, popup actions from `config.getPopups()`, reload_config simplified
- `Source/terminal/action/Action.h` — `clear()` public, `buildKeyMap()` public
- `Source/terminal/action/Action.cpp` — `clear()` implementation, popup key resolution in `buildKeyMap()`
- `Source/terminal/logic/Session.h` — `setShellProgram()`, `shellOverride`/`shellArgsOverride` members
- `Source/terminal/logic/Session.cpp` — `setShellProgram()` implementation, `resized()` uses override
- `Source/component/TerminalComponent.h` — `onProcessExited` callback, `Component (program, args, cwd)` constructor
- `Source/component/TerminalComponent.cpp` — new constructor, `onProcessExited` in `initialise()`, popup bypass in `keyPressed`
- `Source/component/Popup.cpp` — `escapeKeyTriggersCloseButton = false`, `onProcessExited` wired in `show()`
- `Source/terminal/tty/WindowsTTY.cpp` — `WaitForMultipleObjects` on process handle in `waitForData()`
- `Source/terminal/data/Keyboard.cpp` — Ctrl+C sends `\x03` bypassing Win32 Input Mode

### Alignment Check
- **LIFESTAR Lean:** Shell wrapping is one line in MainComponent. Session stays dumb. Popup wires its own dismissal.
- **LIFESTAR Explicit Encapsulation:** Action owned by Main (Context). Config fires `onReload`, doesn't know about Action. Popup wires `onProcessExited` itself — MainComponent doesn't manage popup lifecycle. Each object has one job.
- **LIFESTAR SSOT:** `Config::getPopups()` is the sole source for popup entries. Action registry rebuilt from scratch on every reload.
- **LIFESTAR Findable:** `PopupEntry` in Config.h. Popup actions prefixed `"popup:"`.
- **NAMING-CONVENTION:** `PopupEntry`, `getPopups`, `clearPopups`, `onReload`, `onProcessExited`, `setShellProgram`, `shellOverride` — all semantic.
- **ARCHITECTURAL-MANIFESTO:** Tell don't ask. Config tells listeners via `onReload`. Popup tells itself to dismiss via `onProcessExited`. tmux overlay model: popup owns all input.

### Key Discovery: Win32 Input Mode + TUI Apps

Go/Rust TUI apps don't understand Win32 Input Mode (`?9001h`). When zsh enables it and a TUI launches inside zsh without disabling it, signal-generating keys (Ctrl+C) are sent as Win32 Input Mode sequences that the TUI ignores. Fix: Ctrl+C always sends raw `\x03` regardless of Win32 Input Mode.

### Key Discovery: ConPTY Pipe Stays Open After Process Exit

ConPTY (sideloaded `OpenConsole.exe`) keeps the pipe alive after the child process exits. `ReadFile` never returns `ERROR_BROKEN_PIPE`. Fix: `WaitForMultipleObjects` on both the read event and the process handle. When the process exits, cancel the pending read and signal EOF.

### Technical Debt / Follow-up
- **Multiple popup configs not tested with reload** — hot-reload should re-register popup actions correctly via `onReload` chain
- **Per-popup width/height fallback** — registered in Config but not consumed in MainComponent's `launchPopup` (always uses global defaults)
- **`onProcessExited` naming** — used as a flag to detect popup terminals in `keyPressed`. Should have a dedicated `isPopupTerminal` flag instead of overloading callback presence.
- **Ctrl+C bypass is Ctrl+C only** — other signal keys (Ctrl+Z, Ctrl+\) may have the same Win32 Input Mode issue. Should audit all signal-generating keys.
- **`escapeKeyTriggersCloseButton = false`** — Escape goes to TUI. No way to dismiss popup except process exit. May want a configurable dismiss key in future.

---
