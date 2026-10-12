#include "utils/math.hlsl"
#include "utils/gather.hlsl"
#include "include/shaders/types.hlsl"

// TODO(dx12): binding constants (include/shaders/bindings/fill.h) can't be pasted into register(), keep these in sync
StructuredBuffer<Meshlet>      meshlets:       register(t0);
ByteAddressBuffer              meshlet_data:   register(t1);  // u32 vertices followed by u8 indices
StructuredBuffer<MeshData>     meshes_data:    register(t2);
StructuredBuffer<MeshInstance> mesh_instances: register(t3);

StructuredBuffer<DrawTaskCommandIndirect> draw_mesh_cmds:  register(t4);
StructuredBuffer<ShadowCascadesData>      shadow_cascades: register(t5);

RWStructuredBuffer<uint>                indices:             register(u6);
RWStructuredBuffer<DrawIndexedIndirect> draw_indirects:      register(u7);
RWStructuredBuffer<uint>                indexed_draw_counts: register(u8);

groupshared uint s_indices_count;
groupshared uint s_indices_offset_global;

struct PushConstants
{
    uint offset;
    uint cascade;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

[shader("compute")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint instance_id      = draw_mesh_cmds[group_id.x + pc.offset].instance_id;
    uint meshlets_in_task = draw_mesh_cmds[group_id.x + pc.offset].meshlet_count & 0x7FFFFFFFu;
    uint meshlet_id       = draw_mesh_cmds[group_id.x + pc.offset].meshlet_offset + local_id.x;
    uint cmd_base_vertex  = meshes_data[mesh_instances[instance_id].mesh_data_index].base_vertex;

    if (local_id.x == 0)
    {
        s_indices_count = 0;
        s_indices_offset_global = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    float3 sphere_center = float3(
        meshlets[meshlet_id].sphere_center[0],
        meshlets[meshlet_id].sphere_center[1],
        meshlets[meshlet_id].sphere_center[2]
    );

    float sphere_radius = float(meshlets[meshlet_id].sphere_radius) * mesh_instances[instance_id].pos_and_scale.w;
    float3 view_center = mul(shadow_cascades[0].view, float4(transform_vec3(sphere_center, mesh_instances[instance_id].pos_and_scale, mesh_instances[instance_id].rotation_quat), 1.0F)).xyz;

    bool visible = true;

#ifdef ENABLE_CONE_CULL
    float3 cull_cone_axis = float3(
        meshlets[meshlet_id].cone_axis[0],
        meshlets[meshlet_id].cone_axis[1],
        meshlets[meshlet_id].cone_axis[2]
    );

    float3 view_axis = mul(shadow_cascades[0].view, float4(quat_rotate_vec3(cull_cone_axis, mesh_instances[instance_id].rotation_quat), 0.0F)).xyz;
    visible = -view_axis.z < float(meshlets[meshlet_id].cone_cutoff);
#endif

    Bounds3D bounds = shadow_cascades[0].cascades[pc.cascade].bounds;
    visible = visible &&
        view_center.x + sphere_radius >= bounds.min.x &&
        view_center.x - sphere_radius <= bounds.max.x &&
        view_center.y + sphere_radius >= bounds.min.y &&
        view_center.y - sphere_radius <= bounds.max.y &&
        view_center.z + sphere_radius >= bounds.min.z;

    if (visible)
    {
        float extent = bounds.max.x - bounds.min.x;
        visible = 2.0F * sphere_radius / extent * shadow_cascades[0].sm_resolution > 1.0F;
    }

    bool emit = visible && local_id.x < meshlets_in_task;

    uint indices_offset_local = 0;
    if (emit)
    {
        InterlockedAdd(s_indices_count, meshlets[meshlet_id].triangles_count() * 3, indices_offset_local);
    }

    GroupMemoryBarrierWithGroupSync();
    if (local_id.x == 0)
    {
        uint indices_offset_global;
        InterlockedAdd(indexed_draw_counts[0], s_indices_count, indices_offset_global);
        s_indices_offset_global = indices_offset_global;

        uint dci;
        InterlockedAdd(indexed_draw_counts[1], 1, dci);

#ifndef PLATFORM_MVK
        draw_indirects[dci].first_instance = 0;
#else
        draw_indirects[dci].first_instance = dci;
#endif
        draw_indirects[dci].instance_count = 1;
        draw_indirects[dci].instance_id = instance_id;
        draw_indirects[dci].vertex_offset = int(cmd_base_vertex);

        draw_indirects[dci].index_count = s_indices_count;
        draw_indirects[dci].first_index = s_indices_offset_global;
    }

    GroupMemoryBarrierWithGroupSync();
    if (emit)
    {
        indices_offset_local += s_indices_offset_global;

        const uint base_vertex = meshlets[meshlet_id].data_offset / 4;
        const uint base_index  = meshlets[meshlet_id].data_offset + meshlets[meshlet_id].vertices_count() * 4;

        for (uint i = 0; i < meshlets[meshlet_id].triangles_count(); ++i)
        {
            const uint store_base = indices_offset_local + i * 3;
            const uint iid = base_index + i * 3;

            indices[store_base + 0] = meshlet_data.Load((base_vertex + load_u8(meshlet_data, iid + 0)) * 4);
            indices[store_base + 1] = meshlet_data.Load((base_vertex + load_u8(meshlet_data, iid + 1)) * 4);
            indices[store_base + 2] = meshlet_data.Load((base_vertex + load_u8(meshlet_data, iid + 2)) * 4);
        }
    }
}
