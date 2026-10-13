#include "utils/math.hlsl"
#include "include/shaders/bindings/draw.hlsl"

#if !defined(EMULATE_DRAW_ID) && !defined(__spirv__)
#error "TODO(dx12): D3D12 has no gl_DrawID equivalent, enable EMULATE_DRAW_ID to use SV_StartInstanceLocation instead"
#endif

struct Input
{
    uint vertex_id: SV_VertexID;
    uint base_vertex: SV_StartVertexLocation;
#ifdef EMULATE_DRAW_ID
    uint base_instance: SV_StartInstanceLocation;
#else
    [[vk::builtin("DrawIndex")]] uint draw_id: DRAW_ID;
#endif
};

struct Output
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 uv: TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint instance_id: INSTANCE_ID;
    [[vk::location(2)]] nointerpolation uint first_index: FIRST_INDEX;
};

[shader("vertex")]
Output main(Input input)
{
#ifdef EMULATE_DRAW_ID
    uint cid = input.base_instance;
#else
    uint cid = input.draw_id;
#endif

    // unlike gl_VertexIndex, SV_VertexID doesn't include the base vertex (Vulkan matches it with -fvk-support-nonzero-base-vertex)
    uint vid = input.vertex_id + input.base_vertex;

    Output vs_out;
    vs_out.first_index = draw_cmds[cid].first_index;
    vs_out.instance_id = draw_cmds[cid].instance_id;
    vs_out.uv = float2(vertices[vid].ux, vertices[vid].uy);

    float3 vpos = float3(vertices[vid].px, vertices[vid].py, vertices[vid].pz);
    float4 world_pos = float4(transform_vec3(vpos, mesh_instances[vs_out.instance_id].pos_and_scale, mesh_instances[vs_out.instance_id].rotation_quat), 1.0F);

    vs_out.position = mul(pc.vp, world_pos);
    return vs_out;
}
