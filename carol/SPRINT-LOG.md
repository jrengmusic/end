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
## Handoff to COUNSELOR: Action::List (Command Palette)

**From:** COUNSELOR
**Date:** 2026-04-04
**Status:** In Progress

### Context
Building the command palette (Action::List) for END. Restructured `Terminal::Action` → `Action::Registry`, built full command palette with fuzzy search, keyboard navigation, shortcut display, and inline key remap dialog.

SPEC document: `SPEC-ACTION-LIST.md` (project root) — 10-step plan, Steps 1-10 complete with modifications.

### Completed
- **Steps 1-3:** Namespace restructure — moved `Source/terminal/action/` → `Source/action/`, renamed `Terminal::Action` → `Action::Registry`, `Terminal::ActionList` → `Action::List`, updated all call sites (Main.cpp, MainComponent.h/cpp, both InputHandler.cpp)
- **Step 4:** `Registry::shortcutToString` — inverse of `parseShortcut`, round-trips correctly
- **Step 5:** Config additions — `action_list.*` keys for fonts, colours, padding, close_on_run
- **Step 6:** `Config::patchKey` — targeted line replacement in end.lua
- **Step 7:** `Action::LookAndFeel` — `getLabelFont` dispatch via `dynamic_cast` on `NameLabel`/`ShortcutLabel`
- **Step 8:** `Action::Row` — name + shortcut labels, double-click handlers
- **Step 9:** `Action::KeyRemapDialog` — rewritten from ModalWindow to inline child Component with learn mode, modal toggle, OK/Cancel
- **Step 10:** `Action::List` — full implementation with fuzzy search, keyboard nav, popup-level window flags
- **`Registry::configKeyForAction`** — resolves action ID to config key via actionKeyTable
- **`Registry::buildKeyMap`** — now populates `Entry::shortcut` field
- **`jreng::ChildWindow`** — OS-level child window utility (macOS + Windows) in jreng_gui module
- **Window flags** — `getDesktopWindowStyleFlags` returns `windowIsTemporary | windowHasTitleBar | windowHasDropShadow` (popup-level, invisible to tiling WMs)
- **Hammerspoon config** — `~/.config/hammerspoon/paper-wm/init.lua` updated with `rejectTitles` filter for END

