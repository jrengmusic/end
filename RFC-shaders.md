# RFC — Custom GL Shader Pipeline
Date: 2026-04-26
Status: Ready for COUNSELOR handoff

## Problem Statement

Enable END to load user-authored GLSL shaders from `~/.config/end/shaders/` as background animations (rendered behind terminal content) or overlay post-processing effects (composited on top). Shadertoy shaders are drop-in — assign file in config, it runs. END is a minified Shadertoy built into a terminal. No other terminal ships this capability.

## Research Summary

### Prior Art

| Terminal | Shader Support | Layers | Multi-pass | Mouse | Textures |
|---|---|---|---|---|---|
| Ghostty | Shadertoy `mainImage` convention | Post-process only | No (chain only) | No | No |
| Windows Terminal | HLSL, custom names | Post-process only | No | No | No |
| CRTty | LD_PRELOAD injection | Post-process only | No | No | No |
| Alacritty/Kitty/WezTerm | None | — | — | — | — |

No terminal ships true background layer support. All are post-process only — user shader receives the already-composited frame. No terminal supports user-supplied textures or multi-pass buffer chains.

### Shadertoy Convention — THE spec

Shadertoy is the contract. All conventions, coordinate spaces, uniform names, and semantics match Shadertoy exactly. Any Shadertoy shader is drop-in.

- Entry: `void mainImage(out vec4 fragColor, in vec2 fragCoord)`
- Uniforms: `iTime`, `iResolution`, `iChannel0`-`iChannel3`, `iFrame`, `iMouse`, `iDate`
- Multi-pass: Buffer A/B/C/D (4 max), each renders to FBO, feeds subsequent passes
- Coordinate space: bottom-left origin (GL convention). `iMouse.xy` = current position, `iMouse.zw` = click position — all bottom-left origin, Y flipped from JUCE mouse events on upload
- `iChannel0` = previous pass output. `iChannel1`-`iChannel3` = user textures

### END's Current Pipeline

Three instanced draw calls in `Screen::renderOpenGL()` via `jam::Glyph::GLContext`:

1. `drawBackgrounds()` — background quads (backgroundShader)
2. `drawQuads(mono)` — R8 atlas glyphs (monoShader)
3. `drawQuads(emoji)` — RGBA8 atlas glyphs (emojiShader)
4. `drawCursor()` — cursor quad

All render directly to default framebuffer. GLSL 330 core. No intermediate FBO exists.

Physical dimensions already computed by `Screen::setViewport()` via `jam::Typeface::getDisplayScale()` and stored as `Screen::glViewportWidth` / `Screen::glViewportHeight`.

### jam::gl Current State

**Has:** `ShaderCompiler`, `createAtlasTexture()`, instanced draw infra, `VertexLayout`
**Missing:** FBO wrapper with fullscreen draw, persistent texture loader with multi-unit binding

## Decisions (locked)

1. **Shadertoy is the spec.** All conventions match exactly. Drop-in compatibility is the contract.
2. **Two layers:** background (multi-pass, up to 4 buffers) + overlay (single-pass post-process).
3. **iChannel0** reserved for previous pass output. User textures on `iChannel1`-`iChannel3`. Matches Shadertoy.
4. **iMouse** — bottom-left origin, Y flipped from JUCE on upload. Matches Shadertoy.
5. **Buffer limit:** 4 (A/B/C/D). Matches Shadertoy. YAGNI.
6. **Opacity** — config float `[0.0 - 1.0]` for terminal content layer over background shader.
7. **Enrich jam::gl** with two types: `jam::gl::Buffer` (FBO + `drawToScreen()`), `jam::gl::Texture` (file-loaded, multi-unit bind).
8. **No new orchestrator** — shader passes integrate into existing `jam::gl::Renderer` → `renderOpenGL()` flow.
9. **FBO resize** — debounced via `juce::Timer` restart pattern on `Terminal::Display`. Physical dims from `Screen::glViewportWidth/Height` (already DPI-scaled). `Buffer` stays stateless — told dimensions, doesn't track them.
10. **Shader compile errors** — route through existing config error overlay mechanism. Same path, different backend.
11. **Hot reload** — poll shader file mtime on GL thread. Compile new program, swap on success only. Previous valid program continues on failure.
12. **Animation gating** — `setContinuousRepainting(true)` only when shader active + focused (default). Config: `"focused"` | `"always"` | `"off"`.

