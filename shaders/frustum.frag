#version 450

layout (location = 0) out vec4 out_color;

void main()
{
    // TODO: use a specialization constant?
    out_color = vec4(1.0, 1.0, 0.0, 1.0);
}
