// TODO(dx12): D3D12 has no combined image samplers, the texture and its sampler are bound separately there (t0 + s0)
#if defined(FOR_SAMPLER_2D)
[[vk::combinedImageSampler]] Texture2D    u_source:         register(t0);
[[vk::combinedImageSampler]] SamplerState u_source_sampler: register(s0);
#elif defined(FOR_SAMPLER_2D_ARRAY)
[[vk::combinedImageSampler]] Texture2DArray u_source:         register(t0);
[[vk::combinedImageSampler]] SamplerState   u_source_sampler: register(s0);
#endif

struct PushConstants
{
    float znear;
    float mip_level;
    float array_layer;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

struct Input
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 in_uv: TEXCOORD0;
};

struct Output
{
    float4 out_color: SV_Target0;
};

float linearize_depth(float depth)
{
    const float kVizFar = 250.0F;
    return 1.0F - clamp((pc.znear / max(depth, 1e-7F) - pc.znear) / (kVizFar - pc.znear), 0.0F, 1.0F);
}

[shader("pixel")]
Output main(Input input)
{
    float4 sampled = 1.0F.xxxx;
#if defined(FOR_SAMPLER_2D)
    sampled = u_source.SampleLevel(u_source_sampler, input.in_uv, pc.mip_level);
#elif defined(FOR_SAMPLER_2D_ARRAY)
    sampled = u_source.SampleLevel(u_source_sampler, float3(input.in_uv, pc.array_layer), pc.mip_level);
#endif

    Output result;
    result.out_color = pc.znear > 0.0F ? float4(linearize_depth(sampled.r).xxx, 1.0F) : sampled;

    return result;
}
