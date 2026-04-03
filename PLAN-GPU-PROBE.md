# PLAN: GPU Probe Auto-Detection

**Objective:** Replace the broken `gpu.acceleration = "auto"` (which blindly selects GPU) with a real OpenGL capability probe that detects software renderers and falls back to CPU.

**Flow:**
1. `Main::initialise()` calls `Gpu::probe()` before window creation
2. Stores result in `AppState::setGpuAvailable(bool)`
3. Config read calls `AppState::setRendererType(configSetting)` — resolves internally from `gpuAvailable` + config setting, writes `renderer` property
4. `applyConfig()` same as step 3
5. `getRendererType()` reads the stored `renderer` property — no computation

**Truth table (encapsulated in `setRendererType`):**

| config setting | gpuAvailable | renderer |
|---|---|---|
| `"auto"` | true | `"gpu"` |
| `"auto"` | false | `"cpu"` |
| `"true"` | true | `"gpu"` |
| `"true"` | false | `"cpu"` |
| `"false"` | true | `"cpu"` |
| `"false"` | false | `"cpu"` |

---

## Step 1: Create `Source/Gpu.h`

**New file.** Header-only declaration.

- Namespace `Gpu`
- `struct ProbeResult { juce::String rendererName; bool isAvailable; }`
- `ProbeResult probe() noexcept;` — stateless free function, no side effects
- Platform implementations are in separate files

**Validate:** File exists, compiles (header-only, no linkage yet).

---

## Step 2: Create `Source/Gpu_windows.cpp`

**New file.** Windows implementation of `Gpu::probe()`.

- Entire file guarded with `#if JUCE_WINDOWS ... #endif`
- Creates dummy HWND, WGL legacy context, bootstraps to 3.2 Core via `wglCreateContextAttribsARB`
- Queries `GL_RENDERER` string
- Checks against known software renderer list:
  - `llvmpipe`, `softpipe`, `Microsoft Basic Render`, `SwiftShader`
  - `VirtIO`, `QEMU`, `VMware SVGA`, `Parallels`, `virgl`
  - `GDI Generic` (Windows inbox software GL — primary UTM hit)
- If not known software: runs pixel probe (16x16 FBO, clear to seed color, readback, verify tolerance)
- Tears down all resources (context, DC, HWND) on every exit path
- Any failure at any step returns `{ {}, false }`

**Validate:** Compiles on Windows. Does not compile ObjC syntax. Returns correct result on UTM (expect `isAvailable = false`).

---

## Step 3: Create `Source/Gpu_mac.mm`

**New file.** macOS implementation of `Gpu::probe()`.

- Entire file guarded with `#if JUCE_MAC ... #endif` (matches `Notifications.mm` pattern)
- `NSOpenGLContext` with `NSOpenGLPFAAccelerated` attribute (fails fast if no hardware GL)
- Queries `GL_RENDERER`, checks against known software list (same list + `Apple Software Renderer`)
- Pixel probe same as Windows (FBO, clear, readback)
- Tears down context on every exit path
- Any failure returns `{ {}, false }`

**Validate:** Compiles on macOS. Empty compilation unit on Windows (guarded). Returns correct result.

---

## Step 4: `AppIdentifier.h` — add `gpuAvailable`

**Modify existing file.**

- Add `static const juce::Identifier gpuAvailable { "gpuAvailable" };` in `App::ID` namespace

**Validate:** Compiles. No conflicts with existing identifiers.

---

## Step 5: `AppState.h` / `AppState.cpp` — add gpuAvailable storage, modify setRendererType

**Modify existing files.**

### AppState.h:
- Add `void setGpuAvailable (bool available);`
- `setRendererType` signature stays: `void setRendererType (const juce::String& type);`
  - But semantics change: `type` is now the raw config string ("auto"/"true"/"false"), resolved internally
- `getRendererType()` unchanged — reads stored `renderer` property

### AppState.cpp:

**Add `setGpuAvailable`:**
```cpp
void AppState::setGpuAvailable (bool available)
{
    getWindow().setProperty (App::ID::gpuAvailable, available, nullptr);
}
```

**Modify `setRendererType`:**
```cpp
void AppState::setRendererType (const juce::String& type)
{
    auto window { getWindow() };
    const bool gpuAvailable { window.getProperty (App::ID::gpuAvailable, false) };
    const bool wantsGpu { type != "false" };
    const juce::String resolved { wantsGpu && gpuAvailable ? "gpu" : "cpu" };
    window.setProperty (App::ID::renderer, resolved, nullptr);
}
```

**Modify `initDefaults()`:**
- Remove the existing renderer resolution block (lines 342-344)
- `renderer` is written by `setRendererType()` after probe runs — `initDefaults()` no longer sets it
- `gpuAvailable` defaults to `false` (safe — CPU until probe says otherwise)

**Validate:** Compiles. `getRendererType()` returns `"cpu"` before probe runs (safe default). After probe + setRendererType, returns correct resolved value.

---

## Step 6: `Main.cpp` — probe at startup, fix glass and reload

**Modify existing file.**

### initialise():

**Add probe call** at top, before `mainWindow.reset(...)`:
```cpp
#include "Gpu.h"

// Top of initialise(), before window creation:
{
    const auto probeResult { Gpu::probe() };
    appState.setGpuAvailable (probeResult.isAvailable);
    DBG ("GPU probe: " + probeResult.rendererName + " -> " + juce::String (probeResult.isAvailable ? "available" : "unavailable"));
}
```

**Add setRendererType call** after probe, before window creation:
```cpp
appState.setRendererType (cfg->getString (Config::Key::gpuAcceleration));
```

