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
