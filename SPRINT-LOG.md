# SPRINT LOG

---

## Sprint 121: Plan 4 — Runtime GPU/CPU Switching

**Date:** 2026-03-26
**Duration:** ~8h

### Agents Participated
- COUNSELOR: Planning, diagnosis, delegation, root cause analysis
- Pathfinder: Plan 4 touch points discovery
- Oracle: SSE2 mono tint interleave, CPU garbled glyph root cause (LRU eviction + orphaned staged bitmaps)
- Engineer (x8): Config key, RendererType, Screen variant, MainComponent wiring, AppState SSOT, ValueTree listener, Popup renderer, dirty bit accumulation, SIMD NEON tint, popup border, BackgroundBlur rename
- Auditor: default_end.lua user-friendliness audit (28 findings)

### Files Modified (28 total)
- `Source/AppIdentifier.h` — added `renderer` property identifier
- `Source/AppState.h` — added `getRendererType()` / `setRendererType()`
- `Source/AppState.cpp` — implemented renderer getter/setter, added to `initDefaults()`
- `Source/MainComponent.h` — added `ValueTree::Listener`, `paint()` override, `windowState` member
- `Source/MainComponent.cpp` — constructor: resolve renderer → AppState → listener; `applyConfig()`: writes renderer to AppState, re-applies blur/opacity via deferred callAsync; `valueTreePropertyChanged`: GL lifecycle + atlas resize + terminal switch; `paint()`: fills background when opaque; `onRepaintNeeded`: always calls both `terminal->repaint()` + `glRenderer.triggerRepaint()`
- `Source/component/RendererType.h` — `RendererType` enum, `getRendererType()` reads from AppState (SSOT)
- `Source/component/TerminalComponent.h` — `ScreenVariant` (std::variant), `switchRenderer()`, `screenBase()`, `visitScreen()`, `std::optional` handlers
- `Source/component/TerminalComponent.cpp` — all `screen.` calls → `visitScreen`/`screenBase()`; constructors use `std::in_place_type`; `initialise()` calls `switchRenderer(getRendererType())`; `switchRenderer()` emplaces variant, reconstructs handlers, applies config; `paint()` fills + renders for CPU variant only; `glContextCreated/Closing/renderGL` gated by variant; `onVBlank` modal component check for popup focus
- `Source/component/Tabs.h` — added `switchRenderer()` declaration
- `Source/component/Tabs.cpp` — `switchRenderer()` iterates all panes/terminals
- `Source/component/Popup.h` — `Window::paint()` override for border
- `Source/component/Popup.cpp` — `onRepaintNeeded` calls both repaint + triggerRepaint; `initialiseGL()` guards on `getRendererType()`; `Window::paint()` draws configurable border
- `Source/component/LookAndFeel.cpp:69` — `ResizableWindow::backgroundColourId` wired from `Config::Key::coloursBackground`
- `Source/component/MessageOverlay.h` — `withPointHeight()` fix for font size
- `Source/terminal/rendering/Screen.h` — `getNumCols()` added to `ScreenBase`; `previousCells` member
- `Source/terminal/rendering/Screen.cpp` — `previousCells` allocation; `setFontSize()` guarded against redundant atlas clear
- `Source/terminal/rendering/ScreenRender.cpp` — `fullRebuild` flag; scroll force-dirty; row-level memcmp skip (gated by fullRebuild); blank trim; `frameDirtyBits` OR-accumulation
- `Source/terminal/rendering/ScreenGL.cpp` — `frameDirtyBits` reset after `prepareFrame()` in `renderPaint()`
- `Source/config/Config.h` — added `Key::gpuAcceleration`, `Key::popupBorderColour`, `Key::popupBorderWidth`
- `Source/config/Config.cpp` — `initKeys()` entries for gpu/popup border keys
- `Source/config/default_end.lua` — gpu section at top; user-friendly comment rewrite (28 jargon removals); popup border keys; per-key GPU dependency notes
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.h` — stale doxygen fixed (push, prepareFrame)
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp` — NativeImageType; prepareFrame selective clearing with accumulated dirty bits; SIMD compositing loops
- `modules/jreng_graphics/rendering/jreng_simd_blend.h` — full SSE2 + NEON blendMonoTinted4 (tint+interleave+blend inline)
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.h` — added `setAtlasSize()`
- `modules/jreng_graphics/fonts/jreng_typeface.h` — added `setAtlasSize()` delegate
- `modules/jreng_opengl/renderers/jreng_gl_text_renderer.h` — added `contextInitialised` guard
- `modules/jreng_opengl/renderers/jreng_gl_text_renderer.cpp` — guarded `createContext()`/`closeContext()` with `contextInitialised`
- `modules/jreng_gui/glass/jreng_background_blur.h` — renamed `enableGLTransparency` → `enableWindowTransparency`; added `disableWindowTransparency(Component*)`
- `modules/jreng_gui/glass/jreng_background_blur.mm` — `enableWindowTransparency` GL-surface only; `disableWindowTransparency` removes blur + sets opaque via component peer
- `modules/jreng_gui/glass/jreng_background_blur.cpp` — Windows implementations updated
- `PLAN.md` — Plan 4 marked Done (Sprint 121)
- `PLAN-cpu-rendering-optimization.md` — optimization plan
- `SPEC.md` — software renderer fallback marked Done
- `DEBT.md` — `enableGLTransparency` references updated
- `SPRINT-LOG.md` — Sprint 120 + 121 logged

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [ ] LEAN violated during garble fix attempts — multiple speculative changes without diagnosis

### Problems Solved
- **GPU/CPU hot-reload:** Config → AppState (SSOT) → ValueTree listener → GL lifecycle + atlas resize + terminal switch. One write, one reaction.
- **Atlas size mismatch:** GPU (4096) vs CPU (2048) — `Typeface::setAtlasSize()` resizes on switch.
- **GL ref count leak:** `GLTextRenderer::closeContext()` never called on variant destruction. Fix: `contextInitialised` guard + `glContextClosing()` in `switchRenderer()`.
- **Redundant atlas clear:** `Screen::setFontSize()` unconditionally cleared shared atlas even when size unchanged. Fix: guard with `pointSize != baseFontSize`.
- **Popup focus steal:** `onVBlank` `toFront(true)` fought modal popup. Fix: skip when `getCurrentlyModalComponent() != nullptr`.
- **MessageOverlay font size:** `FontOptions` constructor used height units, not points. Fix: `withPointHeight()`.
- **Orphaned staged bitmaps:** Popup terminal staged bitmaps polluted shared atlas after close — root cause identified by Oracle but fixed by the `setFontSize` guard (prevents atlas clear).
- **VBlank outpacing paint:** Intermediate snapshot dirty bits lost. Fix: `frameDirtyBits` OR-accumulation, reset after paint consumes.
- **Window transparency lifecycle:** `enableGLTransparency` renamed to `enableWindowTransparency`; `disableWindowTransparency` added; blur re-applied on every reload via deferred callAsync.

### Technical Debt / Follow-up
- **Block character overlap on CPU alternate screen** — primitive background quads show old content during scroll region operations. Selective row clearing doesn't fully resolve. Root cause: render target retains stale pixels for rows cleared by scroll region ops where `viewportChanged` is false. Needs investigation — may require tracking scroll region operations in `prepareFrame()` or always full-clearing on alternate screen.
- **Popup terminal atlas interaction** — popup's terminal shares the Typeface atlas. LRU eviction during popup can displace main terminal glyphs. Currently mitigated by `setFontSize` guard. A proper fix would use per-terminal atlas instances or reference-counted glyph pinning.
- **`default_end.lua` regeneration** — existing user configs don't get the new gpu section. Only affects fresh installs or manual config reset.

---

## Sprint 120: CPU Rendering Optimization — SIMD Compositing

**Date:** 2026-03-25
**Duration:** ~4h

### Agents Participated
- COUNSELOR: Led research, planning, delegated execution
- Pathfinder: Discovered current rendering pipeline, verified NativeImageType safety
- Researcher (x2): Deep analysis of xterm and foot rendering architectures
- Oracle (x2): JUCE rendering constraints, SSE2 mono tint interleave approach
- Engineer (x5): Phase 1/2/3 implementation, SIMD header, audit fixes
- Auditor (x2): Cell-level skip architecture validation, comprehensive sprint audit

### Files Modified (10 total)
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp:105` — NativeImageType for renderTarget (cached CGImageRef on macOS)
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp:134-175` — prepareFrame rewrite: full clear on scroll, per-row clear otherwise
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp:234-300` — SIMD drawBackgrounds: fillOpaque4 + blendSrcOver4
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp:330-367` — SIMD compositeMonoGlyph: blendMonoTinted4
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp:394-431` — SIMD compositeEmojiGlyph: blendSrcOver4
- `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.h:108-148` — Stale doxygen fixed (push, prepareFrame)
- `modules/jreng_graphics/rendering/jreng_simd_blend.h` — NEW: SSE2/NEON/scalar SIMD blend header (blendSrcOver4, blendMonoTinted4, fillOpaque4)
- `Source/component/TerminalComponent.cpp:129-131,482` — setOpaque(true), setBufferedToImage(true), fillAll bg from LookAndFeel
- `Source/component/LookAndFeel.cpp:69` — ResizableWindow::backgroundColourId wired from Config::Key::coloursBackground
- `Source/MainComponent.cpp:542` — Scoped repaint to terminal->repaint() instead of MainComponent
- `Source/terminal/rendering/ScreenRender.cpp:498-533` — fullRebuild flag, scroll force-dirty, row-level memcmp skip, blank trim
- `Source/terminal/rendering/Screen.h:918` — previousCells member for row memcmp
- `Source/terminal/rendering/Screen.cpp:386` — previousCells allocation
- `SPEC.md:65,430` — Software renderer fallback marked Done
- `PLAN.md:6,23-24` — Plan 3 Done, Plan 4 Next
- `PLAN-cpu-rendering-optimization.md` — NEW: optimization plan (research, 3 phases, 9 steps)

