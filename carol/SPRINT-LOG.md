# SPRINT-LOG.md

**Project:** end
**Repository:** /Users/jreng/Documents/Poems/dev/end
**Started:** 2026-02-02

**Purpose:** Long-term context memory across sessions. Tracks completed work, technical debt, and unresolved issues. Written by PRIMARY agents only when user explicitly requests.

---

## CRITICAL RULES (DO NOT ROTATE)
- **YOU ARE NOT ALLOWED TO BUILD, UNLESS ARCHITECT EXPLICITLY ASKS**

---

<!-- SPRINT HISTORY — latest first, keep last 5, rotate older to git history -->
## Sprint 22: ModalWindow refactor, forceDWM, middle-click drag, ligature fix, CPU atlas ownership

**Date:** 2026-04-03 — 2026-04-04

### Agents Participated
- COUNSELOR — root cause analysis for all issues, architectural decisions (ModalWindow extraction, atlas ownership in Typeface over static refcount), plan design, delegation
- Pathfinder (x6) — glass window analysis, Popup vs DialogWindow comparison, window drag code, terminal text rendering pipeline, pane/split font ownership, CPU renderer font lifecycle
- Engineer (x5) — ModalWindow module, Popup refactor, forceDWM registry, middle-click drag, ligature/xOffset fixes, DWM corner fix, CPU atlas ownership refactor
- Auditor (x2) — verified ligature/xOffset/DWM fixes (flagged stale DWM corner value), verified atlas ownership refactor (full PASS)

### Files Modified (18 total)

**ModalWindow refactor:**
- `modules/jreng_gui/glass/jreng_modal_window.h` — NEW: `ModalWindow : public GlassWindow` with Escape dismiss, input blocking, close button handling, `onModalDismissed` callback
- `modules/jreng_gui/glass/jreng_modal_window.cpp` — NEW: constructor delegates to GlassWindow, `closeButtonPressed` exits modal and invokes callback
- `modules/jreng_gui/jreng_gui.h` — added `#include "glass/jreng_modal_window.h"`
- `modules/jreng_gui/jreng_gui.cpp` — added `#include "glass/jreng_modal_window.cpp"`
- `Source/terminal/action/ActionList.h` — changed base from `GlassWindow` to `ModalWindow`, removed redundant override declarations
- `Source/terminal/action/ActionList.cpp` — changed base init, removed `closeButtonPressed`/`keyPressed` implementations
- `Source/component/Popup.h` — changed `Window` base from `DialogWindow` to `ModalWindow`, removed reimplemented glass/modal overrides
- `Source/component/Popup.cpp` — IIFE constructor for ContentView, LookAndFeel propagation, `keyPressed` returns false, deferred focus via `callAsync`

**forceDWM registry:**
- `Source/config/Config.h` — added `windowForceDwm` key
- `Source/config/Config.cpp` — registered boolean key
- `Source/config/default_end.lua` — documented entry in window section
- `Source/Main.cpp` — `applyForceDwmRegistry()` with named constants, call sites in `initialise()` and `config.onReload`
- `modules/jreng_core/utilities/jreng_platform.h` — added `isWindows11()` next to `isWindows10()`

**Rendering fixes and window drag:**
- `modules/jreng_gui/glass/jreng_glass_window.h:80-84,111-114` — replaced `findControlAtPoint` with `mouseDown`/`mouseDrag` overrides, added `ComponentDragger` member
- `modules/jreng_gui/glass/jreng_glass_window.cpp:63-65,119,142-162` — added `addMouseListener(this,true)`, fixed DWM corner value 4→2, mouseDown/mouseDrag with ComponentDragger
- `Source/terminal/rendering/ScreenRender.cpp:266,939` — added `sg.xOffset` to glyph positioning, reject partial ligatures (`shaped.count == 1`)

