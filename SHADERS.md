# END Shader System — Complete Guide

Shadertoy, RetroArch Slang, and practical examples for END v0.0.1.

---

## Quick Start (< 2 min)

### Shadertoy Background
```bash
mkdir -p ~/.config/end/shaders/my_shader
# Create: Image (required — no file extension)
```

Edit `~/.config/end/display.lua`:
```lua
graphics = {
    background = "my_shader",
    background_opacity = 0.5,
    frame_rate = 30,
}
```

Press `Cmd+R`. Done.

### Slang (.slangp) Background
```bash
mkdir -p ~/.config/end/shaders/my_slang
# Create: any_name.slangp (manifest — any filename, .slangp extension; first match wins)
# Create: pass.slang or any source files referenced in the manifest
```

Same config, reload with `Cmd+R`.

---

## Format Overview

| Aspect | Shadertoy | Slang |
|--------|-----------|-------|
| **Files** | Extensionless: `Image`, `BufferA`…`BufferU`, `Common` | Flexible: any `*.slangp` manifest + any source filenames |
| **Detection** | Content-based (see below) | Content-based (see below) |
| **Passes** | Up to 21 buffers (`ShaderUniforms::maxChannelCount`) + final Image | Same 21-buffer ceiling, named in the manifest |
| **Complexity** | Simple, paste-friendly | Advanced, explicit control |
| **Use Case** | One-shot effects, prototypes | Multi-pass chains, RetroArch presets |

**Detection is content-based, never filename-based.** END always looks for the first `*.slangp` file in a project directory (any name, extension-only match), parses it, and reads the RESULT: a manifest that parses to one or more passes (a `shaders=` directive) is Slang; an absent manifest, or one that parses to zero passes (a resource-manifest-only `.slangp` — `textures=`/`mesh=`, no `shaders=`), is Shadertoy. A Shadertoy project may legitimately carry its own `.slangp` — see **Universal Resource Manifest** below.

---

## Shadertoy Format (Simple)

### Directory Structure
```
~/.config/end/shaders/plasma/
├── Image              ← Final output (REQUIRED, no extension)
├── BufferA            ← Optional intermediate pass
├── BufferB            ← Optional intermediate pass
├── …                  ← Up to BufferU (21 buffer passes total, ShaderUniforms::maxChannelCount)
└── Common             ← Optional shared code
```

Real projects on this machine, exactly as they sit on disk: `~/.config/end/shaders/plasma/{Image,BufferA,Common}`, `~/.config/end/shaders/weird/{Image,BufferA,Common}`, `~/.config/end/shaders/sirenian-dawn/{Image,BufferA}`, `~/.config/end/shaders/ether/Image` — every pass file is extensionless. There is no `.glsl`/`.frag` suffix anywhere in this format.

### Minimal Working Example
```glsl
// Image
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 col = vec3(0.5 + 0.5 * sin(iTime + uv.x * 3.0));
    fragColor = vec4(col, 1.0);
}
```

### Standard Uniforms (All Passes)
```glsl
uniform float iTime;        // Elapsed seconds
uniform float iTimeDelta;   // Frame duration (1/frame_rate)
uniform int iFrame;         // Frame number
uniform vec3 iResolution;   // (width, height, aspect)
uniform vec4 iMouse;        // (x, y, clickX, clickY) in pixels, sign-encoded (see note below)

// Buffer pass history — iChannel0-3 are paste-compat ALIASES for buffer
// ordinals 0-3 only. Buffer passes past the 4th have no iChannelN alias —
// reference them by their own canon BufferX name directly.
uniform sampler2D iChannel0; // Last frame of BufferA
uniform sampler2D iChannel1; // Last frame of BufferB
uniform sampler2D iChannel2; // Last frame of BufferC
uniform sampler2D iChannel3; // Last frame of BufferD
uniform sampler2D BufferE;   // 5th buffer pass and beyond — name only, no alias
```

**iMouse note:** real, sign-encoded Shadertoy mouse state is only stamped for BACKGROUND shaders, gated by `display.lua`'s `graphics.mouse.enabled` (false zeroes it unconditionally) and driven by whichever button `graphics.mouse.imouse` names ("left", "middle", "right", or "none" — see `display.lua`'s own `mouse` block for every field). A post-processing shader always receives `iMouse = (0, 0, 0, 0)` — mouse capture into the post-process chain is not yet wired.

### Example: Feedback Trail
```glsl
// BufferA — accumulation pass
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 prev = texture(iChannel0, uv).rgb;
    vec3 newContent = vec3(uv.x);
    vec3 col = mix(prev * 0.95, newContent, 0.05);
    fragColor = vec4(col, 1.0);
}

// Image — final composite
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 col = texture(iChannel0, uv).rgb;
    fragColor = vec4(col, 1.0);
}
```

