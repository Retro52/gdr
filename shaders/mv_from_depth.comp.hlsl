#include "utils/post.hlsl"
#include "include/shaders/types.hlsl"

[[vk::combinedImageSampler]] Texture2D              i_depth:         register(t0);
[[vk::combinedImageSampler]] SamplerState           i_depth_sampler: register(s0);
StructuredBuffer<MotionVectorsData>                 i_mv_data:       register(t1);
[[vk::image_format("unknown")]] RWTexture2D<float2> o_motion:        register(u2);

struct PushConstantData
{
    uint2 size;
    float2 jitter;
};

[[vk::push_constant]] ConstantBuffer<PushConstantData> pc: register(b0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int2 pixel = int2(global_id.xy);
    if (any(pixel >= pc.size))
    {
        return;
    }

    float2 uv        = (float2(pixel) + 0.5) / float2(pc.size);
    float3 this_ndc  = float3(uv * 2.0 - 1.0 - pc.jitter, i_depth.SampleLevel(i_depth_sampler, uv, 0.0F).x);
    float4 world_pos = mul(i_mv_data[0].inv_frame_vp, float4(this_ndc, 1.0));
    float4 prev_clip = mul(i_mv_data[0].prev_frame_vp, world_pos);

    float2 prev_ndc = prev_clip.xy / prev_clip.w;
    o_motion[pixel] = (prev_ndc - this_ndc.xy) * 0.5.xx;
}
