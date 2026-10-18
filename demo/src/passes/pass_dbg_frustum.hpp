#pragma once

#include <glm/mat4x4.hpp>
#include <render/rhi.hpp>

namespace app
{
    struct pass_dbg_frustum
    {
        struct draw_params
        {
            rhi::buffer cull_data;

            rhi::attachment depth;
            rhi::attachment output;

            ivec2 viewport;
            glm::mat4 camera_vp;
        };

        pass_dbg_frustum(const rhi::impl& impl, rhi::context context, rhi::pipeline pipeline);

        void draw(rhi::command_buffer cmd, const draw_params& params) const;

    private:
        const rhi::impl& m_rhi;

        rhi::context m_ctx       = rhi::null_context;
        rhi::pipeline m_pipeline = rhi::null_pipeline;
    };
}
