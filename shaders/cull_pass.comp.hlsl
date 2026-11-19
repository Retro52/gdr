#include "utils/cull.hlsl"
#include "utils/parsers.hlsl"
#include "include/shaders/types.hlsl.h"

#define TRANSPARENCIES_BUCKET_ONLY 0

StructuredBuffer<MeshData>     meshes_data:         register(t0);
StructuredBuffer<MeshInstance> mesh_instances:      register(t1);
StructuredBuffer<MeshMaterial> mesh_materials:      register(t2);
RWStructuredBuffer<uint>       draw_commands_count: register(u3);

// Bitfields mode: visible last frame, written by the late pass. Prev-frame HiZB mode: drawn early, written by the early pass
RWStructuredBuffer<uint> mesh_visibility_buffer: register(u4);

StructuredBuffer<FrameCullData> frame_cull: register(t5);

// Prev-frame HiZB mode, late pass: appends to the list the early task / mesh_to_indexed passes started
RWStructuredBuffer<DrawTaskCommandIndirect> draw_indirect_cmds: register(u6);

// Early pass: previous frame HiZB (prev-frame HiZB mode only). Late pass: this frame's HiZB
[[vk::combinedImageSampler]] Texture2D    depth_pyramid:         register(t7);
[[vk::combinedImageSampler]] SamplerState depth_pyramid_sampler: register(s7);

struct PushConstants
{
    uint3 offset_table;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    uint idx = global_id.x;
    if (idx >= frame_cull[0].draw_count)
    {
        return;
    }

    const bool prev_hizb      = GET_BIT(frame_cull[0].flags, kPrevFrameHiZBBit) == 1;
    const bool occlusion_cull = DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 1;

    bool inv_visibility_bit = (GET_BIT(mesh_visibility_buffer[idx >> 5], idx & 31u) == 0);
    uint material_class = uint(mesh_materials[mesh_instances[idx].material_index].material_class);

#if TRANSPARENCIES_BUCKET_ONLY
    const bool bucket_only = material_class == kMatClassTranslucent;
#else
    const bool bucket_only = false;
#endif

#ifdef FOR_LATE_OCCLUSION_TEST
    // Prev-frame HiZB mode: instances drawn early are done, their tasks with rejected meshlets are already
    // in the late list. Not drawn ones were either frustum culled or rejected by the previous frame HiZB.
    const bool early_out = (!occlusion_cull || (prev_hizb && !inv_visibility_bit)) && !bucket_only;
#else
    bool early_out = (!prev_hizb && inv_visibility_bit && occlusion_cull) || bucket_only;
#endif

    if (early_out)
    {
        return;
    }

    uint mesh_idx = mesh_instances[idx].mesh_data_index;

    float4 sphere;
    unpack_instance_data(
        meshes_data[mesh_idx],
        frame_cull[0].view,
        mesh_instances[idx].pos_and_scale,
        mesh_instances[idx].rotation_quat,
        sphere
    );

    bool visible = DBG_GET_BIT1(frame_cull[0].flags, kFrustumCullBit) == 0
                   || is_within_frustum(sphere.xyz, sphere.w, frame_cull[0].frustum);

#ifdef FOR_LATE_OCCLUSION_TEST
    if (visible && occlusion_cull)
    {
        visible = !is_hizb_occluded2(
                sphere.xyz,
                sphere.w,
                frame_cull[0].frustum[4],
                frame_cull[0].p00,
                frame_cull[0].p11,
                frame_cull[0].pyramid_size,
                depth_pyramid,
                depth_pyramid_sampler
        );
    }
#else
    // Test against the previous frame HiZB, reprojected with the view & projection it was built with
    if (prev_hizb && visible && occlusion_cull)
    {
        float4 prev_sphere;
        unpack_instance_data(
            meshes_data[mesh_idx],
            frame_cull[0].prev_view,
            mesh_instances[idx].pos_and_scale,
            mesh_instances[idx].rotation_quat,
            prev_sphere
        );

        visible = !is_hizb_occluded2(
                prev_sphere.xyz,
                prev_sphere.w,
                frame_cull[0].prev_znear,
                frame_cull[0].prev_p00,
                frame_cull[0].prev_p11,
                frame_cull[0].pyramid_size,
                depth_pyramid,
                depth_pyramid_sampler
        );
    }
#endif

    if (visible)
    {
        uint lod_base = 0;
        float scale = mesh_instances[idx].pos_and_scale.w;
        float dist = max(length(sphere.xyz) - sphere.w, 0);

        for (uint i = 1; i < meshes_data[mesh_idx].lod_count; ++i)
        {
            if ((meshes_data[mesh_idx].lod_array[i].error * scale / dist) <= frame_cull[0].lod_threshold)
                lod_base = i;
            else
                break;
        }

        lod_base = DBG_GET_BIT1(frame_cull[0].flags, kLodFlagBit) == 0 ? 0 : lod_base;
        MeshLod selected_lod = meshes_data[mesh_idx].lod_array[lod_base];

        uint task_count = (selected_lod.meshlets_count + kTaskWorkGroups - 1) / kTaskWorkGroups;

        uint dci;
        InterlockedAdd(draw_commands_count[material_class], task_count, dci);
        dci += pc.offset_table[material_class];

        for (uint i = task_count; i > 0; i--)
        {
            uint dcii = dci + task_count - i;
            uint offset = kTaskWorkGroups * (task_count - i);
            draw_indirect_cmds[dcii].instance_id = idx;
            draw_indirect_cmds[dcii].meshlet_offset = selected_lod.base_meshlet + offset;
            draw_indirect_cmds[dcii].visibility_offset = mesh_instances[idx].visibility_offset + offset;
#ifdef FOR_LATE_OCCLUSION_TEST
            uint drawn_in_early_bit = (!inv_visibility_bit && !bucket_only) ? 1 : 0;
            draw_indirect_cmds[dcii].meshlet_count = min(selected_lod.meshlets_count - offset, kTaskWorkGroups) | drawn_in_early_bit << 31;
#else
            draw_indirect_cmds[dcii].meshlet_count = min(selected_lod.meshlets_count - offset, kTaskWorkGroups);
#endif
        }
    }

    const uint bit = 1u << (idx & 31u);
#ifdef FOR_LATE_OCCLUSION_TEST
    if (!prev_hizb)
    {
        if (visible)
            InterlockedOr(mesh_visibility_buffer[idx >> 5], bit);
        else
            InterlockedAnd(mesh_visibility_buffer[idx >> 5], ~bit);
    }
#else
    if (prev_hizb)
    {
        if (visible)
            InterlockedOr(mesh_visibility_buffer[idx >> 5], bit);
        else
            InterlockedAnd(mesh_visibility_buffer[idx >> 5], ~bit);
    }
#endif
}
