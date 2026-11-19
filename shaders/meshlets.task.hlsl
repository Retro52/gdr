#include "utils/cull.hlsl"
#include "utils/parsers.hlsl"
#include "include/shaders/types.hlsl.h"
#include "include/shaders/bindings/draw.hlsl"

groupshared uint meshlets_count;
groupshared DrawMeshletsTask meshlet_task;

#ifndef FOR_LATE_OCCLUSION_TEST
groupshared uint needs_late_retest;
#endif

[shader("amplification")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint count = 0;
    uint group_idx = group_id.y * kMaxTaskDispatchDimension + group_id.x;

    if (group_idx < draw_mesh_count[pc.count_offset])
    {
        const bool prev_hizb              = GET_BIT(frame_cull[0].flags, kPrevFrameHiZBBit) == 1;
        const bool meshlet_occlusion_cull = DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 1;

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
        visible = visible && (DBG_GET_BIT1(frame_cull[0].flags, kMeshletFrustumCullBit) == 0
                              || is_within_frustum(sphere.xyz, sphere.w, frame_cull[0].frustum));

        float4 extents;
        const bool is_valid_proj = project_sphere(sphere.xyz, sphere.w, frame_cull[0].frustum[4], frame_cull[0].p00, frame_cull[0].p11, extents);
        if (visible && DBG_GET_BIT1(frame_cull[0].flags, kSmallMeshletsCullBit) == 1 && is_valid_proj)
        {
            // TODO: I think this should be 1/(2^subPixPrecBits) or smth like that
            const float kPrec = 1.0 / 256.0;
            visible = is_over_thresh(extents, frame_cull[0].viewport_size, kPrec);
        }

        const bool in_range = local_id.x < meshlets_in_task;
        bool emit = visible && in_range;

        // Prev-frame HiZB mode: same test in both passes, the late pass repeats it to know what the early one drew
        bool prev_occluded = false;
        if (prev_hizb && visible && meshlet_occlusion_cull)
        {
            prev_occluded = is_meshlet_prev_hizb_occluded(
                meshlets[meshlet_id],
                mesh_instances[instance_id],
                frame_cull[0],
                prev_depth_pyramid,
                prev_depth_pyramid_sampler
            );
        }

#ifdef FOR_LATE_OCCLUSION_TEST
        bool drawn_in_early;
        if (prev_hizb)
        {
            // late list holds new instances (nothing drawn early) and early tasks to retest (bit 31)
            drawn_in_early = visible && !prev_occluded && GET_BIT(draw_mesh_cmds[cmd_idx].meshlet_count, 31) == 1;
        }
        else
        {
            const bool was_visible_last_frame = GET_BIT(meshlets_visibility_buffer[visibility_bit >> 5], visibility_bit & 31) == 1;
            drawn_in_early = was_visible_last_frame && visible && GET_BIT(draw_mesh_cmds[cmd_idx].meshlet_count, 31) == 1;
        }

        if (visible && meshlet_occlusion_cull && is_valid_proj)
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

        emit = emit && visible && (!drawn_in_early || !meshlet_occlusion_cull);
#else
        if (prev_hizb)
        {
            emit = emit && !prev_occluded;
        }
        else
        {
            const bool was_visible_last_frame = GET_BIT(meshlets_visibility_buffer[visibility_bit >> 5], visibility_bit & 31) == 1;
            emit = emit && (was_visible_last_frame || !meshlet_occlusion_cull);
        }
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
        if (!prev_hizb && in_range)
        {
            if (visible)
                InterlockedOr(meshlets_visibility_buffer[visibility_bit >> 5], 1u << (visibility_bit & 31));
            else
                InterlockedAnd(meshlets_visibility_buffer[visibility_bit >> 5], ~(1u << (visibility_bit & 31)));
        }
#else
        if (prev_hizb)
        {
            // Hand the whole task over to the late pass if the previous frame HiZB rejected any of its meshlets
            if (local_id.x == 0)
                needs_late_retest = 0;
            GroupMemoryBarrierWithGroupSync();

            if (WaveActiveAnyTrue(in_range && prev_occluded) && WaveIsFirstLane())
                InterlockedOr(needs_late_retest, 1u);
            GroupMemoryBarrierWithGroupSync();

            if (local_id.x == 0 && needs_late_retest != 0)
            {
                uint late_slot;
                InterlockedAdd(late_draw_count[pc.count_offset], 1, late_slot);

                DrawTaskCommandIndirect late_cmd = draw_mesh_cmds[cmd_idx];
                late_cmd.meshlet_count |= 1u << 31;
                late_draw_cmds[pc.cmd_offset + late_slot] = late_cmd;
            }
        }
#endif
    }

    DispatchMesh(count, 1, 1, meshlet_task);
}