**CPU atlas ownership:**
- `modules/jreng_graphics/fonts/jreng_typeface.h:810-825,1179-1180` — added `cpuMonoAtlas`/`cpuEmojiAtlas` members and `ensureCpuAtlas`/`getCpuMonoAtlas`/`getCpuEmojiAtlas` accessors
- `modules/jreng_graphics/rendering/jreng_glyph_graphics_context.h:275-278` — removed static shared atlas + refcount, replaced with per-frame cached pointers
- `modules/jreng_graphics/rendering/jreng_glyph_graphics_context.cpp` — rewrote lifecycle: no refcount, atlas access through Typeface via `uploadStagedBitmaps`
- `Source/terminal/rendering/ScreenGL.cpp:189` — unconditional `textRenderer.createContext()` (idempotent)
- `Source/terminal/rendering/ScreenGL.cpp:189` — unconditional `textRenderer.createContext()` (idempotent, no isReady guard)

### Alignment Check
- [x] BLESSED principles followed — B: atlas owned by Typeface (clear owner, RAII), no manual refcount; S(SOT): single atlas source in Typeface; S(tateless): GraphicsContext holds no persistent atlas state, caches per-frame; E(xplicit): jassert added for atlas pointer contract; E(ncapsulation): Typeface owns, renderer accesses via API
- [x] NAMES.md adhered — `cpuMonoAtlas`, `cpuEmojiAtlas`, `ensureCpuAtlas`, `windowDragger` follow semantic naming
- [x] MANIFESTO.md principles applied — no new patterns invented, follows existing Typeface delegation pattern

### Problems Solved
- **Popup white flash and reimplemented glass** — Popup subclassed `juce::DialogWindow` and reimplemented glass blur with `AsyncUpdater`, creating a visible white flash on Windows. Extracted `jreng::ModalWindow` (GlassWindow subclass with modal semantics). Popup now inherits ModalWindow — unified glass lifecycle, synchronous blur on Windows, no flash.
- **Popup missing LookAndFeel** — Popup is a separate top-level window, doesn't inherit MainComponent's Terminal::LookAndFeel. Fix: set LookAndFeel on ContentView from `centreAround` component.
- **ESC dismissing Popup** — ModalWindow::keyPressed intercepts Escape. But Popup hosts Terminal::Component which needs Escape for terminal operations. Fix: override `keyPressed` in Popup::Window to return false unconditionally.
- **Popup keyboard focus** — `enterModalState(true)` stole focus after initial grab. Fix: deferred focus via `juce::MessageManager::callAsync` after `enterModalState`.
- **forceDWM for Windows 11 VMs** — DWM rounded corners not applied on VMs without GPU compositing. Added `window.force_dwm` config entry that writes `ForceEffectMode=2` to HKLM registry.
- **Middle-click window drag** — Meta+left-click replaced with middle mouse button drag. `findControlAtPoint` only works for left-click hit-testing (WM_NCHITTEST). Replaced with `mouseDown`/`mouseDrag` overrides using `juce::ComponentDragger` and `addMouseListener(this, true)` to receive child events.
- **Double-colon glyph overlap** — character after `::` drawn one cell left. Root cause: `tryLigature` accepted 3→2 partial ligatures (e.g. `::c` shaped as 2 glyphs), but assigned fixed `xAdvance=physCellWidth` per glyph, mispositioning the second glyph. Fix: only accept full-collapse ligatures (`shaped.count == 1`).
- **HarfBuzz xOffset dropped** — `emitShapedGlyphsToCache` computed `glyphX = currentX + bearingX` without `sg.xOffset`. GPOS mark/kern adjustments were silently ignored. Fix: added `sg.xOffset` term.
- **DWM corner value** — `DWMWCP_ROUND` is 2, not 4. Stale comment/value mismatch from previous sprint.
- **CPU atlas destruction on first pane close** — `GraphicsContext` used static shared atlas images with manual refcount. Second pane never called `createContext()` because `isReady()` checked global atlas state (already valid from first pane). First pane close decremented refcount to 0, destroying atlas. Fix: moved atlas ownership to `Typeface` (outlives all renderers). No refcount needed. `GraphicsContext` caches atlas pointers per-frame via `uploadStagedBitmaps(typeface)`.

### Technical Debt / Follow-up
- `GLContext` has the same static refcount pattern for GL textures (`sharedMonoAtlas`/`sharedEmojiAtlas`/`sharedAtlasRefCount`). Currently works because GL context outlives panes, but same B violation exists. Consider moving GL atlas texture ownership to a long-lived GL-thread object.
- `GLContext::~GLContext() = default` never calls `closeContext()` — GL resources (shaders, VAO, VBOs) leaked when pane closes. Separate from the atlas bug but related B violation.
- `addMouseListener(this, true)` on all GlassWindows (including ModalWindow subclasses) — verify middle-click drag doesn't interfere with popup/ActionList behavior.

