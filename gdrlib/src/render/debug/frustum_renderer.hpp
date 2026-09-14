#pragma once

#include <glm/mat4x4.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>

namespace render::debug
{
    class frustum_renderer
    {
    private:
        struct frustum_pc_data
        {
            glm::mat4 renderer_vp;
        };

    public:
        explicit frustum_renderer(const vk_pipeline& pipeline)
            : m_pipeline(pipeline)
        {
        }

        void draw(VkCommandBuffer cmd, const glm::mat4& camera_vp, const render::vk_mapped_buffer& cull_data) const
        {
            ZoneScoped;
            const frustum_pc_data pc {
                .renderer_vp = camera_vp,
            };

            m_pipeline.bind(cmd);
            m_pipeline.push_constant(cmd, pc);

            const render::vk_descriptor_info bindings[] = {cull_data.buffer};
            m_pipeline.push_descriptor_set(cmd, bindings);
            vkCmdDraw(cmd, 24, 1, 0, 0);
        }

    private:
        const vk_pipeline& m_pipeline;
    };
}
