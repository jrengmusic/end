# END Shader System — Complete Guide

Shadertoy, RetroArch Slang, and practical examples for END v0.0.1.

---

## Quick Start (< 2 min)

### Shadertoy Background
```bash
mkdir -p ~/.config/end/shaders/my_shader
# Create: Image.glsl (required)
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
# Create: shader.slangp (required manifest)
# Create: pass.glsl or any .glsl files referenced in manifest
```

Same config, reload with `Cmd+R`.

---

## Format Overview

| Aspect | Shadertoy | Slang |
|--------|-----------|-------|
| **Files** | Fixed: Image.glsl, BufferA-D.glsl, Common.glsl | Flexible: shader.slangp manifest + any .glsl files |
| **Detection** | Absence of .slangp | Presence of shader.slangp |
| **Passes** | Up to 4 buffers + final Image | Unlimited passes, named in manifest |
| **Complexity** | Simple, paste-friendly | Advanced, explicit control |
| **Use Case** | One-shot effects, prototypes | Multi-pass chains, RetroArch presets |

---

## Shadertoy Format (Simple)

### Directory Structure
```
~/.config/end/shaders/plasma/
├── Image.glsl           ← Final output (REQUIRED)
├── BufferA.glsl         ← Optional intermediate pass
├── BufferB.glsl         ← Optional intermediate pass
├── BufferC.glsl         ← Optional intermediate pass
├── BufferD.glsl         ← Optional intermediate pass
└── Common.glsl          ← Optional shared code
```

### Minimal Working Example
```glsl
// Image.glsl
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
uniform vec4 iMouse;        // (x, y, clickX, clickY) in pixels

// Buffer pass history (iChannel0-3)
uniform sampler2D iChannel0; // Last frame of BufferA
uniform sampler2D iChannel1; // Last frame of BufferB
uniform sampler2D iChannel2; // Last frame of BufferC
uniform sampler2D iChannel3; // Last frame of BufferD
```

### Example: Feedback Trail
```glsl
// BufferA.glsl — accumulation pass
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 prev = texture(iChannel0, uv).rgb;
    vec3 newContent = vec3(uv.x);
    vec3 col = mix(prev * 0.95, newContent, 0.05);
    fragColor = vec4(col, 1.0);
}

// Image.glsl — final composite
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 col = texture(iChannel0, uv).rgb;
    fragColor = vec4(col, 1.0);
}
```

---

## Slang Format (Advanced)

### Directory Structure
```
~/.config/end/shaders/my_slang/
├── shader.slangp          ← MANIFEST (REQUIRED, exact name)
├── pass0.glsl             ← Source files (any names)
├── pass1.glsl
├── common.glsl            ← Optional shared code
├── texture.png            ← Optional resources
└── model.obj              ← Optional geometry
```

### Manifest Anatomy (shader.slangp)
```ini
# Total passes (required)
shaders = 2

# Shared code (optional)
common = common.glsl

# Per-pass directives
shader0 = pass0.glsl
scale0 = 1.0
filter_linear0 = true

shader1 = pass1.glsl
scale1 = 1.0
filter_linear1 = true

# Parameter overrides (optional)
MyParam = 0.5
```

### Per-Pass Options
```ini
# Source file (required)
shaderN = source.glsl

# Resolution scale (1.0 = source/default)
scaleN = 0.5
scale_typeN = viewport    # source/viewport/absolute
# OR separate axes:
scale_type_xN = viewport
scale_type_yN = source
scaleN = 1.5

# Filtering
filter_linearN = true     # true=smooth, false=sharp

# Wrap mode
wrap_modeN = repeat       # border/edge/repeat/mirroredRepeat

# Framebuffer format
srgb_framebufferN = false
float_framebufferN = false

# Input mipmap
mipmap_inputN = true      # Mipmaps for texture feeding INTO this pass

# Frame counter
frame_count_modN = 0      # Modulo frame counter (0=no wrap)

# Alias
aliasN = History          # Alternative iChannel name (optional)
```

