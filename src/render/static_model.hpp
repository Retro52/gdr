#pragma once

#define SM_USE_MESHLETS 1

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>
#include <render/platform/vk/vk_renderer.hpp>

#include <stack>

class static_model
{
public:
    using static_model_index = u32;

    struct static_model_vertex
    {
        vec3 position;
        vec3 normal;
        vec2 uv;
        vec3 tangent;
    };

#if SM_USE_MESHLETS
    constexpr static u32 kMaxVerticesPerMeshlet  = 64;
    constexpr static u32 kMaxTrianglesPerMeshlet = 42;
    constexpr static u32 kMaxIndicesPerMeshlet   = kMaxTrianglesPerMeshlet * 3;

    struct static_model_meshlet
    {
        u32 vertices[kMaxVerticesPerMeshlet];
        u8 indices[kMaxIndicesPerMeshlet];
        u8 vertices_count;
        u8 triangles_count;
        // f32 cull_cone[4];  // xyz - axis, w - angle
    };
#endif

    struct mesh_data
    {
#if SM_USE_MESHLETS
        std::vector<static_model_vertex> vertices;
        std::vector<static_model_meshlet> meshlets;
#else
        std::vector<u32> indices;
        std::vector<static_model_vertex> vertices;
#endif
    };

    struct mesh_buffers
    {
        u64 indices_count {0};
        render::vk_buffer index;
        render::vk_buffer vertex;
    };

public:
    static result<static_model> load_model(const bytes& data, render::vk_renderer& renderer,
                                           render::vk_scene_geometry_pool& geometry_pool);

    void draw(VkCommandBuffer buffer) const;

    [[nodiscard]] u32 indices_count() const;

private:
    struct offsets
    {
        u64 meshlets_count;
        u64 vertex_offset;
        u64 vertex_count;
        u64 index_offset;
        u64 index_count;
    };

    explicit static_model(const offsets& offsets);

private:
    offsets m_offsets;
};
