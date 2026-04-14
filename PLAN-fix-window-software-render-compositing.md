# PLAN: Fix Window Software-Render Compositing — `jreng::Window` Self-Managed Dual Backend

**RFC:** none — objective from ARCHITECT prompt (session)
**Date:** 2026-04-14
**BLESSED Compliance:** verified (see BLESSED Alignment section)
**Language Constraints:** C++17 / JUCE (no LANGUAGE.md present in repo)

**Baseline anchor:** the two-edit probe (in HEAD working tree, uncommitted) already validates that plain `juce::Component` content (Action::List) renders alpha-correctly on `jreng::Window` glass via the existing `Terminal::ModalWindow::ContentView` + `setComponentPaintingEnabled(true)` mechanism. This plan RELOCATES that working mechanism into `jreng::Window` at module level. The GPU-mode logic moves; behavior does not change. The CPU-mode branch in `jreng::Window::visibilityChanged` (HEAD `cpp:146-151`) is preserved verbatim — only its trigger condition changes.

---

## Overview

Generalize `jreng::Window` to self-manage a binary render backend. GPU mode: Window owns a `std::unique_ptr<jreng::GLRenderer>`, attaches it to its content, applies DWM acrylic glass on Windows. CPU mode: no GL objects allocated, solid `windowColour` (alpha-aware) + DWM rounded corners only — no acrylic. Mode is told by renderer presence (the unique_ptr): non-null = GPU, null = CPU. SSOT is the renderer-presence; no parallel boolean flag.

Module dependency `jreng_gui → jreng_opengl` is rejected (ARCHITECT). Resolution: dissolve `modules/jreng_opengl/` into `modules/jreng_gui/opengl/` so the GL renderer becomes co-located with `jreng::Window`. After dissolution, Window can name `jreng::GLRenderer` directly.

Renderer construction stays inside `MainComponent` (where the typeface dependencies live) on the GPU branch of `MainComponent::setRenderer(App::RendererType)`. Constructed unique_ptr is immediately `std::move`'d up to the parent `jreng::Window` via `window->setRenderer(...)`. No intermediate `MainComponent::releaseRenderer` API. To make the dynamic_cast valid at first call, `applyConfig()` is moved out of `MainComponent`'s ctor body and into `Main.cpp` after Window construction (decision P).

Whelmed-font scaffolding is **removed in Step 1** as dead code. The `whelmedBodyFont`/`whelmedCodeFont` typeface references in `MainComponent`, `Tabs`, `Panes`, and the `GLAtlasRenderer` ctor are forward-looking placeholders for a Whelmed GL integration that hasn't shipped (`Whelmed::Component::switchRenderer` is a no-op, `Whelmed` is not a `jreng::GLComponent`, `managedTypefaces` whelmed atlases are never uploaded today). They are deleted to avoid confusion. `Whelmed::Config` font keys and `Whelmed::Screen`'s independent config reads are untouched — Whelmed continues to render exactly as HEAD does.

---

## Language / Framework Constraints

- **JUCE pre-attach invariants (F9 from Sprint 16 filter):** `setRenderer`, `setPixelFormat`, `setOpenGLVersionRequired`, `setComponentPaintingEnabled`, `setNativeSharedContext` all `jassert(nativeContext == nullptr)` on `juce::OpenGLContext`. `jreng::GLRenderer` ctor (`jreng_gl_renderer.cpp:4-16`) already issues the five required configuration calls. These five calls MUST stay inside the GLRenderer ctor — Sprint 16 F1 root cause was relocating them.
- **JUCE one-Context-per-Component (F10):** Exactly one `attachTo` per HWND. After this sprint, Window owns the attach; no caller code may call `attachTo` directly.
- **JUCE nesting forbidden (`juce_OpenGLContext.cpp:224-241`):** A GL-attached Component whose subtree contains another GL-attached Component asserts. Modal `DocumentWindow`s are sibling peers — legal. Do not embed one `jreng::Window` as a child Component of another's content subtree (no risk in current code paths).
- **Windows software-render + DWM acrylic = alpha-zero (F7):** CPU branch MUST NOT call `setGlass` / `DwmExtendFrameIntoClientArea` / `ACCENT_ENABLE_ACRYLICBLURBEHIND`. HEAD's `else` branch at `jreng_window.cpp:146-151` (`setOpaque(true); setBackgroundColour(windowColour);`) is the validated CPU path — preserved verbatim, only trigger swaps.
- **Build verification (F4):** Only ARCHITECT runs `build.bat`. Engineer reports diffs; no Engineer-claimed build status is accepted.
- **Sprint 16 F3:** If ARCHITECT invokes "from step 1," COUNSELOR executes full revert — no micro-fix on a broken foundation.

