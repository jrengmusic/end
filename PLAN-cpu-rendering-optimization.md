# PLAN — CPU Rendering Optimization

**Project:** END
**Date:** 2026-03-25
**Author:** COUNSELOR
**Status:** Active

**Contracts:** LIFESTAR, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO, JRENG-CODING-STANDARD

---

## Overview

Push CPU rendering performance to match xterm / foot on a 2015 iMac 5K (full-screen, 2 splits). Three phases, each independently shippable. Every step validates before the next begins.

**Research basis:** Deep analysis of xterm (charproc.c, screen.c, fontutils.c, util.c) and foot (render.c, terminal.c, shm.c, grid.c). Neither has SIMD in their own code. xterm avoids work (deferred scroll, XCopyArea blit, blank trimming). foot delegates SIMD to pixman and uses two-level dirty tracking + memmove scroll + multithreaded row rendering.

**Bottleneck:** Rasterization and compositing in `GraphicsTextRenderer`. The inner loops (`compositeMonoGlyph`, `compositeEmojiGlyph`, `drawBackgrounds`) are scalar byte-at-a-time. Scroll triggers full-clear + full-redraw of all rows. No cell-level skip.

| Phase | Objective | Leverage |
|-------|-----------|----------|
| 1 | Free wins — image type, opaque mode, repaint scope | Low effort, measurable |
| 2 | Avoid work — memmove scroll, cell skip, blank trim | Highest leverage |
| 3 | SIMD compositing — vectorize hot loops | Ceiling push |

---

## Target Files

Primary modification targets (all changes scoped to these):

| File | Role |
|------|------|
| `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.h` | Renderer class declaration |
| `modules/jreng_graphics/rendering/jreng_graphics_text_renderer.cpp` | Renderer implementation — compositing loops, prepareFrame |
| `Source/component/TerminalComponent.h` | Component declaration |
| `Source/component/TerminalComponent.cpp` | paint(), onVBlank(), repaint scope |
| `Source/terminal/rendering/ScreenRender.cpp` | buildSnapshot() — cell processing |
| `Source/terminal/rendering/ScreenSnapshot.cpp` | updateSnapshot() — snapshot packing |
| `Source/terminal/rendering/Screen.h` | Screen template — render cache structures |

---

## Phase 1: Free Wins

Renderer-only changes. No architectural changes. Each step is a one-line to few-line change.

### Step 1.0 — NativeImageType for renderTarget (macOS)

**Problem:** `SoftwareImageType` forces a new `CGImageRef` creation every frame via `CGDataProviderCreateWithData` in the `drawImageAt` path. `NativeImageType` uses `CGBitmapContext` backing which caches the `CGImageRef`.

**Change:** In `push()` at `jreng_graphics_text_renderer.cpp:105`:

```cpp
// Before:
renderTarget = juce::Image (juce::Image::ARGB, w, h, true,
                             juce::SoftwareImageType());

// After:
renderTarget = juce::Image (juce::Image::ARGB, w, h, true,
                             juce::NativeImageType());
```

**Risk:** `NativeImageType` may use different line stride or pixel layout. Must verify `BitmapData::getLinePointer()` still returns contiguous ARGB rows and that `reinterpret_cast<juce::PixelARGB*>` math works.

**Validation:**
- [ ] Verify `BitmapData::lineStride` after change
- [ ] Visual correctness — no colour channel swaps, no garbled output
- [ ] Measure frame time before/after with `time seq 1 1000000`

**Files:** `jreng_graphics_text_renderer.cpp:105`

---

### Step 1.1 — setOpaque + setBufferedToImage on Terminal::Component

**Problem:** When TerminalComponent is not opaque, JUCE allocates an ARGB temp buffer and alpha-blends it. The terminal fills its entire background — alpha channel is wasted work.

**Decision (ARCHITECT):** Terminal::Component owns this. CPU rendering = opaque. GPU rendering = transparent (glassmorphism preserved). Only glassmorphism is lost on CPU path.

**Context:** Plan 4 makes rendering engine hot-reloadable via config. Currently GPU is commented out, temporarily hardcoded to CPU only. When both paths are active, the opaque/transparent decision must follow the active backend automatically.

**Change:** In `Terminal::Component::initialise()`, conditional on CPU rendering:

```cpp
// CPU path: opaque + buffered. GPU path: transparent (Plan 4 wires this).
setOpaque (true);
setBufferedToImage (true);
```

**Background fill:** In `paint()`, fill with `juce::ResizableWindow::backgroundColourId` from `Terminal::LookAndFeel` (inherited as default). This ensures no transparent gaps at component edges or during resize. On GPU path (Plan 4), this fill is skipped — transparency handled by blur shader.

