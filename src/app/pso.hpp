#pragma once

#include <cpp/hash/crc_hash.hpp>
#include <pod_types.hpp>
#include <render/platform/vk/vk_descriptor_set.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>

#include <thread>
#include <unordered_map>

namespace app
{
    enum class pso_id : u32
    {
        blit_sampler2d_pipeline       = "blit_sampler2d_pipeline"_crc32,
        blit_sampler2d_array_pipeline = "blit_sampler2d_array_pipeline"_crc32,

        frustum_debug = "frustum_debug"_crc32,
        fxaa_pipeline = "fxaa_pipeline"_crc32,

        mesh_resolve_pipeline = "mesh_resolve_pipeline"_crc32,
        vert_resolve_pipeline = "vert_resolve_pipeline"_crc32,

        task_cull_pipeline           = "task_cull_pipeline"_crc32,
        task_occlusion_cull_pipeline = "task_occlusion_cull_pipeline"_crc32,

        task_render_pipeline         = "task_render_pipeline"_crc32,
        task_render_late_pipeline    = "task_render_late_pipeline"_crc32,
        task_render_ds_pipeline      = "task_render_ds_pipeline"_crc32,
        task_render_ds_late_pipeline = "task_render_ds_late_pipeline"_crc32,

        indexed_render_pipeline    = "indexed_render_pipeline"_crc32,
        indexed_render_ds_pipeline = "indexed_render_ds_pipeline"_crc32,

        indexed_fill_pipeline         = "indexed_fill_pipeline"_crc32,
        indexed_fill_late_pipeline    = "indexed_fill_late_pipeline"_crc32,
        indexed_fill_ds_pipeline      = "indexed_fill_ds_pipeline"_crc32,
        indexed_fill_ds_late_pipeline = "indexed_fill_ds_late_pipeline"_crc32,

        depth_reduce_pipeline      = "depth_reduce_pipeline"_crc32,
        equirect_unpack_pipeline   = "equirect_unpack_pipeline"_crc32,
        make_brdf_lookup_pipeline  = "make_brdf_lookup_pipeline"_crc32,
        cubemap_convolute_pipeline = "cubemap_convolute_pipeline"_crc32,
        cubemap_prefilter_pipeline = "cubemap_prefilter_pipeline"_crc32,

        shadow_cull            = "shadow_cull"_crc32,
        shadow_fill_ss         = "shadow_fill_ss"_crc32,
        shadow_fill_ds         = "shadow_fill_ds"_crc32,
        shadow_draw_task_ss    = "shadow_draw_task_ss"_crc32,
        shadow_draw_task_ds    = "shadow_draw_task_ds"_crc32,
        shadow_draw_indexed_ss = "shadow_draw_indexed_ss"_crc32,
        shadow_draw_indexed_ds = "shadow_draw_indexed_ds"_crc32,
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

    struct pso_watcher
    {
    private:
        std::thread m_worker;
        std::atomic_bool m_terminate;
        u64 m_last_write_time;

        pso_data& m_pdata;
        render::vk_renderer& m_renderer;

        const render::vk_descriptor_set& m_textures_set;

    public:
        pso_watcher(pso_data& pipelines, render::vk_renderer& renderer,
                    const render::vk_descriptor_set& textures_set);

        void shutdown();
    };
}
