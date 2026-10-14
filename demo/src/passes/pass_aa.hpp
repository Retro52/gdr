#pragma once

#include <app/pso.hpp>
#include <passes/common_resources.hpp>
#include <reflection/enum.hpp>
#include <render/rhi.hpp>

namespace app
{
    REGISTER_ENUM(aa_backend, blit, fxaa, fsr3);

    struct pass_aa
    {
        struct fxaa_params
        {
            f32 camera_near = 0.0F;
            f32 camera_far  = kF32Max;
        };

        struct fsr3_params
        {
            rhi::buffer mv_matrices;
            rhi::ffx_upscale_params ffx_upscale;
        };

        struct draw_params
        {
            rhi::image color = rhi::null_image;
            rhi::image depth = rhi::null_image;

            union
            {
                fxaa_params fxaa;
                fsr3_params ffx;
            } params;
        };

        aa_backend backend = aa_backend::fsr3;

        rhi::image output               = rhi::null_image;
        rhi::image motion_vectors       = rhi::null_image;
        rhi::sampler fxaa_color_sampler = rhi::null_sampler;
        rhi::sampler fxaa_depth_sampler = rhi::null_sampler;

        rhi::ffx_context ffx_context = rhi::null_ffx_context;

        pass_aa(const rhi::impl& impl, const app::pso_data& pipelines, rhi::context context, uvec2 render_res,
                uvec2 display_res);

        void shutdown();

        void next_jitter();

        [[nodiscard]] vec2 get_frame_jitter() const;

        [[nodiscard]] glm::mat4 get_jittered_matrix(const glm::mat4& proj) const;

        void draw(rhi::command_buffer cmd, const draw_params& params) const;

        void on_render_display_size_changed(uvec2 new_render_size, uvec2 new_display_size);

    private:
        void run_blit(rhi::command_buffer cmd, const draw_params& params) const;

        void run_fxaa(rhi::command_buffer cmd, const draw_params& params) const;

        void run_fsr3(rhi::command_buffer cmd, const draw_params& params) const;

        void destroy_resources();

        void create_resources(uvec2 render_res, uvec2 display_res);

    private:
        i32 m_jitter_offset = 0;

        uvec2 m_render_res  = uvec2(0);
        uvec2 m_display_res = uvec2(0);

        rhi::context m_ctx = rhi::null_context;

        const rhi::impl& m_rhi;
        const app::pso_data& m_pipelines;
    };
}
