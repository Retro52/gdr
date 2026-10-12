#include "utils/math.hlsl"
#include "utils/gather.hlsl"
#include "include/shaders/types.hlsl"

// TODO(dx12): binding constants (include/shaders/bindings/shadow_cull.h) can't be pasted into register(), keep in sync
StructuredBuffer<MeshData>     meshes_data:         register(t0);
StructuredBuffer<MeshInstance> mesh_instances:      register(t1);
StructuredBuffer<MeshMaterial> mesh_materials:      register(t2);
RWStructuredBuffer<uint>       draw_commands_count: register(u3);

StructuredBuffer<FrameCullData>             frame_cull:         register(t4);
StructuredBuffer<ShadowCascadesData>        shadow_cascades:    register(t5);
RWStructuredBuffer<DrawTaskCommandIndirect> draw_indirect_cmds: register(u6);

struct PushConstants
{
    uint cascade;
    // TODO(dx12): cbuffer arrays have a 16-byte element stride, so offset_table[kMatClassCount] is packed into a vector
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

    uint mesh_idx = mesh_instances[idx].mesh_data_index;
    float3 world_center = transform_vec3(float3(meshes_data[mesh_idx].center[0], meshes_data[mesh_idx].center[1], meshes_data[mesh_idx].center[2]), mesh_instances[idx].pos_and_scale, mesh_instances[idx].rotation_quat);

    float3 center = mul(shadow_cascades[0].view, float4(world_center, 1.0F)).xyz;
    float3 camera_center = mul(frame_cull[0].view, float4(world_center, 1.0F)).xyz;

    float scale = mesh_instances[idx].pos_and_scale.w;
    float radius = meshes_data[mesh_idx].radius * scale;

    Bounds3D bounds = shadow_cascades[0].cascades[pc.cascade].bounds;
    bool visible = center.x + radius >= bounds.min.x &&
        center.x - radius <= bounds.max.x &&
        center.y + radius >= bounds.min.y &&
        center.y - radius <= bounds.max.y &&
        center.z + radius >= bounds.min.z;

    if (visible)
    {
        uint lod_base = 0;
        float dist = max(length(camera_center) - radius, 0);

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
        uint material_class = uint(mesh_materials[mesh_instances[idx].material_index].material_class);

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
            draw_indirect_cmds[dcii].meshlet_count = min(selected_lod.meshlets_count - offset, kTaskWorkGroups);
        }
    }
}
