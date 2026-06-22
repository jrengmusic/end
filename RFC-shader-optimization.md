# RFC — Shader Pipeline Optimization: Frame Rate Control, Resolution Scaling, Opacity

Date: 2026-06-22
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END's shader pipeline renders at full VSync rate (60–120 fps) at full screen resolution. Background shaders are decorative — they don't need full fidelity. Heavy Shadertoy shaders at 5K resolution cause significant GPU load (fans spinning, thermal throttling). The current `Uniform::advance()` queries `juce::Time::getMillisecondCounterHiRes()` every frame — a wall-clock dependency that introduces jitter and is architecturally unnecessary when frame rate is a declared constant.

ARCHITECT wants video-game-style graphics settings: configurable frame rate, resolution scale, texture filtering, and opacity — all controllable from `display.lua`.

---

## Research Summary

### VSync and renderOpenGL() Scheduling

`renderOpenGL()` is already VBlank-driven:

- **macOS:** CVDisplayLink per-screen drives `triggerRepaint()` on the render thread. Each `renderOpenGL()` call corresponds to one VBlank. (`juce_OpenGLContext.cpp:909`, `juce_PerScreenDisplayLinks_mac.h:51-126`)
- **Windows:** Render thread free-spins, limited by `swapBuffers()` blocking on VSync (swap interval = 1 by default). (`juce_OpenGLContext.cpp:361, 670`)

Swap interval defaults to 1 (VSync ON) on both platforms. Configurable via `context.setSwapInterval()` but affects the entire context — cannot selectively throttle shader passes without throttling future terminal rendering.

### Display Refresh Rate Availability

| Platform | API | Status |
|---|---|---|
| Windows | `Displays::Display::verticalFrequencyHz` | Populated via `EnumDisplaySettingsW` |
| macOS | `Displays::Display::verticalFrequencyHz` | `std::nullopt` — not populated |
| macOS | `PerScreenDisplayLinks::getNominalVideoRefreshPeriodS` | Internal to JUCE, not public API |

Querying the VSync rate is unreliable cross-platform. This ruled out a decimation-ratio approach (skipFactor = vsyncRate / configuredFPS).

### Timer vs VBlankAttachment for Frame Rate Control

- **juce::Timer:** One shared JUCE timer thread services all timers. Negligible resource cost. Message-thread callback. Exact Hz via `startTimerHz()`.
- **juce::VBlankAttachment:** macOS — near-free (CVDisplayLink already running). Windows — spawns a **dedicated high-priority thread** per DXGI output calling `IDXGIOutput::WaitForVBlank()`. Fires on message thread.

Timer is lighter and more predictable. No need for actual VBlank timing — the shader frame rate is a declared constant, not derived from display refresh.

### glBlitFramebuffer vs Textured Quad for Upscale

- `juce::OpenGLFrameBuffer` does NOT expose `glBlitFramebuffer`. Would require raw `juce::gl` calls.
- `glBlitFramebuffer` is core since OpenGL 3.0, fully supported on GL 4.1. Requires `GL_READ_FRAMEBUFFER`/`GL_DRAW_FRAMEBUFFER` binding.
- **Textured quad alternative:** The pipeline already draws fullscreen textured quads sampling FBO textures. Using the same pattern for the final upscale stays within JUCE API and is consistent with the existing architecture.

ARCHITECT prefers JUCE API. Textured quad selected over raw `glBlitFramebuffer`.

### Audio Analogy

The design maps directly to audio DSP:

| Audio | Shader |
|---|---|
| Audio interface sample rate | VSync (master clock) |
| Plugin internal sample rate | `frame_rate` (shader operating rate) |
| Oversampling ratio | Inverse of `resolution_scale` |
| Decimation | Frame skipping via timer dirty flag |
| Sample-and-hold | FBO retaining last rendered frame |

The shader pipeline is downsampled from the VSync master clock. Each `renderOpenGL()` tick checks if the shader "sample" is due. When not due, the FBO holds its last value — sample-and-hold.

---

## Principles and Rationale

### Why Timer (not VBlankAttachment, not clock query, not skip factor)

**Timer** is the cleanest solution:
- No VSync rate query needed (unreliable on macOS)
- No clock query (`getMillisecondCounterHiRes()` eliminated entirely)
- No skip factor math (which needs the VSync rate)
- Exact Hz control via `startTimerHz(fps)`
- Negligible resource cost (shared timer thread)
- Same pattern as audio parameter dirty flags