## Sprint 21: GPU Probe Auto-Detection

Date: 2026-04-03

### Agents Participated

COUNSELOR: Led requirements gathering, designed resolution flow and truth table, planned all steps, directed all subagents

Pathfinder: Discovered GPU acceleration code paths, Config/AppState patterns, Context CRTP pattern, GL header/link setup

Librarian: Researched JUCE juceopengl GL symbol namespace (juce::gl::) and function pointer loading

Engineer: Implemented Gpu struct, platform probes, AppState changes, MainComponent refactor, App::RendererType enum migration

Auditor: Three audit passes — caught SSOT violations (duplicated constants/functions, magic strings), stale includes, naming issues

Machinist: Cleaned stale includes from MainComponent.h and TerminalComponent.h

Files Modified (15 total)

Source/Gpu.h — NEW struct with probe(), isSoftwareRenderer(), probe constants, softwareRenderers StringArray

Source/Gpu.cpp — NEW Windows WGL probe: dummy HWND, 3.2 Core context bootstrap, pixel readback, RAII ProbeContext

Source/Gpu.mm — NEW macOS NSOpenGLContext probe: NSOpenGLPFAAccelerated, pixel readback

Source/AppIdentifier.h — Added App::RendererType enum (gpu/cpu), gpuAvailable identifier, rendererGpu/rendererCpu string constants

Source/AppState.h — getRendererType() returns App::RendererType enum; added setGpuAvailable(bool); setRendererType() takes raw config string

Source/AppState.cpp — setRendererType() resolves internally (config setting AND gpuAvailable); getRendererType() maps ValueTree string to enum; removed renderer from initDefaults()

Source/Main.cpp — Probe at startup before window creation, store result in AppState, glass/reload read AppState enum

Source/MainComponent.h — Removed ValueTree::Listener, removed windowState member, added setRenderer(App::RendererType)

Source/MainComponent.cpp — Removed all config ternaries, direct setRenderer() calls from applyConfig(), default compact atlas in constructor

Source/component/PaneComponent.h — Removed RendererType enum (moved to App namespace), uses App::RendererType

Source/component/TerminalComponent.h — Updated include chain, uses App::RendererType

Source/component/TerminalComponent.cpp — switchRenderer uses AppState::getContext()->getRendererType(), updated stale comment

Source/component/Tabs.h / Tabs.cpp — Updated to App::RendererType

Source/component/Popup.cpp — Updated to App::RendererType, reads AppState directly

Source/whelmed/Component.h / Component.cpp — Updated to App::RendererType

### Alignment Check

x BLESSED principles followed — stateless probe, SSOT in AppState, RAII resource management, no shadow state

x NAMES.md adhered — semantic naming, verbs for actions, no hungarian notation

x MANIFESTO.md principles applied — unidirectional flow, tell-don't-ask, encapsulation

### Problems Solved

gpu.acceleration = "auto" blindly selected GPU — no actual hardware detection existed. On Windows ARM64 UTM (no real GPU), OpenGL context creation failed silently and terminals never rendered

GPU probe now creates a native GL context, queries GLRENDERER against known software renderers (GDI Generic, llvmpipe, SwiftShader, etc.), runs FBO pixel readback verification

Resolution truth table: renderer = wantsGpu AND gpuAvailable — encapsulated in AppState::setRendererType()

Removed self-listening ValueTree pattern from MainComponent — was reacting to its own writes

Moved RendererType enum from PaneComponent to App namespace — renderer type is an app concern, not a component concern

### Technical Debt / Follow-up

Source/Gpu_windows.cpp, Source/Gpu_mac.mm, Source/component/RendererType.h — dead stub files, need git rm

PLAN-GPU-PROBE.md — can be deleted after commit

macOS probe untested (implemented, platform-guarded, mirrors Windows pattern)

