#pragma once

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_renderer.hpp>

#include <stack>

// @imgui
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
    static result<static_model> load_model(render::vk_renderer& renderer, const bytes& data);

    void draw(VkCommandBuffer buffer);

private:
    explicit static_model(const std::vector<mesh_buffers>& meshes);

private:
    std::vector<mesh_buffers> m_meshes;
};
