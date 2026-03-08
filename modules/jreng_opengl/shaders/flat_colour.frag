#version 330 core

in vec4 vColour;
out vec4 fragColour;

uniform int uHasMask;
uniform vec2 uViewportSize;
uniform sampler2D uMaskTexture;

void main ()
{
    fragColour = vColour;

    if (uHasMask != 0)
    {
        vec2 uv = gl_FragCoord.xy / uViewportSize;
        float maskAlpha = texture (uMaskTexture, uv).a;
        fragColour.a *= maskAlpha;
    }
}
