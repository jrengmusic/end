# PLAN: Custom GL Shader Pipeline

**RFC:** RFC-shaders.md
**Date:** 2026-04-26
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (LANGUAGE.md)

## Overview

Enable END to load user-authored GLSL shaders as background animations or overlay post-processing effects. Shadertoy convention is the 1:1 contract — all uniforms, entry point, coordinate space, semantics match exactly. Any Shadertoy shader is drop-in.

## Language / Framework Constraints

C++ / JUCE is the reference implementation. All BLESSED principles enforced as written. JUCE-specific: `juce::OpenGLShaderProgram` for compilation, `juce::Image` for texture loading, `juce::ValueTree` for AppState shader error property, `juce::Timer` for FBO resize debounce.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Locked Decisions

1. **Shadertoy is the spec.** All uniforms, entry point, coordinate space match 1:1. Drop-in.
2. **Two layers:** background (multi-pass, up to 4 buffers) + overlay (single-pass post-process).
3. **iChannel0 = previous pass output.** Pass 0: own previous frame (ping-pong). Pass N>0: pass N-1 output. User textures on iChannel1-3. Fixed rule, no per-pass user configuration.
4. **Pure Shadertoy preamble.** No END-specific uniforms (iFocus/iCursor dropped).
5. **Buffer limit: 4** (A/B/C/D). Matches Shadertoy.
6. **Opacity** — config float [0.0-1.0] for terminal content over background shader.
7. **Enrich jam::gl** with `jam::gl::Buffer` (FBO) and `jam::gl::Texture` (file-loaded).
8. **No new orchestrator** — integrates into existing `renderOpenGL()` flow.
9. **FBO resize** — debounced via `juce::Timer`. Buffer told dimensions, doesn't track them.
10. **Shader errors** — AppState `juce::var` property (void = valid, string = error). MessageOverlay listens and fires automatically. Event-driven.
11. **Hot reload** — poll mtime on GL thread. Swap on success. Previous valid program continues on failure.
12. **Animation gating** — `setContinuousRepainting` toggled dynamically. Config: `"focused"` | `"always"` | `"off"`.
13. **ShaderCompiler error surfacing** — in scope. Retrieve `getLastError()` before destroying failed program.

## Complete Shadertoy Preamble

```glsl
#version 330 core
uniform vec3      iResolution;
uniform float     iTime;
uniform float     iTimeDelta;
uniform float     iFrameRate;
uniform int       iFrame;
uniform float     iChannelTime[4];
uniform vec3      iChannelResolution[4];
uniform vec4      iMouse;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
uniform vec4      iDate;
uniform float     iSampleRate;
out vec4 _fragColor;

// --- user shader pasted here ---
void mainImage(out vec4 fragColor, in vec2 fragCoord) { ... }

// --- epilogue (injected by END) ---
void main() {
    mainImage(_fragColor, gl_FragCoord.xy);
}
```

## Compositing Order Per Frame

**Background + overlay active:**
1. Bind terminal Buffer -> existing instanced draws (bg, mono, emoji, cursor)
2. Background passes: each binds previous output as iChannel0 + user textures as iChannel1-3. Pass 0 ping-pongs for self-feedback.
3. Composite terminal Buffer over background at configured opacity (alpha blend, internal shader).
4. Capture composited result to Buffer. Overlay reads as iChannel0, renders to screen.

**Background only:** steps 1-3, composited result goes to screen.

**Overlay only:** terminal Buffer -> overlay reads as iChannel0 -> screen.

**No shader:** existing flow unchanged. No FBO allocation. Zero overhead.

## Config (`~/.config/end/end.lua`)

```lua
END = {
    shader = {
        background = {
            passes   = { "shaders/buf_a.frag", "shaders/image.frag" },
            textures = { "shaders/noise256.png", "shaders/bluenoise.png" },
        },
        overlay   = "shaders/crt.frag",
        animation = "focused",
        opacity   = 0.85,
    }
}
```

Simple case:
```lua
END = {
    shader = {
        background = "shaders/nebula.frag",
        opacity    = 0.9,
    }
}
```

## Steps

### Step 1: ShaderCompiler Error Return

**Scope:** `jam/jam_gui/opengl/context/jam_gl_shader_compiler.h`, `.cpp`

**Action:**
- Add `juce::String& error` output parameter to `compile()`
- On failure: retrieve `program->getLastError()` into `error` before destroying the program
- On success: clear `error`
- Update all existing callers to pass error string

**Validation:**
- Existing shader compilation unchanged in behavior
- Error string populated on failure, empty on success
- No early returns
- NAMES: `error` — semantic (Rule 3)

