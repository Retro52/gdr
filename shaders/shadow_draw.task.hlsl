#include "utils/math.hlsl"
#include "include/shaders/types.hlsl"

// TODO(dx12): binding constants (include/shaders/bindings/shadow_draw.h) can't be pasted into register(), keep in sync
StructuredBuffer<Meshlet>      meshlets:       register(t3);
StructuredBuffer<MeshData>     meshes_data:    register(t5);
StructuredBuffer<MeshInstance> mesh_instances: register(t6);

StructuredBuffer<DrawTaskCommandIndirect> draw_mesh_cmds:  register(t7);
StructuredBuffer<ShadowCascadesData>      shadow_cascades: register(t8);

[[vk::push_constant]] ConstantBuffer<ShadowDrawPushConstants> pc: register(b0);

groupshared uint meshlets_count;
groupshared DrawMeshletsTask meshlet_task;

[shader("amplification")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint instance_id       = draw_mesh_cmds[group_id.x + pc.cmd_offset].instance_id;
    uint meshlets_in_task  = draw_mesh_cmds[group_id.x + pc.cmd_offset].meshlet_count & 0x7FFFFFFFu;
    uint meshlet_id        = draw_mesh_cmds[group_id.x + pc.cmd_offset].meshlet_offset + local_id.x;

    meshlet_task.instance_id = instance_id;
    meshlet_task.base_vertex = meshes_data[mesh_instances[instance_id].mesh_data_index].base_vertex;

    meshlets_count = 0;
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

    if (visible && local_id.x < meshlets_in_task)
    {
        uint idx;
        InterlockedAdd(meshlets_count, 1, idx);
        meshlet_task.meshlet_ids[idx] = meshlet_id;
    }

    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(meshlets_count, 1, 1, meshlet_task);
}
