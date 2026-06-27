#version 410 core

uniform vec3  iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;
uniform vec4  iMouse;

uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;

uniform sampler2D iScene;

uniform float iPostOpacity;

out vec4 fragColor;

%%source%%

void main()
{
    vec4 col;
    mainImage (col, gl_FragCoord.xy);

    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 scene = texture (iScene, uv);
    float pp = step (0.0, iPostOpacity);

    fragColor = vec4 (mix (col.rgb, mix (scene.rgb, col.rgb, iPostOpacity), pp),
                      mix (col.a, scene.a, pp));
}
