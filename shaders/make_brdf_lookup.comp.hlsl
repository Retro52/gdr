#include "utils/pbr.hlsl"
#include "include/shaders/types.hlsl.h"

[[vk::image_format("unknown")]] RWTexture2D<float4> o_lut: register(u0);

float2 approx_brdf(float n_dot_v, float roughness)
{
    float3 view;
    view.x = sqrt(1.0 - n_dot_v * n_dot_v);
    view.y = 0.0;
    view.z = n_dot_v;

    float u = 0.0;
    float v = 0.0;

    float3 nv = float3(0.0, 0.0, 1.0);
    float alpha = roughness * roughness / 2;

    const uint kSampleCount = 1024U;
    for(uint i = 0U; i < kSampleCount; ++i)
    {
        float2 xi = hammersley(i, kSampleCount);
        float3 hv = importance_sample_ggx(xi, nv, roughness);
        float3 lv = normalize(2.0 * dot(view, hv) * hv - view);

        float n_dot_l = max(lv.z, 0.0);
        float n_dot_h = max(hv.z, 0.0);
        float v_dot_h = max(dot(view, hv), 0.0);

        if(n_dot_l > 0.0)
        {
            float gsmt = geometry_smith(n_dot_v, n_dot_l, alpha);
            float gvis = (gsmt * v_dot_h) / (n_dot_h * n_dot_v);
            float f_c = pow(1.0 - v_dot_h, 5.0);

            u += (1.0 - f_c) * gvis;
            v += f_c * gvis;
        }
    }

    return float2(u, v) / float(kSampleCount);
}

struct PushConstants
{
    int resolution;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int2 px = int2(global_id.xy);
    float2 uv = (float2(px) + 0.5F) / float(pc.resolution);

    o_lut[px] = float4(approx_brdf(uv.x, uv.y), 0.0F, 0.0F);
}
