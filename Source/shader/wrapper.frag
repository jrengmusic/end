#version 410 core

uniform vec3  iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;

uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;

out vec4 fragColor;

%%source%%

void main()
{
    vec4 col;
    mainImage (col, gl_FragCoord.xy);
    fragColor = col;
}