The timer IS the shader's sample rate. VSync is the output stage that checks the buffer.

### Why constant delta (not measured delta)

`iTimeDelta` derived from config (`1000 / fps`) rather than measured:
- Deterministic — same value every frame, no jitter
- No clock dependency — `advance()` is pure arithmetic
- Drift from wall time is a feature — shader animation runs at declared rate regardless of actual frame timing
- Precision loss is irrelevant — values are packed as int milliseconds anyway

### Why textured quad (not glBlitFramebuffer)

- Consistent with existing pipeline — buffer passes already sample FBO textures via quads
- Stays within JUCE API surface — no raw framebuffer binding
- Opacity application is natural — the output shader multiplies alpha
- Filter control is natural — `glTexParameteri` on the texture bind

### Why opacity at output stage (not in wrapper.frag)

Discussed initially as a `wrapper.frag` change, but refined: opacity on buffer passes is wrong. Buffer passes are intermediate — premultiplying alpha there affects inter-pass compositing via `iChannel` samplers. Opacity belongs at the final output stage only, applied by the output program when drawing the Image FBO to screen.

`wrapper.frag` stays clean. `iOpacity` does not exist as a per-pass uniform.

### Why Image pass gets its own FBO

Currently Image renders to the default framebuffer. With resolution scaling, ALL passes (including Image) render at reduced resolution. The Image pass needs its own FBO so the output can upscale the final result. One extra FBO allocation — trivial.

---

## Scaffold

### Config — display.lua

```lua
graphics = {
    background = "",
    background_opacity = 0.5,
    post_processing = "",
    frame_rate = 30,            -- shader fps, default 30, range 1–120
    resolution_scale = 0.5,     -- FBO size multiplier, default 0.5, range 0.1–1.0
    filter = "linear",          -- "linear" (bilinear) or "nearest" (pixel-sharp)
},
```

### New Identifiers

```
IDENTIFIER_SHADER:
    frameRate
    resolutionScale
    filter

IDref (GLSL uniform name refs):
    iOpacity
```

### Uniform — remove clock, add frameDelta

```cpp
struct Uniform
{
    jam::HashMap<juce::Identifier, int> values {
        { ID::iResolution, 0 },
        { ID::iTime,       0 },
        { ID::iTimeDelta,  0 },
        { ID::iFrame,      0 }
    };

    jam::Function::Map<juce::Identifier, void> setters;
    int frameDelta { 1000 / 30 };   // precomputed from frame_rate config

    Uniform()
    {
        // existing setters for iResolution, iTime, iTimeDelta, iFrame (unchanged)
    }

    void setFrameRate (int fps) { frameDelta = 1000 / fps; }

    void resize (int w, int h) { values.at (ID::iResolution) = end::Size (w, h).toInt(); }

    // pure arithmetic — no clock query
    void advance()
    {
        values.at (ID::iTimeDelta) = frameDelta;
        values.at (ID::iTime) += frameDelta;
        ++values.at (ID::iFrame);
    }

    void set (juce::OpenGLShaderProgram& p) { /* unchanged */ }
    juce::Point<int> getSize() const { /* unchanged */ }
};
```

Removed: `lastFrameTime`, `juce::Time::getMillisecondCounterHiRes()`.

### Controller — timer + resolution split + output pass

```
struct Controller
    : private juce::OpenGLRenderer
    , private jam::Model::Listener
    , private juce::Timer              // NEW — shader frame rate
{
    // ... existing members ...

    // NEW — frame rate control
    std::atomic<bool> shaderDirty { true };

    // NEW — resolution state (GL thread only)
    juce::Point<int> screenSize;
    juce::Point<int> bufferSize;
    float resolutionScale { 0.5f };
    GLenum textureFilter { GL_LINEAR };
    float opacity { 0.5f };

    // NEW — output resources (GL thread only)
    juce::OpenGLFrameBuffer outputFBO;
    std::unique_ptr<juce::OpenGLShaderProgram> outputProgram;
};
```

### Timer Callback (message thread)

```cpp
void Controller::timerCallback()
{
    shaderDirty.store (true);
}
```