**Fix glass block** (lines 139-145) — replace:
```cpp
// BEFORE:
const bool isGpu { cfg->getString (Config::Key::gpuAcceleration) != "false" };

// AFTER:
const bool isGpu { appState.getRendererType() == "gpu" };
```

**Fix onReload lambda** (lines 148-160):
- After `content->applyConfig()`, the renderer is already resolved in AppState
- Replace glass `isGpu` with `appState.getRendererType() == "gpu"`

### Validate:
- Probe runs before window creation
- gpuAvailable stored before setRendererType called
- Glass reads resolved value from AppState
- Reload re-reads config, re-resolves via applyConfig path

---

## Step 7: `MainComponent.cpp` — remove ternaries, remove listener, direct calls

**Modify existing file.**

### Constructor (line 52-95):

**Fix typeface initializer** (line 56-57) — replace:
```cpp
// BEFORE:
config.getString (Config::Key::gpuAcceleration) != "false" ? jreng::Glyph::AtlasSize::standard
                                                           : jreng::Glyph::AtlasSize::compact

// AFTER:
AppState::getContext()->getRendererType() == "gpu" ? jreng::Glyph::AtlasSize::standard
                                                   : jreng::Glyph::AtlasSize::compact
```

**Remove ternary** (lines 74-75):
```cpp
// DELETE:
const auto gpuSetting { config.getString (Config::Key::gpuAcceleration) };
appState.setRendererType (gpuSetting == "false" ? "cpu" : "gpu");
```
Renderer is already resolved in `Main::initialise()` before MainComponent is constructed.

**Remove listener registration** (lines 78-79):
```cpp
// DELETE:
windowState = appState.getWindow();
windowState.addListener (this);
```

**Replace listener trigger** (line 94):
```cpp
// BEFORE:
valueTreePropertyChanged (windowState, App::ID::renderer);

// AFTER:
applyRendererType();
```

### applyConfig() (line 97-113):

**Replace ternary** (lines 102-103):
```cpp
// BEFORE:
const auto gpuSetting { config.getString (Config::Key::gpuAcceleration) };
appState.setRendererType (gpuSetting == "false" ? "cpu" : "gpu");

// AFTER:
appState.setRendererType (config.getString (Config::Key::gpuAcceleration));
applyRendererType();
```

### Extract `applyRendererType()`:

**New private method.** Move GL lifecycle code from `valueTreePropertyChanged` (lines 119-147):

```cpp
void MainComponent::applyRendererType()
{
    const auto rendererType { getRendererType() };

    const auto atlasSize { rendererType == PaneComponent::RendererType::gpu
                               ? jreng::Glyph::AtlasSize::standard
                               : jreng::Glyph::AtlasSize::compact };
    typeface.setAtlasSize (atlasSize);

    glRenderer.detach();
    glRenderer.setComponentPaintingEnabled (true);

    if (rendererType == PaneComponent::RendererType::gpu)
    {
        glRenderer.attachTo (*this);
        setOpaque (false);
    }
    else
    {
        setOpaque (true);
    }

    if (tabs != nullptr)
    {
        tabs->switchRenderer (rendererType);
    }
}
```

### Remove `valueTreePropertyChanged`:

**Delete entire method** (lines 115-148). Remove `juce::ValueTree::Listener` inheritance from MainComponent if no other properties are listened to.

### Remove destructor listener cleanup:

**Delete** from destructor (line 185):
```cpp
// DELETE:
windowState.removeListener (this);
```

### MainComponent.h:

- Remove `juce::ValueTree::Listener` from inheritance (if only used for renderer)
- Remove `valueTreePropertyChanged` override declaration
- Remove `windowState` member variable
- Add `void applyRendererType();` private method declaration

**Validate:** Compiles. GL lifecycle triggers correctly from constructor and applyConfig(). No listener involved. Config reload path works: applyConfig() calls setRendererType(configString) which re-resolves, then applyRendererType() applies GL state.

---

## Step 8: Validate end-to-end

- **Windows UTM (ARM64):** Probe detects software GL, `gpuAvailable = false`. All config settings resolve to `"cpu"`. Terminal renders.
- **Windows native GPU:** Probe detects hardware GL, `gpuAvailable = true`. `"auto"` and `"true"` resolve to `"gpu"`. `"false"` still forces `"cpu"`.
- **macOS:** Same behavior as Windows native GPU (or CPU on VM).
- **Config reload:** Changing `gpu.acceleration` from `"auto"` to `"false"` and reloading switches to CPU correctly. Changing to `"true"` switches to GPU (if available).
- **Fresh install:** No state.xml, `initDefaults()` runs, `gpuAvailable` defaults to false, probe overrides in `initialise()`.
- **Existing state.xml:** Loaded, probe overrides `gpuAvailable` in `initialise()`, renderer re-resolved.

---

## Files Modified

| File | Action |
|---|---|
| `Source/Gpu.h` | **NEW** — header, namespace, ProbeResult, probe() declaration |
| `Source/Gpu_windows.cpp` | **NEW** — Windows WGL probe implementation |
| `Source/Gpu_mac.mm` | **NEW** — macOS NSOpenGLContext probe implementation |
| `Source/AppIdentifier.h` | **MODIFY** — add `gpuAvailable` identifier |
| `Source/AppState.h` | **MODIFY** — add `setGpuAvailable()`, declare `applyRendererType` friend or keep setRendererType |
| `Source/AppState.cpp` | **MODIFY** — implement setGpuAvailable, modify setRendererType to resolve internally, remove renderer from initDefaults |
| `Source/Main.cpp` | **MODIFY** — probe at startup, fix glass, fix reload |
| `Source/MainComponent.h` | **MODIFY** — remove listener, remove windowState, add applyRendererType |
| `Source/MainComponent.cpp` | **MODIFY** — remove ternaries, remove listener, extract applyRendererType, direct calls |
