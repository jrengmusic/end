#version 330 core

in vec2 vTextureCoordinates;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uEmojiTexture;

void main ()
{
    fragColor = texture (uEmojiTexture, vTextureCoordinates);
}