---

## Converting RetroArch Slang Shaders to END

### What You're Starting With

A RetroArch shader directory (`slang-shaders/crt-*`, `slang-shaders/motion-blur/`, etc.) contains:

```
retroarch/shaders/crt-lottes/
├── crt-lottes.slangp           ← Preset 1 manifest
├── crt-lottes.glsl             ← Preset 1 source
├── crt-lottes-multisample.slangp   ← Preset 2 (alternative)
└── crt-lottes-multisample.glsl    ← Preset 2 source
```

**Each `.slangp` file is a separate preset** with different settings. Files with the same base name usually share implementation.

### Step-by-Step: Use One Preset

Example: Use `crt-lottes.slangp` preset.

**1. Create END's shader directory:**
```bash
mkdir -p ~/.config/end/shaders/crt-lottes
```

**2. Copy two files from RetroArch:**
```bash
# Copy the manifest and rename it to shader.slangp
cp path/to/retroarch/shaders/crt-lottes/crt-lottes.slangp \
   ~/.config/end/shaders/crt-lottes/shader.slangp

# Copy the shader source (keep original name)
cp path/to/retroarch/shaders/crt-lottes/crt-lottes.glsl \
   ~/.config/end/shaders/crt-lottes/crt-lottes.glsl
```

**Result:**
```
~/.config/end/shaders/crt-lottes/
├── shader.slangp        ← Renamed from crt-lottes.slangp
└── crt-lottes.glsl      ← Copied as-is
```

**Key rule:** Only the `.slangp` file gets renamed to `shader.slangp`. All `.glsl` files keep their original names.

**3. Verify the manifest references the source:**
```bash
# Open shader.slangp
cat ~/.config/end/shaders/crt-lottes/shader.slangp
```

Look for the line:
```ini
shader0 = crt-lottes.glsl
```

✅ Does it match the `.glsl` filename you copied? If yes, you're good.

**4. Enable in config:**
```bash
# Edit ~/.config/end/display.lua
```

```lua
graphics = {
    post_processing = "crt-lottes",    ← Must match directory name
    post_processing_opacity = 0.8,
    frame_rate = 60,
}
```

**5. Reload:**
Press `Cmd+R` (macOS/Linux) or `Ctrl+R` (Windows).

---

### Step-by-Step: Complex Shader (Multiple Files)

Some shaders use multiple `.glsl` files:

**RetroArch source:**
```
retroarch/shaders/crt-megatron/
├── crt-megatron.slangp
├── pass0.glsl
├── pass1.glsl
├── pass2.glsl
└── common.glsl
```

**Manifest contents:**
```ini
shaders = 3
common = common.glsl
shader0 = pass0.glsl
shader1 = pass1.glsl
shader2 = pass2.glsl
```

**Copy all of them:**
```bash
mkdir -p ~/.config/end/shaders/crt-megatron

# Manifest (renamed)
cp path/to/retroarch/shaders/crt-megatron/crt-megatron.slangp \
   ~/.config/end/shaders/crt-megatron/shader.slangp

# All .glsl files (keep original names)
cp path/to/retroarch/shaders/crt-megatron/pass0.glsl \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/pass1.glsl \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/pass2.glsl \
   ~/.config/end/shaders/crt-megatron/
cp path/to/retroarch/shaders/crt-megatron/common.glsl \
   ~/.config/end/shaders/crt-megatron/
```

**Result:**
```
~/.config/end/shaders/crt-megatron/
├── shader.slangp      ← Renamed
├── common.glsl        ← Kept as-is
├── pass0.glsl         ← Kept as-is
├── pass1.glsl         ← Kept as-is
└── pass2.glsl         ← Kept as-is
```

The manifest lines still say `common = common.glsl` and `shader0 = pass0.glsl` — **do not rename these**, only the `.slangp` file.