**Validation:**
- [ ] Terminal renders with opaque background, correct colour from LookAndFeel
- [ ] No transparent gaps at edges during resize
- [ ] `setBufferedToImage` — JUCE caches the component image, repaints only when dirty
- [ ] Measure frame time before/after

**Files:** `TerminalComponent.cpp` (initialise, paint)

---

### Step 1.2 — Scope repaint to TerminalComponent

**Problem:** Current `onRepaintNeeded` triggers `repaint()` on MainComponent. This causes JUCE to create a temp image the size of the entire window and repaint all children.

**Investigation needed:** Verify current repaint target. If already scoped to TerminalComponent, skip this step.

**Change:** Ensure `repaint()` is called on the TerminalComponent itself, not the parent.

**Validation:**
- [ ] Only terminal bounds repainted per frame
- [ ] No visual regressions
- [ ] Measure frame time before/after

**Files:** `TerminalComponent.cpp` (onVBlank / onRepaintNeeded path)

---

## Phase 2: Avoid Work

Highest leverage. Reduces the number of pixels touched per frame. On a typical terminal session, 90%+ of cells are unchanged between frames.

### Step 2.0 — memmove scroll on CPU path

**Problem:** `prepareFrame()` full-clears the render target on scroll (`needsFullClear`), then redraws all rows. On a 5K display, this is ~16MB of pixel data cleared + recomposited every scroll frame.

**Fact:** The comment at line 140 says memmove is unsafe due to snapshot double-buffer frame skipping. This concern applies to the GL path (GL thread can miss frames). In the CPU path, `write()` and `read()` both execute on the MESSAGE THREAD sequentially — `write()` in `onVBlank` -> `render()`, `read()` in `paint()` -> `renderPaint()`. Every write is consumed by exactly one read. No frame skipping. memmove is safe.

**Change:** In `prepareFrame()`, when `hasScroll` is true and `not viewportChanged`:

```
1. Compute shift = scrollDelta * cellHeight (pixels to move up)
2. memmove render target rows up by shift pixels:
   - BitmapData on renderTarget (readWrite)
   - memmove (linePtr(0), linePtr(shift), bytesPerRow * (viewportHeight - shift))
3. Clear only the newly exposed rows at bottom (shift pixels tall)
4. Dirty bitmask already marks the new rows — they will be redrawn
```

xterm uses `XCopyArea` for this. foot uses `memmove` on raw pixel rows. Both avoid redrawing already-rendered content.

**Constraint:** Only safe when `not viewportChanged` (scrollback position unchanged). When user scrolls back in history, full clear remains correct.

**Validation:**
- [ ] `seq 1 10000000` — scrolling renders correctly, no visual artifacts
- [ ] Scrollback navigation — no stale pixels
- [ ] Measure `time seq 1 10000000` before/after (expect significant improvement)
- [ ] Resize during scroll — no artifacts

**Files:** `jreng_graphics_text_renderer.cpp` (prepareFrame)

---

### Step 2.1 — Cell-level skip in buildSnapshot

**Problem:** `buildSnapshot()` processes every cell on every dirty row, even if most cells are unchanged (e.g. cursor blink only changes 1 cell, but the entire row is rebuilt).

**Approach:** Maintain a parallel buffer of previously-rendered cells per row in the render cache. When `buildSnapshot()` processes a dirty row, `memcmp` each 16-byte Cell against its previous value. If identical, skip shaping and quad generation for that cell.

foot uses `cell.attrs.clean` packed in the cell struct. We cannot do this — Cell is the data model shared across threads, and adding mutable renderer state to it is a data race. Instead, store previous-frame cells in the render cache (already per-row, MESSAGE THREAD only).

**Data:**
- Add `juce::HeapBlock<Terminal::Cell> previousCells` to the render cache (per-row, `cacheCols` wide)
- After processing a row, `memcpy` the current row's cells into `previousCells` for next-frame comparison
- On grid resize: clear `previousCells` (force full rebuild)

**Design:** In `processCellForSnapshot()` (or the row loop in `buildSnapshot()`):

```
if (memcmp(&cell, &previousCells[col], sizeof(Cell)) == 0)
    skip this cell — no quad/background generation
```

**Decision (ARCHITECT):** `memcmp` on 16-byte Cell. Compiles to one or two 64-bit compares (or single 128-bit SIMD compare). Fewer instructions and zero collision risk vs hash. More performant.

**Cost:** 16 bytes per cell of extra memory. For 256 rows x 256 cols = ~1MB. Negligible.

**Constraint:** Row-level dirty bitmask is still the first gate. Cell-level skip only applies within already-dirty rows. Clean rows are skipped entirely (existing optimization).