---

## Validation Gate

Each step MUST be validated before proceeding. Validation = `@Auditor` confirms step output complies with ALL of:
- `carol/MANIFESTO.md` (BLESSED principles)
- `carol/NAMES.md` (no improvised names; new identifiers gated by Rule -1)
- `carol/JRENG-CODING-STANDARD.md` (alternative tokens, brace init, positive control flow, no anonymous namespaces, etc.)
- The locked PLAN decisions: dissolve `jreng_opengl` into `jreng_gui/opengl/`; renderer-presence as TELL; Popup single 5-arg `show()`; whelmed font ownership unchanged; `applyConfig()` relocated to Main.cpp (decision P)
- JUCE invariants F1/F7/F9/F10 above
- F5 (no intermediate uncompilable state when Engineer batches across files)

After each step passes Auditor, ARCHITECT runs `build.bat` and confirms. Only then proceed to the next step.

---

## Steps

### Step 0 — Dissolve `jreng_opengl` module into `jreng_gui/opengl/`

**Scope:**
- Move every file under `modules/jreng_opengl/` into `modules/jreng_gui/opengl/` (preserving the `context/`, `renderers/`, `shaders/` subdirectory structure).
- Delete `modules/jreng_opengl/jreng_opengl.h`, `jreng_opengl.cpp`, `jreng_opengl.mm` (module declaration + unity aggregator).
- Update `modules/jreng_gui/jreng_gui.h` to add the new sources (extend `dependencies` if needed; ensure `juce_opengl` is a dependency now that GL sits inside this module).
- Update `modules/jreng_gui/jreng_gui.cpp` and `.mm` (unity aggregators) to include the absorbed `.cpp` and `.mm` files.
- Update `Builds/Ninja/END_App_artefacts/JuceLibraryCode/JuceHeader.h:26`: remove `#include <jreng_opengl/jreng_opengl.h>` (already covered by the now-extended `<jreng_gui/jreng_gui.h>` include).
- Update `END.jucer` (or whatever Projucer file declares modules) to remove `jreng_opengl` as a separate module entry; ensure `jreng_gui` declares the new files.
- Verify CMake/Ninja build config picks up the new file locations (`Builds/Ninja/CMakeLists.txt` or equivalent).

**Action (delegate to `@Engineer`):** Mechanical file move + build-config update. Behavior-preserving — no source-code changes inside the moved files. Pathfinder confirms zero project-code direct includes of `jreng_opengl/...` outside `JuceHeader.h`, so migration footprint is exactly: file relocation + Projucer + CMake + JuceHeader.

**Validation (`@Auditor`):**
- Every file previously in `modules/jreng_opengl/` now exists in `modules/jreng_gui/opengl/` with identical content (byte-for-byte).
- `modules/jreng_opengl/` directory is empty / removed.
- `JuceHeader.h` no longer references `jreng_opengl`.
- `END.jucer` no longer lists `jreng_opengl` as a separate module.
- `modules/jreng_gui/jreng_gui.h` declares the absorbed sources and includes `juce_opengl` as a dependency.
- Build config (CMake/Ninja) reflects new paths.
- BLESSED: no semantic change introduced; this is structural only. F6 satisfied — the module reorganization is the precondition for Shape B; not gratuitous.

**ARCHITECT build gate:** ARCHITECT runs `build.bat`. Must compile + link clean and produce a runnable binary identical in behavior to pre-S0.

---

### Step 1 — Remove Whelmed font scaffolding (pure dead-code deletion, no behavior change)

