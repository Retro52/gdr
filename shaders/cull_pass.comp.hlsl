#include "utils/math.hlsl"
#include "utils/gather.hlsl"
#include "include/shaders/types.hlsl.h"

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
    bool inv_visibility_bit = (GET_BIT(mesh_visibility_buffer[idx >> 5], idx & 31u) == 0);
    uint material_class = uint(mesh_materials[mesh_instances[idx].material_index].material_class);

#ifdef FOR_LATE_OCCLUSION_TEST
    const bool early_out = DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 0 && material_class != kMatClassTranslucent;
#else
    bool early_out = (inv_visibility_bit && DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 1) || material_class == kMatClassTranslucent;
#endif

    if (idx >= frame_cull[0].draw_count || early_out)
    {
        return;
    }

    // draw only last frame occluders
    uint mesh_idx = mesh_instances[idx].mesh_data_index;
    float3 center = transform_vec3(float3(meshes_data[mesh_idx].center[0], meshes_data[mesh_idx].center[1], meshes_data[mesh_idx].center[2]), mesh_instances[idx].pos_and_scale, mesh_instances[idx].rotation_quat);
    center = mul(frame_cull[0].view, float4(center, 1.0F)).xyz;

    float scale = mesh_instances[idx].pos_and_scale.w;
    float radius = meshes_data[mesh_idx].radius * scale;

    bool visible = true;
    visible = visible && center.z * frame_cull[0].frustum[1] - abs(center.x) * frame_cull[0].frustum[0] > -radius;
    visible = visible && center.z * frame_cull[0].frustum[3] - abs(center.y) * frame_cull[0].frustum[2] > -radius;
    visible = visible && center.z - radius < -frame_cull[0].frustum[4] && center.z + radius > -frame_cull[0].frustum[5];
    visible = visible || DBG_GET_BIT1(frame_cull[0].flags, kFrustumCullBit) == 0;

#ifdef FOR_LATE_OCCLUSION_TEST
    float4 p_aabb;
    if (visible && DBG_GET_BIT1(frame_cull[0].flags, kOcclusionCullBit) == 1 && project_sphere(center, radius, frame_cull[0].frustum[4], frame_cull[0].p00, frame_cull[0].p11, p_aabb))
    {
        float2 pmd_size = float2(p_aabb.z - p_aabb.x, p_aabb.w - p_aabb.y) * frame_cull[0].pyramid_size;
        float level = ceil(log2(max(pmd_size.x, pmd_size.y)));
        float spr_depth = -frame_cull[0].frustum[4] / (center.z + radius);

#ifdef EMULATE_SAMPLER_REDUCE
        float pmd_depth = textureGatherLodMin(depth_pyramid, depth_pyramid_sampler, (p_aabb.xy + p_aabb.zw) * 0.5, level, 0);
#else
        float pmd_depth = depth_pyramid.SampleLevel(depth_pyramid_sampler, (p_aabb.xy + p_aabb.zw) * 0.5, level).x;
#endif

        visible = visible && spr_depth >= pmd_depth;
    }
#endif

    if (visible)
    {
        uint lod_base = 0;
        float dist = max(length(center) - radius, 0);

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
        InterlockedAdd(draw_commands_count[material_class * 3], task_count, dci);
        dci += pc.offset_table[material_class];

        for (uint i = task_count; i > 0; i--)
        {
            uint dcii = dci + task_count - i;
            uint offset = kTaskWorkGroups * (task_count - i);
            draw_indirect_cmds[dcii].instance_id = idx;
            draw_indirect_cmds[dcii].meshlet_offset = selected_lod.base_meshlet + offset;
            draw_indirect_cmds[dcii].visibility_offset = mesh_instances[idx].visibility_offset + offset;
#ifdef FOR_LATE_OCCLUSION_TEST
            uint drawn_in_early_bit = (!inv_visibility_bit && material_class != kMatClassTranslucent) ? 1 : 0;
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
