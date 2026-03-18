# Technical Debt

**Project:** END  
**Purpose:** Long-lived technical debt items that span multiple sprints.

---

## Software Rendering Fallback (No GPU)

**Priority:** High  
**Context:** END and wezterm both crash on Windows 11 UTM (no GPU acceleration). The GL + DWM compositing stack requires GPU drivers. This blocks END from running in virtualized environments without GPU passthrough.

**Current state:** END requires OpenGL for all rendering. The glyph atlas is a GPU texture (`GL_R8` mono, `GL_RGBA` emoji). Without a GL context, nothing renders.

**Proposed approach:** CPU-side glyph atlas via `juce::Image` + `juce::Graphics` as a second rendering backend.

- Pre-rasterize each unique glyph once into a `juce::Image` cache (same atlas concept, CPU-side)
- Per frame: blit cached images to cell positions via `drawImageAt()`, fill background rects
- Skip GL entirely — no shaders, no textures, no `OpenGLContext`
- Skip DWM blur — opaque window with tint from config background colour
- The terminal data model (Grid, Parser, Session, TTY) is rendering-agnostic — only the `Screen` layer needs a second implementation

**Why this would be faster than the original `juce::GlyphArrangement`:** The first END iteration used `juce::GlyphArrangement` which does full text shaping + layout per frame (slow for 4800+ cells). The atlas approach pre-rasterizes once and blits — no shaping, no layout. `juce::Graphics` backed by Direct2D can handle ~10000 rect+blit draw calls per frame.

**Scope:** New `ScreenSoftware` class alongside existing `ScreenGL`. Renderer selection at startup based on GL context availability.

---

## Windows 11 UTM/No-GPU Crash

**Priority:** High  
**Sprint:** 100  
**Context:** Black window, no rendering, crash on Windows 11 UTM (virtio-ramfb-gl). The Win11 DWM blur path (strip `WS_EX_LAYERED` + `ACCENT_ENABLE_ACRYLICBLURBEHIND`) likely fails without GPU acceleration.

**Needs:** Dedicated debug session on UTM with file-based diagnostics to identify exact failure point. Then add graceful fallback — detect no-GPU and skip DWM blur, render opaque window.

**Blocked by:** Software rendering fallback (above). Even if DWM blur is skipped, GL rendering may not work without GPU drivers.

---

## Windows 10 Regression Test

**Priority:** Medium  
**Sprint:** 100  
**Context:** The Windows 10 blur path (`ACCENT_ENABLE_BLURBEHIND` + `AccentFlags=2`) is preserved but untested after Sprint 100 changes: ConPTY sideload always-on, `isWindows10()` moved to `jreng_platform.h`, `enableGLTransparency()` unchanged.

**Needs:** Build and test on Windows 10 22H2 to verify blur, tint, ConPTY, mouse, and terminal functionality.

---

## enableGLTransparency() Redundancy on Windows 11

**Priority:** Low  
**Sprint:** 100  
**Context:** On Windows 11, `applyDwmGlass()` already strips `WS_EX_LAYERED` and calls `DwmExtendFrameIntoClientArea`. `enableGLTransparency()` does the same — idempotent but redundant. On Windows 10, `enableGLTransparency()` is still required (applyDwmGlass doesn't do these operations on Win10).

**Needs:** Could be guarded with `if (isWindows10())` inside `enableGLTransparency()`, but the redundancy is harmless. Low priority.

---

## Pre-existing Debt (from Sprint 91+)

- **`getTreeMode()` / `getTreeKeyboardFlags()` naming** — violates NAMING-CONVENTION Rule 2 (encodes "Tree" in name). Should be renamed to semantic names.
- **`seq 1M` performance gap** — 2m33s vs Windows Terminal's 1m12s. Reader thread drain loop exits too early.
- **`CursorComponent` missing `setInterceptsMouseClicks(false, false)`** — cursor cell swallows clicks.
