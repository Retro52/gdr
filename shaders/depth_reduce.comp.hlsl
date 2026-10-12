#include "utils/math.hlsl"
#include "utils/gather.hlsl"
#include "include/shaders/types.hlsl"

// TODO(dx12): D3D12 has no combined image samplers, the texture and its sampler are bound separately there (t0 + s0)
[[vk::combinedImageSampler]] Texture2D    input_depth:         register(t0);
[[vk::combinedImageSampler]] SamplerState input_depth_sampler: register(s0);

[[vk::image_format("r32f")]] RWTexture2D<float> output_depth: register(u1);

struct PushConstants
{
    float2 output_image_size;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    uint2 pos = global_id.xy;
#ifndef PLATFORM_MVK
    float result = input_depth.SampleLevel(input_depth_sampler, (float2(pos) + 0.5.xx) / pc.output_image_size, 0.0).r;
#else
    float result = textureGatherMin(input_depth, input_depth_sampler, (float2(pos) + 0.5.xx) / pc.output_image_size, 0);
#endif
    output_depth[pos] = result;
}
