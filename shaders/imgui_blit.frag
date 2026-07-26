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
    float znear;
    float mip_level;
    float array_layer;
} pc;

float linearize_depth(float depth)
{
    const float kVizFar = 250.0F;
    return 1.0F - clamp((pc.znear / max(depth, 1e-7F) - pc.znear) / (kVizFar - pc.znear), 0.0F, 1.0F);
}

void main()
{
    vec4 sampled = vec4(1.0F);
#if defined(FOR_SAMPLER_2D)
    sampled = textureLod(u_source, in_uv, pc.mip_level);
#elif defined(FOR_SAMPLER_2D_ARRAY)
    sampled = textureLod(u_source, vec3(in_uv, pc.array_layer), pc.mip_level);
#endif

    out_color = pc.znear > 0.0F ? vec4(vec3(linearize_depth(sampled.r)), 1.0F) : sampled;
}
