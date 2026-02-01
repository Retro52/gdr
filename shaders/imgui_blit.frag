#version 450

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler2D u_source;

layout (push_constant) uniform PushConstants
{
    uint is_depth;
};

void main()
{
    vec4 sampled = texture(u_source, in_uv);

    if (is_depth == 1)
    {
        float d = sampled.r;
        const float kContrast = 20.0F;
        out_color = vec4(d * kContrast * kContrast, d * kContrast, d, 1.0);
    }
    else
    {
        out_color = sampled;
    }
}