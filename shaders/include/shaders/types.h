// ReSharper disable once CppMissingIncludeGuard
#ifdef __cplusplus
#pragma once
#include <cpp/f16.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <pod_types.hpp>
#include <shaders/constants.h>
#else
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#include "constants.h"
#endif

#ifdef __cplusplus
namespace shader_types
{
    using glm::mat4;
    using glm::vec2;

    using uint8_t   = u8;
    using float16_t = cpp::f16;
    using uint      = unsigned int;

    using shader_constants::kLODCount;
    using shader_constants::kMaxVerticesPerMeshlet;

#define QUAT glm::quat
#else
#define QUAT vec4
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
        uint8_t vertices_count;
        uint8_t triangles_count;
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
        vec4 pos_and_scale;  // xyz - position, w - uniform scale
        QUAT rotation_quat;  // quaternion representing object position
        uint material_index;
        uint mesh_data_index;
        uint visibility_offset;
    };

    struct MeshMaterial
    {
        vec4 diffuse_factor;
        vec4 met_roughness_factor;

        uint albedo_idx;
        uint normal_idx;
        uint met_roughness_idx;
        uint material_flags;
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
        DrawPushConstants(const glm::mat4& ivp)
            : vp(ivp)
            , vp_inverse(glm::inverse(ivp))
        {
        }
#endif
        mat4 vp;
        mat4 vp_inverse;
    };

    struct FrameWorldData
    {
        vec4 sun_color;
        vec3 camera_pos;
        uint debug_mode;
        vec3 sun_direction;
        float camera_exposure;
    };

    struct FrameCullData
    {
        mat4 view;
        float frustum[6];  // left/right/top/bottom/znear/zfar
        vec2 pyramid_size;
        vec2 viewport_size;
        float p00;
        float p11;
        float lod_threshold;
        uint draw_count;
        uint flags;
    };

#ifdef __cplusplus
}
#endif
