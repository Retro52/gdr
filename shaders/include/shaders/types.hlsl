#pragma once

#include "constants.hlsl"

struct Vertex
{
    float px, py, pz;
    uint packed_normal;       // 10-10-10-2 bit quantized normal in [-1; 1] range
    uint16_t packed_tangent;  // octahedral encoding
    float16_t ux, uy;
};

struct Meshlet
{
    uint data_offset;  // offset (in bytes) into a shader vertices/indices array
    float16_t cone_axis[3];
    float16_t cone_cutoff;
    float16_t sphere_center[3];
    float16_t sphere_radius;

    uint16_t packed_counts;

    uint vertices_count() { return uint(packed_counts) & 0xFFu; }
    uint triangles_count() { return uint(packed_counts) >> 8u; }
};

struct Bounds3D
{
    float3 min;
    float3 max;
};

struct DrawMeshletsTask
{
    uint instance_id;
    uint base_vertex;
    uint meshlet_ids[kMaxVerticesPerMeshlet];
};

struct MeshLod
{
    uint base_meshlet;
    uint meshlets_count;
    float error;
};

struct MeshData
{
    float center[3];
    float radius;
    uint base_vertex;
    uint lod_count;
    MeshLod lod_array[kLODCount];
};

struct MeshInstance
{
    float4 pos_and_scale;  // xyz - position, w - uniform scale
    float4 rotation_quat;  // quaternion representing object position
    uint material_index;
    uint mesh_data_index;
    uint visibility_offset;
    uint base_vertex;  // mirrors MeshData.base_vertex for locality in resolve pass
};

struct MeshMaterial
{
    float4 diffuse_factor;
    float4 met_roughness_factor;

    uint albedo_idx;
    uint normal_idx;
    uint met_roughness_idx;
    uint16_t material_bits;
    uint16_t material_class;
};

struct DrawIndexedIndirect
{
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
    uint instance_id;
};

struct DrawTaskCommandIndirect
{
    uint instance_id;
    uint meshlet_count;
    uint meshlet_offset;
    uint visibility_offset;
};

struct DrawPushConstants
{
    float4x4 vp;
    uint cmd_offset;
};

struct ShadowDrawPushConstants
{
    float4x4 vp;
    uint cascade;
    uint cmd_offset;
};

struct ResolvePassPushConstants
{
    float4x4 vp;
    float4x4 vp_inverse;
};

// jittered world matrices
struct MotionVectorsData
{
    float4x4 inv_frame_vp;
    float4x4 prev_frame_vp;
};

struct FrameWorldData
{
    float4 sun_color;
    float3 camera_pos;
    uint debug_mode;
    float3 sun_direction;
    float camera_exposure;
    float envmap_scale;
    int2 render_resolution;
};

struct FrameCullData
{
    float4x4 view;
    float frustum[6];  // left/right/top/bottom/znear/zfar
    float2 pyramid_size;
    float2 viewport_size;
    float p00;
    float p11;
    float lod_threshold;
    uint draw_count;
    uint flags;
};

struct CascadeData
{
    float4x4 vp;
    Bounds3D bounds;

    float split;
    float texel_world_size;
};

struct ShadowCascadesData
{
    float4x4 view;
    CascadeData cascades[kMaxShadowCascades];

    float max_range;
    float depth_bias;
    float blend_ratio;
    float normal_offset;
    float sm_resolution;
};
