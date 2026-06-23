#version 410 core

in vec2 textureCoordOut;

uniform sampler2D outputTexture;
uniform float iOpacity;

out vec4 fragColor;

void main()
{
    vec4 col = texture (outputTexture, textureCoordOut);
    float a = col.a * iOpacity;
    fragColor = vec4 (col.rgb * a, a);
}
