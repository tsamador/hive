
#version 330 core
out vec4 FragColor;

in vec2 texCoord;
in vec3 ourColor;

uniform sampler2D ourTexture;

void main()
{
    FragColor = texture(ourTexture, texCoord) *  vec4(ourColor, 1.0);
}
