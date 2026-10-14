#pragma once

#include <app/pso.hpp>
#include <app/render.hpp>
#include <passes/common_resources.hpp>
#include <render/rhi.hpp>
#include <scene/loader.hpp>

namespace app
{
    // clang-format off
    REGISTER_FLAGS(gpass_flags,
        none             = 0,
        cull             = 1 << 0,
        hizb_cull        = 1 << 1,
        build_hizb       = 1 << 2,
        use_mesh_shaders = 1 << 3,

        any_cull = hizb_cull | cull,

        post_hizb_indexed = hizb_cull,
        pre_hizb_indexed  = build_hizb | cull,

        post_hizb_mesh = use_mesh_shaders | hizb_cull,
        pre_hizb_mesh  = use_mesh_shaders | build_hizb | cull
    );
    // clang-format on

    struct pass_geometry
    {
        struct draw_params
        {
            glm::mat4 view;
            glm::mat4 projection;

            gpass_flags_bits flags = gpass_flags::none;
        };

        rhi::image depth    = rhi::null_image;
        rhi::image v_buffer = rhi::null_image;
        depth_pyramid_data hizb;

        rhi::buffer mesh_draw_count      = rhi::null_buffer;
        rhi::buffer indexed_count_buffer = rhi::null_buffer;

        rhi::buffer mesh_visibility     = rhi::null_buffer;
        rhi::buffer meshlets_visibility = rhi::null_buffer;

        rhi::buffer indices               = rhi::null_buffer;
        rhi::buffer mesh_draw_commands    = rhi::null_buffer;
        rhi::buffer indexed_draw_commands = rhi::null_buffer;

        constexpr static u32 kMaxMeshlets  = ~static_cast<u32>(0) >> 10;
        constexpr static u32 kMaxInstances = kMaxMeshlets >> 2;

        pass_geometry(const rhi::impl& impl, const app::pso_data& pipelines, rhi::context context, uvec2 render_res);

        void shutdown();

        void draw(rhi::command_buffer cmd, const common_resources& res, const loader::scene_counters& counters,
                  const draw_params& params) const;

        void on_render_size_changed(uvec2 new_render_size);

    private:
        void destroy_resources();

        void create_resources(uvec2 render_res);

        void on_pre_draw(rhi::command_buffer cmd) const;

        void build_hzb(rhi::command_buffer cmd) const;

        void cull_scene(rhi::command_buffer cmd, const common_resources& res, const loader::scene_counters& counters,
                        bool occlusion_cull) const;

        void draw_scene(rhi::command_buffer cmd, const common_resources& res, const rhi::pipeline& pipeline,
                        const loader::scene_counters& counters, u32 material_class, bool mesh_pipeline) const;

        void build_indexed_draw_commands(rhi::command_buffer cmd, const common_resources& res,
                                         const loader::scene_counters& counters, u32 material_class,
                                         bool enable_occlusion_cull) const;

    private:
        uvec2 m_render_res = uvec2(0);
        rhi::context m_ctx = rhi::null_context;

        const rhi::impl& m_rhi;
        const app::pso_data& m_pipelines;
    };
}