**Scope:**
- `Source/MainComponent.h` — delete `whelmedBodyFont` (line 158) and `whelmedCodeFont` (line 161) member declarations. Update `glRenderer` initializer (line 164) from `{ &typeface, &whelmedBodyFont, &whelmedCodeFont }` to `{ &typeface }`.
- `Source/MainComponent.cpp` — delete `whelmedBodyFont` ctor init block (lines 60–64) and `whelmedCodeFont` ctor init block (lines 65–69). Update `initialiseTabs()` (line 374) `make_unique<Terminal::Tabs>(typeface, whelmedBodyFont, whelmedCodeFont, orientation)` → `make_unique<Terminal::Tabs>(typeface, orientation)`. Update component iterator wiring (`MainComponent.cpp:382-392`) if it touches whelmed refs.
- `Source/component/Tabs.h` — delete the two `jreng::Typeface&` whelmed parameters from ctor signature; delete corresponding member fields.
- `Source/component/Tabs.cpp` — delete whelmed param storage in ctor init list; update internal `make_unique<Panes>(font, whelmedBodyFont, whelmedCodeFont)` calls (Section 2.3 cited `Tabs.cpp:95`) to `make_unique<Panes>(font)`.
- `Source/component/Panes.h` — delete whelmed `jreng::Typeface&` ctor params; delete member fields.
- `Source/component/Panes.cpp` — delete whelmed param storage in ctor init list; verify `createWhelmed()` (Panes.cpp:203) does not reference removed members.

(Note: `MainComponentActions.cpp:435` currently passes `glRenderer` by reference — no whelmed initializer at this site in HEAD. The renderer construction site that names the whelmed members is solely `MainComponent.h:164`. After Step 3 migration, new construction sites will appear; those pick up the already-cleaned `{ &typeface }` initializer naturally.)

**Action (delegate to `@Engineer`):** Pure deletion. No new code, no behavior change. Engineer must Pathfinder-sweep for any other consumer of `whelmedBodyFont` / `whelmedCodeFont` in `Source/` to catch references not enumerated above; if any consumer beyond this list exists, STOP and report (discrepancy = Decision Gate trigger).

**What is NOT touched:**
- `Whelmed::Config` keys (`fontFamily`, `fontSize`, `codeFamily`, `codeSize`) — `Whelmed::Screen` reads them directly (Pathfinder Section 3.3).
- `Whelmed::Component` — does not reference these MainComponent members today; no change required.
- `jreng::GLAtlasRenderer` — ctor signature (`std::initializer_list<jreng::Typeface*>`) unchanged. It just receives a one-element list now.

**Validation (`@Auditor`):**
- All deleted references removed; no dangling reference, no orphaned ctor param.
- Pathfinder sweep: zero matches for `whelmedBodyFont` and `whelmedCodeFont` in `Source/` and `modules/`.
- Behavior unchanged: terminal renders identically; Whelmed pane (if invoked via `openMarkdown`) renders identically (Whelmed reads its own config keys via `Whelmed::Screen.cpp:17`).
- BLESSED: `L` (lean) — net deletion, no new code. `S` (SSOT) — eliminates dual-truth between `MainComponent::whelmedBodyFont` Typeface and `Whelmed::Config::Key::fontFamily` (the config key is the surviving SSOT). `E` (Encapsulation) — Whelmed pane now owns its full font lifecycle via its own config reads; no upstream coupling through MainComponent's typefaces.
- NAMES / JRENG-CODING-STANDARD: not applicable (deletion only, no new identifiers).
- F5: builds clean atomically — every file consistent.

**ARCHITECT build gate:** ARCHITECT runs `build.bat`. Smoke test: open Whelmed via `openMarkdown` (drag-drop or action) — it must render exactly as before. Terminal must render exactly as before. Then proceed to S2 (API extension).

---

### Step 2 — Extend `jreng::Window` API (additive, dormant)

**Scope:**
- `modules/jreng_gui/window/jreng_window.h`
- `modules/jreng_gui/window/jreng_window.cpp`
- `modules/jreng_gui/opengl/context/jreng_gl_renderer.h` (sub-change 1b)
- `modules/jreng_gui/opengl/context/jreng_gl_renderer.cpp` (sub-change 1b)

**Action (delegate to `@Engineer`):**

Add the new API surface and internal members to `jreng::Window` WITHOUT altering existing behavior. New members are dormant; `setGpuRenderer(bool)` + `gpuRenderer` member remain functional and continue to drive `visibilityChanged`.

