/**
 * @file jreng_glyph_shaders.h
 * @brief Embedded GLSL shader sources for the glyph and background render pipelines.
 *
 * All five shader sources are embedded as `constexpr const char*` so the
 * `jreng_glyph` module does not depend on app-level `BinaryData`.
 *
 * @see jreng::Glyph::GLContext
 */
#pragma once

namespace jreng::Glyph::Shaders
{

/**
 * @brief Vertex shader for instanced glyph rendering.
 *
 * Inputs (per-vertex):
 * - location 0: `aQuadVertex` — unit quad corner (0..1).
 *
 * Inputs (per-instance):
 * - location 1: `aScreenPosition` — top-left corner in physical pixel space.
 * - location 2: `aGlyphSize`      — glyph width/height in physical pixels.
 * - location 3: `aTextureCoordinates` — UV rect (x, y, w, h) in atlas [0, 1].
 * - location 4: `aColor`          — foreground colour RGBA.
 *
 * Uniform:
 * - `uViewportSize` — viewport width/height in physical pixels.
 */
constexpr const char* glyphVert = R"(
#version 330 core

layout (location = 0) in vec2 aQuadVertex;
layout (location = 1) in vec2 aScreenPosition;
layout (location = 2) in vec2 aGlyphSize;
layout (location = 3) in vec4 aTextureCoordinates;
layout (location = 4) in vec4 aColor;

uniform vec2 uViewportSize;

out vec2 vTextureCoordinates;
out vec4 vColor;

void main ()
{
    vec2 pixelPos = aScreenPosition + aQuadVertex * aGlyphSize;
    vec2 ndc = (pixelPos / uViewportSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4 (ndc, 0.0, 1.0);
    vTextureCoordinates = aTextureCoordinates.xy + aQuadVertex * aTextureCoordinates.zw;
    vColor = aColor;
}
)";

/**
 * @brief Fragment shader for monochrome glyph rendering.
 *
 * Samples the R8 mono atlas and multiplies by the instance colour alpha.
 *
 * Uniform:
 * - `uAtlasTexture` — sampler2D bound to GL_TEXTURE0 (mono R8 atlas).
 */
constexpr const char* glyphMonoFrag = R"(
#version 330 core

in vec2 vTextureCoordinates;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uAtlasTexture;

void main ()
{
    float alpha = texture (uAtlasTexture, vTextureCoordinates).r;
    fragColor = vec4 (vColor.rgb, vColor.a * alpha);
}
)";

/**
 * @brief Fragment shader for colour emoji glyph rendering.
 *
 * Samples the RGBA8 emoji atlas directly; ignores instance colour.
 *
 * Uniform:
 * - `uEmojiTexture` — sampler2D bound to GL_TEXTURE0 (emoji RGBA8 atlas).
 */
constexpr const char* glyphEmojiFrag = R"(
#version 330 core

in vec2 vTextureCoordinates;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uEmojiTexture;

void main ()
{
    fragColor = texture (uEmojiTexture, vTextureCoordinates);
}
)";

/**
 * @brief Vertex shader for instanced background quad rendering.
 *
 * Inputs (per-vertex):
 * - location 0: `aQuadVertex` — unit quad corner (0..1).
 *
 * Inputs (per-instance):
 * - location 1: `aScreenPosition` — top-left corner in physical pixel space.
 * - location 2: `aSize`           — quad width/height in physical pixels.
 * - location 3: `aColor`          — background colour RGBA.
 *
 * Uniform:
 * - `uViewportSize` — viewport width/height in physical pixels.
 */
constexpr const char* backgroundVert = R"(
#version 330 core

layout (location = 0) in vec2 aQuadVertex;
layout (location = 1) in vec2 aScreenPosition;
layout (location = 2) in vec2 aSize;
layout (location = 3) in vec4 aColor;

uniform vec2 uViewportSize;

out vec4 vColor;

void main ()
{
    vec2 pixelPos = aScreenPosition + aQuadVertex * aSize;
    vec2 ndc = (pixelPos / uViewportSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4 (ndc, 0.0, 1.0);
    vColor = aColor;
}
)";

/**
 * @brief Fragment shader for background quad rendering.
 *
 * Passes through the interpolated instance colour unchanged.
 */
constexpr const char* backgroundFrag = R"(
#version 330 core

in vec4 vColor;
out vec4 fragColor;

void main ()
{
    fragColor = vColor;
}
)";

} // namespace jreng::Glyph::Shaders
