#include "utils/math.hlsl"
#include "utils/gather.hlsl"
#include "include/shaders/types.hlsl.h"

StructuredBuffer<Meshlet>      meshlets:       register(t0);
ByteAddressBuffer              meshlet_data:   register(t1);
StructuredBuffer<MeshData>     meshes_data:    register(t2);
StructuredBuffer<MeshInstance> mesh_instances: register(t3);

StructuredBuffer<DrawTaskCommandIndirect> draw_mesh_cmds: register(t4);
StructuredBuffer<FrameCullData>           frame_cull:     register(t5);

RWStructuredBuffer<uint>                indices:             register(u6);
RWStructuredBuffer<DrawIndexedIndirect> draw_indirects:      register(u7);
RWStructuredBuffer<uint>                indexed_draw_counts: register(u8);

#ifdef FOR_LATE_OCCLUSION_TEST
RWStructuredBuffer<uint> meshlets_visibility_buffer: register(u9);

[[vk::combinedImageSampler]] Texture2D    depth_pyramid:         register(t10);
[[vk::combinedImageSampler]] SamplerState depth_pyramid_sampler: register(s10);
#else
StructuredBuffer<uint> meshlets_visibility_buffer: register(t9);
#endif

groupshared uint s_indices_count;
groupshared uint s_indices_offset_global;

struct PushConstants
{
    uint offset;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

[shader("compute")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint instance_id      = draw_mesh_cmds[group_id.x + pc.offset].instance_id;
    uint meshlets_in_task = draw_mesh_cmds[group_id.x + pc.offset].meshlet_count & 0x7FFFFFFFu;
    uint meshlet_id       = draw_mesh_cmds[group_id.x + pc.offset].meshlet_offset + local_id.x;
    uint visibility_bit   = draw_mesh_cmds[group_id.x + pc.offset].visibility_offset + local_id.x;
    uint cmd_base_vertex = meshes_data[mesh_instances[instance_id].mesh_data_index].base_vertex;

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
    bool drawn_in_early = was_visible_last_frame && visible && GET_BIT(draw_mesh_cmds[group_id.x + pc.offset].meshlet_count, 31) == 1;
    if (visible && DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 1 && project_sphere(view_center, sphere_radius, frame_cull[0].frustum[4], frame_cull[0].p00, frame_cull[0].p11, p_aabb))
    {
        float2 pmd_size = float2(p_aabb.z - p_aabb.x, p_aabb.w - p_aabb.y) * frame_cull[0].pyramid_size;
        float level = ceil(log2(max(pmd_size.x, pmd_size.y)));
        float spr_depth = -frame_cull[0].frustum[4] / (view_center.z + sphere_radius);

#ifdef EMULATE_SAMPLER_REDUCE
        float pmd_depth = textureGatherLodMin(depth_pyramid, depth_pyramid_sampler, (p_aabb.xy + p_aabb.zw) * 0.5, level, 0);
#else
        float pmd_depth = textureGatherLodMin(depth_pyramid, depth_pyramid_sampler, (p_aabb.xy + p_aabb.zw) * 0.5, level, 0);
#endif

        visible = visible && spr_depth >= pmd_depth;
    }

    if (local_id.x < meshlets_in_task)
    {
        if (visible)
            InterlockedOr(meshlets_visibility_buffer[visibility_bit >> 5], 1u << (visibility_bit & 31));
        else
            InterlockedAnd(meshlets_visibility_buffer[visibility_bit >> 5], ~(1u << (visibility_bit & 31)));
    }

    emit = emit && visible && (!drawn_in_early || DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 0);
#else
    emit = emit && (was_visible_last_frame || DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 0);
#endif

    uint indices_offset_local = 0;
    if (emit)
    {
        InterlockedAdd(s_indices_count, meshlets[meshlet_id].triangles_count() * 3, indices_offset_local);
    }

    GroupMemoryBarrierWithGroupSync();
    if (s_indices_count == 0)
        return;

    if (local_id.x == 0)
    {
        uint indices_offset_global;
        InterlockedAdd(indexed_draw_counts[0], s_indices_count, indices_offset_global);
        s_indices_offset_global = indices_offset_global;

        uint dci;
        InterlockedAdd(indexed_draw_counts[1], 1, dci);

#ifdef EMULATE_DRAW_ID
        draw_indirects[dci].first_instance = dci;
#else
        draw_indirects[dci].first_instance = 0;
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