### Alignment Check
- [x] LIFESTAR principles followed
- [x] NAMING-CONVENTION.md adhered
- [x] ARCHITECTURAL-MANIFESTO.md principles applied
- [x] Never overengineered — cell-level cache rejected in favor of row-level memcmp (LEAN)

### Problems Solved
- **Scroll freeze bug:** memmove optimization caused stale cached quads to overwrite shifted pixels. Fix: force all-dirty in buildSnapshot when scrollDelta > 0, revert to full clear on scroll.
- **Selection break:** Row-level memcmp skip prevented selection overlay regeneration. Fix: gate memcmp skip with `not fullRebuild`.
- **`and` vs `&`:** SIMD header used logical `and` (returns 0/1) for bitwise masking — corrupted all colour extraction. Fix: `&` for bitwise.
- **Operator precedence:** `invA + 128u >> 8u` parsed as `invA + (128u >> 8u)`. Fix: parentheses.
- **NEON OOB:** `vld4q_u8` reads 64 bytes for 16-byte input. Fix: split-half approach with `vld1q_u8`.

### Performance Results
| Build | seq 1 10000000 | CPU% |
|-------|----------------|------|
| -O0 debug | 47.3s | 27% |
| **-O3 release** | **12.2s** | **99%** |
| GL baseline | 12.4s | — |

CPU rendering (-O3) now matches GPU baseline. Faster than kitty, wezterm, ghostty on raw byte throughput.

### Research Findings (xterm + foot analysis)
- Neither xterm nor foot has SIMD in their own code
- xterm: all performance from avoiding work (deferred scroll, XCopyArea, blank trim, run-length batching)
- foot: SIMD delegated to pixman library, two-level dirty tracking, memmove scroll, multithreaded row rendering
- Key insight: our bottleneck was always the scalar compositing loops, not the snapshot pipeline

### Technical Debt / Follow-up
- NEON blendMonoTinted4 still uses scalar pixel build + blendSrcOver4 delegation (SSE2 is fully inlined)
- memmove scroll optimization deferred — requires row-boundary-aware rendering to skip clean rows during compositing
- Plan 4: runtime GPU/CPU switching, rendering engine hot-reload via config
