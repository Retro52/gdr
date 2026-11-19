#include "utils/cull.hlsl"
#include "utils/gather.hlsl"
#include "utils/parsers.hlsl"
#include "include/shaders/types.hlsl.h"

StructuredBuffer<Meshlet>      meshlets:       register(t0);
ByteAddressBuffer              meshlet_data:   register(t1);
StructuredBuffer<MeshData>     meshes_data:    register(t2);
StructuredBuffer<MeshInstance> mesh_instances: register(t3);

StructuredBuffer<DrawTaskCommandIndirect> draw_mesh_cmds:      register(t4);
StructuredBuffer<uint>                    draw_mesh_cmd_count: register(t5);

StructuredBuffer<FrameCullData>           frame_cull:          register(t6);
RWStructuredBuffer<uint>                  indices:             register(u7);
RWStructuredBuffer<DrawIndexedIndirect>   draw_indirects:      register(u8);
RWStructuredBuffer<uint>                  indexed_draw_counts: register(u9);

#ifdef FOR_LATE_OCCLUSION_TEST
RWStructuredBuffer<uint> meshlets_visibility_buffer: register(u10);  // bitfields mode only

[[vk::combinedImageSampler]] Texture2D    depth_pyramid:         register(t11);  // this frame's HiZB
[[vk::combinedImageSampler]] SamplerState depth_pyramid_sampler: register(s11);
#else
StructuredBuffer<uint> meshlets_visibility_buffer: register(t10);  // bitfields mode only

// Prev-frame HiZB mode: tasks with meshlets rejected by the previous frame HiZB, retested in the late pass
RWStructuredBuffer<DrawTaskCommandIndirect> late_draw_cmds:  register(u13);
RWStructuredBuffer<uint>                    late_draw_count: register(u14);

groupshared uint s_needs_late_retest;
#endif

// Prev-frame HiZB mode: early pass tests against it, late pass repeats that test to skip already drawn meshlets
[[vk::combinedImageSampler]] Texture2D    prev_depth_pyramid:         register(t12);
[[vk::combinedImageSampler]] SamplerState prev_depth_pyramid_sampler: register(s12);

static const uint kEmitMaskWords = (kTaskWorkGroups + 31) / 32;
groupshared uint s_emit_mask[kEmitMaskWords];

struct PushConstants
{
    uint cmd_offset;
    uint count_offset;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

[shader("compute")]
[numthreads(kTaskWorkGroups, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint3 local_id: SV_GroupThreadID)
{
    uint group_idx = group_id.y * kMaxTaskDispatchDimension + group_id.x;
    if (group_idx >= draw_mesh_cmd_count[pc.count_offset])
        return;

    const bool prev_hizb              = GET_BIT(frame_cull[0].flags, kPrevFrameHiZBBit) == 1;
    const bool meshlet_occlusion_cull = DBG_GET_BIT1(frame_cull[0].flags, kMeshletOcclusionCullBit) == 1;

    uint cmd_idx = group_idx + pc.cmd_offset;

    uint instance_id      = draw_mesh_cmds[cmd_idx].instance_id;
    uint meshlets_in_task = draw_mesh_cmds[cmd_idx].meshlet_count & 0x7FFFFFFFu;
    uint meshlet_id       = draw_mesh_cmds[cmd_idx].meshlet_offset + local_id.x;
    uint visibility_bit   = draw_mesh_cmds[cmd_idx].visibility_offset + local_id.x;
    uint cmd_base_vertex = meshes_data[mesh_instances[instance_id].mesh_data_index].base_vertex;

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

    if (!prev_hizb && in_range)
    {
        if (visible)
            InterlockedOr(meshlets_visibility_buffer[visibility_bit >> 5], 1u << (visibility_bit & 31));
        else
            InterlockedAnd(meshlets_visibility_buffer[visibility_bit >> 5], ~(1u << (visibility_bit & 31)));
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

    const uint t = local_id.x;

    if (t < kEmitMaskWords)
        s_emit_mask[t] = 0;
#ifndef FOR_LATE_OCCLUSION_TEST
    if (t == 0)
        s_needs_late_retest = 0;
#endif
    GroupMemoryBarrierWithGroupSync();

    if (emit)
        InterlockedOr(s_emit_mask[t >> 5], 1u << (t & 31));
#ifndef FOR_LATE_OCCLUSION_TEST
    // Hand the whole task over to the late pass if the previous frame HiZB rejected any of its meshlets
    if (WaveActiveAnyTrue(in_range && prev_occluded) && WaveIsFirstLane())
        InterlockedOr(s_needs_late_retest, 1u);
#endif
    GroupMemoryBarrierWithGroupSync();

#ifndef FOR_LATE_OCCLUSION_TEST
    if (t == 0 && s_needs_late_retest != 0)
    {
        uint late_slot;
        InterlockedAdd(late_draw_count[pc.count_offset], 1, late_slot);

        DrawTaskCommandIndirect late_cmd = draw_mesh_cmds[cmd_idx];
        late_cmd.meshlet_count |= 1u << 31;
        late_draw_cmds[pc.cmd_offset + late_slot] = late_cmd;
    }
#endif

    const bool prev_emit = t > 0 && ((s_emit_mask[(t - 1) >> 5] >> ((t - 1) & 31)) & 1u);
    const bool is_start  = emit && !prev_emit;

    const uint run_count = WaveActiveCountBits(is_start);
    const uint run_slot  = WavePrefixCountBits(is_start);

    uint base_dci = 0;
    if (WaveIsFirstLane() && run_count > 0)
        InterlockedAdd(indexed_draw_counts[1], run_count, base_dci);

    base_dci = WaveReadLaneFirst(base_dci);

    if (is_start)
    {
        uint w     = t >> 5;
        uint zeros = ~s_emit_mask[w] & ~((2u << (t & 31)) - 1u);   // t & 31 == 31 -> zeros == 0
        while (zeros == 0 && w + 1 < kEmitMaskWords)
        {
            ++w;
            zeros = ~s_emit_mask[w];
        }
        const uint last = (zeros != 0 ? w * 32 + firstbitlow(zeros) : kEmitMaskWords * 32) - 1;

        const uint last_meshlet = meshlet_id + (last - t);
        const uint first_index  = meshlets[meshlet_id].index_offset;
        const uint end_index    = meshlets[last_meshlet].index_offset + meshlets[last_meshlet].triangles_count() * 3;

        const uint dci = base_dci + run_slot;

#ifdef EMULATE_DRAW_ID
        draw_indirects[dci].first_instance = dci;
#else
        draw_indirects[dci].first_instance = 0;
#endif
        draw_indirects[dci].instance_count = 1;
        draw_indirects[dci].instance_id    = instance_id;
        draw_indirects[dci].vertex_offset  = int(cmd_base_vertex);

        draw_indirects[dci].first_index = first_index;
        draw_indirects[dci].index_count = end_index - first_index;
    }
}
