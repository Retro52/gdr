#include "utils/math.hlsl"
#include "include/shaders/types.hlsl.h"
#include "include/shaders/bindings/draw.hlsl"

groupshared uint meshlets_count;
groupshared DrawMeshletsTask meshlet_task;

[shader("amplification")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint instance_id       = draw_mesh_cmds[group_id.x + pc.cmd_offset].instance_id;
    uint meshlets_in_task  = draw_mesh_cmds[group_id.x + pc.cmd_offset].meshlet_count & 0x7FFFFFFFu;
    uint meshlet_id        = draw_mesh_cmds[group_id.x + pc.cmd_offset].meshlet_offset + local_id.x;
    uint visibility_bit    = draw_mesh_cmds[group_id.x + pc.cmd_offset].visibility_offset + local_id.x;

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
    float3 view_center = mul(frame_cull[0].view, float4(transform_vec3(sphere_center, mesh_instances[instance_id].pos_and_scale, mesh_instances[instance_id].rotation_quat), 1.0F)).xyz;

    bool visible = true;
#ifdef ENABLE_CONE_CULL
    if (DBG_GET_BIT1(frame_cull[0].flags, kMeshletConeCullBit) == 1)
    {
        float3 cull_cone_axis = float3(
            meshlets[meshlet_id].cone_axis[0],
            meshlets[meshlet_id].cone_axis[1],
            meshlets[meshlet_id].cone_axis[2]
        );

        float3 view_axis = mul(frame_cull[0].view, float4(quat_rotate_vec3(cull_cone_axis, mesh_instances[instance_id].rotation_quat), 0.0F)).xyz;
        visible = dot(view_center, view_axis) < float(meshlets[meshlet_id].cone_cutoff) * length(view_center) + sphere_radius;
    }
#endif

    if (DBG_GET_BIT1(frame_cull[0].flags, kMeshletFrustumCullBit) == 1)
    {
        visible = visible && view_center.z * frame_cull[0].frustum[1] - abs(view_center.x) * frame_cull[0].frustum[0] > -sphere_radius;
        visible = visible && view_center.z * frame_cull[0].frustum[3] - abs(view_center.y) * frame_cull[0].frustum[2] > -sphere_radius;
        visible = visible && view_center.z - sphere_radius < -frame_cull[0].frustum[4] && view_center.z + sphere_radius > -frame_cull[0].frustum[5];
    }

    float4 p_aabb;
    if (visible && DBG_GET_BIT1(frame_cull[0].flags, kSmallMeshletsCullBit) == 1 && project_sphere(view_center, sphere_radius, frame_cull[0].frustum[4], frame_cull[0].p00, frame_cull[0].p11, p_aabb))
    {
        float2 pmin = p_aabb.xy * frame_cull[0].viewport_size;
        float2 pmax = p_aabb.zw * frame_cull[0].viewport_size;

        // TODO: I think this should be 1/(2^subPixPrecBits) or smth like that
        const float sp_prec = 1.0 / 256.0;
        visible = visible && !(round(pmin.x - sp_prec) == round(pmax.x) || round(pmin.y) == round(pmax.y + sp_prec));
    }

    bool was_visible_last_frame = GET_BIT(meshlets_visibility_buffer[visibility_bit >> 5], visibility_bit & 31) == 1;
    bool emit = visible && local_id.x < meshlets_in_task;

#ifdef FOR_LATE_OCCLUSION_TEST
    bool drawn_in_early = was_visible_last_frame && visible && GET_BIT(draw_mesh_cmds[group_id.x + pc.cmd_offset].meshlet_count, 31) == 1;
    if (visible && DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 1 && project_sphere(view_center, sphere_radius, frame_cull[0].frustum[4], frame_cull[0].p00, frame_cull[0].p11, p_aabb))
    {
        float2 pmd_size = float2(p_aabb.z - p_aabb.x, p_aabb.w - p_aabb.y) * frame_cull[0].pyramid_size;
        float level = ceil(log2(max(pmd_size.x, pmd_size.y)));

        float pmd_depth = depth_pyramid.SampleLevel(depth_pyramid_sampler, (p_aabb.xy + p_aabb.zw) * 0.5, level).x;
        float spr_depth = -frame_cull[0].frustum[4] / (view_center.z + sphere_radius);

        visible = visible && spr_depth >= pmd_depth;
    }

    emit = emit && visible && (!drawn_in_early || DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 0);
#else
    emit = emit && (was_visible_last_frame || DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 0);
#endif

    if (emit)
    {
        uint idx;
        InterlockedAdd(meshlets_count, 1, idx);
        meshlet_task.meshlet_ids[idx] = meshlet_id;
    }

#ifdef FOR_LATE_OCCLUSION_TEST
    if (local_id.x < meshlets_in_task)
    {
        if (visible)
            InterlockedOr(meshlets_visibility_buffer[visibility_bit >> 5], 1u << (visibility_bit & 31));
        else
            InterlockedAnd(meshlets_visibility_buffer[visibility_bit >> 5], ~(1u << (visibility_bit & 31)));
    }
#endif

    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(meshlets_count, 1, 1, meshlet_task);
}
