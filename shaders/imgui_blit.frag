#version 450

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

#if defined(FOR_SAMPLER_2D)
layout (set = 0, binding = 0) uniform sampler2D u_source;
#elif defined(FOR_SAMPLER_2D_ARRAY)
layout (set = 0, binding = 0) uniform sampler2DArray u_source;
#endif

layout (push_constant) uniform PushConstants
{
    float mip_level;
    float brightness;
    float array_layer;
} pc;

void main()
{
    vec4 sampled = vec4(1.0F);
#if defined(FOR_SAMPLER_2D)
    sampled = textureLod(u_source, in_uv, pc.mip_level);
#elif defined(FOR_SAMPLER_2D_ARRAY)
    sampled = textureLod(u_source, vec3(in_uv, pc.array_layer), pc.mip_level);
#endif

    const float kContrast = 250.0F;
    out_color = sampled * mix(1.0F, kContrast, pc.brightness);
}
