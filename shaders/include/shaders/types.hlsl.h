#pragma once

#ifdef __cplusplus
#pragma once
#include <cpp/f16.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <pod_types.hpp>
#include <shaders/constants.hlsl.h>

#include <limits>
#else
#include "constants.hlsl.h"
#endif

#ifdef __cplusplus
namespace shader_types
{
    using glm::mat4;
    using glm::vec2;
    using glm::vec3;

    using uint     = u32;
    using uint16_t = u16;

    using int2 = ivec2;
    using int3 = ivec3;
    using int4 = ivec4;

    using float2 = vec2;
    using float3 = vec3;
    using float4 = vec4;

    using uint2 = uvec2;
    using uint3 = uvec3;
    using uint4 = uvec4;

    using float2x2 = glm::mat2;
    using float3x3 = glm::mat3;
    using float4x4 = glm::mat4;

    using float16_t = cpp::f16;

    using shader_constants::kLODCount;
    using shader_constants::kMatClassCount;
    using shader_constants::kMaxVerticesPerMeshlet;

#define QUAT glm::quat
#else
#define QUAT float4
#endif

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

#ifdef __cplusplus
        [[nodiscard]] uint vertices_count() const { return static_cast<uint>(packed_counts) & 0xFFu; }

        void set_vertices_count(const uint count) { packed_counts = (packed_counts & ~0xFFu) | count & 0xFFu; }

        [[nodiscard]] uint triangles_count() const { return static_cast<uint>(packed_counts) >> 8u; }

        void set_triangles_count(const uint count) { packed_counts = count << 8u | vertices_count(); }
#else
    uint vertices_count() { return uint(packed_counts) & 0xFFu; }

    uint triangles_count() { return uint(packed_counts) >> 8u; }
#endif
    };

    struct Bounds3D
    {
#ifdef __cplusplus
        Bounds3D()
            : min {std::numeric_limits<f32>::max()}
            , max {std::numeric_limits<f32>::lowest()}
        {
        }
#endif

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
        QUAT rotation_quat;    // quaternion representing object position
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
#ifdef __cplusplus
        DrawPushConstants() = default;

        DrawPushConstants(const glm::mat4& ivp, const uint ico)
            : vp(ivp)
            , cmd_offset(ico)
        {
        }
#endif

        float4x4 vp;
        uint cmd_offset;
    };

    struct ShadowDrawPushConstants
    {
#ifdef __cplusplus
        ShadowDrawPushConstants() = default;

        ShadowDrawPushConstants(const glm::mat4& ivp, const uint ico, const uint ic)
            : vp(ivp)
            , cascade(ic)
            , cmd_offset(ico)
        {
        }
#endif

        float4x4 vp;
        uint cascade;
        uint cmd_offset;
    };

    struct ResolvePassPushConstants
    {
#ifdef __cplusplus
        ResolvePassPushConstants(glm::mat4 view, const glm::mat4& proj)
        {
            view[3]    = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);
            vp         = proj * view;
            vp_inverse = glm::inverse(vp);
        }
#endif

        float4x4 vp;
        float4x4 vp_inverse;
    };

    // jittered world matrices
    struct MotionVectorsData
    {
        float4x4 inv_frame_vp;
        float4x4 this_frame_vp;
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

#if 0
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
#endif

#ifdef __cplusplus
}
#endif
