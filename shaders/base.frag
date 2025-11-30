#version 450

layout (location = 0) out vec4 outColor;
layout (location = 0) in vec3 iVertColor;

void main()
{
    outColor = vec4(iVertColor, 1.0);
}
