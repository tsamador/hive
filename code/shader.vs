//Vertex Shader
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aColor;

uniform float yOffset = 0.0f;
uniform float xOffset = 0.0f;

out vec3 ourColor;
out vec2 texCoord;

void main()
{
   gl_Position = vec4(aPos.x + xOffset, aPos.y + yOffset, aPos.z, 1.0);
   ourColor = aColor;
   texCoord = aTexCoord;
}