---

### Troubleshooting RetroArch Imports

**"file not found" error in debug log**
- Check that all files mentioned in the manifest exist in the directory
- Example: if manifest says `shader0 = crt-lottes.glsl`, verify that file exists
- Check spelling and case sensitivity (Linux is case-sensitive)

**Shader compiles but looks wrong**
- The preset may have parameter defaults you can override
- Check `shader.slangp` for lines like `#pragma parameter SHARPNESS`
- You can override these values in the manifest or adjust opacity

**Shader doesn't appear**
- Verify directory name matches config: `graphics.post_processing = "crt-lottes"`
- Verify `shader.slangp` exists (exact name, lowercase)
- Press `Cmd+R` to reload
- Check debug log: `~/.local/share/END/debug.log`

**Nothing renders, no error at all (silent failure)** — the most deceptive failure mode
- The `.slang`/`.glsl` source file has `#include "../../something.h"` lines that reach OUTSIDE its own folder — common in shaders that share code across the slang-shaders repo (e.g. `crt-royale` helper headers, `include/compat_macros.inc`, `include/colorspace-tools.h`)
- END resolves `#include` relative to the including file's own directory, recursively. A missing include target expands to **empty text, not an error** — the shader silently loses macros/functions it depends on and produces nothing
- **Check every `.slang`/`.glsl` file you copy for `#include` lines with `../` in the path** — each one points at a file that lived somewhere else in the original repo tree
- **Fix:** copy those dependency files alongside your shader, then rewrite each `#include` path to match your new flat layout

**Example — a shader that depends on files two directories up:**

Original repo layout:
```
slang-shaders/
├── include/compat_macros.inc
└── crt/crt-effects/
    ├── crt-effects.slangp
    └── shaders/
        └── crt-effects.slang       ← contains:
                                       #include "../../../include/compat_macros.inc"
                                       #include "../../shaders/crt-royale/src/tex2Dantialias.h"
```

Copying only `crt-effects.slangp` + `crt-effects.slang` breaks both includes — the paths assume the full repo tree above them.

**Fix — flatten + rewrite:**
```
~/.config/end/shaders/crt-effects/
├── shader.slangp              ← shader0 = crt-effects.slang (no subfolder)
├── crt-effects.slang          ← moved out of shaders/ subfolder to project root
├── compat_macros.inc          ← copied from slang-shaders/include/
└── tex2Dantialias.h           ← copied from slang-shaders/crt/shaders/crt-royale/src/
```

Edit the two `#include` lines inside your copy of `crt-effects.slang`:
```glsl
// Before (repo-relative, breaks when flattened):
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
├── *.slangp file   → Copy, then RENAME to shader.slangp
├── *.glsl files    → Copy as-is, keep original names
└── Other files     → Ignore (textures, docs, configs)
```

---

## Real-World Example: CRT Scanlines (Slang Format)

Minimal working CRT shader — ready to copy-paste.

### Setup
```bash
mkdir -p ~/.config/end/shaders/crt_simple
cd ~/.config/end/shaders/crt_simple
# Create three files below
```

### File 1: `shader.slangp`
```ini
# Simple CRT scanlines effect
shaders = 1

# Pass 0: The only pass (becomes Image)
shader0 = crt.glsl
filter_linear0 = true
```

