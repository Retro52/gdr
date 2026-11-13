#pragma once

#include "utils/math.hlsl"
#include "utils/gather.hlsl"

bool is_within_frustum(float3 center, float radius, float frustum[6])
{
    bool visible = true;
    visible = visible && center.z * frustum[1] - abs(center.x) * frustum[0] > -radius;
    visible = visible && center.z * frustum[3] - abs(center.y) * frustum[2] > -radius;
    visible = visible && center.z - radius < -frustum[4] && center.z + radius > -frustum[5];

    return visible;
}

bool is_cone_aligned(float3 center, float radius, float3 axis, float cutoff)
{
    return dot(center, axis) < cutoff * length(center) + radius;
}

bool is_over_thresh(float4 extents, float2 size, float prec)
{
    float2 pmin = extents.xy * size;
    float2 pmax = extents.zw * size;

    return !(round(pmin.x - prec) == round(pmax.x) || round(pmin.y) == round(pmax.y + prec));
}

bool is_hizb_occluded(
    float4 extents,
    float3 center,
    float radius,
    float znear,
    float2 hizb_size,
    Texture2D hizb,
    SamplerState hizb_sampler
)
{
    float2 pmd_size = float2(extents.z - extents.x, extents.w - extents.y) * hizb_size;
    float level = ceil(log2(max(pmd_size.x, pmd_size.y)));
    float spr_depth = -znear / (center.z + radius);
    float2 texel = (extents.xy + extents.zw) * 0.5;

#if 1
    float one_mip_lower = max(level - 1.0F, 0.0F);
    hizb_size *= exp2 (-one_mip_lower);
    extents   = float4(floor(extents.xy * hizb_size), ceil(extents.zw * hizb_size));
    if (all(float2(extents.zw - extents.xy) <= 2.0F))
        level = one_mip_lower;
#endif

#ifdef EMULATE_SAMPLER_REDUCE
    float pmd_depth = textureGatherLodMin(hizb, hizb_sampler, texel, level, 0);
#else
    float pmd_depth = hizb.SampleLevel(hizb_sampler, texel, level).x;
#endif

    return spr_depth < pmd_depth;
}

bool is_hizb_occluded2(
    float3 center,
    float radius,
    float znear,
    float proj_00,
    float proj_11,
    float2 hizb_size,
    Texture2D hizb,
    SamplerState hizb_sampler
)
{
    float4 extents;
    if (project_sphere(center, radius, znear, proj_00, proj_11, extents))
    {
        return is_hizb_occluded(extents, center, radius, znear, hizb_size, hizb, hizb_sampler);
    }

    return false;
}
