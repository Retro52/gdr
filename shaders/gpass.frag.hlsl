#include "include/shaders/types.hlsl.h"
#include "include/shaders/bindings/draw.hlsl"

#ifdef ENABLE_ALPHA_TESTING
Texture2D textures[]: register(t0, space1);
#endif

#define TEXTURE2D(id, uv) textures[NonUniformResourceIndex(id)].Sample(textures_sampler, uv)

struct Input
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 uv: TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint instance_id: INSTANCE_ID;
#ifndef FOR_MESH_PIPELINE
    [[vk::location(2)]] nointerpolation uint first_index: FIRST_INDEX;
#endif
    uint primitive_id: SV_PrimitiveID;
    bool front_facing: SV_IsFrontFace;
};

struct Output
{
    uint2  o_indices: SV_Target0;
};

[shader("pixel")]
Output main(Input vs_in)
{
#ifdef ENABLE_ALPHA_TESTING
    uint albedo_idx = materials[mesh_instances[vs_in.instance_id].material_index].albedo_idx;
    float4 frag_color = albedo_idx > 0 ? TEXTURE2D(albedo_idx, vs_in.uv) : 1.0F.xxxx;
    frag_color *= materials[mesh_instances[vs_in.instance_id].material_index].diffuse_factor;

    if (frag_color.a < 0.5F)
        discard;

    uint instance_id = (uint(!vs_in.front_facing) << 31) | (vs_in.instance_id & kInstanceIdMask);
#else
    uint instance_id = vs_in.instance_id & kInstanceIdMask;
#endif

    Output result;
#ifdef FOR_MESH_PIPELINE
    result.o_indices = uint2(instance_id, vs_in.primitive_id);
#else
    result.o_indices = uint2(instance_id, vs_in.first_index + 3 * vs_in.primitive_id);
#endif

    return result;
}