## Sprint 20: Popup PATH injection from active terminal

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR — root cause analysis, plan design, delegation, auditor coordination
- Pathfinder (x2) — popup/process spawning code discovery, shell integration scripts, Terminal::Component structure
- Engineer (x3) — TTY layer `getEnvVar` implementation, Session/TerminalComponent `getShellEnvVar`+`addExtraEnv`, MainComponent popup PATH wiring
- Auditor (x2) — TTY layer verification, full feature audit (found C1 noexcept+alloc, H1 null guard, H2 early returns)
- Machinist — fixed C1 (removed noexcept), fixed H1 (added tty null guard)

### Files Modified (8 total)
- `Source/terminal/tty/TTY.h:252-262` — added `getEnvVar` virtual (reads env var from PID, default returns 0)
- `Source/terminal/tty/UnixTTY.h:210-223` — added `getEnvVar` override declaration
- `Source/terminal/tty/UnixTTY.cpp:27-32` — added `<sys/sysctl.h>` to `__APPLE__` include block
- `Source/terminal/tty/UnixTTY.cpp:499-613` — implemented `getEnvVar` (macOS: `sysctl KERN_PROCARGS2` env parse, Linux: `/proc/pid/environ` parse)
- `Source/terminal/logic/Session.h:140-162` — added `getShellEnvVar` and `addExtraEnv` public declarations
- `Source/terminal/logic/Session.cpp:283-311` — implemented both methods (getShellEnvVar reads from foreground PID with tty null guard, addExtraEnv delegates to tty->addShellEnv)
- `Source/component/TerminalComponent.h:284-302` — exposed `getShellEnvVar` and `addExtraEnv` (delegates to Session)
- `Source/component/TerminalComponent.cpp` — delegating implementations
- `Source/MainComponent.cpp:564-572` — popup launch reads PATH from active terminal, injects into popup via `addExtraEnv`

### Alignment Check
- [x] BLESSED principles followed — Bound (TTY owns env reading, Session delegates, no floating state), Lean (minimal additions per layer), Explicit (semantic names, no magic values, `maxEnvValueLength` named constant), SSOT (PATH read from live process, no cached copy), Encapsulation (TerminalComponent→Session→TTY delegation chain, no internals poked)
- [x] NAMES.md adhered — `getEnvVar`, `getShellEnvVar`, `addExtraEnv`, `shellPath`, `maxEnvValueLength` follow verb+noun, semantic naming, consistency with `getCwd`/`getProcessName`/`setShellProgram`
- [x] MANIFESTO.md principles applied — follows existing `getCwd`/`getProcessName` pattern exactly, no new patterns invented
- [x] JRENG-CODING-STANDARD — brace init, `and`/`or`/`not` tokens, braces on new lines, space after function names, `.at()` not applicable (raw buffers)

### Problems Solved
- **Popup commands not found on macOS** — popups run `zsh -c "command"` which only sources `.zshenv` (non-interactive, non-login). If user's PATH additions are in `.zshrc`/`.zprofile`, popup can't find commands in `~/.local/bin` etc. Fixed by reading PATH from the active terminal's shell process (which has the full interactive PATH) and injecting it into the popup's environment before PTY open. Works on MSYS2 already because Windows GUI apps inherit full user env.

### Technical Debt / Follow-up
- Early returns in `getEnvVar` match pre-existing pattern in `getCwd`/`getProcessName` — accepted as TTY layer convention, not refactored
- `std::vector` heap allocation in `getEnvVar` (macOS path: variable-size `sysctl` buffer) — `noexcept` correctly removed, but stack allocation alternative could be considered if OOM is a concern
- Linux `/proc/pid/environ` reads up to 64KB (`envBufSize`) — sufficient for typical PATH but could theoretically truncate extreme environments
- WindowsTTY has no `getEnvVar` override (returns 0 from base) — not needed since Windows popup PATH works OOTB, but could be added for completeness

## Sprint 19: Font metrics — aspect ratio, DPI restore, popup sizing

**Date:** 2026-04-02

### Agents Participated
- COUNSELOR — analysis of cell metrics vs Kitty/Ghostty/WezTerm, plan design, delegation, auditor coordination
- Pathfinder (x5) — font metrics code discovery, FreeType/CoreText paths, config key patterns, embedded font data, popup padding flow
- Librarian — JUCE Windows DPI awareness research
- Engineer (x3) — font.line_height config + calc(), font.cell_width config + calc(), popup sizing fix
- Auditor (x2) — verified line_height implementation (found placeholder mismatch C1), verified popup sizing (found rounding divergence M1, naming m1)