---

### Step 2: jam::gl::Buffer

**Scope:** `jam/jam_gui/opengl/jam_gl_buffer.h` (new), `jam_gl_buffer.cpp` (new), `jam_gui.h`, `jam_gui.cpp`

**Action:**
- Create `jam::gl::Buffer`: FBO resource holder with fullscreen draw
- `void create (int width, int height) noexcept` — allocates framebuffer + RGBA8 colour attachment texture + empty VAO
- `void bind() noexcept` — binds framebuffer as render target
- `void unbind() noexcept` — binds default framebuffer
- `void bindTexture (int unit) noexcept` — binds colour attachment to texture unit for reading
- `void drawToScreen (juce::OpenGLShaderProgram& shader) noexcept` — unbinds FBO, binds colour texture to unit 0, draws bufferless fullscreen triangle via `gl_VertexID` (`glDrawArrays(GL_TRIANGLES, 0, 3)`)
- `GLuint getTextureID() const noexcept`
- `void destroy() noexcept` — tears down GL resources
- RAII: destructor calls `destroy()`
- Conventions: `jam_gl_` prefix, `namespace jam::gl`, PascalCase, `#pragma once`, `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`
- Register in `jam_gui.h` + `jam_gui.cpp`

**Validation:**
- BLESSED-B: RAII, owns GLuint framebuffer + texture + VAO, teardown in destructor
- BLESSED-S (Stateless): told dimensions, doesn't track resize
- BLESSED-L: one class, one job
- No early returns, no anonymous namespaces

---

### Step 3: jam::gl::Texture

**Scope:** `jam/jam_gui/opengl/jam_gl_texture.h` (new), `jam_gl_texture.cpp` (new), `jam_gui.h`, `jam_gui.cpp`

**Action:**
- Create `jam::gl::Texture`: persistent file-loaded GL texture
- `bool load (const juce::File& file) noexcept` — `juce::ImageFileFormat::loadFrom()` -> `glTexImage2D` (GL_RGBA8)
- `void bind (int unit) noexcept` — `glActiveTexture(GL_TEXTURE0 + unit)` + `glBindTexture`
- `int getWidth() const noexcept`, `int getHeight() const noexcept` — for iChannelResolution
- `void destroy() noexcept` — `glDeleteTextures`
- RAII: destructor calls `destroy()`

**Validation:**
- BLESSED-B: owns GLuint, RAII teardown
- BLESSED-E: `load()` returns false on failure
- NAMES: `Texture`, `load`, `bind` — nouns for things, verbs for actions (Rule 1)
- No early returns

---

### Step 4: Shader Config + AppState Error Property

**Scope:** `Source/lua/Engine.h`, `Source/lua/EngineParse.cpp`, `Source/lua/EngineDefaults.cpp`, `Source/AppState.h`, `Source/AppState.cpp`, `Source/AppIdentifier.h`

**Action:**

*lua::Engine:*
- Add `Engine::Shader` struct to Engine.h:
  - `juce::StringArray backgroundPasses` — file paths relative to config dir
  - `juce::StringArray textures` — file paths, max 3
  - `juce::String overlay` — file path or empty
  - `juce::String animation { "focused" }` — `"focused"` | `"always"` | `"off"`
  - `float opacity { 0.85f }`
- Add `Shader shader;` member to Engine
- Add `parseShader()` following `parseDisplay()` pattern — `optional<table>` guard, static helpers
- Handle both string form (`background = "file.frag"` -> single-pass) and table form (`background = { passes = {...}, textures = {...} }`)
- `initDefaults()`: animation = "focused", opacity = 0.85, no shader paths
- Validate: passes max 4 (`juce::jmin`), textures max 3, opacity clamped [0.0, 1.0] (`juce::jlimit`)

*AppState:*
- Add `App::ID::shaderError` identifier to AppIdentifier.h
- Add `void setShaderError (const juce::String& error)` — sets property on WINDOW subtree
- Add `void clearShaderError()` — removes property
- `juce::var`: void = valid, string = error message

**Validation:**
- Parse pattern matches existing `parseDisplayFont` / `parseDisplayWindow` exactly
- BLESSED-S (SSOT): shader config in Engine::Shader, error state in AppState
- BLESSED-E: clamped values, no magic numbers
- NAMES: `Shader` — noun, `backgroundPasses` — plural noun (Rule 1), `opacity` — semantic (Rule 3)
- No early returns in parse functions

---

### Step 5: MessageOverlay Shader Error Reaction

**Scope:** `Source/component/MessageOverlay.h`, `Source/MainComponent.cpp`

