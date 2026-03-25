# SPRINT LOG

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
