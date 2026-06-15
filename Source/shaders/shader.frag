uniform vec3  iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;

out vec4 fragColor;

void mainImage (out vec4 fragColor, in vec2 fragCoord);

void main()
{
    vec4 col;
    mainImage (col, gl_FragCoord.xy);
    fragColor = col;
}