**Action:**
- MessageOverlay: add `juce::ValueTree::Listener` (private inheritance or composition)
- MainComponent: after creating MessageOverlay, pass AppState WINDOW subtree reference
- `valueTreePropertyChanged`: when `shaderError` changes to non-void string -> `showMessage (value.toString())`
- Existing config error display via `getLoadError()` unchanged

**Validation:**
- BLESSED-E (Encapsulation): MessageOverlay reacts to state, not told imperatively
- BLESSED-S (Stateless): overlay reads state, holds no shader knowledge
- Existing startup/reload error path unaffected
- No early returns

---

### Step 6: Shadertoy Shader Compilation

**Scope:** `Source/terminal/rendering/ScreenGL.cpp` (extend with shader compilation logic)

**Action:**
- Implement Shadertoy wrapping: concatenate preamble + user source + epilogue at compile time
- Fullscreen vertex shader (shared source):
  ```glsl
  #version 330 core
  void main() {
      vec2 v[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
      gl_Position = vec4(v[gl_VertexID], 0.0, 1.0);
  }
  ```
- Compile via `jam::gl::ShaderCompiler::compile()`, pass error output
- On failure: `AppState::getContext()->setShaderError(error)` -> MessageOverlay fires
- On success: `clearShaderError()`, store `std::unique_ptr<juce::OpenGLShaderProgram>`
- Read shader files from paths relative to config dir (`~/.config/end/`)
- Compile on GL context creation. Store programs in Screen::Resources.
- Internal compositing shader (alpha blend terminal over background):
  ```glsl
  #version 330 core
  uniform sampler2D terminalTexture;
  uniform float opacity;
  out vec4 fragColor;
  void main() {
      vec2 uv = gl_FragCoord.xy / vec2(textureSize(terminalTexture, 0));
      vec4 term = texture(terminalTexture, uv);
      fragColor = vec4(term.rgb, term.a * opacity);
  }
  ```

**Validation:**
- Preamble contains all 14 Shadertoy uniforms — no more, no less
- No iFocus, no iCursor
- Error path is event-driven: AppState -> MessageOverlay
- BLESSED-B: programs owned by Screen, destroyed on context close
- No early returns

---

### Step 7: Background Shader Pipeline

**Scope:** `Source/terminal/rendering/ScreenGL.cpp`

**Action:**
- **Terminal FBO:** when any shader active, existing instanced draws target `terminalBuffer` instead of default framebuffer
- **Background passes:** for each pass:
  - Bind pass's write Buffer
  - Upload all Shadertoy uniforms:
    - `iResolution` = vec3(glViewportWidth, glViewportHeight, 1.0)
    - `iTime` = seconds since first shader frame (`juce::Time::getMillisecondCounterHiRes()`)
    - `iTimeDelta` = frame delta
    - `iFrameRate` = 1.0 / iTimeDelta
    - `iFrame` = integer counter
    - `iChannelTime[4]` = all iTime
    - `iChannelResolution[i]` = vec3(width, height, 1.0) for bound channels, vec3(0) for unbound
    - `iDate` = vec4(year, month 0-indexed, day 1-indexed, seconds since midnight)
    - `iSampleRate` = 44100.0
    - `iMouse` = vec4(0) placeholder (wired in Step 9)
  - Bind iChannel0: pass 0 = own previous frame texture (ping-pong), pass N>0 = pass N-1 output
  - Bind iChannel1-3: user `jam::gl::Texture`s
  - Draw fullscreen triangle
  - Swap ping-pong for pass 0
- **Compositing:** draw terminal Buffer over background using compositing shader at configured opacity. GL_BLEND enabled.
- **FBO resize:** debounced via `juce::Timer` on Display. On timer callback, set dirty flag. renderOpenGL recreates Buffers at current `glViewportWidth` x `glViewportHeight`.
- **No shader active:** existing renderOpenGL unchanged, no FBO allocated

**Validation:**
- No shader = identical output to current, zero overhead
- FBO dimensions match physical viewport (DPI-scaled via `jam::Typeface::getDisplayScale()`)
- All uniform values match Shadertoy semantics (verified against spec)
- Ping-pong only on pass 0 (self-feedback). Later passes chain forward.
- BLESSED-B: FBOs lifecycle bound to GL context
- BLESSED-S: Buffer told dimensions, uniforms computed each frame
- No early returns

---

### Step 8: Overlay Shader Pipeline

**Scope:** `Source/terminal/rendering/ScreenGL.cpp`

**Action:**
- Single-pass post-process after compositing
- If overlay + background: capture composited result to Buffer, overlay reads as iChannel0
- If overlay only: terminal Buffer = iChannel0
- Same uniform upload as Step 7
- Overlay renders to default framebuffer (screen)