`startTimerHz(frameRate)` called in `attach()` after context setup. `stopTimer()` in `detach()`/destructor.

### Event Wiring (registerEvents)

```cpp
// frame_rate → update timer interval + uniform frameDelta
events.add<const juce::var&> (ID::frameRate,
    [this] (const juce::var& newValue)
    {
        int fps = static_cast<int> (newValue);
        startTimerHz (fps);
        // GL thread reads frameDelta — single int write, naturally atomic
        uniform.setFrameRate (fps);
    });

// resolution_scale → re-initialise FBOs at new buffer size
events.add<const juce::var&> (ID::resolutionScale,
    [this] (const juce::var& newValue)
    {
        resolutionScale = static_cast<float> (newValue);
        // trigger FBO resize on GL thread via resizer
        // (same path as ID::size — reuses existing resize flow)
        auto [w, h] = screenSize;
        if (w > 0 and h > 0)
            resizer.set (IDtype::view, w, h);
    });

// filter → update texture filter mode
events.add<const juce::var&> (ID::filter,
    [this] (const juce::var& newValue)
    {
        auto filterStr = newValue.toString();
        textureFilter = (filterStr == "nearest") ? GL_NEAREST : GL_LINEAR;
    });

// background_opacity → update opacity value
events.add<const juce::var&> (ID::backgroundOpacity,
    [this] (const juce::var& newValue)
    {
        opacity = static_cast<float> (newValue);
    });
```

### Resize — two sizes

```cpp
void Controller::resize (int w, int h)
{
    screenSize = { w, h };

    int bw = juce::roundToInt (w * resolutionScale);
    int bh = juce::roundToInt (h * resolutionScale);
    bufferSize = { bw, bh };

    // all pass FBOs at buffer size (including Image)
    uniform.resize (bw, bh);
    for (auto& [id, shader] : programs)
        shader->resize (context, bw, bh);

    // output FBO at buffer size (holds Image output for upscale)
    outputFBO.initialise (context, bw, bh);
    outputFBO.makeCurrentAndClear();
    outputFBO.releaseAsRenderingTarget();
}
```

### Render Loop — frame skip + output upscale

```cpp
void Controller::renderOpenGL()
{
    using namespace ::juce::gl;

    // future: terminal renders every frame here (unaffected by shader frame rate)

    if (not shaderDirty.exchange (false))
        return;     // shader frame not due — FBOs retain last content

    uniform.advance();
    auto [bw, bh] = bufferSize;

    // --- buffer passes at reduced resolution ---
    for (auto& [id, pass] : programs)
    {
        if (pass->buffer.has_value())
        {
            pass->writeBuffer().makeCurrentAndClear();
            glViewport (0, 0, bw, bh);

            pass->program->use();
            setChannels (*pass->program);
            uniform.set (*pass->program);
            quad->draw();

            pass->writeBuffer().releaseAsRenderingTarget();
            pass->swap();
        }
    }

    // --- Image pass at reduced resolution into output FBO ---
    if (programs.contains (ID::image))
    {
        outputFBO.makeCurrentAndClear();
        glViewport (0, 0, bw, bh);

        auto& image { programs.at (ID::image) };
        image->program->use();
        setChannels (*image->program);
        uniform.set (*image->program);
        quad->draw();

        outputFBO.releaseAsRenderingTarget();
    }

    unbindChannels();

    // --- output upscale: textured quad from output FBO to screen ---
    auto [sw, sh] = screenSize;

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
    glViewport (0, 0, sw, sh);

    outputProgram->use();
    outputProgram->setUniform ("iOpacity", opacity);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, outputFBO.getTextureID());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureFilter);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureFilter);
    outputProgram->setUniform ("outputTexture", 0);

    quad->draw();

    glBindTexture (GL_TEXTURE_2D, 0);
}
```

### setChannels — apply filter mode

```cpp
void Controller::setChannels (juce::OpenGLShaderProgram& program)
{
    using namespace ::juce::gl;

    for (auto& [id, channelName] : file::BufferChannel::get())
    {
        juce::Identifier passId { file::Shaders::get().at (id) };

        if (programs.contains (passId) and programs.at (passId)->buffer.has_value())
        {
            glActiveTexture (GL_TEXTURE0 + id);
            glBindTexture (GL_TEXTURE_2D, programs.at (passId)->readBuffer().getTextureID());
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureFilter);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureFilter);
            program.setUniform (channelName.toRawUTF8(), id);
        }
    }
}
```

