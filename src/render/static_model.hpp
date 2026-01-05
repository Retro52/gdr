#pragma once

#include <fs/path.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <render/sm_cache.hpp>
#include <shaders/constants.h>

#define SM_USE_TS       USE_TASK_SHADER
#define SM_USE_MESHLETS 1

class static_model
{
public:
    struct static_model_vertex
    {
        vec3 position;
        vec3 normal;
        vec2 uv;
        vec3 tangent;
    };

#if SM_USE_MESHLETS
    constexpr static u32 kMaxVerticesPerMeshlet  = shader_constants::kMaxVerticesPerMeshlet;
    constexpr static u32 kMaxTrianglesPerMeshlet = shader_constants::kMaxTrianglesPerMeshlet;
    constexpr static u32 kMaxIndicesPerMeshlet   = shader_constants::kMaxIndicesPerMeshlet;

    struct static_model_meshlet
    {
        u32 vertices[kMaxVerticesPerMeshlet];
        u8 indices[kMaxIndicesPerMeshlet];
        u8 vertices_count;
        u8 triangles_count;
        f32 cull_cone[4];        // xyz - direction; w - alpha encoded in -127 to +127 range
        f32 bounding_sphere[4];  // xyz - center, w - radius
    };
#endif

    using mesh_data = render::mesh_data<static_model::static_model_vertex>;

    struct mesh_buffers
    {
        u64 indices_count {0};
        render::vk_buffer index;
        render::vk_buffer vertex;
    };

public:
    static result<static_model> load_model(const fs::path& path, render::vk_renderer& renderer,
                                           render::vk_scene_geometry_pool& geometry_pool);

    void draw(VkCommandBuffer buffer) const;

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
