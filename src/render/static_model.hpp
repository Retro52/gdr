#pragma once

#include <fs/path.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <render/sm_cache.hpp>
#include <shaders/constants.h>

#define SM_USE_MESHLETS 1

class static_model
{
public:
    struct vertex
    {
        vec3 position;
        vec3 normal;
#if 0
        vec2 uv;
        vec3 tangent;
#endif
    };

#if SM_USE_MESHLETS
    constexpr static u32 kMaxVerticesPerMeshlet  = shader_constants::kMaxVerticesPerMeshlet;
    constexpr static u32 kMaxTrianglesPerMeshlet = shader_constants::kMaxTrianglesPerMeshlet;
    constexpr static u32 kMaxIndicesPerMeshlet   = shader_constants::kMaxIndicesPerMeshlet;

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
#endif

    using mesh_data = render::mesh_data<static_model::vertex>;

    struct mesh_buffers
    {
        u64 indices_count {0};
        render::vk_buffer index;
        render::vk_buffer vertex;
    };

public:
    static result<static_model> load_model(const fs::path& path, render::vk_renderer& renderer,
                                           render::vk_scene_geometry_pool& geometry_pool);

    [[nodiscard]] u32 indices_count() const;

    [[nodiscard]] u32 meshlets_count() const;

private:
    struct stats
    {
        u64 meshlets_count;
        u64 vertex_count;
        u64 index_count;
    };

    explicit static_model(const stats& offsets);

private:
    stats m_stats;
};