### Output Program — fixed passthrough shader

Compiled once in `initialise()`, not part of `loadShaders()`. Two binary resources:

**output.vert** (same as screen.vert — fullscreen quad):
```glsl
#version 410 core
layout (location = 0) in vec2 position;
void main()
{
    gl_Position = vec4 (position, 0.0, 1.0);
}
```

Or reuse the existing `screen.vert` binary resource.

**output.frag:**
```glsl
#version 410 core
uniform sampler2D outputTexture;
uniform float iOpacity;
uniform vec2 screenSize;
out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / screenSize;
    vec4 col = texture (outputTexture, uv);
    fragColor = vec4 (col.rgb, col.a * iOpacity);
}
```

Compiled once at `initialise()`:
```cpp
void Controller::initialise()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad = std::make_unique<Quad>();

    outputProgram = std::make_unique<juce::OpenGLShaderProgram> (context);
    outputProgram->addVertexShader (screenQuad);
    outputProgram->addFragmentShader (BinaryData::getString ("output.frag"));
    outputProgram->link();
}
```

### Shader Loading — Image pass gets FBO

```cpp
// In loadShaders(), change Image pass creation:
if (id != ID::image)
    pass->buffer.emplace();     // buffer passes: ping-pong FBO pair
// Image pass: no buffer (output FBO is separate, owned by Controller)
```

Image pass no longer renders to default framebuffer. Controller routes it to `outputFBO` in `renderOpenGL()`.

### wrapper.frag — unchanged

No `iOpacity` in wrapper. Opacity is applied at the output stage only. `wrapper.frag` stays clean — shader authors see standard Shadertoy uniforms.

### Pipeline Diagram

```
                    bufferSize                                  screenSize
                  (w*scale, h*scale)                            (w, h)
                        │                                          │
    ┌───────────────────┼──────────────────────┐    ┌──────────────┼──────────┐
    │              SHADER PASSES               │    │         OUTPUT          │
    │                                          │    │                         │
    │  BufferA FBO ─┐                          │    │                         │
    │  BufferB FBO ─┤  iChannel0-3 samplers    │    │                         │
    │  BufferC FBO ─┤  ───────────────────►    │    │                         │
    │  BufferD FBO ─┘                          │    │                         │
    │                    │                     │    │                         │
    │                    ▼                     │    │                         │
    │              Image pass ──► outputFBO ───┼───►│  textured quad          │
    │                                          │    │  + iOpacity             │
    │                                          │    │  + filter (lin/near)    │
    │                                          │    │  ──► default FB         │
    └──────────────────────────────────────────┘    └─────────────────────────┘
                        │                                          │
                   Timer dirty flag                         VSync presents
                   gates execution
```

### Thread Contract Update

| Method | Thread |
|---|---|
| attach / detach / isAttached | MESSAGE |
| newOpenGLContextCreated / renderOpenGL / openGLContextClosing | GL |
| parameterChanged | MESSAGE (AsyncUpdater delivery) |
| **timerCallback** | **MESSAGE (juce::Timer)** |
| loadShaders (GL callback) | GL (via executeOnGLThread) |
| Resizer stop trigger | MESSAGE (juce::Timer) |
| resize / FBO initialise | GL (via executeOnGLThread) |

`shaderDirty` atomic bridges message thread (timer write) to GL thread (render read). Single bool, `exchange()` — no ABA, no torn read.

### Config Event Wiring

Same path as existing `ID::background` and `ID::size`:

1. Lua field under `graphics {}` in `display.lua`
2. Identifier in `IDENTIFIER_SHADER` X-macro
3. `watcher::Model::initialise()` creates parameter via `createAndAddParameter<T>()`
4. `watcher::Model::loadFromPath()` reads value from disk, overlays onto VT
5. Controller `registerEvents()` adds event for the new ID
6. Controller `parameterChanged()` dispatches automatically via existing events map

### Performance Impact

