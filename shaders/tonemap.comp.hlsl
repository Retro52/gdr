#include "utils/post.hlsl"
#include "include/shaders/constants.hlsl"

[[vk::image_format("unknown")]] RWTexture2D<float4> i_hdr_color: register(u0);
[[vk::image_format("unknown")]] RWTexture2D<float4> o_color:     register(u1);

struct PushConstantData
{
    uint2 size;
    uint  debug_mode;
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

    float3 color  = i_hdr_color[pixel].xyz;
#if SHADERS_DEBUG
    float4 result = 0.0F.xxxx;
    switch (pc.debug_mode)
    {
        case kDebugModeShaded:
        case kDebugModeLit:
        case kDebugModeLitDiffuse:
        case kDebugModeLitAmbient:
            result = from_linear(float4(tonemap(color), 1.0F));
            break;
        default:
            result = from_linear(float4(color, 1.0F));
            break;
    }
#else
    float4 result = from_linear(float4(tonemap(color), 1.0F));
#endif
    o_color[pixel] = result;
}