**Validation:**
- [ ] Cursor blink — only cursor cell reprocessed, rest of row skipped
- [ ] Typing — only new character cell + cursor cell processed
- [ ] `seq 1 10000000` — full-screen scroll still correct
- [ ] Colour theme change — all cells reprocessed (all changed)
- [ ] Measure frame time for cursor-blink-only frames

**Files:**
- `Screen.h` — add `previousCells` to render cache
- `ScreenRender.cpp` — add memcmp skip in buildSnapshot row loop
- `Screen.cpp` — allocate `previousCells` in `allocateRenderCache()`

---

### Step 2.2 — Leading/trailing blank trim per row

**Problem:** Rows with trailing whitespace (most rows) still generate background quads for every blank cell.

**Approach:** Before entering the cell loop in `buildSnapshot()`, scan inward from both ends of the row past blank cells with default background. Skip those cells entirely.

xterm does exactly this in `ScrnRefresh` (screen.c:1759-1765): walks `col` forward and `maxcol` backward while cells are blank + no attributes.

**Change:** In the row processing loop of `buildSnapshot()`:

```
// Leading blank trim
int firstCol = 0;
while (firstCol < cols and cell[firstCol].codepoint == 0
       and cell[firstCol].bg == defaultBg)
    ++firstCol;

// Trailing blank trim
int lastCol = cols - 1;
while (lastCol >= firstCol and cell[lastCol].codepoint == 0
       and cell[lastCol].bg == defaultBg)
    --lastCol;

// Process only firstCol..lastCol
```

**Constraint:** Only skip cells whose background matches the terminal's default background. Cells with non-default backgrounds must still generate background quads even if empty.

**Validation:**
- [ ] Prompt at column 10 — cells 0-9 have prompt bg, cells 10+ are blank
- [ ] Background colours preserved for all non-default cells
- [ ] `ls --color` output — coloured backgrounds at correct positions
- [ ] Measure frame time on rows with 80% trailing whitespace

**Files:** `ScreenRender.cpp` (buildSnapshot row loop)

---

## Phase 3: SIMD Compositing

Vectorize the three hot compositing loops. SSE2 on x86_64 (guaranteed baseline), NEON on arm64 (guaranteed baseline). No runtime detection needed for 128-bit.

### Step 3.0 — SIMD infrastructure header

**New file:** `modules/jreng_graphics/rendering/jreng_simd_blend.h`

Platform abstraction for 128-bit SIMD compositing. Single header, inline functions.

**Contents:**
- `#if defined(__arm64__) || defined(__aarch64__)` -> NEON path (`<arm_neon.h>`)
- `#elif defined(__x86_64__) || defined(_M_X64)` -> SSE2 path (`<emmintrin.h>`)
- `#else` -> scalar fallback (current code, no regression)

**Core operation — premultiplied src-over blend of 4 ARGB pixels:**

```
Input:  4 source pixels (premultiplied ARGB), 4 destination pixels
Output: 4 blended pixels

dest = src + dest * (255 - srcAlpha) / 255
```

SSE2: `_mm_loadu_si128`, `_mm_srli_epi32` (extract alpha), `_mm_mullo_epi16`, `_mm_adds_epu8`
NEON: `vld4_u8` (deinterleave ARGB), `vmull_u8`, `vadd_u8`, `vst4_u8`

**Functions:**
- `blendFourPixelsSrcOver (uint32_t* dest, const uint32_t* src)` — 4 ARGB pixels, premul src-over
- `blendMonoFourPixels (uint32_t* dest, const uint8_t* alpha, uint32_t premulFgColor)` — 4 mono glyphs tinted with fg colour
- `fillFourPixels (uint32_t* dest, uint32_t premulColor)` — 4 opaque ARGB pixels (background fill, trivial store)

**Validation:**
- [ ] Unit test: blend results match scalar reference for all alpha values (0, 1, 127, 128, 254, 255)
- [ ] Unit test: mono tint matches scalar reference
- [ ] Builds on macOS ARM, macOS Intel, Windows x64, Linux x64

**Files:** New: `modules/jreng_graphics/rendering/jreng_simd_blend.h`

---

### Step 3.1 — SIMD compositeMonoGlyph

**Problem:** Inner loop at `jreng_graphics_text_renderer.cpp:298-319` processes one pixel per iteration. This is the hottest path — every visible text glyph passes through it.

**Change:** Process 4 pixels per iteration using `blendMonoFourPixels`. Handle remainder (0-3 pixels) with scalar tail loop.

