#include "utils/post.hlsl"
#include "utils/math.hlsl"
#include "include/shaders/types.hlsl.h"

[[vk::combinedImageSampler]] Texture2D    color_texture:         register(t0);
[[vk::combinedImageSampler]] SamplerState color_texture_sampler: register(s0);
[[vk::combinedImageSampler]] Texture2D    depth_texture:         register(t1);
[[vk::combinedImageSampler]] SamplerState depth_texture_sampler: register(s1);

[[vk::image_format("unknown")]] RWTexture2D<float4> o_color: register(u2);

struct PushConstants
{
    float camera_near;
    int2 render_size;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

static const float kFxaaSpanMax        = 8.0F;
static const float kFxaaReduceMul      = 1.0F / 8.0F;
static const float kFxaaReduceMin      = 1.0F / 128.0F;
static const float kFxaaLumaThreshold  = 1.0F / 16.0F;
static const float kDepthEdgeThreshold = 0.02F;
static const float kTextureEdgeBlend   = 0.5F;

float fxaa_luma(float3 rgb)
{
    return dot(tonemap(rgb), float3(0.299F, 0.587F, 0.114F));
}

float linearize_depth(float depth)
{
    return pc.camera_near / max(depth, 1e-7F);
}

float fetch_view_z(int2 px, int2 image_size)
{
    return linearize_depth(depth_texture.Load(int3(clamp(px, int2(0, 0), image_size - 1), 0)).r);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int2 image_size = pc.render_size;
    int2 px = int2(global_id.xy);

    if (any(px >= image_size))
    return;

    float2 inv_size = 1.0F / float2(image_size);
    float2 uv = (float2(px) + 0.5F) * inv_size;

    float3 rgb_m = color_texture.SampleLevel(color_texture_sampler, uv, 0.0F).rgb;
    float3 rgb_nw = color_texture.SampleLevel(color_texture_sampler, uv + float2(-1.0F, -1.0F) * inv_size, 0.0F).rgb;
    float3 rgb_ne = color_texture.SampleLevel(color_texture_sampler, uv + float2(1.0F, -1.0F) * inv_size, 0.0F).rgb;
    float3 rgb_sw = color_texture.SampleLevel(color_texture_sampler, uv + float2(-1.0F, 1.0F) * inv_size, 0.0F).rgb;
    float3 rgb_se = color_texture.SampleLevel(color_texture_sampler, uv + float2(1.0F, 1.0F) * inv_size, 0.0F).rgb;

    float luma_m = fxaa_luma(rgb_m);
    float luma_nw = fxaa_luma(rgb_nw);
    float luma_ne = fxaa_luma(rgb_ne);
    float luma_sw = fxaa_luma(rgb_sw);
    float luma_se = fxaa_luma(rgb_se);

    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));
    float contrast = luma_max - luma_min;

    float z_m = fetch_view_z(px, image_size);
    float z_nw = fetch_view_z(px + int2(-1, -1), image_size);
    float z_ne = fetch_view_z(px + int2(1, -1), image_size);
    float z_sw = fetch_view_z(px + int2(-1, 1), image_size);
    float z_se = fetch_view_z(px + int2(1, 1), image_size);

    float depth_delta = max(max(abs(z_m - z_nw), abs(z_m - z_ne)),
                            max(abs(z_m - z_sw), abs(z_m - z_se)));

    bool geometric_edge = depth_delta > z_m * kDepthEdgeThreshold;
    bool luma_edge = contrast >= max(kFxaaReduceMin, luma_max * kFxaaLumaThreshold);

    if (!geometric_edge && !luma_edge)
    {
        o_color[px] = float4(rgb_m, 1.0F);
        return;
    }

    float2 dir;
    dir.x = -((luma_nw + luma_ne) - (luma_sw + luma_se));
    dir.y = ((luma_nw + luma_sw) - (luma_ne + luma_se));

    float dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * (0.25F * kFxaaReduceMul), kFxaaReduceMin);
    float rcp_dir_min = 1.0F / (min(abs(dir.x), abs(dir.y)) + dir_reduce);

    dir = clamp(dir * rcp_dir_min, (-kFxaaSpanMax).xx, kFxaaSpanMax.xx) * inv_size;

    float3 rgb_a = 0.5F * (
    color_texture.SampleLevel(color_texture_sampler, uv + dir * (1.0F / 3.0F - 0.5F), 0.0F).rgb +
    color_texture.SampleLevel(color_texture_sampler, uv + dir * (2.0F / 3.0F - 0.5F), 0.0F).rgb);

    float3 rgb_b = rgb_a * 0.5F + 0.25F * (
    color_texture.SampleLevel(color_texture_sampler, uv + dir * (0.0F / 3.0F - 0.5F), 0.0F).rgb +
    color_texture.SampleLevel(color_texture_sampler, uv + dir * (3.0F / 3.0F - 0.5F), 0.0F).rgb);

    float luma_b = fxaa_luma(rgb_b);
    float3 filtered = (luma_b < luma_min || luma_b > luma_max) ? rgb_a : rgb_b;

    float blend = geometric_edge ? 1.0F : kTextureEdgeBlend;
    o_color[px] = float4(lerp(rgb_m, filtered, blend), 1.0F);
}
