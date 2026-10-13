// ReSharper disable once CppMissingIncludeGuard
#ifdef __cplusplus
#pragma once

namespace shader_bindings::draw
{
    using uint = unsigned int;

    const uint kVertexBinding      = 0;
    const uint kMaterialBinding    = 1;
    const uint kMeshletBinding     = 2;
    const uint kMeshletDataBinding = 3;
    const uint kPrimitiveBinding   = 4;
    const uint kInstanceBinding    = 5;
    const uint kMotVecBinding      = 6;
    const uint kTextureBinding     = 7;
    const uint kDrawBinding        = 8;
    const uint kCullBinding        = 9;
    const uint kVisibilityBinding  = 10;
    const uint kHiZBinding         = 11;
}
#endif

#ifndef __cplusplus
#include "include/shaders/types.hlsl"

StructuredBuffer<Vertex>              vertices:                  register(t0);
StructuredBuffer<MeshMaterial>        materials:                 register(t1);
StructuredBuffer<Meshlet>             meshlets:                  register(t2);
ByteAddressBuffer                     meshlet_data:              register(t3);
StructuredBuffer<MeshData>            meshes_data:               register(t4);
StructuredBuffer<MeshInstance>        mesh_instances:            register(t5);
StructuredBuffer<MotionVectorsData>   mv_matrices:               register(t6);
SamplerState                          textures_sampler:          register(s7);
#ifdef FOR_MESH_PIPELINE
StructuredBuffer<DrawTaskCommandIndirect> draw_mesh_cmds:        register(t8);
StructuredBuffer<FrameCullData>           frame_cull:            register(t9);

#ifdef FOR_LATE_OCCLUSION_TEST
RWStructuredBuffer<uint> meshlets_visibility_buffer:             register(u10);
[[vk::combinedImageSampler]] Texture2D    depth_pyramid:         register(t11);
[[vk::combinedImageSampler]] SamplerState depth_pyramid_sampler: register(s11);
#else
StructuredBuffer<uint> meshlets_visibility_buffer:               register(t10);
#endif
#else
StructuredBuffer<DrawIndexedIndirect>     draw_cmds:             register(t8);
#endif

[[vk::push_constant]] ConstantBuffer<DrawPushConstants> pc: register(b0);
#endif