### File 2: `crt.glsl`
```glsl
// Simple CRT scanlines effect
// Based on RetroArch crt-simple.glsl

uniform sampler2D iScene;     // The rendered scene
uniform vec3 iResolution;
uniform float iTime;

// Scanline intensity (0.0 = invisible, 1.0 = harsh)
#pragma parameter scanline_intensity "Scanline Intensity" 0.5 0.0 1.0

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    
    // Read the original scene
    vec3 scene = texture(iScene, uv).rgb;
    
    // Apply horizontal scanlines
    float scanline = sin(gl_FragCoord.y * 3.0) * 0.5;
    vec3 col = scene * (1.0 - scanline * 0.25);
    
    // Slight desaturation (CRT aging effect)
    float gray = dot(col, vec3(0.3, 0.59, 0.11));
    col = mix(col, vec3(gray), 0.05);
    
    // Slight vignette
    vec2 vign = uv - 0.5;
    float vignette = 1.0 - dot(vign, vign) * 0.5;
    col *= vignette;
    
    fragColor = vec4(col, 1.0);
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

### Setup
```bash
mkdir -p ~/.config/end/shaders/feedback_trail
cd ~/.config/end/shaders/feedback_trail
# Create files below
```

### File 1: `shader.slangp`
```ini
# Feedback trail effect with 3 passes
shaders = 3
common = common.glsl

# Pass 0: Accumulation buffer
shader0 = accumulate.glsl
scale0 = 0.5
filter_linear0 = true

# Pass 1: Processing
shader1 = process.glsl
scale1 = 1.0
filter_linear1 = true
mipmap_input1 = true

# Pass 2: Final composite
shader2 = composite.glsl
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

### File 3: `accumulate.glsl`
```glsl
#include "common.glsl"

uniform sampler2D iChannel0;
uniform vec3 iResolution;
uniform float iTime;
uniform int iFrame;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    
    // Previous frame
    vec3 prev = texture(iChannel0, uv).rgb;
    
    // New content (example: noise + time)
    float n = sin(iTime * 0.5 + uv.x * 10.0) * 0.5 + 0.5;
    vec3 newContent = vec3(n);
    
    // Accumulate with decay
    vec3 accumulated = mix(prev * 0.92, newContent, 0.08);
    
    fragColor = vec4(accumulated, 1.0);
}
```

### File 4: `process.glsl`
```glsl
#include "common.glsl"

uniform sampler2D iChannel0;
uniform vec3 iResolution;
uniform float iTime;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    
    // Blur the accumulated buffer
    vec3 col = vec3(0.0);
    float blur = 0.01;
    
    for(int i = -2; i <= 2; i++) {
        col += texture(iChannel0, uv + vec2(float(i) * blur, 0.0)).rgb;
    }
    col /= 5.0;
    
    fragColor = vec4(col, 1.0);
}
```

