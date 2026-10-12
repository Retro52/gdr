#include "utils/math.hlsl"
#include "include/shaders/types.hlsl"

// TODO(dx12): binding constants (include/shaders/bindings/shadow_draw.h) can't be pasted into register(), keep in sync
StructuredBuffer<Vertex>              vertices:       register(t0);
StructuredBuffer<MeshInstance>        mesh_instances: register(t6);
StructuredBuffer<MeshMaterial>        mesh_materials: register(t1);
StructuredBuffer<DrawIndexedIndirect> draw_cmds:      register(t7);

[[vk::push_constant]] ConstantBuffer<ShadowDrawPushConstants> pc: register(b0);

#if !defined(PLATFORM_MVK) && !defined(__spirv__)
#error "TODO(dx12): D3D12 has no gl_DrawID equivalent, only the PLATFORM_MVK (base instance) path is supported"
#endif

struct Input
{
    uint vertex_id: SV_VertexID;
    uint base_vertex: SV_StartVertexLocation;
#ifdef PLATFORM_MVK
    uint base_instance: SV_StartInstanceLocation;
#else
    [[vk::builtin("DrawIndex")]] uint draw_id: DRAW_ID;
#endif
};

struct Output
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 uv: TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint material_albedo: TEXCOORD1;
    [[vk::location(2)]] nointerpolation float material_alpha: TEXCOORD2;
};

[shader("vertex")]
Output main(Input input)
{
#ifdef PLATFORM_MVK
    uint cid = input.base_instance;
#else
    uint cid = input.draw_id;
#endif

    // unlike gl_VertexIndex, SV_VertexID doesn't include the base vertex (Vulkan matches it with -fvk-support-nonzero-base-vertex)
    uint vid = input.vertex_id + input.base_vertex;
    uint instance_id = draw_cmds[cid].instance_id;

    Output vs_out;
    vs_out.uv = float2(vertices[vid].ux, vertices[vid].uy);
    vs_out.material_albedo = mesh_materials[mesh_instances[instance_id].material_index].albedo_idx;
    vs_out.material_alpha  = mesh_materials[mesh_instances[instance_id].material_index].diffuse_factor.a;

    float3 vpos = float3(vertices[vid].px, vertices[vid].py, vertices[vid].pz);
    vs_out.position = mul(pc.vp, float4(transform_vec3(vpos, mesh_instances[instance_id].pos_and_scale, mesh_instances[instance_id].rotation_quat), 1.0F));

    return vs_out;
}
