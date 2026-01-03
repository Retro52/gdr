#version 450

layout (location = 0) out vec4 o_frag_color;
layout (location = 0) in vec3 normal;

void main()
{
    o_frag_color = vec4(normal, 1.0F);
}
