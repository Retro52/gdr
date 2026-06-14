#pragma once

#include <cpp/hash/crc_hash.hpp>
#include <pod_types.hpp>
#include <render/platform/vk/vk_descriptor_set.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>

#include <unordered_map>

namespace app
{
    enum class pso_id : u32
    {
        imgui_blit       = "imgui_blit"_crc32,
        frustum_debug    = "frustum_debug"_crc32,

        mesh_resolve_pipeline = "mesh_resolve_pipeline"_crc32,
        vert_resolve_pipeline = "vert_resolve_pipeline"_crc32,

        task_cull_pipeline           = "task_cull_pipeline"_crc32,
        task_render_pipeline         = "task_render_pipeline"_crc32,
        task_render_late_pipeline    = "task_render_late_pipeline"_crc32,
        task_occlusion_cull_pipeline = "task_occlusion_cull_pipeline"_crc32,

        indexed_cull_pipeline           = "indexed_cull_pipeline"_crc32,
        indexed_render_pipeline         = "indexed_render_pipeline"_crc32,
        indexed_cull_occlusion_pipeline = "indexed_cull_occlusion_pipeline"_crc32,

        depth_reduce_pipeline = "depth_reduce_pipeline"_crc32,
    };

    struct pso_data
    {
    private:
        std::unordered_map<u32, render::vk_pipeline> m_pipelines;

    public:
        render::vk_pipeline& operator[](const pso_id id) { return m_pipelines[static_cast<u32>(id)]; }

        void load(const render::vk_renderer& renderer, const render::vk_descriptor_set& textures_set);

        void shutdown(const render::vk_renderer& renderer);

        void destroy(const render::vk_renderer& renderer, pso_id id);
    };
}
