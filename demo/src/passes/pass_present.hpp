#pragma once

#include <render/rhi.hpp>

namespace app
{
    struct pass_present
    {
        struct draw_params
        {
            rhi::image color;
            rhi::image output;

            uvec2 color_res;
            uvec2 display_res;
        };

        pass_present(const rhi::impl& impl, rhi::context context);

        void present(rhi::command_buffer cmd, rhi::swapchain sc, const draw_params& params) const;

    private:
        const rhi::impl& m_rhi;
        rhi::context m_ctx = rhi::null_context;
    };
}
