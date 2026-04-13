# PLAN: Window Glass Modes

**RFC:** none -- objective from ARCHITECT prompt
**Date:** 2026-04-13
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

jreng::Window gains three rendering modes for glass/blur: GPU (GL compositing), Software (child HWND on Windows, native blur on macOS), and Solid (opaque background + DWM rounded corners). Mode is derived internally from AppState renderer type + platform capability. `setGlass` API drops the opacity parameter -- colour AARRGGBB carries tint alpha. `Gpu::resolveOpacity` is removed. The `glass/` subdirectory is renamed to `window/`.

## Validation Gate
Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Rename `glass/` to `window/`
**Scope:** `modules/jreng_gui/glass/` -> `modules/jreng_gui/window/`, `jreng_gui.h`, `jreng_gui.cpp`, `jreng_gui.mm`
**Action:** Move all 12 files from `glass/` to `window/`. Update all `#include "glass/..."` to `#include "window/..."` in the three module unity files.
**Validation:** All includes resolve. No references to `glass/` remain in the module. Build succeeds.

### Step 2: Change `setGlass` signature
**Scope:** `jreng_window.h`, `jreng_window.cpp`, all callers
**Action:** Change `setGlass(Colour colour, float opacity, float blur)` to `setGlass(Colour colour, float blur)`. Tint alpha comes from `colour.getFloatAlpha()`. Remove `opacity` parameter. Remove `Gpu::resolveOpacity` from `Gpu.h` (dead code after this change). Update all callers:
- `Main.cpp` (~2 callsites) -- pass colour with alpha directly, remove `Gpu::resolveOpacity` wrapper
- `ModalWindow.cpp` (setupWindow) -- same
- Internal re-entrant calls in `jreng_window.cpp` -- adjust
**Validation:** `setGlass` has 2 params everywhere. No `resolveOpacity` calls remain. Build succeeds.

### Step 3: jreng::Window reads AppState for mode
**Scope:** `jreng_window.h`, `jreng_window.cpp`
**Action:** In `visibilityChanged()`, read `AppState::getContext()->getRendererType()`. Derive behaviour:
- `gpu` -> current single-HWND path: `BackgroundBlur::enable` + DWM corners (unchanged)
- `cpu` + Windows -> Mode 2: software glass via child HWND (Step 4)
- `cpu` + macOS -> current path (macOS native blur handles software alpha correctly)
- `cpu` + no DWM -> Mode 3: solid background colour + DWM rounded corners only

No new enum exposed. Window self-manages. AppState is the SSOT. One `#include` added for AppState access (or via existing jreng_gui include chain).
**Validation:** `visibilityChanged` reads AppState once. No mode enum leaked to callers. BLESSED E (Explicit) -- no magic opacity threshold. BLESSED S (SSOT) -- AppState is single source. BLESSED E (Encapsulation) -- Window self-manages mode.

### Step 4: Implement child HWND mode (Windows, CPU renderer)
**Scope:** `jreng_window.h`, `jreng_window.cpp`
**Action:** When `cpu` + Windows in `visibilityChanged()`:
1. Apply DWM glass (BackgroundBlur::enable) on the main HWND -- glass shell, no JUCE content paints here
2. Apply DWM rounded corners on the main HWND
3. Get content component via `getContentComponent()`
4. Call `content->addToDesktop(ComponentPeer::windowIsTemporary, thisHWND)` to give content its own child peer
5. Content renders via standard GDI in child HWND -- alpha is correct because child is not glass
6. Content background is transparent -- parent glass shows through unpainted areas
7. Sync child position/size in `resized()` override

jreng::Window stores a bool `isUsingChildPeer` to track the mode (needed for cleanup in destructor and resize sync). Use existing `jreng::ChildWindow::attach` for native-level parent-child relationship.

**Validation:** Software-painted content visible through glass on Windows CPU path. DWM rounded corners present. No GL dependency. Child HWND lifecycle is RAII-bound (B). Window self-manages (E-Encapsulation).

### Step 5: Solid fallback (no DWM)
**Scope:** `jreng_window.cpp`
**Action:** When `cpu` + no DWM capability (`BackgroundBlur::isDwmAvailable()` returns false):
- Set opaque background with `windowColour`
- Apply DWM rounded corners if available (separate from blur)
- No BackgroundBlur::enable call
This path already partially exists in the `opacity >= 1.0` branch of current `setGlass`. Formalize it.
**Validation:** Solid window with rounded corners renders correctly. No glass, no crash.

### Step 6: Clean up Action::List and Terminal::ModalWindow
**Scope:** `Source/action/ActionList.h`, `Source/action/ActionList.cpp`, `Source/component/ModalWindow.h`, `Source/component/ModalWindow.cpp`, `Source/component/Popup.h`, `Source/component/Popup.cpp`, `Source/MainComponentActions.cpp`
**Action:**
- Action::List is a plain `juce::Component` + `juce::ValueTree::Listener` (current state, keep)
- Remove debug logging from ModalWindow.cpp
- Remove debug DBG lines from MainComponentActions.cpp
- Terminal::ModalWindow non-GL constructor works correctly now (jreng::Window handles child HWND internally)
- Verify Popup GL path still works (no regression)
- Verify Action::List through Popup non-GL show() works with the new child HWND mode
**Validation:** Action list renders with glass on GPU path, renders with software glass (child HWND) on CPU path, renders with solid bg on fallback. Popup terminal still works. No debug artifacts remain.

### Step 7: Remove dead code
**Scope:** `Gpu.h`, `Source/action/LookAndFeel.h/.cpp` (if still present), any orphaned ContentView references
**Action:** Remove `Gpu::resolveOpacity` (dead after Step 2). Remove any remaining dead `Action::LookAndFeel` references. Clean unused includes.
**Validation:** No dead code. No unused includes. Build clean.

## BLESSED Alignment
- **B (Bound):** Child HWND lifecycle owned by jreng::Window. RAII cleanup in destructor. No floating resources.
- **L (Lean):** No new classes. jreng::Window gains ~30 lines for child HWND mode. No god objects.
- **E (Explicit):** No magic opacity threshold. Mode derived from named AppState value + platform. `setGlass(colour, blur)` -- two semantic params.
- **S (SSOT):** AppState owns renderer type. GpuProbe writes it once. Everything reads from AppState. No shadow state.
- **S (Stateless):** Window reads AppState, derives mode, acts. No persistent mode tracking beyond the child peer bool.
- **E (Encapsulation):** Window self-manages all three modes. Callers don't know about child HWNDs. Tell, don't ask.
- **D (Deterministic):** Same AppState value + same platform = same window mode. Always.

## Risks / Open Questions
- **Child HWND resize sync:** `DeferWindowPos` needed to avoid tearing on resize? Or is single `WM_SIZE` handler sufficient since child is `WS_CHILD` (auto-clipped)?
- **Child HWND transparency:** Can a `WS_CHILD` have transparent background so parent glass shows through? @Researcher confirmed yes on Win8+ with `WS_EX_LAYERED` on child. JUCE's `setOpaque(false)` on the content component should trigger this. Needs verification.
- **macOS CPU path:** Assumed to work as-is (native blur + software rendering = correct alpha). Needs verification on macOS hardware.
