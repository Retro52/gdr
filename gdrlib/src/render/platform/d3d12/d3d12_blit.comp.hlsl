// dxc.exe -T cs_6_0 -E main -WX -O3 -Vn kD3D12_BlitShader -Fh d3d12_blit_shader.hpp d3d12_blit.comp.hlsl

#ifdef __cplusplus
namespace platform::blit_shader
{
    using uint = u32;
    using uint2 = uvec2;
    using float2 = vec2;
#endif
    static const uint kD3D12_BlitGroupSize = 8;

    struct d3d12_blit_params
    {
        float2 scale;
        float2 offset;
        float2 inv_src_size;
        uint2 dst_offset;
        uint2 dst_extent;
        uint linear_filter;
    };
#ifdef __cplusplus
}
#endif

#ifndef __cplusplus

#define BLIT_ROOT_SIGNATURE                         \
    "RootConstants(num32BitConstants = 11, b0),"    \
    "DescriptorTable(SRV(t0), UAV(u0)),"            \
    "StaticSampler(s0,"                             \
    "    filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
    "    addressU = TEXTURE_ADDRESS_CLAMP,"         \
    "    addressV = TEXTURE_ADDRESS_CLAMP)"


ConstantBuffer<d3d12_blit_params> params: register(b0);

Texture2DArray<float4> i_src:   register(t0);
RWTexture2DArray<float4> o_dst: register(u0);
SamplerState linear_clamp:      register(s0);

[RootSignature(BLIT_ROOT_SIGNATURE)]
[numthreads(kD3D12_BlitGroupSize, kD3D12_BlitGroupSize, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    if (any(global_id.xy >= params.dst_extent))
    {
        return;
    }

    const float2 texel = (float2(global_id.xy) + 0.5F) * params.scale + params.offset;

    float4 color;
    if (params.linear_filter)
    {
        color = i_src.SampleLevel(linear_clamp, float3(texel * params.inv_src_size, global_id.z), 0);
    }
    else
    {
        color = i_src.Load(int4(int2(texel), global_id.z, 0));
    }

    o_dst[uint3(global_id.xy + params.dst_offset, global_id.z)] = color;
}

#endif
