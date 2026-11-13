#include "utils/cull.hlsl"
#include "utils/parsers.hlsl"
#include "include/shaders/types.hlsl.h"
#include "include/shaders/bindings/draw.hlsl"

groupshared uint meshlets_count;
groupshared DrawMeshletsTask meshlet_task;

[shader("amplification")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint count = 0;
    uint group_idx = group_id.y * kMaxTaskDispatchDimension + group_id.x;

    if (group_idx < draw_mesh_count[pc.count_offset])
    {
        uint cmd_idx = group_idx + pc.cmd_offset;
        uint instance_id       = draw_mesh_cmds[cmd_idx].instance_id;
        uint meshlets_in_task  = draw_mesh_cmds[cmd_idx].meshlet_count & 0x7FFFFFFFu;
        uint meshlet_id        = draw_mesh_cmds[cmd_idx].meshlet_offset + local_id.x;
        uint visibility_bit    = draw_mesh_cmds[cmd_idx].visibility_offset + local_id.x;

        meshlet_task.instance_id = instance_id;
        meshlet_task.base_vertex = meshes_data[mesh_instances[instance_id].mesh_data_index].base_vertex;

        float4 cone;
        float4 sphere;
        unpack_meshlet_data(
            meshlets[meshlet_id],
            frame_cull[0].view,
            mesh_instances[instance_id].pos_and_scale,
            mesh_instances[instance_id].rotation_quat,
            sphere,
            cone
        );

        bool visible = true;
#ifdef ENABLE_CONE_CULL
        visible = DBG_GET_BIT1(frame_cull[0].flags, kMeshletConeCullBit) == 0
                  || is_cone_aligned(sphere.xyz, sphere.w, cone.xyz, cone.w);
#endif
        visible = DBG_GET_BIT1(frame_cull[0].flags, kMeshletFrustumCullBit) == 0
                  || is_within_frustum(sphere.xyz, sphere.w, frame_cull[0].frustum);

        float4 extents;
        const bool is_valid_proj = project_sphere(sphere.xyz, sphere.w, frame_cull[0].frustum[4], frame_cull[0].p00, frame_cull[0].p11, extents);
        if (visible && DBG_GET_BIT1(frame_cull[0].flags, kSmallMeshletsCullBit) == 1 && is_valid_proj)
        {
            // TODO: I think this should be 1/(2^subPixPrecBits) or smth like that
            const float kPrec = 1.0 / 256.0;
            visible = is_over_thresh(extents, frame_cull[0].viewport_size, kPrec);
        }

        bool was_visible_last_frame = GET_BIT(meshlets_visibility_buffer[visibility_bit >> 5], visibility_bit & 31) == 1;
        bool emit = visible && local_id.x < meshlets_in_task;

#ifdef FOR_LATE_OCCLUSION_TEST
        bool drawn_in_early = was_visible_last_frame && visible && GET_BIT(draw_mesh_cmds[cmd_idx].meshlet_count, 31) == 1;
        if (visible && DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 1 && is_valid_proj)
        {
            visible = !is_hizb_occluded(
                          extents,
                          sphere.xyz,
                          sphere.w,
                          frame_cull[0].frustum[4],
                          frame_cull[0].pyramid_size,
                          depth_pyramid,
                          depth_pyramid_sampler
                      );
        }

        emit = emit && visible && (!drawn_in_early || DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 0);
#else
        emit = emit && (was_visible_last_frame || DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 0);
#endif

        uint slot  = 0;
        // That yields a bit better performance on some GPUs,
        // and also serves a workaround for an AMD-specific default shader compiler synchronization bug
        if (WaveGetLaneCount() == kTaskWorkGroups)
        {
            slot  = WavePrefixCountBits(emit);
            count = WaveActiveCountBits(emit);
        }
        else
        {
            if (local_id.x == 0)
                meshlets_count = 0;
            GroupMemoryBarrierWithGroupSync();

            if (emit)
                InterlockedAdd(meshlets_count, 1, slot);
            GroupMemoryBarrierWithGroupSync();

            count = meshlets_count;
        }

        if (emit)
            meshlet_task.meshlet_ids[slot] = meshlet_id;

#ifdef FOR_LATE_OCCLUSION_TEST
        if (local_id.x < meshlets_in_task)
        {
            if (visible)
                InterlockedOr(meshlets_visibility_buffer[visibility_bit >> 5], 1u << (visibility_bit & 31));
            else
                InterlockedAnd(meshlets_visibility_buffer[visibility_bit >> 5], ~(1u << (visibility_bit & 31)));
        }
#endif
    }

    DispatchMesh(count, 1, 1, meshlet_task);
}