### File 5: `composite.glsl`
```glsl
#include "common.glsl"

uniform sampler2D iChannel0;
uniform vec3 iResolution;
uniform float iTime;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    
    vec3 col = texture(iChannel0, uv).rgb;
    
    // Boost contrast
    col = pow(col, vec3(0.95));
    
    fragColor = vec4(col, 1.0);
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

### `~/.config/end/display.lua` Graphics Block

```lua
graphics = {
    -- Background shader
    background = "my_shader",              -- Directory name or empty
    background_opacity = 0.5,              -- 0.0 = transparent, 1.0 = opaque
    background_resolution = 0.5,           -- 0.0-1.0, intermediate passes only
    frame_rate = 30,                       -- 1-120 Hz, controls GPU load
    
    -- Post-processing shader (runs after text)
    post_processing = "",                  -- Directory name or empty
    post_processing_opacity = 1.0,         -- 0.0 = original, 1.0 = fully processed
    post_processing_resolution = 0.5,      -- Same as background
    
    -- Shared settings
    filter = "linear",                     -- "linear" (smooth) or "nearest" (sharp)
    gpu = true,                            -- Enable GPU rendering
    
    -- Font rasterization
    font_rasterizer = "freetype",          -- "edge_table", "freetype", or "native"
    font_gamma = 2.2,                      -- sRGB gamma correction
    font_contrast = 0.0,                   -- Synthetic darkening boost
}
```

### Hot Reload
Edit config and press `Cmd+R` (macOS/Linux) or `Ctrl+R` (Windows) — no restart needed.

---

## Standard Uniforms (All Formats)

### Available in Every Shader Pass
```glsl
uniform float iTime;        // Seconds since start
uniform float iTimeDelta;   // Frame duration (1 / frame_rate)
uniform int iFrame;         // Frame counter (0, 1, 2, ...)
uniform vec3 iResolution;   // (width, height, aspect_ratio)
uniform vec4 iMouse;        // (x, y, clickX, clickY) in pixels; y=0 at bottom
```

### Buffer Feedback (Shadertoy/Slang)
```glsl
uniform sampler2D iChannel0; // Last frame of BufferA (or first buffer pass)
uniform sampler2D iChannel1; // Last frame of BufferB (or second buffer pass)
uniform sampler2D iChannel2; // Last frame of BufferC (or third buffer pass)
uniform sampler2D iChannel3; // Last frame of BufferD (or fourth buffer pass)
```

### Post-Process Only
```glsl
uniform sampler2D iScene;   // The rendered scene (before post-process shader)
```

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
| Debug log | `~/.local/share/END/debug.log` (macOS/Linux) |
| Vulkan cache | `~/.config/end/cache/vulkan_pipeline.cache` |

---

## Debugging

### Compilation Errors
Check debug log:
```bash
tail -f ~/.local/share/END/debug.log
```

### Common Issues

**"undefined reference to iChannel"**
- Shader uses iChannel but passes aren't named BufferA/B/C/D (Shadertoy only)

**"file not found: pass0.glsl"**
- Referenced file doesn't exist or path is wrong in manifest

**Shader doesn't render**
- GPU disabled: set `gpu = true` in config
- Project directory name mismatch: verify `background = "exact_dir_name"`
- Check that `Cmd+R` was pressed (or `auto_reload = true`)

### GPU Disabled / CPU Fallback
```lua
graphics = { gpu = false }  -- Forces software rendering (slower, no effects)
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

### Shader Detection (Config)
```cpp
// END scans for these files:
// 1. shader.slangp (Slang format)
// 2. Image.glsl + BufferX.glsl (Shadertoy format, default)

int format = jam::vulkan::ShaderFormat::shadertoy;
if (dir.findChildFiles("*.slangp").isNotEmpty())
    format = jam::vulkan::ShaderFormat::slang;
```

### Shader Compilation
```cpp
// ShaderCompiler reads format + source files
// Compiles to SPIR-V via shaderc (vendored)
// Caches pipelines at ~/.config/end/cache/vulkan_pipeline.cache

jam::vulkan::ShaderCompiler::compile(
    shaderState,    // ValueTree with Common/BufferX/Image/Preset properties
    isBackground,   // true = background, false = post-process
    format,         // ShaderFormat::shadertoy or ::slang
    filter          // ImageResample::Type::linear or ::nearest
);
```

---

## Examples: Copy-Paste Ready

### Minimal CRT (Post-Process)
Already shown above — see "Real-World Example: CRT Scanlines".

### Minimal Plasma (Background)
```glsl
// Image.glsl
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
- [ ] Shadertoy: `Image.glsl` exists
- [ ] Slang: `shader.slangp` exists (exact name) + referenced files
- [ ] Config matches directory name: `graphics.background = "name"`
- [ ] Reload pressed: `Cmd+R` (or `auto_reload = true`)
- [ ] GPU enabled: `graphics.gpu = true`
- [ ] No syntax errors in `.glsl` files
- [ ] Check debug log for details: `~/.local/share/END/debug.log`

---

## References

- **Shadertoy:** https://www.shadertoy.com
- **RetroArch Slang Shaders:** https://github.com/libretro/slang-shaders
- **GLSL Reference:** https://www.khronos.org/opengl/wiki/OpenGL_Shading_Language
- **END Docs:** SPEC.md (Phase 14) · ARCHITECTURE.md (Rendering Engine)

---

*This guide covers END v0.0.1 shader system. Both Shadertoy and Slang formats fully supported. GPU rendering via Vulkan with CPU software fallback.*