---

## Slang Format (Advanced)

END discovers a project's manifest by **extension only** — the first `*.slangp` file found in the project directory, whatever it's named. `shader.slangp`, `my_slang.slangp`, `crt-lottes.slangp` — all equally valid, and none of the examples below require a rename. A project declares exactly one (if you leave more than one `.slangp` in a directory, only the first match — filesystem order, not guaranteed alphabetical — is read).

### Directory Structure
```
~/.config/end/shaders/my_slang/
├── my_slang.slangp        ← MANIFEST — any filename, .slangp extension
├── pass0.slang            ← Source files (any names; corpus convention is .slang)
├── pass1.slang
├── common.glsl            ← Optional shared code, pulled in via #include
├── texture.png            ← Optional resources
└── model.obj              ← Optional geometry
```

### Manifest Anatomy (my_slang.slangp)
```ini
# Total passes (required)
shaders = 2

# Per-pass directives
shader0 = pass0.slang
scale0 = 1.0
filter_linear0 = true

shader1 = pass1.slang
scale1 = 1.0
filter_linear1 = true

# Parameter overrides (optional)
MyParam = 0.5
```

There is no `common=` manifest key in slang format — that is Shadertoy's `Common` file slot. Shared slang code is pulled in with a plain `#include "common.glsl"` inside each pass source that needs it.

### Per-Pass Options
```ini
# Source file (required)
shaderN = source.slang

# Resolution scale (1.0 = source/default)
scaleN = 0.5
scale_typeN = viewport    # source/viewport/absolute
# OR separate axes:
scale_type_xN = viewport
scale_type_yN = source
scaleN = 1.5

# Filtering
filter_linearN = true     # true=smooth, false=sharp (absent falls back to the global filter config key)

# Wrap mode
wrap_modeN = repeat       # clamp_to_border / clamp_to_edge / repeat / mirrored_repeat (absent = clamp_to_border)

# Framebuffer format
srgb_framebufferN = false
float_framebufferN = false

# Input mipmap
mipmap_inputN = true      # Mipmaps for texture feeding INTO this pass

# Frame counter
frame_count_modN = 0      # Modulo frame counter (0=no wrap)

# Alias
aliasN = History          # Alternative iChannel name (optional; falls back to a #pragma name in the source)
```

---

## Universal Resource Manifest (Both Formats)

Every shader project — Shadertoy or Slang — declares external textures and a mesh through the SAME `.slangp` manifest vocabulary. A Shadertoy project's own manifest simply never declares `shaders=`, so it stays Shadertoy-detected; a Slang project carries this alongside its passes in the one manifest it already has.

```ini
# Semicolon-separated list of texture names (RetroArch's own textures= grammar)
textures = "name1;name2"

# Per-name directives — <name> itself is the path
name1 = texture.png
name1_linear = true          # bilinear filter (absent falls back to the global filter config key)
name1_wrap_mode = repeat     # clamp_to_border / clamp_to_edge / repeat / mirrored_repeat
name1_mipmap = true

name2 = lut.png

# Renders a Wavefront OBJ as this project's own output (END extension key,
# not RetroArch vocabulary) — auto-fit orbit camera, drag to orbit. Shapes
# with no assigned MTL material render as cyan line-art over a transparent,
# low-alpha cyan fill.
mesh = model.obj

# Animates the mesh's own vertices (position/normal, object space) via a
# plain mainMesh(inout vec3, inout vec3) snippet — the engine's own default
# lit pipeline/look is unchanged, only animated — END extension key, not
# RetroArch vocabulary. See "Mesh Shader" below.
mesh_shader = spin.slang
```

### Mesh Shader (`mesh_shader=`) — a vertex-animation hook, not a replacement

**Format contract:** the correct way to author a custom shader is EITHER Shadertoy format (extensionless `Image`/`Common`/`BufferA`…, plus an optional `.slangp` resource manifest — textures/mesh only, no `shaders=`) OR slang-shaders format (`.slangp` with a `shaders=` chain of `.slang` files) — never mixed. `mesh_shader=` is orthogonal to that choice — the SAME resource-manifest key either format's `.slangp` may declare, alongside `mesh=`/`textures=`.

`mesh_shader=` points at a plain GLSL snippet — its entire content is exactly one function:

```glsl
void mainMesh (inout vec3 position, inout vec3 normal)
```