### Files Modified (8 total)
- `Source/config/Config.h:195-198` — added `fontLineHeight` ("font.line_height") and `fontCellWidth` ("font.cell_width") key declarations
- `Source/config/Config.cpp:102-103` — registered both keys: number, range 0.5–3.0, default 1.0
- `Source/config/default_end.lua:71-77` — added `line_height = %%font_line_height%%` and `cell_width = %%font_cell_width%%` to font section
- `Source/terminal/rendering/Screen.h:373,379,921-922` — added `setLineHeight()` and `setCellWidth()` declarations, `lineHeightMultiplier` and `cellWidthMultiplier` members (default 1.0f)
- `Source/terminal/rendering/Screen.cpp:66-86` — `calc()` applies both multipliers: height with baseline centering (extra/2 shift), width with truncation
- `Source/terminal/rendering/Screen.cpp:271-285` — `setLineHeight()` and `setCellWidth()` implementations (guard + recalc pattern matching `setFontSize`)
- `Source/component/TerminalComponent.cpp:712-713` — `applyScreenSettings()` reads config, calls `setLineHeight()` and `setCellWidth()`
- `Source/MainComponent.cpp:544-557` — popup pixel sizing now includes terminal padding (all 4 sides), titleBarHeight, and cell multipliers
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:247-249` — added render DPI restore after metric computation (implements documented step 8)

### Alignment Check
- [x] BLESSED principles followed — SSOT (cell multipliers from Config, popup sizing reads same Config keys as TerminalComponent), Explicit (multiplier names semantic, all params visible), Encapsulation (Screen owns multipliers, told via setLineHeight/setCellWidth API), Lean (minimal additions)
- [x] NAMES.md adhered — `lineHeightMultiplier`, `cellWidthMultiplier`, `effectiveCellW/H`, `paddingTop/Right/Bottom/Left` match established patterns
- [x] MANIFESTO.md principles applied — no new patterns invented, follows existing setFontSize guard+recalc pattern
- [x] JRENG-CODING-STANDARD — brace init, `not`/`and`/`or` tokens, no early returns, space after function names

### Problems Solved
- **Cell aspect ratio** — terminal had no user control over cell dimensions. Added `font.line_height` (height multiplier) and `font.cell_width` (width multiplier), matching Kitty/Ghostty/WezTerm pattern. Extra height centered via baseline shift (half above, half below).
- **DPI restore bug** — `calcMetrics()` doc promised step 8 (restore face to renderDpi) but code never did it. Face left at baseDpi (96) after every cache miss. On HiDPI (scale > 1.0), glyphs rasterized at wrong DPI — 43% too small at 175% scaling (UTM Windows 11). Fixed by adding `FT_Set_Char_Size` restore after metric read.
- **Popup sizing** — popup pixel dimensions computed from raw `cols * cellW` without accounting for terminal padding (default 10px all sides) or titleBarHeight. Grid always smaller than configured cols/rows. Fixed by adding padding + titleBar + multipliers to pixel calculation.

### Analysis Data (for future reference)
- Display Mono Book: UPM=1003, hhea ascender=881, descender=-122, lineGap=250
- Competitor cell width: all use max ASCII advance (32-127), same as end
- Competitor cell height: Kitty/WezTerm use `face->height` (design units, includes lineGap), Ghostty reads hhea tables directly. End uses `face->size->metrics.height` (should be equivalent per FreeType internals)
- DPI restore bug: only affects non-Mac (FreeType path). Severity = `1 - 1/scale` (20% at 125%, 43% at 175%)
- Mac calcMetrics (CoreText) measures only space glyph for width; Kitty measures max across ASCII 32-127. For true monospace fonts these should be equal.

### Technical Debt / Follow-up
- `PLAN-font-metrics.md` at project root — should be removed after commit (analysis artifact)
- Mac CoreText `calcMetrics` measures space glyph only for width vs competitors measuring max ASCII — investigate if this causes width mismatch for non-monospace fallback fonts
- The `24` magic number for titleBarHeight appears in TerminalComponent.h:699 and now MainComponent.cpp — consider extracting to a named constant