**`jreng::Window` additions:**
- Private member: `std::unique_ptr<jreng::GLRenderer> glRenderer;` — null = CPU mode (after Step 3 swap).
- Private member: `void* nativeSharedContext { nullptr };`
- Public method: `void setRenderer (std::unique_ptr<jreng::GLRenderer> renderer);` — stores. If Window already has a peer (visible) and the new renderer is non-null, performs attach sequence immediately. If non-null becomes null, detaches and releases prior renderer. Before first `setVisible`, stores only — first-show in `visibilityChanged` performs the attach (after Step 3 swap).
- Public method: `void setNativeSharedContext (void* sharedContext) noexcept;` — stores raw HGLRC. Asserts `glRenderer == nullptr or not glRenderer->isAttached()` (cannot change shared context on an attached renderer per JUCE F9).
- Public method: `void* getNativeSharedContext() const noexcept;` — returns the internal renderer's native context handle if renderer present and attached, else `nullptr`. Modal Windows query this on the root Window to set their own shared context.
- Public method: `void triggerRepaint();` — delegates to `glRenderer->triggerRepaint()` if present, else no-op.
- Private helper: `void attachRendererToContent();` — orders strictly per F9: `glRenderer->setNativeSharedContext(nativeSharedContext)` → `glRenderer->setComponentPaintingEnabled(true)` → `glRenderer->attachTo(*getContentComponent())`. Called from `visibilityChanged` (after Step 3 swap) and from `setRenderer` when called post-`setVisible`.

