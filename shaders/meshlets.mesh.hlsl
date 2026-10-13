#include "utils/math.hlsl"
#include "include/shaders/types.hlsl"
#include "include/shaders/bindings/draw.hlsl"

struct VertexOutput
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 uv: TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint instance_id: INSTANCE_ID;
};

struct PrimitiveOutput
{
    uint primitive_id: SV_PrimitiveID;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(kMeshWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID, in payload DrawMeshletsTask meshlet_task,
          out vertices VertexOutput vs_out[64], out indices uint3 triangles[96], out primitives PrimitiveOutput prims[96])
{
    const uint t_idx = local_id.x;
    const uint m_idx = meshlet_task.meshlet_ids[group_id.x];

    const uint vertex_count = meshlets[m_idx].vertices_count();
    const uint triangle_count = meshlets[m_idx].triangles_count();

    const uint base_vertex = meshlets[m_idx].data_offset / 4;
    const uint base_index  = meshlets[m_idx].data_offset + vertex_count * 4;

    SetMeshOutputCounts(vertex_count, triangle_count);

    // Each thread processes multiple vertices, striding by workgroup size
    for (uint i = t_idx; i < vertex_count; i += kMeshWorkGroups)
    {
        const uint vid = meshlet_task.base_vertex + meshlet_data.Load((base_vertex + i) * 4);
        float4 world_pos = float4(transform_vec3(
            float3(vertices[vid].px, vertices[vid].py, vertices[vid].pz),
            mesh_instances[meshlet_task.instance_id].pos_and_scale,
            mesh_instances[meshlet_task.instance_id].rotation_quat), 1.0F);

        vs_out[i].instance_id = meshlet_task.instance_id;
        vs_out[i].uv = float2(vertices[vid].ux, vertices[vid].uy);

        vs_out[i].position = mul(pc.vp, world_pos);
    }

    // Same for triangles
    for (uint i = t_idx; i < triangle_count; i += kMeshWorkGroups)
    {
        const uint iid = base_index + i * 3;
        triangles[i] = uint3(
            load_u8(meshlet_data, iid + 0),
            load_u8(meshlet_data, iid + 1),
            load_u8(meshlet_data, iid + 2)
        );

        prims[i].primitive_id = (m_idx << 7) | i;
    }
}