your own object-space vertex transform, mirroring Shadertoy's `mainImage` paradigm one level down. Declare **nothing else** — no uniform blocks, no descriptor sets, no `#pragma stage` markers, no engine tokens. The engine's own mesh vertex-stage templates declare everything and splice your `mainMesh` in, so you read the standard `iTime`/`iTimeDelta`/`iFrame`/`iResolution`/`iMouse` names bare, exactly like any other shader pass — zero bespoke uniform vocabulary, zero data plumbing.

`mesh_shader=` **animates** the engine's own default mesh look — it never replaces it. The default rendering contract is completely untouched: per-`MaterialRange` lit/transparent fills, the default transparent cyan fill for material-less shapes, the blender-style feature-edge line-art overlay — every one of these still draws exactly as documented above. `mainMesh` runs once per vertex, in object space, before the `mvp`/normal-matrix transform, for every one of those draws (including both endpoints of every feature-edge quad) — so the WHOLE default look animates in lock-step, fill and line art alike.

Absent (no `mesh_shader=` key), or a `mesh_shader=` file that fails to compile, the engine falls back to its own no-op `mainMesh` — the exact default, unanimated look, logged via the engine's diagnostic log on a compile failure, never a crash.

**Reference example** — `~/.config/end/shaders/j3d/` is a pure slang-format project: `j3d.slangp` declares `shaders = 1`, `shader0 = j3d.slang` (the backdrop pass — the rotating banded gradient, ported from the project's former Shadertoy `Image`), plus `mesh = /Users/jreng/Desktop/j3d.obj` and `mesh_shader = spin.slang`. `spin.slang` is now nothing but `mainMesh`, spinning `position`/`normal` around Y by plain `iTime` — the mesh's own fill/transparent-fill/feature-edge draws animate with it, untouched otherwise.

**Channel binding is the entry NAME itself:**
- **Shadertoy** — name a texture `iChannel2` to bind it to that paste-compat reference directly; any other name works too, becoming a first-class `sampler2D` your source samples by that name (no iChannelN alias is generated for a named texture — the name IS the binding).
- **Slang** — reference by an author-declared `uniform sampler2D <name>`, matched by name via SPIR-V reflection.

Paths are relative to the project directory. Absolute paths are also accepted — real example on this machine: `~/.config/end/shaders/j3d/j3d.slangp` declares `mesh = /Users/jreng/Desktop/j3d.obj`, an absolute path outside the project entirely. **Prefer relative, in-project paths regardless — see the isolation principle below.** An absolute path pointing outside the project directory breaks the moment that outside file moves, exactly the fragility isolation avoids.

---

## Converting RetroArch Slang Shaders to END

### Isolate Each Preset (read this first)

Although END's manifest parser resolves relative AND absolute paths anywhere on disk, **copy every dependency a preset needs INTO its own project directory**, and rewrite any `#include`/path that escapes outward (`../`) to a local, in-project path. A self-contained project can't break when the surrounding tree it was copied from — a cloned `slang-shaders` repo, a shared `include/` folder, anything outside `~/.config/end/shaders/<name>/` — changes, moves, or gets deleted.

### What You're Starting With

A RetroArch shader directory (`slang-shaders/crt-*`, `slang-shaders/motion-blur/`, etc.) contains:

```
retroarch/shaders/crt-lottes/
├── crt-lottes.slangp           ← Preset 1 manifest
├── crt-lottes.slang            ← Preset 1 source
├── crt-lottes-multisample.slangp   ← Preset 2 (alternative)
└── crt-lottes-multisample.slang   ← Preset 2 source
```

**Each `.slangp` file is a separate preset** with different settings. Files with the same base name usually share implementation.

### Step-by-Step: Use One Preset

Example: Use `crt-lottes.slangp` preset.

**1. Create END's shader directory:**
```bash
mkdir -p ~/.config/end/shaders/crt-lottes
```

**2. Copy both files, keeping their original names — no rename needed:**
```bash
cp path/to/retroarch/shaders/crt-lottes/crt-lottes.slangp \
   ~/.config/end/shaders/crt-lottes/crt-lottes.slangp

cp path/to/retroarch/shaders/crt-lottes/crt-lottes.slang \
   ~/.config/end/shaders/crt-lottes/crt-lottes.slang
```

**Result:**
```
~/.config/end/shaders/crt-lottes/
├── crt-lottes.slangp    ← Kept as-is (END finds any *.slangp)
└── crt-lottes.slang     ← Kept as-is
```

**3. Verify the manifest references the source:**
```bash
cat ~/.config/end/shaders/crt-lottes/crt-lottes.slangp
```

Look for the line:
```ini
shader0 = crt-lottes.slang
```

✅ Does it match the source filename you copied? If yes, you're good. (The engine is extension-agnostic for `shaderN=` paths — `.slang` is the corpus convention, but a preset referencing `.glsl` sources works identically.)

**4. Enable in config:**
```lua
graphics = {
    post_processing = "crt-lottes",    -- Must match directory name
    post_processing_opacity = 0.8,
    frame_rate = 60,
}
```

**5. Reload:**
Press `Cmd+R` (macOS/Linux) or `Ctrl+R` (Windows).

---

### Step-by-Step: Complex Shader (Multiple Files)

Some shaders use multiple `.slang` files (plus `#include`d shared code):

**RetroArch source:**
```
retroarch/shaders/crt-megatron/
├── crt-megatron.slangp
├── pass0.slang
├── pass1.slang
├── pass2.slang
└── common.glsl           ← #include'd by the pass sources
```

**Manifest contents:**
```ini
shaders = 3
shader0 = pass0.slang
shader1 = pass1.slang
shader2 = pass2.slang
```

**Copy all of them, every filename unchanged:**
```bash
mkdir -p ~/.config/end/shaders/crt-megatron

cp path/to/retroarch/shaders/crt-megatron/crt-megatron.slangp \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/pass0.slang \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/pass1.slang \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/pass2.slang \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/common.glsl \
   ~/.config/end/shaders/crt-megatron/
```

**Result:**
```
~/.config/end/shaders/crt-megatron/
├── crt-megatron.slangp   ← Kept as-is
├── common.glsl           ← Kept as-is
├── pass0.slang           ← Kept as-is
├── pass1.slang           ← Kept as-is
└── pass2.slang           ← Kept as-is
```

The manifest lines still say `shader0 = pass0.slang`, and every `#include "common.glsl"` still resolves — every reference stays intact because every file kept its own name **and** its own flat position, directly inside the project directory (the isolation principle above).

---

### Troubleshooting RetroArch Imports

**"file not found" error in debug log**
- Check that all files mentioned in the manifest exist in the directory
- Example: if manifest says `shader0 = crt-lottes.slang`, verify that file exists
- Check spelling and case sensitivity (Linux is case-sensitive)

**Shader compiles but looks wrong**
- The preset may have parameter defaults you can override
- Check the manifest for lines like `#pragma parameter SHARPNESS`
- You can override these values in the manifest or adjust opacity

**Shader doesn't appear**
- Verify directory name matches config: `graphics.post_processing = "crt-lottes"`
- Verify a `*.slangp` file exists in the project directory (any name)
- Press `Cmd+R` to reload
- Check debug log: `~/.config/end/END.ode`

**Missing `#include` fails cleanly and names the exact file** — no more silent, blank-frame failures
- A copied `.slang`/`.glsl` file may carry `#include "../../something.h"` lines that reach OUTSIDE its own folder — common in shaders pulled from the slang-shaders repo, where every file assumes it still sits at its ORIGINAL repo depth
- END resolves `#include` relative to the including file's own directory, recursively. A missing include target now fails the compile cleanly — the debug log names the exact referencing file and the exact unresolved path, and the last-good shader is retained — never a silent, empty splice
- **Fix, per the isolation directive above:** copy every dependency file alongside your shader, flatten it out of any nested subfolder into the project root, then rewrite each `#include` path to a bare local filename

**Real example, exactly as it sits on this machine today:**

`~/.config/end/shaders/analog-service-menu/analog-service-menu.slangp` references `shaders/analog-service-menu.slang`, which carries these two `#include` lines, copied verbatim from the original RetroArch repo tree:

```glsl
#include "../../../include/compat_macros.inc"
#include "../../shaders/crt-royale/src/tex2Dantialias.h"
```

In the ORIGINAL `slang-shaders` repo, this file lived one directory deeper (`slang-shaders/crt/analog-service-menu/shaders/analog-service-menu.slang`) — from there, those exact `../` counts correctly reach `slang-shaders/include/` and `slang-shaders/crt/shaders/crt-royale/src/`. Copied flat into `~/.config/end/shaders/analog-service-menu/` (missing that `crt/` category level the original repo had), the SAME `../` counts now escape one directory too far and resolve to nothing — the project's own on-disk state today is exactly the broken, un-isolated case this section warns about.

**Fix — flatten + rewrite:**
```
~/.config/end/shaders/analog-service-menu/
├── analog-service-menu.slangp   ← shader0 = analog-service-menu.slang (no subfolder)
├── analog-service-menu.slang    ← moved out of shaders/ subfolder to project root
├── compat_macros.inc            ← copied from ~/.config/end/shaders/include/
└── tex2Dantialias.h             ← copied from shaders/crt-royale/src/ (already present locally)
```

```glsl
// Before (repo-relative, breaks once isolated):
#include "../../../include/compat_macros.inc"
#include "../../shaders/crt-royale/src/tex2Dantialias.h"

// After (same directory as the shader):
#include "compat_macros.inc"
#include "tex2Dantialias.h"
```

---

### Rule of Thumb

```
When importing from RetroArch:
├── *.slangp file   → Copy as-is, keep its own name (no rename — any *.slangp is discovered)
├── source files    → Copy as-is, keep original names — rewrite only #include paths that escape the project directory
└── Other files     → Ignore, or fold into the resource manifest instead (textures=/mesh=)
```

---

## Real-World Example: CRT Scanlines (Slang Format)

Minimal working CRT shader — ready to copy-paste, real RetroArch slang vocabulary this engine actually reflects (`jam::VulkanShaderReflection::reflect()`/`populateMemberBuffer()`, `jam_vulkan/shader/jam_VulkanShaderReflection.cpp:16-25,143-163`; texture-name resolution `jam::VulkanGraphics::resolveSlangTextureBindings()`, `jam_vulkan/context/jam_VulkanGraphicsSlangPass.cpp:403-423`). The manifest filename below (`shader.slangp`) is just one valid choice — any `*.slangp` name works.

A single-pass slang shader (`shaders = 1`) IS the mandatory Image pass — there is no separate Image slot to declare. `Source` at this pass's own ordinal 0, in a post-process chain, resolves to the resolved (straight-alpha) scene — the exact same input the fabricated Shadertoy-style `iScene` uniform used to stand in for.

### Setup
```bash
mkdir -p ~/.config/end/shaders/crt_simple
cd ~/.config/end/shaders/crt_simple
# Create two files below
```

### File 1: `shader.slangp`
```ini
# Simple CRT scanlines effect (post-process, single pass)
shaders = 1

# Pass 0: the only pass — this IS the mandatory Image pass
shader0 = crt.slang
filter_linear0 = true
```

### File 2: `crt.slang`
```glsl
#version 450

// Simple CRT scanlines effect — single-pass post-process.
// There is no seconds-based iTime/gl_FragCoord/iScene vocabulary for a
// slang pass — that belongs to the Shadertoy wrapper only
// (jam_vulkan/shader/jam_VulkanShaderUniforms.h). OutputSize is this
// pass's own reflected render-target extent (fixed "<X>Size" vocabulary,
// jam_VulkanShaderReflection.cpp:27-33) — used here to recover a pixel row
// from vTexCoord's own normalized y for the scanline modulation.

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
    vec4 OutputSize;
} global;

layout(push_constant) uniform Push
{
    float scanline_intensity;
} params;

// Scanline intensity (0.0 = invisible, 1.0 = harsh) — default, min, max, step.
#pragma parameter scanline_intensity "Scanline Intensity" 0.5 0.0 1.0 0.05

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
    // Source: the rendered scene — this pass being ordinal 0 of a
    // post-process chain, Source resolves to the straight-alpha scene
    // (Graphics::resolveSlangTextureBindings()'s Original/Source branch).
    vec3 scene = texture(Source, vTexCoord).rgb;

    // Horizontal scanlines — pixel row recovered from vTexCoord.y * OutputSize.y.
    float scanline = sin(vTexCoord.y * global.OutputSize.y * 3.0) * 0.5;
    vec3 col = scene * (1.0 - scanline * 0.25 * params.scanline_intensity);

    // Slight desaturation (CRT aging effect).
    float gray = dot(col, vec3(0.3, 0.59, 0.11));
    col = mix(col, vec3(gray), 0.05);

    // Slight vignette.
    vec2 vign = vTexCoord - 0.5;
    float vignette = 1.0 - dot(vign, vign) * 0.5;
    col *= vignette;

    FragColor = vec4(col, 1.0);
}
```

### Config: `~/.config/end/display.lua`
```lua
graphics = {
    post_processing = "crt_simple",    -- Post-process (runs after text)
    post_processing_opacity = 0.8,     -- 80% effect intensity
    frame_rate = 60,                   -- Smooth CRT look
    filter = "linear",
}
```

### Result
Press `Cmd+R` → retro CRT scanlines effect over your terminal.

---

## Practical Multi-Pass Example: Feedback Accumulator (Slang)

Real slang chaining vocabulary this engine actually reflects: `PassFeedback0` (a buffer pass's own explicit self-feedback read — every buffer pass is unconditionally feedback-capable, `jam::VulkanShaderPass`'s own doc comment) and `Source` (RetroArch's own "the stage immediately preceding this one in the chain" semantic — resolves to the PREVIOUS buffer pass's own current output whenever this pass's own ordinal is > 0, `Graphics::resolveSlangTextureBindings()`, `jam_vulkan/context/jam_VulkanGraphicsSlangPass.cpp:403-423`). There is no `common=` manifest directive for a slang chain (that key is Shadertoy-only vocabulary, `jam_vulkan/bimap/jam_VulkanShaderFormat.cpp:95` — never registered for the slang canon-slot map) — shared code is pulled in purely via a plain `#include`, exactly like any other file dependency.

### Setup
```bash
mkdir -p ~/.config/end/shaders/feedback_trail
cd ~/.config/end/shaders/feedback_trail
# Create files below
```

### File 1: `shader.slangp`
```ini
# Feedback accumulator background — 3 passes (accumulate, blur, composite)
shaders = 3

# Pass 0: accumulation buffer — self-feeds its own previous frame via PassFeedback0
shader0 = accumulate.slang
scale0 = 0.5
filter_linear0 = true

# Pass 1: blur — reads pass 0's own current output via Source (the stage
# immediately preceding this one in the chain)
shader1 = process.slang
scale1 = 1.0
filter_linear1 = true
mipmap_input1 = true

# Pass 2: composite — the mandatory Image pass, reads pass 1's own current
# output via Source
shader2 = composite.slang
scale2 = 1.0
filter_linear2 = true
```

### File 2: `common.glsl`
```glsl
#ifndef COMMON_H
#define COMMON_H

const float PI = 3.14159265359;
const float EPSILON = 0.00001;

vec3 linearize(vec3 col) {
    return pow(col, vec3(2.2));
}

vec3 delinearize(vec3 col) {
    return pow(col, vec3(1.0 / 2.2));
}

#endif
```

### File 3: `accumulate.slang`
```glsl
#version 450

// Accumulation pass — self-feeds via PassFeedback0, its own previous
// frame's output. There is no seconds-based iTime uniform for a slang
// pass — elapsed seconds are reconstructed from FrameCount at this
// project's own configured frame_rate (display.lua: background =
// "feedback_trail", frame_rate = 30), same technique as
// ~/.config/end/shaders/j3d/j3d.slang.
#include "common.glsl"

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
} global;

layout(push_constant) uniform Push
{
    uint FrameCount;
} params;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D PassFeedback0;

void main()
{
    // PassFeedback0: this pass's own previous frame's output (self-feedback).
    vec3 prev = texture(PassFeedback0, vTexCoord).rgb;

    // New content — reconstructed elapsed seconds drive a phase-shifting band.
    float t = float(params.FrameCount) / 30.0;
    float n = sin((t * 0.5 + vTexCoord.x * 10.0) * PI) * 0.5 + 0.5;
    vec3 newContent = vec3(n);

    // Accumulate with decay.
    vec3 accumulated = mix(prev * 0.92, newContent, 0.08);

    FragColor = vec4(accumulated, 1.0);
}
```

### File 4: `process.slang`
```glsl
#version 450

// Blur pass — reads pass 0's own current output via Source.

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
    vec4 SourceSize;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
    // Horizontal 5-tap box blur — texel step from SourceSize.z (1/width,
    // the fixed "<X>Size" vocabulary's own reciprocal slot,
    // jam_VulkanShaderReflection.cpp's writeSizeVec4()).
    vec3 col = vec3(0.0);

    for (int i = -2; i <= 2; i++)
        col += texture(Source, vTexCoord + vec2(float(i) * global.SourceSize.z, 0.0)).rgb;

    col /= 5.0;

    FragColor = vec4(col, 1.0);
}
```

### File 5: `composite.slang`
```glsl
#version 450

// Composite pass — the mandatory Image pass (ordinal 2), reads pass 1's
// own current output via Source.
#include "common.glsl"

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
    vec3 col = texture(Source, vTexCoord).rgb;

    // Boost contrast in linear space, back to display gamma.
    col = delinearize(pow(linearize(col), vec3(0.95)));

    FragColor = vec4(col, 1.0);
}
```

### Config
```lua
graphics = {
    background = "feedback_trail",
    background_opacity = 0.6,
    background_resolution = 0.5,
    frame_rate = 30,
}
```

---

## Configuration Reference

### `~/.config/end/display.lua`

`gpu` is a top-level key, separate from `graphics` — it toggles GPU rendering for the whole application, not just shaders:
```lua
gpu = true,   -- Enable GPU rendering (top-level, sibling of graphics, not inside it)
```

```lua
graphics = {
    -- Background shader
    background = "my_shader",              -- Directory name or empty
    background_opacity = 0.5,              -- 0.0 = transparent, 1.0 = opaque
    frame_rate = 30,                       -- 1-120 Hz, controls GPU load
    background_resolution = 0.5,           -- 0.0-1.0, intermediate passes only

    -- Post-processing shader (runs after text)
    post_processing = "",                  -- Directory name or empty
    post_processing_opacity = 1.0,         -- 0.0 = original, 1.0 = fully processed
    post_processing_resolution = 0.5,      -- Same as background

    -- Shared settings
    filter = "linear",                     -- "linear" (smooth) or "nearest" (sharp)

    -- Font rasterization
    font_rasterizer = "freetype",          -- "edgeTable", "freetype", or "native"
    font_gamma = 2.2,                      -- sRGB gamma correction
    font_contrast = 0.0,                   -- Synthetic darkening boost
}
```

### Hot Reload
Edit config and press `Cmd+R` (macOS/Linux) or `Ctrl+R` (Windows) — no restart needed.

---

## Standard Uniforms — Shadertoy Format

### Available in Every Shadertoy Pass
```glsl
uniform float iTime;        // Seconds since start
uniform float iTimeDelta;   // Frame duration (1 / frame_rate)
uniform int iFrame;         // Frame counter (0, 1, 2, ...)
uniform vec3 iResolution;   // (width, height, aspect_ratio)
uniform vec4 iMouse;        // (x, y, clickX, clickY) in pixels; sign-encoded per Shadertoy convention
```

Real, sign-encoded mouse state is only stamped for BACKGROUND shaders (their own click/drag events feed it), gated by `display.lua`'s `graphics.mouse.enabled` and driven by whichever button `graphics.mouse.imouse` names — see `display.lua`'s own `mouse` block. A post-processing shader always receives `iMouse = (0, 0, 0, 0)` — mouse capture into the post-process chain is not yet wired.

### Buffer Feedback (Shadertoy)
```glsl
uniform sampler2D iChannel0; // Last frame of BufferA — iChannel0-3 are paste-compat aliases only
uniform sampler2D iChannel1; // Last frame of BufferB
uniform sampler2D iChannel2; // Last frame of BufferC
uniform sampler2D iChannel3; // Last frame of BufferD
uniform sampler2D BufferE;   // 5th buffer pass onward — referenced by name, no iChannelN alias exists
```

Up to 21 buffer passes total (`ShaderUniforms::maxChannelCount`, derived from the 128-byte guaranteed Vulkan push-constant floor) — named `BufferA` through `BufferU` by a bijective base-26 scheme (the same one spreadsheet columns use).

### Post-Process Only (Shadertoy)
```glsl
uniform sampler2D iScene;   // The rendered scene (before post-process shader)
```

## Standard Uniforms — Slang Format

A slang pass gets NONE of the Shadertoy uniforms above. Its vocabulary is RetroArch's, reflected from whatever UBO/push-constant blocks the pass itself declares — the engine fills recognized member names by SPIR-V reflection:

```glsl
// Reflected fixed members (declare inside your own UBO or push_constant block)
mat4  MVP;             // Identity — pass through in the vertex stage
uint  FrameCount;      // Raw frame counter; elapsed seconds = float(FrameCount) / frame_rate
int   FrameDirection;  // Always 1 (END never rewinds)
vec4  SourceSize;      // (w, h, 1/w, 1/h) — the <X>Size pattern works for any bound texture
vec4  OutputSize;      // This pass's own output
vec4  FinalViewportSize;

// #pragma parameter values are reflected by name the same way
#pragma parameter my_param "Label" 0.5 0.0 1.0
```

Texture inputs are named samplers, resolved by reflection: `Source` (previous pass output; for a post-process chain's first pass, the rendered scene), `Original` (the scene), `PassOutputN`, `PassFeedbackN` (this pass's own previous frame), `OriginalHistoryN`, plus any `textures=` manifest name. Mouse state has no slang vocabulary — `iMouse` is Shadertoy-only.

---

## Rendering Pipeline

### Background Shader Path
1. Background shader executes (fullscreen, at frame_rate Hz)
2. JUCE renders text, UI, components on top
3. Output to screen

**Result:** Background appears behind text.

### Post-Processing Path
1. JUCE renders everything to an offscreen buffer
2. Post-process shader executes on that buffer
3. Output to screen

**Result:** Effect applies over text + background + UI.

---

## File Locations

| Item | Path |
|------|------|
| Shader projects | `~/.config/end/shaders/` |
| Config | `~/.config/end/display.lua` |
| Debug log | `~/.config/end/END.ode` |
| Vulkan pipeline cache | `~/.config/end/cache/END.cache` |

---

## Debugging

### Compilation Errors
Check debug log:
```bash
tail -f ~/.config/end/END.ode
```

### Common Issues

**"undefined reference to iChannel"**
- Shader uses iChannel but passes aren't named BufferA/B/C/D (Shadertoy only)

**"file not found: pass0.slang"**
- Referenced file doesn't exist or path is wrong in manifest

**Shader doesn't render**
- GPU disabled: set `gpu = true` in config (top-level key, not inside `graphics`)
- Project directory name mismatch: verify `background = "exact_dir_name"`
- Check that `Cmd+R` was pressed (or `auto_reload = true`)

### GPU Disabled / CPU Fallback
```lua
gpu = false  -- Forces software rendering (slower, no effects)
```

---

## Performance Tips

### Resolution Scaling
| Setting | GPU Load | Quality |
|---------|----------|---------|
| 1.0 | 100% | Best |
| 0.75 | 56% | Good |
| 0.5 | 25% | Acceptable |
| 0.25 | 6% | Fast |

### Frame Rate
```lua
graphics = {
    frame_rate = 10,  -- 90% GPU savings (vs 120 Hz)
    frame_rate = 30,  -- Standard (typical)
    frame_rate = 120, -- Smooth (high GPU load)
}
```

### Optimization Strategies
1. Lower resolution for background (text unaffected)
2. Reduce frame rate (30 Hz is imperceptible for most effects)
3. Cache computed values in buffer passes (reuse next frame)
4. Avoid expensive math in final Image pass (use simple composite)

---


---

## API Reference (For Developers)

### Shader Format Detection (content-based)
```cpp
// config::Shader::loadFromPath(), Source/config/Config.cpp — format is
// resolved from the PARSED manifest result, never from mere .slangp
// presence/absence:
const juce::File dir { file::Shaders::getPath (path.toString()) };
const auto presetFiles { dir.findChildFiles (juce::File::findFiles, false,
    jam::VulkanShaderFormat::getExtension().at (jam::VulkanShaderFormat::slang)) };
const auto presetFile { not presetFiles.isEmpty() ? presetFiles.getReference (0) : juce::File() };
const auto preset { jam::VulkanShaderPreset::parse (presetFile.loadFileAsString()) };

const int format { preset.passes.empty() ? jam::VulkanShaderFormat::shadertoy
                                          : jam::VulkanShaderFormat::slang };
```

### Shader Compilation
```cpp
// ShaderCompiler reads format + source files, keyed by ShaderFormat's own
// canon pass-name vocabulary (Common/Image/BufferX/Preset), compiles to
// SPIR-V via shaderc (vendored), caches pipelines at
// ~/.config/end/cache/END.cache.
static std::unique_ptr<jam::VulkanShader> compile (
    const juce::ValueTree& shaderState,          // Common/BufferX/Image/Preset properties + root-level path
    bool isBackground,                           // true = background, false = post-process
    int format,                                  // ShaderFormat::shadertoy or ::slang
    jam::map::ImageResample::Type filter);       // linear or nearest upscale
```

---

## Examples: Copy-Paste Ready

### Minimal CRT (Post-Process)
Already shown above — see "Real-World Example: CRT Scanlines".

### Minimal Plasma (Background)
```glsl
// Image
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    float t = iTime * 0.5;
    float wave = sin(uv.x * 10.0 + t) + cos(uv.y * 8.0 + t * 0.7);
    vec3 col = vec3(0.5 + 0.5 * sin(wave + t));
    fragColor = vec4(col * 0.3, 1.0);  // Dim for text visibility
}
```

Config:
```lua
graphics = {
    background = "plasma",
    background_opacity = 0.7,
    frame_rate = 30,
}
```

---

## Troubleshooting Checklist

- [ ] Directory exists at `~/.config/end/shaders/<name>/`
- [ ] Shadertoy: `Image` exists (no file extension)
- [ ] Slang: a `*.slangp` file exists (any name) + every file it references
- [ ] Config matches directory name: `graphics.background = "name"`
- [ ] Reload pressed: `Cmd+R` (or `auto_reload = true`)
- [ ] GPU enabled: `gpu = true` (top-level config key)
- [ ] No syntax errors in source files
- [ ] Every dependency copied INTO the project directory — no `#include`/path escaping it (isolation principle)
- [ ] Check debug log for details: `~/.config/end/END.ode`

---

## References

- **Shadertoy:** https://www.shadertoy.com
- **RetroArch Slang Shaders:** https://github.com/libretro/slang-shaders
- **GLSL Reference:** https://www.khronos.org/opengl/wiki/OpenGL_Shading_Language
- **END Docs:** SPEC.md (Phase 14) · ARCHITECTURE.md (Rendering Engine)

---

*This guide covers END v0.0.1 shader system. Both Shadertoy and Slang formats fully supported. GPU rendering via Vulkan with CPU software fallback.*
