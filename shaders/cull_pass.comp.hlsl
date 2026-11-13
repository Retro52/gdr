#include "utils/cull.hlsl"
#include "utils/parsers.hlsl"
#include "include/shaders/types.hlsl.h"

#define TRANSPARENCIES_BUCKET_ONLY 0

StructuredBuffer<MeshData>     meshes_data:         register(t0);
StructuredBuffer<MeshInstance> mesh_instances:      register(t1);
StructuredBuffer<MeshMaterial> mesh_materials:      register(t2);
RWStructuredBuffer<uint>       draw_commands_count: register(u3);

#ifdef FOR_LATE_OCCLUSION_TEST
RWStructuredBuffer<uint> mesh_visibility_buffer: register(u4);
#else
StructuredBuffer<uint> mesh_visibility_buffer: register(t4);
#endif

StructuredBuffer<FrameCullData>             frame_cull:         register(t5);
RWStructuredBuffer<DrawTaskCommandIndirect> draw_indirect_cmds: register(u6);

#ifdef FOR_LATE_OCCLUSION_TEST
[[vk::combinedImageSampler]] Texture2D    depth_pyramid:         register(t7);
[[vk::combinedImageSampler]] SamplerState depth_pyramid_sampler: register(s7);
#endif

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

    bool inv_visibility_bit = (GET_BIT(mesh_visibility_buffer[idx >> 5], idx & 31u) == 0);
    uint material_class = uint(mesh_materials[mesh_instances[idx].material_index].material_class);

#if TRANSPARENCIES_BUCKET_ONLY
    const bool bucket_only = material_class == kMatClassTranslucent;
#else
    const bool bucket_only = false;
#endif

#ifdef FOR_LATE_OCCLUSION_TEST
    const bool early_out = DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 0 && !bucket_only;
#else
    bool early_out = (inv_visibility_bit && DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 1) || bucket_only;
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

    // draw only last frame occluders
#ifdef FOR_LATE_OCCLUSION_TEST
    if (visible && DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 1)
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

#ifdef FOR_LATE_OCCLUSION_TEST
    if (visible)
        InterlockedOr(mesh_visibility_buffer[idx >> 5], 1u << (idx & 31u));
    else
        InterlockedAnd(mesh_visibility_buffer[idx >> 5], ~(1u << (idx & 31u)));
#endif
}