**`jreng::GLRenderer` sub-change (Step 2b):**
- Add `void setNativeSharedContext (void* handle) noexcept;` to header + cpp. Thin passthrough: `openGLContext.setNativeSharedContext(handle);`. Asserts `not openGLContext.isAttached()` per F9. Existing `setSharedRenderer(GLRenderer&)` stays — it remains the project-internal API. The new void* form lets `jreng::Window` forward its stored handle without holding a reference to the root renderer object.
- Add `bool isAttached() const noexcept;` if not already present (used by Window's assertion above). Check whether HEAD `jreng::GLRenderer` exposes this; if `openGLContext.isAttached()` is wrapped, use it; else add the wrapper.

**Do NOT yet:**
- Change `visibilityChanged`'s if/else condition (still gates on `gpuRenderer`).
- Delete `setGpuRenderer(bool)` or `gpuRenderer` member.
- Touch any caller (Main.cpp, MainComponent, Popup, Terminal::ModalWindow, MainComponentActions).

**Validation (`@Auditor`):**
- Compiles clean with zero callers of the new API.
- BLESSED: `B` — `std::unique_ptr` ownership unambiguous; Window dtor releases renderer (which detaches its own context). `E` (Explicit) — every new method documented with `@param`, `@note`, ordering preconditions; positive-check control flow; no early returns; all `noexcept` where applicable. `E` (Encapsulation) — new members private; public verbs only.
- NAMES: `setRenderer`, `setNativeSharedContext`, `getNativeSharedContext`, `triggerRepaint`, `attachRendererToContent`, `glRenderer`, `nativeSharedContext`, `isAttached` — all verbs-for-functions / nouns-for-members. No type-encoding suffixes. New names gated under Rule -1 — Auditor confirms each is approved by ARCHITECT (this PLAN approval doubles as approval for these names).
- JRENG-CODING-STANDARD: alternative tokens (`not`, `and`, `or`); brace init; `noexcept`; const correctness; no anonymous namespaces; no `namespace detail`; no early returns.
- JUCE invariants: pre-attach asserts present on `setNativeSharedContext` (Window + GLRenderer). `attachRendererToContent` orders share-context → componentPaintingEnabled → attachTo (F9).
- F5: builds clean with zero downstream changes (dormant API).

---

### Step 3 — Atomic migration

**Scope (single Engineer delegation, must compile at end — F5):**
- `modules/jreng_gui/window/jreng_window.h/.cpp` — swap `visibilityChanged` trigger, delete `setGpuRenderer` + `gpuRenderer`.
- `Source/Main.cpp` — drop `setGpuRenderer` call (line 353); insert `mainComponent->applyConfig()` after `setGlass` (P).
- `Source/MainComponent.h/.cpp` — delete `glRenderer` member (h:164); rewrite `setRenderer` to construct + move-up; delete `glRenderer.detach()` from dtor (cpp:182); remove `applyConfig()` call from ctor body (cpp:94).
- `Source/component/Popup.h/.cpp` — collapse two `show()` overloads into one; signature accepts `std::unique_ptr<jreng::GLRenderer>` (nullptr = CPU); Popup extracts `getNativeSharedContext()` from caller's top-level `jreng::Window` and forwards to `Terminal::ModalWindow`.
- `Source/component/ModalWindow.h/.cpp` — collapse two ctors into one accepting renderer uptr + shared-context handle; delete `ContentView` struct entirely; route through base `jreng::Window::setRenderer` + `setNativeSharedContext`.
- `Source/MainComponentActions.cpp` — Action::List call (line 154) and Terminal popup call (line 435) construct the appropriate renderer subclass per `AppState::getRendererType()` and pass to new Popup signature.
- `Source/component/TerminalDisplay.cpp` — `Terminal::Display::onRepaintNeeded` wiring relocates from former ContentView to new ModalWindow ctor; lambda captures the parent `jreng::Window*` and calls `window->triggerRepaint()` (was `glRenderer.triggerRepaint()` on a local renderer).

**Action (delegate to `@Engineer`, single atomic batch):**

1. **`jreng::Window::visibilityChanged`** (`jreng_window.cpp:125-154`):
   - Replace condition `if (gpuRenderer)` with `if (glRenderer != nullptr)`.
   - Inside the GPU branch: BEFORE `setGlass(tintColour, blurRadius)`, call `attachRendererToContent()`.
   - CPU branch body (`setOpaque(true); setBackgroundColour(windowColour);`): unchanged.
   - DWM rounded-corners block at top of `#elif JUCE_WINDOWS` block: unchanged.

2. **Delete `jreng::Window::setGpuRenderer(bool)`** from header (h:106) + implementation (cpp:86-89).

3. **Delete `jreng::Window::gpuRenderer` member** (h:143).

4. **`Source/Main.cpp:343-359`** reshape:
   - Capture the MainComponent pointer before Window construction so it can be addressed afterward:
     ```cpp
     auto* mainComponent { new MainComponent (fontRegistry) };
     mainWindow.reset (new jreng::Window (mainComponent, /* name */, /* alwaysOnTop */, /* showWindowButtons */));
     mainWindow->setGlass (cfg->getColour (Config::Key::windowColour)
                                .withAlpha (cfg->getFloat (Config::Key::windowOpacity)),
                            cfg->getFloat (Config::Key::windowBlurRadius));
     mainComponent->applyConfig();   // P: was MainComponent::ctor body — now fires here, after Window exists, so dynamic_cast<jreng::Window*>(getTopLevelComponent()) succeeds.
     mainWindow->setVisible (true);
     ```
   - Delete line 353 (`mainWindow->setGpuRenderer (...)`).

5. **`Source/MainComponent.h:164`** — delete `jreng::GLAtlasRenderer glRenderer { ... };` member.

6. **`Source/MainComponent.cpp`:**
   - Delete `applyConfig();` call from ctor body (line 94). It now fires from Main.cpp.
   - `setRenderer (App::RendererType rendererType)` rewrite (cpp:114-134):
     ```cpp
     void MainComponent::setRenderer (App::RendererType rendererType)
     {
         const bool isUsingGpu { rendererType == App::RendererType::gpu };
         const auto atlasSize { isUsingGpu ? jreng::Glyph::AtlasSize::standard : jreng::Glyph::AtlasSize::compact };
         typeface.setAtlasSize (atlasSize);

         if (auto* window { dynamic_cast<jreng::Window*> (getTopLevelComponent()) })
         {
             if (isUsingGpu)
             {
                 window->setRenderer (std::make_unique<jreng::GLAtlasRenderer> (
                     std::initializer_list<jreng::Typeface*> { &typeface }));
             }
             else
             {
                 window->setRenderer (nullptr);
             }
         }

         setOpaque (not isUsingGpu);

         if (tabs != nullptr)
             tabs->switchRenderer (rendererType);
     }
     ```
   - Delete `glRenderer.detach();` from dtor (line 182).

7. **`Source/component/Popup.h/.cpp`:**
   - Delete the 4-arg `show()` overload (h:110-113) and its implementation.
   - Reshape remaining `show()`:
     ```cpp
     void show (juce::Component& caller,
                std::unique_ptr<juce::Component> content,
                int width,
                int height,
                std::unique_ptr<jreng::GLRenderer> renderer);   // nullptr = CPU mode
     ```
   - Implementation: extract shared context handle = `dynamic_cast<jreng::Window*>(caller.getTopLevelComponent())->getNativeSharedContext()`. Construct the single `Terminal::ModalWindow` with content + renderer + shared-context handle.

8. **`Source/component/ModalWindow.h/.cpp`:**
   - Delete `struct ContentView` definition + implementation (entire block).
   - Delete the non-GL ctor (cpp:122-145).
   - Collapse to single ctor:
     ```cpp
     ModalWindow (std::unique_ptr<juce::Component> content,
                  juce::Component& centreAround,
                  std::unique_ptr<jreng::GLRenderer> renderer,
                  void* nativeSharedContext,
                  std::function<void()> dismissCallback);
     ```
   - Body: pass `content.release()` directly to base `jreng::ModalWindow` ctor (no ContentView wrapping). Then on the Window base: `setNativeSharedContext(nativeSharedContext); setRenderer(std::move(renderer));`. Then `setupWindow(centreAround); setVisible(true); enterModalState(true);` and the focus-grab async callback.
   - Delete `setGpuRenderer(false)` call (cpp:142), `setOpaque(true)` (cpp:142), `setBackgroundColour(...)` (cpp:142) — Window's CPU branch handles all of these via its own `visibilityChanged`.

9. **`Source/MainComponentActions.cpp:154`** (Action::List):
   ```cpp
   auto renderer { (appState.getRendererType() == App::RendererType::gpu)
       ? std::unique_ptr<jreng::GLRenderer> { std::make_unique<jreng::GLRenderer>() }
       : nullptr };
   popup.show (*this, std::move (list), width, height, std::move (renderer));
   ```

10. **`Source/MainComponentActions.cpp:435`** (Terminal popup):
    ```cpp
    auto renderer { (appState.getRendererType() == App::RendererType::gpu)
        ? std::unique_ptr<jreng::GLRenderer> { std::make_unique<jreng::GLAtlasRenderer> (
              std::initializer_list<jreng::Typeface*> { &typeface }) }
        : nullptr };
    popup.show (*getTopLevelComponent(), std::move (terminal), pixelWidth, pixelHeight, std::move (renderer));
    ```

11. **`Source/component/TerminalDisplay.cpp`** — audit `Terminal::Display::onRepaintNeeded` wiring. The lambda formerly inside `ContentView::ContentView` (`ModalWindow.cpp:31-35`) relocates into the new `Terminal::ModalWindow` ctor. New lambda captures the Window (`this` is `Terminal::ModalWindow`, which is a `jreng::Window`) and calls `triggerRepaint()` directly:
    ```cpp
    if (auto* terminal { dynamic_cast<Terminal::Display*> (getContentComponent()) })
    {
        terminal->onRepaintNeeded = [this, terminal]
        {
            terminal->repaint();
            triggerRepaint();
        };
    }
    ```

**Validation (`@Auditor`):**
- Compiles clean — entire batch consistent (F5).
- BLESSED:
  - `B` — single ownership chain per Window (Window owns renderer; MainComponent no longer holds GL). RAII destructor sequence verified: derived Window destructs → renderer member destructs → renderer dtor calls `openGLContext.detach()` → only THEN base `juce::DocumentWindow` destructs its owned content (MainComponent / Display / Action::List), so font members in MainComponent outlive the renderer's references to them.
  - `S` (SSOT) — render mode: ONE source of truth = renderer presence on Window. No `gpuRenderer` flag anywhere.
  - `S` (Stateless) / Tell-don't-ask — orchestrator TELLs `setRenderer`; Window self-manages attach/detach lifecycle. No getter on Window asks "are you GPU mode?".
  - `E` (Explicit) — caller's intent carried in the `unique_ptr` (present/null); no boolean+object tuple. Pre-attach ordering documented and asserted.
  - `E` (Encapsulation) — all GL wiring (`attachTo`, `setComponentPaintingEnabled`, `setSharedRenderer`, `setNativeSharedContext` on the renderer) lives inside `jreng_gui/window/`. Callers see only `Window::setRenderer`, `setNativeSharedContext`, `getNativeSharedContext`, `triggerRepaint`. F6 satisfied.
  - `L` — net deletion: `ContentView`, non-GL `Popup::show`, non-GL `ModalWindow` ctor, `setGpuRenderer(bool)`, `gpuRenderer` member, `MainComponent::glRenderer` member, `glRenderer.detach()` calls in MainComponent ctor/dtor. CPU mode allocates zero GL objects (no faking).
- NAMES: zero new improvised names beyond those approved in Step 2. Existing names preserved.
- JRENG-CODING-STANDARD: alternative tokens; brace init; noexcept; positive control flow; no early returns; const correctness; no anonymous namespaces.
- JUCE invariants:
  - F1/F9: `jreng::GLRenderer` ctor's five config calls (`setPixelFormat`, `setOpenGLVersionRequired`, `setRenderer(this)`, `setComponentPaintingEnabled(false)`, `setContinuousRepainting(false)`) UNTOUCHED. Renderer construction in `MainComponent::setRenderer` and `MainComponentActions.cpp` constructs fresh GLRenderer/GLAtlasRenderer instances — those calls fire each time.
  - F7: CPU branch of `visibilityChanged` body unchanged. DWM acrylic never fires in CPU mode.
  - F10: Exactly one `attachTo` per Window peer. `attachRendererToContent` is the sole attach site. No caller calls `attachTo` directly.
- Probe Edit 2 (`ModalWindow.cpp:52` outer-guard restructure) is GONE because `ContentView` is deleted entirely.
- Pathfinder-confirmed sweep: zero matches for `setGpuRenderer`, `gpuRenderer`, `Terminal::ModalWindow::ContentView`, `Popup::show` 4-arg signature in `Source/` and `modules/`.
- No new dynamic_cast added beyond the existing `dynamic_cast<jreng::Window*>(getTopLevelComponent())` pattern (already in HEAD at `MainComponent.cpp:149`).

**ARCHITECT build gate:** ARCHITECT runs `build.bat` after Auditor passes. Live smoke tests:
- App starts in GPU mode → main terminal renders correctly on glass.
- Open Action::List (Cmd/Ctrl+P) → renders correctly on glass with alpha (probe-validated baseline preserved).
- Open Terminal popup (split or new) → renders correctly on glass.
- Toggle config `gpuAcceleration = "cpu"` → reload → main window switches to opaque solid colour with rounded corners; Action::List + popup paths still functional (CPU mode, no GL).
- Toggle back to `"gpu"` → reload → all GL paths restored.

If any smoke test fails AND ARCHITECT invokes "from step 1" — full revert (F3), no micro-fix.

---

### Step 4 — Documentation + dead-reference sweep

**Scope:**
- `modules/jreng_gui/window/jreng_window.h` — class doxygen update.
- `modules/jreng_gui/opengl/context/jreng_gl_component.h` — class doxygen update (Q3 α-formalization).
- `carol/SPRINT-LOG.md` — sprint receipt (written only on ARCHITECT's `log sprint` command).

**Action (delegate to `@Engineer` for doxygen, `@Pathfinder` for sweep):**

1. **`jreng::Window` class doxygen:** add a `### Render backend` section documenting the dual-backend contract:
   > Window self-manages render backend by renderer ownership. Pass a `std::unique_ptr<jreng::GLRenderer>` via `setRenderer` to enable GPU mode (DWM acrylic on Windows, CoreGraphics blur on macOS). Pass `nullptr` (or never call) for CPU mode (solid `windowColour`, DWM rounded corners on Windows). Any `juce::Component` or `jreng::GLComponent` works as content in either mode. The `paint(juce::Graphics&)` software path is the baseline contract honored by every subclass; `paintGL()` is an optional GPU-accelerated optimization that fires only in GPU mode.
2. **`jreng::GLComponent` class doxygen** (Q3 α): add a paragraph:
   > `jreng::GLComponent` is a `juce::Component` with optional GL-accelerated render hooks. The inherited `paint(juce::Graphics&)` is the baseline software contract every subclass honors. `paintGL()` and `paintGL(GLGraphics&)` are optional optimizations that fire only when a `jreng::GLRenderer` is active on the owning `jreng::Window`. In CPU mode, only `paint()` runs; in GPU mode, both `paint()` (rasterized into the GL framebuffer via `setComponentPaintingEnabled(true)`) and `paintGL()` (composited into the same framebuffer) run together.
3. **`@Pathfinder` sweep** — confirm zero remaining matches in `Source/` and `modules/` for:
   - `setGpuRenderer`
   - `gpuRenderer` (as a Window member or flag, not local variables)
   - `Terminal::ModalWindow::ContentView`
   - `Popup::show` 4-arg signature
   - `modules/jreng_opengl` (any path or include reference)
4. **Sprint receipt** — when ARCHITECT says `log sprint`, write the `## Sprint N: ...` block to `carol/SPRINT-LOG.md` per CAROL format. Drain DEBT.md if any IDs were paid in scope.

**Validation (`@Auditor`):**
- Doxygen renders clean (no broken `@see`, balanced blocks).
- Pathfinder sweep returns zero hits on every search target.
- Sprint receipt format matches CAROL spec when written.

---

## BLESSED Alignment

- **B (Bound):** `jreng::Window` owns its `std::unique_ptr<jreng::GLRenderer>`. One owner, unambiguous. RAII detach + release in dtor. No raw owning pointers introduced. No `SafePointer`, no lifetime guards. Lifecycle order verified: Window dtor → renderer dtor (detaches) → DocumentWindow dtor (releases content). Fonts in MainComponent outlive renderer's raw-pointer references to them.
- **L (Lean):** Net deletion. `Terminal::ModalWindow` shrinks dramatically (ContentView struct + its initialiseGL gone, two ctors collapse to one). `Popup` simpler (one show overload). `jreng::Window` adds API surface but eliminates `setGpuRenderer`/`gpuRenderer` parallel mechanism. `MainComponent` loses `glRenderer` member + dtor cleanup. CPU mode allocates zero GL objects (no faking). YAGNI: no template gymnastics, no sentinel types, no factory functions.
- **E (Explicit):** Caller's intent carried in the type system (`std::unique_ptr<jreng::GLRenderer>` present or absent). All new methods `noexcept` where applicable. Pre-attach ordering documented + asserted. No magic flags.
- **S (SSOT):** Single source of truth for render mode = renderer presence on Window. `AppState::getRendererType()` remains the app-wide renderer-type SSOT (read at construction sites — `MainComponent::setRenderer`, `MainComponentActions:154`, `:435`); Window does not shadow it.
- **S (Stateless) / Tell-don't-ask:** Orchestrator TELLs `setRenderer`. Window manages itself. No `Window::isGpuMode()` getter exists.
- **E (Encapsulation):** GL wiring (`attachTo`, `setComponentPaintingEnabled`, `setSharedRenderer`, `setNativeSharedContext` on renderer) moves INSIDE `jreng_gui/window/`. Callers see only Window's verbs. The module-dependency concern (Sprint 16 F6) is resolved by S0 dissolving `jreng_opengl` — the dep no longer exists because the modules merge.
- **D (Deterministic):** Same content + same renderer-presence state always produces same render path.

---

## Risks / Open Questions

All open questions resolved per ARCHITECT decisions earlier in session. Residual risks:

1. **S0 build-config update.** Projucer (`END.jucer`) module declarations and CMake/Ninja generators must be updated atomically with the file moves. If any reference to `jreng_opengl` is missed, the build fails. Engineer must Pathfinder-sweep for `jreng_opengl` references across `Builds/`, `END.jucer`, and any CMakeLists.

2. **MainComponent `applyConfig` relocation (decision P).** Removing `applyConfig()` from MainComponent ctor body changes the construction-time semantics. Any code that assumed MainComponent is fully configured immediately after its ctor returns is now wrong. Pathfinder confirms `applyConfig` is only invoked from MainComponent ctor (cpp:94) and from `config.onReload` callback (Main.cpp:391) — neither relies on completion-by-end-of-ctor. Safe to relocate.

3. **Runtime GPU↔CPU swap during live session.** Config reload via `Cmd/Ctrl+,` triggers `applyConfig` → `setRenderer(newType)` → constructs/releases renderer → `window->setRenderer(...)`. This must work seamlessly mid-session without losing tabs, sessions, or focus. Smoke test in S3 ARCHITECT build gate covers this.

4. **Future Whelmed GL integration.** With whelmed font scaffolding deleted in Step 1, when ARCHITECT later integrates Whelmed::Component as a `jreng::GLComponent` (per SPEC end-game), the natural extension point is `MainComponent::setRenderer`'s GLAtlasRenderer initializer list and Whelmed::Component itself owning its typefaces (lazy creation per ARCHITECT's earlier hint).

5. **Sprint 16 foundation-failure protocol (F3).** If ARCHITECT invokes "from step 1" at any point during S3 (or any prior step), COUNSELOR executes full revert — not micro-fix.

---

**End of PLAN.**
