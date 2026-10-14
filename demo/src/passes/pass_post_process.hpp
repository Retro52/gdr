#pragma once

#include <passes/common_resources.hpp>
#include <render/rhi.hpp>

namespace app
{
    struct pass_post_process
    {
        struct draw_params
        {
            rhi::image color       = rhi::null_image;
            rhi::pipeline pipeline = rhi::null_pipeline;
        };

        rhi::image output          = rhi::null_image;

        pass_post_process(const rhi::impl& impl, rhi::context context, uvec2 display_res);

        void shutdown();

        void on_display_size_changed(uvec2 new_display_size);

        void draw(rhi::command_buffer cmd, const draw_params& params) const;

    private:
        void destroy_resources();

        void create_resources(uvec2 display_res);

    private:
        uvec2 m_display_res = uvec2(0);
        rhi::context m_ctx = rhi::null_context;

        const rhi::impl& m_rhi;
    };
}