| Config | GPU Work |
|---|---|
| Current (60fps, 1.0 scale) | 60 × fullRes fragments/sec |
| `frame_rate=30, resolution_scale=1.0` | 30 × fullRes = **2× reduction** |
| `frame_rate=30, resolution_scale=0.5` | 30 × quarterRes = **8× reduction** |
| `frame_rate=24, resolution_scale=0.25` | 24 × 1/16 res = **~40× reduction** |

The output upscale quad is effectively free — one texture sample per screen pixel, no complex fragment shader.

---

## BLESSED Compliance Checklist

- [x] **Bounds** — Timer owned by Controller (RAII via inheritance). outputFBO owned by Controller. shaderDirty atomic has clear ownership (timer writes, GL reads). No resource floats free.
- [x] **Lean** — No new files beyond `output.frag`. Changes within existing structs (Uniform, Controller, Pass). No new abstractions.
- [x] **Explicit** — All config in `display.lua` with semantic names. No magic numbers — `frameDelta` computed from declared `frame_rate`. Filter mode named (`"linear"` / `"nearest"`), not numeric. No implicit clock dependency.
- [x] **SSOT** — `display.lua` is SSOT for all graphics settings. `frameDelta` derived from `frame_rate` (one source). `bufferSize` derived from `screenSize * resolutionScale` (one computation in `resize()`).
- [x] **Stateless** — Uniform is pure calculation state. Timer sets a flag, doesn't track history. Controller doesn't remember previous frames — each render is independent given current FBO content.
- [x] **Encapsulation** — Optimization is internal to shader::Controller. end::View unaware. wrapper.frag unchanged. Shader authors see standard Shadertoy uniforms. Output upscale is Controller's private concern.
- [x] **Deterministic** — Constant `frameDelta` = deterministic time progression. No clock jitter. Same config = same animation behavior on any display.

---

## Open Questions

None. All decisions resolved in discussion:

1. ~~Default frame rate~~ → 30 fps
2. ~~Timer vs VBlankAttachment~~ → Timer (lighter, reliable, no VSync rate needed)
3. ~~Uniforms only vs actual throttle~~ → Actual throttle via frame skip (real GPU savings)
4. ~~Resolution scale scope~~ → All passes including Image
5. ~~Blit vs textured quad~~ → Textured quad (JUCE API preference, consistent with pipeline)
6. ~~Filter scope~~ → Same filter for both channel sampling and output upscale
7. ~~Opacity location~~ → Output stage only, not wrapper.frag

---

## Handoff Notes

- **watcher module is mid-refactor.** `Source/watcher/` has stubs — method bodies empty. The config wiring pattern is documented in doxygen and the existing `Source/config/` (deleted but in git history). COUNSELOR should wire the new fields when the watcher implementation lands, or implement alongside.
- **Output program reuses `screen.vert`.** No new vertex shader needed — the existing binary resource works for the output quad.
- **`output.frag` is a new binary resource.** Add to CMake binary data target. Trivial shader — 8 lines.
- **Image pass FBO change.** Image previously rendered to default framebuffer (no `buffer.emplace()`). Now Controller routes it to `outputFBO` explicitly in `renderOpenGL()`. The Pass struct for Image is unchanged — it still has no buffer. The output FBO is Controller's concern, not Pass's.
- **Thread safety of config values.** `resolutionScale`, `textureFilter`, `opacity` are written on message thread (parameterChanged), read on GL thread (renderOpenGL). Single-word writes — naturally atomic on all targets (int, float, GLenum). No std::atomic wrapper needed, but could add for documentation clarity. `frameDelta` same — int written on message thread, read on GL thread.
- **Future terminal rendering.** The `if (not shaderDirty.exchange(false)) return;` early return in `renderOpenGL()` will need adjustment when terminal rendering is added. Terminal renders every frame; shader passes are gated. The early return should only skip the shader section, not the entire `renderOpenGL()`. Scaffold shows this with the comment marker.
- **This RFC is independent of RFC-rendering-optimization.md** (GPU instanced glyph draw). Both modify the render loop but are orthogonal — glyph rendering happens in `paint()` (component FBO), shader rendering happens in `renderOpenGL()` (default framebuffer). No conflict.
- **ARCHITECTURE.md update required.** Shader Pipeline section needs: frame rate control (timer + dirty flag), resolution scaling (two sizes), output pass (textured quad + opacity), removal of clock query from Uniform. Thread contract table gains `timerCallback`.
