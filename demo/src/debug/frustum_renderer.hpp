#pragma once

#include <glm/mat4x4.hpp>
#include <render/rhi.hpp>

namespace app::debug
{
    class frustum_renderer
    {
    private:
        struct frustum_pc_data
        {
            glm::mat4 renderer_vp;
        };

    public:
        explicit frustum_renderer(const rhi::pipeline& pipeline)
            : m_pipeline(pipeline)
        {
        }

        void draw(const rhi::impl& impl, rhi::command_buffer cmd, const glm::mat4& camera_vp,
                  const rhi::buffer& cull_data) const
        {
            ZoneScoped;
            const frustum_pc_data pc {
                .renderer_vp = camera_vp,
            };

            const rhi::binding bindings[] = {cull_data};

            impl.cmd_bind_pso(cmd, m_pipeline);
            impl.cmd_push_constants(cmd, m_pipeline, &pc, sizeof(pc), 0);
            impl.cmd_push_bindings(cmd, m_pipeline, std::span {bindings});
            impl.cmd_draw(cmd, 24, 1, 0, 0);
        }

    private:
        const rhi::pipeline& m_pipeline;
    };
}
