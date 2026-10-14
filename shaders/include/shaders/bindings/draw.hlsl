#pragma once

#ifdef __cplusplus
namespace shader_bindings::draw
{
    using uint = unsigned int;

    const uint kVertexBinding      = 0;
    const uint kMaterialBinding    = 1;
    const uint kMeshletBinding     = 2;
    const uint kMeshletDataBinding = 3;
    const uint kPrimitiveBinding   = 4;
    const uint kInstanceBinding    = 5;
    const uint kTextureBinding     = 6;
    const uint kDrawBinding        = 7;
    const uint kCullBinding        = 8;
    const uint kVisibilityBinding  = 9;
    const uint kHiZBinding         = 10;
}
#endif

#ifndef __cplusplus
#include "include/shaders/types.hlsl.h"

StructuredBuffer<Vertex>              vertices:                  register(t0);
StructuredBuffer<MeshMaterial>        materials:                 register(t1);
StructuredBuffer<Meshlet>             meshlets:                  register(t2);
ByteAddressBuffer                     meshlet_data:              register(t3);
StructuredBuffer<MeshData>            meshes_data:               register(t4);
StructuredBuffer<MeshInstance>        mesh_instances:            register(t5);
SamplerState                          textures_sampler:          register(s6);
#ifdef FOR_MESH_PIPELINE
StructuredBuffer<DrawTaskCommandIndirect> draw_mesh_cmds:        register(t7);
StructuredBuffer<FrameCullData>           frame_cull:            register(t8);

#ifdef FOR_LATE_OCCLUSION_TEST
RWStructuredBuffer<uint> meshlets_visibility_buffer:             register(u9);
[[vk::combinedImageSampler]] Texture2D    depth_pyramid:         register(t10);
[[vk::combinedImageSampler]] SamplerState depth_pyramid_sampler: register(s10);
#else
StructuredBuffer<uint> meshlets_visibility_buffer:               register(t9);
#endif
#else
StructuredBuffer<DrawIndexedIndirect>     draw_cmds:             register(t7);
#endif

[[vk::push_constant]] ConstantBuffer<DrawPushConstants> pc: register(b0);
#endif

