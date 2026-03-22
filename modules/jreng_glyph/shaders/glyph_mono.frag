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
