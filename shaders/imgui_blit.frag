#version 450

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler2D u_source;
layout (set = 0, binding = 1) uniform sampler2DArray u_array;

layout (push_constant) uniform PushConstants
{
    uint type;
    float mip_level;
    float brightness;
    float array_layer;
} pc;

const uint sampler_type_2d = 0;
const uint sampler_type_2darray = 1;

void main()
{
    vec4 sampled = vec4(1.0F);

    switch (pc.type)
    {
        case sampler_type_2d:      sampled = textureLod(u_source, in_uv, pc.mip_level);
        case sampler_type_2darray: sampled = textureLod(u_array, vec3(in_uv, pc.array_layer), pc.mip_level);
    }

    const float kContrast = 250.0F;
    out_color = sampled * mix(1.0F, kContrast, pc.brightness);
}