```
// Hot loop body (4 pixels at a time):
for (int px = clipX0; px + 3 < clipX1; px += 4)
{
    blendMonoFourPixels (
        reinterpret_cast<uint32_t*> (destRow),
        srcRow,
        premulFgColor);
    srcRow  += 4;
    destRow += 4;
}
// Scalar tail for remaining 0-3 pixels
```

**Constraint:** Atlas line pointers may not be 16-byte aligned. Use unaligned loads (`_mm_loadu_si128` / `vld1q_u8`). Never assume alignment.

**Validation:**
- [ ] Visual correctness — all glyph styles (regular, bold, italic, bold-italic)
- [ ] Edge cases: 1-pixel wide glyph, glyph at viewport edge (clipped)
- [ ] Measure `time seq 1 10000000` before/after
- [ ] Builds and runs on all platforms

**Files:** `jreng_graphics_text_renderer.cpp` (compositeMonoGlyph)

---

### Step 3.2 — SIMD drawBackgrounds

**Problem:** Background fill loops at `jreng_graphics_text_renderer.cpp:234-264`. Opaque path writes one pixel at a time. Semi-transparent path blends one pixel at a time.

**Change:**
- Opaque path: `fillFourPixels` — 4 pixels per store (16 bytes). For full cell widths this is a significant reduction in store instructions.
- Semi-transparent path: `blendFourPixelsSrcOver` — 4 pixels per blend.

**Validation:**
- [ ] Visual correctness — selection highlight, coloured backgrounds, cursor block
- [ ] Semi-transparent backgrounds blend correctly
- [ ] Measure frame time on colour-heavy output (`ls --color`, vim with theme)

**Files:** `jreng_graphics_text_renderer.cpp` (drawBackgrounds)

---

### Step 3.3 — SIMD compositeEmojiGlyph

**Problem:** Emoji compositing loop at `jreng_graphics_text_renderer.cpp:347-369`. Same premul src-over blend as mono but source is ARGB instead of alpha-only.

**Change:** Process 4 pixels per iteration using `blendFourPixelsSrcOver`. Handle opaque fast-path (srcA == 255) with 16-byte memcpy for 4 pixels. Scalar tail for remainder.

**Validation:**
- [ ] Visual correctness — emoji at various sizes, skin tone modifiers, ZWJ sequences
- [ ] Fully opaque emoji renders correctly (fast path)
- [ ] Semi-transparent emoji edges blend correctly
- [ ] Builds on all platforms

**Files:** `jreng_graphics_text_renderer.cpp` (compositeEmojiGlyph)

---

## Execution Rules

1. **One step at a time.** ARCHITECT builds and tests after each step before proceeding.
2. **Measure before and after every step.** `time seq 1 10000000` is the benchmark. Record wall time.
3. **Never assume, never decide.** Discrepancy between plan and code — STOP, discuss with ARCHITECT.
4. **Follow all contracts.** JRENG-CODING-STANDARD, NAMING-CONVENTION, ARCHITECTURAL-MANIFESTO.
5. **ARCHITECT runs all git commands.** Agents prepare changes only.
6. **When uncertain — STOP and ASK.**
7. **No scope creep.** Each step does exactly what is described, nothing more.
8. **Verify `NativeImageType` pixel layout before relying on it.** Read `BitmapData` from a test image and confirm ARGB byte order and stride.
9. **SIMD code must have scalar fallback.** No platform left behind.
10. **No early returns.** Positive checks only. Zero early returns.

---

## Baseline Measurement (before any changes)

Capture before starting Phase 1:

```
time seq 1 10000000    # wall time, full-screen terminal, 2 splits
```

Record: wall time, CPU usage, visual smoothness during scroll.

---

## Dependencies

- Phase 1: No dependencies. Can start immediately.
- Phase 2: Step 2.0 (memmove) independent of Steps 2.1/2.2. All three can be done in any order.
- Phase 3: Depends on Phase 2 for meaningful measurement (SIMD on already-minimal work has less absolute impact). Step 3.0 (header) must come first. Steps 3.1/3.2/3.3 independent of each other after 3.0.

---

## Resolved Decisions

1. **Step 1.1 (setOpaque):** Terminal::Component owns the decision. `setOpaque(true)` + `setBufferedToImage(true)` + fill bg from LookAndFeel. CPU rendering = opaque, GPU rendering = transparent. Only glassmorphism is lost on CPU path. Plan 4 makes this user-configurable via rendering engine config (hot-reloadable). Machines without GPU capability auto-fallback to CPU path (opaque).
2. **Step 2.1 (cell skip):** `memcmp` on 16-byte Cell. More performant than hash — fewer instructions, zero collision risk. ~1MB per terminal acceptable.
3. **Step 3.0 (SIMD header):** `jreng_graphics` — SIMD optimization is specific to the Graphics rendering pipeline.