### Remaining
- **Build verification** — full build not yet confirmed after KeyRemapDialog rewrite to child Component
- **KeyRemapDialog visual polish** — buttons, toggle, layout need glass-style colours and font consistency
- **KeyRemapDialog modal toggle** — `isModal` parameter passed through `onCommit` but not yet consumed (patchKey writes shortcut only, doesn't change modal/global slot)
- **ValueTree cleanup** — `jreng::ValueTree state` member removed from List, but `handleValueChanged` wiring and `jreng::ValueTree::attach` calls were removed. Verify no dangling references
- **Shortcut remap end-to-end** — `onCommit` calls `patchKey` + `reload()` + `closeButtonPressed()`. Needs testing that reload doesn't crash (rebuilds all actions while List is alive)
- **`ActionList.cpp` exceeds 300 lines** (378) — BLESSED Lean violation, may need further extraction
- **Block caret** — ARCHITECT deferred, using JUCE default caret for now
- **`jreng::ChildWindow`** — created but may be unused now (popup-level flags solved the PaperWM problem). ARCHITECT may want to keep or remove

### Key Decisions
- `Terminal::Action` → `Action::Registry` namespace restructure (ARCHITECT approved)
- `Action::List` inherits `jreng::ModalWindow` (not child Component) — keeps glass blur, modal semantics
- `getDesktopWindowStyleFlags` returns `windowIsTemporary` — makes window popup-level, invisible to tiling WMs (PaperWM, Yabai)
- `windowHasTitleBar` added back for native rounded corners (macOS)
- `KeyRemapDialog` changed from separate ModalWindow to inline child Component — avoids nested modal crash
- Font names: `"Display"` (proportional) and `"Display Mono Bold"` (mono bold) — matches embedded binary font family names
- Config colours: `#A1D6E5` (skyFall) for action names, `#00C8D8` (blueBikini) for shortcuts
- `Config::patchKey` + `Config::reload()` for shortcut remapping — writes to end.lua, reloads everything
- After remap commit, List closes via `closeButtonPressed()` to avoid stale entry references

### Files Modified (26 total)

**New files (Source/action/):**
- `Source/action/Action.h` — `Action::Registry` (was `Terminal::Action`)
- `Source/action/Action.cpp` — registry impl + `shortcutToString` + `configKeyForAction`
- `Source/action/ActionList.h` — `Action::List` class
- `Source/action/ActionList.cpp` — full command palette implementation
- `Source/action/ActionRow.h` — `Row`, `NameLabel`, `ShortcutLabel`
- `Source/action/ActionRow.cpp` — row layout, double-click, config-driven colours
- `Source/action/KeyRemapDialog.h` — inline remap overlay (child Component)
- `Source/action/KeyRemapDialog.cpp` — learn mode, OK/Cancel, modal toggle
- `Source/action/LookAndFeel.h` — font dispatch for labels
- `Source/action/LookAndFeel.cpp` — `getLabelFont` override

**New files (jreng_gui module):**
- `modules/jreng_gui/glass/jreng_child_window.h` — `jreng::ChildWindow` utility
- `modules/jreng_gui/glass/jreng_child_window.mm` — macOS impl (NSWindow addChildWindow)
- `modules/jreng_gui/glass/jreng_child_window.cpp` — Windows impl (GWLP_HWNDPARENT)

**Modified files:**
- `Source/Main.cpp` — `Action::Registry action;`, include path
- `Source/MainComponent.h` — includes, `std::unique_ptr<Action::List>`
- `Source/MainComponent.cpp` — `Action::Registry::getContext()`, `onModalDismissed` wiring
- `Source/component/InputHandler.cpp` — include path, `Action::Registry::` namespace
- `Source/component/TerminalComponent.cpp` — removed stale include
- `Source/whelmed/InputHandler.cpp` — include path, `Action::Registry::` namespace
- `Source/config/Config.h` — action_list Key constants, `patchKey` declaration
- `Source/config/Config.cpp` — action_list defaults, `patchKey` implementation
- `Source/config/default_end.lua` — action_list block with all config keys
- `modules/jreng_gui/jreng_gui.h` — `jreng_child_window.h` include
- `modules/jreng_gui/jreng_gui.cpp` — `jreng_child_window.cpp` include
- `modules/jreng_gui/jreng_gui.mm` — `jreng_child_window.mm` include

**Deleted:**
- `Source/terminal/action/` — entire directory (4 files moved to Source/action/)

**External:**
- `~/.config/hammerspoon/paper-wm/init.lua` — END window filter

### Open Questions
- Should `jreng::ChildWindow` be kept (general utility) or removed (popup-level flags solved the immediate need)?
- Should the modal toggle in KeyRemapDialog actually change the binding type (modal ↔ global)? Currently the `isModal` flag is passed to `onCommit` but not consumed
- `ActionList.cpp` at 378 lines violates BLESSED Lean 300-line limit — needs further method extraction

### Next Steps
1. Build and verify no compilation errors after KeyRemapDialog rewrite
2. Test Action::List end-to-end: open, search, navigate, execute, dismiss
3. Test shortcut remap: click shortcut → dialog → learn/type → OK → verify end.lua patched → verify new binding works
4. Visual polish: KeyRemapDialog needs glass-consistent styling (button colours, label fonts)
5. Address 300-line violation in ActionList.cpp

---

## Sprint 23: JRENG — Debt Cleanup, GLAtlasRenderer, Popup Shell Fix

**Date:** 2026-04-04

### Agents Participated
- COUNSELOR — debt inventory across SPRINT-LOG/DEBT.md/SPEC.md/SPEC-MODAL.md/SPEC-details.md, cross-referenced codebase state, planned all fixes, delegated to subagents
- Pathfinder (x5) — GLContext atlas/destructor analysis, kuassa GL module comparison, CoreText calcMetrics code, TTY getEnvVar call chain, titleBarHeight sites
- Engineer (x6) — titleBarHeight constant, Typeface GL atlas storage, GLContext statics removal, GLRenderer decoupling, GLAtlasRenderer subclass, CoreText max ASCII fix, platform-guard getEnvVar chain
- Auditor (x2) — full audit of GL refactor (found 4 flags: C-1 early return, H-1 ownership comment, H-2 thread contract, L-2 terse naming), re-verification pass (all PASS)

### Files Modified (20 total)

**Dead file cleanup:**
- `Source/Gpu_windows.cpp` — `git rm` (dead stub)
- `Source/Gpu_mac.mm` — `git rm` (dead stub)
- `Source/component/RendererType.h` — `git rm` (dead stub)
- `PLAN-font-metrics.md` — `git rm` (analysis artifact)

**titleBarHeight constant:**
- `Source/AppIdentifier.h:36` — added `App::titleBarHeight` constexpr
- `Source/component/TerminalComponent.h:718` — replaced magic `24` with `App::titleBarHeight`
- `Source/MainComponent.cpp:523,609` — replaced magic `24` with `App::titleBarHeight` (2 sites)

**GL atlas ownership (B violation fix):**
- `modules/jreng_graphics/fonts/jreng_typeface.h:827-840,1204-1205` — added `glMonoAtlas`/`glEmojiAtlas` uint32_t handles + get/set/reset accessors, GL THREAD section comment
- `modules/jreng_opengl/renderers/jreng_gl_context.h` — removed 3 statics, added per-frame cached `monoAtlas`/`emojiAtlas` members, destructor calls `closeContext()`, `createAtlasTexture` made public
- `modules/jreng_opengl/renderers/jreng_gl_context.cpp` — removed static definitions, `createContext()` shaders+buffers only, `closeContext()` shaders+buffers only, `uploadStagedBitmaps()` lazy-creates atlas on Typeface and caches per-frame, `drawQuads()` uses member handles

**GLRenderer decoupling:**
- `modules/jreng_opengl/context/jreng_gl_renderer.h` — `private` inheritance to `protected`, virtual destructor, removed `glyphContext` member, added `contextReady()`/`contextClosing()`/`renderText()` virtual hooks, `renderComponent` protected virtual, `notifyComponentsCreated()`/`notifyComponentsClosing()` helpers
- `modules/jreng_opengl/context/jreng_gl_renderer.cpp` — lifecycle restructured with hooks, text rendering removed from `renderComponent`, `renderText()` called after path/shape, `drawVertices` early return converted to positive nested check

**GLAtlasRenderer subclass:**
- `modules/jreng_opengl/context/jreng_gl_atlas_renderer.h` — NEW: `GLAtlasRenderer : public GLRenderer`, owns `Glyph::GLContext`, manages typeface GL atlas lifecycle via initializer_list
- `modules/jreng_opengl/context/jreng_gl_atlas_renderer.cpp` — NEW: `contextReady()` creates glyph context, `contextClosing()` closes glyph context + deletes typeface GL textures, `renderText()` merged upload+draw loop per font command

**Module + call site updates:**
- `modules/jreng_opengl/jreng_opengl.h` — added `#include "context/jreng_gl_atlas_renderer.h"`
- `modules/jreng_opengl/jreng_opengl.cpp` — added `#include "context/jreng_gl_atlas_renderer.cpp"`
- `Source/MainComponent.h` — member reorder (typefaces before glRenderer), type changed to `GLAtlasRenderer` with brace-init typeface list

**CoreText max ASCII cell width:**
- `modules/jreng_graphics/fonts/jreng_typeface.mm:760-773` — replaced space-glyph-only measurement with max-advance scan across ASCII 32-127, matching FreeType path

**Platform-guard getEnvVar chain:**
- `Source/terminal/tty/TTY.h:253-264` — `getEnvVar` virtual wrapped with `#if ! JUCE_WINDOWS`
- `Source/terminal/logic/Session.h:150` — `getShellEnvVar` declaration guarded
- `Source/terminal/logic/Session.cpp:284-306` — `getShellEnvVar` implementation guarded
- `Source/component/TerminalComponent.h:290` — `getShellEnvVar` declaration guarded
- `Source/component/TerminalComponent.cpp:576-579` — `getShellEnvVar` implementation guarded
- `Source/MainComponent.cpp:543-551` — popup PATH injection block guarded

**Popup shell fix:**
- `Source/MainComponent.cpp:513-514` — popup shellArgs now prepends `config.getString(Config::Key::shellArgs)` before `-c`

### Alignment Check
- [x] BLESSED principles followed — B: atlas ownership explicit (Typeface stores handles, GLAtlasRenderer owns GPU resource, one owner one lifecycle), no refcount; L: GLAtlasRenderer is lean subclass, no god objects; E: all names semantic, no magic numbers, thread contracts documented; S(SOT): atlas source is Typeface (single truth, mirrors CPU); S(tateless): GLContext caches per-frame, holds no persistent atlas state; E(ncapsulation): GLRenderer general-purpose, text capability via subclass, Unix-only feature platform-guarded; D: same typeface + same atlas = same rendering
- [x] NAMES.md adhered — `contextReady`/`contextClosing`/`renderText` (verbs, tell-don't-ask), `GLAtlasRenderer` (semantic), `monoHandle`/`emojiHandle` (clarity over brevity), `App::titleBarHeight` (named constant)
- [x] MANIFESTO.md principles applied — no early returns (drawVertices fixed), positive nested checks throughout, no manual boolean flags, no refcount

### Problems Solved
- **GLContext static refcount (B violation)** — `sharedMonoAtlas`/`sharedEmojiAtlas`/`sharedAtlasRefCount` were statics shared across GLContext instances via manual refcount. Same bug pattern as the CPU atlas crash (Sprint 22). Fixed by moving atlas handle storage to Typeface (mirroring CPU pattern), lazy creation in `uploadStagedBitmaps`, lifecycle managed by single `GLAtlasRenderer` instance. No refcount, no manual tracking.
- **GLContext destructor resource leak** — `~GLContext() = default` never called `closeContext()`. GL shaders/VAO/VBOs leaked on pane close. Fixed: destructor calls `closeContext()` (guarded by `contextInitialised` for idempotency).
- **GLRenderer text coupling** — GLRenderer owned `Glyph::GLContext glyphContext` and rendered text in `renderComponent()`, coupling the general-purpose renderer to text. Blocks clean fork-back to kuassa. Fixed: extracted text to `GLAtlasRenderer` subclass via virtual hooks. GLRenderer is now general-purpose.
- **titleBarHeight magic number** — `24` appeared in 3 locations. Extracted to `App::titleBarHeight` constant.
- **drawVertices early return** — pre-existing MANIFESTO E violation. Converted to positive nested check.
- **CoreText cell width measurement** — Mac measured only space glyph advance for cell width. FreeType scanned max across ASCII 32-127. Asymmetry could cause width mismatch with non-monospace fallback fonts. Fixed: CoreText now scans ASCII 32-127 max advance.
- **getEnvVar leaky abstraction** — base `TTY` exposed Unix-only `getEnvVar` virtual. Windows inherits full user env — never needs it. Fixed: entire `getEnvVar`/`getShellEnvVar` chain platform-guarded to `#if ! JUCE_WINDOWS`.
- **Popup shell ignoring config shell.args** — popup hardcoded `-c <cmd>`, ignoring `Config::Key::shellArgs` (defaults to `-l` on macOS/Linux). Main terminal used config args, popup didn't — shell mode mismatch. From Finder launch: popup ran `zsh -c tit` (non-login, non-interactive → minimal PATH → command not found → immediate exit). Fixed: popup now prepends config `shell.args` → `zsh -l -c tit` (login shell, sources full env). Cross-platform — Windows config sets appropriate args.

### Technical Debt / Follow-up
- `addMouseListener(this, true)` on GlassWindows — verify middle-click drag doesn't interfere with popup/ActionList behavior. Deferred: ActionList rework planned for future sprint.
- Windows 10 regression test (DEBT.md) — needs Windows 10 22H2 hardware.
- macOS GPU probe untested (Sprint 21).

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
- ~~`GLContext` static refcount for GL textures~~ — resolved Sprint 23: atlas ownership on Typeface, `GLAtlasRenderer` subclass, no refcount.
- ~~`GLContext::~GLContext() = default` never calls `closeContext()`~~ — resolved Sprint 23: destructor calls `closeContext()`.
- `addMouseListener(this, true)` on all GlassWindows (including ModalWindow subclasses) — verify middle-click drag doesn't interfere with popup/ActionList behavior. Deferred: ActionList rework planned.

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

~~Source/Gpu_windows.cpp, Source/Gpu_mac.mm, Source/component/RendererType.h — dead stub files, need git rm~~ — resolved Sprint 23

~~PLAN-GPU-PROBE.md — can be deleted after commit~~ — already gone

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
- ~~Early returns in `getEnvVar`~~ — resolved Sprint 23: audit found no early returns, debt was stale
- `std::vector` heap allocation in `getEnvVar` (macOS path: variable-size `sysctl` buffer) — `noexcept` correctly removed, but stack allocation alternative could be considered if OOM is a concern
- Linux `/proc/pid/environ` reads up to 64KB (`envBufSize`) — sufficient for typical PATH but could theoretically truncate extreme environments
- ~~WindowsTTY has no `getEnvVar` override~~ — resolved Sprint 23: not debt. Full `getEnvVar`/`getShellEnvVar` chain platform-guarded to `#if ! JUCE_WINDOWS`. Unix-only feature no longer exposed on Windows.

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
- ~~`PLAN-font-metrics.md` at project root~~ — resolved Sprint 23: `git rm`
- ~~Mac CoreText `calcMetrics` measures space glyph only~~ — resolved Sprint 23: now scans max across ASCII 32-127, matching FreeType
- ~~`24` magic number for titleBarHeight~~ — resolved Sprint 23: extracted to `App::titleBarHeight` constant