## Principles and Rationale

**Why Shadertoy convention:** Drop-in compatibility with thousands of existing shaders. Zero learning curve. The wrapping (injecting uniforms + `main()` around user's `mainImage`) is invisible to the user.

**Why two explicit layers:** Every other terminal conflates background and overlay into one post-process slot. Separating them in config gives users precise control and enables true background animation — architecturally impossible with post-process-only design.

**Why multi-pass for background:** Complex generative effects (fluid, particles, feedback) require buffer chains. Background shaders are self-contained (no terminal texture dependency), so multi-pass is pure FBO chaining. Cap at 4 matches Shadertoy.

**Why enrich jam::gl:** FBO and texture are generic GL primitives. jam is shared with Kuassa. These belong in the framework, not END-local. (B — clear ownership; S/SSOT — one implementation; E/Encapsulation — single responsibility.)

**Why opacity in config:** Without it, users must hack alpha into theme colors or write shader math to fake transparency. One float in config controls the terminal-over-background blend cleanly. (E/Explicit — visible parameter, no magic.)

## Scaffold

### jam::gl Enrichment (2 types)

Following existing conventions: `jam_gl_` prefix, `namespace jam::gl`, PascalCase, `#pragma once`, `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`. Registered in `jam_gui.h` + `jam_gui.cpp`.

**1. `jam_gl_buffer.h/.cpp` → `jam::gl::Buffer`**

FBO resource holder with fullscreen draw. Owns `GLuint` framebuffer + colour attachment texture. `create(width, height)`, `bind()`, `unbind()`, `drawToScreen()` (binds texture, emits bufferless fullscreen triangle via `gl_VertexID`, one `glDrawArrays(GL_TRIANGLES, 0, 3)` call). RAII teardown.

**2. `jam_gl_texture.h/.cpp` → `jam::gl::Texture`**

Persistent file-loaded texture. `load(juce::File)` → `juce::Image` → `glTexImage2D`. Owns `GLuint`. `bind(unit)` calls `glActiveTexture(GL_TEXTURE0 + unit)` before `glBindTexture`.

### END Shader Pipeline

Lives in `Source/terminal/rendering/`. Namespace `Terminal`. Integrates into existing `renderOpenGL()` flow.

**Compositing order per frame:**

1. Bind terminal `Buffer` → run existing instanced draws (backgrounds, glyphs, cursor)
2. Bind default framebuffer
3. If background shader: execute up to 4 passes (each binds previous `Buffer` output as `iChannel0` + user `Texture`s as `iChannel1`-`iChannel3`, `Buffer::drawToScreen()`), final pass outputs to screen
4. Composite terminal `Buffer` texture over background at configured opacity (alpha blend)
5. If overlay shader: single pass, `iChannel0` = composited result, `Buffer::drawToScreen()`

**Shadertoy wrapping** — END concatenates at compile time:

```glsl
// --- preamble (injected by END) ---
#version 330 core
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
uniform vec2      iResolution;
uniform float     iTime;
uniform int       iFrame;
uniform vec4      iMouse;
uniform bool      iFocus;
uniform vec4      iCursor;
out vec4 _fragColor;

// --- user shader pasted here ---
void mainImage(out vec4 fragColor, in vec2 fragCoord) { ... }

// --- epilogue (injected by END) ---
void main() {
    mainImage(_fragColor, gl_FragCoord.xy);
}
```

**Fixed vertex shader** (fullscreen tri, shared by all user shader passes):

```glsl
#version 330 core
out vec2 fragCoord;
void main() {
    vec2 vertices[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    fragCoord = gl_Position.xy * 0.5 + 0.5;
}
```

**FBO resize debounce** — `Terminal::Display` inherits `juce::Timer` privately:

```cpp
void resized() override
{
    startTimer (150);  // restart countdown on every resize
}

void timerCallback() override
{
    stopTimer();
    // recreate Buffer at Screen::glViewportWidth/Height (physical, DPI-scaled)
}
```

Buffer told dimensions, doesn't track them. AppState owns logical dims. Screen owns physical dims. Timer owns debounce. Each owns one thing.

**Config** (`~/.config/end/end.lua`):

```lua
END = {
    shader = {
        background = {
            passes   = { "shaders/buf_a.frag", "shaders/image.frag" },
            textures = { "shaders/noise256.png", "shaders/bluenoise.png" },
        },
        overlay   = "shaders/crt.frag",
        animation = "focused",   -- "focused" | "always" | "off"
        opacity   = 0.85,        -- terminal content layer [0.0 - 1.0]
    }
}
```

Simple case (single background, no textures):

```lua
END = {
    shader = {
        background = "shaders/nebula.frag",
        opacity    = 0.9,
    }
}
```

**Hot reload:** Poll shader file mtime on GL thread each frame. On change: compile new `OpenGLShaderProgram` via `jam::gl::ShaderCompiler`. Swap only on success. Previous valid program continues on failure. Errors routed through existing config error overlay.

**Animation gating:** `shader.animation = "focused"` (default): `setContinuousRepainting(true)` only while window has focus. `"always"` = continuous always. `"off"` = single render on load, no animation. No shader loaded = current dirty-only repaint unchanged.

## BLESSED Compliance Checklist

- [x] **Bounds** — Buffer, Texture: RAII, deterministic lifecycle, teardown in destructor. Shader programs owned within renderOpenGL flow, destroyed on context close.
- [x] **Lean** — Two jam::gl types, each one job. No new orchestrator — existing Renderer flow extended.
- [x] **Explicit** — All uniforms named per Shadertoy spec. All config keys visible. Opacity is a visible parameter. No magic values.
- [x] **SSOT** — jam::gl types defined once, referenced by END (and Kuassa future). Shadertoy convention = one vocabulary. Config = single source for shader paths. Physical dims from Screen (already computed).
- [x] **Stateless** — Buffer is a dumb resource holder, told dimensions, doesn't track them. Shader passes are dumb workers — pipeline tells them to render.
- [x] **Encapsulation** — jam::gl types don't know about END. END's pipeline doesn't poke into jam::gl internals. User shaders don't know about the wrapping. Timer debounce on Display, not on Buffer.
- [x] **Deterministic** — Same shader + same uniforms = same output. Frame counter and time are explicit inputs, not hidden state.

## Handoff Notes

- PLAN-tabs.md is currently modified on main — tab work may be in flight. COUNSELOR should check for sequencing dependencies.
- `jam::gl::Renderer` uses `setContinuousRepainting(false)` today. Shader animation requires toggling this dynamically — verify JUCE supports runtime toggle without context recreation.
- The existing `setComponentPaintingEnabled(true)` means JUCE already runs an internal FBO compositing path. Adding END's own Buffer is a second FBO — confirm no performance cliff from double-FBO on Apple Silicon (the CALayer bug workaround in `GLRoot` may interact).
- `jam::gl::ShaderCompiler::compile()` returns `nullptr` on failure but loses the error string. Should surface `getLastError()` — needed for hot reload error reporting through config error overlay.
- Physical dimensions for Buffer creation come from `Screen::glViewportWidth/Height`, already DPI-scaled via `jam::Typeface::getDisplayScale()`. No new conversion path.