**Validation:**
- Overlay reads full composited result
- Same Shadertoy uniform set as background passes
- No early returns

---

### Step 9: iMouse Tracking

**Scope:** `Source/terminal/rendering/Screen.h` (mouse state), terminal component mouse callbacks

**Action:**
- Track mouse state per Shadertoy post-Nov 2020 semantics:
  - `.xy` = cursor position (pixels, bottom-left origin). Updated only while LMB held. Retains last click position when idle.
  - `.z` = click-start x. Positive = button down. Negative = released.
  - `.w` = click-start y. Positive = pressed this frame. Negative = held or released.
- Y-flip from JUCE (top-left) to Shadertoy (bottom-left): `y = glViewportHeight - juceY`
- Source: `mouseDown`, `mouseDrag`, `mouseUp`, `mouseMove` on terminal component
- Wire into uniform upload (replace vec4(0) from Step 7)

**Validation:**
- Coordinate space matches Shadertoy (bottom-left origin)
- Sign encoding on `.z`/`.w` matches post-Nov 2020 behavior
- `iMouse = vec4(0)` until first interaction
- No early returns

---

### Step 10: Hot Reload

**Scope:** `Source/terminal/rendering/ScreenGL.cpp`

**Action:**
- On GL thread: check shader file `getLastModificationTime()` periodically (every ~60 frames, not every frame)
- On mtime change: read source, wrap preamble, compile
- Success: swap program, `clearShaderError()`
- Failure: keep previous valid program, `setShaderError(error)` -> MessageOverlay fires
- Check all configured files (background passes + overlay)

**Validation:**
- Edit-save-see cycle works without restart
- Failed compile preserves previous working shader
- Error display automatic via AppState
- No filesystem pressure from per-frame stat
- No early returns

---

### Step 11: Animation Gating

**Scope:** `Source/terminal/rendering/ScreenGL.cpp`, `Source/MainComponent.cpp`

**Action:**
- `animation = "focused"` (default): `setContinuousRepainting(true)` while window has focus + shader active. Toggle on `focusGained`/`focusLost`.
- `animation = "always"`: `setContinuousRepainting(true)` unconditionally while shader active
- `animation = "off"`: single render on load, demand-driven after
- No shader loaded: existing `setContinuousRepainting(false)` unchanged
- Config reload updates mode dynamically

**Validation:**
- GPU idle when unfocused + "focused" mode
- No continuous repainting without active shader
- BLESSED-E: mode is explicit config
- No early returns

---

## Dependency Chain

```
Step 1 (ShaderCompiler) ─┐
Step 2 (Buffer)          ─┼─► Step 6 (Compile) ─► Step 7 (Background) ─► Step 8 (Overlay)
Step 3 (Texture)         ─┘                                            ─► Step 9 (iMouse)
Step 4 (Config+AppState) ─► Step 5 (Overlay Reaction) ─► Step 6        ─► Step 10 (Hot Reload)
                                                                       ─► Step 11 (Animation)
```

Steps 1-3 (jam) and Steps 4-5 (END) are independent — can execute in parallel.
Steps 9-11 are independent of each other — can execute in parallel after Step 7/8.

## BLESSED Alignment

- **B (Bound):** Buffer + Texture: RAII, deterministic lifecycle. Shader programs owned by Screen, destroyed on context close.
- **L (Lean):** Two jam::gl types, each one job. No new orchestrator. Shader rendering inline in renderOpenGL.
- **E (Explicit):** All 14 Shadertoy uniforms named per spec. Config keys visible. Opacity is a visible parameter. No magic. No early returns.
- **S (SSOT):** Preamble template = one uniform definition. Config in Engine::Shader. Error in AppState. Physical dims from Screen.
- **S (Stateless):** Buffer is dumb resource holder. Uniforms computed each frame. Mouse state is input data.
- **E (Encapsulation):** jam::gl types don't know about END. MessageOverlay reacts to AppState. User shaders don't know about wrapping.
- **D (Deterministic):** Same shader + same uniforms = same output. Time and frame counter are explicit inputs.

## Risks

- **Ping-pong limited to pass 0:** fixed iChannel0 rule means only the first pass gets self-feedback. Accepted trade-off (ARCHITECT decision).
- **Hot reload filesystem cost:** rate-limited mtime checks mitigate. Monitor.
- **Apple Silicon FBO:** `setComponentPaintingEnabled` is `false` — JUCE internal FBO disabled. END's Buffer is the only FBO. No double-FBO concern.
