#pragma once

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

    struct mesh_data
    {
        std::vector<u32> indices;
        std::vector<static_model_vertex> vertices;
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

    void draw(VkCommandBuffer buffer);

private:
    struct offsets
    {
        u64 vertex_offset;
        u64 vertex_count;
        u64 index_offset;
        u64 index_count;
    };

    explicit static_model(const offsets& offsets);

private:
    offsets m_offsets;
};
