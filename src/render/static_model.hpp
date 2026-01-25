#pragma once

#include <fs/path.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <render/sm_cache.hpp>
#include <shaders/constants.h>

struct static_model
{
    vec4 b_sphere;
    u32 base_vertex;
    u32 base_meshlet;    // Only used for meshlets path
    u32 meshlets_count;  // Only used for meshlets path
    u32 base_index;      // Only used for non-meshlets path
    u32 indices_count;   // Only used for non-meshlets path

    constexpr static u32 kMaxIndicesPerMeshlet   = shader_constants::kMaxIndicesPerMeshlet;
    constexpr static u32 kMaxVerticesPerMeshlet  = shader_constants::kMaxVerticesPerMeshlet;
    constexpr static u32 kMaxTrianglesPerMeshlet = shader_constants::kMaxTrianglesPerMeshlet;

    struct meshlet
    {
        u32 payload_offset;  // offset to the meshlet payload in a shared array
        f32 cone_axis[3];
        f32 cone_cutoff;
        f32 sphere_center[3];
        f32 sphere_radius;
        u8 vertices_count;
        u8 triangles_count;
    };

    struct vertex
    {
        vec3 position;
        vec3 normal;
#if 0
        vec2 uv;
        vec3 tangent;
#endif
    };

    using mesh_data = render::mesh_data<vertex>;
    static result<std::vector<static_model>> load(const fs::path& path, render::vk_scene_geometry_pool& geometry_pool);
};
