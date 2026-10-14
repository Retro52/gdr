#pragma once

#include <app/pso.hpp>
#include <passes/common_resources.hpp>
#include <render/rhi.hpp>

namespace app
{
    struct pass_light_resolve
    {
        struct draw_params
        {
            rhi::image color = rhi::null_image;
            rhi::image depth = rhi::null_image;

            rhi::buffer indices    = rhi::null_buffer;
            rhi::pipeline pipeline = rhi::null_pipeline;

            rhi::binding brdf_lookup;

            rhi::binding env_cube;
            rhi::binding env_conv;
            rhi::binding env_prefilter;

            glm::mat4 view;
            glm::mat4 projection;
        };

        rhi::image output          = rhi::null_image;
        rhi::sampler depth_sampler = rhi::null_sampler;

        pass_light_resolve(const rhi::impl& impl, rhi::context context, uvec2 render_res);

        void shutdown();

        void on_render_size_changed(uvec2 new_render_size);

        void draw(rhi::command_buffer cmd, const common_resources& res, const draw_params& params) const;

    private:
        void destroy_resources();

        void create_resources(uvec2 render_res);

    private:
        uvec2 m_render_res = uvec2(0);
        rhi::context m_ctx = rhi::null_context;

        const rhi::impl& m_rhi;
    };
}
