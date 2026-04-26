#version 450

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler2D u_source;

layout (push_constant) uniform PushConstants
{
    float brightness;
};

void main()
{
    vec4 sampled = texture(u_source, in_uv);

    const float kContrast = 1000.0F;
    out_color = sampled * mix(1.0F, kContrast, brightness);
}
